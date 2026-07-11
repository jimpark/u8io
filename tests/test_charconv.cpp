#include <string_view>
#include <system_error>

#include "u8io/charconv.hpp"
#include "u8test.hpp"

using namespace std::literals;

TEST_CASE("to_chars into a char8_t buffer") {
    char8_t buf[32];
    const auto r = u8io::to_chars(buf, buf + sizeof buf, 42);
    CHECK(r.ec == std::errc{});
    CHECK(std::u8string_view(buf, r.ptr) == u8"42");

    const auto hex = u8io::to_chars(buf, buf + sizeof buf, 255, 16);
    CHECK(hex.ec == std::errc{});
    CHECK(std::u8string_view(buf, hex.ptr) == u8"ff");

    const auto fp = u8io::to_chars(buf, buf + sizeof buf, 2.5);
    CHECK(fp.ec == std::errc{});
    CHECK(std::u8string_view(buf, fp.ptr) == u8"2.5");
}

TEST_CASE("to_chars reports overflow like the standard") {
    char8_t tiny[2];
    const auto r = u8io::to_chars(tiny, tiny + sizeof tiny, 12345);
    CHECK(r.ec == std::errc::value_too_large);
}

TEST_CASE("from_chars with pointer pair") {
    const std::u8string_view text = u8"1234xyz";
    int value = 0;
    const auto r =
        u8io::from_chars(text.data(), text.data() + text.size(), value);
    CHECK(r.ec == std::errc{});
    CHECK(value == 1234);
    CHECK(r.ptr == text.data() + 4);
}

TEST_CASE("from_chars from a u8string_view") {
    int value = 0;
    const auto r = u8io::from_chars(u8"-17 apples"sv, value);
    CHECK(r.ec == std::errc{});
    CHECK(value == -17);

    unsigned hex = 0;
    const auto rh = u8io::from_chars(u8"deadBEEF"sv, hex, 16);
    CHECK(rh.ec == std::errc{});
    CHECK(hex == 0xDEADBEEFu);

    // libc++ before 20 declares floating-point from_chars deleted (and
    // leaves __cpp_lib_to_chars undefined); the integer overloads above
    // work everywhere.
#ifdef __cpp_lib_to_chars
    double d = 0;
    const auto rd = u8io::from_chars(u8"6.25e-2"sv, d);
    CHECK(rd.ec == std::errc{});
    CHECK(d == 0.0625);
#endif
}

TEST_CASE("from_chars failure modes") {
    int value = 0;
    CHECK(u8io::from_chars(u8"pas un nombre"sv, value).ec ==
          std::errc::invalid_argument);
    CHECK(u8io::from_chars(u8"99999999999999999999"sv, value).ec ==
          std::errc::result_out_of_range);
}

TEST_CASE("parse succeeds on whole-string numbers") {
    CHECK(u8io::parse<int>(u8"42"sv).value() == 42);
    CHECK(u8io::parse<int>(u8"-17"sv).value() == -17);
    CHECK(u8io::parse<unsigned>(u8"ff"sv, 16).value() == 255u);
#ifdef __cpp_lib_to_chars
    CHECK(u8io::parse<double>(u8"6.25e-2"sv).value() == 0.0625);
    CHECK(u8io::parse<double>(u8"2.5"sv, std::chars_format::fixed).value() ==
          2.5);
#endif
}

TEST_CASE("parse rejects non-numbers, junk, and overflow") {
    const auto bad = u8io::parse<int>(u8"trois"sv);
    CHECK(!bad.has_value());
    CHECK(bad.error().full_message() == u8"expected a number, got \"trois\"");

    const auto junk = u8io::parse<int>(u8"42 pommes"sv);
    CHECK(!junk.has_value());
    CHECK(junk.error().full_message() ==
          u8"trailing characters after number in \"42 pommes\"");

    // As strict as from_chars: no whitespace skipping, empty is an error.
    CHECK(!u8io::parse<int>(u8" 42"sv).has_value());
    CHECK(!u8io::parse<int>(u8""sv).has_value());
    CHECK(!u8io::parse<unsigned>(u8"-1"sv).has_value());

    const auto big = u8io::parse<int>(u8"99999999999999999999"sv);
    CHECK(!big.has_value());
    CHECK(big.error().full_message().starts_with(u8"number out of range"));
}

TEST_CASE("parse errors point at the caller, and format through expected") {
    const auto bad = u8io::parse<int>(u8"nope"sv);
    CHECK(std::string_view(bad.error().location().file_name())
              .ends_with("test_charconv.cpp"));
    CHECK(u8io::format(u8"{}", bad) ==
          u8"unexpected(expected a number, got \"nope\")");
}
