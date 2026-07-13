#include <string>
#include <string_view>

#include "u8io/transcode.hpp"
#include "u8test.hpp"

TEST_CASE("is_scalar_value excludes surrogates and out-of-range values") {
    CHECK(u8io::is_scalar_value(U'A'));
    CHECK(u8io::is_scalar_value(char32_t{0}));
    CHECK(u8io::is_scalar_value(char32_t{0xD7FF}));
    CHECK(!u8io::is_scalar_value(char32_t{0xD800}));
    CHECK(!u8io::is_scalar_value(char32_t{0xDFFF}));
    CHECK(u8io::is_scalar_value(char32_t{0xE000}));
    CHECK(u8io::is_scalar_value(char32_t{0x10FFFF}));
    CHECK(!u8io::is_scalar_value(char32_t{0x110000}));
}

TEST_CASE("encode produces all four UTF-8 lengths, at compile time too") {
    static_assert(u8io::encode(U'A').count == 1);
    static_assert(u8io::encode(U'😀').count == 4);
    CHECK(u8io::encode(U'A').view() == u8"A");
    CHECK(u8io::encode(U'é').view() == u8"é");    // 2 bytes
    CHECK(u8io::encode(U'€').view() == u8"€");    // 3 bytes
    CHECK(u8io::encode(U'😀').view() == u8"😀");  // 4 bytes
}

TEST_CASE("encode replaces non-scalar values with U+FFFD") {
    CHECK(u8io::encode(char32_t{0xD800}).view() == u8"�");
    CHECK(u8io::encode(char32_t{0xDFFF}).view() == u8"�");
    CHECK(u8io::encode(char32_t{0x110000}).view() == u8"�");
}

TEST_CASE("append builds UTF-8 from code points") {
    std::u8string out;
    for (const char32_t cp : u8io::code_points(u8"héllo 🌍")) {
        u8io::append(out, cp);
    }
    CHECK(out == u8"héllo 🌍");
    out += u8io::encode(U'!');  // encoded_code_point converts to a view
    CHECK(out == u8"héllo 🌍!");
}

TEST_CASE("UTF-8 <-> UTF-16 round-trip") {
    const std::u8string_view text = u8"café 日本語 🎉";
    const auto utf16 = u8io::to_u16string(text);
    CHECK(utf16.has_value());
    CHECK(*utf16 == u"café 日本語 🎉");
    const auto back = u8io::to_u8string(*utf16);
    CHECK(back.has_value());
    CHECK(*back == text);
}

TEST_CASE("UTF-8 <-> UTF-32 round-trip") {
    const std::u8string_view text = u8"café 日本語 🎉";
    const auto utf32 = u8io::to_u32string(text);
    CHECK(utf32.has_value());
    CHECK(*utf32 == U"café 日本語 🎉");
    const auto back = u8io::to_u8string(*utf32);
    CHECK(back.has_value());
    CHECK(*back == text);
}

TEST_CASE("to_u16string reports the byte offset of ill-formed UTF-8") {
    const auto bad = u8io::to_u16string(u8io::as_u8("ok\xC0\xAF!"));
    CHECK(!bad.has_value());
    CHECK(bad.error().offset == 2);
    // Each bogus byte becomes its own replacement character.
    CHECK(u8io::to_u16string_lossy(u8io::as_u8("ok\xC0\xAF!")) == u"ok��!");
    CHECK(!u8io::to_u32string(u8io::as_u8("\x80")).has_value());
    CHECK(u8io::to_u32string_lossy(u8io::as_u8("\x80")) == U"�");
}

TEST_CASE("to_u8string rejects unpaired surrogates; lossy replaces them") {
    std::u16string bad;
    bad += u'a';
    bad += char16_t{0xD800};  // high surrogate with no low surrogate
    bad += u'b';
    const auto strict = u8io::to_u8string(bad);
    CHECK(!strict.has_value());
    CHECK(strict.error().offset == 1);
    CHECK(u8io::to_u8string_lossy(bad) == u8"a�b");

    const std::u16string lone_low(1, char16_t{0xDC00});
    CHECK(!u8io::to_u8string(lone_low).has_value());
    CHECK(u8io::to_u8string_lossy(lone_low) == u8"�");
}

TEST_CASE("to_u8string from UTF-32 validates scalar values") {
    const auto ok = u8io::to_u8string(std::u32string_view(U"añ😀"));
    CHECK(ok.has_value());
    CHECK(*ok == u8"añ😀");

    std::u32string bad = U"x";
    bad += char32_t{0x110000};
    const auto strict = u8io::to_u8string(bad);
    CHECK(!strict.has_value());
    CHECK(strict.error().offset == 1);
    CHECK(u8io::to_u8string_lossy(bad) == u8"x�");
}

TEST_CASE("empty strings transcode to empty strings") {
    CHECK(u8io::to_u16string(u8"")->empty());
    CHECK(u8io::to_u32string(u8"")->empty());
    CHECK(u8io::to_u8string(u"")->empty());
    CHECK(u8io::to_u8string(U"")->empty());
    CHECK(u8io::to_u16string_lossy(u8"").empty());
    CHECK(u8io::to_u8string_lossy(u"").empty());
}

#if defined(_WIN32)

TEST_CASE("wstring conversions cover the Windows API boundary") {
    const auto wide = u8io::to_wstring(u8"Zürich 🏔");
    CHECK(wide.has_value());
    CHECK(*wide == L"Zürich 🏔");
    const auto back = u8io::to_u8string(*wide);
    CHECK(back.has_value());
    CHECK(*back == u8"Zürich 🏔");

    CHECK(u8io::as_wide(u"wide") == L"wide");
    CHECK(u8io::as_u16(L"wide") == u"wide");

    CHECK(!u8io::to_wstring(u8io::as_u8("\xFF")).has_value());
    CHECK(u8io::to_wstring_lossy(u8io::as_u8("\xFF")) == L"�");
    CHECK(u8io::to_u8string_lossy(L"ok") == u8"ok");
}

#endif  // _WIN32
