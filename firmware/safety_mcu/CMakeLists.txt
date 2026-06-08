# NeuroPulse Safety MCU Firmware — CMake Build
# Target: STMicroelectronics STM32G071 (Cortex-M0+, 64 MHz)
# Document: NP-SW-001 Rev A — SW-01 Class C firmware
#           NP-FW-EMMC-001 Rev A §4.2, NP-FMEA-001 Rev A
#
# IEC 62304 Class C — safety-critical; MISRA C:2012 mandatory.
# Safety MCU owns all stimulation GPIO enable lines (14 channels).
# SPI slave to i.MX RT1062 main processor; 200ms heartbeat, 1.5s watchdog.
#
# ── SW-01 Modules (NP-SW-001 Rev A §4.1) ─────────────────────────────────────
#   SW01-M01  np_gpio_mgr          — stimulation GPIO owner (14 channels)
#   SW01-M02  np_spi_watchdog      — 200ms heartbeat + 1.5s watchdog cutoff
#   SW01-M03  np_charge_monitor    — per-electrode charge accumulator (40µC/cm²)
#   SW01-M04  np_thermal_interlock — NTC zone thermal cutoff (62°C junction)
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

# ── Toolchain guard ────────────────────────────────────────────────────────────
if(NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "Safety MCU requires the ARM cross-compiler toolchain.\n"
        "Pass -DCMAKE_TOOLCHAIN_FILE=../cmake/stm32g071.cmake")
endif()

# ── STM32G071 device define ────────────────────────────────────────────────────
add_compile_definitions(
    STM32G071xx
    USE_HAL_DRIVER
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

# ── Firmware image ─────────────────────────────────────────────────────────────
add_executable(np_safety_mcu ${SAFETY_SOURCES})

target_include_directories(np_safety_mcu PRIVATE include)

target_compile_options(np_safety_mcu PRIVATE
    ${CPU_FLAGS}
    -std=c11
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

target_link_options(np_safety_mcu PRIVATE
    ${CPU_FLAGS}
    -T${CMAKE_CURRENT_SOURCE_DIR}/startup/stm32g071_flash.ld
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/np_safety_mcu.map
    -specs=nano.specs
    -specs=nosys.specs
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
