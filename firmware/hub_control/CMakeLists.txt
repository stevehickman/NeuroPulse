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
    src/np_adaptation_log.c
    src/np_safety_spi.c
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
# infrastructure.  The platform HAL (FreeRTOS, MCUX SDK) is provided by the
# parent application target and does not need to be listed here.
target_link_libraries(np_hub_control
    PUBLIC
        np_pbm_1064nm
        np_hrv_biofeedback
        np_zone_announce
        np_cervical_vns
        m
)
