# SW-01 Safety MCU Unit-Level FMEA

**Project:** NeurOne  
**Document:** NP-FMEA-001  
**Revision:** 9
**Date:** 2026-09-24  
**Status:** DRAFT  
**Effective Date:** 2026-07-13  
**Author:** SmartyPants / PAI  
**Approved By:** TBD (Quality Lead)  
**References:** NP-SW-001 Rev 8, NP-RM-001 Rev 1, NP-FW-CVNS-001 Rev 1, IEC 62304:2006+AMD1:2015 §7.1, ISO 14971:2019  
**Related Issues:** —  
**Gate:** —  
**IEC 62304 Class:** C (SW-01 Safety MCU)  
**Jurisdiction Scope:** —  
**Change Summary:** Rev 9 (2026-09-24) — **New §3.10: SW01-M10 tier identity gate** (`NP-REG-UPG-001` §7.7, `OI-UPG-01`). Seven failure modes. Worst initial S4×P2 = 8 (ALARP). Residual S4×P1 = 4 (ACCEPTABLE), **conditional on `OI-UPG-08`** for the one mode (FMEA-M10-05) that a field-programmable blank OTP window leaves open. §1.2 corrected: SW01-M09 exists and has no section here (`OI-FMEA-10`). Rev 8 (2026-09-23) — **FMEA-M05-06 corrected: its mitigation was never implementable.** It relied on battery-backed RTC backup registers, and the headset has no battery (CLAUDE.md §4.5). Now mitigated by the SW01-M09 NV log (`NP-SW-FAULTMSG-001` P1), and the residual is re-derived. Found by the RISK-25 re-score (`NP-RISK-002` §4.3), which also raised `OI-RISK2-05` for the other M05 rows and §2's IWDG claim. Rev 7 (2026-09-15) — **§3.3 (SW01-M03) re-authored against the module that exists, and risk scores CHANGED** (`OI-FMEA-07`; Revs 5 and 6 flagged the gap and froze the scores, this one closes it). Two ADC failure modes retired and replaced — the real hazard is **delivered current diverging from commanded**, not a stuck ADC — and FMEA-M03-03 keeps its S5/P2 = 10 UNACCEPTABLE-initial score with the cause corrected to *commanded current never reaching the monitor*, which was the live state of the system until the hub half of `OI-CHARGE-01` was wired. FMEA-M03-06 added (wrong ceiling applied to a channel). The module is named a **commanded-dose** control throughout, with the safety argument stated for the first time: signature independence, not privacy. **SW01-M03's residual rating in §4 is now explicitly conditional** on `OI-FMEA-07`'s cross-check, which is specified but not built. `OI-FMEA-04` closed NOT APPLICABLE; `OI-FMEA-08` raised for the 95% ramp-down margin both requirement documents specified and neither implementation has. Rev 6 (2026-09-09) — §3.3's flag corrected: the commanded-current substitution is **not** a privacy constraint. `np_safety_imp_report_t` already moves a UHDR-class measurement across the same SPI boundary and reports only a biology-free divergence flag to SHDR, so the rule is "UHDR may not land in SHDR", not "UHDR may not cross SPI". What constrains a delivered-dose interlock is signature independence and a current-sense ADC channel the MCU does not have. `OI-FMEA-07` now carries a recommended resolution needing no new hardware. No risk scores changed. Rev 5 (2026-09-09) — §3.3 (SW01-M03 charge density monitor) flagged as **not analysing the module that exists**: `np_charge_density.c/.h` never existed, there is no ADC, and FMEA-M03-02/-03 are hazards of one — so FMEA-M03-03 (S5/P2 = 10, UNACCEPTABLE initial) is carried as mitigated by a control that was never built, while its effect line describes the live system. `OI-FMEA-07` raised. No risk scores changed. Rev 4 — §3.4 (SW01-M04 thermal interlock) flagged as written against the **retired 5-PBM-slot architecture**; FMEA-M04-01's zone-index control marked INVALID; PA4 double-assignment recorded as an open instance under FMEA-M08-04; OI-FMEA-06 raised for re-analysis. **No risk scores changed and no failure modes added or removed** — re-scoring is hazard analysis and is deferred to OI-FMEA-06.

---

## 1. Purpose and Scope

### 1.1 Purpose

This document provides the unit-level Failure Mode and Effects Analysis (FMEA) for SW-01 — the NeurOne Safety MCU bare-metal firmware executing on the STM32G071 (Cortex-M0+, 64 MHz, 36 KB SRAM, 128 KB flash). It satisfies IEC 62304:2006+AMD1:2015 §7.1 Class C requirement to identify software items that could contribute to hazardous situations and to document the failure modes, potential harms, and risk controls for each.

This FMEA is also required by ISO 14971:2019 as part of the software-related hazard analysis and complements the system-level risk register (NP-RM-001 / NP-RISK-001, RISK-01 through RISK-25).

### 1.2 Scope

This document covers the **SW-01 Safety MCU firmware only** — the bare-metal C modules executing on the STM32G071: SW01-M01 through SW01-M08 as first analysed, and SW01-M10 (§3.10, Rev 9). **SW01-M09** (the flash NV state log, `np_nv_state.c`) exists in the firmware and has **no section here**. It is referenced only as the control in FMEA-M05-06 (`OI-FMEA-10`). The name SW01-M09 is also used by `NP-FW-M09-ARCH-001` for a different, unbuilt module, the operating-envelope gate. That collision is recorded in `OI-FMEA-10`, not resolved here. It does not cover:
- SW-02 main processor firmware (NXP i.MX RT1062, FreeRTOS, Class B)
- SW-03 iOS/Android application (Class B)
- Hardware-level failure modes outside firmware behaviour (covered in NP-RISK-003 and NP-RISK-004; NP-RISK-001 superseded 2026-08-11)

### 1.3 Architectural safety significance

The Safety MCU occupies the most safety-critical position in the NeurOne dual-processor architecture. Key architectural properties that are themselves risk mitigations:

1. **Hardware GPIO ownership:** All stimulation enable GPIO lines are physically wired to the STM32G071 and cannot be driven by the i.MX RT1062 main processor. This is a PCB-level constraint, not a software policy.
2. **Bare-metal execution:** No RTOS, no dynamic memory allocation (malloc/free prohibited by MISRA C:2012 compliance). This eliminates entire classes of failure modes: scheduler failures, heap fragmentation, stack corruption from dynamic allocation, and RTOS task priority inversions.
3. **No external network path:** The Safety MCU has no direct USB, BLE, or Wi-Fi interface. All commands flow through SW-02 via the SPI interface; the Safety MCU independently validates all enable requests against its own sensor readings before granting.
4. **Independent sensor inputs:** The Safety MCU reads NTC thermistors, electrode impedance, and the R-peak GPIO pulse independently of SW-02's sensor processing. A divergence between Safety MCU readings and main processor readings causes the safety MCU to deny enable rather than grant it.

IEC 62304 §4.3 Class C rationale: SW-01 directly controls all stimulation enable GPIO. A software failure (erroneous enable, failure to disable, incorrect charge calculation) could result in patient harm up to and including serious injury or death (cervical VNS cardiac arrhythmia, tDCS/BES charge density overdose, PBM scalp thermal burn, retinal injury). No independent hardware backstop exists for a Safety MCU failure; this is why Class C applies.

---

## 2. FMEA Methodology

### 2.1 Process

Each module is analysed using the following failure mode chain:

**Failure Mode → Effect on Patient Safety → Severity → Probability (unmitigated) → Initial Risk → Mitigations → Residual Severity → Residual Probability → Residual Risk → Acceptability**

### 2.2 Severity scale (per NP-RM-001 §4.1)

| Level | Definition | NeurOne examples |
|---|---|---|
| S1 — Negligible | No injury or discomfort | Spurious debug log entry |
| S2 — Minor | Temporary, reversible discomfort | Mild skin irritation, session interrupted unnecessarily |
| S3 — Moderate | Reversible injury requiring medical attention | Skin burn from thermal runaway, transient seizure |
| S4 — Serious | Irreversible injury or serious harm | Sustained seizure, significant burn, permanent hearing damage |
| S5 — Critical | Life-threatening or fatal | Cardiac arrhythmia from cervical VNS, fatal thermal injury |

### 2.3 Probability scale (per NP-RM-001 §4.2)

| Level | Definition | Approximate frequency |
|---|---|---|
| P1 — Improbable | Unlikely in device lifetime | < 1 in 10,000 devices |
| P2 — Remote | Could occur rarely | 1 in 1,000–10,000 devices |
| P3 — Occasional | Could occur in some devices | 1 in 100–1,000 devices |
| P4 — Probable | Likely in some devices | 1 in 10–100 devices |
| P5 — Frequent | Expected to occur regularly | > 1 in 10 devices |

### 2.4 Risk acceptability (per NP-RM-001 §4.3)

| Risk Score (S×P) | Rating | Action |
|---|---|---|
| ≤4 | ACCEPTABLE | Document rationale; no further reduction required |
| 5–9 | ALARP | Document ALARP justification; reduce further if reasonably practicable |
| ≥10 | UNACCEPTABLE | Design must be modified; device cannot be released |

**Note:** The matrix is not purely multiplicative. The full 5×5 table in NP-RM-001 §4.3 is the authoritative reference. S×P scores in this document are used as a shorthand consistent with that table.

### 2.5 Failure mode ID convention

Format: `FMEA-M{module_number}-{sequence_number}`  
Example: `FMEA-M01-03` = SW01-M01 (GPIO enable management), third failure mode.

### 2.6 Mitigation evidence types

Mitigations in this FMEA are categorised by evidence type:
- **Design constraint:** An architectural property that prevents the failure mode by design (e.g., hardware GPIO ownership, no dynamic allocation)
- **Implementation control:** Code-level practice that prevents the failure (e.g., atomic register write, bounds checking)
- **Verification test:** A unit test, FAI, or bench test that confirms the mitigation is effective
- **Independent backstop:** A separate hardware or software mechanism that catches the failure if the primary control fails

For Class C software (SW-01), all mitigations must be verifiable — each mitigation must have at least one verification test or a documented rationale for why testing is not feasible (e.g., PCB-level hardware constraint verified by schematic review rather than runtime test).

---

## 3. Module-by-Module FMEA

### 3.1 SW01-M01 — Stimulation Enable GPIO Management (`np_gpio_enable.c/.h`)

