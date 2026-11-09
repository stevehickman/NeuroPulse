# Audio Cup Assembly — Hardware Specification

**Project:** NeurOne
**Document:** NP-HW-AUDIO-001
**Revision:** 7
**Date:** 2026-11-09
**Status:** DRAFT — **requirements-grade. Every geometric and acoustic value an FAI would inspect is absent, and §8 says which, rather than supplying a placeholder.**
**Effective Date:** —
**Author:** NeurOne Systems Engineering
**Approved By:** — (new document)
**References:** CLAUDE.md §3 modality ⑦ (neural audio entrainment), §4.2 (safety architecture), §4.3 (EMF shielding), §4.7 (status indicators), §2.3 (consumables); `docs/reference/modality-stack.md` (modality 7 detail); `docs/reference/durability-maintenance.md` (bayonet and mesh frame cost deltas, §condition monitoring); `docs/reference/commercial-model.md` §2.3 (foam and mesh frame rows); NP-FW-HUB-001 Rev 1 §8.5 (`np_mod_audio.c`); NP-ART-001 Rev 3 §2.3 A11, §3.2, §4, OI-ART-04; NP-FAI-001 Rev 3 §2 (F1–F4); NP-RISK-002 Rev 3 §4, OI-RISK2-02; NP-BIB-EMF-001 Rev 1 (per-layer shielding evidence audit); NP-ACC-PRIORITY-001 Rev 3 §5 ranks 13 and 14; NP-HFE-001 CT-01; NP-HFE-002 §7.5; NP-CONV-001 Rev 6 §4, §5, §6; `firmware/hub_control/modules/np_mod_audio.c`; `docs/np_hex_zm_isa.md` (audio cups out of cranial-tile scope)
**Related Issues:** GitHub Issue #332 (OI-ART-04 — A11 has no owning document)
**Gate:** NP-COORD-001 G2 (pre-tooling) — this document is A11's entry in the G2 completeness check, not a release of it
**IEC 62304 Class:** N/A (hardware specification). It constrains SW-02 Class B code (`firmware/hub_control/modules/np_mod_audio.c`) and is **not** in any Class C path — §5 states why, and that this is a design property rather than an omission.
**Supersedes:** None — first issue. It removes `CLAUDE.md` §3 modality ⑦ from the role of governing specification for artifact A11.
**Parent Document:** None

---

> **Why this document exists.** `NP-ART-001` §4 found that nine of fifteen manufactured artifacts
> had no owning specification, and that four of the nine are shipping T1 modalities. A11 — the audio
> cup assembly — was one of them: its governing specification was a **one-line roster entry in
> `CLAUDE.md` §3**, which is a statement of what the product includes, not a statement of what gets
> manufactured. `OI-ART-04` asked for the specification. This is it.
>
> **What it is not.** It is not a design. Nothing here is dimensioned, no driver is selected, no
> acoustic performance is specified, and **no value in this document was invented for it**: every
> figure is carried from a document that already stated it, with the citation on the row. Where the
> record is silent, §8 names the silence and routes it to an open item. That is the whole difference
> between an owning document and a design, and `NP-FAI-001` §2's F1–F4 is the reason the difference
> is worth a document: an FAI checklist written against invented numbers produces a pass record
> that means nothing.
>
> **The one finding that is new here is §6.3:** the audio cup mesh carries a **40 dB RF** figure that
> appears in no layer of the `CLAUDE.md` §4.3 stack and in no row of `NP-BIB-EMF-001`'s per-layer
> value audit — a shielding claim on a **user-replaceable consumable**, outside the audited set,
> whose replacement is prompted by a measurement that does not exist.

---

## 1. Scope

**In scope, and binding:**

| § | What it pins |
|---|---|
| §2 | What the artifact is, and the sub-parts it decomposes into for tooling, inspection and consumable purposes |
| §3 | The requirements the existing record already binds, stated as requirements so they can be argued with |
| §4 | The electrical and firmware interface, as the shipped driver has it |
| §5 | The safety position: audio is the one modality gated by no safety-MCU enable line, and why that is correct |
| §6 | The two consumables, their trigger kinds under `CLAUDE.md` §2.3, and the RF claim §6.3 surfaces |
| §7 | Hazard-analysis inputs — what `OI-RISK2-02` must analyse, now that it has a specification to analyse |

**Out of scope:** acoustic tuning and psychoacoustic content (binaural/isochronic generation is
firmware, `NP-FW-HUB-001` §8.5); the hub I²S codec and SAI peripheral, which belong to the hub PCB
(artifact A9, `NP-HW-HUB-001`); and the cranial hex-tile lattice — **audio cups are not
cranial-tiled and never enter the socket map** (`docs/np_hex_zm_isa.md`).

---

## 2. The artifact

**A11 is a bilateral head-worn acoustic assembly with two independent transducer paths and one
user-replaceable acoustic/RF element.** It mounts to the headset arms; it does not occupy a socket
and carries no module UID in `np_module_map`.

| Sub-part | What it is | Status of the record |
|---|---|---|
| **A11.1 — planar magnetic driver** | Over-ear, **40 mm** (`CLAUDE.md` §3 modality ⑦) | Diameter only. No impedance, sensitivity, frequency response, magnet grade or diaphragm material anywhere in the set |
| **A11.2 — bone-conduction transducer** | Piezoelectric element at the **mastoid**, with a **silicone isolator** (`docs/reference/modality-stack.md`) | Named, not specified. No drive level, no contact force, no isolator durometer |
| **A11.3 — cup body and ear pad** | Carries A11.1, A11.2, the mesh frame and — from Rev 4 — the in-cup microphone (`REQ-AUDIO-14`); the foam ear pad is a consumable (§6.1) | No geometry of any kind |
| **A11.4 — mesh frame** | **Silver-coated nylon**, user-replaceable, **snap-in**, sold as a pair (§6.2) | Material and retention method named; aperture, open area and the 40 dB figure's basis are not |
| **A11.5 — bayonet mount** | **Aluminium**, replacing plastic — *"plastic detent flattens at 500–1,000 cycles; metal-to-metal rated for device lifetime"* (`docs/reference/durability-maintenance.md`) | A rationale and a cost delta (+$1.60). No interface geometry, no engagement torque, no cycle rating of its own |

> **`REQ-AUDIO-01`** — A11 decomposes for tooling, inspection and consumable purposes exactly as the
> five rows above. A tooling specification that treats the cup as one moulding, or an FAI that
> inspects it as one part, does not inspect A11.4 or A11.5, which are the two parts a **user**
> removes and refits.

### 2.1 Which configurations ship it

Audio is **modality ⑦**, present from **Home Standard** upward (`NP-ACC-PRIORITY-001` §5 ranks 13
and 14 both read *"Home Standard and up (audio is modality ⑦)"*). It is therefore in the **T1
flagship box**, which is why `NP-ART-001` §4 counts it among the shipping artifacts with no
document. **Per-configuration inclusion is `docs/reference/commercial-model.md` §2.1's to state and
is not restated here** — CLAUDE.md §2 holds no number and neither does this document.

---

## 3. Requirements the record already binds

Each row is carried from a document that states it. **No row in this table originates here** —
§3.1's rows do, from a principal decision, and say so.

| ID | Requirement | Source |
|---|---|---|
| **REQ-AUDIO-02** | Over-ear planar magnetic driver, 40 mm, one per side | `CLAUDE.md` §3 modality ⑦ |
| **REQ-AUDIO-03** | Mastoid bone-conduction piezoelectric element, one per side, mounted on a silicone isolator | `docs/reference/modality-stack.md` modality 7 |
| **REQ-AUDIO-04** | Both transducer paths are driven from the **hub** I²S codec via SAI — audio origination, mixing and entrainment synthesis are hub-side, not in the cup | `NP-FW-HUB-001` §8.5 |
| **REQ-AUDIO-04a** | **Whether any amplification sits in the cup is unstated and is not decided here.** `REQ-AUDIO-04` fixes where the signal comes from, not where it is amplified; a planar magnetic driver's drive topology follows from `OI-AUDIOHW-04`'s selection and changes the cable, the cup's power and its thermal behaviour | — (absence, recorded) |
| **REQ-AUDIO-05** | The mesh frame is **user-replaceable without tools**, snap-in, and replaceable as a pair | `docs/reference/modality-stack.md`; `docs/reference/durability-maintenance.md` |
| **REQ-AUDIO-06** | The cup-to-arm mount is an **aluminium bayonet**, not a plastic detent, and is rated for device lifetime rather than a cycle count | `docs/reference/durability-maintenance.md` |
| **REQ-AUDIO-07** | Mesh fouling is detected by **driver impedance**, which is stated to detect acoustic degradation and RF shielding loss simultaneously | `docs/reference/modality-stack.md`; `docs/reference/durability-maintenance.md` §condition monitoring |
| **REQ-AUDIO-08** | A mesh cleaning brush ships in the box | `docs/reference/modality-stack.md` |
| **REQ-AUDIO-09** | The assembly delivers **no current, no light and no field** to the user, and therefore requests no safety-MCU enable | `NP-FW-HUB-001` §8.5; §5 below |
| **REQ-AUDIO-10** | Bone conduction requires transducer contact with the head, so it is usable **only while worn** — insertion-time confirmation is the companion app's, not the cup's | `NP-HFE-001` CT-01; `NP-HFE-002` §7.5 |

**`REQ-AUDIO-07` is the load-bearing row in this table and it is not satisfied by anything that
exists.** See §6.2.

### 3.1 Requirements that originate here — `OI-AUDIOHW-08`, option B (Rev 4, principal, 2026-09-24)

These are the first rows in this document that are **not** carried from another one. They come from
the principal decision recorded in §6.4, and each row states what fails without it and where that is
traceable (`CLAUDE.md` §18). **No value is set by any of them.**

