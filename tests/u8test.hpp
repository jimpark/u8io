#pragma once

// Minimal zero-dependency test harness: TEST_CASE registration, CHECK,
// CHECK_THROWS_AS, and a run_all() driver. Kept deliberately tiny so the
// library has no external dependencies to fetch on any platform.

#include <cstdio>
#include <exception>
#include <vector>

namespace u8test {

struct test_case {
    const char* name;
    void (*fn)();
};

inline std::vector<test_case>& registry() {
    static std::vector<test_case> cases;
    return cases;
}

inline int& failure_count() {
    static int count = 0;
    return count;
}

struct registrar {
    registrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};

inline void report_failure(const char* what, const char* file, int line) {
    ++failure_count();
    std::printf("FAILED  %s:%d  %s\n", file, line, what);
}

inline int run_all() {
    for (const auto& test : registry()) {
        try {
            test.fn();
        } catch (const std::exception& e) {
            ++failure_count();
            std::printf("FAILED  [%s] unexpected exception: %s\n", test.name,
                        e.what());
        } catch (...) {
            ++failure_count();
            std::printf("FAILED  [%s] unexpected non-std exception\n", test.name);
        }
    }
    std::printf("%zu test case(s), %d failure(s)\n", registry().size(),
                failure_count());
    return failure_count() == 0 ? 0 : 1;
}

}  // namespace u8test

#define U8TEST_CAT2(a, b) a##b
#define U8TEST_CAT(a, b) U8TEST_CAT2(a, b)

#define U8TEST_CASE_IMPL(name, id)                                     \
    static void id();                                                  \
    static const ::u8test::registrar U8TEST_CAT(id, _registrar){name, &id}; \
    static void id()

#define TEST_CASE(name) \
    U8TEST_CASE_IMPL(name, U8TEST_CAT(u8test_case_, __COUNTER__))

#define CHECK(...)                                                          \
    do {                                                                    \
        if (!(__VA_ARGS__)) {                                               \
            ::u8test::report_failure("CHECK(" #__VA_ARGS__ ")", __FILE__,   \
                                     __LINE__);                             \
        }                                                                   \
    } while (0)

#define CHECK_THROWS_AS(expr, exception_type)                                \
    do {                                                                     \
        bool u8test_caught = false;                                          \
        try {                                                                \
            (void)(expr);                                                    \
        } catch (const exception_type&) {                                    \
            u8test_caught = true;                                            \
        } catch (...) {                                                      \
        }                                                                    \
        if (!u8test_caught) {                                                \
            ::u8test::report_failure(                                        \
                "CHECK_THROWS_AS(" #expr ", " #exception_type ")", __FILE__, \
                __LINE__);                                                   \
        }                                                                    \
    } while (0)
