# NPPS Language Reference

**Project:** NeurOne  
**Document:** NP-NPPS-REF-001  
**Revision:** B  
**Date:** 2026-07-17  
**Status:** ACTIVE  
**Effective Date:** 2026-07-17  
**Author:** Steve Hickman (CEO, interim Quality authority)  
**Approved By:** Steve Hickman, CEO  
**References:** NP-NPPS-GRAM-001 Rev B (`npps/grammar/npps.peggy`); NP-HEX-ZM-001 (module redesign)  
**Related Issues:** —  
**Gate:** —  
**IEC 62304 Class:** —

---

> **Rev B (2026-07-17) — module-set zones, conditions, references, one namespace.** With the hexagonal module redesign (NP-HEX-ZM-001) a *zone* is no longer a fixed hardware index — it is a **named set of modules**, and nothing more. Several predefined sets happen to correspond to anatomical lobes, but that is a property of how their author chose the membership, not a concept any code models: the firmware lobe/hemisphere assignment, the eight-entry predefined-lobe-group table and the `NP_GROUP_KIND_LOBE` query were all retired outright (OI-HUB-C14 — see the header of `firmware/hub_control/include/np_module_map.h`). Rev B adds top-level `zone` blocks (**14** predefined zones in `00-zones.npps` + user-defined), top-level `condition` blocks (condition name → external definition link), protocol `conditions` and `references` fields, and the single-namespace / whole-directory loading model (§1.6). Legacy `zones: all|front|rear` and `zones: [0,1]` numeric forms still parse.

**NeurOne Protocol Script (NPPS)** is the text format used to define, share, and store NeurOne session protocols. Files use the `.npps` extension.

## Contents

