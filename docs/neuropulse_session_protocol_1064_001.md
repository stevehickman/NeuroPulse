# NP-SES-1064-001 Rev A — 1064 nm Multi-Wavelength Session Protocol
**Project:** NeuroPulse  
**Document number:** NP-SES-1064-001  
**Revision:** A  
**Date:** 2026-05-12  
**Status:** Baselined  
**Author:** Firmware Engineering / Clinical  
**Related issues:** GitHub Issue #53 (firmware), Issue #54 (hardware), Issue #55 (T2 combined session)

---

## 1. Scope

This document specifies the session protocol requirements for the 1064 nm smart zone module, covering:

- Session descriptor format extensions for three-wavelength PBM
- T1 three-wavelength (660 nm + 808 nm + 1064 nm) session lifecycle
- T2 combined 1064 nm + 1170 nm cortical+deep session lifecycle
- Per-wavelength dose tracking, safety enforcement, and closed-loop EEG adaptation
- UHDR/SHDR data routing for all new session data elements
- Session signing and integrity (extending existing CSPRNG session protocol)

**Relationship to NP-FW-PBM1064-001 Rev A:** That document specifies the firmware module (hardware interface, I2C protocol, dose metering algorithm). This document specifies the session-level abstraction: how presets are selected, signed, delivered to the hub, adapted in real time by EEG, and logged.

---

## 2. Session Descriptor Extension

All NeuroPulse session protocols are cryptographically signed by the app and rejected by the hub if the signature is invalid (CLAUDE.md §4.2 CSPRNG signing). The session descriptor schema is extended to support smart modules:

### 2.1 New session descriptor fields

```c
typedef struct {
    /* Existing fields (unchanged) */
    uint32_t    schema_version;          /* incremented to 4 for smart module support */
    uint8_t     session_uuid[16];        /* CSPRNG */
    uint8_t     modality_mask;           /* bitmask of active modalities */
    /* ... all existing fields ... */

    /* New: smart module PBM extension */
    uint8_t     smart_module_mask;       /* bitmask: which of 5 zone slots have smart modules */
    np_pbm1064_preset_t pbm_preset[5];  /* per-zone preset (660/808/1064 nm) */
    bool        three_wl_enable;         /* true = use 3-channel smart module path */
    bool        t2_combined_enable;      /* true = coordinate with 1170 nm T2 laser */
    np_t2_pbm1170_preset_t t2_preset;   /* T2 laser preset (T2 only, ignored on T1) */

    /* Session signature (Ed25519, over entire descriptor) */
    uint8_t     signature[64];
} np_session_descriptor_v4_t;
```

Schema version 4 descriptors are rejected by hub firmware < NP-FW-PBM1064-001 base version. Schema version 3 descriptors (base module sessions) continue to be accepted by all firmware versions.

### 2.2 Preset selection and validation

The app selects a preset from the preset library (§7 of NP-FW-PBM1064-001) and writes it into `pbm_preset[zone]` for each active zone. The hub firmware validates:

1. `duty_pct_A/B/C ≤ 25` for all channels (hardware ceiling)
2. `freq_hz_A/B/C` in range 0–100 (0 = CW)
3. `cur_pct` in range 80–100
4. `smart_module_mask` matches detected smart modules at session start (mismatch → session abort with user notification)
5. Ed25519 signature valid over entire descriptor (hub rejects unsigned or corrupted sessions)

---

## 3. T1 Three-Wavelength Session Lifecycle

### 3.1 Pre-session checklist

| Check | Pass criteria | Fail action |
|-------|--------------|-------------|
| Smart module detected in each armed zone | ADC < 1100 counts, I2C ACK | Session abort, bone-conduction error cue |
| InGaAs PD calibration coefficients loaded | Config partition read success | Use DEFAULT coefficients, SHDR flag cal_source=DEFAULT |
| Driver IC STATUS register: ready=1, no fault flags | All fault flags clear | Session abort, user notification with fault detail |
| Baseline NTC read | All armed zones < 33°C | Warn user, start only if user confirms |
| Session descriptor signature | Ed25519 PASS | Hard reject — no override |
| Safety MCU SPI heartbeat | ACK within 200 ms | Session abort |

