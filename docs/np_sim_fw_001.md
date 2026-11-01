# Firmware-in-the-Loop Helmet Simulator — Design

**Project:** NeurOne
**Document:** NP-SIM-FW-001
**Revision:** 4
**Date:** 2026-11-01
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
> isolation and cannot. **A second aim, added at Rev 2:** to the degree possible the same code is the basis
> of a bench test apparatus (§11). **Rev 3 records the first build, the PBM spike (§12).** §8 is the
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
| Does the safety MCU run on the host? (added at Rev 2) | **Yes.** All 12 sources in `firmware/safety_mcu/src/` compile with gcc and no warnings; 11 as they are and `np_gpio_mgr.c` with the force-included `tests/np_gpio_mgr_test_ports.h` its own host test already uses. Linked against a scratch HAL that defines the 41 `np_hal_*` functions of `np_safety_hal.h` plus `firmware/crypto`, the real `main()` ran its init sequence and its `for(;;)` loop with no source change. At boot all ten enable lines went to 1 (disabled). With no heartbeat it granted nothing. Valid heartbeats requesting the cranial enable were answered. What was **not** exercised: the signature path, the thermal, charge and cardiac interlocks, the tier gate, and the watchdog cutoff after a running grant. The loop is a busy-poll, so the host HAL needs a pacing point. |
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
in the loop. **It can** (§2, `OI-SIMFW-01` closed at Rev 2): the safety MCU is a separate host process or
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
| ~~`OI-SIMFW-01`~~ | **CLOSED at Rev 2.** The safety MCU builds and runs on the host (§2). *The item read:* can the safety MCU firmware run host-side? | — (closed) |
| **`OI-SIMFW-09`** | **The safety MCU's host HAL.** 41 functions, including the SPI frame path, flash, OTP and the independent watchdog. The busy-poll main loop needs a pacing point that is not in the firmware source (`REQ-SIMFW-03`), so it must live in the HAL (for example a short sleep in `np_hal_get_tick_ms`). Whether the signature, thermal, charge and cardiac paths behave on the host as they do in the existing host tests is untested. | The host HAL exists and a scenario drives each interlock to its cutoff. |
| **`OI-SIMFW-11`** | **Should the simulator admit what the device refuses?** The production power governor refuses every load (`np_socket_dispatch.h`, `OI-FWHUB-09`), so against it no tile can be driven. The spike admits every load and says so in its event stream (`sim_policy`). Whether that is right, or whether the governor should be scriptable like a module, is a decision. | A decision is recorded, and the event stream and the on-screen notice agree with it. |
| **`OI-SIMFW-12`** | **`scripts/req-trace.ts` scans `docs/ firmware/ app/ common/ ci/` and not `simulator/`.** The JavaScript tests carry `@verifies` tags (`REQ-SIMFW-04`, `-11`, `-12`) that the report does not count, so those requirements show as tested only by the C tests, and `REQ-SIMFW-12` as untested. | `simulator/` is added to the scan, or the tests move. |
| **`OI-SIMFW-13`** | **CI.** The simulator is opt-in (`-DNP_BUILD_SIM=ON`) and in no CI job, because a CMake test target must be in `ci/host-test-partition.txt`, in three workflow counts and in `ci/host-test-floors.txt`, and the gates need their path-relevance lists widened. The JavaScript tests run by hand (`node --test`). | The tests run in a PR job, with the partition, counts and floors updated together. |
| **`OI-SIMFW-14`** | **`REQ-SIMFW-01` and `-02` have no CI check yet.** The guard is the CMake `return()` under a cross toolchain and the opt-in option. Nothing fails a device target that links `firmware/platform_sim/`, and nothing counts the seams the sim layer routes against the 119. | §8 step 2 builds both. |
| **`OI-SIMFW-10`** | **The bench apparatus (§11).** No document specifies one. | §11's questions are answered by whoever owns bench and first-article test. |
| **`OI-SIMFW-02`** | **Reconcile the seam counts.** The census says 119; the scan of undefined symbols in §2 found about 94 seam-shaped names. The scan excluded `np_pbm_hal.c` and counted only what the compiled sources reference. The difference is unexplained. | The sim layer's route table accounts for every one of the 119, or the census is corrected. |
| **`OI-SIMFW-03`** | **POSIX port gaps.** `xPortIsInsideInterrupt` is undefined; `portMAX_DELAY` truncates against the 32-bit tick type. | A shim exists in `platform_sim/` (no change to the vendored kernel, which is SOUP) and the truncation is shown to be harmless or is avoided. |
| **`OI-SIMFW-04`** | **Time model.** Real time makes the run representative of task interaction but not of timing. A virtual clock gives repeatable runs but changes how the POSIX port schedules. | A choice is recorded, with whether the heartbeat and watchdog run on the same clock. |
| **`OI-SIMFW-05`** | **Storage.** littlefs needs a block device. A file-backed one is the obvious choice; whether the existing power-loss test device (`np_lfs_powerbd`) can be reused was not checked. | The block device is chosen and `REQ-SIMFW-08` is checked against it. |
| **`OI-SIMFW-06`** | **Index space and id space.** The socket dispatch is in 0-based index space and other parts of the firmware use ids. The route table must not confuse them. **Met once at Rev 3, and it was hit on the first run:** the page numbers sockets 1 to 80 and the firmware 0 to 79 (§12). The mapping is now one named function with a test. | The route table states the space for each seam. |
| **`OI-SIMFW-07`** | **Bridge security.** The bridge accepts commands that inject faults into a process that runs the stimulation logic. It must bind to localhost only. | Binding and origin checks are specified and tested. |
| **`OI-SIMFW-08`** | **Which modules are in the first release.** §8 starts with PBM. The accessories each have their own seams and their own hardware documents (`np_hw_*_001.md`), several of them DRAFT. | A list is agreed, with each accessory's seam owner. |

