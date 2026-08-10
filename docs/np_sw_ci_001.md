# Firmware Cross-Compile Continuous Integration Plan

**Project:** NeurOne  
**Document:** NP-SW-CI-001  
**Revision:** G  
**Date:** 2026-08-09  
**Status:** DRAFT  
**Effective Date:** —  
**Author:** Steve Hickman (CEO, interim Quality authority)  
**Approved By:** — (DRAFT, not approved)  
**References:** NP-SW-001 Rev C (Software Development Plan), NP-SOUP-CMSIS-001 Rev A (Class C CMSIS anomaly evaluation, §7.1.2), NP-FW-EMMC-001 Rev A (bootloader boot sequence), NP-FW-CVNS-001 Rev A (safety MCU interlock), `.github/workflows/firmware-host-tests.yml` (retired 2026-08-08, Phase 0 — see §6.1)  
**Related Issues:** PR #250 (workflow least-privilege pass — where this gap was found)  
**Gate:** —  
**IEC 62304 Class:** SW-01 Class C (safety MCU), SW-02 Class B (main processor)  
**Supersedes:** None  
**Change Summary:** Rev G (2026-08-09) — **phase 5 complete; Defect D closed; OI-SWCI-12 closed; the main-firmware leg is promoted from reporting-only to gating, and it is the last leg to be promoted.** (1) New §4.6 gives Defect D its own subsection — it had only ever been recorded inline in §6.1 and in OI-SWCI-12, which is why §4's "six defects" count did not resolve to six headings. Resolution: `np_mod_pbm.c` now calls `np_pbm1064_dose_load_cal_stub()` **once per zone, one wavelength row per call**, copying the pattern already used at `np_pbm1064_session.c:271-273`. **The rename the compiler suggests is recorded as rejected**, with the measurement: `-Wincompatible-pointer-types` under `-Werror` rejects passing the 2-D `s_cal` to a one-row parameter, and had it been coerced through it would have populated row 0 only, leaving zones 1–4 with zeroed dose-metering calibration — a wrong J/cm² instead of a loud build failure. `s_cal`'s bound moves from `NP_PBM1064_ZONE_COUNT` to `NP_HUB_ZONE_SLOT_COUNT`, the macro that already guards every index into it; `np_pbm1064_config.h:147-154` states in terms that `NP_PBM1064_ZONE_COUNT` bounds the retired ZONE_ID detection path *only* and must not size a calibration array, so the old declaration was a documented rule violation one line from the defect — the same one-value-two-places shape as Defects C and F. (2) **§6's phase-5 row is corrected on its premise, not on its criterion.** Rev A–F said phase 5 required "Populate `NP_PLATFORM_INCLUDE_DIRS` with the MCUX SDK path". Measured: the cross-build has **zero** missing-header errors, because the platform layer is stubbed rather than called into; Defect D was the *only* error in the whole super-project. The MCUX SDK is real device-bring-up work and is not build-gating — it is decoupled from this phase and re-raised as **OI-SWCI-20**. The exit criterion itself, "Main firmware leg builds", stands verbatim — the first phase in this plan whose criterion has not needed correcting. (3) **New §4.7 records a measured limit of the promoted check, discovered while verifying that a link had actually happened rather than inferring it from a clean compile.** There is **no SW-02 application executable target in the repository**: the cross-build archives 13 static libraries and links exactly one ELF, `np_bootloader`, which the bootloader leg has gated since phase 3. `np_hub_control` is a `STATIC` library whose own CMakeLists names a parent `np_application` target that does not exist. So "zero undefined references" on this leg is true and **vacuous for SW-02** — the SW-02 HAL stubs (OI-PBM-HAL-01..03 and the OI-ZA/OI-HRV/OI-PBM/OI-CVNS families) can never be diagnosed by it. This is Defect E's shape one level further out, and it is raised as **OI-SWCI-21**, not fixed. §6's phase-5 row, §8's SW-02 traceability row and both workflow comment blocks are worded as **compiles**, never links, so a green check cannot be read as an image existing. (4) New §6.6 records the measured before/after (1 error → **0**, build `rc=1` → **`rc=0`**, 0 warnings and 0 unresolved throughout), the artifact enumeration behind §4.7, and a 5-run mutation set with a control either side — including M1, the Defect D regression itself, and M2, the naive rename. **M4 survives and is named rather than buried:** reverting `s_cal`'s bound to `NP_PBM1064_ZONE_COUNT` still compiles, because both macros are 5 today, so that half of the fix is a property of the source and not of any build output. (5) New §6.6.1 records the gate demonstration. Because this phase removes the *last* two masked legs, phases 3 and 4's within-run control — a second failing-but-masked job in the same run — no longer exists. It is replaced by a stronger one: the **breakage is held constant and only the setting varies**, so the run-conclusion difference is attributable to the setting on identical code. (6) `continue-on-error: true` removed from `firmware-cross-build.yml` job `main-firmware` and `build-all.yml` job `main-firmware-cross`; `rg 'continue-on-error: true' .github/workflows/` now returns nothing. Both `EXPECTED TO FAIL` comment blocks rewritten. (7) §8 traceability's SW-02 row is split along the same compile/link line phase 4 used for SW-01. (8) All three test-count guards untouched (`NP_SAFETY_TEST_COUNT` 6, `NP_CLASS_B_TEST_COUNT` 21, partition 6 + 21 = 27); no `permissions:` block and no branch-protection object touched; Defect E and phases 6–7 unchanged. Rev F (2026-08-09) — **phase 4 complete; Defect A closed; OI-SWCI-06 closed as device-headers-only.** (1) §4.1 gains a Resolution subsection: two Class C SOUP components vendored — ARM CMSIS-Core(M) 5.6.0 (CMSIS_5 tag `5.9.0`) and ST CMSIS-Device STM32G0 `v1.4.5`, eight Apache-2.0 header files, no translation unit — with `VERSION` records carrying per-file provenance and SHA-256, `README-NEURONE.md` integration notes, and explicit "intentionally NOT vendored" lists per the FreeRTOS template. **The ST HAL and LL drivers are excluded on evidence** (zero `HAL_*`, zero `LL_*` calls under word-boundary matching), and `USE_HAL_DRIVER` is removed — `stm32g0xx.h` includes `stm32g0xx_hal.h` under it, so it had become a hard build break the moment a device header existed. (2) **The §7.1.2 anomaly evaluation is performed and recorded as NP-SOUP-CMSIS-001 Rev A**, the first for Class C software: method, per-item hazard assessment, and stated residuals. Its principal finding is a scoping one — CMSIS_5's 188 open issues sit almost entirely in components not vendored — which is the measurable return on §9.4's narrowness instruction. §9.3 gains obligation 5 making a versioned anomaly record mandatory for Class C SOUP, and §9.4 gains a resolution note. (3) **Two premises in the phase-4 framing were falsified and are corrected in §4.1:** `SysTick` is *not* why CMSIS-Core is needed (every occurrence in `firmware/safety_mcu` is a comment or a NeurOne identifier; the real reason is `stm32g071xx.h:115`), and `system_stm32g0xx.c` is *not* needed because `Reset_Handler` branches straight to `main` — determined by reading the startup assembly, whose own comment says otherwise (OI-SWCI-19). (4) **Two defects were discovered behind Defect A, previously unreachable.** New §4.5 records Defect F, a duplicate `-specs=` on the link line, fixed here by deduplication — the same one-value-two-places shape as Defect C, and toolchain-version-sensitive (fails on ARM GNU 14.2.1, tolerated on CI's 13.2.Rel1). New §4.4 records **Defect E: SW-01 has no platform layer** — 24 first-party `np_hal_*` symbols defined only as test doubles inside the six host-test files, so the image has never linked. Not fixed: Class C driver work needing design review. Raised as OI-SWCI-17, with OI-SWCI-18 on the duplicated test doubles. (5) **§6's phase-4 exit criterion is corrected**, as phases 2 and 3 were before it. "Safety MCU compiles; drop its `continue-on-error`" assumed Defect A was the only obstacle; vendoring makes the firmware compile but not link, and a leg pointed at the executable stays permanently red — the exact outcome §6 opens by refusing. The criterion now splits along where the defects fall: **compilation** of all 10 Class C TUs is gated from phase 4 via a new `np_safety_mcu_objs` target, **linking** becomes a new phase 7 gated on OI-SWCI-17. §8's traceability row is split to match, so a green check cannot be read as "an image exists". (6) New §6.5 records the measured before/after (4 errors → **0**, 9 of 10 TUs → **10 of 10**, 0 warnings throughout), the `objdump` evidence that `GPIOA`/`GPIOB` resolve to the correct `0x50000000`/`0x50000400` — a compile proves resolution, not correctness — and a 3-of-3 mutation run with a control run either side, including M1, the Defect A regression itself, and M2, reinstating `USE_HAL_DRIVER`. Also corrects §4's row-2 `rc=2` to `rc=1` under Ninja. (7) All three test-count guards untouched (`NP_SAFETY_TEST_COUNT` 6, Class B 21, partition 6 + 21 = 27); no `permissions:` block and no branch-protection object touched. Defect D unchanged; phases 5–6 unchanged. Rev E (2026-08-09) — **phase 3 complete; the bootloader cross-build leg is promoted from reporting-only to gating.** (1) `continue-on-error: true` removed from the bootloader leg in both workflows that carry it — `firmware-cross-build.yml` job `bootloader` and `build-all.yml` job `bootloader-cross`. Four settings remain, all on legs with open defects: main-firmware ×2 (Defect D, phase 5) and safety MCU ×2 (Defect A, phase 4). (2) **§6's phase-3 exit criterion is corrected.** "Bootloader leg blocking" overclaimed: dropping `continue-on-error` changes what the workflow *reports*, not whether a merge is *prevented*. Merge enforcement is branch protection — an admin setting outside the repository, still OI-SWCI-04 at phase 6. The criterion now reads "a failing bootloader leg makes the workflow run conclude `failure`". (3) §6 additionally records that `continue-on-error` suppresses less than its name suggests: a masked job still reports `conclusion: failure` and still shows a red ✗ in the PR check list — only its contribution to the *run* conclusion is suppressed. A phase-3 verification reading job or check-run state alone would have passed identically before the change, so the run conclusion is the sole discriminating signal. (4) New §6.4 records the phase-3 demonstration: a deliberate unresolved-symbol breakage in `np_main.c` turned the run red, the revert turned it green, and a within-run control (main-firmware failing and masked on both runs) attributes the difference to the removed setting rather than to anything else. (5) §4's measured-results table gains a supersession banner — the 2026-08-06 measurement is preserved verbatim as a record, with a row-by-row pointer to §6.2/§6.3 and a caution that row 4's stated cause has not been true since phase 1. (6) The `EXPECTED TO FAIL` comment blocks on both promoted legs are rewritten, and record that OCRAM at exactly 100.00% is the tiling design rather than a near miss, the code-size gate being the separate 48 KiB linker `ASSERT` (~26 KB used). (7) §8 traceability updated. Defects A and D unchanged; phases 4–6 unchanged; no test-count guard, `permissions:` block or branch-protection object touched. Rev D (2026-08-09) — **phase 2 complete; Defect C closed.** (1) §4.3 gains a Resolution subsection: the defect was the *duplication*, not the number, so neither `448K` nor `440K` is written anywhere in the build. `bootloader_imxrt1062.ld` derives the reservation as `LENGTH(OCRAM) - _app_load_offset - _stack_size` and exports `_app_staging_start`/`_app_staging_size`; the new `np_app_image.c` reads those symbols and `np_main.c` computes neither the ceiling nor the load address — `NP_APP_LOAD_ADDR` is retired along with `remaining_ocram`, being the same defect shape one line away. Two link-time `ASSERT`s make the OCRAM tiling a law: staging starts at `_app_load_offset` and ends at `_stack_base`. (2) New §6.3 records the measured before/after (101.56% → **100.00%**, build `rc=2` → **`rc=0`**, region overflow gone), the `nm`/`objdump` artifact evidence that the compiled `np_app_max_image_size()` returns the linker's own `0x6e000` — a binding no host test can reach, so it is checked where it exists — and a 7-of-7 mutation run that includes M2, the forbidden "change 448 to 440" fix, and M4, the original `np_main.c` stack-overwrite bug. (3) **§6's phase-2 exit criterion is corrected.** "`--print-memory-usage` under 100%" was unachievable as written: the three regions are specified to tile OCRAM, so a correct layout reports exactly 100.00%, and `ld` errors on overflow rather than on full allocation. It now reads "links successfully; no region overflow". (4) OI-SWCI-02 closed. (5) Class B host-test count 20 → 21 and repo total 26 → 27 across `firmware-cross-build.yml` and `build-all.yml`, for the new `np_bootloader_app_image_tests`; `NP_SAFETY_TEST_COUNT` untouched at 6. (6) §8 traceability gains two rows. Defects A and D unchanged; phases 3–6 unchanged. Rev C (2026-08-09) — **phase 1 complete; Defect B closed.** (1) §4.2 rewritten: `-lgcc` is removed as an option because it was empirically falsified — the Cortex-M7 hard-float libgcc defines neither `memcpy`/`memset` nor any `__aeabi_mem*` variant — and `--specs=nano.specs` is recorded as rejected for pulling newlib into a Class B bootloader as new SOUP. The chosen fix, freestanding `memcpy`/`memset` in `firmware/bootloader/src/np_mem.c`, is recorded with its rationale, the `-O2` `-ftree-loop-distribute-patterns` self-recursion trap, and the disassembly check that is the actual evidence (a successful link is not). (2) New §6.2 records the measured before/after (26 unresolved → 0, region overflow now the sole error), the bootloader's first host test target, and a mutation run in which five of six broken implementations were killed and the survivor — an unaligned word-copy, undetectable by any output-comparison test — is named rather than buried. (3) Class B host-test count 19 → 20 and repo total 25 → 26 across `firmware-cross-build.yml` and `build-all.yml`; `NP_SAFETY_TEST_COUNT` untouched. (4) §8 traceability updated. Defects A, C and D unchanged; phases 2–6 unchanged. Rev B (2026-08-08) — records five decisions and one investigation. (1) §9: all SDKs vendored in-tree, closing OI-SWCI-01 and unblocking phases 4–5; flags the STM32G0 CMSIS/HAL as Class C SOUP carrying the IEC 62304 §7.1.2 anomaly-list obligation, raising OI-SWCI-06. (2) §5: the Class C safety MCU gets its own workflow rather than a matrix leg, closing OI-SWCI-03. (3) §5.0: build and test only what the PR could have changed — `paths:` lists narrowed to real dependency sets, not `firmware/**`. (4) §5.5: `build-all.yml`, an unscoped scheduled backstop, without which per-PR scoping would let undeclared dependency edges and out-of-repo rot go undetected indefinitely. (5) §5.0.1: no cross-build unless the native build is green; because `needs:` works only within a workflow, this forces each workflow to be self-contained and closes OI-SWCI-07 (safety-MCU host tests move in), at the cost of absorbing and retiring `firmware-host-tests.yml` (OI-SWCI-09). Investigation: §4.3 now records that the 448 KiB OCRAM staging figure is derived arithmetic from the original bootloader commit rather than a requirement, that the correct value is 440 KiB, and that the same wrong constant in `np_main.c:220` is a latent stack-corruption bug rather than merely a link error — restating OI-SWCI-02. No change to Defects A or B. Rev A (2026-08-06) — initial revision: establishes that no CI verifies the firmware cross-compiles for its target, records three measured blocking defects, and specifies a phased cross-compile workflow.  
**Review Cadence:** On each phase transition in §6, and on any change to `firmware/cmake/*.cmake`

---

> **⚠ DRAFT — not a baselined verification plan.** The three workflows in §5/§5.5 landed in phase 0 (§6.1), the bootloader's C-runtime defect closed in phase 1 (§6.2), its OCRAM over-subscription closed in phase 2 (§6.3), the bootloader leg was promoted out of `continue-on-error` in phase 3 (§6.4), the safety MCU's missing device headers closed in phase 4 (§6.5), and the main-firmware defect closed and its leg was promoted in phase 5 (§6.6).
>
> §4 records **six** independent blocking defects: **A, B, C, D and F are closed; E is open.** The bootloader configures and links cleanly (`rc=0`, OCRAM 100.00%). The safety MCU **compiles** for STM32G071 — all 10 Class C translation units, 0 errors, 0 warnings — and its compile leg gates from phase 4. It does **not link**: Defect E (§4.4, OI-SWCI-17) leaves 24 first-party `np_hal_*` platform symbols undefined, so no `np_safety_mcu` image has ever been produced, and phase 4's exit criterion was corrected accordingly (§6). The main-firmware super-project builds clean as of phase 5 — `rc=0`, 0 errors, 0 warnings, 0 unresolved symbols.
>
> **As of phase 5 no cross-compile leg carries `continue-on-error`.** All four are gating; the main-firmware leg was the last promoted.
>
> **Read each check for exactly what it claims.** The safety-MCU check says every Class C source compiles for the target — not that a linkable image exists. **The main-firmware check says every SW-02 translation unit compiles for the i.MX RT1062 — also not that an image exists**, and for a different reason: there is no SW-02 application executable target in this repository at all (§4.7, OI-SWCI-21). The bootloader check is the only one of the three that covers a real link. **No cross-compile leg is a required check.** A failing leg makes its workflow run conclude `failure`; none prevents a merge, because no branch protection exists (OI-SWCI-04, phase 6).

## 1. Purpose and scope

This plan covers **build verification for the cross-compiled firmware targets** — that the NeurOne firmware compiles and links for the hardware it ships on. It is a software-verification activity under NP-SW-001.

In scope: the i.MX RT1062 main-processor firmware (SW-02 Class B), the STM32G071 safety MCU firmware (SW-01 Class C), and the dual-bank OTA bootloader.

Out of scope: flashing, on-target execution, and hardware-in-the-loop testing. This plan verifies that the firmware **builds**, nothing more. Host-native unit testing is not restated here — as of Phase 0 it lives in the `host-tests` job of each of the two scoped workflows, absorbed from the retired `firmware-host-tests.yml` (§5.0.1, §6.1).

## 2. The gap

> **Historical as of Phase 0 (2026-08-08).** This section records the state that motivated the plan. The workflows described in §5 now exist and `firmware-host-tests.yml` is retired — see §6.1. Points 1 and 2 below remain true of the *code*; point 3's "nothing anywhere builds them" is closed.

The repository has one firmware workflow, `.github/workflows/firmware-host-tests.yml`. It configures with `-DNP_BUILD_TESTS=ON -DCMAKE_CROSSCOMPILING=OFF`, builds 13 modules host-native, and runs 25 ctest targets. That verifies *logic*. It does not verify that any of it builds for the target, because:

1. **The test branch returns before the cross-compiled tree.** `firmware/CMakeLists.txt` lines 122–242 are the `if(NP_BUILD_TESTS)` branch, and it ends in `return()`. Every `add_subdirectory()` below line 243 — the whole cross-compiled tree — is unreachable in the CI configuration.

2. **The safety MCU is a separate project.** `firmware/safety_mcu/CMakeLists.txt` declares `project(np_safety_mcu C ASM)` with its own toolchain file (`firmware/cmake/stm32g071.cmake`). Its cross-compile is built by no workflow. This is the **Class C** software that owns every stimulation enable GPIO.

3. **Two modules have no CI at all.** `firmware/bootloader` and `firmware/hrv_biofeedback` are reachable only through the cross-compiled path. They are correctly absent from the host-test workflow's `paths:` filter — that workflow could not build them even if triggered — which means nothing anywhere builds them.

Point 3 is worth stating precisely, because an audit that greps every `add_subdirectory()` call in `firmware/CMakeLists.txt` rather than only the `NP_BUILD_TESTS` branch will misreport it as a paths-filter drift bug. It is not. The paths filter is correct; the gap is the absent workflow. A note to that effect is in the workflow header.

## 3. Method

Cross-compile run locally, macOS, ARM GNU Toolchain 14.2.1 (satisfies the `NP_ARM_GCC_MIN_VERSION 12.3` gate in `firmware/cmake/arm-none-eabi.cmake`). Exit codes captured directly from each command rather than through a pipe — `cmd | tail` reports the exit status of `tail`, which masked a failure on the first attempt and would mask it again in any CI step written that way.

Four configurations were built: the three cross-compiled targets, plus the host-test build as a control.

## 4. Measured results

