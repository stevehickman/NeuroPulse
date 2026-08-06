# Cervical VNS Safety Interlock Firmware Specification

**Project:** NeurOne
**Document:** NP-FW-CVNS-001
**Revision:** B
**Date:** 2026-08-05
**Status:** BASELINED
**Effective Date:** 2026-08-05
**Author:** Steve Hickman (CEO, interim Quality authority)
**Approved By:** Steve Hickman, CEO
**References:** CLAUDE.md §3 T2 additions (cervical VNS accessory)
**Related Issues:** GitHub Issue #24
**Gate:** NP-COORD-001 G3-08
**IEC 62304 Class:** SW-01 Class C (safety MCU) / SW-02 Class B (main processor)
**Supersedes:** NP-FW-CVNS-001 Rev A
**Parent Document:** NP-SW-001

---

**Rev B (2026-08-05):** Reconciles §5 (safety MCU, Class C) with the firmware it specifies. Rev A §5 described the safety MCU using the **main processor's** constants and behaviours — the root cause of most of the divergences below — and specified one operation the firmware cannot perform.

*Corrected outright (documentation errors; no firmware behaviour changed):*
- **§5.3 constants.** Rev A cited `NP_CVNS_RR_WINDOW_SIZE` (20) and `NP_CVNS_BASELINE_BEATS_MIN` (5) as the safety MCU's. Both live in `firmware/cervical_vns/include/np_cvns_config.h` — the **main-processor** module specified in §6 of this same document. The safety MCU has its own header and its own values (`NP_CARDIAC_BASELINE_BEATS` = 8, `NP_RR_BUF_SIZE` = 8). §5.3 now names the safety MCU's own constants and states explicitly that the two sides compute baselines independently and cross-validate at ±5 BPM — which is the *reason* they are separate implementations.
- **§5.3 step 2 R-R validity window.** The 300–2000 ms discard rule is main-processor behaviour; the safety MCU applies no such filter. Marked as not implemented on the MCU side and escalated (OI-CVNS-11).
- **§5.4.2a dual-enable cutoff.** Rev A instructed asserting `CVNS_ENABLE_L` **and** `CVNS_ENABLE_R`. The firmware has a single CVNS enable (`NP_EN_CVNS`); the two-line assertion is unimplementable as written. §5.4.2a now describes the cutoff the firmware actually performs, and the enable-line count is escalated (OI-CVNS-09).
- **§5.4.3 conservative hold.** The "fewer than 3 valid intervals → warning flag → 10 s → soft cutoff" behaviour is the main processor's (`NP_CVNS_DATA_LOSS_TIMEOUT_S`). The safety MCU's hold is different and stricter: it will not arm at all until 8 intervals have accumulated, and it has no soft-cutoff timer. Corrected to describe the MCU.
- **§5.1 pin map.** Marked **unverified** rather than corrected in either direction — see OI-CVNS-08. Rev A's map is retained verbatim as the claim under dispute; it is no longer presented as the requirement.

*Escalated, deliberately NOT resolved here* — each is a design or clinical decision, not a documentation defect: **OI-CVNS-08** (STM32G071 pin map), **OI-CVNS-09** (single vs dual CVNS enable line), **OI-CVNS-10** (baseline window 5 vs 8), **OI-CVNS-11** (R-R validity filtering on the MCU). Each carries two candidate resolutions, the evidence for each, and what would settle it. No number in either the doc or the firmware was changed to make the other side agree.

*Verification added:* `firmware/safety_mcu/tests/np_cardiac_interlock_tests.c` — the first host-test coverage of the Class C cardiac interlock. Rev A's only "cardiac interlock" suite (`np_cvns_fai_tests`, registered as NP-FAI-CVNS-001) exercises the **main-processor** module on the other side of the SPI boundary and asserts on main-processor constants; it never touched the unit that owns the enable GPIO. The new suite pins current behaviour, including the constants under OI-CVNS-10, so any later change surfaces as a reviewed failing assertion rather than a silent edit.

---

## 1. Scope

This document specifies the firmware implementation of the NeurOne cervical VNS (tcVNS) T2 accessory safety interlock. The accessory stimulates the cervical vagus trunk via transcutaneous gel electrodes placed over the carotid sheath. Because the current path runs near the carotid artery and vagus nerve trunk, a dedicated cardiac safety interlock in the STM32G071 safety MCU owns stimulation enable and monitors cardiac rhythm in real time.

Modules covered:

| Module | Location | Description |
|--------|----------|-------------|
| `np_cvns_interlock` | `firmware/cervical_vns/` | Cardiac interlock state machine (main processor side) |
| `np_cvns_stim` | `firmware/cervical_vns/` | Stimulation delivery, ramp control, charge balance |
| `np_cvns_session` | `firmware/cervical_vns/` | Session orchestration, prechecks, UHDR/SHDR record |

**Safety MCU bare-metal behavior** is specified in §5 below; the C implementation of the safety MCU firmware is separate (IEC 62304 Class C, ~500 lines bare-metal on STM32G071, certified independently).

**Not in scope:** auricular taVNS (handled by HRV biofeedback firmware, NP-FW-HRV-001 Rev A); hub accessory port electrical driver.

---

## 2. Clinical Context

### 2.1 Indication and predicate

| Item | Detail |
|------|--------|
| Target anatomy | Cervical vagus nerve trunk, stimulated transcutaneously via skin overlying the carotid sheath |
| Electrode placement | Bilateral or unilateral gel electrodes; left or right lateral neck |
| Stimulation modality | Biphasic charge-balanced electrical stimulation |
| Indication (T2 target) | Cluster headache, migraine, depression, PTSD, post-stroke rehabilitation |
| FDA predicate | electroCore gammaCore K163334 (cluster headache, 2016) and K173323 (migraine, 2017) |
| Regulatory pathway | 510(k); substantial equivalence to gammaCore predicates (see NP-REG-CVNS-001 Rev A) |

### 2.2 Rationale for safety MCU ownership

The carotid sheath contains the common carotid artery, internal jugular vein, and vagus nerve trunk. Electrical stimulation in this region can influence:
- Baroreceptor reflex (carotid sinus → HR and blood pressure changes)
- Direct vagal efferent activity (cardiac rate, AV conduction)

The safety MCU (STM32G071, bare-metal, IEC 62304 Class C) must own the cervical VNS enable GPIO independently of the main processor. An app crash, main processor fault, or SPI communication loss must result in stimulation cutoff within the watchdog interval (1.5 s), but the cardiac interlock cutoff must occur within 100 ms of detecting a qualifying HR change — this is faster than the SPI heartbeat period and is therefore implemented by the safety MCU independently using a dedicated R-peak GPIO signal from the main processor.

---

## 3. Configuration Constants (`np_cvns_config.h`)

### 3.1 Stimulation parameters

| Constant | Value | Description |
|----------|-------|-------------|
| `NP_CVNS_FREQ_HZ_MIN` | 1 | Minimum stimulation frequency (Hz) |
| `NP_CVNS_FREQ_HZ_MAX` | 25 | Maximum stimulation frequency (Hz) — matches gammaCore range |
| `NP_CVNS_CURRENT_UA_MAX` | 2000 | Maximum current: 2 mA (CLAUDE.md §3 T2 spec) |
| `NP_CVNS_PULSE_WIDTH_US_MIN` | 200 | Minimum pulse width (µs) |
| `NP_CVNS_PULSE_WIDTH_US_MAX` | 1000 | Maximum pulse width (µs) |
| `NP_CVNS_SESSION_MIN_S` | 60 | Minimum session duration (s) |
| `NP_CVNS_SESSION_MAX_S` | 120 | Maximum session duration (s) — gammaCore protocol: 2 min |

