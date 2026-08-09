# Vendored ST CMSIS-Device STM32G0 — NeurOne integration notes

STMicroelectronics **CMSIS-Device STM32G0 `v1.4.5`** — the device peripheral
access layer for the **SW-01 safety MCU** (STM32G071, Cortex-M0+ @ 64 MHz).
**IEC 62304 Class C SOUP.** The formal records are `VERSION`, NP-SW-001 §9.4, and
the §7.1.2 anomaly evaluation in `docs/np_soup_cmsis_001.md`.

## What it fixes

Defect A in NP-SW-CI-001 §4.1: `np_safety_config.h` mapped every stimulation
enable line to `GPIOA`/`GPIOB`, CMSIS device symbols that nothing in-tree defined,
so the Class C firmware did not compile for its target in a clean checkout. This
component supplies them.

## Device headers only — the ST HAL and LL drivers are NOT here

This is the resolution of **OI-SWCI-06**, and it is a deliberate engineering
decision rather than the easier copy.

The safety MCU makes **zero `HAL_*` and zero `LL_*` calls**. Measured with
word-boundary matching over `firmware/safety_mcu/{src,include,startup}`:

```bash
rg '\bHAL_|\bLL_[A-Z]' firmware/safety_mcu/src firmware/safety_mcu/include \
                       firmware/safety_mcu/startup
```

returns nothing. A naive substring search for `LL_` *appears* to find hits — they
are `NP_SAFETY_EN_ALL_MASK` and `NP_THERMAL_POLL_MS`, and they are not driver
calls. The firmware is register-level throughout.

Register/bit definitions are essentially a machine-readable hardware description.
The HAL is tens of thousands of lines of substantive third-party control flow.
Under IEC 62304 §7.1.2 every vendored file is Class C SOUP surface that must be
justified and anomaly-reviewed, so the two carry very different review burdens —
which is the real argument for headers-only, beyond repository size.

**`USE_HAL_DRIVER` is therefore not defined by the build.** `stm32g0xx.h` ends
with:

```c
#if defined (USE_HAL_DRIVER)
 #include "stm32g0xx_hal.h"
#endif
```

so defining it would demand the un-vendored HAL and hard-fail the compile. It
*was* defined in `firmware/safety_mcu/CMakeLists.txt` before this change, harmless
only because no device header existed in-tree to act on it. Removing it is part of
this vendoring, and NP-SW-CI-001 §6.5 records a mutation test (M2) that reinstates
it and confirms the build goes red.

## `system_stm32g0xx.c` is not vendored, and that was determined, not assumed

The `.c` template providing `SystemInit()` is absent because **nothing calls it**.
`Reset_Handler` in `firmware/safety_mcu/startup/startup_stm32g071xx.s` copies
`.data`, zeroes `.bss`, and branches straight to `main` — read from the assembly
rather than inferred.

Note that the startup file's own header comment says *"then call SystemInit and
main"*. The comment is wrong; the code is the record. Flagged as **OI-SWCI-19** so
the divergence is fixed deliberately rather than by someone "restoring" a call
that was never there.

`system_stm32g0xx.h` **is** vendored — `stm32g071xx.h` line 116 includes it
unconditionally — but it contains only declarations (`SystemInit`,
`SystemCoreClock`, `AHBPrescTable`, `APBPrescTable`), none of which SW-01
references, so no definition is needed to link. Clock bring-up is
`np_hal_clock_init()`, first-party code.

## Layout

```
vendor/cmsis_device_g0/
├── VERSION                       SOUP record (per-file provenance + SHA-256)
├── LICENSE.md                    Apache-2.0 (upstream, same tag)
├── README-NEURONE.md             this file
└── Include/
    ├── stm32g0xx.h               family header; dispatches on STM32G071xx
    ├── stm32g071xx.h             GPIOA/GPIOB, SPI1, TIM2, ADC1, memory map
    └── system_stm32g0xx.h        declarations only (see above)
```

No `CMakeLists.txt`: this component compiles nothing. The include path is added by
`firmware/safety_mcu/CMakeLists.txt` as a `SYSTEM` directory, keeping
`-Wall -Wextra -Werror` in full force on first-party code.

## Licence note

The headers themselves do **not** carry an SPDX identifier — they carry ST's
"licensed under terms that can be found in the LICENSE file" attention block. The
licence was therefore read from `LICENSE.md` at the same tag, which is the Apache
License 2.0. Worth knowing because ST has moved several CMSIS-Device repositories
between licences across releases, so the licence is a property of the pinned tag
and must be re-read if the tag moves.

## Verification

```bash
cmake -B build/safety -G Ninja firmware/safety_mcu \
      -DCMAKE_TOOLCHAIN_FILE="$PWD/firmware/cmake/stm32g071.cmake" \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build/safety --target np_safety_mcu_objs
```

A successful compile proves the symbols resolved, not that they resolved to the
right addresses, so that is checked on the artifact:

```bash
arm-none-eabi-objdump -d \
  build/safety/CMakeFiles/np_safety_mcu_objs.dir/src/np_gpio_mgr.c.obj \
  --section=.text.np_gpio_mgr_init
```

Measured: `GPIOA` as `movs r0,#160; lsls r0,#23` = `0x50000000`, and `GPIOB` as a
literal-pool `0x50000400` — the correct `GPIOA_BASE`/`GPIOB_BASE` for STM32G071.

## Standing obligation

Changing the pinned tag **requires re-running the §7.1.2 anomaly evaluation** and
revising `docs/np_soup_cmsis_001.md`, and re-reading the licence (see above). This
is Class C SOUP; the evaluation is attached to a version, not to the component.
