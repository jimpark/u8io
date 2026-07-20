// Regression: u8io's private ReadConsoleW declaration must match the one in
// <windows.h> exactly, or a translation unit including both fails with C2116
// ("function parameter lists differ") and C2733 ("illegal overload of
// extern C function"). This TU includes <windows.h> first;
// test_windows_headers_reverse.cpp covers the other order. Both are
// compile-time tests — building this file at all is the assertion.

#if defined(_WIN32)
// clang-format off
#include <windows.h>

#include "u8io/io.hpp"
// clang-format on

#include <cstdio>

#include "u8test.hpp"

TEST_CASE("win32 console declarations match <windows.h> and link") {
    // Compiling proves the signatures match; calling proves they still bind to
    // the real imports. The result depends on how the suite was launched, so
    // it is deliberately not asserted — only that the call resolves and
    // returns.
    (void)u8io::detail::is_console(stdin);

    // The parameter is PCONSOLE_READCONSOLE_CONTROL; nullptr must remain a
    // valid argument for it, and the SDK's own type must be interchangeable
    // with the one io.hpp forward-declares.
    CONSOLE_READCONSOLE_CONTROL control{};
    PCONSOLE_READCONSOLE_CONTROL sdk_pointer = &control;
    ::_CONSOLE_READCONSOLE_CONTROL* u8io_pointer = sdk_pointer;
    CHECK(u8io_pointer == sdk_pointer);
    CHECK(static_cast<::_CONSOLE_READCONSOLE_CONTROL*>(nullptr) == nullptr);
}
#endif  // _WIN32