**Description:** SW01-M01 implements the hardware interlock state machine that owns all stimulation GPIO enable lines. It is the final arbiter of whether electrical or optical stimulation is delivered to the patient. The module maintains an enable bitmask (one bit per modality/channel), processes enable grant requests from SW-02 via SPI, and drives the GPIO lines. On any fault condition — from any module — SW01-M01 receives a fault notification and deasserts all enable GPIO within one software tick. It also receives the global watchdog cutoff signal from SW01-M02 and responds by driving all enables LOW immediately within the ISR.

The module uses a static state machine with the states: `IDLE` → `ENABLED` → `FAULT`. Transitions into `FAULT` state are irreversible without explicit app-confirmed reset.

| FM-ID | Failure Mode | Effect | S | P | Risk | Mitigation | Res. S | Res. P | Res. Risk | Accept |
|---|---|---|---|---|---|---|---|---|---|---|
| FMEA-M01-01 | Stale enable bitmask not cleared on fault entry — one or more GPIO remain HIGH after fault | Stimulation continues after fault condition detected (e.g., overheat fault fires but one PBM zone LED continues driving) | S4 | P2 | 8 (ALARP) | State machine fault entry function unconditionally clears all bits in enable mask before driving GPIOs LOW; code path is unit-tested with 100% branch coverage; watchdog cutoff path is a separate ISR that operates on GPIO directly, bypassing bitmask | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M01-02 | SPI enable grant processed after fault state entered — spurious late enable overrides fault | A fault event and an in-flight SPI enable grant race; GPIO re-asserted HIGH after fault deassertion | S4 | P2 | 8 (ALARP) | SPI command handler checks state machine state before processing any enable grant; `FAULT` state silently discards all enable grants and returns `ENABLE_REJECTED`; state machine entry order: fault fires → state set to `FAULT` → SPI handler checks state | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M01-03 | **[RE-CHECK ON ENABLE-WORD CHANGE — OI-FMEA-06]** Wrong GPIO bit position in enable mask — incorrect modality enabled or disabled | Incorrect stimulation channel active (e.g., tDCS enabled instead of PBM zone, or safety cutoff drives wrong GPIO) | S4 | P2 | 8 (ALARP) | GPIO pin assignments are compile-time constants in `np_gpio_enable.h`; constants reviewed against schematic in design review; unit tests verify GPIO mapping for all 8 modality channels; PCB layout review confirms physical GPIO-to-driver mapping | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M01-04 | GPIO output register set/clear operation is non-atomic — partial update during ISR preemption | Transient state where half the channels are in an incorrect enable state | S3 | P3 | 9 (ALARP) | STM32G071 GPIO BSRR (Bit Set/Reset Register) is a single 32-bit atomic write — setting and clearing GPIO in the same register write is guaranteed atomic by the ARM Cortex-M0+ architecture; all multi-channel GPIO changes use a single BSRR write | S1 | P1 | 1 | ACCEPTABLE |
| FMEA-M01-05 | State machine initialised incorrectly — device boots with stimulation enabled | Patient receives stimulation without consent or session start | S5 | P2 | 10 (UNACCEPTABLE initial) | All GPIO outputs initialised LOW (disabled) in `np_gpio_enable_init()` before any other module initialises; boot self-test verifies GPIO state reads back as LOW before any SPI communication begins; initial state is `IDLE` with empty enable mask | S2 | P1 | 2 | ACCEPTABLE |

---

### 3.2 SW01-M02 — SPI Heartbeat Watchdog (`np_spi_watchdog.c/.h`)

**Description:** SW01-M02 monitors the 200 ms SPI heartbeat from SW-02 (the i.MX RT1062 main processor). If no valid heartbeat is received within 1,500 ms, the watchdog fires and commands SW01-M01 to drive all stimulation GPIO LOW within ≤50 ms. This is the primary interlock against SW-02 software crashes, hangs, or communication failures during a session.

The heartbeat SPI message carries the SW-02 requested-enable bitmask (what SW-02 wants enabled). The Safety MCU uses this as an input to its own enable decision but never grants solely on this basis — it cross-checks all interlock sensor readings independently. The watchdog timer is a hardware TIM peripheral (not a software counter) to resist firmware hang scenarios.

| FM-ID | Failure Mode | Effect | S | P | Risk | Mitigation | Res. S | Res. P | Res. Risk | Accept |
|---|---|---|---|---|---|---|---|---|---|---|
| FMEA-M02-01 | Watchdog timer register corrupted by stack overwrite or adjacent memory fault — timer period shortened | Watchdog fires too early (false positive); stimulation session interrupted unnecessarily | S2 | P3 | 6 (ALARP) | No dynamic memory allocation (bare-metal, MISRA C:2012); all variables are static or stack-allocated with fixed size; watchdog timer is a hardware peripheral (TIM register, not a software variable); MISRA C:2012 prohibits pointer arithmetic that could corrupt peripheral registers | S1 | P2 | 2 | ACCEPTABLE |
| FMEA-M02-02 | Watchdog timer register corrupted — timer period lengthened or timer disabled | Watchdog fails to fire within 1.5 s after SW-02 crash; stimulation continues without safety oversight | S5 | P2 | 10 (UNACCEPTABLE initial) | Hardware watchdog timer (IWDG — Independent Watchdog) used as a secondary backstop; IWDG is clocked from LSI oscillator independent of main clock and cannot be disabled once started; SW-01 also monitors the SPI `NSS` chip-select pulse rate in firmware as a tertiary check | S3 | P1 | 3 | ACCEPTABLE |
| FMEA-M02-03 | SPI bus contention or glitch causes spurious heartbeat reception — watchdog reset without valid SW-02 communication | Watchdog reset by noise; SW-02 can be crashed/hung without triggering cutoff | S5 | P2 | 10 (UNACCEPTABLE initial) | Heartbeat SPI packet contains a 16-bit counter (monotonically incrementing) and a 1-byte checksum; Safety MCU validates counter increments by exactly 1 each heartbeat; checksum mismatch or non-sequential counter → packet rejected (watchdog NOT reset) | S3 | P1 | 3 | ACCEPTABLE |
| FMEA-M02-04 | SNVS register write fails on warm reset — watchdog countdown state lost | Watchdog timer resets its countdown on a warm reset even if SW-02 is still unresponsive; stimulation continues | S4 | P2 | 8 (ALARP) | Watchdog is a hardware IWDG — on any reset (warm or cold) the IWDG resumes counting from its initial value; the Safety MCU also drives all stimulation GPIO LOW during the reset vector before re-entering normal operation | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M02-05 | Watchdog fires but SW01-M01 fault handler is not called — GPIO not driven LOW | SW-02 crash not handled; stimulation continues | S5 | P2 | 10 (UNACCEPTABLE initial) | Watchdog fires via hardware IWDG reset; reset vector unconditionally calls `np_gpio_enable_cutoff_all()` at the earliest possible point in boot sequence before any SPI initialisation; GPIO cutoff happens inside the ISR, not via a function call from a separate module | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M02-06 | SPI heartbeat message format changed in SW-02 without updating SW-01 — Safety MCU misinterprets valid heartbeats as invalid | False watchdog timeouts under normal operation; device inoperative | S2 | P3 | 6 (ALARP) | SPI heartbeat protocol version field in each packet; Safety MCU rejects packets with unsupported version and continues existing timeout countdown (conservative: treats version mismatch as missed heartbeat); firmware version coordination required at release (NP-SW-001 §9) | S1 | P2 | 2 | ACCEPTABLE |

---

### 3.3 SW01-M03 — Charge Monitor, commanded dose (`np_charge_monitor.c`)

> **RE-AUTHORED 2026-09-15 against the implemented module (`OI-FMEA-07`, Rev 7).** Revisions 5 and
> 6 carried a warning banner over an analysis of a module that was never built: a
> `np_charge_density.c/.h` with a 1 kHz current-sense ADC, a 95% abort margin and a <25 µs
> response. Two of its three principal failure modes (FMEA-M03-02, -03) were hazards **of that
> ADC**, mitigated by an ADC self-test and a 2-point calibration that cannot exist on this
> architecture — so an **UNACCEPTABLE-initial** hazard was carried as mitigated to acceptable by a
> control that had never been built and could not be. This section now analyses
> `firmware/safety_mcu/src/np_charge_monitor.c` as it is, and **risk scores have changed**; the
> retired rows and where each went are tabulated at the end.

**Description.** SW01-M03 bounds the charge the safety MCU has been *commanded* to deliver on each
electrode-bearing channel, and clears that channel from `granted_mask` when the bound is reached.
It is **waveform-aware** (`OI-CHARGE-05`), because the two waveform classes do not share a
meaningful dose quantity:

- **DC channels** (tDCS, HD-tDCS) — commanded charge is integrated across the session as
  `|I| × dt` and compared against `DI-SAFE-01`'s **150 mC/cm² × the declared electrode area**.
- **Pulsed/AC channels** (BES/tACS, VNS, cervical VNS, clinical tACS) — charge-balanced biphasic,
  so net delivered charge is ~zero and a session integral of `|I|` is not a dose. These are
  evaluated per heartbeat as a **predicate on charge per phase** (amplitude × phase width, with a
  2/π factor for sinusoids) against `DI-SAFE-01a`'s **40 µC/cm² × the declared area**. No state is
  accumulated.

A channel may carry both classes: `NP_SAFETY_CH_CLIN_STIM` is shared by HD-tDCS and clinical tACS,
and both checks then run.

**It is a COMMANDED-dose control, not a delivered-dose one, and that is the safety argument this
section previously failed to state.** The input is `current_ua[]` from the heartbeat frame, which
carries what the cryptographically signed session descriptor asked for — never an ADC measurement.
The reason is **signature independence**: a measured value cannot be signed in advance, so a
Class C cutoff keyed to one would inherit the Class B hub's integrity, and a faulty or compromised
hub could under-report without this MCU being able to tell. (The source previously recorded the
reason as a UHDR/SHDR privacy constraint. That was wrong: `np_safety_imp_report_t` already moves a
UHDR-class measurement across this same SPI boundary for cross-validation. The rule is *"UHDR may
not land in SHDR"*, not *"UHDR may not cross SPI"*, so privacy is not what blocks a delivered-dose
interlock.) What **does** block the strong form is hardware: ADC1 is fully allocated to six NTC and
six impedance channels, and there is no current-sense input.

