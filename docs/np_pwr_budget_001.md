# Power Budget — Reverse-Engineered from Safety Ceilings (Design Study)

**Project:** NeurOne
**Document:** NP-PWR-BUDGET-001
**Revision:** 2
**Date:** 2026-08-21
**Status:** DESIGN STUDY — not a tooling baseline. Every numeric value below is a derived engineering estimate with its assumption chain stated inline, not a measured or locked figure. See §7 (Decisions) and §8 (Open Items). **Does not modify CLAUDE.md §4.5** — that requires a principal decision (§7).
**Effective Date:** —
**Author:** NeurOne Hardware Engineering
**Approved By:** — (pending design review)
**References:** CLAUDE.md §3 (modality stack), §4.2 (safety architecture), §4.5 (power — the figure this document interrogates); NP-HW-HEXTILE-001 Rev 1 §9 (concurrent-tile ceiling, power-derived); NP-THERM-CFD-R1-001 Rev 1 (single-tile thermal model, BN-boss export study); NP-THERM-CFD-001 (heat-source model, η_wp figures); NP-ENV-OPRANGE-001 (ambient/duty envelope); NP-HELMET-GEOM-001 (scalp area estimate); NP-DT-001 (DI-PERF-12/13, TMS and 1170nm design inputs); NP-HW-HEXTILE-001 Rev 6 (§4.2/§4.3 emitter allocation and irradiance, §6.4 BOM, §8.1 VLED rail, §8.3 logic budget, §9 concurrency ceiling); NP-OPT-PSF-001 Rev 1 (§3.3 cortical resolution floor); `docs/pbm_neuro_protocols.md` (efficacy band — the floor added at Rev 2); `docs/reference/competitive-position.md` (comparative form of §3.7)
**Related Issues:** —
**Gate:** Routes two items to principal decision (§7) before any change to CLAUDE.md §4.5
**IEC 62304 Class:** — (analysis document; no code)
**Supersedes:** — (new document; first attempt to derive CLAUDE.md §4.5 from safety ceilings rather than treat it as a given input)
**Parent Document:** CLAUDE.md §4.5

---

> **Rev 2 (2026-08-21) — an efficacy FLOOR added to a methodology that had only ceilings (new §3.4); the full-population bound stated for the first time (§3.5); two findings the bound produced (§3.6, §3.7). No figure in §3.1–§3.3, §4, §5 or §6 changes, and CLAUDE.md §4.5 is still not modified.**
>
> Rev 1 §1 classified every modality as *inelastic* (dose-capped) or *elastic* (heat-removal-bound). **Both are ceilings, and a budget bounded only from above cannot say what the power is for** — which is how §3.2 bracketed the tile count without asking what a tile delivers. `docs/pbm_neuro_protocols.md` supplies the missing lower bound: **0.02–0.3 W/cm² at scalp, 10–120 J/cm², 6–30 min**, with *"under-dosing, not mechanism failure, explains most nulls"* as the attached lesson. §3.4 records it.
>
> **§3.5 answers a question the tree had never written down: a fully populated, fully driven 80-tile lattice draws ≈ 2.0 kW** (80 × 25.0 W VLED = 83.3 A at 24 V, plus 6.6 W logic and ~7 W overhead). That is 20× the maximum negotiable USB-C PD contract and 8.3× the entire PD 3.1 EPR ceiling — **but the electrical collision is the mild one.** At η_wp 0.30–0.45 it puts **1.1–1.4 W/cm² at the LED plane, 4–5.6× the worst-case flux modelled anywhere in the tree**, and §3.3's sealed cavity means no supply or fan reaches it. **T1-A, not T1-C, is the worst-case tile** — adding the 1064 nm channel *lowers* per-tile draw, because its V_f is 1.40 V against CH_A's 2.10 V.
>
> **The distinction that makes §3.5 usable, and which is routinely lost: *populated* is not *driven*.** Populating 80 sockets costs BOM and 0.53 W of standby, not LED power; a fully populated helmet under the ~6-tile governor draws what a 20-tile build draws. Full population is `OI-HEXTILE-06`'s cost decision; full *activation* is the impossible one.
>
> **Two findings fall out of the same arithmetic run backwards.** (i) **§3.6 — whole-vault simultaneous illumination is affordable at ~30 W** (1.5 % drive), inside the existing envelope, and is specified nowhere; it matters because the Grade A Alzheimer's protocol asks for *whole-head* geometry, so `NP-HW-HEXTILE-001` §9.3's *"placement options, not capability"* is **too strong**. It is also honestly bounded: at that irradiance a 20-min session delivers 7.2 J/cm², **below** the §3.4 threshold — coverage, not dose. (ii) **§3.7 — local irradiance and total optical output are different quantities.** Total optical is envelope-capped at ~13–14 W *whatever the tile population*; only **coverage** scales with tiles. With `NP-OPT-PSF-001` §3.3's 26.2 mm cortical resolution floor at ~65 % of the tile pitch, the dense lattice buys **irradiance, not spatial precision** — an emitter count is not a capability claim.
>
> **One finding runs against the product.** §3.4 records that the **1064 nm channel delivers 28 mW/cm², 9× below the 0.25 W/cm² its own Grade A cognitive-enhancement protocol specifies**, and a 90-site 1064-only tile would still reach only ~85 mW/cm². That is an η_wp ≈ 4.8 % emitter wall, not a budget shortfall — **more watts cannot fix it.** Raised on the owning document as `OI-HEXTILE-21`.
>
> New open items **OI-PWR-07–10**; new decision **D-4** (governor denominated in watts, not tile count). §3.5 and §3.6 are routed to `OI-HEXTILE-06` and `OI-HEXTILE-09` as decision inputs — this document does not own them and does not decide them.

