# Audio Cup Assembly — Hardware Specification

**Project:** NeurOne
**Document:** NP-HW-AUDIO-001
**Revision:** 8
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
| **REQ-AUDIO-04a** | **DECIDED 2026-11-09 (principal): the cup holds no amplifier, converter, clock or switching stage; all amplification, including any high-voltage stage the piezo needs, is in the hub.** (a) The cup carries only passive transducers (the planar driver, the bone element on its isolator) and the in-cup microphone's line; (b) the hub drives each transducer over the cable, the bone element on its own conductor(s) so the two paths stay independently drivable (`REQ-AUDIO-11`); (c) any piezo high-voltage stage is in the hub and its voltage is carried to the element on an insulated conductor; (d) the planar selection prefers the candidate with the higher sensitivity and impedance, which lowers the current the cable carries. *What fails without it:* an active stage in the cup sits beside the temporal EEG sites where `NP-EMC-CAV-001` has no audio source, adds a switching or clock source (`SEL-P10`, `SEL-B11`), and puts a drive voltage one insulation fault from the skin (`SEL-B07`), against `REQ-AUDIO-09`. *Reopened only* by the principal, if the selected planar part needs a current the cable cannot carry acceptably (the measurement in `OI-AUDIOHW-04`). **Not decided here:** the hub-side stage, rail and codec (`OI-AUDIOHW-14`) and the microphone's type (`OI-AUDIOHW-09`, which this narrows toward analogue) | `CLAUDE.md` §4.3 (EEG signal quality under the EMF stack); `REQ-AUDIO-09`, `-11`, `-12`; §8.2.4 Q1 and its analysis; principal direction 2026-11-09 |
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

| **REQ-AUDIO-21** | **The bone-conduction transducer's output is limited at the same hearing level as the planar path, stated as an absolute force level.** (a) The bone level is its own absolute quantity, **force level in dB re 1 µN on an IEC 60318-6 mechanical coupler (artificial mastoid), at the carrier frequency**, never a percentage and never the planar level number; (b) the ceiling at carrier frequency *f* is **L_F,max(f) = RETVFL_mastoid(f) + (85 − RETSPL_air(f))**, which is the hearing level that `REQ-AUDIO-19`'s 85 dB SPL gives on the air path, carried to the bone path through the reference thresholds of ISO 389-3 and the matching air reference of ISO 389-1/-7; (c) a command above it is refused, never clamped, in the compiler and the hub driver; (d) the delivered force level is verified on a built unit. The bone and planar paths are each held to their own ceiling; their combined effect is not summed (`OI-AUDIOHW-13`) | A planar ceiling written in dB SPL does not bound a vibrator: the same number drives a mastoid transducer to a different loudness, and through a different path that bypasses the ear canal and the cup's seal. Without its own ceiling, a bone cue (breathing pacer, sync cue) is limited by nothing | ISO 389-3:2016 (reference equivalent threshold vibratory force levels, mastoid); ISO 389-1/-7; IEC 60318-6; IEC 60645-1:2017 cl. 6.1.5 (bone vibrator output level). Principal direction 2026-11-09 (all scales absolute, `REQ-AUDIO-20`). `CLAUDE.md` §3 (a ceiling refuses, never reshapes). **The dB re 1 µN figure is not stated here and the field that would carry it does not exist: the standards' tables could not be read from any source reachable in the 2026-11-09 attempt, so (b) is a rule without a number, and (c) and (d) are not buildable until it has one (§5.2, `OI-AUDIOHW-13`)** |

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

**Why 85.** NIOSH's recommended limit and the EU Directive below put the onset of hearing-conservation obligations at 85 dB(A) for a full
working day. Setting the ceiling there means a session can never, by level alone, be louder than the
level at which those standards begin to require action, and it sits far under the level at which
sound is painful or injurious in seconds. What the ceiling does **not** mean is that a session at the
ceiling is harmless: at NIOSH's 3 dB exchange rate (`T = 8 h / 2^((L − 85)/3)`) a 20-minute session at
85 dB uses about 4 % of a day's 8-hour budget and a 60-minute session 12.5 %, on top of whatever the
wearer was exposed to otherwise. The shipped protocols play at 75 dB SPL or less (`protocols/predefined/`),
which that formula allows for roughly 80 hours.

| Standard | What it says | Use here |
|---|---|---|
| NIOSH, *Criteria for a Recommended Standard: Occupational Noise Exposure*, Pub. 98-126 (1998) | REL 85 dBA, 8 h time-weighted average, 3 dB exchange rate | The 85 and the dose arithmetic above. **VERIFIED** (`OI-AUDIOHW-12`, 2026-11-09), from excerpts of the archived CDC text (§1.1.2); the PDF host was unreachable |
| WHO–ITU H.870 (2018), *Guidelines for safe listening devices/systems*; WHO–ITU global standard for safe listening devices and systems (2019) | Adult weekly dose of 80 dB(A) for 40 h/week (children 75 dB(A), 40 h). **VERIFIED** from WHO's publication page and secondary summaries, not the ITU text. **H.870 has a 2022 edition (approved 03/2022) that supersedes the 2018 one**; the 2018 text was not compared with it | Consumer listening devices are held to a *lower* weekly dose than 85 dB(A) for a full day, which is why the shipped 75 dB protocols matter and why 85 is a ceiling, not a target |
| IEC 62368-1 clause 10.6 and EN 50332-1/-2/-3 (personal audio players and headphones) | Acoustic-energy-source classification for personal audio. **CORRECTED (`OI-AUDIOHW-12`):** the earlier claim that EN 50332 treats 85 dB(A) as the normal maximum is **not supported**. Summaries report the older EN 50332-1 package limit as 100 dB(A) (150 mV analogue), with IEC 62368-1 ed. 2 cl. 10.6 built on an 85 dB(A) level, and the A11:2020 amendment replacing cl. 10.6 with RS1/RS2/RS3 classes (RS1 reported as ≤ 80 dB(A)). **All of this is from search-engine summaries, none from the paywalled texts: PARTLY / UNVERIFIABLE-PAYWALLED** | The nearest product standard for a head-worn audio output. **No EN 50332 or IEC 62368-1 figure or clause is relied on here**; the 85 rests on NIOSH and the Directive. The text is to be read before either is cited for a number |
| Directive 2003/10/EC (physical agents, noise) | Art. 3: upper exposure action value L_EX,8h = 85 dB(A) and peak 140 Pa (137 dB(C), footnote); lower 80 dB(A) / 112 Pa; limit value 87 dB(A) / 200 Pa. Peak values are stated in Pa, with the dB(C) figure in a footnote. **VERIFIED**, from a search summary of the legislation.gov.uk copy (the retained UK text, not the Official Journal) | The EU basis for treating 85 dB(A) as the action level |

**Check status (`OI-AUDIOHW-12`, 2026-11-09).** NIOSH, WHO–ITU and the Directive were checked against
public excerpts (the CDC, ITU, EUR-Lex and legislation.gov.uk hosts could not be fetched directly, so each
reading is of a search-engine summary of the source; the Directive and NIOSH figures agree across more than
one source). The EN 50332 / IEC 62368-1 row was corrected. The IEC and EN texts are paywalled and are not
in the repository. Per `CLAUDE.md` §19 what remains is an open item, not a register row.

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

