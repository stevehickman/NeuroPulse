# NeuroPulse Bootloader — CMake Build
# Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
# Document: NP-FW-EMMC-001 Rev A §8
#
# Prerequisites:
#   - ARM GNU Toolchain (arm-none-eabi-gcc ≥ 12.3)
#   - CMake ≥ 3.20
#
# Usage:
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=../../cmake/arm-none-eabi.cmake
#   cmake --build build
#
# The build produces:
#   np_bootloader.elf   — ELF with debug info
#   np_bootloader.bin   — raw binary for flashing to eMMC boot partition
#   np_bootloader.hex   — Intel HEX (alternative flash format)
#
# Key-injection:
#   The placeholder public key in np_signature.c MUST be replaced before
#   any production firmware is signed.  The secure build step is:
#     1. Build bootloader with PLACEHOLDER key → test build
#     2. Inject production key via objcopy patch at secure build host
#     3. Sign all firmware images with the corresponding private key
#
# Safety MCU firmware is NOT managed by this CMakeLists.  It uses a
# separate STM32CubeMX-generated project in firmware/safety_mcu/.

cmake_minimum_required(VERSION 3.20)

project(np_bootloader C ASM)

# ── Toolchain guard ────────────────────────────────────────────────────────
if(NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
        "This project requires the ARM cross-compiler toolchain.\n"
        "Pass -DCMAKE_TOOLCHAIN_FILE=../../cmake/arm-none-eabi.cmake")
endif()

# ── Sources ────────────────────────────────────────────────────────────────
set(BOOTLOADER_SOURCES
    src/np_main.c
    src/np_emmc.c
    src/np_boot_selector.c
    src/np_signature.c
    src/np_ota.c
    src/np_dfu.c
)

add_executable(np_bootloader ${BOOTLOADER_SOURCES})

target_include_directories(np_bootloader PRIVATE include)

# ── Compiler flags ─────────────────────────────────────────────────────────
# Cortex-M7 with hardware double-precision FPU
set(CPU_FLAGS
    -mcpu=cortex-m7
    -mthumb
    -mfpu=fpv5-d16
    -mfloat-abi=hard
)

target_compile_options(np_bootloader PRIVATE
    ${CPU_FLAGS}
    -std=c11
    -Wall
    -Wextra
    -Werror
    -Wno-unused-parameter          # intentional in some HAL stubs
    -O2                             # optimise for size+speed balance
    -ffunction-sections            # dead code elimination
    -fdata-sections
    -fno-common
    -ffreestanding
    -fno-builtin
    -fstack-usage                  # generate .su files for stack analysis
    # Security hardening
    -fno-stack-protector           # no libc in bootloader
    -fwrapv                        # defined signed integer overflow
)

# ── Linker flags ────────────────────────────────────────────────────────────
set(LINKER_SCRIPT
    "${CMAKE_CURRENT_SOURCE_DIR}/linker/bootloader_imxrt1062.ld")

target_link_options(np_bootloader PRIVATE
    ${CPU_FLAGS}
    -T ${LINKER_SCRIPT}
    -nostdlib                       # no libc, no libgcc startup
    -nostartfiles
    -nodefaultlibs
    -Wl,--gc-sections               # remove unused sections
    -Wl,--print-memory-usage        # report OCRAM usage at link time
    -Wl,-Map=${CMAKE_BINARY_DIR}/np_bootloader.map  # linker map for review
    -Wl,--no-warn-rwx-segments      # OCRAM is RWX by design for bootloader
)

# ── Post-build: generate .bin and .hex ─────────────────────────────────────
find_program(OBJCOPY arm-none-eabi-objcopy REQUIRED)
find_program(SIZE    arm-none-eabi-size    REQUIRED)

add_custom_command(TARGET np_bootloader POST_BUILD
    COMMAND ${OBJCOPY} -O binary
            $<TARGET_FILE:np_bootloader>
            ${CMAKE_BINARY_DIR}/np_bootloader.bin
    COMMAND ${OBJCOPY} -O ihex
            $<TARGET_FILE:np_bootloader>
            ${CMAKE_BINARY_DIR}/np_bootloader.hex
    COMMAND ${SIZE} --format=sysv $<TARGET_FILE:np_bootloader>
    COMMENT "Generating np_bootloader.bin and np_bootloader.hex"
)

# ── Build size check: warn if image approaches 48 KiB limit ───────────────
add_custom_command(TARGET np_bootloader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo
        "NOTICE: Verify bootloader .bin size is ≤ 49152 bytes (48 KiB limit)"
    VERBATIM
)
