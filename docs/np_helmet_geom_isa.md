# ISA — NeurOne Helmet Layer Geometry (NP-HELMET-GEOM)

**Artifact under design:** the four-station mechanical layer stack (L0 per-module sealed
faces → L1 socket layer → L2+L3 EMF stack + structural shell).
**Brief:** `docs/np_helmet_geom_001.md` · **Effort:** E3 · **Date:** 2026-07-21

---

## Problem
The helmet needs a defined layer geometry and per-layer material set that (a) keeps skin
oils/water/flakes/hair out while passing every modality signal, (b) is user-serviceable for
module swaps, and (c) protects modules and shielding — none of which had a physical-
requirements derivation. A naïve single transparent inner shield is physically impossible
because electrode modules must galvanically contact skin.

## Vision
A concave stack, ~30–35 mm scalp-to-exterior, where each module carries its own IP seal, the
socket layer levers out from a permanently-shielded outer bowl, and the cheapest parts
(bezels, gaskets, plugs) absorb all wear and impact — leaving the expensive modules and the
never-opened EMF envelope untouched across the device lifetime.

## Out of scope
- Re-deriving the inner surface (fixed to the scan datum, principal decision).
- Ear-cup, intranasal, VNS-clip, goggle internal design (separate accessories).
- Firmware/addressing (owned by np_hex_zm_001 / np_module_map).
- Final FMEA numbers (owned by np_fmea_001).

## Principles
1. Impermeability is per-module, not per-helmet.
2. The inner bowl is non-magnetic (fluxgates live there).
3. Shielding stays on one unbroken outer bowl (mu-metal continuity).
4. Cheap, replaceable parts take wear and impact before modules or shield.
5. Offset outward from the concave datum — depth never costs socket count.

## Constraints
- Ambient 60–110 °F, 0–100 % RH; scalp cap 42 °C (IEC 60601).
- Signals to pass: 660–1170 nm optical, galvanic tES current, EEG biopotential, acoustic/bone.
- ISO 10993-5/-10 skin contact; food-contact-grade cleanability at L0.
- One adult SKU, 52–62 cm; ~80-socket lattice.
- RF residual slot ≤ 2.5 mm (6 GHz); shield-ground bond ≤ 50 mΩ.
- Mechanical BOM consistent with the Home Standard $405 target.

## Goal
Deliver locked geometry + material constraints + structural-element inventory + MTBF/cost
comparison for every layer, self-consistent with the committed two-bowl architecture.

## Criteria (ISC)
- **ISC-1** Radial stack-up defined for L0–L3 with nominal + tolerance, offset from the scan datum. *(met — brief §2)*
- **ISC-2** Each L0 element class has hard constraints + candidate materials + a lead pick. *(met — §3.1)*
- **ISC-3** L1 material honors non-magnetic + dimensional-stability + heat-egress constraints. *(met — §3.2)*
- **ISC-4** Outer bowl preserves the unbroken 5-layer shield + permanent-shielding claim. *(met — §3.3)*
- **ISC-5** Complete key-structural-element inventory, incl. the new L0 sealing parts. *(met — §4)*
- **ISC-6** Impact strategy resolves "protect modules, sacrifice cheap part" without a shield. *(met — §5)*
- **ISC-7** MTBF-vs-cost comparison per layer with a rolled-up recommendation. *(met — §6, estimates pending FMEA-RECON)*
- **ISC-8** Every principal requirement traced to where it is met. *(met — §7)*
- **ISC-9** REG-1, PDMS-QUAL, SEAL-1, THERM-1, BEZEL-1, FMEA-RECON gates named. *(met — §8; **BEZEL-1 resolved** PASS via NP-THERM-BEZEL-001; **THERM-1 re-specified** to a face-temp ceiling with sub-gates 1a/1b/1c open; others OPEN)*
- **ISC-10** THERM-1/BEZEL-1 coupling worked: standoff→dose/scalp/junction/pod-travel modelled, bezel value landed. *(met — NP-THERM-BEZEL-001)*
- **ISC-11** FMEA-RECON: §6 mechanical failure modes re-expressed in the NP-RM-001 S×P framework, cross-referenced to RISK IDs + software backstops; new thermal-path gap surfaced. *(met — NP-FMEA-GEOM-001; OI-GEOM-FMEA-01/02/03 open)*

## Test strategy
Analytical/geometry claims verified by construction against the scan constants; material
claims by constraint match + existing qual (PDMS 200-cycle); the physical gates (§8) are
bench/CFD/ingress tests owned downstream. This ISA is verified when ISC-1..9 are all met and
the open gates are explicitly carried, not silently assumed closed.

## Features / decisions
See brief §0 decision record and §2–§6. Load-bearing decisions: shield abandoned (per-module
seal); per-socket LSR land over grommet sheet; glass-filled PBT inner bowl; cluster clamps
over per-module levers; CFRP outer bowl as specced.

## Changelog
- **2026-07-21 Rev A** — first derivation; ISC-1..8 met, ISC-9 gates carried open.
- **2026-07-21 Rev A.1** — THERM-1/BEZEL-1 coupling worked (NP-THERM-BEZEL-001): BEZEL-1 resolved
  (bezel 0.6 → 1.0 mm, optical modules only), THERM-1 re-specified to a face-temp ceiling; ISC-10 added.
- **2026-07-21 Rev A.2** — FMEA-RECON done (NP-FMEA-GEOM-001): mechanical failure modes moved into the
  NP-RM-001 S×P framework; new un-interlocked thermal-path failure mode FMEA-G07-01 raised
  (OI-GEOM-FMEA-01); G06-01 confirmed = RISK-20; ISC-11 added.

## Verification
ISC-1..8, ISC-10, ISC-11 **MET** at the design-brief/analysis level. ISC-9 status: **BEZEL-1 resolved
PASS**; **THERM-1 re-specified** (face-temp ceiling, THERM-1a/1b/1c open); **FMEA-RECON done** (feeds
OI-GEOM-FMEA-01/02/03); REG-1, PDMS-QUAL, SEAL-1 **OPEN**; G06-01/**RISK-20** OPEN/BLOCKING for tooling.
The artifact is a design study, not a tooling lock.
