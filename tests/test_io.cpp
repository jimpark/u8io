#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "u8io/io.hpp"
#include "u8test.hpp"

namespace {

std::filesystem::path temp_file(std::u8string_view name) {
    return std::filesystem::temp_directory_path() / std::filesystem::path(name);
}

}  // namespace

TEST_CASE("std::filesystem::path formats directly, including non-ASCII") {
    const std::filesystem::path p =
        std::filesystem::path(u8"docs") / std::filesystem::path(u8"日誌.txt");
    const std::u8string formatted = u8io::format(u8"log at {}", p);
    CHECK(formatted.starts_with(u8"log at docs"));
    CHECK(formatted.ends_with(u8"日誌.txt"));  // separator is
                                               // platform-specific
    // String specs apply to the mapped text.
    CHECK(u8io::format(u8"[{:>7}]", std::filesystem::path(u8"a.txt")) ==
          u8"[  a.txt]");
}

TEST_CASE("write_file / read_file round-trip with a non-ASCII filename") {
    const auto path = temp_file(u8"u8io_tßt_ファイル.txt");
    const std::u8string text = u8"line one\nligne dëux ✓\n";

    const auto written = u8io::write_file(path, text);
    CHECK(written.has_value());

    const auto read_back = u8io::read_file(path);
    CHECK(read_back.has_value());
    CHECK(*read_back == text);
    std::filesystem::remove(path);
}

TEST_CASE("read_file strips a UTF-8 BOM") {
    const auto path = temp_file(u8"u8io_bom.txt");
    {
        std::ofstream out(path, std::ios::binary);
        out << "\xEF\xBB\xBF" << "après le BOM";
    }
    const auto content = u8io::read_file(path);
    CHECK(content.has_value());
    CHECK(*content == u8"après le BOM");
    std::filesystem::remove(path);
}

TEST_CASE("read_file rejects invalid UTF-8 unless told otherwise") {
    const auto path = temp_file(u8"u8io_invalid.txt");
    {
        std::ofstream out(path, std::ios::binary);
        out << "ok\xC0\xAFnot ok";  // overlong '/'
    }
    const auto strict = u8io::read_file(path);
    CHECK(!strict.has_value());
    CHECK(strict.error().message().find(u8"byte offset 2") !=
          std::u8string::npos);

    const auto lax = u8io::read_file(path, /*validate_utf8=*/false);
    CHECK(lax.has_value());
    CHECK(lax->size() == 10);
    std::filesystem::remove(path);
}

TEST_CASE("read_file reports a missing file as an error") {
    const auto missing = u8io::read_file(temp_file(u8"u8io_absent_ø.txt"));
    CHECK(!missing.has_value());
    CHECK(missing.error().message().find(u8"cannot open") !=
          std::u8string::npos);
}

TEST_CASE("read_line reads lines, strips endings, and signals end of input") {
    const auto path = temp_file(u8"u8io_lines.txt");
    {
        std::ofstream out(path, std::ios::binary);
        out << "líne one\nline two\r\n\nlast";
    }
    std::FILE* f = std::fopen(path.string().c_str(), "rb");
    CHECK(f != nullptr);

    const auto l1 = u8io::read_line(f);
    CHECK(l1.has_value() && l1->has_value() && **l1 == u8"líne one");
    const auto l2 = u8io::read_line(f);
    CHECK(l2.has_value() && l2->has_value() && **l2 == u8"line two");
    const auto l3 = u8io::read_line(f);
    CHECK(l3.has_value() && l3->has_value() && (*l3)->empty());
    const auto l4 = u8io::read_line(f);
    CHECK(l4.has_value() && l4->has_value() && **l4 == u8"last");
    const auto eof = u8io::read_line(f);
    CHECK(eof.has_value() && !eof->has_value());
    const auto still_eof = u8io::read_line(f);
    CHECK(still_eof.has_value() && !still_eof->has_value());

    std::fclose(f);
    std::filesystem::remove(path);
}

TEST_CASE("read_line validates UTF-8 unless told otherwise") {
    const auto path = temp_file(u8"u8io_lines_bad.txt");
    {
        std::ofstream out(path, std::ios::binary);
        out << "\xFF\xFEoops\nnext\n";
    }
    std::FILE* f = std::fopen(path.string().c_str(), "rb");
    CHECK(f != nullptr);
    const auto strict = u8io::read_line(f);
    CHECK(!strict.has_value());
    CHECK(strict.error().message().find(u8"byte offset 0") !=
          std::u8string::npos);
    // The bad line was still consumed; reading continues on the next one.
    const auto next = u8io::read_line(f);
    CHECK(next.has_value() && next->has_value() && **next == u8"next");
    std::fclose(f);

    f = std::fopen(path.string().c_str(), "rb");
    CHECK(f != nullptr);
    const auto lax = u8io::read_line(f, /*validate_utf8=*/false);
    CHECK(lax.has_value() && lax->has_value() && (*lax)->size() == 6);
    std::fclose(f);
    std::filesystem::remove(path);
}

TEST_CASE("read_line handles empty input and embedded NULs") {
    const auto path = temp_file(u8"u8io_lines_empty.txt");
    {
        std::ofstream out(path, std::ios::binary);
    }
    std::FILE* f = std::fopen(path.string().c_str(), "rb");
    CHECK(f != nullptr);
    const auto eof = u8io::read_line(f);
    CHECK(eof.has_value() && !eof->has_value());
    std::fclose(f);

    {
        std::ofstream out(path, std::ios::binary);
        out.write("a\0b\n", 4);
    }
    f = std::fopen(path.string().c_str(), "rb");
    CHECK(f != nullptr);
    const auto line = u8io::read_line(f);
    CHECK(line.has_value() && line->has_value());
    CHECK((*line)->size() == 3);
    CHECK((**line)[1] == u8'\0');
    std::fclose(f);
    std::filesystem::remove(path);
}

TEST_CASE("write_to sends UTF-8 bytes to an ostream") {
    std::ostringstream os;
    u8io::write_to(os, u8"strëam");
    os << " and " << u8io::as_char(u8"möre");
    CHECK(os.str() == "strëam and möre");
}

TEST_CASE("filesystem paths are formattable") {
    const std::filesystem::path p =
        std::filesystem::path(u8"dossier/fichiér.txt");
    CHECK(u8io::format(u8"path: {}", p) == u8"path: dossier/fichiér.txt");
}
