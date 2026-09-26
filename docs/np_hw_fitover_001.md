# Fit-Over Goggle Assembly — Concept Specification (Deferred)

**Project:** NeurOne
**Document:** NP-HW-FITOVER-001
**Revision:** 2
**Date:** 2026-09-26
**Status:** DRAFT — **DEFERRED CONCEPT. Not on the development plan, not funded, not scheduled.** Specifies an assembly the programme has deliberately chosen not to build. No tooling, firmware, BOM or schedule commitment follows from this document.
**Effective Date:** —
**Author:** NeurOne Systems Engineering
**Approved By:** — (concept record; no approval sought)
**References:** CLAUDE.md §3 (modality ⑧ visual stimulation), §4.2 (the visual/retinal interlock — IR proximity + Hall sensor + hardware current limit, three independent layers), §4.3 (EMF shielding), §4.4 (fit system, 1 adult SKU 52–62 cm); `docs/reference/modality-stack.md` (lens stack, shade system, EC lens, Mode F); NP-TOOL-LENS-001 Rev 2 (sliding rail, N42 magnet positions, single tooling family, goggle-arm tether hook MR-19/MR-20); NP-HELMET-GEOM-001 (§2 radial stack); NP-HFE-001 CT-03 (safety is not shade-dependent); NP-FW-BENCH-001 (§ocular gate, `OI-VIS-03`/`-04`, `OI-BENCH-08`); `firmware/hub_control/modules/np_mod_visual.c` (IR proximity eye-open, Hall lifted); NP-ACC-PRIORITY-001 Rev 2 (§3 the ranking criterion, §5 rank 19, §8 `OI-ACC-01`); `docs/reference/service-network.md` (Tier A optician network); `docs/reference/commercial-model.md` §2.3 (S3 Rx insert); IEC 62471 (photobiological safety of lamps and lamp systems)
**Related Issues:** GitHub Issue #344 (the priority set that placed it), #25 (S3 Rx programme — the committed path this defers to)
**Gate:** **DEMAND GATE (EXTERNAL).** Nothing in this document starts without a specific, recorded user request signal — see §2.2. Not tooling-blocking; provisions nothing at first cut and forecloses nothing by waiting (§6.3).
**IEC 62304 Class:** — (concept document; no code exists or is proposed). §5 records that a built assembly **would** carry Class C consequences, which is a finding, not a classification.
**Supersedes:** None — new document.
**Parent Document:** None. Sibling of NP-TOOL-LENS-001 Rev 2, which tools the committed assembly.

---

## 1. The decision this document records

**Principal decision, 2026-09-14.** Users who need corrective lenses while wearing the goggles
**buy prescription lenses for the goggle assembly**. That is the S3 prescription clip and Rx insert
already specified in `docs/reference/modality-stack.md` and already supported by the Tier A optician
partner network (`docs/reference/service-network.md`, recorded as *already engaged via the S3
programme*).

**There is no intent at this time to build a goggle assembly large enough to be worn over the user's
own glasses.** This is a positive design decision, not an omission:

- **The committed path is optical, not dimensional.** Correction is delivered by putting the right
  optic *in* the assembly, at the lens plane the whole visual modality is characterised against —
  not by enlarging the cavity so a second, uncharacterised optic can sit behind it.
- **It keeps one goggle assembly.** `NP-TOOL-LENS-001` Rev 2's single-tooling-family property —
  common rail geometry across S1, S2 and S3 shade bodies — survives only while there is one lens
  rim to share.
- **It keeps one visual safety case.** §5 is why this is the load-bearing reason rather than the
  cost reason.

This document exists so that the alternative is **specified and deferred** rather than
**unconsidered**. If demand appears (§2.2), the work starts from §3–§5 rather than from zero, and
the reasons it was deferred are on the record rather than re-derived.

> **`OI-ACC-01` is closed by this decision.** It asked whether the goggle assembly can be worn over
> corrective eyewear. **It cannot, and by design.** The consequence for the accessory ranking is in
> §6.1: the S3 Rx system is confirmed as access-enabling rather than convenience, which is what
> `NP-ACC-PRIORITY-001` had been holding on a flagged judgement.

---

## 2. Scope and status

### 2.1 What this document is

A **concept specification**: the requirements a fit-over assembly would have to meet, and — more
usefully — an inventory of what it would cost in constraints that are currently locked. It is
written to be read once, by whoever is asked to cost this if it is ever raised again.

It is **not** a design, a tooling input, a BOM, or a roadmap item with a date. `NP-ART-001` should
not acquire an artifact row for it, and no FAI, risk register or tooling specification is owed
against it while it stays deferred.

### 2.2 The demand gate

**Nothing here proceeds without specific user requests for it.** That is the principal's condition
and it is the whole gate.

