cmake_minimum_required(VERSION 3.25)

project(u8io VERSION 0.1.0 LANGUAGES CXX)

add_library(u8io INTERFACE)
add_library(u8io::u8io ALIAS u8io)

target_include_directories(u8io INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_features(u8io INTERFACE cxx_std_23)

# The library statically asserts that the ordinary literal encoding is UTF-8;
# make that true for MSVC consumers by default.
if(MSVC)
    target_compile_options(u8io INTERFACE /utf-8)
endif()

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

option(U8IO_BUILD_TESTS "Build u8io tests" ${PROJECT_IS_TOP_LEVEL})
if(U8IO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
