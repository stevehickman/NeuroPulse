# NeuroPulse Device Factory Reset — CMake Build
# Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
# Document: NP-FW-EMMC-002 Rev A §B
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
#   add_subdirectory(firmware/factory_reset)
#   target_link_libraries(np_application PRIVATE np_factory_reset)

cmake_minimum_required(VERSION 3.20)

project(np_factory_reset C)

# ── Toolchain guard ────────────────────────────────────────────────────────────
if(NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "This project requires the ARM cross-compiler toolchain.\n"
        "Pass -DCMAKE_TOOLCHAIN_FILE=../../cmake/arm-none-eabi.cmake")
endif()

# ── Sources ────────────────────────────────────────────────────────────────────
set(FR_SOURCES
    src/np_factory_reset.c
)

add_library(np_factory_reset STATIC ${FR_SOURCES})

target_include_directories(np_factory_reset
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

target_compile_options(np_factory_reset PRIVATE
    ${CPU_FLAGS}
    -std=c11
    -Wall
    -Wextra
    -Werror
    -O2
    -ffunction-sections
    -fdata-sections
    -fno-common
    -ffreestanding
    -fstack-usage
)

# ── Host smoke test (host, not cross-compiled) ─────────────────────────────────
# Builds a host-executable that runs np_factory_reset_is_in_progress() (and the
# rest of the R-5..R-10 logic) against the host-backed SNVS word and no-op HAL
# stubs, confirming the SNVS read path does not crash on the host.  Real
# hardware verification is i.MX RT1062 bench work.
if(NP_BUILD_TESTS)
    add_executable(np_factory_reset_tests
        tests/np_factory_reset_tests.c
        src/np_factory_reset.c
    )
    target_include_directories(np_factory_reset_tests PRIVATE include)
    target_compile_options(np_factory_reset_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g -DNPTEST_HOST
    )
endif()
