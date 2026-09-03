# NeurOne SW-02 Application — CMake build
# Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz), FreeRTOS
# Document: NP-SW-CI-001 §4.8 (phase 8, closes OI-SWCI-21), NP-FW-HUB-001
#
# Produces np_application.elf / .bin / .hex — the SW-02 image the bootloader
# stages into OCRAM and jumps to.  Before this target existed the cross-build
# archived thirteen static libraries and linked one ELF (the bootloader), so
# nothing verified that SW-02 links at all.
#
# THE IMAGE IS NOT RUNNABLE.  Its platform layer is firmware/platform/, which
# traps rather than driving hardware.  src/np_app_main.c says so in a string
# inside the binary.

cmake_minimum_required(VERSION 3.20)

# ── Host test: the two linker scripts and np_config.h must agree ─────────────
# Runs in the NP_BUILD_TESTS build, before the cross-compile guard, because it
# needs no ARM toolchain — it reads two text files and includes one header.
if(NP_BUILD_TESTS)
    add_executable(np_app_link_agreement_tests
        tests/np_app_link_agreement_tests.c)
    # The bootloader's include/ — for np_config.h, which since NP-SW-CI-001
    # §4.10 is the THIRD declaration of the FlexRAM partition and the one the
    # device is actually configured from.  Included rather than text-parsed:
    # the other two copies are linker scripts and need the parser below, this
    # one is C and the compiler reads it better than the parser would.
    target_include_directories(np_app_link_agreement_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../bootloader/include)
    target_compile_options(np_app_link_agreement_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g)
    target_compile_definitions(np_app_link_agreement_tests PRIVATE
        NP_BOOTLOADER_LD_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../bootloader/linker/bootloader_imxrt1062.ld"
        NP_APPLICATION_LD_PATH="${CMAKE_CURRENT_SOURCE_DIR}/linker/app_imxrt1062.ld")
    return()
endif()

if(NOT CMAKE_CROSSCOMPILING)
    return()
endif()

# ── Vendored MCUX SDK device layer (SOUP: firmware/vendor/mcux_sdk/VERSION) ───
set(NP_MCUX_DEVICE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/mcux_sdk/devices/MIMXRT1062)
set(NP_CMSIS_CORE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/cmsis_core/Include)

set(CPU_FLAGS
    -mcpu=cortex-m7
    -mthumb
    -mfpu=fpv5-d16
    -mfloat-abi=hard
)

# CPU_MIMXRT1062DVL6A selects the device branch in fsl_device_registers.h.  The
# part number is the one CLAUDE.md §4.1 describes (i.MX RT1062, 600 MHz); the
# package suffix has no effect on anything in the vendored subset, which is
# register definitions and startup, but it has to be one of the recognised
# macros or the header dispatches to #error.
set(NP_MCUX_DEVICE_DEFINE CPU_MIMXRT1062DVL6A)

add_executable(np_application
    src/np_app_main.c
    # DTCM-resident data (NP-SW-CI-001 §4.10, closes OI-SWCI-42): defines
    # ucHeap in .dtcm_bss under configAPPLICATION_ALLOCATED_HEAP, and zeroes
    # that region — the vendored startup zeroes only one bss span and is
    # byte-exact, so a second region is first-party work by construction.
    src/np_app_dtcm.c
    ${NP_MCUX_DEVICE_DIR}/system_MIMXRT1062.c
    ${NP_MCUX_DEVICE_DIR}/gcc/startup_MIMXRT1062.S
)

# CMake gives a cross-compiled executable no suffix, so without this the target
# produces a file named `np_application` while every document calls it
# `np_application.elf` — and a CI step globbing '*.elf' then reports on nothing
# and exits 0.  That is OI-SWCI-35, found on the safety-MCU leg in phase 7; it
# is not repeated here.
set_target_properties(np_application PROPERTIES SUFFIX ".elf")

target_include_directories(np_application PRIVATE
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/../hub_control/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/include
)

# SYSTEM so -Wall -Wextra -Werror stays in force on first-party code while the
# vendored headers' own diagnostics are suppressed — the same treatment
# firmware/safety_mcu gives the CMSIS headers.
target_include_directories(np_application SYSTEM PRIVATE
    ${NP_MCUX_DEVICE_DIR}
    ${NP_CMSIS_CORE_DIR}
)

target_compile_definitions(np_application PRIVATE ${NP_MCUX_DEVICE_DEFINE})