| # | Build | Configure | Build | Blocking defect |
|---|-------|-----------|-------|-----------------|
| 1 | Host tests (control) | `rc=0` | `rc=0` | — matches CI, no defect |
| 2 | Safety MCU (STM32G071, Class C) | `rc=0` | `rc=2` | Defect A — device headers absent |
| 3 | Bootloader (i.MX RT1062) | `rc=0` | `rc=2` | Defects B and C |
| 4 | Main firmware super-project | `rc=0` | `rc=2` | Fails at bootloader; remainder never attempted |

All three cross-compile targets **configure** cleanly and **fail to build**. The control builds clean, which confirms the method and the toolchain.

> **⚠ This table is the original investigation (2026-08-06) and is preserved as the measurement that was taken, not as a statement of current state.** All three of its failing rows have since been superseded:
>
> | Row | As measured 2026-08-06 | Current | Superseded by |
> |---|---|---|---|
> | 2 — Safety MCU | `rc=2`, Defect A | **compiles** (`rc=0` on `np_safety_mcu_objs`); does **not** link — Defect E open | §6.5 (Defect A), §4.4 (Defect E) |
> | 3 — Bootloader | `rc=2`, Defects B and C | **`rc=0`**, both defects closed | §6.2 (Defect B), §6.3 (Defect C) |
> | 4 — Main firmware | `rc=2`, "fails at bootloader" | **`rc=0`**, Defect D closed | §4.6, §6.6, OI-SWCI-12 |
>
> §4.2, §4.3 and §4.6 each carry their own Resolution subsection. Row 4's stated *cause* was the one to be careful with while it stood: it was measured under a sequential generator, and under Ninja the super-project failed first at `np_mod_pbm.c:112`, so "fails at bootloader" had not been true since phase 1 even while the row's `rc=2` still was. Both are now superseded. Row 4's `rc=0` should be read alongside §4.7 — the super-project builds clean, and what "builds" covers for SW-02 is narrower than the row's wording suggests.

### 4.1 Defect A — safety MCU has no CMSIS device headers

```
firmware/safety_mcu/include/np_safety_config.h:49:33:
  error: 'GPIOA' undeclared (first use in this function)
firmware/safety_mcu/include/np_safety_config.h:52:33:
  error: 'GPIOB' undeclared (first use in this function)
```

`np_safety_config.h` maps the stimulation enable lines to CMSIS device symbols (`GPIOA`, `GPIOB`) that nothing in-tree defines. The STM32G0 CMSIS/HAL headers are not vendored. The Class C firmware therefore does not compile for its target in a clean checkout.

**Resolution (2026-08-09, phase 4): CMSIS device headers vendored; the HAL is not.** Two SOUP components, two upstreams, two licences, two records: ARM CMSIS-Core(M) 5.6.0 from CMSIS_5 tag `5.9.0` in `firmware/vendor/cmsis_core/`, and ST CMSIS-Device STM32G0 `v1.4.5` in `firmware/vendor/cmsis_device_g0/`. Both Apache-2.0, both header-only, eight files in total, both with `VERSION` records carrying per-file provenance and SHA-256. `np_safety_config.h` now includes `stm32g0xx.h`, gated on `STM32G071xx` so the host-test build — which never compiles `np_gpio_mgr.c`, the sole consumer of the peripheral macros — is untouched. **This closes OI-SWCI-06 in favour of device headers only**, and the IEC 62304 §7.1.2 anomaly evaluation the Class C classification demands is recorded in NP-SOUP-CMSIS-001 Rev A. Measured: 4 errors → **0**, all 10 Class C translation units compile, 0 warnings. See §6.5.

**Two corrections to the framing this defect was investigated under, both recorded because they changed the work:**

- **`SysTick` is not why CMSIS-Core is needed.** The phase-4 brief named `SysTick` as used in six source files. Every occurrence in `firmware/safety_mcu` is a comment or a NeurOne-authored identifier — `NP_SAFETY_SYSTICK_HZ`, `np_hal_systick_init`, and the `SysTick_Handler` vector entry defined in our own startup assembly. There is no CMSIS `SysTick` struct access and no `SysTick_Config()` call anywhere. CMSIS-Core is required because `stm32g071xx.h` line 115 includes `core_cm0plus.h` unconditionally — a structural dependency, not a usage one. This is the same shape of error as the `LL_` substring trap the brief itself warned about, and it matters because a transitively-included header nothing calls carries a smaller hazard surface than one on a timing-critical path; inheriting the stronger claim by repetition would have overstated the review.
- **`USE_HAL_DRIVER` had to be removed, and was load-bearing in the opposite direction to how it read.** `firmware/safety_mcu/CMakeLists.txt` defined it alongside `STM32G071xx`, which looked like evidence the build expected the HAL. `stm32g0xx.h` ends with `#if defined (USE_HAL_DRIVER) / #include "stm32g0xx_hal.h" / #endif`, so with device headers present and no HAL vendored it is a hard compile failure. It had never been load-bearing before only because no device header existed in-tree to act on it. Regression-tested as mutant M2 in §6.5.

**Vendoring did not make this leg green.** It made it *compile*. Two further blocking defects sat behind Defect A, unreachable while the build died at compile time — see §4.4 and §4.5. The safety MCU still does not link.

### 4.2 Defect B — bootloader links with no C runtime

`firmware/bootloader/CMakeLists.txt` sets `-nostdlib -nostartfiles -nodefaultlibs`. GCC still emits implicit `memcpy` and `memset` calls for structure copies and array initialisation, and nothing provides them:

```
np_dfu.c:(.text.np_dfu_enter+0xb0): undefined reference to `memset'
  → 19 unresolved memcpy, 7 unresolved memset
```

Note this is consistent with the design intent recorded for `np_signature.c` in the document register — the bootloader deliberately avoids `np_crypto` because `-nostdlib/-nodefaultlibs` makes Monocypher unavailable. The freestanding choice is intentional; providing the handful of primitives GCC assumes is the missing piece.

This is not a defect in the freestanding configuration. A freestanding C implementation is *required* to supply `memcpy`, `memmove`, `memset` and `memcmp`; the compiler emits calls to them for aggregate assignment and array initialisation, and `-ffreestanding -fno-builtin` do not change that — those flags stop GCC assuming library *semantics* for calls the programmer writes, not GCC emitting these four. Providing them is conformance, not a workaround.

**Resolution (2026-08-09, phase 1): implemented in the bootloader.** `firmware/bootloader/src/np_mem.c` defines `np_memcpy`/`np_memset`, with the ABI names attached by `__attribute__((alias(...)))` so the shipped symbol and the host-tested symbol are one address rather than two copies. Byte-at-a-time, no alignment assumption — `load_and_jump()` stages an image whose Ed25519 signature has *already* been checked, so a subtly wrong copy corrupts it undetectably; the largest copy through these functions is 256 bytes, so there is nothing to buy with a word-at-a-time path and three extra chances to be wrong. `memcpy` is deliberately not overlap-safe: a caller needing that is itself the defect.

Only those two. `memmove` and `memcmp` are not implemented — they are not unresolved today, and CI will say so if that changes.

**Two of the three options originally listed here were wrong or unacceptable, and the list is corrected accordingly:**

- ~~`-lgcc`~~ — **does not work, verified.**
  `arm-none-eabi-nm --defined-only $(arm-none-eabi-gcc -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard -print-libgcc-file-name)` filtered for `mem(cpy|set|move|cmp)` and `__aeabi_mem` returns **nothing**. libgcc carries compiler support routines — integer division, soft-float, unwinding — not memory functions. Rev A listed this as viable; it was never viable.
- ~~`--specs=nano.specs` with libc~~ — would link, but reverses the deliberate no-libc decision and pulls newlib into a Class B bootloader: new SOUP with IEC 62304 §7.1.2 obligations, in exchange for two functions of about twenty lines each.

**One trap, recorded because it is silent.** At `-O2` GCC's `-ftree-loop-distribute-patterns` recognises a naive byte loop and rewrites it into a call to `memcpy`/`memset` — including when that loop is the body of `memcpy` itself. The result links cleanly and recurses until the 8 KiB stack is gone. Measured on ARM GNU 14.2.1 against these exact loops: plain `-O2` emits a `memset` call; adding `-fno-builtin` suppresses it, so the bootloader's existing flags already cover this *on this compiler*. That is an implementation detail of how the pass looks up an implicit builtin declaration, not a guarantee, and it does not extend to the host-test build. `np_mem.c` is therefore compiled with `-fno-tree-loop-distribute-patterns` in both builds, applied per-source so no other translation unit loses an optimisation.

**A successful link is not evidence here** — it proves the symbol resolved, not that the body is sane. The check is on the artifact:

```
arm-none-eabi-objdump -d np_bootloader.elf | sed -n '/<memcpy>:/,/^$/p'
```

Measured 2026-08-09 on the linked ELF (not the object — the object is not necessarily what wins at link time): both bodies are leaves (`bx lr`, no `push`), contain no `bl`/`blx` and no branch to any function entry, and contain **none** of `ldr str ldrh strh ldrd strd ldm stm vldr vstr ldrex strex` at any width. That absence is the machine-checkable proof that no alignment is assumed. State it as an absence rather than a whitelist of `ldrb.w`/`strb.w`: register allocation may legally emit the 16-bit `ldrb`/`strb` encodings, and a check pinned to `.w` would break on a benign codegen change.

Three traps in writing that probe, all of which produce a false pass:

- The body *does* contain `bne.n … <memcpy+0x8>` — its own loop branch. A grep for the bare string `memcpy` inside the body matches it and must not be read as a self-call. Match a branch to the function *entry* (`<memcpy>` with no `+offset`), or simply assert there is no `bl`/`blx` at all.
- `objdump` labels the address with the alias name `<memcpy>`, not `<np_memcpy>`. A probe written against the latter returns empty, and an empty result reads as clean.
- Zero unresolved symbols does not by itself prove the alias survived. Assert it directly: `nm` must report `memcpy` and `np_memcpy` at the *same address* (measured: both `0x20200d9c`; `memset`/`np_memset` both `0x20200db4`).

Also assert on the mem-family symbol set as a whole. ARM EABI GCC can emit `__aeabi_memcpy`, `__aeabi_memcpy4/8`, `__aeabi_memset`, `__aeabi_memclr/4/8`, and the `4`/`8` variants **assume 4- and 8-byte alignment by contract** — if any of those were satisfied from elsewhere, the link would close at zero unresolved while some call sites bypassed this code and voided the alignment-agnostic property. Measured: the ELF defines exactly four symbols in that family, `memcpy`, `memset`, `np_memcpy`, `np_memset`, and no `__aeabi_mem*` of any kind.

### 4.3 Defect C — bootloader OCRAM region over-subscribed by 8 KiB

```
Memory region  Used Size  Region Size  %age Used
      OCRAM:      520 KB       512 KB     101.56%
```

This is **not** code size. Summing the compiled objects gives text 17 688, data 21, bss 9 029 — **26.1 KB total**. It is arithmetic in `firmware/bootloader/linker/bootloader_imxrt1062.ld`:

| Element | Directive | Consumes |
|---------|-----------|----------|
| Region | `OCRAM (rwx) : LENGTH = 512K` | 512 KiB available |
| App staging | `.app_staging (NOLOAD) : ALIGN(65536) { . += 448K; }` | starts at the 64 KiB boundary, spans 448 KiB → ends at exactly 512 KiB |
| Stack | `.stack (NOLOAD) : ALIGN(8) { . += _stack_size; }` where `_stack_size = 8K` | a further 8 KiB → 520 KiB |

The script's own comment describes the intent as "bootloader (low) + app staging (mid) + stack (high)". The staging allocation, starting at the 64 KiB boundary and spanning 448 KiB, consumes the region to its final byte, leaving nothing for the stack.

**Provenance of the 448 KiB (investigated 2026-08-08): it is not a requirement, it is derived arithmetic, and the derivation is wrong.**

The figure entered in commit `b24c356` — the original bootloader commit — and has never been revised since apart from the rebrand. There is no design decision behind it. The script states its own derivation inline: `/* 448 KiB for application (512 - 64 = 448 KiB) */`, i.e. OCRAM total minus the app load offset. The 64 KiB offset is itself derived: the bootloader's 48 KiB ROM load limit (`_bl_max_size = 48K`, ASSERT-enforced) rounded up to the next `ALIGN(65536)` boundary, which also leaves room for `.usb_qh`.

The error is that `512 − 64` ignores the 8 KiB stack the same script reserves at the top of OCRAM. The correct figure is `512 − 64 − 8 = 440 KiB`. So there is no product requirement to weigh — the earlier framing of this as "is 448 KiB a hard requirement?" was wrong, and OI-SWCI-02 is restated accordingly.

**The same wrong constant is computed independently in C, and there it is a memory-corruption bug rather than a link error.** `firmware/bootloader/src/np_main.c` line 220:

```c
uint32_t remaining_ocram = NP_OCRAM_SIZE - (NP_APP_LOAD_ADDR - NP_FW_LOAD_ADDR);
if (hdr.image_size > remaining_ocram) return NP_ERR_IMAGE_TOO_LARGE;
```

That evaluates to `512K − 64K = 448K` again. `load_and_jump()` will therefore accept an application image up to 448 KiB and `np_emmc_read()` it to `NP_APP_LOAD_ADDR`. An image in the 440–448 KiB band writes into `0x2027E000–0x2027FFFF` — the 8 KiB stack region — **while the bootloader is still executing on that stack**. Nothing in C accounts for the reservation; `_stack_top` is imported into `np_main.c` solely to seed the vector table's initial SP.

The link failure is loud and blocks the build. This one is silent and fires only for images in a specific 8 KiB band. **Critically, the obvious fix for the link error — dropping the linker script to 440K — leaves the C guard untouched.** Both constants must change together.

Note also that the application executes *from OCRAM*: `load_and_jump()` copies the image in and jumps, and the XIP-from-eMMC alternative is explicitly unimplemented (`return NP_ERR_IMAGE_TOO_LARGE`). So 440 KiB is a genuine hard ceiling on application image size, not a staging buffer — worth knowing whether the real application is anywhere near it.

**Resolution (phase 2, 2026-08-09): the defect is the duplication, not the number, so neither number is written down any more.** Setting both sites to 440 would have been correct on the day and would have re-armed the identical trap — two independent computations of one limit is precisely what produced this. Instead:

- `bootloader_imxrt1062.ld` derives the reservation from the boundaries that actually bound it: `_app_staging_size = LENGTH(OCRAM) - _app_load_offset - _stack_size`. `448K` is gone and `440K` was never written; the 64 KiB offset now exists once, used both as the `.app_staging` alignment and in the derivation. Two `ASSERT`s make the tiling a link-time law rather than a comment: staging must *start* at `ORIGIN(OCRAM) + _app_load_offset` (so bootloader growth past 64 KiB reports its own cause instead of a bare byte count) and must *end* at `_stack_base`, which catches over-subscription — Defect C — and silent under-subscription alike.
- Both bounds are exported (`_app_staging_start`, `_app_staging_size`). `np_app_image.c` reads them; `np_main.c` computes neither. `NP_APP_LOAD_ADDR` went with `remaining_ocram`: it duplicated the load *address* in exactly the same shape, one line away from the size bug, so leaving it would have de-duplicated half the defect. The linker-symbol-import idiom is not new here — `np_main.c` already imported `_stack_top` this way to seed the vector table.
- The runtime guard is now a pure function, `np_app_image_size_check(image_size, staging_size)`, separated from the symbol read so the boundary is testable on a host that has no linker script. `np_bootloader_app_image_tests` covers it and — the assertion that would have caught this defect — checks it against the reservation **parsed out of the shipping linker script**, so the two cannot diverge again without failing CI.

The C-to-linker binding itself is verified on the artifact rather than in a host test, because it is not a property any host-computable value can express (§6.3).

### 4.4 Defect E — safety MCU has no platform layer (OPEN)

**Discovered 2026-08-09 during phase 4, and only discoverable then.** Defect A stopped the Class C build at compile time, so nothing had ever reached its link. With the CMSIS headers vendored, the link runs — and fails on **24 distinct undefined first-party symbols**:

```
np_gpio_mgr.c:(.text.np_gpio_mgr_init+0xa): undefined reference to `np_hal_gpio_write_pin'
np_fault_latch.c:(.text.np_fault_latch_commit+0x14): undefined reference to `np_hal_get_tick_ms'
np_session_sig.c:(.text.np_session_sig_init+0x8): undefined reference to `np_hal_otp_read_pubkey'
  → 24 distinct np_hal_* symbols, 34 references
```

The full set spans GPIO (`np_hal_gpio_init`, `np_hal_gpio_write_pin`), timebase (`np_hal_get_tick_ms`, `np_hal_systick_init`), clock (`np_hal_clock_init`), SPI slave (8 symbols), ADC (`np_hal_adc_init`, `np_hal_adc_read_channel`), TIM2 capture (`np_hal_tim2_init`, `np_hal_tim2_get_capture`), the R-peak edge pair, impedance (4 symbols), and OTP (`np_hal_otp_read_pubkey`).

**This is not something CMSIS vendoring can fix, and it is not the ST HAL.** The `np_` prefix is the giveaway: these are NeurOne-authored abstractions, declared `extern` at the top of each module. They are **defined only as test doubles inside the six Class C host-test files** — which is exactly why the host suite has always been green while the target image has never existed. Vendoring the full ST HAL, as phase 4's original wording ("Vendor STM32G0 CMSIS/HAL") anticipated, would not have defined a single one of them; the plan's phase-4 criterion was written without knowledge of this defect.

The gap is acknowledged in-tree, if obliquely: `firmware/safety_mcu/CMakeLists.txt` carries `-Wno-unused-parameter  # remove after HAL stubs filled in`. It also matches an established repository pattern — OI-ZA-01..04, OI-HRV-01..05, OI-PBM-01..08, OI-CVNS-01, OI-ANON-AES-01..05 are all "platform HAL stubs pending". SW-01 simply had no such open item until now.

**Not fixed here, and deliberately so.** These are the STM32G071 register drivers for stimulation GPIO, ADC, SPI and timers in **Class C** software. Writing them speculatively, without design review or bench validation, to make a CI leg go green would invert the purpose of the gate. Raised as **OI-SWCI-17**, and it is what phase 4's exit criterion had to be corrected around (§6).

### 4.5 Defect F — duplicate `-specs=` on the safety MCU link line (CLOSED, phase 4)

Also unreachable behind Defect A, and hit before Defect E:

```
arm-none-eabi-gcc: fatal error: .../nano.specs:
  attempt to rename spec 'link' to already defined spec 'nano_link'
```

`-specs=nano.specs -specs=nosys.specs` appeared **twice** on the link line: once from `CMAKE_EXE_LINKER_FLAGS_INIT` in `firmware/cmake/stm32g071.cmake`, and again from `target_link_options` in `firmware/safety_mcu/CMakeLists.txt`. ARM GNU 14.2.1 rejects the repeat outright.

**Resolution: deduplicated, not version-guarded.** The toolchain file is the single source; the repetition is removed from the target. This is the same defect shape as Defect C — one value, two places to write it — and the same resolution: delete the duplicate rather than make both copies agree.

Worth recording that this one is toolchain-version-sensitive: it was measured on ARM GNU **14.2.1** locally, while CI pins **13.2.Rel1**, where the duplicate is tolerated. A CI-only investigation would not have found it, and it would have surfaced later as an unexplained red build on a toolchain bump — the failure mode §5.3's pinning rationale exists to avoid. Fixing it costs nothing and is correct on both versions.

### 4.6 Defect D — main firmware calls a symbol that does not exist (CLOSED, phase 5)

**Given its own subsection at Rev G.** Defect D was discovered during phase 0 and recorded inline in §6.1 and in OI-SWCI-12, which is why §4's "six defects" count did not previously resolve to six headings. The content below is the same defect, relocated and completed with its resolution.

```
firmware/hub_control/modules/np_mod_pbm.c:112:9:
  error: implicit declaration of function 'np_pbm1064_dose_load_cal';
         did you mean 'np_pbm1064_dose_load_cal_stub'?
         [-Wimplicit-function-declaration]
```

`np_pbm1064_dose_load_cal` has no declaration and no definition anywhere in the tree. `np_hub_control` compiles with `-Wall -Wextra -Werror`, so the implicit declaration is a hard error. Measured on `143b023` with `-k 0` — keep-going, so every error surfaces rather than only the first — this was the **sole** error in the entire main-firmware super-project: 1 error, 0 warnings, 0 unresolved symbols, 0 missing headers.

**Resolution (2026-08-09, phase 5): call the real function once per zone, one wavelength row per call.** `np_pbm1064_dose_load_cal_stub()` is declared at `firmware/pbm_1064nm/include/np_pbm1064_dose.h:28` — in a header `np_mod_pbm.c` already includes, which is why the compiler could suggest it. `np_mod_pbm.c` now reads:

