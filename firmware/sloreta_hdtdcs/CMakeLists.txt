# NeuroPulse sLORETA-guided HD-tDCS — CMake Build
# Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
# Document: NP-FW-HD-001 Rev A
#
# Static library linked into the main application.  Does NOT produce a
# standalone binary.
#
# Prerequisites:
#   - ARM GNU Toolchain (arm-none-eabi-gcc >= 12.3)
#   - CMake >= 3.20
#   - Parent project must provide:
#       NP_PLATFORM_INCLUDE_DIRS — FreeRTOS headers and platform HAL
#
# Usage (from parent CMakeLists.txt):
#   add_subdirectory(firmware/sloreta_hdtdcs)
#   target_link_libraries(np_application PRIVATE np_sloreta_hdtdcs)

cmake_minimum_required(VERSION 3.20)

project(np_sloreta_hdtdcs C)

# ── Toolchain guard ────────────────────────────────────────────────────────────
if(NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "This project requires the ARM cross-compiler toolchain.\n"
        "Pass -DCMAKE_TOOLCHAIN_FILE=../../cmake/arm-none-eabi.cmake")
endif()

# ── Sources ────────────────────────────────────────────────────────────────────
set(HD_SOURCES
    src/np_sloreta.c
    src/np_hd_montage.c
    src/np_hd_stim.c
    src/np_hd_session.c
)

add_library(np_sloreta_hdtdcs STATIC ${HD_SOURCES})

target_include_directories(np_sloreta_hdtdcs
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

target_compile_options(np_sloreta_hdtdcs PRIVATE
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
# sqrtf, fabsf used in sloreta and montage source files.
target_link_libraries(np_sloreta_hdtdcs PUBLIC m)

# ── FAI test build (host, not cross-compiled) ──────────────────────────────────
# Build FAI tests for host execution only (electrode mapping + algorithm tests).
# Hardware-dependent tests (FAI-HD01, FAI-HD03, FAI-HD04) require bench setup.
if(NP_BUILD_TESTS)
    add_executable(np_hd_fai_tests
        tests/np_hd_fai_tests.c
        src/np_sloreta.c
        src/np_hd_montage.c
        src/np_hd_stim.c
        src/np_hd_session.c
    )
    target_include_directories(np_hd_fai_tests PRIVATE include)
    target_compile_options(np_hd_fai_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g -DNPTEST_HOST
    )
    target_link_libraries(np_hd_fai_tests PRIVATE m)
endif()
