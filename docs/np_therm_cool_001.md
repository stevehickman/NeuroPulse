# Cooling Architecture Options for the Sealed Cavity — Airflow, Liquid, Stored Coolth, and the Ambient Lever

**Project:** NeurOne
**Document:** NP-THERM-COOL-001
**Revision:** 2
**Date:** 2026-08-30
**Status:** DRAFT — DESIGN STUDY. Not a tooling, firmware or release baseline. Modifies no locked section and changes no safety requirement.
**Effective Date:** —
**Author:** NeurOne Systems Engineering
**Approved By:** — (pending design review)
**References:** NP-THERM-CFD-R1-001 Rev 1 (§2 the resistance network, §3 the inward-flux ceiling, §5 BN-boss export study, §5.3 findings, OI-R1-01…05); NP-THERM-CFD-001 (BC spec, case matrix); NP-THERM-CFD-C2-001 (§2 stack-up, §7 the 1D network); NP-THERM-BEZEL-001 (THERM-1 coupling, the 0.6–1.0 mm scalp gap); NP-REQ-FANHEALTH-001 (SR-FAN-01…06, Path B1); NP-PWR-BUDGET-001 Rev 3 (§3.2 aggregate estimate, §3.3 the three levers, OI-PWR-01/08); NP-PWRSRC-001 Rev 1 (§4.1 the cavity wall, §7.0 coverage 2/23); NP-HEX-ZM-001 (§5.1–5.3 two-bowl shell, §5.3a rim slot, §5.3c posterior boss, §5.3d mu-metal continuity); NP-DRV-SHELL-002 Rev 2 (§4.3 one aperture, segregated returns); NP-ENV-001 (§1 two envelopes, §2 survival, §5 humidity survival-only); NP-ENV-OPRANGE-001 (§2 per-modality ambient bounds); NP-DT-001 Rev 2 (DI-SAFE-13); NP-HELMET-GEOM-001 (§2 radial stack, §8 THERM-1a gate); CLAUDE.md §4.2 (42/62 °C interlocks), §4.3 (EMF stack), §4.5 (power); IEC 60601-1 (42 °C applied part); `scripts/check-thermal-network.ts`
**Related Issues:** —
**Gate:** No gate. Routes two items to principal decision (D-1, D-2) and raises `OI-THCOOL-01…09`.
**IEC 62304 Class:** — (analysis document; no code changed). No SR-FAN requirement is altered.
**Supersedes:** None — new document.
**Parent Document:** NP-THERM-CFD-R1-001

---