target_compile_options(np_application PRIVATE
    ${CPU_FLAGS}
    $<$<COMPILE_LANGUAGE:C>:-std=c11>
    -Wall
    -Wextra
    -Werror
    -O2
    -ffunction-sections
    -fdata-sections
    -fno-common
    -ffreestanding
)

# ── Assembling the vendored startup ───────────────────────────────────────────
#
# __STARTUP_CLEAR_BSS   the C library startup normally zeroes .bss.  This image
#                       does not run one, so the startup must.
# __START=main          the default branch target is newlib's _start, which
#                       expects a hosted environment (_sbrk, _write, _exit).
#                       This image is -ffreestanding and takes its heap from
#                       FreeRTOS heap_4, so it enters main() directly and
#                       __libc_init_array never runs — which is correct here:
#                       there are no C++ constructors and no .init_array
#                       entries (the linker script keeps the sections so the
#                       symbols resolve, and they come out empty).
#
# Both are switches the vendored file already tests for.  Neither is a patch, so
# the copy under firmware/vendor/ stays byte-exact.
set_source_files_properties(${NP_MCUX_DEVICE_DIR}/gcc/startup_MIMXRT1062.S
    PROPERTIES COMPILE_OPTIONS "-D__STARTUP_CLEAR_BSS;-D__START=main")

# ── Link ──────────────────────────────────────────────────────────────────────
#
# --whole-archive on the first-party libraries is what makes this target worth
# having.  Without it, ld pulls only the archive members that resolve an
# already-undefined symbol, so a module nothing references contributes no
# objects and its unresolved platform seams are never demanded — the link goes
# green having checked a subset of SW-02.  That is the same shape as OI-SWCI-21
# itself, one level down: a check that passes because it never looked.  With it,
# every SW-02 translation unit is in the link and every symbol any of them
# references has to resolve.
#
# --gc-sections still runs afterwards and drops what the entry point cannot
# reach, so forcing the objects in costs image size only for code that is
# genuinely reachable.  Symbol resolution happens first, which is why the two
# are not in conflict.
set(NP_APP_LIBS
    np_hub_control
    np_pbm
    np_hrv_biofeedback
    np_zone_announce
    np_cervical_vns
    np_sloreta_hdtdcs
    np_anon_scratch
    np_edf
    np_factory_reset
    np_uhdr_key
    np_accel_shdr
    np_ota_state
    np_crypto
    np_freertos
    np_platform
)

target_link_libraries(np_application PRIVATE
    -Wl,--whole-archive
    ${NP_APP_LIBS}
    -Wl,--no-whole-archive
    m
)

set(NP_APP_LINKER_SCRIPT
    "${CMAKE_CURRENT_SOURCE_DIR}/linker/app_imxrt1062.ld")

target_link_options(np_application PRIVATE
    ${CPU_FLAGS}
    -T ${NP_APP_LINKER_SCRIPT}
    -nostartfiles                   # the vendored startup is the entry, not crt0
    # No --specs=nosys.specs, and its absence is measured rather than assumed.
    # The Class C leg needs libnosys because its link pulls in newlib's reent
    # machinery; this image does not — it uses newlib only for memcpy/memset/
    # memcmp and sinf/cosf/sqrtf, none of which touch a syscall.  Adding the
    # spec anyway links libnosys and produces four "_close/_lseek/_read/_write
    # is not implemented and will always fail" link warnings for stubs nothing
    # reaches.  Measured 2026-09-01: with the spec, 4 warnings; without it,
    # 0 warnings and a byte-identical image (text 47,899 / data 304 /
    # bss 162,132 either way).  Carrying a spec whose only effect is warnings is
    # how a build learns to have warnings.
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -Wl,-Map=${CMAKE_BINARY_DIR}/np_application.map
)
set_target_properties(np_application PROPERTIES
    LINK_DEPENDS ${NP_APP_LINKER_SCRIPT})

# ── Post-build artifacts ──────────────────────────────────────────────────────
find_program(OBJCOPY arm-none-eabi-objcopy REQUIRED)
find_program(SIZE    arm-none-eabi-size    REQUIRED)

add_custom_command(TARGET np_application POST_BUILD
    COMMAND ${OBJCOPY} -O binary
            $<TARGET_FILE:np_application>
            ${CMAKE_BINARY_DIR}/np_application.bin
    COMMAND ${OBJCOPY} -O ihex
            $<TARGET_FILE:np_application>
            ${CMAKE_BINARY_DIR}/np_application.hex
    COMMAND ${SIZE} --format=sysv $<TARGET_FILE:np_application>
    COMMENT "Generating np_application.bin and np_application.hex")
