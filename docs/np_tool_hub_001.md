# Hub Enclosure Tooling Specification

**Project:** NeurOne
**Document:** NP-TOOL-HUB-001
**Revision:** 1
**Date:** 2026-07-27
**Status:** BASELINED
**Effective Date:** 2026-07-27
**Author:** NeurOne Mechanical Engineering
**Approved By:** Steve Hickman, CEO
**References:** CLAUDE.md §2.1, §2.3, §3 (PBM Intranasal), §4.1, §4.3, §4.4, §4.5, §4.7, §5.2; NP-HW-HUB-001 Rev 2; NP-DRV-SHELL-001 Rev 2 §2.4.5 (locked EEG/Hub routing); NP-REQ-FANHEALTH-001 Rev 1 (SR-FAN-01…06); NP-THERM-CFD-R1-001 Rev 1 §5 (BN-boss conductive export); NP-FMEA-GEOM-001 Rev 1 (FMEA-G07-01 / RISK-26); NP-TOOL-ZM-001 Rev 1 (mold design review checklist pattern); NP-TOOL-SHELL-001 Rev 1 (anchor post / tether pattern); NP-TOOL-LENS-001 Rev 2 (captive-fastener / loss-prevention pattern); NP-DP-001 §6.3 (Hub tooling design review, Month 4); docs/status/pending-decisions.md §13.4
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
2. **Antennas live in the hub, not the shell** (CLAUDE.md §4.1), which is only meaningful if the hub sits outside (or at the boundary of) the 5-layer shielded shell envelope described in CLAUDE.md §4.3.
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

### F-02 — Accessory Anchor Posts (Hub Port Covers)

**Purpose:** the hub carries its own service/charge ports (USB-C charge/data, and a DFU/service access port distinct from the headset's zone-slot and lens-rim accessory ports already covered by `NP-TOOL-SHELL-001` F-02). Per CLAUDE.md §2.3, "Interface protection covers (complete kit) ... All tethered — loss prevention by design"; this feature extends that principle to the hub's own ports.

**Geometry:**
- **Two anchor posts**, one per hub port (USB-C port cover, DFU/service port cover), each a molded boss (1.0mm diameter, 0.5mm protrusion — same boss dimension used for the smart-module rigidizer alignment bosses, `NP-TOOL-ZM-SM-001` F-SM-01) that captures a silicone tether loop molded into each port cover.
- **Tether length ≤ 20mm free length** — shorter than the lens rim-guard tether (45mm, `NP-TOOL-LENS-001` F-07) because the hub tether has no field-of-view constraint to satisfy, but a hard maximum is still required so a detached-but-tethered cover **cannot reach the F-04 fan intake grille** (§3, F-04) — an untethered-length foreign object drawn toward a rotating fan blade is a distinct hazard from the FOV-crossing concern that bounds the lens tether. This is the governing constraint on tether length, not cosmetics.
- Anchor posts and covers are not color-coded (unlike the shell's zone-slot plugs, `NP-TOOL-SHELL-001` F-01) — the hub has no zone-position ambiguity to disambiguate.

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
| HUB-MDR-01 | Probe dock Y-junction saddle radius sized to Y-junction body OD (not lead OD) — CAD verification | Open |
| HUB-MDR-02 | Probe dock silicone insert pads at both probe-tip receptacles — bonding process matches F-SM-01 precedent | Open |
| HUB-MDR-03 | Probe dock retention force ≤2N insertion/extraction — bench measurement on first-shot part | Open |
| HUB-MDR-04 | Two accessory anchor posts (USB-C, DFU/service) present and dimensioned per §3 F-02 boss spec (1.0mm dia, 0.5mm protrusion) | Open |
| HUB-MDR-05 | Port cover tether length ≤20mm confirmed geometrically unable to reach F-04 fan intake grille in CAD assembly | Open — **BLOCKING for FAI-HTOOL-02** |
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
| FAI-HTOOL-01 | Probe dock retention/extraction force bench test (≤2N) and 500-cycle dock/undock durability (no visible wear at Y-junction saddle) | Hardware build |
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
| OI-HTOOL-06 | Hub housing mold steel-cut approval — this §4 checklist (11 items) must be fully signed off, matching the gating pattern used for the zone module (`NP-TOOL-ZM-001` §5) and shell (`NP-TOOL-SHELL-001`) tooling | All FAI-HTOOL items |