> **Rev 2 (2026-08-30) — `OI-R1-03` is answerable from architecture alone, and the answer is
> "outer shell only." No figure in this study changes.** Rev 1 §6.4 presented *"does the fan ventilate
> the inter-bowl gap or only cool the outer shell?"* as a live empirical question gating the §5
> baseline. It is not: the inter-bowl gap is **inside** the Faraday envelope and the fan is **outside**
> it, so the ventilating branch has no physical path that does not breach the shield (§6.4). Every
> figure here was already computed on the correct branch — the model uses R_out = 0.41 regardless of
> fan state, per `NP-PWR-BUDGET-001` §3.2 — so §4, §5 and §7 are unaffected. Two consequences are new:
> `NP-THERM-CFD-R1-001` §4's *"up to ~6 °C"* fan-loss branch describes a configuration that does not
> exist and should collapse to the 0.6 °C branch (§6.4), and the inner bowl's moulded vent paths
> currently discharge into a stagnant dead end (§6.1). New `OI-THCOOL-10` routes the closure to the
> owning document rather than asserting it here.
>
> **⚠ READ FIRST — five answers, before the analysis that produced them.**
>
> **1. Cooling the cavity does almost nothing for scalp safety, and that is not a reason to drop it.**
> Once the adopted BN-boss via is fitted, only **7.9 %** of tile heat reaches the scalp and only **2.1 %**
> uses the cavity path at all (§4). Driving the outward resistance from 0.41 to 0.13 m²K/W — everything
> in this study, stacked — moves the inward fraction from 7.9 % to **7.5 %**. **No cooling architecture
> in this document is a safety improvement, and none is offered as one.** `SR-FAN-01/03`, Path B1 and
> the `NP-ENV-OPRANGE-001` ambient gate stand exactly as written. What cooling buys is **capability**,
> and that is a different and much larger prize.
>
> **2. The capability prize is real and scales cleanly: the aggregate tile ceiling goes as 1/R_out.**
> The cavity residual is what sets concurrency, and concurrency is what sets `NP-PWRSRC-001` §7.0's
> **2-of-23 thermally achievable protocols**. At equal cavity temperature rise the full shield-safe
> stack takes the ceiling from the published ~6 tiles to **~20 (3.3×)** — see §5 and its warning label.
>
> **3. The decisive result: the shield-safe option BEATS the shield-breaching one.** Ventilating the
> cavity to outside air — the thing `NP-THERM-CFD-R1-001` §5 was written to avoid — is worth **2.05×**.
> **Sealed recirculation with a thermally-specified absorber and forced external convection is worth
> 3.28×, with the EMF envelope untouched** (§4, §6). The question "can we ventilate without breaching
> the shield" has a better answer than "yes": *breaching it was never the stronger option.*
>
> **4. All three of the principal's architectural challenges hold.** Air need not cross the shield —
> **recirculate inside it** and the dominant stagnant-gap term collapses anyway (§6.1). The motor need
> not be near the helmet — **put the blower in the hub and run dielectric tubes**, the MRI pneumatic-audio
> pattern, which also passes RF cleanly: a 20 mm bore is below TE11 cutoff at **8.8 GHz**, above the
> 6 GHz band `NP-HEX-ZM-001` §5.3a bounds, giving ~96 dB at L/D = 3 (§6.2). And lowering max ambient is
> a genuine, zero-BOM lever — **but §7 finds `NP-ENV-OPRANGE-001`'s provisional bounds have already
> taken most of it**, which is a validation of those bounds, not a new opportunity.
>
> **5. The fan does not, and cannot, ventilate the inter-bowl gap — so `OI-R1-03` needs no CFD and no
> bench.** The gap is inside the EMF envelope and the fan is outside it; the only two routes across
> (through the outer bowl, or through the rim labyrinth) each breach a stated shielding requirement.
> The open item reads as a measurement question but is settled by geometry (§6.4).
>
> **What this study does not do.** It does not resurrect Path A (§7.3 — lowering ambient does not, and
> cannot, because the fault case pins the junction at 62 °C independently of ambient). It sets no
> requirement, changes no constant, and commits no hardware. Every figure inherits `OI-R1-01`'s missing
> verification-grade CFD and is **directional**.

---

## 1. Method, and the one script

Per `NP-SES-PWR-001` **D-1** — *the audit is a committed script, not a table in a document* — every
figure below that is not quoted from another document is produced by:

```bash
bun scripts/check-thermal-network.ts
```

The model is a parallel-conductance network at the junction node, built on the resistances
`NP-THERM-CFD-R1-001` §2 and §3 publish. Two parameters are **calibrated to reproduce published
results** rather than asserted:

| Calibrated | To reproduce | Value |
|---|---|---|
| `R_via_eff` | R1 §5.1's 90 % export fraction | **0.0096 m²K/W** |
| Cavity ΔT budget | `NP-PWR-BUDGET-001` §3.2's ~6-tile rule | **6.7 K** |

Calibrating rather than asserting matters twice over. It keeps this study consistent with the
document it extends, and it produces the first finding in §3.

**Validation.** The network returns **78.8 %** of junction heat flowing inward at baseline against
R1 §2's published **~83 %** — within 4 pp on a 1D network against a 2D FD result, which is the
agreement needed for the *ratios* in §4 and §5 to carry weight.

### 1.1 Five limits every figure inherits

1. **`OI-R1-01` is open.** The verification-grade 2D-axisymmetric CFD has not been run. R1 §8 calls its
   own numbers directional; this study is a layer on top of them.
2. **`OI-PWR-08` bounds the lumped aggregate model to roughly N ≤ 8 tiles.** §5 predicts ceilings of
   10–20 tiles, which is **outside the validity of the model producing it.** §5 states the ratio and
   refuses the absolute number for exactly this reason.
3. **The 6.7 K cavity budget is calibrated to the published ~6-tile rule**, so absolute tile counts
   inherit whatever uncertainty that rule carries. The **ratios** are the load-bearing output.
4. **§7's ambient crossovers are a two-point linear fit** through R1 §5.1's only two published ambients
   (25 °C and 43.3 °C), assuming a config-independent slope. Treat as ±2–3 °C.
5. **The convection coefficients are assumed, not measured** — h = 30 W/m²K stirred, h = 30 forced
   external, against h ≈ 10 natural. `OI-THCOOL-03`.