```c
if (!s_cal_loaded) {
    for (uint8_t z = 0U; z < NP_HUB_ZONE_SLOT_COUNT; z++) {
        np_pbm1064_dose_load_cal_stub(s_cal[z]);
    }
    s_cal_loaded = true;
}
```

This is not a new idiom. It is the pattern `np_pbm1064_session.c:271-273` already uses over active sockets, and `np_pbm1064_fai.c:366` calls the same function on a single row. This one site was the outlier.

**The rename the compiler suggests is the wrong fix, and is recorded as rejected rather than left as an obvious-looking alternative.** The shapes differ:

| | Type |
|---|---|
| `s_cal` (`np_mod_pbm.c`) | `np_pbm1064_cal_t[…][NP_PBM1064_WL_COUNT]` — 2-D |
| stub parameter | `np_pbm1064_cal_t cal_out[NP_PBM1064_WL_COUNT]` — one row |

Measured: `np_pbm1064_dose_load_cal_stub(s_cal)` is rejected as `error: passing argument 1 … from incompatible pointer type [-Wincompatible-pointer-types]` under `-Werror`, so on this toolchain the compiler does stop it — it takes an added cast to get through. **What is on the other side of that cast is the reason to write it down:** the call would populate row 0 only and leave every remaining zone holding zeroed calibration. That is a dose-metering path — `np_pbm1064_dose_tick()` consumes `s_cal[slot]` to accumulate J/cm² per wavelength — so a silent partial initialisation there yields a wrong delivered dose, which is materially worse than the loud compile error it replaced. Regression-tested as mutant M2 in §6.6.

**The array's bound moved in the same change, because it was a documented rule violation one line from the defect.** `s_cal` was declared `[NP_PBM1064_ZONE_COUNT][NP_PBM1064_WL_COUNT]`, but every index into it — at the load site and at the `np_pbm1064_dose_tick()` site — is a hub slot guarded by `slot >= NP_HUB_ZONE_SLOT_COUNT`. The two macros are both `5` today, so nothing failed. But `np_pbm1064_config.h:147-154` says in terms that `NP_PBM1064_ZONE_COUNT` bounds the **retired** ZONE_ID resistor-ladder detection state machine *only*, and closes: *"Do NOT use it to size any session-descriptor / dose / drive / calibration array."* `s_cal` is a calibration array.

Rather than assert the equality with a `_Static_assert`, the declaration is bound by `NP_HUB_ZONE_SLOT_COUNT` — the macro that already indexes it — so the two are no longer required to stay equal and there is nothing left to assert. Same resolution as Defects C and F: one value, one place, delete the duplicate rather than make the copies agree.

**This half of the fix is not verifiable from any build output, and that is stated rather than implied.** Because both macros are `5`, reverting the bound still compiles clean — mutant M4 in §6.6 survives, deliberately. The compensating control is that the constraint is written where a reader changing either macro will meet it: in the comment on the declaration, and in `np_pbm1064_config.h` itself.

### 4.7 Measured limit of the main-firmware check: there is no SW-02 image to link

**Not a defect, and not fixed here — a property of the repository that phase 5's verification surfaced and that governs how §6 and §8 are worded.**

Phase 4 was caught by the gap between *compiles* and *links*, so phase 5's verification checked for a linked artifact rather than inferring one from a clean compile. The check found something different from Defect E's shape:

```
$ grep 'Linking C executable' build.log
[77/86] Linking C executable bootloader/np_bootloader; Generating np_bootloader.bin …

$ grep -c 'Linking C static library' build.log
13
```

**The cross-build links exactly one executable, and it is the bootloader.** Every SW-02 module — `np_hub_control`, `np_pbm_1064nm`, `np_hrv_biofeedback`, `np_zone_announce`, `np_cervical_vns`, `np_sloreta_hdtdcs`, `np_anon`, `np_edf`, `np_factory_reset`, `np_uhdr_key`, `np_ota`, `np_crypto`, `np_freertos` — is archived to a `.a` and stops there. `firmware/hub_control/CMakeLists.txt:146` is `add_library(np_hub_control STATIC …)`, and the file's own header states the intended consumer:

```
#   add_subdirectory(firmware/hub_control)
#   target_link_libraries(np_application PRIVATE np_hub_control)
```

`np_application` does not exist in this repository. The same file says so again from the other direction at line 182: *"The MCUX SDK HAL is still provided by the parent application target."*

**The consequence for this plan is precise.** "Zero unresolved symbols" on the main-firmware leg is true, and it is **vacuous for SW-02** — no SW-02 link step runs, so nothing can be unresolved. SW-02 carries the same species of first-party HAL stub that Defect E records for SW-01: `np_mod_pbm_hal_pwm_set`, `np_mod_pbm_hal_ntc_read` and `np_mod_pbm_hal_pd_read` are declared `extern` at the top of `np_mod_pbm.c` (OI-PBM-HAL-01..03), alongside the OI-ZA-01..04, OI-HRV-01..05, OI-PBM-01..08, OI-CVNS-01 and OI-ANON-AES-01..05 families. **Defect E is visible only because SW-01 has an executable target; SW-02's equivalent gap is invisible for the opposite reason.** It is the same hazard behind a different symptom, and it does not become measurable until an application target exists.

Two things follow, and both are done rather than noted:

- §6's phase-5 row, §8's SW-02 traceability row, and the comment blocks on both promoted legs say **compiles**, never links. A green main-firmware check means every SW-02 translation unit compiled for the i.MX RT1062 under `-Wall -Wextra -Werror`. It does not mean an image exists.
- `np_bootloader` is **not** offered as the main-firmware leg's artifact. It is the bootloader's image, it is covered by the bootloader leg's own traceability row, and it has gated since phase 3. Counting it twice would make the main-firmware leg look like it verifies a link it does not.

Raised as **OI-SWCI-21**. Creating an `np_application` target is not CI work — it needs the platform layer and the MCUX SDK integration (OI-SWCI-20) that a real device bring-up entails, which is exactly the argument §4.4 makes for not writing Class C drivers to turn a leg green.

## 5. Specified workflows

**Two** scoped workflow files, not one, plus the unscoped `build-all.yml` backstop in §5.5 — three files in total. The Class C safety MCU gets its own workflow rather than a matrix leg — decision 2026-08-08, closing OI-SWCI-03.

| Workflow | Covers | Class |
|----------|--------|-------|
| `.github/workflows/safety-mcu-ci.yml` | STM32G071 safety MCU cross-compile | SW-01 **Class C** |
| `.github/workflows/firmware-cross-build.yml` | Bootloader + main firmware cross-compile (i.MX RT1062) | SW-02 Class B |

The rationale is auditability. The safety MCU is a separate CMake project with a separate toolchain file and a different IEC 62304 class, and it is the software that owns every stimulation enable GPIO. Burying its build status as one leg of a mixed-class matrix makes "is the Class C firmware building?" a question you answer by expanding a matrix and reading leg names. As its own workflow it is a single named check, which is what an auditor — or a reviewer under time pressure — actually reads.

Neither is a job added to `firmware-host-tests.yml`: both need a wider trigger surface, both need a toolchain install the host-test job does not, and keeping them separate means a cross-build failure does not obscure host-test results.

### 5.0 Governing principle — build only what the change could have affected

**Decision (2026-08-08, Steve Hickman): a workflow builds — and tests — only what the PR could have changed. Anything that was green and whose dependencies are untouched is neither rebuilt nor retested. A separate `build-all` / `test-all` exists for when everything should be rebuilt regardless (§5.5).**

The two halves are one decision. Per-PR scoping is only safe *because* an unscoped build-all backstops it, for reasons in §5.5 — without that backstop, an undeclared dependency edge means silent staleness with nothing to ever catch it.

This governs every `paths:` list in this document and any workflow added later. Two consequences applied here:

- `firmware-cross-build.yml` triggers on the modules it actually compiles, **not** on `firmware/**`. A change confined to `firmware/safety_mcu/**` must not rebuild the bootloader — the safety MCU is a separate CMake project and is not a dependency of it.
- `safety-mcu-ci.yml` triggers on `firmware/safety_mcu/**`, its toolchain file, and `firmware/crypto/**` — the last because `np_crypto` is consumed by SW-01 and a change there genuinely can break this build. Dependencies count; proximity in the tree does not.

The cost is enumerated path lists that can drift from the build graph, which is the same failure mode `firmware-host-tests.yml` warns about in its header. The mitigation is the same too, and a guard that fails CI when a module in the build graph is missing from the corresponding `paths:` list would make the drift self-detecting rather than audit-dependent. Not built here; see OI-SWCI-08.

> **The mechanism moved at phase 6; the principle did not (§6.7).** The `paths:` lists quoted in §5.1 and §5.2 below are the phase-0 form and are retained as the specification record. As implemented since 2026-08-10 the same lists live in a `changes` job and are applied with job-level `if:` conditions, because a workflow-level `paths:` filter makes a check un-requirable — it suppresses the check entirely rather than reporting it as skipped. Nothing about *what* gets built changed. Read §5.1/§5.2's `on:` blocks as the relevance lists they always were, not as the current trigger syntax.

**Known accepted exception:** `web-ci.yml` triggers on `docs/**`, `firmware/**` and `app/android/**` because the section-ref guard it runs scans the whole repository, so it has to be triggerable from anywhere a citation can live. That workflow's own header records the trade explicitly — "a guard that cannot see the edit that breaks it is not a guard." That exception is deliberate and stands; it is noted here so a later reader does not read it as a violation of this principle.

### 5.0.1 Ordering — cross-build runs only after the native build is green

**Decision (2026-08-08, Steve Hickman): do not cross-build if the native build fails.** A cross-compile of code that does not compile natively spends runner minutes to produce a second copy of a failure already reported, in a noisier form.

Expressing this needs `needs:` between jobs, which only works **within a single workflow**. The alternative for crossing workflow boundaries is the `workflow_run` trigger, and it is a poor fit here: it only fires for workflows on the default branch, it runs in base-repo context rather than PR context, and its results do not attach cleanly to a pull request's check list. Gating a PR check on it would be fragile in exactly the situation the gate exists for.

So the ordering decision forces each workflow to be **self-contained** — native tests and cross-build for the same scope, in one file:

| Workflow | Jobs |
|----------|------|
| `safety-mcu-ci.yml` | `host-tests` (the 6 safety-MCU targets) → `cross-build` (`needs: host-tests`) |
| `firmware-cross-build.yml` | `host-tests` (Class B modules) → `bootloader`, `main-firmware` (`needs: host-tests`) |

**This resolves OI-SWCI-07 as a consequence, not a separate choice.** The six safety-MCU host targets (`np_safety_spi_proto_tests`, `np_charge_monitor_tests`, `np_thermal_interlock_tests`, `np_impedance_report_tests`, `np_fault_latch_privacy_tests`, `np_cardiac_interlock_tests`) move into `safety-mcu-ci.yml`, which is also the better answer for Class C audit legibility — one workflow, one class, one named check covering both native and target verification.

Two consequences to be honest about:

1. **`firmware-host-tests.yml` gets absorbed and retired.** That is a restructure of something currently green and working, and it is a larger change than adding a new workflow. Its job name, `25 targets` count and header inventory all have to move with it or the file starts lying about itself. Sequencing this safely is OI-SWCI-09.

2. **The bootloader has no host tests to gate on.** It is cross-compile-only — it is not in the `NP_BUILD_TESTS` branch at all, and `np_signature.c` is deliberately self-contained rather than depending on `np_crypto`. Gating the bootloader cross-build on unrelated Class B host tests would be a *false* dependency: an unrelated `np_edf_tests` failure would block bootloader verification for no reason. The bootloader leg should therefore run unconditionally, and the gating rule applies only where a genuine native counterpart exists. The principle is "do not cross-build what failed natively", not "serialise everything behind something".

### 5.1 `firmware-cross-build.yml` (Class B — bootloader + main firmware)

```yaml
name: Firmware Cross-Compile (ARM)

# Trigger only on what this build actually compiles. firmware/safety_mcu/** is
# deliberately ABSENT: it is a separate CMake project with its own toolchain and
# its own workflow, and is not a dependency of the bootloader or main firmware.
#
# The two lists below MUST stay identical. GitHub Actions does not support YAML
# anchors/aliases, so the duplication cannot be collapsed — same constraint, and
# same reason, as firmware-host-tests.yml. When adding a module, edit BOTH.
on:
  push:
    paths:
      - 'firmware/bootloader/**'
      - 'firmware/hub_control/**'
      - 'firmware/vendor/**'
      - 'firmware/crypto/**'
      - 'firmware/ota/**'
      - 'firmware/zone_announce/**'
      - 'firmware/hrv_biofeedback/**'
      - 'firmware/pbm_1064nm/**'
      - 'firmware/cervical_vns/**'
      - 'firmware/sloreta_hdtdcs/**'
      - 'firmware/anon/**'
      - 'firmware/edf/**'
      - 'firmware/factory_reset/**'
      - 'firmware/uhdr_key/**'
      - 'firmware/common/**'
      - 'firmware/CMakeLists.txt'
      - 'firmware/cmake/arm-none-eabi.cmake'
      - '.github/workflows/firmware-cross-build.yml'
  pull_request:
    paths:
      - 'firmware/bootloader/**'
      - 'firmware/hub_control/**'
      - 'firmware/vendor/**'
      - 'firmware/crypto/**'
      - 'firmware/ota/**'
      - 'firmware/zone_announce/**'
      - 'firmware/hrv_biofeedback/**'
      - 'firmware/pbm_1064nm/**'
      - 'firmware/cervical_vns/**'
      - 'firmware/sloreta_hdtdcs/**'
      - 'firmware/anon/**'
      - 'firmware/edf/**'
      - 'firmware/factory_reset/**'
      - 'firmware/uhdr_key/**'
      - 'firmware/common/**'
      - 'firmware/CMakeLists.txt'
      - 'firmware/cmake/arm-none-eabi.cmake'
      - '.github/workflows/firmware-cross-build.yml'
  workflow_dispatch:

# Least privilege. Required for actions/checkout, which hard-fails without it
# once the repository is private. Matches every other workflow in the repo.
permissions:
  contents: read

jobs:
  cross-build:
    name: ${{ matrix.name }}
    runs-on: ubuntu-latest
    continue-on-error: ${{ matrix.experimental }}
    strategy:
      fail-fast: false
      matrix:
        include:
          - name: Bootloader (i.MX RT1062)
            src: firmware/bootloader
            toolchain: firmware/cmake/arm-none-eabi.cmake
            experimental: true
          - name: Main firmware (i.MX RT1062, Class B)
            src: firmware
            toolchain: firmware/cmake/arm-none-eabi.cmake
            experimental: true
    steps:
      - uses: actions/checkout@v4

      - name: Install ARM GNU toolchain
        uses: carlosperate/arm-none-eabi-gcc-action@v1
        with:
          release: '13.2.Rel1'

      - name: Record toolchain version
        run: arm-none-eabi-gcc --version

      - name: Configure
        run: |
          cmake -B build/cross -G Ninja \
                -DCMAKE_TOOLCHAIN_FILE=${{ github.workspace }}/${{ matrix.toolchain }} \
                -DCMAKE_BUILD_TYPE=Release \
                ${{ matrix.src }}

      - name: Build
        run: cmake --build build/cross

      - name: Report image size
        if: success()
        run: find build/cross -name '*.elf' | xargs -r arm-none-eabi-size

      - name: Upload map and logs on failure
        if: failure()
        uses: actions/upload-artifact@v4
        with:
          name: cross-build-${{ strategy.job-index }}
          path: |
            build/cross/**/*.map
            build/cross/**/CMakeFiles/*.log
          retention-days: 7
          if-no-files-found: ignore
```

### 5.2 `safety-mcu-ci.yml` (Class C — safety MCU)

Deliberately a single job, not a matrix. One named check, one answer.

```yaml
name: Safety MCU CI (STM32G071, Class C)

# SW-01 Class C — the firmware that owns every stimulation enable GPIO.
# Its own workflow rather than a matrix leg so its build status is a single
# named check rather than something you read out of a mixed-class matrix
# (NP-SW-CI-001 §5, OI-SWCI-03).

on:
  push:
    paths:
      - 'firmware/safety_mcu/**'
      - 'firmware/cmake/stm32g071.cmake'
      - 'firmware/crypto/**'          # np_crypto is consumed by SW-01 (Class C)
      - '.github/workflows/safety-mcu-ci.yml'
  pull_request:
    paths:
      - 'firmware/safety_mcu/**'
      - 'firmware/cmake/stm32g071.cmake'
      - 'firmware/crypto/**'
      - '.github/workflows/safety-mcu-ci.yml'
  workflow_dispatch:

# Least privilege. Required for actions/checkout, which hard-fails without it
# once the repository is private. Matches every other workflow in the repo.
permissions:
  contents: read

jobs:
  safety-mcu-cross-build:
    # Phase 4 (§6.5) renamed this to "…, Class C" and promoted it out of
    # continue-on-error.  "Cross-compile" is now literal: the step below builds
    # np_safety_mcu_objs, not the image, because Defect E (§4.4) means no image
    # can link.  Phase 7 re-points it at the full build.
    name: Cross-compile (STM32G071, Cortex-M0+, Class C)
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install ARM GNU toolchain
        uses: carlosperate/arm-none-eabi-gcc-action@v1
        with:
          release: '13.2.Rel1'

      - name: Record toolchain version
        run: arm-none-eabi-gcc --version

      - name: Configure
        run: |
          cmake -B build/safety-mcu -G Ninja \
                -DCMAKE_TOOLCHAIN_FILE=${{ github.workspace }}/firmware/cmake/stm32g071.cmake \
                -DCMAKE_BUILD_TYPE=Release \
                firmware/safety_mcu

      # Phase 4: --target np_safety_mcu_objs.  The image-size step that stood
      # here is removed with it — there is no image, and `find … *.elf` matching
      # nothing exits 0, so it would have been a step that always passed while
      # proving nothing.  Both return at phase 7.
      - name: Compile (all Class C translation units)
        run: cmake --build build/safety-mcu --target np_safety_mcu_objs

      - name: Upload map and logs on failure
        if: failure()
        uses: actions/upload-artifact@v4
        with:
          name: safety-mcu-build
          path: |
            build/safety-mcu/**/*.map
            build/safety-mcu/**/CMakeFiles/*.log
          retention-days: 7
          if-no-files-found: ignore
```

Note the `firmware/crypto/**` trigger. `np_crypto` is consumed by the safety MCU and is recorded as Class C as well as Class B, so a change there can break this build. `firmware-host-tests.yml` already lists `firmware/crypto/**` for the same reason.

### 5.3 Toolchain version pinning

`firmware/cmake/arm-none-eabi.cmake` already enforces a minimum of 12.3 and queries `-dumpfullversion` rather than `-dumpversion` — correct, since plain `-dumpversion` returns only the major number on GCC 7 and later, which would let "12" falsely satisfy a "12.3" gate. The action is pinned to a specific release rather than `latest` so that a toolchain change is a reviewable commit and not an unexplained red build. `13.2.Rel1` is the conservative pin; 14.2.1 was used for the measurements in §4 and also satisfies the gate.

### 5.4 Size gating

No separate size-checking step. The bootloader already links with `-Wl,--print-memory-usage`, and its linker script carries `ASSERT((_bootloader_end - ORIGIN(OCRAM)) <= _bl_max_size, ...)`. A region overflow fails the link and therefore the job — as Defect C demonstrates. **The link failing is the size test.** A second size-parsing step could disagree with the linker, and the linker is authoritative.

### 5.5 `build-all.yml` — the unscoped backstop

Per-PR path scoping has two blind spots that no amount of careful `paths:` maintenance closes:

1. **Undeclared dependency edges.** If module A's build depends on module B and nobody put `firmware/B/**` in A's paths list, a change to B never triggers A. A stays green forever while being broken. The enumerated lists in §5.1 and §5.2 are hand-maintained and *will* drift — that is not pessimism, it is the failure mode `firmware-host-tests.yml` already documents in its own header.
2. **Rot from outside the repository.** GitHub runner images update, actions publish new majors, the pinned toolchain gets yanked, an upstream SOUP dependency changes. None of that is a code change, so no `paths:` filter can ever fire on it. A repository with only scoped triggers can sit broken for months and look green, because nothing ran.

`build-all.yml` closes both. It has **no `paths:` filter at all** — that is the entire point — and builds and tests everything from a clean checkout.