| ID | Requirement | What fails if it is not met | Traceable to |
|---|---|---|---|
| **REQ-AUDIO-14** | **One microphone per cup, in the front (ear-side) acoustic volume, used for measurement only.** The driver plays **no anti-noise**, and the microphone closes no control loop | Without the microphone, the foam's seal is not measured and `OI-ACC-04` has no condition measurement; with a loop, the §6.4.3–§6.4.5 content, safety and latency consequences arrive with no requirement or control behind them | Principal decision 2026-09-24 (§6.4); `OI-ACC-04` principal decision 2026-09-23; `CLAUDE.md` §2.3 kind 1 |
| **REQ-AUDIO-15** | **Raw microphone audio is excluded by design, not by setting.** The microphone is unpowered outside a measurement window; the capture buffer is reduced to the derived figure and cleared in the same call; no interface returns samples; and nothing can store, log, stream (Mode 1), write to eMMC or transmit the audio | A bystander's voice is captured by a device on which they are neither consent subject, and no consent flow can reach them; the wearer's speech is captured outside any UHDR purpose | `CLAUDE.md` §6.0 (two consent subjects), §5.1; §6.4.6 |
| **REQ-AUDIO-16** | **The foam seal is the dominant leak from the front volume.** Any designed vent is sized so the seal measurement still resolves the foam; a fully open-back cup does not meet this | The measurement reads the vent, not the foam, and `OI-ACC-04` does not close | Derivation from the measurement (§6.4.7); how much vent is tolerable is `OI-AUDIOHW-04`'s to show |

| **REQ-AUDIO-19** | **The planar path's sound pressure level at the ear never exceeds 85 dB SPL.** (a) The protocol compiler refuses, before signing, a protocol whose audio level is above it, or not positive and finite; (b) the hub driver refuses a command above it before any state or output changes; (c) the level delivered at the ear is verified against it on a built unit. A limits block (`max_volume_db`) may lower the ceiling and never raise it. **Refused, never clamped:** a clamped level is a different stimulus from the one authored | A level above it risks pain and injury, and with no way to know the wearer's exposure outside a session no dose margin can be assumed. A clamp would silently deliver a stimulus nobody authored | NIOSH Pub. 98-126; WHO–ITU H.870; IEC 62368-1 cl. 10.6 / EN 50332; Directive 2003/10/EC (§5.1, checked by `OI-AUDIOHW-12`). Principal direction 2026-11-09. `CLAUDE.md` §3 (a ceiling refuses, never reshapes) |
| **REQ-AUDIO-20** | **Every audio level, on every interface, is an absolute sound pressure level in dB SPL at the ear, never a percentage or any scale relative to a component.** (a) The hub wire field is `level_half_db`, 2 × dB SPL, 0 meaning off; (b) each component's HAL owns its own calibration from that level to its drive, so the same command sounds the same on any driver; (c) that calibration is measured at the ear (`OI-AUDIOHW-11`). The bone transducer is outside (c) until `OI-AUDIOHW-02` | A relative scale means one command produces different sound on different components, so REQ-AUDIO-19 would bound a number rather than a sound | Principal direction 2026-11-09 (all scales are absolute); `NP-NPPS-REF-001` Rev 18 §4.1b (the protocol layer is already absolute) |

| **REQ-AUDIO-21** | **The bone-conduction transducer's output is limited at the same hearing level as the planar path, stated as an absolute force level.** (a) The bone level is its own absolute quantity, **force level in dB re 1 µN on an IEC 60318-6 mechanical coupler (artificial mastoid), at the carrier frequency**, never a percentage and never the planar level number; (b) the ceiling at carrier frequency *f* is **L_F,max(f) = RETVFL_mastoid(f) + (85 − RETSPL_air(f))**, which is the hearing level that `REQ-AUDIO-19`'s 85 dB SPL gives on the air path, carried to the bone path through the reference thresholds of ISO 389-3 and the matching air reference of ISO 389-1/-7; (c) a command above it is refused, never clamped, in the compiler and the hub driver; (d) the delivered force level is verified on a built unit. The bone and planar paths are each held to their own ceiling; their combined effect is not summed (`OI-AUDIOHW-13`) | A planar ceiling written in dB SPL does not bound a vibrator: the same number drives a mastoid transducer to a different loudness, and through a different path that bypasses the ear canal and the cup's seal. Without its own ceiling, a bone cue (breathing pacer, sync cue) is limited by nothing | ISO 389-3:2016 (reference equivalent threshold vibratory force levels, mastoid); ISO 389-1/-7; IEC 60318-6; IEC 60645-1:2017 cl. 6.1.5 (bone vibrator output level). Principal direction 2026-11-09 (all scales absolute, `REQ-AUDIO-20`). `CLAUDE.md` §3 (a ceiling refuses, never reshapes). **The dB re 1 µN figure is not stated here: it needs the standards' tables (§5.2, `OI-AUDIOHW-13`)** |

Figures derived from the microphone — the seal score and any in-cup level — are **UHDR**, computed
and used on-device (`docs/reference/data-architecture-detail.md` §5.1 boundary resolutions).

---

## 4. Electrical and firmware interface

The interface is fully determined by shipped Class B code and is restated here so that a hardware
change surfaces as a disagreement with a document rather than with a driver.

| Parameter | Value | Source |
|---|---|---|
| Hub slot | `NP_HUB_SLOT_AUDIO`, module type `NP_MOD_AUDIO` = `0x08` | `firmware/hub_control/include/np_hub_types.h` |
| Transport | I²S from the hub codec via SAI; both paths from the same codec | `NP-FW-HUB-001` §8.5 |
| Command block | `np_mod_audio_params_t` — `mode` (0 binaural, 1 isochronic, 2 pink, 3 brown, 4 off), `carrier_hz`, `beat_mhz`, `level_half_db` (absolute dB SPL at the ear × 2, `REQ-AUDIO-20`), `bone_conduct_en`, `eeg_adaptive` | `np_hub_types.h` |
| Adaptive band map | delta/theta/alpha/beta/gamma → 2/6/10/20/40 Hz (`k_band_beat_mhz`), retuned every `NP_AUDIO_ADAPT_INTERVAL_MS` | `NP-FW-HUB-001` §8.5 |
| Safety enable | `NP_SAFETY_EN_AUDIO` = **0** — no bit is requested and `slot_to_safety_bit()` returns 0 | `firmware/hub_control/include/np_hub_config.h`; §5 |
| Condition telemetry | `np_mod_audio_hal_mesh_impedance()` → float Ω | `firmware/hub_control/modules/np_mod_audio.c` |

> **`REQ-AUDIO-11`** — `bone_conduct_en` is a **per-command** field, so the two transducer paths are
> independently addressable and must remain independently drivable in hardware. The HRV biofeedback
> protocols rely on this: `CLAUDE.md` §3 modality ⑥ reserves the bone-conduction channel for the
> breathing cue while the planar path carries binaural content.

> ~~**`REQ-AUDIO-17`** — `volume_pct` is **0–100**, and a command above 100 is **refused**~~ **RETIRED (2026-11-09), superseded by `REQ-AUDIO-19` and `-20`:** the field is no longer a 0–100 scale. It is `level_half_db`, an absolute dB SPL level × 2, refused above 85 dB SPL. The refusal before any state change, which this row bound, is kept as `REQ-AUDIO-19`(b), and the "sets no sound level" caveat is withdrawn. Retired in place, not deleted (`CLAUDE.md` §18).

> **`REQ-AUDIO-18`** — An EEG band update (`np_mod_audio_set_eeg_band()`) retunes **only a running binaural command** (mode 0). An isochronic, noise or off command is left as commanded. *What fails without it:* an adaptive isochronic or noise session turns into binaural, and an off command starts a tone. *Traceable to:* `NP-FW-HUB-001` §8.5 ("EEG-adaptive mode retunes the binaural carrier"). Verified by `np_mod_audio_tests` (`OI-SIMFW-19`).

---

## 5. Safety position — the one modality with no enable line

`CLAUDE.md` §4.2's interlock table has no row for audio, and `NP_SAFETY_EN_AUDIO` is deliberately
`0`. **That is correct and this document affirms it rather than treating it as a gap**: the safety
MCU owns *stimulation* enable lines, and audio delivers no current, no light and no field
(`REQ-AUDIO-09`).

> **`REQ-AUDIO-12`** — Any future change that puts current, light or a field into this assembly —
> a transcutaneous electrode on the cup, an LED, an added coil — **moves A11 into `CLAUDE.md` §4.2's
> table and requires a safety-MCU enable line**. It is a Class C change, not a variant.

**The planar path has an acoustic output ceiling (Rev 6, `REQ-AUDIO-19`, §5.1).** The bone-conduction
path does not, and an air-conduction limit does not transfer to it unexamined:

- **Bone-conduction drive limit.** Decided as a **rule**, not yet as a figure (Rev 7, §5.2,
  `REQ-AUDIO-21`). The driver still passes the bone transducer the same level number it passes the planar
  drivers, so that number is an upper bound on its command and **not** a bone level; the field that
  carries a bone force level does not exist yet (`OI-AUDIOHW-13`).

### 5.1 The acoustic output ceiling — 85 dB SPL, absolute (Rev 6, principal direction 2026-11-09)

**The ceiling.** The sound pressure level at the ear, on the planar path, never exceeds **85 dB SPL**
(re 20 µPa). It is a level ceiling and nothing else: **no exposure-duration model and no daily-dose
figure is set or claimed.**

**Why it is a level and not a dose.** Every exposure standard below is a *daily* dose: a level and
the hours it may last. The device cannot know what the wearer is exposed to outside a session, so it
cannot know how much of any day's budget a session leaves, and no session may be built to approach one.
A bound that holds whatever else happened that day has to be a level the device never exceeds.

**Why 85.** The standards below put the onset of hearing-conservation obligations at 85 dB(A) for a full
working day. Setting the ceiling there means a session can never, by level alone, be louder than the
level at which those standards begin to require action, and it sits far under the level at which
sound is painful or injurious in seconds. What the ceiling does **not** mean is that a session at the
ceiling is harmless: at NIOSH's 3 dB exchange rate (`T = 8 h / 2^((L − 85)/3)`) a 20-minute session at
85 dB uses about 4 % of a day's 8-hour budget and a 60-minute session 12.5 %, on top of whatever the
wearer was exposed to otherwise. The shipped protocols play at 75 dB SPL or less (`protocols/predefined/`),
which that formula allows for roughly 80 hours.

