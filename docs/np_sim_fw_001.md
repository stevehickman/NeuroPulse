# Firmware-in-the-Loop Helmet Simulator — Design

**Project:** NeurOne
**Document:** NP-SIM-FW-001
**Revision:** 8
**Date:** 2026-11-08
**Status:** DRAFT
**Effective Date:** —
**Author:** NeurOne Software Engineering
**Approved By:** — (new document; decisions D-1…D-3 in §9 are the principal's)
**References:** CLAUDE.md §4.1, §4.2 (safety MCU owns every enable line), §5.1 (UHDR is never accessible to NeurOne), §18 (a requirement must be required), §19 (unjustified choices), §20 (shared files), §21 (tests name the requirement they verify), §22 (Rust first); `NP-SIM-001` (the helmet simulator this extends; `simulator/HOWTO.md`); `NP-SW-CI-001` §4.8 (the SW-02 platform layer and its census); `NP-FW-HUB-001` (hub control program, module registry, socket dispatch); `NP-CONV-001` §4, §7.1; `NP-FW-BENCH-001` (bench and service mode; §8.3 is what a bench rig needs); `firmware/platform/include/np_platform_trap.h`; `firmware/platform/include/np_sw02_platform_hal.h`
**Related Issues:** — (raised by the principal in the 2026-11-01 session; no GitHub issue yet)
**Gate:** N/A — design study; nothing here gates a hardware release
**IEC 62304 Class:** N/A — a development tool that is not part of the device, never linked into a device image (`REQ-SIMFW-01`) and never verification evidence (`REQ-SIMFW-06`). It changes no SW-01 or SW-02 source.
**Supersedes:** None
**Parent Document:** None

---

> **Scope in one paragraph.** The helmet simulator (`NP-SIM-001`) draws a helmet and replays a protocol
> against a model written for the purpose. This document designs the next step: **the simulator runs the
> real hub firmware**, with each physical module an object that holds its own state, draws itself, and
> can be told what to answer. The purpose is **whole-system effects**: a developer changes one module's
> behaviour and sees what the rest of the system does about it, where a unit test calls one function in
> isolation and cannot. **A second aim:** to the degree possible the same code is the basis
> of a bench test apparatus (§11). **§12 records the first build, the PBM spike.** §8 is the
> build order and §9 the decisions needed first.

---

## 1. What exists today

Read from the tree on 2026-11-01. Nothing in this section is a proposal.

| Question | Answer | Where |
|---|---|---|
| Does the simulator run helmet code? | **No.** `SessionEngine` is a JavaScript model that reads a parsed protocol and produces plausible numbers. Its header says all physics is fake, and its dose accumulation line is multiplied by zero. The only shared code is the Rust NPPS parser, run as WebAssembly. Nothing under `simulator/` names `firmware/`. | `simulator/js/session.js` |
| Is a module an object with its own state? | **No.** `HelmetModel` keeps per-zone configuration and recomputes each socket from the winning zone. A socket is a render slot. Session state is one global snapshot keyed by modality. | `simulator/js/helmet.js` |
| Does a module draw itself? | **No.** `HelmetModel` and the app's render loop draw everything. | `simulator/js/helmet.js`, `app.js` |
| Can a module be told what to answer? | **No.** The device API accepts `SESSION_START`, `SESSION_PAUSE`, `SESSION_STOP`, `ZONE_CONFIG` and `ACCESSORY_CONFIG`, and sends `TELEMETRY`, `FAULT` and `SESSION_COMPLETE`. `sendFault()` exists and nothing calls it. | `simulator/js/api.js` |
| Is there a firmware seam to attach to? | **Yes, and it is already single-sourced.** `np_sw02_platform_hal.h` plus eleven module headers declare the whole hardware-facing contract, `NP_SW02_PLATFORM_SYMBOL_COUNT` = 119. | `firmware/platform/include/` |
| What does `firmware/platform/` do with that seam? | **Every symbol is a trap that halts.** It is cross-compile only (`if(NOT CMAKE_CROSSCOMPILING) return()`). Its header argues that a stub answering plausibly is how an interlock is silently defeated. | `firmware/platform/` |
| Is there a plausible-answer double? | **Yes, for PBM only, host-test only, global and not scriptable.** It answers ACK on every probe and returns nominal photodiode counts. | `firmware/pbm/src/np_pbm_hal.c` |

---

## 2. Feasibility, measured

The design depends on two facts that could have been false. Both were tested on 2026-11-01 in a scratch
build outside the tree (`cmake -DNP_BUILD_TESTS=ON`, gcc 13 / clang 18, Linux x86-64).

| Finding | Result |
|---|---|
| Does FreeRTOS run on the host? | **Yes.** `vendor/freertos/portable/ThirdParty/GCC/Posix` is vendored and `FreeRTOSConfig.h` has an `NP_FREERTOS_POSIX_HOST` branch. The full host suite builds and **all 63 tests pass**, including `np_freertos_smoke_tests`, which runs the real scheduler. |
| Does the hub compile on the host? | **Yes.** Every source in `hub_control/`, `pbm/`, `zone_announce/`, `cervical_vns/`, `shdr/`, `hrv_biofeedback/` and `sloreta_hdtdcs/` compiled with the POSIX port and zero failures, including `np_hub_control_main.c` (five tasks and `vTaskStartScheduler`) and every module under `hub_control/modules/`. `np_pbm_hal.c` was excluded, because the simulator's platform layer replaces it. |
| How much is unresolved at link? | **116 symbols**, of which about **94 are platform seams** and the rest are libraries already in the tree: `lfs_*` (vendored littlefs), `np_ed25519_verify` and `np_sha256` (`firmware/crypto`), `np_factory_reset_boot_check`, `np_za_platform_*`. This is a scan of undefined symbols, not a link. |
| Can the firmware run in the browser as WebAssembly? | **Not without new work, and not recommended.** The POSIX port is built on `pthread_create`, `pthread_kill`, `sigwait`, `sigaction` and `pthread_cancel`. Those need SharedArrayBuffer plus special HTTP headers in a browser, and signals do not exist there. No FreeRTOS WebAssembly port exists, so it would be a custom port (Emscripten fibers or Asyncify), which is a new kernel to qualify. Emscripten (`emcc`) is not installed here, and clang has no C sysroot for `wasm32`. |
| Does the safety MCU run on the host? | **Yes.** All 12 sources in `firmware/safety_mcu/src/` compile with gcc and no warnings; 11 as they are and `np_gpio_mgr.c` with the force-included `tests/np_gpio_mgr_test_ports.h` its own host test already uses. Linked against a scratch HAL that defines the 41 `np_hal_*` functions of `np_safety_hal.h` plus `firmware/crypto`, the real `main()` ran its init sequence and its `for(;;)` loop with no source change. At boot all ten enable lines went to 1 (disabled). With no heartbeat it granted nothing. Valid heartbeats requesting the cranial enable were answered. What was **not** exercised: the signature path, the thermal, charge and cardiac interlocks, the tier gate, and the watchdog cutoff after a running grant. The loop is a busy-poll, so the host HAL needs a pacing point. |
| Two small defects the host build exposed | `xPortIsInsideInterrupt` is missing from the POSIX port (`np_hub_control_main.c:125` calls it). `portMAX_DELAY` is 64-bit in the POSIX port and the tick type is 32-bit, so the constant truncates; it still works as a very long wait. Both are `OI-SIMFW-03`. |

**Conclusion.** The firmware, hub and safety MCU, can run natively on the host with its real tasks. It
cannot run in the browser without a custom kernel port. The design in §3 follows from that.

---

## 3. Architecture

```
 ┌──────────────────────────── browser (simulator/) ───────────────────────────┐
 │  SimModule instances ── state · render() · inspector · fault controls        │
 │       ▲  ▼  MODULE_LIST / MODULE_SET / MODULE_FAIL        (WebSocket)        │
 └───────┼──┼──────────────────────────────────────────────────────────────────┘
         │  │            ws://localhost:9000  (simulator/server/index.js)
 ┌───────┴──┴───────────────────────────────────────────────────────────────────┐
 │  bridge (Node) ── starts and supervises the firmware process, relays messages │
 └───────┬──────────────────────────────────────────────────────────────────────┘
         │ local socket
 ┌───────┴──────────────────── firmware process (native) ───────────────────────┐
 │  unmodified hub_control / pbm / zone_announce / cervical_vns / … sources      │
 │  FreeRTOS POSIX port (real tasks, real heartbeat)                             │
 │  sim platform layer: defines every seam symbol, routes by (kind, index)       │
 │  module mirror: one small state record per module, synchronous to read        │
 │  file-backed block device for littlefs · firmware/crypto for signatures       │
 └───────────────────────────────────────────────────────────────────────────────┘
```

### 3.1 The firmware process

- **Built from the firmware's own sources, unmodified.** The simulator links the same `.c` files the
  device image does, with the sim platform layer in place of `firmware/platform/`. **No
  `#ifdef SIM` appears in a firmware source file** (`REQ-SIMFW-03`). A simulator that runs
  different code from the device is the thing this design exists to avoid.
- **Real tasks, real timing.** `task_safety_heartbeat`, `task_hub_control`, `task_protocol_rx`,
  `task_telemetry` and `task_module_detect` run under the POSIX port. The interactions between them
  are what a unit test cannot show.
- **Timing is indicative, not evidence.** The POSIX port schedules on a general-purpose OS, so a
  200 ms heartbeat or a 1.5 s watchdog observed here says nothing about the device (`REQ-SIMFW-06`).

### 3.2 The sim platform layer (`firmware/platform_sim/`, proposed)

- **Defines every seam symbol once.** The set is parsed out of the headers at build time, as
  `scripts/check-platform-seam-decls.ts` already does, and never restated in a second list.
- **Routes by `(kind, index)`.** Each seam call becomes `sim_bus(kind, index, op, args)`:
  `np_mod_pbm_hal_socket_ntc_read(socket_id)` goes to the module at that socket;
  `np_mod_visual_hal_goggle_seated()` goes to the goggle singleton. A seam with no route fails the
  build (`REQ-SIMFW-02`), mirroring the census at `firmware/cmake/np_platform_census.cmake`.
- **Answers from the module first.** If the module has an injected response for that op, that wins.
  Otherwise the module's default model answers.
- **Cannot reach a device image** (`REQ-SIMFW-01`). The directory builds only under the host
  toolchain, as `firmware/platform/` builds only under the cross toolchain, and a CI check fails if
  an ARM target links it.

### 3.3 `SimModule`

One instance per physical thing. Addressing is the firmware's own: **socket** (PBM base and smart
tiles, 0-based index space), **slot** (accessories), **side** and **pair** (stim and VNS).

