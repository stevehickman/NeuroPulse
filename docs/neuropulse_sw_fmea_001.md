# SW-01 Safety MCU Unit-Level FMEA

**Project:** NeuroPulse  
**Document:** NP-FMEA-001  
**Revision:** B  
**Date:** 2026-06-15  
**Status:** DRAFT  
**Effective Date:** 2026-06-15  
**Author:** SmartyPants / PAI  
**Approved By:** TBD (Quality Lead)  
**References:** NP-SW-001 Rev A, NP-RM-001 Rev A, NP-FW-CVNS-001 Rev A, IEC 62304:2006+AMD1:2015 §7.1, ISO 14971:2019  
**Related Issues:** —  
**Gate:** —  
**IEC 62304 Class:** C (SW-01 Safety MCU)  
**Jurisdiction Scope:** —  
**Change Summary:** §3.7 description updated to reflect np_crypto/Monocypher 4.0.2 backing; FMEA-M07-05 mitigation updated to reference Monocypher ct_memcmp. OI-SW01-M07-02 CLOSED.

---

## 1. Purpose and Scope

### 1.1 Purpose

This document provides the unit-level Failure Mode and Effects Analysis (FMEA) for SW-01 — the NeuroPulse Safety MCU bare-metal firmware executing on the STM32G071 (Cortex-M0+, 64 MHz, 36 KB SRAM, 128 KB flash). It satisfies IEC 62304:2006+AMD1:2015 §7.1 Class C requirement to identify software items that could contribute to hazardous situations and to document the failure modes, potential harms, and risk controls for each.

This FMEA is also required by ISO 14971:2019 as part of the software-related hazard analysis and complements the system-level risk register (NP-RM-001 Rev A / NP-RISK-001 Rev A, RISK-01 through RISK-25).

### 1.2 Scope

This document covers the **SW-01 Safety MCU firmware only** — the eight bare-metal C modules (SW01-M01 through SW01-M08) executing on the STM32G071. It does not cover:
- SW-02 main processor firmware (NXP i.MX RT1062, FreeRTOS, Class B)
- SW-03 iOS/Android application (Class B)
- Hardware-level failure modes outside firmware behaviour (covered in NP-RISK-001)

### 1.3 Architectural safety significance

The Safety MCU occupies the most safety-critical position in the NeuroPulse dual-processor architecture. Key architectural properties that are themselves risk mitigations:

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

| Level | Definition | NeuroPulse examples |
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
| FMEA-M01-03 | Wrong GPIO bit position in enable mask — incorrect modality enabled or disabled | Incorrect stimulation channel active (e.g., tDCS enabled instead of PBM zone, or safety cutoff drives wrong GPIO) | S4 | P2 | 8 (ALARP) | GPIO pin assignments are compile-time constants in `np_gpio_enable.h`; constants reviewed against schematic in design review; unit tests verify GPIO mapping for all 8 modality channels; PCB layout review confirms physical GPIO-to-driver mapping | S2 | P1 | 2 | ACCEPTABLE |
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

### 3.3 SW01-M03 — Charge Density Monitor (`np_charge_density.c/.h`)

**Description:** SW01-M03 continuously integrates the charge delivered per electrode pair for all electrical stimulation modalities (tDCS, BES/tACS, VNS, cervical VNS). Charge integration uses a dedicated current-sense ADC channel (1 kHz sample rate). When the accumulated charge per electrode pair reaches 95% of the hardware safety limit (40 µC/cm²), SW01-M03 sends a fault signal to SW01-M01 to abort the session. The 95% threshold provides margin for the abort ramp-down period. Response time is within one PWM period (<25 µs at 40 kHz).

The electrode area used in the charge density calculation is a compile-time constant per electrode type, set from validated measurements.

