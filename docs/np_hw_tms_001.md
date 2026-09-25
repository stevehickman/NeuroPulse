# TMS Focal Figure-8 Coil Applicator (T2) — Hardware Specification

**Project:** NeurOne
**Document:** NP-HW-TMS-001
**Revision:** 1
**Date:** 2026-09-20
**Status:** DRAFT — **requirements-grade, and gated. Two BLOCKING items (`OI-PWRTH-01`, `OI-PWR-02`) sit upstream of every mechanical value this document would otherwise set; §5 says why writing them now would be writing them twice.**
**Effective Date:** —
**Author:** NeurOne Systems Engineering
**Approved By:** — (new document)
**References:** CLAUDE.md §3 T2 additions (focal figure-8 TMS), §4.2 (TMS interlock row), §4.3 (non-conductive CFRP window at the coil site; active Helmholtz cancellation), §4.4 (fit system), §4.5 (power); NP-PWR-THERM-001 Rev 1 §7 (field vs threshold), **§8 (`SPEC-TMS-01…05`)**, §8.3 (topology), §8.4 (the thermal wall), §8.5 (cancellation duty), §11, §13, §15 (`RISK-PWRTH-01`, `RISK-PWRTH-03`), §16 (`OI-PWRTH-01…09`); NP-PWR-BUDGET-001 §4.1, §4.2 (`OI-PWR-02`, `OI-PWR-03`); NP-THERM-SINK-001 (`SPEC-SINK-01`, `SPEC-SINK-04`, `OI-SINK-01`); NP-ART-001 Rev 3 §2.4 A15, §3.2, OI-ART-04; NP-FAI-001 Rev 3 §2 (F1–F4); NP-RISK-002 Rev 3; NP-CONV-001 Rev 6 §4, §5, §6; `docs/neuromod_neuro_protocols.md` (rTMS and iTBS protocol shapes); `firmware/hub_control/include/np_hub_config.h` (`NP_SAFETY_EN_TMS`)
**Related Issues:** GitHub Issue #332 (OI-ART-04 — A15 has no owning document); GitHub Issue #336 (the power/thermal group that produced `SPEC-TMS`); GitHub Issue #73 (T2 prototype)
**Gate:** **`OI-PWR-02` (BLOCKING) and `OI-PWRTH-01` (BLOCKING).** This document is A15's entry in the artifact register and a requirements frame; it is **not** a release, and §5.1 states the sequencing.
**IEC 62304 Class:** N/A (hardware specification). The coil sits behind the Class C enable `NP_SAFETY_EN_TMS` and inside the Class C cancellation-gating interlock (`CLAUDE.md` §4.2).
**Supersedes:** None — first issue. It ends the state `NP-ART-001` §2.4 records as *"concept only"*, and gives `SPEC-TMS-01…05` an owning artifact document rather than leaving an electrical specification lodged in a power-and-thermal study.
**Parent Document:** None

---

> **Why this document exists.** A15's governing specification in `NP-ART-001` §2.4 was *"CLAUDE.md
> §3 T2 additions"* plus, since 2026-09-15, `SPEC-TMS-01…05` in **`NP-PWR-THERM-001` §8** — a
> coil's electrical specification living inside a power-and-thermal study, *"but **not** a
> mechanical envelope, a thermal path or a released owning document."* `OI-ART-04` asked for the
> owning document, and asked for it to be written **against** `OI-PWRTH-01` and `OI-PWRTH-09`.
> This is it.
>
> **The honest position this document takes.** A15 is the one artifact in the register where
> writing the missing values now would be **actively wrong**, not merely premature. `OI-PWR-02`
> (BLOCKING) asks whether a 25 mm-wing coil at 0.5 T delivers a therapeutic field at all, and
> `NP-PWR-THERM-001` §7 puts it at **17 %** of a clinical figure-8's maximum cortical field and
> **4.1× short** of 120 % RMT; closing it by field alone needs **2.06 T → 613 J → 2,886 W**. The two
> answers are **17× apart in supply**, and a coil sized for either is a different part. §5.1 makes
> that sequencing explicit instead of leaving it as a reason nobody wrote down.
>
> **What the document does do**: it states the requirements that hold under *either* answer, it
> records `SPEC-TMS`'s status precisely (design-study grade, on a geometry the study chose and ME
> did not), and §6 records **three requirement classes that are absent from the record entirely** —
> an acoustic requirement, a patient-facing interlock, and a high-voltage energy-store safety case.

---

## 1. Scope

