#include "word_lattice.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace quanpin
{
namespace
{

constexpr double kNegInf = -std::numeric_limits<double>::infinity();

std::string join_span(const Segments &span)
{
    std::string key;
    for (size_t i = 0; i < span.size(); ++i)
    {
        if (i)
            key.push_back('\'');
        key += span[i];
    }
    return key;
}

size_t syllable_count_from_key(const std::string &key)
{
    if (key.empty())
        return 0;
    size_t n = 1;
    for (char c : key)
    {
        if (c == '\'')
            ++n;
    }
    return n;
}

struct LatticeEdge
{
    size_t end = 0;
    std::string word;
    std::string key;
    std::int64_t weight = 0;
    double log_prob = 0;
};

double edge_log_prob(std::int64_t weight, size_t syllables, const WordLatticeOptions &options)
{
    const double w = weight > 0 ? static_cast<double>(weight) : 1.0;
    const double z = options.unigram_z > 1.0 ? options.unigram_z : 1e6;
    const double lp = std::log(w);
    // Single-character rows in msime.db are raw corpus counts (often 1e6+);
    // multi-syllable rows are phrase weights on a much smaller scale.
    // libpinyin stores comparable log-probabilities; we approximate that by
    // down-projecting unigrams and giving dictionary phrases a length bonus.
    if (syllables <= 1)
        return lp - std::log(z);
    return lp + options.phrase_length_bonus * static_cast<double>(syllables);
}

struct Hyp
{
    double score = kNegInf;
    int prev_pos = -1;
    int prev_idx = -1;
    std::string word;
    std::string key;
};

void keep_beam(std::vector<Hyp> &column, int beam)
{
    if (static_cast<int>(column.size()) <= beam)
        return;
    std::partial_sort(column.begin(), column.begin() + beam, column.end(),
                      [](const Hyp &a, const Hyp &b) { return a.score > b.score; });
    column.resize(static_cast<size_t>(beam));
}

std::vector<std::vector<LatticeEdge>> build_graph(const Segments &syllables, const WordLatticeLookup &lookup,
                                                  const WordLatticeOptions &options)
{
    const size_t n = syllables.size();
    std::vector<std::vector<LatticeEdge>> graph(n);
    const size_t max_len = static_cast<size_t>(std::max(1, options.max_phrase_syllables));
    std::unordered_map<std::string, std::vector<LatticeLexeme>> span_cache;
    for (size_t start = 0; start < n; ++start)
    {
        const size_t max_end = (std::min)(n, start + max_len);
        for (size_t end = start + 1; end <= max_end; ++end)
        {
            Segments span(syllables.begin() + static_cast<std::ptrdiff_t>(start),
                          syllables.begin() + static_cast<std::ptrdiff_t>(end));
            const std::string span_key = join_span(span);
            auto cached = span_cache.find(span_key);
            if (cached == span_cache.end())
            {
                cached = span_cache.emplace(span_key, lookup(span)).first;
            }
            const auto &rows = cached->second;
            const size_t take = (std::min)(rows.size(), static_cast<size_t>(std::max(0, options.span_limit)));
            for (size_t i = 0; i < take; ++i)
            {
                const auto &row = rows[i];
                if (row.value.empty())
                    continue;
                LatticeEdge edge;
                edge.end = end;
                edge.word = row.value;
                edge.key = row.key.empty() ? span_key : row.key;
                edge.weight = row.weight;
                edge.log_prob = edge_log_prob(edge.weight, end - start, options);
                graph[start].push_back(std::move(edge));
            }
        }
    }
    return graph;
}

size_t utf8_codepoints(const std::string &text)
{
    size_t n = 0;
    for (unsigned char c : text)
    {
        if ((c & 0xC0) != 0x80)
            ++n;
    }
    return n;
}

bool covers_all_syllables(const WordItem &item, size_t n_syllables)
{
    if (n_syllables == 0)
        return false;
    if (utf8_codepoints(item.word) == n_syllables)
        return true;
    if (!item.canonical_pinyin.empty() && syllable_count_from_key(item.canonical_pinyin) == n_syllables)
        return true;
    return false;
}

} // namespace

std::vector<LatticePath> decode_word_lattice(const Segments &syllables, const WordLatticeLookup &lookup,
                                             const WordLatticeOptions &options)
{
    if (syllables.empty() || !lookup)
        return {};

    const size_t n = syllables.size();
    const auto graph = build_graph(syllables, lookup, options);
    std::vector<std::vector<Hyp>> columns(n + 1);
    columns[0].push_back(Hyp{0.0, -1, -1, {}, {}});

    for (size_t pos = 0; pos < n; ++pos)
    {
        keep_beam(columns[pos], options.beam);
        if (columns[pos].empty())
            continue;
        for (int hi = 0; hi < static_cast<int>(columns[pos].size()); ++hi)
        {
            const Hyp &hyp = columns[pos][static_cast<size_t>(hi)];
            for (const auto &edge : graph[pos])
            {
                Hyp next;
                next.score = hyp.score + edge.log_prob;
                next.prev_pos = static_cast<int>(pos);
                next.prev_idx = hi;
                next.word = edge.word;
                next.key = edge.key;
                columns[edge.end].push_back(std::move(next));
            }
        }
    }

    auto &final_col = columns[n];
    if (final_col.empty())
        return {};
    std::sort(final_col.begin(), final_col.end(), [](const Hyp &a, const Hyp &b) { return a.score > b.score; });
    const int take = (std::min)(options.nbest, static_cast<int>(final_col.size()));

    std::vector<LatticePath> paths;
    paths.reserve(static_cast<size_t>(take));
    std::unordered_set<std::string> seen;
    for (int i = 0; i < static_cast<int>(final_col.size()) && static_cast<int>(paths.size()) < take; ++i)
    {
        LatticePath path;
        path.log_prob = final_col[static_cast<size_t>(i)].score;
        int pos = static_cast<int>(n);
        int idx = i;
        std::vector<std::string> keys;
        while (pos > 0 && idx >= 0)
        {
            const Hyp &h = columns[static_cast<size_t>(pos)][static_cast<size_t>(idx)];
            path.words.push_back(h.word);
            keys.push_back(h.key);
            pos = h.prev_pos;
            idx = h.prev_idx;
        }
        std::reverse(path.words.begin(), path.words.end());
        std::reverse(keys.begin(), keys.end());
        for (const auto &w : path.words)
            path.sentence += w;
        for (size_t k = 0; k < keys.size(); ++k)
        {
            if (k)
                path.key.push_back('\'');
            path.key += keys[k];
        }
        if (path.sentence.empty() || !seen.insert(path.sentence).second)
            continue;
        paths.push_back(std::move(path));
    }
    return paths;
}

void merge_lattice_candidates(std::vector<WordItem> &candidates, const Segments &syllables,
                              const WordLatticeLookup &lookup, const std::string &typed_pinyin,
                              const WordLatticeOptions &options)
{
    if (!lookup || syllables.size() < 3)
        return;
    if (!has_only_complete_pinyin_segments(syllables))
        return;

    const auto paths = decode_word_lattice(syllables, lookup, options);
    if (paths.empty())
        return;

    std::unordered_set<std::string> already;
    for (const auto &item : candidates)
        already.insert(item.word);

    std::vector<WordItem> extra;
    for (const auto &path : paths)
    {
        if (!already.insert(path.sentence).second)
            continue;
        // Often negative (log-space). Ranking is insert order, not weight.
        const auto weight = static_cast<std::int64_t>(path.log_prob * 1000.0);
        extra.emplace_back(typed_pinyin, path.sentence, weight, CandidateSource::Generated, path.key);
    }
    if (extra.empty())
        return;

    size_t insert_at = 0;
    while (insert_at < candidates.size() && covers_all_syllables(candidates[insert_at], syllables.size()) &&
           (candidates[insert_at].source == CandidateSource::Database ||
            candidates[insert_at].source == CandidateSource::UserDatabase))
        ++insert_at;
    candidates.insert(candidates.begin() + static_cast<std::ptrdiff_t>(insert_at), extra.begin(), extra.end());
}

} // namespace quanpin
