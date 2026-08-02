# Power Budget — Reverse-Engineered from Safety Ceilings (Design Study)

**Project:** NeurOne
**Document:** NP-PWR-BUDGET-001
**Revision:** A
**Date:** 2026-08-02
**Status:** DESIGN STUDY — not a tooling baseline. Every numeric value below is a derived engineering estimate with its assumption chain stated inline, not a measured or locked figure. See §7 (Decisions) and §8 (Open Items). **Does not modify CLAUDE.md §4.5** — that requires a principal decision (§7).
**Effective Date:** —
**Author:** NeurOne Hardware Engineering
**Approved By:** — (pending design review)
**References:** CLAUDE.md §3 (modality stack), §4.2 (safety architecture), §4.5 (power — the figure this document interrogates); NP-HW-HEXTILE-001 Rev A §9 (concurrent-tile ceiling, power-derived); NP-THERM-CFD-R1-001 Rev A (single-tile thermal model, BN-boss export study); NP-THERM-CFD-001 (heat-source model, η_wp figures); NP-ENV-OPRANGE-001 (ambient/duty envelope); NP-HELMET-GEOM-001 (scalp area estimate); NP-DT-001 (DI-PERF-12/13, TMS and 1170nm design inputs)
**Related Issues:** —
**Gate:** Routes two items to principal decision (§7) before any change to CLAUDE.md §4.5
**IEC 62304 Class:** — (analysis document; no code)
**Supersedes:** — (new document; first attempt to derive CLAUDE.md §4.5 from safety ceilings rather than treat it as a given input)
**Parent Document:** CLAUDE.md §4.5

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
| T1 standard / T1 peak | 17–20 W / 45–50 W | Plausible — §3's independent thermal estimate brackets the existing PBM-driven number (4–8 tiles vs. the assumed 6). Not confirmed to verification grade, but not obviously wrong either. |
| T2 standard / T2 peak | 44–46 W / 70–74 W | **Not trustworthy as stated.** Built for 1170nm PBM only (§5); does not include TMS (§4), which by itself likely exceeds the entire current T2-peak envelope during an active pulse train at clinically-relevant field strength. |

**This document does not propose new locked numbers for §4.5.** T1's figures can likely stand pending the real multi-tile CFD (§3.2). T2's cannot be revised responsibly until §4's two open questions (real field-strength requirement, real coil energy) are answered — at which point the honest range is anywhere from "no change" (if TMS ends up externally powered) to "PD contract and onboard energy storage both need a substantial increase" (if TMS must share the wearable's rail).

## 7. Decisions

Recorded so they can be challenged individually. None is locked; all are proposals for design review.

| ID | Decision | Rationale | Reversible? |
|---|---|---|---|
| **D-1** | Frame future power-budget work as elastic (heat-removal-bound) vs. inelastic (dose-capped) per modality, not as a single wattage number | Matches how the constraints actually behave under "add more battery"; prevents re-deriving §4.5 from an inherited figure again | Yes — a documentation/process convention |
| **D-2** | PBM concurrency: keep the existing ~6-tile rule pending verification-grade multi-tile CFD, but add a montage-clustering caveat (§3.2) rather than treating "6 tiles anywhere" as equivalent | The lumped estimate suggests clustered montages carry more local risk than distributed ones, which the current flat rule doesn't capture | Yes |
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

## 9. Cross-references

CLAUDE.md §3 (modality stack), §4.5 (power — under interrogation here) · NP-HW-HEXTILE-001 §9 (power-derived concurrency ceiling this document reverses) · NP-THERM-CFD-R1-001 (single-tile thermal model, BN-boss export study) · NP-THERM-CFD-001 §4 (heat-source model, η_wp, T2-peak = 1170-zone derivation) · NP-HEX-ZM-001 §6 (aggregate thermal concern, raised but not resolved) · NP-ENV-OPRANGE-001 (ambient/duty firmware gate) · `docs/neuromod_neuro_protocols.md` (TMS clinical protocol parameters used in §4.1) · NP-DT-001 DI-PERF-12/13 (TMS and 1170nm design inputs, both still Open/Partial)
