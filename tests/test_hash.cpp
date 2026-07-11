#include <string>
#include <string_view>

#include "u8io/hash.hpp"
#include "u8test.hpp"

using namespace std::literals;

TEST_CASE("u8string_map heterogeneous lookup") {
    u8io::u8string_map<int> ages;
    ages[u8"amélie"] = 34;
    ages[u8"bob"] = 61;

    // Lookups with a view, a literal, and a u8string — none should require
    // constructing a temporary key of a different type at the call site.
    const std::u8string_view amelie = u8"amélie";
    CHECK(ages.find(amelie) != ages.end());
    CHECK(ages.find(amelie)->second == 34);
    CHECK(ages.find(u8"bob") != ages.end());
    CHECK(ages.contains(u8"bob"sv));
    CHECK(!ages.contains(u8"carol"sv));
    CHECK(ages.count(std::u8string(u8"bob")) == 1);
}

TEST_CASE("u8string_set heterogeneous lookup") {
    u8io::u8string_set seen{u8"π", u8"é"};
    CHECK(seen.contains(u8"π"sv));
    CHECK(seen.contains(u8"é"));
    CHECK(!seen.contains(u8"x"sv));
}

TEST_CASE("string_hash agrees with std::hash across key types") {
    const u8io::string_hash h;
    const std::u8string owned = u8"clé";
    CHECK(h(owned) == h(std::u8string_view(owned)));
    CHECK(h(owned) == h(u8"clé"));
}