---

## 2. The outward path, decomposed

R1 §2 publishes the outward path as **0.41 m²K/W total, of which the stagnant inter-bowl air gap is
0.23**. The remaining 0.18 is unallocated in the source. Allocating it from `NP-THERM-CFD-C2-001` §2's
stack is what makes each term separately attackable:

| Term | R (m²K/W) | Share of outward path | Attackable by |
|---|---:|---:|---|
| **Stagnant inter-bowl air gap** (6 mm, k = 0.026) | **0.23** | 56 % | stirring the gas (§6.1) |
| **Carbon-loaded EMI absorber foam** (3 mm, k ≈ 0.04) | **0.075** | 18 % | specifying it thermally (§6.3) |
| **External natural convection** (h ≈ 10) | **0.10** | 24 % | forced external air (§6.4) |
| Shell — CFRP 2.5 mm + Pd-polyester + mu-metal | 0.005 | 1 % | nothing; already negligible |
| — inward path, junction → perfused core | **0.11** | — | **nothing. This is a floor.** |

Two things fall out immediately.

**The EMI absorber foam is a thermal insulator that nobody thermally specified.** It is Layer 4 of the
CLAUDE.md §4.3 stack, chosen for cavity-resonance suppression in dB. At ~0.075 m²K/W it is **18 % of
the entire outward resistance** — the second-largest term after the air gap, larger than external
convection would be with a fan on it. It appears in no thermal requirement anywhere in the document
set. **`OI-THCOOL-04`.**

**The inward path is a floor and cannot be fought.** R_in = 0.11 m²K/W is 1.6 mm of PDMS and air to a
*perfused* sink. No outward-path work changes it. This is why §4's inward fractions asymptote, and it
is the quantitative statement of R1 §5.3's finding that the face "tracks the junction to within ~1 °C".

---

## 3. Where the via's resistance actually is

Calibrating `R_via_eff` to the published 90 % export returns **0.0096 m²K/W**. An ideal 4 mm copper via
through 14 mm at k = 400, normalised to the 1,385 mm² tile, is **~0.001 m²K/W**.

> **~90 % of the via path's resistance is not the via.** It is contact resistance at the boss, spreading
> resistance into a 4 mm target, and the real sink at the far end — not the copper.

This redirects `NP-PWR-BUDGET-001` §3.3's first lever ("improve via export efficiency beyond ~90 %").
Going to a 5 mm via bought 91 % → 93 % in R1 §5.1, which is the geometry term, already near its
asymptote. **The recoverable resistance is in the interface, not the conductor** — boss TIM selection,
mounting pressure, and sink design. `OI-THCOOL-05`.

---

## 4. Heat split — the safety question, answered negatively

`bun scripts/check-thermal-network.ts` §4:

| ID | Option | R_out | → scalp | → cavity | → exported | Shield |
|---|---|---:|---:|---:|---:|---|
| BASE | As-modelled, fan off, no via | 0.41 | **78.8 %** | 21.2 % | — | ok |
| **V** | **BN-boss export (ADOPTED)** | 0.41 | **7.9 %** | 2.1 % | 90.0 % | ok |
| X | External cavity ventilation | 0.20 | 64.5 % | 35.5 % | — | **BREACH** |
| R | Sealed recirculation | 0.25 | 7.8 % | 3.5 % | 88.8 % | ok |
| RF | R + thermal absorber | 0.19 | 7.7 % | 4.4 % | 87.9 % | ok |
| RFE | RF + forced external | 0.13 | **7.5 %** | 6.6 % | 85.9 % | ok |

**Three findings, and the first is the one that governs this study.**

**4.1 — No cooling option here is a safety improvement.** With the via fitted, the scalp fraction moves
7.9 % → 7.5 % across the entire option space. The via already won that fight; everything after it is
rearranging 2 % of the heat. **Nothing in this study justifies reopening `SR-FAN-01/03`, Path B1, or
`DI-SAFE-13`.** Any proposal that arrives claiming a cooling change improves scalp safety should be
checked against this row first.

**4.2 — Cavity ventilation was never the strong option, even ignoring the shield.** Row X — a *perfect*
external ventilation of the cavity, granted for argument — reaches 64.5 % inward. The adopted via, with
the cavity left stagnant, reaches 7.9 %. R1 §5's decision to export conductively rather than ventilate
was not a compromise forced by the shield; **it was the better thermal answer**, and the shield was
preserved for free. This study confirms that finding from an independent direction.

