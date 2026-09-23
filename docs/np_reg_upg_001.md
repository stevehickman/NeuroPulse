# T1 → T2 Upgrade Path — Analysis and Recommendation

**Project:** NeurOne
**Document:** NP-REG-UPG-001
**Revision:** 1
**Date:** 2026-09-23
**Status:** DRAFT — **analysis and recommendation. Decides nothing.** The decision is `OI-TACSDRV-06`'s, owned by Product + Regulatory + ME, and the principal takes it (§8).
**Effective Date:** —
**Author:** NeurOne Systems Engineering
**Approved By:** Pending — principal decision required (§8)
**References:** CLAUDE.md §1, §2.1, §2.2, §4.1, §4.2, §5.1; `docs/reference/regulatory-strategy.md`; `docs/reference/service-network.md` §8.1, §8.2; `docs/reference/commercial-model.md` §2.1, §2.2; `docs/reference/hardware-detail.md` §4.3, §4.5; **NP-HW-TACSDRV-001 Rev 2 §4.1.1 (a)–(g), §9**; NP-HEX-ZM-001 §4a, §5.1, §5.2, §5.5, §5.6; NP-HELMET-GEOM-001 §2, §3.3; NP-DRV-SHELL-002 §3.5, §4.2, §4.3; NP-HW-HUB-001 §7.4 (`OI-HUB-C16`, `OI-HUB-C19`); NP-HW-EEGNET-001 §6.1, §6.2 (`REQ-NET-16`); NP-HW-TCAP-001 §5, §8; NP-HW-TMS-001 §2, §4; NP-HW-CVNS-001; NP-REG-CVNS-001 §1; NP-HW-HEXTILE-001 (scope note); NP-PWRSRC-001 §6.2, §6.7, §14.2, D-9; NP-THERM-COOL-001 Rev 6; NP-ENV-OPRANGE-001 §2; NP-COST-001 §2 (A-1, A-2); NP-BIB-EMF-001 §7.6; NP-REG-PBM1064-001 §1, §8; `firmware/hub_control/include/np_hub_types.h`; `app/web/src/lib/hubCompiler.ts`
**Related Issues:** `OI-TACSDRV-06` (the item this document answers); `OI-TACSDRV-01`; `OI-TACS-02`; GitHub Issue #5 (RISK-03 counsel engagement); GitHub Issue #332
**Gate:** NP-COORD-001 G2 (pre-tooling) — §5 names the provisions that must be settled before **MECH-1**
**IEC 62304 Class:** N/A (analysis record, not device software)
**Jurisdiction Scope:** US federal (FDA) as analysed; EU/EEA raised as an open question only (§6.5)
**Change Summary:** First issue. Answers `OI-TACSDRV-06` with an analysis and a recommendation, not a decision.

---

> **Why this document exists.** `NP-HW-TACSDRV-001` Rev 2 §4.1.1 (g) found that its siting
> recommendation had assumed, without saying so, that **T2 is always built as T2**. The record treats a
> T1 → T2 upgrade as something that will happen and never says how. That is `OI-TACSDRV-06`. It has
> three candidate answers — a **new unit** (trade-in or second purchase), a **service conversion** of
> the customer's T1 headset, or **user-installed parts** — and it reaches past the tACS driver to every
> T2 addition that lives in the shell or the hub.
>
> **This document is an engineering reading of the record. It is not a regulatory opinion.** Where it
> reaches a regulatory consequence it states it as a question, and §6.6 routes those questions to the
> counsel engagement that already exists rather than opening another one.

**Summary, for a reader who stops here:**

| | |
|---|---|
| **Recommendation** | **(a) New unit.** A T2 headset is built, tested, labelled and released as T2. A T1 owner reaches T2 by buying one, with trade-in as a commercial option. **No T1 unit is ever converted into a T2 device, by NeurOne, a partner or the user.** |
| **Why, in one line each** | Two T2 provisions cannot be added after manufacture at all: the PAN's ADS1299 bank is sized per tier (8 vs 21 channels) on the laminated L1 carrier, whose tails are formed once and never handled again, and the TMS window is laid up into an outer bowl that is never opened (§4). A conversion is manufacturing, not service, so no service-network tier can do it (§6.3). And a converted wellness unit is a medical device with a build history that began outside medical-device controls. Only (a) avoids that question entirely (§6). |
| **What it costs** | T1 carries no populated T2 content. It carries only the tooled features one production line gives it anyway. The "customer upgrading T1 → T2" becomes a customer buying a second device, and the commercial case for that is Product's to make (§7.2) |
| **Two things needed whatever is decided** | **The device holds no tier identity**, and three T2 parts fit interfaces every T1 unit has (§3.3, `OI-UPG-01`). **The regulatory questions in §6 go to counsel** under the existing engagement (`OI-UPG-02`) |
| **For `OI-TACSDRV-01`** | Under (a), **Q1 holds as written and §4.1.1 (e)1's saving holds in full. Q3's hub-side magnetics survive.** Q2 is unaffected and still waits on `OI-TACSDRV-04` (§7.3) |

