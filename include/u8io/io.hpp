#pragma once

// The remaining I/O edges: iostreams, filesystem paths, and whole files.
//
// C++20 deliberately deleted operator<< for char8_t/u8string on ostreams;
// u8io does not fight that. Write `os << u8io::as_char(text)` or use
// write_to().

#include <cstddef>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <string>
#include <string_view>

#include "error.hpp"

namespace u8io {

inline std::ostream& write_to(std::ostream& os, std::u8string_view text) {
    return os.write(reinterpret_cast<const char*>(text.data()),
                    static_cast<std::streamsize>(text.size()));
}

// Paths format through their native u8string form (C++23 has no
// std::formatter<path>; this also sidesteps path::string()'s possible
// narrowing failure on Windows).
template <>
struct arg_mapper<std::filesystem::path> {
    using mapped_type = std::string;
    static std::string map(const std::filesystem::path& p) {
        return to_string(p.u8string());
    }
};

// Reads a whole file as UTF-8 text. Strips a UTF-8 BOM if present. With
// validate_utf8 (the default), malformed content is an error — this is the
// boundary where trust-by-convention meets outside data.
[[nodiscard]] inline std::expected<std::u8string, error> read_file(
    const std::filesystem::path& file, bool validate_utf8 = true) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return fail(u8"cannot open {} for reading", file);

    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    if (size < 0) return fail(u8"cannot determine size of {}", file);
    in.seekg(0, std::ios::beg);

    std::u8string content(static_cast<std::size_t>(size), u8'\0');
    in.read(reinterpret_cast<char*>(content.data()),
            static_cast<std::streamsize>(content.size()));
    if (!in || in.gcount() != static_cast<std::streamsize>(content.size())) {
        return fail(u8"error reading {}", file);
    }

    if (content.starts_with(u8"﻿")) content.erase(0, 3);

    if (validate_utf8) {
        if (const auto valid = validate(content); !valid) {
            return fail(u8"{} is not valid UTF-8 (at byte offset {})", file,
                        valid.error().offset);
        }
    }
    return content;
}

// Writes UTF-8 text to a file (truncating). No BOM is written.
[[nodiscard]] inline std::expected<void, error> write_file(
    const std::filesystem::path& file, std::u8string_view content) {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) return fail(u8"cannot open {} for writing", file);
    out.write(reinterpret_cast<const char*>(content.data()),
              static_cast<std::streamsize>(content.size()));
    out.flush();
    if (!out) return fail(u8"error writing {}", file);
    return {};
}

}  // namespace u8io