| Standard | What it says | Use here |
|---|---|---|
| NIOSH, *Criteria for a Recommended Standard: Occupational Noise Exposure*, Pub. 98-126 (1998) | REL 85 dBA, 8 h time-weighted average, 3 dB exchange rate | The 85 and the dose arithmetic above |
| WHO–ITU H.870 (2018), *Guidelines for safe listening devices/systems*; WHO–ITU global standard for safe listening devices and systems (2019) | Adult weekly dose of 80 dB(A) for 40 h/week | Consumer listening devices are held to a *lower* weekly dose than 85 dB(A) for a full day, which is why the shipped 75 dB protocols matter and why 85 is a ceiling, not a target |
| IEC 62368-1 clause 10.6 and EN 50332-1/-2/-3 (personal audio players and headphones) | Acoustic-energy-source classification; 85 dB(A) is the output level EN 50332 treats as the normal maximum for personal music players | The nearest product standard for a head-worn audio output. Clause designations (RS1/RS2) are **not** quoted here: they were not checked against the text |
| Directive 2003/10/EC (physical agents, noise) | Upper exposure action value 85 dB(A) daily, with peak action values in dB(C) | The EU basis for treating 85 dB(A) as the action level |

**Citations are carried from published summaries and have not been checked against the purchased
standards** (IEC and EN texts are not in the repository). Checking them is `OI-AUDIOHW-12`; per
`CLAUDE.md` §19 that check is an open item, not a register row.

**Weighting.** The standards state dB(A); the `.npps` `volume` field and this ceiling state dB SPL
(unweighted, `NP-NPPS-REF-001` §4.1b). Below about 1 kHz A-weighting only lowers the figure, so an
unweighted 85 never exceeds 85 dB(A) there. Between roughly 2 and 4 kHz A-weighting adds up to about
1.3 dB. Every shipped carrier is 440 Hz. **Decided (§5.3, computed from the A-weighting formula, not
measured):** the maximum is +1.27 dB near 2.5 kHz and 440 Hz is −4.10 dB, so a tone above 1 kHz is bounded
rather than given a lower ceiling (`REQ-AUDIO-25`).

> **`REQ-AUDIO-19`** and **`REQ-AUDIO-20`** are in §3.1. How the ceiling is calibrated and verified, and the margin under it, is §5.3.

### 5.2 The bone-conduction ceiling — the same hearing level, as an absolute force level (Rev 7, `OI-AUDIOHW-02`)

**Why the planar number does not transfer.** Bone conduction bypasses the ear canal, the ear pad's seal
and the cup volume that `REQ-AUDIO-19` is measured in. A planar level in dB SPL says nothing about how
hard a mastoid vibrator drives the skull, and the same drive number gives a different vibration on every
element, isolator and contact force (`OI-AUDIOHW-04`). By `REQ-AUDIO-20` the bone level cannot be a fraction
of a component's range either, so it needs its own absolute unit.

**The unit.** Force level in dB re 1 µN, measured on an artificial mastoid (IEC 60318-6). It is the unit the
bone-conduction reference thresholds are stated in (ISO 389-3), so the ceiling can be tied to hearing
level; acceleration at the mastoid is not, so it is not used.

**The rule.** The bone path is held to the **same hearing level** as the air path, at each carrier
frequency: the hearing level is the planar ceiling's 85 dB SPL minus the air reference threshold at that
frequency, and the bone ceiling is that hearing level above the bone reference threshold. Written out in
`REQ-AUDIO-21`(b). Equal hearing level is a stated choice, derived from the two reference-threshold
standards, with no figure in it that this document invented.

**What is not here.** The dB re 1 µN number. It needs the ISO 389-3 mastoid values and the matching air
values from ISO 389-1/-7 at the carrier frequencies (every shipped carrier is 440 Hz), and those tables
are not in the repository and were not readable from the published summaries. Stating one from memory is
the failure this document exists to avoid. Computing it is `OI-AUDIOHW-13`.

**Standards used.**

| Standard | What it contributes | Checked |
|---|---|---|
| ISO 389-3:2016 | Reference equivalent threshold vibratory force levels for pure-tone bone vibrators, mastoid, with the forehead difference in an informative annex | That it exists and covers the mastoid vibrator, from its published scope. The values were not read |
| IEC 60318-6 | The mechanical coupler (artificial mastoid) the force level is measured on | **Not checked**: the title and number are carried from memory |
| IEC 60645-1:2017, cl. 6.1.5 | Bone vibrator output level for audiometric equipment | That the clause exists, from a table of contents. The limits were not read |
| ISO 389-1/-7 | The air reference thresholds the hearing level is taken against | Not checked. Which one applies depends on the measurement point `OI-AUDIOHW-11` chooses |

**Two things this rule does not settle.** Whether a cue played on both paths at once is bounded by two
ceilings or needs a combined one (`OI-AUDIOHW-13`); and whether an element can deliver the ceiling cleanly,
or distorts and is felt as vibration before it reaches it, which is the element's characterisation
(`OI-AUDIOHW-04`) and is **not** a reason to lower the ceiling here.

### 5.3 Calibrating and verifying the 85 dB SPL ceiling at the ear — the method (`OI-AUDIOHW-11`, decided on paper; nothing here is measured)

`REQ-AUDIO-19`(c) and `REQ-AUDIO-20`(b),(c) need a measurement that does not yet exist: no driver is selected
(`OI-AUDIOHW-04`) and the audio HAL functions trap. This section fixes **how** the ceiling is calibrated and
verified, **what the margin under 85 is made of**, and **whether the weighting needs a bound**, so a unit built
under `OI-AUDIOHW-04` has a method to be measured against. **It sets no size for any margin term and measures
nothing.** The planar path only; the bone path is `REQ-AUDIO-21` and `OI-AUDIOHW-13`.

**What each instrument can and cannot see.**

| | Ear simulator, at its eardrum reference point | In-cup microphone (`REQ-AUDIO-14`) |
|---|---|---|
| Sees | A level at a defined point behind a model of the ear canal and eardrum impedance, on a fixture the cup is mounted on, repeatably and traceably | The pressure in the front volume, on the wearer's own fit, in the field |
| Cannot see | A particular wearer's ear, hair, glasses or fit: it stands for an ear and is not the wearer's | The eardrum. It sits before the canal, so the canal's resonance, the pinna and the canal volume are outside it, and one wearer's cup-to-eardrum difference is not a constant |
| Bone path | Not measured (`REQ-AUDIO-21`) | Blind to it (§6.4.2) |
| Data class | Nothing about a person: **SHDR-class** | Wearer- and fit-dependent: **UHDR** (§3.1, last paragraph) |

**Decision 1 — the ceiling is verified at the ear simulator, and the in-cup microphone is not a safety
measurement.** A table built from the simulator is the only route to a level whose source is traceable and
that carries nothing about a wearer. **No cup-to-eardrum correction is applied or needed**, because nothing
converts an in-cup reading to an eardrum level: the simulator is read at its eardrum point directly, and the
in-cup microphone closes no loop and enforces nothing (`REQ-AUDIO-14`). A correction would be needed only if
the microphone enforced the ceiling at run time. That is **not adopted**, for three reasons from the record: it
would put a wearer-dependent UHDR figure into a safety control; a correction fixed on a simulator is the
simulator's and not the wearer's; and a loop is what `REQ-AUDIO-14` excludes. What the microphone adds at the
factory is a **reference reading**, taken at the same instant as the simulator reading, so the unit's
microphone-to-eardrum-point relationship on the simulator is on file and the microphone's own function is
cross-checked. In the field it can be compared only against a relationship made on a simulator fit, so a
deviation cannot be told from a different fit. It therefore cannot bound drift (m3 below), and whether it
flags a large change is `OI-ACC-04`'s question and not this section's.

**Decision 2 — the margin is a structure of named terms, and its size is open.** The level delivered at the
simulator is the commanded level plus an error. The error has the sources below, each bounded by a different
measurement, and each is a number nobody has.

| Term | What it is | Bounded by | Size |
|---|---|---|---|
| **m1** | Expanded uncertainty of the ear-simulator chain: reference calibrator, microphone, and repeatability of seating the cup on the fixture. A verification pass on the **same** chain does not remove its systematic part | The chain's calibration certificate and a seating-repeatability test on a built unit | **Open** |
| **m2** | Table representation error: the table has finitely many frequency and level points, and the author picks carriers and levels between them | The verification pass at points not used to build the table (`REQ-AUDIO-22`(e)) | **Open** |
| **m3** | Drift between calibration and use: temperature, driver ageing, mesh and foam state (`REQ-AUDIO-07`, §6.1) | Lifetime and temperature characterisation of the selected driver (`OI-AUDIOHW-04`) | **Open** |
| **q** | Quantisation: codec gain steps are not the 0.5 dB steps of `level_half_db` | **Zero by construction**: every HAL rounds level-to-drive toward lower output (`REQ-AUDIO-23`(c)) | 0 |

Unit-to-unit spread is **not** a term: the per-unit table exists to remove it, and what survives is m1 and m2.
It matters elsewhere, because the driver and amplifier must reach the commanded cap with headroom across that
spread, which is `OI-AUDIOHW-04`'s to show.

**Decision 3 — the terms add arithmetically.** `REQ-AUDIO-19` says *never exceeds*. A root-sum-square
combination states a probability of exceeding, and no distribution exists for any term. The sum of the bounds
is the only combination that needs none. Its cost is a larger margin, accepted until a measurement shows two
terms are not independent in the bad direction.

**One term is deliberately absent: simulator to wearer.** A real ear differs from the simulator and no factory
measurement bounds that. It is not added to M and not hidden: `UC-134` records that the simulator's level
stands for the wearer's by assumption, and `OI-RISK2-02` owns whether it needs a term.

**Decision 4 — the weighting needs a bound on the tone frequency, not a frequency-dependent ceiling.** The
standards in §5.1 state dB(A) and the ceiling is unweighted SPL. A-weighting is, from the formula of
IEC 61672-1 (the public formula; its constants are carried from published summaries and **not checked against
the purchased text**, which `OI-AUDIOHW-12` now also covers):

    R_A(f) = 12194² f⁴ / [ (f² + 20.6²) · sqrt( (f² + 107.7²)(f² + 737.9²) ) · (f² + 12194²) ]
    A(f)   = 20 · log10 R_A(f) + 2.00 dB

