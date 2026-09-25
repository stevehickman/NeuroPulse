# Two-Bowl Headset Shell Tooling Specification

**Project:** NeurOne
**Document:** NP-TOOL-SHELL-002
**Revision:** 1
**Date:** 2026-09-25
**Status:** DRAFT
**Effective Date:** — (opens for sign-off when `MECH-1` delivers the §3 geometry)
**Author:** NeurOne Systems Engineering
**Approved By:** —
**References:** NP-TOOL-SHELL-001 Rev 3 (superseded predecessor, §2 dispositions every feature); NP-HEX-ZM-001 Rev 6 §5.1–§5.6.1, §7 (MECH-1, EMF-1, EMF-2, EMF-3); NP-HELMET-GEOM-001 Rev 2 §2, §3.1, §4; NP-DRV-SHELL-002 Rev 4 §4.1–§4.3, §12 (OI-SHELL2-02); NP-EMC-CAV-001 Rev 22 §8.2 (REQ-CAV-04); NP-REG-UPG-001 Rev 3 §7.4 (OI-UPG-05); NP-THERM-COOL-001 Rev 16 (OI-THCOOL-06); NP-HW-TACSDRV-001 Rev 4 (OI-TACSDRV-04); NP-FMEA-GEOM-001 G03, G05; NP-RISK-004 Rev 3 (RISK-20); NP-BIB-EMF-001 Rev 6 (OI-BIBEMF-01); NP-ART-001 Rev 9 (A5, A6, OI-ART-01); NP-CONV-001 Rev 9 §4, §6, §7.1
**Related Issues:** GitHub Issue #331 (OI-ART-01, MECH-1, EMF-1); GitHub Issue #16 (closed predecessor); GitHub Issue #6 (RISK-20, blocks tooling release)
**Gate:** NP-COORD-001 G2 (pre-tooling) — shell tooling first cut. `NP-REV-SHELL-001` is the review record that gates the same cut
**IEC 62304 Class:** N/A
**Supersedes:** NP-TOOL-SHELL-001 Rev 3 (moved to `docs/superseded/`; every feature dispositioned in §2)
**Parent Document:** NP-HEX-ZM-001 Rev 6 §5 (two-bowl shell architecture)

---

> **What this revision is, and what it is not.** It is the successor document `OI-ART-01` asked for.
> It takes a **new serial**, not Rev 4 of `NP-TOOL-SHELL-001`, because the architecture that document
> tooled was replaced (§1.1). It carries forward the **one** surviving feature, the temporal wing boss,
> unchanged. It lists every shell feature the two-bowl architecture needs. It collects every
> constraint that binds `MECH-1` into one register (§5).
>
> **It invents no geometry.** The four-corner latches, the posterior-centre boss, the Hall interlock
> detail and the labyrinth lip are `MECH-1`'s, and `MECH-1` is CAD work. Each of those features is
> listed here with its requirements and sources and **no dimension**. Where a number appears in this
> document, it is carried verbatim from the document that derived it and is cited to it. **Shell
> tooling cannot be released from this revision.**

---

## 1. Purpose and scope

### 1.1 Why a new serial rather than NP-TOOL-SHELL-001 Rev 4

`NP-CONV-001` §4 decides this: **new number when the architecture is replaced; new revision when
decisions are inherited.** `NP-TOOL-SHELL-001` was written on 2026-05-10 for a single CFRP shell with
five position-unique zone slots. Of its four features, **three are retired** (F-01 and F-03 at Rev 2,
F-02 at Rev 3), and both of its stated parents (`NP-DRV-SHELL-001` and `NP-TOOL-ZM-001`) are
superseded. What survives is one boss on the temporal wing and a set of reusable engineering rules
(§6). Nothing here inherits its feature list. A Rev 4 would have been a new document under an old
serial, which is the authoring error `NP-CONV-001` §4.0.4 names in `NP-DB-001…005`. This is the same
call `NP-DRV-SHELL-002` §1.2 made for its predecessor.

`NP-TOOL-SHELL-001` Rev 3 moves to `docs/superseded/` unedited. It is referenced from many documents,
so it is retained (`NP-CONV-001` §4.0.4). Its serial still resolves, and its successor is named in
`docs/superseded/README.md`.

`NP-ART-001` §5 kept `NP-TOOL-SHELL-001` in place for one reason: it was *"the only tooling
specification the headset shell has"*. This document is now that specification, so the reason no
longer applies.

### 1.2 In scope

| Part | Artifact (`NP-ART-001` §2) | What this document governs |
|---|---|---|
| **Outer bowl**: CFRP structural shell with the EMF stack laminated to its inner face | **A6** | Every moulded or laid-up feature: the bowl loft, latches, boss, lip, coil formers and TMS window (§3) |
| **Inner bowl**: the moulded module carrier | **A5** (carrier moulding only) | Features the carrier **mould** must carry: latch halves, the carrier side of the boss, the lip's mating face, and the cluster-clamp and controller-board seats (§3, S2-F09). **The rigid-flex lamination into the carrier stays `NP-DRV-SHELL-002`'s** |
| **Temporal stability wing**: the anchor boss only | — (see OI-TSHELL2-03) | S2-F01, carried from `NP-TOOL-SHELL-001` F-04 |

### 1.3 Out of scope

- **Hub enclosure and every hub port cover:** `NP-TOOL-HUB-001` (`OI-ART-07`, closed 2026-09-23).
- **Lens rim and goggle arm:** `NP-TOOL-LENS-001`.
- **Hex-tile module shell:** `NP-TOOL-HEXTILE-001`.
- **Socket bodies and the 19-contact array (A3):** tooling blocked on `OI-SHELL2-03(b)`, and
  `OI-MMSOCK-12` requires the 19 → 20 contact change before socket tooling. This document lists only
  where the carrier mould has to receive them.
