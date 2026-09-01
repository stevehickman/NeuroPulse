# ARM GNU Toolchain file for NeurOne embedded firmware
# Target: NXP i.MX RT1062 (Cortex-M7) — the SW-02 Class B build.
# The STM32G071 (Cortex-M0+) SW-01 Class C build uses cmake/stm32g071.cmake.

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

# ── i.MX RT1062 CPU flags ─────────────────────────────────────────────────────
# Cortex-M7 with the hardware double-precision FPU, Thumb-2, hard float ABI.
#
# Added 2026-09-01 (NP-SW-CI-001 §4.8, phase 8).  This block is the counterpart
# of the STM32G071_CPU_FLAGS block in cmake/stm32g071.cmake and it was missing,
# which was a real defect rather than an inconsistency of style.
#
# Every per-target CMakeLists in this project sets these same flags itself, so
# the omission was invisible — EXCEPT for the two libraries that deliberately
# set no CPU flags of their own because they are shared between the Cortex-M7
# and Cortex-M0+ builds: np_crypto (whose header says in as many words "the
# consuming project's CMakeLists sets CPU flags") and np_ota_state.  On the
# SW-01 side that works, because stm32g071.cmake supplies them through
# CMAKE_C_FLAGS_INIT.  On the SW-02 side there was nothing to supply them, so
# both libraries compiled for the compiler's bare default: measured 2026-09-01,
# `readelf -A` on np_crypto.c.obj reported Tag_CPU_arch **v4T**, ARM ISA, soft
# float — not Cortex-M7, not even Thumb-only.
#
# Nothing observed it for the same reason nothing observed Defect E or §4.7:
# no image linked them.  The bootloader carries its own np_signature.c and does
# not link np_crypto, and there was no SW-02 application target at all.  The
# first link that included both (np_application, phase 8) failed immediately
# with `uses VFP register arguments, libnp_crypto.a(...) does not`.
#
# Duplication with the per-target flags is harmless — they are identical
# strings — and is not tidied away here: doing so would touch every module's
# CMakeLists for no behavioural gain, and a target that states its own ABI is
# the safer thing to read.
set(IMXRT1062_CPU_FLAGS
    "-mcpu=cortex-m7"
    "-mthumb"
    "-mfpu=fpv5-d16"
    "-mfloat-abi=hard"
)

string(JOIN " " _NP_IMXRT_CPU_FLAGS_STR ${IMXRT1062_CPU_FLAGS})

set(CMAKE_C_FLAGS_INIT   "${_NP_IMXRT_CPU_FLAGS_STR}")
set(CMAKE_ASM_FLAGS_INIT "${_NP_IMXRT_CPU_FLAGS_STR} -x assembler-with-cpp")

# CMAKE_EXE_LINKER_FLAGS_INIT is deliberately NOT set here, unlike on the SW-01
# side.  Both executable targets in this project already pass the CPU flags on
# their own link lines, and the bootloader links -nostdlib -nostartfiles
# -nodefaultlibs while np_application links newlib; there is no single specs
# choice that suits both.  Adding one here would also re-create Defect F
# (NP-SW-CI-001 §4.5) — the duplicate -specs= on a link line — from the other
# direction.

# Prevent CMake from testing the compiler for a host executable
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Search paths: never look in host sysroot
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
