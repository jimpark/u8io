#pragma once

// Zero-cost views between the char8_t world and the char world, copying
// conversions for when ownership must change, and an opt-in UTF-8 validator
// for data arriving from outside the u8 world.
//
// Policy (trust by convention): u8string/u8string_view contents are presumed
// valid UTF-8. as_u8()/to_u8string() do not validate; call validate() at
// boundaries where the bytes come from an untrusted source.

#include <cstddef>
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

// Checks well-formedness per the Unicode 15 table: rejects overlong
// encodings, surrogates (U+D800..U+DFFF), values above U+10FFFF, stray
// continuation bytes, and truncated sequences.
[[nodiscard]] inline std::expected<std::u8string_view, decode_error> validate(
    std::string_view bytes) noexcept {
    const std::size_t n = bytes.size();
    std::size_t i = 0;
    while (i < n) {
        const auto b0 = static_cast<unsigned char>(bytes[i]);
        if (b0 < 0x80) {
            ++i;
            continue;
        }
        std::size_t len = 0;
        unsigned char lo = 0x80;
        unsigned char hi = 0xBF;
        if (b0 >= 0xC2 && b0 <= 0xDF) {
            len = 2;
        } else if (b0 == 0xE0) {
            len = 3;
            lo = 0xA0;  // reject overlong 3-byte forms
        } else if ((b0 >= 0xE1 && b0 <= 0xEC) || b0 == 0xEE || b0 == 0xEF) {
            len = 3;
        } else if (b0 == 0xED) {
            len = 3;
            hi = 0x9F;  // reject surrogates
        } else if (b0 == 0xF0) {
            len = 4;
            lo = 0x90;  // reject overlong 4-byte forms
        } else if (b0 >= 0xF1 && b0 <= 0xF3) {
            len = 4;
        } else if (b0 == 0xF4) {
            len = 4;
            hi = 0x8F;  // reject values above U+10FFFF
        } else {
            return std::unexpected(decode_error{i});
        }
        if (n - i < len) return std::unexpected(decode_error{i});
        const auto b1 = static_cast<unsigned char>(bytes[i + 1]);
        if (b1 < lo || b1 > hi) return std::unexpected(decode_error{i});
        for (std::size_t k = 2; k < len; ++k) {
            const auto bk = static_cast<unsigned char>(bytes[i + k]);
            if (bk < 0x80 || bk > 0xBF) return std::unexpected(decode_error{i});
        }
        i += len;
    }
    return as_u8(bytes);
}

[[nodiscard]] inline std::expected<std::u8string_view, decode_error> validate(
    std::u8string_view text) noexcept {
    return validate(as_char(text));
}

}  // namespace u8io
