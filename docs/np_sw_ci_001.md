# Firmware Cross-Compile Continuous Integration Plan

**Project:** NeurOne  
**Document:** NP-SW-CI-001  
**Revision:** A  
**Date:** 2026-08-06  
**Status:** DRAFT  
**Effective Date:** —  
**Author:** Steve Hickman (CEO, interim Quality authority)  
**Approved By:** — (DRAFT, not approved)  
**References:** NP-SW-001 Rev C (Software Development Plan), NP-FW-EMMC-001 Rev A (bootloader boot sequence), NP-FW-CVNS-001 Rev A (safety MCU interlock), `.github/workflows/firmware-host-tests.yml`  
**Related Issues:** PR #250 (workflow least-privilege pass — where this gap was found)  
**Gate:** —  
**IEC 62304 Class:** SW-01 Class C (safety MCU), SW-02 Class B (main processor)  
**Supersedes:** None  
**Change Summary:** Initial revision. Establishes that no CI verifies the firmware cross-compiles for its target, records three measured blocking defects, and specifies a phased cross-compile workflow.  
**Review Cadence:** On each phase transition in §6, and on any change to `firmware/cmake/*.cmake`

---

> **⚠ DRAFT — records a verification gap and three measured defects. Not a baselined verification plan.** The workflow in §5 is specified but not implemented, and cannot pass today: §4 documents three independent blocking defects found by running the cross-compile locally. Defect C (bootloader OCRAM over-subscription) needs a product decision on maximum OTA image size before it can be fixed. This document locates and measures; it does not fix.

## 1. Purpose and scope

This plan covers **build verification for the cross-compiled firmware targets** — that the NeurOne firmware compiles and links for the hardware it ships on. It is a software-verification activity under NP-SW-001.

In scope: the i.MX RT1062 main-processor firmware (SW-02 Class B), the STM32G071 safety MCU firmware (SW-01 Class C), and the dual-bank OTA bootloader.

Out of scope: flashing, on-target execution, and hardware-in-the-loop testing. This plan verifies that the firmware **builds**, nothing more. Host-native unit testing is already covered by `firmware-host-tests.yml` and is not restated here.

## 2. The gap

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

### 4.1 Defect A — safety MCU has no CMSIS device headers

```
firmware/safety_mcu/include/np_safety_config.h:49:33:
  error: 'GPIOA' undeclared (first use in this function)
firmware/safety_mcu/include/np_safety_config.h:52:33:
  error: 'GPIOB' undeclared (first use in this function)
```

`np_safety_config.h` maps the stimulation enable lines to CMSIS device symbols (`GPIOA`, `GPIOB`) that nothing in-tree defines. The STM32G0 CMSIS/HAL headers are not vendored. The Class C firmware therefore does not compile for its target in a clean checkout.

### 4.2 Defect B — bootloader links with no C runtime

`firmware/bootloader/CMakeLists.txt` sets `-nostdlib -nostartfiles -nodefaultlibs`. GCC still emits implicit `memcpy` and `memset` calls for structure copies and array initialisation, and nothing provides them:

```
np_dfu.c:(.text.np_dfu_enter+0xb0): undefined reference to `memset'
  → 19 unresolved memcpy, 7 unresolved memset