```yaml
name: Build All / Test All

# The unscoped backstop for the per-PR path scoping in NP-SW-CI-001 §5.0.
# No paths filter, deliberately: this exists to catch what scoped triggers
# structurally cannot — undeclared dependency edges between modules, and rot
# originating outside the repo (runner image updates, action majors, toolchain
# changes) that no code change would ever trigger.

on:
  schedule:
    - cron: '0 6 * * 1'        # Mondays 06:00 UTC — one clean full build per week
  workflow_dispatch:            # on demand, e.g. before a release or a tooling cut

permissions:
  contents: read

jobs:
  # host tests — all targets, no ctest filtering
  # firmware cross-build — bootloader + main firmware
  # safety MCU cross-build
  # (each mirrors its scoped counterpart, minus the paths: filter)
```

**Failure policy differs from the scoped workflows.** A red `build-all` does not block any PR — it is not a PR check. It means something drifted, and it should be loud out-of-band rather than silently reported: it wants a notification, not a status badge nobody reads. How that notification is delivered is OI-SWCI-10.

**Cadence.** Weekly is the starting proposal, not a measured one. Nightly costs more runner minutes for a repo where firmware changes are not daily; weekly risks up to seven days of undetected drift. Worth revisiting once there is a base rate for how often it actually catches something.

## 6. Phased rollout

A permanently red required check trains reviewers to ignore it, which is worse than no check. So both workflows land reporting-only and are promoted independently as each defect closes. The two workflows are on separate tracks — the Class C safety MCU is not gated behind Class B bootloader work.

| Phase | Workflow | Action | Exit criterion |
|-------|----------|--------|----------------|
| ~~0~~ | both | ~~Land `firmware-cross-build.yml` and `safety-mcu-ci.yml`, cross-build legs `continue-on-error: true`~~ **COMPLETE 2026-08-08** | ~~Jobs run, log the known failures, block nothing~~ **Met — see §6.1** |
| ~~1~~ | cross-build | ~~Fix Defect B (bootloader C runtime)~~ **COMPLETE 2026-08-09** | ~~Bootloader link resolves `memcpy`/`memset`~~ **Met — 26 unresolved → 0; see §6.2** |
| ~~2~~ | cross-build | ~~Fix Defect C (OCRAM arithmetic) — see §4.3; the linker script **and** the duplicate constant in `np_main.c`~~ **COMPLETE 2026-08-09** | ~~Bootloader links; no region overflow~~ **Met — 101.56% → 100.00%, `rc=0`; see §6.3** |
| ~~3~~ | cross-build | ~~Drop `continue-on-error` on the bootloader leg~~ **COMPLETE 2026-08-09** | ~~Bootloader leg blocking~~ **Corrected — see below. Met: a failing bootloader leg makes the workflow run conclude `failure`; demonstrated by breakage/revert, §6.4** |
| ~~4~~ | safety-mcu | ~~Vendor STM32G0 CMSIS/HAL (Defect A) per §9~~ **COMPLETE 2026-08-09 — CMSIS device headers only, not the HAL (OI-SWCI-06 closed)** | ~~Safety MCU compiles; drop its `continue-on-error`~~ **Corrected — see below. Met: all 10 Class C TUs compile for STM32G071 (4 errors → 0), and the compile leg is promoted to gating; see §6.5** |
| ~~5~~ | cross-build | ~~Populate `NP_PLATFORM_INCLUDE_DIRS` with the MCUX SDK path — currently an empty stub in `firmware/CMakeLists.txt`~~ **COMPLETE 2026-08-09 — the MCUX premise was wrong; the work was Defect D (§4.6). MCUX decoupled to OI-SWCI-20** | ~~Main firmware leg builds; drop its `continue-on-error`~~ **Met as written — 1 error → 0, `rc=1` → `rc=0`, and the leg is promoted to gating; see §6.6** |
| 6 | both | **PREPARED, NOT APPLIED (2026-08-10).** Make the checks *requirable* — convert workflow-level `paths:` filters to job-level `if:` so every check reports on every PR, and remove the moving counts from two check contexts. Adding the `required_status_checks` rule is an admin action gated on OI-SWCI-04 and is **Steve's to run**; the exact payload is in §6.7 | Cross-compile regressions become unmergeable — **NOT MET.** The mechanical blocker is closed and measured (§6.7); the policy decision (OI-SWCI-04) and the `PUT` remain outstanding. Phase 6 is not complete |
| 7 | safety-mcu | Implement the SW-01 platform layer (Defect E, §4.4, OI-SWCI-17) and re-point the CI leg from `np_safety_mcu_objs` to the full build | Safety MCU **links**; the image exists |
| 8 | cross-build | Create the SW-02 application target (§4.7, OI-SWCI-21), which needs the SW-02 platform layer and the MCUX SDK integration (OI-SWCI-20), and point the main-firmware leg at it | Main firmware **links**; an SW-02 image exists |

**The phase-5 exit criterion was NOT corrected, and that is worth stating explicitly because every phase before it needed one.** "Main firmware leg builds; drop its `continue-on-error`" is met verbatim. What was wrong in phase 5's row was its *premise*, not its criterion: "Populate `NP_PLATFORM_INCLUDE_DIRS` with the MCUX SDK path" asserted that the SDK was what stood between this leg and green. Measured with `-k 0` on `143b023`, the whole super-project produced **one** error — Defect D — and **zero** missing-header errors, because the platform layer is stubbed rather than called into. No MCUX header is needed to build what is in this repository today.

The SDK is still needed for real device bring-up and the `# ← append MCUX SDK path` marker at `firmware/CMakeLists.txt:132` stands, but it is not build-gating work and leaving it in the phase-5 row implied phase 5 was blocked on it. It is decoupled and re-raised as **OI-SWCI-20**.

Read this alongside §4.7, which is where the caution that phases 2–4 spent on their criteria has gone instead. The criterion is achievable exactly as written, and the reason it is achievable is that "builds" for SW-02 means *compiles* — there is no SW-02 executable target to link. That is the same distinction phase 4 was caught by, arriving one level further out, and it is handled by wording rather than by correction.

**The phase-4 exit criterion was corrected when phase 4 landed**, in the same way and for the same reason as phases 2 and 3. Rev A–E stated it as "Safety MCU compiles; drop its `continue-on-error`", written on the assumption that Defect A was the only thing between the Class C firmware and a working build. It was not. Vendoring the headers made the firmware **compile**; it did not make it **link**, because Defect E (§4.4) leaves 24 first-party `np_hal_*` platform symbols undefined and was invisible until the compile stage was cleared. Note the criterion's own wording is "compiles", which is now literally true — but `cmake --build` on the executable target includes the link, so a leg pointed at the executable stays red no matter how correct the vendoring is.

Promoting a permanently-red leg is precisely what §6 opens by refusing to do. So the criterion is now split along the line the defects actually fall on: **compilation of every Class C translation unit for STM32G071 is gated from phase 4** — that is exactly the property Defect A broke, it is real regression surface, and it is verified by its own `np_safety_mcu_objs` target — while **linking the image** becomes phase 7, gated on the platform layer existing. The alternative was to hold a working gate hostage to an unrelated open defect for as long as Class C driver development takes.

This is deliberately *not* dressed up as full coverage. A green safety-MCU check after phase 4 means "every Class C source compiles for the target"; it does **not** mean an image exists. §5.2's job name and §8's traceability row both say so, so the check cannot be read as more than it is.

**The phase-3 exit criterion was corrected when phase 3 landed.** Rev A–D stated it as "Bootloader leg blocking", which overclaims in a way worth being precise about, because the two properties have different owners. Dropping `continue-on-error` changes what the *workflow* reports: a failing job stops being excluded from the run conclusion, so the run concludes `failure` instead of `success`. Whether a failing run can actually **prevent a merge** is branch protection — a required-check policy, an admin setting outside the repository, and still an open decision (OI-SWCI-04, phase 6). Nothing in phase 3 touches merge enforcement. Measured on the day phase 3 landed: `main` carries no branch-protection object (`GET /branches/main/protection` → 404), and the sole ruleset (`Safety`, active) contains exactly two rules, `deletion` and `non_fast_forward`. Neither is a `required_status_checks` rule, so no check of any kind can currently prevent a merge. (§6.1 recorded that ruleset as having an empty rules array in 2026-08-08; the two protective rules have been added since, and neither changes the conclusion.) The criterion now reads "a failing bootloader leg makes the workflow run conclude `failure`" — which is what was demonstrated.

Worth reading alongside that: `continue-on-error` suppresses **less** than the name suggests. It does not hide the job and it does not hide the check. A masked job still reports `conclusion: failure` at job level, and its check-run still shows a red ✗ in the PR check list — `Main firmware (i.MX RT1062, Class B)` does exactly this on every run today. The *only* thing `continue-on-error` suppresses is the job's contribution to the run conclusion. So "the job goes red" was never the thing phase 3 changed, and a phase-3 verification that looked only at job or check-run state would have passed identically before the change. The run conclusion is the single discriminating signal, which is why §6.4 is built around it.

**The phase-2 exit criterion was corrected when phase 2 landed.** Rev B and Rev C stated it as "`--print-memory-usage` under 100%", which is not achievable and never was: the bootloader, the staging area and the stack are specified to *tile* OCRAM, so a correct layout reports exactly 100.00%. `ld` errors on **overflow**, not on full allocation, so 100.00% links cleanly and zero headroom is the design rather than a near miss. Anyone working to the original wording would have concluded a correct fix had failed, and the obvious way to "reach" under 100% is to shrink the staging reservation below what the memory map actually allows. The criterion now reads "links successfully; no region overflow".

Phase 4 is independent of phases 1–3 and 5 and can be worked in parallel — giving the safety MCU its own workflow is what makes that separation clean.

~~Phases 4 and 5 both mean bringing a vendor SDK into the build, which is an SOUP decision with design-control consequences.~~ **Half true, and corrected at phase 5.** Phase 4 did mean bringing a vendor SDK in, and the SOUP decision in §9 is what unblocked it. **Phase 5 did not.** It was a one-line first-party firmware defect (§4.6), and no vendored header was needed to close it. The MCUX SDK remains a live SOUP obligation under §9.3 whenever it is integrated, but it is no longer on any phase's critical path — OI-SWCI-20.

### 6.1 Phase 0 as implemented (2026-08-08)

> **Historical record — the counts and the blocking column below are as of 2026-08-08.** Phase 1 added `np_bootloader_mem_tests` and phase 2 added `np_bootloader_app_image_tests` to the Class B side, so the live figures are 6 + 21 = 27 (see §6.2, §6.3). The "Blocking?" column is also superseded: phase 3 promoted the bootloader leg in both `firmware-cross-build.yml` and `build-all.yml`, so "cross legs no" is no longer true of the bootloader (see §6.4). The reasoning in this section is unaffected; only the arithmetic and the one column moved.

Three workflows landed, not two: §5's table covers the two scoped ones, and §5.5's `build-all.yml` is the third.

| File | Jobs | Blocking? |
|------|------|-----------|
| `safety-mcu-ci.yml` | `host-tests` (6 Class C targets) → `cross-build` (`needs: host-tests`) | host-tests yes; cross-build no |
| `firmware-cross-build.yml` | `host-tests` (19 Class B targets) → `main-firmware` (`needs: host-tests`); `bootloader` unconditional | host-tests yes; cross legs no |
| `build-all.yml` | `host-tests-all` (25, unfiltered, + partition guard), `safety-mcu-cross`, `bootloader-cross`, `main-firmware-cross` | not a PR check |

**`firmware-host-tests.yml` is retired** (OI-SWCI-09). Its 25 ctest targets were partitioned 6 + 19 across the two scoped workflows — disjoint, union verified equal to the original 25-name list. Two things make the split self-detecting rather than audit-dependent: each scoped workflow asserts its own selected-target count before running anything, and `build-all.yml` re-checks the whole partition (total = 25, 6 + 19 = 25, no overlap, no orphan) every week. That is a partial down-payment on OI-SWCI-08 for the ctest axis; the `paths:`-vs-build-graph axis is still open.

**The host-test jobs are NOT `continue-on-error`.** Phase 0's reporting-only mandate covers the *new* cross-compile verification. `firmware-host-tests.yml` was a blocking check, and making its successor non-blocking during absorption would have been exactly the silent coverage regression OI-SWCI-09 warned about.

**Verified before retirement:** `main` carries no branch-protection object and the sole ruleset (`Safety`) has an empty rules array, so no required status check referenced `CMake host tests (25 targets)`. Retiring the workflow could not wedge merges.

**Defect D (new, undocumented in §4).** §4 row 4 records the main-firmware super-project as "fails at bootloader; remainder never attempted" — measured under a sequential generator. Under Ninja it fails *first* at `firmware/hub_control/modules/np_mod_pbm.c:112`, `implicit declaration of function 'np_pbm1064_dose_load_cal'` (did you mean `np_pbm1064_dose_load_cal_stub`?). With `-k 0` both that and the bootloader link failure appear. This is a fourth blocking defect, independent of A, B and C; the main-firmware leg therefore carries diagnostic value the bootloader leg does not. Not fixed here — it is a firmware change needing design review. Raised as OI-SWCI-12.

#### 6.1.1 What the partition guard does and does not prove

The absorption was reviewed against the question "what could silently lose coverage that a count assertion would not catch?" Four things were checked and are closed; four are named limitations rather than gaps that were missed.

**Closed by verification, not by assumption:**

- **Argument-vector drift.** The classic lossless-absorption failure is 6 + 19 green targets that verify materially less than the 25 did, because a successor quietly dropped `CMAKE_BUILD_TYPE=Debug` (→ `NDEBUG` → every `assert()` becomes a no-op), `--no-tests=error`, a sanitiser, or `-Werror`. Diffed against `git show origin/main:.github/workflows/firmware-host-tests.yml`: both successors configure with exactly `-DCMAKE_BUILD_TYPE=Debug -DNP_BUILD_TESTS=ON -DCMAKE_CROSSCOMPILING=OFF` and run `ctest --output-on-failure --no-tests=error`. The only delta is the `-R`/`-E` selection, which is the intended change.
- **Guards that cannot fire.** No `set +e`, no `|| true`, no `--no-tests=ignore`, no `cmake --build -k 0`, no `if: always()`, no `--repeat` in any of the three files. `continue-on-error` is job-level only and appears on exactly the cross-compile legs — never on a step, never on a `host-tests` job.
- **Orphaned targets are impossible by construction.** Class B's selection is the *complement* (`ctest -E`) of the Class C regex, not an independently maintained list of 19. A newly added test therefore lands in Class B automatically and trips its count assertion **on the PR that adds it**, rather than surfacing in the weekly run up to seven days later. "No orphan" is a per-PR structural property, not an audit.
- **`firmware/cmake/`** holds exactly `arm-none-eabi.cmake` and `stm32g071.cmake`, both named in the union of the two path lists, so the whole-directory glob the retired workflow used is fully reproduced *today*.

**Named limitations — recorded, not closed (OI-SWCI-14):**

1. **The counts are literals, not a manifest.** `build-all.yml` compares `ctest -N` against hardcoded 25/6/19. That catches arithmetic drift (add, delete, rename-out-of-regex) but not *substitution*: renaming a target, or deleting one and adding a trivial replacement in the same PR, leaves 25 = 6 + 19 intact and passes. A checked-in name manifest diffed against `ctest -N` would make substitution a visible line in the diff. Not built here.
2. **Intra-target erosion is invisible by construction.** A ctest entry is one process. Dropping a source file from a test executable, `#ifdef`-ing out a block, losing runner lines, or an early `return` under a CI env check all leave the name registered and green. **`6 + 19 = 25` is a partition guarantee, not a coverage guarantee**, and should not be read as one. A per-target case count or a gcov floor in the weekly job is the natural place to address this.
3. **A third file added under `firmware/cmake/`** would trigger neither workflow, because §5.1/§5.2 specify single named files rather than the directory glob. Complete today; not future-proof.
4. **The Class C regex is duplicated across all three workflows** — GitHub Actions has no mechanism to share an `env:` across files short of a composite action or reusable workflow. Divergence is partially self-catching: widening or narrowing the regex in a scoped workflow trips that workflow's own count assertion.

A terminal `gate` job asserting `needs.host-tests.result == 'success'` was considered and not added: it is redundant while the `host-tests` jobs are themselves blocking. It becomes relevant at Phase 6, when required-check policy is set.

**Also still stale:** references to `firmware-host-tests.yml` remain in `docs/np_sw_001.md`, `docs/np_dhf_001.md`, `docs/status/pending-decisions.md`, `docs/status/document-register.md` and `docs/status/completed-decisions.md`. The in-repo section-ref guard does not validate workflow filenames, so nothing fails on them. Sweeping them is OI-SWCI-13.

### 6.2 Phase 1 as implemented (2026-08-09)

Defect B is closed. Measured before and after on the same host (macOS, ARM GNU 14.2.1), exit codes captured directly rather than through a pipe:

| | before | after |
|---|---|---|
| configure | `rc=0` | `rc=0` |
| build | `rc=2` | `rc=2` |
| unresolved `memcpy` | 19 | **0** |
| unresolved `memset` | 7 | **0** |
| unresolved, any symbol | 26 | **0** |
| remaining errors | region overflow + 26 unresolved | **region overflow only** |

**The bootloader leg is still red, and that is the expected phase-1 outcome, not a failure.** The sole remaining error is Defect C — `OCRAM: 520 KB / 512 KB / 101.56%`, `region 'OCRAM' overflowed by 8192 bytes`. That was phase 2, **closed 2026-08-09 — see §6.3**; it needed `bootloader_imxrt1062.ld` and the duplicate constant at `np_main.c:220` addressed together (OI-SWCI-02). `continue-on-error` stays on the bootloader leg even now that it is green; dropping it is phase 3 — **done 2026-08-09, see §6.4**.

**The bootloader gained its first host test.** `np_bootloader_mem_tests` covers `src/np_mem.c` only — zero length; 1/2/3/4/8-byte lengths; unaligned source, unaligned destination and both; lengths crossing a word boundary; an exhaustive 8×8×18 offset/length matrix; non-zero and high-bit `memset` fills; `int`→`unsigned char` truncation; 4 KiB and 8 KiB transfers at every start alignment — against the host libc as oracle, in canary-framed buffers so a one-byte overrun fails rather than passes. The rest of the bootloader stays cross-compile-only: it is device firmware with no host-executable surface, and a host job that cannot run it would produce a green check proving nothing.

**The suite was falsified, and one survivor is recorded rather than buried.** Built against six deliberately broken implementations: five were killed (off-by-one short copy, one-byte overrun, masked fill value, a `do/while` that writes when `n == 0`, wrong return pointer), at 359–1371 assertions. The sixth — a word-at-a-time copy casting both operands to `uint32_t *` without checking alignment — **survived**, because its output is byte-identical on any host tolerating unaligned access. No output-comparison test can catch it; the defect is undefined behaviour and a strict-alignment fault, not a wrong byte. The alignment obligation is therefore verified at the artifact level instead, by the wide-access-absence disassembly check in §4.2.

**That compensating control was itself falsified rather than asserted.** The surviving mutant was compiled into the *cross* build at the bootloader's own flags and run through the disassembly check: it emits `ldr.w`/`str.w` in `memcpy` and the check fails, where the real implementation emits none and passes. Without that step the mutant would have lived in one build while its claimed killer lived in another — a seam, not a control. Across the two probes the score is 6 of 6.

A UBSan host build would also catch it and is worth having, but it is a wider CI change than this phase and is not a substitute for inspecting what ships.

**Residual, stated rather than left implicit:** nothing executes the cross-compiled machine code. The host tests prove the algorithm, the disassembly proves the codegen shape, and neither proves the `arm-none-eabi` output behaves correctly when run — that needs on-target or emulated execution, which §1 puts out of scope for this plan. For a byte loop with no alignment path and no branches beyond the loop, the residual is small and is accepted here with the disassembly evidence as the compensating control.

**Count guards moved in the same commit.** Adding a target made the Class B selection 20 and the repo total 26. `NP_CLASS_B_TEST_COUNT` is `20` in `firmware-cross-build.yml`; `build-all.yml`'s partition arithmetic is `6 + 20 = 26`, changed because its own error text says to update all three workflows together. `NP_SAFETY_TEST_COUNT` is untouched at 6 — the bootloader is Class B. Verified locally under Ninja: the drift guard reports `Total Tests: 20` and all 20 Class B targets pass. The new target lands in the Class B selection automatically, because that selection is the *complement* of the Class C regex rather than a maintained list (§6.1.1).

This is a partial down-payment on the "two modules have no CI at all" gap in §2: `firmware/bootloader` now has both a cross-compile leg and a host test. `firmware/hrv_biofeedback` still has neither a host test nor a reason recorded for wanting one — that remains OI-SWCI-05.