### 3.2 Session phases

```
APP_SESSION_START
  │ Signed session descriptor uploaded to hub (Mode 2 Programming, < 5 s)
  │ Hub validates descriptor (§2.2)
  ▼
HUB_INIT (< 2 s)
  │ Load calibration coefficients from Config partition
  │ Detect and initialise smart modules (§4, NP-FW-PBM1064-001)
  │ Safety MCU SPI: confirm PBM_EN_L available for all armed zones
  ▼
RAMP_UP (30 s)
  │ All three channels ramp from 0 to target current via driver IC soft-start
  │ Dose accumulation begins at tick 0 (includes ramp-up in J/cm² total)
  │ EEG session begins concurrently if EEG modality active (closed-loop adaptation, §3.4)
  ▼
STEADY_STATE (session duration)
  │ Real-time dose accumulation (10 Hz PD tick)
  │ Thermal budget monitoring (1 Hz NTC read, §5.4 of NP-FW-PBM1064-001)
  │ EEG-adaptive frequency update (§3.4) — closed-loop
  │ Safety MCU watchdog heartbeat every 200 ms (existing architecture, unchanged)
  │ App display: per-zone per-wavelength irradiance and cumulative J/cm² (Mode 1 Connected)
  ▼
RAMP_DOWN (30 s)
  │ All channels ramped to zero
  │ Dose accumulation continues through ramp-down
  ▼
SESSION_COMPLETE
  │ Final dose totals written to UHDR
  │ SHDR record written (device health fields only)
  │ Driver IC: CH_ENABLE = 0x00 (all channels disabled)
  │ Safety MCU SPI: PBM_EN_L deasserted for all zones
  │ Mode 4 Download available: EDF+ + session logs to app on USB-C reconnect
```

### 3.3 Session dose limits (per-wavelength, per-zone)

| Wavelength | Max single-session dose | Basis |
|-----------|------------------------|-------|
| 660 nm | 60 J/cm² | Conservative extension of Hamblin 2017 meta-analysis CW dose range |
| 808 nm | 60 J/cm² | Same |
| 1064 nm | 36 J/cm² | 60% of 660/808 nm limit — lower melanin absorption at 1064 nm allows higher tissue penetration; conservative pending dedicated RCT dose-response data |

These are software limits enforced by hub firmware. When the cumulative dose for any wavelength at any zone reaches 90% of its limit, the app is notified and displays a dose-approaching warning. At 100%, that channel is disabled for the remainder of the session; the session continues on remaining channels.

### 3.4 EEG-adaptive frequency closed loop — three-wavelength extension

The existing closed-loop EEG-adaptive PBM frequency algorithm (running on the main processor at 500 Hz EEG sample rate, 24-bit ADS1299) adapts stimulation frequency to the user's dominant EEG band. With three-wavelength smart modules, the adaptation has an additional degree of freedom:

**New adaptive rule:** The EEG adaptation algorithm continues to set a single target frequency. For smart modules, the firmware maps this frequency to all three channels uniformly (maintaining the backwards-compatible behaviour), unless the session descriptor specifies `cortical_gradient_adapt = true`, in which case:

- CH_A (660 nm, ~surface): adapts to the EEG-derived frequency (fastest response, shallowest tissue)
- CH_B (808 nm, ~medium depth): adapts to 0.5× the EEG frequency (smoother, mid-depth)
- CH_C (1064 nm, ~deep cortical): adapts to 0.25× the EEG frequency (slowest modulation, deepest tissue)

The rationale is that cortical oscillation frequencies reported by EEG represent primarily superficial layer activity; deeper layers are believed to oscillate at lower frequencies. This gradient approach is experimental and labelled in the app as "Research mode — not validated in RCT."

All closed-loop adaptations are logged in UHDR: EEG band power per adaptation event, resulting frequency per channel.

---

## 4. T2 Combined 1064 nm + 1170 nm Cortical+Deep Session Lifecycle

### 4.1 Hardware prerequisites

| Requirement | Source |
|-------------|--------|
| Smart modules in ≥1 zone slot | Smart module detection (§3.1) |
| T2 1170 nm laser system powered and TEC at setpoint | Existing T2 firmware (np_t2_pbm1170 API) |
| T2 21-ch qEEG cap connected | T2 hardware requirement |
| Safety MCU: both LASER_EN_L and PBM_EN_L available | SPI confirm at session start |

