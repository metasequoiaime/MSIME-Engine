#include <metasequoia/session.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace metasequoia;

// 九键延迟基准兼候选快照:第二个参数给出文件名时,把每次按键的候选逐条写出,用于证明
// 优化前后候选完全一致。
int main(int argc, char **argv)
{
    const std::filesystem::path dictionary = argc > 1 ? argv[1] : "/tmp/msime-real-dict";
    const std::string snapshot_path = argc > 2 ? argv[2] : "";
    const std::filesystem::path user = std::filesystem::temp_directory_path() / "msime-nine-key-user";
    std::filesystem::remove_all(user);
    std::filesystem::create_directories(user);
    SessionOptions options;
    options.paths = {dictionary, user, user, dictionary};
    options.learning = false;

    std::vector<std::string> corpus;
    for (const char *code :
         {"6442",  "94664",  "7356918", "74463626", "9668262", "7828974", "3426242", "64362624", "74263284", "4667736",
          "8464",  "2647",   "36428",   "9427463",  "2684",    "76482",   "34783",   "9463",     "58464",    "7464266",
          "46337", "846284", "26384",   "946837",   "3648",    "74628",   "9273",    "64738264"})
        corpus.emplace_back(code);

    std::ofstream snapshot;
    if (!snapshot_path.empty())
        snapshot.open(snapshot_path);

    double total = 0;
    int taps = 0;
    for (const auto &code : corpus)
    {
        Session session(options);
        session.set_nine_key_enabled(true);
        for (char digit : code)
        {
            const auto start = std::chrono::steady_clock::now();
            session.character(digit);
            total += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            ++taps;
            if (snapshot.is_open())
            {
                const auto view = session.snapshot();
                snapshot << code << ' ' << taps << " |";
                for (const auto &item : view.candidates)
                    snapshot << ' ' << item.word;
                snapshot << " |";
                for (const auto &spelling : view.nine_key_spellings)
                    snapshot << ' ' << spelling;
                snapshot << '\n';
            }
        }
    }
    std::cout << "每键平均 " << (total / taps) << " ms  (" << taps << " taps)\n";
    std::filesystem::remove_all(user);
    return 0;
}
