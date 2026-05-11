cmake_minimum_required(VERSION 3.20)
project(np_cervical_vns C)

# ── Toolchain ────────────────────────────────────────────────────────────────── #
# Cross-compile target: arm-none-eabi (NXP i.MX RT1062)                         #
# Host build for test: -DNPTEST_HOST=ON                                          #
if(NOT NPTEST_HOST)
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/../cmake/arm-none-eabi.cmake)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ── Compiler flags ───────────────────────────────────────────────────────────── #
if(NOT NPTEST_HOST)
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
option(NP_BUILD_TESTS "Build FAI test binary" OFF)

if(NP_BUILD_TESTS OR NPTEST_HOST)
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
