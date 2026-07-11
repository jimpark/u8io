add_executable(u8io_tests
    test_main.cpp
    test_cast.cpp
    test_format.cpp
    test_error.cpp
    test_print.cpp
    test_io.cpp
    test_ascii.cpp
    test_charconv.cpp
    test_hash.cpp
    test_text.cpp)

target_link_libraries(u8io_tests PRIVATE u8io::u8io)

if(MSVC)
    target_compile_options(u8io_tests PRIVATE /W4 /permissive-)
    # The tests use std::fopen portably; suppress MSVC's fopen_s suggestion.
    target_compile_definitions(u8io_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(u8io_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME u8io_tests COMMAND u8io_tests)
