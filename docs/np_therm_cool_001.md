# Cooling Architecture Options for the Sealed Cavity — Airflow, Liquid, Stored Coolth, and the Ambient Lever

**Project:** NeurOne
**Document:** NP-THERM-COOL-001
**Revision:** 9
**Date:** 2026-09-02
**Status:** DRAFT — DESIGN STUDY. Not a tooling, firmware or release baseline. Modifies no locked section and changes no safety requirement.
**Effective Date:** —
**Author:** NeurOne Systems Engineering
**Approved By:** — (pending design review)
**References:** NP-THERM-CFD-R1-001 Rev 1 (§2 the resistance network, §3 the inward-flux ceiling, §5 BN-boss export study, §5.3 findings, OI-R1-01…05); NP-THERM-CFD-001 (BC spec, case matrix); NP-THERM-CFD-C2-001 (§2 stack-up, §7 the 1D network); NP-THERM-BEZEL-001 (THERM-1 coupling, the 0.6–1.0 mm scalp gap); NP-REQ-FANHEALTH-001 (SR-FAN-01…06, Path B1); NP-PWR-BUDGET-001 Rev 3 (§3.2 aggregate estimate, §3.3 the three levers, OI-PWR-01/08); NP-PWRSRC-001 Rev 1 (§4.1 the cavity wall, §7.0 coverage 2/23); NP-HEX-ZM-001 (§5.1–5.3 two-bowl shell, §5.3a rim slot, §5.3c posterior boss, §5.3d mu-metal continuity); NP-DRV-SHELL-002 Rev 2 (§4.3 one aperture, segregated returns); NP-ENV-001 (§1 two envelopes, §2 survival, §5 humidity survival-only); NP-ENV-OPRANGE-001 (§2 per-modality ambient bounds); NP-DT-001 Rev 2 (DI-SAFE-13); NP-HELMET-GEOM-001 (§2 radial stack, §8 THERM-1a gate); CLAUDE.md §4.2 (42/62 °C interlocks), §4.3 (EMF stack), §4.5 (power); IEC 60601-1 (42 °C applied part); `scripts/check-thermal-network.ts` (§18, the hysteresis sizing); `firmware/safety_mcu/src/np_thermal_interlock.c` + `np_safety_config.h` (the 62/55 °C junction re-arm precedent §7.5.1 declines to copy)
**Related Issues:** —
**Gate:** No gate. D-1, D-2 and D-3 are all **decided** (2026-08-30/31, principal); raises `OI-THCOOL-01…17`, of which `OI-THCOOL-06` and `OI-THCOOL-16` are closed.
**IEC 62304 Class:** — (analysis document; no code changed). No SR-FAN requirement is altered.
**Supersedes:** None — new document.
**Parent Document:** NP-THERM-CFD-R1-001

---

