# CMSIS SOUP Anomaly Evaluation — Safety MCU (SW-01, Class C)

**Project:** NeurOne  
**Document:** NP-SOUP-CMSIS-001  
**Revision:** 1
**Date:** 2026-08-09  
**Status:** DRAFT  
**Effective Date:** —  
**Author:** Steve Hickman (CEO, interim Quality authority)  
**Approved By:** — (DRAFT, not approved)  
**References:** NP-SW-001 Rev 3 §9.4 (SOUP register), NP-SW-CI-001 Rev 6 §9 (vendoring decision) and §6.5 (phase 4), `firmware/vendor/cmsis_core/VERSION`, `firmware/vendor/cmsis_device_g0/VERSION`  
**Related Issues:** OI-SWCI-06 (device headers vs full HAL — closed by this work)  
**Gate:** G2  
**IEC 62304 Class:** SW-01 Class C  
**Supersedes:** None  
**Change Summary:** Rev 1 (2026-08-09) — initial revision. The IEC 62304 §7.1.2 anomaly-list evaluation for the two CMSIS SOUP components vendored for the safety MCU at NP-SW-CI-001 phase 4.  
**Review Cadence:** On any change to the pinned tag of either component, and at G2

---

> **Why this document exists.** IEC 62304 §7.1.2 requires that, for SOUP in the
> software system, the **publicly available list of known anomalies for the SOUP
> version actually in use** be evaluated for anomalies that could result in a
> hazardous situation, and that the evaluation be recorded. SW-01 is the software
> that owns every stimulation enable GPIO, so this is the first NeurOne SOUP
> component to which the Class C bar applies: FreeRTOS is Class B and Monocypher
> was handled as B + C on a much smaller surface. Recording the conclusion is not
> the same as performing the activity, so the method and the evidence are below,
> not just the verdict.

## 1. Scope

Two components, two upstreams, two licences, evaluated separately:

| # | Component | Pinned version | Upstream | Files |
|---|-----------|----------------|----------|-------|
| C1 | ARM CMSIS-Core(M) | 5.6.0, from CMSIS_5 tag `5.9.0` | ARM-software/CMSIS_5 | 5 headers |
| C2 | ST CMSIS-Device STM32G0 | `v1.4.5` | STMicroelectronics/cmsis-device-g0 | 3 headers |

Both are **header-only**: register/bit definitions, inline intrinsics, and type
declarations. Neither contributes a translation unit to the image. That is a
consequence of the OI-SWCI-06 decision to vendor device headers rather than the
ST HAL, and it materially bounds this evaluation — there is no third-party
control flow in SW-01 to review, only declarations that first-party code acts on.

**Out of scope.** Silicon errata for the STM32G071 device (ST document ES0418)
are a hardware anomaly list, not a SOUP anomaly list, and are handled under the
hardware DHF rather than here. The distinction matters: a wrong bit definition in
a header is a SOUP defect; a peripheral that misbehaves as specified is not.

## 2. Method, and what it does and does not cover

Performed 2026-08-09 against the pinned versions:

1. **Vendor-published release notes** for the exact tag — for C2, `Release_Notes.html`
   at tag `v1.4.5`, which carries an explicit *Known Limitations* section per
   release. This is the vendor's own anomaly list and is the primary source.
2. **The upstream public issue tracker**, filtered to the vendored files. For C1
   this is the GitHub issue list for ARM-software/CMSIS_5, queried for
   `core_cm0plus`, `mpu_armv7`, and `cmsis_gcc.h` against Cortex-M0/M0+.
3. **A version-to-version differential** for C2, diffing every peripheral struct
   definition in `stm32g071xx.h` between `v1.4.4` and `v1.4.5`, to establish
   whether the fixes named in the release notes touched anything SW-01 depends on.

**Limitations, stated rather than left implicit.** A public issue tracker is not a
formal errata list, and ARM does not publish a numbered errata document for
CMSIS-Core the way ST publishes one for silicon. The search is keyword-driven and
therefore not exhaustive: an anomaly in a vendored file that mentions none of the
searched terms would be missed. This is a real residual and is accepted here on
the strength of the surface being five headers of declarations rather than a body
of logic — but it is the reason the review cadence above is *on any tag change*
rather than annual. Re-running the search is cheap; assuming the result carries
forward to a different version is not.

## 3. C1 — ARM CMSIS-Core(M) 5.6.0 (CMSIS_5 tag 5.9.0)

### 3.1 The dominant finding is a scoping one