Computed from it (not measured, and not read from a table):

| Frequency | A(f) |
|---|---|
| 200 Hz | −10.85 dB |
| **440 Hz** (every shipped carrier) | **−4.10 dB** |
| 1,000 Hz | 0.00 dB (the +2.00 normalises it) |
| 2,000 Hz | +1.20 dB |
| **about 2,500 Hz** (the maximum) | **+1.27 dB** |
| 4,000 Hz | +0.96 dB |
| about 6,100 Hz | 0.00 dB, and negative above |

So a 440 Hz carrier at 85 dB SPL is **80.9 dB(A)**, and a tone near 2.5 kHz at 85 dB SPL is **86.3 dB(A)**,
above the 85 dB(A) action level §5.1 cites. For an ideal 1/f (pink) and 1/f² (brown) spectrum over 20 Hz to
20 kHz the same formula gives −12.8 and −23.7 dB, which shows the sign only: **no noise spectrum is specified**
(`OI-AUDIOHW-04`), so the noise commands are measured at both weightings (`REQ-AUDIO-22`(b)).

**Does something real fail without a bound? Yes, narrowly.** `carrier_hz` is a 16-bit field. The compiler only
clamps it into the field's width (`min(65535)` in `common/npps-core/src/compiler.rs`), and neither the
compiler, the hub driver nor `NP-NPPS-REF-001` bounds it otherwise, so any signed protocol may command a tone
anywhere in that range. §5.1 chose 85 so that a session can never, by level alone, be louder than the level at
which the cited standards begin to require action. That property is false between about 1 kHz and 6.1 kHz, by up
to 1.27 dB. A tone far above half the codec's sample rate would also alias to a different, audible frequency;
the codec's rate is not recorded, so the size of that is unknown.

**Why a bound on frequency and not a ceiling that falls with frequency.** Both refuse and neither reshapes. The
bound is one comparison, identical in the Rust compiler and the C hub driver. A frequency-dependent ceiling
would carry the weighting curve into both, a derived function written twice in two languages (§20's
one-home rule is the same idea). The bound sits where the nominal weighting is zero, so everything under it is
at or below 85 dB(A) with no curve. And **nothing in the record needs a carrier above 1 kHz**: every shipped
protocol sets 440 Hz, the app default is 200 Hz, and `NP-NPPS-REF-001` §4.7 gives the field no purpose beyond
"Hz of carrier tone". The cost is the 1 to 6.1 kHz range, which nothing uses. If a protocol ever needs it, the
alternative is one derivation away (the ceiling becomes 85 − A(f) wherever A(f) is positive). **The bound
restricts a field an author could set before, so the principal confirms it** (`OI-AUDIOHW-11`).

| ID | Requirement | What fails if it is not met | Traceable to |
|---|---|---|---|
| **REQ-AUDIO-22** | **The ceiling is verified, and the level-to-drive table is measured, at the eardrum reference point of an ear simulator, on every unit, for each cup mounted on the simulator's fixture as worn.** (a) The device is an ear simulator that models the ear canal and eardrum impedance and states a level at a defined eardrum reference point; a flat coupler and the in-cup microphone are not accepted for the table or the verification, and the exact standard and configuration for a circumaural cup (IEC 60318-4 with a pinna simulator, or IEC 60318-1) is chosen against the standards' text (`OI-AUDIOHW-11`); (b) the quantity is unweighted SPL in steady state: for a pulsed (isochronic) command the level during the pulse and never an average over its off time, for a binaural command each cup's own tone, and for a noise command both the unweighted broadband level and the A-weighted level are recorded and neither may exceed 85; (c) the fit is the best seal the cup makes on the fixture, unless characterisation shows a permitted frequency whose level rises with a leak, in which case the worst fit for that frequency is used; (d) the table covers the frequencies and levels the product permits, and a command outside the measured domain is refused, never extrapolated; (e) verification is a separate pass, at points not used to build the table, and a unit is accepted only if every measured level plus m1 plus m3 is at or below 85 dB SPL | Without a defined point and quantity, "85 dB SPL at the ear" has no measurement, and a time average of a pulsed tone, or a coupler with no ear impedance, reads lower than the ear receives. Without (d), a level at an unmeasured frequency is a guess, and a table extrapolated past its range can deliver more than it says. Without (e), the table's own error is never observed | `REQ-AUDIO-19`(c), `REQ-AUDIO-20`(b),(c); §5.1 (the dB(A) basis of the noise check; no exposure-duration model is set, so an averaged level would be a dose model by another name). The simulator standards are carried from memory and are not checked (`OI-AUDIOHW-12`) |
| **REQ-AUDIO-23** | **The command ceiling is the delivered-level ceiling less a guard band: C = 85 − M, with M at least m1 + m2 + m3.** (a) The refusals of `REQ-AUDIO-19`(a),(b) are at C and not at 85, so a command at C delivers at most 85 across the terms; (b) the terms are added arithmetically, not statistically; (c) every conversion from level to drive in a HAL rounds toward lower output; (d) the sizes of m1, m2 and m3 come from measurement, and M is entered in `docs/status/unjustified-choices.md` (`UC-133`) until it has a derivation. **Until M is set the shipped ceiling (`NP_AUDIO_MAX_LEVEL_HALF_DB`, 85 dB SPL) is a zero-margin stand-in that does not meet (a), and no unit can be released on it.** No output exists today, because the HAL functions trap | Without a margin, a command at 85 delivers 85 plus the calibration's error on a unit whose error is positive, and `REQ-AUDIO-19` is broken by exactly the amount nobody measured. Without (c), a codec step that rounds up delivers more than commanded | `REQ-AUDIO-19` (*never exceeds*); `REQ-AUDIO-20`(b); `CLAUDE.md` §3 (a ceiling refuses, never reshapes) |
| **REQ-AUDIO-24** | **The calibration table is a record of the device's condition and of nothing about a wearer, and the planar path plays nothing without a valid one.** (a) It is **SHDR-class**: measured on a simulator before any wearer exists, linked to the device and not to a person, and no value from the in-cup microphone or any wearer measurement is written into it or alters it; (b) it is bound to the chain it was measured on (the hub, and the cup where the cup carries an identity, `OI-AUDIOHW-07`) and integrity-checked, and a missing, mismatched or corrupt record means **the planar path refuses to output**, never a datasheet default; (c) it is written only by factory or service calibration, never by a session, a protocol or the app's ordinary paths; (d) it lives with the factory calibration already kept in the hub's Config/Calibration partition (`NP-FW-NVRAM-001` §2, row 3), unless `OI-AUDIOHW-07` makes the cup a separately replaceable part, in which case the transducer's half travels with the cup | Without (a), a wearer-derived figure sits in a device-condition store and the SHDR/UHDR line (`CLAUDE.md` §5.1) is crossed, or a wearer's fit sets a safety control. Without (b), a defaulted or stale table maps a command to a drive nobody measured, which is how a command at 85 reaches an ear above it. Without (c), a session can rewrite the one control the audio path has, because the safety MCU holds no enable for it (§5) | `CLAUDE.md` §5.1 (SHDR defining test, rule 1), §4.2; `REQ-AUDIO-09`; `NP-FW-HUB-001` §9 (no stored value widens a grant); `NP-FW-NVRAM-001` §2 |
| **REQ-AUDIO-25** | **Every tone frequency the planar path is commanded to produce is at most 1,000 Hz: `carrier_hz`, and for a binaural or isochronic command `carrier_hz` plus the beat or pulse rate.** The compiler refuses a protocol above it before signing and the hub driver refuses the command before any state or output changes. Refused, never clamped | A-weighting adds up to 1.27 dB between about 1 kHz and 6.1 kHz, so an unweighted 85 dB SPL tone there is up to 86.3 dB(A), above the 85 dB(A) action level §5.1 gives as the reason for 85. The field is otherwise unbounded to 65,535 Hz | §5.1 (85 chosen at the action level in dB(A)); NIOSH Pub. 98-126 and Directive 2003/10/EC (dB(A)); the IEC 61672-1 weighting, computed above (`OI-AUDIOHW-12` checks it); `CLAUDE.md` §3 (a ceiling refuses, never reshapes). The 1,000 Hz is not chosen: it is where the nominal weighting is zero |

**Not built:** no compiler or driver check, no calibration record, no table. `REQ-AUDIO-22` to `-25` are
untested by design (`CLAUDE.md` §21: a requirement for an unbuilt feature stays untested until the code that
meets it is written). The `carrier_hz` row of `NP-NPPS-REF-001` §4.7 and `common/npps-core` carry
`REQ-AUDIO-25` when it is implemented.

---

## 6. Consumables

`CLAUDE.md` §2.3 (Rev 48) admits exactly two trigger kinds for a consumable replacement prompt — a
**condition measurement** of the part, or an **exposure count** with the degradation mechanism named
and what the count cannot see stated — and holds that the trigger *kind* and the threshold's
*provenance* are two separate claims. Both of A11's consumables are assessed against that rule here.
**Neither is changed by this document**; prices, intervals and margins stay where they are stated,
in `docs/reference/commercial-model.md` §2.3.

### 6.1 Foam ear pad (set) — exposure count, placeholder threshold

Trigger kind is settled: an **exposure count** (session count from the hub's SHDR-class
`CONSUMABLE_STATUS`), with the mechanism named — compression set under wear — and the blind spots
stated (tear, contamination, storage set). The **threshold** is an unvalidated placeholder and is
labelled one, carried by `OI-ACC-04`. `OI-ACC-02` closed on this on 2026-09-15.

**This document adds nothing to that disposition and deliberately does not re-derive the
threshold.** ~~What it adds is the hardware consequence: closing `OI-ACC-04` needs a foam whose
compression-set behaviour is characterised, and **no foam is specified** (§8).~~ *Superseded by Rev 2
below: the threshold is no longer a compression-set figure.*