It is classified **EXTERNAL** under `NP-ACC-PRIORITY-001` §3.3 — the result is outside the
project's control — which is the same class as a trial readout, and it is recorded rather than
ranked for the same reason: a demand signal arriving sooner would not make the assembly deliver more.

**Where the signal would come from.** The document set already has instruments for exactly this and
this document proposes no new ones:

- the research suggestion portal's **"would participate" intent flags** and pledge mechanism
  (CLAUDE.md §6.3) — intent, never a charge;
- the **at-cost upgrade as intent signal** pattern that CLAUDE.md §2.2 already uses for the 65 W
  charger;
- support and optician-partner contact volume from the S3 programme, which is the population that
  would raise it.

**What would count as a signal is deliberately not defined here.** A threshold written now, with no
fleet and no S3 programme running, would be a number invented to look decisive. Setting it is part
of the work in §7, not a precondition for filing this document.

---

## 3. Requirements, if it were ever built

Stated as requirements so they can be argued with. None is satisfied by anything that exists.

| ID | Requirement | Note |
|---|---|---|
| **REQ-FITOVER-01** | Accommodate a spectacle frame within the goggle cavity without contact loading on the frame, across the frame-size range the programme chooses to serve | The range is itself undecided and is not a detail — it sets the cavity depth, which sets everything in §4 |
| **REQ-FITOVER-02** | Preserve the visual modality's delivered irradiance at the cornea within the tolerance the IEC 62471 MPE argument is built on, at the new lens-to-eye distance | §4.1. Re-qualification, not re-use |
| **REQ-FITOVER-03** | Preserve all three independent layers of the visual/retinal interlock (CLAUDE.md §4.2) with a user-supplied optic present in the cavity | §5.1. **The hardest requirement in this table** |
| **REQ-FITOVER-04** | Preserve dual-path dose metering for Mode F retinal PBM, or withdraw Mode F on this assembly | §5.2. The document's recommendation is **withdraw** |
| **REQ-FITOVER-05** | Preserve the EMF envelope (CLAUDE.md §4.3) across the enlarged enclosure | §4.4 |
| **REQ-FITOVER-06** | Retain fit across the 1 adult SKU 52–62 cm range (CLAUDE.md §4.4) with the added mass and the moved centre of mass | §4.3 |
| **REQ-FITOVER-07** | Either join `NP-TOOL-LENS-001`'s single tooling family, or state explicitly what forking it costs | §4.5 |
| **REQ-FITOVER-08** | Deliver no therapeutic claim the committed assembly does not already carry | §6.2. A second assembly must not become a second evidence base |

---

## 4. What it would cost in locked constraints

The useful content of this document. Each row is a constraint that is **locked today** and that an
enlarged cavity reopens.

### 4.1 The IEC 62471 MPE argument is distance-dependent

The visual ceiling is *IEC 62471 MPE at 50 % of the exempt-group threshold*, enforced in part by a
**hardware current limit** (CLAUDE.md §3, §4.2). That argument is made at one source-to-eye geometry:
108 micro-LEDs per lens in 6 zones per eye, at the committed lens plane.

Moving the lens plane forward to clear a spectacle frame changes irradiance at the eye. The
direction is favourable — more distance, less irradiance — and that is precisely why it is not free:

- a **hardware** current limit sized for one geometry is no longer the same margin at another, so
  the limit is re-derived, not re-used;
- restoring delivered dose means driving harder, which moves back toward the ceiling the limit
  exists to hold;
- accepting the lower dose means the assembly delivers a different dose from the committed one,
  which is a claims problem (`REQ-FITOVER-08`), not an engineering one.

**None of these is a blocker. All three are re-qualification**, and the third is the one that
quietly costs the most.

### 4.2 The zone structure assumes a known eye position

6 zones per eye, EMDR left/right alternation and photic driving 0.5–100 Hz all address the eye
through a geometry the assembly fixes. A spectacle frame inside the cavity introduces a variable
the committed assembly does not have: **frame position and size differ per user and are not
measured.** Zone-to-retina correspondence becomes a distribution rather than a value.

### 4.3 Fit, mass and the bridge

CLAUDE.md §4.4: one adult SKU covers 52–62 cm using a Boa occipital dial, a 5-position bridge and
spring-decoupled pods. A deeper goggle cavity adds mass **and** moves it forward, increasing the
moment about the nose bridge — the one contact point the fit system does not tension. The 5-position
bridge is a locked adjustment range, not an adjustable one.

### 4.4 EMF envelope

The four-layer stack (CLAUDE.md §4.3; five-layer until the Layer 4 absorber was deleted 2026-09-23) is an enclosure property. An enlarged enclosure is a different
enclosure; the 35–45 dB ELF / 40–60 dB RF figure that every §1 claim rests on is measured, and would
be re-measured.

