#pragma once
#include "../include/metasequoia/session.h"
#include "../quanpin/quanpin_dictionary.h"

namespace metasequoia
{
// Owns ambiguous digit composition; platforms only send keys and choose snapshot indices.
class NineKeySession
{
  public:
    explicit NineKeySession(RuntimePaths paths, bool learning = false, FrequencyAdjustmentOptions frequency = {})
        : paths_(std::move(paths)), learning_(learning), frequency_(frequency)
    {
    }
    bool active() const
    {
        return !digits_.empty();
    }
    KeyResult character(char digit);
    KeyResult command(Command command);
    KeyResult select(std::size_t index);
    KeyResult finish(std::size_t index);
    KeyResult choose_spelling(std::size_t index);
    KeyResult pin(std::size_t index);
    KeyResult remove(std::size_t index);
    KeyResult set_position(std::size_t index, int position);
    SessionSnapshot snapshot() const;

  private:
    void refresh();
    std::size_t locked_length() const;
    std::string ranking_context() const;
    bool editable(std::size_t index) const;
    std::optional<std::string> adjust_frequency(std::size_t index, bool force_top);
    RuntimePaths paths_;
    bool learning_;
    FrequencyAdjustmentOptions frequency_;
    std::unique_ptr<QuanpinDictionary> dictionary_;
    std::string digits_;
    std::vector<std::string> locked_;
    std::vector<std::string> spellings_;
    std::vector<WordItem> candidates_;
};
} // namespace metasequoia
