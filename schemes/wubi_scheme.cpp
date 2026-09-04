#include "wubi_scheme.h"
#include <algorithm>
#include <cctype>

namespace
{
bool is_wubi_vk(ImeKeyCode vk)
{
    return vk >= 'A' && vk <= 'Y';
}

std::string normalize_wubi_code(const std::string &input, size_t max_length)
{
    std::string normalized;
    normalized.reserve((std::min)(input.size(), max_length));
    for (const unsigned char ch : input)
    {
        const char lower = static_cast<char>(std::tolower(ch));
        if (lower < 'a' || lower > 'y')
        {
            continue;
        }
        normalized.push_back(lower);
        if (normalized.size() == max_length)
        {
            break;
        }
    }
    return normalized;
}
} // namespace

void WubiScheme::reset()
{
    raw_input_.clear();
    key_strokes_.clear();
}

void WubiScheme::handle_key(ImeKeyCode vk, ImeModifierMask modifiers_down, ImeCharacter wch)
{
    if (vk == ImeKey::Backspace)
    {
        if (!raw_input_.empty())
        {
            raw_input_.pop_back();
        }
        if (!key_strokes_.empty())
        {
            key_strokes_.pop_back();
        }
        return;
    }

    if (vk == ImeKey::Escape || vk == ImeKey::Return)
    {
        reset();
        return;
    }

    if (!is_wubi_vk(vk) || raw_input_.size() >= kMaxCodeLength)
    {
        return;
    }

    key_strokes_.push_back(KeyStroke{vk, modifiers_down, wch});
    raw_input_.push_back(static_cast<char>(vk + ('a' - 'A')));
}

void WubiScheme::set_raw_input(const std::string &raw_input, const std::string &raw_input_with_cases)
{
    raw_input_ = normalize_wubi_code(raw_input_with_cases.empty() ? raw_input : raw_input_with_cases, kMaxCodeLength);
    key_strokes_.clear();
}

QueryRequest WubiScheme::build_request() const
{
    QueryRequest request;
    request.scheme = type();
    request.raw_input = raw_input_;
    request.raw_input_with_cases = raw_input_;
    request.normalized_input = raw_input_;
    request.raw_segmentation = raw_input_;
    request.normalized_segmentation = raw_input_;
    request.segmentation = raw_input_;
    request.key_strokes = key_strokes_;
    request.valid = !raw_input_.empty();
    return request;
}

std::string WubiScheme::get_preedit() const
{
    return raw_input_;
}

SchemeType WubiScheme::type() const
{
    return SchemeType::Wubi;
}
