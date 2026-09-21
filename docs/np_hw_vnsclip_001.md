# Auricular VNS / HRV Clip — Hardware Specification

**Project:** NeurOne
**Document:** NP-HW-VNSCLIP-001
**Revision:** 1
**Date:** 2026-09-20
**Status:** DRAFT — **requirements-grade. The clip's electrode area is a PROVISIONAL firmware constant and its geometry is unspecified; §8 names each absence rather than supplying a value.**
**Effective Date:** —
**Author:** NeurOne Systems Engineering
**Approved By:** — (new document)
**References:** CLAUDE.md §3 modality ⑥ (VNS + HRV + HRV biofeedback), §3 T2 additions (21-ch qEEG linked-ear reference), §3 hard-limits table (VNS auricular; the 40 µC/cm² per-phase ceiling), §4.2 (VNS contact confirmation), §2.3 (consumables), §5.1 (UHDR/SHDR); `docs/reference/modality-stack.md` (modality 6 detail); `docs/reference/commercial-model.md` §2.3 (VNS clip pads row); NP-FW-HRV-001 Rev 1 §3.5, §5.2 (taVNS constants and interlocks); NP-FW-HUB-001 Rev 1 §8.4 (`np_mod_vns.c`); NP-HW-EEGNET-001 Rev 7 (EEG reference architecture); NP-DT-001 §3.2.1 DI-SAFE-01a (per-phase charge ceiling derivation); NP-SW-001 Rev 8 §5.1 SW01-M03 (charge monitor); NP-ART-001 Rev 3 §2.4 A13, §3.2, §4, OI-ART-04; NP-FAI-001 Rev 3 §2 (F1–F4); NP-RISK-002 Rev 3 §4, OI-RISK2-02; NP-CONV-001 Rev 6 §4, §5, §6; `firmware/hub_control/modules/np_mod_vns.c`; `firmware/hub_control/include/np_hub_config.h` (`NP_VNS_ELECTRODE_AREA_MCM2`)
**Related Issues:** GitHub Issue #332 (OI-ART-04 — A13 hardware has no owning document); `OI-CHARGE-07` (the clip pad area is an unmeasured firmware constant)
**Gate:** NP-COORD-001 G2 (pre-tooling) — A13's entry in the G2 completeness check, not a release of it
**IEC 62304 Class:** N/A (hardware specification). It supplies a value the Class C charge monitor divides by (§5.2) and constrains SW-02 Class B code (`np_mod_vns.c`, `firmware/hrv_biofeedback/`).
**Supersedes:** None — first issue. It ends the state `NP-ART-001` §2.4 records as *"firmware specified, hardware not"*: `NP-FW-HRV-001` remains the firmware specification and stops being the only document A13 has.
**Parent Document:** None

---

> **Why this document exists.** A13's governing specification in `NP-ART-001` §2.4 is
> `NP-FW-HRV-001` Rev 1, annotated *"(firmware only)"*. Firmware is well served: four protocols,
> a constants table, an interlock set. **The part the user clips to their ear has no document at
> all** — no geometry, no clamping force, no electrode material, no pad area, no cable. `OI-ART-04`
> asked for it; this is it.
>
> **Two findings are new here.**
>
> - **§5.2 — the Class C per-phase charge interlock divides by a number that has never been
>   measured.** `NP_VNS_ELECTRODE_AREA_MCM2` = 0.5 cm² is marked *PROVISIONAL* in
>   `np_hub_config.h`, and `OI-CHARGE-07` records it as one of three areas that *"are firmware
>   constants that have never been measured."* **The reason it is legitimately a firmware constant
>   at all is that it is a fixed product part** — and a fixed product part is exactly the thing a
>   hardware specification is supposed to state. This document is where the measurement lands, and
>   until it does, the safety argument has a hardware input with no hardware source.
> - **§4.3 — a consumable sits in the T2 EEG reference path.** The clip's contact pads carry the
>   **A1/A2 linked-ear reference** for the 21-channel qEEG normative montage, and the same pads are
>   a **consumable that degrades electrochemically from VNS current** (20–40 sessions). The
>   programme's clinical reference electrode and its stimulation electrode are the same worn part,
>   and no document says so.

---

## 1. Scope

**In scope, and binding:**

