# NeurOne Firmware — Root Super-Project
# Document: NP-FW-HUB-001 Rev A (⚠ registered but NOT WRITTEN — docs/np_fw_hub_001.md
#           does not exist; this code is its only description. See OI-DOC-01),
#           NP-FW-EMMC-001 Rev A, NP-SW-001 Rev A
#
# Aggregates all i.MX RT1062 (main processor) firmware modules.
# The STM32G071 safety MCU has its own separate project in firmware/safety_mcu/.
#
# ── Build modes ──────────────────────────────────────────────────────────────
#
# Cross-compiled (requires ARM GNU toolchain):
#   cmake -B build/firmware \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
#         -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/firmware
#
# Host unit tests (no ARM toolchain required):
#   cmake -B build/host-tests -DNP_BUILD_TESTS=ON
#   cmake --build build/host-tests
#   ctest --test-dir build/host-tests --output-on-failure
#
# Safety MCU (separate toolchain, separate build):
#   cmake -B build/safety_mcu firmware/safety_mcu \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/stm32g071.cmake \
#         -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/safety_mcu
#
# ── Module inventory (NP-SW-001 Rev A) ───────────────────────────────────────
#
# SW-02 (Class B) — i.MX RT1062 main processor firmware modules:
#   firmware/vendor/freertos/ — vendored FreeRTOS-Kernel V11.3.0 SOUP (SW-02 RTOS)
#   firmware/bootloader/      — dual-bank OTA bootloader, Ed25519, DFU
#   firmware/hub_control/     — FreeRTOS hub control (module registry, session
#                                 runner, session log, safety SPI heartbeat)
#   firmware/pbm_1064nm/      — 1064nm smart zone module detection + drive
#   firmware/hrv_biofeedback/ — HRV+taVNS biofeedback protocol library
#   firmware/zone_announce/   — zone-module insertion FSM → companion-app notify
#                                 (module-status GATT characteristic).  The Rev A
#                                 bone-conduction insertion cue is retired: it
#                                 fired with the helmet off the head, so it could
#                                 never be heard.  See the header of
#                                 src/np_zone_announce.c and docs/np_fw_za_001.md.
#   firmware/sloreta_hdtdcs/  — sLORETA-guided HD-tDCS (T2)
#   firmware/cervical_vns/    — Cervical VNS main-processor side (T2)
#   firmware/anon/            — Research anonymisation Scratch encryption
#   firmware/edf/             — EDF+ privacy header writer/validator
#   firmware/factory_reset/   — Device factory reset (eMMC SANITIZE + TRNG)
#   firmware/uhdr_key/        — Two-layer UHDR key scheme (UKMD + WKMD)
#
# SW-01 (Class C) — Safety MCU firmware:
#   firmware/safety_mcu/      — STM32G071 bare-metal, separate build
#
# ── Host-test modules ─────────────────────────────────────────────────────────
# Modules with NP_BUILD_TESTS host targets:
#   firmware/crypto/          → np_crypto_tests (SHA-256/512, CRC-32, Ed25519)
#   firmware/anon/            → np_anon_tests
#   firmware/edf/             → np_edf_tests
#   firmware/factory_reset/   → np_factory_reset_tests
#   firmware/uhdr_key/        → np_uhdr_key_tests  (NP-FW-EMMC-002 §C)
#   firmware/ota/             → np_ota_tests  (OTA state CRC + rollback logic)
#   firmware/safety_mcu/      → np_safety_spi_proto_tests (OI-SW01-M07-01)
#                             → np_charge_monitor_tests   (OI-CHARGE-01)
#                             → np_impedance_report_tests (OI-CVNS-HUB-11)
#                             → np_fault_latch_privacy_tests (fault-latch privacy gate)
#   firmware/hub_control/     → np_cvns_reenable_tests    (OI-CVNS-HUB-01)
#                             → np_log_backend_tests      (OI-LOG-01..04)
#                             → np_transport_tests        (OI-HUB-MAIN-01)
#                             → np_mod_cvns_tests         (hub ↔ CVNS library)
#                             → np_module_map_tests       (hex zone-module addressing/NVRAM map)
#                             → np_protocol_tests         (hub protocol v2 socket-mask targets)
#                             → np_mod_stim_tests         (BES/tACS + tDCS slot dispatch)
#   firmware/zone_announce/   → np_zone_announce_tests    (module-status frame encoder)
#                             → np_zone_notify_tests      (app-notify dispatch)
#   firmware/pbm_1064nm/      → np_pbm1064_session_desc_tests (NP-SES-1064-001 v5 wire format)
#   firmware/cervical_vns/    → np_cvns_fai_tests         (FAI CV01..CV06, cardiac interlock)
#   firmware/sloreta_hdtdcs/  → np_hd_fai_tests           (FAI montage + sLORETA algorithm)
#   firmware/vendor/freertos/ → np_freertos_smoke_tests   (kernel + config, POSIX port)
#
# Every module listed above must also appear in the `paths:` filters of
# .github/workflows/firmware-host-tests.yml, or edits to it run no CI.

cmake_minimum_required(VERSION 3.20)

# ── Project declaration ───────────────────────────────────────────────────────
# The project language depends on whether we're building for a host or the MCU.
# Both use C; C++ is not used in this firmware.
project(neurone_firmware C ASM)