CMSIS_5 carried **188 open issues** at the time of review. Essentially all of them
are in components NeurOne does not vendor: CMSIS-DSP, CMSIS-NN, SVDConv, CMSIS-DAP,
CMSIS-RTOS2, and the Pack tooling. The vendored subset is five files out of a
repository of thousands, and the anomaly surface follows the subset, not the
repository.

This is worth stating plainly because it is the return on the narrow-vendoring
decision in NP-SW-CI-001 §9.4, and it is the reason a Class C anomaly review of a
large upstream came out this clean. Had the full HAL/driver/DSP surface been
vendored "because it was easier to copy", this section would be a very different
piece of work.

### 3.2 Anomalies touching the vendored files

| Upstream item | State | Touches | Hazard assessment |
|---|---|---|---|
| #617 `[cmsis_gcc.h] nested extern declaration` | open | `cmsis_gcc.h` | **Not applicable.** A `-Wnested-externs` diagnostic on `extern` declarations inside `__cmsis_start()`. It is a warning, not codegen. SW-01 does not use `__cmsis_start` — startup is first-party assembly (`startup_stm32g071xx.s`) branching to `main`. Additionally the vendored headers are on a `SYSTEM` include path, so their diagnostics cannot reach the first-party `-Werror`. No hazardous situation. |
| #1397 `cmsis_gcc.h warnings` | open | `cmsis_gcc.h` | **Not applicable.** Compiler diagnostics under specific warning sets. Same reasoning as #617. No behavioural claim. |
| `Core: cmsis_gcc.h missing v8.1-M support in ifdefs` | open | `cmsis_gcc.h` | **Not applicable.** Armv8.1-M feature detection. STM32G071 is Cortex-M0+ (Armv6-M). The affected `#ifdef` branches are unreachable for this target. |
| #227 `Misra violation in mpu_armv7.h, mpu_armv8.h` | closed 2017 | `mpu_armv7.h` | **Not a defect; a scope question.** MISRA C:2012 is mandatory for SW-01 (NP-SW-001 §4.3). Vendored SOUP is exempt from the MISRA scan with documented origin per IEC 62304 §8.1.2 — the same treatment already applied to Monocypher, whose SOUP record records the scan as targeting first-party `np_crypto.c` only. NeurOne calls no MPU function; `mpu_armv7.h` is present solely because `core_cm0plus.h` includes it when `__MPU_PRESENT == 1U`. No hazardous situation. |
| `fixed typo in MPU->RASR register name`, `Correct typo in comment MPU Sample Register` | closed | `mpu_armv7.h` | **Not applicable.** Comment/name corrections already present at 5.9.0. No MPU code is called. |

Queries for `core_cm0plus` returned no issues at all.

### 3.3 The two paths that actually matter

SW-01's hazard-relevant behaviour reaches this component through exactly two
surfaces, so both were checked directly rather than left to the keyword search:

- **The enable-GPIO path.** `np_gpio_mgr.c` is the only module that writes
  stimulation GPIO. It touches C1 not at all — `GPIOA`/`GPIOB` come from C2, and
  the writes go through first-party `np_hal_gpio_write_pin()`. C1's contribution
  is limited to being transitively included. Verified on the compiled artifact:
  the object references `0x50000000` (`movs r0,#160; lsls r0,#23`) and
  `0x50000400`, the correct STM32G071 `GPIOA_BASE` and `GPIOB_BASE`.
- **The SysTick path.** Named in the phase-4 brief as the reason CMSIS-Core is
  needed, and **it is not** — every `SysTick` occurrence in `firmware/safety_mcu`
  is a comment or a NeurOne-authored identifier (`NP_SAFETY_SYSTICK_HZ`,
  `np_hal_systick_init`, the `SysTick_Handler` vector entry defined in first-party
  assembly). No CMSIS `SysTick` struct access and no `SysTick_Config()` call
  exists. C1 is required because `stm32g071xx.h` line 115 includes
  `core_cm0plus.h` unconditionally, which is a structural dependency, not a usage
  one. Recorded because the two justifications have different consequences: a
  transitively-included header that nothing calls has a smaller hazard surface
  than one whose inline functions are on a timing-critical path, and it would be
  wrong to inherit the stronger claim by repetition.

### 3.4 C1 conclusion

**No anomaly in the vendored CMSIS-Core subset can contribute to a hazardous
situation in SW-01 at version 5.6.0 (tag 5.9.0).** Every item found against the
vendored files is a compiler diagnostic, a comment, a non-Armv6-M code path, or a
MISRA-scope question. No item alters the value of a register definition, the
layout of a peripheral struct, or the code generated on any path SW-01 executes.
The enable-GPIO path does not depend on this component's logic at all.