| § | What it pins |
|---|---|
| §2 | The artifact, its sub-parts, and the three functions the single clip performs |
| §3 | The requirements the record already binds |
| §4 | Interfaces — electrical, the 6-conductor cable, and the dual-rating of the pads |
| §5 | Safety: contact confirmation, and the unmeasured area the charge interlock divides by |
| §6 | The hydrogel pad consumable and its trigger under `CLAUDE.md` §2.3 |
| §7 | Hazard-analysis inputs for `OI-RISK2-02` |

**Out of scope:** the four HRV biofeedback protocols and their algorithms (`NP-FW-HRV-001`); the
hub's stimulation and PPG analog front ends (artifact A9, `NP-HW-HUB-001`); the cervical VNS
accessory, which is a **different artifact** (A14, `NP-HW-CVNS-001`) with a different nerve target,
a different electrode and a cardiac interlock this one does not have.

---

## 2. The artifact

**A13 is a bilateral ear-worn clip performing three functions on one part**: transcutaneous
auricular VNS, PPG heart-rate sensing, and EEG reference contact.

| Sub-part | What it is | Status of the record |
|---|---|---|
| **A13.1 — clip body** | Spring clip to the auricle, one per ear | Nothing. No geometry, no clamping force, no size range across ear anatomy |
| **A13.2 — stimulation contacts** | Target: **auricular branch of CN X**; carry the pad (A13.5) | Target named. No contact geometry, **no area of record outside a PROVISIONAL firmware constant** (§5.2) |
| **A13.3 — PPG optics** | **808–830 nm** emitter + detector in the same clip | Wavelength band only. No emitter, no detector, no optical geometry |
| **A13.4 — A1/A2 EEG reference tap** | The clip contact pads doubling as the linked-ear reference | **Dual-rated by decision** (§4.3), with no separate conductor or pad of its own |
| **A13.5 — PDMS hydrogel pad (consumable)** | 2-pack, 20–40 sessions | Interval, price and mechanism stated; **material formulation, area and adhesion are not** |
| **A13.6 — cable and connector** | **6-pin**, of which **2 conductors were spare and are now A1/A2** | The one interface fact the record actually fixes (§4.2) |

> **`REQ-VNSC-01`** — A13 performs three functions on one mechanical part, and they have different
> failure consequences: a degraded pad *under-stimulates* (a therapy issue), *corrupts the EEG
> reference* (a measurement issue, T1 and T2) and *changes the electrode area the charge interlock
> assumes* (a safety-argument issue). A specification, tooling item or FAI that addresses only the
> stimulation duty addresses one of three.

### 2.1 Which configurations ship it

Modality ⑥ is a **T1 shipping modality**, and the clips additionally carry the **T2** 21-channel
qEEG's A1/A2 linked-ear reference (`CLAUDE.md` §3 T2 additions). Per-configuration inclusion is
`docs/reference/commercial-model.md` §2.1's to state; no figure is restated here.

---

## 3. Requirements the record already binds

| ID | Requirement | Source |
|---|---|---|
| **REQ-VNSC-02** | Stimulation target is the **auricular branch of CN X**; the clip is worn on the auricle | `CLAUDE.md` §3 modality ⑥ |
| **REQ-VNSC-03** | **1–25 Hz**, **≤ 2 mA**, **biphasic charge-balanced** | `CLAUDE.md` §3 hard-limits table; `NP_TAVNS_MAX_CURRENT_UA` = 2000 |
| **REQ-VNSC-04** | Per-phase commanded-charge ceiling **40 µC/cm²**, enforced by the safety MCU against the **declared electrode area** | `CLAUDE.md` §3, §4.2; `NP-DT-001` DI-SAFE-01a; `NP-SW-001` §5.1 SW01-M03 |
| **REQ-VNSC-05** | **PPG HRV at 808–830 nm in the same clip** | `docs/reference/modality-stack.md` modality 6 |
| **REQ-VNSC-06** | **A1/A2 EEG references on the clip contact pads**, using **2 spare conductors in the existing 6-pin cable** | `docs/reference/modality-stack.md` modality 6 |
| **REQ-VNSC-07** | **Force contact confirmation** — impedance must fall within **[500 Ω, 5000 Ω]** before an enable is requested, and the **safety MCU independently holds** the enable if contacts are unconfirmed | `CLAUDE.md` §4.2; `NP-FW-HUB-001` §8.4 |
| **REQ-VNSC-08** | Safety-MCU veto is independent of the hub check: `np_tavns_enable_cb_t` requests, the MCU checks clip impedance itself, and a negative response forces disable | `NP-FW-HRV-001` §5.2 |
| **REQ-VNSC-09** | **PDMS hydrogel pads**, replaceable, 20–40 sessions | `docs/reference/modality-stack.md`; `docs/reference/commercial-model.md` §2.3 |
| **REQ-VNSC-10** | The clip supports **RSA-synchronised stimulation** — the gate opens on inspiration and closes on expiration, with a **6 s failsafe** — so its stimulation is gated at the breath, not only at the session | `NP-FW-HRV-001` §5.1, §5.2 |