### 6.3 Phase 2 as implemented (2026-08-09)

Defect C is closed and OI-SWCI-02 with it. Measured on the same host as phase 1 (macOS, ARM GNU 14.2.1), exit codes captured directly rather than through a pipe — `cmd | tail` reports `tail`'s status and had already masked a failure twice in this work:

| | before | after |
|---|---|---|
| configure | `rc=0` | `rc=0` |
| build | `rc=2` | **`rc=0`** |
| OCRAM used | 520 KB / 512 KB — **101.56%** | 512 KB / 512 KB — **100.00%** |
| `region 'OCRAM' overflowed` | by 8192 bytes | **absent** |
| errors of any kind in the build log | 1 | **0** |
| C image-size ceiling | 448 KiB, computed in `np_main.c` | 440 KiB, read from the linker |

**100.00% is the pass condition, not a near miss.** `ld` errors on overflow, so a fully allocated region links cleanly. The three regions are specified to tile OCRAM and now provably do:

```
bootloader ≤48K │ slack + .usb_qh │ .app_staging 64K–504K (440K) │ .stack 504K–512K (8K)
```

The 48 KiB bootloader `ASSERT` is unaffected and still passes — measured `_bootloader_end = 0x20207078`, i.e. 28 792 bytes of the 49 152 allowed. `_bl_max_size`, `_stack_size` and the OCRAM `LENGTH` are untouched.

**The C-to-linker binding is verified on the artifact, because it is not in any output a host can compute.** A host build has no linker script. Feeding `np_app_max_image_size()` a host stand-in would let the suite pass while the device binding was broken — the exact failure the suite exists to catch — so `np_app_image.c`'s symbol readers are `#ifdef`-excluded from the host build (`NP_APP_IMAGE_HOST_TEST`) rather than substituted. This is the same shape of compensating control as the §4.2 disassembly check, and for the same reason: some properties are properties of the linked image, not of any value. Measured on `np_bootloader`:

```
$ arm-none-eabi-nm np_bootloader | grep -E '_app_staging|_stack_base'
0006e000 A _app_staging_size      ← 440 KiB, derived by ld
20210000 B _app_staging_start     ← ORIGIN(OCRAM) + 64 KiB
2027e000 B _app_staging_end       ← identical to _stack_base
2027e000 A _stack_base

$ arm-none-eabi-objdump -d np_bootloader | sed -n '/<np_app_max_image_size>:/,/^$/p'
20203d30 <np_app_max_image_size>:
20203d30:  4800   ldr r0, [pc, #0]
20203d32:  4770   bx  lr
20203d34:  0006e000  .word 0x0006e000     ← the linker's value, not C's
```

**The suite was falsified before it was believed.** Seven mutants, all killed:

| Mutant | Killed by | Assertions |
|---|---|---|
| M1 linker reverted to the literal `448K` — the original defect | derivation + value + tiling | 4 |
| M2 linker set to the literal `440K` — right number, still duplicated | "must reference LENGTH/offset/stack, no numeric literal" | 4 |
| M3 `.app_staging` body hardcodes its size, symbol defined but unused | section-body check | 1 |
| M4 C ceiling = `staging_size + 8192` — **the original `np_main.c` bug** | 440–448 KiB band scan | 20 |
| M5 C off-by-one (`>=`), rejects an image that exactly fits | boundary | 2 |
| M6 C zero-length check removed | empty-image | 2 |
| M7 C size limit removed entirely | boundary + band | 21 |

M2 is the one worth noting: it is the fix this phase was told not to make, and the suite rejects it. M4 is the defect itself, and the assertion that kills it is a scan of every 512-byte-aligned image size in the old 440–448 KiB acceptance band — each one a size that would have been copied over the live stack.

**The linker script is parsed, not grepped.** The §4.3 rewrite above and the new comment block in the script both contain the strings "448" and "440", and the derivation comment contains the literal text `_stack_size) = 440K`. A regex for `_stack_size *= *([0-9]+)K` finds that comment. The test therefore strips block comments first and reads the assignments out of the stripped text — a probe that matches the text the change itself just wrote is not a probe.

**Residual, stated rather than left implicit.** Nothing executes the cross-compiled image, so the guard is verified as an algorithm (host tests), as a value in the linked artifact (`nm`/`objdump`), and as a layout invariant (link-time `ASSERT`) — but not as behaviour on target. §1 puts on-target execution out of scope. Separately, OI-SWCI-15 remains open and touches this same function: the Ed25519 check covers the image in its eMMC bank, not the copy in staging that actually executes.

**Count guards moved in the same commit.** Adding `np_bootloader_app_image_tests` makes the Class B selection 21 and the repo total 27. `NP_CLASS_B_TEST_COUNT` is `21` in both `firmware-cross-build.yml` and `build-all.yml`; `build-all.yml`'s partition arithmetic is `6 + 21 = 27`. `NP_SAFETY_TEST_COUNT` is untouched at 6 — the bootloader is Class B. Verified locally: `ctest` reports `Total Tests: 27`, the Class C selection 6, its complement 21, and 27/27 pass.

**The bootloader leg is now green, and `continue-on-error` still stays on it.** Dropping it is phase 3, deliberately a separate reviewable change — **done 2026-08-09, see §6.4**. The main-firmware leg remains red on Defect D (`np_mod_pbm.c:112`, OI-SWCI-12) — unrelated to this fix and expected until phase 5.

### 6.4 Phase 3 as implemented (2026-08-09)

`continue-on-error: true` is removed from the bootloader cross-compile leg in the two workflows that carried it — `firmware-cross-build.yml` job `bootloader` and `build-all.yml` job `bootloader-cross`. Four settings remain, all on legs whose defects are still open: `main-firmware` and `main-firmware-cross` (Defect D, OI-SWCI-12, phase 5), `safety-mcu-cross` and `safety-mcu-ci.yml`'s `cross-build` (Defect A, phase 4).

**A green run is not evidence that a gate gates, so the gate was demonstrated by making it fail.** Through phases 0–2 this leg was *already* green while blocking nothing; a phase-3 verification that only observed a passing run would have been satisfied identically by the unchanged file. A deliberate unresolved-symbol breakage was pushed, the run was measured, the breakage was reverted, and the run was measured again.

