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
1.3 dB. Every shipped carrier is 440 Hz; whether a carrier above 1 kHz needs a lower ceiling is
for `OI-AUDIOHW-11`, which measures at the ear.

> **`REQ-AUDIO-19`** and **`REQ-AUDIO-20`** are in §3.1.

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
cup with no anti-noise (`REQ-AUDIO-14`–`16`), and that amplification location is **not** decided
(`REQ-AUDIO-04a`).

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
| Q1 | **Where the amplifiers sit** (`REQ-AUDIO-04a`): all in the hub, all in the cup, or planar in the hub and bone in the cup | A planar driver with low impedance and low sensitivity needs current (`SEL-P01`, `SEL-P02`), which a hub-side stage must carry down the cable (conductor gauge and loss, mass on the head). A piezo needs voltage (`SEL-B06`), so a stage that makes it needs a rail the hub PCB has not recorded. An in-cup stage puts power, heat and, if class D, switching edges beside the temporal EEG sites, where `NP-EMC-CAV-001` has no audio source and `REQ-AUDIO-09` says the cup delivers nothing to the wearer | `REQ-AUDIO-04a`; `OI-AUDIOHW-04` |
| Q2 | **Cable conductors** | Per transducer class: planar drive pair per ear; one or two bone drive conductors; microphone signal, bias and return per cup if analogue, or clock and data if PDM; shield; and power and a digital link if any stage is in the cup. An analogue microphone line running beside a loaded planar line is a crosstalk case — **reasoned**. The conductor total follows from Q1 and is not counted here | `OI-AUDIOHW-07` |
| Q3 | **Microphone type** | Analogue output adds no clock near the EEG but needs a preamplifier and a quiet routing; PDM adds a clock of the order of a megahertz (§6.4.6). Where the converter sits is Q1's answer | `OI-AUDIOHW-09` |
| Q4 | **Hub codec channels** | At least two planar channels and one or two bone channels (gap 3), against a codec the hub document does not record. `REQ-AUDIO-11` forbids a design in which bone and planar share one channel | `REQ-AUDIO-11`; `NP-HW-HUB-001` (no audio section) |
| Q5 | **Hardware limit behind the 85 dB** | `REQ-AUDIO-19` refuses in the compiler and the hub driver, both Class B. How far the hardware can exceed 85 dB (`H_P`) is a selection outcome, and whether a hardware limit is needed is an ISO 14971 decision | `OI-AUDIOHW-12` |
| Q6 | **Tier landing** | A change to the cup's connector or conductor set lands on both tiers or neither | `REQ-UPG-03` |

