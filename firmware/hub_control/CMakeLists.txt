# NeurOne Hub Control Program — CMake Build
# Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz), FreeRTOS
# Document: NP-FW-HUB-001 Rev 1
#
# Produces a static library (np_hub_control) linked into the main application
# image (alongside FreeRTOS, np_bootloader, np_pbm, np_hrv_biofeedback,
# np_zone_announce, np_sloreta_hdtdcs, np_cervical_vns, and the platform HAL).
#
# Prerequisites:
#   - ARM GNU Toolchain (arm-none-eabi-gcc ≥ 12.3)
#   - CMake ≥ 3.20
#   - Parent project must provide:
#       NP_PLATFORM_INCLUDE_DIRS  — MCUX SDK + FreeRTOS headers
#       np_pbm             — PBM detection and drive libraries
#       np_hrv_biofeedback        — HRV biofeedback session library
#       np_zone_announce          — Zone module bone-conduction announcement
#       np_cervical_vns           — Cervical VNS safety interlock library
#
# Usage (from parent CMakeLists.txt):
#   add_subdirectory(firmware/hub_control)
#   target_link_libraries(np_application PRIVATE np_hub_control)

cmake_minimum_required(VERSION 3.20)
project(np_hub_control C)

# ── Host unit tests (no ARM toolchain required) ────────────────────────────────
# Build with -DNP_BUILD_TESTS=ON from the firmware super-project.
# np_cvns_reenable is pure C (no FreeRTOS) and host-testable (OI-CVNS-HUB-01).
option(NP_BUILD_TESTS "Build host-native unit tests (no ARM toolchain needed)" OFF)

set(NP_BOOTLOADER_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../bootloader/include)

# np_sw02_platform_hal.h — the single declaration point for the SW-02 platform
# seams this library calls (NP-SW-CI-001 §4.8.4, OI-SWCI-40).  Headers only: the
# np_platform library itself is cross-compile-only and is deliberately NOT linked
# into any host test, because its definitions are traps that halt the process.
# Declaring the seams is what makes drift a compile error; defining them here
# would kill every suite at its first HAL call.  ../shdr/include comes with it —
# np_sw02_platform_hal.h includes np_accel_shdr.h for np_accel_status_t.
set(NP_SW02_PLATFORM_HAL_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../shdr/include
)