**The consequence is the hazard class this section must actually analyse.** A monitor fed commanded
current cannot detect delivered charge diverging from commanded — a driver stuck on, an electrode
lifting, a current source out of calibration. That is a real hazard, it is detectable, and the
control for it is the hub's commanded-versus-delivered cross-check with an SHDR **divergence flag**
(device condition, no biology — the `np_safety_imp_report_t` pattern reused). FMEA-M03-02 and
FMEA-M03-03 below are that hazard, written against this architecture instead of an imaginary one.

**Timing and implementation facts** (each of which contradicted an earlier revision): the monitor
ticks on the 200 ms SPI heartbeat loop, so worst-case detection latency is **200 ms**, not <25 µs,
and its sampling rate is **5 Hz**, not 1 kHz. The trip is at **100%** of the derived limit
(`>=`); there is **no 95% ramp-down margin** in Class C — that margin exists only in Class B
HD-tDCS (`np_hd_stim.c`) — and whether Class C should have one is carried as `OI-FMEA-08` below
rather than asserted here. The accumulator is **`uint64_t`**, with no roll-over detection and no
32-bit-boundary unit test, because at these ceilings a 64-bit accumulator cannot wrap.

**Electrode area is declared, not assumed** (`OI-CHARGE-02`/`-04`): the hub transmits the area from
the signed descriptor and the MCU derives both limits from it, so the density constants never cross
SPI. Channels whose electrodes are fixed parts of the product (BES pads, auricular clip, cervical
collar) use firmware constants that are **PROVISIONAL and not measured** — `OI-CHARGE-07`.

| FM-ID | Failure Mode | Effect | S | P | Risk | Mitigation | Res. S | Res. P | Res. Risk | Accept |
|---|---|---|---|---|---|---|---|---|---|---|
| FMEA-M03-01 | Charge accumulator overflows or is mis-indexed — accumulated charge reads low for the channel actually stimulating | DC charge limit not enforced on that channel; patient receives charge beyond the DI-SAFE-01 ceiling | S4 | P1 | 4 (ALARP) | Accumulator is `uint64_t`; the largest representable value exceeds any reachable session charge by >9 orders of magnitude, so wrap is not a credible mechanism and no roll-over detection is claimed. The mis-indexing mechanism is the live one: enable-bit position ≡ `current_ua[]` slot ≡ accumulator index is a three-way identity, now **asserted at compile time** (`_np_safety_ch_index_check`, `np_safety_protocol.h`) and cross-checked hub-to-MCU by `np_safety_spi_proto_tests.c` through `np_hub_enable_mirror`. Host tests cover per-channel isolation (a trip on one channel leaves others granted). | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M03-02 | **Delivered current exceeds commanded** — driver stuck on, current source out of calibration, or electrode area smaller than declared | Monitor's accumulated figure under-states delivered charge; the ceiling is reached in tissue before the monitor reaches it, so the cutoff is late or absent. **This is the residual hazard inherent to a commanded-dose control** and cannot be mitigated within this module. | S4 | P2 | 8 (ALARP) | Hub cross-checks its own delivered-current telemetry (`NP_LOG_TAG_UHDR_STIM`) against commanded and raises an SHDR **divergence flag** — device condition, no biology (`OI-FMEA-07`, not yet built). Independent backstops that do not depend on this module: hardware current limit on the driver circuit; the SW01-M06 impedance check refuses enable on an out-of-range contact; SW01-M04 thermal cutoff bounds the energy path. **Declared** electrode area removes the assumed-geometry mechanism (`OI-CHARGE-04`). | S3 | P1 | 3 | ACCEPTABLE |
| FMEA-M03-03 | **Commanded current never reaches the monitor** — hub sends no `current_ua[]`, sends zeros, or the heartbeat's ext-checksum fails every beat | No charge accumulates and no per-phase predicate evaluates; the charge limit is never reached; patient receives unlimited charge. **This was the live state of the system until 2026-09-15**: `np_hub_control_main.c` called the heartbeat with `current_ua = NULL, channel_count = 0` (`OI-CHARGE-05` (c)). | S5 | P2 | 10 (UNACCEPTABLE initial) | **The hub half is now wired** — modality modules publish each commanded current beside the `request_enable` they already make, so a channel enabled without a published current is a single-site coding error rather than an architectural gap, and `np_mod_stim_tests.c` asserts the publish. **Fail-closed declaration gate**: an electrical channel whose waveform class was never declared is cleared from `granted_mask` (`np_charge_monitor_decl_gate`), so a session whose setup frames are lost runs **no** electrical modality rather than running one unmonitored; the gate is driven by a compile-time channel set, so a silent hub cannot exempt itself. A heartbeat that fails checksum does not refresh the watchdog, and SW01-M02 cuts all stimulation at 1.5 s. | S3 | P1 | 3 | ACCEPTABLE |
| FMEA-M03-04 | Charge accumulator not reset between sessions — cumulative dose carried across consecutive sessions, or a latched trip carried forward | Either a session is cut using a previous session's charge, or (the converse) a pause/resume cycle zeroes the accumulator mid-session and circumvents the limit | S4 | P2 | 8 (ALARP) | `np_charge_monitor_reset_session()` zeroes accumulators, areas, waveform declarations and the latched `NP_SAFETY_STATUS_CHARGE` bit, and is called on the `session_active` **0→1 transition only**. `np_safety_session_status_bits()` maps RUNNING, PAUSED **and** STOPPING to ACTIVE precisely so a pause cannot present as a new session — that mapping is the control for the circumvention direction and is host-tested. Two-session host tests cover re-arming in both directions; a per-session reset of the declarations makes a stale declaration unable to outlive its session. | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M03-05 | Electrode area declared larger than the physical electrode, or a provisional firmware area is too large | Derived limit exceeds the true ceiling; charge density in tissue exceeds DI-SAFE-01/-01a while the monitor reads compliant | S4 | P2 | 8 (ALARP) | Area travels in the **signed** descriptor, so altering it invalidates the signature SW01-M07 verifies. The hub floors the area (truncates toward zero), so the derived limit can only err low. Where several commands land on one channel the **smallest** declared area wins. An undeclared area on a gated channel is fail-CLOSED, not a fallback (`OI-CHARGE-03`/`-04`). The three provisional product-electrode areas are asserted in test to be non-zero and no larger than the 25 cm² default, so the failure direction is bounded by construction (`OI-CHARGE-07` tracks measuring them). | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M03-06 | **Wrong ceiling applied to a channel** — a charge-balanced channel judged against the DC session budget, or a DC channel against the per-phase predicate | Judging a charge-balanced channel by a session integral of \|I\| trips it in 0.4–1.0 s at its rated current — a spurious cutoff of a safe protocol. The converse leaves a DC channel with no session bound at all. **The first direction was the implemented behaviour until 2026-09-15**, latent only because FMEA-M03-03 kept the monitor inert. | S3 | P2 | 6 (ALARP) | Waveform class is declared per channel by the hub from the signed descriptor and applied on the Class C side (`np_safety_chan_wave_cmd_t`); the ceilings themselves never cross SPI. Classes OR together, so a shared enable bit carrying two modalities is held to **both** ceilings rather than the last one written. An undeclared class is fail-closed, not a default. Host tests pin both directions: that an hour of pulsed stimulation raises no session-dose fault, and that the per-phase ceiling still bites on a channel also declared DC. | S2 | P1 | 2 | ACCEPTABLE |

**Retired rows and where they went.** Rev 7 removed two failure modes and re-scored a third; none
is silently dropped:

| Retired row | Why | Successor |
|---|---|---|
| FMEA-M03-02 (*"ADC read returns saturated value (0xFFF stuck high)"*) | Hazard of a current-sense ADC this design does not have. Its mitigations — out-of-range detection, ADC self-test at session start — could not be built. | Re-authored as the **commanded-versus-delivered divergence** hazard above, which is the real form of "the monitor's number is not what is in tissue" and is detectable. |
| FMEA-M03-03 (*"ADC read returns 0x000 (stuck low)"*, S5/P2 = 10) | Same: an ADC hazard with a 2-point-calibration mitigation that cannot exist here. **But its effect line described the live system**, reached by a different cause. | Kept at **S5/P2 = 10 UNACCEPTABLE initial** with the cause corrected to *commanded current never reaching the monitor*, and mitigated by controls that now exist (the wired hub half, the fail-closed declaration gate, the SW01-M02 watchdog) rather than by an unbuilt self-test. |
| FMEA-M03-01's *"32-bit accumulator … roll-over detection … unit test validates 32-bit boundary"* | The accumulator is `uint64_t`; there is no roll-over detection and no such test. The claimed controls did not exist. | Re-scored P3→P1 on the overflow mechanism (not credible at 64 bits) and re-pointed at the **mis-indexing** mechanism, which is credible and now has a compile-time assertion behind it. |

**What is still true and still owed.** `OI-FMEA-04` (ADC self-test bench validation) is **closed as
not applicable** — there is no ADC to validate. The 95% ramp-down margin question moves to
`OI-FMEA-08`. The commanded-versus-delivered cross-check and its SHDR divergence flag, which
FMEA-M03-02's residual score depends on, are **specified but not built** — that is `OI-FMEA-07`'s
remaining scope, and FMEA-M03-02's residual rating should be read as conditional on it.

---

### 3.4 SW01-M04 — Thermal Interlock (`np_thermal.c/.h`)

> **⚠ ARCHITECTURAL BASIS SUPERSEDED (Rev D, 2026-08-04) — this module's failure modes need re-analysis, not renaming. See OI-FMEA-06.**
>
> §3.4 below is written against the **retired 5-module-slot** architecture, in which "zone" meant one of five physical PBM slots, each with its own NTC ADC channel at the safety MCU (`NP_NTC_CHANNEL_COUNT 6 /* 5 zones + 1 hub */`). **That architecture no longer exists**, and the change is not cosmetic:
>
> - **"Zone" has been redefined.** A zone is now *"a named SET OF MODULES, defined as a list of socket addresses"*, authored in `protocols/predefined/00-zones.npps`, user-extensible, with **no fixed count** — and **zones overlap** (the inclusive-midline rule places every midline socket in BOTH hemisphere zones of its lobe). An overlapping, user-definable set cannot be a thermal or enable domain at all.
> - **The thermal domain is now the tile, and the aggregation boundary is the cluster.** The lattice carries ~80 sockets in 18 clusters (NP-HW-HEXTILE-001 §8.2.1). **The safety MCU cannot present 80 NTC ADC channels**, so the per-tile 62 °C junction throttle moved on-module (Class B, hardware current throttle per CLAUDE.md §4.2, required to act even with the I2C bus silent), and module faults reach the safety MCU via the **per-cluster wire-OR `ALERT#`** rather than by the safety MCU sampling each zone (NP-DRV-SHELL-002 §6).
>
> **Consequently the mitigations below that bound a zone index to 0–4 are no longer valid controls** — see the row annotations. Re-analysis is deferred to **OI-FMEA-06** rather than performed here, because assigning S/P scores to the new architecture is hazard analysis, not an editorial correction. Risk scores in this section are **unchanged and should not be relied on** until that closes.