---

## 1. Scope

**In scope:** the three upgrade models; every T2 addition in `CLAUDE.md` §3 that lives in or on the
headset or hub; for each model, what it requires of T1 hardware, of service and of regulatory
status; a recommendation.

**Out of scope, and why:**

| Not here | Why | Where |
|---|---|---|
| Any price, trade-in value or cost figure | `CLAUDE.md` §2.1: no figure may be quoted or acted on before `docs/np_cost_001.md` is read, and **no price may be set before `OI-HEXTILE-06`** (`OI-COST-10`). `NP-COST-001` was read for this analysis; **no figure from it is used**, because the recommendation does not depend on one | `NP-COST-001`; Product |
| A regulatory opinion | Counsel's, not Systems Engineering's | §6.6, `OI-UPG-02` |
| Upgrades **within** a tier (Core → Home Standard; fitting T1-C; Pro Entry → Pro Full) | Mostly unaffected. Tile additions within T1 are user operations already, and stay so. Pro Entry → Pro Full is discussed where it meets the TMS window (§4.2, §7.1) | §2.2 |
| Any new requirement | `CLAUDE.md` §18: the regulatory answers that would derive one do not exist yet. **This document states recommendations and open items and adds no `REQ-`** | §9 |

---

## 2. What the record actually assumes

### 2.1 The two citations are weaker than the item states

`OI-TACSDRV-06` cites two places that assume a T1 → T2 upgrade. Read against their sources, **neither
one requires a conversion:**

| Citation | What it says | What it needs from the upgrade model |
|---|---|---|
| `NP-THERM-COOL-001` Rev 6 (a); `NP-ENV-OPRANGE-001` §2 ‖ | T1's PBM block moved +38 → +35 °C *"so a customer upgrading T1 → T2 meets the same usage limit rather than a tighter one"*. The operative reason was consistency: *"one sentence now describes the thermal envelope of the entire product line"* | **Nothing.** A customer moving from a T1 to a T2 meets the same envelope whether the T2 is a new unit or a converted one |
| `commercial-model.md` §2.2 (locked) | Charger *"auto-included at every upgrade by serial number tracking"* | **Nothing specific to T1 → T2.** `NP-PWRSRC-001` §14.2 reads the rule as serving **configuration upgrades**: *"Fitting a T1-C module is a serial event."* Those upgrades exist inside T1 under any model. A new T2 unit ships with its own configuration's charger under the same rule |

So the record commits to **configuration upgrades**, which are user-installed tile additions under one
wellness status. It does **not** commit to converting a T1 into a T2. The question is open, not
half-answered.

### 2.2 Upgrades within a tier are already user operations, and that is not in question

`NP-HEX-ZM-001` §4a makes tiles type-agnostic and user-inserted, with software placement checking.
`NP-COST-001` §2 A-2 carries the full L1 carrier (all ~80 sockets, all 18 cluster controllers) in
every configuration, so Core → Home Lite → Home Standard adds tiles and changes no base hardware.
`CLAUDE.md` §3 sells T1-C as a snap-in upgrade. **None of this crosses the T1/T2 line.** This
document does not touch it.

---

## 3. Three facts from the record that decide most of the answer

### 3.1 Users already open the parting plane. What they never open is the outer bowl

`NP-HEX-ZM-001` §5.1: the two bowls *"clamp together for use and **separate for module
replacement**"*. The outer bowl carries the whole passive stack and *"is NEVER opened for a module
swap"*. The seam is protected by design for exactly that routine. §5.5 has a Hall/contact sensor on
each of the four latches, and *"the safety architecture refuses to enable any modality unless all four
report closed"*. §5.6 trends the ground-bond contact resistance in SHDR (System Health Data Record)
and makes the parting-plane gasket a replaceable service part.

**So "a conversion opens the shielded shell" is not what distinguishes a conversion.** Every T1 owner
separates the bowls whenever they swap a tile. `OI-TACSDRV-06` (iii) asks whether a conversion needs
its seams re-verified. That question turns on **which parts** the conversion touches:

| Interface class | Members | Who can work on it |
|---|---|---|
| **User-serviceable** | Tile sockets (via bowl separation); hub accessory ports; the lens; the tool-free hub fan | The user |
| **Factory-only** | The **outer bowl laminate** (L0/L2/L3 stack, Helmholtz formers, TMS window); the **L1 carrier lamination**, including the PAN and the 18 laminated cluster tails (*"formed once at assembly and never handled again"*, `NP-DRV-SHELL-002` §4.2); the **posterior blind-mate boss** (tooled at MECH-1, §4.3); the **Hub PCB** | Nobody outside a manufacturing process |

The aperture-limited finding of `NP-BIB-EMF-001` §7.6 bites at the boss and the outer bowl, not at the
routine seam. The boss is `OI-THCOOL-06`'s *"a hole in mu-metal is a hole"*, and the outer bowl is
where `RISK-20`'s rim roughness lives.

