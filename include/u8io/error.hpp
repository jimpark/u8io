#pragma once

// u8string-based error handling for std::expected pipelines.
//
//   u8io::error        value-type error: UTF-8 message, source location,
//                      optional std::error_code, optional chained cause.
//   u8io::fail(...)    std::unexpected<error> with a u8-formatted message.
//   u8io::fail_with()  like fail(), chaining an underlying cause.
//   u8io::from_edge()  adapt expected<T, std::string / std::error_code /
//                      std::u8string> from third-party code.
//
// Both u8io::error and std::expected<T, E> are formattable through
// u8io::format / u8io::print. std::formatter cannot legally be specialized
// for std::expected (standard template, standard types), so the arg_mapper
// layer maps them to program-defined proxies whose formatters are ours to
// define.

#include <expected>
#include <format>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

#include "format.hpp"

namespace u8io {

class error {
public:
    error() = default;

    explicit error(std::u8string message,
                   std::source_location loc = std::source_location::current())
        : message_(std::move(message)), location_(loc) {}

    explicit error(std::error_code code, std::u8string message = {},
                   std::source_location loc = std::source_location::current())
        : message_(std::move(message)), code_(code), location_(loc) {}

    [[nodiscard]] const std::u8string& message() const noexcept { return message_; }
    [[nodiscard]] std::error_code code() const noexcept { return code_; }
    [[nodiscard]] const std::source_location& location() const noexcept {
        return location_;
    }
    [[nodiscard]] const error* cause() const noexcept { return cause_.get(); }

    // The cause is shared, keeping error cheap to copy through expected
    // plumbing.
    error& caused_by(error cause) & {
        cause_ = std::make_shared<const error>(std::move(cause));
        return *this;
    }
    [[nodiscard]] error&& caused_by(error cause) && {
        cause_ = std::make_shared<const error>(std::move(cause));
        return std::move(*this);
    }

    // Message with the full cause chain: "top: mid: root".
    [[nodiscard]] std::u8string full_message() const {
        std::u8string out = render_one(*this);
        for (const error* c = cause_.get(); c != nullptr; c = c->cause()) {
            out += u8": ";
            out += render_one(*c);
        }
        return out;
    }

    // For the char-based edges (loggers, exceptions, third-party APIs).
    [[nodiscard]] std::string what() const { return to_string(full_message()); }

private:
    static std::u8string render_one(const error& e) {
        std::u8string out = e.message_;
        if (e.code_) {
            // error_code::message() encoding is the system's; on the
            // platforms u8io targets with standard categories it is ASCII.
            if (!out.empty()) out += u8' ';
            out += u8'[';
            out += to_u8string(e.code_.category().name());
            out += u8':';
            out += to_u8string(std::to_string(e.code_.value()));
            out += u8": ";
            out += to_u8string(e.code_.message());
            out += u8']';
        }
        return out;
    }

    std::u8string message_;
    std::error_code code_{};
    std::source_location location_{};
    std::shared_ptr<const error> cause_;
};

// ---- constructing failures ---------------------------------------------------

template <class... Args>
[[nodiscard]] std::unexpected<error> fail(u8format_string<Args...> fmt,
                                          Args&&... args) {
    return std::unexpected<error>(
        error(u8io::format(fmt, std::forward<Args>(args)...), fmt.location()));
}

template <class... Args>
[[nodiscard]] std::unexpected<error> fail_with(error cause,
                                               u8format_string<Args...> fmt,
                                               Args&&... args) {
    return std::unexpected<error>(
        error(u8io::format(fmt, std::forward<Args>(args)...), fmt.location())
            .caused_by(std::move(cause)));
}

// For messages held in runtime u8strings (fail() wants a format string).
[[nodiscard]] inline std::unexpected<error> u8unexpected(
    std::u8string message,
    std::source_location loc = std::source_location::current()) {
    return std::unexpected<error>(error(std::move(message), loc));
}

// ---- adapters for the char-based edges ----------------------------------------

namespace detail {

inline error edge_to_error(std::string message, std::source_location loc) {
    return error(to_u8string(message), loc);
}
inline error edge_to_error(std::u8string message, std::source_location loc) {
    return error(std::move(message), loc);
}
inline error edge_to_error(std::error_code code, std::source_location loc) {
    return error(code, {}, loc);
}

}  // namespace detail

[[nodiscard]] inline error from_error_code(
    std::error_code code,
    std::source_location loc = std::source_location::current()) {
    return error(code, {}, loc);
}

// Adapt an expected from third-party code into the u8 error world.
// EdgeError may be std::string, std::u8string, or std::error_code.
template <class T, class EdgeError>
[[nodiscard]] std::expected<T, error> from_edge(
    std::expected<T, EdgeError> edge,
    std::source_location loc = std::source_location::current()) {
    if (edge.has_value()) {
        if constexpr (std::is_void_v<T>) {
            return {};
        } else {
            return std::expected<T, error>(std::in_place, std::move(*edge));
        }
    }
    return std::unexpected<error>(
        detail::edge_to_error(std::move(edge).error(), loc));
}

// ---- formatting support --------------------------------------------------------

namespace detail {

struct error_proxy {
    const error* err;
};

template <class T, class E>
struct expected_proxy {
    const std::expected<T, E>* exp;
};

}  // namespace detail

template <>
struct arg_mapper<error> {
    using mapped_type = detail::error_proxy;
    static detail::error_proxy map(const error& e) noexcept { return {&e}; }
};

template <class T, class E>
struct arg_mapper<std::expected<T, E>> {
    using mapped_type = detail::expected_proxy<T, E>;
    static detail::expected_proxy<T, E> map(const std::expected<T, E>& e) noexcept {
        return {&e};
    }
};

}  // namespace u8io

template <>
struct std::formatter<u8io::detail::error_proxy, char> {
    constexpr auto parse(std::basic_format_parse_context<char>& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw std::format_error("u8io::error accepts no format specifier");
        }
        return it;
    }

    template <class FormatContext>
    auto format(u8io::detail::error_proxy proxy, FormatContext& ctx) const {
        const std::u8string message = proxy.err->full_message();
        return std::format_to(ctx.out(), "{}", u8io::as_char(message));
    }
};

// A held value renders bare (a void value renders as nothing); an error state
// renders as "unexpected(<error>)". Value and error types are rendered
// through the arg_mapper layer, so nested u8strings, u8io::error, and even
// nested expecteds work.
template <class T, class E>
struct std::formatter<u8io::detail::expected_proxy<T, E>, char> {
    constexpr auto parse(std::basic_format_parse_context<char>& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw std::format_error("std::expected accepts no format specifier");
        }
        return it;
    }

    template <class FormatContext>
    auto format(u8io::detail::expected_proxy<T, E> proxy, FormatContext& ctx) const {
        if (proxy.exp->has_value()) {
            if constexpr (std::is_void_v<T>) {
                return ctx.out();
            } else {
                return std::format_to(ctx.out(), "{}",
                                      u8io::detail::map_arg(**proxy.exp));
            }
        }
        return std::format_to(ctx.out(), "unexpected({})",
                              u8io::detail::map_arg(proxy.exp->error()));
    }
};
