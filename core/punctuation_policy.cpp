#include "punctuation_policy.h"
#include "../contracts/punctuation/policy.h"
namespace metasequoia
{
const char *PunctuationPolicy::translate(char character)
{
    using namespace punctuation_contract;
    if (const char *punctuation = simple_output(character))
        return punctuation;

    if (const auto *mapping = alternating_mapping(character))
    {
        const bool opening = character == '"' ? next_double_quote_is_opening_ : next_single_quote_is_opening_;
        if (character == '"')
            next_double_quote_is_opening_ = !next_double_quote_is_opening_;
        else
            next_single_quote_is_opening_ = !next_single_quote_is_opening_;
        return opening ? mapping->opening : mapping->closing;
    }

    // Book title marks nest: the outer pair is 《》 and anything inside it uses 〈〉. The counter is
    // what decides which, so an unmatched '>' has to leave it at zero rather than drive it negative.
    if (character == nested_opening_input)
        return book_title_nesting_++ == 0 ? nested_opening : nested_opening_inner;
    if (character == nested_closing_input)
    {
        if (book_title_nesting_ > 0)
        {
            --book_title_nesting_;
            return book_title_nesting_ == 0 ? nested_closing : nested_closing_inner;
        }
        return nested_closing;
    }
    return nullptr;
}
} // namespace metasequoia