```

Note this is consistent with the design intent recorded for `np_signature.c` in the document register — the bootloader deliberately avoids `np_crypto` because `-nostdlib/-nodefaultlibs` makes Monocypher unavailable. The freestanding choice is intentional; providing the handful of primitives GCC assumes is the missing piece. Options: link `-lgcc`, adopt `--specs=nano.specs` with libc, or implement freestanding `memcpy`/`memset` inside the bootloader (common practice, and consistent with the no-libc intent).

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

**This needs a product decision, not a build fix.** Reducing staging to 440 KiB resolves it arithmetically but reduces the maximum OTA application image by 8 KiB. Whether 448 KiB is a hard requirement is a question for the OTA design (NP-FW-EMMC-001), not for CI.

## 5. Specified workflow

A new file, `.github/workflows/firmware-cross-build.yml`, rather than a job added to `firmware-host-tests.yml`. Three reasons: it needs a wider trigger surface (all of `firmware/**`, including `bootloader/`, `hrv_biofeedback/`, `safety_mcu/` and `cmake/`); it needs a toolchain install the host-test job does not; and keeping them separate means a cross-build failure does not obscure host-test results.

```yaml
name: Firmware Cross-Compile (ARM)

on:
  push:
    paths: ['firmware/**', '.github/workflows/firmware-cross-build.yml']
  pull_request:
    paths: ['firmware/**', '.github/workflows/firmware-cross-build.yml']
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
          - name: Safety MCU (STM32G071, Class C)
            src: firmware/safety_mcu
            toolchain: firmware/cmake/stm32g071.cmake
            experimental: true
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

### 5.1 Toolchain version pinning

`firmware/cmake/arm-none-eabi.cmake` already enforces a minimum of 12.3 and queries `-dumpfullversion` rather than `-dumpversion` — correct, since plain `-dumpversion` returns only the major number on GCC 7 and later, which would let "12" falsely satisfy a "12.3" gate. The action is pinned to a specific release rather than `latest` so that a toolchain change is a reviewable commit and not an unexplained red build. `13.2.Rel1` is the conservative pin; 14.2.1 was used for the measurements in §4 and also satisfies the gate.

### 5.2 Size gating

No separate size-checking step. The bootloader already links with `-Wl,--print-memory-usage`, and its linker script carries `ASSERT((_bootloader_end - ORIGIN(OCRAM)) <= _bl_max_size, ...)`. A region overflow fails the link and therefore the job — as Defect C demonstrates. **The link failing is the size test.** A second size-parsing step could disagree with the linker, and the linker is authoritative.

## 6. Phased rollout

A permanently red required check trains reviewers to ignore it, which is worse than no check. So the workflow lands reporting-only and is promoted per leg as each defect closes.

| Phase | Action | Exit criterion |
|-------|--------|----------------|
| 0 | Land the workflow with `continue-on-error: true` on all three legs | Job runs, logs the three known failures, blocks nothing |
| 1 | Fix Defect B (bootloader C runtime) | Bootloader link resolves `memcpy`/`memset` |
| 2 | Fix Defect C (OCRAM arithmetic) — requires the §4.3 product decision | Bootloader links; `--print-memory-usage` reports under 100% |
| 3 | Drop `experimental` on the bootloader leg | Bootloader leg blocking |
| 4 | Vendor STM32G0 CMSIS/HAL (Defect A) | Safety MCU compiles; drop its `experimental` |
| 5 | Populate `NP_PLATFORM_INCLUDE_DIRS` with the MCUX SDK path — currently an empty stub in `firmware/CMakeLists.txt` | Main firmware leg builds; drop its `experimental` |
| 6 | Add the three legs to branch protection as required checks | Cross-compile regressions become unmergeable |

Phases 4 and 5 both mean bringing a vendor SDK into the build, which is an SOUP decision with design-control consequences. FreeRTOS is the existing precedent — vendored in `firmware/vendor/freertos/` and recorded as Class B SOUP under NP-SW-001 — and MCUX SDK and STM32G0 CMSIS would follow that pattern if vendored.

## 7. Open items

| ID | Item | Owner | Blocking |
|----|------|-------|----------|
| OI-SWCI-01 | Vendored or fetched SDKs? FreeRTOS sets a vendoring precedent; fetching at build time is lighter but adds a network dependency and supply-chain surface to a medical-device build | Quality / Firmware | Phases 4, 5 |
| OI-SWCI-02 | Is 448 KiB app staging a hard requirement, or can it drop to 440 KiB to make room for the 8 KiB stack? Product constraint, not a build one | Firmware / Product | Phase 2 |
| OI-SWCI-03 | Should the Class C safety MCU get its own workflow rather than a matrix leg? Separate project, separate toolchain, different 62304 class — a dedicated workflow makes its status legible at a glance, which has audit value | Quality | Phase 4 |
| OI-SWCI-04 | Required-check policy: all PRs, or `main` only? Branch protection is an admin setting outside the repository | Steve | Phase 6 |
| OI-SWCI-05 | `firmware/hrv_biofeedback` has no test target at all, only a static library. Whether it warrants host tests alongside cross-compile coverage is a separate question this plan does not answer | Firmware | — |

## 8. Traceability

| Requirement | Source | Verified by |
|-------------|--------|-------------|
| SW-02 Class B firmware builds for i.MX RT1062 | NP-SW-001 Rev C | Matrix leg 3, phase 5 |
| SW-01 Class C firmware builds for STM32G071 | NP-SW-001 Rev C | Matrix leg 1, phase 4 |
| Bootloader fits its OCRAM allocation | `bootloader_imxrt1062.ld` ASSERT | Matrix leg 2, phase 3 |
| Host-native logic verified | NP-SW-001 Rev C | `firmware-host-tests.yml` (existing, unchanged) |
