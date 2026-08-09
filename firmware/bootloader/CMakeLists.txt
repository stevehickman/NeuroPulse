# NeurOne Bootloader — CMake Build
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
# Only the bootloader IMAGE requires the cross-compiler.  Since NP-SW-CI-001
# phase 1 this file also carries a host-native test target for the freestanding
# memory primitives (below, under NP_BUILD_TESTS), which builds with the host
# compiler and no ARM toolchain at all — the same shape every other module in
# firmware/ uses.
if(NOT NP_BUILD_TESTS AND NOT CMAKE_CROSSCOMPILING)
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
    src/np_mem.c        # freestanding memcpy/memset (NP-SW-CI-001 §4.2)
    src/np_app_image.c  # OCRAM staging limits from the linker (NP-SW-CI-001 §4.3)
)

if(CMAKE_CROSSCOMPILING)

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

# ── np_mem.c: suppress loop-idiom recognition ──────────────────────────────
# NP-SW-CI-001 §4.2.  At -O2, -ftree-loop-distribute-patterns rewrites a naive
# byte-copy or byte-fill loop into a call to memcpy/memset — including when
# that loop is the body of memcpy/memset itself.  The result LINKS, and
# recurses until the 8 KiB stack is gone.
#
# Applied per-source, not to the target: every other bootloader translation
# unit is entitled to the optimisation, and the risk is confined to the one
# file that defines the primitives the pass would call.
#
# Measured 2026-08-09 on ARM GNU 14.2.1 (cortex-m7, hard float) against these
# exact loops: plain -O2 emits a memset call; adding -fno-builtin suppresses
# it.  So the bootloader's existing -fno-builtin above already covers this on
# THIS compiler.  That is an implementation detail of how GCC's loop
# distribution looks up an implicit builtin declaration, it is not a
# guarantee, and the host-test build is a different compiler at different
# flags.  Belt and braces, on a failure mode that is silent and fatal.
set_source_files_properties(src/np_mem.c
    PROPERTIES COMPILE_OPTIONS "-fno-tree-loop-distribute-patterns")

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

endif() # CMAKE_CROSSCOMPILING

# ── Host test build (NOT cross-compiled) ───────────────────────────────────
# NP-SW-CI-001 phase 1.  The bootloader's first host test target: the
# freestanding memory primitives, checked against the host libc as oracle.
#
# NP_MEM_HOST_TEST drops the memcpy/memset alias definitions from np_mem.c, so
# the host libc used as the oracle is not displaced by the code under test —
# and so the tests call np_memcpy/np_memset, names for which the host compiler
# has no builtin to substitute.  Without that, a test could pass having never
# entered this repository's code.
#
# The rest of the bootloader stays cross-compile-only: it is device firmware
# with no host-executable surface.  Adding it here would produce a green check
# that proves nothing.
if(NP_BUILD_TESTS)
    add_executable(np_bootloader_mem_tests
        tests/np_bootloader_mem_tests.c
        src/np_mem.c
    )
    target_include_directories(np_bootloader_mem_tests PRIVATE include)
    target_compile_options(np_bootloader_mem_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g -DNP_MEM_HOST_TEST)

    # Same reasoning as the cross build above, for the host compiler.  -O0 does
    # not run loop distribution today, but the flag is what keeps that true if
    # the host optimisation level is ever raised.  Guarded because clang, which
    # is the host compiler on macOS, rejects the flag outright.
    include(CheckCCompilerFlag)
    check_c_compiler_flag("-fno-tree-loop-distribute-patterns"
                          NP_HAVE_NO_LOOP_DISTRIBUTE)
    if(NP_HAVE_NO_LOOP_DISTRIBUTE)
        set_source_files_properties(src/np_mem.c PROPERTIES COMPILE_OPTIONS
            "-fno-builtin;-fno-tree-loop-distribute-patterns")
    else()
        set_source_files_properties(src/np_mem.c PROPERTIES COMPILE_OPTIONS
            "-fno-builtin")
    endif()

    # ── Application staging limits (NP-SW-CI-001 §4.3, Defect C) ────────────
    # NP_APP_IMAGE_HOST_TEST drops np_app_image.c's linker-symbol readers, which
    # cannot exist on a host.  Deliberately NOT replaced with a host stand-in: a
    # stand-in would let this suite pass while the device's C-to-linker binding
    # was broken, which is the exact failure it exists to catch.  That binding is
    # verified on the linked artifact instead (nm/objdump — see the test header).
    #
    # The test reads the real linker script and evaluates its arithmetic, so the
    # limit it checks the predicate against is the one that actually ships.  The
    # path is passed in rather than discovered at runtime, because ctest runs the
    # binary from the build tree.
    add_executable(np_bootloader_app_image_tests
        tests/np_bootloader_app_image_tests.c
        src/np_app_image.c
    )
    target_include_directories(np_bootloader_app_image_tests PRIVATE include)
    target_compile_options(np_bootloader_app_image_tests PRIVATE
        -std=c11 -Wall -Wextra -O0 -g)
    target_compile_definitions(np_bootloader_app_image_tests PRIVATE
        NP_APP_IMAGE_HOST_TEST
        NP_LINKER_SCRIPT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/linker/bootloader_imxrt1062.ld")
endif()
