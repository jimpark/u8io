#pragma once

// Code point iteration: u8io::code_points(text) is a forward range of
// char32_t scalar values decoded from a u8string_view's UTF-8 bytes.
//
// Consistent with the trust-by-convention policy, iteration does not fail:
// each ill-formed sequence decodes to U+FFFD (consuming the maximal
// subpart, Unicode's recommended substitution). Run u8io::validate() first
// if a replacement character would be the wrong answer.
//
// Code points are still not "characters" as a user perceives them — no
// grapheme clustering, normalization, or width here. That is ICU's job.

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <string_view>

#include "cast.hpp"
#include "config.hpp"

namespace u8io {

inline constexpr char32_t replacement_character = U'�';

class code_point_iterator {
public:
    using value_type = char32_t;
    using difference_type = std::ptrdiff_t;
    // Satisfies std::forward_iterator (multi-pass over an immutable view)
    // but dereferences to a value, so C++17-style traits see an input
    // iterator.
    using iterator_concept = std::forward_iterator_tag;
    using iterator_category = std::input_iterator_tag;

    constexpr code_point_iterator() = default;
    constexpr code_point_iterator(const char8_t* pos,
                                  const char8_t* end) noexcept
        : pos_(pos), end_(end) {
        decode();
    }

    [[nodiscard]] constexpr char32_t operator*() const noexcept {
        return code_point_;
    }

    constexpr code_point_iterator& operator++() noexcept {
        pos_ += size_;
        decode();
        return *this;
    }

    constexpr code_point_iterator operator++(int) noexcept {
        auto old = *this;
        ++*this;
        return old;
    }

    [[nodiscard]] friend constexpr bool operator==(
        const code_point_iterator& a, const code_point_iterator& b) noexcept {
        return a.pos_ == b.pos_;
    }

    // Byte position of the current code point — for slicing the underlying
    // view or reporting offsets.
    [[nodiscard]] constexpr const char8_t* base() const noexcept {
        return pos_;
    }

private:
    constexpr void decode() noexcept {
        if (pos_ == end_) {
            size_ = 0;
            return;
        }
        const auto r = detail::decode_one(pos_, end_);
        code_point_ = r.code_point;
        size_ = r.size;
    }

    const char8_t* pos_ = nullptr;
    const char8_t* end_ = nullptr;
    char32_t code_point_ = 0;
    std::uint8_t size_ = 0;
};

static_assert(std::forward_iterator<code_point_iterator>);

class code_points_view : public std::ranges::view_interface<code_points_view> {
public:
    constexpr code_points_view() = default;
    constexpr explicit code_points_view(std::u8string_view text) noexcept
        : text_(text) {}

    [[nodiscard]] constexpr code_point_iterator begin() const noexcept {
        return {text_.data(), text_.data() + text_.size()};
    }

    [[nodiscard]] constexpr code_point_iterator end() const noexcept {
        const char8_t* const e = text_.data() + text_.size();
        return {e, e};
    }

private:
    std::u8string_view text_;
};

[[nodiscard]] constexpr code_points_view code_points(
    std::u8string_view text) noexcept {
    return code_points_view(text);
}

}  // namespace u8io

// Iterators point into the underlying string, not the view, so they may
// outlive it — same borrowing rules as u8string_view itself.
template <>
inline constexpr bool
    std::ranges::enable_borrowed_range<u8io::code_points_view> = true;
