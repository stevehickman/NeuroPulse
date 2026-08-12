# ARM GNU Toolchain file for NeurOne Safety MCU
# Target: STMicroelectronics STM32G071 (Cortex-M0+, 64 MHz)
# Document: NP-SW-001 Rev 1 — SW-01 Class C firmware
#
# The safety MCU uses the same ARM GNU cross-compiler as the main processor
# firmware but with Cortex-M0+ flags and no FPU.
#
# Prerequisites:
#   - ARM GNU Toolchain (arm-none-eabi-gcc ≥ 12.3)
#     https://developer.arm.com/Tools%20and%20Software/GNU%20Toolchain
#
# Usage:
#   cmake -B build/safety_mcu firmware/safety_mcu \
#         -DCMAKE_TOOLCHAIN_FILE=firmware/cmake/stm32g071.cmake \
#         -DCMAKE_BUILD_TYPE=Release

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Locate toolchain — prefer environment variable, fall back to PATH
if(DEFINED ENV{ARM_TOOLCHAIN_PATH})
    set(_TC_PREFIX "$ENV{ARM_TOOLCHAIN_PATH}/")
else()
    set(_TC_PREFIX "")
endif()

set(CMAKE_C_COMPILER   "${_TC_PREFIX}arm-none-eabi-gcc")
set(CMAKE_CXX_COMPILER "${_TC_PREFIX}arm-none-eabi-g++")
set(CMAKE_ASM_COMPILER "${_TC_PREFIX}arm-none-eabi-gcc")
set(CMAKE_LINKER       "${_TC_PREFIX}arm-none-eabi-ld")
set(CMAKE_AR           "${_TC_PREFIX}arm-none-eabi-ar")
set(CMAKE_RANLIB       "${_TC_PREFIX}arm-none-eabi-ranlib")
set(CMAKE_OBJCOPY      "${_TC_PREFIX}arm-none-eabi-objcopy")
set(CMAKE_OBJDUMP      "${_TC_PREFIX}arm-none-eabi-objdump")
set(CMAKE_SIZE         "${_TC_PREFIX}arm-none-eabi-size")
set(CMAKE_NM           "${_TC_PREFIX}arm-none-eabi-nm")

# Prevent CMake from testing the compiler for a host executable
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Search paths: never look in host sysroot
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ── STM32G071 CPU flags ────────────────────────────────────────────────────────
# Cortex-M0+ has no FPU and uses Thumb-2 instruction set.
# -mfloat-abi=soft uses software floating-point library (libgcc).
set(STM32G071_CPU_FLAGS
    "-mcpu=cortex-m0plus"
    "-mthumb"
    "-mfloat-abi=soft"
)

string(JOIN " " _CPU_FLAGS_STR ${STM32G071_CPU_FLAGS})

set(CMAKE_C_FLAGS_INIT   "${_CPU_FLAGS_STR}")
set(CMAKE_ASM_FLAGS_INIT "${_CPU_FLAGS_STR} -x assembler-with-cpp")
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "${_CPU_FLAGS_STR} -specs=nano.specs -specs=nosys.specs")
