#pragma once

// Zero-cost views between the char8_t world and the char world, copying
// conversions for when ownership must change, and an opt-in UTF-8 validator
// for data arriving from outside the u8 world.
//
// Policy (trust by convention): u8string/u8string_view contents are presumed
// valid UTF-8. as_u8()/to_u8string() do not validate; call validate() at
// boundaries where the bytes come from an untrusted source.

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include "config.hpp"

namespace u8io {

// View UTF-8 code units as chars. Always well-defined: char may alias any
// object type.
[[nodiscard]] inline std::string_view as_char(
    std::u8string_view text) noexcept {
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}

// View chars as UTF-8 code units without copying. The reverse aliasing
// direction is not formally blessed by the standard, but is well-defined on
// every implementation this library targets (char8_t has the same size,
// alignment, and representation as unsigned char). Use to_u8string() where
// a copy is acceptable and strict conformance matters.
[[nodiscard]] inline std::u8string_view as_u8(std::string_view bytes) noexcept {
    return {reinterpret_cast<const char8_t*>(bytes.data()), bytes.size()};
}

[[nodiscard]] inline std::string to_string(std::u8string_view text) {
    return std::string(as_char(text));
}

[[nodiscard]] inline std::u8string to_u8string(std::string_view bytes) {
    return std::u8string(as_u8(bytes));
}

struct decode_error {
    // Byte offset of the first invalid UTF-8 sequence.
    std::size_t offset = 0;
};

namespace detail {

struct decode_result {
    char32_t code_point;  // U+FFFD when !ok
    std::uint8_t size;    // bytes consumed, >= 1
    bool ok;
};

// Decodes one scalar value per the Unicode 15 well-formedness table:
// rejects overlong encodings, surrogates (U+D800..U+DFFF), values above
// U+10FFFF, stray continuation bytes, and truncated sequences. Requires
// pos != end. On malformed input, yields U+FFFD and consumes the maximal
// subpart of the ill-formed sequence (Unicode's recommended substitution),
// so a decoding loop always makes progress and never resynchronizes inside
// a following well-formed sequence.
constexpr decode_result decode_one(const char8_t* pos,
                                   const char8_t* end) noexcept {
    const auto b0 = static_cast<unsigned char>(*pos);
    if (b0 < 0x80) return {b0, 1, true};
    std::uint8_t len = 0;
    unsigned char lo = 0x80;
    unsigned char hi = 0xBF;
    char32_t cp = 0;
    if (b0 >= 0xC2 && b0 <= 0xDF) {
        len = 2;
        cp = b0 & 0x1F;
    } else if (b0 == 0xE0) {
        len = 3;
        lo = 0xA0;  // reject overlong 3-byte forms
    } else if ((b0 >= 0xE1 && b0 <= 0xEC) || b0 == 0xEE || b0 == 0xEF) {
        len = 3;
        cp = b0 & 0x0F;
    } else if (b0 == 0xED) {
        len = 3;
        hi = 0x9F;  // reject surrogates
        cp = 0x0D;
    } else if (b0 == 0xF0) {
        len = 4;
        lo = 0x90;  // reject overlong 4-byte forms
    } else if (b0 >= 0xF1 && b0 <= 0xF3) {
        len = 4;
        cp = b0 & 0x07;
    } else if (b0 == 0xF4) {
        len = 4;
        hi = 0x8F;  // reject values above U+10FFFF
        cp = 4;
    } else {
        return {U'�', 1, false};
    }
    std::uint8_t have = 1;
    for (; have < len && pos + have != end; ++have) {
        const auto b = static_cast<unsigned char>(pos[have]);
        if (b < (have == 1 ? lo : 0x80) || b > (have == 1 ? hi : 0xBF)) break;
        cp = (cp << 6) | (b & 0x3F);
    }
    if (have < len) return {U'�', have, false};
    return {cp, len, true};
}

}  // namespace detail

// Checks well-formedness; see detail::decode_one for the exact rules.
[[nodiscard]] inline std::expected<std::u8string_view, decode_error> validate(
    std::u8string_view text) noexcept {
    const char8_t* pos = text.data();
    const char8_t* const end = pos + text.size();
    while (pos != end) {
        const auto r = detail::decode_one(pos, end);
        if (!r.ok) {
            return std::unexpected(
                decode_error{static_cast<std::size_t>(pos - text.data())});
        }
        pos += r.size;
    }
    return text;
}

[[nodiscard]] inline std::expected<std::u8string_view, decode_error> validate(
    std::string_view bytes) noexcept {
    return validate(as_u8(bytes));
}

}  // namespace u8io