### 3.2 Electrode and contact

| Constant | Value | Description |
|----------|-------|-------------|
| `NP_CVNS_ELECTRODE_COUNT` | 2 | Bilateral electrode assembly |
| `NP_CVNS_IMPEDANCE_MAX_KOHM` | 5.0 | Cervical gel electrode impedance limit (kΩ) |
| `NP_CVNS_IMPEDANCE_CHECK_FREQ_HZ` | 1000 | AC impedance check frequency (1 kHz) |

### 3.3 Ramp timing

| Constant | Value | Description |
|----------|-------|-------------|
| `NP_CVNS_RAMP_UP_S` | 10 | Current ramp-up period (s) — comfort and vagal adaptation |
| `NP_CVNS_RAMP_DOWN_S` | 5 | Current ramp-down period (s) |

### 3.4 Cardiac interlock

| Constant | Value | Description |
|----------|-------|-------------|
| `NP_CVNS_HR_CHANGE_LIMIT_BPM` | 15 | Cutoff threshold (BPM change from baseline) — CLAUDE.md §3 |
| `NP_CVNS_HR_WINDOW_S` | 5 | Observation window (s) |
| `NP_CVNS_CARDIAC_POLL_MS` | 5 | Safety MCU cardiac polling interval (200 Hz) |
| `NP_CVNS_CUTOFF_LATENCY_MAX_MS` | 100 | FAI-CV02 pass criterion: GPIO low within 100 ms of detection |
| `NP_CVNS_BASELINE_BEATS_MIN` | 5 | Minimum beats to establish stable baseline before enable |
| `NP_CVNS_RR_WINDOW_SIZE` | 20 | Rolling R-R interval buffer depth (samples) |
| `NP_CVNS_RR_MAX_VALID_MS` | 2000 | Maximum valid R-R interval (≈30 BPM) |
| `NP_CVNS_RR_MIN_VALID_MS` | 300 | Minimum valid R-R interval (≈200 BPM) |
| `NP_CVNS_RPEAK_DEBOUNCE_US` | 30 | R-peak GPIO edge debounce (µs) |

### 3.5 Re-enable policy

| Constant | Value | Description |
|----------|-------|-------------|
| `NP_CVNS_REENABLE_LOCKOUT_S` | 30 | Minimum lockout period after cardiac interlock cutoff |

After any cardiac interlock cutoff, re-enabling requires **all three** of:
1. Lockout period elapsed (`NP_CVNS_REENABLE_LOCKOUT_S`)
2. Explicit app confirmation command over signed session protocol
3. Repeat impedance check (both electrodes must pass) and cardiac baseline re-established

---

## 4. Type Definitions (`np_cvns_types.h`)

### 4.1 Status codes

```c
typedef enum {
    NP_CVNS_OK                    =  0,
    NP_CVNS_ERR_INVALID_ARG       = -1,
    NP_CVNS_ERR_IMPEDANCE_HIGH    = -2,  /* electrode impedance exceeds limit     */
    NP_CVNS_ERR_SAFETY_REJECTED   = -3,  /* safety MCU denied enable              */
    NP_CVNS_ERR_CARDIAC_CUTOFF    = -4,  /* cardiac interlock triggered           */
    NP_CVNS_ERR_BASELINE_INVALID  = -5,  /* baseline HR not yet established       */
    NP_CVNS_ERR_SESSION_ACTIVE    = -6,
    NP_CVNS_ERR_NO_SESSION        = -7,
    NP_CVNS_ERR_LOCKOUT           = -8,  /* re-enable lockout active              */
    NP_CVNS_ERR_CHARGE_LIMIT      = -9,  /* charge density limit would be exceeded*/
    NP_CVNS_ERR_WAVEFORM_INVALID  = -10, /* biphasic balance check failed         */
} np_cvns_status_t;
```

### 4.2 Interlock state machine

```
PRE_SESSION ──[impedance OK + baseline established]──► ENABLED
    ENABLED ──[HR change > 15 BPM]──────────────────► FAULT
    ENABLED ──[session end]──────────────────────────► IDLE
      FAULT ──[lockout elapsed + app confirm + re-check]──► PRE_SESSION
```

```c
typedef enum {
    NP_CVNS_INTERLOCK_IDLE        = 0,
    NP_CVNS_INTERLOCK_PRE_SESSION = 1,  /* impedance + baseline checks           */
    NP_CVNS_INTERLOCK_ENABLED     = 2,  /* stimulation active; cardiac monitoring */
    NP_CVNS_INTERLOCK_FAULT       = 3,  /* cutoff; lockout active                */
} np_cvns_interlock_state_t;
```

### 4.3 Session workflow stage

```c
typedef enum {
    NP_CVNS_STAGE_IDLE         = 0,
    NP_CVNS_STAGE_IMPEDANCE    = 1,  /* impedance check in progress             */
    NP_CVNS_STAGE_BASELINE     = 2,  /* cardiac baseline accumulation           */
    NP_CVNS_STAGE_RAMP_UP      = 3,  /* current ramping to target              */
    NP_CVNS_STAGE_ACTIVE       = 4,  /* full stimulation                        */
    NP_CVNS_STAGE_RAMP_DOWN    = 5,  /* current ramping to zero                */
    NP_CVNS_STAGE_COMPLETE     = 6,  /* session ended normally                  */
    NP_CVNS_STAGE_FAULT        = 7,  /* cardiac interlock or safety rejection   */
} np_cvns_stage_t;
```

### 4.4 Stimulation phase

```c
typedef enum {
    NP_CVNS_STIM_IDLE      = 0,
    NP_CVNS_STIM_RAMP_UP   = 1,
    NP_CVNS_STIM_ACTIVE    = 2,
    NP_CVNS_STIM_RAMP_DOWN = 3,
    NP_CVNS_STIM_DONE      = 4,
    NP_CVNS_STIM_FAULT     = 5,
} np_cvns_stim_phase_t;
```

### 4.5 UHDR session record

Written to the UHDR partition at session end, AES-256-XTS encrypted with the user's biometric-derived key. NeurOne never holds the decryption key.

```c
typedef struct {
    uint32_t session_start_unix;     /* UTC epoch seconds                       */
    uint32_t session_duration_s;     /* actual stimulation duration              */
    uint16_t stim_freq_hz;           /* programmed frequency                    */
    uint16_t stim_current_ua;        /* programmed current at end of ramp       */
    uint16_t stim_pulse_width_us;    /* pulse width                             */
    uint8_t  electrode_config;       /* 0=bilateral, 1=unilateral left, 2=right */
    uint8_t  cutoff_occurred;        /* 1 if cardiac interlock fired            */
    uint16_t cutoff_hr_baseline_bpm; /* baseline HR at session start (× 10)    */
    uint16_t cutoff_hr_at_event_bpm; /* HR at cutoff moment (× 10)             */
    uint32_t cutoff_time_offset_s;   /* seconds after stim onset (0 if no cut) */
    float    impedance_left_kohm;    /* pre-session impedance                   */
    float    impedance_right_kohm;
    float    mean_impedance_kohm;
    uint8_t  abort_reason;           /* 0=normal, else np_cvns_status_t        */
    uint8_t  reserved[3];
} np_cvns_session_record_t;
```