**4.3 — The via's dominance is why the cavity work has to be justified on capability.** Which §5 does.

---

## 5. Aggregate cavity ceiling — the capability question, answered positively

The per-tile split is not the concurrency question. Concurrency is set by the **cavity's own temperature
rise**: N tiles each dump their un-exported residual into a shared, sealed volume that rejects through
R_out over the vault. Holding the allowed rise fixed, the tolerable tile count scales as **1/R_out**.

| ID | Option | R_out | Tile ceiling | vs baseline | Shield |
|---|---|---:|---:|---:|---|
| BASE / V | As adopted | 0.41 | 6.0 | 1.00× | ok |
| X | External ventilation | 0.20 | 12.3 | **2.05×** | **BREACH** |
| R | Sealed recirculation | 0.25 | 10.0 | 1.66× | ok |
| RF | R + thermal absorber | 0.19 | 12.8 | 2.14× | ok |
| **RFE** | **RF + forced external** | **0.13** | **19.7** | **3.28×** | **ok** |

> ⚠ **The ratios are the result. The absolute tile counts are not.** The baseline is *calibrated* to the
> published ~6-tile rule, and `OI-PWR-08` bounds the underlying lumped model to roughly N ≤ 8 — so the
> 19.7 in that last row is produced by a model that is not valid there. Read it as *"the same order as
> a threefold improvement,"* never as twenty tiles. `OI-PWR-01`'s multi-tile CFD is the only thing that
> can turn this into a number.

**Why this matters more than it looks.** `NP-PWRSRC-001` §7.0 finds **2 of 23 protocols thermally
achievable under every candidate power source**, and §4.1 identifies the sealed cavity — not the supply
— as the wall. `NP-SES-PWR-001` §4's sanctioned workaround for insufficient power is *cascading*, which
`NP-PWRSRC-001` §5.5 then shows is what generates the **292 CEM43** thermal-dose exposure on Vascular
Baseline. **Raising the cavity ceiling attacks the root of both**: it is the term that makes protocols
unachievable *and* the term that makes cascading necessary. That is a stronger case for this work than
any per-session temperature figure.

**And row RFE beats row X.** The shield-safe stack outperforms the shield-breaching one by 60 %. This is
the study's central result and it is worth stating in the form the design conversation needs:

> **We do not have to choose between the EMF claim and the thermal ceiling.** The best available
> thermal answer is also the one that leaves the envelope intact.

---

## 6. The four architectural options

### 6.1 Sealed recirculation — stir the cavity, exchange nothing

**Air need not cross the shield to be useful.** The dominant outward term is the *stagnancy* of the
inter-bowl gap, not the presence of air in it. A closed loop that circulates the cavity's own fixed air
mass converts that gap from conduction (k = 0.026) to forced convection, collapsing 0.23 → ~0.067 m²K/W
without a single gas molecule crossing the envelope.

This disposes of every objection that applies to *ventilation*, because none of them was ever an
objection to *circulation*:

| Objection to ventilating | Applies to a sealed loop? |
|---|---|
| Apertures through mu-metal / Pd-polyester / CFRP | **No** — nothing is exchanged |
| Dust and particulate onto PDMS windows and the PD2 funnel | **No** — fixed, filtered-once air mass |
| Hygiene / cross-contamination in multi-patient T2 use | **No** — no path to the wearer |
| Condensation risk (`NP-ENV-001` §2.2) | **Reduced** — fixed absolute humidity, no moist make-up air |
| IP-seal and ingress qualification (`NP-FAI-001`) | **No** — the seal is unbroken |

**The inner bowl is already moulded for the half of this mechanism that does not work.**
`NP-HELMET-GEOM-001` §3.2 specifies, for the L1 inner bowl, *"boron-nitride-filled thermally-conductive
polymer bosses at module heat pickups **+ molded vent paths** for the peak-power configs"* — channels
whose function is to deliver module heat **into** the inter-bowl gap. That gap is stagnant at
0.23 m²K/W, the largest single term in the outward path (§2), and §6.4 establishes that nothing
ventilates it. **The vent paths therefore discharge into a dead end.** Getting heat into the gap is
solved and moulded; moving it across the gap is not addressed anywhere. Sealed recirculation is the
missing half of a mechanism the tooling already anticipates.

