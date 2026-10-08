# HRV Biofeedback Protocol Firmware Specification

**Project:** NeurOne
**Document:** NP-FW-HRV-001
**Revision:** 3
**Date:** 2026-09-27
**Status:** BASELINED
**Effective Date:** 2026-05-11
**Author:** Steve Hickman (CEO, interim Quality authority)
**Approved By:** Steve Hickman, CEO
**References:** CLAUDE.md §3 modality 6 (VNS + HRV + HRV Biofeedback)
**Related Issues:** GitHub Issue #21
**Gate:** NP-COORD-001 G2-12
**IEC 62304 Class:** SW-02 Class B (main processor)
**Supersedes:** —
**Parent Document:** NP-SW-001
**Change Summary:** Rev 3 (2026-09-27) — **OI-HRV-03 CLOSED: the session record reaches UHDR.** The hub's `end_cb` (`np_mod_vns.c`) discarded the finalised `np_hrv_session_record_t`, so no HRV session summary was ever stored. It now commits it through the hub session logger, `np_log_hrv_session()` (new UHDR tag `0x1B`, `NP-FW-HUB-001` Rev 19 §6.1). The logger appends to this session's file on the UHDR partition, which `np_uhdr_key_unlock()` mounts AES-256-XTS under the user's biometric-derived key. That is the storage layer's AES-256-XTS write, and the logger never sees key material. §8.1 now states the serialized layout (31 bytes, field by field) and that `session_start_unix` is not written. `np_log_backend_tests` pins the layout and the routing: UHDR only, ahead of `SESSION_END`, and never in another session's file. **Two gaps found while closing it, raised and not fixed:** nothing feeds PPG samples to the HRV session or ticks it, so on today's firmware the committed record carries a duration and a protocol and zero statistics (**OI-HRV-07**). The §8.1 R-R interval series has no writer (**OI-HRV-06**). — Rev 2 (2026-09-26, GitHub #447) — **Two taVNS defects found by the library's first host test (`np_hrv_session_tests`); the §10 gate closure's "taVNS inspiration-phase timing fully specified and implemented" was not true of the code.** (1) **§5.1 as implemented never opened the gate.** `np_hrv_tavns_process_rr()` read the "previous R-R interval" out of the slope buffer, which holds dRR values, so dRR was 0 for the whole session and the mean slope never crossed −5 ms/beat. The state now carries `last_rr_ms`. (2) **OI-HRV-05, firmware half:** an out-of-range `tavns_freq_hz` or current was refused by `np_hrv_tavns_init()` *before* it reset its module-static callback, rate and current, and `np_hrv_session_start()` ignored the refusal and started the session anyway. With (1) fixed, the next inspiration would have fired the previous session's enable at the previous session's rate. A refused configuration now disarms everything, and the session refuses to start (§5.2 item 4). Neither defect reached a stimulation line: the enable callbacks are still stubs (OI-HRV-01), and the safety MCU owns every enable. Rev 1 (2026-05-11): initial release.

---

## 1. Scope

This document specifies the firmware implementation of the four HRV biofeedback protocols listed in CLAUDE.md §3 modality 6. All four protocols are software-only additions to the existing VNS clip hardware (PPG sensor + auricular taVNS electrodes). No new BOM additions are required.

Protocols covered:

| ID | Name | Hardware used |
|----|------|---------------|
| 0 | Standalone coherence training | PPG clip (HRV only) |
| 1 | HRV + taVNS synchronised | PPG clip + VNS electrodes |
| 2 | HRV + EEG dual biofeedback | PPG clip + ADS1299 (8-ch EEG) |
| 3 | HRV + PBM | PPG clip + PBM zone modules |

---

## 2. Architecture Overview

```
PPG ISR (200 Hz)
    │
    ▼
np_hrv_ppg_process_sample()
    │  new R-peak → np_hrv_rr_push()
    ▼
np_rr_buffer_t  ─────────────────────────────────┐
    │                                             │
    │ (every 5 s)                                 │ RMSSD
    ▼                                             ▼
np_hrv_coherence_update()              np_hrv_ppg_rmssd()
    │                                             │
    ▼                                             │
np_hrv_psd_t (Welch PSD)               ──────────┘
    │                                             │
    ▼                                             │
np_hrv_coherence_compute() ──────────────────────┘
    │
    ▼
np_hrv_coherence_result_t
    ├── coherence_score (0–10)
    ├── lf_peak_power
    ├── hf_total_power
    └── rmssd_ms

EEG DMA (500 Hz, protocol 2 only)
    │
    ▼
np_hrv_eeg_push_sample()
    │ (every 30 s)
    ▼
np_hrv_eeg_compute_bands()
    │
    ▼
np_eeg_bands_t
    └── alpha_theta_ratio (frontal F3/F4)
              │
              ▼ np_hrv_eeg_adaptive_step()
              │
              ▼ np_hrv_pacer_set_rate()

Safety MCU SPI (protocol 1 only)
    ▲
    │ np_hrv_tavns_safety_mcu_response()
    │
np_hrv_tavns_process_rr()
    │ RSA slope detection
    ▼
np_tavns_enable_cb_t → safety MCU enable request
```

---

## 3. Configuration Constants (np_hrv_config.h)

### 3.1 Signal acquisition

| Constant | Value | Description |
|----------|-------|-------------|
| `NP_PPG_SAMPLE_RATE_HZ` | 200 | PPG ADC rate (Hz) |
| `NP_EEG_SAMPLE_RATE_HZ` | 500 | ADS1299 rate (Hz) |
| `NP_EEG_CHANNELS` | 8 | Fp1/2, F3/4, C3/4, P3/4 |

### 3.2 HRV spectral analysis

| Constant | Value | Description |
|----------|-------|-------------|
| `NP_RR_INTERP_RATE_HZ` | 4 | Uniform interpolation grid (Hz) |
| `NP_HRV_FFT_SIZE` | 256 | Welch window length (samples) |
| `NP_HRV_FREQ_RESOLUTION` | 0.015625 Hz | 4 Hz / 256 |
| `NP_HRV_LF_BIN_LO` | 3 | 0.046875 Hz |
| `NP_HRV_LF_BIN_HI` | 9 | 0.140625 Hz |
| `NP_HRV_HF_BIN_LO` | 10 | 0.156250 Hz |
| `NP_HRV_HF_BIN_HI` | 25 | 0.390625 Hz |
| `NP_HRV_MIN_RR_FOR_COHERENCE` | 64 | Intervals before PSD is valid |

### 3.3 EEG band power

| Constant | Value | Description |
|----------|-------|-------------|
| `NP_EEG_FFT_SIZE` | 1024 | Welch window length (2.048 s at 500 Hz) |
| `NP_EEG_FREQ_RESOLUTION` | 0.4883 Hz | 500 Hz / 1024 |
| Theta bins | 9–16 | 4.394–7.813 Hz |
| Alpha bins | 17–26 | 8.301–12.695 Hz |

### 3.4 Breathing pacer

| Constant | Value | Description |
|----------|-------|-------------|
| `NP_PACER_RATE_DEFAULT_BPM` | 6.0 | 0.1 Hz resonance default |
| `NP_PACER_RATE_MIN_BPM` | 4.0 | Lower sweep bound |
| `NP_PACER_RATE_MAX_BPM` | 7.0 | Upper sweep bound |
| `NP_PACER_SWEEP_STEP_BPM` | 0.5 | RF sweep step size |
| `NP_PACER_SWEEP_DWELL_S` | 120 | Seconds at each sweep rate |
| `NP_PACER_INSP_RATIO` | 0.4 | Inspiration fraction of cycle |

### 3.5 taVNS

| Constant | Value | Description |
|----------|-------|-------------|
| `NP_TAVNS_DEFAULT_FREQ_HZ` | 25 | Carrier frequency |
| `NP_TAVNS_DEFAULT_CURRENT_UA` | 500 | 0.5 mA default |
| `NP_VNS_MAX_CURRENT_UA` | 40000 | 40 mA stop-gap ceiling (Rev 68), derived from the per-phase charge limit; see `np_shared_constants.h` |
| `NP_TAVNS_SLOPE_HYSTERESIS` | 5 ms/beat | Deadband for phase detection |
| `NP_TAVNS_MAX_INSP_DURATION_MS` | 6000 | Failsafe gate-open limit |

---

## 4. Coherence Algorithm (np_hrv_coherence.c)

### 4.1 Definition

```
Coherence = LF_peak_power / (LF_total_power + HF_total_power) × 10
```

- **LF_peak_power**: maximum PSD value (ms²/Hz) within LF band (bins 3–9)
- **LF_total_power**: integral of PSD over LF band (bins 3–9) × frequency resolution
- **HF_total_power**: integral of PSD over HF band (bins 10–25) × frequency resolution
- Result clipped to [0, 10]

### 4.2 Computation pipeline

1. **R-R interpolation**: linear interpolation of the `np_rr_buffer_t` to a uniform 4 Hz grid using the stored R-peak timestamps. The interpolation spans the most recent 256 uniform-grid samples (64 seconds of data).
2. **Detrend**: subtract the mean of the 256-sample window.
3. **Hann window**: multiply element-wise by pre-computed Hann coefficients (computed at `np_hrv_coherence_init()`).
4. **Radix-2 DIT FFT**: in-place 256-point float32 FFT (Cooley-Tukey, self-contained — no external DSP library dependency).
5. **One-sided PSD**: `PSD[k] = 2 |X[k]|² / (N × Fs)` for k ∈ [1, N/2-1]; no doubling for k=0, k=N/2.
6. **Welch averaging**: running mean accumulated into `np_hrv_psd_t.psd[]`; `num_averages` tracks the count.
7. **LF/HF band integration** and coherence score extraction.

### 4.3 Validity gate

A coherence result is only returned if:
- `psd.valid == true` (at least one FFT average has been computed)
- `rr_buf.count >= NP_HRV_MIN_RR_FOR_COHERENCE` (64 intervals ≈ 64 s at 60 BPM)

### 4.4 RMSSD

Computed independently by `np_hrv_ppg_rmssd()` over the most recent `NP_RR_RMSSD_WINDOW` (20) successive R-R differences:

```
RMSSD = sqrt( mean( (RR[i+1] - RR[i])² ) )
```

---

## 5. taVNS Inspiration-Phase Synchronisation (np_hrv_tavns_sync.c)

### 5.1 RSA slope detection

The respiratory signal is extracted from the R-R interval series using the Respiratory Sinus Arrhythmia (RSA) mechanism:

- **Inspiration**: sympathetic withdrawal → HR increases → RR interval shortens → `dRR/dt < 0`
- **Expiration**: parasympathetic rebound → HR decreases → RR interval lengthens → `dRR/dt > 0`

A sliding buffer of `NP_TAVNS_INSP_SLOPE_WIN` (6) consecutive R-R differences is maintained. Each difference is taken against the previous R-R **interval** (`last_rr_ms`), and the first interval of a session contributes 0. *(Rev 2: the implementation took it against the previous buffer entry, a difference rather than an interval, so the slope stayed at 0 and the gate never opened.)* The mean of this buffer gives `slope` (ms/beat):

| Condition | Phase detected |
|-----------|---------------|
| slope < −5 ms/beat | Inspiration onset → open taVNS gate |
| slope > +5 ms/beat | Expiration onset → close taVNS gate |
| −5 ≤ slope ≤ +5 | Hysteresis: no state change |

### 5.2 Safety interlocks

1. **Failsafe timeout**: if the gate remains open for > `NP_TAVNS_MAX_INSP_DURATION_MS` (6 s), `np_hrv_tavns_force_disable()` is called unconditionally. Normal inspiration at 4–7 BPM lasts 4–6 s; the 6 s limit trips only if RSA detection degrades (e.g., motion artefact).

2. **Safety MCU veto**: `np_tavns_enable_cb_t` requests enable from the safety MCU via SPI. The safety MCU independently checks clip impedance. If `np_hrv_tavns_safety_mcu_response(tavns, false)` is called (impedance check failed), `np_hrv_tavns_force_disable()` runs.

3. **Current**: `stim_current_ua` is validated at `np_hrv_tavns_init()` against a 100 µA minimum and `NP_VNS_MAX_CURRENT_UA` (40000 µA). Values outside that range are rejected with `NP_HRV_ERR_INVALID_ARG`. The 40 mA is a stop-gap derived from the per-phase charge limit (Rev 68, GitHub #554 and #559, `OI-VNSCLIP-09`); it replaced the 2 mA ceiling, which Rev 67 removed.

4. **A refused stimulus refuses the session** (Rev 2, OI-HRV-05). `np_hrv_tavns_init()` rejects a frequency outside 1–`NP_TAVNS_DEFAULT_FREQ_HZ` (25 Hz) or a current outside 100–2000 µA with `NP_HRV_ERR_INVALID_ARG`, and on rejection it **clears** its enable/disable callbacks, rate and current, so nothing from an earlier session stays armed. `np_hrv_session_start()` runs that validation before marking the session running and returns the error without starting. A value of 0 in either field still selects the default.

### 5.3 State machine

```
       slope < −5              slope > +5
IDLE ──────────────► STIMULATING ──────────────► COOLDOWN
  ▲                                                  │
  │              slope < −5 (next inspiration)       │
  └──────────────────────────────────────────────────┘
```

---

## 6. Dual EEG + HRV Biofeedback (np_hrv_eeg_biofeedback.c)

### 6.1 EEG band power

Welch's periodogram, 1024-point Hann window (2.048 s), 50% overlap:
- Per-channel power computed for delta/theta/alpha/beta/gamma bands
- Frontal alpha/theta ratio: mean of F3 (ch 2) and F4 (ch 3) alpha ÷ theta

### 6.2 Closed-loop adaptive rate (protocol 2 only)

Every `NP_ADAPTIVE_UPDATE_INTERVAL_S` (30 s):

```
if coherence < 5.0:
    rate += NP_ADAPTIVE_PACER_STEP_BPM   # recover coherence
elif alpha_theta_ratio < 1.5:
    rate -= NP_ADAPTIVE_PACER_STEP_BPM   # deepen relaxation
```

Rate clamped to [4.0, 7.0] BPM at all times.

---

## 7. Breathing Pacer — Resonance Frequency Sweep

On first session where no personalised resonance frequency (RF) is stored, the pacer automatically sweeps from 4.0 to 7.0 BPM in 0.5 BPM steps, dwelling for 120 seconds at each rate. Coherence scores are accumulated at each step via `np_hrv_pacer_sweep_coherence()`. At sweep completion, `np_hrv_pacer_sweep_finalise()` selects the rate with the highest mean coherence.

The personalised rate is returned via `best_rate_bpm_out` and must be persisted to the Config partition (SHDR — the rate value itself carries no user biology; see CLAUDE.md §5.1 boundary resolution rule).

---

## 8. UHDR / SHDR Data Routing

### 8.1 UHDR (User Health Data Record)

Written to UHDR partition at session end, encrypted with biometric-derived AES-256-XTS key (NeurOne never holds this key):

| Data element | Format | Notes |
|---|---|---|
| R-R interval time series | `uint16_t[]` in EDF+ annotation | Full session record. **No writer exists** (OI-HRV-06) |
| Coherence score array | `float[]` | One per 5-second update. The hub's 1 s `0x14` VNS/HRV telemetry record carries the current coherence, RMSSD and HR, read through HAL stubs `OI-VNS-07…09` |
| Session record (`np_hrv_session_record_t`) | UHDR record `0x1B`, 31 bytes serialized | Duration, protocol, mean/min/max coherence, RMSSD, RR count, taVNS count. Committed since Rev 3 (§8.1.1) |

#### 8.1.1 Session record commit path (Rev 3, OI-HRV-03)

The library does not write storage. `np_hrv_session_stop()` builds the record and passes it to the `end_cb` given to `np_hrv_session_create()`. On the hub that is `vns_hrv_session_end_cb()` in `firmware/hub_control/modules/np_mod_vns.c`, which calls `np_log_hrv_session()` (`firmware/hub_control/include/np_log_hrv.h`).

- **Encryption.** The session logger appends plaintext to the UHDR partition, which `np_uhdr_key_unlock()` has mounted AES-256-XTS under the user's biometric-derived key (`NP-FW-EMMC-002` §C). Encryption happens at the mounted block device. Neither the HRV library nor the logger holds or sees the key (`np_log_backend.h`, "Encryption boundary").
- **Which file.** The record goes into the session file that is open when it is logged. The runner stops every module before `np_log_session_end()`, and stopping the VNS module (`control(NULL)`) is what calls `np_hrv_session_stop()`. So the record lands in the session it describes, ahead of `SESSION_END`. An auto-complete from `np_hrv_session_tick()` fires during the session, and the later stop is then a no-op, so the record is written once. If the record is logged outside a session, UHDR accepts no append and the record is lost. It is never written into another session's file.
- **Layout** after tag `0x1B`, each field little-endian: reason (1, `np_hrv_status_t` as `int8`), `duration_s` (4), `protocol` (1), `target_rate_bpm` (4), `mean_coherence` (4), `min_coherence` (4), `max_coherence` (4), `mean_rmssd_ms` (4), `rr_sample_count` (2), `tavns_stim_count` (2). That is 31 bytes with the tag. The record is serialized field by field, so struct padding and `reserved[8]` never reach the medium.
- **`session_start_unix` is not written.** The library never sets it, because the device has no RTC backup (CLAUDE.md §4.5). A zero written there would read as a 1970 start time. The session file's `0x10` start record already carries the session's start time.
- **The statistics are valid only when the library saw data.** With no coherence update, the library leaves mean, min and max coherence and RMSSD at 0. A reader must treat `rr_sample_count = 0` as "no data" and never as a measured zero. On today's firmware that is every record (OI-HRV-07).
- **UHDR only.** Nothing from this record reaches SHDR. The SHDR coherence trend slope is §8.2 and OI-HRV-04.

### 8.2 SHDR (System Health Data Record)

Written to SHDR partition at session end, encrypted with NeurOne manufacturing key:

| Data element | Key | Notes |
|---|---|---|
| Coherence trend slope | `hrv_coherence_slope_v1` | Linear regression slope over last 30 sessions — no user biology |
| Session count increment | (existing counter) | Unsigned integer |

The coherence trend slope is computed from `np_hrv_coherence_trend_t` which stores mean coherence per session (not per-sample). The slope itself (positive/negative rate of change) carries no individual biometric information — it is a device performance/usage metric.

---

## 9. Module File Inventory

| File | Contents |
|------|---------|
| `include/np_hrv_config.h` | All configuration constants |
| `include/np_hrv_types.h` | All shared type definitions |
| `include/np_hrv_ppg.h` | PPG processing API |
| `include/np_hrv_coherence.h` | Coherence algorithm API |
| `include/np_hrv_pacer.h` | Breathing pacer API |
| `include/np_hrv_tavns_sync.h` | taVNS sync API |
| `include/np_hrv_eeg_biofeedback.h` | Dual EEG+HRV API |
| `include/np_hrv_session.h` | Session management API |
| `src/np_hrv_ppg.c` | PPG peak detection, RR buffer, RMSSD |
| `src/np_hrv_coherence.c` | FFT, Welch PSD, LF/HF band extraction |
| `src/np_hrv_pacer.c` | Breathing pacer, RF sweep |
| `src/np_hrv_tavns_sync.c` | RSA slope detection, taVNS gate |
| `src/np_hrv_eeg_biofeedback.c` | EEG band power, adaptive step |
| `src/np_hrv_session.c` | Protocol orchestration, UHDR record |
| `CMakeLists.txt` | Static library build, links libm |

---

## 10. Gate Closure: NP-COORD-001 G2-12

This document and its accompanying firmware (`firmware/hrv_biofeedback/`) satisfy the G2-12 gate requirement:

- [x] Coherence algorithm fully specified and implemented (§4, np_hrv_coherence.c)
- [x] taVNS inspiration-phase timing fully specified and implemented (§5, np_hrv_tavns_sync.c)
- [x] Dual EEG+HRV display state and adaptive step implemented (§6, np_hrv_eeg_biofeedback.c)
- [x] All four protocols orchestrated in session manager (np_hrv_session.c)
- [x] UHDR/SHDR data routing consistent with NP-FW-EMMC-001 Rev 1 §12 classification table
- [x] Safety interlocks documented and implemented (§5.2)
- [x] No new hardware required (BOM delta $0)

**G2-12 CLOSED — 2026-05-11**

---

## 11. Open Items

| ID | Description | Owner | Blocking |
|----|-------------|-------|---------|
| OI-HRV-01 | Platform HAL stubs (`np_platform_tavns_enable`, `np_platform_tavns_disable`, `np_platform_pacer_phase_notify`) must be implemented before integration testing | FW team | Integration test |
| OI-HRV-02 | Config partition API for persisting personalised resonance frequency (feeds output of `np_hrv_pacer_sweep_finalise`) must be wired in application layer | FW team | RF personalisation |
| ~~OI-HRV-03~~ | ✅ **CLOSED 2026-09-27 (Rev 3, §8.1.1).** The hub's `end_cb` commits the record through `np_log_hrv_session()` (UHDR tag `0x1B`) into this session's file on the AES-256-XTS-mounted UHDR partition. `np_log_backend_tests` pins the layout, the UHDR-only routing, the order ahead of `SESSION_END`, and the loss outside a session (never misfiled). *Was:* UHDR session record commit to eMMC must call storage layer AES-256-XTS write — `end_cb` currently returns raw plaintext record | FW/Storage team | — |
| OI-HRV-04 | SHDR coherence trend slope write after each session (uses `np_hrv_coherence_trend_t`) must call SHDR storage API | FW team | SHDR compliance |
| OI-HRV-05 | `NP_TAVNS_DEFAULT_FREQ_HZ` (25 Hz) vs user-configurable range 1–25 Hz: app layer must validate and pass `tavns_freq_hz` in `np_hrv_session_config_t` — **Firmware half CLOSED 2026-09-26 (Rev 2, §5.2 item 4, GitHub #447):** firmware no longer trusts the app to validate. An out-of-range frequency or current refuses the session and leaves nothing armed. `np_hrv_session_tests` covers 30 Hz (#386's value), 26 Hz, both current bounds, and the accepted edges. **Open:** the app still has to send a valid value, or the user gets a refused session. That is #386 `OI-NPPS-LIMITS-01` (a shipped 30 Hz protocol) | App team | Protocol 1 |
| OI-HRV-06 | **§8.1's R-R interval series has no writer.** The row specifies the full-session `uint16_t[]` series as an EDF+ annotation. Nothing in `firmware/hrv_biofeedback/`, `firmware/edf/` or `firmware/hub_control/` writes it, so a stored HRV session has a summary (`0x1B`) and 1 s telemetry snapshots (`0x14`), and no series. Found while closing OI-HRV-03 | FW team | UHDR completeness |
| OI-HRV-07 | **No task feeds or ticks the HRV session.** `np_hrv_session_push_ppg()`, `np_hrv_session_push_eeg()` and `np_hrv_session_tick()` have no caller outside the library. `np_mod_vns.c`'s banner says the library "runs as a separate FreeRTOS thread", but no such task is created. So on today's firmware no coherence is computed, the taVNS gate never opens, the session never auto-completes, and every committed `0x1B` record has zero statistics (`rr_sample_count = 0`, §8.1.1). No stimulation consequence: the safety MCU owns the enable. Found while closing OI-HRV-03 | FW team | Integration test |
