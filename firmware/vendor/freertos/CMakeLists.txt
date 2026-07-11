# Vendored FreeRTOS-Kernel V11.3.0 (FreeRTOS-LTS 202604.00-LTS)
# SOUP — see VERSION and README-NEURONE.md. Document: NP-SW-001 §9.4.
#
# Two build modes:
#   - Cross (ARM_CM7): produces the np_freertos static library for the SW-02
#     main-processor image (Cortex-M7 r0p1 port + heap_4).
#   - Host (-DNP_BUILD_TESTS=ON): builds np_freertos_smoke_tests, which compiles
#     the SAME kernel sources against NeurOne's FreeRTOSConfig.h using the POSIX
#     port and runs the scheduler natively (no ARM toolchain required).
#
# The FreeRTOSConfig.h is application-owned and lives with the hub control
# program (single source of truth); both modes consume it from there.

cmake_minimum_required(VERSION 3.20)

set(NP_FREERTOS_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Portable kernel sources (croutine.c is deprecated and intentionally omitted;
# configUSE_CO_ROUTINES = 0). event_groups.c references xTimerPendFunctionCall,
# so timers.c is required.
set(NP_FREERTOS_KERNEL_SRC
    ${NP_FREERTOS_DIR}/kernel/tasks.c
    ${NP_FREERTOS_DIR}/kernel/list.c
    ${NP_FREERTOS_DIR}/kernel/queue.c
    ${NP_FREERTOS_DIR}/kernel/event_groups.c
    ${NP_FREERTOS_DIR}/kernel/timers.c
    ${NP_FREERTOS_DIR}/kernel/stream_buffer.c
    ${NP_FREERTOS_DIR}/portable/MemMang/heap_4.c
)

# FreeRTOSConfig.h + application hooks live in the hub control program.
set(NP_FREERTOS_CONFIG_DIR ${NP_FREERTOS_DIR}/../../hub_control/include
    CACHE PATH "Directory containing NeurOne's FreeRTOSConfig.h")
set(NP_FREERTOS_HOOKS_SRC ${NP_FREERTOS_DIR}/../../hub_control/src/np_hub_freertos_hooks.c)

# ── Host smoke test (POSIX port) ───────────────────────────────────────────────
if(NP_BUILD_TESTS)
    add_executable(np_freertos_smoke_tests
        ${NP_FREERTOS_KERNEL_SRC}
        ${NP_FREERTOS_DIR}/portable/ThirdParty/GCC/Posix/port.c
        ${NP_FREERTOS_DIR}/portable/ThirdParty/GCC/Posix/utils/wait_for_event.c
        ${NP_FREERTOS_HOOKS_SRC}
        ${NP_FREERTOS_DIR}/test/np_freertos_smoke_tests.c
    )
    target_include_directories(np_freertos_smoke_tests PRIVATE
        ${NP_FREERTOS_DIR}/kernel/include
        ${NP_FREERTOS_DIR}/portable/ThirdParty/GCC/Posix
        ${NP_FREERTOS_DIR}/portable/ThirdParty/GCC/Posix/utils
        ${NP_FREERTOS_CONFIG_DIR}
    )
    # NP_FREERTOS_POSIX_HOST selects the host branches of FreeRTOSConfig.h and
    # the application hooks. _GNU_SOURCE exposes usleep/clock_gettime/pthread_kill
    # prototypes on both glibc (Linux CI) and Darwin.
    target_compile_definitions(np_freertos_smoke_tests PRIVATE
        NP_FREERTOS_POSIX_HOST
        _GNU_SOURCE
    )
    # gnu11 (not strict c11) so the POSIX port sees the POSIX prototypes.
    # No -Werror: vendored kernel sources are SOUP, not first-party code.
    target_compile_options(np_freertos_smoke_tests PRIVATE -std=gnu11 -Wall -Wextra)

    find_package(Threads REQUIRED)
    target_link_libraries(np_freertos_smoke_tests PRIVATE Threads::Threads)
    return()
endif()

# ── Cross build: Cortex-M7 static library ──────────────────────────────────────
add_library(np_freertos STATIC
    ${NP_FREERTOS_KERNEL_SRC}
    ${NP_FREERTOS_DIR}/portable/GCC/ARM_CM7/r0p1/port.c
)

# Kernel headers, the ARM_CM7 port, and FreeRTOSConfig.h are exported so that any
# consumer (np_hub_control and the other modules that include FreeRTOS.h) resolves
# them via linkage to np_freertos.
target_include_directories(np_freertos PUBLIC
    ${NP_FREERTOS_DIR}/kernel/include
    ${NP_FREERTOS_DIR}/portable/GCC/ARM_CM7/r0p1
    ${NP_FREERTOS_CONFIG_DIR}
)

set(NP_FREERTOS_CPU_FLAGS
    -mcpu=cortex-m7
    -mthumb
    -mfpu=fpv5-d16
    -mfloat-abi=hard
)

# Vendored SOUP: build clean but do NOT enforce -Werror on third-party sources.
target_compile_options(np_freertos PRIVATE
    ${NP_FREERTOS_CPU_FLAGS}
    -std=c11
    -Wall
    -Wextra
    -O2
    -ffunction-sections
    -fdata-sections
    -fno-common
    -ffreestanding
)
target_compile_options(np_freertos PUBLIC ${NP_FREERTOS_CPU_FLAGS})