### 4.5 The single tooling family

`NP-TOOL-LENS-001` Rev 2 records a deliberate win: *common rail geometry on all three shade bodies
(S1 opaque, S2 polarising, S3 prescription carrier)*, one tooling family, with the S3 Rx insert
snapping into the shade carrier rather than the rail. It also carries the N42 magnet pocket wall
thickness (≥1 mm, marked BLOCKING and an FAI dimension) and the goggle-arm tether hook (MR-19/MR-20).

A fit-over assembly either constrains its own depth to stay inside that family — which may defeat
its purpose — or forks it, and the fork is a second mould family plus a second FAI programme
(`NP-FAI-LENS-001` today covers one). **`REQ-FITOVER-07` exists to stop that fork happening
implicitly.**

---

## 5. The safety findings — why this is not a mechanical scale-up

Two findings, and they are the reason the deferral is a design decision rather than a scheduling one.

### 5.1 A spectacle lens sits *inside* the interlock path; a shade does not

CLAUDE.md §4.2 gives the visual/retinal interlock three independent layers: **IR proximity
(940 nm) + Hall sensor + hardware current limit**. The first is eye-open detection, implemented at
the lens (`np_mod_visual.c`, `np_mod_visual_hal_ir_eye_open()`, `OI-VIS-03`).

`NP-HFE-001` CT-03 makes an explicit argument that the interlocks are **not shade-dependent**:
*"Hall sensor detects goggle lift regardless of shade; IR proximity + photoparoxysmal EEG detection
at Oz halts session <200 ms independent of shade choice — shade selection affects comfort/immersion,
not the safety interlock."*

**That argument does not transfer, and the reason is positional.** The shade system snaps on
**outboard** of the lens; the IR sensor looks **inboard**, toward the eye. A shade is never between
the sensor and the eye. A spectacle lens worn behind the assembly **always is** — and it is
user-supplied, so its 940 nm behaviour is unknown and varies by lens material (CR-39, polycarbonate,
high-index), by anti-reflective coating stack, and by photochromic state.

The failure mode is specific: a coating that returns 940 nm strongly can read as an eye where there
is none. That is a **false eye-open**, which is the permissive direction, on a Class C interlock.

