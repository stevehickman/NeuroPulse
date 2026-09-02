# IEC 62304 Software Development Plan

**Project:** NeurOne  
**Document:** NP-SW-001  
**Revision:** 4
**Date:** 2026-09-01  
**Status:** ACTIVE  
**Effective Date:** 2026-07-27  
**Author:** Quality Lead (interim: Steve Hickman, CEO)  
**Approved By:** Steve Hickman, CEO  
**References:** —  
**Related Issues:** GitHub Issue #33  
**Gate:** —  
**IEC 62304 Class:** SW-01 Class C, SW-02 Class B, SW-03 Class B  
**Applicable Standard:** IEC 62304:2006 + AMD1:2015 — Medical Device Software: Software Lifecycle Processes  
**Next Review:** 2027-05-13 or upon significant architecture change

---

**Rev C (2026-07-27):** Adds §5.2.1 ZONE_ID Detection Debounce Requirement (RISK-18) — states the 3-read/100ms/2-of-3-majority debounce requirement at the software-development-plan level, cross-referencing the existing implementations in NP-FW-ZA-001 and NP-FW-PBM1064-001. Closes the corresponding `docs/status/pending-decisions.md` open item. Rebased on top of Rev 2 (2026-07-22, SR-FAN-01…06 fan-health interlock, landed via PR #217) — no Rev 2 content changed by this revision.

---

## 1. Purpose

This Software Development Plan defines the software lifecycle processes, safety classifications, development requirements, and records for all NeurOne software components, in compliance with IEC 62304:2006+AMD1:2015 and 21 CFR §820.30.

---

## 2. Software System Overview

The NeurOne device contains three software items:

| Software item | ID | Platform | Language | Repo path |
|---|---|---|---|---|
| Safety MCU firmware | SW-01 | STM32G071 (Cortex-M0+, bare-metal) | C (C11) | `firmware/` (safety-critical modules) |
| Main processor firmware | SW-02 | NXP i.MX RT1062 (Cortex-M7, FreeRTOS) | C (C11) | `firmware/` (all non-safety-MCU modules) |
| iOS/Android application | SW-03 | iOS (Swift) / Android (Kotlin) | Swift / Kotlin | `app/` |

All three software items interact:
- SW-02 communicates with SW-01 via SPI heartbeat (200ms interval) and stimulation enable commands
- SW-02 communicates with SW-03 via USB-C (wired, primary) and BLE GATT (wireless)
- SW-03 signs session protocols; SW-01 and SW-02 reject unsigned protocols

---

## 3. Software Safety Classification

### 3.1 Classification rationale

Per IEC 62304 §4.3, each software item is classified based on the severity of harm that could result from software failure:

| Software item | Class | Rationale |
|---|---|---|
| **SW-01 — Safety MCU firmware** | **Class C** | Directly controls all stimulation enable GPIO lines. A software failure (incorrect enable, failure to disable, incorrect current calculation) could cause patient harm up to and including serious injury or death (cervical VNS cardiac arrhythmia, tDCS charge density overdose, PBM thermal burn). No independent hardware backstop for Safety MCU failure. |
| **SW-02 — Main processor firmware** | **Class B** | Orchestrates session protocols, EEG signal processing, and device management. Software failure could lead to incorrect session delivery; however, SW-01 provides an independent hardware backstop (SPI heartbeat watchdog, hardware GPIO ownership). Injury severity limited by SW-01 hardware controls. |
| **SW-03 — iOS/Android application** | **Class B** | Signs session protocols and manages UHDR/SHDR. A failure (unsigned protocol accepted by device — but firmware rejects; UHDR data loss) could cause harm through incorrect clinical decisions or loss of data, but SW-01 and SW-02 provide independent stimulation safety. |

### 3.2 Legacy of safety (Class B rationale for SW-02/SW-03)

SW-01 independently owns all stimulation GPIO. Even if SW-02 crashes, hangs, or generates a malicious command, SW-01:
- Stops all stimulation on SPI heartbeat timeout (1.5s → cutoff <50ms)
- Cannot be overridden by SW-02 to exceed hardware-enforced limits (40µC/cm², thermal throttle)
- Independently monitors cardiac rhythm for cervical VNS (T2)

This architecture provides the independence required to classify SW-02 and SW-03 as Class B rather than Class C.

### 3.3 Class-specific requirements summary

| Requirement | Class C (SW-01) | Class B (SW-02, SW-03) |
|---|---|---|
| Software development planning | Required | Required |
| Requirements analysis | Required — unit level | Required — system level |
| Architectural design | Required | Required |
| Detailed design | Required | Required (as needed) |
| Unit implementation | Required | Required |
| Unit verification (unit tests) | **Required — all units** | Required for safety-related units |
| Integration testing | Required | Required |
| System testing | Required | Required |
| Problem resolution | **Full SOUP tracking** | Required |
| Change management | Full change control | Change control |
| Configuration management | Required | Required |
| Traceability (req → design → test) | **Required — unit level** | Required — system level |

---

## 4. Software Development Lifecycle

### 4.1 Lifecycle model

NeurOne uses a **V-model** lifecycle for safety-critical firmware (SW-01) and an **iterative V-model** for SW-02 and SW-03:

```
SW-01 (Class C — strict V-model):

Requirements  ─────────────────────────────────►  System Test
    │                                                    ▲
    ▼                                                    │
Architecture  ───────────────────────────────►  Integration Test
    │                                                    ▲
    ▼                                                    │
Detailed Design ──────────────────────────────►  Unit Test
    │                                                    ▲
    └──────────────── Implementation ────────────────────┘

SW-02 / SW-03 (Class B — iterative V-model):
Sprint-based development with formal test gates at
each software version release
```

### 4.2 Development environment

| Item | SW-01 | SW-02 | SW-03 |
|---|---|---|---|
| Compiler | GNU Arm Embedded Toolchain (arm-none-eabi-gcc 12+) | GNU Arm Embedded Toolchain (arm-none-eabi-gcc 12+) | Xcode 15+ (iOS), Android Studio Hedgehog+ |
| RTOS | None (bare-metal) | FreeRTOS-Kernel V11.3.0 (LTS 202604.00), vendored `firmware/vendor/freertos/` | N/A |
| Version control | Git (stevehickman/NeurOne) | Git (stevehickman/NeurOne) | Git (stevehickman/NeurOne) |
| Build system | CMake 3.22+ | CMake 3.22+ | Xcode build / Gradle |
| Static analysis | (to be selected — Year 1) | (to be selected — Year 1) | SwiftLint / detekt |
| Unit test framework | Unity (C) | Unity (C) | XCTest / JUnit |

### 4.3 Coding standards

- SW-01 and SW-02: MISRA C:2012 (safety-critical subset) — mandatory for SW-01; advisory for SW-02 with documented deviations
- SW-03: Swift API Design Guidelines (iOS); Kotlin Coding Conventions (Android)
- No dynamic memory allocation in SW-01 (bare-metal safety MCU)
- SW-02 FreeRTOS tasks: fixed stack sizes, no heap allocation in real-time paths

---

## 5. Software Items and Modules

### 5.1 SW-01 — Safety MCU firmware modules (Class C)

Each module listed constitutes a **software unit** requiring individual unit verification:

| Module ID | Module name | File(s) | Safety function |
|---|---|---|---|
| SW01-M01 | Stimulation enable GPIO management | `np_gpio_enable.c/.h` | Owns all stimulation GPIO; enforces hardware interlock state machine |
| SW01-M02 | SPI heartbeat watchdog | `np_spi_watchdog.c/.h` | Monitors 200ms heartbeat from SW-02; 1.5s timeout → all-cutoff |
| SW01-M03 | Charge density monitor | `np_charge_density.c/.h` | Integrates delivered charge per electrode pair; aborts at 95% of 40µC/cm² |
| SW01-M04 | Thermal interlock | `np_thermal.c/.h` | Reads NTC ADC per zone; throttles current at 62°C junction; additionally reads the scalp-facing NTC (Path B1) and bounds scalp-facing surface ≤42°C under loss of forced convection — PBM duty derate to the natural-convection-safe ceiling (SR-FAN-01/03/04/06, NP-REQ-FANHEALTH-001) |
| SW01-M05 | Cervical VNS cardiac interlock | `np_cvns_interlock.c/.h` | RPEAK_IN GPIO timer; rolling HR window; >15 BPM change → cutoff <5.1ms |
| SW01-M06 | Impedance check | `np_impedance.c/.h` | 1kHz AC impedance before session enable; blocks if out of range |
| SW01-M07 | Session protocol signature verification | `np_session_sig.c/.h` | Ed25519 verify on session descriptor before enabling any stimulation |
| SW01-M08 | Fault latch and fault log | `np_fault.c/.h` | Latches fault state; logs to SHDR via SPI to SW-02; requires explicit app clear |

### 5.2 SW-02 — Main processor firmware modules (Class B)

| Module ID | Module name | Repo path | Function |
|---|---|---|---|
| SW02-M01 | OTA bootloader | `firmware/bootloader/` | Dual-bank OTA; Ed25519 firmware verification; DFU recovery |
| SW02-M02 | eMMC storage management | (spec: NP-FW-EMMC-001) | UHDR/SHDR partition management; AES-256-XTS; LittleFS |
| SW02-M03 | EEG signal processing | (planned) | ADS1299 driver; impedance; session data to UHDR |
| SW02-M04 | PBM session orchestrator | `firmware/pbm/` | 1064nm smart module; zone management; dose metering |
| SW02-M05 | HRV biofeedback | `firmware/hrv_biofeedback/` | PPG peak detection; coherence; taVNS sync; EEG-HRV biofeedback |
| SW02-M06 | Zone module detect and announce | `firmware/zone_announce/` | ZONE_ID detection; bone conduction audio. **⚠ SUPERSEDED 2026-07-28** — ZONE_ID detection retired by `np_module_map` (SW02-M09); this module needs porting to trigger from module-map events (see `docs/superseded/np_fw_za_001.md`) |
| SW02-M07 | sLORETA HD-tDCS (T2) | `firmware/sloreta_hdtdcs/` | Weight matrix; electrode mapping; current distribution |
| SW02-M08 | Cervical VNS session (T2) | `firmware/cervical_vns/` | Biphasic waveform; session orchestration; UHDR/SHDR |
| SW02-M09 | Hub control program — module registry | `firmware/hub_control/` | Probes all 11 slots/accessory ports on powerup; registers init/control/telemetry/shutdown function pointers; T2 stubs return NOT_PRESENT until hardware drivers built |
| SW02-M10 | Hub control program — session runner | `firmware/hub_control/` | Receives Ed25519-signed binary protocol; insertion-sort commands by start_ms; dispatches to module control functions at session-relative times; device-serial replay guard |
| SW02-M11 | Hub control program — session log | `firmware/hub_control/` | Collects per-module telemetry every 1s; routes to UHDR or SHDR per 27-element classification table (NP-FW-EMMC-001 §12); EEG blocks written direct to UHDR partition via HAL |
| SW02-M12 | Hub control program — safety SPI | `firmware/hub_control/` | 200ms SPI heartbeat to STM32G071 Safety MCU carrying requested-enable bitmask; Safety MCU cuts all stimulation GPIO on 1,500ms watchdog expiry |
| SW02-M13 | BLE GATT service | (planned) | BLE 5.3 LE Audio; GATT custom service; session sync |
| SW02-M14 | USB-C communications | (planned) | USB-C 3.2 Gen1; session data download; DFU interface |
| SW02-M15 | Fluxgate magnetometer | (planned) | EMF measurement; Helmholtz coil control; SHDR attenuation log |

#### 5.2.1 ZONE_ID Detection Debounce Requirement (RISK-18)

Firmware requirement, binding on both SW02-M04 (PBM session orchestrator, smart module ZONE_ID variant) and SW02-M06 (zone module detect and announce, base module ZONE_ID): a module insertion or removal is confirmed only after **3 consecutive ADC reads at 100ms intervals** with a **≥2/3 majority** agreeing on the same slot state (present/absent/smart-module-detected). A single noisy or transitional ADC read must never toggle a slot's state. This is the firmware-requirements-level statement of RISK-18 (zone module miskeying / false insertion detection); the full algorithm and state machine are specified in `firmware/zone_announce/` (`docs/superseded/np_fw_za_001.md` §6.2) and, for the 1064nm smart-module ZONE_ID variant, in `firmware/pbm/` (`docs/np_fw_pbm1064_001.md` §4). Both implementations satisfy this same 3-read/2-of-3 requirement; this subsection exists so the requirement is traceable from the software development plan itself, not only from the two module-level specs that implement it.

### 5.3 SW-03 — iOS/Android application (Class B)

| Module ID | Module name | Repo path | Function |
|---|---|---|---|
| SW03-M01 | Session protocol builder and signer | `app/` | Constructs session descriptor; Ed25519 signs with device key |
| SW03-M02 | UHDR management | `app/` | Encrypted UHDR partition access; backup; consent enforcement |
| SW03-M03 | Research anonymisation | `app/` | On-device anonymisation per study descriptor; k≥10 |
| SW03-M04 | Clinician consent engine | `app/` | Use case library; UHDR element mapper; consent document display |
| SW03-M05 | Predictive maintenance alerts | `app/` | SHDR trend analysis; reminder engine; consumable tracking |
| SW03-M06 | Apple Watch sync | `app/` | BLE GATT + WatchConnectivity; haptic/audio/visual sync |
| SW03-M07 | Device configuration and OTA trigger | `app/` | Initiates OTA; device pairing; configuration management |

---

## 6. Software Requirements

### 6.1 Requirement traceability

Per IEC 62304 §5.2, software requirements shall be:
- Documented (in firmware specifications or a dedicated requirements document)
- Traceable to system-level design inputs
- Each requirement shall have a unique identifier
- Each requirement shall have at least one verification test

**Traceability matrix:** NP-DT-001 (planned Month 6) will provide requirement-to-test traceability. Until authored, each firmware specification's FAI section serves as the traceability record for that module.

### 6.2 Software requirements for Class C (SW-01)

SW-01 requirements shall additionally specify:
- All software failure modes and their potential harm (per IEC 62304 §7.1 for Class C)
- The safety function performed by each module
- The maximum tolerable fault response time for each safety function

Currently documented SW-01 safety requirements with response times:
- SPI heartbeat timeout → all-stimulation cutoff: **≤50ms** (spec: 1.5s watchdog with <50ms GPIO response)
- HR change >15 BPM → cervical VNS cutoff: **≤100ms** (spec: <5.1ms worst-case, NP-FW-CVNS-001)
- Photoparoxysmal EEG → goggle LED cutoff: **≤200ms** (spec: Oz electrode pathway, SW-02 driven)
- Impedance check fail → session block: **before any stimulation pulse** (synchronous check at session start)
- Charge density ≥95% of limit → stimulation abort: **within one PWM period** (<25µs at 40kHz)
- Loss of forced convection / scalp-facing surface over-temp → PBM duty limited to natural-convection-safe ceiling: **≤ T_resp (thermal-class, ~10 s; < k·τ_face per THERM-1a)**, face temperature ≤ 42 °C maintained (NP-REQ-FANHEALTH-001)

**SR-FAN-01 … SR-FAN-06 — forced-convection (fan) health thermal interlock** (accepted from NP-REQ-FANHEALTH-001 under change control, 2026-07-22; source hazard FMEA-G07-01 / NP-FMEA-GEOM-001):

| ID | Requirement (abbrev. — full statement in NP-REQ-FANHEALTH-001 §2) | Class |
|----|------------------------------------------------------------------|-------|
| SR-FAN-01 | Scalp-facing module surface ≤ 42 °C (IEC 60601-1 applied-part limit) in normal operation **and under single-fault loss/degradation of forced convection** | C |
| SR-FAN-02 | Safety function rests on a measurement that bounds face temperature independently of forced-convection state — **Path B1: direct scalp-facing NTC co-located with PD2** (selected; Path A rejected per NP-THERM-CFD-R1-001) | C |
| SR-FAN-03 | On a face-temp / forced-convection fault, limit PBM zone duty to the **natural-convection-safe ceiling** (fan-off steady-state face ≤ 42 °C); provisional ≈ 4.5 mW/cm² at 43.3 °C ambient, stored as safety-MCU config constants (TBD-per-datasheet / THERM-1b) | C |
| SR-FAN-04 | Transition to the SR-FAN-03 safe state within **T_resp < k·τ_face** (nominal ≤ 10 s; τ_face ≈ tens of min per NP-THERM-CFD-R1-001 → wide margin) | C |
| SR-FAN-05 | SW-02 samples fan RPM → SHDR, computes forced-convection headroom trend, raises a predictive-maintenance alert (SW03-M05) **before** the safety derate | B |
| SR-FAN-06 | SR-FAN-01/03 decision made in the Class C domain (not dependent on SW-02); any fan-health input **fail-safe** (absent/stale/invalid → convection NOT confirmed → derate) | C |

Allocation: SW01-M04 (scalp-facing NTC read + derate; SR-FAN-01/02/03/04/06), SW-02 hub telemetry (fan-RPM log + fail-safe advisory; SR-FAN-05/06), SW03-M05 (maintenance alert; SR-FAN-05). Response-time class justified in NP-REQ-FANHEALTH-001 §3 (thermal-class seconds, not the ms-class of electrical interlocks). Verification per NP-REQ-FANHEALTH-001 §6, including **THERM-1b** scalp-phantom fan-stall design verification.

---

## 7. Software Architecture

### 7.1 Inter-processor communication architecture

```
┌─────────────────────────────────┐
│      iOS/Android App (SW-03)    │
│  Session protocol (Ed25519)     │
└────────────┬────────────────────┘
             │ USB-C / BLE
┌────────────▼────────────────────┐
│   Main Processor (SW-02)        │
│   NXP i.MX RT1062 / FreeRTOS    │
│   Session orchestration          │
│   EEG processing, HRV, PBM      │
│   UHDR/SHDR management           │
└────────────┬────────────────────┘
             │ SPI (heartbeat + enable commands)
             │ GPIO (fault, status)
┌────────────▼────────────────────┐
│   Safety MCU (SW-01)            │
│   STM32G071 / bare-metal        │
│   Hardware GPIO ownership        │
│   Stimulation interlocks         │
│   Independent cardiac monitor    │
└────────────┬────────────────────┘
             │ GPIO enable lines (hardware)
    ┌────────┼────────┐
    ▼        ▼        ▼
  tDCS    PBM zones   VNS/BES   (all stimulation hardware)
```

### 7.2 Architecture constraints (safety-critical)

1. SW-01 hardware GPIO ownership is **not software-assignable** — it is fixed in PCB layout. SW-02 cannot take over stimulation GPIO even if commanded to.
2. The SPI interface between SW-02 and SW-01 is **one-way for enables**: SW-02 sends enable requests; SW-01 decides grant/deny based on its own interlock state.
3. SW-01 has **no network connectivity** — it cannot receive commands from external sources. All commands flow through SW-02.
4. SW-01 firmware updates require **explicit user confirmation** via the app + page-by-page SPI flash with readback (NP-FW-EMMC-001 §8.5) — not automated OTA.

---

## 8. Verification and Testing

### 8.1 Unit verification (SW-01 — Class C required; SW-02/SW-03 safety units)

Each SW-01 module (SW01-M01 through SW01-M08) requires:
- Unit test specification: inputs, expected outputs, pass/fail criteria
- Test execution records with tool version and date
- 100% branch coverage target for safety-critical paths (documented deviation required for any uncovered branch)
- Static analysis results (MISRA compliance report)

### 8.2 Integration testing

Integration tests verify the interfaces between modules. Priority integration tests:
1. SPI heartbeat: SW-02 stops heartbeat → SW-01 fires all-cutoff within 1.5s + <50ms GPIO
2. Session signing: SW-03 signs descriptor → SW-01 verifies → stimulation enabled; unsigned descriptor → rejected
3. Charge density: simulated overdose scenario → SW-01 aborts at 95% threshold
4. Cervical VNS cardiac interlock: simulated HR spike >15 BPM → SW-01 cutoff within spec
5. Dual-bank OTA: corrupted firmware image → bootloader rejects (Ed25519 fail) and reverts

### 8.3 System testing

System testing verifies end-to-end session behaviour against the session protocol specification. System test cases map to design inputs. For T2 510(k), system test results constitute design verification evidence.

### 8.4 Software problem resolution

Per IEC 62304 §9, all software problems (bugs, failures during testing) shall be:
1. Logged with unique identifier, description, severity, and discovery context
2. Analysed for root cause and patient safety impact
3. Resolved with documented fix and re-test
4. Reviewed by the Quality Lead for safety significance
5. Evaluated for whether the problem constitutes a previously unidentified risk (feeds back to NP-RM-001)

**Problem severity classification:**
- **Critical:** Could cause or contribute to patient harm; device cannot be released with this problem open
- **Major:** Affects primary function; must be resolved before release
- **Minor:** Cosmetic or low-impact; may be deferred to next release with documented rationale

---

## 9. Configuration Management

### 9.1 Version identification

All software items shall have a version identifier following this scheme:

```
MAJOR.MINOR.PATCH[-QUALIFIER]
e.g., 1.0.0, 1.2.3-RC1, 0.9.0-BETA
```

- **MAJOR:** Incremented for architecture changes or 510(k)-significant updates
- **MINOR:** Incremented for feature additions or significant bug fixes
- **PATCH:** Incremented for minor bug fixes

### 9.2 Release tagging

Release candidates shall be tagged in git:
```
git tag -a v1.0.0-RC1 -m "Release candidate 1 for 510(k) submission"
```

Production firmware images shall be tagged `vMAJOR.MINOR.PATCH` without qualifier.

### 9.3 Build reproducibility

Firmware builds shall be reproducible from a tagged commit. Build environment (compiler version, CMake version, toolchain) shall be documented in the build record for each release.

### 9.4 SOUP (Software of Unknown Provenance)

Per IEC 62304 §8, all third-party libraries and components used in SW-01 and SW-02 must be identified and managed:

| SOUP item | Version | SW item | Safety class impact | Verification |
|---|---|---|---|---|
| FreeRTOS-Kernel | V11.3.0 (FreeRTOS-LTS 202604.00-LTS) | SW-02 | Class B | Vendored byte-exact in `firmware/vendor/freertos/` (SOUP record `VERSION`, MIT license); subset = ARM_CM7 r0p1 port + heap_4 (shipped) + POSIX port (host test only); `croutine.c` excluded. Configuration `firmware/hub_control/include/FreeRTOSConfig.h` enables `configCHECK_FOR_STACK_OVERFLOW=2` + `configUSE_MALLOC_FAILED_HOOK=1` (hooks in `np_hub_freertos_hooks.c`, fail-safe halt). Host verification: `np_freertos_smoke_tests` compiles the kernel + this config against the POSIX port and runs the scheduler (event group + `vTaskDelayUntil` + prioritised tasks) — passes in `firmware-host-tests` CI. FreeRTOS-Kernel known-anomalies list to be reviewed and recorded at G2. **On-target ARM_CM7 build is no longer pending (2026-09-01, NP-SW-CI-001 §4.8):** the kernel, this configuration and the ARM_CM7 r0p1 port are compiled for the i.MX RT1062 and linked into `np_application.elf`, whose FreeRTOS heap (`ucHeap`, `configTOTAL_HEAP_SIZE` = 64 KiB) is the single largest object in the image. `vPortSVCHandler`/`xPortPendSVHandler`/`xPortSysTickHandler` resolve against the vendored MCUX vector table through the `FreeRTOSConfig.h` aliases, with no FreeRTOS-specific entries added to the startup file. **The image links; it does not run** — the SW-02 platform layer is 93 traps (NP-SW-CI-001 §4.8.4) and `configCPU_CLOCK_HZ`'s 600 MHz is not established by anything in the boot path (OI-SWCI-41). |
| LittleFS | 2.x | SW-02 | Class B | Power-loss testing per LittleFS test suite |
| Monocypher (Ed25519 optional SHA-512 module) | 4.0.2 | SW-01, SW-02 | Class C | Vendored in `firmware/crypto/vendor/monocypher/`; SOUP record `VERSION` file; 11-test suite (RFC 8032 TV1/TV2 + all-zero pubkey guard) passes; BSD-2-Clause OR CC0-1.0; OI-SW01-M07-02 CLOSED 2026-06-11. Note: source files carry `__git__` version string — normal upstream behaviour; canonical version is git tag 4.0.2. Bootloader (`firmware/bootloader/`) retains self-contained Ed25519 (uses `-nostdlib/-nodefaultlibs`; Monocypher requires libc symbols). |
| Unity (unit test framework) | 2.x | SW-01 test | Test only — not shipped | N/A |
| ARM CMSIS-DSP | 1.x | SW-02 | Class B | Uses validated DSP functions; input range validation in calling code |
| ARM CMSIS-Core(M) | 5.6.0, from CMSIS_5 tag `5.9.0` | SW-01 **and SW-02** | **Class C** (SW-01 subset); Class B (SW-02 subset) | Vendored byte-exact in `firmware/vendor/cmsis_core/` (SOUP record `VERSION` with per-file provenance + SHA-256, Apache-2.0 per the SPDX identifier in every header). Subset = `core_cm0plus.h`, `cmsis_version.h`, `cmsis_compiler.h`, `cmsis_gcc.h`, `mpu_armv7.h` — five headers, no translation unit — **plus `core_cm7.h` and `cachel1_armv7.h`, added 2026-09-01 for SW-02** (NP-SW-CI-001 §4.8.2, OI-SWCI-20). **One component, two SW items, and the subsets are disjoint at the core header:** `core_cm0plus.h` is reachable only from `stm32g071xx.h` (SW-01) and `core_cm7.h` only from `MIMXRT1062.h` (SW-02), and the two builds are separate CMake projects with separate toolchain files. The alternative — a second copy of the four core-agnostic headers under `firmware/vendor/mcux_sdk/` — was refused: two byte-identical copies of one component double the surface to justify and make a tag bump something that can be applied to one and not the other. **The §7.1.2 evaluation below did not change scope**, because neither added file is on SW-01's include path; if that ever stops being true it must be revised before the change lands. Present because `stm32g071xx.h` includes `core_cm0plus.h` unconditionally, **not** for `SysTick`, which SW-01 never accesses through CMSIS. DSP/NN/RTOS/RTOS2/DAP/Driver/Pack, all other cores, and the non-GCC compiler headers are excluded. **IEC 62304 §7.1.2 anomaly evaluation performed and recorded: NP-SOUP-CMSIS-001 Rev 1 §3** — conclusion: no anomaly in the vendored subset can contribute to a hazardous situation; every item found against these files is a compiler diagnostic, a comment, a non-Armv6-M path, or a MISRA-scope question. Re-evaluation required on any tag change. Verification: `np_safety_mcu_objs` compiles all 10 Class C TUs for STM32G071 under `-Wall -Wextra -Werror` (gating in `safety-mcu-ci.yml`, NP-SW-CI-001 §6.5); the SW-02 pair is verified by `np_application.elf` linking (`firmware-cross-build.yml`, NP-SW-CI-001 §4.8), and `cachel1_armv7.h`'s necessity by deletion, as `mpu_armv7.h`'s was. |
| ST CMSIS-Device STM32G0 | `v1.4.5` | SW-01 | **Class C** | Vendored byte-exact in `firmware/vendor/cmsis_device_g0/` (SOUP record `VERSION` with per-file provenance + SHA-256, Apache-2.0 read from `LICENSE.md` at the tag — the headers carry no SPDX identifier, and ST has moved CMSIS-Device repos between licences across releases, so the licence is a property of the pinned tag). Subset = `stm32g0xx.h`, `stm32g071xx.h`, `system_stm32g0xx.h`. **The ST HAL and LL drivers are deliberately NOT vendored** (OI-SWCI-06 closed): SW-01 makes zero `HAL_*` and zero `LL_*` calls, verified with word-boundary matching, so the HAL would add substantive third-party logic to Class C SOUP surface for no benefit. `USE_HAL_DRIVER` is consequently undefined. `system_stm32g0xx.c` excluded — nothing calls `SystemInit` (read from `startup_stm32g071xx.s`). **IEC 62304 §7.1.2 anomaly evaluation performed and recorded: NP-SOUP-CMSIS-001 Rev 1 §4** — ST publishes "Known Limitations: None" for v1.4.5, and the one v1.4.5 correction that could have mattered (`TIMx_CCR5`) is a bit-mask SW-01 does not reference, with a v1.4.4→v1.4.5 struct diff confirming no peripheral layout moved. Re-evaluation required on any tag change. Verification: as above, plus an `objdump` check that `GPIOA`/`GPIOB` resolve to `0x50000000`/`0x50000400` in the compiled `np_gpio_mgr.c` object. |
| NXP MCUXpresso SDK — MIMXRT1062 device layer | 2.16.0, tag `MCUX_2.16.000` | SW-02 | Class B | Vendored byte-exact in `firmware/vendor/mcux_sdk/` (SOUP record `VERSION` with per-file provenance + SHA-256, BSD-3-Clause per the SPDX identifier in every file; full text in `COPYING-BSD-3` from the same tag). Subset = `MIMXRT1062.h`, `MIMXRT1062_features.h`, `fsl_device_registers.h`, `system_MIMXRT1062.{h,c}`, `gcc/startup_MIMXRT1062.S` — six files, one translation unit. **No peripheral drivers:** SW-02 makes zero `fsl_*` calls (word-boundary grep, nothing outside comments), because the platform layer that would call them is not written; vendoring for a caller that does not exist would mean guessing the subset. **No linker script, deliberately:** every MCUX script for this part links `.text` at `0x60002400` for XIP from FlexSPI NOR, and NeurOne stages its application into OCRAM from an eMMC bank, so an SDK script would place the image where the bootloader never looks — `firmware/application/linker/app_imxrt1062.ld` is NeurOne-authored instead, and its agreement with the bootloader's reservation is enforced by `np_app_link_agreement_tests` rather than asserted. **Class B, so the IEC 62304 §7.1.2 anomaly evaluation does not attach** — a reachability claim, recorded in `VERSION` with the check that makes it observable (`firmware/safety_mcu/` is a separate CMake project whose toolchain file and include paths never mention MIMXRT1062) and with the standing obligation that the evaluation must exist before any change that makes a file here reachable from SW-01. Verification: `np_application.elf` links with 0 unresolved symbols, gating in `firmware-cross-build.yml` (NP-SW-CI-001 §4.8, phase 8, closes OI-SWCI-20). `MIMXRT1062_features.h`'s necessity established by deletion. |

Additional SOUP items must be added to this table as third-party components are integrated.

**Class C SOUP note.** The two CMSIS rows above are the first SOUP components vendored specifically for **Class C** software (SW-01 owns every stimulation enable GPIO). FreeRTOS is Class B and Monocypher was handled as Class B + C on a much smaller surface. The IEC 62304 §7.1.2 obligation — evaluate the publicly available anomaly list *for the version in use* for anomalies that could result in a hazardous situation, and record the evaluation — is discharged by **NP-SOUP-CMSIS-001**, which records the method, the sources consulted, the per-item assessment, and the residual limitations, rather than a bare conclusion. That evaluation is attached to the pinned tags; moving either tag voids it and requires a new revision.

**Class B SOUP note, added 2026-09-01.** The MCUX row is the first component vendored for SW-02 since FreeRTOS, and the absence of a §7.1.2 record against it is a consequence of its class, not an omission. That is worth stating plainly because the row sits directly beneath two that do carry one, and a later reader scanning the column would otherwise read the gap as an oversight. The claim that keeps it Class B is about **reachability** — the component is compiled into `np_application` and into nothing else — and reachability claims decay silently, so it is recorded with the check that makes it observable rather than as an assertion. The same standing obligation applies to `core_cm7.h` and `cachel1_armv7.h` in the CMSIS row above.

---

## 10. Maintenance and Post-Market Software Support

### 10.1 OTA update process

OTA firmware updates follow the 9-step process defined in NP-FW-EMMC-001 §8.1–8.4:
1. New firmware version tested and approved per this plan
2. Image signed with Ed25519 manufacturing key
3. App delivers image to device over USB-C or BLE
4. Bootloader verifies signature; writes to inactive bank; verifies readback
5. Bootloader flags OTA_PENDING; reboots to new firmware
6. New firmware runs for 3 successful boots → marks bank as primary
7. Failure: automatic rollback to previous bank after 3 failed attempts

SW-01 (Safety MCU) firmware is **never updated via automated OTA** — requires explicit user confirmation + in-app process (NP-FW-EMMC-001 §8.5).

### 10.2 Software problem resolution in production

Post-market software problems are reported through the CAPA process (NP-QMS-CAPA-001). A software problem in production that could harm users constitutes a Field Safety Corrective Action and must be escalated to the Quality Lead within 24 hours.

### 10.3 OS compatibility

iOS and Android OS updates may affect SW-03 behaviour. NeurOne must:
- Participate in Apple and Google developer beta programmes
- Test SW-03 against each OS beta before public release
- Target a 7-day OS compatibility SLA (patch available within 7 days of an OS release that breaks SW-03)
- Mode 3 (Autonomous — no phone) provides structural fallback if OS update causes SW-03 incompatibility

---

## 11. IEC 62304 Compliance Checklist

The following table maps IEC 62304 clauses to NeurOne implementation status:

| IEC 62304 Clause | Requirement | Status | Reference |
|---|---|---|---|
| §4.3 | Safety class assignment | **COMPLETE** | §3 this document |
| §5.1 | Software development planning | **COMPLETE** | This document (NP-SW-001) |
| §5.2 | Software requirements analysis | **PARTIAL** | Per-module in NP-FW-* specs; unified requirements doc pending |
| §5.3 | Software architectural design | **PARTIAL** | §7 this document; detailed architecture per module pending |
| §5.4 | Software detailed design | **IN PROGRESS** | NP-FW-* specifications authored for 6 of 11 SW-02 modules |
| §5.5 | Software unit implementation | **IN PROGRESS** | `firmware/` directories authored for completed modules |
| §5.6 | Software unit verification | **PARTIAL** | FAI software items PASS for completed modules; hardware bench items PENDING |
| §5.7 | Software integration and integration testing | **NOT STARTED** | Requires prototype hardware |
| §5.8 | Software system testing | **NOT STARTED** | Requires prototype hardware + complete firmware |
| §6 | Software maintenance | **PLANNED** | §10 this document |
| §7.1 | Identify software items contributing to hazardous situations (Class C) | **PARTIAL** | SW-01 modules identified; unit-level FMEA not yet authored |
| §7.2 | Software risk management measures | **PARTIAL** | Safety MCU architecture documented; formal SWFMEA pending |
| §8 | Software configuration management | **PARTIAL** | Git version control in use; §9 this document |
| §9 | Software problem resolution | **PLANNED** | Process defined §8.4 this document; tracking system pending |

---

## 12. Document History

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-05-13 | Interim Quality (CEO) | Initial release. IEC 62304 software plan established at QMS formation. Three software items classified: SW-01 Class C, SW-02 Class B, SW-03 Class B. All existing firmware modules indexed. |
| 2 | 2026-07-22 | Steve Hickman (CEO, interim Quality authority) | **SR-FAN-01…06 accepted into §6.2** (forced-convection/fan-health thermal interlock) from NP-REQ-FANHEALTH-001 under change control; source hazard FMEA-G07-01 (NP-FMEA-GEOM-001). Adds the loss-of-forced-convection response-time bullet + the six SR-FAN requirement statements (five Class C, one Class B), allocated to SW01-M04 / SW-02 / SW03-M05. §5.1 SW01-M04 module safety-function text extended (scalp-facing NTC Path B1 + face ≤42 °C under fan loss). Path selection per NP-THERM-CFD-R1-001: Path A (junction-throttle-only) rejected, **Path B1 (scalp-facing NTC at PD2) selected**; SR-FAN-03/04 constants provisional pending verification-grade CFD + THERM-1b. Traces to NP-DHF-001 Rev 22, NP-DT-001 DI-SAFE-13. Rev 1 → B. |
| 3 | 2026-07-27 | Steve Hickman (CEO, interim Quality authority) | **§5.2.1 ZONE_ID Detection Debounce Requirement (RISK-18) added** — states the 3-read/100ms/2-of-3-majority debounce requirement at the software-development-plan level, cross-referencing the existing implementations in NP-FW-ZA-001 §6.2 and NP-FW-PBM1064-001 §4. Closes the corresponding `docs/status/pending-decisions.md` open item. Rebased on top of Rev 2 (PR #217); no Rev 2 content changed. Traces to NP-DHF-001 Rev 23. Rev 2 → C. |
| 4 | 2026-09-01 | Steve Hickman (CEO, interim Quality authority) | **§9.4 SOUP: NXP MCUXpresso SDK 2.16.0 (MIMXRT1062 device layer) added as SW-02 Class B SOUP** — six files and a licence, byte-exact from tag `MCUX_2.16.000`, per-file SHA-256, with the "intentionally NOT vendored" list the FreeRTOS/CMSIS precedents require (NP-SW-CI-001 §4.8.2, closes OI-SWCI-20). Peripheral drivers and every SDK linker script are excluded on stated grounds, not omitted. **The ARM CMSIS-Core(M) row now serves two SW items** — `core_cm7.h` and `cachel1_armv7.h` were added to the existing vendored component rather than duplicated, and the two core headers are disjoint at the file level, so the §7.1.2 evaluation's scope is unchanged. **FreeRTOS's "on-target ARM_CM7 build pending" verification note is discharged**: the kernel and the shipped port are linked into `np_application.elf` (NP-SW-CI-001 §4.8, closes OI-SWCI-21). A **Class B SOUP note** was added beneath the Class C one, because the new row sits under two that carry a §7.1.2 record and the gap would otherwise read as an oversight rather than as the class. No requirement, classification or safety claim in §§1–8 changed. |
