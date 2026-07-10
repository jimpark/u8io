#include <string>
#include <string_view>

#include "u8io/cast.hpp"
#include "u8test.hpp"

using namespace std::literals;

TEST_CASE("as_char / as_u8 round-trip") {
    const std::u8string_view u8text = u8"héllo wörld ✓";
    const std::string_view chars = u8io::as_char(u8text);
    CHECK(chars.size() == u8text.size());
    CHECK(u8io::as_u8(chars) == u8text);
}

TEST_CASE("as_char of empty view") {
    CHECK(u8io::as_char(std::u8string_view{}).empty());
    CHECK(u8io::as_u8(std::string_view{}).empty());
}

TEST_CASE("copying conversions") {
    CHECK(u8io::to_string(u8"π ≈ 3.14159") == "π ≈ 3.14159");
    CHECK(u8io::to_u8string("π ≈ 3.14159") == u8"π ≈ 3.14159");
}

TEST_CASE("validate accepts well-formed UTF-8") {
    CHECK(u8io::validate("plain ascii"sv).has_value());
    CHECK(u8io::validate("two-byte: é, three-byte: ✓, four-byte: 🎉"sv).has_value());
    CHECK(u8io::validate(""sv).has_value());
    CHECK(u8io::validate(u8"écrit en UTF-8"sv).has_value());
    // Highest scalar value U+10FFFF: F4 8F BF BF.
    CHECK(u8io::validate("\xF4\x8F\xBF\xBF"sv).has_value());
    // NUL is a valid code point; embedded NULs must pass.
    CHECK(u8io::validate("a\0b"sv).has_value());
}

TEST_CASE("validate rejects malformed UTF-8 with the right offset") {
    // Stray continuation byte.
    auto r1 = u8io::validate("ab\x80"sv);
    CHECK(!r1.has_value());
    CHECK(r1.error().offset == 2);

    // Overlong encoding of '/' (C0 AF).
    auto r2 = u8io::validate("\xC0\xAF"sv);
    CHECK(!r2.has_value());
    CHECK(r2.error().offset == 0);

    // Overlong 3-byte form (E0 80 80).
    CHECK(!u8io::validate("\xE0\x80\x80"sv).has_value());

    // CESU-8 style surrogate (ED A0 80 = U+D800).
    CHECK(!u8io::validate("\xED\xA0\x80"sv).has_value());

    // Above U+10FFFF (F4 90 80 80).
    CHECK(!u8io::validate("\xF4\x90\x80\x80"sv).has_value());

    // Truncated sequence at end of input.
    auto r3 = u8io::validate("ok\xE2\x9C"sv);
    CHECK(!r3.has_value());
    CHECK(r3.error().offset == 2);

    // Second byte out of range for the lead byte.
    CHECK(!u8io::validate("\xC2\x20"sv).has_value());
}
