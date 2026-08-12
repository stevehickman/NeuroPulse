# NeurOne — Shared Cryptographic Primitives
# Document: NP-SW-001 Rev 1 §4.1
#
# Static library consumed by:
#   • firmware/bootloader/  (Cortex-M7 hard-FPU, SW-02 Class B)
#   • firmware/safety_mcu/  (Cortex-M0+ soft-float, SW-01 Class C)
#
# IMPORTANT: No CPU flags are set here.  The consuming project's CMakeLists
# sets CPU flags (e.g. -mcpu=cortex-m7 or -mcpu=cortex-m0plus) via its own
# target_compile_options / target_link_options.  This lets the same source
# compile correctly for both targets and for the host (x86/arm64) test runner.
#
# Resolves OI-SW01-M07-02: provides np_ed25519_verify for the safety MCU.

cmake_minimum_required(VERSION 3.20)
project(np_crypto C)

add_library(np_crypto STATIC
    src/np_crypto.c
    vendor/monocypher/monocypher.c
    vendor/monocypher/monocypher-ed25519.c
)

target_include_directories(np_crypto
    PUBLIC  include
    PRIVATE vendor/monocypher
)

# C standard: enforce C11 without GNU extensions (MISRA C:2012 requires this).
# C_EXTENSIONS OFF maps to -std=c11 rather than -std=gnu11.
set_target_properties(np_crypto PROPERTIES
    C_STANDARD          11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS        OFF
)

# First-party source: strict warnings.  Vendored SOUP files are excluded below.
target_compile_options(np_crypto PRIVATE
    -Wall
    -Wextra
    -ffunction-sections
    -fdata-sections
)

# SOUP: suppress all diagnostics on vendored Monocypher sources.
# Monocypher 4.0.2 is not warning-clean under -Wextra on all GCC versions;
# adding -Werror to np_crypto later must not break on third-party files.
# MISRA C:2012 scans apply to src/np_crypto.c only (see VERSION file).
set_source_files_properties(
    vendor/monocypher/monocypher.c
    vendor/monocypher/monocypher-ed25519.c
    PROPERTIES COMPILE_FLAGS "-w"
)

# ── Host unit tests (NP_BUILD_TESTS=ON) ──────────────────────────────────────
if(NP_BUILD_TESTS)
    add_executable(np_crypto_tests
        tests/np_crypto_tests.c
    )
    target_link_libraries(np_crypto_tests PRIVATE np_crypto)
    target_compile_options(np_crypto_tests PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()
