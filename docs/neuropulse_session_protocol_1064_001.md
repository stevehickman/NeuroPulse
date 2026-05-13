# NP-SES-1064-001 Rev A
## 1064nm Smart Zone Module — Session Protocol Specification

**Document:** NP-SES-1064-001 Rev A  
**Date:** 2026-05-13  
**Status:** Baselined  
**References:** NP-FW-PBM1064-001 Rev A; NP-FW-EMMC-001 Rev A; CLAUDE.md §3 (PBM Transcranial); Issue #53

---

## 1. Session Descriptor v4

The app generates a session descriptor, signs it with the device-resident Ed25519 signing key, and delivers it over USB-C or BLE to the hub. The hub firmware verifies the signature before any I2C writes.

```c
/*
 * np_pbm1064_session_desc_t — NP-SES-1064-001 Rev A
 * Serialised as little-endian byte stream; Ed25519 signature covers
 * all fields at byte offsets 0 .. (sizeof - 64).
 */

#define NP_SES1064_VERSION  0x04

typedef struct {
    uint8_t  version;               /* 0x04 */
    uint8_t  smart_module_mask;     /* bit[n]=1 → slot n carries smart module */
    uint8_t  eeg_adaptive_mode;     /* 0=uniform, 1=gradient (OI-SES-02) */
    uint8_t  reserved0;
    uint16_t duration_s;            /* total session duration (60–3600) */
    uint16_t reserved1;

    /* Per-zone preset (index 0–4 = ZM-01–ZM-05). Ignored if slot not in mask. */
    struct {
        uint8_t  cur_a;             /* 660nm current setpoint (0–255 → 0–180 mA) */
        uint8_t  cur_b;             /* 808nm current setpoint */
        uint8_t  cur_c;             /* 1064nm current setpoint */
        uint8_t  freq_hz;           /* initial PWM frequency (2/6/10/20/40/0=CW) */
        uint8_t  duty;              /* initial duty cycle (0–50; ≤0x32 enforced) */
        uint8_t  channel_mask;      /* bit[0]=CH_A, bit[1]=CH_B, bit[2]=CH_C */
        uint8_t  reserved[2];
    } zone[5];

    uint8_t  signature[64];         /* Ed25519; covers bytes 0 .. (sizeof-64)-1 */
} np_pbm1064_session_desc_t;        /* total: 52 bytes header + 40 bytes zones = 92 + 64 sig = 156 bytes */
```

---

## 2. Session Presets (Named Protocols)

| Preset ID | Name | CH_A (660nm) | CH_B (808nm) | CH_C (1064nm) | Freq | Duty | Duration |
|-----------|------|-------------|-------------|--------------|------|------|----------|
| 0x01 | Gamma clarity | 180 mA | 180 mA | 150 mA | 40 Hz | 25% | 20 min |
| 0x02 | Alpha calm | 150 mA | 150 mA | 120 mA | 10 Hz | 20% | 20 min |
| 0x03 | Theta memory | 150 mA | 150 mA | 120 mA | 6 Hz | 20% | 20 min |
| 0x04 | Sleep deep | 120 mA | 120 mA | 100 mA | 2 Hz | 15% | 30 min |
| 0x05 | Gamma+theta | 180 mA | 180 mA | 150 mA | 40/6 Hz split | 25% | 20 min |
| 0x06 | Focus prime | 160 mA | 160 mA | 130 mA | 20 Hz | 22% | 20 min |
| 0x07 | Vascular baseline | 100 mA | 100 mA | 80 mA | CW | 100% CW | 10 min |
| 0xFF | Custom | app-defined | app-defined | app-defined | app-defined | ≤25% | app-defined |

All preset duty values are ≤ 25% (DUTY_MAX_REG enforcement). Preset 0x07 uses CW mode (PWM_FREQ = 0x00, DUTY register unused; CH_ENABLE drives constant current).

---

## 3. EEG-Adaptive Frequency Mapping

The EEG processing layer delivers `eeg_dominant_freq_hz` as a float once per second. Session firmware maps it to the nearest supported driver PWM code:

| EEG dominant band | Freq (Hz) | Driver code |
|-------------------|-----------|-------------|
| Delta (0.5–4) | 2.0 | 0x01 |
| Theta (4–8) | 6.0 | 0x06 |
| Alpha (8–12) | 10.0 | 0x0A |
| Beta (12–30) | 20.0 | 0x14 |
| Gamma (>30) | 40.0 | 0x28 |

