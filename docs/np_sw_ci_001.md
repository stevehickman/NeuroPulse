# Firmware Cross-Compile Continuous Integration Plan

**Project:** NeurOne  
**Document:** NP-SW-CI-001  
**Revision:** B  
**Date:** 2026-08-08  
**Status:** DRAFT  
**Effective Date:** —  
**Author:** Steve Hickman (CEO, interim Quality authority)  
**Approved By:** — (DRAFT, not approved)  
**References:** NP-SW-001 Rev C (Software Development Plan), NP-FW-EMMC-001 Rev A (bootloader boot sequence), NP-FW-CVNS-001 Rev A (safety MCU interlock), `.github/workflows/firmware-host-tests.yml`  
**Related Issues:** PR #250 (workflow least-privilege pass — where this gap was found)  
**Gate:** —  
**IEC 62304 Class:** SW-01 Class C (safety MCU), SW-02 Class B (main processor)  
**Supersedes:** None  
**Change Summary:** Rev B (2026-08-08) — records five decisions and one investigation. (1) §9: all SDKs vendored in-tree, closing OI-SWCI-01 and unblocking phases 4–5; flags the STM32G0 CMSIS/HAL as Class C SOUP carrying the IEC 62304 §7.1.2 anomaly-list obligation, raising OI-SWCI-06. (2) §5: the Class C safety MCU gets its own workflow rather than a matrix leg, closing OI-SWCI-03. (3) §5.0: build and test only what the PR could have changed — `paths:` lists narrowed to real dependency sets, not `firmware/**`. (4) §5.5: `build-all.yml`, an unscoped scheduled backstop, without which per-PR scoping would let undeclared dependency edges and out-of-repo rot go undetected indefinitely. (5) §5.0.1: no cross-build unless the native build is green; because `needs:` works only within a workflow, this forces each workflow to be self-contained and closes OI-SWCI-07 (safety-MCU host tests move in), at the cost of absorbing and retiring `firmware-host-tests.yml` (OI-SWCI-09). Investigation: §4.3 now records that the 448 KiB OCRAM staging figure is derived arithmetic from the original bootloader commit rather than a requirement, that the correct value is 440 KiB, and that the same wrong constant in `np_main.c:220` is a latent stack-corruption bug rather than merely a link error — restating OI-SWCI-02. No change to Defects A or B. Rev A (2026-08-06) — initial revision: establishes that no CI verifies the firmware cross-compiles for its target, records three measured blocking defects, and specifies a phased cross-compile workflow.  
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

## 5. Specified workflows

**Two** new workflow files, not one. The Class C safety MCU gets its own workflow rather than a matrix leg — decision 2026-08-08, closing OI-SWCI-03.

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
    name: Cross-compile (STM32G071, Cortex-M0+)
    runs-on: ubuntu-latest
    continue-on-error: true      # ← Phase 0 only; drop at phase 4
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

      - name: Build
        run: cmake --build build/safety-mcu

      - name: Report image size
        if: success()
        run: find build/safety-mcu -name '*.elf' | xargs -r arm-none-eabi-size

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
| 0 | both | Land `firmware-cross-build.yml` (2 legs) and `safety-mcu-ci.yml` (1 job), all with `continue-on-error: true` | Jobs run, log the three known failures, block nothing |
| 1 | cross-build | Fix Defect B (bootloader C runtime) | Bootloader link resolves `memcpy`/`memset` |
| 2 | cross-build | Fix Defect C (OCRAM arithmetic) — see §4.3; the linker script **and** the duplicate constant in `np_main.c` | Bootloader links; `--print-memory-usage` under 100% |
| 3 | cross-build | Drop `continue-on-error` on the bootloader leg | Bootloader leg blocking |
| 4 | safety-mcu | Vendor STM32G0 CMSIS/HAL (Defect A) per §9 | Safety MCU compiles; drop its `continue-on-error` |
| 5 | cross-build | Populate `NP_PLATFORM_INCLUDE_DIRS` with the MCUX SDK path — currently an empty stub in `firmware/CMakeLists.txt` | Main firmware leg builds; drop its `continue-on-error` |
| 6 | both | Add all three checks to branch protection | Cross-compile regressions become unmergeable |

