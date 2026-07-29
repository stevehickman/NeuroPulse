# NeurOne Zone Module Announcement — CMake Build
# Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
# Document: NP-FW-ZA-001 Rev A
#
# Compiled as a static library linked into the main application.
# Does NOT produce a standalone binary.
#
# Prerequisites:
#   - ARM GNU Toolchain (arm-none-eabi-gcc ≥ 12.3)
#   - CMake ≥ 3.20
#   - Parent project must provide:
#       NP_PLATFORM_INCLUDE_DIRS — paths to FreeRTOS headers and platform HAL
#
# Usage (from parent CMakeLists.txt):
#   add_subdirectory(firmware/zone_announce)
#   target_link_libraries(np_application PRIVATE np_zone_announce)
#
# Platform HAL stubs (np_za_platform_*) must be implemented in the
# application target or a separate np_platform library; they are declared
# extern in np_zone_announce.h and link at application link time.

cmake_minimum_required(VERSION 3.20)

project(np_zone_announce C)

option(NP_BUILD_TESTS "Build host-native unit tests (no ARM toolchain needed)" OFF)

# ── Host unit-test build ───────────────────────────────────────────────────────
# np_zone_notify is pure C with no HAL and no FreeRTOS dependency — it is the
# frame encoder for the module-status characteristic, so it is host-testable and
# its tests pin the wire contract the iOS GATTParser decodes.
#
# It includes np_module_map.h only for the static assertions that keep the wire
# socket/lobe/side domains pinned to the addressing domains they mirror, so the
# hub_control include dir is on the path but no hub_control source is linked.
if(NP_BUILD_TESTS)
    add_executable(np_zone_notify_tests
        tests/np_zone_notify_tests.c
        src/np_zone_notify.c
    )
    target_include_directories(np_zone_notify_tests
        PRIVATE include ${CMAKE_CURRENT_SOURCE_DIR}/../hub_control/include)
    target_compile_options(np_zone_notify_tests
        PRIVATE -Wall -Wextra -Werror -std=c11)

    # Slot state machine. Host-testable for the first time here: the platform HAL
    # (LPADC, SAI3, tick) is stubbed in the test, so the insertion path can be
    # asserted behaviourally — above all that it notifies the app and starts NO
    # bone-conduction audio.
    add_executable(np_zone_announce_tests
        tests/np_zone_announce_tests.c
        src/np_zone_announce.c
        src/np_zone_detect.c
        src/np_zone_audio.c
        src/np_zone_notify.c
    )
    target_include_directories(np_zone_announce_tests
        PRIVATE include ${CMAKE_CURRENT_SOURCE_DIR}/../hub_control/include)
    target_compile_options(np_zone_announce_tests
        PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter -std=c11)
    return()
endif()

# ── Toolchain guard ────────────────────────────────────────────────────────────
if(NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "This project requires the ARM cross-compiler toolchain.\n"
        "Pass -DCMAKE_TOOLCHAIN_FILE=../../cmake/arm-none-eabi.cmake")
endif()

# ── Sources ────────────────────────────────────────────────────────────────────
set(ZA_SOURCES
    src/np_zone_detect.c
    src/np_zone_audio.c
    src/np_zone_announce.c
    src/np_zone_notify.c
)

add_library(np_zone_announce STATIC ${ZA_SOURCES})

target_include_directories(np_zone_announce
    PUBLIC  include
    # np_zone_notify.c includes np_module_map.h for the static assertions that
    # pin the wire domains to the addressing domains.
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../hub_control/include
            ${NP_PLATFORM_INCLUDE_DIRS}
)

# ── Compiler flags ─────────────────────────────────────────────────────────────
set(CPU_FLAGS
    -mcpu=cortex-m7
    -mthumb
    -mfpu=fpv5-d16
    -mfloat-abi=hard
)

target_compile_options(np_zone_announce PRIVATE
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

# ── No external library dependencies ──────────────────────────────────────────
# All audio synthesis uses integer arithmetic (DDS with Q15 LUT).
# No libm required.
