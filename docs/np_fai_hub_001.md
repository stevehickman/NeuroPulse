# Hub Enclosure — First Article Inspection Checklist

**Project:** NeurOne
**Document:** NP-FAI-HUB-001
**Revision:** 1
**Date:** 2026-08-11
**Status:** DRAFT — issued for first-article use; two items gated (see §0)
**Effective Date:** — (effective at hub housing first shot)
**Author:** NeurOne Quality (interim: Steve Hickman, CEO)
**Approved By:** —
**References:** NP-TOOL-HUB-001 Rev 1 (**the sole source of every dimension below**); NP-FAI-001 Rev 1 §2–§3 (issue conditions, structure, markers); NP-ART-001 Rev 1 §2.2 (artifact A8); NP-RISK-004 Rev 1; NP-REQ-FANHEALTH-001 Rev 1 (SR-FAN-05); NP-FMEA-GEOM-001 Rev 1 (FMEA-G07-01 / RISK-26); IEC 60529; IEC 60601-1 Fig. 6 test finger; CLAUDE.md §3 (PBM Intranasal), §4.4 (Boa), §4.5
**Related Issues:** —
**Gate:** NP-COORD-001 G1 (hub tooling design review) → G3 (pre-production)
**IEC 62304 Class:** N/A
**Supersedes:** Expands `NP-TOOL-HUB-001` Rev 1 §5, which carried FAI-HTOOL-01…04 as a four-line stub. Those four items are preserved by number.
**Parent Document:** NP-TOOL-HUB-001 Rev 1

---

## 0. Why this checklist exists and the two others do not

`NP-FAI-001` §2 requires a `BASELINED` or `ACTIVE` governing specification before an FAI checklist
may be written. **`NP-TOOL-HUB-001` Rev 1 is the only artifact specification in the NeurOne set with
status BASELINED**, and its four features carry dimensioned geometry — saddle radius keyed to the
Y-junction OD, 1.0 mm × 0.5 mm anchor bosses, ≤ 20 mm tether, ≤ 6 mm louvre slots, two diagonally
opposed quarter-turn captives. That is why this checklist exists and the other eleven in
`NP-ART-001` §3.2 do not.

**Two items are `[GATED]` rather than fabricated:**

| Item | Gated on | What is missing |
|---|---|---|
| **FAI-HUB-11** | `OI-HTOOL-02` | The F-03 minimum bend radius is stated as *≥ 12 × nominal Boa lace OD*. The lace OD is a supplier datasheet value not yet on file, so the radius is a multiple without a multiplicand. |
| **FAI-HUB-16** | `OI-HTOOL-03` | The hub enclosure has **no environmental rating** — no NeurOne document sets one. CLAUDE.md's IPX4 references are scoped to the module swap connector, not the hub. Without a class there is no ingress criterion, and whether F-04's door needs a gasket at all is undecided. |

Neither carries a number. Per `NP-FAI-001` §3 a `[GATED]` item is numbered, present, and unsignable.

**Build record**

| Field | Value |
|---|---|
| Build lot / serial | |
| FAI date | |
| Hub housing mould / shot number | |
| Hub housing lot number | |
| Inspector | |
| Reviewed by | |
| Final result | **PASS** / **FAIL** |

---

## 1. Pre-inspection document check

| Item | Check | Method | Accept criterion | Result | Sign |
|---|---|---|---|---|---|
| FAI-HUB-01 | `NP-TOOL-HUB-001` Rev 1 or later on file | Document check | Revision confirmed, status BASELINED | | |
| FAI-HUB-02 | All eleven `NP-TOOL-HUB-001` §4 mould-design-review items (HUB-MDR-01…11) signed off | Document check | 11/11 closed. **Any open item → this FAI does not proceed** (`OI-HTOOL-06` gates steel cut on exactly this) | | |
| FAI-HUB-03 | Hub-to-shell interface CAD locked | Document check | `OI-HTOOL-01` closed; HUB-MDR-11 evidence on file | | |
| FAI-HUB-04 | BOM revision matches `NP-TOOL-HUB-001` §6 — quarter-turn captives from a standard catalogue line, not a bespoke part | BOM review | Catalogue part number recorded, captive-retention feature confirmed | | |

---

## 2. Incoming component inspection

