# NP-HELMET-GEOM-001 Rev A — Helmet Layer Geometry & Material Constraints

**Program:** NeurOne chassis / mechanical stack
**Status:** DESIGN STUDY — first physical-requirements derivation of the four-station
layer stack. Geometry basis is the scan-grounded inner surface (offset-outward
method, principal decision). NOT a locked tooling baseline; gated by REG-1
(10-20 registration), the PDMS thermal-cycle qual, and FMEA reconciliation (§9).
**Companion ISA:** `docs/np_helmet_geom_isa.md`
**Sources reconciled:** `docs/np_hex_zm_001.md` (§3.4 scan geometry, §5 two-bowl
shell), `CLAUDE.md` §4.2 (interlocks/thermal), §4.3 (EMF stack), §4.5 (power/heat),
`docs/np_fmea_001.md`, `docs/np_opt_psf_001.md`, `docs/np_hw_hub_001.md`.
**Date:** 2026-07-21

---

## 0. Decision record (2026-07-21)

| Item | Decision | Source |
|------|----------|--------|
| **Single inner transparent shield** | **ABANDONED.** Any socket can hold an electrode module, and electrodes must galvanically contact skin, which a continuous dielectric barrier physically blocks. The skull-facing "surface" is the aggregate of per-module sealed faces + inter-socket gap sealing + plugs/caps. | Principal, Q2 |
| **Layer topology** | Skull→out: **L0** per-module sealed faces (user-swapped) → **L1** socket layer / inner bowl (user-separable, cluster levers on outer face) → **L2+L3** EMF stack + structural shell, bonded into one service-only outer bowl. | Principal, Q1 |
| **Geometry basis** | **Offset outward** from the scan-grounded inner datum (np_hex_zm_001 §3.4). No re-derivation of the inner surface. | Principal, Q4 |
| **Ear cups** | **Separate rim-mounted subassembly.** Vault stack terminates at the ear boundary already excluded from the lattice. | Principal, Q3 |
| **Reconciliation** | This stack **is** the committed two-nested-bowl shell (np_hex_zm_001 §5). Inner bowl = L1; outer bowl = L2+L3. No architectural conflict; this brief adds the L0 sealing detail and the full radial/material/MTBF treatment. | This doc |

**Governing environmental envelope (all layers):** split into two distinct envelopes in **NP-ENV-001**
— a **survival/warranty (non-degradation)** envelope (**committed target −20/+60 °C**, qualified −30/+70,
TARGETED pending qual per NP-ENV-001 §2.1; the former 60–110 °F / 0–100 % RH was mis-scoped as operating
and is re-designated survival) and
**operating** envelopes that are per-modality → per-module → per-protocol (⊂ survival). The
scalp-contact cap **42 °C** (IEC 60601) governs emitting zones during operation; repeated NIR flux
660–1170 nm at the L0 faces;
handling flex; food contact + cleaning agents; skin oils. Skin-contact elements meet
ISO 10993-5/-10; L0 faces cleanable to a food-contact-grade proxy (21 CFR 177 /
EU 10/2011).

---

## 1. Datum & method

The **innermost (scalp-facing) surface is the fixed datum**, taken directly from the
LiDAR interior scan (np_hex_zm_001 §3.4): median curvature **R_m ≈ 87 mm**
(κ = 0.0115 mm⁻¹), **rim→crown 157 mm**, tileable vault **~1370 cm²**, one adult SKU
**52–62 cm**, ~80-socket parity-alternating lattice. Every outward layer is a radial
offset from this datum.

**Concavity works in our favor.** Offsetting a concave bowl *outward* **increases**
surface area (~+2 %/mm; verified in np_hex_zm_001 §3.4). So each outward layer is
strictly roomier than the one inside it — the socket layer is necessarily larger than
the scalp datum (as required), and the shell is larger still. Nothing about the stack
depth reduces the socket count.

---

## 2. PHASE 1 — Radial geometry stack-up

Nominal radii, scalp datum → exterior. Tolerances are design-stage targets pending
tooling DFM.

