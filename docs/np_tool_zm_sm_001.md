# 1064nm Smart Zone Module — Mould Variant Specification

**Project:** NeurOne  
**Document:** NP-TOOL-ZM-SM-001  
**Revision:** A  
**Date:** 2026-05-13  
**Status:** BASELINED  
**Effective Date:** 2026-05-13  
**Author:** NeurOne Mechanical Engineering  
**Approved By:** Steve Hickman, CEO  
**References:** NP-TOOL-ZM-001 Rev A; NP-HW-FPC-001 Rev E; NP-PROC-FPC-1064-001 Rev A; NP-FAI-ZM-001 Rev A  
**Related Issues:** GitHub Issue #54  
**Gate:** —  
**IEC 62304 Class:** —  
**Supersedes:** —  
**Parent Document:** NP-TOOL-ZM-001 Rev A (base zone module mould spec — unchanged)

---

> **⚠ SUPERSEDED (2026-07-28) — parent mould family retired, do not use for new tooling work.** This is a variant of `NP-TOOL-ZM-001`, itself named a "legacy zone-module tooling" predecessor in `docs/np_hex_zm_001.md` §8. `NP-HEX-ZM-001` §1 replaces the whole position-unique-mould-plus-variant approach with **one universal 40mm hex-tile mould**, where element population (T1-A/B/C) is an FPC/loading difference, not a separate mould tool — see §6 "one universal module-shell mould... the inventory/tooling win."
>
> **CONFIRMED 2026-07-28 — the F-SM-03 mechanical key is not needed, do not carry it forward.** F-SM-03 existed to physically prevent a smart module from being inserted into a hub slot not wired for it. Under the SMART-1 decision, *every* socket is I2C/TIA-capable, so there is no "wrong socket" left to key against — for T1-C or any future smart module type. `NP-HEX-ZM-001` §4a is explicit that hex-tile modules use an orientation-only key with no type differentiation. **Downstream effect on this document's own content:** §3 F-SM-03 (the "two-slot design," the ≥2.0mm wider key body, the per-zone smart-key dimension table) and `OI-SM-SHELL-01` (the shell Rev B dual-slot-cutout requirement this key drove) do not carry forward to the hex-tile mould — that requirement is gone, not just the mould geometry. §4 checklist items SM-MDR-07/SM-MDR-08 and §5 FAI-SM-MOULD-02 (all key-specific) are moot for the same reason. The rest of the F-SM-03 section below is retained as historical record of the retired design, not as a spec to implement.
>
> **Possibly still-reusable:** the rigidizer-cavity concept (F-SM-01, accommodating an on-module driver sub-board) — NP-HEX-ZM-001's T1-C also carries an on-module driver, so a cavity of this kind is still plausibly needed, but its dimensions (sized for the retired 66×78mm module) are not validated against the 40mm hex tile.

---

## 1. Scope

This document specifies the mould variant for the 1064nm smart zone module shell (SKU: NP-ZM-1064). The base zone module mould (NP-TOOL-ZM-001 Rev A, features F-01 through F-08) is **unchanged and not re-tooled**. The smart module mould variant is a **separate mould tool** designated **NP-MOULD-ZM-SM-01**, sharing geometry with the base module mould with the following differences:

1. Rigidizer cavity (F-SM-01) — new; accommodates on-module driver IC sub-board
2. LED array window geometry (F-SM-02) — updated for 3-wavelength tri-emitter layout
3. Mechanical key variant (F-SM-03) — same asymmetric key principle as base module, but uses a distinct key profile to prevent a smart module from being inserted into a slot that has not been verified as hub-PCB compatible (hub PCB must have OI-PBM-HW-01 and OI-PBM-HW-02 modifications installed)

All other moulded features are **identical** to NP-TOOL-ZM-001 Rev A and are inherited without modification.

---

## 2. Inherited Features — Unchanged from NP-TOOL-ZM-001 Rev A

The following base module features are carried into NP-MOULD-ZM-SM-01 without dimensional change:

| Feature | NP-TOOL-ZM-001 Ref | Description |
|---------|-------------------|-------------|
| Co-moulded gasket groove | F-02 | Shore 40–50A silicone, D-section 2.5 × 2.0 mm, 20% compression when seated; IPX4 rated ≥10 swap cycles (FAI-IPX-02 BLOCKING) |
| Sliding eject lever recess | F-03 | 10–12 mm lever arm, 3:1 mechanical advantage, ≤1N extraction, 316SS hinge pin, snap-fit detent (RISK-22 Option A) |
| Braille + raised numeral | F-05 | ISO 17049 braille + raised numeral per zone (RISK-15 Layer 3) |
| N tactile dots on shell | F-06 | N dots (1–5) per zone slot position on headset shell (RISK-15 Layer 4) |
| PD2 aperture | F-04 | X = 33.0 mm, Y = 39.0 mm from module reference corner, ±0.2 mm; compatible with InGaAs PD2 annular ring (§5.2 of NP-HW-FPC-001 Rev E) |
| PDMS optical window | F-07 | Plasma-activated anti-fouling PDMS, SiO₂ 75 nm interlayer; thermal cycling qualification 200-cycle IEC 60068-2-14 (FAI-TC02 BLOCKING) |
| Lever-actuated ZIF access | F-08 | Back-flip lever access for Hirose FH34S-20S-0.5SH; tool-free extraction |