If the 1170 nm system is absent (T1 hardware), `t2_combined_enable = true` in the descriptor is a session validation error → abort.

### 4.2 Dose targeting

The T2 combined session targets two independent depth profiles simultaneously:

| Depth | System | Target tissue | Typical preset |
|-------|--------|--------------|----------------|
| Surface + cortical (~0–6 cm) | 1064 nm zone modules | Cortex, grey matter | 10–40 Hz, 20–25% duty |
| Subcortical (~35–40 mm from surface) | 1170 nm laser | White matter tracts, subcortical structures | 10 Hz or CW, 15–20% duty |

Doses are independently metered and logged. There is no dose coupling between the two systems beyond shared thermal budget (§4.3).

### 4.3 Shared thermal budget management

Both systems deliver optical power that deposits heat in scalp and underlying tissue. The combined thermal budget rule (§8.2, NP-FW-PBM1064-001) applies: when NTC reads indicate heating above thermal thresholds, the 1170 nm system is throttled first (its heat deposition is deeper and diffuses more slowly to the scalp surface NTC), followed by 1064 nm channel C, then B.

**Throttle priority order:** 1170 nm (throttle first) → 1064 nm CH_C → 1064 nm CH_B → 1064 nm CH_A (never throttle in T2 combined session unless NTC > 42°C hardware limit)

### 4.4 sLORETA coordination (depression protocol)

When the T2 combined session is run with the depression protocol preset, the session orchestrator coordinates with the sLORETA HD-tDCS system (NP-FW-HD-001 Rev A):

1. sLORETA identifies DLPFC hypoactivity from the 21-ch resting-state qEEG (first 128 s of session)
2. sLORETA HD-tDCS positions 4×1 ring montage on DLPFC (this is a tDCS function, separate system)
3. The 1064 nm zone modules covering the frontal zones (ZM-01 Frontal Left, ZM-02 Frontal Right) are set to the Prefrontal Prime preset (40 Hz, 25% duty)
4. The 1170 nm laser is aimed at the sLORETA-identified peak DLPFC MNI coordinate
5. The three systems (1064 nm PBM + 1170 nm PBM + HD-tDCS) run concurrently under their independent safety MCU interlocks

This is the most complex multi-modal T2 session. The session descriptor must specify all three system presets. The hub firmware session orchestrator (np_pbm1064_t2_combined + np_sloreta_hdtdcs) shares a common session tick and UHDR record.

---

## 5. UHDR/SHDR Classification — All New Session Data Elements

Per NP-FW-EMMC-001 Rev A §12 boundary resolution rule: when in doubt → UHDR.

| Data element | Classification | Rationale |
|-------------|---------------|-----------|
| Per-zone per-wavelength J/cm² dose | UHDR | Tells us something about this person's tissue |
| PD1 irradiance time series (10 Hz) | UHDR | Reflects tissue optical properties (user biology) |
| PD2 irradiance time series (10 Hz) | UHDR | Same |
| EEG-adaptive frequency events (per-channel) | UHDR | Derived from EEG (user biology) |
| NTC temperature time series during session | UHDR | User physiology |
| Throttle events during session (timestamps, reason) | UHDR | Contains session timing |
| PD1/PD2 ratio at session end | **SHDR** | Device metric only — no user biology in the ratio alone |
| Fouling flag per zone | **SHDR** | Device condition |
| Aging flag per zone | **SHDR** | Device condition |
| Driver IC fault count per zone | **SHDR** | Device condition |
| Peak NTC per session (no timestamp) | **SHDR** | Device thermal metric; no timestamp, no biology |
| Cal source per zone (FACTORY/DEFAULT) | **SHDR** | Manufacturing quality indicator |
| Combined session count (unsigned int, no timestamp) | **SHDR** | Device usage counter |
| 1170 nm dose (J/cm²) | UHDR | Tissue-delivered dose — user biology |
| sLORETA target MNI coordinate | UHDR | Individual brain anatomy data |

---

## 6. App Display Requirements