### 3.2 Tooled features are shared by construction. Populated parts are what vary

`CLAUDE.md` §1 runs *"one production line, two markets"*, and `NP-COST-001` §2 A-2 builds every
configuration on the same L1 carrier. **One mould makes one boss, one outer bowl shape and one hub
enclosure**, so any contact position or cavity that T2 needs exists physically on every T1 unit under
**every** upgrade model. That costs tooling once, not a part per T1 unit.

So "what must every T1 unit be provisioned with?" is a question about **populated components,
laminate schedules and factory-only assemblies**, and it has a different answer per model.

### 3.3 The device holds no tier identity, and "T2 only" is a hardware fact for only some T2 parts

`NP-PWRSRC-001` §6.2: *"Today 'T2 only' is a **hardware fact**. A T1 helmet has no TMS coil and no
1170 nm laser, so a T2 protocol fails the ordinary F1 (missing element) check. Enforcement is physical
— the hardware is simply not there, **which is why nothing reads the flag**."* The flag is
`NP_PROTO_FLAG_T2_TIER` (`np_hub_types.h`). `hubCompiler.ts` sets it when a protocol contains any T2
modality. **No firmware file reads it.** The web app holds the device tier as UI state.

§6.2's premise holds for parts that cannot reach a T1 unit. **It fails for the T2 parts that fit
interfaces every T1 unit has:**

| T2 part | T1 interface it fits | Is "not there" enforcement? |
|---|---|---|
| **T2-D** 1170 nm laser tile | Any socket. Tiles are type-agnostic and user-inserted (`NP-HEX-ZM-001` §4a) | **No**, if T2-D uses the standard socket interface. Unspecified: `NP-HW-HEXTILE-001`'s scope note puts T2-D *"out of scope entirely (laser drive ≠ LED drive)"* |
| **Cervical VNS** accessory (A14) | The hub accessory port. `CLAUDE.md` §3: *"connects via existing hub accessory port"* | **No** |
| **21-ch qEEG cap** | The PAN termination (`REQ-NET-16`), if the connector (`OI-TCAP-05`) is common to both tiers | **Depends on `OI-TCAP-05`** |

That is true under **every** upgrade model, including a new unit. T2 accessories exist in the world,
and a clinic may own both tiers. `NP-PWRSRC-001` D-9 (principal) already chose the mechanism for a
gate that hardware does not back: *"a device-bound signed entitlement"*, redeemed *"in the partition
that owns the enables"*. D-9 was decided for **F4 protocol** gating, not for **modality** gating. The
gap is **`OI-UPG-01`**. What the upgrade model decides is only **when the entitlement is issued**: at
manufacture under (a), at a depot under (b), or on purchase under (c).

---

## 4. Survey — every T2 addition that lives in the shell or hub

### 4.1 Where each one sits

| T2 addition | Where it lives | Interface class (§3.1) | Source |
|---|---|---|---|
| **A16 — 21-ch tACS driver** | Recommended at the **PAN** on L1, inside the shield; digital link, `NP_SAFETY_EN_CLIN_STIM` and a compliance-supply pair through the **boss**; supply magnetics on the **Hub PCB** | Factory-only (PAN, boss, hub) | `NP-HW-TACSDRV-001` §4.1.1 |
| **21-ch ADS1299 bank** (for 21-ch qEEG) | At the **PAN**. N4 is *"sized by channel count (8 T1 / 21 T2)"*. The ADS1299 is an 8-channel part, so T1's PAN carries **one** and T2's needs **three** | **Factory-only.** The PAN is part of the laminated L1 carrier | `NP-DRV-SHELL-002` §3.5, §4.2; `NP-HW-HUB-001` `OI-HUB-C16` closure |
| **21-ch qEEG / HD-tDCS cap** | Donned under the helmet. Its tail lands at the PAN, *"no pogo contact and no analog mux"*. The net lives inside the Faraday envelope (`NP-HW-EEGNET-001` §6.1) | User-attachable **if** its connector is reachable from the scalp side. **No connector is selected** (`OI-TCAP-05`), and nothing in the record puts an opening in the outer bowl for it | `REQ-NET-16`; `NP-HW-TCAP-001` §5, §8 |
| **T2-D — 1170 nm laser tile** | A socket | User-serviceable, **if** it fits the 19-contact socket. That is unspecified | `NP-HEX-ZM-001` §4a; `NP-HW-HEXTILE-001` scope note |
| **TMS — non-conductive CFRP window** | **In the outer-bowl laminate.** *"The CFRP L3 is broken by a non-conductive CFRP window at the coil site; local L2 mu-metal is routed around it"* | **Factory-only.** A laminate feature of the never-opened bowl | `NP-HELMET-GEOM-001` §2, §3.3; `hardware-detail.md` §4.3 D3 |
| **TMS — coil applicator (A15) and its supply** | Off the shell. Whether the 28 µF bank and driver are in the applicator is `OI-PWRTH-07`. The supply is ~176 W on a 240 W EPR contract, conditional on `OI-PWR-02` | Applicator: separate article. Supply: **a hub power-path property** | `NP-HW-TMS-001` §2, §4; `hardware-detail.md` §4.5 |
| **Cervical VNS (A14)** | Hub accessory port; separate cable and electrodes. Its cardiac interlock takes R-peaks from **A13's PPG**, a T1 accessory (`OI-CVNSHW-03`) | User-serviceable | `CLAUDE.md` §3; `NP-HW-CVNS-001` |
| **T2 power path** | Hub PD sink. The T2 peak row is 70–74 W (`CLAUDE.md` §4.5), and TMS adds the 240 W EPR case above | Factory-only (Hub PCB) | `hardware-detail.md` §4.5 |
| **T2 software** (sLORETA, FHIR R4, LSL, scripting API, HIPAA cloud) | App and cloud. Enabled on the device by the entitlement in §3.3 | Software | `CLAUDE.md` §3 |

