# NeuroPulse HRV Biofeedback — CMake Build
# Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
# Document: NP-FW-HRV-001 Rev A
#
# This module is compiled as a static library linked into the main application.
# It does NOT produce a standalone binary — link np_hrv_biofeedback into the
# application target alongside np_bootloader and the platform HAL.
#
# Prerequisites:
#   - ARM GNU Toolchain (arm-none-eabi-gcc ≥ 12.3)
#   - CMake ≥ 3.20
#   - Parent project must provide:
#       NP_PLATFORM_INCLUDE_DIRS — paths to FreeRTOS headers and platform HAL
#
# Usage (from parent CMakeLists.txt):
#   add_subdirectory(firmware/hrv_biofeedback)
#   target_link_libraries(np_application PRIVATE np_hrv_biofeedback)

cmake_minimum_required(VERSION 3.20)

project(np_hrv_biofeedback C)

# ── Toolchain guard ────────────────────────────────────────────────────────────
if(NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "This project requires the ARM cross-compiler toolchain.\n"
        "Pass -DCMAKE_TOOLCHAIN_FILE=../../cmake/arm-none-eabi.cmake")
endif()

# ── Sources ────────────────────────────────────────────────────────────────────
set(HRV_SOURCES
    src/np_hrv_ppg.c
    src/np_hrv_coherence.c
    src/np_hrv_pacer.c
    src/np_hrv_tavns_sync.c
    src/np_hrv_eeg_biofeedback.c
    src/np_hrv_session.c
)

add_library(np_hrv_biofeedback STATIC ${HRV_SOURCES})

target_include_directories(np_hrv_biofeedback
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

target_compile_options(np_hrv_biofeedback PRIVATE
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
    # Math: use hardware FPU for sinf/cosf/sqrtf (no soft-float fallback).
    # These intrinsics are provided by libm; application must link -lm.
    -ffast-math
    -fno-math-errno
)

# ── Dependencies ───────────────────────────────────────────────────────────────
# libm provides sinf, cosf, sqrtf, powf used in HRV DSP.
# The parent application target must also link -lm.
target_link_libraries(np_hrv_biofeedback PUBLIC m)

# ── Static analysis helpers ────────────────────────────────────────────────────
# Generate .su stack-usage files for all TUs (review in CI).
target_compile_options(np_hrv_biofeedback PRIVATE -fstack-usage)
