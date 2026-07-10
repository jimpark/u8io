#include <cstdio>
#include <filesystem>
#include <string>

#include "u8io/io.hpp"
#include "u8io/print.hpp"
#include "u8test.hpp"

namespace {

// Print to a real FILE* and read the bytes back; ASCII file name so the
// narrow fopen path works everywhere.
std::filesystem::path print_capture_path() {
    return std::filesystem::temp_directory_path() / "u8io_print_capture.txt";
}

}  // namespace

TEST_CASE("print and println write UTF-8 bytes to a FILE*") {
    const auto path = print_capture_path();
    std::FILE* f = std::fopen(path.string().c_str(), "wb");
    CHECK(f != nullptr);
    if (f == nullptr) return;

    u8io::print(f, u8"héllo {}", u8"wörld");
    u8io::println(f, u8" — done ({})", 1);
    u8io::println(f);
    std::fclose(f);

    const auto content = u8io::read_file(path);
    CHECK(content.has_value());
    CHECK(*content == u8"héllo wörld — done (1)\n\n");
    std::filesystem::remove(path);
}