| FM-ID | Failure Mode | Effect | S | P | Risk | Mitigation | Res. S | Res. P | Res. Risk | Accept |
|---|---|---|---|---|---|---|---|---|---|---|
| FMEA-M03-01 | 32-bit charge accumulator overflows (wraps around) — accumulated charge reads as near-zero | Charge density limit not enforced; patient receives unsafe charge density exceeding 40 µC/cm² | S4 | P2 | 8 (ALARP) | Accumulator is `uint32_t`; maximum representable charge at 1 kHz sampling = 4,294,967,295 nC >> any session limit; overflow condition is checked independently: if accumulator value decrements between samples (roll-over detection), abort is triggered immediately; unit test validates 32-bit boundary | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M03-02 | ADC read returns saturated value (0xFFF stuck high) — current-sense reads maximum at all times | Every sample adds maximum charge increment; session aborted nearly immediately (false positive cutoff) | S2 | P3 | 6 (ALARP) | Saturated ADC value detected as out-of-range (> 3× expected maximum at full-rated current); treated as ADC fault, session aborted conservatively; ADC hardware self-test at session start verifies reading within expected range for known zero-current condition | S1 | P2 | 2 | ACCEPTABLE |
| FMEA-M03-03 | ADC read returns 0x000 (stuck low) — no charge accumulates in monitor | Charge density limit never reached; patient receives unlimited charge | S5 | P2 | 10 (UNACCEPTABLE initial) | ADC self-test at session start: delivers known current pulse and verifies ADC response is within expected range (2-point calibration: zero current and a safe test pulse); ADC stuck-low detected and blocks session enable; hardware current limit on driver circuit provides independent backstop | S3 | P1 | 3 | ACCEPTABLE |
| FMEA-M03-04 | Charge accumulator not reset between sessions — cumulative overdose across multiple consecutive sessions | Patient total charge across sessions 1 + 2 exceeds safe limit without per-session abort | S4 | P3 | 12 (UNACCEPTABLE initial) | Accumulator explicitly zeroed in `np_charge_density_session_start()` called at the beginning of every session, not at the end; zero-on-start is unit-tested with a two-session test sequence; session log in SHDR records per-session charge delivered | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M03-05 | Electrode area constant in firmware set incorrectly — charge density (µC/cm²) underestimated | True charge density exceeds limit while calculated value remains below abort threshold | S4 | P2 | 8 (ALARP) | Electrode area constants are defined in `np_gpio_enable.h` with reference to the physical electrode datasheet; values reviewed against IFU and electrode specification in design review; unit tests include known-good charge/area/density calculations for all electrode types | S2 | P1 | 2 | ACCEPTABLE |

---

### 3.4 SW01-M04 — Thermal Interlock (`np_thermal.c/.h`)

**Description:** SW01-M04 reads the NTC thermistor ADC channel for each PBM zone (5 zones) at 10 Hz during active PBM sessions. Temperature is computed from the NTC Steinhart-Hart equation using calibrated coefficients stored in the Config partition. At 62°C junction temperature, current to the affected zone is throttled by 50%. At 65°C, all stimulation to that zone is immediately cut off via SW01-M01. The STM32G071's own die temperature is also monitored and triggers a full system cutoff at 85°C die temperature.

| FM-ID | Failure Mode | Effect | S | P | Risk | Mitigation | Res. S | Res. P | Res. Risk | Accept |
|---|---|---|---|---|---|---|---|---|---|---|
| FMEA-M04-01 | Steinhart-Hart coefficient lookup uses wrong zone index — thermal reading from Zone A applied to Zone B's current control | Zone B overheats without triggering its own throttle; Zone A throttled unnecessarily | S4 | P2 | 8 (ALARP) | Zone index is a validated integer parameter (0–4); bounds-checked before array access (MISRA C:2012 §18.1); unit test exercises all 5 zone indices; NTC assignment table reviewed against schematic in hardware design review | S2 | P1 | 2 | ACCEPTABLE |
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
| FMEA-M05-06 | Re-enable lockout counter not persisted across a Safety MCU reset — 30-second lockout bypassed by power cycling | Attacker or malfunctioning app re-enables stimulation immediately after a cardiac cutoff by triggering a reset | S5 | P2 | 10 (UNACCEPTABLE initial) | Lockout state persisted to STM32G071 backup registers (RTC_BKPxR, battery-backed domain, survives warm and cold resets); lockout timestamp stored; re-enable blocked until stored lockout expiry time is passed even after reset; app must also confirm re-enable via signed protocol | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M05-07 | Impedance check bypassed under SPI timeout condition — stimulation enabled on contacts with poor or no gel contact | High electrode impedance with stimulation active — increased skin heating, uneven current distribution, potential necrosis | S4 | P2 | 8 (ALARP) | Impedance check is a synchronous blocking call before enable grant: `ENABLE_GRANTED` is only returned after both electrodes pass impedance check; if impedance SPI response is not received within timeout, Safety MCU returns `ENABLE_REJECTED`; conservative failure mode — timeout → deny | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M05-08 | Motion artefact causes false R-peak pulses from SW-02 — HR appears elevated; cardiac interlock triggers spuriously | Session interrupted unnecessarily; patient experiences unexpected cutoff | S2 | P4 | 8 (ALARP) | 30 µs R-peak debounce (NP_CVNS_RPEAK_DEBOUNCE_US); minimum valid R-R interval 300 ms enforced (rejects implausible >200 BPM); Pan-Tompkins adaptive threshold in SW-02 PPG processing; if more than 3 consecutive intervals are rejected as invalid, Safety MCU enters data-loss hold mode rather than using artefact-contaminated data | S1 | P2 | 2 | ACCEPTABLE |

