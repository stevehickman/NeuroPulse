# Vendored NXP MCUXpresso SDK (MIMXRT1062 device layer) — NeurOne integration notes

**MCUX SDK 2.16.0**, tag `MCUX_2.16.000` — the device layer for the **SW-02 main
processor** (NXP i.MX RT1062, Cortex-M7 @ 600 MHz). **IEC 62304 Class B SOUP.**
Formal records: `VERSION` and NP-SW-001 §9.4. Vendored under the NP-SW-CI-001 §9
decision; closes **OI-SWCI-20**.

Six files and a licence. Read `VERSION` before adding a seventh.

## What is here, and what "device layer" means

This is *not* the MCUX SDK. It is the part of it that a linked image cannot be
produced without:

| File | What it is |
|---|---|
| `devices/MIMXRT1062/MIMXRT1062.h` | the CMSIS device header — every peripheral's register map |
| `devices/MIMXRT1062/MIMXRT1062_features.h` | per-peripheral feature macros the header keys off |
| `devices/MIMXRT1062/fsl_device_registers.h` | dispatches a `CPU_MIMXRT1062*` macro to the two above |
| `devices/MIMXRT1062/system_MIMXRT1062.{h,c}` | `SystemInit()`, `SystemCoreClock` |
| `devices/MIMXRT1062/gcc/startup_MIMXRT1062.S` | vector table + `Reset_Handler` |

The peripheral **drivers** (`fsl_clock.c`, `fsl_lpi2c.c`, `fsl_pwm.c`, …) are
deliberately absent. SW-02 makes zero `fsl_*` calls today — verified by
word-boundary grep, which returns nothing outside comments — because the layer
that would call them has not been written. That layer is the 92 unresolved
symbols `firmware/platform/` currently stands in for (NP-SW-CI-001 §4.8). Vendoring
drivers for a caller that does not exist means guessing the subset, and every
guessed file is Class B SOUP surface to justify. They get vendored when the
driver that needs them is written.

## The linker script is NOT vendored, and that is the important one

Every SDK linker script for this part — `MIMXRT1062xxxxx_flexspi_nor.ld` and its
siblings — places `.text` at `0x60002400`, for execution in place from FlexSPI
NOR flash. **NeurOne has no FlexSPI application image.** The bootloader reads the
application out of an eMMC bank into the OCRAM staging area and jumps through the
vector table it finds there (`firmware/bootloader/src/np_main.c`
`load_and_jump()`). An SDK script would put the image somewhere the bootloader
never looks.

So NeurOne authors its own: `firmware/application/linker/app_imxrt1062.ld`. Its
agreement with the bootloader's reservation is not a comment — it is
`np_app_link_agreement_tests`, which parses both scripts and fails if the two
disagree about where the staging area starts or how large it is. That test exists
because the same duplication, written once in C and once in a linker script, is
Defect C (NP-SW-CI-001 §4.3).

## What the startup file needs from us

`startup_MIMXRT1062.S` is assembled with two `-D`s, both set in
`firmware/application/CMakeLists.txt`:

- `__STARTUP_CLEAR_BSS` — the C library startup normally zeroes `.bss`; this
  image does not run one.
- `__START=main` — the default target is newlib's `_start`, which expects a
  hosted environment (`_sbrk`, `_write`, `_exit`). This image is `-ffreestanding`
  and gets its heap from FreeRTOS `heap_4`, so it enters `main()` directly.

Neither is a patch to the vendored file. Both are documented switches the file
already tests for, which is why the copy here stays byte-exact.

## Why vendored (not fetched, not a submodule)

NP-SW-CI-001 §9: a medical-device build that reaches the network is neither
reproducible nor free of an unassessed supply-chain surface. Same pattern as
`firmware/vendor/freertos`, `firmware/vendor/cmsis_core` and
`firmware/crypto/vendor/monocypher`.

## Why Class B, and where the Class C bar would apply

SW-02 is Class B (NP-SW-001), so NP-SW-CI-001 §9.3 obligation 5 — the recorded
IEC 62304 §7.1.2 anomaly evaluation — does not attach here. That is a claim about
reachability and `VERSION` states how it is checked: `firmware/safety_mcu/` is a
separate CMake project with a separate toolchain file, and nothing on its include
path mentions MIMXRT1062. If that ever stops being true, obligation 5 applies and
this record is incomplete until the evaluation exists.

## CMSIS-Core(M) is not duplicated here

`MIMXRT1062.h:280` includes `core_cm7.h`, which lives once at
`firmware/vendor/cmsis_core/Include/` alongside the Cortex-M0+ header SW-01 uses.
One copy, two SW items — see that directory's `VERSION`.

## Verification

```bash
cmake -B build/cross -G Ninja firmware \
      -DCMAKE_TOOLCHAIN_FILE="$PWD/firmware/cmake/arm-none-eabi.cmake" \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build/cross --target np_application
arm-none-eabi-size build/cross/application/np_application.elf
```

`MIMXRT1062_features.h`'s necessity was established by deletion, not by reading
the `#if`: removing it fails the compile of `system_MIMXRT1062.c`.

## Standing obligation

Moving the pinned tag is a reviewable commit, not a refresh. Re-verify byte
exactness, update the SHA-256 list in `VERSION`, and re-check that the "why each
file is here" line numbers still name the right lines — they are the evidence
that the subset is minimal rather than convenient.