**Rev 2 (2026-09-23) — the foam's prompt measures loss of seal, and needs a microphone this
document does not have.** By principal decision, comfort and perceived hygiene are the wearer's to
judge, and the foam can be replaced voluntarily at any time; the prompt covers only **loss of
function**, which for the ear pad is **loss of seal** (bass level and passive isolation). The
trigger that measures it is the in-cup (feedback) microphone of noise cancelling, which the
programme intends to add for session quality and which **does not exist in this document**
(`OI-AUDIOHW-08`). Until it does, the placeholder exposure count stands. The threshold becomes a
seal loss in dB derived from what a session needs, so it no longer depends on the foam material
(`OI-AUDIOHW-06`) or the clamping pressure (`OI-AUDIOHW-05`) — both still matter to the part.
Full disposition: `docs/status/pending-decisions.md`, `OI-ACC-04`.

**Rev 3 (2026-09-24):** the seal measurement needs an in-cup microphone, **not** noise cancelling —
a probe-tone transfer measurement is a block measurement that needs no anti-noise loop. The options
and a recommendation are §6.4.

**Rev 4 (2026-09-24): option B taken (principal).** The in-cup microphone is now required
(`REQ-AUDIO-14`); the foam's `150` still stands until it is specified, implemented and the dB
criterion is set (`OI-ACC-04`).

### 6.2 Mesh frame (pair) — the trigger named in the record does not exist

`REQ-AUDIO-07` says driver impedance detects fouling. It does not, today:

- `np_mod_audio_hal_mesh_impedance()` is an **unimplemented HAL stub** (`OI-AUDIO-08`);
- its only call site, `np_mod_audio_telemetry()`, **discards the value** — `(void)mesh_ohm;`;
- there is **no `ConsumableKind` case** for the mesh frame on either app platform.

So the shipped prompt is the Interval column — *"Annual"* — which is a calendar, and
`CLAUDE.md` §2.3 forbids one. This is `OI-ACC-05`, raised 2026-09-15, and it is **the worse case of
its class precisely because the Notes column sounds like a measurement.**

> **`REQ-AUDIO-13`** — The mesh frame's replacement trigger is a **condition measurement of the
> mesh** (`CLAUDE.md` §2.3 kind 1). Closing `OI-ACC-05` by substituting an exposure count is a
> different claim and must be argued as one, because a session count cannot see the two mechanisms
> that actually foul a mesh — sweat/sebum ingress and handling damage — neither of which scales with
> session time.

### 6.3 The 40 dB RF claim on a consumable — a finding

The mesh frame is specified as *silver-coated nylon, **40 dB RF***
(`docs/reference/modality-stack.md`). **That figure is outside every audited shielding claim in the
programme:**

| Where a shielding figure normally lives | Does the mesh appear? |
|---|---|
| `CLAUDE.md` §4.3 four-layer stack (five-layer before 2026-09-23) | **No.** Layers 1, 2, 3 and 5 are CFRP, mu-metal, palladium-coated polyester and port filters (Layer 4, absorber foam, was deleted 2026-09-23 — `REQ-CAV-04`). The mesh is none of them |
| `docs/reference/hardware-detail.md` §4.3 per-layer dB | **No** |
| `NP-BIB-EMF-001` per-layer value audit | **No row** |
| `NP-DT-001` `DI-PERF-22` design trace | **No** |

Three consequences follow, and they compound:

1. **It is an unmeasured dB figure**, like every other in the set (`CLAUDE.md` §4.3: *no dB figure
   may be published as measured*) — but unlike the others it has never been through
   `NP-BIB-EMF-001`'s question of *what the layer buys*.
2. **It is on a part the user removes.** Every other shielding element is assembled once. A
   shielding claim whose element is user-serviceable has a failure mode the stack does not:
   refitted incorrectly, refitted with a third-party part, or run fouled.
3. **Its degradation trigger is the one that does not work** (§6.2). `REQ-AUDIO-07`'s claim that
   impedance detects *"both acoustic degradation AND RF shielding loss simultaneously"* is, as
   of today, two unverified claims resting on an unimplemented stub.

> **`OI-AUDIOHW-03`** — Decide whether the mesh's 40 dB is a shielding claim the programme makes or
> a supplier datasheet figure that should not be quoted as one. If it is a claim, it belongs in
> `NP-BIB-EMF-001`'s audit and in the `CLAUDE.md` §4.3 stack with a layer identity; if it is not, it
> should stop appearing next to figures that are. **This document does not decide it** — the §4.3
> stack is locked and adding to it is a `CLAUDE.md` decision, not a specification edit.

### 6.4 In-cup microphone — the options for `OI-AUDIOHW-08` (Rev 3 analysed; Rev 4 **DECIDED: option B**)

> **Decision (principal, 2026-09-24): option B — one in-cup microphone per side, for measurement
> only, with no anti-noise.** It is bound as `REQ-AUDIO-14`–`16` (§3.1). The analysis below is kept
> as it was written, because it is the record of why. What moved with the decision: the probe-tone
> hazard in §7 is now live and the anti-noise one is not; the §6.4.6 privacy position is adopted as
> `REQ-AUDIO-15`; the first §6.4.7 condition is adopted as `REQ-AUDIO-16`. **The second — the mic on
> the ear side of the mesh — stays a condition**, because the mesh candidate it serves (`OI-ACC-05`)
> is not adopted. Microphone selection, including analogue vs PDM, is `OI-AUDIOHW-09`; the firmware
> is `OI-AUDIOHW-10`. The route to noise cancelling stays as §6.4.8 states it: a stated requirement
> first, then A as an increment on B.

`OI-AUDIOHW-08` asks whether to add noise cancelling with an in-cup (feedback) microphone, or not.
It reverses a property the record states — *there is no microphone anywhere in the design* — so it
is a principal decision, and this section does what `NP-REG-UPG-001` did for `OI-TACSDRV-06`:
it states the options, what each costs and buys, and recommends one. **Nothing below is adopted.
No requirement is added to §3, no value is set, and the foam's `150` is unchanged.**

#### 6.4.1 The finding that reframes the question — the measurement needs the microphone, not the noise cancelling

The item couples two things that separate cleanly:

- **What `OI-ACC-04` needs is a transfer-function measurement:** the driver-to-in-cup-mic path at
  low frequency, taken with a short probe tone at session start (§6.1; `OI-ACC-04`'s principal
  decision already names the probe tone). That is a **block measurement**. It tolerates any
  latency, runs once per donning, and needs a microphone, a probe signal and a few hundred
  milliseconds of arithmetic. It does not need anti-noise.
- **What noise cancelling needs is a closed control loop** that drives anti-noise from the same
  microphone continuously, for the whole session. Adaptive ANC happens to estimate the same path
  as a by-product — which is why the principal decision reached the measurement through ANC — but
  it is one way of obtaining it, not a precondition for it.

So there are three options, not two:

| | **A — feedback (or hybrid) ANC** | **B — in-cup mic, sensing only** | **C — no microphone** |
|---|---|---|---|
| What is added | Mic per cup + a low-latency anti-noise loop + playback compensation + instability protection | Mic per cup; probe-tone measurements only; the driver never plays anti-noise | Nothing |
| Foam seal (`OI-ACC-04`) | Measured | **Measured** — the same path, by probe tone | Not measured; `150` stays a placeholder with no route to a measurement, and the compression-set route the principal decision withdrew would have to be reopened |
| Mesh fouling candidate (`OI-ACC-05`) | Candidate, if the mic sits on the ear side of the mesh | **Candidate**, same condition | Driver impedance only (§6.2 — weak for a planar driver) |
| In-cup SPL for `OI-AUDIOHW-01` (§6.4.2) | Yes, but the delivered level now depends on the loop's state | **Yes** | No |
| Session quality (ambient noise) | Improved at low frequency | Passive isolation only, as today | Passive isolation only, as today |
| New acoustic hazard (§6.4.4) | **Yes** — anti-noise output and loop instability | Probe tone only, bounded by `OI-AUDIOHW-01` | None |
| Conflict with the entrainment content (§6.4.3) | **Yes** — the carriers sit in the cancellation band | None | None |
| Forces `REQ-AUDIO-04a` (§6.4.5) | **Yes** — a low-latency path the hub's SAI/DMA chain cannot provide | No | No |
| Privacy exposure (§6.4.6) | Continuous capture for the whole session, confinable to the loop | Windowed capture during the probe only | None |

#### 6.4.2 A use the record had not noticed — the microphone is also what `OI-AUDIOHW-01` is missing

*(Rev 6: `OI-AUDIOHW-01` is decided and the wire field is absolute, §5.1. The analysis below is as written in Rev 3.)* `OI-AUDIOHW-01` recorded that no acoustic output ceiling existed and that `volume_pct` is an
**uncalibrated** 0–100 wire field. Any ceiling written against `volume_pct` has to assume a driver
sensitivity, a cup and a fit; the level actually delivered to a given wearer varies with all three,
and most of all with the seal this item is about. An in-cup microphone measures the level in the
wearer's own cup on the wearer's own fit — which is what a ceiling needs to be enforced as a
measured quantity rather than a table lookup.

Two limits on this, stated so it is not over-read: an in-cup microphone is **not at the eardrum**,
so a cup-to-eardrum correction is still needed and is itself a derivation `OI-AUDIOHW-01` would
own; and it **cannot see the bone-conduction path** at all, so it does nothing for `OI-AUDIOHW-02`.
This strengthens the case for having a microphone (A or B). It is no argument for anti-noise.

#### 6.4.3 Anti-noise and the entrainment content occupy the same band

Feedback ANC does its work at low frequency, and that is where the shipped content lives: **every
predefined protocol that sets a carrier sets `carrier_hz: 440Hz`** (`protocols/predefined/*.npps`),
and the app's default is 200 Hz (`common/types/protocol.ts`). The feedback microphone hears the
playback as well as the ambient noise, and a feedback loop cancels whatever it hears unless the
playback is subtracted from its error signal first (playback compensation). That subtraction is
only as accurate as the loop's model of the driver-to-mic path — **the same seal-dependent path**
§6.1 proposes to measure. So under A:

- the level at which each ear receives its carrier depends on the loop's state and on the seal,
  per side, and the two sides can differ — which is a binaural-content property, not only a
  loudness one;