Note this is **not** `NP-PWR-BUDGET-001` §3.3's second lever. That lever proposes a better *conductive*
path to the shell via the existing metallic layers. This is convective, it attacks a different term, and
the two compose. §3.3's three-item list has no fourth entry for stirring the gas; it should. `OI-THCOOL-01`.

### 6.2 Remote blower and dielectric tubes — the MRI pneumatic pattern

**The motor does not have to be anywhere near the helmet.** MRI-suite audio drives long plastic tubes
from a remote transducer precisely because no motor or conductor may approach the magnet. The same
pattern applies here and removes three objections at once — the fan's ELF magnetic signature at the
inner-bowl fluxgates, its vibration into the ±12 mm electrode pods, and its acoustic noise beside the
bone-conduction modality.

**One honest difference from the MRI case:** pneumatic audio transmits *pressure waves*, not bulk flow.
This needs mass flow. The numbers still work — that is the point of running them:

| Bore | Velocity | Re | Δp/tube | TE11 cutoff | BCO atten @ L/D = 3 |
|---:|---:|---:|---:|---:|---:|
| 10 mm | 18.4 m/s | 11,476 | 888 Pa | 17.6 GHz | ~96 dB |
| 12 mm | 12.8 m/s | 9,563 | 373 Pa | 14.7 GHz | ~96 dB |
| 16 mm | 7.2 m/s | 7,172 | 95 Pa | 11.0 GHz | ~96 dB |
| **20 mm** | **4.6 m/s** | 5,738 | **33 Pa** | **8.8 GHz** | **~96 dB** |

*(20 W removed at a 12 K rise, 1.5 m each way.)*

**Velocity, not pressure drop, is the binding constraint** — because the audio entrainment and bone
conduction modalities are adjacent, and jet noise goes as a high power of velocity. The 10 mm bore is
pneumatically trivial and acoustically unacceptable; the 20 mm bore is the opposite trade and needs a
blower delivering only tens of pascals. **`OI-THCOOL-02` owns the acoustic bound**, which should be set
against the audio modality's noise floor, not against a comfort number.

**On the aperture — there is already exactly one, and the convention is to share it.** `NP-HEX-ZM-001`
§5.3c routes the fluxgate and Helmholtz harness through a **standalone blind-mate boss at the posterior
centreline**, so the outer bowl and its mu-metal are *already* penetrated at that point. `NP-DRV-SHELL-002`
§4.3 then establishes the governing principle explicitly, for the module interconnect: share the existing
aperture rather than defend a second one. Air tubes should follow the same rule and enter at the same
boss. This does not make the penetration free — it enlarges an existing defect rather than creating a
new class of one — and the two shielding regimes behave completely differently:

- **RF:** clean. Every bore above is below TE11 cutoff far above the 6 GHz Wi-Fi 6 band `NP-HEX-ZM-001`
  §5.3a bounds the concern to, and a dielectric tube in a conductive collar of L/D ≥ 3 gives ~96 dB —
  comfortably beyond the 40–60 dB claim.
- **ELF magnetic:** **not clean, and waveguide-below-cutoff does not apply at DC–100 Hz.** A hole in
  mu-metal is a hole. The mitigation is a mu-metal chimney collar (L/D ≥ 3–5) restoring reluctance
  geometrically. **This must be bench-measured with `EMF-1`, not argued** — the measured-shielding claim
  is the product's primary technical differentiator. **`OI-THCOOL-06`, BLOCKING on any adoption.**

### 6.3 Specify the EMI absorber thermally as well as electrically

§2's finding. 18 % of the outward path is a foam chosen only for dB. Conductive-filled absorber
elastomers reach k ≈ 1–3 W/m·K against ~0.04 for open-cell carbon foam. If the RF absorption is held,
0.075 → ~0.02 m²K/W is a materials substitution with no architectural consequence — the cheapest row in
§5's table by a wide margin. `OI-THCOOL-04`.

### 6.4 Forced external convection on the outer shell

The remaining 0.10 m²K/W, on the outer shell's **external** face — which the hub fan can reach, and
which is the only thermal service it can perform for the helmet.

**`OI-R1-03` is not an empirical question.** It asks *"does the fan ventilate the inter-bowl gap or only
cool the outer shell?"*, which reads as a measurement to be taken. It is settled by geometry:

- The inter-bowl gap is **inside** the Faraday envelope — `NP-HEX-ZM-001` §5.1: the outer bowl is the
  complete EMF envelope and the inner bowl nests inside it. The fan is **outside** that envelope, in the
  hub at the occipital arch (`NP-TOOL-HUB-001` §3, F-04).
