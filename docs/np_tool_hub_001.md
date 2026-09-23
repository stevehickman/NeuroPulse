# Hub Enclosure Tooling Specification

**Project:** NeurOne
**Document:** NP-TOOL-HUB-001
**Revision:** 3
**Date:** 2026-09-23
**Status:** BASELINED — **F-01's probe-contact geometry is GATED on `NP-HW-NASAL-001` OI-NASAL-09 (Rev 2; see F-01)**
**Effective Date:** 2026-07-27
**Author:** NeurOne Mechanical Engineering
**Approved By:** Steve Hickman, CEO
**References:** CLAUDE.md §2.1, §2.3, §3 (PBM Intranasal), §4.1, §4.3, §4.4, §4.5, §4.7, §5.2; NP-HW-HUB-001 Rev 2; NP-DRV-SHELL-001 Rev 2 §2.4.5 (locked EEG/Hub routing); NP-REQ-FANHEALTH-001 Rev 1 (SR-FAN-01…06); NP-THERM-CFD-R1-001 Rev 1 §5 (BN-boss conductive export); NP-FMEA-GEOM-001 Rev 1 (FMEA-G07-01 / RISK-26); NP-TOOL-ZM-001 Rev 1 (mold design review checklist pattern); NP-TOOL-SHELL-001 Rev 3 (anchor post / tether pattern; its F-02 retired in favour of this document's F-02 — `OI-ART-07`, closed 2026-09-23); NP-TOOL-LENS-001 Rev 2 (captive-fastener / loss-prevention pattern); NP-DP-001 §6.3 (Hub tooling design review, Month 4); docs/status/pending-decisions.md §13.4
**Related Issues:** N/A — closes the `docs/status/pending-decisions.md` §13.4 item "Hub tooling: probe dock + anchor posts + large-radius Boa cable channel + tool-free fan (quarter-turn captive fastener)"
**Gate:** NP-COORD-001 G1 (Hub tooling design review, per NP-DP-001 §6.3 Phase 1 Month 4 milestone)
**IEC 62304 Class:** N/A
**Supersedes:** None
**Parent Document:** None — this document establishes the base Hub enclosure tooling spec (parallel in status to NP-TOOL-ZM-001 for the zone module and NP-TOOL-SHELL-001 for the headset shell)

---

## 1. Scope

This document specifies the four mandatory mechanical/tooling features of the NeurOne control hub enclosure identified in `docs/status/pending-decisions.md` §13.4 and `NP-DP-001` §6.3:

1. **F-01 — Intranasal probe dock:** on-hub storage cradle for the bilateral Y-probe intranasal applicator.
2. **F-02 — Accessory anchor posts:** tethered-cover anchor points for the hub's own service/charge ports.
3. **F-03 — Large-radius Boa cable channel:** a generous-bend-radius, PTFE-lined continuation of the occipital Boa lace path through the hub housing.
4. **F-04 — Tool-free fan/heatsink access door:** a quarter-turn captive-fastener service door over the fan-cooled heatsink assembly.

This is a **tooling specification**, not a PCB or firmware document — it governs the molded/machined hub enclosure only. Electrical content of the hub is specified in `NP-HW-HUB-001`; hub application firmware in `NP-FW-HUB-001`; the fan-health safety interlock in `NP-REQ-FANHEALTH-001`. This document does not re-derive any of those; it specifies the mechanical accommodations the enclosure must provide for them.

**Out of scope:** hub PCB layout, EMF shielding stack-up, thermal/CFD analysis of the fan-cooled heatsink, and the safety interlock logic that governs fan-loss behavior — all addressed in the referenced documents.

---

## 2. Background — Hub Placement Assumption

No prior document fixes the hub enclosure's physical location or its mechanical interface to the headset shell. This specification adopts a placement consistent with three already-locked or already-adopted decisions, rather than inventing a new one:

1. **EEG cable routing is locked from the hub PCB.** `NP-DRV-SHELL-001` Rev 2 (2026-05-10, `completed-decisions.md`) routes the EEG harness "Hub PCB EEG connector → occipital arch main trunk (~120mm) → crown junction → left/right branches." The hub PCB is therefore already anchored, by an existing locked routing decision, at or immediately adjacent to the headset's **occipital arch** — the same region occupied by the Boa occipital dial (CLAUDE.md §4.4).
2. **Antennas live in the hub, not the shell** (CLAUDE.md §4.1), which is only meaningful if the hub sits outside (or at the boundary of) the 4-layer shielded shell envelope described in CLAUDE.md §4.3 (5-layer until the Layer 4 absorber was deleted 2026-09-23).
3. **The BN-boss thermal export path terminates at an external fan-cooled heatsink**, deliberately kept outside the sealed/shielded interior to preserve the shield/IP moat (`NP-THERM-CFD-R1-001` §5). "The hub fan" is the fan referenced by `NP-REQ-FANHEALTH-001` §1 ("depends on forced convection (the hub fan; RPM already logged to SHDR per CLAUDE.md §5.1)") — i.e., the hub enclosure is the physical home of this fan-cooled heatsink.

Taken together, these three facts place the hub enclosure **externally mounted at the headset's occipital arch, outside the shielded shell envelope**, adjacent to the Boa dial. This document treats that placement as the working assumption for all four features below. The exact hub-to-shell mechanical interface (mounting method, cable entry/strain relief, EMI bonding at the shell boundary) is not yet CAD-locked; see OI-HTOOL-01.

---

## 3. New Features

### F-01 — Intranasal Probe Dock

**Purpose:** closes CLAUDE.md §3 (PBM Intranasal) — "Hub dock storage (in hub tooling from day one — prevents Y-junction fracture)." The bilateral Y-probe (15/20/25mm silicone depth-stop rings, probe-tip photodiode dose sensing, optical-code + pogo-pin sleeve authentication) must have a storage position that does not cantilever load onto the silicone-overmolded Y-junction when the probe is not in use.

**Geometry:**
- A molded cradle on the hub housing exterior with **two bilateral probe-tip receptacles** and a **central Y-junction saddle**. The saddle radius is sized to the Y-junction body OD (not the thinner bilateral leads), so the junction bears its own weight in storage rather than the leads.
- Each probe-tip receptacle carries a **silicone insert pad** (Shore 30–40A, same family as the electrode pod mounts, CLAUDE.md §4.4) at the point of probe-tip contact, protecting the probe-tip PD window and the optical-code/pogo-pin authentication contacts from abrasion.
- Retention: light snap-detent, ≤2N insertion/extraction force (a passive holster, not a keyed slot — no mis-insertion hazard exists here since both probe tips are identical and either may seat in either receptacle).
- The dock is a fixed molded feature of the hub housing, not a separate accessory — it ships in place from day one, consistent with CLAUDE.md's box-contents philosophy of parity across configurations.

**Material:** same CFRP-filled polymer as the hub housing shell; silicone inserts co-molded or bonded per the same 3M 9088 double-sided tape process used for zone-module rigidizer retention (`NP-TOOL-ZM-SM-001` §3, F-SM-01).

> **⚠ Gate, 2026-09-23 (Rev 2) — F-01's geometry is stated against a part no document dimensions, and is now marked so.** The saddle radius above is *"sized to the Y-junction body OD"*; the insert pads sit at *"the probe-tip PD window and the optical-code/pogo-pin authentication contacts"*; the receptacles hold *"probe tips"*. **`NP-HW-NASAL-001` — the owning specification for the probe, artifact A12, first issued 2026-09-20 — dimensions none of the four**: Y-junction body OD, probe-tip envelope, PD-window location and finish, authentication-contact location (`REQ-NASAL-03`). This document was baselined with each of them as a relation to a part rather than a value, so every F-01 feature that touches the probe had a *rule* and no *number*. That is the same shape as F-03's *"≥ 12 × lace OD"* — a multiple without a multiplicand — and it is recorded the same way: **gated, not fabricated** (`NP-FAI-001` §2 F2).
>
> **What is gated:** `HUB-MDR-01`, `HUB-MDR-02` (placement only — the bonding process is not), `HUB-MDR-03` and `FAI-HTOOL-01` (§4, §5), and `NP-FAI-HUB-001` FAI-HUB-09, -10 and -20, all on **`NP-HW-NASAL-001` OI-NASAL-09**, which owns the four values. **What is not:** the dock's *existence*, its placement on the hub exterior, its material, the pad durometer, the bonding process and the passive-holster (non-keyed) principle are all independent of the probe's dimensions and stand as written. **No F-01 value is changed by this gate** — the ≤ 2 N detent's own provenance is a separate question, already open under `NP-CONV-001` OI-CONV-08 (c), and nothing here answers it.
>
> **Consequence for steel:** `OI-HTOOL-06` gates the hub mould steel cut on §4 being fully signed, and three §4 items are now unsignable until OI-NASAL-09 closes. **The hub mould therefore cannot be cut on this revision.** `OI-HTOOL-07` records the one design change that would release it without waiting for the probe — **recommended, not executed**.

### F-02 — Accessory Anchor Posts (Hub Port Covers)

**Purpose:** the hub carries its own service/charge ports (USB-C charge/data, and a DFU/service access port).

> **⚠ Correction, 2026-08-18 — the back-reference this paragraph used to carry was wrong in both halves, and it concealed a live conflict.** It read that these ports are "distinct from the headset's zone-slot and lens-rim accessory ports already covered by `NP-TOOL-SHELL-001` F-02." `NP-TOOL-SHELL-001` F-02 is **not** on the headset — §2.2 places its three anchor posts "on the control hub shell" for the USB-C port and the left and right hub accessory ports — and lens-rim guards are explicitly out of scope there, belonging to `NP-TOOL-LENS-001` F-07. **The two documents therefore specify the same hub port covers with incompatible geometry**: Ø 4.0 mm × 3.5 mm bosses and a **50 mm** tether there, against 1.0 mm × 0.5 mm bosses and a **≤20 mm** tether here. This document's limit is the safety-derived one — it exists so a tethered cover cannot reach the F-04 fan intake grille (`FAI-HTOOL-02`, BLOCKING) — and a 50 mm tether defeats it. **Treat this document's geometry as governing** pending **OI-ART-07**, which owns the ownership decision. Per CLAUDE.md §2.3, "Interface protection covers (complete kit) ... All tethered — loss prevention by design"; this feature extends that principle to the hub's own ports.

> **✅ Resolved 2026-09-23 — `OI-ART-07` closed: this document owns every tethered cover on the hub, and `NP-TOOL-SHELL-001` F-02 is retired (Rev 3, in place, not deleted).** Three grounds, any one sufficient. (1) **Scope:** SHELL §1 lists "control hub tooling" as *out of scope* of its own document, and its §9 table says hub anchor posts need "a separate document" — this one. Its F-02 was hub geometry written into a shell tooling spec that excluded the hub. (2) **Hazard control:** the ≤ 20 mm free-length maximum below is the control for `RISK-HUB-01` (`NP-RISK-004`) and is verified by `FAI-HUB-23` [BLOCKING]; SHELL has no fan in its frame of reference and no reach limit. (3) **Status:** this document is BASELINED and has an issued FAI checklist (`NP-FAI-HUB-001`); SHELL is releasable for F-04 only. The back-reference the correction note above describes is already corrected — the purpose paragraph no longer carries it — and nothing further is owed there.
>
> **One correction to the magnitude the notes above repeat.** SHELL's tether is a loop of **50 mm nominal *circumference*** (SHELL §2.2), not a 50 mm free length. A loop spans at most about half its circumference between two anchors, so its reach is **≈ 25 mm, not 50 mm** — it exceeds this document's ≤ 20 mm by about **1.25×, not 2.5×**. The conclusion stands (it exceeds a hazard-control maximum, which is disqualifying at any ratio); the "2.5×" in the historical records dated 2026-08-18 overstates it.
>
> **What the retirement does NOT settle — and why this feature's port list is not taken as given.** Of the two documents, SHELL named the right ports and HUB named the right geometry. **This document's port list has no source:** no other specification describes a separate DFU/service port on the hub — the bootloader performs DFU **over USB-C** (`firmware/bootloader/src/np_dfu.c`, "USB-C DFU Recovery", `NP-FW-EMMC-001` §8.4) — while it **omits the hub accessory port(s)** that four accessory specifications connect to (`NP-HW-NASAL-001` A12.7, `NP-HW-VNSCLIP-001` §4 and `NP-FW-HUB-001` §8.4 "accessory-port impedance", `NP-HW-CVNS-001` A14.4/A14.5, `NP-HW-AUDIO-001` §8), whose EMF filtering is §4.3 Layer 5, and whose covers are a shipping SKU ("accessory port covers, 3 per headset, magnetic", `docs/reference/service-network.md` §8.3; `NP-ACC-PRIORITY-001` rank 11). Retiring SHELL F-02 must not orphan those covers, so **the port inventory is raised as `OI-HTOOL-08`** rather than corrected by invention: HUB-MDR-04 and `FAI-HUB-14` count two posts at USB-C and DFU/service until it closes, and **the ≤ 20 mm maximum and the reach sweep apply to every tethered cover the hub ends up carrying**, whatever the count. The boss dimension below is raised separately as `OI-HTOOL-09` (its stated provenance is an alignment boss, not a tether anchor).

**Geometry:**
- **Two anchor posts**, one per hub port (USB-C port cover, DFU/service port cover), each a molded boss (1.0mm diameter, 0.5mm protrusion — same boss dimension used for the smart-module rigidizer alignment bosses, `NP-TOOL-ZM-SM-001` F-SM-01) that captures a silicone tether loop molded into each port cover.
- **Tether length ≤ 20mm free length** — shorter than the lens rim-guard tether (45mm, `NP-TOOL-LENS-001` F-07) because the hub tether has no field-of-view constraint to satisfy, but a hard maximum is still required so a detached-but-tethered cover **cannot reach the F-04 fan intake grille** (§3, F-04) — an untethered-length foreign object drawn toward a rotating fan blade is a distinct hazard from the FOV-crossing concern that bounds the lens tether. This is the governing constraint on tether length, not cosmetics.
- Anchor posts and covers are not color-coded — the hub has no port-position ambiguity to disambiguate. *(This bullet previously contrasted against "the shell's zone-slot plugs, `NP-TOOL-SHELL-001` F-01". Those are retired — `NP-HEX-ZM-001` §3.4/§4a replaced the five colour-coded zone slots with ~80 identical sockets carrying no zone identity — so the contrast no longer names anything. The requirement itself is unchanged.)*

**Material:** same silicone family as the co-molded gasket groove material used on zone modules (Shore 40–50A, `NP-TOOL-ZM-SM-001` §2), for compression-fit sealing at the port opening in addition to tether capture.

### F-03 — Large-Radius Boa Cable Channel

**Purpose:** the Boa occipital dial's lace is already specified at CLAUDE.md §4.4 as a 50,000-cycle-rated, PTFE-lined, enclosed cable channel running through the headset shell. Because the hub sits at the occipital arch (§2) — the same region the Boa lace passes through — the hub housing itself forms a segment of that channel. A tight bend radius at the hub-to-shell transition would concentrate cyclic fatigue exactly at the one point in the path that is hardest to inspect or replace (inside the hub, not the field-replaceable shell segment covered by the in-box spare cable + hook tool).

**Geometry:**
- The channel through the hub housing continues the shell's enclosed, PTFE-lined cross-section without discontinuity at the shell/hub interface.
- **Minimum bend radius ≥ 12× nominal Boa lace OD** at every turn within the hub housing — a conservative multiple for a 50,000-cycle wire-lace duty cycle, consistent with the shell channel's existing cycle rating. The exact lace OD is a Boa-supplier reel/lace datasheet value not yet on file; see OI-HTOOL-02 for the resulting exact radius once the datasheet is obtained.
- No sharp (< 90°) turns are permitted within the hub housing segment; where the channel must change plane (e.g., routing around the fan/heatsink cavity, F-04), the transition uses a swept arc, not a corner.
- The channel remains mechanically and thermally isolated from the F-04 fan/heatsink cavity — the PTFE liner and the lace it carries must not be exposed to the heatsink's elevated surface temperature or to fan-drawn airflow (dust ingress into the liner would defeat its purpose).

**Material:** PTFE liner within the CFRP-filled polymer channel wall, matching the shell channel spec (CLAUDE.md §4.4) — same supplier and process, no new qualification required.

### F-04 — Tool-Free Fan/Heatsink Access Door

**Purpose:** the hub fan is safety-relevant — its loss is FMEA-G07-01 / **RISK-26** (fan/heatsink loss → scalp face > 42°C), mitigated by the SW01-M04 face-NTC interlock (`NP-REQ-FANHEALTH-001` Path B1) that derates PBM duty on fan-health loss, and by the SR-FAN-05 predictive-maintenance alert that is meant to catch degradation *before* the safety derate engages. A door that lets the user clear dust/lint/hair from the fan and heatsink fins without tools directly supports the SR-FAN-05 intent: most fan-health degradation in a consumer environment is foulable and user-clearable, and giving the user no way to act on the predictive-maintenance alert would make it useless.

> **⚠ Note added 2026-09-08 — `NP-THERM-SINK-001` removes this feature's stated safety rationale, and does not
> remove the feature.** The paragraph above justifies F-04 by `FMEA-G07-01` / `RISK-26` ("fan/heatsink loss → scalp
> face > 42 °C") and by `SR-FAN-05` catching foulable degradation before the safety derate engages. That chain
> assumes the hub fan is in the **tile** export path. It is not: the BN-boss via terminates on the outer bowl ~32 mm
> out, a median 188 mm from this enclosure, and `SPEC-SINK-03` finds `R_sink` **unchanged** by fan loss (§2.3, §11 of
> that document). What does raise the outward resistance is **vault occlusion**, which no fan-RPM signal observes.
> **F-04 still earns its place** — the hub sink it serves is real, it carries the hub's own electronics load
> (`OI-HUB-C19`, now decoupled from the tile field), and a user-clearable dust path for a fan is good practice — but
> the *scalp-safety* framing above overstates what cleaning this fan protects. §2's placement inference is likewise
> only weakened in its third leg: the EEG-routing and antenna arguments stand, the "physical home of this fan-cooled
> heatsink" argument does not. Correction routed to the owning files as `OI-SINK-04`; **nothing in this
> specification's geometry, checklist or FAI items changes.**

**Geometry:**
- A single access door over the fan + heatsink cavity, opened by **quarter-turn (90°) captive fasteners** (thumb-turn or coin-slot head, no tool required beyond a coin/thumbnail) — captive so the fastener cannot be removed from the door or lost, consistent with the loss-prevention principle applied to F-02's tethered covers.
- **Two fasteners, diagonally opposed**, sized so the door cannot be removed with only one released (prevents partial opening under vibration or accidental contact).
- Behind the door, the fan intake/exhaust grille uses **louvre slot width ≤ 6mm**, sized to the IEC 60529/60601-1 finger-probe test dimension so a finger cannot reach the moving fan blade with the door open — the door is a service access point, not a substitute for blade guarding.
- The door does not need to be opened for normal use; it exists solely for periodic user-performed cleaning (compressed air or soft brush — no proprietary consumable is specified; see §6, no new consumable SKU is required).
- Gasket/ingress sealing for the closed door is **not yet specified** — the hub's overall environmental rating has not been set in any prior document (CLAUDE.md's IPX4 references are scoped to the zone-module swap connector, not the hub enclosure). See OI-HTOOL-03.

**Material:** same CFRP-filled polymer as the hub housing; captive fastener hardware per a standard 1/4-turn quarter-turn fastener line (e.g., Southco-equivalent), not a bespoke part.

---

## 4. Hub Enclosure Mold/Tooling Design Review Checklist

| Item | Description | Status |
|------|-------------|--------|
| HUB-MDR-01 | Probe dock Y-junction saddle radius sized to Y-junction body OD (not lead OD) — CAD verification | Open — **GATED: `NP-HW-NASAL-001` OI-NASAL-09** (no Y-junction OD exists) |
| HUB-MDR-02 | Probe dock silicone insert pads at both probe-tip receptacles — bonding process matches F-SM-01 precedent | Open — pad **placement GATED: OI-NASAL-09** (no PD-window or contact location exists); bonding process not gated |
| HUB-MDR-03 | Probe dock retention force ≤2N insertion/extraction — bench measurement on first-shot part | Open — **GATED: OI-NASAL-09** (the measurement needs a mating probe; none is dimensioned). The ≤2 N figure's provenance is separately open under `NP-CONV-001` OI-CONV-08 (c) |
| HUB-MDR-04 | Two accessory anchor posts (USB-C, DFU/service) present and dimensioned per §3 F-02 boss spec (1.0mm dia, 0.5mm protrusion) | Open — **port list pending OI-HTOOL-08** (one post per tethered hub port cover, whatever the inventory settles at); boss dimension pending OI-HTOOL-09 |
| HUB-MDR-05 | Port cover tether length ≤20mm confirmed geometrically unable to reach F-04 fan intake grille in CAD assembly — **for every tethered cover on the hub** (OI-HTOOL-08) | Open — **BLOCKING for FAI-HTOOL-02** |
| HUB-MDR-06 | Boa cable channel continuity from shell segment into hub housing — no cross-section discontinuity at interface | Open |
| HUB-MDR-07 | Boa cable channel minimum bend radius ≥12× lace OD at every turn within hub housing | Open — pending OI-HTOOL-02 (lace OD datasheet) |
| HUB-MDR-08 | Boa channel thermally/mechanically isolated from F-04 fan/heatsink cavity | Open |
| HUB-MDR-09 | Fan/heatsink door: two diagonally-opposed captive quarter-turn fasteners, door cannot open with only one released | Open |
| HUB-MDR-10 | Fan intake/exhaust grille louvre slot width ≤6mm (finger-probe safety) confirmed in CAD | Open |
| HUB-MDR-11 | Hub-to-shell mechanical interface (mounting, cable entry, EMI bonding) — CAD confirmation | Open — see OI-HTOOL-01 |

---

## 5. FAI Cross-Reference

| FAI ID | Description | Blocking |
|--------|-------------|---------|
| FAI-HTOOL-01 | Probe dock retention/extraction force bench test (≤2N) and 500-cycle dock/undock durability (no visible wear at Y-junction saddle) | Hardware build — **GATED: `NP-HW-NASAL-001` OI-NASAL-09** (no probe article to dock) |
| FAI-HTOOL-02 | Port cover tether reach test: with cover detached from port and tether fully extended, confirm tether/cover cannot contact the F-04 fan intake grille from any hub orientation | Hardware build — **BLOCKING**, foreign-object-near-fan-blade hazard |
| FAI-HTOOL-03 | Boa channel 50,000-cycle fatigue test through the hub housing segment specifically (not just the shell segment already implied by CLAUDE.md §4.4) — confirms the hub does not introduce a new fatigue-limiting bend | Hardware bench |
| FAI-HTOOL-04 | Fan door finger-probe test (IEC 60529/60601-1 test probe, door open) — confirms no contact with rotating fan blade | Hardware bench |

---

## 6. BOM Impact

| Item | Delta | Notes |
|------|-------|-------|
| Probe dock silicone insert pads (×2) | +$0.10–0.20 | Same silicone family/process as existing electrode pod / rigidizer bonding |
| Port cover tether + anchor boss (×2) | +$0.05–0.10 | Molded-in feature; tether material shared with existing tethered-cover BOM line (CLAUDE.md §2.3) |
| Quarter-turn captive fasteners (×2) | +$0.30–0.60 | Standard catalog part (Southco-equivalent 1/4-turn line), no bespoke tooling |
| Fan door gasket (if required, pending OI-HTOOL-03) | +$0.10–0.30 | Contingent on hub environmental rating decision |
| Mold feature additions (probe dock cradle, anchor bosses, Boa channel geometry, fan door recess + hinge/latch bosses) | one-time tooling, included in hub housing mold cost | No separate mold tool required — these are features of the single hub housing mold, not a variant tool (unlike `NP-TOOL-ZM-SM-001`, which required a separate mold from the base zone module) |
| **No new consumable SKU** | $0 | Fan/heatsink cleaning is tool-free and consumable-free (compressed air or soft brush, user-supplied) |

---

## 7. Open Items

| ID | Description | Blocking |
|----|-------------|---------|
| OI-HTOOL-01 | Hub-to-shell mechanical interface CAD lock: mounting method, Boa/EEG/power cable entry and strain relief, EMI bonding at the shell boundary (§2 placement assumption depends on this) | Pre-prototype; **feeds HUB-MDR-11** |
| OI-HTOOL-02 | Obtain Boa-supplier lace OD datasheet; compute and lock the exact F-03 minimum bend radius (currently a 12×-OD placeholder multiple, §3 F-03) | Pre-prototype; **feeds HUB-MDR-07** |
| OI-HTOOL-03 | Set the hub enclosure's environmental (ingress) rating — no prior document specifies one; determines whether F-04's door requires a gasket and what class | Pre-prototype; **feeds HUB-MDR-09, FAI-HTOOL-04 pass criteria** |
| OI-HTOOL-04 | Confirm F-04 fan/heatsink cavity geometry against the eventual verification-grade CFD and THERM-1b bench outcome (`NP-THERM-CFD-001` §9 step 2) — this document specifies the *access door*, not the heatsink/fan sizing itself, which remains provisional pending that analysis | Post-THERM-1b |
| OI-HTOOL-05 | Port cover tether reach geometry (HUB-MDR-05) — full 3D CAD sweep of the tether's reachable envelope in every hub orientation, not just the nominal orientation | Pre-prototype; **BLOCKING for FAI-HTOOL-02** |
| OI-HTOOL-06 | Hub housing mold steel-cut approval — this §4 checklist (11 items) must be fully signed off, matching the gating pattern used for the zone module (`NP-TOOL-ZM-001` §5) and shell (`NP-TOOL-SHELL-001`) tooling. **Since Rev 2, HUB-MDR-01…03 are gated on `NP-HW-NASAL-001` OI-NASAL-09, so this item cannot close until either that one does or `OI-HTOOL-07` is taken** | All FAI-HTOOL items |
| **OI-HTOOL-07** | **Decide how F-01 is released from the probe's geometry (Rev 2, 2026-09-23).** Two routes. **(a) Wait:** the hub mould steel cut waits on `NP-HW-NASAL-001` OI-NASAL-09's four values, and on the emitter selection and optical work behind the probe tip. **(b) Move every probe-contact surface into the silicone inserts — RECOMMENDED, NOT EXECUTED.** The only F-01 surfaces that touch the probe are already specified as silicone parts *bonded* to the housing (§3 F-01 *Material*); extend that to the saddle (a bonded silicone saddle liner in place of a moulded radius), and have the rigid housing carry only a **pocket whose envelope this document allocates from hub exterior real estate**. The four probe-dependent values then bind two soft parts that are re-made without touching the hub steel, and the one value the hard mould needs — the pocket envelope — is the hub's own to set. Route (b) also **reverses the dependency to the right way round**: today a BASELINED tooling specification waits on a DRAFT probe specification; under (b) the probe is designed to fit an envelope the baselined document already owns, which `NP-HW-NASAL-001` would then carry as a requirement traceable to this allocation. What (b) costs: the *co-moulded* insert option in §3 F-01 *Material* is foreclosed (a co-moulded insert is cut into the steel), and the pocket envelope is a number this document does not yet state and must — it is **not** supplied here, because it depends on hub exterior layout (`OI-HTOOL-01`) and not on anything this item can derive. **Decision owner: ME + Systems. This revision does not execute (b)**: F-01's geometry text is unchanged, and adopting (b) is a Rev 3 change to a BASELINED document | **Hub mould steel cut** (via `OI-HTOOL-06`); `NP-FAI-HUB-001` FAI-HUB-09, -10, -20 |
| OI-HTOOL-08 | **Hub port inventory — which ports the hub carries and which take a tethered cover.** Raised 2026-09-23 at the closure of `OI-ART-07`. F-02 lists USB-C and a "DFU/service" port; no other document describes a separate DFU/service port (DFU runs over USB-C, `np_dfu.c`), and F-02 omits the hub accessory port(s) that A12, A13, A14 and the mastoid pad connect to (§3 F-02 resolution note). The retired `NP-TOOL-SHELL-001` F-02 named "USB-C, left and right hub accessory ports"; `docs/reference/service-network.md` §8.3 sells 3 accessory port covers per headset, **magnetic** (N42 steel disc), where F-02 specifies a **compression-fit** silicone cover. Close by stating (a) the port list with its source — `NP-HW-HUB-001` and the shared accessory-connector decision (`OI-NASAL-07` / `OI-VNSCLIP-06` / `OI-CVNSHW-08` / `OI-AUDIOHW-07`); (b) the cover retention method; (c) the resulting post count in HUB-MDR-04 and `FAI-HUB-14`. Each added cover is another captive object for `RISK-HUB-01`: the ≤ 20 mm maximum and the OI-HTOOL-05 sweep apply to each. **Does not reopen the tether limit** | Pre-prototype; **feeds HUB-MDR-04, HUB-MDR-05, FAI-HUB-14, FAI-HUB-23** |
| OI-HTOOL-09 | **F-02 boss dimension has no derivation for its function.** The 1.0 mm × 0.5 mm boss is stated as "the same boss dimension used for the smart-module rigidizer *alignment* bosses" — a locating feature, not a tether anchor — while `FAI-HUB-19` requires the tether not to release from the boss under a 10 N pull. The retired SHELL F-02 pattern (Ø 4.0 × 3.5 mm with a 2.0 × 2.5 mm through-slot, rated 500 tug cycles at 5 N) was sized for tether capture. Per CLAUDE.md §18 this is raised rather than retired or replaced: show that the boss captures the tether at the FAI-HUB-19 load, or re-derive it from that load. **The ≤ 20 mm tether limit is a hazard control and is not in scope of this item** | Pre-prototype; **feeds HUB-MDR-04, FAI-HUB-14, FAI-HUB-19** |

---

## 8. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 3 | 2026-09-23 | NeurOne Systems Engineering | **`OI-ART-07` closed: this document owns every tethered cover on the hub; `NP-TOOL-SHELL-001` F-02 retired (SHELL Rev 3).** No geometry, tether limit, checklist item or FAI item changes. §3 F-02 gains the resolution note, which also corrects the "2.5×" magnitude carried by the 2026-08-18 records (SHELL's 50 mm is a loop *circumference*, reach ≈ 25 mm, ≈ 1.25× the limit). **Two open items raised rather than invented:** `OI-HTOOL-08` — F-02's port list names a DFU/service port no other document has and omits the hub accessory port(s) four accessory specs connect to; `OI-HTOOL-09` — the F-02 boss dimension is borrowed from an alignment boss and has no derivation against `FAI-HUB-19`'s 10 N tether pull. HUB-MDR-04 and HUB-MDR-05 statuses cite them. |
| 2 | 2026-09-23 | NeurOne Systems Engineering | **F-01's probe-contact geometry recorded as GATED on `NP-HW-NASAL-001` OI-NASAL-09 — closes that document's `OI-NASAL-01` by its second branch.** Rev 1 was baselined with the saddle radius, insert-pad placement and retention test stated as relations to features of the intranasal probe (A12) that no document dimensions; `NP-HW-NASAL-001` Rev 1 (2026-09-20) made that visible. Adds the gate note to §3 F-01, gates HUB-MDR-01…03 and FAI-HTOOL-01, extends OI-HTOOL-06, and raises **OI-HTOOL-07** with a recommended — not executed — route to release the hub mould without waiting for the probe. **No F-01 value, material, feature or BOM line is changed**; the ≤ 2 N detent's provenance stays with `NP-CONV-001` OI-CONV-08 (c). Status remains BASELINED: a gate added to a baselined document narrows what it can release, it does not reopen what it says. |
| 1 | 2026-07-27 | NeurOne Mechanical Engineering | Initial release (F-01…F-04, §4 checklist HUB-MDR-01…11, FAI-HTOOL-01…04, OI-HTOOL-01…06). Approved by Steve Hickman, CEO. In-place correction to §3 F-02 on 2026-08-18 (tether/boss conflict with `NP-TOOL-SHELL-001` F-02, `OI-ART-07`), made without a revision increment. |
