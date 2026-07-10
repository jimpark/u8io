#pragma once

// u8string-first formatting on top of std::format.
//
// The standard forbids extending std::format itself for char8_t:
// std::formatter<std::u8string, char> would be a specialization of a standard
// template for standard types (ill-formed, no diagnostic required), and
// basic_format_context<*, char8_t> is not a supported instantiation. So this
// layer (a) transcodes the format string at zero cost (UTF-8 literal encoding
// is enforced by config.hpp), (b) maps u8-flavored arguments to char-flavored
// ones, and (c) forwards to std::vformat.
//
// Compile-time format-string checking is preserved: u8format_string's
// consteval constructor re-runs the standard library's own checker on a
// transcoded copy, against the *mapped* argument types, so u8"{}" with a
// std::u8string argument checks exactly like "{}" with a std::string_view.
//
// Extension point: specialize u8io::arg_mapper<T> to teach u8io::format how
// to render your type through a char-formattable proxy (see error.hpp for
// how std::expected and u8io::error plug in).

#include <format>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "cast.hpp"

namespace u8io {

// ---- argument mapping ------------------------------------------------------

template <class T>
struct arg_mapper {
    using mapped_type = T;
    template <class U>
    static constexpr U&& map(U&& value) noexcept {
        return std::forward<U>(value);
    }
};

template <>
struct arg_mapper<std::u8string_view> {
    using mapped_type = std::string_view;
    static std::string_view map(std::u8string_view value) noexcept {
        return as_char(value);
    }
};

template <>
struct arg_mapper<std::u8string> {
    using mapped_type = std::string_view;
    static std::string_view map(const std::u8string& value) noexcept {
        return as_char(value);
    }
};

template <>
struct arg_mapper<const char8_t*> {
    using mapped_type = const char*;
    static const char* map(const char8_t* value) noexcept {
        return reinterpret_cast<const char*>(value);
    }
};

template <>
struct arg_mapper<char8_t*> : arg_mapper<const char8_t*> {};

// A lone char8_t is a UTF-8 code unit, not necessarily a whole character;
// it renders as a single byte.
template <>
struct arg_mapper<char8_t> {
    using mapped_type = char;
    static constexpr char map(char8_t value) noexcept {
        return static_cast<char>(value);
    }
};

namespace detail {

// Arrays decay so that u8"literal" arguments select the const char8_t*
// mapper.
template <class T>
using mapper_for = ::u8io::arg_mapper<std::decay_t<T>>;

template <class T>
using mapped_check_t = typename mapper_for<T>::mapped_type;

template <class T>
constexpr decltype(auto) map_arg(T&& value) {
    return mapper_for<T>::map(std::forward<T>(value));
}

}  // namespace detail

// ---- format strings --------------------------------------------------------

// Escape hatch for format strings not known until runtime; errors surface as
// std::format_error thrown from u8io::format.
struct runtime_u8format {
    std::u8string_view str;
};

[[nodiscard]] inline runtime_u8format runtime_format(std::u8string_view fmt) noexcept {
    return {fmt};
}

template <class... Args>
class basic_u8format_string {
public:
    template <class S>
        requires std::convertible_to<const S&, std::u8string_view>
    consteval basic_u8format_string(
        const S& fmt,
        std::source_location loc = std::source_location::current())
        : str_(fmt), loc_(loc) {
#ifndef U8IO_NO_FORMAT_CHECKS
        // Re-run the standard checker on a char transcription. A parse error
        // or an argument mismatch makes this constructor not a constant
        // expression, i.e. a compile error at the call site. Define
        // U8IO_NO_FORMAT_CHECKS to fall back to runtime std::format_error
        // on toolchains whose consteval support chokes on this.
        std::string transcoded;
        transcoded.reserve(str_.size());
        for (const char8_t c : str_) transcoded.push_back(static_cast<char>(c));
        (void)std::basic_format_string<char, detail::mapped_check_t<Args>...>(
            std::string_view(transcoded));
#endif
    }

    constexpr basic_u8format_string(
        runtime_u8format fmt,
        std::source_location loc = std::source_location::current()) noexcept
        : str_(fmt.str), loc_(loc) {}

    [[nodiscard]] constexpr std::u8string_view get() const noexcept { return str_; }
    [[nodiscard]] constexpr std::source_location location() const noexcept { return loc_; }

private:
    std::u8string_view str_;
    std::source_location loc_;
};

template <class... Args>
using u8format_string = basic_u8format_string<std::type_identity_t<Args>...>;

// ---- formatting functions ---------------------------------------------------

namespace detail {

// Mapped arguments arrive as function parameters so that make_format_args
// (which requires lvalues) sees named objects that outlive the vformat call.
template <class... M>
[[nodiscard]] std::string vformat_mapped(std::string_view fmt, M&&... mapped) {
    return std::vformat(fmt, std::make_format_args(mapped...));
}

template <class Out, class... M>
Out vformat_to_mapped(Out out, std::string_view fmt, M&&... mapped) {
    return std::vformat_to(std::move(out), fmt, std::make_format_args(mapped...));
}

}  // namespace detail

template <class... Args>
[[nodiscard]] std::u8string format(u8format_string<Args...> fmt, Args&&... args) {
    return to_u8string(detail::vformat_mapped(
        as_char(fmt.get()), detail::map_arg(std::forward<Args>(args))...));
}

// Out must accept char writes; std::back_inserter over a std::u8string works
// (char converts to char8_t code units).
template <class Out, class... Args>
Out format_to(Out out, u8format_string<Args...> fmt, Args&&... args) {
    return detail::vformat_to_mapped(
        std::move(out), as_char(fmt.get()),
        detail::map_arg(std::forward<Args>(args))...);
}

template <class... Args>
[[nodiscard]] std::size_t formatted_size(u8format_string<Args...> fmt, Args&&... args) {
    return detail::vformat_mapped(
               as_char(fmt.get()), detail::map_arg(std::forward<Args>(args))...)
        .size();
}

}  // namespace u8io