---

## 4. Interfaces

### 4.1 Electrical and firmware

| Parameter | Value | Source |
|---|---|---|
| Hub slot | 8, `NP_MOD_VNS_HRV` = `0x07`, detected by **accessory-port impedance** | `np_hub_config.h`; `NP-FW-HUB-001` §8.4 |
| Command block | `np_mod_vns_hrv_params_t` — `side`, `freq_mhz` (1000–25000), `amplitude_ua` (≤ 2000), `pulse_width_us` (0 = default **250 µs**), `ppg_enable`, `eeg_ref_enable`, `hrv_proto` | `np_hub_types.h` |
| Contact window | `[VNS_CONTACT_MIN_OHM, VNS_CONTACT_MAX_OHM]` = **[500 Ω, 5000 Ω]** | `NP-FW-HUB-001` §8.4 |
| Safety enable | `NP_SAFETY_EN_VNS_HRV` = bit 7 | `np_hub_config.h` |
| Declared electrode area | `NP_VNS_ELECTRODE_AREA_MCM2` = **500** (0.5 cm²), **PROVISIONAL — NOT MEASURED** | `np_hub_config.h`; `OI-CHARGE-07` |
| HAL | `OI-VNS-01…10` — impedance check, stim set/stop, PPG start/stop, **A1/A2 EEG reference routing** (`np_mod_vns_hal_eeg_ref_route`), RMSSD, coherence, HR, ms tick | `np_mod_vns.c` |

> **`REQ-VNSC-11`** — `pulse_width_us` is a **`uint8_t`**, so the wire format cannot express a phase
> longer than 255 µs. **No document states a pulse-width range for auricular VNS** — the 200–1000 µs
> range in the record belongs to *cervical* VNS (`NP-FW-CVNS-001` §3.1), a different artifact. The
> clip's range is therefore bounded by a struct field and by nothing else, and the bound has never
> been reviewed as a requirement. Stated here so it is a decision rather than an artefact
> (**`OI-VNSCLIP-04`**).

### 4.2 The cable is the one fixed interface

*"A1/A2 EEG references on clip contact pads (**2 spare conductors in existing 6-pin cable**, +$15
BOM)"* — `docs/reference/modality-stack.md`.

> **`REQ-VNSC-12`** — The clip cable is **6 conductors**, and after the A1/A2 decision it has
> **zero spares**. Any future function added to the clip — a temperature sensor, a second PPG
> channel, an accelerometer for motion rejection — is a **cable change**, not a firmware change.
> This is the only accessory in the set whose conductor budget is both stated and fully spent, and
> it is worth stating because the spare pair was assumed available once already.

The **connector** at either end is still unselected — the same gap `NP-HW-AUDIO-001` §8,
`NP-HW-NASAL-001` §8 and `NP-HW-TCAP-001` §8 each record for their own artifact
(**`OI-VNSCLIP-06`**).

### 4.3 The pads are dual-rated, and one of the two duties is a consumable

`REQ-VNSC-06` puts the **A1/A2 linked-ear reference on the clip contact pads**. `CLAUDE.md` §3's T2
additions make those same A1/A2 sites the **linked-ear normative reference** for the 21-channel
qEEG. `REQ-VNSC-09` makes the pad that forms the contact a **consumable with a 20–40 session life,
whose stated degradation mechanism is electrochemical attack by the VNS current itself.**

Putting those three together — which no document currently does:

1. **The clinical EEG reference is on a wear part.** Its impedance drifts across the pad's life by
   design, not by fault.
2. **The wear is driven by the other modality.** A user who runs VNS heavily degrades the reference
   for their EEG sessions; a user who runs EEG only does not, so reference quality depends on
   unrelated usage.