- an adaptive loop is time-varying, and whether its adaptation interacts with isochronic pulsing
  at entrainment rates (2–40 Hz in `k_band_beat_mhz`) or with the EEG-adaptive retune every
  `NP_AUDIO_ADAPT_INTERVAL_MS` is unknown.

None of this makes A impossible — commercial ANC headphones play music through feedback loops — but
it makes A a change to **what the modality delivers**, which would need its own verification that
the content reaching the ear is the content the protocol commanded. B and C carry none of it.

#### 6.4.4 Acoustic safety

Under A the driver plays anti-noise in addition to the programme, so the cup's output is no longer
set by `volume_pct` alone, and a feedback loop whose plant changes — the cup lifted, the seal
broken, a hand cupped over it — can go unstable and produce a sustained tone at the ear. Both need
controls: an output limiter and an instability detector that opens the loop. **Neither has a
requirement, and the ceiling either would enforce does not exist** (`OI-AUDIOHW-01`). Under B the
only added acoustic output is the probe tone — short, once per donning, and required to sit under
whatever ceiling `OI-AUDIOHW-01` sets. Under C nothing changes.

**`REQ-AUDIO-12` is not triggered by any option.** A microphone puts no current, light or field
into the assembly, and anti-noise is acoustic output from the transducer that already exists. The
acoustic hazards above belong in `OI-RISK2-02`'s analysis (§7), not in `CLAUDE.md` §4.2's table.

#### 6.4.5 Latency — A cannot run on the architecture the record has

`REQ-AUDIO-04` places origination, mixing and synthesis in the hub, through the hub I²S codec via
SAI (`NP-FW-HUB-001` §8.5), with the RT1062 processing audio in DMA blocks under FreeRTOS. A feedback
loop's usable bandwidth is limited by its delay: delay in the loop is phase lag, and phase lag at
the loop's crossover is what makes it unstable. A block-processed path through a processor and a
cable is the wrong kind of path for it. **A therefore needs a dedicated low-latency loop** — a
fixed-function ANC engine in a codec, or an analogue loop — sitting with the microphone, the driver
and its amplifier. That decides `REQ-AUDIO-04a` (*whether any amplification sits in the cup*) by
force, and with it the cup's power, its thermal behaviour and the conductor count of
`OI-AUDIOHW-07`'s cable. B needs only one microphone signal per cup returned to a hub codec input,
processed as a block; it leaves `REQ-AUDIO-04a` where it is. Whether the hub codec (`NP-HW-HUB-001`)
has two spare inputs is not recorded, and is B's one hub-side question.

#### 6.4.6 Privacy — a microphone is a new class of sensor, and bystanders are nobody's consent subject

A microphone in the cup hears the wearer's own voice (strongly, through the occluded ear) and the
voices of anyone nearby. The second is a problem the §5/§6 architecture has no place for:
**`CLAUDE.md` §6.0 has exactly two consent subjects — the warranty owner and the wearer — and a
bystander is neither.** No consent flow can cover them. So raw microphone audio must be excluded
from the product **structurally**, not by policy, in the same sense that `SHDRUploader` has no
reference to `ConsentStore`: there is no path by which it could be stored, logged, streamed in Mode
1, written to eMMC, or transmitted, rather than a setting that says it is not.

- **Under A** the loop can be confined to the in-cup ANC engine, so raw audio **never reaches the
  RT1062**; the hub receives, at most, a derived number. That is the stronger structural position,
  and it is A's one privacy advantage.
- **Under B** raw audio reaches the RT1062 for the probe window only. The structural control is that
  the microphone is **unpowered outside the measurement window**, the capture buffer is reduced to
  the derived figure and cleared in the same call, and the audio module exposes no interface that
  returns samples. Whether that is enforced by a CI gate in the manner of
  `scripts/check-redaction-shape.ts` is a decision for the item that implements it.
- **Under both,** the derived seal score is **presumptively UHDR** (`CLAUDE.md` §5.1, *when in
  doubt → UHDR*): it depends on the wearer's head, hair and glasses, and a longitudinal series of it
  describes the person as well as the foam. It is computed and consumed on-device, and the foam
  prompt is raised on-device from it. **Nothing about it is added to SHDR by this section**, and
  `CONSUMABLE_STATUS` stays the SHDR-class session count it is today. A later proposal to report a
  derived "foam worn" flag to the fleet has to make the positive demonstration §5.1 rule 1 asks for.

An EMC consequence belongs with this, because it is a property of the microphone type rather than
of the option: a **digital-output (PDM) MEMS microphone** runs a clock of the order of a megahertz
in the cup, beside the temporal EEG sites; an **analogue-output** microphone runs none. A new clocked
part inside the head-worn envelope is a source for `NP-EMC-CAV-001`'s source analysis, and the
microphone selection should be made knowing that. This section does not choose.

#### 6.4.7 What both A and B need from the cup, and that C does not

Two placement conditions, carried as **conditions on `OI-AUDIOHW-04` and `-05` if A or B is taken**,
not as requirements — nothing requires them until one is:

| Condition | What fails without it | Traceable to |
|---|---|---|
| **The seal must be the dominant leak.** The measurement sees the foam only to the extent that the foam leak dominates every other leak from the front volume; a vented or open-back cup adds a designed leak in parallel. A fully open-back cup is the case where the measurement presumptively does not work; whether a given vent leaves it working is acoustic engineering's to show under `OI-AUDIOHW-04` | The foam trigger measures the vent, not the foam, and `OI-ACC-04` does not close | Derivation from the measurement's physics (§6.1) |
| **The mic sits in the front volume, on the ear side of the mesh**, for `OI-ACC-05`'s candidate to exist | Mesh fouling is outside the measured path; `OI-ACC-05` falls back to driver impedance | `OI-ACC-05` candidate (2026-09-23) |

Whether A-or-B's microphone and cable land on both tiers is `REQ-UPG-03`'s question: A11 is a T1
module from Home Standard up (§2.1), and a change to its cable and connector (`OI-AUDIOHW-07`) is a
change to a module interface that must land on both tiers or neither. **No cost is stated for any
option.** Every T1 configuration is gross-margin negative and every cost figure is a floor
(`CLAUDE.md` §2.1); a BOM delta for a microphone or an ANC codec is quoted only through
`docs/np_cost_001.md`.

#### 6.4.8 Recommendation — **B**, and state a requirement before A (**taken, Rev 4**)

**Recommended: B — add one in-cup microphone per side, for measurement only, with no anti-noise.**

1. **B delivers everything the item was raised for that has a requirement behind it.** The foam's
   condition measurement (`OI-ACC-04`, by principal decision), the mesh-fouling candidate
   (`OI-ACC-05`), and — new here — a measured in-cup level for `OI-AUDIOHW-01`, which has no other
   route to an enforceable ceiling.
2. **Anti-noise is the part with nothing requiring it.** *Session quality* is the stated purpose, and
   no document in the set states an ambient-noise target for entrainment, a level at which ambient
   noise degrades a session, or evidence that it does. Under `CLAUDE.md` §18 that is the test a
   constraint must pass before it enters a controlled document, and ANC adds three: a continuous
   loop, an output limiter, and an instability detector. The item's own cost column — acoustic
   safety, content interaction, latency, power, cable — is almost entirely A's.
3. **B does not foreclose A.** B's microphone sits where A's error microphone would (§6.4.7). What A
   adds is the loop electronics, which §6.4.5 shows is a hardware change of its own; B is **not**
   required to carry A-ready electronics, because nothing requires A — making it a condition would
   be the unrequired constraint §18 forbids.

**The route to A, if the principal wants noise cancelling for session quality:** first state the
requirement — what fails, for which protocols, at what ambient level — and its traceability; then
take A as an increment on B, carrying §6.4.3–§6.4.5 as its verification burden.

**If C is chosen instead,** the foam prompt stays an exposure count permanently, `OI-ACC-04` needs
a threshold route again (the compression-set route its principal decision withdrew), and
`OI-AUDIOHW-01` has no measured level to enforce against. Those are the costs C carries.

**What the decision must also settle, whichever option is taken:** the privacy position of §6.4.6
(stated here, not adopted), and the microphone type's EMC consequence.

---

## 7. Hazard-analysis inputs

`NP-RISK-002` §4 records that A11 has **never had a hazard analysis in any revision of the risk
file**, and `OI-RISK2-02` was blocked behind `OI-ART-04` — *"they have no specification to
analyse."* **That block is lifted by this document.** The analysis is still owed; what follows is
the input list, not the analysis.

| Candidate hazard | Why it is on the list |
|---|---|
| Acoustic overexposure (planar path) | No SPL ceiling exists anywhere (§5, `OI-AUDIOHW-01`) |
| Bone-conduction overexposure | Separate limit, separate transfer path. **Rule set (Rev 7, §5.2, `REQ-AUDIO-21`); the figure and the field that carries it are not (`OI-AUDIOHW-13`)** |
| Mesh refitted wrongly, or a third-party mesh fitted | User-serviceable part carrying an RF claim (§6.3) |
| Mesh fouling undetected | Trigger unimplemented (`OI-ACC-05`); both the acoustic and the RF consequence go unflagged |
| Bayonet failure in service | The part exists **because** the plastic predecessor flattened at 500–1,000 cycles; the replacement has no cycle rating of its own |
| Pinch or entrapment at the bayonet | Head-worn rotating mechanical interface, user-actuated |
| ~~Anti-noise output and feedback-loop instability (sustained tone at the ear)~~ | **Not applicable — option A not taken (Rev 4); `REQ-AUDIO-14` forbids anti-noise.** Kept so the row's retirement is auditable, and live again if A is ever taken |
| Probe-tone exposure | **Live (option B, Rev 4)** (§6.4.4) — bounded by the ceiling `OI-AUDIOHW-01` has not set |
| Ear-pad contact dermatitis / biocompatibility | Prolonged skin contact; no material is specified (§8) |
| Mass and centre-of-mass contribution to fit | A11 hangs off the fit system (`CLAUDE.md` §4.4) and its mass is nowhere stated |

> This list is deliberately **not** scored. `NP-RISK-002` OI-RISK2-01 records that disposition and
> re-scoring are separate acts; the same discipline applies to creating entries.

---

