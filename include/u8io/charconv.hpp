#pragma once

// <charconv> for char8_t buffers. std::to_chars/std::from_chars only accept
// char*; under this library's UTF-8 ground rules the reinterpretation is
// free, so these wrappers forward and translate the result pointers back.
// Numeric text is pure ASCII, so no validation question arises.
//
// Not constexpr: the char8_t* <-> char* reinterpret_cast is not permitted
// in constant evaluation. Use std::to_chars with char buffers if you need
// compile-time conversion.
//
// Capabilities exactly track the underlying standard library. Notably,
// libc++ before 20 has no floating-point from_chars (the overloads are
// declared deleted); check __cpp_lib_to_chars if you need it portably.

#include <charconv>
#include <string_view>
#include <system_error>

#include "config.hpp"

namespace u8io {

struct to_chars_result {
    char8_t* ptr;
    std::errc ec;
    friend bool operator==(const to_chars_result&,
                           const to_chars_result&) = default;
};

struct from_chars_result {
    const char8_t* ptr;
    std::errc ec;
    friend bool operator==(const from_chars_result&,
                           const from_chars_result&) = default;
};

// value plus the same trailing arguments std::to_chars takes: nothing, an
// integer base, or std::chars_format [+ precision].
template <class... Args>
[[nodiscard]] inline to_chars_result to_chars(char8_t* first, char8_t* last,
                                              Args... args) {
    const auto r = std::to_chars(reinterpret_cast<char*>(first),
                                 reinterpret_cast<char*>(last), args...);
    return {reinterpret_cast<char8_t*>(r.ptr), r.ec};
}

template <class T, class... Args>
[[nodiscard]] inline from_chars_result from_chars(const char8_t* first,
                                                  const char8_t* last, T& value,
                                                  Args... args) {
    const auto r =
        std::from_chars(reinterpret_cast<const char*>(first),
                        reinterpret_cast<const char*>(last), value, args...);
    return {reinterpret_cast<const char8_t*>(r.ptr), r.ec};
}

// Convenience: parse from a view. On success ptr points one past the last
// digit consumed, inside (or at the end of) text.
template <class T, class... Args>
[[nodiscard]] inline from_chars_result from_chars(std::u8string_view text,
                                                  T& value, Args... args) {
    return u8io::from_chars(text.data(), text.data() + text.size(), value,
                            args...);
}

}  // namespace u8io