**Description:** SW01-M04 reads the NTC thermistor ADC channel for each PBM zone (5 zones) at 10 Hz during active PBM sessions. Temperature is computed from the NTC Steinhart-Hart equation using calibrated coefficients stored in the Config partition. At 62°C junction temperature, current to the affected zone is throttled by 50%. At 65°C, all stimulation to that zone is immediately cut off via SW01-M01. The STM32G071's own die temperature is also monitored and triggers a full system cutoff at 85°C die temperature.

| FM-ID | Failure Mode | Effect | S | P | Risk | Mitigation | Res. S | Res. P | Res. Risk | Accept |
|---|---|---|---|---|---|---|---|---|---|---|
| FMEA-M04-01 | **[MITIGATION INVALID — OI-FMEA-06]** Steinhart-Hart coefficient lookup uses wrong zone index — thermal reading from Zone A applied to Zone B's current control | Zone B overheats without triggering its own throttle; Zone A throttled unnecessarily | S4 | P2 | 8 (ALARP) | **⚠ The stated control assumes 5 zones and no longer matches the design:** "Zone index is a validated integer parameter (0–4); bounds-checked before array access (MISRA C:2012 §18.1); unit test exercises all 5 zone indices" — the 0–4 bound and the 5-index test are artifacts of the retired 5-slot architecture. The equivalent control under the hex lattice must bound a *tile* or *cluster* index and is not yet specified (OI-FMEA-06). Original text retained verbatim for traceability; NTC assignment table reviewed against schematic in hardware design review | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M04-02 | NTC ADC channel disconnected or open-circuit — reads maximum resistance (infinity) → minimum temperature → no throttle | Zone overheats without firmware-level thermal protection; relies on PTC/thermal fuse backup only | S4 | P3 | 12 (UNACCEPTABLE initial) | Open-circuit NTC returns ADC value above valid range (> maximum expected at ambient); out-of-range value treated as hardware fault, zone immediately disabled (conservative: no reading = worst case); ADC channel monitored for continuity at session start | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M04-03 | Floating-point rounding error in Steinhart-Hart computation — temperature underestimated | Throttle threshold not reached; zone continues at full power past safe temperature | S3 | P2 | 6 (ALARP) | Fixed-point integer arithmetic used for temperature calculation (not floating-point); coefficients are pre-scaled to integer representation; temperature resolution ±1°C at 62°C operating point; unit tests cover the 55–70°C range with known NTC resistances | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M04-04 | Throttle command acknowledged by Safety MCU but not executed by LED driver — hardware fault | Zone continues at full power despite firmware throttle command | S4 | P2 | 8 (ALARP) | Current throttle uses hardware PWM duty cycle register update; verified by reading back the register value after write (write-verify pattern); duty cycle readback mismatch → zone cutoff; PTC thermal fuse on zone module PCB provides hardware-independent backstop at ~70°C | S2 | P1 | 2 | ACCEPTABLE |

---

### 3.5 SW01-M05 — Cervical VNS Cardiac Interlock (`np_cvns_interlock.c/.h`)

**Description:** SW01-M05 is the highest-criticality module in the Safety MCU firmware. It implements the cardiac interlock for the T2 cervical VNS accessory (stimulation near the carotid sheath). The module monitors the `RPEAK_IN` GPIO pulse from SW-02 (5 ms pulse per detected R-peak), computes instantaneous heart rate from the R-R interval measured by TIM2 (1 MHz, 1 µs resolution), maintains a rolling 5-second HR window, and asserts the cervical VNS disable GPIO LOW (active-low enable) within one TIM6 ISR period (5 ms, 200 Hz) when the HR change exceeds 15 BPM from baseline. Total worst-case cutoff latency is <5.1 ms (well within the 100 ms specification).

A baseline cross-validation step before enable ensures the Safety MCU's GPIO-timer-derived baseline HR and SW-02's PPG-derived baseline HR agree within 5 BPM; divergence blocks enable.

| FM-ID | Failure Mode | Effect | S | P | Risk | Mitigation | Res. S | Res. P | Res. Risk | Accept |
|---|---|---|---|---|---|---|---|---|---|---|
| FMEA-M05-01 | RPEAK_IN GPIO not asserted by SW-02 (cable fault, PPG sensor failure, or SW-02 crash) — no R-peak pulses reaching Safety MCU | Safety MCU sees no R-R intervals; baseline cannot be established; enable is blocked (no baseline → no enable) | S2 | P3 | 6 (ALARP) | If R-peak pulses cease during an active session (after enable granted), Safety MCU detects data loss: rolling window fills with no new intervals for >10 s → conservative hold policy fires → soft cutoff via SW01-M01; enables cannot be granted without baseline (minimum 5 valid beats) | S1 | P2 | 2 | ACCEPTABLE |
| FMEA-M05-02 | R-R interval timer overflow — TIM2 wraps (32-bit timer, 1 µs tick, overflow at ~4,294 s) | Interval measurement returns incorrect (very small) value for a very long R-R interval (HR ~14 BPM); HR overestimated; change detection may miss bradycardia | S4 | P1 | 4 (ACCEPTABLE) | TIM2 overflow is detected by checking if current TIM2 value < last stored edge timestamp; any apparent wraparound triggers interval rejection (marked as invalid, excluded from rolling average); minimum valid interval check (300 ms, 200 BPM cap) also filters implausible values; at pathological HR < 15 BPM, cervical VNS is clinically contraindicated and would be blocked at the indication screening stage | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M05-03 | HR delta calculation underflows — computed delta is negative or zero when true delta is positive | HR increase is treated as decrease or no change; cardiac interlock not triggered when it should be | S5 | P2 | 10 (UNACCEPTABLE initial) | HR values stored as `uint16_t` scaled ×10 (tenths of BPM); subtraction performed using saturating arithmetic: if `current_hr < baseline_hr`, delta = 0 (no false negative for rate increases); HR decrease is an independent check: delta = abs(current_hr − baseline_hr), so both increases and decreases trigger at ±15 BPM | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M05-04 | Baseline HR stored incorrectly at session start (cold start, first R-R interval used as baseline) — wrong reference for delta comparison | Delta is computed from an incorrect baseline; the 15 BPM threshold may be triggered too early (false positive) or too late (missed event) | S4 | P2 | 8 (ALARP) | Baseline is not accepted until a minimum of 5 consecutive valid R-R intervals have been recorded (NP_CVNS_BASELINE_BEATS_MIN = 5); each interval is validated against the 300–2000 ms range; the Safety MCU's computed baseline must agree within 5 BPM with SW-02's PPG-derived baseline; cross-validation failure blocks enable | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M05-05 | SPI deadlock between SW-01 and SW-02 — FAULT_NOTIFY message cannot be sent after cutoff | Cardiac interlock GPIO cutoff fires correctly, but SW-02 is not notified of the fault reason; app does not display fault information | S2 | P2 | 4 (ACCEPTABLE) | GPIO cutoff fires first (within TIM6 ISR, no SPI required); FAULT_NOTIFY is a best-effort notification — SPI deadlock causes delayed notification but does not affect the GPIO state; Safety MCU writes fault log to internal SRAM (not SPI-dependent); fault is recovered at next successful SPI transaction | S1 | P1 | 1 | ACCEPTABLE |
| FMEA-M05-06 | Re-enable lockout counter not persisted across a Safety MCU reset — 30-second lockout bypassed by power cycling | Attacker or malfunctioning app re-enables stimulation immediately after a cardiac cutoff by triggering a reset | S5 | P2 | 10 (UNACCEPTABLE initial) | ~~Lockout state persisted to STM32G071 backup registers (RTC_BKPxR, battery-backed domain, survives warm and cold resets)~~ **(Rev 8: this control was never implementable.** The headset has no battery, coin cell or VBAT rail (CLAUDE.md §4.5), so the backup domain loses its contents on every power loss, and the firmware never contained it. The residual below was scored on a control that did not exist; `NP-SW-FAULTMSG-001` §3 F1.) **Mitigation of record (Rev 8):** SW01-M09 `np_nv_state.c` records the cutoff in flash pages 62–63, per user, and fails closed: a torn record, an uncommitted snapshot, an overflow or a write failure all read as *blocked* (`NP-FW-CVNS-001` §5.4.1). At boot, the lockout re-arms for the current user at the first cervical request. The app must still confirm re-enable, followed by a repeat impedance check (`REQ-CVNS-09`). Host-tested with 12 fault-injection tests; **not run on silicon** | S5 | P1 | 5 | **ALARP** (Rev 8; was "S2 × P1 = 2 ACCEPTABLE", scored on the non-existent control. Severity is not reduced: a bypass restarts stimulation after a cardiac event, whose harm is unchanged. Justification: `NP-RISK-002` §4.3.4) |
| FMEA-M05-07 | Impedance check bypassed under SPI timeout condition — stimulation enabled on contacts with poor or no gel contact | High electrode impedance with stimulation active — increased skin heating, uneven current distribution, potential necrosis | S4 | P2 | 8 (ALARP) | Impedance check is a synchronous blocking call before enable grant: `ENABLE_GRANTED` is only returned after both electrodes pass impedance check; if impedance SPI response is not received within timeout, Safety MCU returns `ENABLE_REJECTED`; conservative failure mode — timeout → deny | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M05-08 | Motion artefact causes false R-peak pulses from SW-02 — HR appears elevated; cardiac interlock triggers spuriously | Session interrupted unnecessarily; patient experiences unexpected cutoff | S2 | P4 | 8 (ALARP) | 30 µs R-peak debounce (NP_CVNS_RPEAK_DEBOUNCE_US); minimum valid R-R interval 300 ms enforced (rejects implausible >200 BPM); Pan-Tompkins adaptive threshold in SW-02 PPG processing; if more than 3 consecutive intervals are rejected as invalid, Safety MCU enters data-loss hold mode rather than using artefact-contaminated data | S1 | P2 | 2 | ACCEPTABLE |