| Item | Check | Method | Accept criterion | Result | Sign |
|---|---|---|---|---|---|
| FAI-HUB-05 | Quarter-turn captive fasteners (×2) | Visual + function | Fastener cannot be separated from the door when fully released — captive feature functional out of the box | | |
| FAI-HUB-06 | Probe-dock silicone insert pads (×2), Shore 30–40 A, same family as the electrode-pod mounts | Durometer + certificate | Shore 30–40 A; material certificate references the lot | | |
| FAI-HUB-07 | Port-cover silicone, Shore 40–50 A | Durometer + certificate | Shore 40–50 A; ISO 10993-5 cytotoxicity certificate on file (skin-contact part) | | |
| FAI-HUB-08 | PTFE liner stock for the F-03 channel | Certificate | Same supplier and process as the shell channel (CLAUDE.md §4.4) — **no new qualification permitted**; a different supplier voids the "no new qualification required" basis in `NP-TOOL-HUB-001` §3 F-03 | | |

---

## 3. Moulded housing inspection — first shot

| Item | Check | Method | Accept criterion | Result | Sign |
|---|---|---|---|---|---|
| FAI-HUB-09 | **F-01** probe-dock Y-junction saddle radius | CMM or calibrated comparator | Sized to the Y-junction **body** OD, not the bilateral lead OD. A saddle cut to lead OD lets the junction hang on its leads, which is the exact failure the dock exists to prevent (CLAUDE.md §3: *"prevents Y-junction fracture"*) | | |
| FAI-HUB-10 | **F-01** two probe-tip receptacles, silicone insert pads present and bonded at the probe-tip contact point | Visual, 10× | Both present; bond per the F-SM-01 precedent process; pad covers the PD window and the optical-code/pogo contact landing | | |
| FAI-HUB-11 | **[GATED: OI-HTOOL-02]** **F-03** Boa channel minimum bend radius at every turn within the hub housing | CMM / CAD-to-part comparison | ≥ 12 × nominal lace OD. **Criterion not derivable until the lace OD datasheet is on file.** Item is unsignable until `OI-HTOOL-02` closes | | |
| FAI-HUB-12 | **F-03** channel cross-section continuity at the shell/hub interface | Section cut or borescope | No discontinuity, step or pinch at the interface; PTFE liner continuous | | |
| FAI-HUB-13 | **F-03** channel thermally and mechanically isolated from the F-04 fan/heatsink cavity | Visual + section | No shared wall exposing the liner to heatsink surface temperature or fan-drawn airflow | | |
| FAI-HUB-14 | **F-02** two anchor posts present, at the USB-C and DFU/service ports | Comparator | Ø 1.0 mm, 0.5 mm protrusion, per `NP-TOOL-HUB-001` §3 F-02 | | |
| FAI-HUB-15 | **F-04** fan grille louvre slot width | Calibrated pin gauge | ≤ 6.0 mm on every slot | | |
| FAI-HUB-16 | **[GATED: OI-HTOOL-03]** **F-04** door sealing / hub enclosure ingress rating | IEC 60529 per the assigned class | **No class assigned.** Whether a door gasket is required is contingent on the same decision. Unsignable until `OI-HTOOL-03` closes | | |

---

## 4. Assembly and interface verification

| Item | Check | Method | Accept criterion | Result | Sign |
|---|---|---|---|---|---|
| FAI-HUB-17 | **F-04** door cannot be removed with only one fastener released | Function, both fasteners in turn | Door retained by either fastener alone, in both directions | | |
| FAI-HUB-18 | **F-04** door opens and closes without tools | Function, bare hand + coin | Quarter-turn (90°) actuation; no tool beyond a coin or thumbnail | | |
| FAI-HUB-19 | **F-02** port covers seat and seal on their ports; tether captured on the boss | Function + visual | Compression fit at the port opening; tether does not release from the boss under a 10 N pull | | |

---

## 5. Lifecycle validation

| Item | Check | Method | Accept criterion | Result | Sign |
|---|---|---|---|---|---|
| FAI-HUB-20 | *(was FAI-HTOOL-01)* Probe dock retention/extraction force and durability | Force gauge; 500 dock/undock cycles | ≤ 2 N insertion and extraction throughout; no visible wear at the Y-junction saddle or the insert pads at 500 cycles | | |
| FAI-HUB-21 | *(was FAI-HTOOL-03)* Boa channel fatigue **through the hub housing segment specifically** | 50,000-cycle lace actuation, hub segment instrumented | No lace fracture, no liner wear-through. The point of the test is to confirm the hub does not introduce a fatigue-limiting bend that the shell segment does not have — the hub segment is the one that is **not** field-replaceable with the in-box spare cable and hook tool | | |
| FAI-HUB-22 | Captive fastener cycle life | 1,000 open/close cycles | Captive feature intact; no thread wear preventing quarter-turn engagement | | |

---

## 6. Functional and safety test

