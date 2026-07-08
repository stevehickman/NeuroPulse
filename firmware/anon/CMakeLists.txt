# NeurOne Research Anonymisation — Scratch Encryption — CMake Build
# Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
# Document: NP-FW-EMMC-002 Rev A §D
#
# Static library linked into the main application.  Does NOT produce a
# standalone binary.
#
# Prerequisites:
#   - ARM GNU Toolchain (arm-none-eabi-gcc >= 12.3)
#   - CMake >= 3.20
#   - Parent project must provide:
#       NP_PLATFORM_INCLUDE_DIRS — FreeRTOS headers and platform HAL
#   - Scratch partition geometry + NP_SNVS_BASE come from the bootloader's
#     np_config.h, so the bootloader include dir is on the include path.
#
# Usage (from parent CMakeLists.txt):
#   add_subdirectory(firmware/anon)
#   target_link_libraries(np_application PRIVATE np_anon_scratch)

cmake_minimum_required(VERSION 3.20)

project(np_anon_scratch C)

# Path to the bootloader headers (np_config.h: NP_SNVS_BASE + Scratch LBAs).
set(NP_BOOTLOADER_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../bootloader/include)

# ── Toolchain guard ────────────────────────────────────────────────────────────
# Only required for the cross-compiled library; the host test target below is
# built without cross-compilation.
if(NOT NP_BUILD_TESTS AND NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "This project requires the ARM cross-compiler toolchain.\n"
        "Pass -DCMAKE_TOOLCHAIN_FILE=../../cmake/arm-none-eabi.cmake")
endif()

# ── Sources ────────────────────────────────────────────────────────────────────
set(ANON_SOURCES
    src/np_anon_scratch.c
)

# ── Cross-compiled static library (target firmware) ─────────────────────────────
if(CMAKE_CROSSCOMPILING)
    add_library(np_anon_scratch STATIC ${ANON_SOURCES})

    target_include_directories(np_anon_scratch
        PUBLIC  include
        PRIVATE ${NP_BOOTLOADER_INCLUDE_DIR} ${NP_PLATFORM_INCLUDE_DIRS}
    )

    set(CPU_FLAGS
        -mcpu=cortex-m7
        -mthumb
        -mfpu=fpv5-d16
        -mfloat-abi=hard
    )

    target_compile_options(np_anon_scratch PRIVATE
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

# ── Host test build (NOT cross-compiled) ────────────────────────────────────────
# Exercises the init/write/read/complete cycle and security invariants using
# the NPTEST_HOST HAL stubs in np_anon_scratch.c.
if(NP_BUILD_TESTS)
    add_executable(np_anon_tests
        tests/np_anon_tests.c
        src/np_anon_scratch.c
    )
    target_include_directories(np_anon_tests PRIVATE
        include
        ${NP_BOOTLOADER_INCLUDE_DIR}
    )
    target_compile_options(np_anon_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g -DNPTEST_HOST
    )
endif()