| Station | Element | Radial (mm) | Tol | Note |
|--------:|---------|------------:|-----|------|
| **L0** | Scalp-contact datum (emitting/electrode face plane) | 0.0 | ref | The scan surface itself |
| | Optical window (PDMS + SiO₂ interlayer) or electrode pad | 0.5–1.0 | — | Electrode pods travel ±12 mm on springs; nominal flush |
| | **Sacrificial bezel land** (optical modules; proud of face, §5) | +1.0 | ±0.2 | Impact **and** thermal-decoupling function (NP-THERM-BEZEL-001); electrode-pod aperture is bezel-free |
| **L1** | Module seated body depth (LED/PCB/driver + 20-pin FPC) | 12–16 | ±0.3 | Deepest for T1-C (on-module driver) / T2-D (laser + TEC) |
| | Inner-bowl socket wall + FPC channel | 2.0–2.5 | ±0.2 | Non-magnetic polymer (fluxgates live here) |
| | Cluster-clamp + lever features (outer face) | 3.0–4.0 | ±0.3 | One actuator per 7-hex cluster, gap-facing |
| | **L1 subtotal** | **~18–22** | | Module body dominates |
| **Gap** | Inter-bowl clamp travel + blind-mate boss + labyrinth lip | 5–7 | ±0.5 | Where cluster clamps and the sensor/coil connector live |
| **L2** | Carbon-loaded absorber foam (EMF L4) | 3.0 | ±0.5 | Cavity-resonance suppression |
| | Palladium-polyester (EMF L3) | 0.1 | — | Tarnish-immune RF liner |
| | Mu-metal 0.2 mm + PETG laminate (EMF L2) | 0.5 | ±0.1 | ELF magnetic |
| **L3** | CFRP structural shell + RF outer (EMF L1) | 2.5 | ±0.3 | Structural **and** 30–50 dB RF — one part serves both |
| | **L2+L3 subtotal (outer bowl)** | **~6** | | Coil formers add local thickness only |
| | **TOTAL scalp → exterior** | **~30–35** | | Crown exterior ≈ 157 + 32 ≈ **189 mm** |

**Key geometric consequences**
- **Two user-separable interfaces, both physical seams — not shield cuts** (np_hex_zm_001
  §5.2): (i) L0↔L1 module swap, reached from the scalp side; (ii) L1↔outer-bowl, reached
  by unclamping the four corner latches to expose the cluster levers on L1's outer face.
- **Edge + distributed inner attachment (answers the "inner attachment points" question).**
  L1 is reinforced against the outer bowl at the **four rim latches** (the "outer edge"
  attachment) *and* at the **cluster-clamp bosses**, which sit in the inter-tile gaps —
  so they add mid-span stiffness to the scalp-facing plane **without costing any module
  coverage**. That is the inner attachment you asked about, made free by the lattice gaps.
- **Ear boundary:** the vault stack ends at the lattice's ear cut-out; the ear-cup
  subassembly mounts to the L3 rim there (separate part family, its own seal).
- **TMS window (T2):** the CFRP L3 is broken by a non-conductive CFRP window at the coil
  site (prevents eddy-current field loss); local L2 mu-metal is routed around it.

---

## 3. PHASE 2 — Material & process constraints per station

### 3.1 L0 — skull-facing composite (per-module faces + gap seal + plugs/caps)

L0 is **not one part**. It is four cooperating element classes, each with its own
constraint set. The unifying rule: **each module is individually IP-sealed at its
body**; the *face* is impermeable for optical modules and deliberately conductive
(hence locally non-sealed at the electrode) for electrode modules, with the seal moved
to the pod perimeter.

| L0 element | Hard constraints | Candidate materials | Lead recommendation |
|-----------|------------------|---------------------|---------------------|
| **Optical window** (T1-A/C, visual) | >90 % T at 660–1170 nm; impermeable; anti-fouling; withstand NIR flux + 42 °C + thermal cycle; flex; food/clean; ISO 10993; scratch | (a) PDMS + 75 nm SiO₂ interlayer + O₂-plasma (specced); (b) hard-coated polycarbonate; (c) COC/COP (Zeonor); (d) optical LSR | **PDMS (specced)** for scalp-contact anti-fouling where conformal contact matters; **COC** for the visual lens (clarity + low water uptake + autoclavable). PC only if cost forces it (NIR-yellowing risk). |
| **Electrode face** (T1-B, T2) | Galvanic skin contact; low, stable half-cell potential; chloride-corrosion resistant; biocompatible; cleanable | Sintered **Ag/AgCl** (dual-rated, specced) with consumable hydrogel tip (T1) / wet gel (T2) | **Sintered Ag/AgCl**, hydrogel tip a consumable (already in model). Seal is at the **pod perimeter gasket**, not the contact. |
| **Inter-socket gap seal** | Block liquid/solid ingress into L1 around every module; compliant to module swap; cleanable; temp-stable; biocompatible | (a) **Per-socket LSR gasket co-molded to the socket rim**; (b) continuous perforated silicone "grommet sheet" | **Per-socket co-molded LSR land** (§4 trade). Matches the per-module swap model; isolates a leak to one socket. |
| **Blanking plug** (empty/add-on sockets) | Same seal + retention as a live module; identifies socket as inactive; cleanable | Overmolded PC core + LSR seal face, opaque | **Overmolded PC/LSR plug** on the identical hex footprint — an empty socket seals exactly like a filled one. |
| **Electrode fouling/hydration cap** | Protect Ag/AgCl when module uninstalled; hold hydration (WVTR <0.5 g/m²/day, existing spec); tethered (loss-proof) | Silicone hydration cap (existing part) | **Reuse the existing moisture-barrier silicone hydration cap**, tethered per the loss-prevention design principle. |