**Recommendation — awaiting the principal; nothing here is decided.**

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
**Awaiting the principal:** (1) the amplifier location (`REQ-AUDIO-04a`) and the choice of any part;
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
| **OI-AUDIOHW-04** | **SELECTION PACKAGE READY (§8.2); part selection awaits the principal.** Done: the parameters a selection must fix for each transducer, each with what fails and its trace (`SEL-P01`–`P13`, `SEL-B01`–`B12`); derived relations D1–D4 from decided figures (85 dB SPL, `REQ-AUDIO-21`, the shipped carriers); class matrices marked cited or reasoned, with the locked classes marked as locked; the interface questions (amplifier location, cable, microphone, codec channels) and a recommendation labelled as awaiting the principal; and the vendor request list. **Not done, and not decided here:** no part or class is chosen, no value set, no margin chosen, the cited sources were read as search summaries only, and three gaps need a decision first — the authorable carrier range is unbounded, the noise band is unstated, and the codec channel count is unrecorded. **As raised:** select and specify the planar magnetic driver and the bone-conduction element (electrical, acoustic and mechanical parameters). Nothing beyond *"40 mm planar magnetic"* and *"piezoelectric at the mastoid"* existed | EE + Acoustic | A11 tooling; `NP-FAI-AUDIO-001` F1/F2 |
| **OI-AUDIOHW-05** | Specify cup body, ear-pad and bayonet geometry, clamping force and assembly mass. The bayonet's material decision (`docs/reference/durability-maintenance.md`) has no interface behind it, and A11's mass loads the `CLAUDE.md` §4.4 fit system with a figure nobody has written down | ME | A11 tooling; fit verification |
| **OI-AUDIOHW-06** | Specify ear-pad and silicone isolator materials and their biocompatibility basis (prolonged skin contact) | ME + Quality | Hazard analysis; T1 release |
| **OI-AUDIOHW-07** | Select the cup-to-hub cable, connector and strain relief. The same connector gap `NP-HW-TCAP-001` §8 records for the T2 cap and `NP-HW-VNSCLIP-001` §8 for the clip — worth deciding **once** across the head-worn accessories rather than three times | EE + ME | A11 tooling |
| **OI-AUDIOHW-08** | **DECIDED 2026-09-24 (principal, Rev 4) — option B: one in-cup microphone per side, measurement only, no anti-noise; bound as `REQ-AUDIO-14`–`16` (§3.1); succeeded by `OI-AUDIOHW-09` and `-10`.** Analysed in Rev 3 (§6.4), recommendation B. The measurement needs the microphone, not the noise cancelling (§6.4.1); the microphone is also the only route found to a measured level for `OI-AUDIOHW-01` (§6.4.2); anti-noise sits in the band of every shipped carrier (§6.4.3), adds output and instability hazards (§6.4.4), and needs a low-latency loop the hub's SAI/DMA path cannot provide, which would decide `REQ-AUDIO-04a` by force (§6.4.5); bystander audio has no consent subject, so raw audio must be excluded structurally (§6.4.6); and no requirement states what ambient noise costs a session, so under `CLAUDE.md` §18 nothing yet requires anti-noise (§6.4.8). **As raised:** **Add noise cancelling with an in-cup (feedback) microphone, or decide not to.** Intended for session quality; also the only measurement found for the foam's loss of seal (`OI-ACC-04`, §6.1) and a candidate for mesh fouling if the mic sits on the ear side of the mesh (`OI-ACC-05`, §6.2). **It reverses a property the record states — there is no microphone anywhere in the design** — so it carries: a privacy decision (on-device processing only, a seal score out, no audio stored; the score depends on the wearer's head and is presumptively UHDR); an acoustic-safety one (anti-noise adds output and can howl when the seal breaks, which makes `OI-AUDIOHW-01` more pressing); and placement, cable and power consequences for `OI-AUDIOHW-05` and `-07`. Microphones put no current, light or field into the assembly, so `REQ-AUDIO-12` is not triggered | EE + Acoustic + Privacy | — (decided) |
| **OI-AUDIOHW-09** | **Select the in-cup microphone** (`REQ-AUDIO-14`). Decide analogue vs digital (PDM) output knowing that a PDM part runs a clock of the order of a megahertz beside the temporal EEG sites and is a new source for `NP-EMC-CAV-001`'s analysis; confirm the hub codec (`NP-HW-HUB-001`) has an input per cup; add the microphone conductors to `OI-AUDIOHW-07`'s cable; fix the position within the front volume, and decide whether it goes on the ear side of the mesh (only `OI-ACC-05`'s candidate needs that). The cable change is a module interface under `REQ-UPG-03` and lands on both tiers | EE + Acoustic + EMC | A11 tooling; `OI-ACC-04` closure |
| **OI-AUDIOHW-10** | **Implement the seal measurement in firmware.** Microphone HAL (a new entry beside `OI-AUDIO-01…08`), the probe signal and its level (bounded by `OI-AUDIOHW-01`), the powered capture window, reduction to a seal score, and `REQ-AUDIO-15`'s containment: decide whether a CI gate in the manner of `scripts/check-redaction-shape.ts` enforces that no sample leaves the reduction call. The dB criterion is `OI-ACC-04`'s, not this item's | FW + Privacy | `OI-ACC-04` closure |
| **OI-AUDIOHW-11** | **Calibrate and verify the 85 dB SPL ceiling at the ear** (`REQ-AUDIO-19`(c), `REQ-AUDIO-20`(b),(c)). Nothing yet maps `level_half_db` to a driver, because no driver is selected (`OI-AUDIOHW-04`): the HAL stubs trap. Decide the measurement (an ear simulator such as IEC 60318-4, and what the in-cup microphone adds, §6.4.2), the cup-to-eardrum correction, the unit-to-unit tolerance the calibration must leave under 85, and whether a carrier above 1 kHz needs a lower ceiling (§5.1, weighting) | EE + Acoustic | T1 release; `NP-FAI-AUDIO-001` |
| **OI-AUDIOHW-12** | **Check the §5.1 citations against the standards' texts** (NIOSH 98-126, WHO–ITU H.870, IEC 62368-1 cl. 10.6, EN 50332, Directive 2003/10/EC) **and the §5.2 bone standards** (ISO 389-3, IEC 60318-6, IEC 60645-1 cl. 6.1.5, ISO 389-1/-7). Carried from published summaries; the IEC/EN texts are paywalled. Also decide whether the hazard control sits in Class B code alone or needs more (the ceiling is enforced in compiler and hub driver, both outside a Class C path) — an ISO 14971 decision for `OI-RISK2-02` | Systems + Regulatory | Hazard analysis |
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
