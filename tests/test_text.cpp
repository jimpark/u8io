#include <algorithm>
#include <ranges>
#include <string_view>
#include <vector>

#include "u8io/text.hpp"
#include "u8test.hpp"

using namespace std::literals;

namespace {

std::vector<char32_t> decode_all(std::u8string_view text) {
    std::vector<char32_t> out;
    for (const char32_t cp : u8io::code_points(text)) out.push_back(cp);
    return out;
}

}  // namespace

TEST_CASE("code_points decodes 1- to 4-byte sequences") {
    const auto cps = decode_all(u8"aé✓🎉");
    CHECK(cps == (std::vector<char32_t>{U'a', U'é', U'✓', U'🎉'}));
    CHECK(cps == (std::vector<char32_t>{0x61, 0xE9, 0x2713, 0x1F389}));
}

TEST_CASE("code_points of empty and boundary values") {
    CHECK(decode_all(u8"").empty());
    CHECK(u8io::code_points(u8"").empty());  // view_interface
    // U+0000, U+007F, U+0080, U+07FF, U+0800, U+FFFF, U+10000, U+10FFFF.
    const auto cps =
        decode_all(u8"\0\u007F\u0080\u07FF\u0800\uFFFF\U00010000\U0010FFFF"sv);
    CHECK(cps == (std::vector<char32_t>{0x0, 0x7F, 0x80, 0x7FF, 0x800, 0xFFFF,
                                        0x10000, 0x10FFFF}));
}

TEST_CASE("code_points substitutes U+FFFD for ill-formed sequences") {
    // Stray continuation byte between valid characters.
    const auto stray = decode_all(u8io::as_u8("a\x80z"sv));
    CHECK(stray ==
          (std::vector<char32_t>{U'a', u8io::replacement_character, U'z'}));

    // Truncated 3-byte sequence: its maximal subpart (E2 9C) is consumed as
    // one U+FFFD, and decoding resynchronizes on the following 'b'.
    const auto truncated =
        decode_all(u8io::as_u8("\xE2\x9C"
                               "b"sv));
    CHECK(truncated ==
          (std::vector<char32_t>{u8io::replacement_character, U'b'}));

    // Surrogate ED A0 80: ED consumed alone (A0 is out of range for ED),
    // then A0 and 80 are stray continuations — three replacements.
    const auto surrogate = decode_all(u8io::as_u8("\xED\xA0\x80"sv));
    CHECK(surrogate.size() == 3);
    CHECK(std::ranges::all_of(surrogate, [](char32_t cp) {
        return cp == u8io::replacement_character;
    }));

    // Truncation at end of input.
    const auto cut = decode_all(u8io::as_u8("ok\xF0\x9F"sv));
    CHECK(cut ==
          (std::vector<char32_t>{U'o', U'k', u8io::replacement_character}));
}

TEST_CASE("code_points works with ranges algorithms and adaptors") {
    const std::u8string_view text = u8"héllo wörld";
    CHECK(std::ranges::distance(u8io::code_points(text)) == 11);
    CHECK(std::ranges::count(u8io::code_points(text), U'l') == 3);
    auto first3 = u8io::code_points(text) | std::views::take(3);
    CHECK(std::ranges::equal(first3, std::vector<char32_t>{U'h', U'é', U'l'}));
}

TEST_CASE("code_point_iterator exposes byte positions via base()") {
    const std::u8string_view text = u8"aé✓";
    auto it = u8io::code_points(text).begin();
    CHECK(it.base() == text.data());
    ++it;  // past 'a'
    CHECK(it.base() == text.data() + 1);
    ++it;  // past 'é' (2 bytes)
    CHECK(it.base() == text.data() + 3);
    ++it;  // past '✓' (3 bytes)
    CHECK(it == u8io::code_points(text).end());
}

TEST_CASE("code_points_view models the range concepts") {
    static_assert(std::ranges::forward_range<u8io::code_points_view>);
    static_assert(std::ranges::view<u8io::code_points_view>);
    static_assert(std::ranges::borrowed_range<u8io::code_points_view>);
    CHECK(true);
}

TEST_CASE("code_points decoding is constexpr") {
    constexpr std::u8string_view text = u8"é";
    static_assert(*u8io::code_points(text).begin() == U'é');
    static_assert(std::ranges::distance(
                      u8io::code_points(std::u8string_view(u8"a✓"))) == 2);
    CHECK(true);
}
