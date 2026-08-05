cmake_minimum_required(VERSION 3.20)
project(np_cervical_vns C)

# ── Toolchain ────────────────────────────────────────────────────────────────── #
# Cross-compile target: arm-none-eabi (NXP i.MX RT1062)                         #
# Host build for test: -DNP_BUILD_TESTS=ON (the repo-wide host-test switch)     #
#                                                                               #
# The toolchain is selected by the root project, which is the only scope where  #
# CMAKE_TOOLCHAIN_FILE has any effect — setting it here (after the top-level    #
# project() has already run) was a silent no-op and has been removed.           #

# Declared before first use below.  When this directory is added by the root
# firmware/CMakeLists.txt the value already comes from the cache; this option()
# only supplies the default for a standalone configure of this module.
option(NP_BUILD_TESTS "Build host-native FAI test binary" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ── Compiler flags ───────────────────────────────────────────────────────────── #
# Gated on NP_BUILD_TESTS, the same variable the test target below uses.  These
# previously gated on a separate NPTEST_HOST variable, so a plain
# -DNP_BUILD_TESTS=ON configured cleanly and then failed to build with
# "unsupported option '-mcpu=' for target 'arm64-apple-darwin'".
if(NOT NP_BUILD_TESTS)
    add_compile_options(
        -mcpu=cortex-m7
        -mfpu=fpv5-d16
        -mfloat-abi=hard
        -mthumb
        -ffunction-sections
        -fdata-sections
        -Wall
        -Wextra
        -Werror
        -Os
    )
    add_link_options(-Wl,--gc-sections)
else()
    add_compile_options(-Wall -Wextra -O2)
endif()

# ── Static library: np_cervical_vns ─────────────────────────────────────────── #
add_library(np_cervical_vns STATIC
    src/np_cvns_interlock.c
    src/np_cvns_stim.c
    src/np_cvns_session.c
)

target_include_directories(np_cervical_vns
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(np_cervical_vns m)

# ── FAI test binary ──────────────────────────────────────────────────────────── #
if(NP_BUILD_TESTS)
    add_executable(np_cvns_fai_tests
        tests/np_cvns_fai_tests.c
    )

    target_include_directories(np_cvns_fai_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(np_cvns_fai_tests np_cervical_vns m)
    target_compile_definitions(np_cvns_fai_tests PRIVATE NPTEST_HOST)
endif()
