# NeurOne Hub Control Program — CMake Build
# Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz), FreeRTOS
# Document: NP-FW-HUB-001 Rev A
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
    src/np_hub_freertos_hooks.c   # FreeRTOS app hooks (stack overflow, malloc, assert)

    modules/np_mod_pbm.c
    modules/np_mod_eeg.c
    modules/np_mod_stim.c
    modules/np_mod_vns.c
    modules/np_mod_audio.c
    modules/np_mod_visual.c
    modules/np_mod_intranasal.c   # also contains CVNS wrappers
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
