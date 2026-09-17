// Whole-sentence conversion accuracy over a frozen eval set.
//
// Reads `pinyin<TAB>sentence` rows (tests/scripts/build_eval_set.py writes them), types each pinyin into a fresh composition one letter at a time, and reports how often the engine's answer is the sentence the text came from. Nothing is learned and no personal history carries between rows, so two runs of the same binary on the same set produce the same numbers and two builds can be compared directly.
//
// Four numbers, because they answer different questions:
//   top-1            the first candidate is the whole sentence. This is the ranking quality the user sees.
//   within page      the sentence is somewhere on the first page. This is the ceiling for any reranker: a sentence the decoder never proposed cannot be promoted into first place, so the gap between this and top-1 is all a rescoring pass could ever win.
//   commit           what Session::finish() actually inserts, which for a partially covered input is the leading candidate plus the remaining segments rather than candidate zero.
//   character        per-position agreement between candidate zero and the sentence, so a near miss scores better than a wrong sentence of the same length.
//
// Usage: eval_sentences <dictionary-dir> <sentences.tsv> [--limit N] [--page N] [--detail out.tsv]

#include <metasequoia/session.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace metasequoia;

namespace
{

struct Row
{
    std::string pinyin;
    std::string sentence;
};

// Character count and per-position comparison both need the sentence split at code point boundaries; a byte-wise diff would score a wrong character with a shared lead byte as a partial match.
std::vector<std::string> split_utf8(const std::string &text)
{
    std::vector<std::string> characters;
    for (std::size_t i = 0; i < text.size();)
    {
        const auto lead = static_cast<unsigned char>(text[i]);
        std::size_t width = 1;
        if ((lead & 0xF8) == 0xF0)
            width = 4;
        else if ((lead & 0xF0) == 0xE0)
            width = 3;
        else if ((lead & 0xE0) == 0xC0)
            width = 2;
        width = (std::min)(width, text.size() - i);
        characters.push_back(text.substr(i, width));
        i += width;
    }
    return characters;
}

std::vector<Row> load_rows(const std::filesystem::path &path, std::size_t limit)
{
    std::vector<Row> rows;
    std::ifstream file(path);
    if (!file)
        return rows;
    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        const auto tab = line.find('\t');
        if (tab == std::string::npos || tab == 0 || tab + 1 >= line.size())
            continue;
        rows.push_back(Row{line.substr(0, tab), line.substr(tab + 1)});
        if (limit && rows.size() >= limit)
            break;
    }
    return rows;
}

double percent(std::size_t part, std::size_t whole)
{
    return whole ? 100.0 * static_cast<double>(part) / static_cast<double>(whole) : 0.0;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::cerr << "usage: eval_sentences <dictionary-dir> <sentences.tsv> [--limit N] [--page N] [--detail out.tsv]\n";
        return 2;
    }
    const std::filesystem::path dictionary = argv[1];
    const std::filesystem::path set_path = argv[2];
    std::size_t limit = 0;
    std::size_t page = 5;
    std::string detail_path;
    for (int i = 3; i < argc; ++i)
    {
        const std::string flag = argv[i];
        const bool has_value = i + 1 < argc;
        if (flag == "--limit" && has_value)
            limit = static_cast<std::size_t>(std::strtoul(argv[++i], nullptr, 10));
        else if (flag == "--page" && has_value)
            page = static_cast<std::size_t>(std::strtoul(argv[++i], nullptr, 10));
        else if (flag == "--detail" && has_value)
            detail_path = argv[++i];
        else
        {
            std::cerr << "unknown argument: " << flag << '\n';
            return 2;
        }
    }

    const auto rows = load_rows(set_path, limit);
    if (rows.empty())
    {
        std::cerr << "no usable rows in " << set_path << '\n';
        return 1;
    }

    const auto user = std::filesystem::temp_directory_path() / "msime-eval-sentences-user";
    std::filesystem::remove_all(user);
    std::filesystem::create_directories(user);
    SessionOptions options;
    options.paths = {dictionary, user, user, dictionary};
    // Learning off keeps every row a cold start: the set measures the shipped dictionary, not the order in which the rows happen to be read.
    options.learning = false;
    Session session(options);

    std::ofstream detail;
    if (!detail_path.empty())
    {
        detail.open(detail_path);
        detail << "pinyin\texpected\ttop1\trank\tcommit\n";
    }

    std::size_t top1 = 0;
    std::size_t within_page = 0;
    std::size_t committed = 0;
    std::size_t matching_characters = 0;
    std::size_t total_characters = 0;
    std::vector<double> query_ms;
    query_ms.reserve(rows.size());

    for (const auto &row : rows)
    {
        double last_key_ms = 0;
        for (char letter : row.pinyin)
        {
            const auto start = std::chrono::steady_clock::now();
            session.character(letter);
            last_key_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        }
        query_ms.push_back(last_key_ms);

        const auto view = session.snapshot();
        const std::string best = view.candidates.empty() ? std::string() : view.candidates.front().word;
        if (best == row.sentence)
            ++top1;

        std::size_t rank = 0;
        for (std::size_t i = 0; i < view.candidates.size() && i < page; ++i)
        {
            if (view.candidates[i].word == row.sentence)
            {
                rank = i + 1;
                ++within_page;
                break;
            }
        }

        const auto expected_characters = split_utf8(row.sentence);
        const auto best_characters = split_utf8(best);
        total_characters += expected_characters.size();
        for (std::size_t i = 0; i < expected_characters.size() && i < best_characters.size(); ++i)
            if (expected_characters[i] == best_characters[i])
                ++matching_characters;

        const auto finished = session.finish();
        const std::string commit = finished.commit.value_or(std::string());
        if (commit == row.sentence)
            ++committed;

        if (detail.is_open())
            detail << row.pinyin << '\t' << row.sentence << '\t' << best << '\t' << rank << '\t' << commit << '\n';
    }

    double total_ms = 0;
    for (double value : query_ms)
        total_ms += value;
    std::sort(query_ms.begin(), query_ms.end());
    const double average_ms = total_ms / static_cast<double>(query_ms.size());
    const double p95_ms = query_ms[static_cast<std::size_t>(0.95 * static_cast<double>(query_ms.size() - 1))];
    const double max_ms = query_ms.back();

    std::cout << std::fixed << std::setprecision(1);
    std::cout << "sentences            " << rows.size() << '\n';
    std::cout << "top-1 whole sentence " << percent(top1, rows.size()) << "%\n";
    std::cout << "within first " << page << "       " << percent(within_page, rows.size()) << "%\n";
    std::cout << "commit on space      " << percent(committed, rows.size()) << "%\n";
    std::cout << "character accuracy   " << percent(matching_characters, total_characters) << "%\n";
    std::cout << std::setprecision(2);
    std::cout << "final-key latency    avg " << average_ms << " ms  p95 " << p95_ms << " ms  max " << max_ms << " ms\n";
    if (detail.is_open())
        std::cout << "per-sentence detail  " << detail_path << '\n';

    std::filesystem::remove_all(user);
    return 0;
}