Braille numeral and tactile dot count for smart module zones: same zone-position encoding as the base module slot the smart module occupies. Smart modules are manufactured in zone-specific variants (ZM-SM-01 through ZM-SM-05), each with the correct braille/numeral/dot count for its intended slot.

---

## 3. New and Modified Features

### F-SM-01 — Rigidizer Sub-Board Cavity (NEW)

**Purpose:** Houses the ATtiny402 + MOSFET rigidizer PCB (NP-HW-FPC-001 Rev E §6.1) inside the smart module shell, behind the Hirose connector end of the FPC.

**Dimensions:**
- Internal cavity clear: 24 × 16 × 3.5 mm (length × width × depth)
- Nominal rigidizer sub-board: 22 × 14 × 0.8 mm; clearance: 1.0 mm each side, 2.7 mm depth margin
- Wall thickness above cavity (separating cavity from scalp-facing surface): ≥ 1.5 mm (thermal isolation from NTC measurement zone)
- Cavity location: at the Hirose connector end of the module, entirely within the module body above the connector tab; must not overlap the LED array window or PD aperture area

**Material:** Cavity walls same CFRP-filled polymer as base module shell. No additional insert.

**Thermal vent:** One 1.0 mm diameter vent hole through the side wall of the cavity to allow ATtiny402 self-heating (< 50 mW at full load) to dissipate without affecting scalp NTC reading. Vent exits on the lateral face of the module, protected by the co-moulded gasket when module is inserted.

**Assembly access:** Rigidizer sub-board is placed into cavity before module shell is bonded. Adhesive retention: 3M 9088 double-sided tape (same specification as lens PDMS attachment process). Cavity has a 2-point alignment boss (1.0 mm boss, 0.5 mm protrusion) matching holes in rigidizer PCB.

### F-SM-02 — LED Array Window (MODIFIED)

**Base module (NP-TOOL-ZM-001 Rev A F-01):** Single rectangular LED array window 66 × 78 mm, optimised for 6 mm pitch interleaved 660/808nm LED pairs.

**Smart module:** Same 66 × 78 mm window outer dimensions. Internal PDMS diffuser geometry is updated for tri-wavelength emitter layout (200 × 660nm + 200 × 808nm + 150 × 1064nm = 550 LEDs):

- Window dimensions: unchanged (66 × 78 mm)
- Internal diffuser pocket depth: 1.2 mm (unchanged from base module)
- LED array footprint compatibility: 1064nm LEDs use the same SMD package family as 660/808nm emitters (confirmed by NP-PROC-FPC-1064-001 Rev A §3.2); no window change required
- The 3-wavelength interleaved pattern results in slightly lower LED density per unit area vs. base module, but the window geometry is identical and no mould change to the LED array window dimensions is required

**Conclusion:** F-SM-02 requires **no dimensional change** to the LED array window from the base module mould. The difference is entirely in the FPC artwork. This simplifies tooling: the smart module mould variant re-uses the F-01 window geometry of the base mould unchanged.

### F-SM-03 — Asymmetric Mechanical Key (MODIFIED)

**Base module (RISK-15 Layer 1):** Each zone position (ZM-01 through ZM-05) has a unique asymmetric key that prevents a module from the wrong zone slot. The key profile is unique per zone position (5 variants).

**Smart module requirement:** The smart module key must additionally prevent insertion into a hub that has **not** been modified with OI-PBM-HW-01 (TIA gain switch) and OI-PBM-HW-02 (I2C bus switch). A smart module inserted into an unmodified hub would cause TIA saturation and I2C bus contention.

**Implementation:** The smart module mechanical key uses a distinct overall key body profile that is **incompatible with the base module slot geometry** in the headset shell. The headset shell must have a corresponding modified slot cutout for each zone position that accepts either the base or smart module key.

**Two-slot design:**
- Base module slot profile: base key geometry (5 zone-specific variants) — accepts only base modules
- Smart module slot profile: smart key geometry (5 zone-specific variants, same zone encoding) — accepts only smart modules

The smart key body is ≥ 2.0 mm wider on the lateral face (non-functional protrusion), making it physically impossible to insert into a base-spec slot. This ensures a user cannot inadvertently install a smart module in a hub that lacks the TIA gain selection hardware.

**Headset shell update required:** The headset shell tooling (NP-TOOL-SHELL-001 Rev A) must be updated to Rev B to add smart module slot cutout geometry per zone position. This is a new open item (OI-SM-SHELL-01, see §7).

