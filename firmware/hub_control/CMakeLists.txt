# NeurOne Hub Control Program — CMake Build
# Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz), FreeRTOS
# Document: NP-FW-HUB-001 Rev 1
#
# Produces a static library (np_hub_control) linked into the main application
# image (alongside FreeRTOS, np_bootloader, np_pbm_1064nm, np_hrv_biofeedback,
# np_zone_announce, np_sloreta_hdtdcs, np_cervical_vns, and the platform HAL).
#
# Prerequisites:
#   - ARM GNU Toolchain (arm-none-eabi-gcc ≥ 12.3)
#   - CMake ≥ 3.20
#   - Parent project must provide:
#       NP_PLATFORM_INCLUDE_DIRS  — MCUX SDK + FreeRTOS headers
#       np_pbm_1064nm             — PBM detection and drive libraries
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
    target_include_directories(np_module_map_tests PRIVATE include ../pbm_1064nm/include)
    target_compile_options(np_module_map_tests PRIVATE -Wall -Wextra -std=c11)

    # PBM calibration bridge: np_module_map's UID-keyed record -> the pbm_1064nm
    # library's injected provider/resolver (OI-HUB-C06). Needs both include
    # trees, which is exactly why the bridge lives here and not in either library.
    add_executable(np_pbm_cal_bridge_tests
        tests/np_pbm_cal_bridge_tests.c
        src/np_pbm_cal_bridge.c
        src/np_module_map.c
        ../pbm_1064nm/src/np_pbm1064_session.c
        ../pbm_1064nm/src/np_pbm1064_dose.c
        ../pbm_1064nm/src/np_pbm1064_drive.c
        ../pbm_1064nm/src/np_pbm1064_hal.c
    )
    target_include_directories(np_pbm_cal_bridge_tests PRIVATE include ../pbm_1064nm/include)
    target_compile_options(np_pbm_cal_bridge_tests PRIVATE -Wall -Wextra -std=c11)
    target_link_libraries(np_pbm_cal_bridge_tests m)

    # NP Hub Protocol v2 command-body grammar: the socket-mask target block that
    # replaced the five-bit zone slot mask. Ed25519 + Config HALs are stubbed in
    # the test, so np_protocol.c links standalone on host.
    add_executable(np_protocol_tests
        tests/np_protocol_tests.c
        src/np_protocol.c
    )
    target_include_directories(np_protocol_tests PRIVATE include)
    target_compile_options(np_protocol_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g -DNPTEST_HOST)

    # BES/tACS + tDCS slot dispatch. The driver shares silicon across two slots,
    # so the slot is the only thing distinguishing an AC waveform from a DC ramp.
    add_executable(np_mod_stim_tests
        tests/np_mod_stim_tests.c
        modules/np_mod_stim.c
    )
    target_include_directories(np_mod_stim_tests PRIVATE include)
    target_compile_options(np_mod_stim_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g -DNPTEST_HOST)

    # OI-LOG-01..04: session-log eMMC backing (block-coalescing append writer).
    add_executable(np_log_backend_tests
        tests/np_log_backend_tests.c
        src/np_log_backend.c
    )
    target_include_directories(np_log_backend_tests PRIVATE
        include
        ${NP_BOOTLOADER_INCLUDE_DIR}   # np_config.h: partition sizes
    )
    target_compile_options(np_log_backend_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g -DNPTEST_HOST)

    # OI-HUB-MAIN-01: protocol transport reassembly + single-slot mailbox.
    add_executable(np_transport_tests
        tests/np_transport_tests.c
        src/np_transport.c
    )
    target_include_directories(np_transport_tests PRIVATE include)
    target_compile_options(np_transport_tests PRIVATE
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
    src/np_module_map.c           # Hex zone-module addressing + NVRAM map (redesign study)
    src/np_pbm_cal_bridge.c       # OI-HUB-C06: UID-keyed calibration -> pbm_1064nm seams
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
        np_pbm_1064nm
        np_hrv_biofeedback
        np_zone_announce
        np_cervical_vns
        m
)