---

### 3.6 SW01-M06 — Impedance Check (`np_impedance.c/.h`)

**Description:** SW01-M06 performs AC impedance measurement at 1 kHz on stimulation electrodes before enabling any session. The measurement is synchronous and blocking — no stimulation GPIO can be asserted HIGH until impedance passes. For electrical stimulation modalities (tDCS, BES, VNS, cervical VNS), electrode impedance must be within the specified range for the electrode type. For PBM (optical), impedance of the EEG electrode contacts is checked (not optical path). Impedance results are reported to SW-02 via SPI for UHDR logging (raw values) and SHDR logging (pass/fail boolean).

**OI-CVNS-HUB-11 delta (Rev C, 2026-07-13):** SW01-M06 additionally measures the cervical VNS electrode impedance **per electrode** (new HAL `np_hal_impedance_read_cvns_electrode_ohm`, electrode 0=left/1=right) and reports the two kΩ values to the hub in the spare bytes of the heartbeat reply (`np_safety_imp_report_t`, magic + valid flag + `cvns_kohm_x100[2]` + checksum). The hub cross-validates these against its own per-electrode measurement (OI-CVNS-HUB-09) and fails closed on divergence beyond `NP_CVNS_IMPEDANCE_CROSSVAL_KOHM`. **Safety-significance:** the per-electrode read and report are **additive telemetry** — they do NOT alter the existing single-value enable GATE, so the Class C interlock decision is unchanged (the safety MCU's own impedance/enable gate remains authoritative). The new failure modes FMEA-M06-05/06 below cover the report path. See NP-FW-EMMC-001 §12 (UHDR classification of raw per-electrode kΩ; SHDR divergence flag only).

| FM-ID | Failure Mode | Effect | S | P | Risk | Mitigation | Res. S | Res. P | Res. Risk | Accept |
|---|---|---|---|---|---|---|---|---|---|---|
| FMEA-M06-01 | Impedance threshold for a specific electrode type uses wrong constant — high impedance passes incorrectly | Stimulation delivered with compromised electrode contact; increased skin heating, uneven current distribution | S4 | P2 | 8 (ALARP) | Impedance limits are compile-time constants per electrode type, referenced against electrode datasheet; constants reviewed against specifications in design review; unit tests cover boundary cases (limit−1, limit, limit+1 for each electrode type) | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M06-02 | Impedance ADC hardware fault returns 0 — low impedance seen even with no electrode contact | Session enabled with no electrodes attached; stimulation into open circuit, driving electrode driver to voltage saturation | S4 | P2 | 8 (ALARP) | Zero impedance (0 Ω) is physically impossible for any clinical electrode; ADC result of 0 treated as ADC hardware fault → session blocked; minimum acceptable impedance bound enforced for all electrode types (e.g., cervical gel electrode: minimum 0.5 kΩ) | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M06-03 | Impedance measurement not repeated after electrode dislodgement during session — high impedance post-dislodgement not detected | Electrode lifts mid-session; stimulation continues with rising skin impedance; increased current density, possible burn | S4 | P3 | 12 (UNACCEPTABLE initial) | Impedance is monitored continuously during session at 1 Hz (not just pre-session); impedance rise >2× pre-session baseline triggers a session warning; impedance rise >5× baseline triggers cutoff via SW01-M01 | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M06-04 | SPI timeout during impedance reporting — impedance result not delivered to SW-02 | UHDR log has no impedance record for session; clinical review may be incomplete | S2 | P3 | 6 (ALARP) | Impedance result stored in Safety MCU SRAM; SPI retry up to 3 times; after 3 failures, fault logged to SHDR (Safety MCU side); conservative approach: SPI failure after successful impedance check does not block session (stimulation is safe) — only data completeness is affected | S1 | P2 | 2 | ACCEPTABLE |
| FMEA-M06-05 | Corrupted per-electrode impedance report (OI-CVNS-HUB-11) accepted by the hub — hub cross-validates against garbage kΩ, wrongly faulting a good session or masking a real divergence | Spurious fail-closed (session blocked, nuisance) or a missed sensor discrepancy | S2 | P2 | 4 (ALARP) | Report carries its own magic byte (0x5A) + additive checksum, independently validated on the hub (`parse_cvns_impedance_report`); a corrupt/absent report clears the valid flag so NO cross-check runs (the flow proceeds on the MCU's authoritative gate, exactly as pre-OI-CVNS-HUB-11); the base 8-byte reply frame and its own checksum are untouched | S1 | P1 | 1 | ACCEPTABLE |
| FMEA-M06-06 | Per-electrode read for the report is wrong/stale, but the enable GATE reads correctly — report path diverges from the actual gate | Hub cross-check compares against a stale value → spurious fail-closed or a masked discrepancy | S2 | P2 | 4 (ALARP) | The report is invalidated on every fresh CVNS request (new session) until the new measurement completes (`s_cvns_report_valid` reset in `np_impedance_check_request`); the report is additive and cannot affect the Class C enable gate; divergence is fail-closed (blocks enable) so the conservative outcome is "no stimulation" | S1 | P1 | 1 | ACCEPTABLE |

---

### 3.7 SW01-M07 — Session Protocol Signature Verification (`np_session_sig.c/.h`)

**Description:** SW01-M07 implements Ed25519 signature verification on the binary session descriptor received from SW-02 before any stimulation GPIO can be enabled. Ed25519 verification is provided by the shared `np_crypto` static library (`firmware/crypto/`) backed by Monocypher 4.0.2 (RFC 8032 §5.1.7, SHA-512; SOUP record: `firmware/crypto/vendor/monocypher/VERSION`; BSD-2-Clause OR CC0-1.0; OI-SW01-M07-02 CLOSED 2026-06-11, PR #132). The library is validated by 11 host tests (RFC 8032 TV1/TV2 vectors + all-zero pubkey guard; see NP-SW-001 §9.4 SOUP table). The bootloader retains its own self-contained Ed25519 because it uses `-nostdlib/-nodefaultlibs`, which makes Monocypher's libc dependencies unavailable; both paths are covered by the same RFC 8032 test vectors. The session descriptor includes the protocol parameters, modality configuration, and safety limits. A replay prevention counter (device-serial session counter stored in the Config partition) prevents replay of previously valid descriptors. Unsigned, corrupted, or replayed descriptors cause SW01-M07 to return a rejection code to SW01-M01, which keeps all GPIO LOW.

| FM-ID | Failure Mode | Effect | S | P | Risk | Mitigation | Res. S | Res. P | Res. Risk | Accept |
|---|---|---|---|---|---|---|---|---|---|---|
| FMEA-M07-01 | Manufacturing public key corrupted in flash — all sessions rejected (DoS) | Device inoperative; no stimulation possible; user unable to use device | S2 | P2 | 4 (ACCEPTABLE) | Public key stored in a dedicated read-only flash sector; key integrity verified by CRC-32 at boot; if CRC fails, device enters safe-mode (all GPIO LOW, USB-C DFU available for reflash); does not cause unsafe stimulation — conservative failure mode | S1 | P2 | 2 | ACCEPTABLE |
| FMEA-M07-02 | Flash sector containing public key has a write error — key silently corrupted to all-zeros or all-ones | Ed25519 verification against zero key may produce a false PASS for any message (depending on implementation); attacker could inject arbitrary session | S5 | P1 | 5 (ALARP) | Public key area is in write-protected flash (read-only sector after programming); PCROP (Proprietary Code Read-Out Protection) active on key sector; key CRC-32 checked at every boot and before every signature verification; CRC failure → session rejected; flash ECC flags single-bit errors | S3 | P1 | 3 | ACCEPTABLE |
| FMEA-M07-03 | Replay of a previously valid signed session descriptor — stale protocol executed | Attacker or software bug re-uses an old signed descriptor; patient receives a session with parameters from a prior prescription | S3 | P2 | 6 (ALARP) | Session descriptor includes a monotonically incrementing 32-bit session counter; Safety MCU maintains its own last-seen counter in backup registers; descriptor with counter ≤ stored value is rejected as replay; counter stored in battery-backed domain survives resets | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M07-04 | Partial SPI transfer accepted as complete — truncated descriptor processed | Descriptor length field may not match actual data received; safety limits in the descriptor (e.g., max current, dose ceiling) may be absent or corrupted | S4 | P2 | 8 (ALARP) | SPI transfer includes a 16-bit length prefix verified against the expected descriptor schema size before Ed25519 verification begins; any length mismatch → descriptor rejected; Ed25519 signature covers the full declared payload including length; truncation detected as signature failure | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M07-05 | Ed25519 constant-time comparison not used — timing side-channel allows signature forgery | Attacker on SPI bus can infer valid vs invalid signatures via timing measurement; eventually forge valid session descriptor | S4 | P2 | 8 (ALARP) | Monocypher 4.0.2 `crypto_ed25519_check` uses a constant-time final comparison (ct_memcmp); no branch is taken on secret-dependent data during verification; SPI bus is internal to the device (not user-accessible); physical access is required for a timing attack; Monocypher SOUP record cross-referenced with upstream security review | S2 | P1 | 2 | ACCEPTABLE |

---

### 3.8 SW01-M08 — Fault Latch and Fault Log (`np_fault.c/.h`)

**Description:** SW01-M08 implements the fault latch state and the SHDR fault log write path. When any interlock module triggers a fault, SW01-M08 records the fault type, offset time, and fault source in a circular buffer in SRAM (8 entries), then attempts to write a condensed fault log entry to the SHDR partition via SPI to SW-02. The fault latch ensures the device remains in a safe (stimulation-off) state until an explicit app-confirmed reset is performed via a signed protocol command. SW01-M08 also drives the fault indicator LED (red blink pattern on the left temple LED).

| FM-ID | Failure Mode | Effect | S | P | Risk | Mitigation | Res. S | Res. P | Res. Risk | Accept |
|---|---|---|---|---|---|---|---|---|---|---|
| FMEA-M08-01 | Fault latch state cleared by software bug in another module — stimulation re-enabled without explicit app confirmation | After a cardiac or thermal fault, stimulation automatically resumes; patient unaware of safety event | S5 | P2 | 10 (UNACCEPTABLE initial) | Fault latch state stored in a dedicated `volatile` variable (not cleared by any code path except `np_fault_clear()` which requires explicit validated app command via signed session protocol); only one code path can call `np_fault_clear()` (enforced by static analysis and code review); fault state also persisted in backup register | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M08-02 | SHDR fault log write fails (SPI unavailable) — fault event not recorded | Safety event lost from fleet records; inability to diagnose device issues post-event | S2 | P3 | 6 (ALARP) | SRAM fault ring buffer always written first (SRAM write cannot fail); SPI write is best-effort with 3 retries; if all retries fail, fault is flagged in the Safety MCU's own SRAM fault log (retrieved at next successful SPI transaction); SHDR loss does not affect the safety of the device state | S1 | P2 | 2 | ACCEPTABLE |
| FMEA-M08-03 | UHDR-classified fault data (e.g., HR at cutoff) written to SHDR fault log by mistake | Privacy violation: user biology appears in NeurOne-accessible SHDR partition | S2 | P2 | 4 (ACCEPTABLE) | SHDR fault log schema (per NP-FW-CVNS-001 §5.6) contains only: session_id (unsigned counter), cutoff_offset_ms, and reason (enum — no HR values); HR data is routed exclusively to UHDR; code review + unit tests verify no HR values are written to the SHDR fault log structure | S1 | P1 | 1 | ACCEPTABLE |
| FMEA-M08-04 | Fault indicator LED circuit shares GPIO with stimulation enable — toggling fault LED inadvertently toggles stimulation. **Known open instance of this hazard class (recorded 2026-08-04): `np_safety_config.h:24` declares PA4 as SPI1 NSS (load-bearing — frames are delineated by NSS transfer length) while `:45` assigns `NP_EN_PBM_ZONE4_PIN = (1U << 4)` on GPIOA — same pin, two owners.** The header marks bank assignments *"provisional pending PCB layout (G1 gate)"*, so this is a provisional-allocation artifact, and this row's control (*"GPIO assignment verified against schematic in hardware design review"*) is precisely the control that has **not yet executed**. The collision resolves as a side effect of the NP-HW-HUB-001 §7.2 zone-enable deletion; tracked under OI-FMEA-06 | Fault condition triggers stimulation instead of indicating fault | S5 | P1 | 5 (ALARP) | Fault LED GPIO and stimulation enable GPIO are on different GPIO ports (separate STM32G071 registers); GPIO assignment verified against schematic in hardware design review; unit test verifies fault LED assertion does not change any stimulation enable register bits | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M08-05 | Circular fault buffer overflow — oldest unread fault entries overwritten before being transmitted to SHDR | Historical fault events lost; diagnostic capability reduced post-incident | S1 | P3 | 3 (ACCEPTABLE) | 8-entry ring buffer covers the maximum plausible number of rapid fault-clear-restart cycles before SPI communication is restored; buffer overflow is logged as a separate SHDR event (overflow flag); SW-02 polls for SRAM fault log on every heartbeat when fault state is active | S1 | P2 | 2 | ACCEPTABLE |

---

### 3.10 SW01-M10 — Tier Identity Gate (`np_tier_identity.c`)

**Description:** SW01-M10 implements `REQ-UPG-01`'s stimulation half and `REQ-UPG-02` (`NP-REG-UPG-001`
§7.5, §7.7). At power-on it reads a 72-byte tier-identity record from OTP (offset `0x40`) and the part's
96-bit factory UID. It verifies an Ed25519 signature by the tier authority, whose public key is compiled
into the image, over `"NeurOne.TierId.1"` ‖ the record body ‖ the UID. Only a record that verifies, for
this UID, naming T2 makes the unit T2. Every other outcome makes it T1. Every main-loop iteration it
strips `NP_SAFETY_EN_T2_MASK` (cervical VNS, TMS, 1170 nm, clinical stim) from `granted_mask`, after
the watchdog tick and before charge accumulation and `np_gpio_mgr_apply()`. It withholds; it never
faults. It reports tier, reason and a per-beat REFUSED flag to the hub.

**What the hazard is.** Energising a T2 line on a unit not built, accepted or labelled as T2 delivers a
T2 modality outside the device's intended use and outside clinical supervision. The per-modality
interlocks (M03, M05, M06) still run on that line, so the worst credible harm is the modality's own,
bounded by those interlocks, delivered to a user nobody is supervising. Scored **S4**. The regulatory
consequence, an uncleared wellness-to-medical transition, is real, but it is not a harm on this scale
and is not scored here.

| FM-ID | Failure Mode | Effect | S | P | Risk | Mitigation | Res. S | Res. P | Res. Risk | Accept |
|---|---|---|---|---|---|---|---|---|---|---|
| FMEA-M10-01 | Gate not applied on some path: call omitted, reordered after the GPIO write, or a later module re-grants a T2 bit | T2 line energised on a T1 unit | S4 | P2 | 8 (ALARP) | **Design constraint:** one grant site (`np_spi_watchdog_tick`), one gate call after it. Every later module only clears bits (charge tick, fault paths). **Verification:** gate behaviour host-tested (`np_tier_identity_tests`: each T2 line alone and together, T1 lines kept). **Placement verified by inspection only**, because `np_safety_main.c` has no host test. **Independent backstop:** the hub refuses the protocol at load (`np_protocol_tier_admit`, host-tested) | S4 | P1 | 4 | ACCEPTABLE |
| FMEA-M10-02 | Verdict corrupted in RAM (SEU, stray write) from not-T2 to T2 | As M10-01 | S4 | P2 | 8 (ALARP) | **Implementation control:** the verdict is a 32-bit pattern word and its separately stored complement, not a bool. Only the exact pair reads as T2, so no single corruption produces it | S4 | P1 | 4 | ACCEPTABLE |
| FMEA-M10-03 | A record that is not this unit's genuine T2 record is accepted: copied from another unit, tier byte edited, different signer, or a signature made for another purpose | As M10-01 | S4 | P2 | 8 (ALARP) | **Design constraint:** UID bound into the signed message, and domain-separated. The tier key is distinct from the session root key. **Verification:** each case host-tested against the real `np_crypto` verifier with RFC 8032 signatures. Mutation removing the UID from the message fails the suite | S4 | P1 | 4 | ACCEPTABLE |
| FMEA-M10-04 | A genuine T2 unit fails to verify: placeholder key shipped in a T2 image, record not written, or written for the wrong UID | T2 modalities withheld on a T2 unit (fail-closed denial of service) | S2 | P3 | 6 (ALARP) | **Implementation control:** reason code in every tier report (`NO_AUTHORITY`, `BLANK`, `FORMAT`, `SIGNATURE`). **Verification:** `np_tier_identity_nokey_tests`. **Planned:** final-acceptance check on reason `OK` (`OI-UPG-08` (iii)) | S2 | P1 | 2 | ACCEPTABLE (conditional on `OI-UPG-08`) |
| FMEA-M10-05 | A unit's OTP window is still programmable in the field (a blank T1, or an unlocked debug port), and a party with the tier-authority private key, or with code execution, writes a T2 record | As M10-01 | S4 | P2 | 8 (ALARP) | **Design constraint:** the safety image contains no OTP write code. **Planned:** every unit gets its record at manufacture, T1 included, so no field window is blank. RDP set at production. Key custody decided at the ceremony (`OI-UPG-08` (i), (ii), (iv)) | S4 | P1 | 4 | **ACCEPTABLE, conditional on `OI-UPG-08`.** Until it closes, no unit carries a T2 verdict at all (placeholder key), so the mode cannot occur |
| FMEA-M10-06 | T2 mask includes a T1 line | A T1 modality withheld on every unit, and carried modules stop working on T2 (`REQ-UPG-03`) | S2 | P2 | 4 (ACCEPTABLE) | **Verification:** host test asserts the mask equals exactly the four T2 lines and contains no T1 line | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M10-07 | OTP or UID read returns wrong bytes (wrong offset, bad base address) | Any record verifies against the wrong bytes, so the unit falls to T1 (a signature cannot verify over wrong bytes) | S2 | P2 | 4 (ACCEPTABLE) | **Verification:** `np_hal_platform_tests` reads the record from `0x40` (not the root key at 0) byte-for-byte, and clamps both reads. **Circularity limit** per `np_hal_fake_regs.h`: the UID base address is not verified against silicon | S2 | P1 | 2 | ACCEPTABLE |

---

## 4. Summary Risk Table

| Module | Module Name | Worst Initial Risk (S×P) | Worst Initial Rating | Residual Risk (S×P) | Residual Rating |
|---|---|---|---|---|---|
| SW01-M01 | Stimulation Enable GPIO Management | S5×P2 = 10 (FMEA-M01-05) | UNACCEPTABLE | S2×P1 = 2 | ACCEPTABLE |
| SW01-M02 | SPI Heartbeat Watchdog | S5×P2 = 10 (FMEA-M02-02, -03, -05) | UNACCEPTABLE | S3×P1 = 3 | ACCEPTABLE |
| SW01-M03 | Charge Monitor (commanded dose) | S5×P2 = 10 (FMEA-M03-03) | UNACCEPTABLE | S3×P1 = 3 | ACCEPTABLE\* |
| SW01-M04 | Thermal Interlock | S4×P3 = 12 (FMEA-M04-02) | UNACCEPTABLE | S2×P1 = 2 | ACCEPTABLE |
| SW01-M05 | Cervical VNS Cardiac Interlock | S5×P2 = 10 (FMEA-M05-03, -06) | UNACCEPTABLE | **S5×P1 = 5 (FMEA-M05-06, Rev 8)** | **ALARP** — see `NP-RISK-002` §4.3; the other M05 residuals are under `OI-RISK2-05` |
| SW01-M06 | Impedance Check | S4×P3 = 12 (FMEA-M06-03) | UNACCEPTABLE | S2×P1 = 2 | ACCEPTABLE |
| SW01-M07 | Session Protocol Signature Verification | S5×P1 = 5 (FMEA-M07-02) | ALARP | S3×P1 = 3 | ACCEPTABLE |
| SW01-M08 | Fault Latch and Fault Log | S5×P2 = 10 (FMEA-M08-01) | UNACCEPTABLE | S2×P1 = 2 | ACCEPTABLE |
| SW01-M10 | Tier Identity Gate | S4×P2 = 8 (FMEA-M10-01, -02, -03, -05) | ALARP | S4×P1 = 4 | ACCEPTABLE\*\* |

**All residual risks are ACCEPTABLE per NP-RM-001 §4.3 acceptability criteria.**

**\*** SW01-M03's residual rating is **conditional**: FMEA-M03-02 (delivered current diverging from
commanded) is scored against the hub's commanded-versus-delivered cross-check and SHDR divergence
flag, which are **specified but not yet built** (`OI-FMEA-07`). It is recorded as conditional rather
than as achieved because Rev 5 found this section carrying an UNACCEPTABLE-initial hazard as
mitigated by a control that never existed, and stating the dependency is what stops that recurring.
FMEA-M03-03's residual, by contrast, rests on controls that **are** built and tested.

**\*\*** SW01-M10's residual is **conditional** in the same way. FMEA-M10-04 and -05 are scored against
`OI-UPG-08`'s acceptance check, OTP programming and debug-port lock, which do not exist yet. Until
they do, the placeholder authority key makes every unit T1, so -05 cannot occur and -04 is certain on
any unit meant to be T2.

---

## 5. Overall Residual Risk Assessment

### 5.1 Residual risk summary

After application of all identified mitigations, all 43 failure modes across the 8 Safety MCU modules have residual risk ratings of **ACCEPTABLE** *(Rev 8: no longer true — FMEA-M05-06 is S5×P1 = ALARP; see `NP-RISK-002` §4.3, and `OI-RISK2-05` for M05 rows scored on controls the code lacks)* (S×P ≤ 4). No residual risks remain in the ALARP or UNACCEPTABLE bands.

This assessment is consistent with the NP-RM-001 overall residual risk evaluation requirements (§11). The following conditions, required before the formal ISO 14971 Overall Residual Risk Evaluation, remain outstanding:

- RISK-03 (PBM regulatory opinion) — OPEN, external
- RISK-20 (CFRP shell Ra confirmation) — OPEN, external  
- FAI-CV02 hardware bench (cardiac interlock timing) — PENDING T2 prototype
- FAI-HD01, HD03, HD04 hardware benches (sLORETA HD-tDCS) — PENDING T2 prototype

These outstanding items do not affect the software-level FMEA conclusions above. They are hardware and regulatory items. The software mitigations are implemented and unit-tested.

### 5.2 Architectural safety features not captured in individual failure modes

The bare-metal architecture of SW-01 eliminates entire classes of failure modes that would require additional FMEA entries in an RTOS-based design:

| Eliminated failure mode class | Reason eliminated in SW-01 |
|---|---|
| RTOS scheduler failure / task starvation | No RTOS; interrupt-driven bare-metal loop; interrupt priority assigned at design time |
| Heap fragmentation / malloc failure | No dynamic memory allocation; all storage is static or stack-allocated; MISRA C:2012 §21.3 prohibits malloc/free |
| Stack corruption from recursive calls | No recursion permitted (MISRA C:2012 §17.2); stack usage is bounded and analysed at compile time |
| RTOS inter-task communication deadlock | No tasks or mutexes; SPI access serialised by design |
| Timer drift from OS tick jitter | Cardiac interlock uses hardware TIM2 (1 µs hardware timer), not software delay loops |
| Watchdog refresh from any task | Watchdog (IWDG) refreshed only from the dedicated heartbeat handler; not refreshed from any other code path |

### 5.3 Relationship to system-level risk register

The following system-level risks are addressed by SW-01 FMEA mitigations. **Register moved 2026-08-11**: NP-RISK-001 → NP-RISK-002 (index, with the disposition of all 26 IDs) / NP-RISK-003 (hex-tile) / NP-RISK-004 (shell, socket, interconnect, hub):

| System risk | Addressed by SW01 module(s) | FMEA entries |
|---|---|---|
| RISK-25: Cervical VNS cardiac arrhythmia | SW01-M05 (cardiac interlock) | FMEA-M05-01 through M05-08 |
| RISK-14: PBM dose metering accuracy | SW01-M04 (thermal) | FMEA-M04-01 through M04-04 |
| Commanded charge overdose, DC (tDCS/HD-tDCS) | SW01-M03 (charge monitor, per-session ceiling) | FMEA-M03-01, -03, -04, -05, -06 |
| Commanded charge overdose, pulsed (BES/tACS/VNS/cervical VNS/clinical tACS) | SW01-M03 (charge monitor, per-phase ceiling) | FMEA-M03-03, -05, -06 |
| Delivered charge diverging from commanded | SW01-M03 (residual); hub cross-check + SHDR divergence flag (`OI-FMEA-07`, not yet built) | FMEA-M03-02 |
| Malicious session protocol injection | SW01-M07 (signature verification) | FMEA-M07-01 through M07-05 |
| SW-02 crash during active session | SW01-M02 (heartbeat watchdog) | FMEA-M02-01 through M02-06 |

### 5.4 Benefit-risk context

The NeurOne Safety MCU is the enabling architecture that allows SW-02 (Class B) to orchestrate a complex multi-modal session safely. Without the Safety MCU's independent hardware GPIO ownership, every SW-02 firmware module touching stimulation would require Class C treatment. The cost of the Safety MCU (STM32G071, +$0.45 BOM) and its independent Class C firmware development is justified by the architectural risk reduction it provides across the entire device.

The clinical benefit of the device — multi-modal neurostimulation supporting cognitive function, sleep, mood, and T2 clinical indications — substantially outweighs the residual risks identified in this FMEA, all of which have been reduced to ACCEPTABLE levels through the described mitigations.

---

## 6. Open Items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| OI-FMEA-01 | Hardware bench validation of SW01-M02 watchdog timeout timing (FMEA-M02-02, M02-05): oscilloscope measurement of GPIO cutoff latency from IWDG reset to all stimulation GPIO LOW; must confirm ≤50 ms. Requires hardware bench (FAI-HUB bench). | HW team | Full G1 gate closure for SW01-M02 |
| OI-FMEA-02 | Hardware bench validation of SW01-M05 cardiac interlock cutoff latency (FMEA-M05-03, M05-06): oscilloscope + R-peak signal generator; 10 consecutive trials must measure <5.1 ms worst-case (FAI-CV02). Requires T2 prototype. | HW team | Full G3-08 closure; T2 clinical release |
| OI-FMEA-03 | SW01-M07 manufacturing public key flash sector integrity: CRC-32 of key in flash must be computed and stored at programming time; bootloader must verify CRC before first signature verification; spec to be added to NP-FW-EMMC-001 Rev 2. Currently partial (PCROP active, CRC check planned). | FW team | G2 gate |
| ~~OI-FMEA-04~~ | **CLOSED NOT APPLICABLE 2026-09-15.** Called for bench validation of an SW01-M03 ADC self-test against "the production current-sense circuit". There is no current-sense ADC in this design and never was — the monitor reads commanded current over the SPI heartbeat — so there is nothing to validate and nothing to build. The hazard it was attached to (FMEA-M03-03) is re-authored in §3.3 against its real cause, and the delivered-dose concern it gestured at is FMEA-M03-02, whose control is `OI-FMEA-07`'s cross-check. A Class C current-sense channel remains a **hardware** ask, not a bench task — see `OI-FMEA-07`. | — | — |
| OI-FMEA-08 | **Decide whether the Class C charge monitor should carry a ramp-down margin.** `NP-SW-001` and `NP-FMEA-001` both specified an abort at *"95% of"* the ceiling *"to provide margin for the abort ramp-down period"*, and neither the margin nor the ramp-down exists in `np_charge_monitor.c`: it trips at 100% and clears the enable bit, which is abrupt by construction. The 95% figure exists only in Class B HD-tDCS (`np_hd_stim.c`). The question is real rather than editorial, because tDCS enforces a 30 s minimum ramp and a 100% trip leaves no budget to ramp within. Two honest answers: implement a margin in Class C (and specify what happens in it), or record that a charge cutoff is abrupt and that the ramp requirement does not apply to it. Both documents now say the margin is absent rather than implying it exists. | Safety SW + Quality | Before G2 firmware release |
| OI-FMEA-06 | **Re-analyse SW01-M04 (and the enable-mask rows of SW01-M01) against the hex-lattice architecture.** The retired 5-PBM-slot model that §3.4 is written against no longer exists: "zone" now means an overlapping, user-authored set of sockets (`00-zones.npps`) and cannot be a thermal or enable domain; the thermal domain is the **tile** (~80), the aggregation boundary is the **cluster** (18, NP-HW-HEXTILE-001 §8.2.1), the per-tile 62 °C throttle is on-module Class B, and module faults reach the safety MCU by per-cluster wire-OR `ALERT#`. **Three concrete items:** (a) FMEA-M04-01's control (zone index bounded 0–4, "all 5 zone indices" tested) is **invalid** and needs a replacement control against the real index domain; (b) `NP_NTC_CHANNEL_COUNT 6 /* 5 zones + 1 hub */` in `np_safety_config.h` encodes the retired count and must be re-derived — the safety MCU cannot present 80 NTC channels; (c) the **PA4 double-assignment** under FMEA-M08-04 is an open instance of that hazard class, resolving via the NP-HW-HUB-001 §7.2 zone-enable deletion. **Also re-check FMEA-M01-03 (wrong GPIO bit position) whenever the Class C enable word is re-laid out** — enable-bit position is identical to the charge-monitor channel index (`NP_SAFETY_MAX_CHANNELS`, `NP_SAFETY_CH_CLIN_STIM`, `current_ua[]`), so a bit move silently re-means charge accumulation (NP-HW-HEXTILE-001 §8.4.2). **S/P scores in §3.4 are frozen pending this re-analysis and should not be relied on.** | Quality Lead + Firmware | **Blocks SW01-M04 re-baseline; coordinate with OI-HUB-C07 / OI-HEXTILE-13** |
| OI-FMEA-05 | SW01-M06 mid-session impedance monitoring (FMEA-M06-03): firmware spec for continuous 1 Hz impedance monitoring during session requires authoring as a sub-requirement in NP-SW-001 §6.2; currently only pre-session impedance check is explicitly specified. | FW team | Before G2 firmware release |
| OI-FMEA-10 | **SW01-M09 has no FMEA section.** `np_nv_state.c` (the flash-persisted cardiac-cutoff log, `NP-SW-FAULTMSG-001` P1) is Class C and ships. It is cited here only as FMEA-M05-06's control. Author §3.9 for it. Also resolve the name: `NP-FW-M09-ARCH-001` assigns SW01-M09 to the operating-envelope gate, which is not built, while the firmware's SW01-M09 is the NV log (found at Rev 9) | FW team + Quality | Full G2 FMEA closure |

---

## 7. Revision History

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-06-06 | SmartyPants / PAI | Initial issue. Unit-level FMEA for SW01-M01 through SW01-M08 per IEC 62304 §7.1 Class C requirement. 43 failure modes across 8 modules. All residual risks ACCEPTABLE. Closes SW-01 FMEA pending decision in docs/status/pending-decisions.md §13.4. |
| 2 | 2026-06-15 | SmartyPants / PAI | OI-SW01-M07-02 CLOSED — §3.7 description updated to reflect that Ed25519 is now provided by the shared `np_crypto` library (Monocypher 4.0.2, PR #132) rather than a self-contained implementation. FMEA-M07-05 mitigation updated to reference Monocypher `ct_memcmp`. No failure modes added or removed; no risk scores changed. References: NP-SW-001 §9.4 SOUP table; `firmware/crypto/vendor/monocypher/VERSION`. |
| 9 | 2026-09-24 | NeurOne Firmware + Quality | **§3.10 SW01-M10 added (`OI-UPG-01`, `NP-REG-UPG-001` §7.7).** Seven failure modes. Worst initial S4×P2 = 8 ALARP, residual S4×P1 = 4 ACCEPTABLE, conditional on `OI-UPG-08` for M10-04 and -05, recorded like SW01-M03's. The gate's main-loop placement is verified by inspection only. **§1.2 corrected:** SW01-M09 exists in firmware with no section here, and its name collides with `NP-FW-M09-ARCH-001` (`OI-FMEA-10`). No existing score changed |
| 8 | 2026-09-23 | NeurOne Firmware + Quality | **FMEA-M05-06 corrected, and its residual changed from S2×P1 ACCEPTABLE to S5×P1 ALARP.** Its mitigation, battery-backed RTC backup registers, cannot exist on a device with no battery, and the firmware never implemented it. The real control is now the SW01-M09 NV log (`NP-SW-FAULTMSG-001` P1). The §4 SW01-M05 summary row follows. **§4's "all 43 residuals ACCEPTABLE" statement no longer holds for M05-06.** Other M05 rows and §2's IWDG claim describe controls the code does not contain: `OI-RISK2-05`, not changed here. |
| 7 | 2026-09-15 | NeurOne Firmware + Quality | **§3.3 RE-AUTHORED against the module that exists, and risk scores CHANGED (`OI-FMEA-07`).** Revs 5 and 6 flagged the gap and froze the scores; this revision closes it. **Two failure modes retired**: FMEA-M03-02 (*"ADC saturated"*) and the ADC premise of FMEA-M03-03 (*"ADC stuck low"*) were hazards of a current-sense ADC this design does not have, mitigated by an ADC self-test and 2-point calibration that could not be built — so an UNACCEPTABLE-initial hazard was carried as mitigated by a fiction. **Both are replaced, not dropped.** The real form of "the monitor's number is not what is in tissue" is **delivered current diverging from commanded** (new FMEA-M03-02, S4/P2 = 8), whose control is the hub cross-check and SHDR divergence flag — **specified but not yet built**, so SW01-M03's residual rating is now recorded as explicitly conditional in §4. FMEA-M03-03 keeps its **S5/P2 = 10 UNACCEPTABLE initial** with the cause corrected to *commanded current never reaching the monitor*, which was **the live state of the system** until the hub half of OI-CHARGE-01 was wired on 2026-09-15; its mitigations are now controls that exist and are tested (the wired publish, the fail-closed declaration gate, the SW01-M02 watchdog). **One failure mode added**: FMEA-M03-06, the wrong ceiling applied to a channel — judging a charge-balanced waveform by a session integral of \|I\| trips it in 0.4–1.0 s, which was the implemented behaviour and was latent only because the monitor was inert. FMEA-M03-01 re-scored P3→P1 on overflow (not credible at 64 bits) and re-pointed at mis-indexing, which now has a compile-time assertion behind it. **The module is renamed a commanded-dose control** throughout, and the safety argument for the substitution is stated for the first time: signature independence, not privacy. `OI-FMEA-04` closed NOT APPLICABLE (no ADC to bench-validate); `OI-FMEA-08` raised for the 95% ramp-down margin that both requirement documents specified and neither implementation has. |
| 6 | 2026-09-09 | NeurOne Firmware + Quality | **§3.3's flag corrected — the commanded-current substitution is not a privacy constraint, and a delivered-dose control is available without new hardware. No risk scores changed.** Rev 5's flag read the substitution as the UHDR/SHDR boundary forbidding a delivered-dose interlock. It does not: `np_safety_imp_report_t`'s privacy gate already transfers measured tissue impedance (UHDR-class) MCU→hub for cross-validation, keeps it device-internal, and records only a **divergence flag** (device condition, no biology) to SHDR. The rule is *"UHDR-class data may not land in SHDR"*, not *"may not cross SPI"*, and the pattern is already implemented and reviewed. The genuine constraints are **signature independence** — commanded current comes from a cryptographically signed descriptor whose hash the MCU verifies, a measured value cannot be signed in advance, so a Class C cutoff against a hub-measured number would inherit Class B integrity — and, for the strong form only, **a current-sense ADC channel the safety MCU does not have** (ADC1 fully allocated: six NTC thermal domains, six impedance channels including `NP_IMP1_ADC_CH` for tDCS). `OI-FMEA-07` now carries a recommended resolution requiring no new hardware: MCU keeps the hard cutoff on commanded current; the hub logs **both** doses to UHDR (it already writes delivered current, charge and impedance there — commanded is what is missing); the hub cross-checks the two and raises an SHDR divergence flag. **That is the mitigation FMEA-M03-02 and FMEA-M03-03 need to be scoreable at all** — their real hazard class is delivered charge diverging from what the monitor believes (driver stuck on, electrode lifting), which is undetectable as an ADC failure on this architecture and detectable as a divergence. Scores stay frozen pending that decision. |
| 5 | 2026-09-09 | NeurOne Firmware + Quality | **§3.3 (SW01-M03) flagged as not analysing the implemented module; `OI-FMEA-07` raised. No risk scores changed.** Six divergences between this section and `firmware/safety_mcu/src/np_charge_monitor.c`: the named file `np_charge_density.c/.h` does not exist; there is no current-sense ADC (commanded current over the 200 ms SPI heartbeat, 5 Hz, not measured); the trip is at 100% not 95%, so the ramp-down margin this section specifies is absent from the Class C monitor (it exists only in Class B `np_hd_stim.c`); response time is up to 200 ms not <25 µs; the accumulator is `uint64_t` with no roll-over detection; and the 32-bit-boundary unit test named as a control does not exist. **FMEA-M03-02 and FMEA-M03-03 are hazards of an ADC and their mitigations are fictional for this architecture**, which matters because FMEA-M03-03 is S5/P2 = 10 UNACCEPTABLE initial — an unacceptable hazard carried as mitigated by an unbuilt control — and because its effect (*"limit never reached; patient receives unlimited charge"*) **is the live state of the system**, the hub calling the heartbeat with `current_ua = NULL, channel_count = 0`. The ADC premise cannot simply be reinstated: the substitution was a deliberate UHDR/SHDR decision recorded in the source, and it makes this a commanded-dose control rather than a delivered-dose one — a safety argument no document has yet stated. Scores frozen and not to be relied on pending `OI-FMEA-07`; the ceiling's own validity is `OI-CHARGE-05`. |
| 4 | 2026-08-04 | SmartyPants / PAI | **Retired "zone" concept flagged across SW01-M04 and SW01-M01; OI-FMEA-06 raised. No risk scores changed, no failure modes added or removed.** §3.4 is written against the retired **5-module-slot** architecture in which a "zone" was one of five physical PBM slots with its own NTC ADC channel at the safety MCU. That architecture is gone and the change is not cosmetic: a zone is now *"a named SET OF MODULES, defined as a list of socket addresses"* authored in `protocols/predefined/00-zones.npps`, user-extensible, unbounded in count, and **overlapping** (the inclusive-midline rule puts every midline socket in BOTH hemisphere zones of its lobe) — so a zone **cannot be a thermal or enable domain at all**. The thermal domain is now the **tile** (~80 sockets) and the aggregation boundary the **cluster** (18 — NP-HW-HEXTILE-001 §8.2.1); the safety MCU cannot present 80 NTC ADC channels, so the per-tile 62 °C junction throttle is on-module Class B (CLAUDE.md §4.2, required to act with the I2C bus silent) and module faults arrive by per-cluster wire-OR `ALERT#` (NP-DRV-SHELL-002 §6). Changes made: §3.4 carries an ARCHITECTURAL BASIS SUPERSEDED banner; **FMEA-M04-01's mitigation is marked INVALID** (its control bounds a zone index to 0–4 and unit-tests "all 5 zone indices" — both artifacts of the retired architecture), with the original text retained verbatim for traceability; FMEA-M01-03 flagged for re-check on any Class C enable-word re-layout, since enable-bit position is identical to the charge-monitor channel index (`NP_SAFETY_MAX_CHANNELS` / `NP_SAFETY_CH_CLIN_STIM` / `current_ua[]`), so moving a bit silently re-means charge accumulation; **FMEA-M08-04 gains a recorded open instance** — `np_safety_config.h` declares **PA4** as both SPI1 NSS (load-bearing for frame delineation) and `NP_EN_PBM_ZONE4_PIN`, which is exactly this row's hazard and whose stated control ("GPIO assignment verified against schematic in hardware design review") has not yet executed; and `NP_NTC_CHANNEL_COUNT 6 /* 5 zones + 1 hub */` is flagged as encoding the retired count. **Re-scoring deferred to OI-FMEA-06 by design** — assigning S/P values to the new architecture is hazard analysis, not an editorial correction, so §3.4's scores are frozen and marked not-to-be-relied-on rather than silently updated. **No firmware changed.** Rev 3 → D. Effective 2026-08-04. |
| 3 | 2026-07-13 | SmartyPants / PAI | OI-CVNS-HUB-11 — SW01-M06 §3.6 delta: safety MCU now reports per-electrode cervical VNS impedance to the hub for numeric cross-validation against the hub's own measurement. Two new failure modes added (FMEA-M06-05 corrupted report accepted; FMEA-M06-06 stale report path), both ACCEPTABLE (report is additive telemetry, magic+checksum validated, invalidated per session, cannot alter the Class C enable gate; divergence is fail-closed). No change to the existing single-value enable gate or its risk scores. References: `firmware/common/include/np_spi_wire_types.h` (`np_safety_imp_report_t`), NP-FW-EMMC-001 §12 (classification), NP-FW-CVNS-001. |
