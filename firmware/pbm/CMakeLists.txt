# NeurOne 1064nm Smart Zone Module — CMake Build
# Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
# Document: NP-FW-PBM1064-001 Rev 1
#
# Compiled as a static library linked into the main application.
# Does NOT produce a standalone binary — link np_pbm into the
# application target alongside np_bootloader and the platform HAL.
#
# Prerequisites:
#   - ARM GNU Toolchain (arm-none-eabi-gcc ≥ 12.3)
#   - CMake ≥ 3.20
#   - Parent project must provide NP_PLATFORM_INCLUDE_DIRS
#
# Usage (from parent CMakeLists.txt):
#   add_subdirectory(firmware/pbm)
#   target_link_libraries(np_application PRIVATE np_pbm)

cmake_minimum_required(VERSION 3.20)

project(np_pbm C)

# ── Host unit tests (no ARM toolchain required) ────────────────────────────────
# Build with -DNP_BUILD_TESTS=ON from the firmware super-project.
# Exercises the session descriptor v5 wire format (NP-SES-1064-001 §1): pure C,
# no FreeRTOS dependency, HAL calls are the deterministic stubs in
# src/np_pbm_hal.c.  That file is a HOST-TEST DOUBLE and is linked only here
# and into np_pbm_cal_bridge_tests (firmware/hub_control/CMakeLists.txt) — it
# is deliberately absent from PBM1064_SOURCES below.
option(NP_BUILD_TESTS "Build host-native unit tests (no ARM toolchain needed)" OFF)

if(NP_BUILD_TESTS)
    add_executable(np_pbm_session_desc_tests
        tests/np_pbm_session_desc_tests.c
        src/np_pbm_session.c
        src/np_pbm_dose.c
        src/np_pbm_drive.c
        src/np_pbm_detect.c
        src/np_pbm_t2_combined.c
        src/np_pbm_hal.c
        src/np_pbm_fai.c
    )
    target_include_directories(np_pbm_session_desc_tests PRIVATE include)
    target_compile_options(np_pbm_session_desc_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g)
    target_link_libraries(np_pbm_session_desc_tests m)

    return()
endif()

# ── Toolchain guard ────────────────────────────────────────────────────────────
if(NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "This project requires the ARM cross-compiler toolchain.\n"
        "Pass -DCMAKE_TOOLCHAIN_FILE=../../cmake/arm-none-eabi.cmake")
endif()

# ── Sources ────────────────────────────────────────────────────────────────────
# src/np_pbm_hal.c is NOT here, and must not come back (NP-SW-CI-001 §4.8.7).
# It is the host-test double: its I2C seams serve a RAM register file, its probe
# ACKs socket 0, its safety-MCU enable returns NP_PBM_OK.  Until 2026-09-28 it
# was in this list, so np_application (which links np_pbm --whole-archive)
# resolved every np_pbm_hal_* symbol to a plausible answer instead of a driver
# or a trap.  On target those symbols are owed by the SW-02 platform layer:
# firmware/platform/src/np_platform_stub.c defines each as a trap, and the
# platform census (NP_SW02_PLATFORM_SYMBOL_COUNT) counts them.  Adding the file
# back would fail the link on eighteen duplicate definitions.
set(PBM1064_SOURCES
    src/np_pbm_detect.c
    src/np_pbm_drive.c
    src/np_pbm_dose.c
    src/np_pbm_session.c
    src/np_pbm_t2_combined.c
    src/np_pbm_fai.c
)

add_library(np_pbm STATIC ${PBM1064_SOURCES})

target_include_directories(np_pbm
    PUBLIC  include
    PRIVATE ${NP_PLATFORM_INCLUDE_DIRS}
)

# ── Compiler flags ─────────────────────────────────────────────────────────────
set(CPU_FLAGS
    -mcpu=cortex-m7
    -mthumb
    -mfpu=fpv5-d16
    -mfloat-abi=hard
)

target_compile_options(np_pbm PRIVATE
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
    -ffast-math
    -fno-math-errno
    -fstack-usage
)

# ── Dependencies ───────────────────────────────────────────────────────────────
target_link_libraries(np_pbm PUBLIC m)