Phase 4 is independent of phases 1–3 and 5 and can be worked in parallel — giving the safety MCU its own workflow is what makes that separation clean.

Phases 4 and 5 both mean bringing a vendor SDK into the build, which is an SOUP decision with design-control consequences. That decision is now made — see §9.

## 7. Open items

| ID | Item | Owner | Blocking |
|----|------|-------|----------|
| ~~OI-SWCI-01~~ | ~~Vendored or fetched SDKs?~~ **CLOSED 2026-08-08 — vendored in all cases.** See §9 | Quality / Firmware | ~~Phases 4, 5~~ |
| OI-SWCI-02 | **Restated 2026-08-08.** Not a product decision — §4.3 established 448 KiB is derived arithmetic that omitted the 8 KiB stack, and the correct value is 440 KiB. The remaining work is a fix in *two* places: the linker script **and** the duplicate constant at `np_main.c:220`, where it is a latent stack-corruption bug. Fixing only the linker script leaves the runtime guard wrong | Firmware | Phase 2 |
| ~~OI-SWCI-03~~ | ~~Own workflow or matrix leg for the Class C safety MCU?~~ **CLOSED 2026-08-08 — its own workflow, `safety-mcu-ci.yml`.** See §5 | Quality | ~~Phase 4~~ |
| OI-SWCI-04 | Required-check policy: all PRs, or `main` only? Branch protection is an admin setting outside the repository | Steve | Phase 6 |
| OI-SWCI-05 | `firmware/hrv_biofeedback` has no test target at all, only a static library. Whether it warrants host tests alongside cross-compile coverage is a separate question this plan does not answer | Firmware | — |
| OI-SWCI-06 | For the safety MCU: vendor CMSIS device headers only (register/bit definitions — essentially a hardware description) or the full ST HAL drivers (substantive third-party logic)? Different Class C SOUP consequences under IEC 62304 §7.1.2. Opened by the §9 vendoring decision | Quality / Firmware | Phase 4 |
| ~~OI-SWCI-07~~ | ~~Move the six safety-MCU host-test targets into `safety-mcu-ci.yml`?~~ **CLOSED 2026-08-08 — yes, forced by the §5.0.1 ordering decision.** `needs:` only works within one workflow, so each workflow must be self-contained | Quality / Firmware | ~~—~~ |
| OI-SWCI-08 | Build a guard that fails CI when a module in the CMake build graph is missing from the corresponding workflow's `paths:` list, making enumerated-path drift self-detecting rather than audit-dependent. `scripts/check-section-refs.ts` is the in-repo precedent for this shape of guard | Firmware | — |
| OI-SWCI-09 | Sequence the absorption and retirement of `firmware-host-tests.yml` into the two self-contained workflows (§5.0.1) without losing coverage. It is currently green and working; its job name, `25 targets` count and header inventory must move with it. Highest-risk item in this plan — it is the only one that touches something already working | Firmware | Phase 0 |
| OI-SWCI-10 | How should a red `build-all` be surfaced? It is not a PR check and blocks nothing, so a status badge nobody reads is not sufficient — it needs an out-of-band notification | Steve | Phase 0 |
| OI-SWCI-11 | Per-PR ctest granularity: should a change to one module run only that module's ctest targets, or is the full 25-target host suite cheap enough that selection adds drift risk for no gain? The §5.0 principle argues for selection; the suite's runtime may argue against. Measure before deciding | Firmware | — |

## 8. Traceability