### 4.6 SHDR session summary

Written to the SHDR partition. Contains device metrics only — no user biology.

```c
typedef struct {
    uint8_t  electrode_config;
    uint16_t stim_freq_hz;
    uint16_t stim_current_ua;
    uint32_t stim_duration_s;
    float    impedance_left_kohm;
    float    impedance_right_kohm;
    uint8_t  impedance_check_pass;   /* 1 if both electrodes passed             */
    uint8_t  cutoff_occurred;        /* 1 if cardiac interlock fired — no HR data*/
    uint8_t  abort_reason;           /* 0=normal                                */
} np_cvns_shdr_summary_t;
```

**UHDR/SHDR boundary:** HR time series, baseline HR, cutoff HR, and all cardiovascular signal data are UHDR (user biology). The bare fact that a cutoff occurred (without the HR values) is SHDR (device safety event log). Per CLAUDE.md §5.1 boundary resolution rule: when in doubt → UHDR.

---

## 5. Safety MCU Bare-Metal Cardiac Interlock (STM32G071)

This section specifies the STM32G071 safety MCU bare-metal behavior. The C implementation is certified separately under IEC 62304 Class C.

### 5.1 Hardware interfaces

> **⚠ UNVERIFIED — do not build to this table.** Every row below disagrees with `firmware/safety_mcu/include/np_safety_config.h`, and there is no schematic in this repository to settle which is intended (`hardware/` holds only `np_socket_map.json` and `np_helmet_surface.json`). The table is retained verbatim as Rev A's claim, not as the requirement. **Tracked as OI-CVNS-08 (pin map) and OI-CVNS-09 (enable-line count); both are BLOCKING on PCB layout (gate G1).** `np_safety_config.h` itself carries the caveat "GPIO bank assignments are provisional pending PCB layout (G1 gate)", so neither side is currently authoritative.

| Signal | Direction | STM32G071 pin (Rev A claim) | In-tree firmware | Description |
|--------|-----------|------------------------------|------------------|-------------|
| `CVNS_ENABLE_L` | Output | PA5 | *no such line* | Active-low cervical VNS enable GPIO — left electrode driver |
| `CVNS_ENABLE_R` | Output | PA6 | *no such line* | Active-low cervical VNS enable GPIO — right electrode driver |
| — | Output | — | **`NP_EN_CVNS` on PB5** | The single active-low CVNS enable the firmware actually owns |
| `RPEAK_IN` | Input | PB0 | **PA8** (`NP_RPEAK_IN_PIN`) | R-peak pulse from main processor (rising edge, 5 ms) |
| `SPI1_SCK/MOSI/MISO/NSS` | SPI slave | PA1–PA4 | **PA5/PA6/PA7 + PA4 NSS** per the header comment at `np_safety_config.h:24` | SPI interface to main processor |
| `HEARTBEAT_WATCHDOG` | Input | PA7 | *no such pin* — the heartbeat is **in-band on SPI** (`NP_SAFETY_WDG_TIMEOUT_MS` = 1500) | SPI heartbeat; 1.5 s timeout → force cutoff |

Note that PB0 in the firmware is `NP_EN_BES_PIN`, the BES stimulation **enable output** — so Rev A's map places a cardiac *input* on a stimulation *output*. That single row is sufficient reason not to treat this table as buildable.

### 5.2 R-peak GPIO protocol

The main processor signals each detected R-peak to the safety MCU via a 5 ms active-high pulse on `RPEAK_IN`. The safety MCU measures the interval between consecutive rising edges using TIM2 (32-bit, 1 MHz tick, period = 1 µs) to compute instantaneous HR.

Debounce: any edge detected within 30 µs of the previous edge is discarded (`NP_CVNS_RPEAK_DEBOUNCE_US`).

### 5.3 Baseline establishment

**Two independent baselines exist; do not conflate them.** The main processor computes its own baseline from PPG-derived R-peaks using the §3.4 constants in `firmware/cervical_vns/include/np_cvns_config.h`. The safety MCU computes its own, from the `RPEAK_IN` pulse train, using its own constants. The two sides are **deliberately separate implementations** — that is precisely what makes the ±5 BPM cross-validation in step 5 (`NP_CVNS_BASELINE_CROSSVAL_BPM`) a meaningful check rather than a computation compared against itself. Rev A of this section attributed the main processor's constants to the safety MCU; the safety MCU has never used them.

Safety MCU constants — the values that actually govern §5.3 and §5.4:

| Constant | Value | Defined in | Meaning |
|----------|-------|-----------|---------|
| `NP_RR_BUF_SIZE` | 8 | `src/np_cardiac_interlock.c` (file-private, not the header) | R-R ring-buffer depth |
| `NP_CARDIAC_BASELINE_BEATS` | 8 | `include/np_safety_config.h` | Intervals required before the interlock arms |
| `NP_CARDIAC_HR_DELTA_BPM` | 15 | `include/np_safety_config.h` | Cutoff threshold (strict `>`) |
| `NP_CARDIAC_OBS_MS` | 5 000 | `include/np_safety_config.h` | Rolling-baseline refresh window |
| `NP_CARDIAC_LOCKOUT_MS` | 30 000 | `include/np_safety_config.h` | Re-enable lockout after a cutoff |

> **OI-CVNS-10 — open.** The safety MCU uses an 8-interval baseline window; the main processor uses 5 (`NP_CVNS_BASELINE_BEATS_MIN`). Whether the two windows are *meant* to differ, and why, is not established anywhere. **Do not harmonise by editing one number to match the other** — see §14.

Before stimulation is enabled:
1. Safety MCU accumulates R-R intervals in a circular buffer of depth `NP_RR_BUF_SIZE` (8). The first R-peak after init or re-enable is a priming edge and produces no interval, so 8 intervals require 9 edges.
2. **Not implemented on the safety MCU.** Rev A specified discarding intervals outside [`NP_CVNS_RR_MIN_VALID_MS`, `NP_CVNS_RR_MAX_VALID_MS`] (300–2000 ms); those are main-processor constants and the safety MCU applies no validity filter — every measured interval enters the buffer. A physiologically impossible interval is handled downstream instead, by saturating the BPM conversion at `INT16_MAX` (see `rr_to_bpm()`). Escalated as **OI-CVNS-11**.
3. The baseline arms when `NP_CARDIAC_BASELINE_BEATS` (8) intervals have accumulated. There is no outlier-rejection criterion on the MCU side.
4. Baseline HR (BPM) = 60,000,000 / mean(all intervals currently in the ring buffer, in µs). Because the buffer is 8 deep and arming requires 8 intervals, at the arming tick this is the mean of the last 8.
5. The main processor confirms baseline via `NP_CVNS_SPI_CMD_HR_BASELINE_SET`. Both baselines must agree within `NP_CVNS_BASELINE_CROSSVAL_BPM` (5 BPM); if not, the safety MCU rejects the enable request.
6. Once armed, the baseline is refreshed to the current rate every `NP_CARDIAC_OBS_MS` (5 s) so that slow physiological drift does not accumulate into a cutoff.