3. **Only the stimulation duty has a trigger.** The consumable prompt counts VNS sessions
   (`CLAUDE.md` §2.3's own worked example). **Nothing monitors the pad as an EEG reference** —
   the `[500 Ω, 5000 Ω]` contact window is a stimulation-contact check, and a pad well inside it can
   still be a poor µV-scale reference. `NP-DRV-SHELL-002`'s **< 5 µVpp** artifact threshold is the
   relevant quantity on the EEG side, and nothing measures it here.

> **`OI-VNSCLIP-01`** — Decide whether the A1/A2 reference stays on the stimulation pads. If it
> does, the pad needs an **EEG-grade** acceptance criterion alongside its stimulation one, and the
> consumable trigger has to cover both duties; if it does not, A13 needs a separate reference
> contact and `REQ-VNSC-12` says that costs a conductor the cable does not have. **This document
> does not decide it** — it is a measurement-quality decision with T2 clinical consequences.

---

## 5. Safety

### 5.1 What is enforced

Contact confirmation is the clip's principal interlock and it is **two independent checks**: the hub
confirms `[500 Ω, 5000 Ω]` before requesting `NP_SAFETY_EN_VNS_HRV`, and the safety MCU
independently reads impedance and holds the enable if contacts are not confirmed (`CLAUDE.md`
§4.2). `NP-FW-HUB-001` §8.4 states the relationship exactly — *"the hub check exists to avoid
asking, not to substitute for the MCU's refusal"* — and this document does not alter it.

RSA-synchronised protocols add a **6 s failsafe** on the inspiration gate (`REQ-VNSC-10`), which is
a firmware control over a hardware path and is specified in `NP-FW-HRV-001` §5.2.

### 5.2 The unmeasured divisor — finding

The per-phase ceiling is **40 µC/cm² × the declared electrode area**. For this channel the declared
area is a firmware constant:

```c
#define NP_VNS_ELECTRODE_AREA_MCM2 500U  /* 0.5 cm² auricular clip pad — PROVISIONAL */
```

`OI-CHARGE-07` records why a fixed product part's area may legitimately live in firmware — the user
cannot substitute it, unlike a tDCS pad, so it is a device property rather than a protocol-authoring
fact — and records that it **has never been measured**. The fail-safe direction is asserted rather
than trusted: `np_safety_spi_proto_tests.c` pins the constant non-zero and no larger than the
permissive default, so it may be revised **down** freely and **up** only against a measurement.

**Upward is the direction a real measurement is most likely to go**, because 0.5 cm² is a small pad
and a clip pad is easy to make larger. That is the case the test deliberately blocks, and it can
only be unblocked from here:

> **`REQ-VNSC-13`** — The clip pad's **contact area is a property of this artifact and is specified
> here**, measured on the pad as manufactured (wetted contact area, not the pad outline), with a
> tolerance. `NP_VNS_ELECTRODE_AREA_MCM2` then carries that value and stops being provisional.
> **This document cannot supply the number** — there is no pad, no drawing and no measurement
> (**`OI-VNSCLIP-02`**, and it closes the hardware half of `OI-CHARGE-07`).

Margin today is not the concern and should not be used to defer this: `OI-CHARGE-07` puts VNS at
**3 % of the per-phase ceiling** at the provisional area. The defect is that a **Class C safety
argument has a hardware input with no hardware source**, which is true at 3 % exactly as it would be
at 90 %.

### 5.3 What the record does not limit

- **Clamping force.** The clip is worn on cartilage for 5–20 minute sessions
  (`NP-FW-HRV-001` §3.4 pacer protocols run at 4–7 breaths/min for minutes at a time). No force
  range, and no pressure-necrosis consideration, exists (**`OI-VNSCLIP-03`**).
- **PPG optical output.** 808–830 nm into the ear at an unstated radiant power. The programme's
  optical ceilings are stated for the scalp, for deep PBM and for the retina; **a PPG emitter in
  the ear falls under none of them**, the same class of gap `NP-HW-NASAL-001` §5.2 records for the
  nasal mucosa (**`OI-VNSCLIP-05`**).

---

## 6. The hydrogel pad consumable

`docs/reference/commercial-model.md` §2.3: *VNS clip pads (2-pack) · $8/pack · 20–40 sessions · 65 %
· Electrochemical degradation from VNS current.*

Against `CLAUDE.md` §2.3 (Rev 48), this row is the **best-formed consumable in the table** and is
the invariant's own worked example of an **exposure count**: the count is sessions, the mechanism is
named (electrochemical degradation from VNS current), and what the count cannot see is stated in
the rule itself — a pad that failed early, was damaged, or degraded off the device.

**This document changes nothing about that row.** It adds the two hardware consequences the row does
not carry:

1. **The threshold's provenance is unstated.** *20–40 sessions* is a range, not a measured life, and
   `CLAUDE.md` §2.3 (Rev 48) holds that trigger *kind* and threshold *provenance* are two separate
   claims. Satisfying the first does not satisfy the second. Closing the second needs a pad whose
   electrochemical degradation has been characterised — which needs a pad specification
   (**`OI-VNSCLIP-07`**).
2. **The count covers one of the pad's two duties** — §4.3.

---

## 7. Hazard-analysis inputs

`OI-RISK2-02`'s block is lifted for A13 by this document. Note also that `NP-ART-001` §2.4 marks
A13's risk-register column **✅ `NP-RISK-002` §4** — but the §4 row it points at is the one reading
*"Intranasal probe, audio cup and auricular clip hazards are **not assessed at all**."* That is a
**named absence, not a register entry**, and the register's own tick is therefore misleading; it is
corrected in `NP-ART-001` Rev 3.

| Candidate hazard | Why it is on the list |
|---|---|
| Charge-density overexposure at the true pad area | The interlock divides by an unmeasured constant (§5.2) |
| Pressure necrosis / discomfort on auricular cartilage | No clamping force specified (§5.3) |
| Thermal or optical exposure from the PPG emitter in the ear | No ceiling covers this site (§5.3) |
| Degraded pad → corrupted A1/A2 reference → mis-scored qEEG | §4.3; the failure is silent, because the stimulation-contact window still passes |
| Pad detachment or partial contact mid-session | Contact is confirmed at enable; partial detachment during an RSA-gated session concentrates current on a smaller area |
| Skin irritation / contact dermatitis | PDMS hydrogel against skin for repeated sessions; no biocompatibility basis of record |
| Clip applied to the wrong ear structure | The target is a specific nerve branch; no placement verification exists beyond impedance |
| Cable strain at the clip and at the hub | 6 conductors, no spare, no strain-relief specification |

---

## 8. What this document does **not** specify, and why

| Not specified | Why | Where it goes |
|---|---|---|
| Clip body geometry, clamping force, ear-size range | No geometry exists for A13 anywhere | **`OI-VNSCLIP-03`** |
| Electrode contact geometry and **measured** area | §5.2 — the area exists only as a provisional firmware constant | **`OI-VNSCLIP-02`** (closes `OI-CHARGE-07`'s VNS limb) |
| Pad formulation, adhesion, shelf life, and characterised electrochemical life | §6 | **`OI-VNSCLIP-07`** |
| PPG emitter/detector selection and optical output ceiling | §5.3 | **`OI-VNSCLIP-05`** |
| Auricular pulse-width range | §4.1 — bounded today by a `uint8_t` and nothing else | **`OI-VNSCLIP-04`** |
| Whether A1/A2 stays on the stimulation pads, and its acceptance criterion if it does | §4.3 | **`OI-VNSCLIP-01`** |
| Connector at either end of the 6-conductor cable, and strain relief | Shared accessory-connector gap | **`OI-VNSCLIP-06`** |
| Biocompatibility basis for every skin-contacting surface | Repeated prolonged skin contact, no material of record | **`OI-VNSCLIP-07`** |

### 8.1 FAI readiness — the honest verdict

| | Verdict |
|---|---|
| **F1** — governing spec `BASELINED`/`ACTIVE` | **FAILS** — this document is DRAFT |
| **F2** — dimensions dimensioned or gated | **FAILS** — no dimension exists; the one numeric property with a value (pad area) is explicitly provisional |
| **F3** — supplier category exists | **NOT ASSESSABLE** |
| **F4** — failure modes in a risk register | **FAILS**, now unblocked — §7 is the input list, and the register's ✅ for A13 is corrected (§7) |

> **`NP-FAI-VNSCLIP-001` still cannot be written**, and `NP-ART-001` §3.2 keeps it named.

---

## 9. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-VNSCLIP-01** | **A consumable sits in the T2 EEG reference path (§4.3).** The A1/A2 linked-ear normative reference is on the same pads the VNS current electrochemically degrades, only the stimulation duty has a trigger, and the `[500 Ω, 5000 Ω]` contact window cannot detect a pad that is a poor µV-scale reference. Decide whether the reference stays on these pads; if it does, give the pad an EEG-grade criterion and extend the consumable trigger to cover both duties | Clinical + EE | T2 qEEG measurement quality; consumable trigger |
| **OI-VNSCLIP-02** | **Specify and measure the clip pad's wetted contact area**, with a tolerance, so `NP_VNS_ELECTRODE_AREA_MCM2` stops being PROVISIONAL. This is the hardware half of `OI-CHARGE-07` for this channel, and the Class C per-phase interlock divides by it. Note the test harness permits revision **down** but not **up** without a measurement — and up is the plausible direction | ME + Clinical | `OI-CHARGE-07`; Class C safety argument |
| **OI-VNSCLIP-03** | Specify clip geometry, clamping force range and the ear-size range served. Worn on cartilage for whole sessions with no force of record | ME + HFE | Hazard analysis; A13 tooling |
| **OI-VNSCLIP-04** | **Specify the auricular pulse-width range.** None exists; the wire format bounds it at 255 µs by field width, and the 200–1000 µs figure in the record belongs to cervical VNS, a different artifact. Confirm or change the bound deliberately | Clinical + FW | Protocol authoring; charge-per-phase computation |
| **OI-VNSCLIP-05** | Select the PPG emitter and detector and state an optical output ceiling for an 808–830 nm source at the ear. The programme's optical ceilings cover scalp, deep PBM and retina, and this site falls under none of them | EE + Clinical | Hazard analysis |
| **OI-VNSCLIP-06** | Select connector and strain relief for the 6-conductor clip cable. Worth deciding **once** across A11, A12, A13 and A14 | EE + ME | A13 tooling |
| **OI-VNSCLIP-07** | Specify the PDMS hydrogel pad — formulation, adhesion, shelf life, biocompatibility basis — and characterise its electrochemical life, which is what turns *20–40 sessions* from a stated range into a threshold with provenance (`CLAUDE.md` §2.3 Rev 48) | ME + Quality | Consumable threshold provenance; hazard analysis |

---

## 10. Cross-references

- **Artifact register row:** `docs/np_art_001.md` §2.4 (A13), §3.2
- **Firmware specification (the other half of A13's record):** `docs/np_fw_hrv_001.md`; `docs/np_fw_hub_001.md` §8.4
- **Charge ceiling derivation:** `docs/np_dt_001.md` §3.2.1 DI-SAFE-01a; `docs/np_sw_001.md` §5.1
- **The unmeasured-area item this document must close:** `OI-CHARGE-07`, `docs/status/pending-decisions.md`
- **FAI issue conditions:** `docs/np_fai_001.md` §2
- **Risk gap this unblocks:** `docs/np_risk_002.md` §4, OI-RISK2-02
- **Consumable row:** `docs/reference/commercial-model.md` §2.3

---

## 11. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-09-20 | NeurOne Systems Engineering | Initial release, against GitHub #332 / `NP-ART-001` OI-ART-04. **Gives artifact A13's hardware an owning specification for the first time**; `NP-FW-HRV-001` remains the firmware specification and stops being the artifact's only document. §2 decomposes the clip into six sub-parts and records that **one mechanical part performs three functions with three different failure consequences** (`REQ-VNSC-01`). **Two findings.** (i) **The Class C per-phase charge interlock divides by an electrode area that has never been measured** — `NP_VNS_ELECTRODE_AREA_MCM2` = 0.5 cm², PROVISIONAL, and legitimately a firmware constant *because* it is a fixed product part, which is precisely the thing a hardware specification should state (`OI-VNSCLIP-02`, closing `OI-CHARGE-07`'s VNS limb). Margin at 3 % of the ceiling is recorded as **not** a reason to defer it. (ii) **A consumable sits in the T2 EEG reference path** — the A1/A2 linked-ear normative reference is on the same pads the VNS current degrades by design, only the stimulation duty has a replacement trigger, and the stimulation-contact window cannot see a pad that is a poor µV-scale reference (`OI-VNSCLIP-01`). Also recorded: the 6-conductor cable now has **zero spare conductors** (`REQ-VNSC-12`), the auricular pulse width is bounded by a `uint8_t` and by no specification (`OI-VNSCLIP-04`), and no optical ceiling covers an 808–830 nm PPG emitter at the ear (`OI-VNSCLIP-05`). §7 supplies the hazard-analysis input list and notes that `NP-ART-001` §2.4's ✅ for A13's risk register points at a row stating the hazards are **not assessed** — corrected in that register's Rev 3. §8.1: **`NP-FAI-VNSCLIP-001` still cannot be written**. **No engineering value is set, no figure is invented, no constant's value changes, no price, interval, margin or locked decision changes.** Raises OI-VNSCLIP-01…07. |