### 3.2 L1 — socket layer / inner bowl (structural module carrier)

| Constraint | Requirement | Why |
|-----------|-------------|-----|
| Dimensional stability | Hold ~80 sockets to 10-20 registration tolerance across 60–110 °F | REG-1; ±12 mm pod travel gives margin but the bowl may not creep |
| Stiffness | React head-contact + Boa fit loads; carry cluster clamps + 4 latches | It is the structural backbone of the serviceable assembly |
| **Non-magnetic / low-eddy** | No ferromagnetic or high-conductivity bulk | **Fluxgate magnetometers mount on L1** (np_hex_zm_001 §5.3c); a conductive/ferrous bowl would corrupt active cancellation |
| Heat egress | Conduct module heat outward toward the outer bowl / vents | 42 °C scalp cap; ~17–20 W standard over ~1000 cm² ≈ 0.02 W/cm² (modest); peaks 45–74 W are short-duty, supercap-buffered |
| Not skin-contact | Biocompatibility not required (modules are the contact) | Simplifies material choice |

**Candidates:** (a) **PA66-GF30** — stiff, cheap, ~120 °C service, complex-moldable;
(b) **glass-filled PBT** — best dimensional stability + chemical resistance, slight cost
premium; (c) PC-ABS — tough but lower stiffness/heat; (d) CFRP — **rejected here**
(conductive → fluxgate interference; wrong bowl; unnecessary inside the Faraday envelope;
cost). **Lead: glass-filled PBT** for socket-registration stability, **PA66-GF30** as the
cost-down fallback. Add **boron-nitride-filled thermally-conductive polymer bosses** at
module heat pickups + molded vent paths for the peak-power configs.

### 3.3 L2 + L3 — outer bowl (EMF stack + structural shell, bonded)

| Constraint | Requirement | Why |
|-----------|-------------|-----|
| Structure + impact | Primary stiffness, drop/impact protection, mounts ear cups + Boa + latches | It is the "never-opened" body |
| RF/ELF envelope | Unbroken 5-layer stack on its inner face; 35–45 dB ELF, 40–60 dB RF combined | np_hex_zm_001 §5.1; mu-metal continuity depends on staying one bowl |
| No rust / lifetime shielding | Corrosion-immune inner liner | **Pd-polyester** chosen over silver for tarnish immunity (permanent-shielding claim) |
| Thermal shock / dissipate | Sink module + coil heat to ambient without cracking | 0–100 % RH, 60–110 °F |
| TMS window (T2) | Non-conductive CFRP patch at coil site | Prevents eddy-current field loss |

**Candidates for the structural + RF-outer role:** (a) **CFRP (specced)** — stiff, light,
30–50 dB RF (doubles as EMF L1), needs the non-conductive TMS window; (b) glass-filled
nylon + full reliance on the metal EMF layers for RF — cheaper, heavier; (c) aluminum —
excellent RF/heat but heavy, eddy issues, cost. **Lead: CFRP as specced** — the permanent
shielding claim and weight budget depend on it; the metal EMF liners (mu-metal, Pd-polyester)
laminate to its inner face, absorber foam inboard of those.

---

## 4. PHASE 3 — Key structural elements

Complete inventory of load-bearing features across the stack (▲ = new/refined by this brief):

**Sealing & face plane (L0)**
- ▲ Per-socket co-molded LSR gasket land (environmental seal + sacrificial contact land)
- ▲ Sacrificial bezel standing +0.6 mm proud of each face (impact takes the bezel, not the window/electrode — *your "let the cheaper part take the hit," realized without a shield*)
- ▲ Blanking plugs (empty/add-on sockets) on the module footprint
- ▲ Tethered electrode hydration/fouling caps
- Module retention + asymmetric orientation key (one mount orientation)