---

> **⚠ READ FIRST — what this document is and is not.**
>
> Every existing document that touches power draw (NP-HW-HEXTILE-001, NP-THERM-CFD-001, NP-THERM-CFD-R1-001, NP-THERM-BEZEL-001, NP-HELMET-GEOM-001) cites `CLAUDE.md §4.5` as an **input** and builds thermal/mechanical analysis *from* it. None of them derive it. This document runs the arithmetic the other direction: given the actual biological/optical/thermal ceilings a user can safely be exposed to, what power budget do they imply — and does the current one (T1 peak 45–50 W, T2 peak 70–74 W) hold up?
>
> **Answer, short form:** T1's PBM figure is coincidentally close to a real thermal ceiling, derived for the wrong reason. T2's figure is not close to anything — it was never extended to include TMS at all, and the modality it's actually sized for (1064/1170nm PBM) is well inside the true limit. Both need principal decisions before they can be called "locked" again.

---

## 1. Methodology

Two kinds of constraint govern how much power a modality may safely draw, and they behave completely differently under "give it more battery":

| Constraint type | Examples | Response to more power/battery |
|---|---|---|
| **Inelastic — hard biological ceiling** | 40 µC/cm² charge density (tDCS/tACS/HD-tDCS), IEC 62471 retinal MPE (visual), IEC 60825-1 laser class (1170nm) | **None.** No amount of extra battery lets you exceed a dose limit safely. The electrical budget for these modalities is capped at whatever current/optical-power delivers the dose limit, full stop. |
| **Elastic — heat-removal-bound** | PBM scalp contact (42 °C limit), 1170nm TEC, TMS coil (if scalp-adjacent windings run warm) | **Scales with cooling capacity**, not raw wattage. The ceiling is "how fast can we move heat away from tissue," which a bigger heatsink, more fan airflow, or a better conductive-export path can genuinely raise. More battery only helps if it's paired with more heat-rejection hardware — batteries store energy, they don't remove heat. |

The right procedure, per modality: (1) identify which bucket it's in; (2) for inelastic modalities, the safety ceiling *is* the power ceiling — done; (3) for elastic modalities, find the actual heat-removal capacity of the current design and back-calculate the electrical power it supports, not the other way around. Only after every modality's true ceiling is known does it make sense to sum concurrent combinations and ask what battery/PD contract the result requires.

## 2. Inelastic modalities — not power-budget-relevant

BES/tACS (≤1 mA), tDCS (≤2 mA, 40 µC/cm²), VNS+HRV (≤2 mA), clinical tACS (≤4 mA, 16-ch), sLORETA HD-tDCS (≤2 mA/electrode, ≤5 of 16 channels live), cervical VNS (gel electrode, cardiac-interlocked), and 21-ch qEEG (passive ADS1299 sensing) are all current-limited in the low-milliamp range at low compliance voltages. Order-of-magnitude: a handful of channels at a few mA and a few volts compliance is tens to low hundreds of milliwatts, combined. Visual stimulation (216 micro-LEDs, IEC 62471 hardware MPE at 50% of exempt-group threshold) and neural audio entrainment (planar magnetic + bone conduction) are similarly bounded to low single-digit watts by their own safety/output specs, independent of anything in §4.5.

**None of these can move the power budget meaningfully even at their safety ceiling.** They are correctly treated as fixed small overhead. The two modalities that matter are PBM (§3) and TMS (§4) — and the current T2-peak figure turns out to be built for a third, the 1170nm laser (§5), while never accounting for TMS at all.

## 3. PBM — the aggregate thermal ceiling, estimated

NP-HW-HEXTILE-001 §9 already runs this calculation, but backwards: take the *given* T1 peak envelope (45–50 W), subtract non-PBM overhead (~6–8 W), divide by per-tile draw, and conclude "~6 tiles concurrent, not eighty." That's budget → capability. The question this section asks is capability → budget: independent of the 45–50 W figure, how many tiles can the 42 °C scalp limit actually tolerate?

### 3.1 What the existing single-tile model gives us

NP-THERM-CFD-R1-001 §5.1 models one hex tile under the adopted base-thermal design (BN-boss conductive via to an external fan-cooled heatsink, ~90% of module heat exported, cavity left stagnant/unventilated to preserve the EMF shield):

