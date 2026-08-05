# NeurOne sLORETA-guided HD-tDCS — CMake Build
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
# Mirrors the root firmware/CMakeLists.txt guard: the ARM toolchain is required
# for a device build, but the host unit-test build (-DNP_BUILD_TESTS=ON) is a
# legitimate non-cross configuration.  Without the NP_BUILD_TESTS escape this
# fires before the host test target below, making np_hd_fai_tests unreachable
# in any host build.
if(NOT NP_BUILD_TESTS AND NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "This project requires the ARM cross-compiler toolchain.\n"
        "Pass -DCMAKE_TOOLCHAIN_FILE=../../cmake/arm-none-eabi.cmake\n"
        "or build only host unit tests with -DNP_BUILD_TESTS=ON.")
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
# The Cortex-M7 and freestanding/section-GC flags are meaningful only for the
# cross-compiled device build; a host compiler rejects -mcpu=cortex-m7 outright.
if(CMAKE_CROSSCOMPILING)
    target_compile_options(np_sloreta_hdtdcs PRIVATE
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
        -ffast-math
        -fno-math-errno
        -fstack-usage
    )
else()
    # Host build: same warning discipline, no target-specific codegen flags.
    # NPTEST_HOST drops the ARM linker-script section placement (see
    # src/np_hd_session.c) that a host object format has no equivalent for.
    target_compile_options(np_sloreta_hdtdcs PRIVATE
        -std=c11
        -Wall
        -Wextra
        -Wno-unused-parameter
        -O2
        -DNPTEST_HOST
    )
endif()

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
