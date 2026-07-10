#pragma once

// u8string-first printing.
//
// When <print> is available, output goes through std::vprint_unicode, which
// on Windows writes native Unicode to the console (WriteConsoleW-style) and
// raw UTF-8 bytes when the stream is redirected. Calling vprint_unicode
// explicitly — rather than std::print — guarantees the Unicode path is taken
// regardless of how this header's callers were compiled. On POSIX it just
// writes the bytes. Without <print>, falls back to vformat + fwrite.

#include <cstdio>
#include <string>
#include <string_view>

#include "format.hpp"

#ifdef __cpp_lib_print
#include <print>
#else
#include <cerrno>
#include <system_error>
#endif

namespace u8io {

namespace detail {

template <class... M>
void vprint_mapped(std::FILE* stream, std::string_view fmt, M&&... mapped) {
#ifdef __cpp_lib_print
    std::vprint_unicode(stream, fmt, std::make_format_args(mapped...));
#else
    const std::string out = std::vformat(fmt, std::make_format_args(mapped...));
    if (std::fwrite(out.data(), 1, out.size(), stream) != out.size()) {
        throw std::system_error(errno, std::generic_category(), "u8io::print");
    }
#endif
}

}  // namespace detail

template <class... Args>
void print(std::FILE* stream, u8format_string<Args...> fmt, Args&&... args) {
    detail::vprint_mapped(stream, as_char(fmt.get()),
                          detail::map_arg(std::forward<Args>(args))...);
}

template <class... Args>
void print(u8format_string<Args...> fmt, Args&&... args) {
    u8io::print(stdout, fmt, std::forward<Args>(args)...);
}

template <class... Args>
void println(std::FILE* stream, u8format_string<Args...> fmt, Args&&... args) {
    // fmt was already checked; appending a newline cannot invalidate it.
    std::string line(as_char(fmt.get()));
    line.push_back('\n');
    detail::vprint_mapped(stream, line,
                          detail::map_arg(std::forward<Args>(args))...);
}

template <class... Args>
void println(u8format_string<Args...> fmt, Args&&... args) {
    u8io::println(stdout, fmt, std::forward<Args>(args)...);
}

inline void println(std::FILE* stream) { detail::vprint_mapped(stream, "\n"); }

inline void println() { u8io::println(stdout); }

}  // namespace u8io
