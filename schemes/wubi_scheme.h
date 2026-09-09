#pragma once

#include "input_scheme.h"
#include <string>
#include <vector>

class WubiScheme : public IInputScheme
{
  public:
    void reset() override;
    void handle_key(ImeKeyCode vk, ImeModifierMask modifiers_down, ImeCharacter wch) override;
    QueryRequest build_request() const override;
    std::string get_preedit() const override;
    SchemeType type() const override;
    void set_raw_input(const std::string &raw_input, const std::string &raw_input_with_cases);
    // A wubi code is four letters, and a fifth is normally refused. Mixed input lifts that only
    // once the table has failed the code in hand, so a spelling long enough to need it -- nihao,
    // women, zhongguo -- can be finished. A code the table answers keeps the four-letter limit and
    // a fluent wubi typist sees no change.
    void set_extended_length_allowed(bool allowed);

  private:
    static constexpr size_t kMaxCodeLength = 4;
    // Long enough for any spelling, short enough to stay a composition rather than a paragraph.
    static constexpr size_t kMaxMixedCodeLength = 32;

    size_t max_code_length() const;

    std::string raw_input_;
    std::vector<KeyStroke> key_strokes_;
    bool extended_length_allowed_ = false;
};
