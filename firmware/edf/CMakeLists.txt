# NeurOne EDF+ Privacy Header Writer/Validator — CMake Build
# Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
# Document: NP-FW-EMMC-002 Rev A §E
#
# Static library linked into the main application.  Does NOT produce a
# standalone binary.  The host unit-test target (NP_BUILD_TESTS) builds
# natively and does NOT require the ARM cross-compiler.
#
# Prerequisites:
#   - For the library: ARM GNU Toolchain (arm-none-eabi-gcc >= 12.3)
#   - For the tests:   any host C11 compiler
#   - CMake >= 3.20
#
# Usage (from parent CMakeLists.txt):
#   add_subdirectory(firmware/edf)
#   target_link_libraries(np_application PRIVATE np_edf)
#
# Host test build (from repo root):
#   cmake -B build/edf-tests firmware/edf -DCMAKE_BUILD_TYPE=Debug -DNP_BUILD_TESTS=ON
#   cmake --build build/edf-tests
#   ./build/edf-tests/np_edf_tests

cmake_minimum_required(VERSION 3.20)

project(np_edf C)

# ── Toolchain guard ──────────────────────────────────────────────────────────
# The host test target is NOT cross-compiled; bypass the guard when building
# tests so the suite runs in CI without the ARM toolchain.
if(NOT NP_BUILD_TESTS AND NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "This project requires the ARM cross-compiler toolchain for the library.\n"
        "Pass -DCMAKE_TOOLCHAIN_FILE=../../cmake/arm-none-eabi.cmake,\n"
        "or build the host tests with -DNP_BUILD_TESTS=ON.")
endif()

# ── Sources ──────────────────────────────────────────────────────────────────
set(EDF_SOURCES
    src/np_edf_writer.c
    src/np_edf_validator.c
)

# ── Library (cross-compiled) ─────────────────────────────────────────────────
# Built only when cross-compiling — the host test build skips it because the
# Cortex-M7 flags are invalid for a native compiler.
if(CMAKE_CROSSCOMPILING)
    add_library(np_edf STATIC ${EDF_SOURCES})

    target_include_directories(np_edf PUBLIC include)

    set(CPU_FLAGS
        -mcpu=cortex-m7
        -mthumb
        -mfpu=fpv5-d16
        -mfloat-abi=hard
    )

    target_compile_options(np_edf PRIVATE
        ${CPU_FLAGS}
        -std=c11
        -Wall
        -Wextra
        -Werror
        -Wno-unused-parameter
        -O2
        -ffunction-sections
        -fdata-sections
        -fno-common
        -ffreestanding
        -fstack-usage
    )
endif()

# ── Host test build (native, not cross-compiled) ─────────────────────────────
if(NP_BUILD_TESTS)
    add_executable(np_edf_tests
        tests/np_edf_tests.c
        ${EDF_SOURCES}
    )
    target_include_directories(np_edf_tests PRIVATE include)
    target_compile_options(np_edf_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g -DNPTEST_HOST
    )
endif()
