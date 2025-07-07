cmake_minimum_required(VERSION 3.16)

# ------------------------------------------------------------
# Blaze Compiler (C) - Cross-platform CMake build
# ------------------------------------------------------------
project(Blaze C)

# --------------------------
# Build type
# --------------------------
# Default to Release if the user didn't specify -DCMAKE_BUILD_TYPE
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# --------------------------
# Compiler flags
# --------------------------
if (MSVC)
    # Enable highest warning level and speed optimisations
    add_compile_options(/W4 /O2)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    # Common flags for GCC/Clang
    add_compile_options(-Wall -Wextra -fno-stack-protector)
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3)
    endif()
endif()

# --------------------------
# Include directories
# --------------------------
include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

# --------------------------
# Source files (recursively)
# --------------------------
file(GLOB_RECURSE BLAZE_SOURCES CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/src/*.c
)

add_executable(blaze ${BLAZE_SOURCES})

# Link math library on Unix-like systems
if (UNIX AND NOT APPLE)
    target_link_libraries(blaze m)
endif()

# --------------------------
# Install rules
# --------------------------
install(TARGETS blaze
        RUNTIME DESTINATION bin)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/
        DESTINATION include/blaze)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/docs/
        DESTINATION share/doc/blaze)

# --------------------------
# Tests (optional)
# --------------------------
include(CTest)

if (BUILD_TESTING)
    # Simple smoke test that compiles & runs one Blaze example when ctest is executed
    add_test(NAME smoke_basic_print
             COMMAND blaze ${PROJECT_SOURCE_DIR}/tests/working/01_no_boilerplate.blaze smoke_out)
endif()

# --------------------------
# Per-file compile options
# --------------------------
if (NOT MSVC)
    # The x64 backend needs large model + no PIC on some Unix targets
    set_source_files_properties(
        ${PROJECT_SOURCE_DIR}/src/codegen/codegen_x64.c
        PROPERTIES COMPILE_OPTIONS "-mcmodel=large;-fno-pic"
    )
endif()

# --------------------------
# Debug executable (mirrors Makefile debug target)
# --------------------------
add_executable(blaze_debug ${BLAZE_SOURCES})

if (MSVC)
    target_compile_options(blaze_debug PRIVATE /W4 /Od /Zi)
else()
    target_compile_options(blaze_debug PRIVATE -O0 -g -DDEBUG)
endif()

# Link math lib for debug, too
if (UNIX AND NOT APPLE)
    target_link_libraries(blaze_debug m)
endif()

# --------------------------
# Uninstall target
# --------------------------
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY
)
add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
) 