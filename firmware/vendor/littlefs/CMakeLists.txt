# Vendored littlefs v2.11.3
# SOUP — see VERSION and README-NEURONE.md.  Documents: NP-SW-001 §9.4,
# NP-SOUP-LFS-001 Rev 2.  Closes OI-LFS-01 (pin + vendor).
#
# Produces the np_littlefs static library in BOTH modes:
#   - Cross: for the SW-02 main-processor image (Cortex-M7).  It is built but
#     deliberately linked into no image yet — the OI-LOG-05..07 HAL seams that
#     would call it are unimplemented.  Building it is what makes the pinned
#     bytes and NeurOne's configuration of them compile-checked rather than
#     merely present.
#   - Host (-DNP_BUILD_TESTS=ON): the same sources, same configuration header,
#     no CPU flags, consumed by np_lfs_config_tests in firmware/hub_control.
#
# The configuration is application-owned and lives with the hub control program
# (single source of truth), exactly as FreeRTOSConfig.h does.

cmake_minimum_required(VERSION 3.20)

set(NP_LITTLEFS_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# NeurOne's build-time configuration header.  See np_lfs_config.h for why this
# is one definition rather than six: LFS_THREADSAFE changes the layout of
# struct lfs_config, so every translation unit that includes lfs.h must see the
# same configuration lfs.c was compiled with.  PUBLIC, so consumers inherit both
# the definition and the include path that resolves it.
set(NP_LITTLEFS_CONFIG_DIR ${NP_LITTLEFS_DIR}/../../hub_control/include
    CACHE PATH "Directory containing NeurOne's np_lfs_config.h")

add_library(np_littlefs STATIC
    ${NP_LITTLEFS_DIR}/lfs.c
    ${NP_LITTLEFS_DIR}/lfs_util.c
)

target_include_directories(np_littlefs PUBLIC
    ${NP_LITTLEFS_DIR}
    ${NP_LITTLEFS_CONFIG_DIR}
)

target_compile_definitions(np_littlefs PUBLIC LFS_DEFINES=np_lfs_config.h)

# Vendored SOUP: build clean but do NOT enforce -Werror on third-party sources.
# Same rule as np_freertos and the MCUX device layer.
target_compile_options(np_littlefs PRIVATE
    -std=c11
    -Wall
    -Wextra
    -ffunction-sections
    -fdata-sections
)

if(NOT CMAKE_CROSSCOMPILING)
    # Host build (NP_BUILD_TESTS): no CPU flags, nothing else to add.
    return()
endif()

# ── Cross build: Cortex-M7 ────────────────────────────────────────────────────
set(NP_LITTLEFS_CPU_FLAGS
    -mcpu=cortex-m7
    -mthumb
    -mfpu=fpv5-d16
    -mfloat-abi=hard
)

target_compile_options(np_littlefs PRIVATE
    ${NP_LITTLEFS_CPU_FLAGS}
    -O2
    -fno-common
    -ffreestanding
)
target_compile_options(np_littlefs PUBLIC ${NP_LITTLEFS_CPU_FLAGS})

# FreeRTOS headers: np_lfs_config.h's LFS_ASSERT target is defined in
# np_lfs_instance.c (hub_control), which halts via taskDISABLE_INTERRUPTS().
# The library itself needs no FreeRTOS header, so nothing is linked here — the
# dependency belongs to the caller, and np_hub_control already has it.