Host-test coverage: `firmware/safety_mcu/tests/np_cardiac_interlock_tests.c` (`np_cardiac_interlock_tests`).

### 5.4 Cardiac interlock monitoring loop

Once stimulation is enabled, the safety MCU runs a 200 Hz interrupt-driven loop (TIM6 5 ms ISR):

**Per ISR execution:**

1. Compute `window_hr_bpm` = 60,000,000 / mean(R-R intervals currently in the ring buffer, in µs).
2. If `|window_hr_bpm − baseline_hr_bpm| > NP_CARDIAC_HR_DELTA_BPM` (15 BPM). The comparison is **signed** — `int16_t`, per FMEA-M05-02 — so a fall below baseline is compared by magnitude rather than underflowing to a large positive value. The threshold is a strict `>`: a delta of exactly 15 BPM holds:
   a. Clear `NP_SAFETY_EN_CVNS` from the granted enable mask, driving the single CVNS enable GPIO (`NP_EN_CVNS`) to its disabled state and stopping stimulation. **Rev A specified asserting `CVNS_ENABLE_L` *and* `CVNS_ENABLE_R`; the firmware has one CVNS enable line, so that instruction was unimplementable as written.** Whether per-electrode cutoff is *required* is open — see OI-CVNS-09 — but nothing in this document should be read as a claim that per-electrode cutoff exists today.
   b. Record the fault: `NP_SAFETY_STATUS_CARDIAC` and `NP_SAFETY_STATUS_CUTOFF` set, `fault_slot` = 10 (CVNS).
   c. Start the `NP_CARDIAC_LOCKOUT_MS` (30 s) re-enable lockout. Re-enable is refused for the whole window; after it expires, re-enable additionally requires explicit app confirmation and a repeat impedance check (§5.5, CLAUDE.md §4.2).
   d. Send FAULT SPI notification to main processor on next SPI transaction.
3. **Conservative hold.** The safety MCU will not fire a cutoff at all until the baseline has armed — that is, until `NP_CARDIAC_BASELINE_BEATS` (8) intervals have accumulated. This is stricter than Rev A's "fewer than 3 valid intervals" rule, which described the *main processor's* data-loss handling (`NP_CVNS_DATA_LOSS_TIMEOUT_S`, §6). The safety MCU has **no** warning flag and **no** 10 s soft-cutoff timer; it holds, silently and unconditionally, until armed. Re-enable invalidates the baseline, so the hold applies again after every cutoff.

**Cutoff latency guarantee:** The GPIO assertion occurs within the TIM6 ISR, with a maximum latency of one 5 ms tick after the condition is first detected. The ISR itself executes in < 100 µs on the STM32G071 at 64 MHz (< 6,400 cycles). **Total worst-case cutoff latency: < 5.1 ms — well within the 100 ms specification (`NP_CVNS_CUTOFF_LATENCY_MAX_MS`).**

### 5.5 SPI command set (safety MCU slave)

| Command | Value | Direction | Description |
|---------|-------|-----------|-------------|
| `IMPEDANCE_CHECK` | 0x10 | Main → MCU | Request AC impedance measurement on both electrodes |
| `ENABLE` | 0x11 | Main → MCU | Request stimulation enable; MCU validates baseline first |
| `DISABLE` | 0x12 | Main → MCU | Immediate disable; no fault logged |
| `HR_BASELINE_SET` | 0x13 | Main → MCU | Push main-processor-computed baseline HR for cross-validation |
| `STATUS_QUERY` | 0x14 | Main → MCU | Request status response |
| `STATUS_RESPONSE` | 0x80 | MCU → Main | Current state, impedance values, fault reason |
| `FAULT_NOTIFY` | 0x81 | MCU → Main | Asynchronous fault notification |
| `IMPEDANCE_RESULT` | 0x82 | MCU → Main | Impedance measurement result (both electrodes) |
| `ENABLE_GRANTED` | 0x83 | MCU → Main | Enable request accepted |
| `ENABLE_REJECTED` | 0x84 | MCU → Main | Enable request denied (with reason byte) |

### 5.6 Safety MCU fault log (SHDR)

Each cutoff event generates a 12-byte fault log entry written to a dedicated SHDR sub-partition:

```c
typedef struct {
    uint32_t session_id;         /* session counter (unsigned, no timestamps) */
    uint32_t cutoff_offset_ms;   /* ms after stim enable when cutoff occurred */
    uint8_t  reason;             /* 0=HR_CHANGE, 1=DATA_LOSS, 2=WATCHDOG      */
    uint8_t  reserved[3];
} np_cvns_fault_log_entry_t;
```

**No HR values in the fault log** — those are in the UHDR session record (user biology, biometric-derived AES key).

---

## 6. Cardiac Interlock Module — Main Processor Side (`np_cvns_interlock.c`)

### 6.1 Responsibilities

The main processor side of the interlock:
1. Processes PPG samples (at 200 Hz) to detect R-peaks.
2. Asserts the `RPEAK_IN` GPIO pulse to the safety MCU on each R-peak.
3. Maintains a local R-R interval buffer and computes baseline HR for cross-validation.
4. Handles SPI exchange with the safety MCU for enable/disable/status.
5. Propagates FAULT notifications from the safety MCU to the session manager.

### 6.2 R-peak detection

Pan-Tompkins-derived bandpass detection on the PPG signal from the VNS accessory PPG sensor (808–830 nm, co-located in the clip mount):

1. Bandpass FIR: 0.5–40 Hz (eliminates motion artefact and high-frequency noise).
2. Differentiate: first difference of filtered signal.
3. Square: element-wise squaring.
4. Moving window integrate: 150 ms window.
5. Adaptive threshold: 75% of running maximum over last 2 s; updated after each confirmed R-peak.
6. Refractory period: 200 ms after each confirmed peak (prevents double-detection).

### 6.3 Baseline HR computation

Identical algorithm to the safety MCU (§5.3), running in parallel on the main processor. The main processor pushes its computed baseline to the safety MCU via `NP_CVNS_SPI_CMD_HR_BASELINE_SET` before requesting enable. The safety MCU cross-validates with its own R-peak GPIO-derived baseline; a > 5 BPM discrepancy blocks enable.

### 6.4 API summary

```c
/* Lifecycle */
np_cvns_status_t np_cvns_interlock_init(np_cvns_interlock_ctx_t *ctx,
                                         np_cvns_interlock_config_t config,
                                         np_cvns_fault_cb_t fault_cb);
void             np_cvns_interlock_deinit(np_cvns_interlock_ctx_t *ctx);

/* PPG feed-in (call from PPG ISR at 200 Hz) */
void np_cvns_interlock_push_ppg(np_cvns_interlock_ctx_t *ctx,
                                  uint32_t sample,
                                  uint32_t timestamp_ms);

/* Session control */
np_cvns_status_t np_cvns_interlock_request_enable(np_cvns_interlock_ctx_t *ctx);
void             np_cvns_interlock_disable(np_cvns_interlock_ctx_t *ctx);
np_cvns_status_t np_cvns_interlock_request_reenable(np_cvns_interlock_ctx_t *ctx);

/* Safety MCU SPI callbacks */
void np_cvns_interlock_spi_response(np_cvns_interlock_ctx_t *ctx,
                                     uint8_t cmd,
                                     const uint8_t *payload,
                                     uint8_t len);

/* State accessors */
np_cvns_interlock_state_t np_cvns_interlock_state(const np_cvns_interlock_ctx_t *ctx);
float                     np_cvns_interlock_baseline_hr(const np_cvns_interlock_ctx_t *ctx);
bool                      np_cvns_interlock_baseline_valid(const np_cvns_interlock_ctx_t *ctx);
uint32_t                  np_cvns_interlock_reenable_lockout_remaining_s(
                                     const np_cvns_interlock_ctx_t *ctx,
                                     uint32_t now_s);
```

