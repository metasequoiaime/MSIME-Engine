#include "punctuation_policy.h"
namespace metasequoia
{
const char *PunctuationPolicy::translate(char character)
{
    const char *punctuation = nullptr;
    switch (character)
    {
    case ',':
        punctuation = "，";
        break;
    case '.':
        punctuation = "。";
        break;
    case '?':
        punctuation = "？";
        break;
    case '!':
        punctuation = "！";
        break;
    case ';':
        punctuation = "；";
        break;
    case ':':
        punctuation = "：";
        break;
    case '"':
        punctuation = next_double_quote_is_opening_ ? "“" : "”";
        next_double_quote_is_opening_ = !next_double_quote_is_opening_;
        break;
    case '\'':
        punctuation = next_single_quote_is_opening_ ? "‘" : "’";
        next_single_quote_is_opening_ = !next_single_quote_is_opening_;
        break;
    case '(':
        punctuation = "（";
        break;
    case ')':
        punctuation = "）";
        break;
    case '[':
        punctuation = "【";
        break;
    case ']':
        punctuation = "】";
        break;
    // Book title marks nest: the outer pair is 《》 and anything inside it uses 〈〉. The counter is
    // what decides which, so an unmatched '>' has to leave it at zero rather than drive it negative.
    case '<':
        punctuation = book_title_nesting_++ == 0 ? "《" : "〈";
        break;
    case '>':
        if (book_title_nesting_ > 0)
        {
            --book_title_nesting_;
            punctuation = book_title_nesting_ == 0 ? "》" : "〉";
        }
        else
        {
            punctuation = "》";
        }
        break;
    case '\\':
        punctuation = "、";
        break;
    case '`':
        punctuation = "·";
        break;
    case '$':
        punctuation = "￥";
        break;
    case '^':
        punctuation = "……";
        break;
    case '_':
        punctuation = "——";
        break;
    default:
        return nullptr;
    }

    return punctuation;
}
}
