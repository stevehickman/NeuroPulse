# NPPS Language Reference

**NeuroPulse Protocol Script (NPPS)** is the text format used to define, share, and store NeuroPulse session protocols. Files use the `.npps` extension.

---

## Contents

1. [Basics](#1-basics)
2. [Value types](#2-value-types)
3. [Protocol block](#3-protocol-block)
4. [Modalities](#4-modalities)  
   4.1 [PBM Transcranial](#41-pbm-transcranial)  
   4.2 [PBM Intranasal](#42-pbm-intranasal)  
   4.3 [EEG Neurofeedback](#43-eeg-neurofeedback)  
   4.4 [BES / tACS](#44-bes--tacs)  
   4.5 [tDCS](#45-tdcs)  
   4.6 [VNS + HRV](#46-vns--hrv)  
   4.7 [Audio Entrainment](#47-audio-entrainment)  
   4.8 [Visual Stimulation](#48-visual-stimulation)  
   4.9 [qEEG 21-Channel](#49-qeeg-21-channel)  
   4.10 [TMS](#410-tms)  
   4.11 [Deep PBM 1170nm](#411-deep-pbm-1170-nm)  
   4.12 [Clinical tACS](#412-clinical-tacs)  
   4.13 [HD-tDCS](#413-hd-tdcs)  
   4.14 [Cervical VNS](#414-cervical-vns)  
   4.15 [Vibrotactile 40Hz](#415-vibrotactile-40-hz)  
5. [Intervals](#5-intervals)
6. [Composite block](#6-composite-block)
7. [Limits block](#7-limits-block)
8. [Multi-block files](#8-multi-block-files)
9. [Grammar summary](#9-grammar-summary)

---

## 1. Basics

### Encoding and extension

NPPS files are UTF-8 text with the `.npps` extension.

### Comments

Lines beginning with `#`, or the portion of a line from `#` onward, are ignored.

```
# This entire line is a comment.
duration: 20m    # inline comment
```

### Whitespace

Indentation and blank lines are insignificant. Newlines act as field separators but are otherwise interchangeable with spaces.

### Case sensitivity

All keywords, field names, and enumerated values are **case-sensitive** and lowercase (e.g. `protocol`, `pbm_transcranial`, `sinusoidal`). String values like names, descriptions, and tags are not interpreted by the parser.

---

## 2. Value types

### Strings

Quoted with double quotes. Backslash escapes: `\"`, `\\`, `\n`, `\t`.

```
name: "Gamma Focus"
description: "40Hz entrainment with \"GENUS\" protocol."
```

Unquoted identifiers are also accepted where a string is expected (useful for enum values):

```
zones: all        # same as zones: "all"
waveform: sinusoidal
```

### Numbers

Integer or floating-point. Optionally followed by a **unit suffix** that the lexer attaches to the token:

| Suffix | Meaning | Example |
|--------|---------|---------|
| `Hz`   | Frequency in hertz | `40Hz` |
| `%`    | Percentage (0–100) | `80%` |
| `mA`   | Current in milliamps | `1.5mA` |
| `s`    | Duration in seconds | `30s` |
| `m`    | Duration in minutes | `20m` |

Unit suffixes are cosmetic on most numeric fields; the parser reads the raw number. **Duration fields** (`duration`, `start`, `interval_on`, `interval_off`) use the suffix to convert minutes to seconds automatically.

```
duration: 20m        # stored as 1200 seconds
interval_on: 90s     # stored as 90 seconds
intensity: 80%       # stored as 80
frequency: 40Hz      # stored as 40
```

Negative numbers are supported: `-5`.

### Booleans

```
closed_loop: true
eeg_adaptive: false
```

### Arrays

Comma-separated values inside `[ ]`. Trailing commas are not required. Newlines inside arrays are allowed.

```
tags: [focus, gamma, 40Hz, cognitive]
electrode_pairs: [["Fp1", "P3"], ["F3", "P4"]]
allowed_bands: [alpha, theta, gamma]
```

Tag arrays accept unquoted identifiers and keywords as elements.

---

## 3. Protocol block

A protocol block defines a single session. The protocol name is declared inline in the block header:

```
protocol "Gamma Focus" {
    ...
}
```

### Metadata fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `id` | string | auto UUID | Stable UUID. Present in predefined files. When present, marks the protocol as predefined. |
| `description` | string | `""` | Human-readable description. |
| `author` | string | `"NeuroPulse"` | Creator name. |
| `version` | string | `"1.0"` | Semantic version. Increment when protocol content changes. |
| `readonly` | bool | `false` | Prevents editing or deletion in the app. |
| `tags` | string array | `[]` | Freeform category labels. |
| `duration` | duration | `20m` | Fixed session length. |
| `interval_count` | int | — | Alternative to `duration`: session runs for N modality intervals. |

### Modality blocks

Modalities follow the metadata fields, each as a typed block (see §4):

```
protocol "Gamma Focus" {
    id: "10000001-0000-0000-0000-000000000000"
    description: "40Hz multi-modal entrainment."
    author: "NeuroPulse"
    version: "1.0"
    readonly: true
    tags: [focus, gamma, 40Hz]
    duration: 20m

    pbm_transcranial {
        intensity: 80%
        frequency: 40Hz
        duty_cycle: 25%
        zones: all
        wavelength: 660_808nm
    }

    eeg_neurofeedback {
        band: gamma
        channels: all
        closed_loop: true
    }
}
```

---

## 4. Modalities

Each modality is a typed block. The block keyword is the modality type:

```
pbm_transcranial {
    intensity: 80%
    frequency: 40Hz
    ...
}
```

### Field aliases

The following short names are accepted anywhere and map to the canonical name:

| Short alias | Canonical name |
|-------------|---------------|
| `frequency` | `frequency_hz` |
| `duty_cycle` | `duty_cycle_percent` |
| `closed_loop` | `closed_loop_enabled` |
| `binaural_hz` | `binaural_beats_hz` |
| `isochronic_hz` | `isochronic_tones_hz` |
| `noise` | `noise_type` |
| `volume` | `volume_percent` |
| `breathing_rate` | `resonance_breathing_rate` |
| `ramp` | `ramp_seconds` |
| `emdr_cadence` | `emdr_cadence_hz` |

The `intensity` alias is context-dependent: it maps to `intensity_percent` for optical modalities (`pbm_transcranial`, `pbm_intranasal`, `visual_stimulation`), and `intensity_milliamps` for electrical modalities (`bes_tacs`, `tdcs`, `vns_hrv`, `clinical_tacs`, `hd_tdcs`, `cervical_vns`, `tms`). For `pbm_deep_1170nm` use `intensity_mw_cm2:` directly; for `vibrotactile_40hz` use `intensity_g:` directly.

---

### 4.1 PBM Transcranial

Photobiomodulation via scalp-facing LED zones.

| Field | Canonical | Type | Values |
|-------|-----------|------|--------|
| `intensity` | `intensity_percent` | number | 0–100 |
| `frequency` | `frequency_hz` | number | 0 (CW) or 0.5–100 |
| `duty_cycle` | `duty_cycle_percent` | number | 1–25 (firmware max) |
| `zones` | `zones` | string | `all` `front` `rear` `custom` |
| `custom_zones` | `custom_zones` | int array | zone indices, e.g. `[0,1]` |
| `wavelength` | `wavelength` | string | `660_808nm` `1064nm` `660_808_1064nm` |

```
pbm_transcranial {
    intensity: 80%
    frequency: 40Hz
    duty_cycle: 25%
    zones: all
    wavelength: 660_808nm
}
```

`frequency: 0` (or `0Hz`) selects continuous-wave (CW) mode.  
`zones: custom` requires `custom_zones: [0, 1, 2]` (zone indices 0–4 front-to-rear).

---

### 4.2 PBM Intranasal

Bilateral intranasal probe.

| Field | Canonical | Type | Values |
|-------|-----------|------|--------|
| `intensity` | `intensity_percent` | number | 0–100 |
| `frequency` | `frequency_hz` | number | 0 (CW) or 0.5–100 |
| `duty_cycle` | `duty_cycle_percent` | number | 1–25 |

```
pbm_intranasal {
    intensity: 60%
    frequency: 40Hz
    duty_cycle: 25%
    interval_on: 15m
    interval_off: 15m
    repeat: 1
}
```

---

### 4.3 EEG Neurofeedback

Closed-loop neurofeedback via the EEG electrode array.

| Field | Canonical | Type | Values |
|-------|-----------|------|--------|
| `band` | `band` | string | `delta` `theta` `alpha` `beta` `gamma` `alpha_theta` `gamma_theta` |
| `channels` | `channels` | string | `all` `front` `central` `rear` |
| `custom_channels` | `custom_channels` | string array | electrode labels, e.g. `["Cz", "Pz"]` |
| `closed_loop` | `closed_loop_enabled` | bool | `true` `false` |

```
eeg_neurofeedback {
    band: gamma
    channels: all
    closed_loop: true
}
```

---

### 4.4 BES / tACS

Brainwave Entrainment Stimulation (consumer name) / transcranial alternating current stimulation.

| Field | Canonical | Type | Values |
|-------|-----------|------|--------|
| `intensity` | `intensity_milliamps` | number | 0–1 (T1), 0–4 (T2 clinical) |
| `frequency` | `frequency_hz` | number | 0.5–40 |
| `waveform` | `waveform` | string | `sinusoidal` `square` `triangular` |

```
bes_tacs {
    frequency: 40Hz
    intensity: 0.8mA
    waveform: sinusoidal
    interval_on: 30s
    interval_off: 30s
}
```

---

### 4.5 tDCS

Cortical Priming Stimulation (consumer name) / transcranial direct current stimulation.

| Field | Canonical | Type | Values |
|-------|-----------|------|--------|
| `intensity` | `intensity_milliamps` | number | 0.1–2.0 |
| `electrode_pairs` | `electrode_pairs` | array of `[anode, cathode]` pairs | 10-20 electrode labels |
| `ramp` | `ramp_seconds` | number | default 30 |

```
tdcs {
    intensity: 1.5mA
    electrode_pairs: [["F3", "Fp2"]]
    ramp: 30s
    interval_on: 20m
    interval_off: 20m
    repeat: 1
}
```

The safety MCU enforces the 40 µC/cm² charge density limit regardless of script values. The 30 s ramp is also hardware-enforced; the `ramp` field sets the firmware target.

---

### 4.6 VNS + HRV

Auricular vagus nerve stimulation with HRV biofeedback.

| Field | Canonical | Type | Values |
|-------|-----------|------|--------|
| `intensity` | `intensity_milliamps` | number | 0.1–2.0 |
| `frequency` | `frequency_hz` | number | 1–25 |
| `hrv_protocol` | `hrv_protocol` | string | `standalone` `tavns_sync` `eeg_biofeedback` `pbm_combined` |
| `breathing_rate` | `resonance_breathing_rate` | number | 4.0–7.0 (breaths/min) |

```
vns_hrv {
    frequency: 25Hz
    intensity: 1.5mA
    hrv_protocol: tavns_sync
    breathing_rate: 6.0
}
```

**HRV protocol modes:**

| Value | Description |
|-------|-------------|
| `standalone` | Resonance breathing pacer only; coherence score displayed |
| `tavns_sync` | VNS pulses gated to inspiration phase (PPG R-R based) |
| `eeg_biofeedback` | Dual HRV + EEG biofeedback; pacer rate adapts to alpha/theta |
| `pbm_combined` | HRV coherence training concurrent with PBM (RCT protocol) |

---

### 4.7 Audio Entrainment

Over-ear planar magnetic + bone conduction.

| Field | Canonical | Type | Values |
|-------|-----------|------|--------|
| `binaural_hz` | `binaural_beats_hz` | number (optional) | 0.5–100 |
| `isochronic_hz` | `isochronic_tones_hz` | number (optional) | 0.5–100 |
| `noise` | `noise_type` | string (optional) | `pink` `brown` `none` |
| `carrier_hz` | `carrier_hz` | number | Hz of carrier tone for binaural beats |
| `volume` | `volume_percent` | number | 0–100 |
| `eeg_adaptive` | `eeg_adaptive` | bool | Adjust frequency in real time based on EEG |
| `bone_conduction_pacer` | `bone_conduction_pacer` | bool | Use bone conduction for breathing pacer cue |

Optional fields (`binaural_hz`, `isochronic_hz`, `noise`) may be omitted when not needed. `noise: none` is also accepted and has the same effect as omitting the field.

```
audio_entrainment {
    binaural_hz: 10Hz
    noise: pink
    carrier_hz: 440Hz
    volume: 60%
    eeg_adaptive: true
    bone_conduction_pacer: false
}
```

```
audio_entrainment {
    isochronic_hz: 40Hz
    binaural_hz: 40Hz
    carrier_hz: 440Hz
    volume: 65%
    eeg_adaptive: true
    bone_conduction_pacer: false
}
```

---

### 4.8 Visual Stimulation

108 micro-LEDs per lens (660 + 808–830 nm) with EMDR and Mode F support.

| Field | Canonical | Type | Values |
|-------|-----------|------|--------|
| `intensity` | `intensity_percent` | number | 0–100 |
| `frequency` | `frequency_hz` | number | 0–100 (0 = off / Mode F) |
| `mode` | `mode` | string | `binocular` `emdr` `mode_f` |
| `emdr_cadence` | `emdr_cadence_hz` | number | L/R alternation rate in Hz |
| `enable_mode_f` | `enable_mode_f` | bool | Enable invisible NIR retinal PBM |

```
visual_stimulation {
    frequency: 40Hz
    mode: binocular
    emdr_cadence: 1Hz
}
```

```
# EMDR bilateral alternation
visual_stimulation {
    frequency: 1Hz
    mode: emdr
    emdr_cadence: 1Hz
}
```

```
# Mode F — invisible NIR retinal PBM, no visible flicker
visual_stimulation {
    frequency: 0
    mode: mode_f
    enable_mode_f: true
}
```

---

### 4.9 qEEG 21-Channel

T2 only. 21-channel wet-gel cap with source localisation.

| Field | Canonical | Type | Values |
|-------|-----------|------|--------|
| `montage` | `montage` | string | `10-20` |
| `sloreta_enabled` | `sloreta_enabled` | bool | Enable sLORETA source imaging |
| `reference` | `reference` | string | `linked_ear` `cz` `average` |

```
qeeg_21ch {
    montage: 10-20
    sloreta_enabled: true
    reference: linked_ear
}
```

---

### 4.10 TMS

T2 only. Focal figure-8 coil, rTMS and TBS.

| Field | Canonical | Type | Values |
|-------|-----------|------|--------|
| `intensity` | `intensity_milliamps` | number | 0.1–0.5 T (expressed as % MT in app) |
| `tms_protocol` | `tms_protocol` | string | `rTMS` `TBS` `iTBS` |
| `frequency` | `frequency_hz` | number | 1–50 |
| `intensity_percent_mt` | `intensity_percent_mt` | number | % motor threshold, 80–120 typical |
| `target` | `target` | string | `DLPFC_L` `DLPFC_R` `VLPFC_L` `ACC` `MPFC` `M1_L` `M1_R` |
| `pulse_count` | `pulse_count` | int | total pulses per session |

```
tms {
    tms_protocol: rTMS
    frequency: 10Hz
    intensity_percent_mt: 80
    target: DLPFC_L
    pulse_count: 3000
}
```

---

### 4.11 Deep PBM 1170 nm

T2 only. Laser diodes for subcortical depth (35–40 mm).

| Field | Type | Values |
|-------|------|--------|
| `intensity_mw_cm2` | number | mW/cm², ≤1000 |
| `frequency` | number | 0 (CW) or 0.5–100 Hz |
| `duty_cycle` | number | 1–25 % |

Note: use `intensity_mw_cm2:` directly (not `intensity:`) since the unit is mW/cm², not percent.

```
pbm_deep_1170nm {
    intensity_mw_cm2: 500
    frequency: 40Hz
    duty_cycle: 25%
}
```

---

### 4.12 Clinical tACS

T2 only. Up to 16 independent channels, arbitrary waveform.

| Field | Canonical | Type | Values |
|-------|-----------|------|--------|
| `intensity` | `intensity_milliamps` | number | ≤4 |
| `frequency` | `frequency_hz` | number | 0.5–100 |
| `channel_count` | `channel_count` | int | 1–16 |
| `waveform` | `waveform` | string | `sinusoidal` `square` `triangular` |

```
clinical_tacs {
    frequency: 40Hz
    intensity: 1.2mA
    channel_count: 4
    waveform: sinusoidal
}
```

---

### 4.13 HD-tDCS

T2 only. sLORETA-guided high-definition tDCS (4×1 ring montage).

| Field | Canonical | Type | Values |
|-------|-----------|------|--------|
| `intensity` | `intensity_milliamps` | number | ≤2 per electrode |
| `target` | `target` | string | `DLPFC_L` `DLPFC_R` `VLPFC_L` `ACC` `MPFC` `M1_L` `M1_R` |
| `montage` | `montage` | string | `ring_4x1` `bilateral_4x1` `standard_2_electrode` |

```
hd_tdcs {
    target: DLPFC_L
    montage: ring_4x1
    intensity: 2.0mA
}
```

---

### 4.14 Cervical VNS

T2 accessory. Cervical vagus trunk stimulation via neck gel electrodes.

| Field | Canonical | Type | Values |
|-------|-----------|------|--------|
| `intensity` | `intensity_milliamps` | number | ≤2 |
| `frequency` | `frequency_hz` | number | 1–25 |

The safety MCU owns the enable GPIO and applies a cardiac rhythm interlock (HR change > 15 BPM → cutoff < 100 ms). These safety constraints cannot be overridden from NPPS.

```
cervical_vns {
    frequency: 25Hz
    intensity: 1.5mA
}
```

---

### 4.15 Vibrotactile 40 Hz

Provisional. Mastoid-placement LRA pad.

| Field | Type | Values |
|-------|------|--------|
| `intensity_g` | number | G (acceleration), 0.6–1.2 |
| `sync_to_audio` | bool | Sync start/stop to audio channel |
| `sync_to_visual` | bool | Sync start/stop to visual channel |

Note: use `intensity_g:` directly (not `intensity:`).

```
vibrotactile_40hz {
    intensity_g: 0.8
    sync_to_audio: true
    sync_to_visual: true
}
```

---

## 5. Intervals

By default a modality runs **continuously** for the full session. An interval causes it to pulse on and off. Use inline fields inside the modality block:

```
bes_tacs {
    frequency: 20Hz
    intensity: 0.8mA
    waveform: sinusoidal
    interval_on: 30s    # active period
    interval_off: 30s   # rest period
    repeat: until_end   # cycle until session ends
}
```

| Field | Type | Description |
|-------|------|-------------|
| `interval_on` | duration | Active period. `0` = continuous. |
| `interval_off` | duration | Rest period. `0` = continuous. |
| `repeat` | int or `until_end` | Cycle count. Omit or use `until_end` for continuous cycling. |

Omitting `interval_on` / `interval_off` entirely means run continuously.

---

## 6. Composite block

A composite layers multiple named protocols on a shared timeline.

```
composite "Full Multi-Modal RCT" {
    id: "10000100-0000-0000-0000-000000000000"
    description: "Baseline → combined active phase → cool-down."
    author: "NeuroPulse"
    version: "1.0"
    readonly: true
    tags: [rct, multimodal, research]
    conflict_resolution: merge

    layer "Vascular Baseline" {
        start: 0s
        duration: 5m
        intensity_scale: 0.5
    }

    layer "Gamma Focus" {
        start: 300s
        duration: 20m
        intensity_scale: 1.0
    }

    layer "Alpha Calm" {
        start: 1500s
        duration: 10m
        intensity_scale: 0.6
    }
}
```

### Composite metadata fields

Same as protocol metadata (`id`, `name`, `description`, `author`, `version`, `readonly`, `tags`) plus:

| Field | Type | Values |
|-------|------|--------|
| `conflict_resolution` | string | `merge` `sequential` `override` |

**Conflict resolution modes:**

| Value | Behaviour |
|-------|-----------|
| `merge` | Modalities from all active layers run simultaneously. |
| `sequential` | Layers run end-to-end; layer N starts when layer N-1 finishes. |
| `override` | Later (higher-`start`) layers replace earlier ones for shared modality types. |

### Layer block

The referenced protocol name is declared inline in the block header:

```
layer "Protocol Name" {
    start: 300s          # offset into composite timeline
    duration: 20m        # how long this layer runs (omit = full protocol duration)
    intensity_scale: 1.0 # multiplier applied to all modality intensities (0.0–2.0)
}
```

| Field | Type | Description |
|-------|------|-------------|
| `start` | duration | Start offset in the composite timeline. Default `0`. |
| `duration` | duration | Clip length. Omit to run the referenced protocol to its natural end. |
| `end` | duration | Alternative to `duration`; computed as `end - start`. |
| `intensity_scale` | number | Intensity multiplier (default `1.0`). |

---

## 7. Limits block

Limits blocks define per-modality safety constraints. They can appear in the same file as protocols or in standalone `.npps` files. Three levels form a hierarchy: **global → helmet → individual** (more specific levels override field-by-field).

```
limits "T1 Home Defaults" {
    level: global
    description: "Standard T1 safety limits."

    pbm_transcranial {
        max_intensity: 100
        max_frequency: 100
        max_duty_cycle: 25
        max_session_dose: 60.0
        max_daily_dose: 180.0
    }

    eeg_neurofeedback {
        allowed_bands: [delta, theta, alpha, beta, gamma, alpha_theta, gamma_theta]
        require_closed_loop: false
    }

    bes_tacs {
        max_intensity: 1.0
        max_frequency: 40
        min_frequency: 0.5
        max_session_duration: 1800
        max_sessions_per_day: 2
    }

    tdcs {
        max_intensity: 2.0
        max_session_duration: 1200
        max_sessions_per_day: 1
    }

    vns_hrv {
        max_intensity: 2.0
        max_frequency: 25
        max_session_duration: 3600
        allowed_protocols: [standalone, tavns_sync, eeg_biofeedback, pbm_combined]
    }

    audio_entrainment {
        max_intensity: 85
        max_frequency: 100
        max_binaural_beats: 100
        max_isochronic_tones: 100
    }

    visual_stimulation {
        max_frequency: 100
        min_frequency: 0.5
        allowed_modes: [binocular, emdr, mode_f]
        block_high_risk_range: false
    }

    tms {
        max_intensity_pct_mt: 120
        max_pulses_per_session: 3000
        max_pulses_per_day: 6000
        max_sessions_per_week: 5
        allowed_protocols: [rTMS, TBS, iTBS]
        allowed_targets: [DLPFC_L, DLPFC_R, VLPFC_L, ACC, MPFC, M1_L, M1_R]
    }

    pbm_deep_1170nm {
        max_intensity: 1000
        max_session_duration: 1800
    }

    clinical_tacs {
        max_intensity: 4.0
        max_session_duration: 1800
    }

    hd_tdcs {
        max_intensity: 2.0
        max_session_duration: 2700
        allowed_montages: [ring_4x1, bilateral_4x1, standard_2_electrode]
    }

    cervical_vns {
        max_intensity: 2.0
        max_session_duration: 1800
    }

    vibrotactile_40hz {
        max_intensity: 1.2
        max_session_duration: 1800
    }
}
```

### Limits block fields

**Top-level:**

| Field | Type | Values |
|-------|------|--------|
| `level` | string | `global` `helmet` `individual` |
| `helmet_id` | string | Device serial (if `level: helmet`) |
| `individual_id` | string | User ID (if `level: individual`) |
| `description` | string | Human-readable label |

**Per-modality sub-blocks** (all fields optional — omitted means unrestricted):

| Block | Field | Unit |
|-------|-------|------|
| `pbm_transcranial` | `max_intensity` | % |
| | `max_frequency` | Hz |
| | `max_duty_cycle` | % |
| | `max_session_dose` | J/cm² |
| | `max_daily_dose` | J/cm² |
| `pbm_intranasal` | `max_intensity` | % |
| | `max_session_dose` | J/cm² |
| | `max_session_duration` | seconds |
| `eeg_neurofeedback` | `allowed_bands` | array of band names |
| | `require_closed_loop` | bool |
| `bes_tacs` | `max_intensity` | mA |
| | `max_frequency` | Hz |
| | `min_frequency` | Hz |
| | `max_session_duration` | seconds |
| | `max_sessions_per_day` | int |
| `tdcs` | `max_intensity` | mA |
| | `max_session_duration` | seconds |
| | `max_sessions_per_day` | int |
| `vns_hrv` | `max_intensity` | mA |
| | `max_frequency` | Hz |
| | `max_session_duration` | seconds |
| | `allowed_protocols` | array of protocol names |
| `audio_entrainment` | `max_intensity` | % |
| | `max_binaural_beats` | Hz |
| | `max_isochronic_tones` | Hz |
| `visual_stimulation` | `max_frequency` | Hz |
| | `min_frequency` | Hz |
| | `allowed_modes` | array of mode names |
| | `block_high_risk_range` | bool (blocks 3–30 Hz photoparoxysmal zone) |
| `tms` | `max_intensity_pct_mt` | % MT |
| | `max_pulses_per_session` | int |
| | `max_pulses_per_day` | int |
| | `max_sessions_per_week` | int |
| | `allowed_protocols` | array (`rTMS`, `TBS`, `iTBS`) |
| | `allowed_targets` | array of target names |
| `pbm_deep_1170nm` | `max_intensity` | mW/cm² |
| | `max_session_duration` | seconds |
| `clinical_tacs` | `max_intensity` | mA |
| | `max_session_duration` | seconds |
| `hd_tdcs` | `max_intensity` | mA |
| | `max_session_duration` | seconds |
| | `allowed_montages` | array of montage names |
| `cervical_vns` | `max_intensity` | mA |
| | `max_session_duration` | seconds |
| `vibrotactile_40hz` | `max_intensity` | G |
| | `max_session_duration` | seconds |

---

## 8. Multi-block files

A single `.npps` file may contain any number of `protocol`, `composite`, and `limits` blocks in any order.

```
# combined-sleep.npps

protocol "Alpha Calm" {
    ...
}

protocol "Deep Sleep" {
    ...
}

composite "Sleep Wind-Down" {
    conflict_resolution: sequential
    layer "Alpha Calm" { start: 0s   duration: 10m  intensity_scale: 1.0 }
    layer "Deep Sleep"  { start: 600s                intensity_scale: 1.0 }
}

limits "Sleep Session Limits" {
    level: global
    pbm_transcranial { max_intensity: 60  max_frequency: 4 }
}
```

When parsing, `protocol` and `composite` blocks produce protocol entries; `limits` blocks are extracted separately via `parseNPPSLimits()`. Unknown top-level blocks cause a parse error.

---

## 9. Grammar summary

```
file        := entry*
entry       := protocol | composite | limits

protocol    := 'protocol' STRING '{' proto_field* '}'
proto_field := meta_field | typed_modality_block
meta_field  := IDENT ':' value

typed_modality_block := TYPE_ID '{' modality_param* '}'
modality_param       := IDENT ':' value
TYPE_ID              := 'pbm_transcranial' | 'pbm_intranasal' | 'eeg_neurofeedback'
                      | 'bes_tacs' | 'tdcs' | 'vns_hrv' | 'audio_entrainment'
                      | 'visual_stimulation' | 'qeeg_21ch' | 'tms' | 'pbm_deep_1170nm'
                      | 'clinical_tacs' | 'hd_tdcs' | 'cervical_vns' | 'vibrotactile_40hz'

composite   := 'composite' STRING '{' composite_field* '}'
composite_field := meta_field | layer_block
layer_block := 'layer' STRING '{' layer_field* '}'
layer_field := 'start' ':' DURATION | 'duration' ':' DURATION
             | 'end' ':' DURATION | 'intensity_scale' ':' NUMBER

limits      := 'limits' STRING? '{' limits_top_field* '}'
limits_top_field := 'level' ':' LEVEL_ID | 'helmet_id' ':' STRING
                  | 'individual_id' ':' STRING | 'description' ':' STRING
                  | TYPE_ID '{' limits_modality_field* '}'

value       := STRING | NUMBER | BOOL | array | IDENT
array       := '[' (value (',' value)*)? ']'
DURATION    := NUMBER ('s' | 'm')?   # bare number = seconds
BOOL        := 'true' | 'false'
LEVEL_ID    := 'global' | 'helmet' | 'individual'
```

Unknown `meta_field` keys (and unknown fields in limits sub-blocks) are silently skipped for forward compatibility. `limits` blocks accept an optional name string for existing files that omit it.