**What is not here.** The dB re 1 µN number. It needs the ISO 389-3 mastoid value and the matching ISO 389-1/-7
air value at the carrier (every shipped carrier is 440 Hz; the nearest standard frequency is 500 Hz), and
those tables are not in the repository. **A second attempt on 2026-11-09 (`OI-AUDIOHW-13` part 1) searched the
public web for both values and found neither stated with a citation:** the search results named the standards
and their scope but quoted no 500 Hz value, and the pages that might (a university calibration course, the
standards bodies' free previews, a journal article on mastoid thresholds) could not be fetched from the
session. No value is carried in from memory, so **no L_F,max is computed or stated**. Stating one without a
source is the failure this document exists to avoid.

**Standards used.**

| Standard | What it contributes | Checked |
|---|---|---|
| ISO 389-3:2016 | Reference equivalent threshold vibratory force levels for pure-tone bone vibrators, mastoid, with the forehead difference in an informative annex | **VERIFIED** (title, scope), `OI-AUDIOHW-12`: *Acoustics — Reference zero for the calibration of audiometric equipment — Part 3: Reference equivalent threshold vibratory force levels for pure tones and bone vibrators*, 2016; RETVFL is defined as the force level on a mechanical coupler at normal threshold; forehead–mastoid differences are an informative annex. From the ISO catalogue abstract. The values were not read |
| IEC 60318-6 | The mechanical coupler (artificial mastoid) the force level is measured on | **VERIFIED** (number, title), `OI-AUDIOHW-12`: IEC 60318-6:2007, *Electroacoustics — Simulators of human head and ear — Part 6: Mechanical coupler for the measurement on bone vibrators*; specified 125 Hz to 8 kHz for a 175 mm² ± 25 mm² tip, and **not in general equal to the force on a person's mastoid**. From the IEC webstore listing. The text was not read |
| IEC 60645-1:2017, cl. 6.1.5 | Bone vibrator output level for audiometric equipment | **PARTLY**: the contents lists 6.1.5 *Bone vibrator output level*, but, as the contents were read through a summary, it appears under heading 6.1 beside speech-signal clauses, so the clause number is to be confirmed in the text. The limits were not read (paywalled) |
| ISO 389-1/-7 | The air reference thresholds the hearing level is taken against | **PARTLY**: ISO 389-1:2017 is *Reference equivalent threshold sound pressure levels for pure tones and supra-aural earphones*, and ISO 389-7:2019 is the reference threshold under free-field and diffuse-field listening, not interchangeable with 389-1. Neither is stated for a circumaural cup (another part of ISO 389 may be the one, not checked). Which applies depends on the measurement point `OI-AUDIOHW-11` chooses, and the choice is part of `OI-AUDIOHW-13`. Values not read |

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
| **REQ-AUDIO-25** | **CONFIRMED 2026-11-09 (principal), for now; open to revisit (§5.3.1).** **Every tone frequency the planar path is commanded to produce is at most 1,000 Hz: `carrier_hz`, and for a binaural or isochronic command `carrier_hz` plus the beat or pulse rate.** The compiler refuses a protocol above it before signing and the hub driver refuses the command before any state or output changes. Refused, never clamped | A-weighting adds up to 1.27 dB between about 1 kHz and 6.1 kHz, so an unweighted 85 dB SPL tone there is up to 86.3 dB(A), above the 85 dB(A) action level §5.1 gives as the reason for 85. The field is otherwise unbounded to 65,535 Hz | §5.1 (85 chosen at the action level in dB(A)); NIOSH Pub. 98-126 and Directive 2003/10/EC (dB(A)); the IEC 61672-1 weighting, computed above (`OI-AUDIOHW-12` checks it); `CLAUDE.md` §3 (a ceiling refuses, never reshapes). The 1,000 Hz is not chosen: it is where the nominal weighting is zero |

**Not built:** no compiler or driver check, no calibration record, no table. `REQ-AUDIO-22` to `-25` are
untested by design (`CLAUDE.md` §21: a requirement for an unbuilt feature stays untested until the code that
meets it is written). The `carrier_hz` row of `NP-NPPS-REF-001` §4.7 and `common/npps-core` carry
`REQ-AUDIO-25` when it is implemented.

#### 5.3.1 `REQ-AUDIO-25` confirmed for now, and why (principal, 2026-11-09)

**Decision.** The 1,000 Hz bound on every tone frequency stands, **for now**. The principal confirmed it and
said it may be revisited if researchers want more.

**Why a bound, and why 1,000 Hz.**
1. **It protects the one property §5.1 gives for choosing 85.** The standards state dB(A); the ceiling is
   unweighted dB SPL. Below about 1 kHz A-weighting only lowers the figure, so an unweighted 85 never
   exceeds 85 dB(A). Above it the weighting is positive, up to +1.27 dB near 2.5 kHz (computed from the
   IEC 61672-1 formula, not measured), so an 85 dB SPL tone there is up to 86.3 dB(A). Nothing else in the
   record stops a protocol author choosing such a tone: `carrier_hz` is bounded only by its 16-bit width.
2. **1,000 Hz is not a figure chosen for this purpose.** It is where the nominal weighting is zero, so the
   bound does not rest on the precision of the weighting constants, which are carried from memory and
   unchecked (`OI-AUDIOHW-12`).
3. **Nothing in the record uses a higher frequency.** Every shipped carrier is 440 Hz, and the binaural and
   isochronic rates are far below 1 kHz, so no existing protocol is refused. Only a field an author could
   have set, and no one has, is restricted.
4. **Simpler than the alternative.** A frequency-dependent ceiling needs the weighting curve implemented
   twice, in the compiler (Rust) and the hub driver (C), and kept in agreement. A single comparison is
   one line in each and cannot drift.
5. **It refuses, never clamps** (`CLAUDE.md` §3): a clamped tone is a different stimulus from the one
   authored.

**What revisiting would look like.** The bound is a stand-in for a frequency-dependent ceiling, one
derivation away: at tone frequency *f* the command ceiling becomes 85 dB SPL less the positive part of the
A-weighting at *f* (and less the guard band of `REQ-AUDIO-23`), so a tone above 1 kHz is allowed at a
lower level and the §5.1 property still holds. Taking it needs: a stated use above 1 kHz from the
researchers, the weighting constants checked against IEC 61672-1 (`OI-AUDIOHW-12`), the curve implemented
once and shared between the compiler and the driver, and the request recorded as a decision, since a
looser limit is a change to a hazard control (`CLAUDE.md` §18). Raising or removing the bound without
that is not a detail.

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

### 8.2 `OI-AUDIOHW-04` — the selection package (Rev 8). **Ready for the principal. No part, class or figure is selected here.**

`OI-AUDIOHW-04` asked for the planar magnetic driver and the bone-conduction element to be selected and
specified. Selection is an engineering and principal decision, and no part number, datasheet or measurement
exists in the record, so this subsection does the work that comes **before** a selection: it says which
parameters a selection must fix, which of them follow by arithmetic from figures the record already holds,
what the candidate technology classes look like on public evidence, what the choice forces on other open
items, and which vendor quantities to ask for. **Nothing below sets a value.** Where a number is written,
it is carried from the record and the row says where from. The numbers a selection must still choose (every
margin, every distortion limit, every stray-field limit) are named as *not set*, and each one that is chosen
without a derivation becomes a register row under `CLAUDE.md` §19 in the change that chooses it.

**What is already locked, and not reopened here.** `CLAUDE.md` §3 modality ⑦ locks the roster entry
*planar magnetic + bone conduction*, and `REQ-AUDIO-02`/`-03` bind a 40 mm planar magnetic driver and a
piezoelectric element on a silicone isolator at the mastoid, one per side. The decision matrices in §8.2.3
show the other classes only because the question was asked; **choosing any other class reopens a locked
decision and is the principal's, not this document's.** What is also decided: the planar ceiling (85 dB SPL
at the ear, `REQ-AUDIO-19`/`-20`), the bone ceiling as a rule (`REQ-AUDIO-21`, figure pending in
`OI-AUDIOHW-13`), independent drive of the two paths (`REQ-AUDIO-11`), one measurement-only microphone per
cup with no anti-noise (`REQ-AUDIO-14`–`16`), and that amplification is in the hub, not the cup
(`REQ-AUDIO-04a`, decided 2026-11-09).

#### 8.2.1 What the record gives the selection, and what it does not

| Input | What the record holds | Source |
|---|---|---|
| Shipped carrier | 440 Hz in every predefined protocol that sets one; the app default is 200 Hz | `protocols/predefined/*.npps`; `common/types/protocol.ts` |
| Authorable carrier range | **None found.** The compiler writes `carrier_hz` as a 16-bit field clamped only at 65535, and no validator range for it was found in `common/npps-core/src` | `common/npps-core/src/compiler.rs` (the `put16` of `carrierHz`) |
| Beat and isochronic rate | Authorable 0.5–100 Hz; the EEG-adaptive band map is 2, 6, 10, 20, 40 Hz | `docs/status/unjustified-choices.md` UC-082; §4 |
| Noise content | Pink and brown are enumerated values; **no band limit, spectral shape or crest factor is stated anywhere in the record** | `docs/np_npps_ref_001.md` (`noise`) |
| Bone-path signal | `np_mod_audio_hal_bone_set(freq_hz, level_half_db)`: a tone at the carrier frequency and one level. The breathing cue is 4–7 breaths/min (about 0.067–0.117 Hz), so it is an envelope on that tone, not a sub-hertz transducer response | `firmware/hub_control/modules/np_mod_audio.c`; `docs/reference/modality-stack.md` modality ⑥ |
| Other bone-path tones | `firmware/zone_announce` defines 150, 200, 440 and 880 Hz tones for a bone transducer, 8 kHz sample rate, amplitude −6 dBFS, with a note about DC bias through the piezo. Whether it stays reachable is `OI-FWHUB-18`'s | `firmware/zone_announce/include/np_zone_announce_config.h` |
| Hub-side audio path | "I²S codec via SAI" and nothing else. **`NP-HW-HUB-001` has no audio section: no codec part, channel count, output stage or rail is recorded.** The firmware assumes SAI1/SAI2 for the planar drivers and SAI3 for "the bone conduction amplifier" | `NP-FW-HUB-001` §8.5; `np_zone_announce_config.h` |
| EEG acquisition | ADS1299, 8 channels, 500 Hz (`CLAUDE.md` §3). `A1`/`A2` linked-ear references ride the VNS clip's pads | `CLAUDE.md` §3; `NP-HW-EEGNET-001` `REQ-NET-19` |
| EMC source analysis | `NP-EMC-CAV-001`'s source table lists cluster controllers, the I²C tree, PWM LED drivers and the ADS1299 bank. **No audio source is in it**, and `NP-BIB-EMF-001` §5.4 found no literature relating ambient EMF to audio entrainment outcomes | `docs/np_emc_cav_001.md` source table; `docs/np_bib_emf_001.md` §5.4 |
| Fit system and mass | Spring-decoupled electrode pods, 80–120 g; no mass budget for A11 | `docs/reference/hardware-detail.md` §4.4; `OI-AUDIOHW-05` |

**Three gaps the selection cannot close alone, raised for the principal:**

1. **The carrier range is unbounded on the wire,** so the frequency band each transducer must cover is
   undetermined. Either a range is decided (a requirement, to `CLAUDE.md` §18's standard) or the band is
   whatever the transducers deliver and the compiler refuses carriers outside it.
2. **The noise content has no stated band or crest factor,** so "reaches 85 dB SPL cleanly in brown noise"
   cannot be checked. Brown noise concentrates energy at low frequency, where a driver's excursion limit binds.
3. **Four output channels, or three.** `REQ-AUDIO-11` makes the planar and bone paths independent, and
   `REQ-AUDIO-03` puts one bone element per side. The HAL passes one bone frequency and one level, so one
   mono bone drive feeding both elements is the shipped behaviour; whether the two bone elements are driven
   separately is not stated. The hub codec channel count follows from it.

#### 8.2.2 Requirements table — what a selection must specify

Row IDs `SEL-P` (planar, A11.1) and `SEL-B` (bone, A11.2) name **parameters to be fixed**, not requirements:
none carries a value, so none is a `REQ-` row and none has a test (`CLAUDE.md` §21). A parameter becomes a
requirement when a value is set for it and the value passes §18's two questions. *Derived relations* use
only figures held in the record; every symbol that is not one of those is a quantity the selection measures
or a vendor states. *Margin* terms (`H`) are choices and are **not set**.

**Relations used in the tables below**

- **D1 — planar drive.** For each carrier *f*, let `S(f)` be the transducer's sensitivity at the ear
  (dB SPL per volt rms, in the cup, measured at the point `OI-AUDIOHW-11` chooses, with the worst seal
  `REQ-AUDIO-16` allows and with the mesh in place). The drive that gives the ceiling is
  `V85(f) = 10^((85 − S(f)) / 20)` V. The element and its amplifier must deliver cleanly (within the THD
  limit the selection sets) at least `V85(f) · 10^(H_P / 20)`, where `H_P = H_tol + H_seal + H_crest` covers
  unit-to-unit calibration tolerance (`OI-AUDIOHW-11`), the seal shortfall between calibration and the worst
  allowed seal, and the crest of the content (noise, isochronic edges). Current is the voltage divided by
  the impedance magnitude at *f*.
- **D2 — planar band.** The response must cover `[f_c − b, f_c + b]` for every authorable carrier `f_c`
  and beat `b` (conservative: the HAL stub does not say how the two ears split the beat). At the shipped
  carrier and the authorable limit this is 440 ± 100 Hz, that is 340–540 Hz; the EEG-adaptive maximum
  (40 Hz) gives 400–480 Hz. Pink and brown noise add a band the record does not state (gap 2).
- **D3 — bone force.** The element must deliver, on an IEC 60318-6 artificial mastoid at the static force
  it runs at, a force level of at least `L_F,max(f_c) + H_B` dB re 1 µN at each carrier, where `L_F,max` is
  `REQ-AUDIO-21`(b) and its figure is `OI-AUDIOHW-13`'s. In newtons that is `10^(L / 20)` µN. The drive is
  then `V_B(f) = F_req(f) / K_F(f)`, with `K_F` the force per volt the vendor or the bench gives. `H_B` is
  not set.
- **D4 — bone band.** The bone path plays a tone at the carrier, so its band is the authorable carrier range
  itself (gap 1), plus 150–880 Hz if the zone announcement stays. The breathing envelope needs no
  transducer response below the carrier, but the drive electronics must pass a 0.067–0.117 Hz envelope
  without a DC term reaching the element (`SEL-B06`).

**A11.1 — planar magnetic driver (`SEL-P`)**

| ID | Parameter the selection must specify | Derived relation or where the value comes from | What fails if it is not met | Traceable to |
|---|---|---|---|---|
| SEL-P01 | Impedance magnitude and phase versus frequency over the D2 band, with the unit-to-unit and temperature spread | Vendor curve, then bench. Current is `V85` over the impedance magnitude (D1) | The amplifier cannot supply the current for 85 dB SPL, or the amplifier location (`REQ-AUDIO-04a`) is chosen on a wrong load. Without a clean-unit curve and its spread, impedance cannot detect mesh fouling | `REQ-AUDIO-07`; `REQ-AUDIO-04a`; D1 |
| SEL-P02 | Sensitivity at the ear, dB SPL per volt and per milliwatt, at each shipped carrier, mesh fitted, worst allowed seal | D1 gives `V85` from it | The driver cannot be calibrated to the absolute level the command states, so `REQ-AUDIO-19` bounds a number rather than a sound | `REQ-AUDIO-19`(c); `REQ-AUDIO-20`(b),(c); `OI-AUDIOHW-11` |
| SEL-P03 | Frequency response over D2 with its tolerance, and left-right match in level and phase at the carrier | D2. The match tolerance is not set | Binaural content is built on the two ears receiving the commanded tones, so an unmatched pair delivers a different stimulus. A band that misses the carrier cannot play the protocol | `CLAUDE.md` §3 modality ⑦; `REQ-AUDIO-20`(b) (the same command sounds the same on any driver) |
| SEL-P04 | Total harmonic distortion at 85 dB SPL at each shipped carrier and at the D2 band edges; the limit is **not set** | Vendor curve at the stated drive, then bench at the ear | At the ceiling the output is no longer the authored stimulus. A ceiling that refuses and never reshapes is defeated by a transducer that reshapes | `CLAUDE.md` §3 (a ceiling refuses, never reshapes); `REQ-AUDIO-19` |
| SEL-P05 | Maximum continuous and peak drive (volts, watts, amperes) and the damage threshold of the diaphragm and its conductor, with the thermal limit in the cup | D1 headroom `H_P` must sit under this. Whether the hardware needs its own limit is `OI-AUDIOHW-12`'s: headroom enlarges the gap between what the hardware can output and the 85 dB it may | A command inside the ceiling, or a fault, destroys the driver. Too much headroom means the ceiling rests on Class B code alone | `REQ-AUDIO-19`; `OI-AUDIOHW-12`; §7 (acoustic overexposure) |
| SEL-P06 | Linear excursion limit and low-frequency output at 85 dB SPL in pink and brown noise, with the content's band and crest factor stated by its owner (gap 2) | `H_crest` of D1 | The driver clips or bottoms out on the noise modes, which are shipped content | `docs/np_npps_ref_001.md` (`noise`); `REQ-AUDIO-19` |
| SEL-P07 | Back-wave treatment: open, closed, or damped vent, and the acoustic resistance and compliance of the rear path | Fixes how much leak the driver itself adds to the front volume | A rear path that is a large designed leak makes the seal measurement read the vent instead of the foam | `REQ-AUDIO-16`; `REQ-AUDIO-14`; §6.4.7 |
| SEL-P08 | Insertion loss of the silver-coated mesh in front of the driver at the carrier, clean and fouled | Adds to `S(f)` in D1; spread between mesh states is part of `H_tol` | The calibration holds with one mesh and not the next, and a user-replaceable part moves the delivered level | `REQ-AUDIO-05`; `REQ-AUDIO-07`; `REQ-AUDIO-20`(c) |
| SEL-P09 | Magnetic stray field of the magnet assembly: static field versus distance, on the axis and at the temporal EEG sites, at the L2 mu-metal layer and at the fluxgate positions; **limit not set**, to be derived by EMC from the fluxgate input range and the EEG motion-artefact criterion | Vendor field plot, then bench. The fluxgate range is a part datasheet quantity not read in this task | A static offset on the fluxgate mis-sets the Helmholtz cancellation, a magnet near L2 changes its permeability state, and electrode or lead motion in a static field adds artefact to a µV recording | `CLAUDE.md` §4.3 (fluxgate, mu-metal); `NP-EMC-CAV-001` (audio is absent from its source table); `NP-BIB-EMF-001` §5.4 |
| SEL-P10 | Time-varying emission: audio-frequency magnetic and electric field from the drive current at the carrier and its harmonics, and the amplifier's switching edges if it is class D | Depends on amplifier location (§8.2.4). The ADS1299 samples at 500 Hz and the carrier is 440 Hz, above its Nyquist rate, so what the converter's filter does to a 440 Hz interferer is an EMC question to answer from its datasheet | Carrier pickup in the EEG, aliased into the band the neurofeedback reads, in the modality that closes its loop on that EEG | `CLAUDE.md` §3 (ADS1299, 500 Hz); `NP-EMC-CAV-001` source analysis |
| SEL-P11 | Mass, centre of mass and envelope (frame outer size and thickness around the 40 mm active diameter) | Vendor weight, then weigh the assembly. The mass budget is `OI-AUDIOHW-05`'s and does not exist | The assembly loads the fit system with an unbudgeted mass and the driver does not fit the cup | `REQ-AUDIO-02`; `CLAUDE.md` §4.4; §7 (mass and centre of mass); `OI-AUDIOHW-05` |
| SEL-P12 | Sensitivity drift over temperature, humidity, sweat exposure and life, and how the drift is monitored | Vendor life data, then bench ageing | The calibrated map from `level_half_db` to sound drifts above 85 between verifications | `REQ-AUDIO-19`(c); `OI-AUDIOHW-11` |
| SEL-P13 | Terminals, conductor gauge, polarity and strain relief of the driver's own leads | Fixed together with `OI-AUDIOHW-07` | The cup-to-hub connector cannot be chosen once across the head-worn accessories | `OI-AUDIOHW-07` |

**A11.2 — bone-conduction element (`SEL-B`)**

| ID | Parameter the selection must specify | Derived relation or where the value comes from | What fails if it is not met | Traceable to |
|---|---|---|---|---|
| SEL-B01 | Construction (bimorph, stack, diaphragm-coupled) and how it couples to the isolator | `REQ-AUDIO-03` fixes piezoelectric and a silicone isolator | The isolator, mounting and drive electronics are specified against nothing | `REQ-AUDIO-03` |
| SEL-B02 | Force per volt `K_F(f)` in µN/V on an IEC 60318-6 artificial mastoid, at the static force it runs at, over the D4 band | D3 | The `REQ-AUDIO-21` ceiling cannot be calibrated or verified: a force level is the only unit it is stated in | `REQ-AUDIO-21`(a),(d); `REQ-AUDIO-20` |
| SEL-B03 | Maximum clean force level at each carrier and the THD at it; limit **not set** | D3 with `H_B`. The reference electromagnetic vibrator is reported to distort strongly at low frequency, so this row is not a formality | The element cannot deliver `L_F,max` without distortion or a felt buzz, or only by running its drive at the limit | `REQ-AUDIO-21`; §5.2 (whether an element can deliver the ceiling cleanly is the element's characterisation) |
| SEL-B04 | Static contact force and contact area at which B02 and B03 were measured, and `K_F` versus static force across the range the fit system can produce | Static force comes from the cup clamp (`OI-AUDIOHW-05`). The load dependence of output force is in the §8.2.3 sources | Output differs between wearers and heads with the same command, and the ceiling is verified on a load the product never sees | `REQ-AUDIO-21`(d); `OI-AUDIOHW-05`; `REQ-AUDIO-10` |
| SEL-B05 | Mechanical resonance (free and loaded through the isolator) and its Q, relative to the carriers | Compared with D4 | A resonance near a shipped carrier makes force per volt vary steeply with the exact carrier, with temperature and with contact force | `REQ-AUDIO-21`(b) (the ceiling is a function of frequency) |
| SEL-B06 | Capacitance versus frequency, dissipation factor, maximum drive voltage (peak), and the DC bias the element tolerates without depolarising | `V_B` from D3 against the rail. The zone-announce firmware already notes DC bias through the piezo | The drive stage cannot reach the force, or a DC term at stop or start damages the element and moves the device off its calibration | `REQ-AUDIO-21`; `np_zone_announce.h` (OI-ZA-04) |
| SEL-B07 | Isolation of the drive voltage from the skin: dielectric withstand and leakage across the isolator and housing | Value comes from the drive voltage in SEL-B06 and the applicable patient-leakage limit, **not read here** | The assembly puts current into the wearer, which moves A11 into the §4.2 table and requires a safety-MCU enable line | `REQ-AUDIO-09`; `REQ-AUDIO-12` |
| SEL-B08 | Isolator Shore durometer and thickness, and the loss factor of the silicone; **not set** | Material is `OI-AUDIOHW-06`'s | `K_F`, resonance and comfort depend on it, and a different silicone changes the calibrated force | `REQ-AUDIO-03`; `OI-AUDIOHW-06`; `REQ-AUDIO-21` |
| SEL-B09 | Mass, mounting, and the contact pressure the clamp puts on the mastoid | A pressure limit for the mastoid is **not in the record** and is a question for human-factors | Discomfort or a pressure injury on a long session, and a bone path no one will keep seated | `CLAUDE.md` §4.4; `OI-AUDIOHW-05`; `REQ-AUDIO-10` |
| SEL-B10 | Airborne sound the element radiates into the cup volume at the force ceiling | Measured with the in-cup microphone and the ear simulator | The planar ceiling and the seal measurement both see an uncounted source, and the two ceilings are not independent | `REQ-AUDIO-21`; `REQ-AUDIO-14`; `OI-AUDIOHW-13` item 3 |
| SEL-B11 | Electric field from the drive conductors and the element, and for an electromagnetic class the static and alternating magnetic field, near the `A1`/`A2` reference pads and the temporal sites | Depends on drive voltage and cable routing (§8.2.4) | Pickup in the linked-ear reference degrades every EEG channel | `REQ-NET-19`; `NP-EMC-CAV-001` source analysis |
| SEL-B12 | Termination and fatigue of the element's leads at the isolator, and drift of `K_F` with temperature, sweat and age | Vendor data, then bench | A lead fails in service, or the calibration drifts above the ceiling | `REQ-AUDIO-21`(d); `OI-AUDIOHW-07` |

#### 8.2.3 Decision matrices — candidate technology classes

**Reading rule.** Each cell is marked **cited** (a public source states it, URL in the key) or **reasoned**
(derived here from physics or from the record, with no source). The cited sources are manufacturer and
review pages, patents and clinical papers, not controlled comparisons, and **their pages were read as
search-result summaries only; none was opened in full** (the session could not reach those hosts). A cited
cell is a lead to check before it is relied on, not a datasheet value. Opening each source and recording what
it says is part of the selection (§8.2.5, item 4).

*Source key.* [S1] https://headphones.com/blogs/features/headphone-driver-types-explained ·
[S2] https://www.campfireaudio.com/blogs/insights/iem-driver-types-balanced-armature-dynamic-planar ·
[S3] https://theaudiostuff.com/glossary/planar-magnetic/ ·
[S4] https://www.hifinews.com/content/mrspeakers-ether-2-headphones-lab-report ·
[S5] https://www.hifidiprinzio.it/en/hi-fi-complements/headphones/meze-audio-meze-audio-empyrean.html (a manufacturer claim) ·
[S6] https://patents.google.com/patent/US8107646 ·
[S7] https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/11611836 ·
[S8] https://vde-verlag.de/proceedings-de/456391050.html ·
[S9] https://patents.google.com/patent/US7869610 ·
[S10] https://patents.google.com/patent/US7376237 ·
[S11] https://research.chalmers.se/en/publication/501681 (Radioear B71 and B81 on the mastoid) ·
[S12] https://research.chalmers.se/en/publication/122966 (bone transducer output force and load condition).

**A. Over-ear 40 mm transducer. Planar is the locked class** (`REQ-AUDIO-02`); the other two columns are shown for comparison only.

| Property | Planar magnetic (locked) | Dynamic (moving coil) | Balanced armature |
|---|---|---|---|
| Distortion | Often described as lower, because the thin diaphragm carries its conductor and has no separate coil to break up — **cited** [S1][S3] | Can distort at high level from voice-coil movement — **cited** [S1] | Described as detailed, but limited in range as a single driver — **cited** [S1][S2] |
| Mass and size | Heavier and larger because of large magnet arrays on a large diaphragm; lighter designs exist — **cited** [S1][S3][S4] | Lighter for the same class — **cited** [S1] | Very small; used mostly in-ear — **cited** [S1][S2]. A 40 mm over-ear role is not what the class is used for — **reasoned** |
| Sensitivity and impedance | Often low impedance and low sensitivity, so it needs current rather than voltage; impedance nearly flat across frequency — **cited** [S3] | Typically less demanding of the amplifier — **reasoned**; described as less efficient by one source — **cited** [S1] | Not stated in the sources read |
| Low-frequency output | Large diaphragm moves air at low frequency — **reasoned**; excursion limit at brown-noise level is the open question (gap 2) — **reasoned** | Deep bass is credited to the class — **cited** [S1] | Weaker at strong bass — **cited** [S1][S2] |
| Stray magnetic field | Two magnet arrays around the diaphragm, so a field outside the cup that depends on the shielding plates — **reasoned**. One manufacturer claims its plates keep most of it off the head — **cited** [S5], a marketing claim, **not evidence**. Field magnitude is a vendor plot to request (§8.2.6) | One magnet and coil; smaller field source — **reasoned** | Small magnet near a small armature — **reasoned** |
| Rear radiation | The back wave leaves through the magnet plate, so open-back is common and a closed back needs deliberate damping — **reasoned**. Bears on `REQ-AUDIO-16` | Back wave is usually sealed or vented behind the cone — **reasoned** | Sealed housing — **reasoned** |
| Fit with `CLAUDE.md` §3 | **Locked** | Reopens §3 modality ⑦ | Reopens §3 modality ⑦ |

**B. Bone-conduction element at the mastoid. Piezoelectric is the bound class** (`REQ-AUDIO-03`, from `docs/reference/modality-stack.md`). Choosing another reopens that row.

| Property | Piezo (bound) | Electromagnetic vibrator (B71 type) | Balanced-armature-type vibrator |
|---|---|---|---|
| Low-frequency output | Practical piezo vibrators are reported to resonate around 1 kHz or higher, leaving low-frequency reproduction weak — **cited** [S6], a patent background statement, not a measurement; needs relatively higher voltage at low frequency — **cited** [S7]. **Every shipped bone carrier (200 and 440 Hz and the 150–880 Hz tones) is below that figure** — **reasoned** from the record | Output falls above 6–8 kHz; low frequency is its strength relative to piezo — **cited** [S7][S11] | Balanced design claims a lower resonance without added weight — **cited** [S9][S10], patents |
| Distortion | Not stated for the shipped band in the sources read | The B71 is reported to produce high harmonic distortion at low frequency, the B81 substantially less — **cited** [S11] | Claimed lower than unbalanced vibrators — **cited** [S9], the patentee's claim |
| Size and mass | Main advantage is a small, slim, light element — **cited** [S8] | Heavier; low-frequency response by adding mass — **cited** [S10] | Smaller than a conventional vibrator — **reasoned**; the sources read give no size |
| Drive | Capacitive, so a voltage-driven high-impedance load; a high-voltage stage may be needed — **reasoned**; needs more voltage at low frequency — **cited** [S7] | Current-driven coil — **reasoned**; high current consumption — **cited** [S7] | Current-driven coil — **reasoned** |
| Stray field and EMC | No magnet. High drive voltage and its edges couple capacitively — **reasoned** | Magnet and coil at the mastoid, beside the `A1`/`A2` reference pads; susceptible to environmental electromagnetic interference — **cited** [S7] | Magnet and coil, small — **reasoned** |
| Reference role | None | The audiometric reference vibrator whose thresholds underlie the ISO 389-3 values the `REQ-AUDIO-21` ceiling is taken from — **cited** [S11] | None |
| Load dependence | Output force depends on the load it drives, for every class — **cited** [S12]. The ceiling is stated on the artificial mastoid, not on a head | Same | Same |
| Fit with the record | **Bound** by `REQ-AUDIO-03` | Reopens `REQ-AUDIO-03` | Reopens `REQ-AUDIO-03` |

**The one finding the matrix raises.** The bound piezo class and the shipped bone content meet at the
frequency where the cited evidence says piezo is weakest. That is **not** a finding that piezo fails: the
sources are patents and a conference abstract, and a given element can be built for the band. It is a reason
to measure `SEL-B02` and `SEL-B03` on candidate elements at 200 and 440 Hz **before tooling commits**, and
to bring the result to the principal as the one case where the bound class might not reach the bound
ceiling cleanly. If it does not, the options are a different element class (`REQ-AUDIO-03` reopened) or a
carrier range for the bone path chosen to suit the element (gap 1). Neither is chosen here.

#### 8.2.4 Interface questions the selection forces

| # | Question | What the selection changes | Open item |
|---|---|---|---|
| Q1 | **Where the amplifiers sit** (`REQ-AUDIO-04a`): all in the hub, all in the cup, or planar in the hub and bone in the cup | A planar driver with low impedance and low sensitivity needs current (`SEL-P01`, `SEL-P02`), which a hub-side stage must carry down the cable (conductor gauge and loss, mass on the head). A piezo needs voltage (`SEL-B06`), so a stage that makes it needs a rail the hub PCB has not recorded. An in-cup stage puts power, heat and, if class D, switching edges beside the temporal EEG sites, where `NP-EMC-CAV-001` has no audio source and `REQ-AUDIO-09` says the cup delivers nothing to the wearer | `REQ-AUDIO-04a`; `OI-AUDIOHW-04` **DECIDED 2026-11-09 (principal): all in the hub, `REQ-AUDIO-04a`.** |
| Q2 | **Cable conductors** | Per transducer class: planar drive pair per ear; one or two bone drive conductors; microphone signal, bias and return per cup if analogue, or clock and data if PDM; shield; and power and a digital link if any stage is in the cup. An analogue microphone line running beside a loaded planar line is a crosstalk case — **reasoned**. The conductor total follows from Q1 and is not counted here | `OI-AUDIOHW-07` |
| Q3 | **Microphone type** | Analogue output adds no clock near the EEG but needs a preamplifier and a quiet routing; PDM adds a clock of the order of a megahertz (§6.4.6). Where the converter sits is Q1's answer | `OI-AUDIOHW-09` |
| Q4 | **Hub codec channels** | At least two planar channels and one or two bone channels (gap 3), against a codec the hub document does not record. `REQ-AUDIO-11` forbids a design in which bone and planar share one channel | `REQ-AUDIO-11`; `NP-HW-HUB-001` (no audio section) |
| Q5 | **Hardware limit behind the 85 dB** | `REQ-AUDIO-19` refuses in the compiler and the hub driver, both Class B. How far the hardware can exceed 85 dB (`H_P`) is a selection outcome, and whether a hardware limit is needed is an ISO 14971 decision | `OI-AUDIOHW-12` |
| Q6 | **Tier landing** | A change to the cup's connector or conductor set lands on both tiers or neither | `REQ-UPG-03` |

**Recommendation. Items 1 and 2 were adopted on 2026-11-09 (principal) as `REQ-AUDIO-04a`; items 3 to 5 remain recommendations awaiting the principal.**

1. **Keep the cup free of amplification and clocks, and amplify in the hub** (Q1), *unless* the candidate
   planar part turns out to need a current the cable cannot carry acceptably. Reasoning: the cup is
   the one place that is both beside the temporal EEG sites and in contact with the wearer; every active
   stage put there adds an EMC source the analysis has not yet counted (`SEL-P10`, `SEL-B11`) and, for the
   bone element, a drive voltage one insulation fault from the skin (`SEL-B07`). The cost is a longer
   analogue run, so the selection should **prefer the planar candidate with the higher sensitivity and
   impedance**, which lowers cable current, over one chosen for other properties alone.
2. **If the piezo needs a high-voltage stage, place that stage in the hub too,** and carry the voltage
   on an insulated conductor to the element, rather than putting a converter at the mastoid. The
   isolation row (`SEL-B07`) then specifies the element and cable, not a converter.
3. **Choose the microphone's type after Q1,** analogue if the converter stays in the hub and PDM only if a
   stage is in the cup anyway.
4. **Measure before committing:** the two measurements that can change the class decision are
   `SEL-B02`/`SEL-B03` at 200 and 440 Hz, and `SEL-P09` at the fluxgate and temporal sites.
5. **Decide the carrier range and the noise band first** (gaps 1 and 2). They decide the bands in D2 and
   D4 and the headroom in `SEL-P06`, and a selection made before them fixes a spec against content nobody
   has bounded.

#### 8.2.5 What is delivered, and what awaits a decision

Delivered: the parameter tables (§8.2.2), the derived relations D1–D4, the class matrices with their source
status (§8.2.3), the forced questions and a recommendation (§8.2.4) and the vendor request list (§8.2.6).
**Awaiting the principal:** (1) ~~the amplifier location~~ **decided 2026-11-09: in the hub, `REQ-AUDIO-04a`**, and the choice of any part;
(2) whether to bound the carrier range, and the noise band (gaps 1 and 2); (3) whether the bound piezo class
stands if the §8.2.3 finding is borne out by measurement; and (4) opening the cited sources, which also
belongs to the selection. **No register row is added:** this subsection sets no figure, so there is no
choice without derivation to register. The first margin, distortion limit or stray-field limit chosen in a
later revision adds its row in that change (`CLAUDE.md` §19).

#### 8.2.6 Vendor datasheet quantities to request

For **each planar candidate:** impedance magnitude and phase versus frequency, with tolerance · sensitivity
(dB SPL per volt and per milliwatt, stated measurement method) · frequency response and its tolerance, and
left-right match · THD versus level and frequency · rated and peak power, and the damage threshold ·
diaphragm linear excursion · back-wave and venting construction, and a closed-back variant if one exists ·
magnet material and grade, with a **stray-field plot** (static, versus distance, on and off axis) and the
frame's ferromagnetic content · mass, centre of mass and outline drawing · operating temperature, humidity
and sweat rating, and the sensitivity drift data over life · terminals and lead details.

For **each bone candidate:** construction · force per volt on an IEC 60318-6 artificial mastoid at a stated
static force and area, over the D4 band, and force-versus-static-force data · maximum force level
and THD at it · free and loaded resonance and Q · capacitance versus frequency and dissipation factor (or,
for an electromagnetic type, coil impedance) · maximum drive voltage or current and the DC bias limit ·
dielectric withstand and leakage of the housing · mass and outline · airborne radiation if measured ·
magnetic field plot (electromagnetic types) · operating and ageing data · lead termination details.

For **both:** whether the vendor has measured the part on the IEC 60318-4 and 60318-6 couplers and will
supply the raw curves, not only the datasheet summary, and the lot-to-lot spread.

---

## 9. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-AUDIOHW-01** | **DECIDED 2026-11-09 (principal, Rev 6): 85 dB SPL at the ear on the planar path, absolute scale, level ceiling only — bound as `REQ-AUDIO-19`/`-20` (§5.1).** No exposure-duration or daily-dose model, because the device cannot know exposure outside sessions. Succeeded by `OI-AUDIOHW-11` (calibration and verification at the ear) and `OI-AUDIOHW-12` (check the citations). **As raised:** no acoustic output ceiling existed, and `volume_pct` was an uncalibrated 0–100 wire field. The in-cup microphone (`REQ-AUDIO-14`) gives a measured in-cup level per wearer and fit (§6.4.2), not at the eardrum and blind to bone conduction; the ceiling also bounds the seal probe tone | Systems + Clinical | — (decided) |
| **OI-AUDIOHW-02** | **DECIDED as a rule 2026-11-09 (principal, Rev 7): the bone path is held to the same hearing level as the planar path, stated as an absolute force level in dB re 1 µN on an IEC 60318-6 artificial mastoid — bound as `REQ-AUDIO-21` (§5.2).** The figure and the wire field are `OI-AUDIOHW-13`. No firmware gating in the interim: the bone HAL is an unbuilt stub and no element is selected. **As raised:** no bone-conduction drive ceiling existed, and an air-conduction limit does not transfer to it unexamined | Systems + Clinical | — (decided; figure `OI-AUDIOHW-13`) |
| **OI-AUDIOHW-03** | **The mesh frame's 40 dB RF figure is a shielding claim outside the audited stack (§6.3)** — absent from `CLAUDE.md` §4.3, from `docs/reference/hardware-detail.md` §4.3, from `NP-BIB-EMF-001`'s per-layer audit and from `NP-DT-001` `DI-PERF-22` — on a **user-replaceable** part whose fouling trigger is unimplemented (`OI-ACC-05`). Decide whether it is a programme claim or a supplier figure, and stop quoting it beside audited ones either way | Systems + EMC | Shielding claim integrity; `NP-BIB-EMF-001` completeness |
| **OI-AUDIOHW-04** | **SELECTION PACKAGE READY (§8.2); part selection awaits the principal. Amplifier location DECIDED 2026-11-09: in the hub (`REQ-AUDIO-04a`).** Done: the parameters a selection must fix for each transducer, each with what fails and its trace (`SEL-P01`–`P13`, `SEL-B01`–`B12`); derived relations D1–D4 from decided figures (85 dB SPL, `REQ-AUDIO-21`, the shipped carriers); class matrices marked cited or reasoned, with the locked classes marked as locked; the interface questions (amplifier location, cable, microphone, codec channels) and a recommendation labelled as awaiting the principal; and the vendor request list. **Not done, and not decided here:** no part or class is chosen, no value set, no margin chosen, the cited sources were read as search summaries only, and three gaps need a decision first — the authorable carrier range is unbounded, the noise band is unstated, and the codec channel count is unrecorded. **As raised:** select and specify the planar magnetic driver and the bone-conduction element (electrical, acoustic and mechanical parameters). Nothing beyond *"40 mm planar magnetic"* and *"piezoelectric at the mastoid"* existed | EE + Acoustic | A11 tooling; `NP-FAI-AUDIO-001` F1/F2 |
| **OI-AUDIOHW-05** | Specify cup body, ear-pad and bayonet geometry, clamping force and assembly mass. The bayonet's material decision (`docs/reference/durability-maintenance.md`) has no interface behind it, and A11's mass loads the `CLAUDE.md` §4.4 fit system with a figure nobody has written down | ME | A11 tooling; fit verification |
| **OI-AUDIOHW-06** | Specify ear-pad and silicone isolator materials and their biocompatibility basis (prolonged skin contact) | ME + Quality | Hazard analysis; T1 release |
| **OI-AUDIOHW-07** | Select the cup-to-hub cable, connector and strain relief. The same connector gap `NP-HW-TCAP-001` §8 records for the T2 cap and `NP-HW-VNSCLIP-001` §8 for the clip — worth deciding **once** across the head-worn accessories rather than three times | EE + ME | A11 tooling |
| **OI-AUDIOHW-08** | **DECIDED 2026-09-24 (principal, Rev 4) — option B: one in-cup microphone per side, measurement only, no anti-noise; bound as `REQ-AUDIO-14`–`16` (§3.1); succeeded by `OI-AUDIOHW-09` and `-10`.** Analysed in Rev 3 (§6.4), recommendation B. The measurement needs the microphone, not the noise cancelling (§6.4.1); the microphone is also the only route found to a measured level for `OI-AUDIOHW-01` (§6.4.2); anti-noise sits in the band of every shipped carrier (§6.4.3), adds output and instability hazards (§6.4.4), and needs a low-latency loop the hub's SAI/DMA path cannot provide, which would decide `REQ-AUDIO-04a` by force (§6.4.5); bystander audio has no consent subject, so raw audio must be excluded structurally (§6.4.6); and no requirement states what ambient noise costs a session, so under `CLAUDE.md` §18 nothing yet requires anti-noise (§6.4.8). **As raised:** **Add noise cancelling with an in-cup (feedback) microphone, or decide not to.** Intended for session quality; also the only measurement found for the foam's loss of seal (`OI-ACC-04`, §6.1) and a candidate for mesh fouling if the mic sits on the ear side of the mesh (`OI-ACC-05`, §6.2). **It reverses a property the record states — there is no microphone anywhere in the design** — so it carries: a privacy decision (on-device processing only, a seal score out, no audio stored; the score depends on the wearer's head and is presumptively UHDR); an acoustic-safety one (anti-noise adds output and can howl when the seal breaks, which makes `OI-AUDIOHW-01` more pressing); and placement, cable and power consequences for `OI-AUDIOHW-05` and `-07`. Microphones put no current, light or field into the assembly, so `REQ-AUDIO-12` is not triggered | EE + Acoustic + Privacy | — (decided) |
| **OI-AUDIOHW-09** | **Select the in-cup microphone** (`REQ-AUDIO-14`). Decide analogue vs digital (PDM) output knowing that a PDM part runs a clock of the order of a megahertz beside the temporal EEG sites and is a new source for `NP-EMC-CAV-001`'s analysis; confirm the hub codec (`NP-HW-HUB-001`) has an input per cup; add the microphone conductors to `OI-AUDIOHW-07`'s cable; fix the position within the front volume, and decide whether it goes on the ear side of the mesh (only `OI-ACC-05`'s candidate needs that). The cable change is a module interface under `REQ-UPG-03` and lands on both tiers | EE + Acoustic + EMC | A11 tooling; `OI-ACC-04` closure |
| **OI-AUDIOHW-10** | **Implement the seal measurement in firmware.** Microphone HAL (a new entry beside `OI-AUDIO-01…08`), the probe signal and its level (bounded by `OI-AUDIOHW-01`), the powered capture window, reduction to a seal score, and `REQ-AUDIO-15`'s containment: decide whether a CI gate in the manner of `scripts/check-redaction-shape.ts` enforces that no sample leaves the reduction call. The dB criterion is `OI-ACC-04`'s, not this item's | FW + Privacy | `OI-ACC-04` closure |
| **OI-AUDIOHW-11** | **PART DECIDED on paper 2026-11-09 (the method, §5.3); the measurement and the margin's size stay OPEN because no driver is selected (`OI-AUDIOHW-04`).** **Decided:** the ceiling is verified and the level-to-drive table measured at the eardrum point of an ear simulator, per unit and per cup, never at a coupler or the in-cup microphone (`REQ-AUDIO-22`); the in-cup microphone is a factory reference reading and a field indicator only, and **no cup-to-eardrum correction exists or is needed** because nothing enforces the ceiling from it; the command ceiling is C = 85 − M with M the arithmetic sum of m1 (simulator chain), m2 (table representation) and m3 (drift), quantisation held to zero by rounding down (`REQ-AUDIO-23`); the table is SHDR-class, bound to its chain, fails closed and is written only at factory or service (`REQ-AUDIO-24`); the A-weighting question is answered by computation from the IEC 61672-1 formula (−4.10 dB at 440 Hz, +1.27 dB at about 2.5 kHz) and by a bound of 1,000 Hz on every tone frequency rather than a frequency-dependent ceiling (`REQ-AUDIO-25`). **Open:** (1) measure a built unit: the sizes of m1, m2 and m3, and so M (`UC-133`); the shipped 85 is a zero-margin stand-in until then; (2) choose the exact simulator standard and configuration for a circumaural cup against the standards' text (candidates IEC 60318-4 with a pinna simulator, IEC 60318-1; both carried from memory, `OI-AUDIOHW-12`); (3) ~~the principal confirms `REQ-AUDIO-25`~~ **confirmed 2026-11-09 (principal), for now** (§5.3.1); revisit if researchers need a tone above 1 kHz, by the frequency-dependent ceiling derived there; (4) implement the 1,000 Hz refusal in the compiler and hub driver, and the calibration record and its fail-closed check; (5) whether the record is signed or only CRC-bound is an ISO 14971 decision (`OI-RISK2-02`, with `OI-AUDIOHW-12`), and signing would be a Rust primitive (`CLAUDE.md` §22); (6) simulator-to-wearer difference is unbounded by any factory measurement and is assumed (`UC-134`); (7) the ISO 389 air reference in `REQ-AUDIO-21`(b) is stated for a particular coupler or simulator, and the planar 85 and that reference must be on the same one or differ by the standard's own figure (`OI-AUDIOHW-13`); (8) peak (impulsive) level is not bounded here. **As raised:** calibrate and verify the 85 dB SPL ceiling at the ear (`REQ-AUDIO-19`(c), `REQ-AUDIO-20`(b),(c)); nothing maps `level_half_db` to a driver and the HAL stubs trap | EE + Acoustic | T1 release; `NP-FAI-AUDIO-001` |
| **OI-AUDIOHW-12** | **NARROWED 2026-11-09 (not closed).** Checked against public excerpts: NIOSH 98-126 (85 dBA, 8 h, 3 dB) **verified**; WHO–ITU 80 dB(A)/40 h adult **verified**, and H.870 has a 2022 edition superseding 2018; Directive 2003/10/EC Art. 3 (85 dB(A), 140 Pa) **verified**; IEC 60318-6:2007 title and number **verified**; ISO 389-3:2016 title and scope **verified**. **Corrected:** the claim that EN 50332 treats 85 dB(A) as the normal maximum was unsupported and is withdrawn (§5.1). **Remains open:** (1) the IEC 62368-1 cl. 10.6 and EN 50332-1/-2/-3 texts (paywalled) and the RS1/RS2 wording, only summaries were read; (2) IEC 60645-1:2017 cl. 6.1.5 number and content; (3) ISO 389-1/-7 values and whether either fits a circumaural cup, and the ISO 389-3 values (`OI-AUDIOHW-13`); (4) a re-read of the NIOSH, WHO–ITU and Directive texts at their own hosts, since those were unreachable from the checking environment; (5) whether the hazard control sits in Class B code alone or needs more (the ceiling is enforced in compiler and hub driver, both outside a Class C path), an ISO 14971 decision for `OI-RISK2-02` **Also open, added by `OI-AUDIOHW-11` (§5.3):** (6) the IEC 61672-1 A-weighting formula and constants (computed from memory of the public formula), and (7) the ear-simulator standards IEC 60318-4 and IEC 60318-1, both carried from memory. | Systems + Regulatory | Hazard analysis  |
| **OI-AUDIOHW-13** | **Compute the bone ceiling and give it a field** (`REQ-AUDIO-21`). **State 2026-11-09: part 1 attempted and still open; part 2 not changed, options below; part 3 recommended, not decided; part 4 open (hardware).** **(1) The figure.** Needs two values at 500 Hz: the ISO 389-3 mastoid RETVFL (dB re 1 µN) and the ISO 389-1/-7 RETSPL (dB re 20 µPa) for the air reference that matches the cup. A public-web search found no source that states either with a citation (only standard titles, scope and terminology, and an unverified remark that the 1998 RETFL values carry into the 2016 RETVFL), and the sources that might hold the tables were not reachable from the session (their hosts are outside its allowed network). **Nothing was computed.** To close: someone with the standards (ISO 389-3:2016 and ISO 389-1:2017 or ISO 389-7) reads the two 500 Hz values, **and** says which air reference applies (supra-aural, circumaural or insert, and which coupler: `OI-AUDIOHW-11` chooses the measurement point, §5.1), then L_F,max = RETVFL + (85 − RETSPL). A value read from a secondary source goes in marked *carried from a secondary source, not checked against the standard*, with its URL, and stays in the register (`CLAUDE.md` §19) until `OI-AUDIOHW-12` checks it. The 440 Hz carrier sits between standard frequencies; using the 500 Hz row is a choice to state, and once the neighbouring rows (250 Hz and 500 Hz) are both read, compute L_F,max at each and say which is used. **(2) The wire: not changed, and why.** `np_mod_audio_hal_bone_set` still receives the planar `level_half_db`, which is not a bone level (§4). Adding the field is mechanically small on the firmware side and **not well-bounded as a change**: it touches `np_mod_audio_params_t` (8 bytes today, `NP-FW-HUB-001` §4.6) and `encode_audio`, the two golden files, the simulator runtime bundle, and the authoring layer, because a bone level has no protocol key yet (`boneConductionPacer` is a boolean in `common/npps/fields.json`, `NP-NPPS-REF-001`, and the parsers, editors and fixtures of six platforms read it). It also cannot be enforced without part 1's figure, so a field added now would carry a level with no ceiling behind it, which `REQ-AUDIO-21`(c) does not allow. Principal decisions needed, with the recommendation: **(a) Unit and width.** Recommended: one new little-endian `uint16` `bone_level_half_db`, 2 × force level in dB re 1 µN, 0 meaning bone off, mirroring `level_half_db` (`REQ-AUDIO-20`); whether a `uint8` would do depends on the figure, which is not known, so do not choose the narrow one before it is. **(b) What happens to `bone_conduct_en`.** Recommended: it is removed and a non-zero `bone_level_half_db` is the enable, so no descriptor can say *on* without saying *how loud*; the block grows from 8 to 9 bytes. **(c) Version.** The precedent is Rev 27 of `NP-FW-HUB-001` (the VNS block widened, `NP_HUB_PROTO_VERSION` not bumped, because `REQ-FWHUB-10`: no descriptor has shipped): an old 8-byte block is refused by `len < sizeof`, which fails closed. Recommended: the same, on both tiers (`REQ-UPG-03`). **(d) Authoring.** Who sets the bone level: a fixed value per protocol (a new `.npps` key, `NP-NPPS-REF-001` Rev bump, and the compiler refuses above L_F,max before signing), or derived from the protocol's air level at equal hearing level by the compiler. Recommended: an authored key, because the compiler's derived value would hide the bone level from the protocol's author, which `REQ-AUDIO-20` forbids. **(e) Interim.** Option A: leave as is (the principal chose record-only on 2026-11-09). Option B: **refuse `bone_conduct_en = 1` in the compiler and the hub driver until the field and figure exist**, so no unleveled bone output can be commanded; this removes the breathing cue from every protocol that uses it, which is why it is not done without a decision. **(3) The combined case (both paths playing): recommendation, not a decision.** The two ceilings are each a hearing level of 85 dB, and both stimuli reach the same cochlea, so at one frequency they add; two independently legal commands can exceed the intended hearing level. Options: **(i)** each path to its own ceiling and nothing more (the Rev 7 text); the worst case is an in-phase sum of two equal contributions, **6 dB** above one (20 log10 2, arithmetic and not an invented limit), and phase between a cup driver and a mastoid vibrator is not controlled; **(ii)** a combined ceiling in hearing level, refusing a command whose two path levels, each expressed as hearing level, sum in amplitude above 85 dB HL (20 log10 of the sum of 10^(HL/20)); it adds no constant, reduces to the single-path ceilings when one path is off, and is conservative because it assumes coherent addition; **(iii)** forbid both at once (bone only when the planar level is 0), the simplest and also removes the breathing cue during a binaural session, which is the shipped use. **Recommended: (ii)**, enforced in the compiler and the hub driver by refusal, once part 1 gives each path its hearing level; (i) only if the principal accepts a 6 dB worst case on the hearing-level ceiling. **(4) Verify on a built unit** (`OI-AUDIOHW-04`, `OI-AUDIOHW-11`): hardware, out of scope here and open. `OI-AUDIOHW-12`'s citation check covers the standards named in §5.2 | Systems + Acoustic + FW | T1 release; before the bone HAL is implemented |
| **OI-AUDIOHW-14** | **Specify the hub-side audio output stage** that `REQ-AUDIO-04a` puts in the hub. `NP-HW-HUB-001` has no audio section: no codec, channel count, amplifier, rail or protection is recorded, and the firmware assumes SAI1/SAI2 for the planar drivers and SAI3 for a bone amplifier. Needs, once the planar part is chosen (`OI-AUDIOHW-04`): amplifier class and rail for the planar current over the cable, the piezo high-voltage stage and its isolation to the element (`SEL-B07`), at least two planar and one or two bone channels with `REQ-AUDIO-11`'s independence, the 85 dB SPL headroom without clipping (`REQ-AUDIO-19`), and the EMC of the stage on the hub PCB (`NP-EMC-CAV-001`). A hub PCB change lands on both tiers (`REQ-UPG-03`) | EE | A9 hub PCB; `OI-AUDIOHW-04` |

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
| 8 | 2026-11-09 | NeurOne Systems Engineering | **§5.1, §5.2, §5.3, §8.2, §9: OI-AUDIOHW-12 narrowed (NIOSH, WHO-ITU, Directive 2003/10/EC, IEC 60318-6 and ISO 389-3 verified from public excerpts; the claim that EN 50332 treats 85 dB(A) as the normal maximum withdrawn; paywalled texts stay open). OI-AUDIOHW-11 part decided: new §5.3 and REQ-AUDIO-22 to -25 (ear-simulator verification per unit and cup, guard band of named terms with open sizes, SHDR-class fail-closed calibration table, A-weighting computed and every tone frequency bounded at 1,000 Hz, confirmed by the principal for now with the reasoning and the route to revisit in new §5.3.1; UC-133, UC-134). OI-AUDIOHW-13: no cited source for the 500 Hz values so nothing computed, wire options and a combined-path recommendation recorded. OI-AUDIOHW-04: new §8.2 selection package (SEL-P01-P13, SEL-B01-B12, relations D1-D4, matrices, interface questions, vendor list), no part selected. No firmware, constant or limit changed.** |