- Air can cross that boundary only two ways, and **both breach a stated requirement**. Through the outer
  bowl contradicts `NP-HELMET-GEOM-001` §3.3's *"unbroken 5-layer stack on its inner face"* and its
  *"mu-metal continuity depends on staying one bowl"*. Through the rim defeats `NP-HEX-ZM-001` §5.3a's
  labyrinth lip, whose purpose is that there be *"no line-of-sight aperture from outside to the
  modules"* with any residual slot ≤ 2.5 mm — making that slot continuous is the slot antenna the fold
  exists to prevent.

So the ventilating branch is not a possible description of the current design; it would be a design
change. **The answer is "outer shell only."** `NP-PWR-BUDGET-001` §3.2 already asserts it in passing —
*"the cavity itself is never actively ventilated regardless of fan state — the fan cools the external
heatsink at the via terminus, not the cavity air"* — but never connects that back to `OI-R1-03`, which
is why the item still reads as open.

**Two consequences.**

1. **`NP-THERM-CFD-R1-001` §4's uncertainty band collapses.** It gives fan-loss face rise as *"~0.6 °C
   if the fan cools only the outer shell, up to ~6 °C if the fan ventilates the inter-bowl gap"*. The
   second branch describes a configuration that does not exist, so τ_face and t₄₂ should be taken on
   the 0.6 °C branch. This **tightens** the transient result and strengthens R1's own conclusion that
   response speed is not the binding constraint; `SR-FAN-04` is undisturbed.
2. **Nothing in this study moves.** §4, §5 and §7 use R_out = 0.41 — the fan-off outward path — as the
   cavity's rejection resistance throughout, on §3.2's justification above. Every figure was already
   computed on the correct branch.

**The `NP-HELMET-GEOM-001` "vents" are not a counterexample.** Its vent references trace back to
§3.2's **L1 inner bowl** — moulded channels inboard of the shield that carry module heat into the
inter-bowl gap; the §7 traceability row cites §3.2 explicitly, and the §8 THERM-1 gate's *"fan + vents
+ BN bosses"* names the same features. **No reference anywhere specifies an opening through the outer
bowl, and §3.3 forbids one.** See §6.1 for what this implies.

**This item is not closed here.** `OI-R1-03` belongs to `NP-THERM-CFD-R1-001`; the disposition above is
routed to its owner as `OI-THCOOL-10` rather than marked closed by a document that does not own it.

### 6.5 Two options assessed and not recommended

**Liquid cooling.** Rejects to ambient like air, so it clears no wall air does not. It has one real
structural advantage — it crosses the envelope as a *dielectric tube*, no conductor, routable through the
same below-cutoff collar — but that advantage is equally available to air (§6.2), which does not put
coolant over conductive applied parts. Against it: leak risk with `NP-DT-001` **VE-11** open at the
510(k) tier, pump complexity, and no thermal capability the copper via is not already delivering at 90 %.

**On-head thermoelectric.** Goes sub-ambient, but pumps 2–4× the heat into the path that is already the
constraint, on a supply `NP-PWRSRC-001` §4.1 finds oversubscribed by 4.7–8.4×, with DC loops beside the
fluxgates. `NP-ENV-OPRANGE-001` already records that the T2-D TEC cannot hold setpoint above +35 °C
ambient — the approach is weakest at exactly the ambient that matters.

### 6.6 Stored coolth (PCM) — the only sub-ambient option, held in reserve

A phase-change pack melting at ~28–32 °C in the export path absorbs heat at constant **sub-ambient**
temperature. It is passive, silent, zero-EMF, needs no aperture, and sessions are bounded at 6–30 min:

| Load | Duration | Latent mass |
|---:|---:|---:|
| 10 W | 20 min | 60 g |
| 20 W | 20 min | 120 g |
| 20 W | 30 min | 180 g |

A real pack is 2–3× this with matrix, shell and sensible heat — so **~0.2–0.5 kg**, feasible but not
trivial head-borne mass. Two limits: it is a **capacity, not a rate**, so it saturates; and a 30 °C PCM
stored in a 40 °C room arrives already melted. That second limit points at the only version worth
building — **re-freeze it in the charging dock**, putting the heat pump where power, noise and EMF are
free and carrying stored coolth onto the head. Recorded, not recommended: §7 finds the ambient envelope
is a cheaper answer to the same problem.

---

## 7. The ambient lever — real, zero-BOM, and largely already taken

