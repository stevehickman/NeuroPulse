# NeurOne Safety MCU Firmware — CMake Build
# Target: STMicroelectronics STM32G071 (Cortex-M0+, 64 MHz)
# Document: NP-SW-001 Rev A — SW-01 Class C firmware
#           NP-FW-EMMC-001 Rev A §4.2, NP-FMEA-001 Rev A
#
# IEC 62304 Class C — safety-critical; MISRA C:2012 mandatory.
# Safety MCU owns all stimulation GPIO enable lines (10 since NP-HW-HUB-001
# Rev C §7.2 collapsed the five per-zone PBM enables to one cranial enable).
# SPI slave to i.MX RT1062 main processor; 200ms heartbeat, 1.5s watchdog.
#
# ── SW-01 Modules (NP-SW-001 Rev A §4.1) ─────────────────────────────────────
#   SW01-M01  np_gpio_mgr          — stimulation GPIO owner (10 lines; one
#                                     cranial PBM line since NP-HW-HUB-001 §7.2)
#   SW01-M02  np_spi_watchdog      — 200ms heartbeat + 1.5s watchdog cutoff
#   SW01-M03  np_charge_monitor    — per-electrode charge accumulator (40µC/cm²)
#   SW01-M04  np_thermal_interlock — NTC sense-domain thermal cutoff (62°C junction)
#   SW01-M05  np_cardiac_interlock — cervical VNS cardiac rhythm monitor
#   SW01-M06  np_impedance_check   — pre-session contact impedance verification
#   SW01-M07  np_session_sig       — Ed25519 session descriptor signature gate
#   SW01-M08  np_fault_latch       — non-volatile fault latch in backup SRAM
#
# ── Build ─────────────────────────────────────────────────────────────────────
#   cmake -B build/safety_mcu firmware/safety_mcu \
#         -DCMAKE_TOOLCHAIN_FILE=../cmake/stm32g071.cmake \
#         -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/safety_mcu
#
# Produces:
#   np_safety_mcu.elf  — ELF with debug info
#   np_safety_mcu.bin  — raw binary for SPI flash via main processor DFU
#   np_safety_mcu.hex  — Intel HEX for STM32CubeProgrammer
#
# NOTE: Safety MCU firmware is NEVER updated via automated OTA.
#       Update requires explicit user confirmation + per-page SPI flash with
#       readback verification per NP-FW-EMMC-001 Rev A §8.4.

cmake_minimum_required(VERSION 3.20)

project(np_safety_mcu C ASM)

option(NP_BUILD_TESTS "Build host-native unit tests (no ARM toolchain needed)" OFF)

