# PBM Session Power Audit — Predefined Protocol Library Against the Concurrency Ceiling

**Project:** NeurOne
**Document:** NP-SES-PWR-001
**Revision:** 2
**Date:** 2026-10-04
**Status:** DRAFT — design study; not a tooling or release baseline. Every figure is derived from the cited specifications and from the authored protocol files; none is measured. See §7 (Decisions) and §8 (Open Items).
**Effective Date:** —
**Author:** NeurOne Systems Engineering
**Approved By:** — (pending design review)
**References:** NP-HW-HEXTILE-001 Rev 7 (§4.2 emitter allocation, §4.3 irradiance, §8.1 VLED rail, §9 concurrency ceiling, `OI-HEXTILE-09`/`-20`/`-21`); NP-PWR-BUDGET-001 Rev 2 (§3.4 efficacy floor, §3.5 full-population bound, §3.6 whole-vault mode, §3.7 irradiance vs total output, D-4); NP-NPPS-REF-001 (§4.1 `pbm_transcranial`, §5 intervals, §6 composite); NP-HEX-ZM-001 (§3 lattice, §4b wire format); NP-OPT-PSF-001 Rev 1 (§3.2 cortical PSF); `protocols/predefined/00-zones.npps` (ZONE-1); `docs/pbm_neuro_protocols.md` (evidence band); CLAUDE.md §3 (modality stack), §4.5 (power)
**Related Issues:** PR #284
**Gate:** — (no gate; routes findings to `OI-HEXTILE-09` and to the items in §8)
**IEC 62304 Class:** — (analysis document; the audit script is a build-time report, not device software)
**Supersedes:** — (new document)
**Parent Document:** NP-PWR-BUDGET-001

---