| Condition | Face temp | Margin to 42 °C |
|---|---:|---:|
| Worst-case ambient (43.3 °C), T1-std flux | 46.7 °C | **FAIL by 4.7 °C** — but this ambient is already in NP-ENV-OPRANGE-001's block zone (>43 °C), so PBM wouldn't run here anyway |
| **Nominal ambient (25 °C), T1-std flux** | **30.7 °C** | **PASS, +11.3 °C margin** |

The nominal-ambient case is the one that matters for "how many tiles can run during normal use," and it shows substantial headroom on a single, thermally-isolated tile. But the R1 model explicitly assumes a **single periodic hex cell with adiabatic side walls** — it cannot answer the aggregate question by construction, because it assumes zero heat transfer between adjacent tiles. NP-HEX-ZM-001 §6 flags this directly: "whole-vault active tiling raises aggregate scalp thermal load." NP-HW-HEXTILE-001 §9.3 then resolves that concern circularly — because the power budget already caps concurrency at ~6 tiles, and 6 tiles' area resembles the previously-validated 5-slot footprint, the aggregate question is declared bounded, without ever computing it independently.

### 3.2 A first-order aggregate estimate

The BN-boss via exports ~90% of each active tile's heat directly to an external heatsink, bypassing the stagnant cavity almost entirely. The residual 10% is what accumulates in the shared, unventilated cavity — and *that* residual is the real aggregate-concurrency question, because the exported 90% is a heatsink-sizing problem (elastic — see §3.3), not a scalp-safety problem.

**Assumptions (stated, not measured):**
- Per-tile duty-averaged draw at the R-4 pulsed spec (400 mW/cm², 25% duty): 6.25 W (NP-HW-HEXTILE-001 §9.2).
- Residual to cavity per active tile: 10% × 6.25 W = 0.625 W.
- Effective vault surface area for cavity-to-ambient rejection: ~1000 cm² = 0.1 m² (NP-HELMET-GEOM-001 §... "standard over ~1000 cm²").
- Cavity-to-ambient thermal resistance: using the R1 fan-off "outward path" figure (≈0.41 m²K/W total, dominated by the ≈0.23 m²K/W stagnant air gap) as a stand-in, since the cavity itself is never actively ventilated regardless of fan state — the fan cools the external heatsink at the via terminus, not the cavity air.
- Heat from N concurrently active tiles is treated as uniformly distributed across the whole vault area (a simplification — see caveat below).

**Result:** ΔT per active tile ≈ (0.625 W × N / 0.1 m²) × R, giving roughly **2.6 °C/tile at R = 0.41** or **1.4 °C/tile at R = 0.23**. Against the 11.3 °C single-tile margin, that consumes the margin at:

| Resistance assumed | Tiles before margin exhausted |
|---|---:|
| 0.41 m²K/W (conservative, full outward path) | **~4** |
| 0.23 m²K/W (optimistic, air-gap-only) | **~8** |

**This brackets the existing power-derived ~6-tile figure.** That is worth stating plainly: the current concurrency limit, arrived at for the wrong reason (an inherited wattage number, not a thermal calculation), lands inside the range a first-order thermal estimate independently produces. This is not confirmation — it's a rough lumped model against a single-cell CFD result that wasn't designed to answer this question — but it means the existing ~6-tile rule is probably not leaving large amounts of safe headroom on the table, and is also probably not unsafe by a wide margin either. **It needs the real verification-grade multi-tile CFD (extending OI-R1-01) before either direction can be trusted**, but "redesign the whole power architecture to unlock many more concurrent tiles" is not obviously the right next move based on what's here — improving the export path (§3.3) is more likely to move the number than a bigger battery is.

**Caveat this estimate cannot resolve:** the model spreads N tiles' residual heat uniformly across the *whole* vault area. A protocol that clusters 6 tiles over one region (e.g., bilateral DLPFC) concentrates that residual heat locally rather than spreading it — the true local ΔT for a clustered montage is worse than this whole-vault average suggests, while a spatially-distributed 6-tile activation is better. **The current flat "~6 tiles, anywhere" rule does not distinguish clustered from distributed montages, and this analysis suggests it should.** New open item, §8.

### 3.3 The actual lever: export efficiency, not battery size

Because 90% of each tile's heat already bypasses the scalp-adjacent cavity via the BN-boss/via path to an external, fan-cooled heatsink (idealized as a "perfect sink" in the R1 study), that exported portion is a **heatsink/fan capacity problem** — genuinely elastic, addressable with bigger hardware exactly as the brief for this document invited ("we can adjust battery and power circuits as needed"). But the *cavity* cannot be ventilated without breaching the EMF shield — that's a fixed architectural constraint no battery or fan fixes. The only way to raise the aggregate ceiling beyond what §3.2 estimates is:

1. Improve via export efficiency beyond ~90% (reduces the residual that accumulates in the sealed cavity), or
2. Give the sealed cavity a better non-RF-breaching conductive path to the outer shell (e.g., via the existing mu-metal/palladium-polyester layers, which are already thermally conductive), or
3. Accept montage-dependent concurrency limits (§3.2 caveat) rather than a flat number.