Hysteresis: frequency code only changes if `eeg_dominant_freq_hz` has been in the new band for ≥ 3 consecutive 1 s ticks (prevents rapid switching at band boundaries).

---

## 4. Session Data Flow (UHDR / SHDR)

```
Session start
│
├─ UHDR: session_start_unix, protocol_params, zone_mask        → UHDR partition
│
├─ Every 100 ms (dose tick)
│   ├─ PD1_counts[zone][wl], PD2_counts[zone][wl]            → UHDR (ring buffer)
│   └─ dose_J_cm2[zone][wl] (cumulative)                     → UHDR (updated)
│
├─ Every 1 Hz (NTC tick)
│   └─ NTC_temp_C[zone]                                       → UHDR
│
├─ Every 5 s (I2C status poll)
│   └─ fault_latch register value                             → SHDR (fault log if non-zero)
│
├─ EEG adaptive event (on freq change)
│   └─ {timestamp_relative_s, old_freq, new_freq}            → UHDR
│
└─ Session end
    ├─ np_pbm1064_session_record_t                            → UHDR (EDF+ annotation)
    └─ np_pbm1064_shdr_summary_t                              → SHDR
```

---

## 5. Ramp Profile

30 s linear ramp on all active channels. Ramp tick rate: 10 Hz (same as dose tick).

```
duty_step = target_duty / (30 × 10)   // per 100 ms tick
ramp_duty = duty_step × tick_count
```

Duty is written to all smart module slots simultaneously each tick (I2C write burst per slot). On ramp abort (fault), all channels set to duty=0 and CH_ENABLE=0x00 within 100 ms.

---

## 4. T2 Combined Session (1064nm Cortical + 1170nm Subcortical)

**Document:** NP-SES-1064-001 Rev A §4  
**Firmware:** `np_pbm1064_t2_combined` (`firmware/pbm_1064nm/`)  
**Status:** Issue #55 — software PASS; hardware FAI pending T2 prototype

### 4.1 Three-Tier Penetration Stack

The T2 combined session delivers coordinated multi-depth PBM via three laser subsystems operating simultaneously under a single session orchestrator:

| Tier | Wavelength | Depth | Subsystem | Targeting |
|------|-----------|-------|-----------|-----------|
| Surface | 660 nm (CH_A) | 0–5 mm | Smart zone module | Full scalp coverage |
| Cortical | 1064 nm (CH_C) | ~20–30 mm | Smart zone module | Up to 5 addressable zones |
| Subcortical | 1170 nm | ~35–40 mm | T2 TEC-stabilised laser | sLORETA-guided (T2 depression protocol) |

This architecture replicates the depth-tier rationale from the penetration physics literature (Bashkatov 2005, Strangman 2002 — NP-BIB-1064-001 Rev A entries 12.3a–12.3c).

### 4.2 Combined Session Descriptor

```c
/*
 * np_t2_combined_desc_t — NP-SES-1064-001 Rev A §4.2
 * Ed25519 signature covers all fields at bytes 0 .. (sizeof - 64).
 * Hub firmware verifies before any laser enable.
 */
typedef struct {
    uint8_t  version;              /* NP_T2_COMBINED_VERSION = 0x01                */
    uint8_t  t2_combined_enable;   /* 1 = activate 1170nm laser                    */
    uint8_t  sloreta_enable;       /* 1 = read sLORETA MNI target at session start  */
    uint8_t  reserved;
    np_pbm1064_session_desc_t pbm1064; /* 1064nm smart zone sub-descriptor (v4)   */
    np_t2_1170_preset_t       laser1170; /* { duty_pct, freq_hz, duration_s }      */
    uint8_t  signature[64];        /* Ed25519                                      */
} np_t2_combined_desc_t;
```

### 4.3 Parallel Ramp Coordination

Both the 1064nm smart zone session and the 1170nm laser ramp **in parallel** from a single combined safety MCU enable:

```
t = 0                            t = 30s               t = session_end - 30s
│                                │                      │
├─ 1064nm ramp 0→target duty ───►│ full duty ──────────►│ ramp down 30s
├─ 1170nm ramp 0→laser_duty ────►│ full power ─────────►│ ramp down 30s
```

