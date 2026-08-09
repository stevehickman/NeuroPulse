# Vendored ARM CMSIS-Core(M) — NeurOne integration notes

ARM **CMSIS-Core(M) 5.6.0**, from the **CMSIS_5 `5.9.0`** tag — the processor
core access layer for the **SW-01 safety MCU** (STMicroelectronics STM32G071,
Cortex-M0+ @ 64 MHz). **IEC 62304 Class C SOUP.** The formal records are
`VERSION`, NP-SW-001 §9.4, and the §7.1.2 anomaly evaluation in
`docs/np_soup_cmsis_001.md`.

## Why this component is here at all

Not for `SysTick`, despite appearances. Every `SysTick` string in
`firmware/safety_mcu` is a comment or a NeurOne-authored identifier
(`NP_SAFETY_SYSTICK_HZ`, `np_hal_systick_init`, the vector-table entry in our own
startup assembly). No CMSIS `SysTick` struct access or `SysTick_Config()` call
exists in the tree.

It is here because **`stm32g071xx.h` line 115 includes `core_cm0plus.h`
unconditionally**. The ST device header cannot be used without it. That is a
structural dependency, and the distinction is recorded because a transitively
included header nothing calls carries a smaller hazard surface than one on a
timing-critical path — see `docs/np_soup_cmsis_001.md` §3.3.

## Why vendored (not a submodule, not fetched)

NP-SW-CI-001 §9: a medical-device build that reaches the network is neither
reproducible nor free of an unassessed supply-chain surface. Mirrors the
`firmware/vendor/freertos` and `firmware/crypto/vendor/monocypher` pattern.

## Why 5.9.0 rather than another tag

`5.9.0` is the terminal release of the CMSIS 5 series. Two reasons, both about
the Class C obligation rather than about newness:

- Its anomaly list is **closed**. No further 5.x fixes are coming, so "the
  published anomaly list for the version in use" is a stable thing to have
  evaluated, and will not churn under the record.
- It is the last 5.x, so it carries every 5.x fix.

It is *newer* than the CMSIS-Core that ST's own STM32CubeG0 package pairs with
this device header (Cube ships Core(M) 5.3.0). That is deliberate and is not a
compatibility risk here: the ST device header consumes only `core_cm0plus.h`
structure definitions and the `__NVIC_*` interface, which are stable across the
5.x series, and the vendored subset is verified by the build itself.

CMSIS_6 was not taken. It is a different major series that ST's G0 device headers
at v1.4.5 are not validated against, and the conservative choice for Class C is
the pairing the device vendor's own tooling implies.

## Layout

```
vendor/cmsis_core/
├── VERSION                   SOUP record (per-file provenance + SHA-256)
├── LICENSE.txt               Apache-2.0 (upstream, same tag)
├── README-NEURONE.md         this file
└── Include/
    ├── core_cm0plus.h        Cortex-M0+ core peripheral access layer (V5.0.9)
    ├── cmsis_version.h       included by core_cm0plus.h:63
    ├── cmsis_compiler.h      included by core_cm0plus.h:115
    ├── cmsis_gcc.h           selected by cmsis_compiler.h:54 for __GNUC__
    └── mpu_armv7.h           included by core_cm0plus.h:1007 (__MPU_PRESENT 1U)
```

There is **no CMakeLists.txt here** and that is deliberate — unlike
`vendor/freertos`, this component compiles nothing. It contributes headers only.
The include path is added by `firmware/safety_mcu/CMakeLists.txt` as a `SYSTEM`
directory so that `-Wall -Wextra -Werror` stays fully in force on first-party code
while the vendored headers' diagnostics are suppressed. That is the header-only
analogue of the `-w` the Monocypher precedent applies to SOUP *sources*; no
warning is disabled repository-wide.

## What is NOT here

`VERSION` carries the full list. The short version: everything else in CMSIS_5 —
DSP, NN, RTOS/RTOS2, DAP, Driver, Pack and the other cores and compiler families.
`cmsis_compiler.h` references `cmsis_armcc.h`, `cmsis_armclang*.h`,
`cmsis_iccarm.h` and `cmsis_ccs.h` from `#elif` branches a `__GNUC__` build never
takes; SW-01 builds with `arm-none-eabi-gcc` only, so they are unreachable and are
absent rather than carried as unreviewed Class C SOUP.

## Verification

The component has no test of its own — a header set is verified by the build that
consumes it, and by the artifact that build produces:

```bash
cmake -B build/safety -G Ninja firmware/safety_mcu \
      -DCMAKE_TOOLCHAIN_FILE="$PWD/firmware/cmake/stm32g071.cmake" \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build/safety --target np_safety_mcu_objs
```

`mpu_armv7.h`'s necessity was established by deletion rather than by reading the
`#if`: removing it fails the compile with `mpu_armv7.h: No such file`. NeurOne
calls no MPU function, so it would otherwise look like an unjustified file.

## Standing obligation

Changing the pinned tag **requires re-running the §7.1.2 anomaly evaluation** and
revising `docs/np_soup_cmsis_001.md`. This is Class C SOUP; the evaluation is
attached to a version, not to the component.
