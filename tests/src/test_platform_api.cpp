#include "../../core/query_request.h"

#include <cstdint>
#include <type_traits>

#ifdef _WIN32
static_assert(std::is_same_v<decltype(KeyStroke::vk), UINT>);
static_assert(std::is_same_v<decltype(KeyStroke::modifiers_down), UINT>);
static_assert(std::is_same_v<decltype(KeyStroke::wch), WCHAR>);
static_assert(sizeof(WCHAR) == 2);
#else
static_assert(std::is_same_v<decltype(KeyStroke::vk), std::uint32_t>);
static_assert(std::is_same_v<decltype(KeyStroke::modifiers_down), std::uint32_t>);
static_assert(std::is_same_v<decltype(KeyStroke::wch), char16_t>);
#endif

int main()
{
    return 0;
}