| Field | Meaning |
|---|---|
| `id`, `kind`, `address` | identity; `kind` is one of `pbm_base`, `pbm_smart`, `eeg`, `audio`, `visual`, `vns`, `intranasal`, `stim_pair`, `cvns`, `tms` |
| `state` | what the module holds: commanded output, temperature, impedance, seated, present, faulted |
| `handle(op, args)` | returns the HAL result for a firmware call; default model unless overridden |
| `setResponse(op, code, {times, until})` | the next calls to `op` return `code`, a `np_hub_status_t` |
| `setReading(op, value)` | pins a read, for example NTC at 71 °C or an impedance |
| `setPresent(bool)` | detection reports `NP_HUB_ERR_NOT_PRESENT` when false |
| `setSeated(bool)` | fires the Hall interrupt callback when it goes false |
| `render(dt)` | the module draws itself: glow, temperature tint, fault colour |

Status codes are generated from `firmware/hub_control/include/np_hub_types.h` into the browser by the
mechanism that generates `sockets.generated.js` today, so there is one list (`REQ-SIMFW-05`).

### 3.4 Authority and the safety MCU

**A module draws as driven only if the firmware commanded it and the safety MCU's enable line for it is
asserted** (`REQ-SIMFW-07`). Without that, the simulator would show a tile lit that the real helmet
would have cut, and the one property CLAUDE.md §4.2 rests on (the safety MCU owns every enable) would be
invisible. This requires the safety MCU firmware (`firmware/safety_mcu`, STM32G071, bare-metal) to run
in the loop. **It can** (§2, `OI-SIMFW-01` closed): the safety MCU is a separate host process or
thread with its own seam, the 41 `np_hal_*` functions of `np_safety_hal.h`, of which the SPI functions
face the hub's `np_safety_hal_spi_transfer`. It gets its own `SimModule` (`kind` = `safety_mcu`), which is
where a developer forces a thermal reading, a bad signature or a missing heartbeat. Until it is wired in,
the simulator tests hub behaviour only and must say so on screen.

### 3.5 Bridge protocol

New messages beside the existing ones; the existing ones are unchanged.

| Message | Direction | Carries |
|---|---|---|
| `MODULE_LIST` | firmware to browser | every module: id, kind, address, state |
| `MODULE_STATE` | firmware to browser | one module's changed state, including the last commanded value and the enable it holds |
| `MODULE_SET` | browser to firmware | patch to one module's state |
| `MODULE_FAIL` | browser to firmware | `{id, op, code, times}` — an injected response |
| `MODULE_CLEAR` | browser to firmware | removes one module's injected responses |
| `FW_LOG` | firmware to browser | the hub's own log lines, so a developer sees cause and effect together |

The browser's inspector (click a module in the 3D view) sends these. A headless test sends the same
messages, which is how the simulator's own tests drive it.

---

## 4. What this lets a developer see

Each is an effect that crosses a module boundary, which is the case unit tests miss.

| A developer changes… | …and sees |
|---|---|
| A socket's `pwm_set` to fail with `MOD_FAULT` | the dispatch rolls back every socket the command started, the cranial enable drops only when none remain, and the runner's session record shows what was delivered |
| A tile's NTC reading to 71 °C | the thermal path throttles or cuts, the other tiles continue, the app is told |
| Goggle seated to false mid-session | the Hall interrupt fires, visual stops within its limit, and PBM and audio continue |
| A module removed mid-session | `task_module_detect` notices, the registry deregisters, the runner's reaction |
| The heartbeat task delayed | the safety MCU's watchdog behaviour on the cut (needs `OI-SIMFW-01`) |
| A new PBM mask in a protocol | the placement check, the power governor refusal (the production governor refuses every load, `np_socket_dispatch.h`), and where the refusal surfaces |

---

## 5. Requirements

Each row answers `NP-CONV-001` §7.1's two questions: what fails if it is not met, and where that is
traceable. None is tested yet, because nothing is built (CLAUDE.md §21).

| ID | Requirement | What fails if not met | Traceable to |
|---|---|---|---|
| **`REQ-SIMFW-01`** | The sim platform layer is never linked into an image that targets the device. A CI check fails any ARM target that links it. | A layer that answers plausibly sits in a device image and converts a missing driver into a defeated interlock. | `firmware/platform/include/np_platform_trap.h` (the reasoning that forbids exactly this); CLAUDE.md §4.2 |
| **`REQ-SIMFW-02`** | Every symbol in the SW-02 platform contract (`NP_SW02_PLATFORM_SYMBOL_COUNT`) has exactly one sim definition and one route. The build fails if one is missing, duplicated or unrouted. | A new seam falls through unanswered, so the simulator silently runs a different contract from the device. | `firmware/cmake/np_platform_census.cmake` and `scripts/check-platform-seam-decls.ts` (the same property for the trap layer) |
| **`REQ-SIMFW-03`** | The firmware sources the simulator runs are the device's sources, unmodified. No simulator conditional appears in any firmware source file; the build files name the sim layer and nothing else. | The simulator tests code that does not ship, and a defect it shows or hides is not the device's. | This document's purpose (§ scope) |
| **`REQ-SIMFW-04`** | A module answering from its default model, with no injected response, is marked as unscripted in the inspector and in `FW_LOG`. | A developer reads a default answer as a result, as the stubs the trap layer was written to prevent are read as drivers. | `firmware/platform/include/np_platform_trap.h` (a plausible answer is indistinguishable from a correct one) |
| **`REQ-SIMFW-05`** | Status codes and module addresses the browser uses are generated from `np_hub_types.h` and the socket map, never retyped. | A code is renumbered in firmware and the injected fault means something else, with no build failure. | `NP-SW-CI-001` §4.3 (Defect C: one value in two places) |
| **`REQ-SIMFW-06`** | No simulator output is verification evidence. The simulator states on screen that its timing is indicative and that it does not replace the host tests or hardware tests. | A passing simulator run is cited in a design-history file as verification of a Class B or Class C function. | `NP-SW-001` §3 (verification is by the unit tests and the hardware); IEC 62304 §5.5, §5.6 |
| **`REQ-SIMFW-07`** | A module is drawn as driven only if the firmware commanded it and the safety MCU's enable for it is asserted. While the safety MCU is not in the loop, the simulator says so. | The simulator shows stimulation that the real safety MCU would have cut, and hides the control CLAUDE.md §4.2 rests on. | CLAUDE.md §4.2 |
| **`REQ-SIMFW-08`** | The simulator's storage is synthetic. It never mounts, opens or accepts an image of a real UHDR or SHDR partition, and it holds test signing keys only. | A developer loads a person's data image into a browser tool, or a production key is placed where a development tool reads it. | CLAUDE.md §5.1 (NeurOne never has access to UHDR); `simulator/server/keys/README.md` |
| **`REQ-SIMFW-11`** | A sim module answers only for itself. An injected response, pinned reading or absence on one module changes no other module's answers, and a module that was not addressed reports no answer. | A scenario's effect is attributed to a module that did not cause it, or a cross-module effect appears that the firmware does not have. The simulator exists to show the firmware's whole-system effects, not its own. | This document's purpose (the opening scope paragraph) |
| **`REQ-SIMFW-12`** | The bridge builds every command it sends to the firmware process from validated integers and whitelisted names. No string a client sent is placed in a command line. | A WebSocket client adds a command, with an embedded newline, to the line protocol of a process that runs the hub's stimulation logic. | CWE-77 (command injection); `OI-SIMFW-07` |
| **`REQ-SIMFW-13`** | **Every module type the firmware drives has a simulator equivalent of all of that module's functionality, running the module's real firmware source** (its driver under `firmware/hub_control/modules/` and the libraries it calls), not a re-implementation. The module types are the nine drivers (`np_mod_pbm`, `np_mod_eeg`, `np_mod_audio`, `np_mod_visual`, `np_mod_vns`, `np_mod_stim` for BES/tACS and tDCS, `np_mod_intranasal`, `np_mod_cvns`, `np_mod_t2_stubs`) and the 1064 nm smart tile's register path. A function of a module the simulator does not model is **listed as a gap** with the reason, never silently absent, and the line coverage of each module's source under its scenarios is reported, so how much of the code a modality's scenarios reach is a measured number. | The simulator shows only the modalities it models, so a whole-system effect that involves the others (power, heat, an interlock between modalities, the order of stops) is not seen, and a developer reads the absence of an effect as the absence of a defect. | The Principal's direction of 2026-11-03; this document's purpose (the opening scope paragraph) |

---

## 6. Placement and governance

- **`firmware/platform_sim/`** holds the sim platform layer. It is firmware-adjacent and builds with the
  firmware, so it belongs there and not under `simulator/`. Neither side names a path under `app/`
  (CLAUDE.md §20, `scripts/check-shared-placement.ts`). The bridge and `SimModule` live in `simulator/`.
- **Cryptography is the firmware's own.** The simulator links `firmware/crypto` for signature
  verification and never reimplements it in JavaScript (CLAUDE.md §22). The protocol signing keys are
  the simulator's existing test keys.
- **Constants.** A default model value with no derivation (a default NTC reading, a default impedance)
  is an unjustified choice and gets a row in `docs/status/unjustified-choices.md` in the change that
  adds it (CLAUDE.md §19).
- **Tests.** Each test of the above carries `@verifies REQ-SIMFW-nn` (CLAUDE.md §21).
- **`NP-SIM-001` and `simulator/HOWTO.md`** are updated in the change that builds each phase, and the
  standalone mode is documented as such (`D-2`).