## 8. What this document does **not** specify, and why

Naming the boundary is the point of the section, exactly as `NP-HW-TCAP-001` §8 does for the cap.

| Not specified | Why | Where it goes |
|---|---|---|
| Driver impedance, sensitivity, frequency response, magnet grade, diaphragm material | Never stated in any document; selecting them is EE + acoustic engineering | **`OI-AUDIOHW-04`** |
| Bone-conduction element type, drive level, contact force, isolator durometer | As above | **`OI-AUDIOHW-04`** |
| Cup body geometry, ear-pad geometry, clamping force, mass | No geometry exists for A11 anywhere | **`OI-AUDIOHW-05`** |
| Ear-pad and isolator materials, and their biocompatibility basis | Prolonged skin contact with no material of record | **`OI-AUDIOHW-06`** |
| Bayonet interface geometry, engagement torque, cycle rating | The record gives a material and a rationale, not an interface | **`OI-AUDIOHW-05`** |
| Mesh aperture, open area, coating weight, and the basis of 40 dB | §6.3 | **`OI-AUDIOHW-03`** |
| Acoustic output ceiling and exposure model | §5 | **`OI-AUDIOHW-01`**, **`OI-AUDIOHW-02`** |
| Microphone part, type (analogue or PDM), sensitivity, exact position, and its conductors in the cable | `REQ-AUDIO-14`–`16` (§3.1) fix what it is for, where it sits and how its signal is contained; nothing selects a part | **`OI-AUDIOHW-09`** |
| Seal-measurement firmware: microphone HAL, probe signal, capture window, the seal score | No HAL entry exists; `np_mod_audio.c` has no microphone path | **`OI-AUDIOHW-10`** |
| Cable, connector and strain relief to the hub | No connector is selected for any head-worn accessory; the same gap `NP-HW-TCAP-001` §8 records for the T2 cap | **`OI-AUDIOHW-07`** |

### 8.1 FAI readiness — the honest verdict

Against `NP-FAI-001` §2:

| | Verdict |
|---|---|
| **F1** — governing specification `BASELINED` or `ACTIVE` | **FAILS.** This document is DRAFT, and says in its own status line that it is requirements-grade |
| **F2** — every inspected dimension is dimensioned, or gated on a named item | **FAILS.** No dimension exists to inspect; §8 gates all of them |
| **F3** — the manufacturing process has a supplier category in `NP-PROC-SUP-001` | **NOT ASSESSABLE** until A11.1–A11.5 name processes |
| **F4** — failure modes are in a risk register | **FAILS**, but is now *unblocked* — §7 is the input list and `OI-RISK2-02` is live rather than blocked |

> **`NP-FAI-AUDIO-001` therefore still cannot be written, and `NP-ART-001` §3.2 keeps it as a named
> absence.** What changes is the *reason*: it is no longer *"no owning specification document exists
> at all"* but *"the owning specification is DRAFT and carries no dimensioned geometry."* That is a
> smaller absence and a differently-owned one, and stating it precisely is the point.

---