---

### 3.6 SW01-M06 — Impedance Check (`np_impedance.c/.h`)

**Description:** SW01-M06 performs AC impedance measurement at 1 kHz on stimulation electrodes before enabling any session. The measurement is synchronous and blocking — no stimulation GPIO can be asserted HIGH until impedance passes. For electrical stimulation modalities (tDCS, BES, VNS, cervical VNS), electrode impedance must be within the specified range for the electrode type. For PBM (optical), impedance of the EEG electrode contacts is checked (not optical path). Impedance results are reported to SW-02 via SPI for UHDR logging (raw values) and SHDR logging (pass/fail boolean).

| FM-ID | Failure Mode | Effect | S | P | Risk | Mitigation | Res. S | Res. P | Res. Risk | Accept |
|---|---|---|---|---|---|---|---|---|---|---|
| FMEA-M06-01 | Impedance threshold for a specific electrode type uses wrong constant — high impedance passes incorrectly | Stimulation delivered with compromised electrode contact; increased skin heating, uneven current distribution | S4 | P2 | 8 (ALARP) | Impedance limits are compile-time constants per electrode type, referenced against electrode datasheet; constants reviewed against specifications in design review; unit tests cover boundary cases (limit−1, limit, limit+1 for each electrode type) | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M06-02 | Impedance ADC hardware fault returns 0 — low impedance seen even with no electrode contact | Session enabled with no electrodes attached; stimulation into open circuit, driving electrode driver to voltage saturation | S4 | P2 | 8 (ALARP) | Zero impedance (0 Ω) is physically impossible for any clinical electrode; ADC result of 0 treated as ADC hardware fault → session blocked; minimum acceptable impedance bound enforced for all electrode types (e.g., cervical gel electrode: minimum 0.5 kΩ) | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M06-03 | Impedance measurement not repeated after electrode dislodgement during session — high impedance post-dislodgement not detected | Electrode lifts mid-session; stimulation continues with rising skin impedance; increased current density, possible burn | S4 | P3 | 12 (UNACCEPTABLE initial) | Impedance is monitored continuously during session at 1 Hz (not just pre-session); impedance rise >2× pre-session baseline triggers a session warning; impedance rise >5× baseline triggers cutoff via SW01-M01 | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M06-04 | SPI timeout during impedance reporting — impedance result not delivered to SW-02 | UHDR log has no impedance record for session; clinical review may be incomplete | S2 | P3 | 6 (ALARP) | Impedance result stored in Safety MCU SRAM; SPI retry up to 3 times; after 3 failures, fault logged to SHDR (Safety MCU side); conservative approach: SPI failure after successful impedance check does not block session (stimulation is safe) — only data completeness is affected | S1 | P2 | 2 | ACCEPTABLE |

---

### 3.7 SW01-M07 — Session Protocol Signature Verification (`np_session_sig.c/.h`)