# ── Host unit tests (NP_BUILD_TESTS=ON) ───────────────────────────────────────
# Tests exercise the SPI protocol structs, NP_SAFETY_STATUS_SIG_PENDING
# lifecycle logic, and charge monitor logic without requiring ARM
# cross-compilation or SPI hardware.
# OI-SW01-M07-01 CLOSED — sig cmd frame tests.
# OI-CHARGE-01   CLOSED — ext frame + charge monitor tests.
if(NP_BUILD_TESTS)
    # Helper macro to apply common host-test properties
    macro(np_host_test target)
        target_include_directories(${target} PRIVATE include)
        set_target_properties(${target} PROPERTIES
            C_STANDARD          11
            C_STANDARD_REQUIRED ON
            C_EXTENSIONS        OFF
        )
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endmacro()

    # np_safety_spi_proto_tests: SPI frame struct tests (sig cmd + ext heartbeat)
    # np_hub_enable_mirror.c is a test-only TU that reads the hub's enable-word
    # macros, so the "must match hub_control/np_hub_config.h" contract in
    # np_safety_protocol.h can be checked instead of merely stated
    # (NP-HW-HUB-001 Rev C §7.2).  The two headers cannot share a TU.
    add_executable(np_safety_spi_proto_tests
        tests/np_safety_spi_proto_tests.c
        tests/np_hub_enable_mirror.c
    )
    np_host_test(np_safety_spi_proto_tests)

    # np_charge_monitor_tests: SW01-M03 charge density limit logic
    add_executable(np_charge_monitor_tests
        tests/np_charge_monitor_tests.c
        src/np_charge_monitor.c
    )
    np_host_test(np_charge_monitor_tests)

    # np_thermal_interlock_tests: SW01-M04 NTC cutoff.  Covers the §7.2 change
    # that every cranial thermal sense domain now cuts the single
    # NP_SAFETY_EN_PBM_CRANIAL bit — the retired `~(1U << ch)` expression cut
    # nothing for domains 1-4 once bits 1-4 became reserved.
    add_executable(np_thermal_interlock_tests
        tests/np_thermal_interlock_tests.c
        src/np_thermal_interlock.c
    )
    np_host_test(np_thermal_interlock_tests)

    # np_impedance_report_tests: SW01-M06 per-electrode CVNS impedance report
    # for the hub cross-validation (OI-CVNS-HUB-11).
    add_executable(np_impedance_report_tests
        tests/np_impedance_report_tests.c
        src/np_impedance_check.c
    )
    np_host_test(np_impedance_report_tests)

    # np_fault_latch_privacy_tests: SW01-M08 privacy-gated read marshalling —
    # tick_ms/count → SHDR, tick_ms suppressed for cardiac faults
    # (CLAUDE.md §13.4, NP-FW-EMMC-001 §12).  NPTEST_HOST drops the .fault_latch
    # Mach-O section attribute so the source builds host-native.
    add_executable(np_fault_latch_privacy_tests
        tests/np_fault_latch_privacy_tests.c
        src/np_fault_latch.c
    )
    np_host_test(np_fault_latch_privacy_tests)
    target_compile_definitions(np_fault_latch_privacy_tests PRIVATE NPTEST_HOST)

    # np_cardiac_interlock_tests: SW01-M05 cervical VNS cardiac rhythm interlock —
    # baseline arming, ±15 BPM cutoff, FMEA-M05-02 signed-delta underflow guard,
    # 30 s re-enable lockout, conservative pre-baseline hold (NP-FW-CVNS-001 §5).
    # Distinct from np_cvns_fai_tests, which covers the MAIN PROCESSOR module on
    # the other side of the SPI boundary.
    add_executable(np_cardiac_interlock_tests
        tests/np_cardiac_interlock_tests.c
        src/np_cardiac_interlock.c
    )
    np_host_test(np_cardiac_interlock_tests)

    return()
endif()

# ── Toolchain guard ────────────────────────────────────────────────────────────
if(NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "Safety MCU requires the ARM cross-compiler toolchain.\n"
        "Pass -DCMAKE_TOOLCHAIN_FILE=../cmake/stm32g071.cmake\n"
        "or build host unit tests with -DNP_BUILD_TESTS=ON.")
endif()

# ── STM32G071 device define ────────────────────────────────────────────────────
# STM32G071xx selects the device header out of the CMSIS family header, and is
# also the gate on the #include in np_safety_config.h — it is set here, below the
# NP_BUILD_TESTS return(), so it is a cross-build-only define by construction.
#
# USE_HAL_DRIVER is deliberately NOT defined.  stm32g0xx.h ends with
#   #if defined (USE_HAL_DRIVER)
#    #include "stm32g0xx_hal.h"
#   #endif
# so defining it demands the ST HAL, which OI-SWCI-06 resolved NOT to vendor
# (NP-SW-CI-001 §9.4: every vendored file is Class C SOUP surface, and this
# firmware makes zero HAL_* and zero LL_* calls).  It was set before any device
# header existed in-tree, so it had never been load-bearing; it is now.
add_compile_definitions(
    STM32G071xx
)

# ── Sources ───────────────────────────────────────────────────────────────────
set(SAFETY_SOURCES
    startup/startup_stm32g071xx.s
    src/np_safety_main.c
    src/np_gpio_mgr.c
    src/np_spi_watchdog.c
    src/np_charge_monitor.c
    src/np_thermal_interlock.c
    src/np_cardiac_interlock.c
    src/np_impedance_check.c
    src/np_session_sig.c
    src/np_fault_latch.c
)

# ── CPU flags (Cortex-M0+, no FPU) ────────────────────────────────────────────
set(CPU_FLAGS
    -mcpu=cortex-m0plus
    -mthumb
    -mfloat-abi=soft
)

# ── Shared crypto library (OI-SW01-M07-02 CLOSED) ─────────────────────────────
# np_crypto provides np_ed25519_verify, np_sha256, np_sha512, np_crc32.
# Backed by Monocypher 4.0.2 (RFC 8032, SHA-512) — see firmware/crypto/.
# CMAKE_C_FLAGS_INIT (set by stm32g071.cmake) already carries -mcpu=cortex-m0plus
# so np_crypto compiles with the correct CPU flags without any extra step here.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../crypto
                 ${CMAKE_CURRENT_BINARY_DIR}/np_crypto_build)