> **Rev 2 (2026-10-04) — every table re-derived after PBM intensity became absolute (`NP-NPPS-REF-001` Rev 18, `OI-NPPS-ABS-01`).** The library now states irradiance in mW/cm² with one block per wavelength, and `scripts/check-pbm-power.ts` reads **every** PBM block of a file against the per-channel full-scale table in `app/web/src/lib/pbmDrive.ts`, where Rev 1 read the first block against a percentage of full drive. Blocks that start at different times (Schiffer's F3 then F4) are no longer summed as if concurrent, and the CW+duty flag now fires only when a duty below 100 % accompanies `frequency: 0Hz`. **What moved:** the old "Depression (DLPFC)" and "Anxiety (PFC)" rows are gone, replaced by the Cassano, Schiffer, Maiello and Wang protocols that state their own irradiances (36, 250, 30 and 310 mW/cm²); the cognitive-1064 row is now `04-memory-boost` and its zone is 17 sockets; the pediatric autism protocol states 850 nm, which no channel delivers under the default wavelength rules, so it is **refused and has no row**; and **CW draw is no longer one number**, because CW now spans 0.6 to 20.0 W per tile with the irradiance each trial used. The conclusions of Rev 1 survive in kind, and §2.1, §2.2, §2.3 and §2.4 say where they did not survive in degree. **Caveat on the rows that carry a converted figure:** ten wellness presets, Woźniak-Mitał and Parkinson's state an irradiance that is their old percentage restated against 403 mW/cm² (register rows UC-064 to UC-066), and **1064 nm rows are capped at what CH_C delivers** (28 mW/cm²), because the compiler refuses the irradiance the protocols ask for.

> **⚠ READ FIRST — what this document is and is not.**
>
> `NP-HW-HEXTILE-001` §9 derives a concurrency ceiling of *"roughly six tiles… not eighty"* and raises **`OI-HEXTILE-09`**: nothing in the delivered v2 wire format prevents a protocol commanding more. That is stated as a *hypothetical gap*. This document asks whether it is an actual one, by running the arithmetic over the **authored protocol library** rather than over an imagined worst case.
>
> **It is.** Of 22 predefined protocols with a deliverable `pbm_transcranial` block, **2 fit the power envelope as authored**, 19 exceed it — by 1.03× to 40× — and 1 is indeterminate because its sockets are operator-selected. A 23rd, the pediatric autism protocol, is refused outright (850 nm). **Every one of them compiles clean today**, apart from the 1064 nm irradiance refusals: `app/web/src/lib/hubCompiler.ts` contains no power or budget check of any kind.
>
> **The second finding is larger than the first, and it is not a power finding.** The reason most protocols are over budget is that they target **lobe-scale zones where their own cited evidence specifies electrode-scale sites** — the Cassano depression protocol irradiates 37 sockets in service of a *bilateral DLPFC (F3/F4)* indication. That is a clinical-validity defect that happens to show up as a power number, and fixing it is a data edit, not a hardware change. It is filed separately (§8, `OI-SESPWR-01`) so it is not mistaken for a power-budget item.
>
> **This document does not change any protocol.** It measures, and routes.

---

## 1. Method

`scripts/check-pbm-power.ts` — run `bun scripts/check-pbm-power.ts`. It is the source of every table below and re-runs against the library as protocols change, so these figures cannot silently rot.

**Per-tile electrical draw at 100 % intensity and full 150 mA drive**, from `NP-HW-HEXTILE-001` §4.2 (emitter allocation) and §4.3 (V_f design targets — 660 nm 2.10 V, 808 nm 1.60 V, 1064 nm 1.40 V):

| `wavelength` | Allocation | Per tile |
|---|---|---|
| `660_808nm` | 45 + 45 (T1-A) | **25.0 W** |
| `660_808_1064nm` | 30 + 30 + 30 (T1-C) | 22.95 W |
| `1064nm` | 30 (T1-C, CH_C only) | 6.3 W |

Scaled by each protocol's own `intensity`, then by `duty_cycle` for pulsed modes. **Available to emitters: 40 W** — the R-10 envelope (45–50 W) less `NP-HW-HEXTILE-001` §9.1's ~6–8 W non-PBM overhead.

**Three inherited uncertainties, stated so no figure here is quoted as settled.** Every number inherits `OI-HEXTILE-02` (no emitter is selected; the V_f and flux figures are design targets), `OI-HEXTILE-20` (whether R-5's 600 mW/cm² aggregate ceiling makes the 25.0 W two-channel peak unreachable — if it does, every two-channel figure here falls ~25 %), and the **PROVISIONAL** status of the 80-socket lattice pending REG-1/ACT-1. **None of the three changes the conclusion for 17 of the 19 over-budget rows**, whose margins run from 1.7× to 40×. **Two are marginal and the uncertainties could flip them:** Cassano (1.03×) and TBI (1.2×). Both are single-channel CW protocols, so `OI-HEXTILE-20` does not reach them, but the 403 mW/cm² anchor and `OI-HEXTILE-02` do.

## 2. Result 1 — the library against the envelope

| Protocol | Sockets | W/tile | Needs | Max concurrent | Over by |
|---|---|---|---|---|---|
| Vascular Baseline | 80 | 20.0 | **1,600 W** | 2 | **40×** |
| PBM — Alzheimer's 1064 nm *(1064 nm, capped at CH_C's 28 mW/cm²)* | 71 | 6.3 | 447 W | 6 | 11× |
| Focus Prime | 80 | 5.0 | 400 W | 8 | 10× |
| Alpha Calm · Full T1 Immersive | 80 | 4.7 | 375 W | 8 | 9× |
| Gamma Focus | 71 | 5.0 | 355 W | 8 | 9× |
| Flow State | 70 | 5.0 | 340 W | 8 | 9× |
| Gamma + Theta Coupled | 70 | 4.7 | 318 W | 8 | 8× |
| PBM — Alzheimer's (Chun 2026) | 71 | 2.3 | 165 W | 17 | 4× |
| PBM — Anxiety (Wang 2023) | 17 | 9.6 | 163 W | 4 | 4× |
| ADHD Focus | 37 | 4.4 | 162 W | 9 | 4× |
| PBM — Depression (Schiffer 2009) | 20 | 7.8 | 155 W | 5 | 4× |
| Anxiety Relief | 37 | 4.1 | 150 W | 9 | 4× |
| Deep Sleep | 33 | 3.8 | 124 W | 10 | 3× |
| Memory Boost *(1064 nm, capped at 28 mW/cm²)* | 17 | 6.3 | 107 W | 6 | 3× |
| PBM — Mild Cognitive Impairment (Papi 2022) | 37 | 2.2 | 82 W | 18 | 2× |
| PBM — Alzheimer's 40 Hz (Woźniak-Mitał 2026) | 71 | 0.9 | 67 W | 42 | 1.7× |
| PBM — TBI (Naeser 2011/2014) | 71 | 0.7 | 48 W | 58 | 1.2× |
| PBM — Depression (Cassano 2018) | 37 | 1.1 | 41 W | 35 | 1.03× |
| **PBM — Anxiety (Maiello 2019)** | 37 | 0.9 | **34 W** | 42 | **fits** |
| **PBM — Parkinson's** | 7 | 3.1 | **22 W** | 12 | **fits** |
| PBM — Stroke (chronic rehab) | operator | 0.6 | — | 64 | indeterminate |
| PBM — Autism (pediatric) | — | — | — | — | **refused: 850 nm is outside the 808 nm window** |

*Re-derived 2026-10-04 with `bun scripts/check-pbm-power.ts`. Schiffer's draw is the worst of its two serial phases, not their sum. Rows in the audit's order differ only where two protocols tie.*

### 2.1 The "~6 tiles" rule is wrong, in both directions

This is the finding with the widest consequences, and it validates `NP-PWR-BUDGET-001` **D-4** against real data rather than argument.

`NP-HW-HEXTILE-001` §9.2 derives ~6 concurrent tiles from **6.25 W/tile** — 400 mW/cm² at 25 % duty, both channels, 100 % intensity. **No protocol in the library runs at that operating point** (the two 1064 nm rows land at 6.3 W only because they are capped at what the channel can deliver). Actual per-tile draw spans **0.6 W to 20.0 W**, so the honest concurrency limit spans **2 to 64 tiles**:

| Regime | Per tile | Concurrent | Example |
|---|---|---|---|
| Low-irradiance, CW or pulsed | 0.6–2.3 W | **17–64** | Stroke, Maiello, TBI, Cassano, Woźniak-Mitał, Chun |
| Typical pulsed, two channels | 3.8–5.0 W | 8–10 | Alpha Calm, Gamma Focus, Focus Prime |
| §9.2's assumed point | 6.25 W | ~6 | *(only the capped 1064 nm rows)* |
| **High-irradiance CW** | **7.8–20.0 W** | **2–5** | Vascular Baseline, Wang, Schiffer |

**A governor expressed as a tile count is therefore unsafe at one end and needlessly restrictive at the other** — it would permit 6 tiles at 20 W (120 W, 2.7× R-10) while forbidding 64 low-irradiance ones that fit comfortably. The check must be **watts against the negotiated PD contract**, with per-tile drive as an input. Recorded against `OI-HEXTILE-09` at `NP-HW-HEXTILE-001` Rev 7.

### 2.2 The binding constraint is high-irradiance CW, not the lattice

Worth separating, because the intuitive diagnosis — *too many sockets* — is right for one group and wrong for the other:

- **Pulsed protocols fail on socket count.** At 25 % duty they draw 3.8–6.3 W/tile, so 8–10 could run; they are authored against 33–80. The fix is scope (§3), not power.
- **High-irradiance CW protocols fail on per-tile draw.** With no duty cycle to average the draw down, one tile at 322 mW/cm² CW is 20 W — **half the entire emitter budget**. Three protocols (Vascular Baseline, Wang, Schiffer) are in this group, at 20.0, 9.6 and 7.8 W/tile.
- **Rev 1 said CW is always 12.5–20 W. That was an artefact of one percentage for every protocol.** The CW trials the library now follows state irradiances from 22 to 310 mW/cm², and the low-irradiance ones (Naeser 22, Maiello 30, Cassano 36) draw 0.7–1.1 W/tile, so their draw is small and only their socket count is a problem.

### 2.3 A separate compliance question the audit surfaced

`Vascular Baseline` states **322 mW/cm² CW at both 660 and 808 nm** (its former 80 % restated against 403 mW/cm², register row UC-064). Against the weighted-sum rule of CLAUDE.md §3 (Σ Ēᵢ / (200 × C_A(λᵢ)) ≤ 1) that is 322/200 = 1.6 on the 660 nm block alone, before the 808 nm term. **The 400 mW/cm² peak is not breached, and the validator now checks it directly; the average term is a different ceiling and nothing in the app checks it** (`OI-HEXTILE-30`, `OI-HEXTILE-31`).

**Stated as a question, not a defect.** The figure is a conversion, not an authored dose: if the preset's owner chooses a lower irradiance the question disappears. Until then, `hubCompiler.ts` caps duty at 25 % (`dutyReg`, 0x32) and applies no mode-dependent CW clamp, and no such clamp was found in firmware on the searches run for this document. That is not proof of absence. If the clamp exists the audit's CW rows overstate draw; if it does not, this is a breach of the average term independent of power. **`OI-SESPWR-02` — verify before either reading is relied on.** Schiffer (250) and Wang (310) are CW at 808 nm, inside that channel's ≈ 329 average limit, and are refused today for a different reason (§2.4's duty cap and `OI-HEXTILE-30`).