**43.3 °C is a choice, not physics**, and R1 §5.3's wall ("ambient 43.3 °C already exceeds the 42 °C
limit") is a consequence of that choice. Lowering the maximum operating ambient dissolves it at zero BOM
cost. Fitting R1 §5.1's two published ambients gives the max ambient at which each configuration holds
face ≤ 42 °C in the healthy state, with the via fitted:

| Config | Face @ 43.3 °C | **Implied max ambient** | `NP-ENV-OPRANGE-001` full-dose bound |
|---|---:|---:|---|
| T1-std (125 mW/cm²) | 46.7 °C | **37.9 °C** | +35 † |
| T1-peak (190 mW/cm²) | 48.9 °C | **35.4 °C** | +35 † |
| T2-peak (300 mW/cm²) | 52.6 °C | **31.2 °C** | +30 † (T2-D) |

**7.1 — This validates the provisional bounds rather than replacing them.** `NP-ENV-OPRANGE-001`'s
†-marked numbers were placeholders explicitly awaiting THERM-1a C3/C4. They land within ~1–3 °C of what
the R1 data independently implies. **The envelope has already absorbed most of the ambient lever**, which
is why lowering it further buys less than it first appears — and is a good outcome for those bounds.

**7.2 — What remains is the derate band, and it is a product decision.** The band from full-dose to block
(+35 → +43 for T1-A) is the region where the device runs at reduced duty. Narrowing it — say, blocking at
+38 rather than +43 — would align the *block* threshold with the physics rather than sitting 5 °C beyond
it. The cost is availability in hot conditions for a home wellness product, which is a commercial call
about returns and support load, not a thermal one. **D-1, principal.**

**7.3 — Lowering ambient does NOT resurrect Path A, and cannot.** The C2 fault case pins the junction at
its 62 °C throttle setpoint and solves for the face; the face lands at 60.2 °C because it is 1.6 mm from
a 62 °C plane, **almost independently of ambient**. Ambient is not the variable in that case. `SR-FAN-01`,
`SR-FAN-03` and Path B1 stand unchanged. Similarly, R1 §3's fan-off safe-duty ceiling improves only from
~4.5 to ~9.0 mW/cm² between 43.3 °C and 25 °C — **halt-or-trickle at both ends.** No ambient choice
changes the character of the fan-fault derate.

---

## 8. Recommendation

**Sequenced, cheapest-first. Nothing here is a safety change; §4.1 governs.**

| # | Action | Cost | Worth | Owner |
|---|---|---|---|---|
| 0 | **Close `OI-R1-03`** — pin the existing fan's airflow path | ~0 | Gates every baseline below | ME + Thermal |
| 1 | **Thermally specify the EMI absorber** (§6.3) | Materials substitution | 0.075 → ~0.02 | ME + EMC |
| 2 | **Attack the via interface, not the via** (§3) | TIM + mounting | ~90 % of via-path R | ME |
| 3 | **Sealed recirculation, remote blower, tubes at the existing posterior boss** (§6.1–6.2) | New subsystem | 0.23 → ~0.067 | ME + Thermal + EE |
| 4 | **Forced external convection** (§6.4) | Depends on step 0 | 0.10 → ~0.033 | ME |
| — | *Liquid, on-head TEC, scalp-gap ventilation* | — | **Not recommended** (§6.5, §9) | — |

Steps 1 and 2 are incremental work on an adopted architecture with no EMF, regulatory or architectural
consequence, and together are worth roughly half of §5's total. **Do them regardless of what is decided
about step 3.**

**Two decisions for the principal:**

- **D-1 — Does the T1-A block threshold move from +43 °C toward the ~+38 °C the physics implies (§7.2)?**
  A commercial availability call, not a thermal one. Thermal input: +43 is ~5 °C beyond what the design
  supports at full dose, and the derate ramp is already carrying that gap.
- **D-2 — Is a sealed pneumatic loop in scope at all?** It is the largest single term (§5), it is
  shield-safe in principle, and it is a new subsystem on a product with no committed hardware. **It must
  not be adopted before `OI-THCOOL-06`** (the ELF magnetic bench measurement) returns.

---

## 9. What this study rejects, and why

**Ventilating the scalp gap** (inner shell to skull) was assessed and is not carried forward. It is
thermodynamically the correct node — a forced film at the face against 25 °C air removes ~425–850 W/m²
against an inward budget of 47.4 W/m² — and it need not touch the shield, since the helmet mouth is
already open. Four things defeat it:

