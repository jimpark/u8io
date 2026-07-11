#include <expected>
#include <string>
#include <system_error>
#include <utility>

#include "u8io/error.hpp"
#include "u8test.hpp"

namespace {

std::expected<int, u8io::error> parse_positive(int raw) {
    if (raw < 0) return u8io::fail(u8"négative value: {}", raw);
    return raw;
}

}  // namespace

TEST_CASE("fail produces an unexpected error with a formatted message") {
    const auto result = parse_positive(-3);
    CHECK(!result.has_value());
    CHECK(result.error().message() == u8"négative value: -3");
    CHECK(std::string_view(result.error().location().file_name())
              .ends_with("test_error.cpp"));
}

TEST_CASE("the value path is unaffected") {
    const auto result = parse_positive(7);
    CHECK(result.has_value());
    CHECK(*result == 7);
}

TEST_CASE("u8unexpected wraps a runtime message") {
    const std::expected<int, u8io::error> result =
        u8io::u8unexpected(std::u8string(u8"bôom"));
    CHECK(!result.has_value());
    CHECK(result.error().message() == u8"bôom");
}

TEST_CASE("cause chains render in full_message") {
    const std::expected<int, u8io::error> root =
        u8io::fail(u8"disk unréadable");
    const std::expected<int, u8io::error> wrapped =
        u8io::fail_with(root.error(), u8"config lost");
    CHECK(wrapped.error().full_message() == u8"config lost: disk unréadable");
    CHECK(wrapped.error().cause() != nullptr);
    CHECK(wrapped.error().cause()->message() == u8"disk unréadable");
    CHECK(wrapped.error().what() == "config lost: disk unréadable");
}

TEST_CASE("error_code errors carry category, value, and message") {
    const auto code = std::make_error_code(std::errc::invalid_argument);
    const u8io::error err(code, u8"parsing config");
    CHECK(err.code() == code);
    const std::string what = err.what();
    CHECK(what.starts_with("parsing config [generic:"));
}

TEST_CASE("errors are formattable") {
    const u8io::error err(std::u8string(u8"première"));
    const u8io::error chained =
        u8io::error(std::u8string(u8"secônde")).caused_by(err);
    CHECK(u8io::format(u8"{}", chained) == u8"secônde: première");
}

TEST_CASE("expected values format bare, errors as unexpected(...)") {
    const std::expected<int, u8io::error> good = 42;
    const std::expected<int, u8io::error> bad = u8io::fail(u8"bäd");
    CHECK(u8io::format(u8"v={}", good) == u8"v=42");
    CHECK(u8io::format(u8"v={}", bad) == u8"v=unexpected(bäd)");
}

TEST_CASE("expected with u8string payloads formats through the mapper") {
    const std::expected<std::u8string, std::u8string> good{u8"välue"};
    const std::expected<std::u8string, std::u8string> bad{std::unexpect,
                                                          u8"öops"};
    CHECK(u8io::format(u8"{}", good) == u8"välue");
    CHECK(u8io::format(u8"{}", bad) == u8"unexpected(öops)");
}

TEST_CASE("expected<void> formats as empty on success") {
    const std::expected<void, u8io::error> ok;
    const std::expected<void, u8io::error> bad = u8io::fail(u8"nö");
    CHECK(u8io::format(u8"[{}]", ok) == u8"[]");
    CHECK(u8io::format(u8"[{}]", bad) == u8"[unexpected(nö)]");
}

TEST_CASE("from_edge adapts string errors") {
    const std::expected<int, std::string> edge{std::unexpect,
                                               "third-party böom"};
    const auto adapted = u8io::from_edge(edge);
    CHECK(!adapted.has_value());
    CHECK(adapted.error().message() == u8"third-party böom");

    const std::expected<int, std::string> fine{5};
    CHECK(u8io::from_edge(fine).value() == 5);
}

TEST_CASE("from_edge adapts error_code errors and void expecteds") {
    const std::expected<void, std::error_code> edge{
        std::unexpect, std::make_error_code(std::errc::io_error)};
    const auto adapted = u8io::from_edge(edge);
    CHECK(!adapted.has_value());
    CHECK(adapted.error().code() == std::make_error_code(std::errc::io_error));

    const std::expected<void, std::error_code> fine;
    CHECK(u8io::from_edge(fine).has_value());
}

TEST_CASE("errors stay cheap to copy through expected plumbing") {
    auto step1 = []() -> std::expected<int, u8io::error> {
        return u8io::fail(u8"root çause");
    };
    auto step2 = [&]() -> std::expected<std::u8string, u8io::error> {
        return step1().and_then(
            [](int v) -> std::expected<std::u8string, u8io::error> {
                return u8io::format(u8"{}", v);
            });
    };
    const auto result = step2();
    CHECK(!result.has_value());
    CHECK(result.error().message() == u8"root çause");
}
