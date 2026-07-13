#pragma once

// Transcoding between UTF-8 and the other Unicode encoding forms, plus
// single-code-point encoding. This is the Windows API boundary layer: the
// Win32 W-functions speak UTF-16 wchar_t, and the standard offers no
// locale-free way to convert u8string <-> wstring (codecvt is deprecated
// and stateful). Everything here is deterministic and locale-free.
//
// Each direction comes in two flavors, mirroring the library's policy split
// between validate() and code_points():
//   to_X(...)        -> expected<Xstring, decode_error>: fails on the first
//                       ill-formed sequence, reporting its code-unit offset.
//   to_X_lossy(...)  -> Xstring: substitutes U+FFFD (with maximal-subpart
//                       consumption on the UTF-8 side) and always succeeds.
//
// Trust by convention does not extend across an encoding conversion: even
// presumed-valid u8strings are fully checked as a side effect of decoding.

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include "cast.hpp"
#include "text.hpp"

namespace u8io {

// True for code points that may appear in well-formed Unicode text: at most
// U+10FFFF and not a surrogate.
[[nodiscard]] constexpr bool is_scalar_value(char32_t code_point) noexcept {
    return code_point <= 0x10FFFF &&
           (code_point < 0xD800 || code_point > 0xDFFF);
}

// The UTF-8 encoding of one scalar value, in place (no allocation).
struct encoded_code_point {
    std::array<char8_t, 4> units{};
    std::uint8_t count = 0;

    [[nodiscard]] constexpr std::u8string_view view() const noexcept {
        return {units.data(), count};
    }
    constexpr operator std::u8string_view() const noexcept { return view(); }
};

// Encodes one code point as UTF-8. A non-scalar value (surrogate, above
// U+10FFFF) encodes as U+FFFD, consistent with the decoding side; check
// is_scalar_value() first where a replacement character would be the wrong
// answer.
[[nodiscard]] constexpr encoded_code_point encode(
    char32_t code_point) noexcept {
    if (!is_scalar_value(code_point)) code_point = replacement_character;
    encoded_code_point out;
    if (code_point < 0x80) {
        out.units[0] = static_cast<char8_t>(code_point);
        out.count = 1;
    } else if (code_point < 0x800) {
        out.units[0] = static_cast<char8_t>(0xC0 | (code_point >> 6));
        out.units[1] = static_cast<char8_t>(0x80 | (code_point & 0x3F));
        out.count = 2;
    } else if (code_point < 0x10000) {
        out.units[0] = static_cast<char8_t>(0xE0 | (code_point >> 12));
        out.units[1] = static_cast<char8_t>(0x80 | ((code_point >> 6) & 0x3F));
        out.units[2] = static_cast<char8_t>(0x80 | (code_point & 0x3F));
        out.count = 3;
    } else {
        out.units[0] = static_cast<char8_t>(0xF0 | (code_point >> 18));
        out.units[1] = static_cast<char8_t>(0x80 | ((code_point >> 12) & 0x3F));
        out.units[2] = static_cast<char8_t>(0x80 | ((code_point >> 6) & 0x3F));
        out.units[3] = static_cast<char8_t>(0x80 | (code_point & 0x3F));
        out.count = 4;
    }
    return out;
}

constexpr void append(std::u8string& out, char32_t code_point) {
    out += encode(code_point).view();
}

namespace detail {

// Decodes one scalar value from UTF-16. Requires pos != end. A lone or
// mis-paired surrogate yields U+FFFD and consumes one unit.
constexpr decode_result decode_one_utf16(const char16_t* pos,
                                         const char16_t* end) noexcept {
    const char32_t u0 = *pos;
    if (u0 < 0xD800 || u0 > 0xDFFF) return {u0, 1, true};
    if (u0 < 0xDC00 && pos + 1 != end) {
        const char32_t u1 = pos[1];
        if (u1 >= 0xDC00 && u1 <= 0xDFFF) {
            return {0x10000 + (((u0 - 0xD800) << 10) | (u1 - 0xDC00)), 2, true};
        }
    }
    return {replacement_character, 1, false};
}

enum class on_decode_error : bool { fail, replace };

// Templated on the output string so the Windows to_wstring shares the
// UTF-16 encoder (wchar_t is a UTF-16 code unit there).
template <class String16>
std::expected<String16, decode_error> u8_to_utf16(std::u8string_view text,
                                                  on_decode_error mode) {
    using unit = typename String16::value_type;
    String16 out;
    out.reserve(text.size());
    const char8_t* pos = text.data();
    const char8_t* const end = pos + text.size();
    while (pos != end) {
        const auto r = decode_one(pos, end);
        if (!r.ok && mode == on_decode_error::fail) {
            return std::unexpected(
                decode_error{static_cast<std::size_t>(pos - text.data())});
        }
        if (r.code_point < 0x10000) {
            out.push_back(static_cast<unit>(r.code_point));
        } else {
            const char32_t v = r.code_point - 0x10000;
            out.push_back(static_cast<unit>(0xD800 | (v >> 10)));
            out.push_back(static_cast<unit>(0xDC00 | (v & 0x3FF)));
        }
        pos += r.size;
    }
    return out;
}

inline std::expected<std::u8string, decode_error> utf16_to_u8(
    std::u16string_view text, on_decode_error mode) {
    std::u8string out;
    out.reserve(text.size());
    const char16_t* pos = text.data();
    const char16_t* const end = pos + text.size();
    while (pos != end) {
        const auto r = decode_one_utf16(pos, end);
        if (!r.ok && mode == on_decode_error::fail) {
            return std::unexpected(
                decode_error{static_cast<std::size_t>(pos - text.data())});
        }
        out += encode(r.code_point).view();
        pos += r.size;
    }
    return out;
}

}  // namespace detail

// ---- UTF-8 -> UTF-16 / UTF-32 -----------------------------------------------

[[nodiscard]] inline std::expected<std::u16string, decode_error> to_u16string(
    std::u8string_view text) {
    return detail::u8_to_utf16<std::u16string>(text,
                                               detail::on_decode_error::fail);
}

[[nodiscard]] inline std::u16string to_u16string_lossy(
    std::u8string_view text) {
    return *detail::u8_to_utf16<std::u16string>(
        text, detail::on_decode_error::replace);
}

[[nodiscard]] inline std::expected<std::u32string, decode_error> to_u32string(
    std::u8string_view text) {
    std::u32string out;
    out.reserve(text.size());
    const char8_t* pos = text.data();
    const char8_t* const end = pos + text.size();
    while (pos != end) {
        const auto r = detail::decode_one(pos, end);
        if (!r.ok) {
            return std::unexpected(
                decode_error{static_cast<std::size_t>(pos - text.data())});
        }
        out.push_back(r.code_point);
        pos += r.size;
    }
    return out;
}

[[nodiscard]] inline std::u32string to_u32string_lossy(
    std::u8string_view text) {
    std::u32string out;
    out.reserve(text.size());
    for (const char32_t cp : code_points(text)) out.push_back(cp);
    return out;
}

// ---- UTF-16 / UTF-32 -> UTF-8 -----------------------------------------------

[[nodiscard]] inline std::expected<std::u8string, decode_error> to_u8string(
    std::u16string_view text) {
    return detail::utf16_to_u8(text, detail::on_decode_error::fail);
}

[[nodiscard]] inline std::u8string to_u8string_lossy(std::u16string_view text) {
    return *detail::utf16_to_u8(text, detail::on_decode_error::replace);
}

[[nodiscard]] inline std::expected<std::u8string, decode_error> to_u8string(
    std::u32string_view text) {
    std::u8string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (!is_scalar_value(text[i])) return std::unexpected(decode_error{i});
        out += encode(text[i]).view();
    }
    return out;
}

