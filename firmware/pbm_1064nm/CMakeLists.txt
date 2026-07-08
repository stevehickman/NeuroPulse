# NeurOne 1064nm Smart Zone Module — CMake Build
# Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
# Document: NP-FW-PBM1064-001 Rev A
#
# Compiled as a static library linked into the main application.
# Does NOT produce a standalone binary — link np_pbm_1064nm into the
# application target alongside np_bootloader and the platform HAL.
#
# Prerequisites:
#   - ARM GNU Toolchain (arm-none-eabi-gcc ≥ 12.3)
#   - CMake ≥ 3.20
#   - Parent project must provide NP_PLATFORM_INCLUDE_DIRS
#
# Usage (from parent CMakeLists.txt):
#   add_subdirectory(firmware/pbm_1064nm)
#   target_link_libraries(np_application PRIVATE np_pbm_1064nm)

cmake_minimum_required(VERSION 3.20)

project(np_pbm_1064nm C)

# ── Toolchain guard ────────────────────────────────────────────────────────────
if(NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "This project requires the ARM cross-compiler toolchain.\n"
        "Pass -DCMAKE_TOOLCHAIN_FILE=../../cmake/arm-none-eabi.cmake")
endif()

# ── Sources ────────────────────────────────────────────────────────────────────
set(PBM1064_SOURCES
    src/np_pbm1064_detect.c
    src/np_pbm1064_drive.c
    src/np_pbm1064_dose.c
    src/np_pbm1064_session.c
    src/np_pbm1064_t2_combined.c
    src/np_pbm1064_hal.c
    src/np_pbm1064_fai.c
)

add_library(np_pbm_1064nm STATIC ${PBM1064_SOURCES})

target_include_directories(np_pbm_1064nm
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

target_compile_options(np_pbm_1064nm PRIVATE
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
target_link_libraries(np_pbm_1064nm PUBLIC m)