**A bigger battery alone does not raise PBM concurrency** — the bottleneck by this estimate is the sealed cavity's residual-heat export path, not available wattage.

### 3.4 The constraint this document's methodology omitted — an efficacy floor

§1 frames every modality as bounded from **above**: inelastic (dose-capped) or elastic
(heat-removal-bound). Both are ceilings. **A budget bounded only from above has no lower bound**, so
it cannot distinguish *enough* power from *any* power — which is how §3.2 could bracket the tile
count without once asking what a tile is for. The floor was never missing from the programme, only
from this document's method.

`docs/pbm_neuro_protocols.md` supplies it. The positive trials cluster in a band:

| Quantity | Band | Source |
|---|---|---|
| Scalp irradiance | **0.02–0.3 W/cm²** | MASTER SUMMARY, every transcranial row |
| Session dose | **10–120 J/cm²** | ditto |
| Session length | **6–30 min** | ditto |
| Minimum threshold | **≥10–60 J/cm², delivered repeatedly** | dosimetry lesson 1 |

The lesson attached to that table is the load-bearing part: *"Under-dosing, not mechanism failure,
explains most nulls"* — NEST-1/2/3 (808 nm, stroke) and Iosifescu 2022 (830 nm, depression) failed
on delivered energy, not on principle.

**Consequence for this document.** The PBM envelope is not generous headroom above a floor of zero;
it is a ceiling roughly one decade above a floor that matters. §3.2's 4–8 tile estimate should be
read against 0.1–0.3 W/cm² as the thing being bought, and `NP-HW-HEXTILE-001` §4.3.1's design point
(403 mW/cm² per channel at full drive, 200 mW/cm² CW) is the top of the validated band, not an
arbitrary ambition.

**One channel does not clear the floor comfortably.** `NP-HW-HEXTILE-001` §4.3.2 gives the 1064 nm
channel **28 mW/cm²** at 30 sites — inside the band, but **9× below the 0.25 W/cm² that the Grade A
cognitive-enhancement protocol specifies**, and a 90-site 1064-only tile would still reach only
~85 mW/cm². That is an emitter-efficiency wall (η_wp ≈ 4.8 %), not a power-budget one: more watts
cannot fix it, because the tile is already at the R-6 current ceiling. Raised on the owning document
as `OI-HEXTILE-21`; recorded here because it is the one place where the *floor*, not the ceiling,
binds.

### 3.5 Full population at full drive — the bound, stated once

§3.2 asked how many tiles the thermal path tolerates. The inverse — what a **fully populated, fully
driven** lattice would draw — is asked often enough to be worth recording as a bound, and has never
been written down.

Inputs: 80 sockets (`NP-HW-HEXTILE-001` §8.2.1 `ROW_WIDTHS`, **PROVISIONAL** pending REG-1/ACT-1);
the most power-hungry tile type with a specified electrical figure.

**T1-A is the worst case, and T1-C is not.** Counterintuitively, adding the third channel *lowers*
per-tile draw, because 1064 nm's V_f (1.40 V) is well below CH_A's (2.10 V) and the 30/30/30 split
trades away high-V_f red sites:

| Type | Emitters | Instantaneous at 150 mA | Basis |
|---|---|---|---|
| **T1-A ★** | 45 + 45 | **25.0 W** | `NP-HW-HEXTILE-001` §8.1 |
| T1-C | 30 + 30 + 30 | ~23.0 W | `NP-HW-HEXTILE-001` §8.1 V_f figures × §4.2 counts |
| T1-B | ~44 | ~half of T1-A | `NP-HW-HEXTILE-001` §4.5 |

| Rail | 80 × T1-A, all channels |
|---|---|
| VLED at 24 V | 80 × 25.0 W = **2,000 W → 83.3 A** |
| 3.3 V logic, all modules active (`NP-HW-HEXTILE-001` §8.3) | 80 × 25 mA = 2.0 A → 6.6 W |
| Non-PBM overhead (`NP-HW-HEXTILE-001` §9.1) | ~6–8 W |
| **Total** | **≈ 2.0 kW** |

Time-averaged at the R-4 duty cap: **~514 W**. Dual-channel CW at 200 mW/cm²: **~1,014 W**. The
instantaneous 2 kW still sizes the rail, the contacts and the cluster switches in all three cases —
at 80 tiles this is beyond what the carrier bulk decoupling of `NP-DRV-SHELL-002` §9.2 absorbs.

**Four independent collisions, in increasing order of severity:**

1. **Power delivery.** CLAUDE.md §4.5 tops out at 20 V/5 A = 100 W EPR. 2 kW is **20× the maximum
   negotiable contract** and **8.3× the entire USB-C PD 3.1 EPR ceiling** (240 W). At mains it is
   16.7 A on 120 V — over a standard 15 A branch circuit.