| Requirement | Source | Verified by |
|-------------|--------|-------------|
| SW-02 Class B firmware builds for i.MX RT1062 | NP-SW-001 Rev C | `firmware-cross-build.yml` — main firmware leg, phase 5 |
| SW-01 Class C firmware builds for STM32G071 | NP-SW-001 Rev C | `safety-mcu-ci.yml`, phase 4 |
| Bootloader fits its OCRAM allocation | `bootloader_imxrt1062.ld` ASSERT | `firmware-cross-build.yml` — bootloader leg, phase 3 |
| Host-native logic verified | NP-SW-001 Rev C | `firmware-host-tests.yml` (existing, unchanged) |

## 9. Vendoring decision (closes OI-SWCI-01)

**Decision (2026-08-08, Steve Hickman): vendor SDKs in-tree in all cases.** No build-time fetching of MCUX SDK, STM32G0 CMSIS/HAL, or any future third-party SDK.

### 9.1 Rationale

A build that reaches the network is not reproducible, and a medical-device build that reaches the network has a supply-chain surface that has to be assessed and re-assessed. Vendoring makes the exact bytes that went into a release part of the design record, reviewable in a diff and recoverable from the repository alone. It is also what the repo already does twice over, so this is consistency rather than novelty.

### 9.2 Existing precedents to follow

| Component | Location | SOUP record | Class |
|-----------|----------|-------------|-------|
| FreeRTOS-Kernel V11.3.0 | `firmware/vendor/freertos/` | `VERSION` + `README-NEURONE.md`; NP-SW-001 §9.4 | SW-02 Class B |
| Monocypher 4.0.2 | `firmware/crypto/vendor/monocypher/` | `VERSION` (IEC 62304 §8.1.2 header, per-file provenance) | Class B + Class C |

The FreeRTOS record is the stronger template because it does two things the new SDKs also need: it declares a **byte-exact subset** of a named upstream tag, and it declares explicitly what was **intentionally not vendored** and why. The second half is what stops a later reader assuming an omission is an oversight.

### 9.3 Obligations this creates

Vendoring is not just copying files in. Each SDK brought in under this decision needs:

1. **A `VERSION` SOUP record** in the vendored directory, following the Monocypher header format — component, version, upstream URL, exact tag, license, SW item, IEC 62304 class, vendoring date.
2. **A byte-exact subset from a named upstream tag**, with the "intentionally NOT vendored" list spelled out, per the FreeRTOS pattern. Vendor only what the build actually needs — the full MCUX SDK for MIMXRT1062 is large, and pulling all of it in would bloat the repository and widen the SOUP surface for no benefit.
3. **A SOUP entry in NP-SW-001**, alongside the existing §9.4 FreeRTOS entry.
4. **A document-register entry** in `docs/status/document-register.md`.

### 9.4 Consequence worth flagging: the STM32G0 CMSIS/HAL is Class C SOUP

The two existing vendored components are Class B (FreeRTOS) and Class B + C (Monocypher, already handled as such). **The STM32G0 CMSIS/HAL headers feed the safety MCU, which is SW-01 Class C** — the software that owns every stimulation enable GPIO.

SOUP in Class C software carries a higher bar under IEC 62304 than the Class B case: the anomaly-list evaluation under §7.1.2 applies, and the published anomaly list for the SDK version has to be reviewed for defects that could contribute to a hazardous situation, with the review recorded. That is a real activity with a real cost, and it is a consequence of this decision rather than a consequence of the CI work that surfaced it.

Two things follow. First, vendor the **narrowest possible subset** for the safety MCU — the register/bit definitions `np_safety_config.h` actually needs, not the whole HAL, since every vendored file is SOUP surface that has to be justified. Second, whether to vendor CMSIS device headers only (register definitions, essentially a hardware description) versus the full ST HAL drivers (substantive third-party logic) is a genuine engineering choice with different Class C consequences, and should be decided deliberately rather than by whichever is easier to copy. This is raised as OI-SWCI-06.

### 9.5 What this unblocks

Phases 4 and 5 in §6 are no longer blocked on a decision — they are now ordinary work items, gated only on the vendoring being performed to the standard in §9.3. Phase 0 (the workflow itself) never depended on this and can proceed independently.
