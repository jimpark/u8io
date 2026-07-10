#include <format>
#include <iterator>
#include <string>
#include <string_view>

#include "u8io/format.hpp"
#include "u8test.hpp"

TEST_CASE("basic formatting with ordinary arguments") {
    CHECK(u8io::format(u8"hello {}", 42) == u8"hello 42");
    CHECK(u8io::format(u8"{} + {} = {}", 1, 2, 3) == u8"1 + 2 = 3");
    CHECK(u8io::format(u8"{:.2f}", 3.14159) == u8"3.14");
    CHECK(u8io::format(u8"{:>6}", 7) == u8"     7");
    CHECK(u8io::format(u8"{1}{0}", u8"b", u8"a") == u8"ab");
    CHECK(u8io::format(u8"escaped {{}}") == u8"escaped {}");
}

TEST_CASE("u8 arguments in all their forms") {
    const std::u8string owned = u8"öwned";
    const std::u8string_view view = u8"vïew";
    const char8_t* pointer = u8"pöinter";
    CHECK(u8io::format(u8"{} {} {} {}", owned, view, pointer, u8"lïteral") ==
          u8"öwned vïew pöinter lïteral");
}

TEST_CASE("u8string arguments honor string format specs") {
    const std::u8string s = u8"ab";
    CHECK(u8io::format(u8"{:>4}", s) == u8"  ab");
    CHECK(u8io::format(u8"{:.1}", s) == u8"a");
}

TEST_CASE("char-world arguments still work") {
    const std::string narrow = "narrow";
    CHECK(u8io::format(u8"{} {} {}", narrow, "literal", std::string_view("view")) ==
          u8"narrow literal view");
}

TEST_CASE("non-ASCII format strings") {
    CHECK(u8io::format(u8"prix: {}€", 5) == u8"prix: 5€");
    CHECK(u8io::format(u8"日本語 {} テキスト", u8"の") == u8"日本語 の テキスト");
}

TEST_CASE("a lone char8_t renders as one code unit") {
    CHECK(u8io::format(u8"{}{}", char8_t{u8'x'}, char8_t{u8'y'}) == u8"xy");
}

TEST_CASE("runtime format strings") {
    const std::u8string fmt = u8"dynamic: {}";
    CHECK(u8io::format(u8io::runtime_format(fmt), 1) == u8"dynamic: 1");
    CHECK_THROWS_AS(u8io::format(u8io::runtime_format(u8"bad {"), 1),
                    std::format_error);
    CHECK_THROWS_AS(u8io::format(u8io::runtime_format(u8"{} {}"), 1),
                    std::format_error);
}

TEST_CASE("format_to appends to a u8string") {
    std::u8string out = u8"log: ";
    u8io::format_to(std::back_inserter(out), u8"x={} y={}", 3, 4);
    CHECK(out == u8"log: x=3 y=4");
}

TEST_CASE("formatted_size counts UTF-8 code units") {
    CHECK(u8io::formatted_size(u8"{}", 12345) == 5);
    // é is two code units.
    CHECK(u8io::formatted_size(u8"é{}", u8"é") == 4);
}

TEST_CASE("format string location is captured") {
    const auto fmt = u8io::u8format_string<>(u8"x");
    CHECK(std::string_view(fmt.location().file_name()).ends_with("test_format.cpp"));
}