---

## 7. Open items

| ID | Item | Closes when |
|---|---|---|
| ~~`OI-SIMFW-01`~~ | **CLOSED.** The safety MCU builds and runs on the host (§2). *The item read:* can the safety MCU firmware run host-side? | — (closed) |
| **`OI-SIMFW-09`** | **The safety MCU's host HAL.** 41 functions, including the SPI frame path, flash, OTP and the independent watchdog. The busy-poll main loop needs a pacing point that is not in the firmware source (`REQ-SIMFW-03`), so it must live in the HAL (for example a short sleep in `np_hal_get_tick_ms`). Whether the signature, thermal, charge and cardiac paths behave on the host as they do in the existing host tests is untested. | The host HAL exists and a scenario drives each interlock to its cutoff. |
| ~~`OI-SIMFW-11`~~ | **CLOSED (the principal, 2026-11-08): the simulator refuses what the device refuses.** It links the production power governor, which refuses every load (`OI-FWHUB-09`), so a PBM drive over the socket, the bridge or the page returns `NP_HUB_ERR_POWER_BUDGET` (−18) and no tile is driven. The admit-everything override and its `sim_policy` notice are gone. One seam remains, `np_sim_governor_open_for_test()`: C only, not a text command, reachable from no bridge message, used by `np_sim_pbm_tests` so the driver, rollback and fault-injection paths behind the governor stay covered; opening it is announced once as `sim_policy`. *The item read:* should the simulator admit what the device refuses? | — (closed) |
| **`OI-SIMFW-12`** | **`scripts/req-trace.ts` scans `docs/ firmware/ app/ common/ ci/` and not `simulator/`.** The JavaScript tests carry `@verifies` tags (`REQ-SIMFW-04`, `-11`, `-12`) that the report does not count, so those requirements show as tested only by the C tests, and `REQ-SIMFW-12` as untested. | `simulator/` is added to the scan, or the tests move. |
| **`OI-SIMFW-13`** | **CI.** The simulator is opt-in (`-DNP_BUILD_SIM=ON`) and in no CI job, because a CMake test target must be in `ci/host-test-partition.txt`, in three workflow counts and in `ci/host-test-floors.txt`, and the gates need their path-relevance lists widened. The JavaScript tests run by hand (`node --test`). | The tests run in a PR job, with the partition, counts and floors updated together. |
| **`OI-SIMFW-14`** | **`REQ-SIMFW-01` and `-02` have no CI check yet.** The guard is the CMake `return()` under a cross toolchain and the opt-in option. Nothing fails a device target that links `firmware/platform_sim/`, and nothing counts the seams the sim layer routes against the 119. | §8 step 2 builds both. |
| **`OI-SIMFW-15`** | **The stimulation seam contract leaves two things undefined** (§13): what `np_mod_stim_hal_read_charge` totals for a charge-balanced waveform, and what polarity 0 and 1 mean at the DAC and in the sign of `np_mod_stim_hal_read_current`. The simulator reports 0 charge for AC and a magnitude for current, and says so. | `np_sw02_platform_hal.h` defines both, and the model follows the definition. |
| **`OI-SIMFW-16`** | **An EEG stop that is lost cannot be retried** (§14). `np_mod_eeg_hal_stop_continuous` returns nothing and `np_mod_eeg_control` clears `s_recording` regardless, so a stop that did not land is neither reported nor repeated. Found by the simulator; the firmware is unchanged. | Decided: either the seam reports success and the driver keeps the flag on failure, or the lost stop is accepted and recorded as such. |
| ~~`OI-SIMFW-17`~~ | **CLOSED (2026-11-08): `np_mod_vns_control` refuses a non-finite contact impedance.** NaN and infinity now return `NP_HUB_ERR_SAFETY_REJECTED`, as an out-of-range reading does, and start nothing. The test is on the float's exponent bits, because the hub builds with `-ffast-math`. Verified by `np_mod_vns_tests` (`REQ-VNSC-07`) and by this simulator's scenario, which now asserts the refusal. Class B only; no Class C file touched. *The item read:* a non-finite contact impedance enables VNS; the range check `imp < min \|\| imp > max` is false for NaN. | — (closed) |
| **`OI-SIMFW-18`** | **A refused VNS command leaves the PPG and the EEG reference routing on, and a lost stimulator stop is not retried** (§15). | Decided with the runner in the loop: either the driver undoes what it started, or the runner is shown to stop the module after a refusal. |
| **`OI-SIMFW-19`** | **Two behaviours of `np_mod_audio.c`** (§16): an EEG band update re-applies binaural whatever the running mode, and `volume_pct` is not clamped to 100 before it reaches the planar drivers and the bone transducer. | Decided: the driver gates the update on the running mode and clamps, or the limits are shown to be enforced downstream and cited. |
| **`OI-SIMFW-20`** | **`np_mod_visual_control` ignores the status of `np_mod_visual_hal_led_set`** (§17), so a failed LED command reports success and requests the enable with nothing lit. | Decided: the status is checked and returned, or the ignored status is justified. |
| **`OI-SIMFW-21`** | **Two behaviours of `np_mod_intranasal.c`** (§18): authentication is read once at init and never again, and a bilateral command that fails on its second side leaves the first side emitting with no flag set for the stop path to act on. | Decided: the driver re-checks, or the cached answer is justified; and a partly applied command is undone or stopped. |
| ~~**`OI-SIMFW-22`**~~ | **CLOSED (fixed).** A cervical VNS session could not get from the baseline to delivery (§20): `np_cvns_session_tick`, in the BASELINE stage, asked the interlock for the enable on every tick while the baseline was valid; the second ask returned `NP_CVNS_ERR_SESSION_ACTIVE` and the stage treated that as a failure. Found by the simulator. **Fixed in `np_cvns_session.c`: the enable is requested once, when the interlock is IDLE** (`REQ-FWHUB-65`, `NP-FW-HUB-001`), with two host tests in `np_mod_cvns_tests` that drive the real driver from start to steady delivery; they fail without the fix (8 checks) and pass with it. | — (closed) |
| ~~**`OI-SIMFW-23`**~~ | **✅ CLOSED (fixed, with the principal's decision): a faulted cervical VNS session is re-armed after the lockout** (§20). `session_advance_to_fault` used to call `np_cvns_interlock_disable`, which set the interlock to IDLE and erased FAULT, so `np_cvns_interlock_request_reenable` refused every time; and `np_mod_cvns_push_ppg` dropped samples once the driver was inactive, so the baseline could not re-form. **Decision: keep FAULT latched.** `np_cvns_interlock_disable_keep_fault` sends the same DISABLE but keeps FAULT and invalidates the baseline; the lockout runs from the tick that saw the fault (`np_cvns_interlock_mark_fault_time`); the driver feeds PPG while the session is FAULT; a deliberate stop clears the latch. Re-enable needs the 30 s lockout, app confirmation and a fresh baseline, and is permission to start a NEW session: the faulted session ends and nothing is enabled (option 2; resuming the same session, option 1, is a possibility for later, `NP-FW-CVNS-001` §5.3). | Shown by `np_sim_cvns_tests` (refused inside the lockout, accepted after it with the baseline re-formed). |
| **`OI-SIMFW-10`** | **The bench apparatus (§11).** No document specifies one. The four questions are drafted for the owner as `docs/human-input/oi-simfw-10-bench-apparatus.md` (DRAFT, not sent; no answer is assumed). | §11's questions are answered by whoever owns bench and first-article test. |
| **`OI-SIMFW-02`** | **Reconcile the seam counts.** The census says 119; the scan of undefined symbols in §2 found about 94 seam-shaped names. The scan excluded `np_pbm_hal.c` and counted only what the compiled sources reference. The difference is unexplained. | The sim layer's route table accounts for every one of the 119, or the census is corrected. |
| **`OI-SIMFW-03`** | **POSIX port gaps.** `xPortIsInsideInterrupt` is undefined; `portMAX_DELAY` truncates against the 32-bit tick type. | A shim exists in `platform_sim/` (no change to the vendored kernel, which is SOUP) and the truncation is shown to be harmless or is avoided. |
| **`OI-SIMFW-04`** | **Time model.** Real time makes the run representative of task interaction but not of timing. A virtual clock gives repeatable runs but changes how the POSIX port schedules. | A choice is recorded, with whether the heartbeat and watchdog run on the same clock. |
| **`OI-SIMFW-05`** | **Storage.** littlefs needs a block device. A file-backed one is the obvious choice; whether the existing power-loss test device (`np_lfs_powerbd`) can be reused was not checked. | The block device is chosen and `REQ-SIMFW-08` is checked against it. |
| **`OI-SIMFW-06`** | **Index space and id space.** The socket dispatch is in 0-based index space and other parts of the firmware use ids. The route table must not confuse them. **Met once, and it was hit on the first run:** the page numbers sockets 1 to 80 and the firmware 0 to 79 (§12). The mapping is now one named function with a test. | The route table states the space for each seam. |
| **`OI-SIMFW-07`** | **Bridge security.** The bridge accepts commands that inject faults into a process that runs the stimulation logic. It must bind to localhost only. | Binding and origin checks are specified and tested. |
| ~~`OI-SIMFW-08`~~ | **Decided by the Principal, 2026-11-03: every module type, not only PBM** (`REQ-SIMFW-13`). *The item read:* which modules are in the first release. What remains is the order (§8) and, per module type, each seam's route and the gap list. | — (decided) |

---

## 8. Build order

1. **Spike (PBM only). Built; §12.** Host-build the hub core with a sim platform layer for the PBM seams
   (`np_mod_pbm_hal_socket_*`, `np_pbm_hal_*`) and one `SimModule` per socket. Prove one injected
   `MOD_FAULT` on one tile, visible in `FW_LOG` and in the 3D view. This also settles `OI-SIMFW-03`
   and `OI-SIMFW-05`.
2. **Census.** `REQ-SIMFW-01` and `REQ-SIMFW-02` as CI checks, before more seams are added.
3. **Every module type** (`REQ-SIMFW-13`), through the same route table, one module type at a time, each with its scenarios, its gap list and its line coverage (measured with `--coverage`, as the CI floors are). **Order, set by the Principal 2026-11-03: stim first, then the rest in the order proposed here (§13 stim, §14 EEG and §15 auricular VNS, §16 audio, §17 visual, §18 intranasal and §19 the T2 and accessory stubs, and §20 cervical VNS are built: all nine drivers).** **Proposed order**, which the Principal may change: **stim** (BES/tACS and tDCS: the charge and waveform paths, the most safety-relevant and the most sensitive to a failed answer), **EEG** (the input every closed-loop path reads), **VNS**, **audio**, **visual**, **intranasal**, **cervical VNS**, then the T2 stubs; the 1064 nm smart tile's register path (`np_pbm_hal_i2c_*`, the `unrouted` seams of the spike) alongside PBM.
4. **Safety MCU in the loop** (`OI-SIMFW-09`), and `REQ-SIMFW-07` with it.
5. **Retire the fake engine.** `SessionEngine`'s invented numbers are replaced by what the firmware
   reports. The standalone mode stays unless `D-2` says otherwise.

---

## 9. Decisions needed

| | Decision | Recommended |
|---|---|---|
| **D-1** | Native firmware process behind the WebSocket bridge, or WebAssembly in the browser | **Native.** §2 shows WebAssembly needs a custom FreeRTOS port. |
| **D-2** | Keep the standalone mode (open `index.html`, no server, no native build) beside the firmware-driven mode | **Keep it.** It is how the simulator is used for marketing and training today (`simulator/HOWTO.md` §1). |
| **D-3** | A module with no injected response answers plausibly and is marked (`REQ-SIMFW-04`), or fails loudly as the trap layer does | **Plausible and marked.** A simulator that halts on the first call shows nothing. |

---

## 10. What this document does not do

- It does not change any file under `firmware/`. The sim platform layer is a new directory.
- It does not claim the safety MCU's interlocks behave on the host as on the device (`OI-SIMFW-09`).
- It does not make the simulator a verification tool (`REQ-SIMFW-06`).
- It does not fix the order in which module types are built (§8 proposes one).

---

## 11. Reuse as a bench test apparatus

**What is on file.** No document specifies a bench test apparatus. `NP-FW-BENCH-001` designs the
head-presence gate and what bench activity needs from it. Its §8.3 states the one requirement on a rig
that matters here: a rig legitimately has modules out, and the answer is a protocol that requires only
the modules present, never a presence bypass; `SEAT#` and `np_module_map_check_placement()` are not
relaxed (`NP-FW-BENCH-001` D-7). First-article tests (`NP-FAI-001` and the per-artifact lists) are
bench tests run by hand today. The reuse below is therefore a layering that keeps the option open, not
a rig design.

**Three layers, so that one of them is the only thing that changes between simulator and rig.**

| Layer | Holds | Simulator | Bench rig |
|---|---|---|---|
| **Scenario** | A scripted sequence of `MODULE_*` messages with expected observations: "fail `socket_pwm_set` on socket 12 once; expect the other sockets stay driven and the cranial enable stays asserted" | Written once | **The same file** |
| **Bridge protocol** | `MODULE_LIST`, `MODULE_STATE`, `MODULE_SET`, `MODULE_FAIL`, `FW_LOG` (§3.5) | The browser and headless tests speak it | The rig's operator tool speaks it |
| **Platform provider** | What answers the 119 seam functions | `firmware/platform_sim/`: every module simulated | A *rig* provider: real drivers for the hardware present, the simulator's module models for the hardware absent |

**What carries over.**
- **Scenarios and the bridge** are the same code. A scenario proven in the simulator is then run on the
  rig, which is how the simulator tells a developer what to ask of the hardware.
- **Module models** stand in for what a rig lacks. A rig with three tiles and no goggles uses the
  goggle's `SimModule` to answer the hub; the hub cannot tell which seams are real.
- **The firmware process** (tasks, scheduler, log) is the same executable shape, with the provider
  swapped.

**What does not carry over.**
- **The sim platform layer never goes on a device** (`REQ-SIMFW-01`). A rig provider is a different
  layer: real drivers behind the same symbols. On a rig whose hub is the real i.MX RT1062, the firmware
  is the cross-compiled image and only the scenario and bridge layers are shared.
- **The three.js view** is a simulator feature.
- **Fault injection into live hardware is a different hazard.** In the simulator an injected fault
  hurts nothing. On a rig, a scenario that suppresses a real module's answer can leave a real emitter
  driven. `REQ-SIMFW-09` governs.

**Added requirements.**

| ID | Requirement | What fails if not met | Traceable to |
|---|---|---|---|
| **`REQ-SIMFW-09`** | A scenario run against real hardware may not inject a response that makes the hub believe a driven emitter is stopped, or a hot module cool. It may inject only the modules the rig does not have. The rig provider refuses an injection aimed at a module backed by real hardware. | A bench scenario defeats the interlock the bench exists to exercise, or leaves an emitter driven while the hub reports it stopped. | CLAUDE.md §4.2; `NP-FW-BENCH-001` D-7 and §8.2 |
| **`REQ-SIMFW-10`** | Where a scenario run on a rig produces verification evidence, the scenario runner, the bridge and the module models it relies on are validated for that use under the quality system, and the evidence names their versions. Simulator runs remain excluded by `REQ-SIMFW-06`. | A defect in shared test code is silently a defect in a verification record. | ISO 13485:2016 §4.1.6 (software used in the quality system), to be confirmed by Quality; `REQ-SIMFW-06` |

**Questions for whoever owns bench and first-article test (`OI-SIMFW-10`).** Does a rig exist or is one
planned? Is the hub on it the real i.MX RT1062, a development board, or a host process? Which
first-article items would be scripted? Is evidence from a scripted run acceptable in place of manual
steps (`REQ-SIMFW-10`)?

---

## 12. The PBM spike, as built

Built to §8 step 1. **It is a spike, not the simulator**: one modality, a single-threaded hub, no safety
MCU. What exists:

| Piece | Where | What it is |
|---|---|---|
| Sim platform layer | `firmware/platform_sim/` | Host-only (`REQ-SIMFW-01`'s guard), built only with `-DNP_BUILD_TESTS=ON -DNP_BUILD_SIM=ON`. A module bus with per-socket `np_sim_module_t` instances, injected responses and pinned readings, and the PBM socket seams (`np_mod_pbm_hal_socket_pwm_set`, `np_mod_pbm_hal_socket_ntc_read`). |
| Firmware it runs | the device's own files | `np_socket_dispatch`, `np_module_map`, `np_protocol`, `np_mod_pbm`, `np_pbm_socket_telem`, `np_safety_spi`, `np_pbm_drive`, `np_pbm_dose` and `np_pbm_detect`, listed by path in `platform_sim/CMakeLists.txt`. No firmware source file was changed (`REQ-SIMFW-03`); `firmware/CMakeLists.txt` gained one option, `NP_BUILD_SIM`, and one `add_subdirectory`. The module map learns each socket's inventory by asking the sim module, so the placement check sees the simulator's modules and nothing else. |
| Process | `np_sim_pbm_spike` | Text commands on stdin (`add`, `drive`, `stop`, `fail`, `reading`, `absent`, `present`, `clear`, `status`), JSON events on stdout. |
| Bridge | `simulator/server/fw-bridge.js` | Opt-in: `NP_SIM_FW_BIN=<binary> node index.js`. Binds `127.0.0.1` only. `MODULE_SET`, `MODULE_FAIL`, `MODULE_CLEAR` and `FW_COMMAND` in; `MODULE_LIST`, `MODULE_STATE` and `FW_LOG` out (§3.5). `FW_COMMAND` (`drive`, `stop`, `status`) is a spike convenience, standing in for the session runner. |
| Browser | `simulator/js/simmodule.js`, `helmet.js`, `api.js`, `app.js` | A `SimModule` per socket that draws itself: `HelmetModel` hands it the socket's mesh and takes nothing back. The page shows the scope notice of `REQ-SIMFW-07` while any module is attached. A socket with no sim module is drawn as before. |

**Run it.**

```bash
cmake -S firmware -B build/sim -DNP_BUILD_TESTS=ON -DNP_BUILD_SIM=ON
```

```bash
cmake --build build/sim --target np_sim_run_tests np_sim_pbm_spike
```

```bash
NP_SIM_FW_BIN=$PWD/build/sim/platform_sim/np_sim_pbm_spike node simulator/server/index.js
```

```bash
NP_SIM_FW_BIN=$PWD/build/sim/platform_sim/np_sim_pbm_spike node --test simulator/js/simmodule.test.mjs simulator/server/fw-bridge.test.js
```

**What was checked.** The C tests drive the firmware through seven scenarios (baseline drive, default
answers marked, an injected write fault and its rollback, isolation between modules, a hot tile refused, an
absent tile failing placement before any write, and a lost stop). The JavaScript tests include the real
firmware process through the bridge into the browser module registry. In headless Chromium, with
three.js served locally because the CDN was unreachable, the real page connected to the real server and
firmware: driven tiles drew red, and a fault injected on one tile drew it magenta while every other tile
went dark after the rollback. **Not checked:** a real GPU, other browsers, anything beyond the "Inside"
view, and any behaviour with the kernel's tasks running.

**What it found, which is what it is for.**

| Finding | Meaning |
|---|---|
| **The page numbers sockets 1 to 80 and the firmware 0 to 79.** The first run drove nothing visible. | `OI-SIMFW-06` was not hypothetical. The mapping is now one named function and a test. |
| **A command driving more than 32 tiles was refused and rolled back (fixed).** 8, 16, 24 and 32 tiles drove; 40 and 80 returned `NP_HUB_ERR_MOD_FAULT` with nothing left driven. The limit was `NP_PBM_SESSION_MAX_ACTIVE_SOCKETS`, with `NP_PST_MAX_TRACKED` asserted equal to it. | **A requirements gap, ruled so on 2026-11-01, and closed at `NP-FW-HUB-001`** (`REQ-FWHUB-64`, `OI-FWHUB-27`, register row `UC-119` retired): both constants are now the 7-bit address domain, 128. The simulator now drives all 80 sockets of the version 1 lattice in one command and refuses an address past it as not present. A unit test of the dispatcher did not see this; a whole-lattice run did. |
| **The production governor refuses every load, and the simulator now does too.** | No tile can be driven from the page or the bridge, as on the helmet (`OI-SIMFW-11`, closed). Lattice-wide scenarios (tile faults, hot tiles, rollback) run only in the C tests, which open the governor, until `OI-FWHUB-09` lands. |
| **A failed stop leaves the emitter driven.** With a stop failing on one tile, the hub reports no active socket and drops the cranial enable, and the module still holds its drive. | The designed backstop (`np_socket_dispatch.h`: the whole-lattice cut) and exactly what the safety MCU's enable is for. With no safety MCU in the loop the simulator can show the hub's side of it and not the cut. |

**What it does not do.** Smart (1064 nm) tiles: `np_pbm_hal_i2c_*` and the slot-indexed PBM seams report
themselves as `unrouted` and fail. No other modality. No safety MCU (`OI-SIMFW-09`). No FreeRTOS tasks and
no session runner (the spike calls the dispatcher directly). No persistence: the hex-map store and
the protocol identity seams are spike-local stand-ins, named in `np_sim_spike_link.c`. Numeric status codes
only: the generated name table of `REQ-SIMFW-05` is not built.

---

## 13. Stimulation, as built (§8 step 3, first module type)

BES/tACS and tDCS, `REQ-SIMFW-13`. Built on the same bus; the bus is now keyed by **(kind, index)**, so an electrode pair and a
socket may share an index without meeting (`NP_SIM_KIND_STIM_PAIR`; the isolation test covers it).

| Piece | Where | What it is |
|---|---|---|
| Seams | `platform_sim/src/np_sim_stim.c` | The seven `np_mod_stim_hal_*` seams of `np_sw02_platform_hal.h`, each answered from an injected response, a pinned reading, or the default model. |
| Default model | same | Delivered current: the commanded peak for AC (the quantity `np_stim_xcheck` compares, `OI-STIM-06`); for DC a straight line from the present level to the target over the commanded seconds. Charge: the exact integral of the DC magnitude, saturating at the seam's 16 bits. Impedance: whatever the scenario gave the pair, no default. |
| Clock | `np_sim_advance_ms` | Virtual after the first advance, so a 30 s ramp is deterministic and a scenario runs at once. Real until then. |
| Harness | `platform_sim/src/np_sim_stim_harness.c` | Text commands that call the driver's every entry point (`control`, `telemetry`, `tick`, `tdcs_pause`, `tdcs_ramp`, `tdcs_ramp_up_ms`, `detect`, `shutdown`) and the runner's two call sites around the cross-check. |
| Firmware it runs | the device's own files | `np_mod_stim.c`, `np_stim_xcheck.c`, `np_ramp_plan.c` and the stimulation half of `np_safety_spi.c` (`request_enable`, `request_disable`, `set_channel_current`). No firmware file changed. |
| Bridge | `fw-bridge.js` | `kind: "stim_pair"` in `MODULE_SET`, `MODULE_FAIL`, `MODULE_CLEAR`, and `FW_COMMAND` `bes`, `tdcs`, `stim_stop`, `stim_shutdown`, `sample`, `tdcs_pause`, `stim_session_start`, `advance`. Built from validated integers and whitelisted names (`REQ-SIMFW-12`); the hostile-input test covers them. |
| Tests | `platform_sim/tests/np_sim_stim_tests.c` | Seventeen scenarios, 66 checks: drive and stop, the 1 mA cap reaching the DAC, a failed DAC write, an absent pair, over-delivery flagged once, a failed read counted against the bound, the tDCS ramp and its charge (0.5 x 1000 uA x 30 s = 15000 uC), the 30 s floor and the undeclared pad area, stop and its hold, pause and resume, the pause edges, the cross-check's envelope across commands and sessions, malformed commands, the probe, and the isolation and marking properties. |

**Line coverage** (`firmware/platform_sim/tools/sim-coverage.sh`, gcov, the scenario tests only):

| Source | Lines reached |
|---|---|
| `hub_control/modules/np_mod_stim.c` | 99.4 % of 158 (the one line missed is an unreachable `default:` of `td_level`) |
| `hub_control/src/np_stim_xcheck.c` | 93.7 % of 63 |
| `hub_control/modules/np_mod_pbm.c` (the spike, for comparison) | 24.5 % of 184 |
| `hub_control/src/np_socket_dispatch.c` | 51.1 % of 180 |
| `hub_control/src/np_module_map.c` | 21.4 % of 364 |

**Gaps, listed as `REQ-SIMFW-13` requires.**

| Not modelled or not run | Why |
|---|---|
| The session runner's use of the driver | The runner is not in the loop. The harness copies its two call sites (tick before a command, telemetry then `np_stim_xcheck_observe`), so a defect in the runner's own call is not seen. |
| The registry and its probe table | The driver is called directly. `np_mod_stim_detect` and the entry points are run, but the table that routes slot 17 and 18 to them is `np_module_registry.c`. |
| The SHDR fault the cross-check raises | The flag is returned to the runner, which writes it. The harness reports the flag; the log call is the runner's. |
| What a charge-balanced waveform's "session total" charge is | The seam says only "session total". The model reports 0 for AC and the scenario test says so. An open question, not a number. |
| What polarity 0 and 1 mean at the DAC | The seam does not say. Delivered current is reported as a magnitude. The cross-check takes the magnitude too. |
| Waveform shape, frequency accuracy and phase | The model carries the commanded peak only. `np_mod_bes_tacs_params_t.phase_deg` is never passed to the DAC by the driver. |
| The safety MCU's own charge ceiling | Not in the loop (`OI-SIMFW-09`). Every `result` says `safety_mcu: not_in_loop`, and the granted enables are therefore 0. |
| Non-stimulation records and a null record in `np_stim_xcheck_observe` | The harness cannot construct them; the firmware's own `np_stim_xcheck_tests` does. |
| Browser drawing of an electrode pair | Not built. The page draws tiles; a stimulation pair has no place on the lattice. Needs a decision on where a pair is drawn (§3.3). |

**What it found.** Nothing in the firmware. Two things in the seam contract: the charge and the polarity are
defined nowhere (above), so any hardware or simulator answers them by guesswork. `OI-SIMFW-15`.

---

## 14. EEG, as built (§8 step 3, second module type)

The ADS1299 front end, `REQ-SIMFW-13`. One part, so one module (`NP_SIM_KIND_EEG`, index 0). Same bus, same injection, same marking.

| Piece | Where | What it is |
|---|---|---|
| Seams | `platform_sim/src/np_sim_eeg.c` | The eight `np_mod_eeg_hal_*` seams. **The model is a register file**: a write lands in it, a read returns it, the ID register is read-only and reads the value the scenario gave the part (the datasheet's `0x3E` by default). The driver's configuration is therefore visible as what it wrote, not as a flag the simulator computed. |
| Scenario state | `eeg_impedance`, `eeg_band` | Impedance per channel and power per band, set by the scenario; no default (`CLAUDE.md` §19). The dominant band is the first of the five with the highest power, unless a reading is pinned. |
| Injection | `fail`, `reading` | Four of the eight seams return a byte or nothing, so a code cannot come back. **An armed response makes the call not happen**: a write is dropped, a start or stop ignored, a calibration does not run, a read returns 0. Each is reported as injected, with the code. |
| Harness | `platform_sim/src/np_sim_eeg_harness.c` | `eeg_detect`, `eeg_init`, `eeg`, `eeg_stop`, `eeg_shutdown`, `eeg_sample`, `eeg_regs`, `eeg_raw`. |
| Firmware it runs | the device's own file | `np_mod_eeg.c`. No firmware file changed. |
| Bridge | `fw-bridge.js` | `kind: "eeg"` in `MODULE_SET` (`add`, `present`, `impedance`, `band`, `reading`), `MODULE_FAIL`, `MODULE_CLEAR`, and `FW_COMMAND` `eeg`, `eeg_init`, `eeg_detect`, `eeg_stop`, `eeg_shutdown`, `eeg_sample`, `eeg_regs`. |
| Tests | `platform_sim/tests/np_sim_eeg_tests.c` | Ten scenarios, 32 checks: probe by ID, the power-on register sequence, enabled and powered-down channels, stop, a lost stop, a lost start, telemetry, a malformed command, a failed calibration, an absent front end, the marking and isolation properties. |

**Line coverage:** `hub_control/modules/np_mod_eeg.c` 100 % of 56.

**Gaps.**

| Not modelled or not run | Why |
|---|---|
| The sample stream | The DMA ring and the 500 Hz waveform are not behind these seams. The simulator produces no EEG signal, so nothing downstream that reads one (the adaptive outputs, the recording to EDF+, the photoparoxysmal detector) can be exercised by it. |
| How the dominant band is chosen | The seam says "dominant" and no more. The first-highest reading is this model's. |
| The adaptive output | `adaptive_out` is stored by the driver and acted on by the runner, which is not in the loop. |
| Reference mode, notch and the lead-off registers | The driver stores them and never writes the ADS1299 for them (`ref_mode`, `notch` are not in the register sequence). Not modelled because the driver does not drive them. |

**What it found.** A lost stop cannot be retried (`OI-SIMFW-16`): `np_mod_eeg_control` clears its recording flag
whether or not the stop took, and the seam returns nothing, so the driver reports success while the converter runs,
and a second stop does nothing because the flag is already clear. The test `lost stop` shows both. Whether it
matters depends on what runs behind the converter on a real board; it is stated here so it is decided and not
discovered.

---

## 15. Auricular VNS, as built (§8 step 3, third module type)

The ear clip, `REQ-SIMFW-13`. One clip, so one module (`NP_SIM_KIND_VNS`, index 0); its two sides are state inside it.

| Piece | Where | What it is |
|---|---|---|
| Seams | `platform_sim/src/np_sim_vns.c` | The nine `np_mod_vns_hal_*` seams (the tenth, `np_mod_vns_hal_now_ms`, is the simulator's clock). The stimulator records what it was told, per side (0 left, 1 right, 2 both). Contact impedance and the three HRV readouts are what the scenario gave the clip; no default. **A removed clip, or a read that fails, answers 0 ohm**, below the driver's lower contact bound, because the seam returns a bare float and an answer inside the bounds would say contact exists. |
| Firmware it runs | the device's own files | `np_mod_vns.c` **and the real HRV session library** (`hrv_biofeedback/`: session, PPG, coherence, pacer, taVNS sync, EEG biofeedback). The library is pure C with no kernel and no seam, so it runs as it is: the driver creates and starts a real session on `hrv_proto > 0`, and stopping the module ends it and hands the library's finished record to the logger once. No firmware file changed. |
| Harness | `platform_sim/src/np_sim_vns_harness.c` | `vns_detect`, `vns_init`, `vns`, `vns_stop`, `vns_shutdown`, `vns_sample`, `vns_raw`, and scenario state `vns_readouts`. |
| Bridge | `fw-bridge.js` | `kind: "vns"` in `MODULE_SET` (`add`, `present`, `readouts`, `reading`), `MODULE_FAIL`, `MODULE_CLEAR`, and `FW_COMMAND` `vns`, `vns_init`, `vns_detect`, `vns_stop`, `vns_shutdown`, `vns_sample`. |
| Tests | `platform_sim/tests/np_sim_vns_tests.c` | Twelve scenarios, 34 checks: the contact-range probe, drive and stop on either or both ears, the 40 mA cap reaching the stimulator and the safety channel, no contact meaning no stimulation, a non-finite impedance, a failed stimulator write, a lost stop, the real HRV session's lifecycle, telemetry, a malformed command, shutdown, and the marking and isolation properties. |

**Line coverage.**

| Source | Lines reached |
|---|---|
| `hub_control/modules/np_mod_vns.c` | 98.7 % of 78 |
| `hrv_biofeedback/src/np_hrv_session.c` | 40.6 % of 128 |
| `np_hrv_ppg.c`, `np_hrv_coherence.c`, `np_hrv_pacer.c`, `np_hrv_tavns_sync.c` | 13.2 %, 12.4 %, 22.7 %, 21.1 % |
| `np_hrv_eeg_biofeedback.c` | 0 % |

**Gaps.**

| Not modelled or not run | Why |
|---|---|
| The HRV session after it starts | The driver owns the only handle to the session and nothing in the loop calls `np_hrv_session_tick`, `push_ppg` or `push_eeg`. The library runs its create, start, stop and finish, and nothing between. The low figures above are this gap, measured. It closes when the runner, or a PPG source, is in the loop. |
| A PPG waveform | The HRV readouts are scenario state, not derived from a signal, so a readout is not consistent with any pulse train. |
| taVNS gated to the breathing phase | That path is the HRV library's, behind a session that never ticks. |
| The safety MCU's contact check and cardiac interlock | Not in the loop (`OI-SIMFW-09`); every result says `safety_mcu: not_in_loop`. |
| What a lost stimulator stop leaves | Modelled as the stimulator still driving; whether the real part would is not known. |
| Browser drawing of the clip | Not built (as for the electrode pairs, §13). |

**What it found.**

| Finding | Meaning |
|---|---|
| ~~**A NaN contact impedance passes the driver's range check and enables stimulation**~~ (`OI-SIMFW-17`, **fixed 2026-11-08**). `np_mod_vns_control` refused when `imp < min \|\| imp > max`; both are false for NaN. | The driver now refuses a non-finite reading with the out-of-range status (§7). The scenario that recorded the defect asserts the refusal. |
| **A refused command leaves the PPG and the EEG reference routing on, and a lost stimulator stop is neither reported nor retried** (`OI-SIMFW-18`). The driver starts both before the stimulator write that can fail, and its stop path always reports success. | The runner is expected to stop the module after a refusal; that is the runner's behaviour, not in the loop yet. |

---

## 16. Audio, as built (§8 step 3, fourth module type)

The codec, the planar drivers and the bone transducer, `REQ-SIMFW-13`. One path, so one module (`NP_SIM_KIND_AUDIO`, index 0).

| Piece | Where | What it is |
|---|---|---|
| Seams | `platform_sim/src/np_sim_audio.c` | The eight `np_mod_audio_hal_*` seams. The model records what the driver told the output stage: planar mode (off, binaural, isochronic, pink or brown noise), carrier, beat and volume, and the bone transducer's frequency and volume. Mesh impedance is what the scenario gave the part, no default. Five seams return nothing, so an armed response makes the call not happen (as for the EEG front end); `codec_init` returns its armed status. |
| Firmware it runs | the device's own file | `np_mod_audio.c`. No firmware file changed. |
| Harness | `platform_sim/src/np_sim_audio_harness.c` | `audio_detect`, `audio_init`, `audio`, `audio_stop`, `audio_shutdown`, `audio_eeg_band` (the call the EEG telemetry path makes), `audio_sample`, `audio_raw`. |
| Bridge | `fw-bridge.js` | `kind: "audio"` in `MODULE_SET` (`add`, `present`, `reading`), `MODULE_FAIL`, `MODULE_CLEAR`, and `FW_COMMAND` `audio`, `audio_eeg_band`, `audio_init`, `audio_detect`, `audio_stop`, `audio_shutdown`, `audio_sample`. |
| Tests | `platform_sim/tests/np_sim_audio_tests.c` | Eleven scenarios, 36 checks: bring-up, every mode, bone conduction, stop and the two lost stops, EEG-adaptive beat selection, the two findings below, a malformed command, telemetry, and the marking and isolation properties. |

**Line coverage:** `hub_control/modules/np_mod_audio.c` 100 % of 60.

**Gaps.**

| Not modelled or not run | Why |
|---|---|
| Any sound | No sample path and no sound pressure level. Volume is a percentage the driver passed on. Nothing about loudness, which is the audio modality's safety question, can be seen here. |
| The mesh-fouling decision | The driver reads the impedance and discards it ("threshold comparison and SHDR log handled by session_log"); the comparison is not in `np_mod_audio.c`, so no scenario here reaches it. |
| The EEG-to-audio loop | `audio_eeg_band` is the call; who makes it, from which EEG record, is the runner's, which is not in the loop. |

**What it found.** `OI-SIMFW-19`, two things in `np_mod_audio.c`, both recorded by tests that say so:
a band update from an EEG-adaptive session re-applies **binaural** whatever the running mode was, so an isochronic or noise
session becomes binaural; and `volume_pct` is passed to both outputs without a clamp, so 200 reaches the transducers
(the field is documented 0 to 100).

---

## 17. Visual goggles, as built (§8 step 3, fifth module type)

The goggles, `REQ-SIMFW-13`: the module with the IEC 62471 interlocks, so the one where real code matters most. One pair, one module (`NP_SIM_KIND_VISUAL`, index 0).

| Piece | Where | What it is |
|---|---|---|
| Seams | `platform_sim/src/np_sim_visual.c` | The seven `np_mod_visual_hal_*` seams. Seated, eye-open and MPE-ok are what the scenario sets. **The three boolean seams answer false for an injected failure or an absent goggle**, the refusing answer, so a fault cannot read as permission. The LEDs record what the driver commanded. No optical power, irradiance or MPE: the hardware ceiling is not modelled, so `mpe_ok` is a switch. |
| **The Hall edge runs the driver's own handler** | `np_sim_visual_lift()` | A lift clears the seated reading and **calls the handler `np_mod_visual.c` registered** (`hall_unseated_isr`, with `np_mod_visual_hal_led_stop`, `np_safety_spi_request_disable_from_isr` and the `s_hall_cut` latch), the way the platform interrupt would. A registration the scenario failed leaves no handler: "a return other than `NP_HUB_OK` means no edge will be delivered". `vis_lift_during_command` delivers the lift between the seated read and the enable, the window the latch exists for. `vis_lift_silent` lifts with no edge, so only the telemetry poll sees it. |
| Firmware it runs | the device's own file | `np_mod_visual.c`. No firmware file changed. `np_runner_halt`, which `np_mod_visual_ppx_halt` calls (the detector's hold, #789 to #791), is a **sim double** in `np_sim_spike_link.c` that reports the request; `np_ppx_policy.c` and `np_ppx_detect.c` are linked as the device's own files (first-party code, listed below). |
| Harness | `platform_sim/src/np_sim_visual_harness.c` | `vis_detect`, `vis_init`, `vis`, `vis_stop`, `vis_shutdown`, `vis_lift`, `vis_lift_silent`, `vis_lift_during_command`, `vis_lower`, `vis_ppx`, `vis_sample`, `vis_raw`, `vis_sensor`. |
| Bridge | `fw-bridge.js` | `kind: "visual"` in `MODULE_SET` (`add`, `present`, `sensor`, `reading`), `MODULE_FAIL`, `MODULE_CLEAR`, and the `FW_COMMAND`s above. |
| Tests | `platform_sim/tests/np_sim_visual_tests.c` | Fourteen scenarios, 45 checks: arming, no emission without an armed handler, every mode, each interlock, faulted sensors, the edge cut, a missed edge caught by the poll, the lift between read and enable, a failed LED command, a lost LED stop, the photoparoxysmal halt, a malformed command, and the marking and isolation properties. |

**Line coverage:** `hub_control/modules/np_mod_visual.c` 96.6 % of 87.

**Gaps.**

| Not modelled or not run | Why |
|---|---|
| **Mode F (NIR retinal walk)** | Compiled out: `NP_MODE_F_REGULATORY_CLEARED` is 0 (`np_hub_config.h`), so `mode_f_enable` is ignored and the test says so. Running it means building with the flag set, which is a regulatory decision, not a simulator one. This is most of the uncovered lines. |
| Optical output and the hardware MPE ceiling | Not modelled; `mpe_ok` is a switch. |
| The runner | `np_runner_halt` is a double that reports the request. What the runner does with it is not seen. |
| The EEG path that detects a photoparoxysmal pattern | `vis_ppx` is the call that path makes; the detector is not in the loop (and the EEG model produces no signal, §14). |
| The edge-to-dark time | The sim delivers the edge synchronously, so it measures nothing (`OI-FWHUB-26` owns the measurement). |
| Shade control | `shade_req` is carried in the command and never read by the driver. |
| The safety MCU's visual interlocks | Not in the loop (`OI-SIMFW-09`). |

**What it found.** `OI-SIMFW-20`: **`np_mod_visual_control` ignores the status of `np_mod_visual_hal_led_set`.** A failed LED command reports success and requests the safety enable with nothing lit. The direction is the safe one (dark LEDs), but the session and the hub's enable request both say the goggles are running.

---

## 18. Intranasal Y-probe, as built (§8 step 3, sixth module type)

The probe, `REQ-SIMFW-13`. One probe, so one module (`NP_SIM_KIND_INTRANASAL`, index 0), with two sides.

| Piece | Where | What it is |
|---|---|---|
| Seams | `platform_sim/src/np_sim_intranasal.c` | The five `np_mod_ins_hal_*` seams. Sleeve authentication and photodiode contact are what the scenario sets, and **answer false for an injected failure or an absent probe**. The emitters record what each side was told. The dose ADC is the counts the scenario gave each side and wavelength; the model does not derive a photodiode reading from an emitter setting. |
| Injection with a start offset | `fail <index> <op> <code> <times> [after]` | New with this module and for every kind: **answer `after` calls normally, then fail**. It is what "fail the second side" needs, and it exists on the bridge as `MODULE_FAIL.after`. |
| Firmware it runs | the device's own file | `np_mod_intranasal.c`. No firmware file changed. `np_log_shdr_zone_auth` is a logging sink in `np_sim_spike_link.c` that reports the authentication record as it is written. |
| Harness | `platform_sim/src/np_sim_intranasal_harness.c` | `ins_detect`, `ins_init`, `ins`, `ins_stop`, `ins_shutdown`, `ins_sample`, `ins_raw`, and scenario state `ins_sensor`, `ins_pd`. |
| Bridge | `fw-bridge.js` | `kind: "intranasal"` in `MODULE_SET` (`add`, `present`, `sensor`, `pd`, `reading`), `MODULE_FAIL`, `MODULE_CLEAR`, and `FW_COMMAND` `ins`, `ins_init`, `ins_detect`, `ins_stop`, `ins_shutdown`, `ins_sample`. |
| Tests | `platform_sim/tests/np_sim_intranasal_tests.c` | Ten scenarios, 25 checks. |

**Line coverage:** `hub_control/modules/np_mod_intranasal.c` 100 % of 47.

**Gaps.** Optical power and the dose a photodiode would integrate (the dose telemetry is a placeholder constant in the driver, so the figure here is that constant applied to scenario counts); the dock; the sleeve's optical code itself (authentication is a boolean); the safety MCU's intranasal enable (not in the loop, `OI-SIMFW-09`); the hygiene-sleeve consumable accounting, which is not in the driver.

**What it found.** `OI-SIMFW-21`, two things in `np_mod_intranasal.c`, both recorded by tests that say so:
**authentication is read once, at init**, so a probe whose sleeve or contact has since gone is still driven; and **a bilateral command that
fails on the second side leaves the first side emitting**: the driver returns the fault, never sets its active flag, requests
no enable, and its stop path only acts when the flag is set, so the left side cannot be stopped by the driver at all.

---

## 19. T2 and accessory stubs, as built (§8 step 3, last module type)

`np_mod_t2_stubs.c`, `REQ-SIMFW-13`: the 21-channel qEEG cap, the TMS coil, the 1170 nm laser, clinical tACS, HD-tDCS and the
vibrotactile pad. The module has **no seam**: every function is a placeholder that answers `NP_HUB_ERR_NOT_PRESENT`
(shutdown answers success). The simulator equivalent of a module that refuses is to call it and say what it answered and what it
asked of the safety MCU. `stub <name> detect|init|control|telemetry|shutdown` (`np_sim_stubs_harness.c`), bridge `FW_COMMAND` `stub`.
Three scenarios, 6 checks, `np_mod_t2_stubs.c` 100 % of 30: all six refuse on every operation, none requests a safety enable (so no T2 line
is asked for), and all six shut down successfully, which is what lets the runner stop a module it never started.

**Gap.** There is nothing to simulate until a driver exists; each stub becomes a module of its own, with its own section, when it does.
**This closes the list of nine drivers except cervical VNS, which is next (§8).**

---

## 20. Cervical VNS, as built (§8 step 3, last module type)

The T2 cervical VNS accessory, `REQ-SIMFW-13`: the most complicated driver, a state machine of impedance check, cardiac baseline,
enable grant, ramp and delivery, around a safety library. One accessory, one module (`NP_SIM_KIND_CVNS`, index 0).

| Piece | Where | What it is |
|---|---|---|
| Seams | `platform_sim/src/np_sim_cvns.c` | The six seams the driver and the re-enable manager call: accessory presence, the two clocks, the hub-side per-electrode impedance measurement, and the re-enable gate's fresh impedance verdict. Impedance and the wall-clock base are what the scenario gives the accessory. |
| Firmware it runs | the device's own files | `np_mod_cvns.c`, **the whole cervical VNS library** (`cervical_vns/`: session, cardiac interlock with its Pan-Tompkins detector, stimulation ramp), `np_cvns_reenable.c` and `np_cvns_fault_summary.c`. No firmware file changed. `np_hub_cvns_reenable_confirm` (a one-line call into the real re-enable manager), `np_hub_set_active_user`, `np_log_session_count` and the fault summary's RAM-backed persistence are fillers in `np_sim_spike_link.c`. |
| **The scenario plays two parts** | `np_sim_cvns_harness.c` | **The safety MCU:** `cvns_mcu` and `cvns_mcu_imp` publish a heartbeat status and an impedance report through the driver's own entry points (`np_mod_cvns_set_heartbeat_status`, `np_mod_cvns_set_mcu_impedance`), the way the safety-heartbeat task does. **The PPG ISR:** `cvns_run <ms> <bpm>` pushes a deterministic pulse train through `np_mod_cvns_push_ppg` at 200 Hz and ticks the driver every 100 ms, the way the runner does. Every state event says `safety_mcu: "scenario"`. |
| Bridge | `fw-bridge.js` | `kind: "cvns"` in `MODULE_SET`, `MODULE_FAIL`, `MODULE_CLEAR`, and `FW_COMMAND` `cvns`, `cvns_mcu`, `cvns_mcu_imp`, `cvns_run`, `cvns_init`, `cvns_detect`, `cvns_stop`, `cvns_suspend`, `cvns_shutdown`, `cvns_reenable`, `cvns_sample`, `cvns_state`. |
| Tests | `platform_sim/tests/np_sim_cvns_tests.c` | Twenty-two scenarios, 50 checks: a command before init, probe and init, six out-of-range descriptors, a start that enables nothing, the hub impedance step and its need for a heartbeat, a high-impedance electrode, impedance cross-validation, the nominal fallback, the real detector finding a 70 BPM baseline, **baseline to delivery** (ramp, steady delivery, the published current), waiting for the grant, the safety MCU refusing the enable, R-peak data loss in delivery, a heart-rate step, stop in delivery, completion at the session length, the re-enable path, stop, suspend and shutdown, a malformed command, and the marking and isolation properties. |

**Line coverage.**

| Source | Lines reached |
|---|---|
| `hub_control/modules/np_mod_cvns.c` | 91.8 % of 280 |
| `cervical_vns/src/np_cvns_interlock.c` | 77.1 % of 188 |
| `cervical_vns/src/np_cvns_session.c` | 83.8 % of 198 |
| `cervical_vns/src/np_cvns_stim.c` | 75.9 % of 133 |
| `hub_control/src/np_cvns_fault_summary.c` | 7.5 % of 186 |
| `hub_control/src/np_cvns_reenable.c` | 0 % of 84 |

**Gaps.**

| Not modelled or not run | Why |
|---|---|
| **The safety MCU's cardiac excursion cutoff** | A heart-rate step reaches the library and nothing happens on this side: the main-processor interlock has no excursion cutoff of its own, only data loss (`NP-FW-CVNS-001` §6). The cutoff that matters is the Class C safety MCU's, which is not in the loop (`OI-SIMFW-09`), so the simulator cannot show a cardiac cutoff at all. The scenario says so. |
| What the library asks of the stimulator and the safety MCU | The library reaches both through `static` no-op functions inside its own sources (`platform_set_current`, `platform_safety_mcu_request_enable`, `platform_rpeak_gpio_pulse`, ...), not link-time seams, and `REQ-SIMFW-03` forbids changing a firmware file. The simulator sees the library's state and the driver's enable and published current, not the current the library requests or the R-peak pulses it makes. |
| The re-enable manager's three gates | `np_cvns_reenable.c` is driven by the heartbeat task in `np_hub_control_main.c`, which is not in the loop. 0 % is that. |
| The fault summary | Recorded when a fault reaches the driver's callback; the persisted blob is RAM and the notification is reported, not delivered to a central. |
| A real photoplethysmogram | The pulse train is a smooth pulse per beat on a flat baseline: a signal the detector can find, not a model of blood flow. |
| The safety MCU | The scenario stands in for it (`OI-SIMFW-09`). |

**What it found.**

`OI-SIMFW-22` (**fixed**, `REQ-FWHUB-65`): a session started through the real driver could not reach ramp-up. Reproduced in a trace and by the bridge:

| t | stage | interlock | enable requested | what happened |
|---|---|---|---|---|
| tick N | BASELINE | IDLE | no | the baseline reaches five beats between ticks |
| tick N+1 | BASELINE | PRE_SESSION | **yes** | `np_cvns_session_tick` calls `np_cvns_interlock_request_enable`: it succeeds and sends the baseline and the enable request |
| tick N+2 | **FAULT** | IDLE | no | the BASELINE case called `np_cvns_interlock_request_enable` **again**, because the baseline was still valid and the stage had not changed; it returned `NP_CVNS_ERR_SESSION_ACTIVE`, which the case treated as any failure and faulted the session (`abort_reason` 6) |

It happened with the safety MCU's grant present from the start. The unit tests did not see it because they place the session at `RAMP_UP` or `ACTIVE` directly. **The fix** is in `np_cvns_session.c`: the BASELINE stage requests the enable once, when the interlock is IDLE, and waits for the grant while it is PRE_SESSION. Two host tests (`np_mod_cvns_tests`) drive the real driver from start through baseline, enable request and grant to steady delivery, and a session waiting for the grant without faulting; **both fail without the fix (8 checks) and pass with it**, the whole host suite passes (64 of 64), and the simulator's scenarios now reach ramp-up, steady delivery, R-peak data loss, the safety MCU's refusal, stop in delivery and completion at the session length. The cardiac-interlock logic itself (the safety MCU, Class C) is untouched.

`OI-SIMFW-23` (**closed**): a faulted session now keeps the interlock's FAULT latched (`np_cvns_interlock_disable_keep_fault`), the lockout runs from the tick that saw the fault, the driver keeps feeding PPG while the session is FAULT so the baseline re-forms, and a deliberate stop clears the latch (the principal's decision: keep FAULT latched). Re-enable is refused inside the 30 s lockout and accepted after it with a fresh baseline, which ends the faulted session and permits a new one to start (option 2; `np_sim_cvns_tests`). A stop clears the latch, so stop-then-start skips the lockout: recorded as a possible future change and not made, pending actual use (`NP-FW-CVNS-001` §5.3). Resuming the same session (option 1) is a possibility for later.

---

## 21. Accessories in the browser, as built (the Principal, 2026-11-05)

The rule: **an accessory is drawn only while it is in use; one that attaches directly to the helmet is drawn on the helmet, as attached;
every other accessory gets its own window, along the perimeter of the helmet window.** This closes the "browser drawing" gap of §13 to §20.

| Piece | Where | What it is |
|---|---|---|
| The classification and the rules | `simulator/js/accessories.js` | Pure functions, tested in node. **Attached to the helmet:** visual goggles (snap-on lens mounts), audio cups ("mounts to the headset arms", `NP-HW-AUDIO-001` §3), the TMS coil, the mastoid haptic pad, and the tDCS / tACS electrodes at their 10-20 sites. **Tethered to the hub's accessory port, so a window each:** the auricular VNS clip (`NP-HW-VNSCLIP-001`), the intranasal Y-probe (`NP-HW-NASAL-001` A12.7), the cervical VNS pads (`NP-HW-CVNS-001` A14.4). `accessoryStates()` says what is in use; `windowSlots()` says where each window goes. |
| "In use" has one source at a time | same | **With firmware modules attached, only the firmware's own record says**: a module is in use while it is driven (the cervical VNS while its session is active, which is earlier than its enable). Without them, the standalone session says. An accessory the firmware has no driver for (the TMS coil, the haptic pad) is never in use in firmware mode. The two sources are never mixed, the rule `SimModule` already follows for tiles. |
| The registry is keyed by (kind, index) | `simulator/js/simmodule.js` | Until now by index alone, which was correct while tiles were the only module; with the firmware bus keyed by kind and index, a stimulation pair 0, the EEG front end and a tile in socket 0 would have been one module. `byKind()` added; `bySocket` and `byLatticeId` are the tile lookups, unchanged. |
| The windows | `simulator/js/acc-windows.js`, `css/app.css` | One window per tethered accessory, hidden unless in use, with a schematic drawing whose parts light from the module's record (which ear, which nostril, the pads by stage: yellow while checking, cyan while delivering, red on a fault) and one line of what it is doing. Slots: top-right, bottom-right, bottom-left, then top-left and the middle of each edge when the window is tall enough; an accessory keeps its slot; windows never cover the centre, where the helmet is, and shrink (to 55 %) on a small window. |
| On the helmet | `simulator/js/helmet.js` | `setAccessoriesInUse()` and `_drawAttachedAccessories()`. The goggles, audio cups, haptic pad and TMS coil are shown only while in use; **the stimulation electrodes are new**: discs at the 10-20 sites of each driven pair (pair 0 = F3-F4, 1 = P3-P4, 2 = Fz-Pz), tDCS steady with the anode amber and the cathode blue, BES / tACS pulsing cyan. The 3D VNS clip and Y-probe (and its insertion animation) are **removed from the scene**: they are windows now. |
| The server | `simulator/server/fw-bridge.js` | Module state reaches the page only as `module_state` events, which the firmware emits on a `status` command. A command changes state without the firmware volunteering it, so the real server asks for `status` 40 ms after a command (`autoStatus`, off in the tests that count events). |
| The panel | `simulator/index.html`, `js/ui.js` | One demo switch remains, the haptic pad, which no shipped protocol drives; it says "in use" so it can be seen without the firmware. The standalone session now derives the intranasal probe, the cervical VNS pads and the haptic pad from the protocol's `pbm_intranasal`, `cervical_vns` and `vibrotactile_40hz` modalities. Each window's drawing is a labelled schematic (numbered markers, a legend naming each part and its A12.x, A13.x or A14.x row). |
| Tests | `simulator/js/accessories.test.mjs` (11), `simmodule.test.mjs`, and a headless-Chromium run of the real page against the real server and firmware | The rules (what is in use from each source, the slots around the perimeter, that none overlaps another or the centre, that a window keeps its slot), the kind-keyed registry, and the page: nothing drawn when idle or when modules are present and not driven; the three windows appear when the VNS clip, the Y-probe and the cervical VNS are driven, none covers the centre, none of them is drawn on the helmet; the goggles, the audio cups and the stimulation electrodes appear on the helmet when driven; after a stop nothing is drawn. |

**Gaps.** The drawings are schematic, not the hardware's geometry. The windows are not draggable or resizable. The EEG front end, a PBM tile and the hub itself are not accessories and keep their own drawing. The haptic pad and the TMS
coil have no firmware driver (stubs, §19), so in firmware mode they are never drawn. Colour is not the only signal: each window names its state in words.

---

## Revision history

| Rev | Date | Author | Change |
|---|---|---|---|
| 1 | 2026-11-03 | NeurOne Software Engineering | First issue. Measures the two facts the design depends on (§2: FreeRTOS and the hub, and the safety MCU, run on the host; WebAssembly would need a custom kernel port), then designs the firmware-in-the-loop simulator: architecture (§3), requirements `REQ-SIMFW-01` to `-13` (§5, §11), open items (§7), build order (§8), decisions (§9), reuse as a bench apparatus (§11) and the PBM spike as built and what it found (§12). **Scope direction (the Principal, 2026-11-03): the simulator runs the drivers for every module type, not only PBM, and each module type's full functionality has a simulator equivalent that uses as much of the real code as possible** (`REQ-SIMFW-13`, §8). |
| 2 | 2026-11-04 | NeurOne Software Engineering | **New §13 to §20 as built (stimulation, EEG, auricular VNS, audio, visual goggles, intranasal Y-probe, T2 and accessory stubs, cervical VNS), and the §8 step 3 order set by the Principal (stim first, then the rest in the proposed order).** The bus is keyed by module kind and index and gains a start offset for injected faults; the device's own drivers, the cross-check, the HRV session library, the cervical VNS library with its Pan-Tompkins detector and the goggle Hall-edge handler run under 92 new scenarios; the gaps are listed with the measured line coverage of each source; `OI-SIMFW-15` to `OI-SIMFW-22` are raised, `OI-SIMFW-22` being a defect in `np_cvns_session.c` (a session cannot leave its baseline stage). |
| 3 | 2026-11-05 | NeurOne Software Engineering | **§20 and §7: `OI-SIMFW-22` closed (fixed) and `OI-SIMFW-23` raised (a faulted cervical VNS session cannot be re-enabled).** The cervical VNS scenarios grow from 15 to 22 and reach ramp-up, steady delivery, the safety MCU's refusal, R-peak data loss, stop in delivery and completion at the session length; the measured line coverage rises (`np_mod_cvns.c` 91.8 %, the interlock 77.1 %, the session 83.8 %, the stimulation ramp 75.9 %). **New §21: accessories in the browser** (the Principal, 2026-11-05): drawn only while in use, those attached to the helmet on the helmet, the others in a window each around the perimeter of the helmet window; the page's module registry is keyed by (kind, index). |
| 4 | 2026-11-05 | NeurOne Software Engineering | **§21: each accessory window draws a labelled schematic (numbered markers, a legend naming each part and its A12.x, A13.x or A14.x row); the standalone session gains intranasal, cervical VNS and haptic modalities derived from `pbm_intranasal`, `cervical_vns` and `vibrotactile_40hz`; the demo panel keeps one switch, the haptic pad.** |
| 5 | 2026-11-06 | NeurOne Firmware Engineering | **§20 and §7: `OI-SIMFW-23` closed (fixed in `np_cvns_session.c`, `np_cvns_interlock.c` and `np_mod_cvns.c`); the re-enable scenario now shows a refusal inside the lockout and an acceptance after it.** |
| 6 | 2026-11-08 | NeurOne Software Engineering | **§7 OI-SIMFW-10: the four §11 questions are drafted for the bench and first-article owner in docs/human-input/oi-simfw-10-bench-apparatus.md (DRAFT, not sent). The item stays open.** |
| 7 | 2026-11-08 | NeurOne Software Engineering | **§7: `OI-SIMFW-11` closed by the principal (2026-11-08): the simulator refuses what the device refuses; §9 findings row updated.** |
| 8 | 2026-11-08 | NeurOne Software Engineering | **§18 and its gaps: the simulator links the photoparoxysmal detector's own sources (`np_ppx_policy.c`, `np_ppx_detect.c`) and replaces the `np_runner_abort` double with an `np_runner_halt` double in `np_sim_spike_link.c`, so every simulator test target links again after #789 to #791; no file under `firmware/hub_control` changed (`REQ-SIMFW-03`).** |
