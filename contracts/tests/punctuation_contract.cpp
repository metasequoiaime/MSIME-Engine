#include "../punctuation/policy.h"
#include <iostream>

int main()
{
    using namespace metasequoia::punctuation_contract;
    if (version != 1 || simple.size() != 15 || alternating.size() != 2)
        return 1;
    if (!is_supported(',') || !is_supported('"') || !is_supported('<') || !is_supported('_') || is_supported('a'))
        return 1;
    const auto *yen = simple_output('$');
    if (!yen || std::string_view(yen) != "￥" || simple_output('a') != nullptr)
        return 1;
    const auto *quotes = alternating_mapping('"');
    if (!quotes || std::string_view(quotes->opening) != "“" || std::string_view(quotes->closing) != "”")
        return 1;
    if (nested_opening_input != '<' || nested_closing_input != '>' || std::string_view(nested_opening_inner) != "〈")
        return 1;
    std::cout << "Punctuation contract passed\n";
    return 0;
}
