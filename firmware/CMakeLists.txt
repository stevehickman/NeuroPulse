# NeurOne Firmware — Root Super-Project
# Document: NP-FW-HUB-001 Rev 1 (docs/np_fw_hub_001.md — authored 2026-09-13
#           against this tree, closing OI-DOC-01), NP-FW-EMMC-001 Rev 1,
#           NP-SW-001 Rev 1
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
# ── Module inventory (NP-SW-001 Rev 1) ───────────────────────────────────────
#
# SW-02 (Class B) — i.MX RT1062 main processor firmware modules:
#   firmware/vendor/freertos/ — vendored FreeRTOS-Kernel V11.3.0 SOUP (SW-02 RTOS)
#   firmware/vendor/littlefs/ — vendored littlefs v2.11.3 SOUP (SW-02 storage,
#                                 NP-SW-001 §9.4, OI-LFS-01).  Pinned, built, and
#                                 as of OI-LFS-02 anomaly-evaluated and
#                                 power-loss tested (NP-SOUP-LFS-001 Rev 3 §11,
#                                 §12).  STILL CALLED BY NOTHING — the
#                                 OI-LOG-05..07 HAL seams are unimplemented, and
#                                 the L-1…L-4 results are against the
#                                 struct lfs_config contract, not against the
#                                 eMMC behind it (OI-LFS-07).
#   firmware/bootloader/      — dual-bank OTA bootloader, Ed25519, DFU
#   firmware/hub_control/     — FreeRTOS hub control (module registry, session
#                                 runner, session log, safety SPI heartbeat)
#   firmware/pbm/      — 1064nm smart zone module detection + drive
#   firmware/hrv_biofeedback/ — HRV+taVNS biofeedback protocol library
#   firmware/zone_announce/   — zone-module insertion FSM → companion-app notify
#                                 (module-status GATT characteristic).  The Rev 1
#                                 bone-conduction insertion cue is retired: it
#                                 fired with the helmet off the head, so it could
#                                 never be heard.  See the header of
#                                 src/np_zone_announce.c and docs/superseded/np_fw_za_001.md.
#   firmware/sloreta_hdtdcs/  — sLORETA-guided HD-tDCS (T2)
#   firmware/cervical_vns/    — Cervical VNS main-processor side (T2)
#   firmware/anon/            — Research anonymisation Scratch encryption
#   firmware/edf/             — EDF+ privacy header writer/validator
#   firmware/factory_reset/   — Device factory reset (eMMC SANITIZE + TRNG)
#   firmware/uhdr_key/        — Two-layer UHDR key scheme (UKMD + WKMD)
#   firmware/shdr/            — SHDR accelerometer processing: the §G standing
#                                 rule (two derived booleans) plus the §H
#                                 characterisation programme, whose window is
#                                 denominated in records rather than calendar
#                                 time because this device has no clock that
#                                 survives a disconnect.
#
# SW-01 (Class C) — Safety MCU firmware:
#   firmware/safety_mcu/      — STM32G071 bare-metal, separate build
#
# SW-02 (Class B) — the platform layer and the image that links it
# (NP-SW-CI-001 §4.8, phase 8, closes OI-SWCI-21):
#   firmware/vendor/mcux_sdk/ — vendored MCUX SDK 2.16.0 MIMXRT1062 device
#                                 layer SOUP (NP-SW-001 §9.4, OI-SWCI-20):
#                                 device header, SystemInit, GCC startup.
#   firmware/platform/        — the SW-02 platform layer.  93 symbols, every
#                                 one a TRAP rather than a driver: it halts,
#                                 which stops the SPI heartbeat, which makes
#                                 the safety MCU cut all stimulation.  Its
#                                 purpose is to make the driver gap countable,
#                                 not to stand in for drivers.
#   firmware/application/     — np_application.elf, the SW-02 image the
#                                 bootloader stages into OCRAM.  IT LINKS; IT
#                                 DOES NOT RUN, and says so in a string inside
#                                 the binary.
#
# ── Host-test modules ─────────────────────────────────────────────────────────
# Modules with NP_BUILD_TESTS host targets:
#   firmware/crypto/          → np_crypto_tests (SHA-256/512, CRC-32, Ed25519)
#   firmware/anon/            → np_anon_tests
#   firmware/edf/             → np_edf_tests
#   firmware/factory_reset/   → np_factory_reset_tests
#   firmware/uhdr_key/        → np_uhdr_key_tests  (NP-FW-EMMC-002 §C)
#   firmware/shdr/            → np_accel_shdr_tests (NP-FW-EMMC-002 §G/§H)
#   firmware/ota/             → np_ota_tests  (OTA state CRC + rollback logic)
#   firmware/safety_mcu/      → np_safety_spi_proto_tests (OI-SW01-M07-01)
#                             → np_charge_monitor_tests   (OI-CHARGE-01)
#                             → np_thermal_interlock_tests (SW01-M04 NTC cutoff, NP-HW-HUB-001 §7.2)
#                             → np_impedance_report_tests (OI-CVNS-HUB-11)
#                             → np_fault_latch_privacy_tests (fault-latch privacy gate)
#                             → np_cardiac_interlock_tests (SW01-M05 Class C cardiac interlock)
#   firmware/hub_control/     → np_cvns_reenable_tests    (OI-CVNS-HUB-01)
#                             → np_log_backend_tests      (OI-LOG-01..04)
#                             → np_transport_tests        (OI-HUB-MAIN-01)
#                             → np_mod_cvns_tests         (hub ↔ CVNS library)
#                             → np_cvns_fault_summary_tests (NP-SW-FAULTMSG-001 §9.6)
#                             → np_gatt_server_tests      (OI-WA-03 GATT server + warranty token)
#                             → np_consumables_tests      (OI-ACC-08 CONSUMABLE_STATUS producer)
#                             → np_module_map_tests       (hex zone-module addressing/NVRAM map)
#                             → np_protocol_tests         (hub protocol v2 socket-mask targets)
#                             → np_mod_stim_tests         (BES/tACS + tDCS slot dispatch)
#                             → np_module_registry_tests  (OI-FWHUB-16 accessory rescan)
#                             → np_socket_dispatch_tests  (OI-FWHUB-01 socket dispatch)
#                             → np_chan_decl_tests        (OI-MMSOCK-02 electrode-geometry declarations)
#                             → np_stim_xcheck_tests      (OI-FMEA-09 commanded-vs-delivered cross-check)
#   firmware/zone_announce/   → np_zone_announce_tests    (module-status frame encoder)
#                             → np_zone_notify_tests      (app-notify dispatch)
#   firmware/pbm/      → np_pbm_session_desc_tests (NP-SES-1064-001 v5 wire format)
#   firmware/cervical_vns/    → np_cvns_fai_tests         (FAI CV01..CV06, cardiac interlock)
#   firmware/sloreta_hdtdcs/  → np_hd_fai_tests           (FAI montage + sLORETA algorithm)
#   firmware/vendor/freertos/ → np_freertos_smoke_tests   (kernel + config, POSIX port)
#   firmware/vendor/littlefs/ → np_lfs_config_tests       (SOUP hashes + config +
#                              EMMC-FS-01 instance; target lives in hub_control
#                              because the configuration does)
#                             → np_lfs_powerloss_tests   (OI-LFS-02: power-loss
#                              injection against claims L-1…L-4 over the
#                              EMMC-FS-01 instance, swept across every
#                              medium-touching op under three tear models,
#                              falsified in both directions)
#                             → np_cfg_store_tests       (Rev 4 §13: the Config
#                              store's caller rules; #1205 reproduced and stopped)
#                             → np_lfs_log_instance_tests (Rev 4 §13.1: UHDR/SHDR
#                              instances, real geometry; ECR-EMMC-002 evidence)
#   firmware/bootloader/      → np_bootloader_mem_tests   (freestanding memcpy/memset —
#                              NP-SW-CI-001 §4.2)
#                             → np_bootloader_app_image_tests (OCRAM staging limit +
#                              linker-script agreement, NP-SW-CI-001 §4.3)
#                              np_mem.c and np_app_image.c ONLY; the rest of the
#                              bootloader is device firmware with no host-executable
#                              surface and stays cross-compile-only.
#
# Every module listed above must also appear in the `paths:` filters of the
# workflow that owns its host tests, or edits to it run no CI.  Ownership is
# split by IEC 62304 class (NP-SW-CI-001 §5.0.1 — the workflows are
# self-contained because `needs:` only works within one workflow):
#   firmware/safety_mcu/  → .github/workflows/safety-mcu-ci.yml        (10 targets, Class C)
#   everything else above → .github/workflows/firmware-cross-build.yml (32 targets, Class B)
# 10 + 32 = 42 (re-derived with `ctest -N` at OI-UPG-01; it read "27" and
# "8 + 29 = 37" against a real 8 + 32 before that).  .github/workflows/build-all.yml re-checks that partition weekly
# and fails if a target ever belongs to neither list.
# (This line has been wrong more often than right, which is the argument against
#  keeping a count in a comment at all.  It read "6 + 21 = 27" until 2026-08-12
#  and was stale by two even then; the 2026-08-12 correction to "7 + 22 = 29"
#  was itself already stale, because np_pbm_cal_bridge_tests had taken Class B
#  to 23.  Phase 8's np_app_link_agreement_tests took it to 24, phase 9's
#  np_bootloader_flexram_tests to 25, np_app_clock_agreement_tests
#  (NP-SW-CI-001 §4.11) to 26, and np_app_cache_agreement_tests
#  (NP-SW-CI-001 §4.14) to 27.  Phase 8 also moved the count in two workflows and
#  left build-all.yml's NP_TOTAL_TEST_COUNT at 30 against a real 31 — phase 9
#  found and corrected that, and recorded why it stayed invisible (OI-SWCI-10: a
#  red build-all is surfaced nowhere).  The
#  AUTHORITATIVE values are the NP_*_TEST_COUNT env vars in build-all.yml and
#  firmware-cross-build.yml, which are asserted against a live ctest
#  enumeration.  This comment is not, and a reader who needs the number should
#  go there.)
# (firmware-host-tests.yml was absorbed into the two above and retired —
#  NP-SW-CI-001 §5.0.1, OI-SWCI-09.)

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
# Both SDKs are vendored in-tree under the NP-SW-CI-001 §9 decision:
#   vendor/freertos/   FreeRTOS-Kernel V11.3.0   (SOUP: NP-SW-001 §9.4)
#   vendor/mcux_sdk/   MCUX SDK 2.16.0, MIMXRT1062 device layer
#                                                (SOUP: NP-SW-001 §9.4, OI-SWCI-20)
#   vendor/cmsis_core/ CMSIS-Core(M) 5.6.0 — core_cm7.h for SW-02
#
# Until 2026-09-01 the MCUX entry was an empty stub with a "← append MCUX SDK
# path" marker, and the reason it survived so long is worth keeping: the
# cross-build had ZERO missing-header errors without it, because SW-02 was
# stubbed rather than calling into a platform layer at all. A missing include
# path is invisible while nothing includes anything from it.
#
# The per-module CMakeLists files consume NP_PLATFORM_INCLUDE_DIRS as a list.
# Normal (directory-scope) set, not CACHE: a CACHE set() is ignored when the
# cache already holds a value, which would silently pin a stale include list on
# reconfigure. This runs before the add_subdirectory() calls, so subdirectories
# inherit it.
if(CMAKE_CROSSCOMPILING)
    set(NP_PLATFORM_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/freertos/kernel/include
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/freertos/portable/GCC/ARM_CM7/r0p1
        ${CMAKE_CURRENT_SOURCE_DIR}/hub_control/include   # FreeRTOSConfig.h
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/mcux_sdk/devices/MIMXRT1062
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/cmsis_core/Include
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/include      # np_sw02_platform_hal.h
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
    add_subdirectory(shdr)         # NP-FW-EMMC-002 §G/§H: SHDR accelerometer
    add_subdirectory(ota)
    add_subdirectory(safety_mcu)  # OI-SW01-M07-01: SPI frame tests
                                  # OI-CHARGE-01:   charge monitor tests
    add_subdirectory(vendor/littlefs) # NP-SW-001 §9.4: littlefs v2.11.3 SOUP.
                                  # Before hub_control: np_lfs_config_tests
                                  # links np_littlefs.
    add_subdirectory(hub_control) # OI-CVNS-HUB-01: CVNS re-enable manager tests
    add_subdirectory(zone_announce) # module-status frame encoder (app notify path)
    add_subdirectory(pbm)  # NP-SES-1064-001 v5: session descriptor wire format
    add_subdirectory(cervical_vns)   # NP-FW-CVNS-001 §9: main-processor FAI item
                                  # suite (FAI-CV01..CV03 software limbs). NOT the
                                  # Class C cardiac interlock -- that is
                                  # safety_mcu/tests/np_cardiac_interlock_tests.c.
    add_subdirectory(sloreta_hdtdcs) # NP-FW-HD-001: montage + sLORETA FAI suite
    add_subdirectory(vendor/freertos) # NP-SW-001 §9.4: FreeRTOS kernel smoke test
    add_subdirectory(application) # NP-SW-CI-001 §4.8: the two linker scripts
                                  # must agree about the staging reservation.
    add_subdirectory(bootloader)  # NP-SW-CI-001 §4.2: freestanding memcpy/memset.
                                  # ONLY the np_mem.c primitives have a host
                                  # test — the rest of the bootloader is device
                                  # firmware with no host-executable surface and
                                  # stays cross-compile-only.

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

    add_test(NAME np_accel_shdr_tests
        COMMAND np_accel_shdr_tests
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

    add_test(NAME np_thermal_interlock_tests
        COMMAND np_thermal_interlock_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_impedance_report_tests
        COMMAND np_impedance_report_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_fault_latch_privacy_tests
        COMMAND np_fault_latch_privacy_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_cardiac_interlock_tests
        COMMAND np_cardiac_interlock_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    # Seventh Class C target — SW-01 platform layer (NP-SW-CI-001 phase 7,
    # OI-SWCI-17).  Moves the Class C count 6 → 7 and the repo total 27 → 28.
    add_test(NAME np_hal_platform_tests
        COMMAND np_hal_platform_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    # Eighth Class C target — SW01-M09 non-volatile safety state
    # (NP-SW-FAULTMSG-001 P1, OI-FAULTMSG-01).  Class C count 7 → 8, total 36 → 37.
    add_test(NAME np_nv_state_tests
        COMMAND np_nv_state_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    # Ninth and tenth Class C targets — SW01-M10 tier identity
    # (NP-REG-UPG-001 §7.5, REQ-UPG-01/-02, OI-UPG-01).  Class C count 8 → 10,
    # total 40 → 42.  One source, built with the test authority key and with the
    # shipped all-zero placeholder.
    add_test(NAME np_tier_identity_tests
        COMMAND np_tier_identity_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    add_test(NAME np_tier_identity_nokey_tests
        COMMAND np_tier_identity_nokey_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_cvns_reenable_tests
        COMMAND np_cvns_reenable_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_module_map_tests
        COMMAND np_module_map_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    # OI-HUB-C06: np_module_map's UID-keyed calibration record reaching a
    # pbm session through the bridge — including the two swap cases
    # (module moves socket / socket changes module) that a socket-keyed store
    # would get wrong.
    add_test(NAME np_pbm_cal_bridge_tests
        COMMAND np_pbm_cal_bridge_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_protocol_tests
        COMMAND np_protocol_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    # OI-FWHUB-16: accessory slots re-probed while idle, re-initialised only on
    # a presence/type change, retired zone slots 0-4 refused.
    add_test(NAME np_module_registry_tests
        COMMAND np_module_registry_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_mod_stim_tests
        COMMAND np_mod_stim_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    # OI-FWHUB-01: socket-indexed dispatch — all-or-nothing placement/power/
    # driver admission, cranial-gate ownership, and the closed production
    # power governor (OI-FWHUB-09).
    add_test(NAME np_socket_dispatch_tests
        COMMAND np_socket_dispatch_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    # OI-MMSOCK-02: the electrode areas, waveform classes and geometry gates
    # the runner declares to the safety MCU (NP-FW-HUB-001 §5.4).
    add_test(NAME np_chan_decl_tests
        COMMAND np_chan_decl_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    # OI-FMEA-09 / FMEA-M03-02: delivered stimulation current above commanded
    # raises one SHDR divergence flag per channel per session.
    add_test(NAME np_stim_xcheck_tests
        COMMAND np_stim_xcheck_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_log_backend_tests
        COMMAND np_log_backend_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_transport_tests
        COMMAND np_transport_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    # OI-LFS-01.  The vendored littlefs bytes against the SHA-256 in its own
    # SOUP record, the build-time configuration against what that record says a
    # reviewer may rely on, and the EMMC-FS-01 instance against its validator —
    # with every single-field perturbation required to be rejected.
    # Moves the Class B count 27 → 28 and the repo total 34 → 35.
    add_test(NAME np_lfs_config_tests
        COMMAND np_lfs_config_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    # OI-LFS-02.  The other half: NeurOne's own power-loss injection against
    # claims L-1…L-4, swept across every medium-touching op of each commit
    # sequence under three tear models, and falsified in both directions —
    # unsafe orderings must be CAUGHT, and every armed cut must be seen to fire.
    # Moves the Class B count 28 → 29 and the repo total 35 → 36.
    add_test(NAME np_lfs_powerloss_tests
        COMMAND np_lfs_powerloss_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    # NP-SOUP-LFS-001 Rev 4 §13.  The caller rules §11 found the applicable
    # littlefs anomalies depend on (OI-LFS-06/-08/-09), and the UHDR/SHDR
    # instances on their real geometry (OI-LFS-05).  Moves the Class B count
    # 32 → 34 and the repo total 42 → 44.
    add_test(NAME np_cfg_store_tests
        COMMAND np_cfg_store_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    add_test(NAME np_lfs_log_instance_tests
        COMMAND np_lfs_log_instance_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_mod_cvns_tests
        COMMAND np_mod_cvns_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    # NP-SW-FAULTMSG-001 §9.6: the hub's cervical offline-fault summary — the
    # CVNS_FAULT_STATUS frame (byte-compared with the apps' parser fixture),
    # per-user scope, the persisted blob and the two ATT write handlers.
    # Moves the Class B count 29 → 30 and the repo total 37 → 38.
    add_test(NAME np_cvns_fault_summary_tests
        COMMAND np_cvns_fault_summary_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    # OI-WA-03 / GitHub #381: the BLE GATT server — the published table (UUIDs
    # compared with the apps' strings), ATT access rules, long reads, the
    # warranty token's provision-once / never-regenerate rule, and the three
    # cervical fault-summary rows end to end.
    add_test(NAME np_gatt_server_tests
        COMMAND np_gatt_server_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    # OI-ACC-08 (#381): the CONSUMABLE_STATUS producer.
    add_test(NAME np_consumables_tests
        COMMAND np_consumables_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_zone_notify_tests
        COMMAND np_zone_notify_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_zone_announce_tests
        COMMAND np_zone_announce_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_pbm_session_desc_tests
        COMMAND np_pbm_session_desc_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_cvns_fai_tests
        COMMAND np_cvns_fai_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_hd_fai_tests
        COMMAND np_hd_fai_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_bootloader_mem_tests
        COMMAND np_bootloader_mem_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME np_bootloader_app_image_tests
        COMMAND np_bootloader_app_image_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    # NP-SW-CI-001 phase 9, OI-SWCI-39.  The FlexRAM bank-config and TCM window
    # encodings — the half of np_flexram.c that can be wrong without silicon.
    # Moves the Class B count 24 → 25 and the repo total 31 → 32.
    add_test(NAME np_bootloader_flexram_tests
        COMMAND np_bootloader_flexram_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    # NP-SW-CI-001 phase 8, OI-SWCI-21.  The application linker script restates
    # two numbers the bootloader linker script owns; a linker script cannot
    # include another, so this test is what makes the duplication safe.  Moves
    # the Class B count 23 → 24 and the repo total 30 → 31.
    add_test(NAME np_app_link_agreement_tests
        COMMAND np_app_link_agreement_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    # NP-SW-CI-001 §4.11, OI-SWCI-41.  FreeRTOSConfig.h and the vendored SDK
    # header each state a core clock, they disagree, and no code establishes
    # either — so this asserts what the disagreement does to every FreeRTOS
    # interval, including the safety heartbeat, and that the boot path still
    # checks the clock instead of assuming it.
    add_test(NAME np_app_clock_agreement_tests
        COMMAND np_app_clock_agreement_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    # NP-SW-CI-001 §4.14, OI-SWCI-45.  Three files state what the L1 caches do —
    # np_app_cache.h, the vendored startup that enables one of them, and
    # firmware/vendor/cmsis_core/VERSION — and nothing compared them, which is
    # how four NeurOne documents came to describe a D-cache that SystemInit()
    # has never enabled.
    # Moves the Class B count 26 → 27 and the repo total 33 → 34.
    add_test(NAME np_app_cache_agreement_tests
        COMMAND np_app_cache_agreement_tests
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
add_subdirectory(vendor/littlefs)   # np_littlefs storage library (SW-02 SOUP).
                                    # Built, and deliberately reaching no image:
                                    # the OI-LOG-05..07 seams that would call it
                                    # are unimplemented.  See NP-SOUP-LFS-001.


if(NP_BUILD_BOOTLOADER)
    add_subdirectory(bootloader)
endif()

add_subdirectory(ota)
add_subdirectory(zone_announce)
add_subdirectory(hrv_biofeedback)
add_subdirectory(pbm)
add_subdirectory(cervical_vns)
add_subdirectory(sloreta_hdtdcs)
add_subdirectory(anon)
add_subdirectory(edf)
add_subdirectory(factory_reset)
add_subdirectory(uhdr_key)
add_subdirectory(shdr)

add_subdirectory(hub_control)

# ── The SW-02 platform layer and the image that links it ─────────────────────
# NP-SW-CI-001 §4.8 (phase 8, closes OI-SWCI-21).  Order matters: np_platform
# must be configured before np_application, which links it.
#
# hub_control is a STATIC library and is NOT the top-level target — this comment
# used to say it was, and the file it points at said the opposite in its own
# header ("the MCUX SDK HAL is still provided by the parent application
# target").  np_application is that parent, and it did not exist until now.
add_subdirectory(platform)
add_subdirectory(application)