## 9. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-AUDIOHW-01** | **DECIDED 2026-11-09 (principal, Rev 6): 85 dB SPL at the ear on the planar path, absolute scale, level ceiling only — bound as `REQ-AUDIO-19`/`-20` (§5.1).** No exposure-duration or daily-dose model, because the device cannot know exposure outside sessions. Succeeded by `OI-AUDIOHW-11` (calibration and verification at the ear) and `OI-AUDIOHW-12` (check the citations). **As raised:** no acoustic output ceiling existed, and `volume_pct` was an uncalibrated 0–100 wire field. The in-cup microphone (`REQ-AUDIO-14`) gives a measured in-cup level per wearer and fit (§6.4.2), not at the eardrum and blind to bone conduction; the ceiling also bounds the seal probe tone | Systems + Clinical | — (decided) |
| **OI-AUDIOHW-02** | **DECIDED as a rule 2026-11-09 (principal, Rev 7): the bone path is held to the same hearing level as the planar path, stated as an absolute force level in dB re 1 µN on an IEC 60318-6 artificial mastoid — bound as `REQ-AUDIO-21` (§5.2).** The figure and the wire field are `OI-AUDIOHW-13`. No firmware gating in the interim: the bone HAL is an unbuilt stub and no element is selected. **As raised:** no bone-conduction drive ceiling existed, and an air-conduction limit does not transfer to it unexamined | Systems + Clinical | — (decided; figure `OI-AUDIOHW-13`) |
| **OI-AUDIOHW-03** | **The mesh frame's 40 dB RF figure is a shielding claim outside the audited stack (§6.3)** — absent from `CLAUDE.md` §4.3, from `docs/reference/hardware-detail.md` §4.3, from `NP-BIB-EMF-001`'s per-layer audit and from `NP-DT-001` `DI-PERF-22` — on a **user-replaceable** part whose fouling trigger is unimplemented (`OI-ACC-05`). Decide whether it is a programme claim or a supplier figure, and stop quoting it beside audited ones either way | Systems + EMC | Shielding claim integrity; `NP-BIB-EMF-001` completeness |
| **OI-AUDIOHW-04** | Select and specify the planar magnetic driver and the bone-conduction element (electrical, acoustic and mechanical parameters). Nothing beyond *"40 mm planar magnetic"* and *"piezoelectric at the mastoid"* exists | EE + Acoustic | A11 tooling; `NP-FAI-AUDIO-001` F1/F2 |
| **OI-AUDIOHW-05** | Specify cup body, ear-pad and bayonet geometry, clamping force and assembly mass. The bayonet's material decision (`docs/reference/durability-maintenance.md`) has no interface behind it, and A11's mass loads the `CLAUDE.md` §4.4 fit system with a figure nobody has written down | ME | A11 tooling; fit verification |
| **OI-AUDIOHW-06** | Specify ear-pad and silicone isolator materials and their biocompatibility basis (prolonged skin contact) | ME + Quality | Hazard analysis; T1 release |
| **OI-AUDIOHW-07** | Select the cup-to-hub cable, connector and strain relief. The same connector gap `NP-HW-TCAP-001` §8 records for the T2 cap and `NP-HW-VNSCLIP-001` §8 for the clip — worth deciding **once** across the head-worn accessories rather than three times | EE + ME | A11 tooling |
| **OI-AUDIOHW-08** | **DECIDED 2026-09-24 (principal, Rev 4) — option B: one in-cup microphone per side, measurement only, no anti-noise; bound as `REQ-AUDIO-14`–`16` (§3.1); succeeded by `OI-AUDIOHW-09` and `-10`.** Analysed in Rev 3 (§6.4), recommendation B. The measurement needs the microphone, not the noise cancelling (§6.4.1); the microphone is also the only route found to a measured level for `OI-AUDIOHW-01` (§6.4.2); anti-noise sits in the band of every shipped carrier (§6.4.3), adds output and instability hazards (§6.4.4), and needs a low-latency loop the hub's SAI/DMA path cannot provide, which would decide `REQ-AUDIO-04a` by force (§6.4.5); bystander audio has no consent subject, so raw audio must be excluded structurally (§6.4.6); and no requirement states what ambient noise costs a session, so under `CLAUDE.md` §18 nothing yet requires anti-noise (§6.4.8). **As raised:** **Add noise cancelling with an in-cup (feedback) microphone, or decide not to.** Intended for session quality; also the only measurement found for the foam's loss of seal (`OI-ACC-04`, §6.1) and a candidate for mesh fouling if the mic sits on the ear side of the mesh (`OI-ACC-05`, §6.2). **It reverses a property the record states — there is no microphone anywhere in the design** — so it carries: a privacy decision (on-device processing only, a seal score out, no audio stored; the score depends on the wearer's head and is presumptively UHDR); an acoustic-safety one (anti-noise adds output and can howl when the seal breaks, which makes `OI-AUDIOHW-01` more pressing); and placement, cable and power consequences for `OI-AUDIOHW-05` and `-07`. Microphones put no current, light or field into the assembly, so `REQ-AUDIO-12` is not triggered | EE + Acoustic + Privacy | — (decided) |
| **OI-AUDIOHW-09** | **Select the in-cup microphone** (`REQ-AUDIO-14`). Decide analogue vs digital (PDM) output knowing that a PDM part runs a clock of the order of a megahertz beside the temporal EEG sites and is a new source for `NP-EMC-CAV-001`'s analysis; confirm the hub codec (`NP-HW-HUB-001`) has an input per cup; add the microphone conductors to `OI-AUDIOHW-07`'s cable; fix the position within the front volume, and decide whether it goes on the ear side of the mesh (only `OI-ACC-05`'s candidate needs that). The cable change is a module interface under `REQ-UPG-03` and lands on both tiers | EE + Acoustic + EMC | A11 tooling; `OI-ACC-04` closure |
| **OI-AUDIOHW-10** | **Implement the seal measurement in firmware.** Microphone HAL (a new entry beside `OI-AUDIO-01…08`), the probe signal and its level (bounded by `OI-AUDIOHW-01`), the powered capture window, reduction to a seal score, and `REQ-AUDIO-15`'s containment: decide whether a CI gate in the manner of `scripts/check-redaction-shape.ts` enforces that no sample leaves the reduction call. The dB criterion is `OI-ACC-04`'s, not this item's | FW + Privacy | `OI-ACC-04` closure |
| **OI-AUDIOHW-11** | **PART DECIDED on paper 2026-11-09 (the method, §5.3); the measurement and the margin's size stay OPEN because no driver is selected (`OI-AUDIOHW-04`).** **Decided:** the ceiling is verified and the level-to-drive table measured at the eardrum point of an ear simulator, per unit and per cup, never at a coupler or the in-cup microphone (`REQ-AUDIO-22`); the in-cup microphone is a factory reference reading and a field indicator only, and **no cup-to-eardrum correction exists or is needed** because nothing enforces the ceiling from it; the command ceiling is C = 85 − M with M the arithmetic sum of m1 (simulator chain), m2 (table representation) and m3 (drift), quantisation held to zero by rounding down (`REQ-AUDIO-23`); the table is SHDR-class, bound to its chain, fails closed and is written only at factory or service (`REQ-AUDIO-24`); the A-weighting question is answered by computation from the IEC 61672-1 formula (−4.10 dB at 440 Hz, +1.27 dB at about 2.5 kHz) and by a bound of 1,000 Hz on every tone frequency rather than a frequency-dependent ceiling (`REQ-AUDIO-25`). **Open:** (1) measure a built unit: the sizes of m1, m2 and m3, and so M (`UC-133`); the shipped 85 is a zero-margin stand-in until then; (2) choose the exact simulator standard and configuration for a circumaural cup against the standards' text (candidates IEC 60318-4 with a pinna simulator, IEC 60318-1; both carried from memory, `OI-AUDIOHW-12`); (3) the principal confirms `REQ-AUDIO-25`, which restricts a protocol field, or prefers the frequency-dependent ceiling; (4) implement the 1,000 Hz refusal in the compiler and hub driver, and the calibration record and its fail-closed check; (5) whether the record is signed or only CRC-bound is an ISO 14971 decision (`OI-RISK2-02`, with `OI-AUDIOHW-12`), and signing would be a Rust primitive (`CLAUDE.md` §22); (6) simulator-to-wearer difference is unbounded by any factory measurement and is assumed (`UC-134`); (7) the ISO 389 air reference in `REQ-AUDIO-21`(b) is stated for a particular coupler or simulator, and the planar 85 and that reference must be on the same one or differ by the standard's own figure (`OI-AUDIOHW-13`); (8) peak (impulsive) level is not bounded here. **As raised:** calibrate and verify the 85 dB SPL ceiling at the ear (`REQ-AUDIO-19`(c), `REQ-AUDIO-20`(b),(c)); nothing maps `level_half_db` to a driver and the HAL stubs trap | EE + Acoustic | T1 release; `NP-FAI-AUDIO-001` |
| **OI-AUDIOHW-12** | **Check the §5.1 citations against the standards' texts** (NIOSH 98-126, WHO–ITU H.870, IEC 62368-1 cl. 10.6, EN 50332, Directive 2003/10/EC), **the §5.2 bone standards** (ISO 389-3, IEC 60318-6, IEC 60645-1 cl. 6.1.5, ISO 389-1/-7) **and the §5.3 ones** (the IEC 61672-1 A-weighting formula and its constants, and the ear-simulator standards IEC 60318-4 and -1). Carried from published summaries; the IEC/EN texts are paywalled. Also decide whether the hazard control sits in Class B code alone or needs more (the ceiling is enforced in compiler and hub driver, both outside a Class C path) — an ISO 14971 decision for `OI-RISK2-02` | Systems + Regulatory | Hazard analysis |
| **OI-AUDIOHW-13** | **Compute the bone ceiling and give it a field** (`REQ-AUDIO-21`). (1) Read the ISO 389-3 mastoid values and the ISO 389-1/-7 air values at the carrier frequencies and compute L_F,max in dB re 1 µN; the figure then goes in `REQ-AUDIO-21` and, if it is a limit chosen rather than derived, in the register (`CLAUDE.md` §19). (2) Add a bone level to the wire: today `np_mod_audio_hal_bone_set` receives the planar `level_half_db`, which is not a bone level. Decide the field's unit and width (force level in dB re 1 µN needs its own `np_mod_audio_params_t` field and a descriptor change on both tiers, `REQ-UPG-03`), and enforce the ceiling in the compiler and the hub driver by refusal. (3) Decide the combined case when both paths play. (4) Verify on a built unit (`OI-AUDIOHW-04`, `OI-AUDIOHW-11`). `OI-AUDIOHW-12`'s citation check covers the standards named in §5.2 | Systems + Acoustic + FW | T1 release; before the bone HAL is implemented |

> **On the `OI-AUDIOHW-` prefix.** `OI-AUDIO-01…08` is already in use, in
> `firmware/hub_control/modules/np_mod_audio.c`, for the audio **HAL stubs**. Open-item IDs are
> append-only and never renumbered (`NP-CONV-001` §6), so this document takes a distinct prefix
> rather than extending a firmware family with hardware items. `OI-AUDIO-08` is cited here by its
> own name and is not adopted.

---

## 10. Cross-references

- **Artifact register row and FAI readiness:** `docs/np_art_001.md` §2.3 (A11), §3.2
- **FAI issue conditions F1–F4:** `docs/np_fai_001.md` §2
- **Risk file gap this unblocks:** `docs/np_risk_002.md` §4, OI-RISK2-02
- **Driver behaviour:** `docs/np_fw_hub_001.md` §8.5; `firmware/hub_control/modules/np_mod_audio.c`
- **Consumable rows, prices and intervals:** `docs/reference/commercial-model.md` §2.3
- **Accessory ranking:** `docs/np_acc_priority_001.md` §5 ranks 13, 14
- **Shielding evidence base:** `docs/np_bib_emf_001.md`

---

## 11. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-09-20 | NeurOne Systems Engineering | Initial release, against GitHub #332 / `NP-ART-001` OI-ART-04. **Gives artifact A11 an owning specification for the first time**, displacing `CLAUDE.md` §3 modality ⑦ — a roster entry — from that role. Decomposes the assembly into five sub-parts (§2), carries ten requirements from documents that already state them (§3), restates the firmware interface as the shipped Class B driver has it (§4), and affirms rather than flags the absence of a safety-MCU enable line (§5). **Two absent exposure limits are recorded as open items rather than supplied**: no acoustic output ceiling and no bone-conduction drive ceiling exist anywhere in the document set (`OI-AUDIOHW-01`, `-02`). **Principal new finding (§6.3): the mesh frame's 40 dB RF figure is a shielding claim on a user-replaceable consumable that appears in no layer of the `CLAUDE.md` §4.3 stack and in no row of `NP-BIB-EMF-001`'s per-layer audit**, and whose fouling trigger is an unimplemented HAL stub (`OI-ACC-05`, `OI-AUDIO-08`) — three defects that compound. §7 supplies the hazard-analysis input list that `OI-RISK2-02` was blocked for want of; the block is lifted, the analysis is not performed here. §8.1 states the FAI verdict honestly: **`NP-FAI-AUDIO-001` still cannot be written** — F1 and F2 fail on a DRAFT document with no dimensioned geometry — so the absence is narrowed and re-owned rather than closed. **No engineering value is set, no figure is invented, no price, interval, margin or locked decision changes.** Raises OI-AUDIOHW-01…07. |
| 2 | 2026-09-23 | NeurOne Systems Engineering | **Foam prompt re-scoped by principal decision (§6.1, §8).** Comfort and hygiene replacement is at the user's discretion; the prompt covers loss of seal only, measured by the in-cup microphone of noise cancelling the programme intends to add. **Raises `OI-AUDIOHW-08`** (add ANC with a feedback mic, or decide not to) — it reverses the record's *no microphone anywhere in the design*, so privacy, acoustic-safety and placement consequences are listed with it. **No requirement added, no value set, no threshold changed.** |
| 3 | 2026-09-24 | NeurOne Systems Engineering | **`OI-AUDIOHW-08` analysed; not decided (§6.4).** Separates the measurement from the noise cancelling: the foam's seal (`OI-ACC-04`) needs an in-cup microphone and a probe tone, not an anti-noise loop. States three options — **A** feedback/hybrid ANC, **B** in-cup microphone for measurement only, **C** no microphone — and **recommends B**. New findings: the microphone is also the only route to a measured in-cup level for `OI-AUDIOHW-01` (§6.4.2); every predefined protocol's 440 Hz carrier sits in feedback ANC's band, so A changes what the modality delivers (§6.4.3); A needs a low-latency loop the hub SAI/DMA chain cannot provide, deciding `REQ-AUDIO-04a` by force (§6.4.5); bystanders are neither `CLAUDE.md` §6.0 consent subject, so raw audio must be excluded structurally (§6.4.6); a PDM microphone is a new clocked source for `NP-EMC-CAV-001`. Two placement conditions recorded for A or B (§6.4.7), two conditional hazards added to §7, one row to §8. **No requirement added, no value set, no threshold or constant changed, no cost stated.** |
| 4 | 2026-09-24 | NeurOne Systems Engineering | **`OI-AUDIOHW-08` DECIDED (principal): option B** — one in-cup microphone per side, measurement only, no anti-noise. Adds §3.1, the first requirements that originate in this document, each with what fails and its trace (`CLAUDE.md` §18): `REQ-AUDIO-14` (the microphone, measurement only), `REQ-AUDIO-15` (raw audio excluded by design — bystanders are neither §6.0 consent subject), `REQ-AUDIO-16` (the seal is the dominant leak; no fully open-back cup). The ear-side-of-mesh placement stays a condition, because `OI-ACC-05`'s candidate is not adopted. §7: probe-tone hazard live, anti-noise hazard retired in place. Raises `OI-AUDIOHW-09` (microphone selection, including analogue vs PDM and hub codec inputs) and `OI-AUDIOHW-10` (seal-measurement firmware). **No value set; the foam's `150` unchanged; no cost stated; `REQ-AUDIO-04a` and `REQ-AUDIO-12` unaffected.** |
| 5 | 2026-11-09 | NeurOne Firmware Engineering | **REQ-AUDIO-17 and REQ-AUDIO-18 added (OI-SIMFW-19).** Volume range 0-100 enforced by refusal; band update gated on binaural mode. No figure set. |
| 6 | 2026-11-09 | NeurOne Systems Engineering | **§3.1, §4, §5.1, §9: OI-AUDIOHW-01 decided (principal): 85 dB SPL at the ear, level ceiling only with no dose model; REQ-AUDIO-19 and REQ-AUDIO-20 (absolute scale); raises OI-AUDIOHW-11 (calibration at the ear) and OI-AUDIOHW-12 (check the cited standards).** |
| 7 | 2026-11-09 | NeurOne Systems Engineering | **§3.1, §5, §5.2, §7, §9: OI-AUDIOHW-02 decided as a rule (principal): REQ-AUDIO-21, bone ceiling at equal hearing level in dB re 1 µN on an artificial mastoid, refused never clamped; the figure is not stated because the ISO 389-3 and ISO 389-1/-7 tables were not readable; raises OI-AUDIOHW-13. No firmware, figure or constant changed.** |
