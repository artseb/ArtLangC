# ============================================================
# ART — CMake build
#
# Works with GCC, Clang, MSVC, and Apple Clang. On Linux the
# math library is linked explicitly; on macOS it's part of libc
# and the link line is a no-op.
#
#   cmake -B build
#   cmake --build build
#   ctest --test-dir build
#
# Requires CMake 3.16+.
# ============================================================

cmake_minimum_required(VERSION 3.16)
project(art LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# ---- Warnings -----------------------------------------------

if(MSVC)
    add_compile_options(/W4)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# ---- Sources ------------------------------------------------

file(GLOB CORE_SRC CONFIGURE_DEPENDS
    art/core/runtime/*.c
    art/core/syntax/*.c
    art/core/syntax/parser/*.c
    art/core/interp/*.c
    art/features/*.c
    art/features/*/*.c
    art/*.c
)

list(FILTER CORE_SRC EXCLUDE REGEX "art/cli\\.c$")

file(GLOB TEST_SRC CONFIGURE_DEPENDS tests/test_*.c)

# ---- Library ------------------------------------------------

add_library(art_lib STATIC ${CORE_SRC})

if(MSVC)
    target_include_directories(art_lib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/art
        ${CMAKE_CURRENT_SOURCE_DIR}/art/core/runtime
        ${CMAKE_CURRENT_SOURCE_DIR}/art/core/syntax
        ${CMAKE_CURRENT_SOURCE_DIR}/art/core/syntax/parser
        ${CMAKE_CURRENT_SOURCE_DIR}/art/core/interp
        ${CMAKE_CURRENT_SOURCE_DIR}/art/features
    )
else()
    # Use -iquote instead of -I. This keeps project headers available
    # to #include "..." without allowing art/features/features.h to
    # shadow glibc's <features.h> on Unix.
    target_compile_options(art_lib PUBLIC
        -iquote${CMAKE_CURRENT_SOURCE_DIR}/art
        -iquote${CMAKE_CURRENT_SOURCE_DIR}/art/core/runtime
        -iquote${CMAKE_CURRENT_SOURCE_DIR}/art/core/syntax
        -iquote${CMAKE_CURRENT_SOURCE_DIR}/art/core/syntax/parser
        -iquote${CMAKE_CURRENT_SOURCE_DIR}/art/core/interp
        -iquote${CMAKE_CURRENT_SOURCE_DIR}/art/features
    )
endif()

# libm is separate on Linux and glibc-based systems. It's part
# of libc on macOS and Windows, so the link line is a no-op there.
if(UNIX AND NOT APPLE)
    find_library(MATH_LIBRARY m)
    if(MATH_LIBRARY)
        target_link_libraries(art_lib PUBLIC ${MATH_LIBRARY})
    endif()
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
