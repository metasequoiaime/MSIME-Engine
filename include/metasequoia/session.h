#pragma once

#include "../../core/input_session_types.h"
#include "../../core/runtime_paths.h"
#include "../../shuangpin/shuangpin_profile.h"
#include <memory>

namespace metasequoia
{
struct SessionOptions
{
    RuntimePaths paths;
    SchemeType scheme = SchemeType::Quanpin;
    ShuangpinProfile shuangpin_profile = GetXiaoheShuangpinProfile();
    std::string helpcode_schema = "lantian";
    bool autocorrect = true;
    bool helpcode = true;
    bool chinese_punctuation = true;
    bool learning = true;
    FrequencyAdjustmentOptions frequency;
    LocalModeOptions local_modes;
    EnglishInputOptions english;
    MixedExpressiveOptions expressive;
};

struct SessionSnapshot
{
    SchemeType scheme;
    LocalInputMode local_mode;
    std::string preedit;
    std::string raw_segmentation;
    std::string normalized_segmentation;
    std::vector<WordItem> candidates;
};

// Stable platform entry point. One host serializes calls to its session; distinct sessions
// can run concurrently. Snapshots and online requests are values safe to hand to other threads.
// No SQLite, provider registry, raw key codes or mutable composition internals are exposed.
class Session
{
public:
    explicit Session(SessionOptions options);
    ~Session();
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;

    KeyResult character(char value, bool shift_only = false);
    KeyResult command(Command value);
    KeyResult candidate_key(char value);
    KeyResult punctuation(char value);
    KeyResult select(std::size_t index);
    KeyResult select_edge(std::size_t index, CandidateEdge edge);
    KeyResult finish();
    void switch_scheme(SchemeType scheme);
    bool set_helpcode_schema(const std::string &schema);
    void set_dedicated_english(bool enabled);
    SessionSnapshot snapshot() const;
    std::optional<OnlineQuery> online_query() const;
    bool apply_online_candidate(const OnlineQuery &query, std::string candidate, CandidateSource source);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
}
