# PBM Session Power Audit — Predefined Protocol Library Against the Concurrency Ceiling

**Project:** NeurOne
**Document:** NP-SES-PWR-001
**Revision:** 1
**Date:** 2026-08-21
**Status:** DESIGN STUDY — not a tooling or release baseline. Every figure is derived from the cited specifications and from the authored protocol files; none is measured. See §7 (Decisions) and §8 (Open Items).
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

> **⚠ READ FIRST — what this document is and is not.**
>
> `NP-HW-HEXTILE-001` §9 derives a concurrency ceiling of *"roughly six tiles… not eighty"* and raises **`OI-HEXTILE-09`**: nothing in the delivered v2 wire format prevents a protocol commanding more. That is stated as a *hypothetical gap*. This document asks whether it is an actual one, by running the arithmetic over the **authored protocol library** rather than over an imagined worst case.
>
> **It is.** Of 20 predefined protocols carrying a `pbm_transcranial` block, **2 fit the power envelope as authored**, 17 exceed it — by 1.25× to 40× — and 1 is indeterminate because its sockets are operator-selected. **Every one of them compiles clean today**: `app/web/src/lib/hubCompiler.ts` contains no power or budget check of any kind.
>
> **The second finding is larger than the first, and it is not a power finding.** The reason most protocols are over budget is that they target **lobe-scale zones where their own cited evidence specifies electrode-scale sites** — the depression protocol irradiates 37 sockets in service of a *bilateral DLPFC (F3/F4)* indication. That is a clinical-validity defect that happens to show up as a power number, and fixing it is a data edit, not a hardware change. It is filed separately (§8, `OI-SESPWR-01`) so it is not mistaken for a power-budget item.
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