2. **Distribution.** 83.3 A must cross the `NP-HEX-ZM-001` §5.3c posterior blind-mate boss and the two-bowl parting
   plane. `NP-HW-HUB-001` OI-HUB-C19 sizes the 15–20 V → 24 V boost at **~35 W / ~1.46 A**; this is
   ~57× that. Each of the 18 Class B cluster gates would carry a 6-tile cluster at 6.25 A.
3. **The 3.3 V budget breaks its own requirement.** `NP-HW-HEXTILE-001` §8.3's ~310 mA / ~1.0 W total assumes 6 active
   and 74 in standby. All-80-active is 2.0 A / 6.6 W — 6.6× a figure that is binding on
   `OI-HEXTILE-07`.
4. **Thermal — the binding one, and it fails by the widest margin.** At η_wp 0.30–0.45 (`NP-THERM-CFD-001`
   §4), 2 kW puts **1,100–1,400 W at the LED plane**, or **1.1–1.4 W/cm²** over the
   ~1,000 cm² vault. That is **4–5.6× the worst-case flux modelled anywhere in the tree**
   (0.25–0.35 W/cm², T2 peak, 1170 nm laser zone) and ~7× the T1-peak figure. Even the duty-capped
   514 W case lands at 0.28–0.35 W/cm² — the current worst-zone flux, applied vault-wide instead of
   in one zone. Extending §3.2's lumped model to N = 80 gives a rise of order 100 °C; the model
   breaks down long before that, but the sign and order are unambiguous, and they agree with §3.2's
   own conclusion that the 42 °C limit binds somewhere near 4–8 tiles.

**This is not a "buy a bigger supply" problem, and §3.3 is why.** PBM is elastic, but the elastic
lever is export efficiency, and the residual path terminates in a cavity that cannot be ventilated
without breaching the EMF shield. **A fully populated, fully driven lattice is unsafe before it is
unpowered.**

**The distinction that makes this finding usable: *populated* is not *driven*.** Populating 80
sockets costs BOM (`NP-HW-HEXTILE-001` §6.4, ~$920/headset) and **standby** current (80 × 2 mA =
160 mA / 0.53 W) — not LED power. A fully populated helmet obeying the ~6-tile governor draws
exactly what a 20-tile build draws. **Full population is a cost decision (`OI-HEXTILE-06`); full
activation is the physically impossible one**, and the two are routinely conflated. Routed to
`OI-HEXTILE-06` as a decision input.

### 3.6 A whole-vault low-irradiance mode is affordable, and is not specified anywhere

§3.5's arithmetic runs the other way too, and the result is more interesting than the bound.

Because irradiance scales linearly with drive, the whole 80-tile lattice can be lit simultaneously
provided the per-tile drive is scaled down:

