# ARM GNU Toolchain file for NeurOne embedded firmware
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

# ── Toolchain presence + version check ─────────────────────────────────────────
# Fail early with an actionable message if arm-none-eabi-gcc is missing or too
# old, instead of letting CMake fail deep inside its own compiler detection.
# Minimum tracks the hub_control prerequisite (arm-none-eabi-gcc >= 12.3).
set(NP_ARM_GCC_MIN_VERSION "12.3")

find_program(NP_ARM_GCC_EXE
    NAMES arm-none-eabi-gcc
    HINTS "$ENV{ARM_TOOLCHAIN_PATH}"
    DOC   "ARM GNU bare-metal C compiler")

if(NOT NP_ARM_GCC_EXE)
    set(_np_hint "")
    if(DEFINED ENV{ARM_TOOLCHAIN_PATH})
        set(_np_hint " (also not under ARM_TOOLCHAIN_PATH=$ENV{ARM_TOOLCHAIN_PATH})")
    endif()
    message(FATAL_ERROR
        "arm-none-eabi-gcc not found on PATH${_np_hint}.\n"
        "Install the ARM GNU bare-metal toolchain, then re-run cmake:\n"
        "  macOS:  brew install --cask gcc-arm-embedded   (or: brew install arm-none-eabi-gcc)\n"
        "  Linux:  apt install gcc-arm-none-eabi           (or download from arm.com)\n"
        "Installed in a non-standard location? Point CMake at its bin/ directory:\n"
        "  export ARM_TOOLCHAIN_PATH=/path/to/gcc-arm/bin")
endif()

# Query the FULL version — plain -dumpversion returns only the major number on
# GCC >= 7 (e.g. "12"), which would falsely fail the "12.3" gate.
execute_process(
    COMMAND "${NP_ARM_GCC_EXE}" -dumpfullversion
    OUTPUT_VARIABLE _np_arm_gcc_version
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _np_arm_gcc_rc)

if(NOT _np_arm_gcc_rc EQUAL 0 OR _np_arm_gcc_version STREQUAL "")
    message(FATAL_ERROR
        "Could not determine arm-none-eabi-gcc version "
        "(${NP_ARM_GCC_EXE} -dumpfullversion exited '${_np_arm_gcc_rc}'). "
        "The toolchain may be broken or incompatible.")
endif()

if(_np_arm_gcc_version VERSION_LESS NP_ARM_GCC_MIN_VERSION)
    if(NP_ALLOW_OLD_ARM_TOOLCHAIN)
        message(WARNING
            "arm-none-eabi-gcc ${_np_arm_gcc_version} is below the required "
            ">= ${NP_ARM_GCC_MIN_VERSION}; continuing because "
            "NP_ALLOW_OLD_ARM_TOOLCHAIN is set.")
    else()
        message(FATAL_ERROR
            "arm-none-eabi-gcc ${_np_arm_gcc_version} is too old — NeurOne firmware "
            "requires >= ${NP_ARM_GCC_MIN_VERSION} (found ${NP_ARM_GCC_EXE}).\n"
            "Upgrade the toolchain, or override with "
            "-DNP_ALLOW_OLD_ARM_TOOLCHAIN=ON if you accept the risk.")
    endif()
endif()

# Prevent CMake from testing the compiler for a host executable
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Search paths: never look in host sysroot
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
