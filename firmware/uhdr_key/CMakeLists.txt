# NeurOne Two-Layer UHDR Key Scheme — CMake Build
# Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
# Document: NP-FW-EMMC-002 Rev A §C
#
# Static library linked into the main application.  Does NOT produce a
# standalone binary.
#
# Prerequisites:
#   - ARM GNU Toolchain (arm-none-eabi-gcc >= 12.3)
#   - CMake >= 3.20
#   - Parent project must provide:
#       NP_PLATFORM_INCLUDE_DIRS — FreeRTOS headers and platform HAL (TRNG,
#       Argon2id, CAAM/DCP AES-GCM, LittleFS Config, UHDR XTS mount).
#
# Usage (from parent CMakeLists.txt):
#   add_subdirectory(firmware/uhdr_key)
#   target_link_libraries(np_application PRIVATE np_uhdr_key)

cmake_minimum_required(VERSION 3.20)

project(np_uhdr_key C)

# ── Toolchain guard ────────────────────────────────────────────────────────────
# Only required for the cross-compiled library; the host test target below is
# built without cross-compilation.
if(NOT NP_BUILD_TESTS AND NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "This project requires the ARM cross-compiler toolchain.\n"
        "Pass -DCMAKE_TOOLCHAIN_FILE=../../cmake/arm-none-eabi.cmake")
endif()

# ── Sources ────────────────────────────────────────────────────────────────────
set(UHDR_KEY_SOURCES
    src/np_uhdr_key.c
)

# ── Cross-compiled static library (target firmware) ─────────────────────────────
if(CMAKE_CROSSCOMPILING)
    add_library(np_uhdr_key STATIC ${UHDR_KEY_SOURCES})

    target_include_directories(np_uhdr_key
        PUBLIC  include
        PRIVATE ${NP_PLATFORM_INCLUDE_DIRS}
    )

    set(CPU_FLAGS
        -mcpu=cortex-m7
        -mthumb
        -mfpu=fpv5-d16
        -mfloat-abi=hard
    )

    target_compile_options(np_uhdr_key PRIVATE
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
# Exercises the provision/unlock/background/change-credential lifecycle and the
# security invariants using the NPTEST_HOST HAL stubs in np_uhdr_key.c.  The
# stubs model Argon2id/AES-GCM with np_crypto SHA-256 so the wrap authenticates.
if(NP_BUILD_TESTS)
    add_executable(np_uhdr_key_tests
        tests/np_uhdr_key_tests.c
        src/np_uhdr_key.c
    )
    target_include_directories(np_uhdr_key_tests PRIVATE
        include
        ${CMAKE_CURRENT_SOURCE_DIR}/../crypto/include
    )
    target_compile_options(np_uhdr_key_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g -DNPTEST_HOST
    )
    target_link_libraries(np_uhdr_key_tests PRIVATE np_crypto)
endif()