### 2.4 `frequency: 0Hz` with `duty_cycle:` is undefined, and it is worth 4×

Six protocols set `frequency: 0Hz` **and** a `duty_cycle:` below 100 % (Cassano, Maiello, TBI, Stroke, Memory Boost and the 1064 nm Alzheimer's protocol). `NP-NPPS-REF-001` §4.1 states that `frequency: 0` selects CW, and CW means 100 % duty — the two fields contradict each other. (Schiffer and Wang write `duty_cycle: 100%` and are not ambiguous. Rev 1 flagged them anyway, and listed four protocols.)

The compiler does not resolve it: `freqCode(0)` emits `0x00` (CW) and `dutyReg(25)` emits `0x32` **independently**, leaving the hub to decide. So the power draw of more than a quarter of the library depends on an unwritten semantic:

| Reading | Cassano draws |
|---|---|
| CW wins, duty ignored | **41 W** |
| Duty applied to CW | 10 W |

§2 reports the CW reading, the higher of the two. **This is not merely a documentation gap** — it is a 4× uncertainty in the input to any power governor, and it must be resolved *before* the governor is written, not after. **`OI-SESPWR-03`.**

## 3. Result 2 — the zones are lobe-scale; the evidence is electrode-scale

**This is the largest finding in the document and it is not about power.**

The zone vocabulary in `protocols/predefined/00-zones.npps` offers 15 zones, of which the smallest are 5 sockets:

| Zone | Sockets | | Zone | Sockets |
|---|---|---|---|---|
| Temporal L/R · Occipital L/R | 5 | | Frontal L/R | 20 |
| Motor / SMA | 7 | | Posterior | 33 |
| Frontal Right (excl. midline) | 17 *(re-cut 2026-09-29; was 8)* | | Frontal | 37 |
| Parietal L/R | 13 | | Vault (excl. Occipital) · All | 71 · 80 |
| Frontal Left (excl. midline) | 17 | | | |

> **Re-cut 2026-09-29.** `Frontal Right (excl. midline)` is now `Frontal Right` minus its x = 0 sockets: **17 sockets, ~236 cm²** (was 8, ~111 cm²). The old 8-socket list came from a retired row numbering that dropped the sign of x, and four of its sockets were left-hemisphere. `scripts/sync-socket-map.ts` now enforces parent-minus-midline for every "(excl. midline)" zone. The §2 and §4.1 rows for the 1064 nm protocol (now `04-memory-boost`) were computed on the 8-socket zone and are **stale**. They are marked in place, not recomputed; at 17 sockets the draw more than doubles.

**There is no DLPFC zone, no F3/F4 zone, and no zone corresponding to any single 10-20 site.** The finest frontal targeting available is a 20-socket half-lobe (17 without its midline sockets, the zone `clinical-05-pbm-anxiety-wang` uses since 2026-09-29).

Against that, `docs/pbm_neuro_protocols.md` MASTER SUMMARY specifies sites at 10-20 resolution — *"Bilateral DLPFC (F3/F4)"* for depression, *"Right (or bilateral) PFC"* for cognitive enhancement, *"Prefrontal (F3/F4)"* for MCI. `clinical-04-pbm-depression-cassano.npps` targets `["Frontal Left", "Frontal Right"]` for *bilateral DLPFC* — **37 sockets, ~9× the area its indication names.**

**Three consequences, in increasing order of seriousness:**

1. **Power.** Cassano's 37 sockets at 1.1 W is 41 W, just over budget on its own, and Schiffer's 20-socket half-lobe at 250 mW/cm² is 155 W. Four sockets at the same irradiance would be about 4 W and 31 W. **The protocol is over budget mainly because it is over-scoped**, and correcting the scope is a data edit to one `.npps` file — no firmware, no hardware, no revision to any specification.
2. **Dose distribution.** Irradiating the whole frontal lobe to deliver a DLPFC dose does not merely waste energy; it delivers the protocol's dose to tissue the evidence never studied, and — under a fixed power budget — **less** dose to the target than a focused montage would.
3. **Claim integrity.** A protocol titled *DLPFC* that irradiates the frontal lobe cannot support a DLPFC claim. `clinical-09-pbm-stroke-rehab.npps` already recognises exactly this hazard in its own comments — *"Whole-head irradiation is NOT the evidence target and would silently substitute the wrong dose distribution"* — and resolves it with `zones: clinician_selected`. **That reasoning was applied to one protocol and not to the rest.**

**Is finer targeting even meaningful?** Yes, but with a floor. `NP-OPT-PSF-001` §3.2 gives one 40 mm tile a **40.0 mm FWHM at cortex**, so a tile is approximately the resolution unit — targeting *below* one tile buys nothing, and targeting a lobe when the evidence says one electrode wastes ~9 of them. A DLPFC zone of 2 sockets per hemisphere is both physically meaningful and evidence-faithful. **`OI-SESPWR-01`** — and it needs REG-1 to fix socket-to-10-20 registration before the membership can be authored with confidence, which is the honest reason it is not done in this document.

## 4. Result 3 — what cascading can and cannot rescue

### 4.1 The arithmetic

Cascading — rotating through socket groups over time — **preserves total delivered energy and divides per-site time by the number of groups.** To hold dose per site, session length multiplies by the group count:

| Protocol | Groups | Authored | Cascaded to hold dose |
|---|---|---|---|
| PBM — Parkinson's · Maiello · Stroke | 1 | 20m · 20m · 15m | **unchanged — already fit (Stroke: operator-selected)** |
| PBM — Cassano · Woźniak-Mitał · TBI | 2 | 30m · 20m · 10m | 60m · 40m · 20m |
| Memory Boost | 3 | 30m | 1.5h |
| PBM — Mild Cognitive Impairment (Papi) | 3 | 10m | 30m |
| PBM — Schiffer · Deep Sleep | 4 | 8m · 45m | 32m · 3.0h |
| PBM — Chun · Wang · ADHD Focus · Anxiety Relief | 5 | 11m · 31m · 40m · 20m | 55m · 2.6h · 3.3h · 1.7h |
| Gamma + Theta · Gamma Focus · Flow State | 9 | 20m · 20m · 25m | 3.0h · 3.0h · 3.8h |
| Alpha Calm · Focus Prime | 10 | 20m | 3.3h |
| Full T1 Immersive | 10 | 30m | 5.0h |
| PBM — Alzheimer's 1064 nm | 12 | 6m | 72m |
| Vascular Baseline | 40 | 30m | **20.0h** |

*Group counts are the audit's; where one cell lists several protocols they share a count, not a duration. The two 1064 nm protocols are refused today, so their rows describe the capped draw.*

**There is no free lunch in the energy domain.** `NP-PWR-BUDGET-001` §3.7 is the general statement: total optical output is capped by the PD envelope regardless of how many tiles exist, so cascading redistributes joules in time without creating any.

### 4.2 What survives cascading, and why

**Dose-driven, non-rhythmic protocols survive** — Vascular Baseline, the 1064 nm protocols, the depression and anxiety trials, MCI. Photochemical response (CCO absorption, NO release) integrates over minutes, so if the rotation period is short relative to that integration time the tissue cannot distinguish time-multiplexed delivery from continuous delivery at the duty-averaged irradiance.

**Fast cascading is the same thing as `NP-PWR-BUDGET-001` §3.6's whole-vault mode**, expressed in the time domain rather than the current domain. Rotating 10 groups at 1 Hz and lighting all 80 tiles at 10 % drive deliver the same average irradiance to the same tissue. **This is worth stating explicitly because the two proposals look unrelated and are one mechanism.** It also inherits §3.6's honest limit: at whole-vault average irradiance the dose falls below the efficacy threshold unless the session is extended, which is §4.1's table again.

### 4.3 What cascading breaks — and it is the Grade A protocols

**Every 40 Hz entrainment protocol is invalidated by cascading**, not merely degraded: Alzheimer's (Grade A: Chun, Woźniak-Mitał), Gamma Focus, Gamma + Theta Coupled, Autism (Grade B; refused today), Full T1 Immersive.

The mechanism those protocols claim is **network-wide coherent rhythmic drive** — the gamma-entrainment rationale they cite by name, and the basis of the Grade A evidence. Sequentially flashing 40 Hz at region 1, then region 2, then region 3 is not whole-head 40 Hz stimulation; it is a different intervention, with a different temporal structure, and no evidence base. **Cascading such a protocol would preserve its J/cm² and silently destroy the thing being claimed** — which is the same failure `clinical-09` names for whole-head substitution, in the time domain instead of the spatial one.

**Two further classes degrade:**

- **Closed-loop EEG-adaptive protocols** adapt to a global brain state. If 1/10 of the head is lit at any instant, the loop regulates against a signal it is largely not driving.
- **Multi-modal phase-locked protocols.** `15-full-t1-immersive.npps` runs PBM, audio, tACS and visual **all at 40 Hz**. Cascading the PBM breaks the cross-modal phase relationship that is the protocol's entire premise.

**Consequence: cascading needs a per-protocol declaration of whether time-multiplexing is admissible**, and it must default to *no*. A cascade primitive without that field is a mechanism for silently invalidating the strongest protocols in the library. **`OI-SESPWR-04`.**

### 4.4 What the language can express today

| Mechanism | Where | Can it cascade zones? |
|---|---|---|
| `interval_on` / `interval_off` / `repeat` | `NP-NPPS-REF-001` §5, per modality block | **No** — toggles the whole modality on and off; the `zones` set is fixed for the session |
| `composite` + `conflict_resolution: sequential` | §6 | **Yes, clumsily** — chain N single-group protocols. Requires N full protocol definitions with duplicated metadata; Vascular Baseline would need 40 |
| Zone rotation inside `pbm_transcranial` | — | **Does not exist** |

So cascading is *expressible* but not *authorable at scale*, and nothing in the language records whether a given protocol may be cascaded at all.

## 5. What actually fixes this, in order of cost

**Ranked deliberately.** The cheapest fix is also the one that improves clinical fidelity, and the expensive fix (cascading) should be attempted last, on the smallest possible residue.

| # | Action | Cost | Effect |
|---|---|---|---|
| 1 | **Author evidence-faithful zones** (`OI-SESPWR-01`) | Data edit to `00-zones.npps`; gated on REG-1 | Removes most of the over-budget condition **and** corrects a claim-integrity defect. Cassano 41 W → ~4 W |
| 2 | **Resolve `frequency: 0Hz` + `duty_cycle`** (`OI-SESPWR-03`) | Spec sentence + compiler check | Removes a 4× uncertainty on a fifth of the library |
| 3 | **Verify the CW intensity ceiling** (`OI-SESPWR-02`) | Firmware read | R-4 compliance, independent of power |
| 4 | **Build the governor in watts** (`OI-HEXTILE-09`) | Compiler + session runner | Makes the remaining condition *detectable* rather than silent |
| 5 | **Then cascade the residue** (`OI-SESPWR-04`) | New NPPS primitive + firmware | Only for protocols that declare time-multiplexing admissible — which excludes every entrainment protocol |

**Steps 1–3 are prerequisites for 4, not parallel to it.** A governor built against the current library would reject 19 of 22 protocols, and the correct response to most of those rejections is to fix the protocol, not to cascade it.

## 6. Cross-references

`NP-HW-HEXTILE-001` §9 (the ceiling this document tests), §9.3 (`OI-HEXTILE-09`, and consequence 4 added at Rev 7), §4.2/§4.3 (the per-tile model), `OI-HEXTILE-20` (whether 25.0 W is reachable), `OI-HEXTILE-21` (the 1064 nm irradiance wall, which bounds `clinical-03`) · `NP-PWR-BUDGET-001` §3.4 (efficacy floor), §3.6 (whole-vault mode — §4.2 here shows it is the same mechanism as fast cascading), §3.7 (why cascading creates no energy), D-4 (governor in watts, which §2.1 confirms against data) · `NP-NPPS-REF-001` §4.1, §5, §6 · `NP-OPT-PSF-001` §3.2 (40 mm FWHM — the targeting floor for §3) · `docs/pbm_neuro_protocols.md` (site and dose specifications) · `protocols/predefined/00-zones.npps` (ZONE-1) · `app/web/src/lib/hubCompiler.ts` (`freqCode`, `dutyReg`, and the absent budget check) · `scripts/check-pbm-power.ts` (the audit)

## 7. Decisions

Recorded so they can be challenged individually. None is locked; all are proposals for design review.

| ID | Decision | Rationale | Reversible? |
|---|---|---|---|
| **D-1** | **The audit is a committed script, not a table in a document.** `scripts/check-pbm-power.ts` re-derives every figure here on demand | A hand-copied table goes stale the first time a protocol's `irradiance` is edited (it did, at Rev 18, which is why this is Rev 2), and nothing would catch it. `NP-CONV-001` §8's principle — *a convention worth writing down is worth a script* — applied to an analysis | Yes |
| **D-2** | **`--strict` is NOT wired into CI in this revision.** The script reports and exits 0 by default | 19 of 22 protocols fail today, so a gate would fail from the first commit and be disabled or bypassed — worse than no gate. Enable it once `OI-SESPWR-01..03` land (`OI-SESPWR-05`) | Yes — that is the plan, not a permanent exemption |
| **D-3** | **Report the CW reading (the higher draw) where §2.4's ambiguity applies** | The conservative direction while the semantic is undefined. Reverses to the duty reading if `OI-SESPWR-03` resolves that way | Yes |
| **D-4** | **File the zone-granularity finding as a clinical-validity item, not a power item** | It presents as a power number but is a claim-integrity defect (§3), and it would still need fixing if the power envelope were unlimited. Filing it under the power budget would let it close for the wrong reason — a bigger supply | Yes, but the classification is the point |

## 8. Open Items

| ID | Description | Owner / Blocking |
|---|---|---|
| **OI-SESPWR-01** | **Author evidence-faithful zones at 10-20 resolution.** No DLPFC, F3/F4 or single-site zone exists; the finest frontal targeting is a 20-socket half-lobe, and `clinical-04-pbm-depression-cassano` irradiates 37 sockets for a *bilateral DLPFC* indication (§3). One 40 mm tile is ~40.0 mm FWHM at cortex (`NP-OPT-PSF-001` §3.2), so ~2 sockets/hemisphere is both meaningful and faithful. **Blocked on REG-1** — socket-to-10-20 registration must be fixed before membership can be authored with confidence; that is the honest reason this is not simply done. **Interim:** follow `clinical-09`'s precedent and mark affected protocols `clinician_selected` rather than silently substituting a lobe | Clinical + Protocol authoring. **Gated on REG-1.** Largest single reduction in the §2 condition |
| **OI-SESPWR-02** | **Verify whether a mode-dependent CW intensity clamp exists in firmware.** `Vascular Baseline` states 322 mW/cm² CW (a converted percentage, UC-064) against R-4's 200 mW/cm² CW ceiling and the 660 nm average limit of 200 (§2.3). `dutyReg` caps duty and `protocolValidator` enforces a global intensity limit; neither is mode-dependent, and no clamp was found on the searches run here — **which is not proof of absence.** If absent, this is an R-4 breach independent of power | Firmware + Safety. **Potentially a compliance defect, not a budget one** |
| **OI-SESPWR-03** | **Define `frequency: 0Hz` combined with `duty_cycle:` — or reject it.** Six protocols set `frequency: 0Hz` with a duty below 100 %; `NP-NPPS-REF-001` §4.1 says `frequency: 0` selects CW and CW is 100 % duty. `hubCompiler.ts` emits `freqCode` and `dutyReg` independently. **A 4× swing in the power budget of a fifth of the library rests on the answer** (§2.4). Preferred resolution: make it a parser error, so the author states which they mean | NPPS spec + compiler. **Blocks `OI-HEXTILE-09`** — a governor cannot be written against an undefined input |
| **OI-SESPWR-04** | **A cascade primitive needs a per-protocol admissibility declaration, defaulting to *not admissible*.** Cascading preserves dose but destroys the network-wide rhythmic drive that every 40 Hz entrainment protocol claims (§4.3), including the Grade A Alzheimer's protocol; it also breaks closed-loop adaptation and the cross-modal 40 Hz phase-lock in `15-full-t1-immersive`. **A zone-rotation primitive without this field is a mechanism for silently invalidating the strongest protocols in the library.** Zone rotation inside `pbm_transcranial` does not exist today; `composite` + `sequential` can emulate it at N× the authoring cost (§4.4) | NPPS spec + session runner. **Sequence after `OI-SESPWR-01`** — most of the residue disappears once scope is corrected |
| **OI-SESPWR-05** | **Wire `--strict` into CI once `OI-SESPWR-01..03` land**, and decide what the gate does about protocols that are legitimately operator-scoped (`clinician_selected`), which the audit can only mark indeterminate | Systems + CI. Follows D-2 |
| **OI-SESPWR-06** | **`clinical-03-pbm-alzheimers-1064.npps` states an irradiance its hardware does not produce.** Since Rev 18 it states 100 mW/cm² (the low end of §1's band), and the compiler refuses it; before that its comment claimed *"~0.10 W/cm² average"*. `NP-HW-HEXTILE-001` §4.3.2 gives CH_C **28 mW/cm² peak** on a T1-C tile — ~7 mW/cm² at the 25 % firmware duty cap, a ~14× discrepancy with the old comment and 3.6× short of today's 100 mW/cm² peak. The 0.10 W/cm² figure may be inherited from the **retired** 5-zone-module design (150 × 1064 nm emitters per module) rather than from the hex tile. **Reconcile with `OI-HEXTILE-21`**, which establishes the underlying η_wp ≈ 4.8 % wall | Protocol authoring + `NP-FW-PBM1064-001`. **Do not make 1064 nm dose claims until reconciled** |