---

## 7. Stimulation Delivery (`np_cvns_stim.c`)

### 7.1 Waveform specification

Biphasic charge-balanced stimulation, identical waveform topology to the gammaCore predicate:

| Phase | Duration | Polarity | Charge |
|-------|----------|----------|--------|
| Active phase | `pulse_width_us` | Cathodic first (cervical VNS convention) | q = I × t |
| Inter-phase gap | 100 µs | Zero | 0 |
| Charge-recovery phase | `pulse_width_us` | Anodic | −q |
| Inter-stimulus interval | 1/freq_hz − 2×pulse_width_us − 200µs | Zero | 0 |

Charge balance is hardware-enforced: the driver circuit integrates charge delivery and terminates the recovery phase when cumulative charge returns to zero (within ±1 µC tolerance). The safety MCU independently monitors the integrated charge via a dedicated current-sense ADC channel.

### 7.2 Ramp state machine

```
IDLE → RAMP_UP → ACTIVE → RAMP_DOWN → DONE
              ↓                    ↓
            FAULT               FAULT
```

- **RAMP_UP**: current increases linearly from 0 to `target_current_ua` over `NP_CVNS_RAMP_UP_S` (10 s). Step size: `target_ua / (ramp_s × tick_rate_hz)`.
- **ACTIVE**: steady-state stimulation at `target_current_ua` until session duration elapsed or external stop.
- **RAMP_DOWN**: current decreases linearly over `NP_CVNS_RAMP_DOWN_S` (5 s). Initiated by session timer or by external stop request.
- **FAULT**: entered immediately on cardiac interlock cutoff or safety MCU rejection; current set to zero atomically.

### 7.3 Tick rate

The stim module is driven by a 100 ms tick (`np_cvns_stim_tick(ctx, now_ms)`) from the application scheduler. The safety MCU drives the actual pulse generation; the main processor computes target current levels and sends them via SPI.

### 7.4 API summary

```c
np_cvns_status_t np_cvns_stim_init(np_cvns_stim_ctx_t *ctx,
                                    const np_cvns_stim_config_t *config,
                                    np_cvns_safety_response_cb_t safety_cb);
void             np_cvns_stim_deinit(np_cvns_stim_ctx_t *ctx);

np_cvns_status_t np_cvns_stim_start(np_cvns_stim_ctx_t *ctx,
                                     uint16_t current_ua,
                                     uint16_t duration_s,
                                     uint32_t now_ms);
void             np_cvns_stim_stop(np_cvns_stim_ctx_t *ctx);
void             np_cvns_stim_tick(np_cvns_stim_ctx_t *ctx, uint32_t now_ms);

/* Safety MCU callback (invoked from SPI ISR) */
void np_cvns_stim_safety_mcu_response(np_cvns_stim_ctx_t *ctx,
                                       bool granted,
                                       const float impedance_kohm[NP_CVNS_ELECTRODE_COUNT]);

np_cvns_stim_phase_t np_cvns_stim_phase(const np_cvns_stim_ctx_t *ctx);
uint16_t             np_cvns_stim_current_ua(const np_cvns_stim_ctx_t *ctx);
```

---

## 8. Session Management (`np_cvns_session.c`)

### 8.1 Session workflow

```
np_cvns_session_start()
    │
    ▼
STAGE_IMPEDANCE  ── safety MCU measures both electrode impedances
    │                both ≤ 5 kΩ ?
    │ yes
    ▼
STAGE_BASELINE   ── accumulate ≥ 5 valid R-R intervals
    │                cross-validate main processor vs safety MCU baseline
    │ agree within 5 BPM?
    │ yes → safety MCU confirms enable request
    ▼
STAGE_RAMP_UP    ── 10-second current ramp
    ▼
STAGE_ACTIVE     ── steady stimulation; cardiac monitoring at 200 Hz
    │
    │ cardiac interlock fires?        session timer expires?
    ▼                                 ▼
STAGE_FAULT                       STAGE_RAMP_DOWN → STAGE_COMPLETE
```

### 8.2 UHDR / SHDR data routing

| Data element | Partition | Notes |
|---|---|---|
| Full R-R interval time series during session | UHDR | User biology |
| Baseline HR (BPM) | UHDR | User biology |
| Instantaneous HR at cutoff | UHDR | User biology |
| Time-of-cutoff offset (s after stim onset) | UHDR | User biology |
| Impedance (both electrodes, pre-session) | UHDR | Raw measurement = user biology |
| Session start timestamp | UHDR | User biology per CLAUDE.md §5.1 |
| Session duration (s) | UHDR | User biology |
| Stimulation parameters (freq, current, pulse width) | UHDR | Linked to a specific user session |
| Cutoff occurred flag (0/1 only, no HR values) | SHDR | Device safety event — no user biology |
| Electrode impedance pass/fail (boolean) | SHDR | Device contact quality metric |
| Safety MCU fault log (offset, reason, no HR) | SHDR | Device event log |
| Session count increment | SHDR | Unsigned integer |

### 8.3 Session configuration structure

```c
typedef struct {
    uint16_t freq_hz;              /* 1–25 Hz                                   */
    uint16_t current_ua;           /* 0–2000 µA                                 */
    uint16_t pulse_width_us;       /* 200–1000 µs                               */
    uint16_t duration_s;           /* 60–120 s                                  */
    uint8_t  electrode_config;     /* 0=bilateral, 1=unilateral_L, 2=unilateral_R */
} np_cvns_session_config_t;
```

Sessions are delivered via cryptographically signed protocol from the app (same CSPRNG signing mechanism as all other NeurOne modalities).

### 8.4 API summary

```c
np_cvns_status_t np_cvns_session_init(np_cvns_session_ctx_t   *ctx,
                                       np_cvns_interlock_ctx_t *interlock,
                                       np_cvns_stim_ctx_t      *stim,
                                       np_cvns_session_end_cb_t end_cb);

np_cvns_status_t np_cvns_session_start(np_cvns_session_ctx_t       *ctx,
                                        const np_cvns_session_config_t *config,
                                        uint32_t                     now_ms);

void np_cvns_session_stop(np_cvns_session_ctx_t *ctx);
void np_cvns_session_tick(np_cvns_session_ctx_t *ctx, uint32_t now_ms);

np_cvns_stage_t np_cvns_session_stage(const np_cvns_session_ctx_t *ctx);
```

---

## 9. FAI Test Specification

### FAI-CV01 — Cervical electrode placement verification

**Category:** Hardware bench (cannot be software-verified in CI).