**Zone-position encoding:** Within the smart key family, zone-specific encoding (preventing insertion of the wrong zone's smart module) uses the same mechanical dimension differences as the base module key family, just offset by the 2.0 mm body width change.

---

## 4. Mould Design Review Checklist

The base module checklist (NP-TOOL-ZM-001 Rev A §5, 12 items) applies in full. Additional smart module items:

| Item | Description | Status |
|------|-------------|--------|
| SM-MDR-01 | Rigidizer cavity dimensions 24 × 16 × 3.5 mm verified against rigidizer PCB BOM | Open |
| SM-MDR-02 | Cavity wall thickness ≥ 1.5 mm above rigidizer (thermal isolation confirmed) | Open |
| SM-MDR-03 | 1.0 mm thermal vent geometry — exits on lateral face, gasket-protected when inserted | Open |
| SM-MDR-04 | Alignment bosses (2× 1.0 mm) match rigidizer PCB mounting holes | Open |
| SM-MDR-05 | LED array window 66 × 78 mm — confirmed no change required from base module F-01 | ✓ (§3.2 analysis) |
| SM-MDR-06 | PD2 aperture X=33.0, Y=39.0 mm ±0.2 mm — carried from base module F-04 | ✓ |
| SM-MDR-07 | Smart key body +2.0 mm lateral face confirmed incompatible with base module slot | Open — CAD verification |
| SM-MDR-08 | Zone-specific smart key variants (ZM-SM-01 through ZM-SM-05) dimension table produced | Open |
| SM-MDR-09 | Braille / numeral / tactile dots carried from base module per zone variant | ✓ (inherited) |
| SM-MDR-10 | Co-moulded gasket groove geometry unchanged — same IPC-IPX-02 qualification applies | ✓ (inherited) |
| SM-MDR-11 | Sliding eject lever recess unchanged from base module F-03 | ✓ (inherited) |
| SM-MDR-12 | PDMS optical window and SiO₂ interlayer bonding process qualification — same FAI-TC02 applies | ✓ (inherited) |

---

## 5. FAI Cross-Reference

Smart module mould FAI items build on the base module FAI (NP-FAI-ZM-001 Rev A). New items required:

| FAI ID | Description | Blocking |
|--------|-------------|---------|
| FAI-SM-MOULD-01 | Rigidizer cavity dimensional inspection (24 × 16 × 3.5 mm clearance, CMM) | Hardware build |
| FAI-SM-MOULD-02 | Smart key insertion test: smart module inserts into smart-compatible slot; base module cannot; smart module cannot insert into base slot | Headset shell Rev B required (OI-SM-SHELL-01) |
| FAI-SM-MOULD-03 | Thermal vent airflow test: ATtiny402 at 50 mW load; NTC reading deviation < 0.5°C vs ambient | Hardware bench |
| FAI-SM-MOULD-04 | IPX4 qualification with smart module (co-moulded gasket, ≥10 swap cycles) — same protocol as FAI-IPX-02 | Hardware bench (same BLOCKING gate) |

---

## 6. BOM Impact

The smart module mould variant has the following BOM impact vs. base module:

| Item | Delta | Notes |
|------|-------|-------|
| Rigidizer sub-board components | +$1.00 | ATtiny402 + 3× IRLML6344 + passives (§6.2 of NP-HW-FPC-001 Rev E) |
| InGaAs PD1 + PD2 (2 units) | +$16–24 | vs. silicon PD pair ($1.50–3.00 total); see NP-PROC-FPC-1064-001 Rev A §4 |
| 1064nm LED emitters (150 units) | +$9–15 | EPITEX or equivalent; see NP-PROC-FPC-1064-001 Rev A §3 |
| FPC artwork delta (NP-FPC-ZM-SM-01) | +$2–4/FPC | New artwork; same process line as base FPC |
| Mould variant tooling (one-time) | +$15,000–25,000 | Separate mould from base module; partial cavity overlap with base mould possible |
| **Module recurring BOM delta** | **+$28–44** | vs. base zone module |

**Estimated smart module retail price:** $149–199 (upgrade accessory, sold per zone position). Gross margin ~40–50% at scale.

---

## 7. Open Items

| ID | Description | Blocking |
|----|-------------|---------|
| OI-SM-SHELL-01 | Headset shell tooling update (NP-TOOL-SHELL-001 Rev B): add smart module slot cutout geometry per zone position (5 positions). Required before smart module can be physically installed in headset. | Hardware prototype; **BLOCKING for FAI-SM-MOULD-02** |
| OI-SM-MOULD-01 | Rigidizer PCB final dimensions to mould designer (dependent on OI-PBM-HW-04 Gerber release) | SM-MDR-01 |
| OI-SM-MOULD-02 | Zone-specific smart key dimension table (ZM-SM-01 through ZM-SM-05) — ME to produce | SM-MDR-08 |
| OI-SM-MOULD-03 | Mould steel cut approval — 12-item base MDR checklist + 12-item smart MDR checklist must all be signed off | All FAI |
| OI-SM-MOULD-04 | PDMS SiO₂ interlayer qualification for smart module PDMS window — confirm same FAI-TC02 result applies (same bonding process, same substrate) | FAI-SM-MOULD-04 |
