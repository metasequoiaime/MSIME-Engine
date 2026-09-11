#pragma once
namespace metasequoia
{
class PunctuationPolicy
{
  public:
    const char *translate(char character);
    void set_paired_enabled(bool enabled)
    {
        paired_enabled_ = enabled;
    }

  private:
    bool next_double_quote_is_opening_ = true;
    bool next_single_quote_is_opening_ = true;
    int book_title_nesting_ = 0;
    bool paired_enabled_ = true;
};
} // namespace metasequoia
