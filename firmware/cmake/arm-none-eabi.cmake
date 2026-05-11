# ARM GNU Toolchain file for NeuroPulse embedded firmware
# Targets: NXP i.MX RT1062 (Cortex-M7) and STM32G071 (Cortex-M0+)

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