# ── Global options ─────────────────────────────────────────────────────────────
option(NP_BUILD_TESTS  "Build host-native unit tests (no ARM toolchain needed)" OFF)
option(NP_BUILD_BOOTLOADER "Include the bootloader in the build"               ON)

# ── Toolchain guard (cross-compiled modules) ──────────────────────────────────
if(NOT NP_BUILD_TESTS AND NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "This firmware must be built with the ARM cross-compiler toolchain.\n"
        "Pass -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi.cmake\n"
        "or build only host unit tests with -DNP_BUILD_TESTS=ON.")
endif()

# ── Platform include paths (MCUX SDK + FreeRTOS) ──────────────────────────────
# FreeRTOS is vendored in-tree (firmware/vendor/freertos, SOUP: NP-SW-001 §9.4)
# and its headers are wired in below. The MCUX SDK path is still an empty stub —
# a real device integration populates it from the installed SDK.
# The per-module CMakeLists files consume NP_PLATFORM_INCLUDE_DIRS as a list.
# Normal (directory-scope) set, not CACHE: a CACHE set() is ignored when the
# cache already holds a value, which would silently pin a stale include list on
# reconfigure. This runs before the add_subdirectory() calls, so subdirectories
# inherit it. Append the MCUX SDK path here when integrating the full SDK.
if(CMAKE_CROSSCOMPILING)
    set(NP_PLATFORM_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/freertos/kernel/include
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/freertos/portable/GCC/ARM_CM7/r0p1
        ${CMAKE_CURRENT_SOURCE_DIR}/hub_control/include   # FreeRTOSConfig.h
        # ← append MCUX SDK path (e.g. $ENV{MCUX_SDK}/devices/MIMXRT1062/...)
    )
endif()

# ── Host unit-test build ───────────────────────────────────────────────────────
if(NP_BUILD_TESTS)
    enable_testing()

    add_subdirectory(crypto)
    add_subdirectory(anon)
    add_subdirectory(edf)
    add_subdirectory(factory_reset)
    add_subdirectory(uhdr_key)     # NP-FW-EMMC-002 §C: two-layer UHDR key
    add_subdirectory(ota)
    add_subdirectory(safety_mcu)  # OI-SW01-M07-01: SPI frame tests
                                  # OI-CHARGE-01:   charge monitor tests
    add_subdirectory(hub_control) # OI-CVNS-HUB-01: CVNS re-enable manager tests
    add_subdirectory(zone_announce) # module-status frame encoder (app notify path)
    add_subdirectory(pbm_1064nm)  # NP-SES-1064-001 v5: session descriptor wire format
    add_subdirectory(cervical_vns)   # NP-FAI-CVNS-001: cardiac interlock FAI suite
    add_subdirectory(sloreta_hdtdcs) # NP-FW-HD-001: montage + sLORETA FAI suite
    add_subdirectory(vendor/freertos) # NP-SW-001 §9.4: FreeRTOS kernel smoke test

    add_test(NAME np_crypto_tests
        COMMAND np_crypto_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_anon_tests
        COMMAND np_anon_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_edf_tests
        COMMAND np_edf_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_factory_reset_tests
        COMMAND np_factory_reset_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_uhdr_key_tests
        COMMAND np_uhdr_key_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_ota_tests
        COMMAND np_ota_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_safety_spi_proto_tests
        COMMAND np_safety_spi_proto_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_charge_monitor_tests
        COMMAND np_charge_monitor_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_impedance_report_tests
        COMMAND np_impedance_report_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_fault_latch_privacy_tests
        COMMAND np_fault_latch_privacy_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_cvns_reenable_tests
        COMMAND np_cvns_reenable_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_module_map_tests
        COMMAND np_module_map_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_protocol_tests
        COMMAND np_protocol_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_mod_stim_tests
        COMMAND np_mod_stim_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_log_backend_tests
        COMMAND np_log_backend_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_transport_tests
        COMMAND np_transport_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_mod_cvns_tests
        COMMAND np_mod_cvns_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_zone_notify_tests
        COMMAND np_zone_notify_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_zone_announce_tests
        COMMAND np_zone_announce_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_pbm1064_session_desc_tests
        COMMAND np_pbm1064_session_desc_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_cvns_fai_tests
        COMMAND np_cvns_fai_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_hd_fai_tests
        COMMAND np_hd_fai_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_freertos_smoke_tests
        COMMAND np_freertos_smoke_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    # Backstop against a scheduler hang so a failure reports rather than blocks.
    set_tests_properties(np_freertos_smoke_tests PROPERTIES TIMEOUT 30)

    return()
endif()

# ── Cross-compiled modules ────────────────────────────────────────────────────
# Execution order: bootloader → {libraries} → hub_control links them all.

add_subdirectory(crypto)
add_subdirectory(vendor/freertos)   # np_freertos kernel library (SW-02 RTOS)

if(NP_BUILD_BOOTLOADER)
    add_subdirectory(bootloader)
endif()

add_subdirectory(ota)
add_subdirectory(zone_announce)
add_subdirectory(hrv_biofeedback)
add_subdirectory(pbm_1064nm)
add_subdirectory(cervical_vns)
add_subdirectory(sloreta_hdtdcs)
add_subdirectory(anon)
add_subdirectory(edf)
add_subdirectory(factory_reset)
add_subdirectory(uhdr_key)

# hub_control links all other libraries and is the top-level application target.
add_subdirectory(hub_control)