### 6.1 Session real-time display (Mode 1 Connected)

The app must display the following during a smart module session:

- Per zone: irradiance (mW/cm²) for each active wavelength, colour-coded (red=660 nm, amber=808 nm, purple=1064 nm)
- Per zone: cumulative dose (J/cm²) per wavelength, with dose limit progress bar
- Per zone: NTC temperature indicator (green < 36°C, amber 36–39°C, red > 39°C)
- Combined dose readout: total J/cm² summed across all active wavelengths and zones
- For T2 combined: 1170 nm dose separately displayed (different depth tier)
- EEG-adaptive frequency: current target frequency per channel (if cortical gradient mode active: three separate values displayed)
- Thermal throttle notification: banner + which channel was reduced and why

### 6.2 Session summary screen (post-session)

- Per-zone per-wavelength dose received (J/cm²) as bar chart
- Depth penetration tier illustration: 660 nm (surface) / 808 nm (mid) / 1064 nm (cortical) / 1170 nm (deep, T2 only)
- Fouling or aging alerts (from SHDR-flagged events): "Optical window may need cleaning in zone 2" or "LED aging detected in zone 4 — consider ordering replacement module"
- One-tap order link if consumable or module replacement suggested

### 6.3 Module management screen

- Zone slot inventory: which slots have base modules, which have smart modules, which are empty
- Per-slot: smart module session count (from SHDR), calibration status (FACTORY/DEFAULT)
- Upgrade prompt: for users with base modules, show smart module upgrade option with 1064 nm evidence summary (Yao et al. 2022)

---

## 7. Clinical Evidence Basis for 1064 nm Session Protocols

| Protocol | Primary evidence | Effect size | NeuroPulse note |
|----------|-----------------|-------------|-----------------|
| Working memory (Yao protocol, right prefrontal, 1064 nm) | Yao et al., Science Advances 2022 (UT Dallas/Harvard); EEG CDA confirmation | Cohen's d not reported in SA 2022; significant at p < 0.01 for CDA amplitude | Direct replication of UT Dallas right prefrontal, 10 Hz, 1064 nm; add 660+808 CW base layer |
| Multi-depth 10 Hz (cortical + deep) | No combined RCT exists — no such claim made | — | Session offered as protocol; combination RCT is a research priority (Issue #56 bibliography extension) |
| Prefrontal 40 Hz (smart module) | GENUS evidence base (Tsai/MIT, 40 Hz audiovisual); extrapolated to PBM frequency — no direct RCT | Pending | Labelled in app as "Research mode — not validated in RCT" |

**FTC claims substantiation note:** The 1064 nm-specific claims in the app and marketing must be covered by the RISK-03 regulatory opinion extension (Issue #56). Until that opinion is received, no 1064 nm irradiance or dose claim may appear in any public material. App display of real-time dose to the user (private, on-device) is not a marketing claim and is not affected by RISK-03.

---

## 8. Open Items

| ID | Description | Blocking |
|----|-------------|---------|
| OI-SES-01 | Single per-session dose limits (36/60 J/cm²) to be reviewed against dedicated 1064 nm safety literature; update Config partition defaults when reviewed | FAI-SM-06 advisory |
| OI-SES-02 | `cortical_gradient_adapt` algorithm (§3.4): labelled Research mode — pending clinical input from SAB or researcher partners (Issue #27, Issue #28) | Not blocking for launch; feature gated in app |
| OI-SES-03 | T2 depression protocol sLORETA coordination (§4.4): requires T2 prototype bench; coordinate with NP-FW-HD-001 FAI-HD01 gate (Issue #23) | T2 prototype required |
| OI-SES-04 | App display requirements (§6) must be reflected in NP-APP-ROADMAP-001; update Issue #51 (core iOS app) DoD with smart module session display | Not blocking for firmware |
| OI-SES-05 | Bibliography extension: add 1064 nm evidence section to neuropulse_bibliography.docx (Yao et al. 2022 + follow-on UT Dallas publications) | Issue #56 |

---

*NP-SES-1064-001 Rev A. Status: Baselined 2026-05-12. Next revision triggered by: dose limit review (OI-SES-01), cortical gradient algorithm RCT validation, or T2 combined session hardware FAI completion.*