> **Rev 9 (2026-09-02) — `OI-THCOOL-16` CLOSED. New §7.5 specifies the hysteresis on the ambient hard
> edges: Δ = 1.0 °C, anchored on the *effective* block rather than on 35.0, and applied as a raised
> admission bar rather than a hold-off.** Three independent bounds land on the same number — the shipped
> sense path can only express whole degrees, a room's own thermostat differential is 0.5–1.0 °C so
> anything finer sits inside the room's oscillation, and the band is charged to the user as a 20–60 min
> wait. **The junction interlock's 7 °C band is explicitly not the precedent**: that is a reactive fault
> where over-cutting is free, this is an admission gate where the band is a lock-out. Two structural
> results: a mid-session crossing **terminates** the session rather than pausing it, which removes every
> automatic re-entry path and makes chatter impossible by construction; and because the rule is written
> against `T_block_eff`, **it holds whichever way `OI-THCOOL-17` lands, so 16 closes without waiting for
> 17.** Normative encoding in `NP-FW-POE-001` §6.1 (new). **No envelope number changes**; the block stays
> at +35 and the latch can only ever be more restrictive.
>
> **Rev 8 (2026-08-31) — the shared band's derate semantics are unspecified, and under the reading
> actually written it reopens the failure mode D-1 was built to close. New §7.4 states the question and
> proposes an efficacy-floor clamp.** `NP-ENV-OPRANGE-001` §1 specifies *"linear duty derate"* — duty
> scales, and **nothing says the session extends to compensate**. Under that reading a fixed-length
> session delivers proportionally less dose, and above roughly **34.2 °C** (a 60 J/cm² protocol) it
> falls below `NP-PWR-BUDGET-001` §3.4's efficacy floor: **the device runs to completion and cannot
> work.** That is precisely what the D-1 chain removed at Rev 6 and the shared band silently brought
> back, in a ~0.8 °C sliver. The alternative reading — extend the session to hold dose — preserves
> efficacy but turns 20 min into 100 min at 34 °C, and time-at-ceiling is what drives CEM43. **Neither
> reading is free, so the semantics must be chosen, not inherited.** §7.4 proposes clamping the derate
> at the efficacy floor and blocking there instead of ramping to zero. New `OI-THCOOL-17`; `OI-OPR-01`
> is scoped accordingly. **No number in §2–§6 changes; the envelope itself is untouched.**
>
> **Rev 7 (2026-08-31) — one shared thermal envelope for the whole product line: full dose ≤ +30 °C,
> derate +30 → +35, block > +35, on every helmet module and the intranasal probe.** Principal
> direction — *"consistency makes products easier to understand."* T2-D's numbers were adopted
> wholesale, since it was already the tightest and a shared envelope must be the intersection of what
> every module can do. **One sentence now describes the thermal envelope of the entire product line,
> at any tier, in any configuration.**
>
> **Two consequences, stated rather than left to be discovered. (i) T1's full-dose ceiling tightens
> +35 → +30** — a real capability reduction, deliberately more conservative than the physics requires
> (§7 puts T1-std's full-dose ceiling at 37.9 °C), bought on purpose in exchange for one envelope
> instead of four. **(ii) `OI-OPR-01` is live again.** Rev 6 recorded the T1-A derate curve as *moot*
> because the band had zero width; the band is 5 °C wide again, so that curve must be specified after
> all — but **once, for every helmet module, rather than per module.** `OI-THCOOL-16` still applies to
> the block edge at +35.
>
> **Rev 6 (2026-08-31) — two changes. (a) T1 PBM block moves +38 °C → +35 °C to match T2, which
> collapses the T1-A derate band to zero width. (b) New §6.9.1 specifies the gap-pad geometry and
> corrects §6.9's full-area assumption.**
>
> **(a) Block alignment (principal).** T2-D already blocked at +35; T1 now blocks there too, so a
> customer upgrading T1 → T2 meets the same usage limit rather than a tighter one. **The consequence is
> that T1-A's derate band vanishes** — full dose held to ≤ +35 and blocking above it leaves no
> reduced-duty region. That is a simplification, not a loss: §7.2 records that the derate band was
> exactly where sub-threshold "completed" sessions could occur, and **`OI-OPR-01`'s T1-A derate curve
> becomes moot because there is no curve left to specify.** One new item: a hard cliff at a single
> temperature needs **hysteresis** so an ambient NTC sitting on +35 cannot chatter (`OI-THCOOL-16`).
>
> **(b) §6.9.1 — pad geometry, and a correction.** §6.9's 0.0022 m²K/W assumed a **full-area** pad. The
> cluster clamps, fluxgates and the bowl-separation requirement force a **discrete, boss-co-located**
> pattern instead. Modelled properly (§16 of the script), a realistic 20 mm pad per tile gives 22.7 %
> coverage, R_gap 0.0022 → **0.0094**, and the tile ceiling **40.6 → 36.3 — only 1.1× optimistic, not
> the ~5× a first estimate suggested**, because once the gap term is small the other terms dominate
> R_out. **D-2's conclusion is unaffected**: every coverage row from 10 mm up still beats the stirred
> gap's 0.067. Coverage, not pad conductivity, is the design variable.
>
> **Rev 5 (2026-08-30) — D-1 DECIDED: the T1-A PBM block threshold moves +43 °C → +38 °C, and with it
> all three principal decisions are closed.** Band becomes full dose ≤ +35, derate +35 → +38, block
> > +38, applied to `NP-ENV-OPRANGE-001` §2/§4/§5 where those rows are now **decided, not `†`
> provisional**. The decisive argument was **use case, not thermal** — there is no non-emergency reason
> to run the device in a room above +35 °C, so an envelope reaching +43 bought availability nobody
> wants, and paid for it with a derate band the design was never validated across (§7 puts T1-std's
> full-dose ceiling at 37.9 °C). A supporting benefit, recorded but not load-bearing: the old band's
> top approached `NP-PWR-BUDGET-001` §3.4's efficacy floor, where the device could report a finished
> session while delivering a sub-threshold dose — **a null session is indistinguishable from a real one
> to the person wearing it.** The derate *curve* within 35 → 38 remains `OI-OPR-01`.
>
> **Rev 4 (2026-08-30) — D-2 and D-3 both DECIDED by the principal; D-2's criterion retires the
> pneumatic loop.** D-2: *a sealed pneumatic loop is in scope only if it provides a real benefit, and
> one not obtainable by other means.* Applying that test (new §6.9) finds the benefit **is** obtainable
> otherwise, and better: the loop and a **static conductive gap bridge** attack the same 0.23 m²K/W
> stagnant-gap term, and a compliant ceramic-filled gap pad reaches 0.0022 against the stirred gap's
> 0.067. The full static stack reaches **40.6 tiles against the loop stack's 19.7 — 2.1× better with no
> blower, no tubes and no penetration.** So **§6.2's pneumatic loop is not in scope**, and
> **`OI-THCOOL-06` (the ELF bench measurement) is no longer needed** unless the loop is revived. New
> `OI-THCOOL-15` carries the mechanical question the pad now depends on.
>
> **Rev 4 (2026-08-30) — D-3 DECIDED by the principal. Both accessories are on the roadmap; the ice
> pack is LOW priority and the chiller HIGHER, ranked on value delivered; and the honest-claim
> constraint is now binding, not advisory.** The ice pack is to be marketed **only for what it does** —
> it extends the ambient envelope and does **not** shorten sessions on a 45 W brick (§6.7.1). The
> session-time claim belongs to the base-station chiller alone. That ranking follows directly from
> §6.7.1: the chiller arrives with the watts that actually move `maxConcurrent`, and the ice pack does
> not. **A general TODO is raised** for a complete ordered priority set across *all* accessories, not
> just these two — `docs/status/pending-decisions.md` §13.2. No figure changes; §8's D-3 is closed.
>
> **Rev 3 (2026-08-30) — two optional, power-source-keyed cooling accessories added (§6.7), and the
> session-planner requirement they create (§6.8). One Rev 1/2 claim is corrected.** Principal direction:
> an **ice/PCM pack at the hip** for USB-C operation and a **TEC chiller in a mains base station**,
> both *optional purchases*, aimed at users who want shorter treatment times. Analysis of that proposal
> produced the correction: **the chiller belongs on the via's hub heatsink, not on the recirculated
> cavity air** — with the via fitted, the cavity path carries ~2 % of tile heat and the via ~90 % (§4).
> And **cooling raises only the `thermal` term of `NP-PWRSRC-001` §11's `min(electrical, thermal, dose)`
> governor**: on the 45 W brick electrical binds first at 6.4 tiles, so the ice pack buys **ambient
> envelope but no session-length reduction**; only the mains base station buys both, because it brings
> the watts as well (§6.7). §6.5's thermoelectric rejection is narrowed to *on-head* TEC, and §6.6's
> head-borne PCM sizing is superseded by hip-mounted ice at 334 kJ/kg. New `OI-THCOOL-11…14`.
>
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
> **⚠ READ FIRST — six answers, before the analysis that produced them.**
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
> **6. Cooling accessories buy concurrency only when watts come with them.** Cooling raises the
> `thermal` term of `min(electrical, thermal, dose)` and nothing else. On the 45 W brick, electrical
> binds first at **6.4 tiles** and a cooled thermal term changes the answer not at all — so the hip ice
> pack buys **ambient envelope, not shorter sessions**. The mains base station buys both, because it
> raises electrical *and* carries the chiller. Session length is `duration × ceil(sockets /
> maxConcurrent)`, so cooling enters the planner at exactly one variable (§6.7, §6.8).
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

## 6. The architectural options

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

> **Rev 3 narrows this rejection to its stated subject.** Every objection above is about siting a TEC
> **on the head**: the head's power budget, the head's thermal path, the fluxgates. None of them
> survives moving the device into a mains base station, where `NP-PWRSRC-001` §8 already says mains
> hardware belongs. §6.7 carries the base-station case; the *on-head* rejection stands.

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
stored in a 40 °C room arrives already melted.

> **Rev 3 supersedes the sizing above, on two counts.** The mass objection dissolves once the pack is
> **worn at the hip rather than on the head** — the loop reaches it through the hub, which is already
> outside the shield. And **ice at 334 kJ/kg beats paraffin's 200 by 1.67×**, so the store should be
> ice, not PCM: 182 g covers the 6-tile via load for 30 min. The "capacity, not a rate" limit is real
> and becomes §6.8's planning problem. See §6.7.

---

### 6.7 Remote-sink accessories — optional, keyed to the power source

**Principal direction (2026-08-30): two optional accessories, never required purchases**, for users who
want shorter treatment times — clinics above all. Both put the sink *off the head* and both are
`OI-THCOOL-06`-free, because neither crosses the EMF envelope: the BN-boss via already terminates in a
hub heatsink that is outside it.

**The correction that came out of analysing them: chill the via, not the air.** With the via fitted the
cavity path carries ~2 % of tile heat and the via ~90 % (§4). Chilling recirculated cavity air is
chilling the wrong stream.

| | Hip ice/PCM pack | TEC base-station chiller |
|---|---|---|
| Power source | USB-C, incl. power bank | mains |
| Electrical draw | **~1–2 W** (pump only) | 56 W at 6 tiles · 188 W at 20 |
| Heat rejected to room | none — latent storage | 90 W at 6 tiles |
| **Mode 3 autonomy** | **preserved** | not possible |
| Ice / capacity | 182 g per 30 min at 6 tiles | continuous |
| BOM (estimate) | **$52–107** | **$33–71** |

The TEC is cheaper in BOM and far worse in the currency that is actually scarce. `NP-PWRSRC-001` §4.1
finds the supply oversubscribed 4.7–8.4× already; 56 W at six tiles is not available from a brick.
Latent storage decouples cooling from the power budget entirely, which is why the ice pack is the only
sub-ambient option that preserves CLAUDE.md §1's *"runs from any USB-C PD power bank."* Vapour
compression was costed for contrast at $105–240 and is the wrong technology at a 10–30 W duty.

**Use liquid, not air, to the hip.** The same 34 W moves through a 4 mm line at 0.13 m/s, or a 20 mm
duct at 7.8 m/s. Pump hydraulic power is under a milliwatt.

#### 6.7.1 What each one actually buys — and it is not the same thing

Cooling raises the **`thermal`** term of `NP-PWRSRC-001` §11's governor and nothing else:

```
maxConcurrent = min(electrical, thermal, dose)
```

| Source | Electrical tiles | Thermal tiles | min | **min, cooled** |
|---|---:|---:|---:|---:|
| **45 W brick** (Home Standard) | 6.4 | 4.4–7.9 | 6.4 | **6.4 — unchanged** |
| 65 W brick | 9.6 | 4.4–7.9 | 7.9 | 9.6 |
| 100 W EPR | 15.2 | 4.4–7.9 | 7.9 | 15.2 |
| **Mains base station** | 37.6 | 4.4–7.9 | 7.9 | **25.8** |

> **On the 45 W brick — the configuration the ice pack targets — a cooled thermal term changes nothing,
> because electrical binds first.** The gain is bought by **watts first and cooling second**. So the
> hip ice pack buys the **ambient envelope** (running in a hot room instead of derating or blocking
> above +43 °C, which needs no extra watts) and **no session-length reduction**. Only the mains base
> station buys both — because it brings the watts *and* carries the chiller.

This is `NP-PWR-BUDGET-001` §4.4.4's warning run in reverse: relieving power moved the constraint to
heat; relieving heat moves it straight back to power.

#### 6.7.2 The clinic argument is dose, not time

Cascade length scales as `1/maxConcurrent`. Taking 6.4 → 25.8 tiles is a **4.0× reduction**, which on
`NP-PWRSRC-001` §5.5's worst case (Vascular Baseline, 40 groups, 20.0 h, **292 CEM43**) is roughly 10
groups, ~5.0 h and **~73 CEM43** at an unchanged plateau — and CEM43 uses R = 0.25 below 43 °C, so each
1 °C the chiller removes cuts it a further **4×**.

That matters more than the time saved. §5.5's finding is that **cascading is what creates the only real
thermal-injury exposure in the document set**, and cascading exists solely as the sanctioned workaround
for insufficient concurrency. The accessory's best argument is that it retires a hazard. Directional —
`NP-PWRSRC-001` owns the dose model and `scripts/check-thermal-dose.ts` must be re-run at the raised
concurrency (`OI-THCOOL-13`).

#### 6.7.3 The binding constraint is fogging, not cooling

| Air | Dew point |
|---|---:|
| Room, 25 °C / 60 % | 16.7 °C |
| Room, 30 °C / 70 % | 23.9 °C |
| **Scalp gap, ~33 °C / 90 %** | **31.1 °C** |
| Scalp gap, ~35 °C / 95 % | 34.1 °C |

**The scalp gap governs, not the room** — a head makes it warm and near-saturated. Let the module face
fall below ~31 °C and condensation forms on the scalp-facing PDMS window and the PD2 aperture, the
surfaces the J/cm² dose-metering claim depends on. The usable band is therefore **~32 °C to 42 °C** and
a thermostatic tempering valve is mandatory, not optional.

`NP-ENV-001` §5 provides **no live RH sensor** from which to compute that clamp. An RH part is cheap
($2–5, SHT4x / HDC3020 / BME280 class) but three things argue against adding one: it would sample the
room rather than the scalp gap, the assembly is silicone-rich and siloxane outgassing is a known
polymer-RH-sensor contaminant, and it reverses a decided position. **The existing dual-PD loop may
already be the fog detector**: CLAUDE.md §3's RISK-14 Option B puts PD2 on the scalp-facing surface and
uses the PD1/PD2 ratio to separate fouling from ageing, and condensation is a fast fouling step,
separable from slow ageing by rate. Closing the clamp on sensors already specified beats an open-loop
dew-point calculation from air sampled elsewhere. `OI-THCOOL-11`. If an RH sensor is wanted anyway, put
it **in the accessory**, so the fleet and `NP-ENV-001` §5 are untouched.

**Condensation behaves oppositely on the two sides.** The sealed cavity loop self-desiccates — 13–20 mg
of water, once, handled permanently by a desiccant pad. The **external** chilled lines see unlimited
room air and sweat continuously, so they need closed-cell insulation, and a wet line against the body
is a comfort problem.

### 6.8 The session planner must know the cooling state — and detect it, not be told

Session length is `duration × ceil(sockets / maxConcurrent)` (`scripts/check-pbm-power.ts`), so
**cooling enters the planner at exactly one variable**. The plumbing is trivial; knowing the state
reliably is not.

**Three facts are needed, and only one is new:**

| Fact | Source | Status |
|---|---|---|
| Electrical contract | PD negotiation, already logged to SHDR | **exists** |
| Cooling present | accessory-port UID, per the `np_module_map` auto-inventory precedent | new, easy |
| **Cooling effective and not exhausted** | coolant-return thermistor + pump-current check | **new, and the one that matters** |

**Presence is not sufficiency, and this is the real problem.** An ice pack attached but melted reads
"present"; a chiller plugged in but saturated reads "present." Latent storage is a **depleting budget** —
182 g covers ~30 min at six tiles while a cascade can run for hours, so **the planner would be
committing to a plan longer than the accessory's capacity.** Better detection does not fix that; it
needs capacity-aware planning, with a re-plan point or a defined fallback to the uncooled cascade when
the budget runs out (`OI-THCOOL-12`).

The right signal is **coolant return temperature**: it measures the effect rather than the claim, it is
the same signal for both accessories, and it degrades gracefully — as the pack melts, return temperature
rises and the planner can derate before anything becomes unsafe.

**Two rules, both inherited rather than invented:**

1. **Fail-safe, verbatim from `SR-FAN-06`** — *absent, stale, or invalid → treated as "cooling NOT
   confirmed" → uncooled thermal term.* That pattern is already reviewed for exactly this shape of
   input; do not write a second one.
2. **A manual declaration may never raise a ceiling.** Manual entry is error-prone, and here it is
   error-prone in the fail-*dangerous* direction: asserting "chiller attached" when it is not yields a
   plan the thermal path cannot sustain. Manual may lower a ceiling; only measurement may raise one.

**Safety is already covered; planning is not.** Path B1's scalp-facing NTC plus `SR-FAN-03` catch pack
exhaustion as a face-temperature rise, so **no new Class C requirement falls out of this.** What is new
is Class B planning correctness — a session that runs longer than promised, or re-cascades mid-run.
Keeping that line explicit is what keeps an availability feature out of Class C scope.

### 6.9 D-2 applied — the pneumatic loop is not in scope

**D-2 (principal, 2026-08-30): a sealed pneumatic loop is in scope only if it provides a real benefit,
and a benefit not obtainable by other means.** That is a sharper test than "is it shield-safe," and the
loop fails its second limb.

**Both the loop and a static conductive bridge attack the same term.** §2's decomposition makes the
stagnant inter-bowl gap 0.23 m²K/W — 56 % of the outward path. §6.1 stirs it convectively. But a
compliant **thermally-conductive, electrically-insulating gap pad** spanning the same 6 mm shorts it
conductively, and conduction through a solid beats convection across a gap by two orders of magnitude:

| Route | R across the gap |
|---|---:|
| Stagnant air (today) | 0.230 |
| Stirred air (§6.1 loop) | 0.067 |
| **Ceramic-filled gap pad, k ≈ 3 W/m·K** | **0.0022** |

Carried through the network (`bun scripts/check-thermal-network.ts` §15):

| ID | Option | R_out | Tiles | vs base | Moving parts |
|---|---|---:|---:|---:|---|
| RFE | Recirculation + absorber + forced external | 0.125 | 19.7 | 3.28× | blower + tubes |
| **GFE** | **Gap bridge + absorber + forced external** | **0.061** | **40.6** | **6.77×** | **none** |

> **The static stack is 2.1× better than the loop stack, with no blower, no tubes, no acoustic path
> beside the audio modality, and no penetration of any kind.** The loop's benefit is therefore not
> unique to it, and D-2's criterion is not met. **§6.2's pneumatic loop is out of scope**, and with it
> `OI-THCOOL-06` — the ELF magnetic bench measurement was BLOCKING only on the loop's penetration,
> which no longer exists. Both are retained in this document as the record of why, not as live work.

#### 6.9.1 Pad geometry — discrete and boss-co-located, not a sheet

**The obvious shape is wrong.** A continuous sheet across the vault is what the §6.9 arithmetic
assumes, and three fixed features forbid it: `NP-HEX-ZM-001` §5.4a puts **cluster-clamp actuators** in
that gap (one per cluster, on the inner bowl's outer face, **18 clusters** under SYM-1/CONTIG-1);
§5.3c puts the **fluxgate magnetometers** there; and §5.1 requires the bowls to **separate for module
replacement**, so anything spanning the gap is re-compressed at every service event.

**So the shape follows the heat, not the area.** Heat arrives at discrete points — `NP-HELMET-GEOM-001`
§3.2's *"boron-nitride-filled thermally-conductive polymer bosses at module heat pickups"*. The pad
pattern should be **one pad per tile, co-located with that tile's BN boss**, sized to the boss
footprint. Discrete pads route around the clamps and fluxgates instead of fighting them, and the
network sees their parallel sum.

**Coverage is the design variable, and §6.9's number assumed the wrong one.** Pads at area fraction φ
sit in parallel with stagnant air over the remaining (1 − φ). From `bun scripts/check-thermal-network.ts`
§16, on a 40 mm hex tile:

| Pad Ø | Coverage φ | R_gap | R_out (full static stack) | Tiles | vs base |
|---:|---:|---:|---:|---:|---:|
| 10 mm | 5.7 % | 0.0335 | 0.092 | 26.8 | 4.47× |
| 12 mm | 8.2 % | 0.0243 | 0.083 | 29.7 | 4.96× |
| 16 mm | 14.5 % | 0.0144 | 0.073 | 33.8 | 5.64× |
| **20 mm** | **22.7 %** | **0.0094** | **0.068** | **36.3** | **6.05×** |
| 25 mm | 35.4 % | 0.0061 | 0.064 | 38.1 | 6.36× |
| *(full area, §6.9 as written)* | *100 %* | *0.0022* | *0.061* | *40.6* | *6.77×* |

> **§6.9's headline is optimistic by ~1.1×, not by the large factor a first estimate suggested.** R_gap
> alone degrades 4.3× going from full area to a 20 mm pad — but by then the gap has stopped being the
> dominant term, so R_out moves only 0.061 → 0.068 and the ceiling only 40.6 → 36.3. **D-2's conclusion
> is untouched**: every row above, down to a 10 mm pad, still beats the stirred gap's 0.067.

**One mechanical problem this exposes, and it is not small.** The pad must compress against something
rigid. On the outer bowl's inner face the first thing it meets is the **carbon-loaded absorber foam**
(§2's 0.075 m²K/W term), which is itself compressible — so a pad pressed against foam compresses the
*foam*, not the pad, and never reaches its rated conductivity. Two ways out, both needing review:
locally omit the absorber where pads land, creating a rigid path to the CFRP; or make the pad and the
absorber one part. **Local omission is plausible but not free** — the absorber suppresses cavity
resonance rather than providing primary shielding (the mu-metal and Pd-polyester do that), so windows
in it are an EMC question, not a shielding-claim question. **This couples `OI-THCOOL-15` to
`OI-THCOOL-04`** — the absorber's thermal specification and the pad's land are the same decision.

**Two constraints the pad inherits, and they are mechanical rather than electromagnetic.** It must be
**electrically insulating and non-magnetic** — the fluxgates sit on the inner bowl and the Helmholtz
coils on the outer, so a conductive or ferrous bridge between them would perturb the cancellation the
pad is not otherwise touching. Ceramic-filled silicone satisfies both. And it must achieve **real
two-face contact across a curved 5–7 mm gap** with the tolerance stack, through hardware already in
that gap (the cluster clamps), and survive **compression set** over repeated bowl separations for
module replacement. That is `OI-THCOOL-15`, and it is now the gating question for the largest single
term in the outward path.

**This also strengthens `NP-PWR-BUDGET-001` §3.3's second lever** rather than competing with it. That
lever proposes a better conductive path to the outer shell *via the existing metallic layers*; this is
a conductive path *across the gap that precedes them*. They are in series and compose.

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

**7.2 — ✅ RESOLVED (principal): one shared band — full dose ≤ +30 °C, derate +30 → +35, block > +35.**
It arrived in three steps: block +43 → +38 (2026-08-30, physics and use case), +38 → +35 (2026-08-31,
alignment with T2's block), then the whole band shared across every helmet module and the intranasal
probe (2026-08-31, consistency). `NP-ENV-OPRANGE-001` §2, footnote ‖.

**The final step was a product decision, not a thermal one** — *"consistency makes products easier to
understand."* T2-D's numbers were adopted wholesale because it was already the tightest, and a shared
envelope must be the **intersection** of what every module can do, never the union. The result is that
one sentence describes the thermal envelope of the whole line at any tier.

**Two consequences worth stating plainly.**

1. **T1's full-dose ceiling tightens +35 → +30, and that is a real capability reduction.** Full dose in
   a 32 °C room was previously allowed and now derates. It is **deliberately more conservative than the
   physics requires** — §7's fit puts T1-std's full-dose ceiling at 37.9 °C, nearly 8 °C above the new
   bound — and is paid on purpose in exchange for a single envelope.
2. **`OI-OPR-01` is live again, and this reverses what Rev 6 recorded.** Rev 6 called the T1-A derate
   curve *moot* because the band had zero width. The band is 5 °C wide again, so the curve must be
   specified after all. The work is nonetheless **smaller** than before: one curve now serves every
   helmet module rather than one per module.

`OI-THCOOL-16` is **closed by §7.5**: the block edge at +35 is a discrete transition, and it carries a
1.0 °C band with re-arm at 34.0 °C.

The decisive argument was **use case, not thermal**: there is no non-emergency reason to run the device
in a room above +35 °C, so an envelope reaching +43 bought availability nobody wants — and paid for it
with a derate band the design was never validated across, since §7's fit puts T1-std's full-dose ceiling
at **37.9 °C**. Aligning the block with that number costs nothing anyone wanted.

**A third benefit that was not the reason but is worth recording.** `NP-PWR-BUDGET-001` §3.4 supplies an
efficacy **floor** — 0.02–0.3 W/cm², 10–120 J/cm², with *"under-dosing, not mechanism failure, explains
most nulls."* Near the top of the old 35→43 band the derated duty approached that floor, so the device
could run to completion and report a finished session while delivering a sub-threshold dose. **A null
session is indistinguishable from a real one to the person wearing it**, which makes that failure mode
worse than a refusal. The narrower 35→38 band stays clear of it. The exact crossover was not computed —
it needs the derate curve, itself provisional under `OI-OPR-01` — so this is a supporting argument, not
a load-bearing one.

**7.3 — Lowering ambient does NOT resurrect Path A, and cannot.** The C2 fault case pins the junction at
its 62 °C throttle setpoint and solves for the face; the face lands at 60.2 °C because it is 1.6 mm from
a 62 °C plane, **almost independently of ambient**. Ambient is not the variable in that case. `SR-FAN-01`,
`SR-FAN-03` and Path B1 stand unchanged. Similarly, R1 §3's fan-off safe-duty ceiling improves only from
~4.5 to ~9.0 mW/cm² between 43.3 °C and 25 °C — **halt-or-trickle at both ends.** No ambient choice
changes the character of the fan-fault derate.

---

**7.4 — The derate semantics are unspecified, and the default reading reopens what D-1 closed.**

`NP-ENV-OPRANGE-001` §1 says *"linear **duty** derate T_f → T_max."* Duty scales. **Nothing in the
document set says the session extends to compensate**, so on the text as written a fixed-length session
in the derate band delivers proportionally less dose. `bun scripts/check-thermal-network.ts` §17, for a
typical 60 J/cm² protocol:

| Ambient | Duty | Dose | |
|---:|---:|---:|---|
| ≤ 30 °C | 100 % | 60 J/cm² | full dose |
| 32 °C | 60 % | 36 | under-dosed against the pre-Rev-7 envelope |
| 34 °C | 20 % | 12 | just above the floor |
| **34.2 °C** | 17 % | **10** | **reaches `NP-PWR-BUDGET-001` §3.4's efficacy floor** |
| 34.5 °C | 10 % | 6 | **sub-threshold — a session that cannot work** |

> **This is the failure mode the D-1 chain existed to remove, quietly restored.** §7.2 records that the
> old +35 → +43 band was retired partly because its top was where a *"completed"* sub-threshold session
> could occur, and Rev 6's zero-width band eliminated the zone entirely. Rev 7's shared band brings back
> a **~0.8 °C sliver** of it. **A null session is indistinguishable from a real one to the person
> wearing it**, which is what makes it worse than a refusal.

The crossover moves with the protocol's full dose — 33.8 °C at 40 J/cm², 34.6 °C at 120 — so it is
**per-protocol, not a single temperature.**

**The alternative reading is not free either.** If the session instead *extends* to hold dose constant,
efficacy is preserved but a 20-minute session becomes 33 min at 32 °C and **100 min at 34 °C** — and
time-at-ceiling is exactly what drives CEM43 (`NP-PWRSRC-001` §5.5, where cascading generates the only
real thermal-injury exposure in the document set). That trades an efficacy problem for a thermal-dose
one. **Both readings have a cost, which is why the semantics must be chosen rather than inherited.**

**Proposed shape: clamp the derate at the efficacy floor and block there, rather than ramping to zero.**

| Protocol full dose | Blocks at | (instead of a flat 35.0 °C) |
|---:|---:|---|
| 40 J/cm² | 33.8 °C | duty floor 25 % |
| 60 J/cm² | 34.2 °C | duty floor 17 % |
| 120 J/cm² | 34.6 °C | duty floor 8 % |

This is **D-1's own principle one level down — do not run a session that cannot work; refuse it** — and
it costs at most 1.2 °C of an envelope that §7.2 already notes is ~8 °C more conservative than the
physics requires. It makes the block per-protocol rather than a single constant, which is the one real
complication: `NP-FW-POE-001`'s gate would need the protocol's dose as an input. **`OI-THCOOL-17`**, and
`OI-OPR-01` inherits the constraint — the curve it specifies must carry a floor, not run to zero.

**One thing this does not change.** The band applies to PBM only. `NP-ENV-OPRANGE-001` §5's intersection
rule leaves EEG-only at +5 → +45 and tDCS at −10 → +45, so a user in a 33 °C room keeps full-capability
EEG and tES. Only PBM protocols derate, which bounds the blast radius considerably.

---

**7.5 — ✅ RESOLVED (`OI-THCOOL-16`): hysteresis on the ambient hard edges — 1.0 °C, anchored on the
effective block, applied as a raised admission bar rather than a hold-off.**

The block edge is a **discrete** transition: below it a session is admitted, above it denied. Every
discrete transition driven by a noisy, drifting input chatters unless re-entry is separated from exit.
The derate ramp needs no such treatment — it is continuous, and ±0.2 °C of ambient noise moves duty by
±4 %, which nobody can perceive. **Only the hard edges need a band: the +35 °C block, the cold block at
the low bound, and — if `OI-THCOOL-17`'s efficacy clamp is adopted — the floor-clamp edge that replaces
the flat 35.**

**7.5.1 — The band is 1.0 °C, and three independent bounds land on it.**
`bun scripts/check-thermal-network.ts` §18:

| Bound | What it says | Number |
|---|---|---:|
| **Representation** | `adc_to_celsius()` returns whole degrees (`uint8_t`), so 1 °C is the smallest band the shipped sense path can express. The *analog* chain is not the limit — 70 ADC counts/K at the edge is 0.014 K/LSB, ~0.04 K at 3 LSB of noise | **≥ 1.0 °C** |
| **Environment** | A hysteresis band suppresses cycling only when it exceeds the input's peak-to-peak excursion, and a room's own thermostat differential is 0.5–1.0 °C. A band inside that sits inside the room's oscillation and does nothing | **≥ 1.0 °C** |
| **Availability** | The band is a wait. At 1–3 °C/h of room recovery, 1.0 °C costs **20–60 min** before a denied session can be retried; 2.0 °C costs 40–120 min | **≤ ~1.0 °C** |

**The floors and the ceiling meet at one number, which is the whole argument for it.** It is also why
the junction interlock's **7 °C** band (`NP_NTC_CUTOFF_DEG_C` 62 / `NP_NTC_REARM_DEG_C` 55) is not the
precedent to copy, despite being the obvious one in the tree. That latch is a **reactive fault** where
over-cutting is free — a cranial domain that stays cut for an extra minute costs nothing. This is a
**predictive admission gate**, where the band is charged directly to the user as a wait: 7 °C here is
2.5–7 hours, which is not a hysteresis band but a lock-out.

**The 20–60 minute wait is a real cost and is accepted, not waved away.** It is paid only by a user
whose room is within 1 °C of a block that already sits ~2.9 °C below §7's fitted 37.9 °C full-dose
ceiling — and the alternative is a device that starts, refuses, and starts again on the same room.

**7.5.2 — The mechanism: the latch raises the bar, it does not hold the device off.**

The naïve form — "once blocked, deny everything until ambient falls" — is both over-restrictive and
wrong per-protocol, because under `OI-THCOOL-17` the block is not one temperature. A **higher**-dose
protocol tolerates a **higher** ambient (its derated dose reaches the 10 J/cm² floor later: 34.6 °C at
120 J/cm² against 33.8 at 40), so a blanket hold-off would deny a session that is genuinely admissible.

The form specified instead:

> **While the latch is set, admission tests `ambient ≤ T_block_eff − Δ` instead of `ambient < T_block_eff`,
> where `T_block_eff` is that protocol's own effective block anchor and Δ = 1.0 °C.** The latch is set
> whenever an admission is denied on ambient, or a mid-session re-read crosses the anchor. It is cleared
> by any evaluation that passes the margined test continuously for `t_dwell` (§7.5.4).

Three properties follow, and each is the reason for the shape:

1. **It is strictly more restrictive than the unlatched test, at every ambient and every protocol.** So
   it composes with the `min()` of `NP-FW-POE-001` §5 without touching that rule's proof — a term that
   can only move the bar down in temperature cannot widen safety.
2. **It is per-protocol without any latch bookkeeping.** No stored anchor, no map of which envelope
   latched: one boolean and one dwell timer, evaluated against whatever anchor the current request
   computes.
3. **It is anchored on `T_block_eff`, not on 35.0.** `T_block_eff = min(TABLE_block, POE_block)` today
   and per-protocol if `OI-THCOOL-17` adopts the efficacy clamp. **The rule therefore holds whichever
   way 17 lands, which is why 16 closes now rather than waiting on it.** Re-arm points either way:

| Anchor | Block | Re-arm |
|---|---:|---:|
| Flat block, as specified today (§7.2) | 35.0 °C | **34.0 °C** |
| 40 J/cm² protocol, under `OI-THCOOL-17`'s clamp | 33.8 °C | **32.8 °C** |
| 60 J/cm² protocol | 34.2 °C | **33.2 °C** |
| 120 J/cm² protocol | 34.6 °C | **33.6 °C** |

**7.5.3 — A mid-session crossing terminates the session; it never pauses it.**

The coarse periodic ambient re-read (`NP-FW-POE-001` §6) can cross the anchor mid-session. It must end
the session, not suspend it, for three reasons that all point the same way. **(i)** A resumed session is
a split, partially-dosed session, and its completion report would claim a dose it did not deliver —
the D-1 failure mode again, one level down. **(ii)** Suspend/resume cycles *add* time-at-ceiling, and
time-at-ceiling is what drives CEM43 (`NP-PWRSRC-001` §5.5), so the "gentler" option is the thermally
worse one. **(iii)** A resume path is an automatic re-entry loop — precisely the loop this item exists
to remove, sited at the worst possible place. **With no automatic re-entry anywhere, chatter is
impossible by construction, and Δ only governs the next *user-initiated* start.** The session record
reports the dose actually delivered, honestly, as a terminated session.

**7.5.4 — The dwell time is not free-standing; it inherits `OI-ENV-05`.**

Clearing the latch also requires the margined test to hold continuously for a dwell `t_dwell`. Its value
depends on a decision not yet made — **which sensor is "ambient".** `NP-FW-POE-001` §6 leaves it as "NTC
read at session start before self-heating, **and/or** a dedicated ambient NTC", and the shipped MCU
config has no ambient channel at all (five cranial sense domains plus the hub NTC).

- **Dedicated ambient NTC, outside the thermal path:** `t_dwell` = **60 s**, enough to reject transient
  air movement, negligible against a 20–60 min wait.
- **Hub NTC as ambient proxy:** `t_dwell` must exceed the device's own self-heat decay, which is minutes,
  not seconds — **≥ 5τ_hub**, and τ_hub is unmeasured.

**Either way the error is fail-safe**: a self-heat-contaminated proxy reads *high*, making the gate more
restrictive after a session, never less. So the dwell can be specified as a rule now and given its number
when `OI-ENV-05` closes; nothing downstream is blocked on it.

**7.5.5 — Three things this deliberately does not do.**

- **It does not persist across a power cycle.** The latch is RAM state; unplugging clears it. That is a
  dodge, and it is an acceptable one, because **the dodge cannot cross the safety bound** — it buys
  re-entry only in the band between `T_block_eff − Δ` and `T_block_eff`, where the plain admission test
  already passes. Persisting it would mean NVRAM writes (`NP-FW-NVRAM-001`) for a usability latch with
  no safety content.
- **It adds no SHDR field.** The existing `NP_POE_OUT_OF_RANGE` denial reason covers it; distinguishing
  "denied on the margined test" from "denied on the plain test" is diagnostic only, and a new field would
  need a `docs/reference/data-architecture-detail.md` §5.1 boundary resolution to earn its place.
- **It changes no descriptor format.** Δ and `t_dwell` live in the Class C versioned envelope table
  alongside the bounds they modify, not in the POE block — which avoids a trap worth naming: **a margin
  parameter composes by `max()`, not `min()`.** If Δ were ever descriptor-supplied and folded into
  `NP-FW-POE-001` §5's `min()`, a stale or hostile descriptor supplying Δ = 0 would erase the hysteresis
  while appearing to obey the rule that the descriptor can only restrict. The direction of "restrictive"
  inverts for a parameter that is itself a margin. `NP-FW-POE-001` §6.1 records this.

Normative encoding, enforcement flow, failure modes and verification: **`NP-FW-POE-001` §6.1**.

## 8. Recommendation

**Sequenced, cheapest-first. Nothing here is a safety change; §4.1 governs.**

| # | Action | Cost | Worth | Owner |
|---|---|---|---|---|
| 0 | **Close `OI-R1-03`** — pin the existing fan's airflow path | ~0 | Gates every baseline below | ME + Thermal |
| 1 | **Thermally specify the EMI absorber** (§6.3) | Materials substitution | 0.075 → ~0.02 | ME + EMC |
| 2 | **Attack the via interface, not the via** (§3) | TIM + mounting | ~90 % of via-path R | ME |
| 3 | **Conductive gap bridge across the inter-bowl gap** (§6.9) — supersedes the pneumatic loop, which D-2 puts out of scope | Gap pad + assembly force | **0.23 → ~0.002** | ME |
| 4 | **Forced external convection** (§6.4) | Depends on step 0 | 0.10 → ~0.033 | ME |
| 5 | **Optional accessories** (§6.7): hip ice pack, TEC base-station chiller | $52–107 / $33–71, accessory not base BOM | Envelope; concurrency only with mains watts | ME + FW |
| — | *On-head TEC, scalp-gap ventilation, vapour compression* | — | **Not recommended** (§6.5, §6.7, §9) | — |

Steps 1 and 2 are incremental work on an adopted architecture with no EMF, regulatory or architectural
consequence, and together are worth roughly half of §5's total. **Do them regardless of what is decided
about step 3.**

**Two decisions for the principal:**

- **D-1 — ✅ DECIDED (principal), in three steps, ending in one shared band: full dose ≤ +30 °C, derate
  +30 → +35, block > +35, on every helmet module and the intranasal probe.** (i) Block +43 → +38
  (2026-08-30) on **use case, not thermal** — there is no non-emergency reason to run the device in a
  room above +35 °C, so an envelope reaching +43 bought availability nobody wants; it also aligned the
  block with §7's 37.9 °C full-dose ceiling and retired the band where derated duty approached the
  `NP-PWR-BUDGET-001` §3.4 efficacy floor. (ii) Block +38 → +35 (2026-08-31) on **tier consistency** —
  T2-D already blocked at +35, so a T1 → T2 upgrader would otherwise meet a tighter limit on the more
  expensive tier. (iii) The **whole band** shared (2026-08-31) on **product consistency**, adopting
  T2-D's numbers wholesale because a shared envelope must be the intersection of what every module can
  do. Applied to `NP-ENV-OPRANGE-001` §2/§4/§5; those rows are decided rather than `†` provisional.
  **Two consequences:** T1's full-dose ceiling tightens +35 → +30, a real and deliberate capability
  reduction; and **`OI-OPR-01` is live again** — Rev 6 had recorded the derate curve as moot on a
  zero-width band, and the band is 5 °C wide once more, though one curve now serves every module.
  Hysteresis on the +35 block edge is specified in §7.5 (`OI-THCOOL-16`, closed).
- **D-2 — ✅ DECIDED 2026-08-30 (principal): in scope only for a real benefit not obtainable by other
  means — and §6.9 finds it is obtainable otherwise, better.** A static conductive gap bridge attacks
  the same 0.23 m²K/W term and reaches **40.6 tiles against the loop's 19.7**, with no moving parts and
  no penetration. **The pneumatic loop is out of scope; `OI-THCOOL-06` is closed with it.** The
  criterion did the work here — "is it shield-safe" would have kept the loop alive, and "is the benefit
  unique to it" killed it. Replacement gating question: `OI-THCOOL-15`.
- **D-3 — ✅ DECIDED 2026-08-30 (principal).** Both accessories go on the roadmap. **Priority follows
  value delivered: the TEC base-station chiller is the higher priority; the hip ice pack is low.**
  §6.7.1 is why — the chiller brings the watts that actually move `maxConcurrent`, and the ice pack
  does not. **All marketing claims must be honest, so the ice pack is to be marketed only for what it
  does:** it extends the ambient envelope and does **not** shorten sessions on a 45 W brick. The
  session-time claim belongs to the base station alone. This is a binding constraint on copy, not an
  observation, and it is the same prohibition `NP-PWRSRC-001` §12 states — do not sell a source that
  changes nothing. Recorded in `docs/reference/accessories-roadmap.md`; a complete ordered priority
  set across **all** accessories is raised as a general TODO in
  `docs/status/pending-decisions.md` §13.2.

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
| **OI-THCOOL-15** | **Gap-pad geometry and contact (§6.9.1).** Fix the pad diameter and coverage fraction against the cluster-clamp and fluxgate keep-outs; establish real two-face contact across the curved 5–7 mm gap under the tolerance stack; and resolve **what the pad compresses against** — the absorber foam is itself compressible, so a pad pressed against it never reaches rated conductivity. Must be electrically insulating and non-magnetic (fluxgates inner, Helmholtz outer), and survive compression set over repeated bowl separations. **Coupled to `OI-THCOOL-04`** — the absorber's thermal spec and the pad's land are one decision. **The gating question for the largest term in the outward path** | ME (+EMC) | **Gates §6.9** |
| **OI-THCOOL-17** | **Choose the derate semantics, and clamp the ramp at the efficacy floor (§7.4).** As written, *"linear duty derate"* means a fixed-length session under-doses, going sub-threshold above ~34.2 °C for a 60 J/cm² protocol — reopening the completed-but-ineffective session D-1 closed. Extending the session instead preserves dose but multiplies time-at-ceiling, which drives CEM43. **Decide which, and clamp duty at the floor so no session is ever sub-threshold.** Makes the block per-protocol, so `NP-FW-POE-001`'s gate needs protocol dose as an input. **`OI-OPR-01` inherits this: the curve must carry a floor, not run to zero** | FW + Thermal | **Gates `OI-OPR-01`** |
| ~~OI-THCOOL-16~~ | **✅ CLOSED 2026-09-02 by §7.5.** Band **Δ = 1.0 °C** on every ambient hard edge, anchored on `T_block_eff = min(TABLE_block, POE_block)` rather than on the constant 35.0, and applied as a **raised admission bar** (`ambient ≤ T_block_eff − Δ` while latched) rather than a hold-off — strictly more restrictive at every ambient, so it composes with `NP-FW-POE-001` §5's `min()` untouched. A mid-session crossing **terminates** the session rather than pausing it, which removes the last automatic re-entry path. Normative in `NP-FW-POE-001` §6.1. Retained struck-through per `NP-CONV-001` §4. **Residual, not blocking:** `t_dwell` inherits `OI-ENV-05` (60 s with a dedicated ambient NTC, ≥ 5τ_hub with the hub NTC as proxy — fail-safe either way), and a sub-1 °C band would need the ambient path specified at 0.1 °C resolution → `OI-POE-06` | FW (closed) | — |
| ~~OI-THCOOL-16 (original text)~~ | **Hysteresis on the +35 °C PBM ambient cliff.** With the derate band collapsed (Rev 6), the T1-A envelope is a hard block at a single temperature, so an ambient NTC sitting on +35 could chatter start/stop. Specify the hysteresis band and its interaction with `NP-FW-POE-001`'s gate | FW | No |
| ~~OI-THCOOL-06~~ | **✅ CLOSED 2026-08-30 by D-2** — this was BLOCKING only on the pneumatic loop's penetration of the posterior boss, and §6.9 puts that loop out of scope. Retained struck-through rather than deleted, per `NP-CONV-001` §4's append-only open-item rule; reopen only if the loop is revived | — (closed) | — |
| ~~OI-THCOOL-06 (original text)~~ | **Bench-measure ELF magnetic leakage through a mu-metal chimney collar at the posterior boss with tube penetrations.** Waveguide-below-cutoff does not apply below ~100 Hz | EMC (EMF-1) | **BLOCKING on §6.2** |
| **OI-THCOOL-07** | Confirm the sealed loop's condensation behaviour across the `NP-ENV-001` §2.2 warm-up transient — fixed absolute humidity should help, but the cold-optics case is untested | Thermal | No |
| **OI-THCOOL-08** | Re-run §5 against `OI-PWR-01`'s multi-tile CFD; the ratios need a valid model at N > 8 before any number is quoted | Thermal | **Gates §5 numbers** |
| **OI-THCOOL-09** | Assess whether tubes at the posterior boss disturb `NP-DRV-SHELL-002` §9.3's loop-area control or the §4.3 segregated-return requirement | EE | No |
| **OI-THCOOL-11** | **Test whether the existing PD1/PD2 fouling discriminator detects condensation onset** fast enough to serve as the anti-fog clamp, before adding an RH sensor that would sample the wrong air and reverse `NP-ENV-001` §5 | Optical + FW | Gates §6.7.3 |
| **OI-THCOOL-12** | **Capacity-aware planning for a depleting sink.** A latent store is a budget, not a state; specify the re-plan point or the fallback to the uncooled cascade when it is exhausted | FW + Systems | Gates §6.8 |
| **OI-THCOOL-13** | Re-run `scripts/check-thermal-dose.ts` at the raised concurrency to confirm §6.7.2's CEM43 reduction; the model is owned by `NP-PWRSRC-001` | Thermal | No |
| **OI-THCOOL-14** | Confirm the accessory-port UID scheme and the coolant-return thermistor channel against the hub's existing I2C fan-out and `SR-FAN-06`'s fail-safe shape | EE + FW | No |
| **OI-THCOOL-10** | **Close `OI-R1-03` on the architectural grounds in §6.4** (answer: outer shell only) and collapse `NP-THERM-CFD-R1-001` §4's *"up to ~6 °C"* fan-loss branch to the 0.6 °C branch, re-stating τ_face / t₄₂ accordingly. Owned by `NP-THERM-CFD-R1-001`, not by this document — raised, not actioned | Thermal | No |

---

## 11. Cross-references

`NP-THERM-CFD-R1-001` (the network, the via study, the ambient wall) · `NP-THERM-CFD-001` / `-C2-001`
(BCs, stack-up) · `NP-REQ-FANHEALTH-001` (SR-FAN — **unchanged by this study**) · `NP-PWR-BUDGET-001`
§3.2/§3.3 (aggregate ceiling, the three levers this adds a fourth to) · `NP-PWRSRC-001` §4.1/§7.0 (the
cavity wall, 2/23 coverage) · `NP-HEX-ZM-001` §5.1–5.3 (two-bowl shell, the one existing aperture) ·
`NP-DRV-SHELL-002` §4.3 (share the aperture) · `NP-ENV-OPRANGE-001` (the ambient bounds §7 validates) ·
`NP-DT-001` DI-SAFE-13 · `NP-PWRSRC-001` §11 (the `min(electrical, thermal, dose)` governor §6.7.1
raises one term of), §12 (the prohibition D-3 invokes), §5.5 (the CEM43 exposure §6.7.2 attacks) ·
`NP-ENV-001` §5 (no live RH sensor — §6.7.3) · `NP-REQ-FANHEALTH-001` `SR-FAN-06` (the fail-safe rule
§6.8 inherits) · CLAUDE.md §1 (Mode 3 autonomy), §3 (RISK-14 dual-PD), §4.2/§4.3/§4.5 ·
`scripts/check-pbm-power.ts` (where `maxConcurrent` becomes session length) ·
`scripts/check-thermal-network.ts` §9–§18 · `NP-FW-POE-001` §6.1 (the hysteresis §7.5 specifies) ·
`firmware/safety_mcu/src/np_thermal_interlock.c` (the 62/55 °C junction re-arm precedent)
