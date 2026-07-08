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
)

add_library(np_zone_announce STATIC ${ZA_SOURCES})

target_include_directories(np_zone_announce
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
