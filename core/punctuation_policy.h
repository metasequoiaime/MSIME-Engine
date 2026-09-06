#pragma once
namespace metasequoia
{
class PunctuationPolicy
{
  public:
    const char *translate(char character);

  private:
    bool next_double_quote_is_opening_ = true;
    bool next_single_quote_is_opening_ = true;
    int book_title_nesting_ = 0;
};
} // namespace metasequoia
