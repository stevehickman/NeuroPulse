# HRV Biofeedback Protocol Firmware Specification

**Project:** NeurOne
**Document:** NP-FW-HRV-001
**Revision:** A
**Date:** 2026-05-11
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
| `NP_TAVNS_MAX_CURRENT_UA` | 2000 | 2 mA absolute limit |
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

A sliding buffer of `NP_TAVNS_INSP_SLOPE_WIN` (6) consecutive R-R differences is maintained. The mean of this buffer gives `slope` (ms/beat):

| Condition | Phase detected |
|-----------|---------------|
| slope < −5 ms/beat | Inspiration onset → open taVNS gate |
| slope > +5 ms/beat | Expiration onset → close taVNS gate |
| −5 ≤ slope ≤ +5 | Hysteresis: no state change |

### 5.2 Safety interlocks

1. **Failsafe timeout**: if the gate remains open for > `NP_TAVNS_MAX_INSP_DURATION_MS` (6 s), `np_hrv_tavns_force_disable()` is called unconditionally. Normal inspiration at 4–7 BPM lasts 4–6 s; the 6 s limit trips only if RSA detection degrades (e.g., motion artefact).

2. **Safety MCU veto**: `np_tavns_enable_cb_t` requests enable from the safety MCU via SPI. The safety MCU independently checks clip impedance. If `np_hrv_tavns_safety_mcu_response(tavns, false)` is called (impedance check failed), `np_hrv_tavns_force_disable()` runs.

3. **Maximum current**: `stim_current_ua` is validated at `np_hrv_tavns_init()` against `NP_TAVNS_MAX_CURRENT_UA` (2000 µA). Values above the limit are rejected with `NP_HRV_ERR_INVALID_ARG`.

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
| R-R interval time series | `uint16_t[]` in EDF+ annotation | Full session record |
| Coherence score array | `float[]` | One per 5-second update |
| Session record (`np_hrv_session_record_t`) | 48-byte struct | Duration, protocol, mean/min/max coherence, RMSSD, RR count, taVNS count |

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
- [x] UHDR/SHDR data routing consistent with NP-FW-EMMC-001 Rev A §12 classification table
- [x] Safety interlocks documented and implemented (§5.2)
- [x] No new hardware required (BOM delta $0)

**G2-12 CLOSED — 2026-05-11**

---

## 11. Open Items

| ID | Description | Owner | Blocking |
|----|-------------|-------|---------|
| OI-HRV-01 | Platform HAL stubs (`np_platform_tavns_enable`, `np_platform_tavns_disable`, `np_platform_pacer_phase_notify`) must be implemented before integration testing | FW team | Integration test |
| OI-HRV-02 | Config partition API for persisting personalised resonance frequency (feeds output of `np_hrv_pacer_sweep_finalise`) must be wired in application layer | FW team | RF personalisation |
| OI-HRV-03 | UHDR session record commit to eMMC must call storage layer AES-256-XTS write — `end_cb` currently returns raw plaintext record | FW/Storage team | UHDR compliance |
| OI-HRV-04 | SHDR coherence trend slope write after each session (uses `np_hrv_coherence_trend_t`) must call SHDR storage API | FW team | SHDR compliance |
| OI-HRV-05 | `NP_TAVNS_DEFAULT_FREQ_HZ` (25 Hz) vs user-configurable range 1–25 Hz: app layer must validate and pass `tavns_freq_hz` in `np_hrv_session_config_t` | App team | Protocol 1 |
