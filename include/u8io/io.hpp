#pragma once

// The remaining I/O edges: iostreams, filesystem paths, and whole files.
//
// C++20 deliberately deleted operator<< for char8_t/u8string on ostreams;
// u8io does not fight that. Write `os << u8io::as_char(text)` or use
// write_to().

#include <cstddef>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <fstream>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "error.hpp"
#include "transcode.hpp"

#if defined(_WIN32)
#include <io.h>  // _isatty, _get_osfhandle
#endif

namespace u8io {

#if defined(_WIN32)
namespace detail::win32 {

// Declared here instead of dragging <windows.h> (and its macros) into every
// includer of a header-only library. C language linkage makes these the same
// entities <windows.h> declares; the signatures must therefore match it
// exactly (HANDLE = void*, DWORD = unsigned long, BOOL = int).
extern "C" {
__declspec(dllimport) int __stdcall GetConsoleMode(void* handle,
                                                   unsigned long* mode);
__declspec(dllimport) int __stdcall ReadConsoleW(void* handle, void* buffer,
                                                 unsigned long chars_to_read,
                                                 unsigned long* chars_read,
                                                 void* input_control);
__declspec(dllimport) unsigned long __stdcall GetLastError();
}

}  // namespace detail::win32
#endif  // _WIN32

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

namespace detail {

#if defined(_WIN32)

inline bool is_console(std::FILE* stream) {
    const int fd = _fileno(stream);
    if (fd < 0 || _isatty(fd) == 0) return false;
    // _isatty is true for any character device (NUL included); GetConsoleMode
    // succeeding is what distinguishes a real console.
    unsigned long mode = 0;
    return win32::GetConsoleMode(reinterpret_cast<void*>(_get_osfhandle(fd)),
                                 &mode) != 0;
}

inline std::expected<std::optional<std::u8string>, error> read_line_console(
    std::FILE* stream) {
    void* const handle =
        reinterpret_cast<void*>(_get_osfhandle(_fileno(stream)));
    std::wstring wide;
    for (;;) {
        wchar_t buffer[512];
        unsigned long count = 0;
        if (win32::ReadConsoleW(handle, buffer, 512, &count, nullptr) == 0) {
            return std::unexpected(
                error(std::error_code(static_cast<int>(win32::GetLastError()),
                                      std::system_category()),
                      u8"error reading from console"));
        }
        if (count == 0) break;
        wide.append(buffer, count);
        // In the console's default line-input mode a read never spans past
        // '\n', so the terminator can only be the last unit appended.
        if (wide.back() == L'\n') break;
    }
    // Ctrl-Z at the start of a line is the console's end-of-input signal.
    if (wide.empty() || wide.front() == L'\x1a') {
        return std::optional<std::u8string>();
    }
    if (wide.ends_with(L'\n')) wide.pop_back();
    if (wide.ends_with(L'\r')) wide.pop_back();
    // Console input can contain unpaired surrogates (e.g. from Alt codes or
    // pastes); lossy transcoding turns them into U+FFFD rather than failing.
    return std::optional<std::u8string>(to_u8string_lossy(wide));
}

#endif  // _WIN32

}  // namespace detail

// Reads one line of UTF-8 text from a C stream (default: stdin). Returns the
// line without its terminator (a '\r' before the '\n' is also removed),
// std::nullopt at end of input, or an error.
//
// On Windows, when the stream is an actual console, input is read natively
// via ReadConsoleW and transcoded from UTF-16 — the input-side counterpart of
// vprint_unicode's output path — so typed text arrives as UTF-8 regardless of
// the console code page; validate_utf8 is moot there because transcoding
// always produces well-formed UTF-8. Everywhere else (redirected input, POSIX,
// regular files) bytes are read as-is and, with validate_utf8 (the default),
// checked for well-formedness like read_file.
[[nodiscard]] inline std::expected<std::optional<std::u8string>, error>
read_line(std::FILE* stream = stdin, bool validate_utf8 = true) {
#if defined(_WIN32)
    if (detail::is_console(stream)) return detail::read_line_console(stream);
#endif
    std::u8string line;
    for (;;) {
        const int c = std::fgetc(stream);
        if (c == EOF) {
            if (std::ferror(stream) != 0) return fail(u8"error reading line");
            if (line.empty()) return std::optional<std::u8string>();
            break;  // final line without a terminator
        }
        if (c == '\n') break;
        line.push_back(static_cast<char8_t>(c));
    }
    if (line.ends_with(u8'\r')) line.pop_back();
    if (validate_utf8) {
        if (const auto valid = validate(line); !valid) {
            return fail(u8"line is not valid UTF-8 (at byte offset {})",
                        valid.error().offset);
        }
    }
    return std::optional<std::u8string>(std::move(line));
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