- Safety MCU issues a single combined enable at `t = 0` (SPI write from main processor).
- Hub firmware drives the 1170nm ramp duty via `np_pbm1064_hal_t2_1170_set_duty()` at 100 ms tick rate, parallel with the 1064nm ramp in `np_pbm1064_session_tick()`.
- Ramp abort on any fault immediately disables both subsystems within 100 ms.

### 4.4 Thermal Throttle Priority Cascade

When aggregate thermal load triggers throttle, the priority cascade is applied one step per call to `np_pbm1064_t2_apply_thermal_throttle()`:

| Step | Subsystem | Rationale |
|------|-----------|-----------|
| 1 | 1170nm TEC laser | Deepest penetration; highest per-watt subcortical heat deposition |
| 2 | 1064nm CH_C (smart zone) | Cortical depth; second-highest thermal contribution |
| 3 | 808nm CH_B (smart zone) | Mid-surface; lower thermal contribution |
| 4 | 660nm CH_A (smart zone) | Surface only; last resort |

TEC temperature is polled every 1 s via `np_pbm1064_hal_t2_1170_get_temp()`. Threshold `NP_T2_1170_TEC_FAULT_C` (45 °C) triggers step-1 throttle; `NP_T2_1170_TEC_CUTOFF_C` (50 °C) triggers immediate abort.

### 4.5 sLORETA Depression Protocol Coordination (T2, OI-SES-T2-01)

For the T2 depression protocol, the sLORETA engine (NP-FW-HD-001 Rev A) provides the MNI cortical target at session start. The 1170nm laser is positioned to irradiate subcortical regions below the identified cortical source.

**Workflow (when `sloreta_enable = 1`):**
1. App runs T2 21-ch resting-state qEEG session.
2. sLORETA computes source power map; identifies peak MNI target (e.g., DLPFC hypoactivity).
3. App sets `sloreta_enable = 1` in combined descriptor.
4. At `np_pbm1064_t2_start_combined()`, firmware reads MNI target via `np_pbm1064_hal_t2_sloreta_get_target()`.
5. MNI coordinate logged to UHDR combined record. Clinician app displays depth-tier alignment illustration.
6. **Hardware coordination (pending T2 prototype):** Laser arm positioning from sLORETA MNI → physical placement is a procedural step; firmware records the target but does not drive robotic positioning in this revision.

**Stub behaviour (current):** `np_pbm1064_hal_t2_sloreta_get_target()` returns DLPFC_L stub coordinates (MNI x=-40, y=40, z=30) with `valid = true` so that FAI-T2-01 and FAI-T2-03 exercise the logging path.

### 4.6 Session State Machine

```
T2_IDLE → T2_PREFLIGHT → T2_RAMP_UP → T2_ACTIVE → T2_RAMP_DOWN → T2_COMPLETE
                                            ↕
                                        T2_FAULT
```

`T2_PREFLIGHT`: 1064nm preflight (I2C probe, NTC check) + sLORETA target read + 1170nm enable.  
`T2_RAMP_UP`: parallel 30 s ramp for both subsystems.  
`T2_ACTIVE`: full dose; TEC poll at 1 Hz; 1064nm dose tick at 10 Hz.  
`T2_RAMP_DOWN`: parallel ramp-down; 1064nm drives ramp, 1170nm mirrors proportionally.  
`T2_COMPLETE`: final 1170nm dose read; both subsystems disabled; UHDR + SHDR written.  
`T2_FAULT`: all channels disabled immediately; both records written with fault reason.

### 4.7 UHDR / SHDR Records

**UHDR — np_t2_combined_uhdr_record_t (user-owned, biometric-derived AES-256 key)**

| Field | Type | Notes |
|-------|------|-------|
| `pbm1064_record` | `np_pbm1064_session_record_t` | Per-zone, per-wavelength 1064nm dose |
| `dose_1170_J_cm2` | `float` | Total 1170nm subcortical dose this session |
| `irradiance_1170_peak_mW_cm2` | `float` | Peak irradiance recorded by monitor PD |
| `throttle_events_1170` | `uint32_t` | Count of TEC thermal throttle events |
| `throttle_events_ch_c` | `uint32_t` | Count of CH_C throttle events from T2 load |
| `sloreta_mni_x/y/z` | `int16_t` | MNI target coordinate (mm) at session start |
| `sloreta_valid` | `uint8_t` | 1 = sLORETA coordinate was read successfully |
| `abort_reason` | `uint8_t` | 0 = normal; `np_pbm1064_fault_t` on fault |
| `duration_s` | `uint32_t` | Actual session duration |

