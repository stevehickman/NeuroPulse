# NeurOne SHDR accelerometer processing — CMake build
# Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
# Document: NP-FW-EMMC-002 Rev 2 §G (standing rule) and §H (characterisation
#           programme).  NP-PRIV-REM-001 STEP-10.
#
# Static library linked into the main application.  Does NOT produce a
# standalone binary.
#
# Usage (from parent CMakeLists.txt):
#   add_subdirectory(firmware/shdr)
#   target_link_libraries(np_application PRIVATE np_accel_shdr)
#
# NOTE ON THE BUILD GATE.  NP_ACCEL_CHAR_WINDOW_ENABLED is defined in
# include/np_accel_shdr.h and is deliberately NOT overridable from here.  A -D
# would make the value ci/test_shdr_schema.py parses out of that header a
# statement about nothing.  The host tests reach the extended path through
# np_accel_char_build_gate, which exists only under NPTEST_HOST.

cmake_minimum_required(VERSION 3.20)

project(np_accel_shdr C)

set(SHDR_SOURCES
    src/np_accel_shdr.c
)

# ── Cross-compiled static library (target firmware) ───────────────────────────
if(CMAKE_CROSSCOMPILING)
    add_library(np_accel_shdr STATIC ${SHDR_SOURCES})

    target_include_directories(np_accel_shdr
        PUBLIC  include
        PRIVATE ${NP_PLATFORM_INCLUDE_DIRS}
    )

    target_compile_options(np_accel_shdr PRIVATE
        -mcpu=cortex-m7
        -mthumb
        -mfpu=fpv5-d16
        -mfloat-abi=hard
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

# ── Host test build (NOT cross-compiled) ──────────────────────────────────────
# Exercises both modes: every suppression assertion is paired with a positive
# control that runs the same input through an open window, so neither can pass
# vacuously.
if(NP_BUILD_TESTS)
    add_executable(np_accel_shdr_tests
        tests/np_accel_shdr_tests.c
        src/np_accel_shdr.c
    )
    target_include_directories(np_accel_shdr_tests PRIVATE include)
    target_compile_options(np_accel_shdr_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g -DNPTEST_HOST
    )
endif()
