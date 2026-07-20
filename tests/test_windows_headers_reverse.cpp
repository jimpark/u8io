// The other include order for the C2116/C2733 regression in
// test_windows_headers.cpp: u8io first, so <windows.h> later completes the
// type io.hpp forward-declares. Compile-time only — no test case here, since
// the assertion is that this file builds.

#if defined(_WIN32)
// clang-format off
#include "u8io/io.hpp"

#include <windows.h>
// clang-format on
#endif  // _WIN32
