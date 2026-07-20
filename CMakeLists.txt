cmake_minimum_required(VERSION 3.25)

project(u8io VERSION 0.1.1 LANGUAGES CXX)

include(GNUInstallDirs)

add_library(u8io INTERFACE)
add_library(u8io::u8io ALIAS u8io)

target_include_directories(u8io INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
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

option(U8IO_INSTALL "Generate the install target" ${PROJECT_IS_TOP_LEVEL})
if(U8IO_INSTALL)
    include(CMakePackageConfigHelpers)

    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(TARGETS u8io EXPORT u8io-targets)
    install(EXPORT u8io-targets
        NAMESPACE u8io::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/u8io)

    configure_package_config_file(
        cmake/u8io-config.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/u8io-config.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/u8io)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/u8io-config-version.cmake"
        COMPATIBILITY SameMajorVersion
        ARCH_INDEPENDENT)
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/u8io-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/u8io-config-version.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/u8io)
endif()