---

## 8. Build order

1. **Spike (PBM only). Built at Rev 3; §12.** Host-build the hub core with a sim platform layer for the PBM seams
   (`np_mod_pbm_hal_socket_*`, `np_pbm_hal_*`) and one `SimModule` per socket. Prove one injected
   `MOD_FAULT` on one tile, visible in `FW_LOG` and in the 3D view. This also settles `OI-SIMFW-03`
   and `OI-SIMFW-05`.
2. **Census.** `REQ-SIMFW-01` and `REQ-SIMFW-02` as CI checks, before more seams are added.
3. **All modalities,** through the same route table, in the order `OI-SIMFW-08` fixes.
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
- It does not decide the first release's module list (`OI-SIMFW-08`).

---

## 11. Reuse as a bench test apparatus (added at Rev 2)

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

## 12. The PBM spike, as built (added at Rev 3)

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
| **A command driving more than 32 tiles is refused and rolled back.** 8, 16, 24 and 32 tiles drive; 40 and 80 return `NP_HUB_ERR_MOD_FAULT` with nothing left driven. The limit is `NP_PBM_SESSION_MAX_ACTIVE_SOCKETS`, which `NP_PST_MAX_TRACKED` is asserted equal to. | **A requirements gap, ruled so on 2026-11-01:** only the helmet's addressable sockets (valid addresses, not a count of mask bits), how many are filled, and power and heat limits determined at runtime may limit how many modules are driven at once. The 32 is an assumption about typical sessions with no derivation. Raised as `REQ-FWHUB-64` and `OI-FWHUB-27` in `NP-FW-HUB-001` Rev 30, and as register row `UC-119`. A unit test of the dispatcher does not see it; a whole-lattice run does. |
| **The production governor refuses every load.** | Against the shipped firmware no tile can be driven at all. `OI-SIMFW-11`. |
| **A failed stop leaves the emitter driven.** With a stop failing on one tile, the hub reports no active socket and drops the cranial enable, and the module still holds its drive. | The designed backstop (`np_socket_dispatch.h`: the whole-lattice cut) and exactly what the safety MCU's enable is for. With no safety MCU in the loop the simulator can show the hub's side of it and not the cut. |

**What it does not do.** Smart (1064 nm) tiles: `np_pbm_hal_i2c_*` and the slot-indexed PBM seams report
themselves as `unrouted` and fail. No other modality. No safety MCU (`OI-SIMFW-09`). No FreeRTOS tasks and
no session runner (the spike calls the dispatcher directly). No persistence: the hex-map store and
the protocol identity seams are spike-local stand-ins, named in `np_sim_spike_link.c`. Numeric status codes
only: the generated name table of `REQ-SIMFW-05` is not built.

---

## Revision history

| Rev | Date | Author | Change |
|---|---|---|---|
| 4 | 2026-11-01 | NeurOne Software Engineering | §12's 32-tile finding is a requirements gap, not a question: raised as `REQ-FWHUB-64` and `OI-FWHUB-27` (`NP-FW-HUB-001` Rev 30) and `UC-119`. No other change. |
| 3 | 2026-11-01 | NeurOne Software Engineering | The PBM spike is built (§12). Adds `REQ-SIMFW-11` and `-12`, `OI-SIMFW-11` to `-14`; `OI-SIMFW-06` met once. Records what the spike found: the 1-based and 0-based socket numbering, the 32-tile ceiling, the governor that refuses every load. |
| 2 | 2026-11-01 | NeurOne Software Engineering | Closes `OI-SIMFW-01`: the safety MCU builds and runs on the host (§2, §3.4). Raises `OI-SIMFW-09` and `-10`. Adds §11, reuse as a bench test apparatus (three layers; what carries over and what does not), with `REQ-SIMFW-09` and `-10`. |
| 1 | 2026-11-01 | NeurOne Software Engineering | First issue. Measures the two facts the design depends on (§2), then designs the firmware-in-the-loop simulator: architecture (§3), requirements `REQ-SIMFW-01`…`-08` (§5), open items `OI-SIMFW-01`…`-08` (§7), build order (§8) and three decisions for the principal (§9). Builds nothing. |
