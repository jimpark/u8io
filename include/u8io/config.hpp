#pragma once

// u8io requires:
//  - C++23 <format> and <expected>
//  - the ordinary literal encoding to be UTF-8 (default on GCC/Clang;
//    pass /utf-8 on MSVC)
//
// The UTF-8 literal-encoding requirement is what makes the char8_t <-> char
// boundary a zero-cost reinterpretation everywhere in this library, and what
// makes std::print's Unicode-aware terminal path usable on Windows.

#include <version>

// libc++ 17/18 provide everything u8io needs from <format> but withhold
// __cpp_lib_format until libc++ 19, so accept them by version as well.
#if !defined(__cpp_lib_format) && \
    !(defined(_LIBCPP_VERSION) && _LIBCPP_VERSION >= 170000)
#error "u8io requires C++23 <format>"
#endif
#ifndef __cpp_lib_expected
#error \
    "u8io requires C++23 <expected> (note: libstdc++ disables <expected> for Clang < 19)"
#endif

namespace u8io::detail {

// "é" (LATIN SMALL LETTER E WITH ACUTE) is 0xC3 0xA9 in UTF-8 and a
// single byte in every legacy single-byte charset, so it distinguishes the
// two reliably.
inline constexpr char utf8_probe[] = "é";
inline constexpr bool ordinary_literals_are_utf8 =
    sizeof(utf8_probe) == 3 &&
    static_cast<unsigned char>(utf8_probe[0]) == 0xC3 &&
    static_cast<unsigned char>(utf8_probe[1]) == 0xA9;

}  // namespace u8io::detail

static_assert(u8io::detail::ordinary_literals_are_utf8,
              "u8io requires the ordinary literal encoding to be UTF-8. "
              "On MSVC, compile with /utf-8.");