| § | What it pins |
|---|---|
| §2 | The artifact and its sub-parts, including the parts whose *location* is undecided |
| §3 | `SPEC-TMS-01…05` as carried requirements, with their status stated |
| §4 | Interfaces: the Class C enable, the cancellation gate, and the `CLAUDE.md` §4.3 window |
| §5 | The two BLOCKING gates, and why mechanical values wait on them |
| §6 | Three absent requirement classes: acoustic, patient-facing interlock, energy store |
| §7 | Hazard inputs |

**Out of scope:** the field-versus-threshold question itself (`OI-PWR-02`, `NP-PWR-BUDGET-001`
§4.2 and `NP-PWR-THERM-001` §7 own it); the coil's thermal solution (`OI-PWRTH-01`); the hub's
supply architecture (`NP-PWR-THERM-001` §11, PWRTH-D-7); and TMS protocol content
(`docs/neuromod_neuro_protocols.md`).

---

## 2. The artifact

**A15 is a head-applied pulsed-magnetic applicator, and how much of the system it contains is
undecided** — `OI-PWRTH-07` asks whether the applicator is passive or active, i.e. whether the
28 µF / 1,600 V bank and its driver sit in the applicator or off it. That single open item changes
A15's mass, its thermal problem, its cable and its hazard set, so the table below marks location
rather than assuming it.

| Sub-part | What `SPEC-TMS` or the record gives | Location | Status |
|---|---|---|---|
| **A15.1 — coil winding** | Figure-8, **2 wings in series**, wing mean radius **25 mm**, **10 turns/wing**, 6 × 1.5 mm (9 mm²) copper strip; L = 18.2 µH, R_ac = 11.7 mΩ, Q = 69 | In the applicator | **`SPEC-TMS-01`**, design-study grade, geometry chosen by the study (`OI-PWRTH-09`) |
| **A15.2 — coil housing, potting and applicator body** | Nothing. The **200 g / ≈100 J/K** figure in §8.4's adiabatic budget is an *assumption of that calculation*, explicitly first-order | In the applicator | **Unspecified** |
| **A15.3 — thermal path** | **None exists.** 33 W of winding dissipation, a 42 °C applied-part limit, and a non-conductive window beneath that forecloses a metallic spreader | In the applicator | **`OI-PWRTH-01`, BLOCKING** |
| **A15.4 — energy store** | **28 µF at 1,600 V**, biphasic with bank recovery | **Undecided** — `OI-PWRTH-07` | **`SPEC-TMS-02`** |
| **A15.5 — pulse driver and charger** | 169 W average recharge at 10 Hz biphasic-recovered; 176 W with device overhead | **Undecided** — `OI-PWRTH-07`; `NP-PWR-THERM-001` §11 sites the inlet *"on the TMS driver where the bank and the 33 W already are"* | **`SPEC-TMS-04`** |
| **A15.6 — positioning and retention** | Nothing. The coil must be held at a scalp site over the `CLAUDE.md` §4.3 **non-conductive CFRP window** | On/against the headset | **Unspecified** |
| **A15.7 — cable / interconnect** | Nothing. Carries either kilovolt pulse energy or low-voltage supply, depending on `OI-PWRTH-07` | — | **Unspecified** |

> **`REQ-TMS-01`** — A15's mass, thermal problem and cable class are **determined by
> `OI-PWRTH-07`**, not by mechanical preference. A mechanical envelope drawn before it is answered
> is drawn for one of two architectures with a 50 % chance of being the wrong one.

---

## 3. `SPEC-TMS` carried as requirements

`SPEC-TMS-01…05` is the coil's electrical specification and this document **adopts it unchanged**.
It is reproduced rather than summarised because a requirement a reader has to fetch is a
requirement that gets approximated.