| Target scalp irradiance | Fraction of full drive | 80-tile electrical | Inside the 45–50 W envelope? |
|---|---|---|---|
| 6 mW/cm² (a marketed helmet's *measured* output — see `docs/reference/competitive-position.md`) | 1.5 % | **~30 W** + ~14 W overhead ≈ **44 W** | **Yes**, marginally |
| 20 mW/cm² (bottom of the §3.4 band) | 5.0 % | ~99 W | No |
| 40 mW/cm² | 9.9 % | ~198 W | No |

**So whole-vault simultaneous illumination is a deliverable operating mode at the bottom of the
band, and only there.** This matters because the strongest indication in the evidence base asks for
exactly that geometry: `pbm_neuro_protocols.md` grades Alzheimer's **A** and specifies the site as
*"whole-head + intranasal"*, and cross-cutting principle 4 is *"target the network, not one spot."*
`NP-HW-HEXTILE-001` §9.3 concludes that the lattice buys *"placement options, not capability"* — on
this arithmetic that conclusion is too strong, because a mode exists that six tiles cannot produce
at any drive level.

**It is also the honest limit.** At 6 mW/cm² × 20 min the delivered dose is **7.2 J/cm²**, below
§3.4's ≥10 J/cm² threshold. The affordable whole-vault mode is *coverage*, not a therapeutic dose,
and must never be presented as the latter. What the architecture cannot do is whole-vault coverage
**and** in-band irradiance simultaneously; it must choose, and today it can only express one of the
two choices.

Routed to `OI-HEXTILE-09` (the governor has to permit this shape) and `OI-HEXTILE-06` (it is an
argument *for* populating more sockets that the cost model cannot see). New item `OI-PWR-07`.

### 3.7 Local irradiance and total optical output are different quantities

A distinction that is easy to lose and changes which claims are defensible.

**Total optical output is capped by the power envelope, not by emitter count.** At η_wp ≈ 34 % for
the T1-A mix, the ~38–42 W available to emitters (`NP-HW-HEXTILE-001` §9.1) yields **~13–14 W
optical, whatever the tile population.** Adding sockets redistributes that budget; it does not
enlarge it.

| Quantity | Governed by | Scales with more tiles? |
|---|---|---|
| **Local irradiance** (mW/cm²) | emitter areal density × per-emitter flux | No — set by the tile, not the lattice |
| **Total optical output** (W) | the USB-C PD envelope | **No** |
| **Coverage** (cm² illuminated at once) | envelope ÷ chosen irradiance | **Yes** |

**Only the third scales.** The practical consequence is that for a *whole-head dose target* the
concurrency ceiling barely matters — six bright tiles and eighty dim ones deliver the same joules
per second — while for a *focal high-irradiance protocol* (bilateral DLPFC at ≥60 J/cm²/site,
cognitive PFC at 0.25 W/cm²) the tile-level irradiance is the whole ballgame and coverage is
irrelevant.

**Do not state the emitter count as a capability claim.** `NP-OPT-PSF-001` §3.3 settles the
temptation: the cortical resolution floor is **26.2 mm, ~65 % of the 40 mm tile pitch**, and *"there
is little spatial selectivity left to buy below module granularity."* The 8.6 emitters/cm² lattice
buys **irradiance, not spatial precision.** `docs/reference/competitive-position.md` carries the
comparative form of this finding.

---

## 4. TMS — no power budget exists anywhere in the design

A full search of the document tree (CLAUDE.md, NP-DT-001, NP-HELMET-GEOM-001, NP-HW-HUB-001, NP-ENV-OPRANGE-001, NP-THERM-CFD-*) found **zero** electrical specification for TMS coil drive — no capacitor bank, no discharge voltage, no coil current, no driver topology, no pulse energy. Every reference covers field strength (0.1–0.5 T), coil geometry (focal figure-8), and the EMF-gating interlock timing (5 ms pre-pulse / 50 ms hold) — never power. TMS also does not appear in the hub power architecture or the thermal power-budget table (NP-THERM-CFD-001 §4): the T2-peak figure (70–74 W) is explicitly annotated there as **"worst-zone LED-plane heat flux (1170 laser zone)"** — i.e., it was sized for 1170nm PBM, not for TMS. TMS is simply absent from the electrical model.

### 4.1 Why this is not a small gap

This section uses general TMS device engineering knowledge (not project-specific citations — flagged as an open item requiring a real literature/datasheet source before any number here is trusted) to put an order-of-magnitude bound on what TMS coil drive actually needs, so the gap can be sized rather than just noted.

Clinical figure-8 rTMS stimulators discharge a capacitor bank (typically ~1600–2000 V, ~150–200 µF, storing on the order of 200–500 J) into the coil over roughly 100–300 µs to produce ~1.5–2.0 T at the coil face — the field level needed for the 120% resting-motor-threshold protocols the project's own evidence base targets (`docs/neuromod_neuro_protocols.md`: "figure-8, left DLPFC, 10Hz, 120% RMT, 3000 pulses"; iTBS "600 pulses, 3 min"). For a fixed coil geometry, discharge energy scales roughly with the square of peak field (E ∝ I² ∝ B²), so scaling down to NeurOne's stated 0.1–0.5 T target range against a ~1.5–2.0 T clinical reference gives:

| NeurOne target field | Scaled energy/pulse (order of magnitude) |
|---|---:|
| 0.1 T (bottom of range) | ~1–2 J |
| 0.5 T (top of range) | ~16–40 J |

**Average power during an active pulse train** (capacitor must recharge between pulses at the protocol's pulse rate):

| Protocol | Rate | At 0.1 T (~1.5 J/pulse) | At 0.5 T (~40 J/pulse) |
|---|---|---:|---:|
| Standard rTMS | 10 Hz | ~15 W | **~400 W** |
| iTBS | 600 pulses / 190 s ≈ 3.2 Hz avg | ~5 W | **~130 W** |

**At the top of NeurOne's own stated field range, sustaining the pulse rate the project's cited clinical protocols require draws more average power than the entire currently-allocated T2-peak budget (70–74 W) — for TMS alone, before any other T2 modality, EMF-shield gating overhead, or safety-MCU load is added.** Even the *low* end of the field range (0.1 T) consumes a meaningful fraction of the existing peak budget for a single modality that isn't in the model at all today.

### 4.2 A second, separate finding: the field target itself may be too low

Independent of power: 0.1–0.5 T is well below the ~1.5–2.0 T that commercial figure-8 coils typically need to reach 120% RMT in most subjects. If NeurOne's 0.1–0.5 T spec is a hard constraint (driven by the wearable form factor, coil size, or Helmholtz-cancellation interaction), the protocols the evidence base cites (`docs/neuromod_neuro_protocols.md` §TMS) may not be achievable at all with this coil, regardless of how the power budget is resolved. This is a coil electromagnetic design question, not a power question, and is out of scope for this document — flagged as a new open item (§8) because it changes what §4.1's energy estimate should even be scaled to.

### 4.3 Recommendation

TMS cannot be assigned a trustworthy power figure today. Per direction from design review, the next step is **quantifying the real requirement**, not picking an architecture yet: (1) resolve §4.2 — confirm the actual field strength needed for the targeted clinical protocols and whether 0.1–0.5 T is real or a placeholder; (2) get an actual coil inductance/geometry estimate so E ∝ B² scaling can be replaced with a real capacitor-bank calculation; (3) only then decide whether TMS shares the main battery-buffered rail (requiring a purpose-built high-energy capacitor bank and a PD contract likely well above the current 100 W EPR ceiling) or is powered externally/mains-tethered for T2 sessions (excluding TMS from Mode 3 autonomous operation). This is routed to principal decision, §7.

## 5. 1170nm deep PBM — the modality T2-peak is actually sized for

NP-THERM-CFD-001 §4 shows the current T2-peak figure (70–74 W) was derived as a thermal budget for the 1170nm laser zone specifically (worst-zone flux ~0.25–0.35 W/cm², annotated "1170 laser zone"). This modality has a real efficiency penalty the LED-based PBM tiles don't: wall-plug efficiency for laser diode + TEC is η_wp ≈ 0.15–0.25, versus 0.30–0.45 for the 660/808/1064nm LEDs — meaning 75–85% of drawn electrical power becomes heat that the TEC must then actively pump away, at additional power cost. This is a genuinely elastic (heat-removal-bound) modality like PBM tiles, but a more expensive one per watt of useful optical output, and its own dedicated thermal validation is still open (FAI-T2-05, "hardware bench — PENDING" per NP-SES-1064-001 §12). The existing T2-peak figure is a reasonable placeholder for this modality specifically; it simply isn't a number that has ever included TMS, per §4.

## 6. What this means for CLAUDE.md §4.5

| Config | Current figure | This analysis |
|---|---|---|
| T1 standard / T1 peak | 17–20 W / 45–50 W | Plausible — §3's independent thermal estimate brackets the existing PBM-driven number (4–8 tiles vs. the assumed 6). Not confirmed to verification grade, but not obviously wrong either. **Rev 2 adds two bounds on either side of it:** the envelope is ~2 % of what a fully populated, fully driven lattice would need (§3.5), and it is sufficient for whole-vault illumination only at the very bottom of the efficacy band (§3.6). Neither changes the figure; both change what it should be read as — a *choice* between coverage and irradiance, not a headroom margin. |
| T2 standard / T2 peak | 44–46 W / 70–74 W | **Not trustworthy as stated.** Built for 1170nm PBM only (§5); does not include TMS (§4), which by itself likely exceeds the entire current T2-peak envelope during an active pulse train at clinically-relevant field strength. |

**This document does not propose new locked numbers for §4.5.** T1's figures can likely stand pending the real multi-tile CFD (§3.2). T2's cannot be revised responsibly until §4's two open questions (real field-strength requirement, real coil energy) are answered — at which point the honest range is anywhere from "no change" (if TMS ends up externally powered) to "PD contract and onboard energy storage both need a substantial increase" (if TMS must share the wearable's rail).

## 7. Decisions

Recorded so they can be challenged individually. None is locked; all are proposals for design review.

| ID | Decision | Rationale | Reversible? |
|---|---|---|---|
| **D-1** | Frame future power-budget work as elastic (heat-removal-bound) vs. inelastic (dose-capped) per modality, not as a single wattage number | Matches how the constraints actually behave under "add more battery"; prevents re-deriving §4.5 from an inherited figure again | Yes — a documentation/process convention |
| **D-2** | PBM concurrency: keep the existing ~6-tile rule pending verification-grade multi-tile CFD, but add a montage-clustering caveat (§3.2) rather than treating "6 tiles anywhere" as equivalent | The lumped estimate suggests clustered montages carry more local risk than distributed ones, which the current flat rule doesn't capture | Yes |
| **D-4** *(Rev 2)* | **The global concurrent-power governor must be denominated in watts against the negotiated PD contract, not in a tile count.** A fixed "~6 tiles" rule is wrong in both directions: it forbids the ~30 W whole-vault mode of §3.6 (80 tiles, well inside the envelope) and permits 6 tiles at full dual-channel drive (150 W, well outside it) | Tile count is a proxy for power that fails as soon as per-tile drive is variable — and per-tile drive is variable by design, since irradiance is the therapeutic parameter (§3.4). `OI-HEXTILE-09` is the owning item; `NP-DRV-SHELL-002` SH2-DRC-02b's pass condition is currently written as *"global governor present"* and should state the unit | Yes — a firmware/spec convention, not committed hardware |
| **D-5** *(Rev 2)* | **State the efficacy floor alongside every ceiling this document derives.** §1's elastic/inelastic frame is retained unchanged and extended, not replaced | A ceiling-only method cannot distinguish an adequate budget from any budget; §3.4 is the omission being corrected, and it is the reason the T1 envelope reads as tight rather than generous | Yes — a documentation/process convention, like D-1 |
| **D-3** | TMS power architecture: **not decided.** Quantify field-strength requirement and real coil energy (§4.3) before choosing shared-rail vs. externally-powered | Committing to an architecture before the energy number is real risks building the wrong capacitor bank or wrongly ruling out the shared-rail option | N/A — no decision made yet |

## 8. Open items

| ID | Item | Blocking for |
|---|---|---|
| **OI-PWR-01** | Verification-grade multi-tile aggregate CFD (extends OI-R1-01 to N-tile, non-adiabatic case), including montage-clustering sensitivity | Confirms/revises the ~4–8 tile estimate in §3.2 before it can replace the power-derived ~6-tile figure as the authoritative PBM concurrency rule |
| **OI-PWR-02** | TMS field-strength requirement: confirm whether 0.1–0.5 T (CLAUDE.md §3) is achievable-and-correct for the 120% RMT protocols the evidence base targets, or needs revision | Everything downstream of §4 — the energy estimate is only as good as the field target it's scaled to |
| **OI-PWR-03** | Real TMS coil inductance/geometry and capacitor-bank design, replacing the E ∝ B² scaling estimate in §4.1 with an actual calculation | TMS power architecture decision (D-3) |
| **OI-PWR-04** | TMS power-path decision: shared battery-buffered rail (needs purpose-built capacitor bank + likely >100 W EPR PD contract) vs. external/mains-tethered supply for T2 sessions | CLAUDE.md §4.5 T2 figures; Mode 3 autonomous-operation scope for T2 |
| **OI-PWR-05** | 1170nm laser+TEC dedicated thermal/electrical validation (tracks existing FAI-T2-05, still pending bench) | Confirms whether the current T2-peak figure (built for this modality) is itself accurate, independent of the TMS question |
| **OI-PWR-06** | Sourcing for §4.1's clinical TMS energy figures (currently general device knowledge, not a cited datasheet or paper) | Any number in §4 becoming a design input rather than an order-of-magnitude estimate |
| **OI-PWR-07** | **Is a whole-vault low-irradiance mode a product feature?** §3.6 shows ~30 W buys simultaneous illumination of all 80 sockets at ~6 mW/cm², inside the existing envelope, and that the Grade A Alzheimer's protocol asks for exactly that *geometry*. It is **not** a therapeutic dose at that irradiance (7.2 J/cm² in 20 min, below the §3.4 threshold), so adopting it requires deciding what it is *for* — coverage, comfort, a low-intensity maintenance protocol — and saying so in the protocol library, never as a dose claim | Product + Clinical. Interacts with `OI-HEXTILE-06` (an argument for populating more sockets that the cost model cannot see) and `OI-HEXTILE-09` (the governor must permit the shape) |
| **OI-PWR-08** | **The §3.2 lumped model was never exercised beyond N ≈ 8, and §3.5 extrapolates it to N = 80.** The ~100 °C figure is stated only to fix the sign and order; it is outside the model's validity and must not be quoted as a temperature. Fold the high-N case into `OI-PWR-01`'s verification-grade multi-tile CFD rather than treating it as an independent estimate | Any use of §3.5's thermal figure beyond "the 42 °C limit binds long before full population" |
| **OI-PWR-09** | **The efficacy band in §3.4 is drawn from `docs/pbm_neuro_protocols.md`, which is a protocol digest, not a controlled document.** It has no serial, no revision, and no DHF entry, yet Rev 2 now makes it load-bearing for the *lower* bound of the power budget. Either register it or cite its underlying trials directly before any §3.4 figure becomes a design input | Design-control traceability for the floor; `NP-DHF-001` registration |
| **OI-PWR-10** | **CLAUDE.md §4.5's T1 rows do not distinguish clustered from distributed montages, and Rev 1 §3.2's caveat is still unaddressed.** §3.6 sharpens the case: the same envelope supports 6 clustered tiles or 80 distributed ones with very different local thermal outcomes, so a single wattage row cannot describe both. Carried forward rather than closed | Follows `OI-PWR-01`; feeds D-2's montage-clustering caveat |

## 9. Cross-references

CLAUDE.md §3 (modality stack), §4.5 (power — under interrogation here) · NP-HW-HEXTILE-001 §9 (power-derived concurrency ceiling this document reverses), §4.2/§4.3 (emitter allocation and irradiance — the inputs to §3.5), §6.4 (BOM; `OI-HEXTILE-06`, to which §3.5 and §3.6 are routed), §8.1 (VLED rail, the 25.0 W/tile figure), §8.3 (3.3 V logic budget) · NP-OPT-PSF-001 §3.3 (26.2 mm cortical resolution floor — why §3.7 says density buys irradiance, not precision) · `docs/pbm_neuro_protocols.md` (MASTER SUMMARY + dosimetry lesson 1 — the efficacy floor of §3.4) · `docs/reference/competitive-position.md` (the comparative form of §3.6/§3.7) · NP-THERM-CFD-R1-001 (single-tile thermal model, BN-boss export study) · NP-THERM-CFD-001 §4 (heat-source model, η_wp, T2-peak = 1170-zone derivation) · NP-HEX-ZM-001 §6 (aggregate thermal concern, raised but not resolved) · NP-ENV-OPRANGE-001 (ambient/duty firmware gate) · `docs/neuromod_neuro_protocols.md` (TMS clinical protocol parameters used in §4.1) · NP-DT-001 DI-PERF-12/13 (TMS and 1170nm design inputs, both still Open/Partial)
