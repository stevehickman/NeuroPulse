# Audio Cup Assembly — Hardware Specification

**Project:** NeurOne
**Document:** NP-HW-AUDIO-001
**Revision:** 1
**Date:** 2026-09-20
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
| **A11.3 — cup body and ear pad** | Carries A11.1, A11.2 and the mesh frame; the foam ear pad is a consumable (§6.1) | No geometry of any kind |
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

Each row is carried from a document that states it. **No row originates here.**

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

---

## 4. Electrical and firmware interface

The interface is fully determined by shipped Class B code and is restated here so that a hardware
change surfaces as a disagreement with a document rather than with a driver.

| Parameter | Value | Source |
|---|---|---|
| Hub slot | `NP_HUB_SLOT_AUDIO`, module type `NP_MOD_AUDIO` = `0x08` | `firmware/hub_control/include/np_hub_types.h` |
| Transport | I²S from the hub codec via SAI; both paths from the same codec | `NP-FW-HUB-001` §8.5 |
| Command block | `np_mod_audio_params_t` — `mode` (0 binaural, 1 isochronic, 2 pink, 3 brown, 4 off), `carrier_hz`, `beat_mhz`, `volume_pct`, `bone_conduct_en`, `eeg_adaptive` | `np_hub_types.h` |
| Adaptive band map | delta/theta/alpha/beta/gamma → 2/6/10/20/40 Hz (`k_band_beat_mhz`), retuned every `NP_AUDIO_ADAPT_INTERVAL_MS` | `NP-FW-HUB-001` §8.5 |
| Safety enable | `NP_SAFETY_EN_AUDIO` = **0** — no bit is requested and `slot_to_safety_bit()` returns 0 | `firmware/hub_control/include/np_hub_config.h`; §5 |
| Condition telemetry | `np_mod_audio_hal_mesh_impedance()` → float Ω | `firmware/hub_control/modules/np_mod_audio.c` |

> **`REQ-AUDIO-11`** — `bone_conduct_en` is a **per-command** field, so the two transducer paths are
> independently addressable and must remain independently drivable in hardware. The HRV biofeedback
> protocols rely on this: `CLAUDE.md` §3 modality ⑥ reserves the bone-conduction channel for the
> breathing cue while the planar path carries binaural content.

---

## 5. Safety position — the one modality with no enable line

`CLAUDE.md` §4.2's interlock table has no row for audio, and `NP_SAFETY_EN_AUDIO` is deliberately
`0`. **That is correct and this document affirms it rather than treating it as a gap**: the safety
MCU owns *stimulation* enable lines, and audio delivers no current, no light and no field
(`REQ-AUDIO-09`).

> **`REQ-AUDIO-12`** — Any future change that puts current, light or a field into this assembly —
> a transcutaneous electrode on the cup, an LED, an added coil — **moves A11 into `CLAUDE.md` §4.2's
> table and requires a safety-MCU enable line**. It is a Class C change, not a variant.

Two exposure limits that a reader may expect here are deliberately **not** stated, because no
document in the set states them and inventing them is the failure this document exists to avoid:

- **Acoustic output limit.** There is no dB SPL ceiling, no exposure-duration model and no
  hearing-conservation reference anywhere in the document set. `volume_pct` is a 0–100 scale in the
  wire format with no calibration to sound pressure. **`OI-AUDIOHW-01`.**
- **Bone-conduction drive limit.** Likewise absent, and it is the less familiar of the two: bone
  conduction bypasses the ear canal, so an air-conduction limit does not transfer to it unexamined.
  **`OI-AUDIOHW-02`.**

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
threshold.** What it adds is the hardware consequence: closing `OI-ACC-04` needs a foam whose
compression-set behaviour is characterised, and **no foam is specified** (§8).

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
| `CLAUDE.md` §4.3 five-layer stack | **No.** Layers 1–5 are CFRP, mu-metal, palladium-coated polyester, absorber foam and port filters. The mesh is none of them |
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

---

## 7. Hazard-analysis inputs

`NP-RISK-002` §4 records that A11 has **never had a hazard analysis in any revision of the risk
file**, and `OI-RISK2-02` was blocked behind `OI-ART-04` — *"they have no specification to
analyse."* **That block is lifted by this document.** The analysis is still owed; what follows is
the input list, not the analysis.

| Candidate hazard | Why it is on the list |
|---|---|
| Acoustic overexposure (planar path) | No SPL ceiling exists anywhere (§5, `OI-AUDIOHW-01`) |
| Bone-conduction overexposure | Separate limit, separate transfer path, also absent (`OI-AUDIOHW-02`) |
| Mesh refitted wrongly, or a third-party mesh fitted | User-serviceable part carrying an RF claim (§6.3) |
| Mesh fouling undetected | Trigger unimplemented (`OI-ACC-05`); both the acoustic and the RF consequence go unflagged |
| Bayonet failure in service | The part exists **because** the plastic predecessor flattened at 500–1,000 cycles; the replacement has no cycle rating of its own |
| Pinch or entrapment at the bayonet | Head-worn rotating mechanical interface, user-actuated |
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
| **OI-AUDIOHW-01** | **No acoustic output ceiling exists for the planar path.** No dB SPL limit, no exposure-duration model, no hearing-conservation reference, and `volume_pct` is an uncalibrated 0–100 wire field. A consumer wearable that plays tones for 20-minute sessions needs one, and it is a *specification* value, not a firmware constant | Systems + Clinical | Hazard analysis; T1 release |
| **OI-AUDIOHW-02** | **No bone-conduction drive ceiling exists**, and an air-conduction limit does not transfer to it unexamined. Decide the limit and the standard it is taken from | Systems + Clinical | Hazard analysis; T1 release |
| **OI-AUDIOHW-03** | **The mesh frame's 40 dB RF figure is a shielding claim outside the audited stack (§6.3)** — absent from `CLAUDE.md` §4.3, from `docs/reference/hardware-detail.md` §4.3, from `NP-BIB-EMF-001`'s per-layer audit and from `NP-DT-001` `DI-PERF-22` — on a **user-replaceable** part whose fouling trigger is unimplemented (`OI-ACC-05`). Decide whether it is a programme claim or a supplier figure, and stop quoting it beside audited ones either way | Systems + EMC | Shielding claim integrity; `NP-BIB-EMF-001` completeness |
| **OI-AUDIOHW-04** | Select and specify the planar magnetic driver and the bone-conduction element (electrical, acoustic and mechanical parameters). Nothing beyond *"40 mm planar magnetic"* and *"piezoelectric at the mastoid"* exists | EE + Acoustic | A11 tooling; `NP-FAI-AUDIO-001` F1/F2 |
| **OI-AUDIOHW-05** | Specify cup body, ear-pad and bayonet geometry, clamping force and assembly mass. The bayonet's material decision (`docs/reference/durability-maintenance.md`) has no interface behind it, and A11's mass loads the `CLAUDE.md` §4.4 fit system with a figure nobody has written down | ME | A11 tooling; fit verification |
| **OI-AUDIOHW-06** | Specify ear-pad and silicone isolator materials and their biocompatibility basis (prolonged skin contact) | ME + Quality | Hazard analysis; T1 release |
| **OI-AUDIOHW-07** | Select the cup-to-hub cable, connector and strain relief. The same connector gap `NP-HW-TCAP-001` §8 records for the T2 cap and `NP-HW-VNSCLIP-001` §8 for the clip — worth deciding **once** across the head-worn accessories rather than three times | EE + ME | A11 tooling |

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