| | red — breakage present | green — reverted |
|---|---|---|
| commit | `1e63026` | `785f1ef` |
| run | [31333323733](https://github.com/stevehickman/NeuroPulse/actions/runs/31333323733) | [31333406860](https://github.com/stevehickman/NeuroPulse/actions/runs/31333406860) |
| `Bootloader (i.MX RT1062)` | **failure**, failing step `Build` | **success**, `Build` success |
| `CMake host tests (21 Class B targets)` | success | success |
| `Main firmware (i.MX RT1062, Class B)` | failure, failing step `Build` | failure, failing step `Build` |
| **workflow run conclusion** | **`failure`** | **`success`** |

The breakage was an `extern void np_phase3_gate_probe(void)` declared and called from `Bootloader_Reset()`, chosen so the failure is an *unresolved symbol at link* — deliberately the same shape as Defect B, the class of regression this leg exists to catch — rather than a compile diagnostic:

```
np_main.c:(.text.Bootloader_Reset+0x6): undefined reference to `np_phase3_gate_probe'
collect2: error: ld returned 1 exit status
```

**The pair is a within-run control, not just a before/after.** Two things make the run-conclusion difference attributable to the removed setting and nothing else:

- **`main-firmware` fails on *both* runs and is masked on both.** On the green run a job concluded `failure` and the run still concluded `success` — that is `continue-on-error` working, observed in the same workflow, on the same commit, at the same time as the bootloader leg was gating. The only difference between the two legs is the setting this phase removed.
- **`host-tests` succeeds on both runs**, so the red run's redness cannot be coming from the one other non-masked job. This is structural rather than lucky: `firmware/bootloader/CMakeLists.txt` puts only `src/np_mem.c` and `src/np_app_image.c` into host-test targets, so a breakage in `np_main.c` is invisible to them.

**What `continue-on-error` actually suppresses — narrower than the name implies.** A masked job still reports `conclusion: failure` at job level and its check-run still shows a red ✗ in the PR check list; `Main firmware` does exactly this on every run, including the green one above. Only the job's contribution to the **run conclusion** is suppressed. Job state and check-run state therefore cannot distinguish a gating leg from a masked one, and any phase-3 verification built on them would have passed before the change was made. The run conclusion is the sole discriminating signal, which is why it is the row in bold.

**`build-all.yml` was exercised separately.** It is `schedule` + `workflow_dispatch` only and never runs on a pull request, so its promoted leg cannot be observed on the PR that changes it. Dispatched against the branch: run [31333420250](https://github.com/stevehickman/NeuroPulse/actions/runs/31333420250), conclusion `success`, with `Bootloader cross-compile (i.MX RT1062)` success and **both** `main-firmware-cross` (Defect D) and `safety-mcu-cross` (Defect A) concluding `failure` while still masked. That is the same control as above with two masked jobs instead of one: three cross-compile legs, two failing, run green — the promoted leg is the only one whose state the run now follows.

**OCRAM stays at exactly 100.00% and that is not a number to watch.** The region is fully allocated by the `NOLOAD` reservations that tile it (§4.3, §6.3), so the figure is a property of the memory map rather than of code size and will not move as code changes. The code-size gate is a different mechanism entirely: the `ASSERT((_bootloader_end - ORIGIN(OCRAM)) <= _bl_max_size)` on the 48 KiB limit, currently about 26 KB used. Making this leg blocking protects both — the `ASSERT` fails the link, the link failure fails the job, and the job now fails the run.

**What this phase did not do.** It did not make the bootloader leg a *required* check, and nothing here prevents a merge. That is branch protection, it is an admin setting outside the repository, and it is OI-SWCI-04 at phase 6. Measured on the day: `main` has no branch-protection object, and the `Safety` ruleset's two rules (`deletion`, `non_fast_forward`) include no `required_status_checks`. Phases 4 and 5 are untouched, as are all three test-count guards and every `permissions:` block.

### 6.5 Phase 4 as implemented (2026-08-09)

Defect A is closed and OI-SWCI-06 with it. Measured on the same host as phases 1–3 (macOS, ARM GNU 14.2.1, Ninja), exit codes captured directly rather than through a pipe:

| | before | after |
|---|---|---|
| configure | `rc=0` | `rc=0` |
| build (`np_safety_mcu_objs`) | `rc=1` | **`rc=0`** |
| `'GPIOA'`/`'GPIOB' undeclared` errors | 4 | **0** |
| errors of any kind | 4 | **0** |
| warnings of any kind | 0 | **0** |
| Class C translation units compiling | 9 of 10 | **10 of 10** |
| distinct SOUP components for SW-01 | 1 (Monocypher) | 3 |

One correction to §4's original measurement while it is in view: row 2 records `rc=2`, taken under the sequential generator. Under Ninja the pre-fix failure is `rc=1`. The defect is identical; only the generator's exit convention differs.

**The failing set was exactly one translation unit, and that was checked rather than assumed.** A clean `-k 0` build — not the incremental one, which silently reports only what it retried — fails `np_gpio_mgr.c` alone, with 4 errors and no others hidden behind it. This matters because the naive reading of §4.1 is that the whole Class C firmware is broken; in fact nine of ten TUs already compiled, and `np_gpio_mgr.c` is the sole consumer of the peripheral macros. That is also why the host-test build never needed CMSIS: no host-test target compiles `np_gpio_mgr.c`.

**What was vendored, and what deliberately was not.** Two components — ARM CMSIS-Core(M) 5.6.0 (CMSIS_5 tag `5.9.0`) and ST CMSIS-Device STM32G0 `v1.4.5` — eight files, both Apache-2.0, both header-only, both with `VERSION` records carrying per-file provenance and SHA-256, byte-exactness verified by an independent second download and `cmp`. The ST HAL and LL drivers are **not** vendored: SW-01 makes zero `HAL_*` and zero `LL_*` calls under word-boundary matching, so the HAL would be tens of thousands of lines of third-party logic added to **Class C** SOUP surface for no benefit. The full "intentionally NOT vendored" lists are in the two `VERSION` files, per the FreeRTOS template.

**The §7.1.2 anomaly review was performed as an activity, not asserted as a conclusion.** This is the first SOUP vendored specifically for Class C software, so the evaluation is a separate, attributable, versioned record: **NP-SOUP-CMSIS-001 Rev A**. It states its method (vendor release notes at the exact tag; the upstream issue tracker filtered to the vendored files; a v1.4.4→v1.4.5 peripheral-struct differential), its per-item assessment, and its limitations — a keyword-driven issue search is not exhaustive, and ARM publishes no numbered CMSIS-Core errata. Two findings worth surfacing here:

- The dominant result is a **scoping** one. CMSIS_5 carried 188 open issues at review; essentially all are in DSP, NN, RTOS2, DAP, SVDConv and Pack tooling — none of it vendored. That is the measurable return on §9.4's "vendor the narrowest possible subset", and it is why a Class C review of a large upstream came out clean.
- ST publishes "Known Limitations: **None**" for v1.4.5, which is a weak result taken alone, so it was used as a starting point. v1.4.5's own changelog includes *"Correct the definition of the `TIMx_CCR5` Capture/Compare register"*, and SW-01 uses TIM2 for R-peak capture — a plausible hazard. Checked: the correction narrows `TIM_CCR5_CCR5_Msk` from `0xFFFFFFFF` to `0xFFFF`, a bit-mask `#define`, and a diff of **every** `typedef struct` block in `stm32g071xx.h` between v1.4.4 and v1.4.5 shows **no struct layout changes at all**, so no register offset moved. SW-01 never names `TIM_CCR5_*`.

**The symbols were verified on the artifact, because a successful compile only proves they resolved — not that they resolved correctly.** Same principle as the §4.2 disassembly check and the §6.3 `nm` check. Measured on `np_gpio_mgr.c.obj`:

```
00000000 <np_gpio_mgr_init>:
   0:  20a0    movs r0, #160    ┐ 160 << 23 = 0x50000000 = GPIOA_BASE
   8:  05c0    lsls r0, r0, #23 ┘
  12:  4817    ldr  r0, [pc, #92]  → literal 0x50000400 = GPIOB_BASE
```

`IOPORT_BASE` is `0x50000000` with `GPIOA_BASE` at `+0x000` and `GPIOB_BASE` at `+0x400`, so both are right. A wrong-but-defined `GPIOA` would compile identically and silently address the wrong peripheral — on the ten lines that own every stimulation enable.

**The gate was falsified before it was believed.** `np_safety_mcu_objs` is a new target, and a target that compiles nothing also exits 0. Four mutants, three killed and one control, run clean each time:

| Mutant | Result | Killed by |
|---|---|---|
| M0 control (unmutated), run before and after the set | `rc=0` | — passes, twice |
| M1 vendored include dirs removed — **the Defect A regression** | `rc=1` | `fatal error: stm32g0xx.h: No such file` |
| M2 `USE_HAL_DRIVER` reinstated — demands the un-vendored HAL | `rc=1` | `fatal error: stm32g0xx_hal.h: No such file` |
| M3 syntax error in a Class C TU | `rc=1` | compile diagnostic |

M1 is the one that matters: it is the exact defect this phase closed, and the leg rejects it. Note it now fails on the *include* rather than on `'GPIOA' undeclared`, because `np_safety_config.h` names the header — same defect, louder diagnostic. M2 guards the `USE_HAL_DRIVER` removal, which is otherwise the kind of one-line deletion a later reader restores as "obviously missing".

**Two defects were discovered behind Defect A, one closed and one not.** Defect F (duplicate `-specs=`, §4.5) is fixed in this phase. Defect E (§4.4) — 24 undefined `np_hal_*` platform symbols — is **open** and is Class C driver work needing design review; it is OI-SWCI-17 and phase 7. The safety MCU compiles for its target and still has no linkable image, and §6's phase-4 criterion was corrected to say exactly that rather than to imply an image exists.

**Count guards untouched.** No test target was added or removed: `NP_SAFETY_TEST_COUNT` stays `6`, `NP_CLASS_B_TEST_COUNT` stays `21`, and `build-all.yml`'s partition arithmetic stays `6 + 21 = 27`. Verified locally: the Class C selection matches 6 targets and 6/6 pass. The host build is genuinely unaffected — the device-header include is gated on `STM32G071xx`, which the `NP_BUILD_TESTS` branch returns before ever defining.

#### 6.5.1 The gate promotion, and its demonstration

`continue-on-error: true` is removed from the two safety-MCU cross-compile legs — `safety-mcu-ci.yml` job `cross-build` and `build-all.yml` job `safety-mcu-cross`. **Two settings remain, both on `main-firmware` legs, both Defect D / OI-SWCI-12 / phase 5.** Landed as a separate PR from the vendoring so a two-line gate change is reviewable on its own rather than buried under ~14,000 lines of vendored headers.

Both legs build `--target np_safety_mcu_objs`. The job is renamed `Cross-compile (STM32G071, Cortex-M0+, Class C)` and the `Report image size` step is deleted — there is no image, and `find … -name '*.elf'` matching nothing exits `0`, so that step would have passed forever while proving nothing. Both revert at phase 7.

**A green run is not evidence that a gate gates, so the gate was demonstrated by making it fail** — the same requirement and the same reason as phase 3. Through the vendoring PR this leg was *already* green while blocking nothing, so a verification that only observed a passing run would have been satisfied identically by the unpromoted file.

| | red — breakage present | green — reverted |
|---|---|---|
| commit | `a0ee6c6` | `4b203f5` |
| `safety-mcu-ci.yml` run | [31336301542](https://github.com/stevehickman/NeuroPulse/actions/runs/31336301542) | [31336559374](https://github.com/stevehickman/NeuroPulse/actions/runs/31336559374) |
| `Cross-compile (STM32G071, Cortex-M0+, Class C)` | **failure**, failing step `Compile (all Class C translation units)` | **success** |
| `Safety MCU host tests (6 targets)` | success | success |
| **workflow run conclusion** | **`failure`** | **`success`** |

The breakage was an undeclared CMSIS device symbol used from the enable-GPIO path in `np_gpio_mgr.c`, chosen so the failure is **the same shape as Defect A** — a compile diagnostic on a missing device symbol in a Class C translation unit — rather than an arbitrary syntax error:

```
np_gpio_mgr.c:31:27: error: 'GPIOZ' undeclared (first use in this function);
                     did you mean 'GPIOD'?
ninja: build stopped: subcommand failed.
```

**`host-tests` succeeds on both runs**, so the red run's redness cannot be coming from the other non-masked job in that workflow. This is structural rather than lucky: no host-test target compiles `np_gpio_mgr.c` (§6.5), so a breakage there is invisible to them — which is also the reason this leg exists.

**`build-all.yml` was exercised separately, and supplies the within-run masked control.** It is `schedule` + `workflow_dispatch` only and never runs on a pull request, so its promoted leg cannot be observed on the PR that changes it. Dispatched twice against the branch:

| Dispatch | commit | `safety-mcu-cross` | `main-firmware-cross` | `bootloader-cross` | run conclusion |
|---|---|---|---|---|---|
| [31336496931](https://github.com/stevehickman/NeuroPulse/actions/runs/31336496931) | `a0ee6c6` (broken) | **failure** (gating) | failure (masked) | success | **`failure`** |
| [31336735985](https://github.com/stevehickman/NeuroPulse/actions/runs/31336735985) | `4b203f5` (reverted) | success (gating) | failure (**masked**) | success | **`success`** |

The second row is the control that makes the difference attributable to the removed setting and nothing else: **a job concluded `failure` and the run still concluded `success`** — `continue-on-error` working, observed in the same workflow, on the same commit, at the same time as the safety-MCU leg was gating. The only difference between the two legs is the setting phase 4 removed. It also confirms the phase-5 legs were not promoted by accident.

As at phase 3, `continue-on-error` suppresses **less** than its name implies: `main-firmware-cross` reports `conclusion: failure` at job level on both runs and shows a red ✗ regardless. Only its contribution to the **run conclusion** is suppressed, which is why the run conclusion is the row in bold and the sole discriminating signal.

**What this phase did not do.** It did not make the safety-MCU leg a *required* check, and nothing here prevents a merge — that is branch protection, an admin setting outside the repository, and it remains OI-SWCI-04 at phase 6. It did not touch any `permissions:` block, any test-count guard, or the phase-5 `main-firmware` legs. And it did not make the Class C firmware *link* — see Defect E.

### 6.6 Phase 5 as implemented (2026-08-09)

Defect D is closed and OI-SWCI-12 with it. Measured on the same host as phases 1–4 (macOS, ARM GNU 14.2.1, Ninja), exit codes captured directly rather than through a pipe, and built with `-k 0` so every error surfaces rather than only the first:

| | before | after |
|---|---|---|
| configure | `rc=0` | `rc=0` |
| build (super-project, `-k 0`) | `rc=1` | **`rc=0`** |
| `implicit declaration` errors | 1 | **0** |
| errors of any kind | 1 | **0** |
| warnings of any kind | 0 | **0** |
| `undefined reference`, any symbol | 0 | **0** |
| missing-header errors | 0 | **0** |
| targets built | 85 of 86 | **86 of 86** |

**Defect D was the only thing wrong, and that was checked rather than assumed.** A clean `-k 0` build — not the incremental one, which reports only what it retried — produced exactly one error before the fix and none after. The two zero rows matter as much as the first: **zero missing-header errors** is what falsifies phase 5's MCUX premise (§6, OI-SWCI-20), and **zero unresolved symbols** is what had to be read carefully rather than celebrated (§4.7).

**"Every SW-02 translation unit" was measured, not assumed.** That word is load-bearing — `cmake --build` compiles what is reachable from the default target, so a `.c` belonging to no target, or to an `EXCLUDE_FROM_ALL` target, would compile nowhere and leave this leg green forever while covering less than it claims. Checked by set difference: every `.c` under the thirteen SW-02 source roots the super-project adds, excluding `tests/` and `vendor/`, against the `.c.obj` files the cross build actually produced.

```
compiled translation units:  72   (includes vendored FreeRTOS + Monocypher)
first-party source .c files: 62
.c present in the tree but never compiled: 0
```

`rg EXCLUDE_FROM_ALL firmware/` also returns nothing, so no target is withheld from the default build. The claim stands as written. Worth re-running whenever a module is added — this is the same class of gap as OI-SWCI-08 (a module in the build graph missing from a `paths:` list), one layer in, and it is not self-detecting either.

**A successful build is not evidence that an image exists, so the artifacts were enumerated rather than inferred.** This is the phase-4 lesson applied one step earlier. Measured on the same build:

```
$ grep 'Linking C executable' build-after.log
[77/86] Linking C executable bootloader/np_bootloader; Generating np_bootloader.bin and np_bootloader.hex

$ grep -c 'Linking C static library' build-after.log
13

$ file build/fw/bootloader/np_bootloader
ELF 32-bit LSB executable, ARM, EABI5 version 1 (SYSV), statically linked, not stripped

$ arm-none-eabi-size build/fw/bootloader/np_bootloader
   text    data     bss     dec     hex
  17280       1  469112  486393   76bf9
```

One executable, and it is the bootloader — already covered by its own leg and its own traceability row since phase 3. Thirteen static libraries, which is every SW-02 module. **There is no SW-02 application image, because there is no target that would produce one.** That is §4.7 and OI-SWCI-21, and it is why the promoted check is described everywhere as compiling SW-02 rather than building it.

**The fix was falsified before it was believed.** Five runs against `np_hub_control`, with the unmutated control run first and last:

| Mutant | Result | Killed by |
|---|---|---|
| M0 control (unmutated), run before and after the set | `rc=0` | — passes, twice |
| M1 `np_pbm1064_dose_load_cal(s_cal)` restored — **the Defect D regression** | `rc=1` | `implicit declaration of function … did you mean 'np_pbm1064_dose_load_cal_stub'?` |
| M2 naive rename: `…_stub(s_cal)`, 2-D array into a one-row parameter | `rc=1` | `passing argument 1 … from incompatible pointer type` |
| M3 loop kept, but passes the whole array each iteration | `rc=1` | same incompatible-pointer diagnostic |
| M4 `s_cal` bound reverted to `NP_PBM1064_ZONE_COUNT` | `rc=0` | **SURVIVES — see below** |

M1 is the defect this phase closed, and the leg rejects it. M2 is the fix this phase was told not to make, and it is worth being exact about *why* it is rejected: `-Wincompatible-pointer-types` under `-Werror` stops it at compile time, so it takes an added cast to reach the hazard. The hazard on the far side of that cast — row 0 populated, every other zone left with zeroed dose calibration, on the path that accumulates J/cm² — is the reason the alternative is recorded in §4.6 as rejected rather than silently not chosen.

**M4 survives, and is named rather than buried.** `NP_PBM1064_ZONE_COUNT` and `NP_HUB_ZONE_SLOT_COUNT` are both `5`, so reverting the array's bound compiles clean and behaves identically. No build output can distinguish them, which means that half of the fix is a property of the *source* and not of any artifact — the same shape as the surviving unaligned-copy mutant in §6.2, where the compensating control had to live somewhere other than the test. Here the control is placement: the constraint is written on the declaration itself, and `np_pbm1064_config.h:147-154` independently forbids sizing a calibration array with `NP_PBM1064_ZONE_COUNT`. A reader who changes either macro meets the statement at both ends. A `_Static_assert` was considered and not added: it would assert an equality the fix exists to stop depending on.

**Count guards untouched.** No test target was added or removed: `NP_CLASS_B_TEST_COUNT` stays `21`, `NP_SAFETY_TEST_COUNT` stays `6`, and `build-all.yml`'s partition arithmetic stays `6 + 21 = 27`. Verified locally: `ctest` on the Class B complement selection reports `Total Tests: 21` and 21/21 pass.

**One note on the verification command, recorded because it costs a wasted run.** `-DCMAKE_TOOLCHAIN_FILE=firmware/cmake/arm-none-eabi.cmake` given as a *relative* path fails with `Could not find toolchain file`, because CMake resolves a relative toolchain path against the build directory rather than the source directory or the cwd. Use an absolute path, which is also the form both workflows already use (`${{ github.workspace }}/firmware/cmake/arm-none-eabi.cmake`). Not a defect in the tree, and the failure is loud rather than silent — but it presents as a missing file, which reads like a checkout problem.

#### 6.6.1 The gate promotion, and its demonstration

`continue-on-error: true` is removed from the two main-firmware cross-compile legs — `firmware-cross-build.yml` job `main-firmware` and `build-all.yml` job `main-firmware-cross`. **Zero settings remain: `rg 'continue-on-error: true' .github/workflows/` returns nothing.** This is the last leg to be promoted, and all four cross-compile legs across the three workflows now gate. Both `EXPECTED TO FAIL` comment blocks are rewritten to say what a green result does and does not claim (§4.7).

**A green run is not evidence that a gate gates, so the gate was demonstrated by making it fail** — the same requirement and the same reason as phases 3 and 4. `continue-on-error` suppresses only the job's contribution to the **run** conclusion; a masked job still reports `conclusion: failure` at job level and still shows a red ✗ in the PR check list. Job state and check-run state are therefore non-discriminating: a verification built on either would have passed identically before this change was made. The run conclusion is the sole signal that moves.

**Phases 3 and 4's within-run control is not available at phase 5, and the replacement is stronger.** Both earlier phases leaned on a second failing-but-masked job in the same run — "job A fails and the run goes red, job B fails and the run stays green, same run, same commit" — to attribute the difference to the removed setting rather than to anything environmental. This phase removes the last masked legs, so no such job exists any more. Instead the **breakage is held constant and only the setting varies**: the same broken commit is run with the setting restored and with it removed. That isolates the setting on *identical code*, which the within-run control never did — it compared two different legs and relied on them being otherwise alike.

Four runs of `firmware-cross-build.yml` on the phase-5 branch. The middle two are the control pair: **the firmware is byte-identical between them, and the only difference in the entire tree is the one line of YAML this phase removed.**

| | 1 — fixed, gating | 2 — broken, setting **restored** | 3 — broken, setting **removed** | 4 — reverted, gating |
|---|---|---|---|---|
| commit | `b80b4db` | `e5749f6` | `c5a7ed3` | `2de8cd0` |
| run | [31346446631](https://github.com/stevehickman/NeuroPulse/actions/runs/31346446631) | [31346536157](https://github.com/stevehickman/NeuroPulse/actions/runs/31346536157) | [31346603332](https://github.com/stevehickman/NeuroPulse/actions/runs/31346603332) | [31346722647](https://github.com/stevehickman/NeuroPulse/actions/runs/31346722647) |
| `Main firmware (i.MX RT1062, Class B)` | success | **failure**, step `Build` | **failure**, step `Build` | success |
| `Bootloader (i.MX RT1062)` | success | success | success | success |
| `CMake host tests (21 Class B targets)` | success | success | success | success |
| **workflow run conclusion** | `success` | **`success`** | **`failure`** | `success` |

**Read columns 2 and 3 together — that is the whole demonstration.** The `Main firmware` job concludes `failure` in both, with the same failing step, on the same firmware, from the same diagnostic. The **run** concludes `success` in one and `failure` in the other. The only thing that changed is `continue-on-error`. This is a cleaner attribution than phases 3 and 4 achieved: their control compared two *different legs* within one run and relied on the legs being otherwise alike, whereas this compares one leg against itself with the code held constant.

It also demonstrates, on this leg specifically, the property §6 and the phase-3/4 sections both state: **job conclusion is non-discriminating.** Column 2 and column 3 have identical job-level results. A phase-5 verification that read `gh run view --json jobs` and stopped there would have reported the same answer whether or not the setting had been removed.

The breakage was an implicit declaration of a non-existent function called from the calibration-load path in `np_mod_pbm.c`, chosen so the failure is **the same shape as Defect D** — the class of regression this leg exists to catch — rather than an arbitrary syntax error:

```
firmware/hub_control/modules/np_mod_pbm.c:128:9: error: implicit declaration
  of function 'np_phase5_gate_probe' [-Werror=implicit-function-declaration]
ninja: build stopped: subcommand failed.
```

**`host-tests` succeeds on all four runs**, so the red run's redness cannot be coming from the other non-masked job in that workflow. This is structural rather than lucky: `np_mod_pbm.c` is compiled into no host-test target — the Class B suite reaches `np_hub_control` through `np_cvns_reenable_tests`, `np_log_backend_tests`, `np_transport_tests`, `np_mod_cvns_tests`, `np_module_map_tests`, `np_protocol_tests` and `np_mod_stim_tests`, none of which build the PBM module. Which is also why Defect D survived to be found by a cross-build in the first place.

**`build-all.yml` was exercised separately.** It is `schedule` + `workflow_dispatch` only and never runs on a pull request, so its promoted leg cannot be observed on the PR that changes it. Dispatched twice against the branch:

| Dispatch | commit | `main-firmware-cross` | `bootloader-cross` | `safety-mcu-cross` | run conclusion |
|---|---|---|---|---|---|
| [31346672311](https://github.com/stevehickman/NeuroPulse/actions/runs/31346672311) | `c5a7ed3` (broken) | **failure** (gating) | success | success | **`failure`** |
| [31346727224](https://github.com/stevehickman/NeuroPulse/actions/runs/31346727224) | `2de8cd0` (reverted) | success (gating) | success | success | **`success`** |

Both dispatches double as the check that this phase left the other legs alone: `bootloader-cross` and `safety-mcu-cross` succeed on both, including on the run where the main-firmware leg took the run down. `safety-mcu-ci.yml` did not trigger on this branch at all, which is its `paths:` filter behaving correctly — nothing under `firmware/safety_mcu/**` was touched.

**The breakage was reverted in the same PR, and the revert was verified as a revert rather than assumed.** `git diff b80b4db 2de8cd0` is empty: the tree after the demonstration is byte-identical to the tree before it.



**What this phase did not do.** It did not make the main-firmware leg a *required* check, and nothing here prevents a merge — that is branch protection, an admin setting outside the repository, and it remains OI-SWCI-04 at phase 6. It did not touch any `permissions:` block, any test-count guard, or the bootloader and safety-MCU legs. It did not vendor or reference the MCUX SDK (OI-SWCI-20). It did not touch Defect E or the SW-01 platform layer (OI-SWCI-17, phase 7). And it did not produce an SW-02 application image, because no target for one exists (§4.7, OI-SWCI-21).

### 6.7 Phase 6 as prepared (2026-08-10) — NOT COMPLETE

**Phase 6 is prepared, not applied.** Everything inside the repository is done and measured. The remaining step — adding a `required_status_checks` rule to the `Safety` ruleset — is an admin action with merge-blocking consequences for every PR in the repository, it is gated on an unanswered policy question (OI-SWCI-04), and it is **Steve's to run**. The exact command is in §6.7.6. It was deliberately not run, including not run "to test": a failed experiment there blocks every merge in the repository, and the blocked state is not obvious from inside a PR.

#### 6.7.1 The deadlock — `paths:` and "required" are mutually exclusive

A required status check that never reports does not pass. It renders as *"Expected — waiting for status"* and blocks the pull request **forever**; GitHub treats a missing check as pending, not as satisfied. A workflow-level `paths:` filter suppresses the entire workflow, so none of its checks report at all.

So on 2026-08-10, requiring `Bootloader (i.MX RT1062)` would have made **every docs-only PR permanently unmergeable**. That is a direct collision between phase 6 and the §5.0 principle — build only what the PR could have changed — and both are correct. Resolving it is what this phase is for.

Measured on `main` at `9e01c0e`, before any change:

| Workflow | Paths-filtered | Reports on a docs-only PR? |
|---|---|---|
| `firmware-cross-build.yml` | yes | no |
| `safety-mcu-ci.yml` | yes | no |
| `web-ci.yml` | yes | **yes — see below** |
| `codeql.yml` | no | yes |
| `build-all.yml` | n/a — `schedule` + `workflow_dispatch` only | **never, on any PR** |

**One correction to that table, found on inspection and confirmed by measurement.** `web-ci.yml` *is* paths-filtered but its list contains `docs/**`, `CLAUDE.md`, `firmware/**` and `app/android/**` — the §5.0 "known accepted exception" for the section-ref guard. It therefore **does** report on a docs-only PR and always did. It was converted anyway, for the single-list benefit in §6.7.3, but it was never part of the deadlock.

#### 6.7.2 The fix — always report, but still don't build

Workflow-level `paths:` filters become **job-level `if:` conditions**. The workflow always triggers, so every check always reports; jobs whose scope is untouched **skip**, and a skipped job satisfies a required status check where one that never reports does not.

§5.0 is preserved exactly. Nothing irrelevant is compiled or tested — the `changes` job resolves the same list against the same changed-file set and the build surface is identical. Only the *reporting* surface moved.

Change detection is a ~40-line in-repo shell script, `scripts/ci-changed-scope.sh`, not a third-party action. This repository has just completed a Class C SOUP exercise (§9, `NP-SOUP-CMSIS-001`); importing `dorny/paths-filter` or similar would add supply-chain surface for something `git diff` against the merge base does in a few lines — and unlike an action, this file is reviewed, diffed and unit-tested in the same PR as the code it gates.

The script supports exactly two pattern shapes, `prefix/**` and an exact path, which is all the retired `paths:` lists ever used. **Any other shape is a hard error, not a silent non-match** — a pattern the matcher does not understand must be loud, because the failure mode of quietly matching nothing is a gate that reports green having built nothing.

Three guards run in every `changes` job before the matcher is trusted, because everything about this mechanism fails toward green:

| Guard | Catches |
|---|---|
| `--self-test` | The matcher itself being wrong — prefix-boundary bugs, an unsupported shape silently not matching, an empty list read as "nothing is relevant" |
| `--check-tree` | A *well-formed but wrong* pattern. `firmware/bootlaoder/**` is valid, matches nothing, and silently takes the whole module out of scope. Measured: with that one typo, a real `firmware/bootloader/src/np_main.c` change evaluates `relevant=false`. Every pattern must still resolve to something tracked |
| *Scope assertions* | The list being wrong at the §5.0 level — Class B judging a safety-MCU-only change in scope, Class C judging a `firmware/crypto/**` change out of scope |

The first two were each falsified against deliberately broken copies before being relied on: three mutants of the matcher (dropped prefix slash, unsupported shape returning "no match", exact-match becoming a glob) and one typo'd relevance list. The first two mutants and the typo were caught. The third was not, and is an **equivalent mutant** rather than a test gap: shell glob treats `.` literally, and every glob metacharacter is rejected upstream by the shape guard that mutant 2 does test. Recorded rather than papered over.

#### 6.7.3 What the conversion also fixed, and what it broke

Three things came out of this that were not the goal:

**The two-list hazard is gone rather than relocated.** `firmware-cross-build.yml` and `safety-mcu-ci.yml` each carried the `paths:` list twice, under `push:` and under `pull_request:`, with a header warning that the duplication was deliberate — the Actions parser has no YAML anchors — and that a module addition must edit BOTH. There is now one list per workflow.

**The §5.0 dependency reasoning is now executable rather than only written down.** Each `changes` job carries a *Scope assertions* step that fails the job if a safety-MCU-only change is ever judged in scope for Class B, or a `firmware/crypto/**` change out of scope for Class C. Those two facts were previously prose in a header comment that nothing checked.

**A §5.0 regression was introduced and then closed, in this phase.** The first implementation treated any event without a usable base revision as in-scope — the fail-safe direction is "build", because skipping on uncertainty is how a gate becomes a no-op unnoticed. But a branch's *first* push carries an all-zero `before`, so every new branch ran a full firmware build regardless of what it touched. It was caught by measurement rather than by review: on the scratch PR's first commit, for the same head SHA, the `pull_request` event skipped correctly (run [31388449142](https://github.com/stevehickman/NeuroPulse/actions/runs/31388449142)) while the `push` event built everything (run [31388444439](https://github.com/stevehickman/NeuroPulse/actions/runs/31388444439)). `push` now falls back to the default branch. `workflow_dispatch` deliberately keeps the build-everything fail-safe: on the default branch a default-branch fallback would diff `main` against itself, find nothing, and turn the manual "build it now" button into a no-op.

#### 6.7.4 Measured — the docs-only PR

Scratch PR [#262](https://github.com/stevehickman/NeuroPulse/pull/262), based on the phase-6 branch so the converted workflows are the ones under test and the diff is genuinely docs-only (one file under `docs/`, `git diff --name-only` against its base = exactly `docs/_scratch_phase6_probe.md`).

At head `c3999a5`, on **both** the `push` and `pull_request` events:

| Context | Conclusion |
|---|---|
| `Class B scope` | `success` |
| `CMake host tests (Class B)` | **`skipped`** |
| `Bootloader (i.MX RT1062)` | **`skipped`** |
| `Main firmware (i.MX RT1062, Class B)` | **`skipped`** |
| `Class C scope` | `success` |
| `Safety MCU host tests (Class C)` | **`skipped`** |
| `Cross-compile (STM32G071, Cortex-M0+, Class C)` | **`skipped`** |
| `Web scope` | `success` |
| `TypeScript type-check + Vitest + Vite build` | ran for real — `docs/**` is in its list (§5.0 accepted exception) |

Runs: `pull_request` [31388830758](https://github.com/stevehickman/NeuroPulse/actions/runs/31388830758) (Class B), [31388830730](https://github.com/stevehickman/NeuroPulse/actions/runs/31388830730) (Class C); `push` [31388828525](https://github.com/stevehickman/NeuroPulse/actions/runs/31388828525), [31388828490](https://github.com/stevehickman/NeuroPulse/actions/runs/31388828490).

**The load-bearing observation is that these contexts are PRESENT with `conclusion: skipped`, not absent.** That is the whole difference between a check that satisfies a requirement and one that hangs a PR forever. Before the conversion, `GET /commits/{sha}/check-runs` on a docs-only PR returned no row for any of them.

#### 6.7.5 What is still NOT verified, and why — read before applying §6.7.6

**The behaviour is documented by GitHub and corroborated by measurement here, but it has NOT been exercised against a live `required_status_checks` rule on this repository** — because exercising it requires the exact admin action this phase is forbidden to take. Stating that plainly rather than burying it: it is the load-bearing assumption of the whole phase.

**Primary evidence — GitHub's own documentation** ([Troubleshooting required status checks](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/collaborating-on-repositories-with-code-quality-features/troubleshooting-required-status-checks), retrieved 2026-08-10) states both halves of the distinction this phase is built on, and states them separately:

- when a **workflow** is skipped by path or branch filtering — *"Associated checks stay in a 'Pending' state and block merging"*
- when a **job** is skipped by a conditional — *"The job reports 'Success'"*

That is precisely the transformation §6.7.2 performs: the same scoping decision, moved from the first mechanism to the second.

**Corroborating measurement.** On PR #262 with **10 check-runs concluding `skipped`**, GitHub's own status aggregate reports `statusCheckRollup.state = SUCCESS`, not `PENDING`.

Read that as corroboration and nothing more. The rollup is computed over all check runs regardless of whether any are required, and it never touches the required-context **name-matching** path — which is the part that could actually surprise, since a required context is an exact string match and a mismatch produces no error anywhere. A green rollup shows `skipped` is not treated as failing in general; it does not show that these seven specific strings will be matched and satisfied. Absence of a symptom is not absence of the hazard.

**Therefore §6.7.8 is written to be applied in an order that is safe if the assumption is wrong**, and §6.7.9 is the rollback:

1. Run the `PUT` in §6.7.8.
2. **Immediately** re-check scratch PR [#262](https://github.com/stevehickman/NeuroPulse/pull/262), which is **deliberately left open for this** — it is docs-only, based on the phase-6 branch, and already has all six firmware contexts sitting at `conclusion: skipped`. It is the exact shape that would deadlock, ready to answer the question in one page load. (Re-target its base to `main` after phase 6 merges, or open an equivalent one-file docs PR.)
3. If it shows the firmware checks as satisfied, the assumption holds and phase 6 is complete.
4. If it shows *"Expected — waiting for status"*, the assumption does not hold — run the §6.7.9 rollback and require only the checks that never skip: `Class B scope`, `Class C scope`, `Web scope`, plus the `codeql.yml` contexts.

Do not merge anything else until step 2 has been read.

#### 6.7.6 The gate demonstration — the conversion did not create a no-op

A green run is not evidence that a gate gates, and this change is exactly the kind that can silently turn a gate into a no-op: if the matcher stopped matching, every firmware job would skip, every check would go green, and nothing would be built. So it was demonstrated by breaking it — the same requirement and the same reason as phases 3, 4 and 5.

Two breakages, because this phase has two distinct ways to fail silently.

**Demonstration A — the leg still runs and still fails.** Same shape as Defect D (§4.6) and the phase-5 probe: an implicit declaration of a function that does not exist, on the calibration-load path in `np_mod_pbm.c`, chosen so the failure is the class of regression this leg exists to catch and so it is directly comparable with §6.6.1.

| | commit `e7b6ed9` — probe present | commit `1159902` — probe reverted |
|---|---|---|
| run | [31388849670](https://github.com/stevehickman/NeuroPulse/actions/runs/31388849670) | see Demonstration B |
| `Class B scope` | success | success |
| `CMake host tests (Class B)` | success | — |
| `Bootloader (i.MX RT1062)` | success | success |
| `Main firmware (i.MX RT1062, Class B)` | **failure**, step `Build` | — |
| **workflow run conclusion** | **`failure`** | — |

```
firmware/hub_control/modules/np_mod_pbm.c:132:9: error: implicit declaration
  of function 'np_phase6_gate_probe' [-Werror=implicit-function-declaration]
```

`host-tests` and `bootloader` succeed on that run, so the red run's redness is attributable to the `main-firmware` leg and not to something environmental — structurally, for the reason §6.6.1 records: `np_mod_pbm.c` is compiled into no host-test target.

**Demonstration B — the §5.0.1 ordering gate survived the conversion.** This is the failure mode specific to phase 6 and it has no analogue in phases 3–5. `main-firmware` has always been `needs: host-tests`, and GitHub skips a needs-gated job by applying an implicit `success()` — but **a custom `if:` containing a status function replaces that default**. The `if:` this phase adds contains `!cancelled()`. So without an explicit clause, converting `paths:` to `if:` would have quietly allowed the cross-build to run after the host tests had failed, with every file still looking correct. The conditions therefore name `needs.host-tests.result == 'success'` in full.

Measured by failing a Class B host test — a one-line `g_fail_count++` in `np_edf_tests.c`, so every real assertion still runs and the only difference is the exit status, and with `np_mod_pbm.c` byte-identical to the green baseline so a skip cannot be attributed to the firmware:

| Job | `push` run [31389139410](https://github.com/stevehickman/NeuroPulse/actions/runs/31389139410) | `pull_request` run [31389142982](https://github.com/stevehickman/NeuroPulse/actions/runs/31389142982) |
|---|---|---|
| `Class B scope` | success | success |
| `CMake host tests (Class B)` | **failure** | **failure** |
| `Bootloader (i.MX RT1062)` | success — correct: it deliberately has no `needs: host-tests` (§5.0.1) | success |
| `Main firmware (i.MX RT1062, Class B)` | **`skipped`** — the ordering gate held | **`skipped`** |
| **workflow run conclusion** | **`failure`** | **`failure`** |

**Both probes were reverted in this PR, and the reverts were verified as reverts rather than assumed** — `git diff` between the pre-probe and post-revert trees is empty for `np_mod_pbm.c` and for `np_edf_tests.c`.

**Scoping was demonstrated as a side effect, on a single commit.** At `e7b6ed9`, whose last commit touches only `firmware/hub_control/`, the `push`-event Class C run [31388848684](https://github.com/stevehickman/NeuroPulse/actions/runs/31388848684) reports both safety-MCU contexts `skipped` while the Class B legs run for real. §5.0 is doing its job under the new mechanism, on the same commit, in the same minute.

#### 6.7.7 Check names — a count in a required context is a latent deadlock

**A required context is an exact string match.** A context whose name contains a number that changes is a merge deadlock waiting for the next phase: the check gets renamed, the required context stops matching, nothing matches it, and the PR waits forever for a check that will never report under that name — with no error message anywhere pointing at the cause.

Two contexts had this defect:

| Was | Now | Count has moved |
|---|---|---|
| `CMake host tests (21 Class B targets)` | `CMake host tests (Class B)` | 19 → 20 → 21, three times, in phases 0–2 |
| `Safety MCU host tests (6 targets)` | `Safety MCU host tests (Class C)` | not yet — but phase 7 is Class C work |

The second was not in the phase-6 brief; it was found by reading the contexts off a live run rather than assuming them, which is also how the rest of §6.7's context strings were obtained.

**The counts did not go away — they moved to where they belong.** `NP_CLASS_B_TEST_COUNT: '21'` and `NP_SAFETY_TEST_COUNT: '6'` are unchanged, both assertion steps still name their count in the step title, and both still fail the job on selection drift. The number now lives in the assertion, not in the string that branch protection matches on.

**The aggregate-gate alternative was considered and rejected.** §6.1.1 anticipated it: *"A terminal `gate` job asserting `needs.host-tests.result == 'success'` was considered and not added… It becomes relevant at Phase 6."* A single `firmware-ci-gate` job would give one stable context regardless of how the legs are named or how many exist. It was rejected because it adds a second mechanism that can itself skip — an `if: always()` aggregate has to classify `skipped` dependencies correctly or it either blocks everything or passes everything — and the minimal change closes the same deadlock by deleting four characters from a `name:`. If the leg set becomes volatile enough that per-leg contexts are a maintenance burden, the aggregate is the right answer then.

**`build-all.yml` can never be a required check.** It is `schedule` + `workflow_dispatch` only, by design (§5.5), with no `pull_request` trigger — so it never reports on any PR and requiring it would block every PR unconditionally and permanently. It is recorded here, next to the required-checks list, because "add build-all for completeness" is exactly the kind of well-meant later edit that would wedge the repository. Its job names also carry counts (`CMake host tests (27 targets, unfiltered)`); that is harmless precisely because it is not requirable, and it should stay that way.

Workflows still `paths:`-filtered and therefore **still not requirable**: `android-ci.yml`, `ios-ci.yml`, `watchos-ci.yml`, `shdr-schema-ci.yml`, `warranty-nojoin-ci.yml`. Converting them is the same mechanical change if they are ever wanted as required checks.

#### 6.7.8 The prepared payload — Steve runs this, not CI

Read §6.7.5 first. The contexts below were read from live runs on 2026-08-10, not hand-written; `integration_id: 15368` is the GitHub Actions app, confirmed from `GET /commits/{sha}/check-runs`.

> **`PUT /rulesets/{id}` REPLACES the entire ruleset.** It is not a merge. The payload below therefore restates the two rules the `Safety` ruleset already has (`deletion`, `non_fast_forward`), its existing `bypass_actors` entry, **and `enforcement: active`**. Omitting any of them deletes or defaults it — and an `enforcement` that silently defaults to `disabled` would switch the whole `Safety` ruleset off while every check still reported green, which is the exact "gate becomes a no-op" failure this phase exists to prevent.

**The payload below is hand-constructed, not a round-trip of the GET.** A `GET` response carries `id`, `source`, `source_type`, `created_at`, `updated_at`, `node_id` and `_links`, none of which belong in a `PUT` body; piping GET output back into PUT is a known foot-gun. Diff the two by eye before running.

**Read-only pre-flight, verified 2026-08-10** — repeat it, because either could hold an orphaned context referencing the old count-bearing names:

```bash
gh api repos/stevehickman/NeuroPulse/rulesets --jq '.[] | "\(.id) \(.name) \(.enforcement)"'
gh api repos/stevehickman/NeuroPulse/branches/main/protection   # expect: 404 Branch not protected
gh api repos/stevehickman/NeuroPulse/rulesets/16412379 > before.json   # KEEP THIS — it is the rollback
```

Measured: `Safety` is the **only** ruleset (`16412379`, `active`), targeting `~DEFAULT_BRANCH`, rules exactly `deletion` + `non_fast_forward`, one bypass actor (`RepositoryRole` 5, `always`). `GET /branches/main/protection` → 404: there is no classic branch-protection object, so nothing else can be holding a stale required context.

**Precondition:** have an open docs-only PR live *before* running this, or the verification step in §6.7.5 observes nothing and the admin action is spent for no answer. PR #262 is left open for exactly this.

```bash
gh api --method PUT repos/stevehickman/NeuroPulse/rulesets/16412379 --input - <<'JSON'
{
  "name": "Safety",
  "target": "branch",
  "enforcement": "active",
  "conditions": { "ref_name": { "include": ["~DEFAULT_BRANCH"], "exclude": [] } },
  "bypass_actors": [
    { "actor_id": 5, "actor_type": "RepositoryRole", "bypass_mode": "always" }
  ],
  "rules": [
    { "type": "deletion" },
    { "type": "non_fast_forward" },
    {
      "type": "required_status_checks",
      "parameters": {
        "strict_required_status_checks_policy": false,
        "do_not_enforce_on_create": false,
        "required_status_checks": [
          { "context": "Class B scope",                                  "integration_id": 15368 },
          { "context": "CMake host tests (Class B)",                     "integration_id": 15368 },
          { "context": "Bootloader (i.MX RT1062)",                       "integration_id": 15368 },
          { "context": "Main firmware (i.MX RT1062, Class B)",           "integration_id": 15368 },
          { "context": "Class C scope",                                  "integration_id": 15368 },
          { "context": "Safety MCU host tests (Class C)",                "integration_id": 15368 },
          { "context": "Cross-compile (STM32G071, Cortex-M0+, Class C)", "integration_id": 15368 }
        ]
      }
    }
  ]
}
JSON
```

Then, immediately — assert the delta is *only* the added rule, rather than assuming it:

```bash
gh api repos/stevehickman/NeuroPulse/rulesets/16412379 > after.json
diff <(jq -S 'del(.rules)|del(.updated_at)|del(._links)' before.json) \
     <(jq -S 'del(.rules)|del(.updated_at)|del(._links)' after.json)   # expect: no output
jq -r '.rules[].type' after.json   # expect: deletion, non_fast_forward, required_status_checks
jq -r '.enforcement' after.json    # expect: active
```

Then do §6.7.5 step 2 — check the docs-only PR — before merging anything. The discriminator is `gh pr view 262 --json mergeStateStatus`: `BLOCKED` means the assumption failed and §6.7.9 is needed; anything else (`CLEAN`, `UNSTABLE`, `BEHIND`) means the skipped checks were satisfied.

Four notes on the choices, all of which are Steve's to overrule:

- **The two `* scope` jobs are required deliberately.** They are what run the matcher's self-test and the scope assertions. If the matcher breaks, that job goes red and the PR blocks — which is the point. The downstream jobs are already written to build rather than skip when `changes` fails (`!= 'false'`, not `== 'true'`), so this is belt and braces, not the only guard.
- **`strict_required_status_checks_policy: false`** — does not force a branch to be up to date with `main` before merging. `true` adds real merge friction on a busy branch; it is the more conservative choice for correctness and the less conservative one for throughput.
- **A merge queue would reinstate the deadlock in a new shape.** If a `merge_queue` rule is ever added to `Safety`, required checks must also report on `merge_group` events — and none of these workflows subscribe to that trigger, so every check would sit un-reported in the queue. Adding `merge_group:` to the three converted workflows is the fix, and it must land *before* the queue does. Same family as the `build-all.yml` finding: a later well-meant addition that wedges the repository.
- **Web and CodeQL are not in the payload.** `Web scope` + `TypeScript type-check + Vitest + Vite build`, and the seven `Analyze (…)` contexts from `codeql.yml`, all report on every PR and are equally requirable — add them as further `{ "context": …, "integration_id": 15368 }` entries. Left out because OI-SWCI-04 asks about the *firmware* checks and adding more surface is a separate decision.

#### 6.7.9 Rollback

If a docs-only PR shows the firmware checks as *"Expected — waiting for status"* rather than satisfied, §6.7.5's assumption does not hold.

**Prefer the captured `before.json` over the payload below.** Reconstructing a pre-state at rollback time is how a rollback quietly becomes a second change; the file captured in §6.7.8's pre-flight is the authoritative record:

```bash
jq '{name,target,enforcement,conditions,bypass_actors,rules}' before.json \
  | gh api --method PUT repos/stevehickman/NeuroPulse/rulesets/16412379 --input -
```

The literal payload, for the case where `before.json` was not captured:

```bash
gh api --method PUT repos/stevehickman/NeuroPulse/rulesets/16412379 --input - <<'JSON'
{
  "name": "Safety",
  "target": "branch",
  "enforcement": "active",
  "conditions": { "ref_name": { "include": ["~DEFAULT_BRANCH"], "exclude": [] } },
  "bypass_actors": [
    { "actor_id": 5, "actor_type": "RepositoryRole", "bypass_mode": "always" }
  ],
  "rules": [ { "type": "deletion" }, { "type": "non_fast_forward" } ]
}
JSON
```

The fallback is then to require only the checks that never skip: `Class B scope`, `Class C scope`, `Web scope`, and the `codeql.yml` contexts.

#### 6.7.10 Two findings raised, not fixed

**A direct push to `main` is currently permitted and bypasses every check.** The `Safety` ruleset contains `deletion` and `non_fast_forward` and no `pull_request` rule. `non_fast_forward` blocks force-pushes; it does **not** block an ordinary fast-forward push. So requiring status checks constrains the PR path while leaving the direct-push path wide open — and status checks are only ever evaluated on PRs. Verified 2026-08-10: `GET /repos/…/rules/branches/main` returns exactly those two rule types. Raised as **OI-SWCI-22**; not changed here, because adding a `pull_request` rule changes how everyone commits and is Steve's decision, not a side effect of a CI phase.

**`build-all.yml` can never be a required check** — recorded in §6.7.7 next to the required-checks list, and raised as **OI-SWCI-23** so it has an ID to cite when someone proposes adding it.

#### 6.7.11 What this phase did not do

It did not make any check required, and nothing here prevents a merge — that is §6.7.8, and it is Steve's to run under OI-SWCI-04, which remains open. It did not modify branch protection or any ruleset, and made no mutating API call of any kind: every `gh api` call in this phase was a plain `GET`, with no `--method`, no `--input`, and no `-f`/`-F` field flags (any of which silently promote `gh api` to `POST`). Independently corroborated by the ruleset's own `updated_at`, still `2026-05-14T11:50:38.699-07:00` after the phase — untouched since the day it was created. It did not touch any `permissions:` block, or change `NP_CLASS_B_TEST_COUNT` (21) or `NP_SAFETY_TEST_COUNT` (6), or alter `build-all.yml`. It did not add a third-party action. It did not change what any workflow builds — only when the jobs are allowed to skip. It did not touch Defect E, the SW-01 platform layer (OI-SWCI-17, phase 7), the MCUX SDK (OI-SWCI-20) or the missing SW-02 application target (OI-SWCI-21, phase 8). And it did not convert the five remaining `paths:`-filtered workflows, which stay non-requirable by omission rather than by decision.

## 7. Open items

| ID | Item | Owner | Blocking |
|----|------|-------|----------|
| ~~OI-SWCI-01~~ | ~~Vendored or fetched SDKs?~~ **CLOSED 2026-08-08 — vendored in all cases.** See §9 | Quality / Firmware | ~~Phases 4, 5~~ |
| ~~OI-SWCI-02~~ | ~~Restated 2026-08-08. The remaining work is a fix in *two* places: the linker script **and** the duplicate constant at `np_main.c:220`~~ **CLOSED 2026-08-09 — fixed as one place, not two.** The linker script derives the reservation from `LENGTH(OCRAM) - _app_load_offset - _stack_size` and exports it; `np_app_image.c` reads the symbol; `np_main.c` computes neither the limit nor the load address. Neither `448K` nor `440K` appears in the build. Two link-time `ASSERT`s and a host test that parses the shipping linker script make divergence fail CI. See §4.3 and §6.3 | Firmware | ~~Phase 2~~ |
| ~~OI-SWCI-03~~ | ~~Own workflow or matrix leg for the Class C safety MCU?~~ **CLOSED 2026-08-08 — its own workflow, `safety-mcu-ci.yml`.** See §5 | Quality | ~~Phase 4~~ |
| OI-SWCI-04 | Required-check policy: all PRs, or `main` only? Branch protection is an admin setting outside the repository. **Still open — this is what phase 6 is waiting on.** The mechanical blocker is closed as of 2026-08-10: every firmware check now reports on every PR, so the checks are *requirable* without deadlocking docs-only PRs (§6.7). The ready-to-run payload is §6.7.8 and the rollback is §6.7.9; read §6.7.5 first, which records the one assumption that could not be verified from inside the repository | Steve | Phase 6 |
| OI-SWCI-22 | **Raised 2026-08-10 during phase 6 (§6.7.10). A direct push to `main` is currently permitted and bypasses every status check.** The `Safety` ruleset (`16412379`) contains exactly `deletion` and `non_fast_forward`, and there is no classic branch-protection object (`GET /branches/main/protection` → 404). `non_fast_forward` blocks force-pushes but **not** ordinary fast-forward pushes, and no `pull_request` rule exists. Status checks are only ever evaluated on pull requests, so requiring them (OI-SWCI-04) constrains the PR path while leaving the direct-push path entirely open — a regression could land on `main` without any check having run. The decision is whether to add a `pull_request` rule (with what review count, and which bypass actors), which changes how everyone commits and is not a side effect a CI phase should apply. Deliberately not changed by phase 6 | Steve | — |
| OI-SWCI-23 | **Raised 2026-08-10 during phase 6 (§6.7.7). `build-all.yml` must never become a required check.** It is `schedule` + `workflow_dispatch` only by design (§5.5) and has no `pull_request` trigger, so it never reports on any PR; requiring it would block every PR unconditionally and permanently. Its job names also carry counts (`CMake host tests (27 targets, unfiltered)`), which is harmless only because it is not requirable. Recorded as an open item purely so there is an ID to cite when someone proposes adding it "for completeness" | Quality | — |
| OI-SWCI-24 | **Raised 2026-08-10 during phase 6.** Header arithmetic in `safety-mcu-ci.yml` is stale: it says "6 of the repo's 25" and "The remaining 19 belong to firmware-cross-build.yml. 6 + 19 = 25", while the live partition is 6 + 21 = 27 (`build-all.yml`'s own job is named `CMake host tests (27 targets, unfiltered)`, and §8 records 6 + 21 = 27). `firmware-cross-build.yml` has the same drift in the other direction — its header says "21 of the repo's 27" in one place and "6 + 20 = 26" in another. Phases 1 and 2 each added a Class B target and the prose counts were not swept. No check is affected — the enforced counts are the `NP_*_TEST_COUNT` env vars, which are correct — so this is a documentation defect, not a coverage one. Related to OI-SWCI-13 (the other stale-reference sweep) and naturally worked with it | Quality | — |
| OI-SWCI-26 | **Raised 2026-08-10 during phase 6 (§6.7.7).** Once OI-SWCI-04 is applied, seven job `name:` strings become required-check contexts, and a required context is an exact string match whose mismatch produces **no error anywhere** — the check simply never satisfies and the PR waits forever. Phase 6 mitigated this with an in-file `⚠` comment on each of the seven `name:` lines, which is a convention, not a guard. The real fix is a checked-in manifest of the exact context strings, diffed against the parsed workflow YAML in CI, so a phase-7 rename fails a check instead of silently un-gating the branch. Note the same hazard has a second trigger: adding `strategy: matrix:` to any of these jobs renames its check to `name (value)`. Same shape as OI-SWCI-08 and OI-SWCI-14 — a hand-maintained correspondence that nothing verifies — and naturally worked with them | Quality | — |
| OI-SWCI-25 | **Raised 2026-08-10 during phase 6.** `web-ci.yml`'s relevance list does not contain `.github/workflows/web-ci.yml`, so a change to that workflow does not re-run it — observed on PR #261, where a commit touching only the three converted workflow files left `TypeScript type-check + Vitest + Vite build` correctly `skipped`. This is faithful to the retired `paths:` list, which never self-referenced either, and phase 6 preserved it rather than silently widening scope. `firmware-cross-build.yml` and `safety-mcu-ci.yml` both DO list themselves, so the inconsistency is real. Decide whether every workflow should list its own file — the argument for is that a workflow edit is exactly the change most likely to break the workflow | Quality | — |
| OI-SWCI-05 | `firmware/hrv_biofeedback` has no test target at all, only a static library. Whether it warrants host tests alongside cross-compile coverage is a separate question this plan does not answer | Firmware | — |
| ~~OI-SWCI-06~~ | ~~For the safety MCU: vendor CMSIS device headers only or the full ST HAL drivers?~~ **CLOSED 2026-08-09 — device headers only.** Two components, eight header files, no translation unit: ARM CMSIS-Core(M) 5.6.0 (CMSIS_5 `5.9.0`) and ST CMSIS-Device STM32G0 `v1.4.5`, both Apache-2.0, both with `VERSION` SOUP records. **The HAL and LL drivers are not vendored** — SW-01 makes zero `HAL_*` and zero `LL_*` calls (word-boundary matched), so the HAL would add substantive third-party logic to Class C SOUP surface for no benefit; `USE_HAL_DRIVER` removed accordingly. The §7.1.2 anomaly evaluation the Class C classification demands is recorded as NP-SOUP-CMSIS-001 Rev A, and its cleanest finding is a direct consequence of this choice: essentially the whole published anomaly surface of CMSIS_5 lies in components that were not vendored. See §4.1 and §6.5 | Quality / Firmware | ~~Phase 4~~ |
| OI-SWCI-17 | **Defect E (new, §4.4).** SW-01 has no platform layer: 24 distinct first-party `np_hal_*` symbols (GPIO, timebase, clock, SPI slave, ADC, TIM2 capture, impedance, OTP) are declared `extern` and defined **only as test doubles inside the six Class C host-test files**, so the target image has never linked. Independent of Defects A/B/C/D and not fixable by any vendoring — the `np_` prefix is first-party, and the ST HAL would not define one of them. This is Class C register-driver work for the software that owns every stimulation enable GPIO; it needs design review and bench validation, not a speculative implementation written to turn a CI leg green. Blocks the *link*, not the compile — which is why phase 4 gates compilation and phase 7 gates linking | Firmware / Safety SW | Phase 7 |
| OI-SWCI-18 | **Raised 2026-08-09 during phase 4.** The six Class C host tests each define their own copies of the `np_hal_*` doubles (`np_hal_get_tick_ms` appears in four files independently). Once OI-SWCI-17 lands a real platform layer, there is no mechanism ensuring the doubles still match the production signatures — a drifted double compiles and passes while the shipped code does something else. The natural fix is a single shared test-double TU compiled into all six targets, in the same spirit as `np_hub_enable_mirror.c`, which exists precisely so a "must match" contract is checked rather than stated | Firmware | — |
| OI-SWCI-19 | **Raised 2026-08-09 during phase 4.** `firmware/safety_mcu/startup/startup_stm32g071xx.s` states in its header comment that `Reset_Handler` will "copy `.data`, zero `.bss` … then call SystemInit and main". It does not call `SystemInit` — it branches straight to `main`. The comment is wrong and the code is the record; this is what determined that ST's `system_stm32g0xx.c` need not be vendored. Left as-is rather than silently "corrected" in either direction, because the two fixes are materially different decisions: deleting four words, or adding a call to clock-init code that does not exist yet (clock bring-up is `np_hal_clock_init`, OI-SWCI-17). Flagged so it is resolved deliberately | Firmware | — |
| ~~OI-SWCI-07~~ | ~~Move the six safety-MCU host-test targets into `safety-mcu-ci.yml`?~~ **CLOSED 2026-08-08 — yes, forced by the §5.0.1 ordering decision.** `needs:` only works within one workflow, so each workflow must be self-contained | Quality / Firmware | ~~—~~ |
| OI-SWCI-08 | Build a guard that fails CI when a module in the CMake build graph is missing from the corresponding workflow's `paths:` list, making enumerated-path drift self-detecting rather than audit-dependent. `scripts/check-section-refs.ts` is the in-repo precedent for this shape of guard | Firmware | — |
| ~~OI-SWCI-09~~ | ~~Sequence the absorption and retirement of `firmware-host-tests.yml`~~ **CLOSED 2026-08-08 — absorbed and retired in Phase 0.** 25 targets partitioned 6 + 19, union verified identical to the original list, per-workflow count assertions plus a weekly partition guard added so the split is self-detecting. See §6.1 | Firmware | ~~Phase 0~~ |
| ~~OI-SWCI-12~~ | ~~**Defect D (new).** `firmware/hub_control/modules/np_mod_pbm.c:112` calls `np_pbm1064_dose_load_cal`, which is not declared~~ **CLOSED 2026-08-09 — fixed as a per-zone loop, not as a rename.** `np_mod_pbm.c` now calls `np_pbm1064_dose_load_cal_stub()` once per zone with one wavelength row per call, copying the pattern at `np_pbm1064_session.c:271-273`. The suggested rename is recorded as rejected in §4.6: the shapes differ, `-Werror` rejects it as an incompatible pointer type, and behind a cast it would populate row 0 only and leave the remaining zones with zeroed dose calibration — a wrong J/cm² rather than a build error. `s_cal`'s bound moved to `NP_HUB_ZONE_SLOT_COUNT` in the same change, per the explicit prohibition at `np_pbm1064_config.h:147-154`. Measured: 1 error → **0**, `rc=1` → **`rc=0`**, 86 of 86 targets. See §4.6 and §6.6 | Firmware | ~~Phase 5~~ |
| OI-SWCI-20 | **Raised 2026-08-09 during phase 5, decoupled from it.** `NP_PLATFORM_INCLUDE_DIRS` in `firmware/CMakeLists.txt` is an empty MCUX stub, and the `# ← append MCUX SDK path` marker at line 132 stands. Rev A–F carried this as phase 5's stated action, which was wrong on measurement: the cross-build has **zero** missing-header errors because the platform layer is stubbed rather than called into, and Defect D was the only error in the tree. Integrating the MCUX SDK for MIMXRT1062 is real device-bring-up work with §9.3 SOUP obligations attached (vendored subset, `VERSION` record, NP-SW-001 entry, document-register entry) — it is not build-gating and no phase is blocked on it. Naturally worked together with OI-SWCI-21, which needs it | Firmware / Quality | Phase 8 |
| OI-SWCI-21 | **Raised 2026-08-09 during phase 5 (§4.7).** There is **no SW-02 application executable target** in the repository. The cross-build archives 13 static libraries and links exactly one ELF, `np_bootloader`, which the bootloader leg has gated since phase 3. `np_hub_control` is `add_library(… STATIC …)` and its CMakeLists names a consumer, `np_application`, that does not exist; line 182 says the same thing from the other side ("the MCUX SDK HAL is still provided by the parent application target"). **Consequence: "zero unresolved symbols" on the main-firmware leg is true and vacuous for SW-02** — no SW-02 link runs, so the first-party HAL stubs (OI-PBM-HAL-01..03, and the OI-ZA / OI-HRV / OI-PBM / OI-CVNS / OI-ANON-AES families) can never be diagnosed by it. This is the same hazard Defect E (OI-SWCI-17) records for SW-01; it is invisible here only because SW-01 has an executable target and SW-02 does not. Not a CI item to fix — an application target needs the platform layer and OI-SWCI-20 — but until it exists, no check in this plan can verify that SW-02 links. §6's phase-5 row, §8's SW-02 row and both promoted legs' comments are worded as *compiles* accordingly. **Bound to phase 8** so the gap has a place to close rather than sitting behind a green check indefinitely — an open item with no phase is where this kind of finding institutionalises. Distinct from phase 7, which is SW-01 | Firmware | Phase 8 |
| OI-SWCI-14 | Strengthen the host-test absorption guards beyond counts: a checked-in ctest name manifest (catches rename/substitution, which 25 = 6 + 19 cannot) and a per-target case-count or gcov floor in `build-all.yml` (catches intra-target erosion). Also widen `firmware/cmake/**` back to a directory glob if a third toolchain file is ever added. See §6.1.1 | Firmware | — |
| OI-SWCI-13 | Sweep the five documents still citing the retired `firmware-host-tests.yml` (`np_sw_001.md`, `np_dhf_001.md`, `status/pending-decisions.md`, `status/document-register.md`, `status/completed-decisions.md`). The section-ref guard does not validate workflow filenames, so nothing fails on them | Quality | — |
| OI-SWCI-15 | **Raised 2026-08-09 during phase 1 review; a firmware finding, not a CI one.** `load_and_jump()` verifies the image in its eMMC bank, then copies it to OCRAM, then jumps — so the signature check covers the *source*, not the copy that actually executes. A bit flip, a truncated copy, or a wrong length yields corrupt code at an entry point the boot record says was verified. Standard practice is to re-hash (or at minimum CRC) the staged image after the copy and before the jump. While in that path, confirm D-cache clean / I-cache invalidate happens after staging — the classic omission in exactly this sequence. Out of scope for phase 1 (which only supplies the C runtime); it touches the same function as the phase-2 OCRAM fix, so the two are naturally worked together | Firmware / Safety SW | — |
| OI-SWCI-16 | **Raised 2026-08-09.** If `memset` is ever used to zeroise key or signature material in the bootloader, it survives dead-store elimination today only because it is an opaque cross-TU call. Enabling LTO would make those stores removable. Audit the `np_signature.c` scrub sites and give them an explicit volatile-based zeroiser rather than depending on that accident | Firmware / Security | — |
| OI-SWCI-10 | How should a red `build-all` be surfaced? It is not a PR check and blocks nothing, so a status badge nobody reads is not sufficient — it needs an out-of-band notification | Steve | Phase 0 |
| OI-SWCI-11 | Per-PR ctest granularity: should a change to one module run only that module's ctest targets, or is the full 25-target host suite cheap enough that selection adds drift risk for no gain? The §5.0 principle argues for selection; the suite's runtime may argue against. Measure before deciding | Firmware | — |

## 8. Traceability

| Requirement | Source | Verified by |
|-------------|--------|-------------|
| SW-02 Class B firmware **compiles** for i.MX RT1062 | NP-SW-001 Rev C | `firmware-cross-build.yml` — `main-firmware` job, **gating since phase 5 (§6.6)**. Covers every SW-02 translation unit under `-Wall -Wextra -Werror`; 86 of 86 targets, 0 errors, 0 warnings. Read this row as written: it is a compile guarantee, **not** an assertion that an SW-02 image exists |
| SW-02 Class B firmware **links** into an image | NP-SW-001 Rev C | **Not verified, and not verifiable today — there is no SW-02 application executable target (§4.7, OI-SWCI-21).** The cross-build archives 13 static libraries and links one ELF, `np_bootloader`, covered by its own row below. Distinct from the SW-01 case: SW-01 has a target and fails to link (Defect E); SW-02 has no target, so nothing links and nothing can be unresolved |
| SW-01 Class C firmware **compiles** for STM32G071 | NP-SW-001 Rev C | `safety-mcu-ci.yml` — `cross-build` job, target `np_safety_mcu_objs`, **gating since phase 4 (§6.5)**. Covers all 10 Class C translation units under `-Wall -Wextra -Werror`. Read this row as written: it is a compile guarantee, **not** an assertion that a linkable image exists |
| SW-01 Class C firmware **links** into an image | NP-SW-001 Rev C | **Not verified — blocked on Defect E (§4.4, OI-SWCI-17).** 24 first-party `np_hal_*` platform symbols are undefined, so no `np_safety_mcu` image has ever been produced. Phase 7 |
| Vendored CMSIS is byte-exact from its named upstream tags | §9.3 obligation 2 | `firmware/vendor/cmsis_core/VERSION` and `firmware/vendor/cmsis_device_g0/VERSION` — per-file SHA-256, verified against an independent second download by `cmp` |
| Class C SOUP anomaly lists evaluated for hazard contribution | IEC 62304 §7.1.2; §9.4 | **NP-SOUP-CMSIS-001 Rev A** — method, per-item assessment, and stated residuals; bound to the pinned tags and voided by moving either |
| `GPIOA`/`GPIOB` resolve to the correct STM32G071 addresses | §4.1, phase 4 | `objdump` on `np_gpio_mgr.c.obj` — `0x50000000`/`0x50000400` (§6.5). A compile alone proves resolution, not correctness |
| Bootloader fits its OCRAM allocation | `bootloader_imxrt1062.ld` ASSERT | `firmware-cross-build.yml` — bootloader leg, **gating since phase 3 (§6.4)**. The `ASSERT` fails the link, the link failure fails the job, and the job now fails the run |
| Host-native logic verified (Class B, 21 targets) | NP-SW-001 Rev C | `firmware-cross-build.yml` — `host-tests` job |
| Host-native logic verified (Class C, 6 targets) | NP-SW-001 Rev C | `safety-mcu-ci.yml` — `host-tests` job |
| Host-test partition remains complete (6 + 21 = 27) | OI-SWCI-09 | `build-all.yml` — `host-tests-all` partition guard, weekly |
| Bootloader supplies its own freestanding `memcpy`/`memset` | §4.2, phase 1 | `np_bootloader_mem_tests`; `objdump` body check in §4.2 |
| Application staging area is sized by the linker, not by C | §4.3, phase 2 | `np_bootloader_app_image_tests` (parses the linker script); `nm`/`objdump` checks in §6.3 |
| OCRAM is tiled exactly by bootloader + staging + stack | §4.3, phase 2 | `bootloader_imxrt1062.ld` `ASSERT`s (`_app_staging_start`, `_app_staging_end`) — link-time |

## 9. Vendoring decision (closes OI-SWCI-01)

**Decision (2026-08-08, Steve Hickman): vendor SDKs in-tree in all cases.** No build-time fetching of MCUX SDK, STM32G0 CMSIS/HAL, or any future third-party SDK.

### 9.1 Rationale

A build that reaches the network is not reproducible, and a medical-device build that reaches the network has a supply-chain surface that has to be assessed and re-assessed. Vendoring makes the exact bytes that went into a release part of the design record, reviewable in a diff and recoverable from the repository alone. It is also what the repo already does twice over, so this is consistency rather than novelty.

### 9.2 Existing precedents to follow

| Component | Location | SOUP record | Class |
|-----------|----------|-------------|-------|
| FreeRTOS-Kernel V11.3.0 | `firmware/vendor/freertos/` | `VERSION` + `README-NEURONE.md`; NP-SW-001 §9.4 | SW-02 Class B |
| Monocypher 4.0.2 | `firmware/crypto/vendor/monocypher/` | `VERSION` (IEC 62304 §8.1.2 header, per-file provenance) | Class B + Class C |
| **ARM CMSIS-Core(M) 5.6.0** (CMSIS_5 `5.9.0`) | `firmware/vendor/cmsis_core/` | `VERSION` + `README-NEURONE.md`; NP-SW-001 §9.4; **anomaly review NP-SOUP-CMSIS-001 §3** | **SW-01 Class C** |
| **ST CMSIS-Device STM32G0 `v1.4.5`** | `firmware/vendor/cmsis_device_g0/` | `VERSION` + `README-NEURONE.md`; NP-SW-001 §9.4; **anomaly review NP-SOUP-CMSIS-001 §4** | **SW-01 Class C** |

The bottom two rows were added by phase 4 (2026-08-09) and are the first components vendored under this decision. They follow the FreeRTOS template — byte-exact subset of a named tag, plus an explicit "intentionally NOT vendored" list — and add two things the Class C bar required that neither precedent carries: **per-file SHA-256**, so "byte-exact" is checkable offline rather than asserted, and a **separate versioned anomaly-review record** (§9.4 obligation 5 below).

The FreeRTOS record is the stronger template because it does two things the new SDKs also need: it declares a **byte-exact subset** of a named upstream tag, and it declares explicitly what was **intentionally not vendored** and why. The second half is what stops a later reader assuming an omission is an oversight.

### 9.3 Obligations this creates

Vendoring is not just copying files in. Each SDK brought in under this decision needs:

1. **A `VERSION` SOUP record** in the vendored directory, following the Monocypher header format — component, version, upstream URL, exact tag, license, SW item, IEC 62304 class, vendoring date.
2. **A byte-exact subset from a named upstream tag**, with the "intentionally NOT vendored" list spelled out, per the FreeRTOS pattern. Vendor only what the build actually needs — the full MCUX SDK for MIMXRT1062 is large, and pulling all of it in would bloat the repository and widen the SOUP surface for no benefit.
3. **A SOUP entry in NP-SW-001**, alongside the existing §9.4 FreeRTOS entry.
4. **A document-register entry** in `docs/status/document-register.md`.
5. **For Class C components only: a recorded IEC 62304 §7.1.2 anomaly evaluation** — added 2026-08-09 when phase 4 made this concrete. §9.4 below already flagged the obligation; what phase 4 establishes is the *form*. It is a separate, versioned, attributable document (NP-SOUP-CMSIS-001 is the first), not a line in the `VERSION` file, because it must state its method and its residual limitations to be worth anything, and because it is bound to the pinned version rather than to the component — moving a tag voids the evaluation and requires a new revision. A `VERSION` record without a resolvable pointer to a current evaluation is an incomplete Class C SOUP record.

### 9.4 Consequence worth flagging: the STM32G0 CMSIS/HAL is Class C SOUP

The two existing vendored components are Class B (FreeRTOS) and Class B + C (Monocypher, already handled as such). **The STM32G0 CMSIS/HAL headers feed the safety MCU, which is SW-01 Class C** — the software that owns every stimulation enable GPIO.

SOUP in Class C software carries a higher bar under IEC 62304 than the Class B case: the anomaly-list evaluation under §7.1.2 applies, and the published anomaly list for the SDK version has to be reviewed for defects that could contribute to a hazardous situation, with the review recorded. That is a real activity with a real cost, and it is a consequence of this decision rather than a consequence of the CI work that surfaced it.

Two things follow. First, vendor the **narrowest possible subset** for the safety MCU — the register/bit definitions `np_safety_config.h` actually needs, not the whole HAL, since every vendored file is SOUP surface that has to be justified. Second, whether to vendor CMSIS device headers only (register definitions, essentially a hardware description) versus the full ST HAL drivers (substantive third-party logic) is a genuine engineering choice with different Class C consequences, and should be decided deliberately rather than by whichever is easier to copy. This is raised as OI-SWCI-06.

> **Resolved at phase 4 (2026-08-09) — OI-SWCI-06 closed: device headers only.** Both instructions above were followed and both paid off, which is worth recording because the cost of the Class C bar was the argument for taking them seriously.
>
> The vendored surface is **eight header files across two components**, contributing no translation unit to the image. The ST HAL and LL drivers are excluded on evidence rather than preference: SW-01 makes zero `HAL_*` and zero `LL_*` calls under word-boundary matching. `USE_HAL_DRIVER` was removed from the build in the same change, having turned from inert into a hard failure the moment a device header existed in-tree.
>
> The anomaly evaluation is **NP-SOUP-CMSIS-001 Rev A**, and its principal finding vindicates the narrowness instruction directly: CMSIS_5 carried 188 open issues at review, and essentially all of them sit in DSP, NN, RTOS2, DAP, SVDConv and Pack — components that were not vendored. The anomaly surface follows the subset, not the repository. Had the full HAL and driver bodies been taken "because they were easier to copy", that evaluation would have been a substantially larger and less conclusive piece of work.
>
> One thing this section did not anticipate: vendoring closed Defect A but did **not** produce a linkable image, because Defect E (§4.4) leaves the SW-01 platform layer unwritten. The SOUP decision was never the only thing between the Class C firmware and a working build — see the corrected phase-4 criterion in §6.

### 9.5 What this unblocks

Phases 4 and 5 in §6 are no longer blocked on a decision — they are now ordinary work items, gated only on the vendoring being performed to the standard in §9.3. Phase 0 (the workflow itself) never depended on this and can proceed independently.
