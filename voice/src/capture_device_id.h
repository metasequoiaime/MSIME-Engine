#pragma once
#include "miniaudio.h"
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace metasequoia::voice::detail
{
// Encode only the active backend's terminated identifier, never union padding.
// Hex keeps IDs opaque ASCII and preserves UTF-16 WASAPI endpoint code units.
template <typename Unit, size_t N> std::string device_units(std::string_view prefix, const Unit (&units)[N], unsigned digits)
{
    constexpr char hex[] = "0123456789abcdef";
    std::string result(prefix);
    for (size_t index = 0; index < N; ++index)
    {
        const auto value = static_cast<unsigned>(static_cast<std::make_unsigned_t<Unit>>(units[index]));
        if (!value)
            return index ? result : std::string{};
        for (unsigned digit = digits; digit > 0; --digit)
            result.push_back(hex[(value >> ((digit - 1) * 4)) & 15]);
    }
    return {}; // unterminated native identifier
}
inline std::string capture_device_id(ma_backend backend, const ma_device_id &id)
{
    switch (backend)
    {
    case ma_backend_wasapi:
        return device_units("wasapi:", id.wasapi, 4);
    case ma_backend_coreaudio:
        return device_units("coreaudio:", id.coreaudio, 2);
    case ma_backend_pulseaudio:
        return device_units("pulseaudio:", id.pulse, 2);
    case ma_backend_alsa:
        return device_units("alsa:", id.alsa, 2);
    default:
        return {};
    }
}
inline std::optional<ma_backend> capture_backend(std::string_view id)
{
    struct Format
    {
        std::string_view prefix;
        ma_backend backend;
        size_t unit;
        size_t capacity;
    };
    for (const auto &format : {Format{"wasapi:", ma_backend_wasapi, 4, 64}, Format{"coreaudio:", ma_backend_coreaudio, 2, 256}, Format{"pulseaudio:", ma_backend_pulseaudio, 2, 256}, Format{"alsa:", ma_backend_alsa, 2, 256}})
    {
        if (id.substr(0, format.prefix.size()) != format.prefix)
            continue;
        const auto payload = id.substr(format.prefix.size());
        if (payload.empty() || payload.size() % format.unit || payload.size() >= format.unit * format.capacity)
            return std::nullopt;
        for (const char ch : payload)
            if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f')))
                return std::nullopt;
        return format.backend;
    }
    return std::nullopt;
}
inline const ma_device_info *select_capture_device(ma_backend backend, const ma_device_info *devices, ma_uint32 count, std::string_view requested)
{
    if (!devices || requested.empty())
        return nullptr;
    const ma_device_info *selected = nullptr;
    for (ma_uint32 index = 0; index < count; ++index)
        if (capture_device_id(backend, devices[index].id) == requested)
        {
            if (selected)
                return nullptr; // ambiguous identity must not pick arbitrarily
            selected = &devices[index];
        }
    return selected;
}
} // namespace metasequoia::voice::detail