## 4. C2 — ST CMSIS-Device STM32G0 v1.4.5

### 4.1 The vendor's published anomaly list

`Release_Notes.html` at tag `v1.4.5` (27-February-2026) states, under its own
**Known Limitations** heading:

> None

The same is stated for v1.4.4, v1.4.3 and v1.4.2. This is ST's published anomaly
list for the version in use, and it is the authoritative source §7.1.2 asks for.

A "None" from a vendor is a weak result on its own, so the release notes were used
as a starting point rather than an ending one — v1.4.5 lists five *changes*, and a
change fixing something implies that something was wrong in the version before it.
Those five were checked against SW-01's actual dependencies.

### 4.2 What v1.4.5 changed, and whether SW-01 touches it

| v1.4.5 change | Touches SW-01? | Finding |
|---|---|---|
| **Correct the definition of the `TIMx_CCR5` Capture/Compare register** | Investigated closely — SW-01 uses TIM2 for R-peak interval capture | **No.** The correction narrowed `TIM_CCR5_CCR5_Msk` from `0xFFFFFFFF` to `0xFFFF`. It is a bit-mask `#define`, **not a struct member**, so no register offset moved. Confirmed by diffing every `typedef struct` block in `stm32g071xx.h` between v1.4.4 and v1.4.5: **no struct layout changes of any kind**. SW-01 never names `TIM_CCR5_*`; its TIM2 access is through first-party `np_hal_tim2_get_capture()`. We are on the corrected version regardless. |
| Merge `PWR_CR2_PVMEN_USB` / `PWR_CR2_IOSV` / `PWR_CR2_USV` into `PWR_CR2_PVM_VDDIO2`, keeping aliases | No | SW-01 does not use the PWR peripheral. Aliases retained upstream, so nothing breaks even if it later does. |
| Define `RTC_TAMP_NB`, `RTC_TAMP_INT_NB`, `RTC_BACKUP_NB` | No | RTC/tamper unused by SW-01. Additive macros. |
| Add `RCC_CFGR` oscillator-selection bit definitions | No | Additive. Clock setup is first-party `np_hal_clock_init()`. |
| Allow `VECT_TAB_OFFSET` to be redefined externally | No | Consumed by `system_stm32g0xx.c`, which is **not vendored** (SW-01 does not call `SystemInit`). |

### 4.3 The definitions SW-01 actually depends on

The peripherals SW-01 names are `GPIOA`, `GPIOB`, `SPI1`, `TIM2` and `ADC1`. The
v1.4.4→v1.4.5 struct diff being empty establishes that none of their register
layouts moved in the version step, and the base addresses were verified against
the header (`IOPORT_BASE` `0x50000000`; `GPIOA_BASE` `+0x000`, `GPIOB_BASE`
`+0x400`) and then against the compiled object, as recorded in §3.3.

### 4.4 C2 conclusion

**No anomaly in the vendored ST CMSIS-Device subset can contribute to a hazardous
situation in SW-01 at v1.4.5.** The vendor publishes no known limitations for this
version; the one v1.4.5 correction that could plausibly have mattered
(`TIMx_CCR5`) is a bit-mask SW-01 does not reference and did not move any struct
offset; and the remaining four changes are additive or touch unused peripherals.

## 5. Overall conclusion and residuals

**Conclusion.** For both vendored CMSIS components, at the exact versions pinned
in their `VERSION` records, no item on the publicly available anomaly lists can
contribute to a hazardous situation in SW-01. The stimulation enable-GPIO path —
the reason SW-01 is Class C — depends on these components only for the correct
numeric value of two peripheral base addresses, and those values were verified on
the compiled artifact rather than trusted.

**Residuals carried forward:**

1. The keyword-driven issue-tracker search for C1 is not exhaustive (§2). Mitigated
   by the small, declaration-only surface and by re-review on any tag change.
2. Nothing on-target has executed this code. NP-SW-CI-001 §1 puts on-target
   execution out of scope for the CI plan, and SW-01 cannot yet link at all
   (Defect E — the platform layer is unwritten). Both components are therefore
   verified as *compiled* correct, not as *run* correct. The `0x50000000` /
   `0x50000400` artifact check is the compensating control, in the same shape as
   the §4.2 and §6.3 artifact checks elsewhere in NP-SW-CI-001.
3. This document is DRAFT and unapproved, consistent with NP-SW-CI-001. It must be
   approved before the G2 gate, and re-run if either pinned tag moves.