# ── Compilation unit ──────────────────────────────────────────────────────────
# Every Class C translation unit, compiled for STM32G071 — and nothing else.
# Split out of the executable so COMPILING and LINKING are separately buildable
# targets, because as of 2026-08-09 only the first of the two can succeed:
# Defect E (NP-SW-CI-001 §4.4) leaves 24 first-party np_hal_* platform symbols
# undefined, so the image cannot link until the safety MCU's platform layer is
# written.  Compilation for target is exactly the property Defect A broke, it is
# a real regression surface, and it is gated on its own in CI (§5.2, phase 4)
# instead of being held hostage to an unrelated open defect.
#
# `cmake --build <dir> --target np_safety_mcu_objs` is the gated CI step.
add_library(np_safety_mcu_objs OBJECT ${SAFETY_SOURCES})

target_include_directories(np_safety_mcu_objs PRIVATE include)

# Vendored CMSIS SOUP — two components, two upstreams, two records:
#   ../vendor/cmsis_core       ARM CMSIS-Core(M) 5.6.0 from CMSIS_5 @ 5.9.0
#   ../vendor/cmsis_device_g0  ST CMSIS-Device STM32G0 @ v1.4.5
# See each directory's VERSION and NP-SW-001 §9.5/§9.6.
#
# SYSTEM, so the -Wall -Wextra -Werror below stays fully in force on first-party
# code while diagnostics from the vendored headers are suppressed.  This is the
# header-only analogue of the -w the Monocypher precedent puts on SOUP *sources*;
# no warning is disabled repo-wide and no first-party file loses a diagnostic.
target_include_directories(np_safety_mcu_objs SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/cmsis_core/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/cmsis_device_g0/Include
)

# Usage requirements only (np_crypto's headers, for np_session_sig.c).  Linking
# np_crypto into the image stays on the executable target below.
target_link_libraries(np_safety_mcu_objs PRIVATE np_crypto)

# C standard: enforce C11 without GNU extensions (MISRA C:2012 requires this).
set_target_properties(np_safety_mcu_objs PROPERTIES
    C_STANDARD          11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS        OFF
)

target_compile_options(np_safety_mcu_objs PRIVATE
    ${CPU_FLAGS}
    -Wall
    -Wextra
    -Werror
    -O2
    -ffunction-sections
    -fdata-sections
    -fno-common
    -ffreestanding
    -fstack-usage
    # MISRA C:2012 advisory checks (full enforcement via MISRA tool at G2 gate)
    -Wno-unused-parameter    # remove after HAL stubs filled in
)

# ── Firmware image ─────────────────────────────────────────────────────────────
# Links the objects above.  BLOCKED on Defect E (NP-SW-CI-001 §4.4) — 24 first-party
# np_hal_* platform symbols are undefined, so this target cannot link today and is
# deliberately NOT what CI gates on at phase 4.  It stays declared so the day the
# platform layer lands the link is already wired and CI can be pointed at it.
add_executable(np_safety_mcu $<TARGET_OBJECTS:np_safety_mcu_objs>)

target_link_libraries(np_safety_mcu PRIVATE np_crypto)

# -specs=nano.specs / -specs=nosys.specs are NOT repeated here.  They are already
# in CMAKE_EXE_LINKER_FLAGS_INIT in firmware/cmake/stm32g071.cmake, which is the
# single source for them; naming them again put each on the link line twice.
# ARM GNU 14.2.1 rejects the repeat outright —
#   nano.specs: attempt to rename spec 'link' to already defined spec 'nano_link'
# — so the duplication was a latent build break, not a harmless one.  It had
# never been reachable: the link is downstream of Defect A, which stopped this
# firmware at compile time, so nothing had linked it since the duplication
# landed.  Same defect shape as Defect C (NP-SW-CI-001 §4.3): one value, two
# places to write it.  Deduplicated rather than version-guarded.
target_link_options(np_safety_mcu PRIVATE
    ${CPU_FLAGS}
    -T${CMAKE_CURRENT_SOURCE_DIR}/startup/stm32g071_flash.ld
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/np_safety_mcu.map
)

# ── Post-build: binary and hex ─────────────────────────────────────────────────
add_custom_command(TARGET np_safety_mcu POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:np_safety_mcu>
            ${CMAKE_CURRENT_BINARY_DIR}/np_safety_mcu.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:np_safety_mcu>
            ${CMAKE_CURRENT_BINARY_DIR}/np_safety_mcu.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:np_safety_mcu>
    COMMENT "Generating np_safety_mcu.bin and np_safety_mcu.hex"
)