- **Cluster clamp plate and over-centre actuator (A7):** `MECH-2`.

### 1.4 Station names: "L0" does not mean the outer bowl

**This document does not use "L0" or "L1" as bowl names, and it records why.** Three numbering
schemes are in use on the same part:

| Scheme | Owner | What "L0" means | What "L1" means |
|---|---|---|---|
| **Radial stations** | `NP-HELMET-GEOM-001` §2 (the owning radial stack) | the scalp-contact face datum | the inner bowl (module carrier) |
| **EMF layers** | CLAUDE.md §4.3, `NP-HEX-ZM-001` §5.1 | — | the CFRP shell (EMF L1). L2 is mu-metal, L3 palladium, L4 is deleted and retired, L5 the port filters |
| **Artifact labels** (until this revision) | `NP-ART-001` A6, `NP-RISK-004` §1, `NP-TOOL-SHELL-001` Rev 2 banner, GitHub #331 | **the outer bowl** | the inner bowl |

The artifact labels disagree with the radial stack that owns the geometry. Under
`NP-HELMET-GEOM-001` §2, the outer bowl is **stations L2 + L3** and L0 is the scalp face. The third
scheme also lets "L1" name either the carrier or the CFRP shell. **This document says "outer bowl"
and "inner bowl", and writes EMF layers as "EMF L1"…"EMF L5".** `NP-ART-001` (Rev 9) and
`NP-RISK-004` (Rev 3) are corrected to the same words in this change. The dated status-log entries
keep their wording, because they are history.

---

## 2. Disposition of every NP-TOOL-SHELL-001 feature

| Rev 3 feature | Status there | Disposition here |
|---|---|---|
| **F-01** zone-slot plug anchor posts ×5, colour-coded | Retired (Rev 2) | **Not carried.** No zone slots and no zone colours exist. The ~80 sockets are on the inner bowl and face the scalp, and the plugs that fill empty ones are a different question with an answer already on record (§4). **No per-socket anchor post is required by anything** (§4.2) |
| **F-02** hub accessory-port cover anchor posts ×3 | Retired (Rev 3) | **Not carried.** Owned by `NP-TOOL-HUB-001` F-02, with `OI-HTOOL-08` and `OI-HTOOL-09` open there |
| **F-03** EEG cable routing channel, 8 × 5 mm | Premise retired (Rev 2) | **Not carried, and GitHub #331's checklist asks for it in error.** Its line *"Carry forward the EEG cable channel (§2.4), which survives the architecture change"* is contradicted by `NP-TOOL-SHELL-001` Rev 2's own banner, `cad/CAD_PARTS_LIST.md`'s `EEG-ROUTE-CHANNEL` tombstone and `NP-RISK-002`'s RISK-21 disposition. EEG electrodes are inside T1-B tiles, and `NP-DRV-SHELL-002` §5.3 absorbs the harness, so the channel has no cable to carry. What survives is the < 5 µVpp threshold, as `SH2-DRC-16`. **Reopening condition, carried:** if `OI-EEGNET-01` resolves toward the geodesic strut net (`NP-HW-EEGNET-001` §7.3), a routing feature is **re-derived against the two-bowl shell**, not restored from F-03 |
| **F-04** temporal wing anchor boss ×2, PROVISIONAL | Survives | **Carried unchanged as S2-F01.** One correction: the dust-cover tether was specified *"same anchor post tether spec as F-01"*, a retired feature. The geometry is restated inline (§3.1), as Rev 2's banner note (a) asked |

---

## 3. Feature register

**Status key.** **SPECIFIED** means content is carried from a derived source with dimensions.
**REQUIREMENTS ONLY** means what the feature must do and where that is traceable, with no geometry,
awaiting `MECH-1`. **GATED** means the feature's existence or form waits on a named decision.

| ID | Feature | Part | Status | Governing source |
|---|---|---|---|---|
| **S2-F01** | Temporal wing anchor boss ×2 (PROVISIONAL) | wing | **SPECIFIED**, carried | `NP-TOOL-SHELL-001` F-04 |
| **S2-F02** | Outer-bowl loft 3 mm inward; no absorber lamination | A6 | **SPECIFIED** as a loft rule | `REQ-CAV-04`; `NP-HELMET-GEOM-001` §2 |
| **S2-F03** | Four rim latches AL / AR / PL / PR with hard-gold BeCu ground-bond fingers | A6 + A5 | **REQUIREMENTS ONLY** (`MECH-1`) | `NP-HEX-ZM-001` §5.3(b), §5.4 |
| **S2-F04** | Layer-closed Hall / contact sensor at each latch | A6 + A5 | **REQUIREMENTS ONLY** (`MECH-1`) and **no electrical owner** (OI-TSHELL2-02) | `NP-HEX-ZM-001` §5.5 |
| **S2-F05** | Posterior-centre blind-mate boss, outward local emboss | A6 + A5 | **REQUIREMENTS ONLY** (`MECH-1`) | `NP-HEX-ZM-001` §5.3(c), `BOSS-1`; `NP-DRV-SHELL-002` §4.3 |
| **S2-F06** | Labyrinth rim lip (≥ 2× overlap) + conductive elastomer bead groove | A6 + A5 | **REQUIREMENTS ONLY** | `NP-HEX-ZM-001` §5.3(a); `EMF-3` |
| **S2-F07** | Helmholtz coil formers | A6 | **REQUIREMENTS ONLY**. Former geometry is fixed once set, because calibration depends on it | `NP-HELMET-GEOM-001` §2; `NP-HEX-ZM-001` §5.3.1 |
| **S2-F08** | Non-conductive CFRP TMS window (T2 layups only) | A6 | **GATED** on `OI-UPG-05` | `NP-HELMET-GEOM-001` §2 note; `NP-REG-UPG-001` §7.4 (2) |
| **S2-F09** | Carrier seats: socket bodies, cluster-clamp bosses (`PACK-1`), controller-board seats (`BOARD-1`), fluxgate station | A5 | **REQUIREMENTS ONLY**. Positions follow the lattice, and the socket is A3's | `NP-HELMET-GEOM-001` §2 (`PACK-1`), §4; `NP-DRV-SHELL-002` §3.2 (`BOARD-1`), §4.1 |

