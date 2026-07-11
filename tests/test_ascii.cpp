#include <string>

#include "u8io/ascii.hpp"
#include "u8test.hpp"

namespace ascii = u8io::ascii;

TEST_CASE("ascii classification basics") {
    CHECK(ascii::is_alpha(u8'a'));
    CHECK(ascii::is_alpha(u8'Z'));
    CHECK(!ascii::is_alpha(u8'0'));
    CHECK(ascii::is_digit(u8'7'));
    CHECK(!ascii::is_digit(u8'x'));
    CHECK(ascii::is_alnum(u8'q'));
    CHECK(ascii::is_alnum(u8'3'));
    CHECK(!ascii::is_alnum(u8'-'));
    CHECK(ascii::is_xdigit(u8'f'));
    CHECK(ascii::is_xdigit(u8'B'));
    CHECK(!ascii::is_xdigit(u8'g'));
    CHECK(ascii::is_space(u8' '));
    CHECK(ascii::is_space(u8'\t'));
    CHECK(ascii::is_space(u8'\n'));
    CHECK(!ascii::is_space(u8'_'));
    CHECK(ascii::is_blank(u8'\t'));
    CHECK(!ascii::is_blank(u8'\n'));
    CHECK(ascii::is_punct(u8'!'));
    CHECK(!ascii::is_punct(u8'a'));
    CHECK(ascii::is_cntrl(u8'\0'));
    CHECK(ascii::is_cntrl(char8_t{0x7F}));
    CHECK(ascii::is_print(u8' '));
    CHECK(!ascii::is_graph(u8' '));
    CHECK(ascii::is_graph(u8'#'));
}

TEST_CASE("ascii classes reject UTF-8 continuation and lead bytes") {
    // The first byte of u8"é" (0xC3) is alphabetic in Latin-1; it must not
    // be classified here.
    const char8_t e_acute_lead = u8"é"[0];
    CHECK(!ascii::is_ascii(e_acute_lead));
    CHECK(!ascii::is_alpha(e_acute_lead));
    CHECK(!ascii::is_print(e_acute_lead));
    CHECK(!ascii::is_space(char8_t{0xA0}));  // Latin-1 NBSP byte
    CHECK(ascii::is_ascii(u8'~'));
}

TEST_CASE("ascii case conversion of single code units") {
    CHECK(ascii::to_lower(u8'A') == u8'a');
    CHECK(ascii::to_lower(u8'a') == u8'a');
    CHECK(ascii::to_upper(u8'z') == u8'Z');
    CHECK(ascii::to_upper(u8'5') == u8'5');
    // Non-ASCII passes through untouched.
    CHECK(ascii::to_lower(char8_t{0xC3}) == char8_t{0xC3});
    CHECK(ascii::to_upper(char8_t{0xA9}) == char8_t{0xA9});
}

TEST_CASE("ascii case conversion of strings leaves UTF-8 intact") {
    CHECK(ascii::to_upper(u8"héllo, wörld!") == u8"HéLLO, WöRLD!");
    CHECK(ascii::to_lower(u8"Content-Type: TEXT/HTML") ==
          u8"content-type: text/html");
    CHECK(ascii::to_lower(u8"").empty());
}

TEST_CASE("ascii helpers are constexpr") {
    static_assert(ascii::is_alpha(u8'k'));
    static_assert(!ascii::is_digit(u8'k'));
    static_assert(ascii::to_upper(u8'k') == u8'K');
    CHECK(true);
}