**Setup:**
- Cervical VNS accessory gel electrodes applied to anatomical neck phantom (silicone gel head-neck phantom with embedded vasculature model, 0.25 S/m tissue equivalent).
- NeurOne hub accessory port connected to cervical VNS cable assembly.
- Safety MCU impedance measurement activated.

**Procedure:**
1. Apply both bilateral gel electrodes per IFU (skin overlying left and right carotid sheath, 2 cm inferior to angle of jaw).
2. Apply moderate pressure (equivalent to 2N) and measure impedance at 1 kHz AC.
3. Verify both electrodes register ≤ 5 kΩ.
4. Remove left electrode; verify system detects single-electrode failure (impedance > limit or open circuit).
5. Repeat for right electrode.
6. Apply electrodes with deliberate misplacement (5 cm superior to correct position); verify that impedance > 5 kΩ flags the placement as invalid.

**Pass criteria:**
- CV01-A: Both electrodes correctly placed → impedance ≤ 5 kΩ on both channels.
- CV01-B: Single electrode removal → system reports high-impedance fault on removed channel within 500 ms.
- CV01-C: Deliberate misplacement → impedance > 5 kΩ on at least one channel; system blocks enable.
- CV01-D: Safety MCU reports impedance values to main processor within 200 ms of measurement completion.

**Result:** PENDING (hardware bench required).

---

### FAI-CV02 — Cardiac interlock response time

**Category:** Bench test (software plumbing check executable in CI; timing verification requires hardware).

**Setup:**
- R-peak signal generator injecting R-peak GPIO pulses to safety MCU at programmable HR.
- Oscilloscope monitoring `CVNS_ENABLE_L` GPIO.
- NeurOne T2 hub with safety MCU flashed; cervical VNS accessory connected.

**Procedure:**
1. Establish baseline HR: inject R-peak pulses at 70 BPM (R-R = 857 ms) for 10 seconds.
2. Enable stimulation: request enable via main processor SPI; verify safety MCU grants.
3. HR step event: abruptly change R-peak injection rate to 90 BPM (R-R = 667 ms) — a +20 BPM change exceeding the 15 BPM limit.
4. Measure time from first out-of-window R-peak edge to `CVNS_ENABLE_L` falling edge on oscilloscope.
5. Repeat 10 times; record each cutoff latency.

**Pass criteria:**
- CV02-A: All 10 measured cutoff latencies ≤ 100 ms (`NP_CVNS_CUTOFF_LATENCY_MAX_MS`).
- CV02-B: Safety MCU sends FAULT_NOTIFY SPI message within 200 ms of cutoff GPIO event.
- CV02-C: Main processor `np_cvns_interlock_state()` returns `NP_CVNS_INTERLOCK_FAULT` within 300 ms of GPIO event (one SPI heartbeat interval).
- CV02-D: Re-enable is blocked until `NP_CVNS_REENABLE_LOCKOUT_S` (30 s) has elapsed.
- CV02-E: A HR change of exactly 15 BPM does NOT trigger cutoff (boundary condition); >15 BPM does.

**Software-verifiable component (CI):** The state machine boundary conditions (CV02-D, CV02-E) and constant values (`NP_CVNS_CUTOFF_LATENCY_MAX_MS`, `NP_CVNS_REENABLE_LOCKOUT_S`, `NP_CVNS_HR_CHANGE_LIMIT_BPM`) are verified by the FAI test binary.

**Result:** CV02-D, CV02-E SOFTWARE PASS (verified in `np_cvns_fai_tests.c`). CV02-A, CV02-B, CV02-C PENDING (hardware bench required).

---

### FAI-CV03 — Tolerability in 3 healthy adults

**Category:** Clinical (IRB approval required before execution).

**Setup:**
- 3 healthy adult volunteers (screened; exclusion: cardiac arrhythmia, carotid stenosis, implanted stimulator, pregnancy).
- NeurOne T2 system with cervical VNS accessory.
- Attending clinician and emergency equipment present.
- ECG monitoring throughout (12-lead or 3-lead continuous).

**Procedure:**
1. Apply bilateral gel electrodes per IFU. Confirm impedance CV01-A criterion met.
2. Initiate 60-second session at minimum parameters (1 Hz, 500 µA, 300 µs pulse width).
3. Record: sensation reports (0–10 numeric rating scale), HR (via ECG), blood pressure (5-min intervals), adverse events.
4. Confirm no cardiac interlock triggering at minimum parameters.
5. Optionally escalate to mid-range parameters (15 Hz, 1000 µA) per clinician discretion.

**Pass criteria:**
- CV03-A: No serious adverse events (no cardiac arrhythmia, syncope, severe carotid discomfort, or oxygen desaturation) in any of 3 participants.
- CV03-B: Maximum reported sensation ≤ 7/10 at minimum protocol parameters.
- CV03-C: Cardiac interlock does not trigger spuriously at minimum parameters (confirming correct baseline and threshold calibration).
- CV03-D: All 3 participants complete minimum protocol without requesting early termination.

**Result:** PENDING (IRB approval and T2 prototype required; not blocking for G3-08 software gate; blocking for T2 clinical release).

---

## 10. Module File Inventory

| File | Contents |
|------|---------|
| `include/np_cvns_config.h` | All configuration constants |
| `include/np_cvns_types.h` | All shared type definitions |
| `include/np_cvns_interlock.h` | Cardiac interlock API |
| `include/np_cvns_stim.h` | Stimulation delivery API |
| `include/np_cvns_session.h` | Session management API |
| `src/np_cvns_interlock.c` | R-peak detection, baseline, SPI exchange, state machine |
| `src/np_cvns_stim.c` | Biphasic waveform, ramp state machine |
| `src/np_cvns_session.c` | Session orchestration, UHDR/SHDR record |
| `tests/np_cvns_fai_tests.c` | FAI-CV01 procedure, FAI-CV02 constants + state machine, FAI-CV03 procedure |
| `CMakeLists.txt` | Static library build |

**Safety MCU side (SW-01 Class C, `firmware/safety_mcu/` — specified in §5, not §6–§8):**

| File | Contents |
|------|---------|
| `include/np_safety_config.h` | Safety MCU GPIO map + cardiac interlock constants (see OI-CVNS-08) |
| `src/np_cardiac_interlock.c` | SW01-M05: R-R capture, baseline arming, ±15 BPM cutoff, 30 s lockout |
| `tests/np_cardiac_interlock_tests.c` | Host tests for SW01-M05 — arming, cutoff both directions, FMEA-M05-02 signed-delta guard, lockout, conservative hold |

> Note: `np_cvns_fai_tests` is registered as "NP-FAI-CVNS-001: cardiac interlock FAI suite" but exercises the **main-processor** module in this section's table and asserts on main-processor constants. It is not coverage of the Class C interlock; `np_cardiac_interlock_tests` is.

---

## 11. UHDR / SHDR Data Routing Summary

Consistent with NP-FW-EMMC-001 Rev A §12 (27-element classification table). Key additions from the cervical VNS module:

