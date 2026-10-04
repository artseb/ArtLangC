# ============================================================
# ART — CMake build
#
# Primary build is the Makefile (`make`). This file exists so
# Visual Studio, Xcode, CLion, and IDE-less Windows users have
# a path that doesn't require installing GCC.
#
#   cmake -B build
#   cmake --build build
#   ctest --test-dir build
#
# Requires CMake 3.16+ (for CMAKE_C_STANDARD 11 support).
# Works with GCC, Clang, and MSVC.
# ============================================================

cmake_minimum_required(VERSION 3.16)
project(art LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Default to Debug if the user didn't pick a config. Keeps the
# assertions and the -g symbols that make the test output useful.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# ---- Warnings -----------------------------------------------
#
# Mirror the Makefile's flags on GCC/Clang; give MSVC the
# closest /W equivalents. Not -Werror on either side — the
# switch-completeness warnings in test_parser.c are intentional.

if(MSVC)
    add_compile_options(/W4)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# ---- Sources ------------------------------------------------
#
# Same wildcard layout as the Makefile, so adding a .c file
# under art/ requires no edit here. CONFIGURE_DEPENDS makes
# CMake re-glob on rebuild instead of only at configure time.

file(GLOB CORE_SRC CONFIGURE_DEPENDS
    art/core/runtime/*.c
    art/core/syntax/*.c
    art/core/syntax/parser/*.c
    art/core/interp/*.c
    art/features/*.c
    art/features/*/*.c
    art/*.c
)

# cli.c defines main(); it belongs to the executable, not the
# library. Same exclusion the Makefile does with filter-out.
list(FILTER CORE_SRC EXCLUDE REGEX "art/cli\\.c$")

file(GLOB TEST_SRC CONFIGURE_DEPENDS tests/test_*.c)

# ---- Library ------------------------------------------------

add_library(art_lib STATIC ${CORE_SRC})

target_include_directories(art_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/art
    ${CMAKE_CURRENT_SOURCE_DIR}/art/core/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/art/core/syntax
    ${CMAKE_CURRENT_SOURCE_DIR}/art/core/syntax/parser
    ${CMAKE_CURRENT_SOURCE_DIR}/art/core/interp
    ${CMAKE_CURRENT_SOURCE_DIR}/art/features
)

# libm is separate on Linux and macOS, part of the CRT on
# Windows. Linking it explicitly only where it exists.
if(NOT WIN32)
    target_link_libraries(art_lib PUBLIC m)
endif()

# ---- Executable ---------------------------------------------

add_executable(art art/cli.c)
target_link_libraries(art PRIVATE art_lib)

# ---- Tests --------------------------------------------------

enable_testing()

foreach(test_src ${TEST_SRC})
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} PRIVATE art_lib)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
