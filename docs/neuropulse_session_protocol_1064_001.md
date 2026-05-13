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

## 6. Safety Limits Summary

| Parameter | Limit | Enforcement |
|-----------|-------|-------------|
| Duty cycle max | 25% (0x32) | Firmware ceiling, unconditional |
| 660nm dose/session | 60 J/cm² | Dose tick; channel disable on limit |
| 808nm dose/session | 60 J/cm² | Dose tick; channel disable on limit |
| 1064nm dose/session | 36 J/cm² | Dose tick; channel disable on limit (OI-SES-01) |
| Aggregate irradiance | 600 mW/cm² | Throttle cascade; pending OI-PBM-05 |
| NTC junction temp | 62 °C | Hardware current throttle + firmware gate |
| IEC 60601 surface temp | 42 °C | Safety MCU GPIO interlock |
| Safety MCU heartbeat | 200 ms | Main processor SPI; 1.5 s watchdog → all-disable |