| Data element | Partition | Reasoning |
|---|---|---|
| R-R interval time series | UHDR | Directly reveals cardiac rhythm — unambiguously user biology |
| Baseline HR (BPM) | UHDR | Derived from R-R series; identifies resting cardiac state |
| HR at cutoff event | UHDR | User cardiac response data |
| Session timestamps | UHDR | Per CLAUDE.md §5.2 resolution: session timestamps → UHDR |
| Electrode impedance (raw) | UHDR | Raw measurement linked to a user session |
| Cutoff flag (boolean only) | SHDR | Device safety event; no user biology in the flag itself |
| Electrode impedance pass/fail | SHDR | Device contact quality (aggregate) |
| Safety MCU fault log | SHDR | Device event log; no HR values |
| Session count increment | SHDR | Unsigned integer; no timestamps |

---

## 12. Gate Closure: NP-COORD-001 G3-08

This document and its accompanying firmware (`firmware/cervical_vns/`) satisfy the G3-08 gate requirement:

- [x] Safety MCU cardiac interlock fully specified (§5)
- [x] Cutoff latency requirement confirmed: < 5.1 ms worst-case (§5.4), spec 100 ms
- [x] FAI-CV01 procedure specified (§9, hardware bench pending)
- [x] FAI-CV02 software constants verified, hardware bench pending (§9)
- [x] FAI-CV03 tolerability procedure specified (§9, clinical pending)
- [x] UHDR/SHDR data routing consistent with NP-FW-EMMC-001 Rev A §12
- [x] Re-enable policy specified (§3.5): lockout + app confirm + re-check
- [x] RISK-25 documented in risk register (NP-FW-CVNS-001 Rev A §13)
- [x] 510(k) Q-Sub substantial equivalence argument baselined (NP-REG-CVNS-001 Rev A)

**G3-08 SOFTWARE BASELINED — 2026-05-11**
Hardware FAI (CV01 bench, CV02 timing, CV03 clinical) PENDING — blocking for T2 clinical release, not for software gate.

---

## 13. Risk Register Entry — RISK-25

| Field | Value |
|-------|-------|
| Risk ID | RISK-25 |
| Title | Cardiac reflex during cervical VNS — inadequate interlock response time |
| Hazard | Stimulation near carotid sheath activates baroreceptor reflex → uncontrolled HR drop or arrhythmia |
| Severity | Critical (S4: could result in serious injury) |
| Probability (unmitigated) | P3 (possible; documented in gammaCore predicate safety data) |
| Risk (unmitigated) | High |
| Mitigation | Safety MCU TIM6 ISR fires every 5 ms; cardiac interlock GPIO cutoff < 5.1 ms from detection trigger. Baseline cross-validation blocks enable if main processor and safety MCU disagree. 30 s re-enable lockout. Re-enable requires explicit app confirmation. gammaCore predicate demonstrated equivalent interlock concept safe in K163334/K173323. |
| Residual probability | P1 (unlikely; hardware interlock is independent of software stack) |
| Residual risk | Low |
| Verification | FAI-CV02: measured cutoff latency ≤ 100 ms (10 consecutive trials) |
| Status | MITIGATED — pending FAI-CV02 hardware bench confirmation |

---

## 14. Open Items

| ID | Description | Owner | Blocking |
|----|-------------|-------|---------|
| OI-CVNS-01 | Platform HAL stubs (`np_platform_cvns_rpeak_gpio_pulse`, `np_platform_cvns_spi_transfer`) must be implemented before integration testing | FW team | Integration test |
| OI-CVNS-02 | Safety MCU bare-metal firmware (STM32G071, IEC 62304 Class C) must be authored and certified separately | Embedded safety team | T2 clinical release |
| OI-CVNS-03 | UHDR session record commit to eMMC must call AES-256-XTS write with biometric-derived key | FW/Storage team | UHDR compliance |
| OI-CVNS-04 | SHDR session summary write must call SHDR storage API after each session | FW team | SHDR compliance |
| OI-CVNS-05 | IRB approval required before FAI-CV03 tolerability study can be executed | Regulatory/Clinical team | T2 clinical release |
| OI-CVNS-06 | FAI-CV02 hardware bench (R-peak signal generator, oscilloscope) must be completed on T2 prototype | HW team | Full G3-08 closure |
| OI-CVNS-07 | FAI-CV01 anatomical phantom bench must be completed with T2 accessory prototype | HW team | Full G3-08 closure |
| OI-CVNS-08 | STM32G071 pin map: §5.1 and `np_safety_config.h` disagree on every row. Nothing in-tree settles it. | HW/Embedded safety team | PCB layout (G1) |
| OI-CVNS-09 | One CVNS enable line or two (per-electrode)? Clinical/regulatory question, not a code-style one. | Regulatory/Clinical + Embedded safety | PCB layout (G1); T2 510(k) |
| OI-CVNS-10 | Cardiac baseline window: safety MCU 8 intervals vs main processor 5. Deliberate or accidental? | Embedded safety team | Class C design freeze |
| OI-CVNS-11 | Safety MCU applies no R-R validity filter (§5.3 step 2). Intended, or a gap? | Embedded safety team | Class C design freeze |

### 14.1 OI-CVNS-08 — STM32G071 pin map