1. [Basics](#1-basics)  
   1.6 [Single namespace and file loading](#16-single-namespace-and-file-loading)
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
8. [Zone block](#8-zone-block)
9. [Condition block](#9-condition-block)
10. [Multi-block files](#10-multi-block-files)
11. [Grammar summary](#11-grammar-summary)
12. [Predefined protocol coverage (source-doc map)](#12-predefined-protocol-coverage-source-doc-map)

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

### 1.6 Single namespace and file loading

Every `.npps` file the app loads shares **one flat namespace**. There is exactly one namespace; every concept defined in any loaded file — protocols, composites, zones, conditions, limits — is visible to every other loaded file and may be cross-referenced by name. A zone defined in one file can be referenced from a protocol in another; a condition defined once is referenced by many protocols. This is deliberate: one namespace enforces the simplicity and commonality that avoids confusion. Until a proven need for something more sophisticated arises, NPPS uses this single-namespace model with no per-file scoping, imports, or qualified names.

**Directory model.** All loadable `.npps` files live under **one fixed protocol directory** (`/protocols/predefined/` for the shipped library). That directory may contain subdirectories; the **entire tree is read recursively** and every `.npps` file in it is loaded into the single namespace. The location is fixed until a proven need for flexibility arises.

**Name resolution.** References resolve by exact name string:

- A protocol's `conditions` entries must each match the `name` of a loaded `condition` block.
- A `pbm_transcranial` block's named `zones` entries must each match the `name` of a loaded `zone` block (or a predefined lobe zone).

Because the whole tree loads before resolution, definition order and file boundaries do not matter — a protocol may reference a zone or condition defined in a file loaded later. Unresolved references are reported by the loader (`validateNamespaceReferences`) and, for the shipped library, are covered by an automated test. Duplicate zone/condition names across files are a last-write-wins collision and are surfaced as a warning.

**Manifest.** The shipped library lists its files in `manifest.json` with four arrays: `zones`, `conditions`, `protocols`, `composites`. Definition files (`zones`, `conditions`) are loaded first so all references resolve.

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

### Reference arrays

A `references` value is an array whose elements are each either a **bare URL/path string** or a **`[label, url]` pair**:

```
references: [
    "https://doi.org/10.1000/example",
    ["Cassano 2018 RCT", "https://doi.org/10.1089/photob.2018"],
    ["PBM database §4", "docs/pbm_neuro_protocols.md#4-depression-major-depressive-disorder--grade-b"]
]
```

A bare string is shown as its own link text; a pair shows `label` and links to `url`. Links may be external (`https://…`) or in-repo document paths.

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
| `author` | string | `"NeurOne"` | Creator name. |
| `version` | string | `"1.0"` | Semantic version. Increment when protocol content changes. |
| `readonly` | bool | `false` | Prevents editing or deletion in the app. |
| `tags` | string array | `[]` | Freeform category labels. |
| `duration` | duration | `20m` | Fixed session length. |
| `interval_count` | int | — | Alternative to `duration`: session runs for N modality intervals. |
| `conditions` | string array | `[]` | Clinical conditions this protocol targets. Each entry MUST match the `name` of a loaded `condition` block (§9). Used for filtering/search and to surface condition definitions to the user. |
| `references` | reference array | `[]` | Links to documents that define the protocol or show what it is good for (evidence, applicability, expected results). Each entry is either a bare URL/path string or a `[label, url]` pair (§2, Reference arrays). Openable in an external browser. |

**Conditions field.** The `conditions` list uses standard medical condition names, each defined once in a `condition` block that carries a link to an external definition (e.g. Wikipedia / ICD-11). When a user is selecting a condition, the app offers to open that link in an external browser so they understand what the condition is. A condition name that does not resolve to a loaded `condition` block is a reference error (§1.6).

```
protocol "PBM — Depression (DLPFC)" {
    id: "30000004-0000-0000-0000-000000000000"
    tags: [pbm, clinical, depression]
    conditions: ["Major Depressive Disorder"]
    references: [
        ["PBM protocol database §4 (Grade B)", "docs/pbm_neuro_protocols.md#4-depression-major-depressive-disorder--grade-b"],
        ["Cassano 2018 DB RCT", "https://doi.org/10.1089/photob.2018"]
    ]
    duration: 20m
    ...
}
```

### Modality blocks

Modalities follow the metadata fields, each as a typed block (see §4):

```
protocol "Gamma Focus" {
    id: "10000001-0000-0000-0000-000000000000"
    description: "40Hz multi-modal entrainment."
    author: "NeurOne"
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
| `zones` | `zones` | string \| array | `all` `front` `rear` `custom`, a **named-zone-reference array**, or a legacy numeric-index array |
| `custom_zones` | `custom_zones` | int array | legacy zone indices, e.g. `[0,1]` |
| `wavelength` | `wavelength` | string | `660_808nm` `1064nm` `660_808_1064nm` |

**Zones are module sets (Rev B).** With the module redesign a zone is a **named set of modules** (§8), not a fixed hardware index. The preferred way to target zones is to reference zone definitions by name:

```
pbm_transcranial {
    intensity: 80%
    frequency: 40Hz
    duty_cycle: 25%
    zones: ["Frontal Left", "Frontal Right"]   # named zone references (§8)
    wavelength: 660_808nm
}
```

Each string must match the `name` of a loaded `zone` block or one of the eight predefined lobe zones (§8). The eight predefined names are `Frontal Left/Right`, `Temporal Left/Right`, `Parietal Left/Right`, `Occipital Left/Right`.

**Accepted `zones` forms:**

| Form | Meaning |
|------|---------|
| `zones: all` | every module in the map |
| `zones: front` / `zones: rear` | legacy fixed regions (retained) |
| `zones: ["Frontal Left", …]` | **named zone references** (Rev B, preferred) |
| `zones: [0, 1, 2]` | legacy numeric zone indices (equivalent to `zones: custom` + `custom_zones`) |
| `zones: custom` + `custom_zones: [0,1]` | legacy explicit indices |

`frequency: 0` (or `0Hz`) selects continuous-wave (CW) mode.

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
    author: "NeurOne"
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

## 8. Zone block

A **zone** is a named set of modules, **defined as an explicit list of socket (major) addresses**. With the hexagonal module redesign (NP-HEX-ZM-001) each helmet socket holds an interchangeable module, and a socket's id is the module's address — the *first half* of the two-level `(socket:element)` scheme. A zone lists the sockets whose modules it selects.

Listing sockets directly is what makes zones flexible: any set of sockets can be a zone, **including non-contiguous ones** (e.g. a scattered montage, or left + right frontal minus the midline). Zones are referenced by name from modality blocks (e.g. `pbm_transcranial`'s `zones` field, §4.1) and are defined once in one namespace, cross-referenceable from any file (§1.6).

```
zone "Frontal Left" {
    id: "40000001-0000-0000-0000-000000000000"
    description: "Frontal lobe, left hemisphere."
    sockets: [1, 2, 3, 5, 6, 10, 11]
}
```

> **Socket ids** are integers matching the firmware socket (major) address (`np_module_map`). In the module-address diagram (NP-HEX-ZM-001) the `S-NN` labels are these ids — `S-11` is socket `11`. The `S-` prefix is display only.
>
> **Range.** Ids are **1-based** and run `1..N`, where N is the socket count derived from skull anatomy and tile size (NP-HEX-ZM-001 §3.1: a 62 cm skull gives a 429 cm² tileable vault, which a 40 mm hexagonal tile fills with **30** sockets — corroborated by both the area quotient and the row-by-row construction). The numbering base and the bound are both enforced, not merely documented — the parser rejects a `sockets:` entry outside the range, naming the offending id and the valid range. Do not hardcode the count: read `NP_SOCKET_COUNT` / `NP_SOCKET_ID_MIN` / `NP_SOCKET_ID_MAX` from `socketMap.generated.ts`, which `scripts/sync-socket-map.ts` emits.
>
> **A zone is a SET.** Repeated ids in a `sockets:` list are collapsed at parse time and the list is returned sorted, so every consumer receives canonical membership. Unions of zones dedup for the same reason: midline sockets are members of BOTH hemisphere zones of their lobe, so a bilateral protocol referencing `Frontal Left` + `Frontal Right` addresses the shared midline socket once, not twice. Use `unionSockets` / `unionZoneSockets` (`app/web/src/lib/socketSet.ts`) rather than concatenating.

### Predefined zones

Eight predefined lobe zones ship in `00-zones.npps` (read-only), one per lobe × hemisphere — `Frontal Left/Right`, `Temporal Left/Right`, `Parietal Left/Right`, `Occipital Left/Right`. Membership is **derived from skull geography**, not authored: rows are positioned as a fraction of the nasion→inion arc (the 10-20 coordinate), the central sulcus at the C line (50%) divides frontal from parietal, the parieto-occipital sulcus at the PO line (80%) divides parietal from occipital, and temporal is the lateral band below the Sylvian fissure. `scripts/sync-socket-map.ts` regenerates all eight and fails the build if the shipped file disagrees. **Center-column (midline) sockets belong to both the Left and Right zone of their lobe** (sockets 1, 3, 11, 16, 21, 30), so a lobe's two hemispheric zones together cover the whole lobe and a bilateral protocol referencing both gets full coverage. Protocols reference these zones by name.

### User-defined zones

Users may define their own zones in any `.npps` file in the protocol directory tree — just list the sockets:

```
# an arbitrary, non-contiguous zone
zone "My Montage" {
    description: "Two frontal sockets plus one occipital"
    sockets: [4, 6, 30]
}
```

**Optional element-type filter.** A socket's module carries several element types; a zone may restrict itself to some of them (e.g. only the PBM LEDs, or only the EEG electrodes, within the listed sockets):

| Field | Type | Meaning |
|-------|------|---------|
| `types` | element-type array | restrict to these element types within the listed sockets |
| `exclude_types` | bool | `false` (default): include only `types`; `true`: exclude `types` |

**Element-type names** mirror firmware `np_elem_type_t`: `led_660`, `led_808`, `led_1064`, `led_1170`, `eeg_electrode`, `tes_electrode`, `vns_contact`, `ntc`, `pd_forward`, `pd_back`, `ir_prox`, `hall`, `dual_electrode`.

```
# the listed sockets, but only their PBM LEDs
zone "Crown LEDs" {
    sockets: [11, 16, 21]
    types: [led_660, led_808]
}
```

### Zone block fields

| Field | Type | Notes |
|-------|------|-------|
| `sockets` | int array | **The zone's modules, by socket (major) address.** The defining field. 1-based, `1..30` on the current lattice; out-of-range, non-integer, hex, exponent and boolean values are all parse errors. Deduplicated and sorted on parse, and canonicalised again on serialize. |
| `id` | string | Optional stable UUID. Presence marks the zone predefined/read-only. |
| `description` | string | Human-readable label. |
| `types` | element-type array | Optional element-type filter within the listed sockets. |
| `exclude_types` | bool | Invert the type filter. |

---

## 9. Condition block

A **condition** definition pairs a standard medical condition name with a link to an external definition of that condition (e.g. Wikipedia, ICD-11). Its purpose is to let protocols reference conditions using standard medical terms while giving the user reference material to understand what the condition is. Protocols list conditions in their `conditions` field (§3); when a user selects a condition, the app offers to open the `link` in an external browser.

```
condition "Major Depressive Disorder" {
    id: "41000004-0000-0000-0000-000000000000"
    link: "https://en.wikipedia.org/wiki/Major_depressive_disorder"
    code: "6A70"
}
```

### Condition block fields

| Field | Type | Notes |
|-------|------|-------|
| `link` | string | **Required.** URL to an external definition (opened in an external browser). |
| `id` | string | Optional stable UUID. |
| `code` | string | Optional standard code (ICD-11 MMS / SNOMED / MeSH). |
| `description` | string | Optional short gloss. |

The shipped condition registry lives in `00-conditions.npps`. Every condition named by a predefined protocol is defined there; the mapping is validated automatically.

---

## 10. Multi-block files

A single `.npps` file may contain any number of `protocol`, `composite`, `limits`, `zone`, and `condition` blocks in any order.

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

When parsing, `protocol` and `composite` blocks produce protocol entries; `limits` blocks are extracted separately via `parseNPPSLimits()`; `zone` and `condition` blocks populate the namespace (§1.6). Unknown top-level blocks cause a parse error.

---

## 11. Grammar summary

Complete summary of NPPS Rev B (source of truth: `npps/grammar/npps.peggy`, NP-NPPS-GRAM-001 Rev B).

```
# ── Top level ──────────────────────────────────────────────────────────────
file        := entry*
entry       := protocol | composite | limits | zone | condition

# ── Protocol ───────────────────────────────────────────────────────────────
protocol    := 'protocol' STRING '{' proto_field* '}'
proto_field := meta_field | typed_modality_block
meta_field  := IDENT ':' value
# recognised protocol meta_field keys:
#   id, description, author, version, readonly, tags,
#   duration, interval_count, conditions, references
#   conditions := 'conditions' ':' STRING_ARRAY    # names → condition blocks
#   references := 'references' ':' REF_ARRAY

typed_modality_block := TYPE_ID '{' modality_param* '}'
modality_param       := IDENT ':' value
TYPE_ID              := 'pbm_transcranial' | 'pbm_intranasal' | 'eeg_neurofeedback'
                      | 'bes_tacs' | 'tdcs' | 'vns_hrv' | 'audio_entrainment'
                      | 'visual_stimulation' | 'qeeg_21ch' | 'tms' | 'pbm_deep_1170nm'
                      | 'clinical_tacs' | 'hd_tdcs' | 'cervical_vns' | 'vibrotactile_40hz'
# pbm_transcranial 'zones' value:
#   ZONE_KEYWORD ('all'|'front'|'rear'|'custom') | STRING_ARRAY (named zone refs)
#                                                | INT_ARRAY (legacy indices)

# ── Composite ──────────────────────────────────────────────────────────────
composite   := 'composite' STRING '{' composite_field* '}'
composite_field := meta_field | layer_block
# composite meta_field keys add: conflict_resolution, conditions, references
layer_block := 'layer' STRING '{' layer_field* '}'
layer_field := 'start' ':' DURATION | 'duration' ':' DURATION
             | 'end' ':' DURATION | 'intensity_scale' ':' NUMBER

# ── Limits ─────────────────────────────────────────────────────────────────
limits      := 'limits' STRING? '{' limits_top_field* '}'
limits_top_field := 'level' ':' LEVEL_ID | 'helmet_id' ':' STRING
                  | 'individual_id' ':' STRING | 'description' ':' STRING
                  | TYPE_ID '{' limits_modality_field* '}'

# ── Zone (named set of modules = a list of socket addresses) ───────────────
zone        := 'zone' STRING '{' zone_field* '}'
zone_field  := 'sockets' ':' INT_ARRAY                        # the zone's modules (defining field)
             | 'id' ':' STRING | 'description' ':' STRING
             | 'types' ':' ELEM_TYPE_ARRAY | 'exclude_types' ':' BOOL
# 'sockets' lists socket (major) addresses, 1-based and within the derived
# lattice range; arbitrary/non-contiguous sets are fine. Duplicates collapse.
ELEM_TYPE   := 'led_660' | 'led_808' | 'led_1064' | 'led_1170'
             | 'eeg_electrode' | 'tes_electrode' | 'vns_contact' | 'ntc'
             | 'pd_forward' | 'pd_back' | 'ir_prox' | 'hall' | 'dual_electrode'

# ── Condition (name → external definition link) ────────────────────────────
condition   := 'condition' STRING '{' condition_field* '}'
condition_field := 'link' ':' STRING        # required
             | 'id' ':' STRING | 'code' ':' STRING | 'description' ':' STRING

# ── Values ─────────────────────────────────────────────────────────────────
value       := STRING | NUMBER | BOOL | array | IDENT
array       := '[' (value (',' value)*)? ']'
STRING_ARRAY:= '[' (STRING (',' STRING)*)? ']'          # also accepts bare idents
REF_ARRAY   := '[' (ref (',' ref)*)? ']'
ref         := STRING | '[' STRING ',' STRING ']'        # url | [label, url]
INT_ARRAY   := '[' (INT (',' INT)*)? ']'
DURATION    := NUMBER ('s' | 'm')?   # bare number = seconds
BOOL        := 'true' | 'false'
LEVEL_ID    := 'global' | 'helmet' | 'individual'
```

**Namespace & resolution (§1.6):** all loaded files share one namespace. `conditions` entries resolve to `condition` block names; `pbm_transcranial` named `zones` entries resolve to `zone` block names (predefined or user). The whole protocol directory tree loads before resolution.

**Forward compatibility:** unknown `meta_field` keys (and unknown fields in limits/zone/condition sub-blocks) are silently skipped. `limits` blocks accept an optional name string for existing files that omit it. Legacy `zones` forms (`all`/`front`/`rear`/`custom` + `custom_zones`, and numeric `zones: [0,1]`) continue to parse.

---

## 12. Predefined protocol coverage (source-doc map)

The predefined library includes a protocol for **every uniquely-identified, device-expressible** protocol in `docs/pbm_neuro_protocols.md` and `docs/neuromod_neuro_protocols.md`. "Device-expressible" means the protocol maps onto a NeurOne modality block (a scalp/transcranial, intranasal, auricular-taVNS, T2-cervical-tcVNS, T2-focal-TMS, tES, or tACS channel). Protocols the hardware cannot deliver are **excluded and listed below with the reason**, so coverage is auditable rather than silently partial.

Clinical presets are `clinical-NN-*.npps` (ids in the `30000xxx` band), each carrying `conditions` and `references`. The `npps-predefined` test asserts the full set parses and every zone/condition reference resolves.

### Included (device-expressible)

- **PBM** (`docs/pbm_neuro_protocols.md`): Alzheimer's/dementia (40 Hz), MCI, cognitive enhancement (1064 nm), depression (DLPFC), anxiety, TBI (chronic) + intranasal, autism (pediatric 40 Hz), Parkinson's (transcranial channel only), stroke (chronic rehab).
- **TMS** (T2 focal figure-8): depression (10 Hz + accelerated iTBS), neuropathic/chronic pain, migraine prophylaxis, PTSD, stroke motor, Parkinson's motor, fibromyalgia, addiction (DLPFC).
- **VNS** (auricular taVNS + T2 cervical tcVNS): epilepsy, depression, stroke rehab (paired), anxiety, insomnia, PTSD (paired), autonomic/HRV, migraine/cluster (tcVNS), migraine prophylaxis (taVNS 1 Hz), tinnitus (taVNS + tones).
- **tDCS / HD-tDCS**: working-memory/cognition, depression, chronic pain/fibromyalgia, stroke rehab, aphasia, schizophrenia, addiction, epilepsy (cathodal), ADHD, MS fatigue, Alzheimer's, Parkinson's.
- **tACS** (`bes_tacs` T1 / `clinical_tacs` T2): working memory (theta), fluid intelligence (gamma), Alzheimer's (gamma), Parkinson's tremor (phase-locked), depression (alpha), sleep/memory (slow-osc), schizophrenia, ADHD, chronic pain/fibromyalgia, WM restoration (HD).

### Excluded (not device functions) — with reason

| Source-doc protocol | Reason excluded |
|---------------------|-----------------|
| PBM Bell's palsy / facial nerve | Peripheral facial-nerve point application — not a scalp/transcranial helmet site. |
| PBM carpal tunnel | Peripheral wrist/limb site. |
| PBM diabetic peripheral neuropathy | Peripheral feet/limb site (+ documented null for 890 nm LED pads). |
| PBM spinal cord injury | Spinal / transcutaneous / implanted-fiber site. |
| PBM multiple sclerosis (strength) | Peripheral muscle site, not CNS. |
| PBM stroke — acute (<24 h) | Documented null in 3 large RCTs; contraindicated as monotherapy. |
| PBM epilepsy | Grade D projection, no human trials; visual-flicker contraindication — not offered as a preset (safety-gated). |
| PBM Parkinson's neck (vagus) + abdomen (gut) light | Off-scalp peripheral sites; the transcranial channel *is* included. |
| TMS deep-coil indications: depression-deep (H1), OCD (H7), smoking cessation (H4) | Deep H-coils are not the device's focal figure-8 coil. |
| TMS migraine — acute (handheld sTMS, occipital) | Handheld single-pulse device; occipital not a coil target. |
| TMS aphasia (right IFG), schizophrenia (temporoparietal), tinnitus (auditory cortex), Alzheimer's multisite (language/parietal) | Coil targets outside the device target set (`DLPFC_L/R`, `VLPFC_L`, `ACC`, `MPFC`, `M1_L/R`). |
| VNS implanted cervical; VBLOC abdominal block | Surgical implants — 510(k) predicate/reference only, not device functions. |
| VNS cardiac/inflammatory/metabolic taVNS rows (AFib, heart failure, POTS, RA, IBD, obesity, glucose, GI) | taVNS-expressible in principle but out of the neuro/wellness scope and gated by medical-device claims; documented, not shipped as presets. |
| tDCS dysphagia (pharyngeal M1), disorders-of-consciousness | Niche placements / inpatient clinical use; documented, not shipped as presets. |
| tACS essential tremor (cerebellar phase-locked) | Cerebellar placement; documented, not shipped as a preset. |

*Reference material and parameters for excluded protocols remain in the two source docs; exclusion is about what ships as a runnable preset, not about the evidence.*