**SHDR — np_t2_combined_shdr_summary_t (device-owned, no user biology)**

| Field | Type | Notes |
|-------|------|-------|
| `pbm1064_shdr` | `np_pbm1064_shdr_summary_t` | 1064nm device health metrics |
| `tec_throttle_events` | `uint8_t` | Count of TEC temperature threshold crossings |
| `laser_fault_flag` | `uint8_t` | 1 = 1170nm module reported a fault |
| `sloreta_session_flag` | `uint8_t` | 1 = sLORETA session was active |
| `abort_reason` | `uint8_t` | np_pbm1064_fault_t; 0 = normal |

### 4.8 FAI Test Specifications (software-passable)

| ID | Description | Requirement | Status |
|----|-------------|-------------|--------|
| FAI-T2-01 | Combined descriptor validation | NULL and bad-version rejected; valid accepted; sLORETA MNI logged to UHDR | **Software PASS** |
| FAI-T2-02 | Full 4-step thermal throttle cascade | 1170nm → CH_C → CH_B → CH_A; each call advances one step; 5th call no-op | **Software PASS** |
| FAI-T2-03 | Combined UHDR record completeness | 1064nm dose, 1170nm dose, sLORETA fields, SHDR ≤128 bytes, abort_reason=0 on init | **Software PASS** |
| FAI-T2-04 | 1170nm abort path | Abort disables 1170nm and 1064nm; combined stage = FAULT; abort_reason recorded | **Software PASS** |
| FAI-T2-05 | 1170nm dose metering ≤±15% at 1000 mW/cm² | Requires calibrated 1170nm optical bench + T2 prototype | **Hardware bench — PENDING** |
| FAI-T2-06 | TEC temperature interlock | TEC >45°C → step-1 throttle; TEC >50°C → abort | **Hardware bench — PENDING** |

FAI-T2-05 and FAI-T2-06 are blocking for T2 clinical release (NP-COORD-001 G3-07 hardware path).

### 4.9 Open Items

| ID | Description | Blocking |
|----|-------------|---------|
| OI-SES-T2-01 | sLORETA HAL wire-up to np_fw_sloreta engine | T2 prototype |
| OI-SES-T2-02 | 1170nm monitor PD HAL implementation (OI-PBM-08 expansion) | FAI-T2-05 |
| OI-SES-T2-03 | Bilateral 4×1 montage coordination with sLORETA-guided HD-tDCS in same session | T2 prototype |
| OI-SES-T2-04 | Cortical gradient EEG-adaptive mode across 1064nm zones (OI-SES-02) with sLORETA weighting | T2 qEEG integration |

---

## 5. Ramp Profile

30 s linear ramp on all active channels. Ramp tick rate: 10 Hz (same as dose tick).

```
duty_step = target_duty / (30 × 10)   // per 100 ms tick
ramp_duty = duty_step × tick_count
```

Duty is written to all smart module slots simultaneously each tick (I2C write burst per slot). On ramp abort (fault), all channels set to duty=0 and CH_ENABLE=0x00 within 100 ms.

---

## 6. Safety Limits Summary

| Parameter | Limit | Enforcement |
|-----------|-------|-------------|
| Duty cycle max | 25% (0x32) | Firmware ceiling, unconditional |
| 660nm dose/session | 60 J/cm² | Dose tick; channel disable on limit |
| 808nm dose/session | 60 J/cm² | Dose tick; channel disable on limit |
| 1064nm dose/session | 36 J/cm² | Dose tick; channel disable on limit (OI-SES-01) |
| **1170nm dose/session** | **60 J/cm²** | **T2 combined dose tick; laser disable on limit** |
| Aggregate irradiance | 600 mW/cm² | Throttle cascade; pending OI-PBM-05 |
| NTC junction temp | 62 °C | Hardware current throttle + firmware gate |
| IEC 60601 surface temp | 42 °C | Safety MCU GPIO interlock |
| **TEC fault temp** | **45 °C** | **1170nm step-1 throttle** |
| **TEC cutoff temp** | **50 °C** | **Immediate 1170nm disable + session abort** |
| Safety MCU heartbeat | 200 ms | Main processor SPI; 1.5 s watchdog → all-disable |
