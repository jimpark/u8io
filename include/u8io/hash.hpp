#pragma once

// Transparent hashing so unordered containers keyed by std::u8string can be
// queried with a u8string_view or char8_t* literal without allocating a
// temporary key — the boilerplate the standard makes you write by hand.
// (Ordered std::map/std::set need no help: use std::less<>.)

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "config.hpp"

namespace u8io {

struct string_hash {
    using is_transparent = void;
    [[nodiscard]] std::size_t operator()(
        std::u8string_view text) const noexcept {
        return std::hash<std::u8string_view>{}(text);
    }
};

template <class T>
using u8string_map =
    std::unordered_map<std::u8string, T, string_hash, std::equal_to<>>;

using u8string_set =
    std::unordered_set<std::u8string, string_hash, std::equal_to<>>;

}  // namespace u8io