**Serviceable core (L1 inner bowl)**
- ~80 keyed socket bodies on the parity-alternating lattice
- Cluster-clamp actuators, one per 7-hex "flower" (~4–12 clusters) on the outer face
- Spring-decoupled electrode pods (80–120 g, ±12 mm, Shore 30A)
- FPC addressing channels + per-socket power
- Fluxgate magnetometer mounts

**Clamp / seam (L1 ↔ outer bowl)**
- Four rim layer-latches (AL/AR/PL/PR) — symmetric four-corner clamp
- BeCu hard-gold spring fingers at each latch (shield-to-ground bond, ≤50 mΩ)
- Posterior-center blind-mate sensor/coil boss (not a latch)
- Labyrinth rim lip (≤2.5 mm residual slot at 6 GHz) + conductive elastomer bead

**Outer bowl (L2+L3)**
- CFRP structural shell + laminated mu-metal / Pd-polyester / absorber foam
- Helmholtz coil formers (fixed geometry — calibration depends on it)
- Non-conductive CFRP TMS window (T2)
- Ear-cup rim mount interface (separate subassembly)

**Fit system (spans L1/L3)**
- Boa occipital dial + PTFE-lined arch (10 cm, 0.5 mm/click, 50 k cycle)
- 5-position forehead bridge (5 mm steps)
- Snap-on temporal stability wings

(Supercapacitor buffering lives in the control **hub**, not the helmet — noted so the
helmet thermal/mass budget excludes it.)

---

## 5. Impact strategy without a shield (design resolution)

With no continuous shield, the module faces are the innermost surface — but they are
**recessed inside a concave bowl**, so on a flat set-down the **L3 rim** contacts first
and the faces never touch the surface. For point impacts *into* the bowl, the
**sacrificial bezel land (+1.0 mm on optical modules)** bridges a flat intruding object
across module boundaries so pressure lands on the cheap, replaceable gasket/bezel rather
than the optical window or electrode. This preserves your stated preference — protect the
more expensive modules, let the cheap part take damage — **without** reintroducing a shield
that would block electrode current. Modules therefore normally make **no structural
contact** with anything inboard; rigidity comes from L1 + the rim/cluster attachment to L3.

**The bezel is dual-purpose (NP-THERM-BEZEL-001).** The same standoff that protects the
window also **thermally decouples the scalp from the module face**, which is what lets a
module run its junction to the 62 °C throttle for dose while the scalp stays ≤ 42 °C. The
value was raised 0.6 → **1.0 mm** on that analysis (negligible optical/EEG cost, ~1.5 K
easier face-cooling target). The **electrode-pod aperture is bezel-free** — electrode
modules contact skin (s = 0), which is harmless because they are the low-power module type.

---

## 6. PHASE 4 — MTBF vs. cost comparison

> **FMEA-RECON done (2026-07-21) — see NP-FMEA-GEOM-001.** Reconciling these estimates against the
> repo showed NP-FMEA-001 is **software-only** (SW-01 firmware) and hardware failure modes live in
> NP-RISK-001, which had **no layer-stack entries** — so there was nothing to merge into. The
> mechanical failure modes are now expressed in the NP-RM-001 S×P risk framework in
> `docs/np_fmea_geom_001.md`, cross-referenced to existing RISK IDs and software backstops. Two
> results feed back here: (1) a **new un-interlocked failure mode** — fan/vent degradation can push
> heat scalp-ward while the junction NTC still reads safe (FMEA-G07-01, ties THERM-1 to a safety gap,
> OI-GEOM-FMEA-01); (2) **G06-01 = RISK-20** (CFRP slot-rim Ra, already OPEN/BLOCKING). The tables
> below are retained for the **material trade rationale**; the authoritative risk view is the FMEA doc.

MTBF figures below are **design-stage estimates** (dominant-failure-mode reasoning, order-of-
magnitude); real MTBF/L10 numbers come from the reliability tests named in NP-FMEA-GEOM-001's
"MTBF source" column, not from assertion. Cost is relative ($ lowest → $$$ highest), anchored to the
existing BOM tables where possible.

### 6.1 L0 optical window

