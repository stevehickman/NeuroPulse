# NeurOne SW-02 Platform Layer — CMake build
# Target: NXP i.MX RT1062 (Cortex-M7)
# Document: NP-SW-CI-001 §4.8 (phase 8, closes OI-SWCI-21)
#
# Produces np_platform, the static library that resolves every SW-02 platform
# symbol.  Every definition in it is a trap, not a driver — see
# include/np_platform_trap.h for why that is the deliberate choice and a
# plausible return value would not be.
#
# This directory is cross-compile-only.  There is no NP_BUILD_TESTS branch and
# that is on purpose: linking np_platform into a host test would replace that
# test's own doubles with functions that halt the process, so every existing
# suite would die at its first HAL call.  The two things worth testing here —
# that the census matches the header, and that the linker script agrees with the
# bootloader's — are tested where they can be: the census by a POST_BUILD step
# below, the linker agreement by np_app_link_agreement_tests in
# firmware/application/.

cmake_minimum_required(VERSION 3.20)

if(NOT CMAKE_CROSSCOMPILING)
    return()
endif()

# ── The expected seam count comes from the header, not from here ──────────────
# One source of truth.  Restating 93 in this file would create exactly the
# duplicate-arithmetic shape that Defect C (NP-SW-CI-001 §4.3) was.
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/include/np_sw02_platform_hal.h
     NP_PLATFORM_HAL_HEADER)
if(NOT NP_PLATFORM_HAL_HEADER MATCHES
       "#define[ \t]+NP_SW02_PLATFORM_SYMBOL_COUNT[ \t]+([0-9]+)")
    message(FATAL_ERROR
        "np_sw02_platform_hal.h does not define NP_SW02_PLATFORM_SYMBOL_COUNT")
endif()
set(NP_SW02_PLATFORM_SYMBOL_COUNT ${CMAKE_MATCH_1})

set(NP_PLATFORM_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../hub_control/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../bootloader/include   # np_config.h, via np_log_backend.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../shdr/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../pbm/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../anon/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../factory_reset/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../uhdr_key/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../zone_announce/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../cervical_vns/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

set(NP_PLATFORM_CPU_FLAGS
    -mcpu=cortex-m7
    -mthumb
    -mfpu=fpv5-d16
    -mfloat-abi=hard
)

# -Wno-unused-parameter is deliberately ABSENT.  NP-SW-CI-001 §4.4.2 retired it
# from the SW-01 platform layer on the reasoning that a silently ignored
# argument in a stimulation seam is the diagnostic that matters most; a layer
# made entirely of stubs is where the suppression is most tempting and least
# affordable.  np_platform_stub.c casts every parameter to void explicitly.
set(NP_PLATFORM_C_FLAGS
    ${NP_PLATFORM_CPU_FLAGS}
    -std=c11
    -Wall
    -Wextra
    -Werror
    -O2
    -ffunction-sections
    -fdata-sections
    -fno-common
    -ffreestanding
)

# ── The 93 traps, as their own object library ─────────────────────────────────
# Separate from the trap primitive below so the census can count exactly one
# object file.  Counting the archive instead would mean parsing nm's per-member
# output and would silently include np_platform_unimplemented itself.
add_library(np_platform_stub_objs OBJECT src/np_platform_stub.c)
target_include_directories(np_platform_stub_objs PRIVATE ${NP_PLATFORM_INCLUDES})
target_compile_options(np_platform_stub_objs PRIVATE ${NP_PLATFORM_C_FLAGS})

find_program(NP_ARM_NM arm-none-eabi-nm REQUIRED)

add_library(np_platform STATIC
    src/np_platform_trap.c
    $<TARGET_OBJECTS:np_platform_stub_objs>
)
target_include_directories(np_platform PUBLIC  include
                                       PRIVATE ${NP_PLATFORM_INCLUDES})
target_compile_options(np_platform PRIVATE ${NP_PLATFORM_C_FLAGS})

# The census hangs off np_platform rather than np_platform_stub_objs because
# CMake does not allow POST_BUILD on an OBJECT library.  It still counts the
# single stub object — np_platform depends on it, so it exists by the time this
# runs, and pointing at the object rather than the archive keeps
# np_platform_unimplemented() itself out of the count.
add_custom_command(TARGET np_platform POST_BUILD
    COMMAND ${CMAKE_COMMAND}
            -DNM=${NP_ARM_NM}
            -DOBJECT=$<TARGET_OBJECTS:np_platform_stub_objs>
            -DEXPECTED=${NP_SW02_PLATFORM_SYMBOL_COUNT}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/np_platform_census.cmake
    COMMAND_EXPAND_LISTS
    VERBATIM
    COMMENT "Asserting the SW-02 platform seam census (NP-SW-CI-001 §4.8)")