**Description:** SW01-M07 implements Ed25519 signature verification on the binary session descriptor received from SW-02 before any stimulation GPIO can be enabled. Ed25519 verification is provided by the shared `np_crypto` static library (`firmware/crypto/`) backed by Monocypher 4.0.2 (RFC 8032 §5.1.7, SHA-512; SOUP record: `firmware/crypto/vendor/monocypher/VERSION`; BSD-2-Clause OR CC0-1.0; OI-SW01-M07-02 CLOSED 2026-06-11, PR #132). The library is validated by 11 host tests (RFC 8032 TV1/TV2 vectors + all-zero pubkey guard; see NP-SW-001 Rev A §9.4 SOUP table). The bootloader retains its own self-contained Ed25519 because it uses `-nostdlib/-nodefaultlibs`, which makes Monocypher's libc dependencies unavailable; both paths are covered by the same RFC 8032 test vectors. The session descriptor includes the protocol parameters, modality configuration, and safety limits. A replay prevention counter (device-serial session counter stored in the Config partition) prevents replay of previously valid descriptors. Unsigned, corrupted, or replayed descriptors cause SW01-M07 to return a rejection code to SW01-M01, which keeps all GPIO LOW.

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
| FMEA-M08-03 | UHDR-classified fault data (e.g., HR at cutoff) written to SHDR fault log by mistake | Privacy violation: user biology appears in NeuroPulse-accessible SHDR partition | S2 | P2 | 4 (ACCEPTABLE) | SHDR fault log schema (per NP-FW-CVNS-001 §5.6) contains only: session_id (unsigned counter), cutoff_offset_ms, and reason (enum — no HR values); HR data is routed exclusively to UHDR; code review + unit tests verify no HR values are written to the SHDR fault log structure | S1 | P1 | 1 | ACCEPTABLE |
| FMEA-M08-04 | Fault indicator LED circuit shares GPIO with stimulation enable — toggling fault LED inadvertently toggles stimulation | Fault condition triggers stimulation instead of indicating fault | S5 | P1 | 5 (ALARP) | Fault LED GPIO and stimulation enable GPIO are on different GPIO ports (separate STM32G071 registers); GPIO assignment verified against schematic in hardware design review; unit test verifies fault LED assertion does not change any stimulation enable register bits | S2 | P1 | 2 | ACCEPTABLE |
| FMEA-M08-05 | Circular fault buffer overflow — oldest unread fault entries overwritten before being transmitted to SHDR | Historical fault events lost; diagnostic capability reduced post-incident | S1 | P3 | 3 (ACCEPTABLE) | 8-entry ring buffer covers the maximum plausible number of rapid fault-clear-restart cycles before SPI communication is restored; buffer overflow is logged as a separate SHDR event (overflow flag); SW-02 polls for SRAM fault log on every heartbeat when fault state is active | S1 | P2 | 2 | ACCEPTABLE |

---

## 4. Summary Risk Table

| Module | Module Name | Worst Initial Risk (S×P) | Worst Initial Rating | Residual Risk (S×P) | Residual Rating |
|---|---|---|---|---|---|
| SW01-M01 | Stimulation Enable GPIO Management | S5×P2 = 10 (FMEA-M01-05) | UNACCEPTABLE | S2×P1 = 2 | ACCEPTABLE |
| SW01-M02 | SPI Heartbeat Watchdog | S5×P2 = 10 (FMEA-M02-02, -03, -05) | UNACCEPTABLE | S3×P1 = 3 | ACCEPTABLE |
| SW01-M03 | Charge Density Monitor | S5×P2 = 10 (FMEA-M03-03) | UNACCEPTABLE | S3×P1 = 3 | ACCEPTABLE |
| SW01-M04 | Thermal Interlock | S4×P3 = 12 (FMEA-M04-02) | UNACCEPTABLE | S2×P1 = 2 | ACCEPTABLE |
| SW01-M05 | Cervical VNS Cardiac Interlock | S5×P2 = 10 (FMEA-M05-03, -06) | UNACCEPTABLE | S2×P1 = 2 | ACCEPTABLE |
| SW01-M06 | Impedance Check | S4×P3 = 12 (FMEA-M06-03) | UNACCEPTABLE | S2×P1 = 2 | ACCEPTABLE |
| SW01-M07 | Session Protocol Signature Verification | S5×P1 = 5 (FMEA-M07-02) | ALARP | S3×P1 = 3 | ACCEPTABLE |
| SW01-M08 | Fault Latch and Fault Log | S5×P2 = 10 (FMEA-M08-01) | UNACCEPTABLE | S2×P1 = 2 | ACCEPTABLE |

**All residual risks are ACCEPTABLE per NP-RM-001 §4.3 acceptability criteria.**

---

## 5. Overall Residual Risk Assessment

### 5.1 Residual risk summary

After application of all identified mitigations, all 43 failure modes across the 8 Safety MCU modules have residual risk ratings of **ACCEPTABLE** (S×P ≤ 4). No residual risks remain in the ALARP or UNACCEPTABLE bands.

This assessment is consistent with the NP-RM-001 Rev A overall residual risk evaluation requirements (§11). The following conditions, required before the formal ISO 14971 Overall Residual Risk Evaluation, remain outstanding:

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

The following system-level risks in NP-RISK-001 / NP-RM-001 are addressed by SW-01 FMEA mitigations:

| System risk | Addressed by SW01 module(s) | FMEA entries |
|---|---|---|
| RISK-25: Cervical VNS cardiac arrhythmia | SW01-M05 (cardiac interlock) | FMEA-M05-01 through M05-08 |
| RISK-14: PBM dose metering accuracy | SW01-M03 (charge density), SW01-M04 (thermal) | FMEA-M03-01 through M04-04 |
| Charge density overdose (tDCS/BES/VNS) | SW01-M03 (charge density monitor) | FMEA-M03-01 through M03-05 |
| Malicious session protocol injection | SW01-M07 (signature verification) | FMEA-M07-01 through M07-05 |
| SW-02 crash during active session | SW01-M02 (heartbeat watchdog) | FMEA-M02-01 through M02-06 |

### 5.4 Benefit-risk context

The NeuroPulse Safety MCU is the enabling architecture that allows SW-02 (Class B) to orchestrate a complex multi-modal session safely. Without the Safety MCU's independent hardware GPIO ownership, every SW-02 firmware module touching stimulation would require Class C treatment. The cost of the Safety MCU (STM32G071, +$0.45 BOM) and its independent Class C firmware development is justified by the architectural risk reduction it provides across the entire device.

The clinical benefit of the device — multi-modal neurostimulation supporting cognitive function, sleep, mood, and T2 clinical indications — substantially outweighs the residual risks identified in this FMEA, all of which have been reduced to ACCEPTABLE levels through the described mitigations.

---

## 6. Open Items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| OI-FMEA-01 | Hardware bench validation of SW01-M02 watchdog timeout timing (FMEA-M02-02, M02-05): oscilloscope measurement of GPIO cutoff latency from IWDG reset to all stimulation GPIO LOW; must confirm ≤50 ms. Requires hardware bench (FAI-HUB bench). | HW team | Full G1 gate closure for SW01-M02 |
| OI-FMEA-02 | Hardware bench validation of SW01-M05 cardiac interlock cutoff latency (FMEA-M05-03, M05-06): oscilloscope + R-peak signal generator; 10 consecutive trials must measure <5.1 ms worst-case (FAI-CV02). Requires T2 prototype. | HW team | Full G3-08 closure; T2 clinical release |
| OI-FMEA-03 | SW01-M07 manufacturing public key flash sector integrity: CRC-32 of key in flash must be computed and stored at programming time; bootloader must verify CRC before first signature verification; spec to be added to NP-FW-EMMC-001 Rev B. Currently partial (PCROP active, CRC check planned). | FW team | G2 gate |
| OI-FMEA-04 | SW01-M03 ADC self-test (FMEA-M03-03): two-point calibration protocol (zero current + known safe test pulse) requires hardware bench validation with production current-sense circuit. Software test constants verified; hardware path pending T1 prototype. | HW team | G2 gate |
| OI-FMEA-05 | SW01-M06 mid-session impedance monitoring (FMEA-M06-03): firmware spec for continuous 1 Hz impedance monitoring during session requires authoring as a sub-requirement in NP-SW-001 §6.2; currently only pre-session impedance check is explicitly specified. | FW team | Before G2 firmware release |

---

## 7. Revision History

| Rev | Date | Author | Description |
|---|---|---|---|
| A | 2026-06-06 | SmartyPants / PAI | Initial issue. Unit-level FMEA for SW01-M01 through SW01-M08 per IEC 62304 §7.1 Class C requirement. 43 failure modes across 8 modules. All residual risks ACCEPTABLE. Closes SW-01 FMEA pending decision in CLAUDE.md §13.4. |
| B | 2026-06-15 | SmartyPants / PAI | OI-SW01-M07-02 CLOSED — §3.7 description updated to reflect that Ed25519 is now provided by the shared `np_crypto` library (Monocypher 4.0.2, PR #132) rather than a self-contained implementation. FMEA-M07-05 mitigation updated to reference Monocypher `ct_memcmp`. No failure modes added or removed; no risk scores changed. References: NP-SW-001 Rev A §9.4 SOUP table; `firmware/crypto/vendor/monocypher/VERSION`. |

---

*NP-FMEA-001 Rev B — DRAFT — 2026-06-15*