| Item | Check | Method | Accept criterion | Result | Sign |
|---|---|---|---|---|---|
| FAI-HUB-23 | **[BLOCKING]** *(was FAI-HTOOL-02)* Port-cover tether reach — foreign object near a rotating fan blade | Physical sweep, cover detached and tether fully extended, **every** hub orientation | Tether and cover **cannot contact the F-04 fan intake grille from any orientation**. Any contact → HALT; tether length or anchor position non-conformant. Depends on `OI-HTOOL-05` (full 3D CAD envelope sweep) having been done first — a nominal-orientation check is not this test | | |
| FAI-HUB-24 | **[BLOCKING]** *(was FAI-HTOOL-04)* Fan door finger-probe, door **open** | IEC 60601-1 Fig. 6 jointed test finger, fan running at maximum RPM | No contact with the rotating blade at any probe angle or insertion depth. The door is a service access point, **not** a substitute for blade guarding | | |
| FAI-HUB-25 | Fan RPM telemetry reaches SHDR from the assembled hub | Read-back over USB-C | RPM logged; value tracks a commanded change. This is the sensor SR-FAN-05's predictive-maintenance alert depends on, and RISK-26's whole mitigation chain sits behind it | | |
| FAI-HUB-26 | Post-cleaning function: clear the heatsink through the open door with compressed air and a soft brush, reclose | Function | Fan RPM and airflow return to pre-fouling baseline; no consumable required (`NP-TOOL-HUB-001` §6: no new SKU) | | |

---

## 7. Non-conformance summary

| Item | Observed | Disposition | Approved by (Eng) | Approved by (Quality) |
|---|---|---|---|---|
| | | | | |

No waiver without **both** Engineering and Quality sign-off (`NP-FAI-001` §3).

---

## 8. Result and sign-off

| Discipline | Items owned | Name | Date | Signature |
|---|---|---|---|---|
| Mechanical Engineering | FAI-HUB-09…19, 20…22 | | | |
| Thermal / Safety | FAI-HUB-24, 25, 26 | | | |
| Procurement / Supplier Quality | FAI-HUB-05…08 | | | |
| Quality | all; §7 disposition | | | |

**FAI result:** ☐ PASS ☐ PASS WITH DISPOSITION ☐ FAIL

---

## 9. Risk cross-reference

| Risk | Register | Controlled by | Note |
|---|---|---|---|
| **RISK-26** — fan/heatsink airflow loss → scalp face > 42 °C while junction NTC ≤ 62 °C | `NP-RISK-004` | FAI-HUB-25, FAI-HUB-26 | The F-04 door is not a safety control; it is what makes SR-FAN-05's predictive alert **actionable**. The safety control is the SW01-M04 face-NTC interlock (Path B1). FAI-HUB-25 verifies the telemetry the alert rides on. |
| **RISK-HUB-01** — tethered cover drawn into the fan | `NP-RISK-004` | FAI-HUB-23 **[BLOCKING]** | A hazard created *by* a mitigation: F-02's tethers exist for loss prevention (CLAUDE.md §2.3) and introduce a captive object near a rotating blade. |
| **RISK-HUB-02** — finger contact with the fan blade through the service door | `NP-RISK-004` | FAI-HUB-24 **[BLOCKING]** | Same shape: tool-free service access creates an access path that did not exist. |
| **RISK-HUB-03** — Boa lace fatigue at a hub-segment bend | `NP-RISK-004` | FAI-HUB-11 **[GATED]**, FAI-HUB-21 | The hub segment is not field-replaceable; the shell segment is. |

---

## 10. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-08-11 | NeurOne Quality | Initial release. Expands `NP-TOOL-HUB-001` Rev 1 §5's four-item stub (FAI-HTOOL-01…04) into a full nine-section checklist per `NP-FAI-001` §3; the four originals are preserved and mapped (FAI-HUB-20/23/21/24). **First and, at this date, only NeurOne artifact FAI checklist that meets `NP-FAI-001` §2's F1 condition** — `NP-TOOL-HUB-001` is the sole BASELINED artifact specification. Two items issued `[GATED]` rather than given fabricated criteria: FAI-HUB-11 (Boa bend radius, needs `OI-HTOOL-02`'s lace OD) and FAI-HUB-16 (ingress, needs `OI-HTOOL-03` — **the hub enclosure has no environmental rating in any NeurOne document**). Two items are `[BLOCKING]`, both covering hazards that mitigations introduced: FAI-HUB-23 (tethered cover reaching the fan intake) and FAI-HUB-24 (finger access through the service door). §9 records RISK-HUB-01…03, new to `NP-RISK-004`. |