### 4.2 Two findings the survey makes

**(i) The PAN is tier-specific before A16 is placed on it.** The 8-vs-21 ADS1299 bank is on the
laminated L1 carrier. So Pro Entry, which has no TMS, still cannot be reached by conversion without
reworking L1, unless every T1 PAN carries the 21-channel bank populated. `NP-HW-TACSDRV-001` §4.1.1 (g)
did not see this. It makes the PAN a conversion obstacle **with or without A16**.

**(ii) The TMS window cannot be retrofitted.** It is part of the outer-bowl layup, with L2 routed
around it, and that bowl is never opened. Under a conversion model, every bowl carries the window from
manufacture, or a Pro Full conversion replaces the outer bowl. Replacing the outer bowl replaces the
shielding envelope, the Helmholtz formers and the structural shell (`NP-HELMET-GEOM-001` §3.3: *"the
'never-opened' body"*). Putting the window in every bowl puts a routed-around discontinuity in T1's
L2 for a coil T1 never carries. `hardware-detail.md` §4.3 D3 records why that matters: *"Magnetic
shields leak at butt-joints."* **The record does not say whether T1 bowls are laid up with the window
or without it.** It marks the window "(T2)" and runs one production line (`OI-UPG-05`). **The same
question arises for Pro Entry → Pro Full**, which is a T2 → T2 upgrade.

---

## 5. The three models against the survey

### 5.1 What each model requires of every T1 unit

| T2 addition | (a) New unit | (b) Service conversion | (c) User-installed |
|---|---|---|---|
| A16 at the PAN | Nothing populated. Boss positions exist from the shared tool (§3.2) | A PAN mounting and connector for A16 on **every** L1; boss positions; a way to fit a board to the PAN of a laminated carrier. The record has no such procedure | **Not feasible.** The PAN is factory-only |
| 21-ch ADS1299 bank | Nothing (T1 builds one ADS1299) | **Every T1 PAN populated for 21 channels**, or L1 rework on conversion | **Not feasible** |
| qEEG cap connector | Nothing | The T2 cap connector on every T1 PAN | The same, **plus** user mating, if the connector is reachable from the scalp side |
| T2-D tile | Nothing (socket already common) | Nothing beyond the socket, **if** T2-D fits it | The same. The user inserts it like T1-C |
| TMS window | **T1 bowls laid up without it** (if §4.2 (ii)'s question is answered that way) | **Every T1 bowl laid up with it**, or the outer bowl replaced on conversion | **Not feasible** |
| TMS / T2 power path | Nothing | Every T1 hub built to T2's PD and EPR capability, or a hub swap | Hub swap = a new hub, which is (a) for the hub |
| A16 compliance supply (Q3) | On T2 hubs only | Every T1 hub carries it populated, or a hub swap as well as the PAN work: **two assemblies** | **Not feasible** |
| Cervical VNS | Nothing (port already common) | Nothing | Nothing. The user plugs it in |
| **Tier identity** (§3.3) | Issued at manufacture | Issued at the depot. The identity must be **writable after manufacture** | Issued on purchase. The identity must be **writable by a transaction the user starts** |

### 5.2 Service

| | (a) | (b) | (c) |
|---|---|---|---|
| Who does the work | Nobody. The factory builds a T2 | **Depot only.** §6.3: a conversion is manufacturing, so a Tier B partner doing it would become the manufacturer. `service-network.md` §8.1's Tier B task list has no such task and should not gain one | The user, for the three parts in §3.3 only |
| Seam re-verification | Factory acceptance, as for any unit (`NP-HEX-ZM-001` §5.6's prototype acceptance and `EMF-1` when it runs) | The routine seam is already covered by the latch interlock and the SHDR trend (§3.1). **What needs re-verification is the boss and, if touched, the outer bowl.** That means the depot needs `EMF-1`-class attenuation measurement, which has **never run on anything** (`CLAUDE.md` §4.3) | Unchanged from today's tile swap |
| Trade-in / returned units | Returned T1s are refurbished as T1 or scrapped. **The returned unit holds its user's UHDR partition**, and NeurOne never accesses UHDR (`CLAUDE.md` §5.1). The procedure must make it unreadable without the key: destroy the key, never decrypt (`OI-UPG-06`) | The same UHDR constraint, with the unit coming **back** to the user. The depot must convert without ever needing the UHDR key | — |

---

## 6. Regulatory consequences of each path

> This section answers the question *"what are the regulatory effects of each upgrade path?"*. It is an
> engineering reading against the record and general FDA practice, written so counsel can confirm or
> correct it. **It is not a regulatory opinion**, and §6.6 says where the questions go.

### 6.1 The starting point the record uses, stated precisely

`regulatory-strategy.md` calls T1 *"FDA-exempt wellness"*. That is shorthand. FDA's general wellness
policy is **enforcement discretion for low-risk products intended only for general wellness**, set
out in guidance. It is not a statutory exemption. Whether a product qualifies turns on **intended
use**, and intended use is read from labelling, promotion and the product's own features. **That is
why an upgrade path is a regulatory question at all:** each model is a different answer to *"when, and
by whose act, does a wellness product become a device intended for a medical purpose?"* T2 is a 510(k)
target on the predicates in `regulatory-strategy.md`. Its TMS predicates (NeuroStar, BrainsWay) are
prescription devices, and the record does not yet state T2's sales channel or prescription status
(§6.6 Q7).

### 6.2 (a) New unit

| Area | Consequence |
|---|---|
| **Device identity** | **One device per serial for life.** Every T2 is manufactured, finally accepted, labelled and UDI-marked under the QMS as the device the 510(k) describes. The question *"what is a converted unit?"* never arises |
| **T1 status** | T1 stays a wellness product for its whole life. **Its labelling and promotion must not present it as upgradeable to a medical device.** That presentation would itself be intended-use evidence (§6.6 Q1) |
| **Build records** | T1 needs no medical-device traceability on the grounds that it might become one. Whether T1 is built under the full QMS anyway is a separate choice and is not forced by the upgrade path |
| **Trade-in** | Returned T1s never enter a T2 build. Refurbishing a wellness product as a wellness product is not medical-device remanufacturing |
| **Carry-over of T1 parts** | **The one residual exposure.** If a T2 buyer keeps T1-released tiles, the A13 clip (which feeds A14's cardiac interlock, `OI-CVNSHW-03`) or the audio cups, those parts become components of a 510(k) device without having been released as such. That is the conversion question on a small scale (`OI-UPG-04`). The clean answer: a T2 ships complete with T2-released parts, and T1 parts carry over only if counsel says a common part number released under the QMS covers both |
| **Postmarket** | A clean split. Complaint handling, MDR reporting and corrections/removals attach to T2 serials, which were always T2 |
| **Q-Sub** | Nothing new to ask |

### 6.3 (b) Service conversion

| Area | Consequence |
|---|---|
| **What the act is** | **Manufacturing, not servicing.** FDA separates servicing, which returns a device to its specifications, from remanufacturing, which changes its performance, safety specifications or intended use. A conversion changes all three by construction. And the input is not yet a medical device, so the act is closer to building one from parts than to refurbishing one |
| **Who may do it** | Only an establishment operating as the manufacturer, under the QMS (21 CFR 820, which incorporates ISO 13485:2016 by reference since the QMSR took effect on 2026-02-02). In practice, **the depot**. Tier B partners (`service-network.md` §8.1) are excluded, whatever their training |
| **The 510(k)** | The cleared device description must cover converted units, and the submission has to show that a converted unit is equivalent to a factory-built T2. The conversion is a process whose output is not fully verifiable by inspection (seam integrity, shielding), so it needs **process validation** |
| **Build history of every T1** | **The heaviest consequence, and it falls on units that are never converted.** A converted unit's device history record starts with its T1 build. So every component that stays in the unit (outer bowl, L1 carrier, hub) must have DHR-grade traceability **from the day it was built as T1**. That is regulatory provisioning on every T1 unit, alongside the hardware provisioning in §5.1, and it lasts as long as any T1 in the field may still be converted |
| **Used-unit intake** | A T1 comes back with field history: impacts, compression set on the parting-plane gasket, its SHDR ground-bond trend. Conversion needs incoming acceptance criteria and a reject path. **The T2's shielding performance claim has to be established on that unit**, and the stack is aperture-limited (`NP-BIB-EMF-001` §7.6) |
| **Labelling and UDI** | A new label and a T2 device identifier, and the T1 identity retired. SHDR fleet models are *"version-stamped by hardware revision"* (`CLAUDE.md` §5.2), so the conversion also has to decide whether the SHDR device ID continues (`OI-UPG-03`) |
| **Postmarket** | A correction or removal against the T1 design can reach converted units, which are medical devices. Complaint files for a converted unit start before it was a device |
| **Q-Sub** | **Must be a Pre-Submission question** (the ~Month 20 Q-Sub in `regulatory-strategy.md`). FDA's view of a converted wellness unit should come before the provisioning in §5.1 is tooled, not after |

### 6.4 (c) User-installed parts

| Area | Consequence |
|---|---|
| **What the act is** | The person installing the parts **creates the medical device outside any manufacturing control**. There is no final acceptance, no device history record of the assembly, and the base unit is still labelled as a wellness product |
| **Could a 510(k) cover it?** | Only as a kit installed per the instructions for use, with installation validated as a **critical task** under IEC 62366-1 (`NP-HFE-001`'s summative programme). **Even then, (b)'s build-history problem remains:** the base unit is still a T1 built outside medical-device controls |
| **Reach** | **It cannot deliver T2 as currently defined.** A16, the 21-channel ADS1299 bank, the T2 power path and the TMS window are factory-only (§5.1). Only T2-D, the cap (if its connector is reachable) and cervical VNS are user-installable, and those three are exactly the parts §3.3 finds a T1 unit cannot refuse today |
| **Exposure** | The highest of the three models. The regulatory transition happens at a moment NeurOne does not witness. Among the parts it would move, T2-D is a laser, and cervical VNS has its own 510(k) on a prescription predicate (`NP-REG-CVNS-001` §1) |

### 6.5 EU/EEA — raised, not analysed

The record's only EU position is the charger note in `commercial-model.md` §2.2. **Two points look
material and are raised for counsel, not asserted:**

1. EU MDR (Regulation (EU) 2017/745) **Annex XVI** brings certain products without a medical purpose
   into scope, including brain-stimulation equipment that applies electrical currents or magnetic
   fields through the cranium. Implementing acts set common specifications for those products and,
   as understood here, a high risk class. **If that reading is right, T1 is not unregulated in the EU**,
   and the "wellness → medical" transition in §6.2–§6.4 has a different shape there.
2. MDR **Article 16** gives manufacturer obligations to anyone who modifies a device already on the
   market in a way that may affect its compliance. **A partner converting units in the EU would become
   the manufacturer**, which reinforces §6.3's "depot only".

Both are **`OI-UPG-02`** questions. Neither changes the recommendation, and both point the same way.

### 6.6 Where these questions go

`NP-REG-PBM1064-001` §1 is explicit: *"Do not initiate a parallel engagement; add this scope to the
existing instruction"* (RISK-03, GitHub Issue #5, `pending-decisions.md` §13.1). **The questions below
join that one engagement.** Q3–Q5 matter only if the principal rejects (a):

| # | Question for counsel |
|---|---|
| Q1 | Does a T1 built on a shared chassis, with tooled positions for T2 parts but none populated, or promoted as part of a platform with a cleared T2, put T1's general-wellness position at risk? |
| Q2 | Under (a), can T1-released parts (tiles, A13 clip, audio cups) carry over to a T2 unit, and on what conditions (common part numbers, QMS release)? |
| Q3 | Under (b), is converting a general-wellness unit into a 510(k) device something FDA will accept at all, and what must the submission show about converted units? |
| Q4 | Under (b), what build records must a T1 unit carry for its components to enter a T2 device history record later? |
| Q5 | Under (c), can a 510(k) cover user installation of T2 parts onto a unit sold as a wellness product? |
| Q6 | EU: §6.5 points 1 and 2 |
| Q7 | Is T2 prescription-use, and through what channel is it sold? The answer decides whether "a customer upgrading T1 → T2" is even the same customer |

---

## 7. Recommendation — **RECOMMENDED, not decided**

**Status: RECOMMENDED, not decided.** The same pattern as `NP-EMC-CAV-001` `REQ-CAV-04` and
`NP-HW-TACSDRV-001` §4.1.1: the analysis is done, and the decision is Product + Regulatory + ME's for
the principal to take. **No locked decision, price, cost figure, firmware constant or requirement
changes here.**

### 7.1 The recommendation

1. **T1 → T2 is (a), a new unit.** Trade-in is a commercial option, not a manufacturing route. **No T1
   unit is converted**, by the depot, a partner or the user.
2. **T1 carries no populated T2 content.** It carries only what the shared tooling makes anyway (§3.2):
   boss contact positions, socket geometry, hub enclosure features. It carries no A16 mounting or
   connector, no 21-channel ADS1299 population, no T2 cap connector and no compliance supply.
3. **The TMS window is a T2 layup feature.** T1 outer bowls are laid up without it, keeping L2
   continuous. **Every T2 outer bowl carries it**, Pro Entry included, so that **Pro Entry → Pro Full**
   stays an addition of the applicator, its supply and software to a device already built as T2, not
   a new bowl. That depends on the Pro Entry hub's power path. If TMS needs a T2 hub variant
   (`OI-PWR-02`, `OI-PWRTH-07`), Pro Entry → Pro Full is a hub swap, which is a depot task inside one
   cleared device family, not a wellness conversion. **Confirming that the window is a layup choice and
   not a mould feature is `OI-UPG-05`.**
4. **The device's tier is issued at manufacture** as the device-bound signed entitlement
   `NP-PWRSRC-001` D-9 already chose for F4, extended to modality gating (`OI-UPG-01`). It is needed
   under every model. Under (a) it is written once.
5. **The §6.6 questions go to counsel** under the existing RISK-03 engagement (`OI-UPG-02`). Q1, Q2,
   Q6 and Q7 bear on (a) as well.

### 7.2 Why, and what it costs

**For it:**
- **Two provisions cannot be added after manufacture** (§4.2). A conversion model therefore forces
  populated T2 content into every T1 unit, or reworks factory-only assemblies at the depot.
  `CLAUDE.md` §2.1 records every T1 configuration as gross-margin negative, so the first path loads
  cost where the product can least carry it. No figure is stated: `NP-COST-001` cannot price A16,
  its bank or the window, and §1 explains why none is needed.
- **Every T1 unit would carry regulatory provisioning too** (§6.3, build history), not just hardware.
- **It is the only model with no regulatory transition in the field** (§6.2).
- **It keeps T1's shielding simpler**: no window discontinuity in L2 (§4.2 (ii)).
- **It needs no new service capability.** Under (b) the depot would need `EMF-1`-class measurement,
  and `EMF-1` has never run on anything.

**Against it, stated rather than dismissed:**
- **The "customer upgrading T1 → T2" buys a second device.** Whether that is commercially acceptable
  is Product's call. Q7 may make it moot: if T2 is prescription-use and sold to clinics, the T2 owner
  is usually not the T1 owner.
- **Trade-in needs a disposition route** for returned T1s, and a UHDR-safe one (`OI-UPG-06`).
- **The two assuming documents** (§2.1) need no edit, because neither requires conversion. The
  `OI-TACSDRV-06` row's phrase *"the cap connector's shell opening"* assumes an outer-bowl opening that
  nothing in the record puts there (§4.1). It is corrected in `NP-HW-TACSDRV-001` Rev 3.

**What would overturn it:** counsel answering Q3 favourably **and** Product showing that conversion is
worth provisioning every T1 unit for. Both would have to come before **MECH-1**, because the boss and
the outer-bowl layup are fixed there.

### 7.3 What this means for `OI-TACSDRV-01`

| Question | Under the recommendation (a) | If the principal chooses (b) instead |
|---|---|---|
| **Q1: own T2-only board** | **Holds as written, and §4.1.1 (e)1's saving holds in full.** T1 carries no A16 content. It does not even carry a populated connector footprint, only the boss positions the shared tool makes | Holds, with the reduced saving §4.1.1 (g) describes, **plus** the 21-channel ADS1299 population §4.2 (i) adds to every T1 PAN |
| **Q2: sources at the PAN** | **Unaffected.** Still conditional on `OI-TACSDRV-04` (PAN thermal) before MECH-1 | Unaffected in principle. The depot would also need a procedure to fit a board to a laminated carrier, which does not exist |
| **Q3: compliance-supply magnetics on the Hub PCB** | **Survives.** They are populated on T2 hubs only. A T2 hub is a T2 population of A9, a smaller variant than the whole stage (e)1 kept off A9 | **Weakens as (g) said.** Every T1 hub carries the supply populated, or the conversion touches two assemblies (PAN and hub) |
| **`OI-TACSDRV-01`'s dependency on `-06`** | **Discharged if the principal takes (a).** `OI-TACSDRV-01` then waits only on the principal's call on §4.1.1 and on `OI-TACSDRV-04` | Stays live. Q3 should then be re-analysed |

---

## 8. What remains the principal's

- **The model** — (a), (b) or (c). This document recommends (a). `OI-TACSDRV-06` stays **OPEN** until
  it is taken.
- **Whether T1 bowls omit the TMS window**, if `OI-UPG-05` confirms it is a layup choice.
- **Whether T1 parts carry over to T2**, after counsel answers Q2.

---

## 9. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-UPG-01** | **The device holds no tier identity, and three T2 parts fit interfaces every T1 unit has** (§3.3). `NP_PROTO_FLAG_T2_TIER` is set by `hubCompiler.ts` and read by no firmware. `NP-PWRSRC-001` §6.2's premise (*"'T2 only' is a hardware fact"*) holds for A16, the window and the T2 power path, and **fails for T2-D, cervical VNS and possibly the qEEG cap**. D-9 (principal) already chose a device-bound signed entitlement for F4 protocol gating. The item is to **extend it to T2 modality gating**, enforced where the enables are owned (`CLAUDE.md` §4.2). **Needed under every upgrade model.** The model decides only when the entitlement is issued. Not raised as a requirement: what it must refuse, and how strictly, depends on `OI-UPG-02`'s answers (`CLAUDE.md` §18) | FW + Regulatory | T2-D and cervical VNS release; `OI-UPG-02` |
| **OI-UPG-02** | **Put §6.6's questions (Q1–Q7) to counsel under the existing RISK-03 engagement** (GitHub Issue #5; `NP-REG-PBM1064-001` §1 forbids a parallel one). Q1, Q2, Q6 and Q7 bear on the recommended model. Q3–Q5 matter only if it is rejected. Q6 includes whether EU MDR Annex XVI puts T1 in scope in the EU, which reaches beyond the upgrade question | Regulatory + CEO | `OI-TACSDRV-06`; T1 EU market entry |
| **OI-UPG-03** | **SHDR device identity across a tier change.** Only live if (b) is chosen. Does a converted unit keep its SHDR device ID, given fleet models are version-stamped by hardware revision (`CLAUDE.md` §5.2)? | FW + Data | Only under (b) |
| **OI-UPG-04** | **Carry-over of T1-released parts to a T2 unit** (§6.2): tiles, the A13 clip that feeds A14's cardiac interlock (`OI-CVNSHW-03`), audio cups, lens. Until counsel answers Q2, T2 ships complete with T2-released parts | Product + Regulatory + Quality | T2 box contents |
| **OI-UPG-05** | **Is the TMS window a layup choice or a mould feature, and do T1 bowls carry it?** `NP-HELMET-GEOM-001` marks it "(T2)" and the line is shared. If it is a layup choice, the recommendation is: omit it on T1, carry it on every T2. If it is a mould feature, every bowl has the geometry and the question becomes whether T1's layup still keeps L2 continuous across it. **Time-boxed to MECH-1**, which tools the outer bowl | ME | **MECH-1**; `NP-TOOL-SHELL-001` |
| **OI-UPG-06** | **Trade-in disposition.** A returned T1 holds its user's UHDR partition, and NeurOne never accesses UHDR (`CLAUDE.md` §5.1). Specify a user-initiated key destruction before return, and a refurbish-as-T1 or scrap route that never needs the key | Privacy + Service | Trade-in offer |

> Items owned elsewhere and deliberately not duplicated: **`OI-TACSDRV-06`** (the decision itself;
> `NP-HW-TACSDRV-001` §9), **`OI-TACSDRV-01`** and **`-04`** (siting and PAN thermal), **`OI-TCAP-05`**
> (the cap connector), **`OI-PWR-02`** and **`OI-PWRTH-07`** (TMS supply and applicator
> architecture), **`OI-CVNSHW-03`** (A14's dependence on A13), **`OI-COST-10`** (no price before
> `OI-HEXTILE-06`).

---

## 10. Cross-references

- **The item and the siting analysis it gates:** `docs/np_hw_tacsdrv_001.md` §4.1.1, §9
- **Bowl separation, latch interlock, seam monitoring:** `docs/np_hex_zm_001.md` §5.1–§5.6
- **TMS window in the outer bowl:** `docs/np_helmet_geom_001.md` §2, §3.3
- **PAN, N4 sizing, the boss:** `docs/np_drv_shell_002.md` §3.5, §4.2, §4.3
- **Tier gating without hardware; D-9:** `docs/np_pwrsrc_001.md` §6.2, §6.7
- **Aperture-limited shielding:** `docs/np_bib_emf_001.md` §7.6
- **Regulatory strategy:** `docs/reference/regulatory-strategy.md`
- **Service tiers:** `docs/reference/service-network.md` §8.1
- **Cost (read, not used):** `docs/np_cost_001.md`

---

## 11. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-09-23 | NeurOne Systems Engineering | Initial release, answering **`OI-TACSDRV-06`** with an analysis and a recommendation, **not a decision**. **Recommends (a), a new unit:** T2 is built as T2, and no T1 unit is converted by anyone. **Four findings the item's framing did not have.** (1) **Neither citation that "assumes an upgrade" requires a conversion.** The thermal alignment holds under any model, and the charger rule serves configuration upgrades inside a tier (§2.1). (2) **Users already open the parting plane** for every tile swap, with a latch interlock and SHDR seam trending. So a conversion is distinguished by touching **factory-only** assemblies, the boss and the outer bowl, not the routine seam (§3.1). (3) **The PAN is tier-specific before A16 arrives.** Its ADS1299 bank is 8 channels on T1 and 21 on T2, on the laminated L1 carrier. And **the TMS window is laid up into the never-opened outer bowl**, so neither can be added after manufacture (§4.2). (4) **The device holds no tier identity**: `NP_PROTO_FLAG_T2_TIER` is read by no firmware, and T2-D, cervical VNS and possibly the qEEG cap fit interfaces every T1 unit has. `NP-PWRSRC-001` D-9's entitlement is the mechanism, and it needs extending (§3.3). **§6 sets out the regulatory consequences of each path** as an engineering reading, not an opinion. The heaviest consequence of conversion falls on units that are never converted: each T1 would need DHR-grade build history. §6.5 raises EU MDR Annex XVI and Article 16 for counsel. For **`OI-TACSDRV-01`**: under (a), Q1 holds with its saving in full, Q3 survives, and Q2 is unaffected (§7.3). Raises **OI-UPG-01…06**. **No price, cost figure, requirement, firmware constant or locked decision changes.** |
