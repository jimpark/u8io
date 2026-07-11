#pragma once

// ASCII-only classification and case conversion for char8_t.
//
// The <cctype> functions are unusable for char8_t: they take int, are
// locale-sensitive, and have undefined behavior for values that don't fit
// in unsigned char. These replacements are constexpr, locale-free, and
// well-defined for every char8_t value: bytes >= 0x80 (UTF-8 continuation
// and lead bytes) are never in any class and case-convert to themselves.
//
// This is deliberately ASCII-only — the right tool for parsing protocol
// and config text. Unicode-aware classification and case mapping depend on
// large character databases and are ICU's job.

#include <string>
#include <string_view>

#include "config.hpp"

namespace u8io::ascii {

[[nodiscard]] constexpr bool is_ascii(char8_t c) noexcept { return c < 0x80; }

[[nodiscard]] constexpr bool is_digit(char8_t c) noexcept {
    return c >= u8'0' && c <= u8'9';
}

[[nodiscard]] constexpr bool is_upper(char8_t c) noexcept {
    return c >= u8'A' && c <= u8'Z';
}

[[nodiscard]] constexpr bool is_lower(char8_t c) noexcept {
    return c >= u8'a' && c <= u8'z';
}

[[nodiscard]] constexpr bool is_alpha(char8_t c) noexcept {
    return is_upper(c) || is_lower(c);
}

[[nodiscard]] constexpr bool is_alnum(char8_t c) noexcept {
    return is_alpha(c) || is_digit(c);
}

[[nodiscard]] constexpr bool is_xdigit(char8_t c) noexcept {
    return is_digit(c) || (c >= u8'a' && c <= u8'f') ||
           (c >= u8'A' && c <= u8'F');
}

// ' ', '\t', '\n', '\v', '\f', '\r' — same set as std::isspace in the "C"
// locale. Does not include Unicode whitespace such as U+00A0.
[[nodiscard]] constexpr bool is_space(char8_t c) noexcept {
    return c == u8' ' || (c >= u8'\t' && c <= u8'\r');
}

[[nodiscard]] constexpr bool is_blank(char8_t c) noexcept {
    return c == u8' ' || c == u8'\t';
}

[[nodiscard]] constexpr bool is_cntrl(char8_t c) noexcept {
    return c < 0x20 || c == 0x7F;
}

[[nodiscard]] constexpr bool is_print(char8_t c) noexcept {
    return c >= 0x20 && c < 0x7F;
}

[[nodiscard]] constexpr bool is_graph(char8_t c) noexcept {
    return c > 0x20 && c < 0x7F;
}

[[nodiscard]] constexpr bool is_punct(char8_t c) noexcept {
    return is_graph(c) && !is_alnum(c);
}

[[nodiscard]] constexpr char8_t to_lower(char8_t c) noexcept {
    return is_upper(c) ? static_cast<char8_t>(c + 0x20) : c;
}

[[nodiscard]] constexpr char8_t to_upper(char8_t c) noexcept {
    return is_lower(c) ? static_cast<char8_t>(c - 0x20) : c;
}

// Whole-string case conversion; non-ASCII bytes pass through untouched, so
// valid UTF-8 stays valid.
[[nodiscard]] constexpr std::u8string to_lower(std::u8string_view text) {
    std::u8string out(text);
    for (char8_t& c : out) c = to_lower(c);
    return out;
}

[[nodiscard]] constexpr std::u8string to_upper(std::u8string_view text) {
    std::u8string out(text);
    for (char8_t& c : out) c = to_upper(c);
    return out;
}

}  // namespace u8io::ascii