if(NP_BUILD_TESTS)
    add_executable(np_cvns_reenable_tests
        tests/np_cvns_reenable_tests.c
        src/np_cvns_reenable.c
    )
    target_include_directories(np_cvns_reenable_tests PRIVATE include)
    target_compile_options(np_cvns_reenable_tests PRIVATE -Wall -Wextra -std=c11)

    # Hexagonal zone-module addressing + NVRAM map (module-redesign study).
    # Pure C, no FreeRTOS; NVRAM I/O behind an injected HAL.
    add_executable(np_module_map_tests
        tests/np_module_map_tests.c
        src/np_module_map.c
    )
    target_include_directories(np_module_map_tests PRIVATE include ../pbm/include)
    target_compile_options(np_module_map_tests PRIVATE -Wall -Wextra -std=c11)

    # PBM calibration bridge: np_module_map's UID-keyed record -> the pbm
    # library's injected provider/resolver (OI-HUB-C06). Needs both include
    # trees, which is exactly why the bridge lives here and not in either library.
    add_executable(np_pbm_cal_bridge_tests
        tests/np_pbm_cal_bridge_tests.c
        src/np_pbm_cal_bridge.c
        src/np_module_map.c
        ../pbm/src/np_pbm_session.c
        ../pbm/src/np_pbm_dose.c
        ../pbm/src/np_pbm_drive.c
        ../pbm/src/np_pbm_hal.c
    )
    target_include_directories(np_pbm_cal_bridge_tests PRIVATE include ../pbm/include)
    target_compile_options(np_pbm_cal_bridge_tests PRIVATE -Wall -Wextra -std=c11)
    target_link_libraries(np_pbm_cal_bridge_tests m)

    # NP Hub Protocol v2 command-body grammar: the socket-mask target block that
    # replaced the five-bit zone slot mask. Ed25519 + Config HALs are stubbed in
    # the test, so np_protocol.c links standalone on host.
    add_executable(np_protocol_tests
        tests/np_protocol_tests.c
        src/np_protocol.c
    )
    target_include_directories(np_protocol_tests PRIVATE
        include
        ${NP_SW02_PLATFORM_HAL_INCLUDE_DIRS}
        # OI-TACS-01: np_hd_config.h owns NP_HD_DRIVER_CHANNELS, the driver-side
        # view of the count np_hub_types.h encodes as a wire mask.  This target
        # is the only host test that sees both headers, so it is where the two
        # are asserted equal.
        ${CMAKE_CURRENT_SOURCE_DIR}/../sloreta_hdtdcs/include
    )
    target_compile_options(np_protocol_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g -DNPTEST_HOST)

    # BES/tACS + tDCS slot dispatch. The driver shares silicon across two slots,
    # so the slot is the only thing distinguishing an AC waveform from a DC ramp.
    # OI-FWHUB-01: socket-indexed dispatch. Links the real np_module_map and
    # np_protocol (socket expansion); the PBM socket driver and the safety-SPI
    # requests are test doubles, and so is np_pbm_power_admit() — the test needs
    # an admitting governor to reach the drive path at all. The PRODUCTION
    # governor is linked too, renamed, so the test can also assert that what
    # ships refuses every load (the closed-by-default property is the one that
    # matters, and a `return false` nobody runs is one nobody would notice
    # changing). The rename is test-configuration only: this block returns
    # before the cross build is configured.
    add_executable(np_socket_dispatch_tests
        tests/np_socket_dispatch_tests.c
        src/np_socket_dispatch.c
        src/np_pbm_power_gov.c
        src/np_module_map.c
        src/np_protocol.c
    )
    set_source_files_properties(src/np_pbm_power_gov.c PROPERTIES
        COMPILE_DEFINITIONS "np_pbm_power_admit=np_pbm_power_admit_production")
    target_include_directories(np_socket_dispatch_tests PRIVATE
        include
        ${NP_SW02_PLATFORM_HAL_INCLUDE_DIRS}
    )
    target_compile_options(np_socket_dispatch_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g -DNPTEST_HOST)

    # OI-MMSOCK-02 / NP-FW-HUB-001 §5.4: which electrode areas, waveform
    # classes and geometry gates the runner declares to the safety MCU.
    # Extracted from np_runner_run(), which no host test can reach.
    add_executable(np_chan_decl_tests
        tests/np_chan_decl_tests.c
        src/np_chan_decl.c
    )
    target_include_directories(np_chan_decl_tests PRIVATE
        include
        ${NP_SW02_PLATFORM_HAL_INCLUDE_DIRS}
    )
    target_compile_options(np_chan_decl_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g -DNPTEST_HOST)

    # OI-FWHUB-16: the module registry's single-slot rescan — accessory hot-plug,
    # change-only re-init (no SHDR auth flood), retired zone slots refused.
    # Links the production registry against per-slot driver doubles.
    add_executable(np_module_registry_tests
        tests/np_module_registry_tests.c
        src/np_module_registry.c
    )
    target_include_directories(np_module_registry_tests PRIVATE include)
    target_compile_options(np_module_registry_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g -DNPTEST_HOST)

    add_executable(np_mod_stim_tests
        tests/np_mod_stim_tests.c
        modules/np_mod_stim.c
        src/np_stim_xcheck.c          # OI-FMEA-09: the driver feeds it
    )
    target_include_directories(np_mod_stim_tests PRIVATE
        include
        ${NP_SW02_PLATFORM_HAL_INCLUDE_DIRS}
    )
    target_compile_options(np_mod_stim_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g -DNPTEST_HOST)
    target_link_libraries(np_mod_stim_tests PRIVATE m)

    # OI-FMEA-09 / FMEA-M03-02: the commanded-versus-delivered stimulation
    # cross-check. Pure C; the runner that calls it compiles in no host test.
    add_executable(np_stim_xcheck_tests
        tests/np_stim_xcheck_tests.c
        src/np_stim_xcheck.c
    )
    target_include_directories(np_stim_xcheck_tests PRIVATE include)
    target_compile_options(np_stim_xcheck_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g -DNPTEST_HOST)
    target_link_libraries(np_stim_xcheck_tests PRIVATE m)

    # OI-LOG-01..04: session-log eMMC backing (block-coalescing append writer).
    add_executable(np_log_backend_tests
        tests/np_log_backend_tests.c
        src/np_log_backend.c
        # OI-LFS-11: the session logger opens and closes the per-session UHDR
        # files, so its session boundaries are tested here, over the same
        # host model of the lower HAL.
        src/np_session_log.c
        src/np_adaptation_log.c
    )
    target_include_directories(np_log_backend_tests PRIVATE
        include
        ${NP_BOOTLOADER_INCLUDE_DIR}   # np_config.h: partition sizes
    )
    target_compile_options(np_log_backend_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g -DNPTEST_HOST)

    # OI-LFS-01: the vendored littlefs SOUP record, NeurOne's build-time
    # configuration of it, and the EMMC-FS-01 Config instance + mount-time
    # validator.  Links np_littlefs (so the pinned bytes are compile-checked
    # under NeurOne's configuration) and np_crypto (so the VERSION record's
    # per-file SHA-256 is recomputed rather than trusted).
    add_executable(np_lfs_config_tests
        tests/np_lfs_config_tests.c
        src/np_lfs_instance.c
    )
    target_include_directories(np_lfs_config_tests PRIVATE include)
    target_link_libraries(np_lfs_config_tests PRIVATE np_littlefs np_crypto)
    target_compile_definitions(np_lfs_config_tests PRIVATE
        NPTEST_HOST
        NP_LITTLEFS_DIR_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../vendor/littlefs")
    target_compile_options(np_lfs_config_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g)

    # OI-LFS-02: the NeurOne power-loss injection test against claims L-1…L-4.
    # Mounts the EMMC-FS-01 instance over np_lfs_powerbd.c — NeurOne's own
    # injecting block device, written rather than vendored because upstream's
    # bd/lfs_emubd.* is deliberately not in the tree (NP-SOUP-LFS-001 §7.5) —
    # and sweeps a power cut across every medium-touching op of each commit
    # sequence, under three tear models.  Links np_crypto so the record checker
    # is np_crc32 rather than the component's own lfs_crc.
    add_executable(np_lfs_powerloss_tests
        tests/np_lfs_powerloss_tests.c
        tests/np_lfs_powerbd.c
        src/np_lfs_instance.c
    )
    target_include_directories(np_lfs_powerloss_tests PRIVATE include tests)
    target_link_libraries(np_lfs_powerloss_tests PRIVATE np_littlefs np_crypto)
    target_compile_definitions(np_lfs_powerloss_tests PRIVATE NPTEST_HOST)
    target_compile_options(np_lfs_powerloss_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g)

    # NP-SOUP-LFS-001 Rev 4 §13 (OI-LFS-06/-08/-09): the Config store — the
    # caller rules §11 found the applicable anomalies depend on.  Reproduces
    # upstream #1205 through raw littlefs and shows it not reaching a store
    # caller; sweeps the store's own orderings (in-place O_TRUNC replace,
    # journal append, two-copy write) under a power cut at every op.
    add_executable(np_cfg_store_tests
        tests/np_cfg_store_tests.c
        tests/np_lfs_powerbd.c
        tests/np_lfs_sweep.c
        src/np_cfg_store.c
        src/np_session_count.c        # OI-LFS-12: the persisted session count
        src/np_lfs_instance.c
    )
    target_include_directories(np_cfg_store_tests PRIVATE include tests)
    target_link_libraries(np_cfg_store_tests PRIVATE np_littlefs np_crypto)
    target_compile_definitions(np_cfg_store_tests PRIVATE NPTEST_HOST)
    target_compile_options(np_cfg_store_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g)

    # NP-SOUP-LFS-001 Rev 4 §13.1 (OI-LFS-05): the UHDR and SHDR instances.
    # Mounts the REAL geometry (1,767,168 / 131,072 blocks) over a sparse
    # medium and sweeps L-1/L-2 on it; carries the evidence for ECR-EMMC-002
    # (prog_size below the 512-byte XTS unit loses committed data under a
    # read-modify-write tear — for the logs as EMMC-FS-01 Rev 2 printed them, and for
    # the Config geometry at 256, which moved the Config instance to 512 —
    # OI-LFS-10); and measures the specified
    # lookahead's allocator traversals in reads.
    add_executable(np_lfs_log_instance_tests
        tests/np_lfs_log_instance_tests.c
        tests/np_lfs_powerbd.c
        tests/np_lfs_sweep.c
        src/np_lfs_log_instance.c
        src/np_lfs_instance.c
        src/np_cfg_store.c
    )
    target_include_directories(np_lfs_log_instance_tests PRIVATE
        include tests ${NP_BOOTLOADER_INCLUDE_DIR})
    target_link_libraries(np_lfs_log_instance_tests PRIVATE np_littlefs np_crypto)
    target_compile_definitions(np_lfs_log_instance_tests PRIVATE NPTEST_HOST)
    target_compile_options(np_lfs_log_instance_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g)

    # OI-HUB-MAIN-01: protocol transport reassembly + single-slot mailbox.
    add_executable(np_transport_tests
        tests/np_transport_tests.c
        src/np_transport.c
    )
    target_include_directories(np_transport_tests PRIVATE include)
    target_compile_options(np_transport_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g -DNPTEST_HOST)

    # NP-SW-FAULTMSG-001 §9.6: cervical offline-fault summary (CVNS_FAULT_STATUS
    # frame, per-user scope, persisted blob, ATT write handlers).  Links
    # np_transport.c for its host-modeled critical section.
    add_executable(np_cvns_fault_summary_tests
        tests/np_cvns_fault_summary_tests.c
        src/np_cvns_fault_summary.c
        src/np_transport.c
    )
    target_include_directories(np_cvns_fault_summary_tests PRIVATE
        include
        ${CMAKE_CURRENT_SOURCE_DIR}/../cervical_vns/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
    )
    target_compile_options(np_cvns_fault_summary_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g -DNPTEST_HOST)
    target_link_libraries(np_cvns_fault_summary_tests m)

    # OI-WA-03 / GitHub #381: the BLE GATT server table, its read/write/notify
    # dispatch, and the warranty token it serves.  Links the real cervical
    # fault summary so the 0x0014/0x0015/0x0016 rows are tested end to end.
    # np_cfg_store.h is included for its API only (the replicated-record calls
    # are modelled in the test); it pulls in lfs.h, hence the littlefs headers.
    add_executable(np_gatt_server_tests
        tests/np_gatt_server_tests.c
        src/np_gatt_server.c
        src/np_warranty_token.c
        src/np_consumables.c
        src/np_cvns_fault_summary.c
        src/np_transport.c
    )
    target_include_directories(np_gatt_server_tests PRIVATE
        include
        ${CMAKE_CURRENT_SOURCE_DIR}/../cervical_vns/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
        $<TARGET_PROPERTY:np_littlefs,INTERFACE_INCLUDE_DIRECTORIES>
    )
    target_compile_options(np_gatt_server_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g -DNPTEST_HOST)
    target_link_libraries(np_gatt_server_tests m)

    # OI-ACC-08 (#381): the CONSUMABLE_STATUS producer — increment rule,
    # replacement reset, persistence (never zero over an unreadable record,
    # never an older snapshot over a newer one) and change-only notification.
    # Links the real GATT server so the wire bytes are read through 0x0007.
    add_executable(np_consumables_tests
        tests/np_consumables_tests.c
        src/np_consumables.c
        src/np_gatt_server.c
        src/np_warranty_token.c
        src/np_cvns_fault_summary.c
        src/np_transport.c
    )
    target_include_directories(np_consumables_tests PRIVATE
        include
        ${CMAKE_CURRENT_SOURCE_DIR}/../cervical_vns/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
        $<TARGET_PROPERTY:np_littlefs,INTERFACE_INCLUDE_DIRECTORIES>
    )
    target_compile_options(np_consumables_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g -DNPTEST_HOST)
    target_link_libraries(np_consumables_tests m)

    # OI-NVRAM-08: the Map 3 journal record and its version-tolerant scan.
    # The CI check that a journal written under version n loses no record
    # under n+1 (NP-FW-NVRAM-001 Rev 3 §7.2), run against the production
    # reader and falsified against four readers that break the rule.
    add_executable(np_map3_record_tests
        tests/np_map3_record_tests.c
        src/np_map3_record.c
    )
    target_include_directories(np_map3_record_tests PRIVATE include)
    target_link_libraries(np_map3_record_tests PRIVATE np_crypto)
    target_compile_options(np_map3_record_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g -DNPTEST_HOST)

    # Hub ↔ cervical VNS library integration + cardiac-interlock config.
    # Links the real np_cervical_vns library sources directly (host build).
    set(NP_CVNS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../cervical_vns)
    add_executable(np_mod_cvns_tests
        tests/np_mod_cvns_tests.c
        modules/np_mod_cvns.c
        ${NP_CVNS_DIR}/src/np_cvns_session.c
        ${NP_CVNS_DIR}/src/np_cvns_stim.c
        ${NP_CVNS_DIR}/src/np_cvns_interlock.c
    )
    target_include_directories(np_mod_cvns_tests PRIVATE
        include
        ${NP_CVNS_DIR}/include
        ${NP_SW02_PLATFORM_HAL_INCLUDE_DIRS}
    )
    target_compile_options(np_mod_cvns_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g -DNPTEST_HOST)
    target_link_libraries(np_mod_cvns_tests m)

    return()
endif()

# ── Toolchain guard ────────────────────────────────────────────────────────────
if(NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "This project requires the ARM cross-compiler toolchain.\n"
        "Pass -DCMAKE_TOOLCHAIN_FILE=../../cmake/arm-none-eabi.cmake")
endif()

# ── Sources ────────────────────────────────────────────────────────────────────
set(HUB_SOURCES
    src/np_hub_control_main.c
    src/np_module_registry.c
    src/np_protocol.c
    src/np_session_runner.c
    src/np_session_log.c
    src/np_log_backend.c          # OI-LOG-01..04: session-log eMMC backing
    src/np_transport.c            # OI-HUB-MAIN-01: BLE/USB-C protocol transport
    src/np_adaptation_log.c
    src/np_safety_spi.c
    src/np_cvns_reenable.c        # OI-CVNS-HUB-01: CVNS re-enable manager
    src/np_cvns_fault_summary.c   # NP-SW-FAULTMSG-001 §9.6: offline-fault summary + GATT handlers
    src/np_gatt_server.c          # OI-WA-03 / #381: BLE GATT service table + dispatch
    src/np_warranty_token.c       # OI-WA-03: NP-FW-EMMC-002 §A warranty token
    src/np_consumables.c          # OI-ACC-08: CONSUMABLE_STATUS session counts
    src/np_module_map.c           # Hex zone-module addressing + NVRAM map (redesign study)
    src/np_lfs_instance.c         # OI-LFS-01: EMMC-FS-01 littlefs instance + mount validator
    src/np_cfg_store.c            # OI-LFS-06/-08/-09: Config store — the littlefs caller rules
    src/np_map3_record.c          # OI-NVRAM-08: Map 3 journal record + version-tolerant scan
    src/np_lfs_log_instance.c     # OI-LFS-05: UHDR/SHDR littlefs instances + mount validator
    src/np_session_count.c        # OI-LFS-12: device session count persisted in Config
    src/np_pbm_cal_bridge.c       # OI-HUB-C06: UID-keyed calibration -> pbm seams
    src/np_socket_dispatch.c      # OI-FWHUB-01: socket-indexed dispatch registry
    src/np_chan_decl.c            # OI-MMSOCK-02: electrode-geometry declarations to the safety MCU
    src/np_stim_xcheck.c          # OI-FMEA-09: commanded-vs-delivered stimulation cross-check
    src/np_pbm_power_gov.c        # OI-FWHUB-09 / OI-HEXTILE-09: governor, CLOSED by default
    src/np_hub_freertos_hooks.c   # FreeRTOS app hooks (stack overflow, malloc, assert)

    modules/np_mod_pbm.c
    modules/np_mod_eeg.c
    modules/np_mod_stim.c
    modules/np_mod_vns.c
    modules/np_mod_audio.c
    modules/np_mod_visual.c
    modules/np_mod_intranasal.c
    modules/np_mod_cvns.c         # hub ↔ np_cervical_vns integration + cardiac interlock
    modules/np_mod_t2_stubs.c     # T2 + accessory detect/init/control stubs
)

add_library(np_hub_control STATIC ${HUB_SOURCES})

target_include_directories(np_hub_control
    PUBLIC  include
    PRIVATE ${NP_PLATFORM_INCLUDE_DIRS}
            ${NP_SW02_PLATFORM_HAL_INCLUDE_DIRS}  # np_sw02_platform_hal.h (OI-SWCI-40)
            ${NP_BOOTLOADER_INCLUDE_DIR}   # np_config.h: partition sizes (np_log_backend.c)
)

# ── Compiler flags ─────────────────────────────────────────────────────────────
set(CPU_FLAGS
    -mcpu=cortex-m7
    -mthumb
    -mfpu=fpv5-d16
    -mfloat-abi=hard
)

target_compile_options(np_hub_control PRIVATE
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
# np_hub_control depends on the other firmware libraries for existing module
# infrastructure, and on the vendored FreeRTOS kernel (np_freertos) for the RTOS
# and its headers.  The MCUX SDK HAL is still provided by the parent application
# target (clock init, pin mux, peripheral drivers).
target_link_libraries(np_hub_control
    PUBLIC
        np_freertos
        np_littlefs
        np_pbm
        np_hrv_biofeedback
        np_zone_announce
        np_cervical_vns
        m
)