| Option | Dominant failure mode | MTBF driver (est.) | Cost | Verdict |
|--------|----------------------|--------------------|------|---------|
| PDMS + SiO₂ interlayer (specced) | Bond peel under thermal cycling; surface scratch | Peel life gated by the 200-cycle IEC 60068-2-14 qual (BLOCKING); anti-fouling extends clean interval | $$ (plasma+sputter process) | **Lead for scalp contact.** Conformal + anti-fouling; qual is the gate. |
| Hard-coated PC | NIR yellowing (years); coat craze | ~years to optical drift; UV/NIR dose dependent | $ | Cost-down only; monitor NIR aging. |
| COC/COP | Brittle crack on point impact | High if bezel-protected (§5) | $$$ | **Lead for the visual lens** (clarity, low water uptake). |

### 6.2 L0 gap seal — per-socket gasket vs. grommet sheet

| Option | Failure mode | System MTBF driver | Cost | Verdict |
|--------|-------------|--------------------|------|---------|
| **Per-socket co-molded LSR land** | Local gasket set/tear at one socket | ~80 seals, but failures **isolated + locally replaceable**; MTBF = per-seal life (high for LSR) | $$ (80 small features, co-molded) | **Recommended.** Matches the per-module swap model; one bad seal ≠ whole-helmet service. |
| Continuous grommet sheet | Tear anywhere; swap-cycle stress concentrates | Fewer install interfaces (higher assembly MTBF) but **any** tear = whole-sheet replacement; every module swap flexes the sheet | $ (one molding) | Rejected. Fewer leak paths, but monolithic replacement fights the modular architecture. |

### 6.3 L1 socket layer material

| Option | Failure mode | MTBF driver | Cost | Verdict |
|--------|-------------|-------------|------|---------|
| **Glass-filled PBT** | Socket-alignment creep over thermal cycles | Low creep → best registration hold | $$ | **Lead.** Stability protects REG-1. |
| PA66-GF30 | Moisture uptake dimensional shift; creep | Moderate (needs conditioning) | $ | Cost-down fallback. |
| PC-ABS | Heat-soften / stiffness loss | Higher drift near 43 °C ambient + module heat | $ | Rejected on stability. |
| CFRP | (n/a — fluxgate interference) | — | $$$ | Rejected (conductive, wrong bowl). |

### 6.4 L1 clamps / latches

| Option | Failure mode | MTBF driver | Cost | Verdict |
|--------|-------------|-------------|------|---------|
| **Cluster clamps (4–12) + 4 rim latches** | Spring/detent fatigue; BeCu finger fretting | **4–10× fewer** moving parts than per-module levers (np_hex_zm_001 §5.4a); hard-gold plating arrests fretting on the ≤50 mΩ bond | $$ | **Recommended** — fewest failure points for the swap function. |
| Per-module levers (30–42) | Same, ×30–42 | 30–42 spring+pin+detent sets = 30–42 failure points | $$$ | Rejected (part count, interference). |

### 6.5 Outer bowl (L2+L3)

| Option | Failure mode | MTBF driver | Cost | Verdict |
|--------|-------------|-------------|------|---------|
| **CFRP + mu-metal + Pd-polyester + foam (specced)** | Impact delamination (rare); liner corrosion | Pd-polyester **tarnish-immune for device life** (permanent-shielding claim); mu-metal PETG-encapsulated | $$$ | **Lead.** Shielding claim + weight depend on it. |
| Glass-nylon + metal shield | Lower stiffness; RF wholly on metal layers | Adequate but heavier; RF margin thinner if a metal layer degrades | $$ | Cost-down only; weakens the permanent-shielding claim. |

### 6.6 Roll-up recommendation

Lead stack: **PDMS/COC L0 windows · sintered Ag/AgCl electrodes with tethered caps ·
per-socket co-molded LSR seals + PC/LSR blanking plugs · glass-filled PBT socket layer ·
cluster clamps + hard-gold BeCu bonds · CFRP outer bowl with the specced 5-layer stack.**
This maximizes serviceability MTBF (failures isolated to swappable parts), holds REG-1
registration, and preserves the permanent-shielding and non-magnetic-inner-bowl invariants,
at a mechanical BOM consistent with the Home Standard $405 target (the CFRP outer bowl is
the single biggest mechanical line; everything inboard is low-cost polymer + LSR).

---

## 7. Constraints traceability (your requirements → where met)