### 3.1 S2-F01 — Temporal wing anchor boss (×2 bilateral, PROVISIONAL)

**Carried verbatim in substance from `NP-TOOL-SHELL-001` F-04 (§2.4), and nothing is re-derived
here.** The boss provides the mechanical attachment point for the 40 Hz mastoid vibrotactile LRA pad
(A19; `docs/reference/accessories-roadmap.md`). Moulding it at first cut costs nothing; retrofitting
it costs $15,000–40,000 plus 6–8 weeks (F-04's own rationale). If the pad never ships, the boss is a
non-functional recessed cavity.

- **Count:** 2, one per temporal wing (left, right).
- **Location:** the mastoid process contact point on the temporal stability wing: the
  posterior-inferior area of the wing, where it contacts the skull behind the ear.
- **Receiver:** external envelope 34 × 34 × 5 mm proud of the wing surface. Receiver opening
  Ø 30 mm. Snap detent: 0.5 mm undercut bead at 3 mm depth, 360°. Release force 3–6 N axial.
- **Recessed when unused:** flush with, or 0.5 mm below, the wing outer surface. All entry radii
  ≥ 1.5 mm.
- **Connector pocket:** 10 × 6 × 4 mm at the 6 o'clock (inferior) position, ≥ 1.5 mm clearance
  around the connector body.
- **Load path:** 15 N axial pull and 10 N lateral shear without visible deformation. **Verify by
  FEA or hand calculation before first cut** (OI-TSHELL2-06).
- **Material:** integral to the wing (PC/ABS or ABS). A 0.5 mm Shore 30A silicone overmould at
  the receiver face.
- **IPX4:** the receiver pocket must not open an ingress path into the headset interior. With a pad
  fitted, the detent seals against the pad's silicone collar. With no pad fitted, a tethered snap-fit
  dust cover closes the pocket.
- **Dust-cover tether anchor (restated inline; F-04 pointed at retired F-01):** a cylindrical boss
  **Ø 4.0 mm × 3.5 mm** with a **2.0 mm wide × 2.5 mm deep** transverse through-slot for the tether
  loop, base radius **≥ 1.0 mm** and draft **≥ 1.5°**. Tether: **Shore 30A** medical silicone loop,
  2.0 mm cross-section. **Where on the wing it goes is still F-04's open item** (OI-TSHELL2-07).
  **Reach limit.** The loop's 50 mm circumference came from F-01 and is **not carried**. Its reach
  (≈ 25 mm, `NP-TOOL-SHELL-001` Rev 3's own correction) has no hazard analysis on the wing. Size the
  loop under OI-TSHELL2-07, and do not copy it.
- **Marking:** none indicating mastoid-pad compatibility until HOPE Phase 3 reports (OI-TSHELL2-08).
- **Draft:** ≥ 1° on all boss interior surfaces.

### 3.2 S2-F02 — Outer-bowl loft: 3 mm inward, no absorber lamination

`REQ-CAV-04` was taken on 2026-09-23 (principal, GitHub #391). The EMF Layer 4 absorber is deleted,
and the outer bowl is re-lofted **3 mm inward**. **The re-loft is binding with the deletion.** A
vacated station fills with stagnant air and makes the outward thermal path worse (0.410 → 0.450
m²K/W instead of → 0.335; `NP-EMC-CAV-001` §8.2). The loft is `NP-HELMET-GEOM-001` §2's radial stack.
The outer-bowl subtotal is ~3 mm, and the total stack from scalp to exterior is **27–32 mm**.

**What the tool must reflect:**

1. The bowl's inner surface is the palladium liner (EMF L3), laminated over mu-metal + PETG (EMF L2)
   onto the CFRP (EMF L1). **No absorber ply and no absorber bonding step.**
2. **The Gap does not change and the inner bowl does not move.** The cluster clamp now reacts
   against rigid Pd / mu-metal / CFRP, and its tolerance stack drops from ±1.30 to ±0.80
   (`NP-EMC-CAV-001` §8.3).
3. **The layer number L4 is retired and held.** The port filters stay EMF L5.
4. **The only documented way anything returns to the station** is a thin, electrically insulating,
   non-magnetic pad under `OI-EMCCAV-08` (`MECH-2`), sized by the ±0.80 stack. It is never an
   absorber.
5. **One local exception.** `BOSS-1` embosses the bowl **outward** at the occiput centreline
   (S2-F05). The two changes sit at different locations and do not conflict.

### 3.3 S2-F03 … S2-F08 — what MECH-1 must deliver

These features have no geometry anywhere in the record. Each one's requirements are in §5, the
register of every constraint that binds `MECH-1`. **When `MECH-1` delivers, each feature gets its
own subsection here in the form of §3.1**, with dimensions, tolerances, draft, material and sealing,
plus rows in §7 and §8.

---

## 4. Interface covers on ~80 identical sockets

GitHub #331 asked for *"the interface-cover anchor posts re-specified against ~80 identical
sockets"*. `NP-ART-001` `OI-ART-01` called whether unpopulated sockets need plugs *"an open design
question"*. **The question is already answered on the record, and the answer is not anchor posts.**

### 4.1 What the record already says

- **`NP-HELMET-GEOM-001` §3.1** specifies a **blanking plug** for empty and add-on sockets: an
  overmoulded PC core with an LSR seal face, opaque, on **the identical hex footprint**, so *"an
  empty socket seals exactly like a filled one"*. §4 lists it among the load-bearing features
  (*"Blanking plugs (empty/add-on sockets) on the module footprint"*).
- **`NP-FMEA-GEOM-001` FMEA-G03-02** (empty socket left unplugged: ingress plus exposed contacts) has
  three mitigations: **tethered or retained plugs**, the app inventory poll flagging an unplugged
  socket, and socket contacts **de-energised unless a module UID authenticates**. The residual is 2.
- `NP-HEX-ZM-001` §5.1 puts the sockets on the **inner bowl's scalp-facing face**, inside the Faraday
  envelope. F-01 put its posts on the **outer** CFRP surface. That surface now carries no sockets.

### 4.2 What follows for the shell tool

1. **The plug is retained the way a tile is.** It sits on the identical footprint, so the socket's
   own retention holds it: the cluster clamp and the socket key. That satisfies FMEA-G03-02's
   *"retained"* while the plug is seated, and it needs **no shell feature**.
2. **No per-socket anchor post is required.** Nothing in the record asks for one. Eighty posts on the
   scalp-facing face would sit in the inter-tile gaps, which are also the clamp-boss and seal lands
   (`PACK-1`, `NP-HELMET-GEOM-001` §3.1). Under CLAUDE.md §18, **a feature nothing requires is not
   written.**
3. **The open part is loss while the plug is out.** A plug taken out to fit a tile has no tether
   under 4.2(1). FMEA-G03-02 accepts *"tethered **or** retained"*, and the residual rests on the
   other two mitigations (the inventory poll and de-energised contacts), which do not depend on the
   plug. Whether a loose-plug loss rate is acceptable, and what carries the plug in the box or case,
   is **OI-TSHELL2-03**. It is a question for the plug and its accessory, not the shell mould.
4. **The blanking plug has no artifact row.** `NP-ART-001` A22 covers tethered covers. A plug on the
   hex footprint is neither tethered nor a cover, and no tooling specification names it (OI-TSHELL2-03).

---

## 5. MECH-1 design-input register

`MECH-1` is *"Four-corner clamp (AL/AR/PL/PR) + posterior-center connector boss + Hall interlock
detail"* (`NP-HEX-ZM-001` §7), and it blocks shell tooling. **Its inputs are spread across more than
ten documents, and several are time-boxed to it.** Such an item is cheap to settle before `MECH-1`
cuts, and expensive after. This register collects them. It decides none of them.

**Column "Before MECH-1?":** **YES** means the owning document says it must be settled before
`MECH-1` fixes the geometry. **AT** means `MECH-1` itself settles or verifies it. **AFTER** means it
verifies a `MECH-1` output.

### 5.1 Four-corner rim latches (S2-F03)

| # | Constraint | Source | State | Before MECH-1? |
|---|---|---|---|---|
| L-1 | Four latches, symmetric: AL, AR, PL, PR, at the rim between the ear (audio) and neck-attach zones. Recessed lever, flush when closed | `NP-HEX-ZM-001` §5.4 | **Decided** | AT |
| L-2 | AL and AR flank the 5-position forehead bridge, not the centreline | §5.4 | **Decided** | AT |
| L-3 | Each latch carries **hard-gold BeCu spring fingers** (or conductive elastomer) bonding the outer shield to system ground across the parting plane, **≤ 50 mΩ**. This makes the shell a **driven EEG shield** (bonded to DRL) | §5.3(b); CLAUDE.md §4.3 | **Decided** | AT |
| L-4 | Bond resistance holds ≤ 50 mΩ over clamp-cycle life, and is trended in SHDR | `EMF-2`; §5.6; FMEA-G05-02 | **Open** (never run) | AFTER |
| L-5 | Gasket line pressure at the **back-centre (PL–PR) span and both ear spans** stays above the seal threshold with this pattern. If marginal, stiffen the lip or gasket, or add a lateral or posterior-centre latch. **Not more corner latches** | `EMF-3`; §5.4 | **Open** (unmeasured) | AFTER. It can move a latch, so run it on the first article |
| L-6 | The mild over-constraint of four points is absorbed by the compliant bowls and gasket | §5.4 | Assumed | AFTER (`EMF-3`) |
| L-7 | Clamp tolerance stack ±0.80 against a rigid outer bowl (was ±1.30 with foam) | `NP-EMC-CAV-001` §8.3 | **Decided** by `REQ-CAV-04` | AT |
| L-8 | The latches, not the boss, carry the bowl-to-bowl reinforcement at the rim ("outer edge" attachment) | `NP-HELMET-GEOM-001` §2 consequences | **Decided** | AT |

### 5.2 Layer-closed Hall / contact interlock (S2-F04)

| # | Constraint | Source | State | Before MECH-1? |
|---|---|---|---|---|
| H-1 | A Hall or contact sensor on **each** of the four latches reports closed or open | `NP-HEX-ZM-001` §5.5 | **Decided** | AT |
| H-2 | *"The safety architecture **refuses to enable any modality** unless all four report closed"*. Analogous to the goggle-lift Hall cutoff | §5.5; `docs/np_hex_zm_isa.md` ISC-27 | **Decided as a requirement; no implementation, no electrical path, no hazard row** | **YES**. See OI-TSHELL2-02 |
| H-3 | Every `EMF-1` sweep runs with all four latches reporting closed | §5.6.1 | Decided | AFTER |

**H-2 is the finding this register exists to surface.** The interlock is stated once, in
`NP-HEX-ZM-001` §5.5, as a safety function. **Nothing downstream carries it:**

- CLAUDE.md §4.2's interlock table has no row for it.
- `firmware/safety_mcu/src/` has no input for it.
- `NP-HW-HUB-001` and `NP-DRV-SHELL-002` §7 assign no pin or conductor to it.
- `NP-DRV-SHELL-002` §4.3's boss contact groups have no group for it.
- `NP-RISK-004` has no hazard row for a session enabled with the bowls open.

The contact-group point decides whether it is `MECH-1`'s problem. The latches are at the rim, and
the safety MCU is at the hub. **If the four sensor signals cross the parting plane at the boss, they
need contact positions and a segregation class in the boss layout** that `OI-SHELL2-02` is fixing.
If they cross at the latches, the latches need contacts. Either way, it has to be settled before
`MECH-1` cuts. **Raised as OI-TSHELL2-02.** This document does not decide which safety tier owns the
interlock. That is a change to a Class C boundary, and CLAUDE.md §4.2 says the safety MCU owns every
stimulation enable line.

### 5.3 Posterior-centre blind-mate boss (S2-F05)

| # | Constraint | Source | State | Before MECH-1? |
|---|---|---|---|---|
| B-1 | A **standalone** boss at the occiput centreline, **not a latch**, mated automatically as the bowls close and seated by the flanking PL / PR latches | `NP-HEX-ZM-001` §5.3(c), §5.4 | **Decided** | AT |
| B-2 | **`BOSS-1`:** projects **outward** as a local emboss of the outer bowl. Its planform is the boss footprint, and its depth the minimum the blind-mate stack needs (contact wipe + lead-in chamfer + **±0.4 lateral / ±0.5 Z**) | §5.3(c) | **Decided** (principal, 2026-09-20) | AT |
| B-3 | The emboss stays **inside the existing Boa-arch / neck-attach exterior volume**. Otherwise it becomes a new exterior feature, with `NP-HW-FITOVER-001` consequences | §5.3(c) `BOSS-1` constraint 1 | Decided | AT |
| B-4 | It **must not intersect a Helmholtz coil former.** The formers are keep-outs, because calibration depends on their geometry (`REQ-EMI-11`) | `BOSS-1` constraint 2; `NP-HELMET-GEOM-001` §2 | Decided | AT |
| B-5 | **Mu-metal forming cost:** drawing EMF L2 over the dome work-hardens it, and it cannot be re-annealed after lamination. The accepted treatment is a mu-metal chimney collar. **ELF leakage through a formed collar, and the permeability the forming costs, are unmeasured** | `BOSS-1` constraint 3; `OI-THCOOL-06` (reopened 2026-09-20, `EMF-1e`) | **Open**. `OI-THCOOL-06` is **BLOCKING on MECH-1 cutting the boss** | **YES** |
| B-6 | **One aperture, segregated returns.** The module interconnect shares this boss (no second aperture against the λ/20 slot criterion). The boss presents **segregated contact groups with independent returns**: {N1 power}, {N2/N5 digital + safety}, {N4 post-ADC digital}, {fluxgate / coil harness}, star-returned at the Hub PCB, with **no shared return between power and fluxgate / coil** | `NP-DRV-SHELL-002` §4.3; `REQ-EMI-05` | **Open**: `OI-SHELL2-02`, time-boxed. The boss now carries **20** tail groups, and `OI-HUB-C07`'s broadcast enable (closed, adopted at `NP-DRV-SHELL-002` Rev 3) turns the {N2/N5} group into a trunk, which changes the layout | **YES** |
| B-7 | The laminated inner-bowl tails (18 cluster tails plus the PAN) **terminate into this boss**. That is why `MECH-1` blocks A5's FAI as well as A6's | `NP-ART-001` §3.2; `NP-DRV-SHELL-002` §4.2 | Decided | AT |
| B-8 | **T2 content through the boss is fixed here.** The boss and the outer-bowl layup are *"fixed at MECH-1"*. Anything that would overturn the new-unit decision (§7.0) must come before `MECH-1` | `NP-REG-UPG-001` §7.2; `REQ-UPG-03` | **Decided** (new unit, 2026-09-23). T1 carries boss contact **positions** only, with no populated T2 content (§7.4 (1)) | **YES** (already met) |
| B-9 | **PAN thermal budget.** If the 21-channel tACS sources sit at the PAN (`OI-TACSDRV-01` Q2), ~3.4 W worst case sits at the occiput inside the shield | `NP-HW-TACSDRV-001` `OI-TACSDRV-04` | **Open** | **YES** (stated there as *"before MECH-1"*) |
| B-10 | The latch-and-boss arrangement is `NP-HEX-ZM-001` §5.4's. Decoupling the connector from the clamp pattern is deliberate: *"a blind-mate feature only has to mate on closure; it should not dictate latch count"* | §5.3(c) | Decided | AT |

### 5.4 Labyrinth lip, coil formers, TMS window (S2-F06 … S2-F08)

| # | Constraint | Source | State | Before MECH-1? |
|---|---|---|---|---|
| P-1 | The outer bowl overlaps the inner bowl's rim with a **≥ 2× overlap labyrinth lip**, with no line of sight from outside to the modules. Any continuous residual slot **≤ λ/20 at 6 GHz ≈ 2.5 mm**. A conductive elastomer bead closes the residual gap | `NP-HEX-ZM-001` §5.3(a) | **Decided** | AT |
| P-2 | The conductive parting-plane gasket is a **replaceable, tethered service part** | §5.6 | Decided | AT (groove must allow replacement) |
| P-3 | The lip and the closed-state lever footprint set the Gap, together with the 18 controller boards (2.75–3.45 mm closed-state stack). Travel does not (`FLUSH-1`), and neither do the boss (`BOSS-1`) or the fluxgates | `NP-HELMET-GEOM-001` §2 Gap row; `NP-EMC-CAV-001` §8.7 | **Open**: re-derive under `MECH-2` | AT, jointly with `MECH-2` |
| P-4 | **RISK-20** — can moulded CFRP hold **Ra ≤ 1.6 µm** on seal-seat surfaces without secondary operations? A tooling manufacturer must confirm in writing | `NP-RISK-004` RISK-20; `NP-PROC-SUP-001` SUP-M-07 / SUP-B-01; **GitHub #6 blocks #331** | **Open**, never mitigated | **YES** (tooling release) |
| C-1 | Helmholtz coil formers are on the outer bowl and **add local thickness only**. Their geometry is **fixed** once set, because the coil-drive → field transfer function is calibrated against it | `NP-HELMET-GEOM-001` §2; `NP-HEX-ZM-001` §5.3.1 reason 4 | Decided | AT |
| W-1 | The TMS window is **non-conductive CFRP at the coil site**, with local mu-metal routed around it. **T1 bowls are laid up without it, and every T2 bowl carries it** | `NP-HELMET-GEOM-001` §2 note; `NP-REG-UPG-001` §7.4 (2) | **Open**: `OI-UPG-05`, whether it is a **layup choice or a mould feature**. **Time-boxed to MECH-1** | **YES** |

### 5.5 What the register shows

- **Six items must close before `MECH-1` can cut:** B-5 (`OI-THCOOL-06`), B-6 (`OI-SHELL2-02`), B-9
  (`OI-TACSDRV-04`), P-4 (RISK-20, GitHub #6), W-1 (`OI-UPG-05`) and H-2 (OI-TSHELL2-02, new here).
  B-8 is already met.
- **Two items verify `MECH-1` and can move it:** `EMF-3` (L-5) can add a latch, and `EMF-1` can
  reject the stack. Both need a first article. That is why `NP-ART-001` §3.2 lists them against
  `NP-FAI-SHELL-001` rather than against the design.
- **Nothing in the register needs a new principal decision except H-2's ownership.** Every other
  row is decided, or has an owner and an open item.

### 5.6 A note on RISK-20's scope

`NP-RISK-002` records that RISK-20's rim *"moved from 5 shell slots to ~80 socket rims"*.
**That premise needs checking against the two-bowl architecture.** The ~80 socket rims are on the
**inner bowl**, whose lead material is glass-filled PBT, with CFRP explicitly rejected
(`NP-HELMET-GEOM-001` §3.2). Their seals are co-moulded LSR lands (§3.1). **The CFRP seal-seat
surfaces left on this shell are the outer bowl's lip and bead groove (P-1).** If that holds, RISK-20
**shrinks** from ~80 rims to one continuous seat, and the process question is unchanged. This is an
observation for `NP-RISK-004` `OI-RISK4-05`, which already asks for the scope to be re-examined. **It
does not close RISK-20 or GitHub #6.**

---

## 6. Reusable engineering carried from NP-TOOL-SHELL-001

`NP-TOOL-SHELL-001` Rev 2's banner listed what survives the architecture change. Each rule below is
carried as written. **None is new, and each keeps the derivation its source gave it** (CLAUDE.md §18).

| Rule | Value | Source's stated reason |
|---|---|---|
| Boss-root radius on CFRP | ≥ 1.0 mm | Prevents stress concentration at the boss root |
| Sealing at every boss base | IPX4 maintained. Base flush-bonded or overmoulded with silicone sealant | No water ingress path at the boss base |
| Draft on bosses | ≥ 1.5° | Clean demoulding; confirmed at DFM review |
| Draft on interior surfaces | ≥ 1° | Clean demoulding |
| Anchor-post + tether pattern | Boss Ø 4.0 × 3.5 mm, through-slot 2.0 × 2.5 mm | Loss-prevention primitive, already reused by `NP-TOOL-HUB-001` F-02 and `NP-TOOL-LENS-001` F-07. **Reach is not carried** (§3.1) |
| Checklist structure | A named reviewer, a method and a pass criterion per row | Adopted by `NP-TOOL-HUB-001` and `NP-TOOL-HEXTILE-001` |

**Materials, carried for S2-F01 only:**

| Material | Application | Spec | Standard |
|---|---|---|---|
| PC/ABS, UL94-V0 | Temporal wing boss (S2-F01) | Charpy notched ≥ 45 kJ/m²; HDT ≥ 90 °C | UL 94, ISO 179 |
| Shore 30A medical silicone | Receiver-face overmould; dust-cover tether (S2-F01) | ISO 10993-5; UV-stable; compression set ≤ 15 % after 70 h at 70 °C | ASTM D395, ISO 10993-5 |
| Silicone sealant | Temporal wing IPX4 seal; tether-anchor base | Dow Corning RTV 738 or equivalent; Shore A 25–35 cured | IEC 60529 IPX4 |

**Outer and inner bowl materials are not specified here.** CFRP with the laminated EMF stack (outer)
and glass-filled PBT, with PA66-GF30 as fallback (inner), are `NP-HELMET-GEOM-001` §3.2–§3.3's lead
recommendations. They become this document's specifications when `MECH-1` delivers.

---

## 7. Critical dimensions

Carried from `NP-TOOL-SHELL-001` §3 for S2-F01. **Rows for S2-F03 … S2-F08 are added when `MECH-1`
delivers, and not before.**

| Dimension | Feature | Nominal | Tolerance | Critical for |
|---|---|---|---|---|
| Receiver diameter | S2-F01 | Ø 30 mm | ±0.3 mm | Pad clip fit |
| External envelope | S2-F01 | 34 × 34 × 5 mm | ±0.5 mm | Pad clip engagement |
| Snap detent undercut | S2-F01 | 0.5 mm | +0.0 / −0.1 mm | Retention (3–6 N) |
| Snap detent depth | S2-F01 | 3 mm from surface | ±0.3 mm | Release force |
| Connector pocket | S2-F01 | 10 × 6 × 4 mm | +0.5 / −0.0 mm | Connector clearance |
| Silicone overmould | S2-F01 | 0.5 mm | ±0.1 mm | Coupling comfort |
| Tether-anchor boss Ø × height | S2-F01 dust cover | Ø 4.0 × 3.5 mm | ±0.2 / +0.3 −0.0 mm | Tether retention |
| Tether-anchor slot width × depth | S2-F01 dust cover | 2.0 × 2.5 mm | +0.1 −0.0 / ±0.2 mm | Tether passage and lock |
| Tether-anchor base radius | S2-F01 dust cover | ≥ 1.0 mm | min only | Stress concentration |

---

## 8. Design review checklist

Each row needs a named reviewer and pass or fail evidence (a CAD view, a measurement or a DFM letter).
**BLOCKING** rows must pass before shell tooling is cut. **Carried rows keep their `NP-TOOL-SHELL-001`
ID in the "Was" column**, so a citation of the old ID resolves.

| # | Was | Check | Method | Pass criterion | Feature |
|---|---|---|---|---|---|
| TS2-01 | SH-15 | Both temporal wing bosses present at the mastoid location | CAD inspection | 2 bosses (L + R) at mastoid | S2-F01 |
| TS2-02 | SH-16 | Receiver Ø 30 ± 0.3 mm; depth ≥ 5 mm; detent at 3 mm | CAD measurement | As stated | S2-F01 |
| TS2-03 | SH-17 | Flush or recessed with no pad; entry radii ≥ 1.5 mm | CAD cross-section | As stated | S2-F01 |
| TS2-04 | SH-18 | Connector pocket present with ≥ 1.5 mm clearance | CAD measurement | As stated | S2-F01 |
| TS2-05 | SH-19 | ≥ 15 N axial and ≥ 10 N lateral without deformation (OI-TSHELL2-06) | FEA or hand calc | As stated | S2-F01 |
| TS2-06 | SH-20 | Every §7 dimension is a drawing callout with tolerance | Drawing review | All present | All |
| TS2-07 | SH-21 | Tooling-manufacturer DFM: draft floors met, no unintended undercuts, gates confirmed, written sign-off (OI-TSHELL2-05) | DFM letter | Written sign-off on file | All |
| TS2-08 | — | Outer-bowl loft matches `NP-HELMET-GEOM-001` §2 after the 3 mm re-loft, with **no absorber ply** in the layup schedule | CAD + layup schedule review | 27–32 mm total stack; no EMF L4 ply | S2-F02 |
| TS2-09 | — | **BLOCKING.** Every §5 row marked **YES** is closed, or recorded as accepted by its owner | Register review against §5.5 | Six rows closed | S2-F03 … S2-F08 |
| TS2-10 | — | **BLOCKING.** Written tooling-manufacturer confirmation of Ra ≤ 1.6 µm on CFRP seal seats (RISK-20) | Supplier letter | On file | S2-F06 |

**Rows for S2-F03 … S2-F08** (latch geometry, bond-finger contact resistance, Hall sensing, boss
mating tolerance, lip overlap and residual slot, coil-former keep-out, TMS window) **are written when
`MECH-1` delivers**, with pass criteria from §5's sources. They are not written as placeholders now,
because a criterion with no geometry to inspect is the kind `NP-FAI-001` §2 F2 rejects.

---

## 9. FAI cross-reference

| Feature | FAI item | Pass criterion |
|---|---|---|
| S2-F01 | FAI-TW-01 (carried) | PROVISIONAL, run only if HOPE Phase 3 is positive. Reference 30 mm pad clip inserted and removed 50×; detent releases at 3–6 N throughout |
| S2-F01 | FAI-TW-02 (carried) | PROVISIONAL. Boss flush or recessed with no pad; no sharp edges (Ra ≤ 3.2 µm at entry) |
| A6 as a whole | `NP-FAI-SHELL-001` | **Named absence** (`NP-ART-001` §3.2). Blocked on `MECH-1`, `EMF-1`, `EMF-3` and RISK-20. This document removes none of those blockers. It removes the ⚠ on A6's tooling-specification column |
| A5 as a whole | `NP-FAI-L1-001` | **Named absence.** Blocked on `MECH-1` and `OI-SHELL2-02` |

---

## 10. EMF-1 — what this document can and cannot say

`EMF-1` is the prototype measurement: two-bowl attenuation must **meet or exceed the single-shell
baseline** (≥ 35–45 dB ELF magnetic, ≥ 40–60 dB RF; `NP-HEX-ZM-001` §5.6). Its test plan and
fixture are `NP-HEX-ZM-001` §5.6.1, with sweeps `EMF-1a`…`EMF-1f`. **It has never run**, and no
document can close it. It needs a first article, which needs this specification released, which
needs `MECH-1`.

GitHub #331 offered two ways to discharge the item: *"demonstrate two-layer attenuation meets or
beats the single-shell baseline, **or restate the claim**"*. The first waits on hardware. **The
second is done in this change, and only to the extent the record already requires.** CLAUDE.md §1
still stated as a founding principle that NeurOne is the *"only consumer brain wearable with
measured shielding"*. That is the unearned wording `NP-BIB-EMF-001` `OI-BIBEMF-01` identified, and
CLAUDE.md §4.3 in the same file forbids it (*"no dB figure may be published as measured"*).
`docs/reference/competitive-position.md` had already restated its copy line. **CLAUDE.md Rev 58
restates §1 to match §4.3.** The shielding is a design target, and the measured claim waits on
`EMF-1`. What the public claim becomes is still `OI-BIBEMF-01`'s, owned by Marketing and Regulatory.

---

## 11. Open items

| ID | Item | Owner | Blocking |
|---|---|---|---|
| **OI-TSHELL2-01** | **Deliver `MECH-1` geometry for S2-F03 … S2-F08** against §5, then add their §3 subsections and their §7 and §8 rows. This is the residual of `NP-ART-001` `OI-ART-01`, whose document half this revision discharges | ME Lead | Shell tooling release; `NP-FAI-SHELL-001`; `NP-FAI-L1-001` |
| **OI-TSHELL2-02** | **The layer-closed interlock (`NP-HEX-ZM-001` §5.5) has no electrical owner** (§5.2): no row in CLAUDE.md §4.2's interlock table, no safety-MCU input, no Hub PCB pin, no boss contact group and no `NP-RISK-004` hazard row for a session enabled with the bowls open. Decide (i) which tier owns it (§4.2 puts every stimulation enable line on the safety MCU), (ii) where its four signals cross the parting plane, boss or latches, and (iii) its hazard entry. **(ii) must be settled before `MECH-1`**, because a boss crossing needs contact positions in the `OI-SHELL2-02` layout | EE + Safety + ME | **MECH-1** (time-boxed); `OI-SHELL2-02` |
| **OI-TSHELL2-03** | **Two parts this document touches have no artifact row.** (a) **The blanking plug** (`NP-HELMET-GEOM-001` §3.1): no row in `NP-ART-001` §2 (A22 covers tethered covers, and the plug is neither), no tooling specification, and no decision on loss while unplugged (§4.2 (3)). (b) **The temporal stability wing**, which carries S2-F01: A24 (fit frame) names the bridge and Boa assembly, not the wing. Feed both to `NP-ART-001` `OI-ART-12` | Systems + ME | Register integrity; A19 mounting |
| ~~OI-TSHELL2-04~~ | *Not used.* Reserved at drafting for the station-name correction (§1.4). That correction was made in this change, in `NP-ART-001` Rev 9 and `NP-RISK-004` Rev 3, so no item was needed. Kept so the numbering stays append-only (`NP-CONV-001` §6) | — | — |
| **OI-TSHELL2-05** | Tooling-manufacturer DFM review (TS2-07). Carried from `NP-TOOL-SHELL-001` OI-02, now for S2-F01 and, when delivered, S2-F03 … S2-F08 | ME + Tooling | G2 |
| **OI-TSHELL2-06** | Temporal wing boss load analysis: ≥ 15 N axial, ≥ 10 N lateral (TS2-05). Carried from `NP-TOOL-SHELL-001` OI-05 | ME | G2 |
| **OI-TSHELL2-07** | Dust-cover tether anchor location on the wing, and **tether loop size from a reach analysis** rather than the retired F-01 loop (§3.1). Carried from `NP-TOOL-SHELL-001` OI-07, plus the F-04 half of its OI-06 (tether supply and ≥ 500-cycle tug test) | ME + Supply Chain | G2 |
| **OI-TSHELL2-08** | **HOPE Phase 3 (Cognito Therapeutics, n = 670) was "expected mid-2026" and is now overdue as a monitoring item** (`NP-TOOL-SHELL-001` Rev 2 banner note (b)). Check its status. This document supplies no result. If the result is negative, the boss stays and S2-F01 is marked inactive. Carried from `NP-TOOL-SHELL-001` OI-08 | Product / Programme | Accessory A19 decision; nothing in tooling |

**Items this document tracks but does not own:** `MECH-1`, `MECH-2`, `EMF-1`, `EMF-2`, `EMF-3`
(`NP-HEX-ZM-001` §7); `OI-SHELL2-02`; `OI-THCOOL-06`; `OI-TACSDRV-04`; `OI-UPG-05`; `OI-EMCCAV-08`;
RISK-20 / `OI-RISK4-05` / GitHub #6; `OI-BIBEMF-01`.

---

## 12. Cross-references

- **Architecture:** `docs/np_hex_zm_001.md` §5
- **Radial stack, materials, structural elements:** `docs/np_helmet_geom_001.md` §2–§4
- **Interconnect, boss contact groups:** `docs/np_drv_shell_002.md` §4
- **Design review record that gates first cut:** `docs/np_rev_shell_001.md`
- **Artifact register:** `docs/np_art_001.md` (A5, A6)
- **Risk register:** `docs/np_risk_004.md`
- **Cavity and the Layer 4 deletion:** `docs/np_emc_cav_001.md` §8
- **Predecessor:** `docs/superseded/np_tool_shell_001.docx`

---

## 13. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-09-25 | NeurOne Systems Engineering | **Initial issue (GitHub #331). Supersedes `NP-TOOL-SHELL-001` Rev 3**, under `NP-CONV-001` §4's new-architecture rule (§1.1). Dispositions all four predecessor features (§2): F-01, F-02 and F-03 not carried, and F-04 carried unchanged as S2-F01 with its dangling tether reference restated inline. Records that GitHub #331's *"carry forward the EEG cable channel"* is contradicted by the record. Answers the ~80-socket cover question from `NP-HELMET-GEOM-001` §3.1 and FMEA-G03-02: blanking plugs retained by the socket, with no per-socket anchor post (§4). Registers every `MECH-1` input (§5): six must close before `MECH-1` cuts, and one, **the layer-closed interlock's missing electrical owner**, is new (OI-TSHELL2-02). Resolves the "L0" station-name collision (§1.4). Records that RISK-20's scope may shrink, not grow (§5.6). Raises OI-TSHELL2-01…08. **Invents no geometry; shell tooling cannot be released from this revision.** |