**Three inherited uncertainties, stated so no figure here is quoted as settled.** Every number inherits `OI-HEXTILE-02` (no emitter is selected; the V_f and flux figures are design targets), `OI-HEXTILE-20` (whether R-5's 600 mW/cm² aggregate ceiling makes the 25.0 W two-channel peak unreachable — if it does, every `660_808nm` figure here falls ~25 %), and the **PROVISIONAL** status of the 80-socket lattice pending REG-1/ACT-1. **None of the three changes any conclusion**, because the smallest margin of failure in §2 is 1.25× and the largest is 40×.

## 2. Result 1 — the library against the envelope

| Protocol | Sockets | W/tile | Needs | Max concurrent | Over by |
|---|---|---|---|---|---|
| Vascular Baseline | 80 | 20.0 | **1,600 W** | 2 | **40×** |
| PBM — Depression (DLPFC) | 37 | 20.0 | 740 W | 2 | 19× |
| Focus Prime | 80 | 5.0 | 400 W | 8 | 10× |
| Alpha Calm | 80 | 4.7 | 375 W | 8 | 9× |
| Full T1 Immersive | 80 | 4.7 | 375 W | 8 | 9× |
| Gamma Focus | 71 | 5.0 | 355 W | 8 | 9× |
| PBM — TBI (chronic) | 71 | 3.1 | 222 W | 12 | 6× |
| Flow State | 37 | 5.0 | 185 W | 8 | 5× |
| Gamma + Theta Coupled | 37 | 4.7 | 173 W | 8 | 4× |
| Memory Boost · ADHD Focus | 37 | 4.4 | 162 W | 9 | 4× |
| Anxiety Relief | 37 | 4.1 | 150 W | 9 | 4× |
| PBM — Anxiety (PFC) | 37 | 3.8 | 139 W | 10 | 3× |
| PBM — Alzheimer's 40 Hz | 71 | 1.9 | 133 W | 21 | 3× |
| Deep Sleep | 33 | 3.8 | 124 W | 10 | 3× |
| PBM — Mild Cognitive Impairment | 37 | 1.9 | 69 W | 21 | 1.7× |
| PBM — Cognitive Enhancement 1064 | 8 | 6.3 | 50 W | 6 | 1.25× |
| **PBM — Parkinson's** | 7 | 3.1 | **22 W** | 12 | **fits** |
| **PBM — Autism (pediatric)** | 10 | 1.3 | **13 W** | 32 | **fits** |
| PBM — Stroke (chronic rehab) | operator | 10.0 | — | 4 | indeterminate |

### 2.1 The "~6 tiles" rule is wrong, in both directions

This is the finding with the widest consequences, and it validates `NP-PWR-BUDGET-001` **D-4** against real data rather than argument.

`NP-HW-HEXTILE-001` §9.2 derives ~6 concurrent tiles from **6.25 W/tile** — 400 mW/cm² at 25 % duty, both channels, 100 % intensity. **No protocol in the library runs at that operating point.** Actual per-tile draw spans **1.3 W to 20.0 W**, so the honest concurrency limit spans **2 to 32 tiles**:

| Regime | Per tile | Concurrent | Example |
|---|---|---|---|
| Low-intensity pulsed | 1.3–1.9 W | **21–32** | Autism (20 %), Alzheimer's (30 %) |
| Typical pulsed | 3.8–5.0 W | 8–10 | Alpha Calm, Gamma Focus, Focus Prime |
| §9.2's assumed point | 6.25 W | ~6 | *(nothing)* |
| **CW** | **12.5–20.0 W** | **2–3** | Vascular Baseline, Depression |

**A governor expressed as a tile count is therefore unsafe at one end and needlessly restrictive at the other** — it would permit 6 CW tiles (120 W, 2.7× R-10) while forbidding 32 low-intensity ones that fit comfortably. The check must be **watts against the negotiated PD contract**, with per-tile drive as an input. Recorded against `OI-HEXTILE-09` at `NP-HW-HEXTILE-001` Rev 7.

### 2.2 The binding constraint is CW, not the lattice

Worth separating, because the intuitive diagnosis — *too many sockets* — is right for one group and wrong for the other:

- **Pulsed protocols fail on socket count.** At 25 % duty they draw 1.3–5.0 W/tile, so 8–32 could run; they are authored against 37–80. The fix is scope (§3), not power.
- **CW protocols fail on per-tile draw.** With no duty cycle to average the draw down, one tile at 80 % CW is 20 W — **half the entire emitter budget**. Two such protocols (Vascular Baseline, Depression) account for the two worst rows in §2.

### 2.3 A separate compliance question the audit surfaced

`Vascular Baseline` is authored at **80 % intensity CW**. If 100 % intensity corresponds to the 150 mA full-drive point that `NP-HW-HEXTILE-001` §4.3.1 puts at 403 mW/cm², then 80 % CW is **~322 mW/cm² against R-4's 200 mW/cm² CW ceiling.**

**Stated as a question, not a defect.** `hubCompiler.ts` caps duty at 25 % (`dutyReg`, 0x32) and `protocolValidator.ts` enforces a global `maxIntensityPercent`, but **neither applies a mode-dependent CW clamp**, and no such clamp was found in firmware on the searches run for this document. That is not proof of absence. If the clamp exists, the audit's CW rows overstate draw by up to 2× and R-4 is satisfied; if it does not, this is an R-4 breach independent of power. **`OI-SESPWR-02` — verify before either reading is relied on.**

### 2.4 `frequency: 0Hz` with `duty_cycle:` is undefined, and it is worth 4×

Four protocols (Depression, Stroke, Cognitive-1064, MCI) set `frequency: 0Hz` **and** `duty_cycle: 25%`. `NP-NPPS-REF-001` §4.1 states that `frequency: 0` selects CW, and CW means 100 % duty — the two fields contradict each other.

The compiler does not resolve it: `freqCode(0)` emits `0x00` (CW) and `dutyReg(25)` emits `0x32` **independently**, leaving the hub to decide. So the power draw of a fifth of the library depends on an unwritten semantic:

| Reading | Depression draws |
|---|---|
| CW wins, duty ignored | **740 W** |
| Duty applied to CW | 185 W |

§2 reports the CW reading, the higher of the two. **This is not merely a documentation gap** — it is a 4× uncertainty in the input to any power governor, and it must be resolved *before* the governor is written, not after. **`OI-SESPWR-03`.**

## 3. Result 2 — the zones are lobe-scale; the evidence is electrode-scale

**This is the largest finding in the document and it is not about power.**

The zone vocabulary in `protocols/predefined/00-zones.npps` offers 14 zones, of which the smallest are 5 sockets:

| Zone | Sockets | | Zone | Sockets |
|---|---|---|---|---|
| Temporal L/R · Occipital L/R | 5 | | Frontal L/R | 20 |
| Motor / SMA | 7 | | Posterior | 33 |
| Frontal Right (excl. midline) | 8 | | Frontal | 37 |
| Parietal L/R | 13 | | Vault (excl. Occipital) · All | 71 · 80 |

**There is no DLPFC zone, no F3/F4 zone, and no zone corresponding to any single 10-20 site.** The finest frontal targeting available is a 20-socket half-lobe.

Against that, `docs/pbm_neuro_protocols.md` MASTER SUMMARY specifies sites at 10-20 resolution — *"Bilateral DLPFC (F3/F4)"* for depression, *"Right (or bilateral) PFC"* for cognitive enhancement, *"Prefrontal (F3/F4)"* for MCI. `clinical-04-pbm-depression.npps` is named **"PBM — Depression (DLPFC)"** and targets `["Frontal Left", "Frontal Right"]` — **37 sockets, ~9× the area its own title names.**

**Three consequences, in increasing order of seriousness:**

1. **Power.** 37 sockets at 20 W is 740 W. Four sockets at a compliant CW intensity is ~50 W. **The protocol is over budget mainly because it is over-scoped**, and correcting the scope is a data edit to one `.npps` file — no firmware, no hardware, no revision to any specification.
2. **Dose distribution.** Irradiating the whole frontal lobe to deliver a DLPFC dose does not merely waste energy; it delivers the protocol's dose to tissue the evidence never studied, and — under a fixed power budget — **less** dose to the target than a focused montage would.
3. **Claim integrity.** A protocol titled *DLPFC* that irradiates the frontal lobe cannot support a DLPFC claim. `clinical-09-pbm-stroke-rehab.npps` already recognises exactly this hazard in its own comments — *"Whole-head irradiation is NOT the evidence target and would silently substitute the wrong dose distribution"* — and resolves it with `zones: clinician_selected`. **That reasoning was applied to one protocol and not to the rest.**

**Is finer targeting even meaningful?** Yes, but with a floor. `NP-OPT-PSF-001` §3.2 gives one 40 mm tile a **40.0 mm FWHM at cortex**, so a tile is approximately the resolution unit — targeting *below* one tile buys nothing, and targeting a lobe when the evidence says one electrode wastes ~9 of them. A DLPFC zone of 2 sockets per hemisphere is both physically meaningful and evidence-faithful. **`OI-SESPWR-01`** — and it needs REG-1 to fix socket-to-10-20 registration before the membership can be authored with confidence, which is the honest reason it is not done in this document.

## 4. Result 3 — what cascading can and cannot rescue

### 4.1 The arithmetic

Cascading — rotating through socket groups over time — **preserves total delivered energy and divides per-site time by the number of groups.** To hold dose per site, session length multiplies by the group count:

| Protocol | Groups | Authored | Cascaded to hold dose |
|---|---|---|---|
| PBM — Autism · Parkinson's | 1 | 6m · 20m | **unchanged — already fit** |
| PBM — MCI | 2 | 20m | 40m |
| PBM — Cognitive 1064 | 2 | 8m | 16m |
| PBM — Alzheimer's 40 Hz · Anxiety | 4 | 20m | 80m |
| Gamma + Theta · Anxiety Relief | 5 | 20m | 1.7h |
| Gamma Focus | 9 | 20m | 3.0h |
| Alpha Calm · Focus Prime | 10 | 20m | 3.3h |
| Full T1 Immersive | 10 | 30m | 5.0h |
| PBM — Depression | 19 | 20m | 6.3h |
| Vascular Baseline | 40 | 30m | **20.0h** |

**There is no free lunch in the energy domain.** `NP-PWR-BUDGET-001` §3.7 is the general statement: total optical output is capped by the PD envelope regardless of how many tiles exist, so cascading redistributes joules in time without creating any.

### 4.2 What survives cascading, and why

**Dose-driven, non-rhythmic protocols survive** — Vascular Baseline, Cognitive 1064, Depression, MCI. Photochemical response (CCO absorption, NO release) integrates over minutes, so if the rotation period is short relative to that integration time the tissue cannot distinguish time-multiplexed delivery from continuous delivery at the duty-averaged irradiance.

**Fast cascading is the same thing as `NP-PWR-BUDGET-001` §3.6's whole-vault mode**, expressed in the time domain rather than the current domain. Rotating 10 groups at 1 Hz and lighting all 80 tiles at 10 % drive deliver the same average irradiance to the same tissue. **This is worth stating explicitly because the two proposals look unrelated and are one mechanism.** It also inherits §3.6's honest limit: at whole-vault average irradiance the dose falls below the efficacy threshold unless the session is extended, which is §4.1's table again.

### 4.3 What cascading breaks — and it is the Grade A protocols

**Every 40 Hz entrainment protocol is invalidated by cascading**, not merely degraded: Alzheimer's (Grade A), Gamma Focus, Gamma + Theta Coupled, Autism (Grade B), Full T1 Immersive.

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
| 1 | **Author evidence-faithful zones** (`OI-SESPWR-01`) | Data edit to `00-zones.npps`; gated on REG-1 | Removes most of the over-budget condition **and** corrects a claim-integrity defect. Depression 740 W → ~50 W |
| 2 | **Resolve `frequency: 0Hz` + `duty_cycle`** (`OI-SESPWR-03`) | Spec sentence + compiler check | Removes a 4× uncertainty on a fifth of the library |
| 3 | **Verify the CW intensity ceiling** (`OI-SESPWR-02`) | Firmware read | R-4 compliance, independent of power |
| 4 | **Build the governor in watts** (`OI-HEXTILE-09`) | Compiler + session runner | Makes the remaining condition *detectable* rather than silent |
| 5 | **Then cascade the residue** (`OI-SESPWR-04`) | New NPPS primitive + firmware | Only for protocols that declare time-multiplexing admissible — which excludes every entrainment protocol |

**Steps 1–3 are prerequisites for 4, not parallel to it.** A governor built against the current library would reject 17 of 20 protocols, and the correct response to 15 of those rejections is to fix the protocol, not to cascade it.

## 6. Cross-references

`NP-HW-HEXTILE-001` §9 (the ceiling this document tests), §9.3 (`OI-HEXTILE-09`, and consequence 4 added at Rev 7), §4.2/§4.3 (the per-tile model), `OI-HEXTILE-20` (whether 25.0 W is reachable), `OI-HEXTILE-21` (the 1064 nm irradiance wall, which bounds `clinical-03`) · `NP-PWR-BUDGET-001` §3.4 (efficacy floor), §3.6 (whole-vault mode — §4.2 here shows it is the same mechanism as fast cascading), §3.7 (why cascading creates no energy), D-4 (governor in watts, which §2.1 confirms against data) · `NP-NPPS-REF-001` §4.1, §5, §6 · `NP-OPT-PSF-001` §3.2 (40 mm FWHM — the targeting floor for §3) · `docs/pbm_neuro_protocols.md` (site and dose specifications) · `protocols/predefined/00-zones.npps` (ZONE-1) · `app/web/src/lib/hubCompiler.ts` (`freqCode`, `dutyReg`, and the absent budget check) · `scripts/check-pbm-power.ts` (the audit)

## 7. Decisions

Recorded so they can be challenged individually. None is locked; all are proposals for design review.

| ID | Decision | Rationale | Reversible? |
|---|---|---|---|
| **D-1** | **The audit is a committed script, not a table in a document.** `scripts/check-pbm-power.ts` re-derives every figure here on demand | A hand-copied table goes stale the first time a protocol's `intensity` is edited, and nothing would catch it. `NP-CONV-001` §8's principle — *a convention worth writing down is worth a script* — applied to an analysis | Yes |
| **D-2** | **`--strict` is NOT wired into CI in this revision.** The script reports and exits 0 by default | 17 of 20 protocols fail today, so a gate would fail from the first commit and be disabled or bypassed — worse than no gate. Enable it once `OI-SESPWR-01..03` land (`OI-SESPWR-05`) | Yes — that is the plan, not a permanent exemption |
| **D-3** | **Report the CW reading (the higher draw) where §2.4's ambiguity applies** | The conservative direction while the semantic is undefined. Reverses to the duty reading if `OI-SESPWR-03` resolves that way | Yes |
| **D-4** | **File the zone-granularity finding as a clinical-validity item, not a power item** | It presents as a power number but is a claim-integrity defect (§3), and it would still need fixing if the power envelope were unlimited. Filing it under the power budget would let it close for the wrong reason — a bigger supply | Yes, but the classification is the point |

## 8. Open Items

| ID | Description | Owner / Blocking |
|---|---|---|
| **OI-SESPWR-01** | **Author evidence-faithful zones at 10-20 resolution.** No DLPFC, F3/F4 or single-site zone exists; the finest frontal targeting is a 20-socket half-lobe, and `clinical-04` irradiates 37 sockets for a *bilateral DLPFC* indication (§3). One 40 mm tile is ~40.0 mm FWHM at cortex (`NP-OPT-PSF-001` §3.2), so ~2 sockets/hemisphere is both meaningful and faithful. **Blocked on REG-1** — socket-to-10-20 registration must be fixed before membership can be authored with confidence; that is the honest reason this is not simply done. **Interim:** follow `clinical-09`'s precedent and mark affected protocols `clinician_selected` rather than silently substituting a lobe | Clinical + Protocol authoring. **Gated on REG-1.** Largest single reduction in the §2 condition |
| **OI-SESPWR-02** | **Verify whether a mode-dependent CW intensity clamp exists in firmware.** `Vascular Baseline` at 80 % CW implies ~322 mW/cm² against R-4's 200 mW/cm² CW ceiling (§2.3). `dutyReg` caps duty and `protocolValidator` enforces a global intensity limit; neither is mode-dependent, and no clamp was found on the searches run here — **which is not proof of absence.** If absent, this is an R-4 breach independent of power | Firmware + Safety. **Potentially a compliance defect, not a budget one** |
| **OI-SESPWR-03** | **Define `frequency: 0Hz` combined with `duty_cycle:` — or reject it.** Four protocols set both; `NP-NPPS-REF-001` §4.1 says `frequency: 0` selects CW and CW is 100 % duty. `hubCompiler.ts` emits `freqCode` and `dutyReg` independently. **A 4× swing in the power budget of a fifth of the library rests on the answer** (§2.4). Preferred resolution: make it a parser error, so the author states which they mean | NPPS spec + compiler. **Blocks `OI-HEXTILE-09`** — a governor cannot be written against an undefined input |
| **OI-SESPWR-04** | **A cascade primitive needs a per-protocol admissibility declaration, defaulting to *not admissible*.** Cascading preserves dose but destroys the network-wide rhythmic drive that every 40 Hz entrainment protocol claims (§4.3), including the Grade A Alzheimer's protocol; it also breaks closed-loop adaptation and the cross-modal 40 Hz phase-lock in `15-full-t1-immersive`. **A zone-rotation primitive without this field is a mechanism for silently invalidating the strongest protocols in the library.** Zone rotation inside `pbm_transcranial` does not exist today; `composite` + `sequential` can emulate it at N× the authoring cost (§4.4) | NPPS spec + session runner. **Sequence after `OI-SESPWR-01`** — most of the residue disappears once scope is corrected |
| **OI-SESPWR-05** | **Wire `--strict` into CI once `OI-SESPWR-01..03` land**, and decide what the gate does about protocols that are legitimately operator-scoped (`clinician_selected`), which the audit can only mark indeterminate | Systems + CI. Follows D-2 |
| **OI-SESPWR-06** | **`clinical-03-pbm-cognitive-1064.npps` states an irradiance its hardware does not appear to produce.** Its comment claims *"~0.10 W/cm² average"*, but `NP-HW-HEXTILE-001` §4.3.2 gives CH_C **28 mW/cm² peak** on a T1-C tile — ~7 mW/cm² at the 25 % firmware duty cap, a ~14× discrepancy. The 0.10 W/cm² figure may be inherited from the **retired** 5-zone-module design (150 × 1064 nm emitters per module) rather than from the hex tile. **Reconcile with `OI-HEXTILE-21`**, which establishes the underlying η_wp ≈ 4.8 % wall | Protocol authoring + `NP-FW-PBM1064-001`. **Do not make 1064 nm dose claims until reconciled** |
