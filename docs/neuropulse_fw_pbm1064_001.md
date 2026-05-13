# NP-FW-PBM1064-001 Rev A — 1064 nm Smart Zone Module Firmware
**Project:** NeuroPulse  
**Document number:** NP-FW-PBM1064-001  
**Revision:** A  
**Date:** 2026-05-12  
**Status:** Baselined  
**Author:** Firmware Engineering  
**Related issues:** GitHub Issue #53 (hub I2C smart-module protocol + zone firmware), Issue #54 (hardware), Issue #55 (T2 combined session)

---

## 1. Scope

This document specifies the firmware for the 1064 nm smart zone module upgrade accessory for NeuroPulse T1 and T2. It covers the hub-side I2C smart-module detection and control protocol, three-channel LED drive (660 nm, 808 nm, 1064 nm), InGaAs photodiode dose-metering, session orchestration for T1 three-wavelength protocols, and T2 combined 1064 nm + 1170 nm cortical+deep session coordination.

**Architectural decision locked (Issue #53):** Path B smart module. The 1064 nm zone module carries an on-module I2C-controlled LED driver IC that independently drives three LED channels. The hub communicates with the on-module driver over an I2C bus whose signals are carried on repurposed FPC pins (former LED_B current lines), detected via a low-resistance ZONE_ID signature. Base modules (660 nm + 808 nm direct-drive) are unaffected; backwards compatibility is unconditional.

**In scope:**
- Smart module detection via ZONE_ID ADC threshold (pin 18)
- I2C bus initialisation and command protocol (hub ↔ on-module driver IC)
- Three-channel LED PWM control: 660 nm (channel A), 808 nm (channel B), 1064 nm (channel C)
- InGaAs PD dose-metering correction for PD1 and PD2 at 1064 nm
- Per-wavelength J/cm² accumulation and safety enforcement
- Combined thermal safety interlock (three-channel aggregate NTC limit)
- T1 three-wavelength session preset library (extends existing 7 presets)
- T2 combined 1064 nm + 1170 nm cortical+deep session orchestration
- UHDR/SHDR data classification and logging
- FAI test specifications

**Out of scope:**
- On-module driver IC hardware selection (Issue #54 hardware)
- 1064 nm LED emitter procurement and binning (Issue #54)
- InGaAs PD part selection (Issue #54)
- 1170 nm T2 laser system firmware (existing T2 architecture)
- Zone module bone conduction announcement (NP-FW-ZA-001 — unchanged)
- EEG, stimulation, or VNS modalities

---

## 2. Background and Design Rationale

### 2.1 Why a smart module

The existing 20-pin FPC connector (NP-HW-FPC-001 Rev D) has no free pins for a third independent LED drive channel. All 20 pins are allocated across power rails, two LED drive channels (660 nm and 808 nm), dual photodiodes (PD1, PD2), NTC thermistor, and ZONE_ID. Adding a third wavelength without revising the connector requires moving LED drive control onto the module itself and communicating with it over a serial bus on existing pins.

The smart module carries an I2C-controlled LED driver IC that accepts power and I2C commands over FPC pins, then drives three LED channels independently. The hub provides VLED and GND as normal; I2C SDA and SCL repurpose the former LED_B current lines (no current flows through these pins in a smart module's base module operating mode, making repurposing safe and backwards-compatible on the connector).

### 2.2 Backwards compatibility guarantee

The hub firmware determines module type at insertion via the ZONE_ID ADC reading on pin 18:
- ADC reading in ZM-01..ZM-05 range (1400–4000 counts) → base module → existing direct-PWM control path, unchanged
- ADC reading < 1100 counts → smart module → I2C control path

Base modules are never touched by smart-module code. The two paths are fully independent below the session orchestration layer.

### 2.3 Why InGaAs photodiodes for dose metering

Silicon photodiodes exhibit ~8–12% of peak responsivity at 1064 nm (silicon bandgap cutoff ~1100 nm). At the signal levels produced by the PBM dose-metering circuit (photodiode current ~1–50 nA for therapeutic irradiance levels at distance), the silicon PD output at 1064 nm is insufficient for reliable dose measurement. InGaAs photodiodes have ~0.85 A/W responsivity at 1064 nm vs ~0.45 A/W for silicon at 810 nm. The smart module carries InGaAs PDs in the PD1 and PD2 footprints; the FPC pad geometry (1.6 mm annular ring, scalp-facing Cu layer, hard gold ≥0.5 µm, NP-HW-FPC-001 Rev D F-04) is unchanged, ensuring mechanical compatibility with the existing zone module mould PD2 aperture.

The transimpedance amplifier (TIA) on the hub PCB is shared between base and smart modules. Its gain is fixed at the hub-PCB level. The dose-metering firmware compensates using wavelength-specific calibration coefficients loaded from the Config partition at session start.

---

## 3. Hardware Interface

### 3.1 ZONE_ID smart module detection

Source: NP-HW-FPC-001 Rev D, §11.4 (five-layer keying, Layer 2).  
Pull-up: 10 kΩ to 3.3 V on hub PCB. ADC: i.MX RT1062 LPADC1 channel 12, 12-bit, Vref = 3.3 V.

| Module type | Pin 18 resistor | VOUT (V) | ADC counts (nominal) |
|-------------|-----------------|----------|----------------------|
| ZM-01 base | 10 kΩ | 1.650 | 2048 |
| ZM-02 base | 22 kΩ | 2.284 | 2835 |
| ZM-03 base | 47 kΩ | 2.593 | 3217 |
| ZM-04 base | 100 kΩ | 2.750 | 3413 |
| ZM-05 base | 220 kΩ | 2.876 | 3570 |
| **Smart module** | **3.3 kΩ** | **0.811** | **1006** |
| Empty slot | open | 3.300 | 4095 |

**Smart module detection threshold:** ADC < 1100 counts (midpoint between smart module 1006 and ZM-01 2048, with 3 dB margin for 1% resistor tolerance and ADC noise). Debounce: 3×100 ms reads at ≥2/3 majority (RISK-18, same as base module).

Zone position is determined by the physical slot index (slot 0–4 in the hub ZIF array), not by the ZONE_ID resistor, when a smart module is detected. The mechanical key (RISK-15 Layer 1, asymmetric per zone) prevents wrong-slot insertion without relying on the resistor. Bone conduction announcement (NP-FW-ZA-001) uses slot index for zone name lookup when ZONE_ID reads as smart module.

### 3.2 I2C bus

| Parameter | Value |
|-----------|-------|
| Physical pins | FPC pins 9–12 (former LED_B current lines), SDA on pin 10, SCL on pin 11 |
| Hub peripheral | i.MX RT1062 LPI2C3 (separate LPI2C instance per zone-slot pair via GPIO mux — 5 independent buses) |
| Bus speed | 100 kHz (safe for FPC trace capacitance ≤200 pF over 400 mm max trace) |
| On-module driver I2C address | 0x30 (fixed; no address-select pin conflict possible — one driver per bus segment) |
| Pull-ups | 4.7 kΩ to 3.3 V on hub PCB per bus segment |
| Bus isolation | Each zone slot has its own LPI2C bus; no shared bus between zones |

**I2C availability:** For base modules, pins 9–12 carry high-current LED drive; LPI2C3 GPIO mux remains disabled for that slot. The hub firmware enables LPI2C3 for a slot only after confirming ADC < 1100 (smart module). This prevents I2C drive contention on a base-module slot.

### 3.3 On-module LED driver IC

Hardware specification is deferred to Issue #54. Firmware requirements imposed:

| Requirement | Specification |
|-------------|---------------|
| Interface | I2C, address 0x30, 100 kHz |
| Channels | 3 independently controlled (CH_A=660 nm, CH_B=808 nm, CH_C=1064 nm) |
| Per-channel current | 80–200 mA, ≥1 mA resolution |
| Per-channel PWM frequency | 0.5–100 Hz, CW (0 Hz) mode |
| Per-channel duty cycle | 0–100%, ≤25% firmware-enforced maximum (§5.3) |
| PWM resolution | ≥8-bit |
| Enable/disable per channel | Individual GPIO or I2C register bit |
| Status register | Thermal fault flag, OCP flag, open-LED flag per channel |
| Startup time | ≤5 ms from I2C enable to first PWM pulse |
| Power supply | VLED from FPC (hub-regulated) |

### 3.4 InGaAs photodiode interface

PD1 (behind PDMS window, forward-emission) and PD2 (scalp-facing, RISK-14 Option B) are InGaAs parts in the smart module. The TIA on the hub PCB is unchanged. Firmware dose-metering accounts for the different responsivity through wavelength-specific calibration coefficients.

| Parameter | Base module (Si PD, 810 nm) | Smart module (InGaAs PD, 1064 nm) |
|-----------|-----------------------------|-----------------------------------|
| PD responsivity | ~0.45 A/W | ~0.85 A/W |
| TIA gain (hub PCB) | R_TIA (fixed) | Same R_TIA |
| ADC full-scale irradiance | 400 mW/cm² at calibrated distance | different — set by K_PD1_1064 coefficient |
| Calibration source | Factory: integrating sphere reference at 810 nm | Factory: integrating sphere reference at 1064 nm |
| Coefficient storage | Config partition, zone × wavelength | Config partition, zone × wavelength |

**Calibration coefficient table (Config partition, np_pbm_cal_t):**

```
float K_PD1[5][3];   // zone[0..4] × wavelength[660nm, 808nm, 1064nm], mW/cm² per ADC count
float K_PD2[5][3];   // same for PD2
float K_ratio_nom[5][3]; // nominal PD1/PD2 ratio at factory (fouling/aging baseline)
```

At session start, the firmware reads these coefficients from the Config partition. For a freshly manufactured smart module with no factory calibration record, default coefficients (K_DEFAULT_1064_PD1, K_DEFAULT_1064_PD2) from firmware flash are used and a SHDR flag `cal_source = DEFAULT` is written.

---

## 4. Smart Module Detection State Machine

```
SLOT_IDLE
  │ FPC insertion contact event (debounced 20 ms)
  ▼
ZONE_ID_SAMPLING
  │ 3×100 ms ADC reads (RISK-18 debounce)
  ├─ ≥2/3 reads in ZM-01..ZM-05 range (1100–4000) → base module path (NP-FW-ZA-001)
  ├─ ≥2/3 reads < 1100 → SMART_MODULE_DETECTED
  └─ no majority → ZONE_ID_FAULT (bone conduction error cue, SHDR log)

SMART_MODULE_DETECTED
  │ LPI2C3 GPIO mux enabled for this slot
  │ I2C probe: write 0x00 read-register request to address 0x30
  ├─ ACK received within 5 ms → SMART_MODULE_READY
  └─ NACK or timeout → SMART_MODULE_FAULT (bone conduction error, SHDR log)

SMART_MODULE_READY
  │ Query driver IC for status register (channel count, fault flags)
  │ Write calibration coefficients to driver IC configuration registers
  │ Bone conduction announcement: slot-position beep count + confirm tone (NP-FW-ZA-001 callback)
  │ SHDR auth log: zone_id=SMART, slot_index=N, i2c_probe=PASS
  │ Insert callback → application layer (zone available for session scheduling)
  ▼
SMART_MODULE_ACTIVE
  │ Periodic I2C status poll every 5 s during session (thermal fault, OCP, open-LED checks)
  │ NTC temperature read over I2C every 1 s
  │ PD1/PD2 ADC sample at 10 Hz (dose accumulation tick)
  ├─ Thermal fault from driver IC → immediate channel disable → SHDR fault log
  ├─ OCP → immediate channel disable + session abort notification → SHDR fault log
  └─ FPC removal (ZONE_ID reads open, 50 ms debounce) → SLOT_IDLE
```

---

## 5. I2C Command Protocol

All I2C transactions are initiated by the hub (master). The on-module driver IC is slave-only.

### 5.1 Register map

| Register | Address | R/W | Description |
|----------|---------|-----|-------------|
| STATUS | 0x00 | R | [7] thermal fault, [6] OCP_A, [5] OCP_B, [4] OCP_C, [3] open_A, [2] open_B, [1] open_C, [0] ready |
| CH_ENABLE | 0x01 | R/W | [2] en_C (1064 nm), [1] en_B (808 nm), [0] en_A (660 nm) |
| CUR_A | 0x02 | R/W | Channel A (660 nm) current, 1 mA/LSB, range 0x00–0xC8 (0–200 mA) |
| CUR_B | 0x03 | R/W | Channel B (808 nm) current |
| CUR_C | 0x04 | R/W | Channel C (1064 nm) current |
| PWM_FREQ_A | 0x05 | R/W | Channel A PWM frequency, 0.5 Hz/LSB, 0x00 = CW, 0x01 = 0.5 Hz … 0xC8 = 100 Hz |
| PWM_FREQ_B | 0x06 | R/W | Channel B PWM frequency |
| PWM_FREQ_C | 0x07 | R/W | Channel C PWM frequency |
| DUTY_A | 0x08 | R/W | Channel A duty cycle, 0.5%/LSB, 0x00 = 0%, max 0x32 = 25% (firmware-enforced ceiling — driver IC register accepts 0–0xFF but hub firmware never writes > 0x32) |
| DUTY_B | 0x09 | R/W | Channel B duty cycle |
| DUTY_C | 0x0A | R/W | Channel C duty cycle |
| THERMAL | 0x0B | R | NTC temperature, 1°C/LSB, signed int8 |
| FAULT_LATCH | 0x0C | R/W | Latched fault flags; write 0x00 to clear after fault condition resolved |
| CONFIG | 0x0D | R/W | [1] pwm_sync (sync all channel PWM edges to common clock), [0] soft_start (2 s ramp on enable) |

### 5.2 Session startup sequence

```
1.  Write CONFIG: pwm_sync=1, soft_start=1
2.  Write CUR_A, CUR_B, CUR_C: session current (from preset, see §7)
3.  Write PWM_FREQ_A, PWM_FREQ_B, PWM_FREQ_C: session frequency per channel
4.  Write DUTY_A, DUTY_B, DUTY_C: session duty cycle per channel (≤0x32)
5.  Write CH_ENABLE: set enable bits for active wavelengths
6.  Poll STATUS register: wait for ready=1, timeout 100 ms → session abort
7.  Begin dose accumulation tick (10 Hz PD1/PD2 ADC samples)
```

### 5.3 Safety enforcement at hub firmware layer

The hub firmware never writes DUTY_A/B/C > 0x32 (25%). This is a software ceiling, separate from any driver IC limit. The safety MCU (STM32G071) owns PBM_EN_L GPIO for all zones; it does not distinguish smart from base modules at the GPIO level. The existing safety MCU heartbeat and watchdog path (SPI every 200 ms, 1.5 s watchdog → all-stimulation cutoff) applies identically to smart modules.

Additionally, the hub firmware enforces a combined duty cycle check: if any two or more channels are simultaneously enabled on the same zone, the combined irradiance at the scalp must not exceed 400 mW/cm² peak (firmware estimate based on calibrated K_PD1 coefficients per channel). If the sum exceeds the IEC 60601 42°C thermal ceiling (as indicated by NTC, §5.4), the firmware reduces duty cycle of the highest-power channel first.

### 5.4 Thermal interlock — three-channel aggregate

With three wavelengths active simultaneously, the aggregate photon-to-heat deposition in scalp tissue is higher than any single-wavelength case. The existing NTC per zone (hub-PCB thermistor reading scalp-side temperature) remains the primary thermal guard. The firmware adds a three-channel aggregate thermal budget check:

```
P_aggregate_est = (I_PD1_A × K_PD1[zone][0]) +   // 660 nm irradiance estimate
                  (I_PD1_B × K_PD1[zone][1]) +   // 808 nm irradiance estimate
                  (I_PD1_C × K_PD1[zone][2])     // 1064 nm irradiance estimate

if (P_aggregate_est > PBM_AGGREGATE_IRRADIANCE_LIMIT_MW_CM2) {
    /* Throttle CH_C (1064 nm) first (lowest priority in T1 base protocol),
       then CH_B, preserving CH_A (660 nm) as last to be reduced) */
    np_pbm1064_throttle_cascade(zone, P_aggregate_est);
}
```

`PBM_AGGREGATE_IRRADIANCE_LIMIT_MW_CM2` = 600 mW/cm² (three-channel CW equivalent ceiling, mapped to NTC 38°C safety headroom before hardware throttle at 42°C). Value stored in Config partition; factory-adjustable via signed firmware config update.

---

## 6. Dose Metering — 1064 nm InGaAs Correction

### 6.1 Algorithm (per zone, per wavelength tick)

```c
/* Called at 10 Hz dose accumulation tick */
void np_pbm1064_dose_tick(uint8_t zone, np_pbm1064_session_t *s)
{
    uint16_t adc_pd1 = np_adc_read(PD1_CH[zone]);
    uint16_t adc_pd2 = np_adc_read(PD2_CH[zone]);     /* pin 19, PD2_CATHODE */

    /* Convert to irradiance using wavelength-specific calibration coefficients */
    float irr_pd1 = adc_pd1 * s->cal.K_PD1[zone][CH_C];   /* mW/cm² */
    float irr_pd2 = adc_pd2 * s->cal.K_PD2[zone][CH_C];

    /* PD1/PD2 ratio check: separate fouling (PD1↓, PD2 stable) from LED aging (both↓) */
    float ratio = (irr_pd2 > 0.5f) ? (irr_pd1 / irr_pd2) : 0.0f;
    float ratio_delta = ratio - s->cal.K_ratio_nom[zone][CH_C];

    if (ratio_delta < -NP_PBM_FOULING_THRESHOLD) {
        s->fouling_flag[zone] = true;       /* PDMS window fouling — SHDR log */
    } else if (irr_pd1 < s->baseline_pd1[zone][CH_C] * NP_PBM_AGING_THRESHOLD) {
        s->aging_flag[zone] = true;         /* LED aging — SHDR log */
    }

    /* Accumulate dose: J/cm² = mW/cm² × dt (0.1 s) × 1e-3 */
    s->dose_j_cm2[zone][CH_C] += irr_pd1 * 0.1f * 1e-3f;

    /* UHDR: irradiance sample stored in circular buffer for EDF+ export */
    np_uhdr_pbm_sample(zone, WAVELENGTH_1064, irr_pd1, irr_pd2);
}
```

### 6.2 PD1/PD2 ratio disambiguation

The same RISK-14 Option B algorithm used for base modules applies to smart modules, using 1064 nm-specific coefficients:

| Condition | PD1 behaviour | PD2 behaviour | Firmware action |
|-----------|--------------|--------------|----------------|
| PDMS fouling | Decreasing | Stable | SHDR: fouling flag. User prompt: clean optical window |
| LED aging | Both decreasing proportionally | Both decreasing | SHDR: aging flag. Dose correction applied from PD2 |
| Normal operation | Stable | Stable | No action |
| PD2 open-circuit | — | Reads near zero | SHDR: PD2 fault. Fall back to PD1-only dose metering |

---

## 7. T1 Session Preset Library — Three-Wavelength Extension

The existing 7 frequency presets (NP-FW base PBM) apply channel-uniformly. Smart modules add three-wavelength independence. New preset structure:

```c
typedef struct {
    uint8_t  freq_hz_A;      /* 660 nm channel frequency */
    uint8_t  duty_pct_A;     /* 660 nm duty cycle (max 25) */
    uint8_t  freq_hz_B;      /* 808 nm channel frequency */
    uint8_t  duty_pct_B;
    uint8_t  freq_hz_C;      /* 1064 nm channel frequency */
    uint8_t  duty_pct_C;
    uint8_t  cur_pct;        /* common current % of rated (80–100%) */
    char     name[32];
} np_pbm1064_preset_t;
```

### 7.1 Existing 7 presets mapped to smart module (backwards-compatible)

When running base-module presets on a smart module, channel B (808 nm) follows the same parameters as base module, channel C (1064 nm) defaults to the same frequency and duty as channel B unless overridden. This allows session protocols designed for base modules to run unchanged on smart modules.

| Preset | 660 nm (A) | 808 nm (B) | 1064 nm (C) | Rationale |
|--------|-----------|-----------|-------------|-----------|
| Gamma clarity (40 Hz) | 40 Hz, 25% | 40 Hz, 25% | 40 Hz, 25% | Full-depth 40 Hz entrainment |
| Alpha calm (10 Hz) | 10 Hz, 20% | 10 Hz, 20% | 10 Hz, 20% | Alpha band, all depths |
| Theta memory (6 Hz) | 6 Hz, 20% | 6 Hz, 20% | 6 Hz, 20% | Theta band, all depths |
| Sleep deep (2 Hz) | 2 Hz, 15% | 2 Hz, 15% | 2 Hz, 15% | Delta, all depths |
| Gamma+theta coupled | 40 Hz, 25% (F zones) / 6 Hz, 20% (P zones) | same per zone | same per zone | Coupled split-zone |
| Focus prime (20 Hz) | 20 Hz, 25% | 20 Hz, 25% | 20 Hz, 25% | Beta band |
| Vascular baseline (CW) | CW, 100% | CW, 100% | CW, 100% | Continuous wave all channels |

### 7.2 New presets exploiting three-wavelength independence

| Preset | 660 nm (A) | 808 nm (B) | 1064 nm (C) | Rationale |
|--------|-----------|-----------|-------------|-----------|
| Cortical gradient | 40 Hz, 25% | 20 Hz, 20% | 10 Hz, 15% | Frequency decreases with depth — matches cortical oscillatory gradient hypothesis (Yao et al. 2022 inspired) |
| Working memory (Yao protocol) | CW, 100% | CW, 100% | 10 Hz, 20% | Replicates UT Dallas right prefrontal 1064 nm protocol; adds 660+808 CW base; 10 Hz modulation |
| Deep alpha focus | CW, 100% | 10 Hz, 20% | 10 Hz, 20% | Surface CW + mid+deep 10 Hz; extends 810 nm alpha protocol to deeper cortical tissue |
| Prefrontal prime | 40 Hz, 25% | 40 Hz, 25% | 20 Hz, 20% | Frontal zones only: high-frequency surface + deeper 20 Hz; DLPFC working memory target |

---

## 8. T2 Combined 1064 nm + 1170 nm Cortical+Deep Session

### 8.1 Architecture

The 1064 nm zone modules run in the headset zone slots (smart module I2C path, this firmware). The 1170 nm deep PBM system (laser diodes, TEC stabilised, separate T2 hardware) runs under its existing firmware. The combined session coordinator (module `np_pbm1064_t2_combined`) synchronises both:

```
np_pbm1064_t2_combined_session_t {
    np_pbm1064_session_t    zone_session;     /* 1064 nm zone modules */
    np_t2_pbm1170_session_t laser_session;    /* 1170 nm T2 laser (existing API) */
    uint8_t                 active_zones;     /* bitmask of zones with smart modules */
    float                   dose_1064[5];     /* J/cm² per zone, 1064 nm */
    float                   dose_1170;        /* J/cm² total, 1170 nm (single beam) */
    uint32_t                session_tick_ms;
}
```

### 8.2 Session sequencing

```
T2_COMBINED_IDLE
  │ Both hardware present: smart modules detected in ≥1 zone, T2 1170 nm system ready
  ▼
T2_COMBINED_INIT
  │ 1. Initialise 1064 nm smart module protocol (§4, §5.2) for all active zones
  │ 2. Initialise 1170 nm laser system (existing API: np_t2_pbm1170_init())
  │ 3. Run baseline NTC temperature read for all zones
  │ 4. Safety MCU SPI: confirm both PBM_EN_L (zone modules) and LASER_EN_L (1170 nm) available
  ▼
T2_COMBINED_RAMP
  │ 1064 nm: 30 s ramp (current ramp per §5.2 CONFIG soft_start)
  │ 1170 nm: 60 s ramp (existing T2 laser ramp spec, longer due to laser safety margin)
  │ Both ramps run concurrently; dose accumulation begins at ramp start
  ▼
T2_COMBINED_STEADY
  │ All channels at preset parameters
  │ Thermal budget check every 1 s: aggregate NTC reads for all zones
  │   — if any zone NTC > 38°C: throttle 1170 nm dose rate first (deeper = slower surface heating)
  │   — if any zone NTC > 40°C: suspend 1170 nm, continue 1064 nm at 50% duty
  │   — if any zone NTC > 42°C: hardware NTC throttle fires (existing safety path), abort session
  │ Dose metering: both wavelengths logged every 10 Hz tick
  ▼
T2_COMBINED_RAMP_DOWN
  │ 1064 nm: 30 s ramp-down (CH_ENABLE bits cleared progressively)
  │ 1170 nm: 60 s ramp-down
  ▼
T2_COMBINED_COMPLETE
  │ Write UHDR session record (§9.1)
  │ Write SHDR record (§9.2)
```

### 8.3 T2 combined session presets

| Preset | 1064 nm zones | 1170 nm | Clinical target |
|--------|-------------|---------|-----------------|
| Cortical + subcortical (standard) | 10 Hz, 20%, all zones | 10 Hz, 20% | Full-depth alpha entrainment |
| Prefrontal focus (T2) | Prefrontal prime preset (§7.2) frontal zones only | 20 Hz, 15%, fixed beam | DLPFC working memory + subcortical modulation |
| Depression protocol | 10 Hz, 20%, all zones | CW, 50% | sLORETA-guided (DLPFC target — coordinates with NP-FW-HD-001) |
| Vascular deep (CW) | CW, 100%, all zones | CW, 100% | Maximum penetration, vascular baseline |

---

## 9. Data Architecture

### 9.1 UHDR records (user biology — user-owned, AES-256 encrypted)

All UHDR writes go to the UHDR eMMC partition per NP-FW-EMMC-001 Rev A. NeuroPulse never holds the decryption key.

| Field | Type | Source |
|-------|------|--------|
| session_id | uint64 (CSPRNG) | session start |
| timestamp_start | uint64 (UTC ms) | session start |
| duration_ms | uint32 | session end |
| preset_id | uint8 | session config |
| zone_active_mask | uint8 (bitmask) | module detection |
| smart_module_mask | uint8 (bitmask) | module detection |
| dose_j_cm2[5][3] | float[5][3] | dose metering: zone × wavelength |
| irr_pd1_series[5][3][] | float[10 Hz circular] | PD1 irradiance time series per zone per wavelength |
| irr_pd2_series[5][3][] | float[10 Hz circular] | PD2 irradiance time series |
| ntc_series[5][] | int8[1 Hz] | per-zone NTC temperature time series |
| throttle_events[] | event record | timestamps and reasons for any thermal throttle |

### 9.2 SHDR records (device health — NeuroPulse fleet analytics)

Per NP-FW-EMMC-001 Rev A §7. No user biology. Linked to device ID only.

| Field | Type | Notes |
|-------|------|-------|
| session_count_1064 | uint32 | cumulative smart module sessions (no timestamp) |
| pd1_pd2_ratio[5][3] | float | PD1/PD2 ratio at end of session per zone per wavelength |
| fouling_flag[5] | bool | PDMS fouling detected per zone |
| aging_flag[5] | bool | LED aging detected per zone |
| driver_fault_count[5] | uint8 | cumulative I2C driver fault events per zone slot |
| ntc_peak[5] | int8 | peak NTC per zone during session (no timestamp, no biology) |
| cal_source[5] | enum | FACTORY / DEFAULT per zone (for fleet calibration quality tracking) |
| combined_session_count | uint32 | T2 combined 1064+1170 session count (T2 only) |

---

## 10. FAI Test Specifications

| Test ID | Description | Pass criteria |
|---------|-------------|---------------|
| FAI-SM-01 | Smart module ADC detection: insert smart module in each of 5 slots; verify ADC reads < 1100 counts in ≥ 3/3 debounce reads | All 5 slots PASS |
| FAI-SM-02 | I2C bus probe: insert smart module, verify I2C ACK from address 0x30 within 5 ms; repeat 50 insertion cycles | PASS rate ≥ 49/50 |
| FAI-SM-03 | Base module backwards compatibility: insert base module (ZM-01..ZM-05); verify I2C bus stays disabled, direct PWM drive unchanged | All 5 base module variants PASS |
| FAI-SM-04 | Three-channel simultaneous operation: enable CH_A+CH_B+CH_C on smart module; verify all 3 wavelengths present via calibrated optical power meter | ±10% of programmed irradiance per channel |
| FAI-SM-05 | Duty cycle ceiling: attempt to program DUTY_C = 0x40 (50%); verify hub firmware clamps to 0x32 (25%) | Clamped in all cases |
| FAI-SM-06 | InGaAs PD dose metering: expose smart module to calibrated 1064 nm reference source at 100 mW/cm²; verify J/cm² accumulation error ≤ ±15% over 10 min | PASS |
| FAI-SM-07 | PD1/PD2 ratio fouling detection: partially obscure PD1 PDMS window; verify fouling_flag set within 30 s, PD2 remains stable | PASS |
| FAI-SM-08 | PD1/PD2 ratio aging detection: attenuate both PD1 and PD2 equally; verify aging_flag set, fouling_flag not set | PASS |
| FAI-SM-09 | Combined thermal throttle: run three channels at 25% duty until aggregate irradiance estimate exceeds threshold; verify CH_C duty reduces first | PASS |
| FAI-SM-10 | T2 combined session (software): run T2_COMBINED state machine on bench against stub np_t2_pbm1170 API; verify sequencing, dose logging, thermal throttle priority | All state transitions correct |
| FAI-SM-11 | UHDR/SHDR data routing: complete session; verify dose_j_cm2 present in UHDR, PD1/PD2 ratios and fouling/aging flags present in SHDR, no user biology in SHDR | Automated partition-boundary test PASS |

FAI-SM-01 through FAI-SM-11 are software-level tests passable on bench. FAI-SM-04, FAI-SM-06, FAI-SM-07 require calibrated optical bench (1064 nm reference source); blocking for hardware FAI gate on Issue #53.

---

## 11. Platform HAL Stubs

The following HAL functions require implementation by the firmware platform team. These are the only external dependencies of the `pbm_1064nm` library.

| Function | Signature | Notes |
|----------|-----------|-------|
| OI-PBM-01 | `uint16_t np_adc_read_pd(uint8_t zone, uint8_t pd_channel)` | Reads TIA ADC for PD1 or PD2 at given zone |
| OI-PBM-02 | `np_err_t np_i2c_write(uint8_t slot, uint8_t addr, uint8_t reg, uint8_t val)` | I2C write to on-module driver IC for given zone slot |
| OI-PBM-03 | `np_err_t np_i2c_read(uint8_t slot, uint8_t addr, uint8_t reg, uint8_t *val)` | I2C read |
| OI-PBM-04 | `void np_pbm_safety_mcu_enable(uint8_t zone, bool en)` | Assert/deassert PBM_EN_L GPIO via SPI to safety MCU |
| OI-PBM-05 | `void np_uhdr_pbm_sample(uint8_t zone, uint16_t wl_nm, float irr_pd1, float irr_pd2)` | UHDR circular buffer write (per NP-FW-EMMC-001 Rev A §6) |
| OI-PBM-06 | `np_err_t np_config_read_pbm_cal(uint8_t zone, np_pbm_cal_t *out)` | Read calibration coefficients from Config partition |
| OI-PBM-07 | `np_err_t np_t2_pbm1170_init(np_t2_pbm1170_session_t *s, np_t2_pbm1170_preset_t preset)` | T2 laser system init (existing T2 firmware API stub) |
| OI-PBM-08 | `float np_t2_pbm1170_get_dose_j_cm2(void)` | T2 laser dose query (T2 firmware API) |

---

## 12. Open Items

| ID | Description | Owner | Blocking |
|----|-------------|-------|---------|
| OI-PBM-01 | Implement `np_adc_read_pd` HAL stub (shared with base module ADC path) | FW Platform | FAI-SM-06 |
| OI-PBM-02 | Implement `np_i2c_write` / `np_i2c_read` HAL for 5 independent LPI2C3 GPIO mux slots | FW Platform | FAI-SM-02 |
| OI-PBM-03 | `np_pbm_safety_mcu_enable`: confirm safety MCU SPI command encoding for smart module slots | FW Platform + HW | FAI-SM-04 |
| OI-PBM-04 | Factory calibration procedure: define integrating sphere reference protocol for K_PD1_1064, K_PD2_1064 coefficients; write to Config partition at manufacture | Manufacturing | FAI-SM-06 |
| OI-PBM-05 | `PBM_AGGREGATE_IRRADIANCE_LIMIT_MW_CM2` value (600 mW/cm²): obtain regulatory opinion confirmation that three-channel CW aggregate is within IEC 60601-2-10 and IEC 62471 (must be included in RISK-03 regulatory opinion scope expansion — see Issue #56) | Regulatory | FAI-SM-09 (advisory) |
| OI-PBM-06 | T2 HAL stubs OI-PBM-07 + OI-PBM-08: await T2 1170 nm firmware API definition | T2 FW | FAI-SM-10 |

---

*NP-FW-PBM1064-001 Rev A. Status: Baselined 2026-05-12. Next revision triggered by: on-module driver IC selection (Issue #54 hardware), InGaAs PD selection, or regulatory opinion update (Issue #56).*