1. **It inverts at the case it would have to survive.** At 43.3 °C ambient the same film *adds* ~65 W/m²
   to a 42 °C-limited surface. It can never be a safety mechanism.
2. **The channel does not exist.** The gap is 0.6–1.0 mm (`NP-HELMET-GEOM-001` §2, 1.0 mm on optical
   modules), packed with hair — hair being why optical contact is unachievable in the first place
   (`NP-THERM-BEZEL-001` §2) — and discontinuous, since T1-B electrode pods protrude *past* the bezel
   plane to skin contact, blocking flow at every electrode site.
3. **It is the optical path.** It contains the PDMS windows, the PD1 baffle and the PD2 backscatter
   aperture. Forced unfiltered air across them is a fouling accelerator on the surfaces the J/cm² dose
   claim depends on. Filtering returns to point 2.
4. **Hygiene.** T2 is multi-patient by design; forced air over a scalp and back into the assembly is a
   cross-contamination path and fights the `NP-FAI-001` ingress qualifications.

**External cavity ventilation** is rejected on §4.2 and §5: it is worth less than the shield-safe
alternative *and* costs the ELF magnetic claim. It should not be revisited.

---

## 10. Open items

| Ref | Item | Owner | Blocking? |
|---|---|---|---|
| **OI-THCOOL-01** | Add convective stirring of the cavity gas as a fourth lever in `NP-PWR-BUDGET-001` §3.3, whose three-item list covers only conduction | Thermal | No |
| **OI-THCOOL-02** | Set the tube acoustic-velocity bound against the audio/bone-conduction noise floor, not a comfort number; it sizes the bore | ME + Audio | No |
| **OI-THCOOL-03** | Replace assumed convection coefficients (h = 30 stirred, 30 forced external, 10 natural) with CFD or bench values | Thermal | No |
| **OI-THCOOL-04** | Thermally specify the Layer 4 EMI absorber — 18 % of the outward path, currently specified in dB only | ME + EMC | No |
| **OI-THCOOL-05** | Characterise the via *interface* (contact + spreading + sink), which §3 shows is ~90 % of that path's resistance | ME | No |
| **OI-THCOOL-06** | **Bench-measure ELF magnetic leakage through a mu-metal chimney collar at the posterior boss with tube penetrations.** Waveguide-below-cutoff does not apply below ~100 Hz | EMC (EMF-1) | **BLOCKING on §6.2** |
| **OI-THCOOL-07** | Confirm the sealed loop's condensation behaviour across the `NP-ENV-001` §2.2 warm-up transient — fixed absolute humidity should help, but the cold-optics case is untested | Thermal | No |
| **OI-THCOOL-08** | Re-run §5 against `OI-PWR-01`'s multi-tile CFD; the ratios need a valid model at N > 8 before any number is quoted | Thermal | **Gates §5 numbers** |
| **OI-THCOOL-09** | Assess whether tubes at the posterior boss disturb `NP-DRV-SHELL-002` §9.3's loop-area control or the §4.3 segregated-return requirement | EE | No |
| **OI-THCOOL-10** | **Close `OI-R1-03` on the architectural grounds in §6.4** (answer: outer shell only) and collapse `NP-THERM-CFD-R1-001` §4's *"up to ~6 °C"* fan-loss branch to the 0.6 °C branch, re-stating τ_face / t₄₂ accordingly. Owned by `NP-THERM-CFD-R1-001`, not by this document — raised, not actioned | Thermal | No |

---

## 11. Cross-references

`NP-THERM-CFD-R1-001` (the network, the via study, the ambient wall) · `NP-THERM-CFD-001` / `-C2-001`
(BCs, stack-up) · `NP-REQ-FANHEALTH-001` (SR-FAN — **unchanged by this study**) · `NP-PWR-BUDGET-001`
§3.2/§3.3 (aggregate ceiling, the three levers this adds a fourth to) · `NP-PWRSRC-001` §4.1/§7.0 (the
cavity wall, 2/23 coverage) · `NP-HEX-ZM-001` §5.1–5.3 (two-bowl shell, the one existing aperture) ·
`NP-DRV-SHELL-002` §4.3 (share the aperture) · `NP-ENV-OPRANGE-001` (the ambient bounds §7 validates) ·
`NP-DT-001` DI-SAFE-13 · CLAUDE.md §4.2/§4.3/§4.5 · `scripts/check-thermal-network.ts`