| Requirement | Met by |
|-------------|--------|
| Impermeable to liquids/solids, permeable to signals | Per-module IP seal (§3.1); optical windows pass 660–1170 nm; electrodes pass current via galvanic contact; pod-perimeter gasket seals around the conductive contact |
| Withstand module radiation, thermal cycles/gradients, flex, food/clean | PDMS 200-cycle qual (§6.1), COC/Ag-AgCl chemical resistance, ISO 10993 + food-contact-grade cleanability (§0) |
| Strength from material + shape + edge/inner attachment to shell | Concave shell stiffness (§1); rim latches (edge) + cluster bosses (inner, gap-sited) (§2) |
| Three parts firm together, comfortable, damage-resistant on head | Four-corner clamp + labyrinth seal (§4); ~30–35 mm protective stack; recessed faces + bezel (§5) |
| Modules may support rigidity, but shield damage over module damage | No shield; modules make no inboard contact; sacrificial bezel takes the hit (§5) |
| Heat dissipation, thermal shock, no rust/crack, ≤110 °F under helmet | 42 °C scalp cap governs (< 110 °F); BN-filled thermal bosses + vents (§3.2); Pd-polyester non-corroding; CFRP/PBT thermal-shock tolerant |
| Add-on modules: attachment + power, sockets plugged when unused | Universal hex socket + power (np_hex_zm_001); blanking plugs + tethered caps (§3.1) |

---

## 8. Open items / gates

- **REG-1** — 10-20 registration must be fixed against shell CAD before socket positions
  (hence L1 tooling) lock. Inherited from np_hex_zm_001.
- **PDMS-QUAL** — 200-cycle IEC 60068-2-14 thermal-cycle qualification of the PDMS–PI bond
  is BLOCKING for the L0 optical window (CLAUDE.md §3 PBM).
- **SEAL-1** — validate the per-socket LSR land against a full ingress test (IPX-class TBD)
  with modules seated, swapped, and blank-plugged.
- **THERM-1** — *re-specified by NP-THERM-BEZEL-001.* Criterion is now a **module-face-temperature
  ceiling** (~44 °C at 0.6 mm bezel, ~45.5 °C at 1.0 mm) that the **forced-convection** outward path
  (fan + vents + BN bosses) must hold at T1-peak (45–50 W) and T2-peak (70–74 W), treating the scalp
  as near-adiabatic; the junction may run to its 62 °C throttle independently. **THERM-1a BCs now
  specified (NP-THERM-CFD-001, cases C1–C6); the fan-loss failure mode is covered by the drafted
  interlock (NP-REQ-FANHEALTH-001, SR-FAN-01…06).** Sub-gates: THERM-1a (run the CFD), THERM-1b
  (scalp-phantom fan-stall bench), THERM-1c (perfusion assumption) remain OPEN.
- **BEZEL-1** — *resolved by NP-THERM-BEZEL-001 (PASS with spec change).* Bezel does not degrade PBM
  dose (<1 % geometric, no added Fresnel, closed-loop metered) or EEG contact (5–8 % of pod travel).
  Value set to **1.0 mm** on optical modules, electrode-pod aperture bezel-free. Remaining physical
  checks BEZEL-1a (comfort/fit 0.6 vs 1.0 mm) and BEZEL-1b (pod seating with offset) are OPEN.
- **FMEA-RECON** — *done (NP-FMEA-GEOM-001).* Mechanical failure modes re-expressed in the NP-RM-001
  S×P framework; cross-referenced to RISK-14/15/20 + software backstops (FMEA-M04/M06). Surfaced the
  new thermal-path gap (FMEA-G07-01 / OI-GEOM-FMEA-01) and confirmed G06-01 = RISK-20 (OPEN/BLOCKING).
  Remaining: author these into NP-RISK-001 proper (OI-GEOM-FMEA-02) and fill MTBF numbers as
  reliability tests complete (OI-GEOM-FMEA-03).

## 9. Cross-references

np_hex_zm_001 (§3.4 geometry, §4a module taxonomy, §5 two-bowl shell + EMF seam) ·
CLAUDE.md §4.2 (interlocks), §4.3 (EMF stack), §4.5 (power/heat) · np_opt_psf_001
(optical resolution floor) · **np_therm_bezel_001 (THERM-1/BEZEL-1 coupling)** ·
**np_fmea_geom_001 (layer-stack hardware FMEA)** · np_fmea_001 (SW-01 software FMEA) ·
np_rm_001 / NP-RISK-001 (system risk register) · np_hw_hub_001 (hub thermal/power).

---

*Rev A is the first physical-requirements derivation of the layer stack. It formalizes,
rather than replaces, the committed two-nested-bowl architecture, and adds the per-module
L0 sealing model mandated by the 2026-07-21 principal decision to abandon the single inner
shield. Nothing here is locked until REG-1, PDMS-QUAL, and FMEA-RECON close.*