Rev A §5.1 and `firmware/safety_mcu/include/np_safety_config.h` disagree on **every** signal: `RPEAK_IN` (PB0 vs PA8), SPI1 (PA1–PA4 vs the header's own PA4–PA7 comment), `HEARTBEAT_WATCHDOG` (PA7 vs no such pin — the heartbeat is in-band on SPI), and the CVNS enable itself (PA5/PA6 vs PB5).

This is not a typo. Rev A puts the cardiac *input* `RPEAK_IN` on PB0, which the header assigns to `NP_EN_BES_PIN`, a stimulation *output*. That single row means Rev A's map cannot be adopted wholesale, whatever is decided about the rest.

**Updated 2026-08-05 after NP-HW-HUB-001 §7.2 landed (PR #247).** The PA-bank picture changed materially and *in Rev A's favour*. The five per-zone PBM enables were retired for one `NP_EN_PBM_CRANIAL` on PA0, **releasing PA1–PA3 and resolving the PA4 double-assignment** (PA4 was claimed by both `NP_EN_PBM_ZONE4_PIN` and the SPI1 NSS comment; it is now SPI1 NSS only — see the header's own note and NP-HW-HEXTILE-001 OI-HEXTILE-13(c)). Consequences for this open item:

- Rev A's "SPI1 on PA1–PA4" is **no longer blocked** — PA1–PA3 are free and PA4 is already NSS. The remaining disagreement is only *which* of the freed pins carry SCK/MOSI/MISO, not whether they exist.
- Rev A's `CVNS_ENABLE_L`/`_R` on PA5/PA6 and `HEARTBEAT_WATCHDOG` on PA7 are consistent with that placement — the whole PA-bank half of Rev A's map is now internally coherent as a board, which it was not before.
- **`RPEAK_IN` on PB0 remains the one row that cannot work in either reading**, since PB0 is still `NP_EN_BES_PIN`.

So the earlier framing — "one of the two documents a board that cannot exist" — was true of Rev A when written and is now too strong. The disagreement has narrowed to one impossible row plus a genuine bank-allocation choice.

| Candidate | Evidence for | Evidence against |
|-----------|--------------|------------------|
| **A — the code is right; §5.1 is stale.** Correct §5.1 to PA8 / PB5 / in-band SPI heartbeat. | The code is self-consistent for the cardiac path, is internally consistent on the PA bank since §7.2 resolved the PA4 double-assignment, and is what the Class C unit and its new host tests actually exercise. `np_safety_config.h` is the file an implementer reads. | The header states of itself that "GPIO bank assignments are provisional pending PCB layout (G1 gate)" — it does not claim to be authoritative, so "the code is right" is an assumption, not a finding. |
| **B — §5.1 is the intended board; the code's provisional map is stale.** Re-map the firmware at G1. | `np_safety_config.h` states in its own header comment: "GPIO bank assignments are provisional pending PCB layout (G1 gate)" — the code does not claim authority. | §5.1's `RPEAK_IN`-on-a-stimulation-output row means Rev A's map cannot be adopted wholesale regardless. |

**Why the zone enables were never good evidence.** `NP_EN_PBM_ZONE0..4` was stale before it was retired: zones are not a fixed set of five hardware slots — a zone is a named set of sockets on the hex lattice, and `protocols/predefined/00-zones.npps` states in its own header that it "is the only definition of a zone" (see also CLAUDE.md §3 and `hardware/np_socket_map.json`). Anyone re-opening this item should not resurrect "SPI1 collides with the PBM zone enables" as an argument; that collision is gone.

**What would settle it:** the STM32G071 schematic / PCB netlist from the G1 layout package — the single artefact that makes one of these maps a fact. Until it exists, neither is a requirement. The former sequencing dependency on `NP_SAFETY_EN_PBM_CRANIAL` (OI-HUB-C07) is **discharged** — that landed in NP-HW-HUB-001 §7.2 — so this item is now blocked on G1 alone.

**Do not** edit `np_safety_config.h` to match §5.1, or §5.1 to match `np_safety_config.h`, before that package exists. The programme is pre-tooling with no hardware committed (CLAUDE.md), so changing the code is cheap today — but cheap is not the same as decided, and this is the enable path of a Class C cardiac interlock.

### 14.2 OI-CVNS-09 — single vs dual CVNS enable line

Rev A §5.1 specified `CVNS_ENABLE_L` and `CVNS_ENABLE_R`; §5.4.2a instructed asserting both. The firmware has a single `NP_EN_CVNS`. §5.4.2a has been corrected to describe the single-line cutoff the firmware performs — **that correction is descriptive, not a ruling that one line is sufficient.**

| Candidate | Evidence for | Evidence against |
|-----------|--------------|------------------|
| **A — one enable line is sufficient.** The hazard (RISK-25) is a baroreceptor reflex from stimulation near the carotid sheath; cutting all cervical stimulation addresses it. | Simplest thing that mitigates the stated hazard; one line, one failure mode, less Class C surface. `NP_CVNS_ELECTRODE_COUNT` = 2 describes a bilateral *assembly*, not two independent current paths. | Loses the ability to drop one electrode on a unilateral fault (e.g. the per-electrode impedance divergence already tracked as OI-CVNS-HUB-11 has no per-electrode remedy). |
| **B — per-electrode enables are required.** Restore two lines. | §5.2/§7 contemplate bilateral or unilateral montages; a unilateral electrode fault currently has only an all-or-nothing response. | No hazard in RISK-25 is currently shown to need per-electrode granularity; adding a second Class C enable line costs certification surface for an unquantified benefit. |

**What would settle it:** a hazard-analysis pass against the gammaCore 510(k) predicate (K163334 cluster headache, K173323 migraine) asking specifically whether the predicate's cleared interlock acts per-electrode or on the whole cervical channel, plus an FMEA line for "one electrode faults, the other continues". This is a clinical/regulatory determination and belongs with Regulatory/Clinical, not with firmware. Feed the result into RISK-25 before the T2 510(k) submission.

### 14.3 OI-CVNS-10 — cardiac baseline window, 8 vs 5

The safety MCU arms on 8 intervals (`NP_CARDIAC_BASELINE_BEATS`, ring buffer `NP_RR_BUF_SIZE` = 8); the main processor uses 5 (`NP_CVNS_BASELINE_BEATS_MIN`, buffer 20). Rev A documented the main processor's 5 as if it were the MCU's — so **the divergence was invisible until now, and no record exists of anyone choosing it.**

| Candidate | Evidence for | Evidence against |
|-----------|--------------|------------------|
| **A — the difference is deliberate.** The two sides are independent implementations that cross-validate at ±5 BPM; identical windows would make them correlated and weaken the check. Slower arming on the Class C side is the conservative direction. | The independence rationale is real and is now stated in §5.3. A longer window is more noise-tolerant, which matters more on the side with no validity filter (OI-CVNS-11). | Nothing in the DHF, this document, or the code comments says it was chosen. Nobody has written down the intent. |
| **B — the difference is accidental.** One side was written first and the other drifted. | The MCU's 8 equals its ring-buffer depth exactly, which reads more like "use the whole buffer" than like a tuned clinical parameter. | Even if accidental, 8 may still be the right value — accidental origin is not an argument for changing it. |

**What would settle it:** a Class C design rationale entry stating the intended arming latency for each side and the noise assumptions behind it, reviewed against the ±5 BPM cross-validation tolerance (a longer MCU window means the two baselines are computed over different physiological spans, which itself affects how often the ±5 BPM check trips spuriously — that interaction is currently unanalysed).

**Do NOT harmonise by editing one number to match the other.** This is a safety parameter on a cardiac interlock; making the numbers equal is a change to interlock timing dressed as a tidy-up. `np_cardiac_interlock_tests.c` now pins the MCU's 8-interval arm point with assertions that a 5-interval implementation fails, so any change surfaces as a reviewed diff.

### 14.4 OI-CVNS-11 — no R-R validity filter on the safety MCU

Rev A §5.3 step 2 specified discarding intervals outside 300–2000 ms. Those bounds (`NP_CVNS_RR_MIN_VALID_MS` / `_MAX_VALID_MS`) are main-processor constants; the safety MCU has no equivalent and admits every measured interval into its ring buffer.

| Candidate | Evidence for | Evidence against |
|-----------|--------------|------------------|
| **A — intended; filtering belongs upstream.** The main processor detects R-peaks (Pan-Tompkins, with debounce and refractory period) and only pulses `RPEAK_IN` for accepted beats, so the MCU sees pre-filtered edges. | Keeps the Class C unit minimal — the stated design goal (~500 lines bare-metal, CLAUDE.md §4.2). The `rr_to_bpm()` saturation clamp exists precisely to survive artefacts, and its comment names "noise/motion artifacts on the RPEAK_IN line" as the expected source. | If the MCU trusts the main processor to filter, the two sides are no longer independent for artefact rejection — which is the property §5.3's cross-validation depends on. |
| **B — a gap.** A stuck-high or noisy `RPEAK_IN` line injects garbage intervals straight into the baseline. | The saturation clamp is a *containment* measure, not rejection: a burst of impossible intervals still shifts the mean and can move the baseline or trip a cutoff. | Adding a filter adds Class C code and a new way to reject real beats (a false negative on a cardiac interlock is worse than a false positive). |

**What would settle it:** an FMEA line for `RPEAK_IN` line faults (stuck high, stuck low, ringing) tracing what each does to the baseline and to cutoff behaviour. If the answer is "the main processor's Pan-Tompkins stage is the mitigation", that mitigation needs to be stated as a requirement on the SW-02 side rather than left implicit — at which point the ±5 BPM cross-validation's independence assumption should be re-examined.
