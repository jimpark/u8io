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
    CHECK(strict.error().message().find(u8"byte offset 2") != std::u8string::npos);

    const auto lax = u8io::read_file(path, /*validate_utf8=*/false);
    CHECK(lax.has_value());
    CHECK(lax->size() == 10);
    std::filesystem::remove(path);
}

TEST_CASE("read_file reports a missing file as an error") {
    const auto missing = u8io::read_file(temp_file(u8"u8io_absent_ø.txt"));
    CHECK(!missing.has_value());
    CHECK(missing.error().message().find(u8"cannot open") != std::u8string::npos);
}

TEST_CASE("write_to sends UTF-8 bytes to an ostream") {
    std::ostringstream os;
    u8io::write_to(os, u8"strëam");
    os << " and " << u8io::as_char(u8"möre");
    CHECK(os.str() == "strëam and möre");
}

TEST_CASE("filesystem paths are formattable") {
    const std::filesystem::path p = std::filesystem::path(u8"dossier/fichiér.txt");
    CHECK(u8io::format(u8"path: {}", p) == u8"path: dossier/fichiér.txt");
}