[[nodiscard]] inline std::u8string to_u8string_lossy(std::u32string_view text) {
    std::u8string out;
    out.reserve(text.size());
    for (const char32_t cp : text) out += encode(cp).view();
    return out;
}

// ---- the Windows API boundary -------------------------------------------------

#if defined(_WIN32)

// On Windows wchar_t is a UTF-16 code unit; view either 16-bit type as the
// other at zero cost (same size and representation — the aliasing caveat of
// as_u8() applies).
static_assert(sizeof(wchar_t) == sizeof(char16_t));

[[nodiscard]] inline std::u16string_view as_u16(
    std::wstring_view text) noexcept {
    return {reinterpret_cast<const char16_t*>(text.data()), text.size()};
}

[[nodiscard]] inline std::wstring_view as_wide(
    std::u16string_view text) noexcept {
    return {reinterpret_cast<const wchar_t*>(text.data()), text.size()};
}

[[nodiscard]] inline std::expected<std::wstring, decode_error> to_wstring(
    std::u8string_view text) {
    return detail::u8_to_utf16<std::wstring>(text,
                                             detail::on_decode_error::fail);
}

[[nodiscard]] inline std::wstring to_wstring_lossy(std::u8string_view text) {
    return *detail::u8_to_utf16<std::wstring>(text,
                                              detail::on_decode_error::replace);
}

[[nodiscard]] inline std::expected<std::u8string, decode_error> to_u8string(
    std::wstring_view text) {
    return to_u8string(as_u16(text));
}

[[nodiscard]] inline std::u8string to_u8string_lossy(std::wstring_view text) {
    return to_u8string_lossy(as_u16(text));
}

#endif  // _WIN32

}  // namespace u8io
