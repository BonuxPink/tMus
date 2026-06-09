cmake_minimum_required(VERSION 3.20)

project(tMus
    VERSION 0.1.0
    DESCRIPTION "Terminal music player"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Global settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo)
endif()

option(TMUS_BUILD_TESTS "Build the test executables" OFF)

# ---------------------------------------------------------------------------
# Dependencies (all discovered via pkg-config)
# ---------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(NOTCURSES REQUIRED IMPORTED_TARGET
    notcurses++ notcurses notcurses-core)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavutil libavformat libavcodec libswresample libavdevice libswscale)
pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)
pkg_check_modules(JSONCPP REQUIRED IMPORTED_TARGET jsoncpp)

# ---------------------------------------------------------------------------
# Warning / hardening flags (GCC and Clang)
# ---------------------------------------------------------------------------
add_library(tMus_flags INTERFACE)

target_compile_options(tMus_flags INTERFACE
    -Wall -Wextra -Wpedantic
    -Werror=uninitialized
    -Wnon-virtual-dtor -Wdouble-promotion
    -Wnull-dereference -Wconversion -Wno-sign-conversion
    -Wdeprecated -Wdeprecated-copy-dtor
    -fstack-protector-strong -fstack-clash-protection
    -fdiagnostics-color=always)

# GCC-only diagnostics
target_compile_options(tMus_flags INTERFACE
    $<$<CXX_COMPILER_ID:GNU>:-Wduplicated-cond;-Wduplicated-branches;-fcf-protection>)

target_compile_definitions(tMus_flags INTERFACE
    $<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS>)

# ---------------------------------------------------------------------------
# Core library: everything except main.cpp
# ---------------------------------------------------------------------------
file(GLOB_RECURSE TMUS_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(REMOVE_ITEM TMUS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

add_library(tMusCore STATIC ${TMUS_SOURCES})

target_include_directories(tMusCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(tMusCore
    PUBLIC
        PkgConfig::NOTCURSES
        PkgConfig::FFMPEG
        PkgConfig::PIPEWIRE
        Threads::Threads
    PRIVATE
        PkgConfig::JSONCPP
        tMus_flags)

# ---------------------------------------------------------------------------
# Main executable
# ---------------------------------------------------------------------------
add_executable(tMus src/main.cpp)
target_link_libraries(tMus PRIVATE tMusCore tMus_flags)

# ---------------------------------------------------------------------------
# Tests (boost-ext/ut, header-only via tests/ut.hpp)
# ---------------------------------------------------------------------------
if(TMUS_BUILD_TESTS)
    enable_testing()

    set(TMUS_TESTS
        TestAudioLoop
        TestCommandView
        TestConfig
        TestFocus
        TestIniParse
        TestInit
        TestUtil)

    foreach(test_name IN LISTS TMUS_TESTS)
        add_executable(${test_name} "tests/${test_name}.cpp")
        target_include_directories(${test_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        target_link_libraries(${test_name} PRIVATE tMusCore tMus_flags)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()