This is compounded by `OI-BENCH-08`, already open: `np_mod_visual_hal_hall_lifted()` currently
carries three different documented meanings across one file (*"goggles off head"*, *"magnet present
→ goggles seated"*, *"goggles must be on head"*). On the committed assembly that ambiguity is a
clarity defect. On an assembly with a second removable optic in the cavity it becomes load-bearing,
because *"seated"* acquires a second thing it could mean.

> **Rev 2 (2026-09-26).** `OI-BENCH-08` is closed (`NP-FW-BENCH-001` Rev 3). The predicate now
> measures **goggle seating on the headset**, and it is renamed `np_mod_visual_hal_goggle_seated()`
> with positive, fail-safe sense. The argument above still holds, and more sharply. The Hall element
> and its magnet are unplaced (`OI-BENCH-12`), and the only magnets `NP-TOOL-LENS-001` places are the
> shade-retention N42s. A removable optic carrying magnets of its own would be a third candidate
> actuator. Whether that concern survives therefore depends on where `OI-BENCH-12` puts the element.

**Consequence:** `REQ-FITOVER-03` cannot be met by re-using the committed interlock. It needs either
a sensing path that cannot see the spectacle lens, or a fourth layer, and either is a new safety case
with a new hazard analysis — not an inherited one.

### 5.2 Mode F meters a dose through an optic it cannot characterise

Mode F is *invisible NIR retinal walk* — 808–830 nm daily retinal PBM during normal-looking wear.
The platform's PBM discipline throughout is **metered dose**: dual-PD dose metering on the
transcranial tiles, J/cm² per zone recorded in UHDR.

An unknown spectacle lens in the optical path defeats that at the source. NIR transmission at
808–830 nm varies widely across lens materials and coatings, and UV/IR-attenuating coatings are
common and are **not** disclosed on a prescription. A metered dose that passes through an
uncharacterised, undisclosed attenuator is not a metered dose; it is a metered *emission* with an
unknown delivered fraction.

**Recommendation: on a fit-over assembly, Mode F is withdrawn rather than de-rated** — the honest
option under CLAUDE.md's own posture, and the same shape of answer as the §13.2c precedent
(withhold the claim rather than redefine the parameter to fit the hardware). `REQ-FITOVER-04` is
written to allow either outcome, but this document's recommendation is the withdrawal.

Note what this costs: a user on a fit-over assembly loses a modality the S3 path keeps. **The
deferred alternative is not merely later than the committed path — it is capability-negative against
it**, which is the strongest argument in this document for the decision §1 records.

---

## 6. Placement in the accessory priority set

### 6.1 What the decision does to `OI-ACC-01` and to rank 2

`NP-ACC-PRIORITY-001` §5 ranked the **S3 prescription clip + Rx insert** at **rank 2**, class **C1
(access)**, and flagged the class as *contested* because nothing in the document set said whether
the goggles could be worn over glasses — placing it on a stated judgement rather than on evidence.

§1 answers it. **C1 is confirmed, and rank 2 is now derived rather than judged:** the platform
provides exactly one path by which a corrective-eyewear user reaches the visual modality, and it is
the S3 Rx insert. Under §3.1's second form of C1 — *a channel the platform delivers but a given user
cannot receive* — that is access, not convenience.

### 6.2 Where the fit-over assembly itself lands: rank 19, bottom of the set

Placed by `NP-ACC-PRIORITY-001` §3.5's four-step procedure, which touches one class and leaves the
rest of the list alone:

| Step | Result |
|---|---|
| **1 — scope** | In scope: a purchasable assembly that is not the device |
| **2 — class** | **C5 (experience).** Not C1: access to the visual modality is *already delivered* to this cohort by the S3 Rx insert, so a second route to it unlocks nothing. Not C2/C3/C4: dose, protocol, concurrency and envelope are all unchanged — and by §5.2 the delivered capability is *lower*. What it offers is that the user need not buy an Rx insert |
| **3 — tie-break within C5** | Below the Apple Watch sync app (rank 18) on **reach**: the Watch app is available to every iPhone + Watch user across all configurations; this reaches only corrective-eyewear users who decline the committed path. On **Test 4** it is additionally the highest-engineering-cost item in the whole set — a second goggle assembly, a second tooling family, and the new safety case in §5 |
| **4 — gate** | **EXTERNAL — demand** (§2.2) |

**Rank 19 of 19. Bottom of the set, and it arrives there by derivation.** The criterion was written
before this item existed and reproduces the principal's *"very low priority"* without being told
it — which is the second falsification `NP-ACC-PRIORITY-001` §4 said it lacked, and the first one
performed across classes rather than within a pair.

`REQ-FITOVER-08` is the claims consequence of the C5 placement: a second assembly must carry no
therapeutic claim the first does not, or the programme acquires two evidence bases for one modality.

### 6.3 It forecloses nothing by waiting

`NP-ACC-PRIORITY-001` §3.1 Test 3 asks whether deferring an item forecloses anything. Here it does
not, and that is worth stating because it is the opposite of the mastoid-pad anchor boss (§7 of that
document), where the provision window closes at first tooling cut.

A fit-over assembly is a **separate assembly**, not a feature of the committed one. No boss, no
socket, no reserved volume and no first-cut provision is needed to keep it possible. **Deferring it
costs nothing that cannot be recovered**, which is exactly why deferring it is the right call and why
this document asks for no tooling decision.

---

## 7. What would have to be true to start

Not a plan. The four things that would have to exist before this concept could become a project, in
the order they would have to be settled:

1. **A demand signal that was actually specified and then met** (§2.2) — the threshold set when
   there is a fleet to measure, and met, in that order. A signal defined after the fact is not one.
2. **A frame-accommodation range chosen** (`REQ-FITOVER-01`). It sets cavity depth, and cavity depth
   sets every item in §4. Nothing can be costed before it.
3. **A resolution for `REQ-FITOVER-03`** — a sensing path that cannot see the user's spectacle lens,
   or an accepted fourth interlock layer, with its own hazard analysis. **This is the gating
   technical question, not the mechanical design.** `OI-BENCH-08` should be closed on the committed
   assembly first, on its own merits.
4. **A decision on Mode F** (`REQ-FITOVER-04`), taken as a claims decision rather than an
   engineering one, with §5.2's recommendation as the default.

Only after (2) is there anything a cost estimate could be run against — and under CLAUDE.md §2.1's
standing constraint, any figure produced would be a floor excluding term **U**, and would set no
price (`OI-COST-10`).

---

## 8. Open items

None raised. This document **closes** `OI-ACC-01` and deliberately opens nothing in its place: a
deferred concept that generates open items generates work, and the point of the decision in §1 is
that there is no work here.

The questions in §7 are recorded as **preconditions**, not as open items, precisely so they do not
appear on anyone's blocking list. They become open items on the day the demand gate lifts, and not
before.

One item **open elsewhere** at Rev 1 was cross-referenced rather than duplicated: `OI-BENCH-08`
(`NP-FW-BENCH-001`) was worth closing on the committed assembly regardless of this document — §5.1
says why it would become load-bearing here, which strengthened an existing case rather than making a
new one. **Rev 2:** it closed on 2026-09-26 (`NP-FW-BENCH-001` Rev 3). Its successor,
`OI-BENCH-12` (where the Hall element and its magnet are), is the item this document now depends on.