| ID | Requirement | Source |
|---|---|---|
| **REQ-TMS-02** | Coil: figure-8, 2 wings in series, wing mean radius 25 mm, 10 turns/wing, 9 mm² copper strip; **L = 18.2 µH**, Q = 69, 0.251 mT per ampere of coil current (10 turns) at the wing centre *(corrected 2026-09-25, GitHub #394: this read "0.251 mT per amp-turn". μ₀/2a at a = 25 mm is 0.0251 mT per amp-turn, so 0.251 mT is per **ampere** of coil current over 10 turns — the reading `I_peak` = 1,989 A for 0.5 T depends on. No figure changes)* | `SPEC-TMS-01` |
| **REQ-TMS-03** | Energy store: **28 µF at 1,600 V**, **biphasic with bank recovery** | `SPEC-TMS-02` |
| **REQ-TMS-04** | Pulse: series-LC ring **T = 142 µs** biphasic (~71 µs monophasic); **I_peak = 1,989 A**; dB/dt = 22.1 kT/s at the design point | `SPEC-TMS-03` |
| **REQ-TMS-05** | Supply: **169 W** average recharge at 10 Hz / 0.5 T / biphasic-recovered; **176 W** with device overhead; 423 W monophasic | `SPEC-TMS-04` |
| **REQ-TMS-06** | Design point **0.5 T** — the **top** of `CLAUDE.md` §3's 0.1–0.5 T range. *"0.1–0.5 T is an adjustment range whose bottom is not an operating point"* | `SPEC-TMS-05` |
| **REQ-TMS-07** | Biphasic-with-recovery is **specified, not preferred**: the monophasic figure is 430 W of device draw and **no USB-C PD class delivers it** (240 W EPR is the top of the standard) | `NP-PWR-THERM-001` §8.3 |
| **REQ-TMS-08** | The coil site sits over a **non-conductive CFRP window** in the shell — which is what forecloses the obvious metallic heat spreader | `CLAUDE.md` §4.3; `NP-PWR-THERM-001` §8.4 |

### 3.1 The status of these figures, stated rather than assumed

`NP-PWR-THERM-001` is explicit about what `SPEC-TMS` is: **design-study grade**, validated by
`--validate` against `NP-PWR-BUDGET-001` §4.1's independent E ∝ B² estimate at both ends of the
field range, from inputs it shares none of — and **built on a coil geometry that document chose,
not one ME specified** (`RISK-PWRTH-03`, `OI-PWRTH-09`).

> **`REQ-TMS-09`** — Wing radius is the **single largest lever** in the specification: it sets depth
> falloff, L, the ring period and the energy. A coil envelope returned by ME re-runs every figure
> in §3 through one script. **No figure in §3 may be treated as fixed until `OI-PWRTH-09` returns
> an envelope**, and this document's adoption of them is adoption of the *current best derivation*,
> not of a released value.

---

## 4. Interfaces

| Parameter | Value | Source |
|---|---|---|
| Safety enable | `NP_SAFETY_EN_TMS` = bit 11 — *"gates TMS coil; EMF cancellation gated off per `NP-FW-HUB-001` §4.2"* | `np_hub_config.h` |
| Cancellation gate | Helmholtz cancellation off **5 ms before** each pulse, **50 ms hold** after | `CLAUDE.md` §4.2 |
| Shell provision | Non-conductive CFRP window at the coil site | `CLAUDE.md` §4.3; artifact **A6** |
| Supply | 176 W sustained during a train, within a single 240 W EPR contract | `SPEC-TMS-04`; `NP-PWR-THERM-001` §11 |

### 4.1 The cancellation gate is on for most of a train, and that is a claim problem the coil inherits

`NP-PWR-THERM-001` §8.5 derives it from `CLAUDE.md` §4.2 alone: 5 ms + 50 ms is **55 ms per pulse**,
so at 10 Hz active cancellation is **off for 55 % of the train**, above **18.2 Hz** it is off
**continuously**, and iTBS's 50 Hz intra-burst rate means it is off for the whole of every burst.

Nothing about that is a safety finding — the interlock gates cancellation off *because* the
Helmholtz coils must not fight the pulse. It is a **claim** finding, already raised as
`OI-PWRTH-05` and routed to `docs/reference/competitive-position.md` and
`docs/reference/regulatory-strategy.md`. It is restated here because **A15 is the artifact whose
presence causes it**, and an artifact document that omitted it would let the next reader rediscover
it from scratch.

---

## 5. The two gates, and why this document sets no mechanical value

### 5.1 Sequencing, stated

| Gate | Question | Why A15's mechanical design waits on it |
|---|---|---|
| **`OI-PWR-02`** — BLOCKING | Does this coil deliver a therapeutic field? `NP-PWR-THERM-001` §7: **17 %** of a clinical figure-8's maximum cortical field, **4.1× short** of 120 % RMT, the shortfall being depth falloff of a 25 mm wing. Closing it by field alone needs **2.06 T → 613 J → 2,886 W** | The two answers are **17× apart in supply**. A 176 W applicator and a 2,886 W applicator are not the same part, do not share a thermal solution, a cable, a mass or a retention scheme, and only one of them is a head-worn accessory at all |
| **`OI-PWRTH-01`** — BLOCKING | Where does 33 W go? Adiabatic headroom to the 42 °C applied-part limit is **1,700 J**, giving **516 admissible pulses**; a standard 3,000-pulse rTMS protocol needs **9,888 J** (**5.8×**) and even iTBS's 600 pulses needs 1,978 J (**1.2×**). No cooling path is specified anywhere, and `CLAUDE.md` §4.3's non-conductive window forecloses a metallic spreader | The resolution — active cooling, a duty-cycle-aware thermal model, or a firmware pulse-count/train-gap governor — **sets the housing**. §8.4 is explicit that *"no plausible refinement of the thermal mass closes that"*, so this is not a tolerance question |

> **`REQ-TMS-10`** — **No mechanical envelope, mass, retention scheme or thermal solution is
> specified in this revision, and that is a decision.** `NP-FAI-001` §2's principle — *"a criterion
> with no source is not a conservative placeholder"* — applies with unusual force here, because a
> placeholder envelope would be consumed immediately by `OI-PWRTH-09`, which is the item asking ME
> for exactly that envelope.

### 5.2 What can be said about mass without pre-empting either gate

`NP-PWR-THERM-001` §8.4's **200 g** is an input to an adiabatic calculation, described there as
first-order, and it is the only mass figure attached to A15 anywhere.

> **`REQ-TMS-11`** — 200 g is **not a mass budget**. Whatever mass ME returns loads the
> `CLAUDE.md` §4.4 fit system (1 adult SKU, 52–62 cm, Boa occipital dial, spring-decoupled pods)
> at a site the fit system was not characterised against, and a change in coil mass **changes the
> §8.4 thermal bound in the same direction** — a lighter coil has less thermal mass and fewer
> admissible pulses. Mass is therefore a **coupled** variable between ME and Thermal, not an ME
> output. **`OI-TMS-01`.**

---

## 6. Three requirement classes absent from the record

Each is a class of requirement that a TMS applicator normally carries and that **no NeurOne document
states**. They are recorded, not invented.

### 6.1 There is no acoustic requirement

A capacitor discharge of 36 J through a coil at 1,989 A produces a loud acoustic transient — the
coil click — and it is intrinsic to the physics, not a defect. **The document set contains no
acoustic output figure for the TMS pulse, no hearing-protection provision, and no exposure limit**,
and `NP-HW-AUDIO-001` §5 records that the programme has no acoustic ceiling for the audio modality
either, so there is no existing figure to inherit.

The interaction is specific to this product: **A11's audio cups sit over the ears**, so there is
already a head-worn part at the relevant location — which makes this a design opportunity and a
design coupling, not only a gap. **`OI-TMS-02`.**

### 6.2 `CLAUDE.md` §4.2's TMS interlock protects the coil, not the wearer

The §4.2 row reads, in full:

| Modality | Interlock | Implementation |
|---|---|---|
| TMS | **Coil protection** | EMF cancellation gated off 5 ms before pulse, 50 ms hold |

Every other stimulation row in that table names a **patient-facing** control — photoparoxysmal
halt, commanded-charge ceiling, MPE limit, 42 °C throttle, contact confirmation, cardiac cutoff.
**The TMS row names a control on the equipment.** There is a Class C enable (`NP_SAFETY_EN_TMS`),
so the modality can be cut off; what does not exist anywhere is a statement of **what conditions
must cut it off** — no dose or train limit, no inter-train interval floor, no cumulative-pulse
bound, no contraindication interlock.

> **`OI-TMS-03`** — Specify A15's patient-facing safety controls and their source, and add the row
> to `CLAUDE.md` §4.2. Published rTMS safety practice is the obvious starting point and naming which
> standard or guideline binds is a **Clinical + Regulatory** decision with 510(k) surface — it is
> not derivable from this document set, which is why nothing is proposed here. Note that the
> thermal bound in §5.1 already imposes a **pulse-count** limit (516 adiabatic) for an entirely
> different reason; the two must not be confused, and a governor built for the thermal reason must
> not be presented as the clinical control.

### 6.3 There is no energy-store safety case

`SPEC-TMS-02` specifies **28 µF at 1,600 V** — roughly 36 J stored, on a device a user puts on
their head. The record contains no bleed-down requirement, no interlock against servicing a charged
bank, no creepage/clearance statement, no single-fault analysis for the switch, and no insulation
coordination for the applicator cable if `OI-PWRTH-07` puts the bank off-applicator.

`CLAUDE.md` §4.5's power section and `NP-PWRSRC-001` are written for a USB-C PD device. A kilovolt
energy store is a different electrical-safety regime, and **IEC 60601-1's requirements for it have
not been examined anywhere in the set**. **`OI-TMS-04`.**

---

## 7. Hazard inputs

A15 has **no risk register entry of any kind** (`NP-ART-001` §2.4), and `NP-RISK-002` §4's two rows
cover A14 and A11–A13 — **not A15**. `NP-PWR-THERM-001` §15 does carry `RISK-PWRTH-01` (coil
reaches 42 °C mid-train, no cooling path, no verification defined) and `RISK-PWRTH-03` (the
geometry is the study's, not ME's), which are the only hazard rows the artifact has.

| Candidate hazard | Why it is on the list |
|---|---|
| Coil surface exceeds 42 °C against the scalp | `RISK-PWRTH-01`; **no verification defined**, `OI-PWRTH-01` BLOCKING |
| Seizure or other neurological adverse event from rTMS dosing | No patient-facing dose control exists in §4.2 (§6.2) |
| Acoustic injury from the coil click | No acoustic requirement exists (§6.1) |
| Electric shock from the 1,600 V energy store | No energy-store safety case exists (§6.3) |
| Mispositioning over the wrong cortical target | No positioning or retention scheme exists (A15.6) |
| Mass and centre-of-mass load on the fit system | Coupled to the thermal bound (`REQ-TMS-11`) |
| Eddy-current field loss or heating if the window is not the specified non-conductive CFRP | `REQ-TMS-08`; the window is artifact **A6**'s feature and A6's own FAI is blocked on `MECH-1`, `EMF-1`, `EMF-3`, `RISK-20` |
| Interaction with implanted or ferromagnetic material | Standard TMS contraindication territory; no screening requirement exists in the set |

---

## 8. What this document does **not** specify, and why

| Not specified | Why | Where it goes |
|---|---|---|
| Mechanical envelope, housing, potting, mass | §5.1 — both gates change it | **`OI-PWRTH-09`** (ME returns an envelope), **`OI-TMS-01`** (mass is coupled) |
| Thermal path | The BLOCKING item | **`OI-PWRTH-01`** |
| Whether the bank and driver are in the applicator | Decides mass, cable class and hazard set | **`OI-PWRTH-07`** |
| Whether this coil is therapeutically adequate at all | The other BLOCKING item; 17× in supply rides on it | **`OI-PWR-02`** |
| Positioning, retention and target registration | No scheme exists; depends on the envelope | **`OI-TMS-05`** |
| Acoustic output and hearing protection | §6.1 | **`OI-TMS-02`** |
| Patient-facing dose and train limits | §6.2 | **`OI-TMS-03`** |
| High-voltage energy-store safety case | §6.3 | **`OI-TMS-04`** |
| Cable and connector | Class depends on `OI-PWRTH-07`; not the accessory-port connector the other artifacts share | **`OI-TMS-06`** |

### 8.1 FAI readiness — the honest verdict

| | Verdict |
|---|---|
| **F1** | **FAILS** — DRAFT, and gated behind two BLOCKING items |
| **F2** | **FAILS** — no mechanical dimension exists; §3's electrical figures are design-study grade and explicitly re-derivable (`REQ-TMS-09`) |
| **F3** | **NOT ASSESSABLE** — no process named |
| **F4** | **FAILS** — A15 has no risk register; `RISK-PWRTH-01` and `-03` live in a thermal study, not in a per-artifact register |

> **`NP-FAI-TMS-001` still cannot be written**, and `NP-ART-001` §3.2 keeps it named. Of the five
> artifacts this issue covers, A15 is the one furthest from F1, and **that distance is a programme
> fact rather than a documentation one**: `OI-PWR-02` asks whether the part should exist in this
> form.

---

## 9. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-TMS-01** | **Coil mass is a coupled variable, not an ME output.** It loads the `CLAUDE.md` §4.4 fit system at an uncharacterised site, and reducing it **reduces** the §8.4 adiabatic pulse budget. Set it jointly with `OI-PWRTH-01` and `OI-PWRTH-09`; treat `NP-PWR-THERM-001`'s 200 g as a calculation input, never a budget | ME + Thermal | A15 envelope |
| **OI-TMS-02** | **No acoustic requirement exists for the TMS pulse** — no output figure, no exposure limit, no hearing-protection provision — and there is no acoustic ceiling elsewhere in the set to inherit (`NP-HW-AUDIO-001` `OI-AUDIOHW-01`). A11's audio cups already sit over the ears, so this couples to that artifact | Systems + Clinical | Hazard analysis; T2 release |
| **OI-TMS-03** | **`CLAUDE.md` §4.2's TMS row is titled *Coil protection* and names no patient-facing control.** No dose limit, train limit, inter-train interval or contraindication interlock exists anywhere. Name the binding clinical guidance, specify the controls, and add the row. Do not reuse the §8.4 thermal pulse bound as the clinical control | Clinical + Regulatory + Safety | **T2 release; 510(k)** |
| **OI-TMS-04** | **No safety case exists for a 28 µF / 1,600 V energy store on a head-worn device** — no bleed-down, no service interlock, no creepage/clearance, no single-fault analysis, no insulation coordination for the applicator cable. `CLAUDE.md` §4.5 and `NP-PWRSRC-001` are written for USB-C PD and do not cover this regime | EE + Regulatory | **T2 release; IEC 60601-1** |
| **OI-TMS-05** | Specify positioning, retention and target registration against the `CLAUDE.md` §4.3 non-conductive CFRP window. Depends on the envelope (`OI-PWRTH-09`) | ME + Clinical | A15 tooling |
| **OI-TMS-06** | Select the applicator interconnect once `OI-PWRTH-07` fixes its class — kilovolt pulse energy or low-voltage supply. **Not** the shared accessory-port connector the other four artifacts need | EE | A15 tooling |

> **Items owned elsewhere that this document is written against**, and does not duplicate:
> `OI-PWR-02` (BLOCKING, field adequacy), `OI-PWRTH-01` (BLOCKING, thermal path), `OI-PWRTH-05`
> (the cancellation-duty claim), `OI-PWRTH-07` (passive or active applicator), `OI-PWRTH-09`
> (ME returns a coil envelope). All five live in `NP-PWR-THERM-001` §16 / `NP-PWR-BUDGET-001` and
> stay there.

---

## 10. Cross-references

- **Artifact register row:** `docs/np_art_001.md` §2.4 (A15), §3.2
- **The electrical specification this document adopts:** `docs/np_pwr_therm_001.md` §8
- **Field adequacy:** `docs/np_pwr_therm_001.md` §7; `docs/np_pwr_budget_001.md` §4.2
- **The shell window:** `CLAUDE.md` §4.3; artifact A6, `docs/np_art_001.md` §2.1
- **FAI issue conditions:** `docs/np_fai_001.md` §2
- **Protocol shapes referenced by the thermal bound:** `docs/neuromod_neuro_protocols.md`

---

## 11. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-09-20 | NeurOne Systems Engineering | Initial release, against GitHub #332 / `NP-ART-001` OI-ART-04, written — as that item requires — **against `OI-PWRTH-01` and `OI-PWRTH-09`**. **Gives artifact A15 an owning document for the first time** and re-seats `SPEC-TMS-01…05` as adopted requirements (§3) rather than leaving a coil's electrical specification lodged inside a power-and-thermal study, with their **design-study status and re-derivability stated** (`REQ-TMS-09`). **§5.1 states the sequencing explicitly and sets no mechanical value**: `OI-PWR-02` (BLOCKING) leaves two candidate supply answers **17× apart**, and `OI-PWRTH-01` (BLOCKING) leaves 33 W with no path and **516 admissible pulses against a 3,000-pulse protocol** — a placeholder envelope would be consumed immediately by the item that asks ME for the real one. **Three absent requirement classes are recorded (§6):** no acoustic requirement or hearing-protection provision for the coil click, and none to inherit (`OI-TMS-02`); **`CLAUDE.md` §4.2's TMS row is titled *Coil protection* and names no patient-facing control** — no dose, train, interval or contraindication limit exists anywhere in the set (`OI-TMS-03`); and **no safety case exists for a 28 µF / 1,600 V energy store on a head-worn device** (`OI-TMS-04`). §5.2 records that coil mass is **coupled** — lighter means fewer admissible pulses — so it is not an ME output (`OI-TMS-01`). §7 notes A15 has no risk register entry of any kind, and that `RISK-PWRTH-01`/`-03` sit in `NP-PWR-THERM-001` §15, a thermal study, rather than a per-artifact register. §8.1: **`NP-FAI-TMS-001` still cannot be written**, and A15 is the furthest of the five from F1 for a programme reason, not a documentation one. **No engineering value is set, no figure in `SPEC-TMS` is changed, no locked decision moves.** Raises OI-TMS-01…06. |
