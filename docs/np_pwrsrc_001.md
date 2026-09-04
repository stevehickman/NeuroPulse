# Power-Source Architecture — Multi-Source Supply, Purchase-Time Selection, and the Deliberate-Heat Proposition (Design Study)

**Project:** NeurOne
**Document:** NP-PWRSRC-001
**Revision:** 1
**Date:** 2026-08-27
**Status:** DESIGN STUDY — not a tooling, firmware or release baseline. Every numeric value is derived from the cited specifications, from the authored protocol library, or from the two committed scripts named in §1; none is measured. **Modifies no locked section.** CLAUDE.md §2 (charger policy) and §4 (power table) are read, quoted and analysed but not edited — §12 and §13 say what they would have to become and route the change to the principal.
**Effective Date:** —
**Author:** NeurOne Systems Engineering
**Approved By:** — (pending design review)
**References:** CLAUDE.md §1 (founding principles — wired-first USB-C, Mode 3 autonomy), §2 (configurations, retail unlocked, charger policy LOCKED), §3 (modality stack), §4 (safety architecture, EMF shielding, power table), §5 (UHDR/SHDR); NP-PWR-BUDGET-001 Rev 3 (§3.2 aggregate thermal estimate, §3.3 export efficiency, §4.4 second-inlet assessment, §4.4.1 isolation, §4.4.4 what it does not do, D-4, OI-PWR-01…13); NP-SES-PWR-001 Rev 1 (§1 per-tile model, §2.1 the tile-count governor, §2.3 CW ceiling, §2.4 the CW+duty ambiguity, D-1, OI-SESPWR-01…06); NP-THERM-CFD-R1-001 Rev 1 (§2 Path-A rejection, §3 the inward-flux ceiling and R_face→core, §5 BN-boss export study, §5.3 findings, OI-R1-01…05); NP-THERM-CFD-001 (heat-source model, η_wp); NP-ENV-OPRANGE-001 (ambient/duty envelope); NP-ENV-001 (survival vs operating envelopes); NP-DT-001 Rev 2 (DI-REG-01 IEC 60601-1, DI-SAFE-08, DI-SAFE-13, VE-11); NP-HW-HEXTILE-001 Rev 8 (§4.3 V_f design targets, §9.1 overhead, §9.2 concurrency, R-4/R-5/R-6, OI-HEXTILE-02/06/09/20/21); NP-HW-HUB-001 Rev 5; NP-TOOL-HUB-001 Rev 1 (§3 F-02, HUB-MDR-04/05, FAI-HTOOL-02); NP-COST-001 Rev 2 (§2 A-1/A-2/A-3, §6, §8, OI-COST-01/05/10); NP-FW-BENCH-001 Rev 1 (head-presence gate); NP-FW-EMMC-002 Rev 2 (§G.2, §H — record-denominated windows, no wall clock); NP-RISK-003 / NP-RISK-004 (risk-row shape); NP-CONV-001 Rev 6 (§4 document identifiers, §8 a convention worth writing down is worth a script); `docs/reference/regulatory-strategy.md` (T1 wellness pathway); `scripts/check-pbm-power.ts`; `scripts/check-power-source-coverage.ts`
**Related Issues:** —
**Gate:** No gate. Routes four items to principal decision (§17 D-1, D-6, D-9, D-12) and raises `OI-PWRSRC-01…16`, of which four are marked BLOCKING.
**IEC 62304 Class:** — (analysis document; no code changed). §15 argues the classification of the software this study would create if adopted.
**Supersedes:** — (new document). Does **not** supersede `NP-PWR-BUDGET-001` §4.4; it tests it, confirms its conclusion by a different route, and corrects one row of its reasoning (§8).
**Parent Document:** NP-PWR-BUDGET-001

---

> **⚠ READ FIRST — three answers, before the analysis that produced them.**
>
> **1. The wall-plug path is premature for T1, and the margin is far wider than `NP-PWR-BUDGET-001` §4.4.4 states.** §4.4.4 argues it in tiles — the thermal ceiling (4–8) and the power ceiling (~6) are *"coincident"*, so relieving power moves the binding constraint to heat. Restated in the unit **D-4** says the argument must use, it stops being a limit and becomes a wall: **the sealed cavity supports 27.6–49.1 W of aggregate emitter power, and the existing 45 W brick already delivers 40 W of it** (§4.1). PD 240 W EPR would deliver 232 W — **4.7× to 8.4× the heat the assembly can reject**; an 1,800 W mains station, **36× to 65×**. No inlet, connector, brick or base station changes that, because the residual terminates in a cavity that cannot be ventilated without breaching the EMF shield. **§7.0 is the consequence: thermally achievable coverage is 2/23 under *every* candidate source — identical to today. Do not build a mains path for T1 PBM.**
>
> **2. The thermal-dose audit inverts the hazard, and it is the most valuable result here.** Nothing in the design bounds cumulative dose — every thermal control is a real-time reactive interlock, and a repo-wide search finds no CEM43 treatment on `main`. `scripts/check-thermal-dose.ts` supplies it. **The protocols a T1 device can run today are nowhere near harm: 3.5 × 10⁻⁶ CEM43 over a 108-session course** (§5.3), and back-to-back sessions are also a clean null (§5.4 — **which corrects an expectation held going in: repetition was flagged as a hidden hazard and the arithmetic says that was overstated; ambient matters ~8 orders of magnitude more**). **The exposure is created by cascading — `NP-SES-PWR-001` §4's own sanctioned remedy for insufficient power** — which holds the cavity at the interlock ceiling for hours: Vascular Baseline, 40 groups, 20.0 h, **292 CEM43**, with **13 of 20 cascaded protocols past the conservative reference line** (§5.5). *The accepted fix for insufficient power is what generates the thermal-injury exposure*, and the interlock cannot see it because it caps temperature and has no duration input.
>
> **3. The T1/T2 power gate is a sound mechanism resting on a premise §4 has invalidated.** It was decided against a ladder in which more watts unlocked 16, then 21, then 22 of 23 protocols. After the cavity ceiling **every rung unlocks the same two**, and the **thirteen** risky protocols are already reachable on a stock Home Standard with the brick in the box — they are risky by *duration at the ceiling*, not by draw. **Net protocols unlocked by a T2 mains accessory: zero. Price: NeurOne owning an IEC 60601-1 isolation barrier at the 510(k) tier with `VE-11` open, plus a $210–380 accessory whose failure ends a clinic session.** The direction's real objective — supervised-only protocols — is met in full by a **device-bound signed entitlement at $0.00 BOM** (§6.7), which can also express the duration and dose axes a supply cannot see. **`OI-PWRSRC-22`, principal.**
>
> **What this study recommends.** Keep the T1 source set as it ships (§7); put any mains hardware in a **separate attached base station** presenting as a PD source, never in the head-worn assembly or the occipital-arch hub (§8); arbitrate in **hardware**, giving the safety MCU three small Class C elements — a latched `PWR_ADEQUATE`, a time-at-limit counter, an entitlement check (§16); denominate the governor in **`min(electrical, thermal, dose)`** over the summed contracts of sources presently supplying (§11); split the failure classes into **F1/F2/F3-a/F3-b/F4** with two prohibitions, so no buyer is sold a source that changes nothing and no F4 is presented as purchasable (§12); and keep **information for safety on top of the interlocks, never instead of them** (§6.12). **Total T1 BOM delta: $0.00** (§17).
>
> **What it does not do.** It does not set a price, edit any locked section, resolve `OI-PWR-11`, or size TMS. It records where each of those blocks.

---

## 1. Method, and the three scripts

Per `NP-SES-PWR-001` **D-1** — *the audit is a committed script, not a table in a document* — every
demand and coverage figure below is produced by code committed with this study, not transcribed:

```bash
bun scripts/check-pbm-power.ts               # per-protocol demand (pre-existing)
bun scripts/check-power-source-coverage.ts   # source coverage (new, this study)
bun scripts/check-thermal-dose.ts            # cumulative CEM43 (new, this study)
```

The second and third import `analyse()` from the first. That is deliberate and is the whole reason the
refactor was made: a source-coverage table and the protocol audit **cannot** diverge, because there
is one demand model and both read it. `check-pbm-power.ts`'s own output is byte-identical to before.

**Three uncertainties every figure inherits**, restated so nothing here is quoted as settled:

| Inherited from | What it means |
|---|---|
| `OI-HEXTILE-02` | **No 660/808 nm emitter is selected.** Per-tile watts come from `NP-HW-HEXTILE-001` §4.3's V_f *design targets*, not datasheet values. |
| `OI-HEXTILE-20` | If R-5's 600 mW/cm² aggregate ceiling makes the 25.0 W two-channel peak unreachable, every `660_808nm` figure falls ~25 %. |
| `OI-SESPWR-03` | `frequency: 0Hz` with `duty_cycle:` is undefined and worth **4×** on six of 23 protocols. §3.3 states exactly which conclusions survive it and which do not. |

**A fourth, specific to §4 and §5:** the aggregate thermal model is `NP-PWR-BUDGET-001` §3.2's
first-order lumped estimate off a single-cell CFD that was built with adiabatic side walls and was
never intended to answer the aggregate question. `OI-PWR-01`'s verification-grade multi-tile CFD has
not been run, and `OI-PWR-08` bounds the model's validity to roughly N ≤ 8 tiles. §4.1 states where
that bound bites and why the conclusion is nonetheless robust to it.

---

## 2. Premise corrections — the commissioning figures, checked

The brief supplied a demand distribution and a coverage table and instructed that they be re-derived
rather than trusted. They were. **Four of the supplied figures are wrong**, and one of the four
changes a structural conclusion.

### 2.1 The demand distribution

| | Figures |
|---|---|
| **As supplied** | `13 22 40 68 82 98 124 133 143 150 162 163 173 178 185 200 330 355 375 375 400 \| 1600 1775` |
| **As reported on `main` at `d817364`, this study's base** | `13 22 68 82 98 124 133 143 150 162 163 173 178 185 200 200 330 355 375 375 400 1600` |
| **As reported on `main` at `6eda053`, after PR #296 merged** | `13 22 68 82 98 124 133 143 150 162 163 173 178 185 200 330 355 375 375 400 1600 1775` |

Three differences:

1. **There is no protocol at 40 W.** The supplied list carries one; the library does not.
2. **There are two protocols at 200 W, not one** — `Memory Boost` and `PBM — Cognitive Enhancement
   1064nm`, both 8 sockets at 25.0 W/tile.
3. **The maximum is 1,775 W** — `PBM — Alzheimer's 1064nm (deep-cortical)`, 71 sockets ×
   25.0 W/tile. Second is `Vascular Baseline` at 1,600 W, 80 sockets × 20.0 W/tile.

> **CORRECTION, 2026-08-28 — this study's own premise correction did not survive its run, and the
> error is instructive.** As first written, this section reported the maximum as **1,600 W** and
> explained 1,775 W away as *"a cross-platform compiler defect masquerading as a power
> requirement"* whose protocol was *"not on `main`"*. Both limbs are wrong at HEAD:
>
> 1. **It is on `main`.** `clinical-03-pbm-alzheimers-1064.npps` merged as **PR #296** (`f8806dd`)
>    while this study was running. The claim was true of the study's base commit (`d817364`) and
>    was falsified by a merge, not by an error of reading.
> 2. **The number is not a compiler artifact.** `OI-PBMCH-04` is real, but it concerns `wavelength`
>    failing to reach the wire on **iOS and Windows**. The 25.0 W/tile figure comes from the **web**
>    path, which does carry `wavelength` — and that path reported **25.0 W/tile for the other 1064 nm
>    protocol on `main` before #296 merged** (`PBM — Cognitive Enhancement 1064nm`, 8 × 25.0 =
>    200 W). 25.0 W/tile is simply the 1064 nm per-tile rate. The whole difference between 200 W and
>    1,775 W is **zone size** — 8 sockets versus 71 — which was a deliberate authoring decision the
>    principal explicitly declined to narrow, on the grounds that the figure is a requirement to be
>    measured, not a defect to be hidden.
>
> **The lesson is the inverse of the one this programme keeps learning.** The standing rule is that
> *"does not exist"* is not a checkable claim, only *"not on `main`"* is. This section obeyed that
> rule and was still wrong, because **a claim about `main` has a shelf life when work is in flight**.
> A statement about `main` needs its commit named, and needs re-checking before the document lands.
> Recorded as **`OI-PWRSRC-24`**.

**What this changes, and what it does not.** The principal's framing — *the top figure is a
requirement, not a defect* — stands, and stands more strongly than the original text allowed: there
is no bug to discount, and 1,775 W is a real demand a real protocol expresses.

**§4's conclusion is untouched.** The sealed-cavity ceiling is 27.6–49.1 W. Whether the library's
maximum is 1,600 W or 1,775 W is immaterial to it — both are **33–64×** the ceiling, and the
recommendation against a mains path does not turn on which.

### 2.2 The coverage table

| Budget | To emitters | **As supplied** | **As derived** |
|---|---:|---:|---:|
| R-10 today (45–50 W) | 40 W | 3 / 23 | **2 / 23** |
| USB-C PD 100 W | 92 W | 5 / 23 | **4 / 23** |
| USB-C PD 240 W EPR | 232 W | 16 / 23 | 16 / 23 ✓ |
| mains 500 W | 492 W | 21 / 23 | 21 / 23 ✓ |
| mains 1,800 W | 1,792 W | 23 / 23 | **22 / 23** |

The first two are off by one, and both errors trace to the phantom 40 W entry. The bottom row is the
one that matters.

> **No power source can reach 23/23, at any wattage, ever.** `PBM — Stroke (chronic rehab)` sets
> `zones: clinician_selected`, so its socket count — and therefore its demand — is not knowable until
> an operator selects. The audit reports it *indeterminate*, not *over budget*. A table whose bottom
> row reads 23/23 implies that enough watts buys complete coverage. **They do not, and the reason is
> not electrical.** `clinical-09` is operator-scoped deliberately, as the one protocol in the library
> that already resolves `NP-SES-PWR-001` §3's claim-integrity hazard correctly.

**Consequence for the purchaser-facing work in §11:** any UI that promises "this source runs N of 23
protocols" must be able to render *indeterminate* as a third state. A binary runnable/not-runnable
display would have to lie about `clinical-09` under every source, including an unlimited one.

### 2.3 Two live figures the brief's coverage table conceals

Re-deriving surfaced two rows the supplied table does not contain, and both are load-bearing:

| Step | Coverage | Marginal protocols bought |
|---|---:|---|
| R-10 (40 W) → **PD 65 W** (57 W) | 2 → **2** | **none** |
| **2 × PD 240 W EPR** (472 W) | — | **21 / 23 — identical to a 500 W mains station** |

- **A 65 W brick buys zero protocols.** §2 (charger policy) sells a 65 W upgrade at checkout as an
  intent signal; §13 records what that becomes when the buyer is told what a source unlocks.
- **A 500 W mains base station buys nothing over two USB-C PD 240 W EPR sinks.** Both reach 21/23.
  The entire incremental case for mains over dual-EPR-USB-C rests on **one protocol** —
  `Vascular Baseline` at 1,600 W — and that protocol is the same one `NP-SES-PWR-001` §2.3 flags as a
  possible **R-4 CW breach** (80 % intensity CW implies ~322 mW/cm² against a 200 mW/cm² CW ceiling,
  `OI-SESPWR-02`, unresolved). **The single protocol that justifies a wall plug may not be compliant
  as authored.** §6 does not resolve `OI-SESPWR-02` and does not size against `Vascular Baseline`
  until it is resolved.

### 2.4 Two corrections that are not about numbers

**(a) `NP-PWR-BUDGET-001` and `NP-SES-PWR-001` are not registered in the DHF.** `NP-DHF-001` §5
states: *"When a new controlled document is created, this index must be updated before the new
document is released."* Neither power study appears anywhere in `np_dhf_001.md` — zero occurrences of
either serial. This study registers itself (§5.3, the hardware-specification index, not §5.4, which
is Firmware Specifications and holds no analysis document). The two omissions are raised as
`OI-PWRSRC-15` rather than backfilled here, because registering another author's document is an
owner-signed QMS action, not an editorial one.

**(b) The DHF revision header has drifted behind its own revision history, again.** The header and
the §5.1 self-row read **Rev 39**; the §11 revision history carries rows **40** and **41** (PR #292,
PR #297). This is the third recurrence of the bookkeeping fault that Rev 26, Rev 36 and Rev 39 each
repaired. This study adds row **42** and sets header and self-row to 42, repairing the drift and
saying so in the row, per the precedent Rev 39 set.

---

## 3. The demand the sources must meet

### 3.1 The distribution is a cliff, not a slope

```
13  22  68  82  98  124  133  143  150  162  163  173  178  185  200  330  355  375  375  400   1600  1775
                                                                                                    └── 4.0× ──┘
```

**The largest single step in the library is 400 W → 1,600 W, a factor of four, and nothing asks for
anything in between** (1,600 → 1,775 W is a further 11 %, not a rung). That is the single most useful fact for a sizing decision, because it means the
choice is not a continuum. There are exactly four rungs worth considering:

| Rung | To emitters | Coverage | What it is |
|---|---:|---:|---|
| ~40 W | 40 | 2/23 | today |
| ~92–132 W | 92–132 | 4–6/23 | PD 100 W / PD 140 W EPR |
| **~232 W** | 232 | **16/23** | **PD 240 W EPR — one connector, one contract, Mode 3 intact** |
| ~470–500 W | 472–492 | 21/23 | dual EPR **or** a 500 W mains station — indistinguishable |
| ~1,800 W+ | 1,792 | 22/23 | mains only, for the top two protocols |

Everything between 240 W and 472 W buys nothing. Everything between 500 W and 1,600 W buys nothing.
**A source ladder with rungs anywhere else is a ladder of dead rungs**, and §13 records that §2's
existing 15 / 30 / 45 / 65 W ladder has exactly one live rung in protocol terms.

### 3.2 The single biggest step is free, and it is not a power decision

**PD 240 W EPR takes coverage from 2/23 to 16/23 — a 70 % library, from one USB-C connector, on a
commodity brick, with Mode 3 fully intact.** That is 14 of the 21 protocols any source can ever
unlock, bought by a contract renegotiation and a connector already in the design.

The remaining five (330–400 W) and `Vascular Baseline` (1,600 W) are exactly the protocols
`NP-SES-PWR-001` §3 identifies as **over-scoped rather than over-budget** — lobe-scale zones (71 and
80 sockets) authored against electrode-scale evidence. `OI-SESPWR-01`, a data edit to
`00-zones.npps`, would move most of them below 232 W without any hardware at all.

> **The order of operations follows directly. Fix scope (`OI-SESPWR-01`), then contract (PD 240 EPR),
> and only then ask whether any residue justifies mains.** Buying watts first pays hardware money for
> a protocol-authoring defect, and — per §4 — buys watts the assembly cannot dissipate.

### 3.3 What `OI-SESPWR-03` decides, and what it does not

The 4× ambiguity affects six of 23 protocols. Its effect on coverage is **entirely confined to the
bottom of the ladder**:

| Source | To emitters | CW reading | Duty reading | Does the ambiguity change the answer? |
|---|---:|---:|---:|---|
| R-10 (45 W) | 40 W | 2/23 | **5/23** | **Yes — 2.5×** |
| PD 65 W | 57 W | 2/23 | **7/23** | **Yes** |
| PD 100 W | 92 W | 4/23 | **7/23** | **Yes** |
| PD 140 W EPR | 132 W | 6/23 | **8/23** | Yes |
| PD 240 W EPR | 232 W | 16/23 | 16/23 | **No** |
| 2 × PD 240 EPR / mains 500 W | 472–492 W | 21/23 | 21/23 | **No** |
| mains 1,800 W | 1,792 W | 22/23 | 22/23 | **No** |

> **This is the cleanest possible answer to the brief's caveat, so it is stated as a rule.**
>
> **Decidable without `OI-SESPWR-03`:** every question this study is commissioned to answer — the
> source set (§6), where mains hardware lives (§7), isolation (§8), arbitration (§9), the governor's
> unit (§10), the F3 split (§11), and the whole of §4's thermal finding. At and above the 232 W rung
> the two readings are numerically identical.
>
> **NOT decidable without it:** anything keyed to the *current* envelope — whether R-10's 40 W covers
> 2 protocols or 5, and therefore any claim about how much of the library ships working today. §12's
> proposed replacement for the CLAUDE.md §4 power table is written so that it does not depend on the
> answer, and §11's purchaser-facing display is specified to render the ambiguity rather than pick a
> side.
>
> **No hardware in this study is sized against an ambiguous figure.** The recommended T1 source
> (§6 S-2) is chosen at the 232 W rung precisely because that is the first rung where the ambiguity
> stops mattering.

---

## 4. The binding constraint is heat — and in watts it is not close

This section restates `NP-PWR-BUDGET-001` §3.2 and §4.4.4 in watts. Nothing new is modelled; the
same lumped estimate is inverted. The result is the central finding of the study.

### 4.1 The sealed-cavity ceiling, in the unit D-4 requires

`NP-PWR-BUDGET-001` §3.2 fixes four quantities:

| Quantity | Value | Source |
|---|---|---|
| Single-tile face margin at nominal 25 °C ambient | **11.3 °C** (face 30.7 °C vs 42 °C) | NP-THERM-CFD-R1-001 §5.1 |
| Residual fraction into the sealed cavity | **10 %** (BN-boss exports ~90 %) | NP-THERM-CFD-R1-001 §5 |
| Effective vault area for cavity→ambient rejection | **0.1 m²** | NP-HELMET-GEOM-001 |
| Cavity→ambient resistance | **0.23–0.41 m²K/W** | NP-THERM-CFD-R1-001 §2 |

§3.2 combines them into a *tile count*: ~4 tiles at R = 0.41, ~8 at R = 0.23. Combine them into
**watts of total emitter power** instead:

> ΔT = (0.10 × P_total ÷ 0.1 m²) × R = P_total × R
>
> P_total,max = ΔT_margin ÷ R

| Cavity resistance | **Aggregate emitter-power ceiling** |
|---|---:|
| 0.41 m²K/W (conservative, full outward path) | **27.6 W** |
| 0.23 m²K/W (optimistic, air-gap only) | **49.1 W** |

**Three things follow, and the third is the finding.**

1. **The per-tile figure cancels.** 6.25 W/tile appears nowhere in the watt form. In *tiles*,
   §3.2's estimate and `NP-HW-HEXTILE-001` §9.2's ceiling are **not** independent — both divide the
   same envelope by the same 6.25 W/tile, so their agreement is partly an artifact of a shared input.
   In *watts* they are genuinely independent: a thermal model says 27.6–49.1 W, an electrical
   envelope says 40 W, and 40 sits inside the range. **`NP-PWR-BUDGET-001` §3.2's claim that its
   estimate "independently" brackets §9's figure is true only in the unit it did not use.**
   This is a correction of reasoning, not of the conclusion — which strengthens.

2. **The R-10 envelope is already at the ceiling.** 40 W to emitters against a 27.6–49.1 W thermal
   ceiling. The existing 45 W brick is not leaving headroom on the table; it is sitting on the limit,
   possibly 1.4× over it at the conservative resistance.

3. **Every source above ~57 W total is buying heat the assembly cannot reject.**

| Source | To emitters | **Multiple of the thermal ceiling (27.6 / 49.1 W)** |
|---|---:|---:|
| R-10 today | 40 W | 1.4× / 0.8× |
| PD 100 W | 92 W | 3.3× / 1.9× |
| PD 240 W EPR | 232 W | **8.4× / 4.7×** |
| Mains 500 W | 492 W | 17.8× / 10.0× |
| Mains 1,800 W | 1,792 W | **64.9× / 36.5×** |

### 4.2 The inverse form, and the reductio

Run it backwards: what scalp-contact face temperature would be needed to *spend* each source?
Face ≈ 30.7 °C + P_total × R.

| Source | To emitters | Implied face temperature |
|---|---:|---|
| PD 240 W EPR | 232 W | **84 – 126 °C** |
| Mains 500 W | 492 W | 144 – 233 °C |
| Mains 1,800 W | 1,792 W | 443 – 766 °C |

> **These are not temperatures and must never be quoted as temperatures.** `OI-PWR-08` bounds the
> lumped model to roughly N ≤ 8 tiles — about 50 W — and every row above is far outside it. They are
> stated for **sign and order of magnitude only**, exactly as `OI-PWR-08` permits §3.5's figure to be
> used. The direction of the extrapolation is monotone and safe for the conclusion being drawn:
> outside the model's validity the real answer is *worse*, never better, because lateral spreading is
> already assumed and radiative loss is negligible at these fluxes.

The conclusion needs no precision at all. **Water boils at 100 °C.** A 240 W EPR contract is not
"more headroom"; it is a request for a scalp interface in the boiling-water band.

### 4.3 What this does to `NP-PWR-BUDGET-001` §4.4.4 — it confirms it, and widens it

§4.4.4 says a second inlet *"does not raise T1 PBM concurrency"* because the two ceilings are
*"coincident"*, and that this changes *"if `OI-PWR-01`'s multi-tile CFD returns a materially higher
thermal ceiling."*

**The coincidence framing understates the case, and the escape clause is narrower than it reads.**

- Not coincident — **stacked**. 40 W against 27.6–49.1 W is not a tie between two comparable numbers;
  it is one number sitting on top of the other. The next rung anyone would buy (232 W) is 4.7–8.4×
  away. `OI-PWR-01` would have to return a ceiling **five to eight times higher** to make even one
  step of the ladder thermally spendable, and thirty-six to sixty-five times higher to justify mains.
- **A CFD does not produce watts of cooling; it produces a better estimate of a resistance.** The
  physical inputs — 10 % residual, a sealed unventilable cavity, 0.1 m² of vault, ambient at 25 °C —
  are architecture, not modelling error. `NP-PWR-BUDGET-001` §3.3 names the only three levers, and
  all three are export-path changes, not supply changes.

> **D-2 (§17): `OI-PWR-01`'s multi-tile CFD remains the correct next step and this study does not
> displace it — but its result cannot make the wall-plug path timely.** It can move a 4–8 tile
> estimate to 3 or to 12. It cannot move 28–49 W to 1,792 W. **Sequence the CFD because the current
> ceiling may be optimistic and unsafe, not because it may be pessimistic and expensive.**

### 4.4 The one place watts genuinely are the binding constraint

§4's finding is specific to **PBM emitters in the sealed cavity**. It does not generalise, and
treating it as general would be the mirror of the error it corrects:

| Load | Heat path | Bound by |
|---|---|---|
| PBM tiles (T1-A, T1-C) | Sealed cavity residual + BN-boss export | **Heat.** 27.6–49.1 W. No source helps. |
| 1170 nm laser + TEC (T2-D) | Own TEC and thermal path; η_wp 0.15–0.25 | Heat, on its own budget — `OI-PWR-05` |
| **TMS coil drive (T2)** | Coil site, non-conductive CFRP window, outside the tile cavity | **Genuinely power-bound.** 15–400 W avg during a train (`NP-PWR-BUDGET-001` §4.1), and **no electrical specification exists anywhere** |
| Hub electronics, fan, Helmholtz coils, EEG front end | Hub enclosure, externally ventilated | Small and fixed (~6–8 W) |

> **If there is a real case for a source above 240 W anywhere in this programme, it is TMS, and it is
> T2.** It is also unsizeable today: `OI-PWR-02` (is 0.1–0.5 T even the right field?) and `OI-PWR-03`
> (real coil energy) are both open, and `OI-PWR-11` decides whether a dual-inlet architecture was
> already assumed. **§6's source set therefore provides a T2 mains path as an accessory with a stated
> envelope, and explicitly does not size it against TMS.** Sizing hardware against a modality with no
> electrical specification is what `NP-PWR-BUDGET-001` §4 exists to prevent.


---

## 5. Thermal dose — can we tell, before a protocol runs, whether it would injure?

**Principal direction, 2026-08-27:** scalp heat is **not** a goal — it is an incidental by-product to
be bounded. Thermal injury is a time-temperature dose, so: given a defined protocol, can we determine
in advance whether it would inflict one, because it runs too long at too much generated heat?

> **The 42 °C applied-part limit stands. Nothing in this section relaxes a ceiling.** `DI-SAFE-08`,
> `DI-SAFE-13`, `RISK-26` and `SR-FAN-01…06` are unchanged and are used here as given.
>
> *(The principal-supplied citation, Frontiers in Physiology 2019, DOI `10.3389/fphys.2019.01556`,
> could not be fetched from this environment — proxy 403 on both `frontiersin.org` and `doi.org` — and
> is characterised nowhere in this document. Any heat-shock benefit is incidental and unclaimed; no
> clinical claim is made in either direction.)*

### 5.1 The gap, verified

**Every thermal control in the design is a real-time reactive interlock.** The 42 °C face limit, the
scalp-facing NTC at PD2 (Path B1), the 62 °C junction throttle and `SR-FAN-03`'s fan-loss derate all
answer one question: *is the face too hot right now?*

**Nothing answers: will this protocol, over its stated duration, accumulate a harmful dose?** And
nothing considers repeated or back-to-back sessions.

> **Verified rather than assumed.** A search across `docs/`, `firmware/` and `scripts/` for `CEM43`,
> *"cumulative equivalent"*, *"thermal dose"* or *"cumulative thermal"* returns **no hit outside this
> document**. Stated in the form this programme requires: **there is no cumulative thermal-dose
> treatment on `main`.** Concurrent branches were not searched and no claim is made about them.

**Why the gap is not merely an omission.** A temperature interlock bounds **temperature**. Thermal
dose is temperature **integrated over time**. An interlock that holds the face at exactly 42 °C is
functioning perfectly and accumulates dose at **0.25 CEM43 per minute, indefinitely.** *The interlock
cannot bound dose, because duration is not one of its inputs.* That is a structural gap, not a tuning
problem, and §5.5 shows exactly where it bites.

### 5.2 Method — `scripts/check-thermal-dose.ts`

Per `NP-SES-PWR-001` **D-1**, committed as a script, importing `analyse()` from `check-pbm-power.ts`
so the demand model cannot fork. That script already parses `duration:` and already computes cascade
group counts; **the only new parts are the watts → face-temperature transfer and the time
integration.**

> CEM43 = Σ Δt · R^(43 − T),  R = **0.25** for T < 43 °C,  R = **0.5** for T ≥ 43 °C

**The breakpoint is the thing to understand.** Below 43 °C each extra degree **quadruples** dose;
above it, doubles. So dose accumulates very slowly in the regime the interlock permits — and the
consequence is not that dose is negligible, but that **it is duration, not temperature, that
determines whether it matters.**

Transfer model, first-order lumped:

> ΔT_ss = a·(W_tile ⁄ 6.25) + R_cav·P_total,  T_face(t) = T_amb + ΔT_ss·(1 − e^(−t⁄τ)),
> clamped at 42 °C by the interlock

| Constant | Value | Source |
|---|---|---|
| τ_face | **35–45 min** | `NP-THERM-CFD-R1-001` §4 |
| R_cav | **0.23–0.41 °C/W** | `NP-PWR-BUDGET-001` §3.2, over ~0.1 m² |
| Calibration point | 25 °C ambient, single T1-std tile → **30.7 °C face** | `NP-THERM-CFD-R1-001` §5.1 |
| Face limit | **42 °C** | `DI-SAFE-08` / `DI-SAFE-13` |

`a` is back-solved at each R so the calibration point is reproduced **exactly**, which is the only
anchoring the record supports.

> **⚠ This is a FLAG-FOR-REVIEW tool, not a pass/fail gate, and it must not be presented as one.**
> `NP-THERM-CFD-R1-001` is a 1D closed-form plus an explicitly **non-verification-grade** axisymmetric
> FD model, and `OI-PWR-01`'s verification-grade CFD has not run. Every constant is a **named export**
> so the CFD's outputs can replace them without rewriting the check. **No constant here is a derived
> figure and none may be quoted as one.** It becomes a gate when `OI-PWR-01` lands — `OI-PWRSRC-05`.

### 5.3 Result 1 — for protocols that fit the envelope, the answer is a clean null

| Protocol | W | min | Peak face | CEM43/session | **108-session course** |
|---|---:|---:|---:|---:|---:|
| PBM — Parkinson's (transcranial) | 22 | 20 | 29.6 °C | 3.2 × 10⁻⁸ | 3.5 × 10⁻⁶ |
| PBM — Autism (pediatric, 40 Hz) | 13 | 6 | 25.9 °C | 1.8 × 10⁻¹⁰ | 1.9 × 10⁻⁸ |

> **Stated plainly, because a null result is a real result and must not be dressed up: every protocol
> that fits the current power envelope accumulates a thermal dose seven or more orders of magnitude
> below any reference line, over a full 108-session course.** Peak face temperature never approaches
> 42 °C — it never leaves the twenties. **There is no thermal-dose problem in the protocols the device
> can run today.**
>
> The reason is the CEM43 breakpoint: at 30 °C the accumulation rate is 0.25¹³ ≈ 1.5 × 10⁻⁸ per
> minute. **Below the interlock the metric is not close to mattering; it is nowhere near it.**

### 5.4 Result 2 — back-to-back sessions are also fine, and this too is a null

τ_face at tens of minutes against 6–30-minute sessions means a second session starts from an elevated
baseline. Worked for two 20-minute, 40 W sessions:

| Gap | Carried-over rise | Peak face, 2nd session | CEM43, 2nd session |
|---:|---:|---:|---:|
| 0 min | 8.2 °C | 37.9 °C | 3.1 × 10⁻³ |
| 30 min | 3.5 °C | 35.2 °C | 5.4 × 10⁻⁵ |
| 120 min | 0.3 °C | 33.4 °C | 3.6 × 10⁻⁶ |

**Even with zero gap the second session peaks at 37.9 °C**, five degrees below the interlock, and its
dose is still four orders of magnitude below the conservative reference line.

> **A correction of record, because the expectation going in was the opposite.** Repeated and
> back-to-back exposure was flagged during commissioning as a likely hidden hazard — τ_face at tens of
> minutes against sessions of tens of minutes looks like a recipe for accumulation. **The arithmetic
> says that warning was overstated.** The mechanism is real: carry-over at zero gap is 8.2 °C and is
> not nothing. The magnitude is not: **3.1 × 10⁻³ CEM43**, because the CEM43 breakpoint means dose
> below the interlock is dominated by how close the face gets to 43 °C, and 37.9 °C is not close in a
> metric that quarters per degree.
>
> **Ambient temperature matters far more than repetition, by about eight orders of magnitude** (§5.6:
> a single session at 35 °C ambient reaches CEM43 1.5, against 2.9 × 10⁻⁶ at 25 °C). **If there is a
> scarce unit of thermal-safety attention, it belongs on the ambient gate, not on session spacing.**

### 5.5 Result 3 — cascading converts the power problem into a dose problem, and this is the finding

`NP-SES-PWR-001` §4 proposes **cascading** — rotating socket groups over time — as the remedy for the
over-budget condition, and computes the session lengths it implies (up to **20 hours** for Vascular
Baseline). A cascaded protocol runs **at the governor's ceiling** for `groups × duration`. Running the
same dose model over that:

| Protocol | Groups | Hours | Peak face | **CEM43, ONE session** |
|---|---:|---:|---:|---:|
| **Vascular Baseline** | 40 | **20.0** | clamped from 51.4 °C | **292** |
| **Full T1 Immersive** | 10 | 5.0 | clamped from 43.7 °C | **57.0** |
| **Memory Boost** | 8 | 4.0 | clamped from 53.9 °C | **52.8** |
| Focus Prime | 10 | 3.3 | clamped from 43.8 °C | 32.5 |
| Alpha Calm | 10 | 3.3 | clamped from 43.7 °C | 32.0 |
| ADHD Focus | 5 | 3.3 | clamped from 43.5 °C | 31.4 |
| Gamma Focus | 9 | 3.0 | clamped from 43.8 °C | 27.5 |
| Deep Sleep | 4 | 3.0 | clamped from 43.2 °C | 25.2 |
| *…12 more, 1.1 × 10⁻³ to 13.7* | | | | |

**Three of twenty cascaded protocols reach the 40 CEM43 reference line in a single session, and
thirteen of twenty reach 2.**

> **The mechanism is exactly the gap §5.1 identifies, and it is worth stating as a general rule:**
>
> **The interlock caps temperature at 42 °C and says nothing about how long the face may stay there.
> At 42 °C the accumulation rate is 0.25 CEM43/min, so 40 CEM43 is reached in 160 minutes of
> contact at the limit — and a cascaded protocol is *designed* to run for hours.** Cascading does not
> raise the temperature; the interlock forbids that. **It raises the time at temperature, which is the
> other half of the dose, and nothing in the design looks at it.**
>
> **`NP-SES-PWR-001` §4's cascading remedy is therefore not thermally free**, and that document's own
> summary — *"there is no free lunch in the energy domain"* — turns out to understate the case. There
> is no free lunch in the **dose** domain either, and dose is the axis that bears on injury.
> `OI-PWRSRC-01`.

#### 5.5.1 What the *"clamped from"* column actually means, and it is a second finding

Eight of the peaks above exceed 42 °C **before clamping** — Memory Boost at 53.9 °C, Vascular Baseline
at 51.4 °C. **Those are the face temperatures the protocols imply as authored; they are not
temperatures the device would produce**, because the Path B1 NTC and the current throttle hold the
face at the limit. The CEM43 figures in the table are computed on the **clamped** trajectory and are
therefore what the device would actually deliver.

**The consequence is not a safety one. It is an efficacy one, and it has not been stated anywhere:**

> **A protocol whose implied face temperature exceeds 42 °C does not overheat the user. It runs
> throttled — which means it does not deliver the irradiance it was authored at, and therefore does
> not deliver the dose its evidence base specifies.** The interlock converts a thermal-safety problem
> into a **silent under-dosing** problem, and `NP-PWR-BUDGET-001` §3.4 records the attached lesson in
> the programme's own words: *"under-dosing, not mechanism failure, explains most nulls."*
>
> So for the eight clamped rows the honest statement is **"cannot deliver its evidence-grade dose"**,
> not "runs hot". Nothing in the app, the compiler or the session record tells the user or the
> clinician that a throttle occurred, which makes it exactly the kind of silent substitution
> `NP-SES-PWR-001` §3 objects to for zone scope. **`OI-PWRSRC-21`.**

**And the 20-hour row should be read as a reductio, not a scenario.** A 20-hour cascaded Vascular
Baseline is not clinically plausible under any protocol design — nobody wears a helmet for twenty
hours. Its value is that it shows **where the cascading remedy terminates**: the group count is set by
the power budget, and at 40 groups the remedy has stopped being a remedy. The rows that matter
practically are the 3–5 hour ones (Full T1 Immersive at 57 CEM43, Memory Boost at 52.8), which are
long but not absurd.

**Three honest qualifications, all of which cut against the alarming reading:**

1. **The reference lines are literature values and this study asserts none of them.** CEM43 = 2 and
   CEM43 = 40 are printed as reference overlays, not thresholds. The **derived** quantity is the dose;
   the threshold it should be compared against needs a sourced clinical input — `OI-PWRSRC-02`.
2. **This is a flag-for-review, not a finding of injury.** The constants are provisional (§5.2), and a
   20-hour cascaded Vascular Baseline is a hypothetical: `OI-SESPWR-02` may find it non-compliant on
   R-4 grounds before dose ever applies, and `OI-SESPWR-01`'s scope correction removes most of the
   cascade requirement. **The finding is that the check does not exist, not that the library is
   dangerous.**
3. **The clamped rows are bounded by the interlock, which works.** §5.5.1 — the residual exposure is
   *time at the ceiling*, not temperature above it, and the ceiling itself is enforced in hardware by a
   control that this study does not touch and finds no fault in.

### 5.6 Result 4 — ambient temperature dominates everything else

| Ambient | Peak face, 20-min 40 W session | CEM43 |
|---:|---:|---:|
| 25 °C (nominal) | 33.2 °C | 2.9 × 10⁻⁶ |
| **35 °C** (`NP-ENV-OPRANGE-001` derate threshold) | **43.2 °C** | **1.5** |

> **A 10 °C rise in ambient moves a single session's thermal dose by roughly half a million times.**
> `NP-ENV-OPRANGE-001`'s ambient gate (full ≤ +35, derate +35→+43, block > +43) is therefore doing far
> more safety work than its position as an environmental spec suggests — **it is arguably the single
> most load-bearing thermal control in the design**, and it was derived as a headroom calculation
> rather than as a dose control. This is a *confirmation* of that gate, not a criticism: it is right,
> and the reason it is right is stronger than the reason given.

**Sensitivity across the model's own uncertainty**, so the conclusions are not read as precise: at
R_cav 0.23 / τ 45 min a 20-minute 40 W session peaks at 29.5 °C with CEM43 2.8 × 10⁻⁸; at R_cav 0.41 /
τ 35 min, 33.2 °C and 2.9 × 10⁻⁶. **Two orders of magnitude of model spread, against seven orders of
margin in §5.3.** The Case A null survives the spread comfortably; §5.5's cascaded figures do not
have that luxury, which is why they are flagged rather than concluded.

**`OI-SESPWR-03` moves heat by 4× on six of 23 protocols exactly as it moves power** — same input,
same factor. The CW (higher) reading is used throughout, per `NP-SES-PWR-001` D-3.

**`SR-FAN-03`'s fan-loss case is deliberately not dose-modelled:** PBM halts or trickles to
~4.5 mW/cm² on fan loss, so the single-fault case is bounded by the interlock and by `DI-SAFE-13`, not
by dose. Modelling it would be modelling a state the design does not permit to persist.

### 5.7 The runtime counterpart — a predictive gate, and what it costs

A CI script catches an authored protocol. It does not catch a clinician-authored one, an
operator-selected zone set, or a session begun at 38 °C ambient. So the check needs a runtime sibling.

| Element | Where | Class | Why |
|---|---|---|---|
| **Predicted-dose computation** at protocol admission — same model, same constants | Main processor, alongside the governor | **B** | It *permits* rather than *prevents*. Refusing to start is a fail-safe direction, and the Class C interlock remains behind it unchanged |
| **The 42 °C interlock** | Safety MCU | **C** | Unchanged. Nothing in this section touches it |
| **A cumulative time-at-limit counter**, incremented while the face NTC reads within 1 °C of the limit, with a per-session ceiling | Safety MCU | **C** | §5.5's failure mode is duration at the ceiling. **This is the only new Class C element, and it is a counter and a comparison** — the cheapest possible answer to the gap |

> **The predictive gate is Class B; the backstop is Class C and is a counter.** That split matters: a
> *prediction* that a protocol will overheat is a modelling claim built on provisional constants, and
> putting provisional constants in a Class C partition would freeze them there. **A measured
> time-at-limit counter needs no model at all** — it observes the NTC the design already has, and it
> bounds dose directly rather than by prediction. `OI-PWRSRC-03`.
>
> **Divergence control, per D-1:** the transfer model and its constants must come from one source of
> truth into both the TypeScript audit and the C runtime, with a CI check that the two agree — the
> same requirement §11.4 makes for the demand model, and for the same reason.

---

## 6. The T1 / T2 protocol split, F4, and whether power can be the gate

**Principal direction, 2026-08-27:** a T1/T2 distinction for protocols is acceptable — *"anything that
might risk injury should be monitored by health professionals."* And, separately: *"could the T1/T2
protocol enforcement be handled by limiting the kind of power supplied to a T1 device?"*

This is the relief valve for the whole brief, so it is worth being precise about what it fixes.

### 6.1 What exists today, verified line by line

| Claim | Verified |
|---|---|
| A T2 flag exists on the wire | **Yes.** `firmware/hub_control/include/np_hub_types.h:89` — `uint8_t flags; /* bit0=T2_tier; bit1=autonomous */` — and `NP_PROTO_FLAG_T2_TIER (1U << 0)` at `:193`. **No protocol-format change is needed** |
| Nothing in firmware reads it | **Yes.** `grep -rn "T2_TIER\|T2_tier" firmware/` returns **only those two definition lines**. It is set, transmitted, and ignored |
| It is derived solely from the modality type set | **Yes.** `app/web/src/lib/hubCompiler.ts:158` — `T2_MODALITY_TYPES = {qeeg_21ch, tms, pbm_deep_1170nm, clinical_tacs, hd_tdcs, cervical_vns}`; set at `:333` |
| A tier-scoped *parameter range* precedent exists | **Yes, and it is unenforced too** — see below |

**The parameter-range precedent answers the question it was asked to answer, and the answer is
instructive.** `NP-NPPS-REF-001` §4 gives `intensity_milliamps` as *"0–1 (T1), 0–4 (T2 clinical)"*.
`app/web/src/lib/protocolValidator.ts` checks `maxIntensityMilliamps` at **scope `'global'`** — one
limit from `limitsStore`, with **no tier term anywhere in the comparison**.

> **So the closest existing analogue is also documented-and-never-implemented.** The programme has
> **two** declared-but-unenforced tier mechanisms — a wire flag nothing reads, and a parameter range
> nothing scopes. **That is the finding the precedent yields: the pattern here is to write the tier
> down and never build it**, which is precisely why specifying the mechanism is the value this section
> adds rather than a restatement.

### 6.2 The genuinely new thing: a tier gate with no physical backing

Today *"T2 only"* is a **hardware fact**. A T1 helmet has no TMS coil and no 1170 nm laser, so a T2
protocol fails the ordinary **F1** (missing element) check. **Enforcement is physical — the hardware
is simply not there**, which is why nothing reads the flag and why nothing needed to.

This direction asks for something categorically different: **a protocol using only T1 modalities, on
hardware a Home Standard fully possesses, restricted because of the risk its dose, duration or power
carries.** `clinical-03` is exactly that — ordinary `pbm_transcranial` on ordinary tiles.

> **That is the first tier gate with no physical backing, and no mechanism for it exists.**

### 6.3 F4 — tier / supervision, and the rule that must go with it

The eligibility mechanism reports **F1** major (wrong module fitted), **F2** minor (variant cannot
reach the irradiance) and **F3** envelope (budget cannot run them at once). This adds **F4**.

| Class | Blocker | Remedy available to the user |
|---|---|---|
| **F1** | Wrong module fitted | Buy/fit the module |
| **F2** | Fitted variant cannot reach the irradiance | **None by power** — `OI-HEXTILE-21`'s η_wp wall |
| **F3-a** | Electrically bound; a purchasable source clears it | Buy the source |
| **F3-b** | Thermally bound; no source clears it | Reduce scope or extend in time |
| **F4** | **Tier / supervision** | **None the user can perform alone** |

> **F4 must never be presented as F1, F2 or F3.** Telling a home user to buy a module, an emitter
> variant or a bigger supply when the real blocker is *"this protocol requires clinical supervision"*
> **takes their money for a purchase that cannot fix their problem.** That is strictly worse than the
> existing never-present-F3-as-F2 rule, which only wastes trust — this one wastes money too. **F4 is
> the one class not fixable by any purchase the user can make alone, and that is exactly what the
> message must convey.**

### 6.4 Device tier is a proxy for clinical supervision, and an imperfect one

The stated justification is *"monitored by health professionals."* **The device cannot observe that.**
It can observe its own tier and configuration, and nothing else.

- **A Pro device used unsupervised passes an F4 gate.** Nothing about owning T2 hardware implies a
  clinician is in the room, and Pro Entry is a purchasable product.
- **A supervised T1 user fails it.** A clinician with a Home Standard in front of them is blocked.

**What in-tree narrows the gap, and what does not:**

| Mechanism | Does it narrow the gap? |
|---|---|
| T2 service contract ($1,800/yr, CLAUDE.md §2) | **Weakly.** It evidences an institutional relationship at purchase; it says nothing about any given session |
| Clinic warranty registration (CLAUDE.md §6 — the warranty owner may be a clinic) | **Weakly, and only at registration.** §6's own invariant is that a clinic registering warranty *"has NOT consented on behalf of any patient"* — the record deliberately does not bind a clinic to a session |
| Multi-patient dashboard, `anonymized session tag` (T2) | **Weakly.** Their presence implies a clinical deployment; neither is per-session evidence of supervision |
| Anything per-session | **Nothing exists** |

> **State it as it is: device tier is a proxy for supervision, it is the only proxy the device can
> evaluate, and it is loose in both directions.** Presenting it as supervision would be the same
> error the programme already refuses for `sLORETA` (a target being localisable is not it being
> reachable) and for a DLPFC-titled protocol that irradiates a lobe. **`OI-PWRSRC-04`** carries
> whether a per-session supervision assertion is wanted at all — noting that `NP-FW-BENCH-001`'s
> physical-assertion pattern is exactly the shape one would take.

### 6.5 Can power be the gate? Partly — and the numbers say where it fails

**Where it is right, and it is genuinely elegant.** For **F3 envelope** failures, power-gating is not
merely a candidate mechanism — **it is the mechanism already in place.** The governor checks watts
against the negotiated contract (`NP-SES-PWR-001` D-4). **The gate is physics, not policy**: nothing
for firmware to enforce, nothing for an app to bypass, no entitlement, no credential, no key. For the
protocols it covers it is strictly better than any software scheme, and it costs nothing new.

**Where it fails: power is not the risk axis.** Injury risk tracks local irradiance, thermal dose,
duration and charge density. **Computed rather than asserted**, across the 22 protocols with
determinate demand:

| Relationship | Spearman ρ |
|---|---:|
| Total watts vs **per-tile watts** (irradiance proxy) | **0.849** |
| Total watts vs **per-tile watts × duration** (dose proxy) | **0.582** |

> **The first number is the honest concession: total power *is* a good rank proxy for irradiance.** A
> flat objection that power has nothing to do with risk would be wrong, and this study does not make
> it.
>
> **The second is the objection that survives, and duration is what breaks it.** A supply can see
> watts. It cannot see minutes.

**The decisive inversion, from the data rather than from argument.** Rank the library by total watts
and by the dose proxy:

| Rank | By total watts | | By dose proxy |
|---:|---|---:|---|
| 1 | Vascular Baseline — 1,600 W | | **Memory Boost — 750** |
| 2 | Focus Prime — 400 W | | Vascular Baseline — 600 |
| 3 | Alpha Calm — 375 W | | PBM — Cognitive Enhancement — 200 |
| … | | | |
| 7 | **Memory Boost — 200 W** | | |

> **Memory Boost carries the highest dose proxy in the entire library and sits seventh on power.**
> Any cap above 200 W **passes** it. At a 232 W cap the audit reports the inversion directly:
> **Memory Boost (dose proxy 750) passes while `PBM — Alzheimer's (Chun 2026)` (dose proxy 51) is
> blocked**, and **ten** passed protocols carry a higher dose proxy than at least one blocked
> protocol.
>
> **So a power cap set anywhere above 200 W passes the library's highest-dose protocol, and a cap set
> low enough to catch it blocks nearly everything.** There is no cap value that separates the two —
> not because the axes are unrelated, but because the residual 0.582 is concentrated exactly where it
> matters.

**Three further limits, each independent of the correlation:**

1. **It cannot express the tier splits that already exist.** `intensity_milliamps` 0–1 (T1) vs 0–4
   (T2) and tDCS's 40 µC/cm² are **milliamp-scale** distinctions. A 4 mA protocol draws no more from
   the wall than a 1 mA one in any way a supply could detect. **Any power-based scheme leaves the
   existing parameter split exactly as unenforced as it is today** (§6.1).
2. **Thermal dose is orthogonal, and §5 proves it with numbers.** §5.5's worst case is a *cascaded*
   protocol — one that has been made to **fit** the power budget and then runs for twenty hours.
   **A power gate would pass it, because fitting the budget is precisely what cascading achieves.**
3. **It implements the F3/F4 conflation §6.3 forbids.** Power-gating makes tier a function of the
   power budget, so an F4 refusal becomes indistinguishable from an F3 refusal — **and the user's
   obvious response to an F3 is to buy a bigger supply**, which is the money-wasting failure the rule
   exists to prevent.

### 6.6 Two collisions that must be named

**(a) CLAUDE.md §2's charger policy is LOCKED and forecloses the enforcement.** It states that
chargers are *"branded recommendations, not proprietary requirements. Any PD-compliant charger must
work"*, and that the app shows reduced power *"informatively, never blocks."* **A T1 buyer fits a
240 W EPR charger bought anywhere and the gate is gone.** Making it enforceable means rejecting
compliant chargers — contradicting a locked policy, and running at the EU common-charger regime
(whose applicability to a device with no battery is itself open, §14.3). **So as stated the mechanism
is advisory, and locked policy requires it to stay advisory.** An advisory injury gate is not one.

**(b) It collides with the principal's own purchaser-choice direction, and this is the sharpest
tension in the brief.**

> **If purchasers choose their power source knowing what their protocols require, then selling a T1
> buyer a bigger supply sells them T2 capability.**
>
> **Power can be the gate, or it can be the purchase decision. It cannot be both.** The two directions
> of 2026-08-27 are individually reasonable and jointly inconsistent on this one point, and the
> inconsistency should be resolved deliberately rather than discovered in implementation.

### 6.7 The version that does work — and it is not the accessory

The strongest form of the idea is a **proprietary, authenticated T2 power accessory** rather than a PD
charger: then the gate is a **physical entitlement, like the TMS coil**, and enforcement really is
hardware. Precedent exists — accessory authentication pass/fail is already an SHDR field, and the
intranasal sleeve uses **optical code + pogo-pin resistive authentication, deliberately no NFC and no
RF**. That pattern is reusable as-is.

**The §2 escape is available but weaker than it looks.** §2 governs *"chargers"*, and a T2 power
accessory is arguably a different article. But **this device has no battery — nothing in the box
charges anything**, so every "charger" in §2's table is in substance a power supply (§14.3). Reading
the word narrowly to escape the policy is a **semantic escape from a locked decision**, which is
exactly the kind of move that needs a principal's signature rather than an engineer's reading.

**Weighed against a signed, device-bound entitlement over ordinary power** — the `NP-FW-BENCH-001`
shape: an Ed25519 device-bound credential redeemed against a physical assertion, RAM-only, annunciated
on a dedicated LED state that stealth mode cannot suppress, with an audit record:

| Axis | **Proprietary authenticated T2 supply** | **Signed device-bound entitlement** |
|---|---|---|
| Enforceability | Strong — no accessory, no watts. **But the accessory is transferable: one unlocks any device** | Strong — **bound to the device**, and redeemed in the partition that owns the enables |
| BOM / cost | $210–380 accessory + authentication scheme + tooling | **~$0 BOM.** Firmware plus a key-management process |
| **Regulatory surface** | **NeurOne owns a mains-derived isolation barrier** on a device with conductive applied parts, at the 510(k) tier, with `VE-11` **Open** (§9) | **None new.** Reuses the protocol-signing trust root CLAUDE.md §4 already requires |
| EU / §2 | Contradicts the locked stance, or escapes it semantically | **Does not touch it.** Any PD-compliant supply still works |
| **Clinic, accessory fails mid-session** | **Total loss of therapy, immediately, no fallback** — the tier key and the power path are the same object, so one failure takes out both | Protocol will not start; **the device still runs every T1 protocol** and the clinic keeps working |
| Expresses parameter-range and dose gates | **No** (§6.5) | **Yes** — it is an assertion about the session, not about the watts |

> **The entitlement wins on five of seven axes, and the two it does not win are not close.** The
> decisive pair: it adds **no** regulatory surface where the accessory adds the largest one in this
> whole study, and it **decouples the tier key from the power path**, so an accessory failure in a
> clinic does not end the session.
>
> **This also answers the isolation question §9 was already asked to test, and the two are indeed the
> same question:** the only reason to make NeurOne own a mains isolation barrier would be to obtain
> a physical tier key — and a device-bound credential obtains a better one for nothing. **The
> isolation cost buys nothing that the cheaper mechanism does not already buy better.**

### 6.8 The rule this yields, and it is more useful than the answer

> **D-9: each failure class gets the mechanism that matches its remedy.**
>
> | Class | Mechanism | Status |
> |---|---|---|
> | F1 | Physical presence of the module | **Exists** |
> | F2 | Fitted variant's declared capability | **Exists** |
> | **F3** | **The power governor — watts against the summed contract.** Physics, not policy | **Exists in spec (D-4); `OI-HEXTILE-09` to build** |
> | **F4** | **Device-bound signed entitlement, redeemed against a physical assertion** | **Does not exist. §6.7** |
>
> **The principal's intuition is right for F3 and only for F3** — and for F3 it is not a proposal,
> it is a description of the design. Extending it to F4 conflates a purchasable blocker with an
> unpurchasable one, which is the one thing §6.3 forbids.

### 6.9 What the split buys, and it is the resolution of the whole brief

> **T1 stays on USB-C and keeps Mode 3 autonomy. The high-draw, high-dose protocols become T2, where
> mains power, a clinician and a 510(k) posture already exist.** The demand cliff does not have to be
> met by one architecture, and §4's thermal finding does not have to be argued against a library that
> was never going to run on a home device anyway.

**And a commercial consequence that arrived from an unrelated direction.** CLAUDE.md §2.1a records
**`OI-COST-08`**: with retail unlocked, Home Premium at $2,475–2,637 against Pro Entry at $4,999 makes
the two-tier structure *"one tier with a regulatory footnote."*

> **Capability gating gives T2 a reason to exist beyond paperwork.** A protocol library a T1 device
> cannot run is a **product** distinction, not a compliance artefact — and it is one a buyer can
> evaluate. **Recorded as an input to `OI-COST-08`**, which this study does not own and does not
> decide.

### 6.10 Which protocols this study would nominate, and why it does not nominate them

The obvious next step is a candidate list. **It is deliberately not produced here**, and the reason is
`OI-SESPWR-01`: most of the high-demand protocols are over-scoped rather than intrinsically
high-power — `clinical-04` irradiates 37 sockets for a bilateral-DLPFC indication. **Tier-gating a
protocol because of a scope defect would freeze the defect into the product structure**, and it is a
data edit to fix.

> **Sequence: `OI-SESPWR-01` (fix scope) → re-run both audits → *then* nominate the residue for T2.**
> `OI-PWRSRC-06`. Nominating first would gate the wrong protocols, permanently, on the wrong evidence.

---

### 6.11 The residual, quantified — and whether the power gate is still worth its isolation cost

The commissioning question was: how many protocols are *"risky but affordable on T1"*, i.e. inside a
T1 device's reach yet carrying enough injury risk to justify gating? **§4 and §5 between them have
turned that from a sizing question into the whole question**, so it is answered directly.

**Step 1 — what a T2 power accessory would unlock that a T1 device cannot already reach.**

| | Protocols |
|---|---:|
| Runnable **single-pass** on T1 today (40 W) | **2 / 23** |
| Runnable single-pass with a 500 W mains accessory, **electrically** | 21 / 23 |
| Runnable single-pass with a 500 W mains accessory, **after the 28–49 W cavity ceiling** (§7.0) | **2 / 23** |
| **Net unlocked by the accessory** | **0** |

**Step 2 — what a T1 device can already reach without any power upgrade.** Cascading needs no watts;
it trades time for concurrency. Every one of the 20 over-budget protocols becomes runnable on a
stock T1 device by cascading, and **13 of those 20 carry a cascaded dose above the conservative
CEM43 = 2 reference line, three above 40** (§5.5).

> **So the residual is not small. It is the entire risk set, and it sits on the *wrong side* of the
> gate.** The protocols that carry dose risk are **already reachable on a stock Home Standard, with
> the 45 W brick that ships in the box**, because they are risky by virtue of **duration at the
> ceiling**, not by virtue of drawing a lot. A power gate is blind to all thirteen.

**Step 3 — the straight answer.**

> **The mechanism is sound. The premise it rests on has moved, and the principal should hear it from
> this document.**
>
> The power gate was decided against a coverage ladder in which a bigger supply unlocked 16, then 21,
> then 22 of 23 protocols. **§7.0 invalidates that ladder**: after the sealed-cavity ceiling, every
> rung unlocks the same two protocols. A gate whose currency is watts therefore **gates access to
> watts nothing can spend** — and it would do so at the cost of NeurOne owning an **IEC 60601-1
> isolation barrier** on a device with conductive applied parts, at the 510(k) tier, with `VE-11`
> still Open (§9), plus a $210–380 accessory whose failure ends a clinic session (§17.3).
>
> **Recommendation: do not buy the isolation barrier for this.** The benefit is zero net protocols
> and zero of the thirteen risky ones. **`OI-PWRSRC-22`, principal — the decision was correct on its
> stated premise and the premise did not survive §4.** What the direction actually wants — some
> protocols restricted to supervised use — is delivered in full by §6.7's device-bound entitlement, at
> $0.00 BOM, no new regulatory surface, and with the ability to express the duration and dose axes a
> supply cannot see.

### 6.12 Information for safety — the duty transfer the split creates, and its correct rank

Gating a protocol to T2 does not eliminate its hazard; **it transfers responsibility for that hazard
from the device to a person.** ISO 14971 ranks that control class last for a reason, and IEC 62366
makes it a usability obligation rather than a technical one.

> **Binding rule: information for safety sits *on top of* the hardware interlocks, never instead of
> them.** Every §4 interlock, the 42 °C ceiling, `SR-FAN-01…06`, the 40 µC/cm² limit and §5.7's
> time-at-limit counter fire identically for a supervised T2 session and an unsupervised T1 one. **No
> control is relaxed because a clinician is assumed present** — that assumption is precisely the one
> §6.4 shows the device cannot verify. This mirrors `NP-FW-EMMC-002` §H's non-coercion invariant
> (CHAR-4): participation buys *earlier and better*, never *baseline safety*.

Four channels, ranked by how much of the duty each can actually carry:

| Channel | What it can carry | What it cannot |
|---|---|---|
| **Per-protocol clinician acknowledgement**, at first use of each gated protocol, naming the specific hazard — duration at the thermal ceiling, cumulative dose across a course, the charge-density limit | **The most of the four.** It is per-protocol, so it can state *this* protocol's hazard rather than a generic warning, and it creates a record | It is a click. It cannot make an unqualified user qualified |
| **IFU + training**, at device commissioning | The systematic content: what the tier split means, what the interlocks do and do not bound, why a throttled protocol under-doses (§5.5.1) | Read once, at the moment of least context, by whoever unboxed it |
| **The $1,800/yr T2 service contract's annual calibration visit** (CLAUDE.md §2) | **The natural re-acknowledgement point, and it already exists** — an annual, funded, scheduled human touchpoint with the institution. Re-affirm the gated-protocol acknowledgements and deliver changed hazard information there | Annual granularity. It cannot reach a session |
| **CLAUDE.md §6.1's plain-language decision-support document** | — | **This is the wrong channel and should not be used.** §6.1's document is scoped to *what a clinician can and cannot learn from the data, and the privacy implications per element*. It is a **consent** artefact addressed to a data subject. Loading thermal-hazard information into it would dilute a document whose value is that it answers exactly one question, and would address the wearer in their capacity as a data subject about a matter that has nothing to do with their data |

> **The wearer-facing channel is missing, and that is the finding.** The wearer is not the purchaser,
> not the warranty owner, and — per CLAUDE.md §6's own invariant that a clinic registering warranty
> *"has NOT consented on behalf of any patient"* — not represented by either. **A gated protocol
> transfers duty to a clinician and leaves the wearer with no channel at all.** The programme has a
> consent surface for the wearer (`ConsentStore`, L1–L4) and a telemetry surface for the warranty
> owner, and **no safety-information surface for the person the hazard lands on.** `OI-PWRSRC-23`.

**One thing the device can do that no document can:** the amber in-use LED already *"pulse[s] at a
rate mirroring session frequency, so a caregiver can confirm the correct protocol across the room"*
(CLAUDE.md §4). **A gated protocol running is exactly the state a caregiver should be able to confirm
across a room**, and the annunciator exists. It should carry a distinct state for a gated protocol,
and — like the safety-fault annunciation — **stealth mode must not suppress it.**

## 7. The source set

### 7.0 Coverage is an upper bound — the reconciliation §3 and §4 force

§3 says PD 240 W EPR unlocks 16/23. §4 says the assembly cannot reject 232 W. Both are true:

> **A coverage figure states a *necessary* condition, never a sufficient one.** It answers *"can the
> source deliver the watts the protocol commands?"*, not *"can the assembly survive spending them?"*
> The thermal ceiling binds below the electrical one in **every** row of §3.1's ladder.

| | Electrical coverage | **Thermally achievable (28–49 W)** |
|---|---:|---:|
| R-10 today | 2/23 | **2/23** |
| PD 100 W | 4/23 | **2/23** |
| PD 240 W EPR | 16/23 | **2/23** |
| Mains 500 W | 21/23 | **2/23** |
| Mains 1,800 W | 22/23 | **2/23** |

*(Under `OI-SESPWR-03`'s duty reading every column reads 5/23; the conclusion is identical.)*

> **No source in the candidate set changes single-pass protocol coverage at all, today.** The device
> already owns the only source whose watts it can spend. **The route to the rest of the library is
> demand reduction (`OI-SESPWR-01`) and the T1/T2 split (§6), not supply.**

**One qualification — the only argument that could move this.** The 28–49 W ceiling is **steady
state**, and τ_face is 35–45 min against 6–30-minute sessions, so a 20-minute session reaches only
~39 % of its steady rise. That implies a transient-permissible aggregate of roughly **70–125 W for one
isolated session**, decaying toward 28–49 W for long or closely-spaced ones — and §5.4's measured
carry-over shows the decay is real but mild. **If it holds, the 92–132 W rung becomes defensible and
coverage moves to 4–6/23. It does not rescue mains**: 125 W is 4× below a 500 W station and 14× below
an 1,800 W one. **`OI-PWRSRC-07`; fold into `OI-PWR-01`'s scope.**

### 7.1 The set

| ID | Source | Envelope | To emitters | Mode 3? | Status |
|---|---|---:|---:|---|---|
| **S-1** | **USB-C PD, 15–45 W** (today) | 15 / 30 / 45 W | ≤ 40 W | **Yes** | **In force. Recommended as the T1 source, unchanged** |
| **S-2** | **USB-C PD 100–240 W EPR**, single sink | 100–240 W | 92–232 W | Yes, if a bank supports it (§7.2) | **Conditional**, gated on `OI-PWRSRC-07` + `OI-SESPWR-01`. Not a T1 default |
| **S-3** | **USB-C PD power bank** — *this is what Mode 3 is* | 45–140 W realistic | ≤ 132 W | **Yes, by definition** | In force. §7.2 records that its ceiling is below a wall brick's |
| **S-4** | **Mains base station**, separate attached accessory, DC to the helmet | 500 W class | 492 W | No — but §8.3 | **T2 only. Deferred**, not rejected: `OI-PWR-11`/`-02`/`-03` gate it |
| ~~S-5~~ | ~~Mains inlet in the head-worn assembly or the hub~~ | — | — | — | **REJECTED — §8** |

### 7.2 Mode 3's ceiling is below a wall socket's, and nothing says so

CLAUDE.md §4 defines Mode 3 as running *"from any USB-C PD power bank"*. But a bank is a different
source class from a wall brick: **140 W and 240 W EPR wall bricks are commodity and banks at those
contracts are not**, and a bank's deliverable power **falls as its cells sag**, so its contract is not
constant across a session.

> **`Mode 3 Autonomous` is not one mode; it is a source class with its own, lower envelope**, and any
> protocol above what a bank can hold is **wall-only**. CLAUDE.md §4's table has no column for that.
> **Mode 3 is named in CLAUDE.md §1 as the primary competitive moat — a moat whose width nobody has
> written down.** `OI-PWRSRC-08`.

### 7.3 What S-2 actually costs, since the connector already exists

*"A contract renegotiation and a connector already in the design"* is true at the connector and false
at the converter. `NP-PWR-BUDGET-001` §3.5 puts the emitter rail at **24 V**: a 20 V contract into it
is a **boost**, a 48 V EPR contract is a **buck**. That is a topology change, plus a resize from ~40 W
to ~232 W — **~19 W of converter loss inside the hub**, more than the entire current 6–8 W non-PBM
overhead, in an enclosure that already holds a fan, a heatsink and the 22 F supercapacitor. Buck at
48 V is on balance the better topology; **the point is that "the connector exists" is not "the power
path exists", and the two were being conflated.** `OI-PWRSRC-09`.

---

## 8. Where the mains hardware lives

The principal allowed three locations — *"either directly in the helmet or in something that attaches
to it."* They are not equivalent.

| Location | Verdict | Why |
|---|---|---|
| **Head-worn assembly** | **Rejected** | Mass on the head, and a switched mains-derived converter inside the assembly whose **measured** EMF shielding is the primary technical claim |
| **Control hub** | **Rejected** | `NP-TOOL-HUB-001` §2 places the hub PCB *at or immediately adjacent to the occipital arch* — **inside** that same assembly. The hub is not "off the head" in the sense this decision needs |
| **Separate attached base station** | **Recommended (S-4)**, if mains is built at all | Keeps mains entry, the isolation barrier, the bulk converter and its heat entirely outside the shielded volume, and hands the helmet SELV DC exactly as a brick does |

### 8.1 The EMF argument is decisive, and it is not new

`OI-PWR-12` already records it for a *second USB-C inlet*: 5 A of switched current is *"a materially
worse aggressor than a data port"*, and *"the claim is measured, so the qualification must be too."*
**Every term gets worse for a mains path:**

| | Second PD inlet | Mains inlet in the assembly |
|---|---|---|
| Current switched | up to 5 A | 5 A at 100 V+, or a ~20 A DC-side converter |
| Source impedance behind it | a certified brick's output filter | the mains network and its conducted-emission environment |
| Filter | Layer 5, already specified | new topology, **mains-referenced**, which Layer 5 was not designed for |
| Failure consequence | degraded shielding claim | degraded shielding claim **and** a mains-referenced fault current metres from the scalp |

The base station gets this for free: outside every shielded layer, its emissions are an ordinary
product-level EMC problem rather than a threat to the headline figure.

### 8.2 What the base station is

Deliberately unremarkable: a **certified external medical-grade AC/DC** (mains in, SELV DC out, barrier
inside a certified assembly); a **locking DC output connector** (§9.3); **ORing, inrush limiting and
current telemetry**; and a **hub dock** reusing `NP-TOOL-HUB-001`'s mechanical vocabulary. It is an
**accessory, not a BOM line** (§17), so it never touches a T1 configuration's gross margin — which
matters, because all four are negative before it starts.

### 8.3 It can preserve Mode 3's mechanism, and that is a real choice

S-4 is *"Mode 3: no"* because a mains-tethered device is not autonomous by CLAUDE.md §4's definition.
**But the base station can present its DC output as a USB-C PD source**, and then the helmet has one
inlet type, one negotiation path, one filter and one governor input whether it is fed by a brick, a
bank or the base station.

> **D-4: if a mains path is ever built, its output is a USB-C PD source, not a proprietary DC rail.**
> **One inlet type is one governor input, one filter qualification, one arbitration case and one
> failure mode.** It also keeps CLAUDE.md §2's EU stance literally true. **Note this is the same
> conclusion §6.7 reaches from the tier-gating direction** — a proprietary supply buys nothing that a
> device-bound entitlement does not buy better.

---

## 9. Isolation — does connector form decide it? Mostly no

`NP-PWR-BUDGET-001` §4.4.1 puts a two-cell table at the centre of its recommendation — *"Stays SELV —
the certified charger is the barrier"* versus *"NeurOne becomes the isolation barrier"* — and calls it
*"the decisive one."*

### 9.1 The test the brief asked for

**The row conflates connector form with mains entry.** IEC 60601-1's isolation requirement attaches to
**where mains enters the system** and to the separation maintained between mains and the patient. It
does not attach to the shape of a DC connector:

- An **external certified AC/DC supply** with a DC output places the barrier **inside the brick**, and
  the device downstream sinks SELV — **whether that DC arrives on USB-C pins or a locking circular
  connector.**
- **NeurOne becomes the barrier if and only if mains conductors enter NeurOne-designed enclosure** —
  a decision about *where the conversion happens*, not about the connector.

> **§4.4.1's decisive row is decisive about the wrong variable.** §8's recommendation — mains stays in
> a separate certified assembly — resolves the isolation question completely, for any connector.

### 9.2 But its conclusion survives, for three reasons it did not state

1. **The Layer 5 filter is qualified for USB-C and nothing else.** A different connector is a different
   filter topology and a new EMC qualification, inside a volume whose shielding is a measured claim.
   **This is the real cost of a new connector form, and §4.4.1 listed it in the row *above* the one it
   called decisive.**
2. **One inlet type is one arbitration case** (§8.3, §10).
3. **CLAUDE.md §2's EU stance survives verbatim.**

**And §6.7 adds a fourth, from a direction §4.4.1 could not have anticipated:** the only remaining
motive for owning a mains barrier would be to obtain a physical tier key, and a device-bound
entitlement obtains a better one at zero BOM and zero regulatory surface.

### 9.3 Two findings §4.4.1 did not make, and the first is serious

**(a) An ITE-grade charger is not a medical-grade one — and this is about the inlet that ships today.**
A commodity USB-C PD brick is qualified to **IEC 62368-1**. The device it feeds has **conductive
applied parts on skin** (tDCS, tACS, VNS), and `DI-REG-01` makes **IEC 60601-1** binding on the system.
60601-1's patient-leakage limits are substantially tighter than 62368-1's touch-current limits, and a
medical device plus a non-medical supply is what 60601-1's ME-system provisions exist to govern.

> **`OI-PWRSRC-10` — BLOCKING for `VE-11`, and it pre-exists this study.** *"The certified charger is
> the barrier"* does not identify **which standard certified it**. Either the supply must be
> 60601-1-qualified (contradicting *"any PD-compliant charger must work"*), or the device must provide
> the patient-side separation itself, or the applied-part isolation must be shown to make the supply
> irrelevant. **No position is taken here on which resolution is correct** — the finding is that the
> question has never been asked, and that it applies to the **first** inlet, not to any proposal.

**(b) USB-C is designed to be hot-unpluggable, and §10 makes that a safety property.** A device with no
battery loses its rails when its only source is removed. For a base station's DC output — a cable on
the floor near a chair — a locking connector materially reduces the probability of the mid-session
removal `OI-PWR-13` names. **It is the one place a non-USB-C form has a genuine advantage**, and D-4
still prefers USB-C, paying for it with §10's arbitration requirements instead.

---

## 10. Source arbitration (`OI-PWR-13`)

### 10.1 There is no battery, and that changes the problem's shape

CLAUDE.md §4 has no battery row; `NP-FW-EMMC-002` §H states the device has *"no battery or coin cell"*.

> **Loss of the last source is not a power *transition*, it is a power *failure*, at the speed of the
> hub's bulk capacitance.** On a battery device arbitration is a quality-of-service problem. Here it
> is a fail-safe-timing problem.

The only stored energy is the **22 F supercapacitor**, and CLAUDE.md §4 specifies its purpose
precisely: *"absorbs LED duty-cycle transients."* **It is not specified as a hold-up reserve, no
hold-up requirement exists anywhere, and no controlled document gives its rail voltage or usable depth
of discharge.**

> **`OI-PWRSRC-11` — BLOCKING for `OI-PWR-13`.** How long can the hub hold its rails after the last
> source is removed, and is that longer than a safe stimulation ramp-down? CLAUDE.md §3 requires a
> hardware-enforced **30 s tDCS ramp** — **certainly longer than any plausible hold-up.** So either the
> ramp has a defined power-loss exception or the discharge path must be passive and rail-independent.
> **A pre-existing gap in the single-source design that multi-source makes visible.**

### 10.2 Is a source change in the module-change class?

The locked adjacent rule: a module insertion or removal stops any running protocol **with power cut**.

| | Module change | Source change |
|---|---|---|
| What changes | **What is connected to the patient** | **What feeds it** |
| Hazard | Energising an unexpected emitter set; a live contact during mating | Under-supply |
| Right response | Stop **with power cut** — the cut *is* the mitigation | Recompute the budget; derate or halt. **A power cut mitigates nothing and creates the brownout it is meant to prevent** |

> **D-7: a source *change* is not in the module-change class; a source *loss* is not a change at all —
> it is a fault, and belongs to §10.1's hold-up path.** Cutting power when a *second* source is added
> would convert a strictly-improving event into a session termination.

### 10.3 The four cases

| Event | Behaviour | Rationale |
|---|---|---|
| **Source added mid-session** | Accept. Budget recomputed **upward at the next protocol boundary, never mid-stimulus** | An upward recompute mid-stimulus silently changes delivered irradiance inside a session the protocol did not declare |
| **Redundant source removed, ≥1 remains** | Budget recomputed **downward immediately**. If the running protocol exceeds it: derate if the protocol declares derating admissible, else **halt without power cut** | Power is still present, so a clean halt is available. *Derating admissibility is the same per-protocol field `OI-SESPWR-04` requires for cascading, for the same reason: entrainment protocols must not be silently derated either* |
| **Last source removed** | **Fault.** §10.1's hold-up path; fail-closed discharge of every stimulation output | Not a decision — the rails are collapsing |
| **Source degrades in place** (bank sag, thermal foldback) | Continuous downward recompute | §7.2 — a bank's contract is not constant |

### 10.4 OR-ing and inrush

**Ideal-diode ORing with autonomous FETs**, one per inlet — arbitration must complete without
software, because §10.1 gives software no reliable window. **Priority: highest negotiated contract
takes the load**, explicitly not "first plugged wins", which would make the budget depend on insertion
order. **Inrush: a hot-swap controller per inlet with soft-start into the 22 F supercapacitor** — 22 F
is a large capacitance to charge, and an uncontrolled hot-plug into it is an `OI-PWR-12` EMF event as
well as an electrical one.

---

## 11. The governor

**D-4** specifies the check as watts against *the negotiated PD contract*. With multiple sources:

> **budget = Σ (negotiated contract of every source presently supplying) − non-PBM overhead −
> converter loss**, recomputed on every arbitration event, raised only at a protocol boundary.

1. **Watts, never tiles.** Per-tile draw spans 1.3–25.0 W, so a tile count is wrong in both
   directions.
2. **Sources presently *supplying*, not presently *connected*.** A source that is current-limited,
   thermally folded back or sagging contributes its actual deliverable, not its label.
3. **The governor must apply the thermal ceiling as an independent bound, and does not today.** §7.0
   shows it binds below the electrical one in every row: a watts-only governor would **permit** a
   232 W single-pass protocol on a PD 240 W EPR source. **`OI-PWRSRC-12` — the governor takes
   `min(electrical, thermal)`, and the thermal budget is a number that does not exist yet.** Until it
   does, use the conservative **28 W**, marked provisional as `SR-FAN-03`'s constants are.
4. **And a *dose* bound, which §5 shows is a third axis.** A protocol may sit inside both the
   electrical and the instantaneous thermal budget and still accumulate 292 CEM43 over twenty hours.
   §5.7's predicted-dose check is the governor's third comparison, not a separate feature.
5. **The scripts and the runtime must not diverge — D-1.** `check-pbm-power.ts` owns the demand model;
   `check-power-source-coverage.ts` and `check-thermal-dose.ts` import it. **The runtime governor is a
   fourth consumer, in C rather than TypeScript.** That is a real divergence risk and the countermeasure
   is not care: **`OI-PWRSRC-13` — emit `TILE_W`, the overhead constant and the thermal constants from
   one source of truth into both runtimes, with a CI check that the C header and the TS constants
   agree.**

---

## 12. Purchaser-facing selection, and the failure classes

### 12.1 What the buyer must be shown

1. **Demand, not a tier name.** The buyer's question is *"can I run the protocol I came here for?"*
2. **Three states, not two.** `clinical-09` is *indeterminate* under every source (§2.2); a binary
   display must lie about it.
3. **Never present a thermally-bound protocol as source-remediable.** §7.0 says that today *every*
   over-budget protocol is thermally bound, so a *"buy more power to unlock this"* affordance would be
   **false for the entire library as it stands.**

### 12.2 The four classes, and the two prohibitions

| Class | Meaning | What the app may say |
|---|---|---|
| **F1** | Wrong module type fitted | *"This protocol needs a T1-C module."* |
| **F2** | Fitted variant cannot reach the irradiance | *"The fitted module cannot reach this irradiance."* **Never remediable by power** |
| **F3-a** | Electrically bound; a purchasable source clears it | *"This protocol needs N W. A ⟨source⟩ supplies it."* |
| **F3-b** | Thermally or dose bound; no source clears it | *"Commands more simultaneous output than the helmet can dissipate — reduce scope or extend in time."* **Must never name a source** |
| **F4** | **Tier / supervision** | *"This protocol requires clinical supervision."* **Must never name any purchase** |

> **Two prohibitions, both stronger than they look.**
>
> - **An F3 must never be presented as an F2** — F2 is a fitted-hardware fact, F3 a configuration
>   fact; collapsing them tells a buyer to replace a module that is fine.
> - **An F3-b must never be presented as an F3-a, and an F4 must never be presented as any of them.**
>   Given §7.0, **F3-b is the common case and F3-a is currently empty**, so the naive implementation
>   of the purchaser-choice directive would be wrong for every protocol in the library. And F4 is the
>   only class **no purchase the user can make alone will fix** — presenting it as purchasable takes
>   their money for nothing (§6.3).

### 12.3 The data the app needs

| # | Datum | Exists? |
|---|---|---|
| 1 | Per-protocol demand in watts | **Yes** — `check-pbm-power.ts` |
| 2 | Per-source deliverable, less overhead and loss | **Yes** — negotiated at runtime |
| 3 | **The thermal budget as a comparable number** | **No** — `OI-PWRSRC-12`. **Without it F3-a and F3-b cannot be distinguished, and the whole display is blocked** |
| 4 | **The predicted dose** (§5.7) | **No** — `OI-PWRSRC-03` |
| 5 | Minimum source clearing item 1 | Derivable from 1 + 2, once 3 and 4 gate it |
| 6 | Whether the protocol may be derated or time-multiplexed | **No** — `OI-SESPWR-04`'s admissibility field |
| 7 | **Tier requirement, and whether the device holds an entitlement** | **No** — §6.7 |

> **`OI-PWRSRC-14` — the purchaser-facing display is BLOCKED on items 3, 4 and 7.** It is not blocked
> on any hardware. **Shipping it earlier means shipping the F3-b-sold-as-F3-a and F4-sold-as-F3
> defects deliberately.**

### 12.4 One thing the current ladder gets wrong

`check-power-source-coverage.ts` reports that **a 65 W source unlocks zero protocols over 45 W**, and
that **a 500 W mains station unlocks zero over two PD 240 W EPR sinks.** CLAUDE.md §2 sells a 65 W
upgrade at checkout. §14 records the consequence.

---

## 13. What CLAUDE.md §4's power table becomes (proposed; not edited here)

| Mode | Draw | **Min contract** | **Thermal bound** | **Mode 3 (bank) reachable?** | Bank runtime (10,000 mAh) |
|------|------|---|---|---|---|
| Standby | 1 W | 5 V/0.5 A | n/a | Yes | ~330 h |
| EEG only | 2.5 W | 5 V/1 A | n/a | Yes | ~130 h |
| Standard T1 ★ | ~17–20 W | 15 V/2 A (45 W) | **inside** | Yes | ~95–110 min |
| T1 peak | ~45–50 W | 20 V/3 A (65 W) | **at the 28–49 W ceiling — provisional, `OI-PWR-01`** | Yes | ~38–42 min |
| T2 standard | ~44–46 W | 20 V/3 A (65 W) | separate path (1170 nm TEC) | Yes | ~41–43 min |
| T2 peak | ~70–74 W | 20 V/5 A (100 W EPR) | separate path | **Unverified — `OI-PWRSRC-08`** | ~24–27 min |
| **T2 + TMS** | **not specified** | **not specified** | separate path | **No** | — |

**Four changes, each earning its column:** a **thermal-bound** column, because §7.0 shows it binds
below the electrical one and a power table that cannot express its own binding constraint invites the
"buy more watts" reasoning this study rejects; a **Mode 3** column, because *min PD* does not imply a
bank can deliver it (§7.2); a **TMS row that says "not specified"** rather than being absent, so the
gap `NP-PWR-BUDGET-001` §4 found is visible (`NP-CONV-001` §4.0's *a document can be named before it
exists*, applied to a table row); and **provenance on the T2 rows**, since 70–74 W was derived for the
1170 nm laser zone specifically and is cited as an input by five other documents.

**Not proposed:** any change to the draw figures. `NP-PWR-BUDGET-001` §6 routes those to
`OI-PWR-02`/`-03`/`-04`, and this study finds nothing that moves them.

---

## 14. CLAUDE.md §2's charger policy — impact, without touching §2

**§2 is LOCKED and is not modified.** What follows is what it would have to become, raised as
**`OI-PWRSRC-15`**.

### 14.1 The key changes, and that is the whole problem

§2's rule is *"charger scaled to peak draw of configuration."* The direction's rule is *source scaled
to the protocols the purchaser wants.* A **Core** buyer runs no PBM, so nothing above 15 W changes
anything for them; a **Home Standard** buyer has a library spanning **13 W to 1,775 W — a 137× range
inside one configuration.**

> **Configuration is a weak predictor of protocol demand, so a configuration-keyed table cannot express
> a protocol-keyed choice.** The same category error `NP-SES-PWR-001` D-4 identifies in the tile-count
> governor: a proxy that fails as soon as the underlying variable moves independently.

### 14.2 Each mechanism

| §2 mechanism | Under purchaser-chosen sources |
|---|---|
| *"Charger scaled to peak draw of configuration"* | **Becomes two keys** — a *floor* keyed to configuration (the device must always run its fitted hardware) and a *choice* keyed to the protocol set. The floor is what §2 already is |
| **Auto-include at every upgrade by serial number** | **Survives for configuration upgrades; has no trigger for protocol choices.** Fitting a T1-C module is a serial event; deciding to run Vascular Baseline is not |
| **Upfront upgrade as intent signal** ($19 at cost) | **Inverts.** It is a *signal* today precisely because the buyer does not know what it unlocks; told, it becomes a *statement*. **And §12.4 finds a 65 W source unlocks zero protocols, so selling it as a protocol unlock would be false.** Its value as a T2-*intent* signal is unaffected |
| **EU: *"any PD-compliant charger must work"*** | **Survives**, and D-4 preserves it. **But it is also what forecloses power-based tier gating (§6.6a)** — the same sentence protects the buyer and disarms the gate |
| *"power level: reduced… never blocks"* | **Survives and improves**, because §12's class split gives it something true to say |

### 14.3 A question about §2's EU note this study cannot answer

§2's EU note is written against the common-charger regime. **That regime governs charging devices with
rechargeable batteries, and this device has none** — nothing in the box charges anything.

> **`OI-PWRSRC-16` — regulatory counsel, and it cuts both ways.** If the regime does not apply, §2's
> EU note is a self-imposed constraint rather than a legal one, which would widen the options for both
> S-4's connector and §6.7's accessory. If it does apply, it applies to the existing inlet too.
> **No position is taken here**, and nothing in this study depends on the answer: the constraint is
> worth keeping on its architectural merits regardless (§8.3, §9.2).

---

## 15. UHDR / SHDR classification

| Datum | Class | Reasoning |
|---|---|---|
| PD contract negotiated (V, A) | **SHDR** | Existing. Device capability |
| Per-port lifetime insertion counter (unsigned, no timestamps) | **SHDR** | Matches the existing USB-C insertion counter |
| Which source **types** the device has seen | **SHDR** | Device configuration — a purchase fact |
| **Per-session source record** (which, when, how long) | **UHDR** | *"Session timestamps → UHDR"* is explicit. A per-source usage record with durations **is** a record of when a person used the device |
| **Per-source session count** (unsigned, no timestamps) | **UHDR — the finding** | §15.1 |
| **Predicted or accumulated CEM43 per session** | **UHDR** | A cumulative injury-risk dose for a wearer is a health record about that person, on the same footing as PBM dose per zone, which §5 of CLAUDE.md already puts in UHDR |
| Count of thermal-limit trips (unsigned, no timestamps) | **SHDR** | The device-session-count precedent exactly |
| Thermal or brownout **interlock** trip → safety interlock log | **SHDR** | The locked *"safety interlock log → SHDR"* rule; timing discarded per `np_log_shdr_fault()` |
| **T2 entitlement redemption** (§6.7) | **SHDR** as a pass/fail count; **UHDR** if per-session and timed | *"Accessory authentication pass/fail"* is already SHDR. A timed per-session record is a session timestamp |

### 15.1 A per-source session count is a protocol-class proxy, and therefore UHDR

The obvious reading is that a count of mains sessions is device-health data. **The library's own shape
makes that wrong:** only protocols above ~470 W need a source beyond USB-C, and **exactly one protocol
is above 500 W** — `Vascular Baseline`, at 1,600 W.

> **A nonzero "sessions on the 1,800 W source" counter is very nearly a flag reading *this user runs
> Vascular Baseline*.** CLAUDE.md §5 puts *"protocol parameters used"* in UHDR unconditionally, and a
> field that identifies the protocol is that field by another route.
>
> **General rule, worth stating because it will recur: a per-source usage record is a low-cardinality
> projection of the protocol library, and its identifying power is set by how many protocols share a
> rung — not by anything about the source.** Where a rung holds one protocol, the record is that
> protocol's name. **`OI-PWRSRC-17`.**

**Recommendation:** SHDR carries **per-port lifetime insertion counts and PD contract logs only**, and
**no per-session, per-source record of any kind.** If a device-health signal about source stress is
needed, log a coarse cumulative energy figure with no source attribution — the
`anonymization_failed: bool`-without-the-stage precedent.

### 15.2 The conditional-redaction rule, which the obvious design violates

The 2026-08-12 rule: **a redaction applied conditionally on a sensitive predicate leaks that
predicate.** The natural implementation fails it exactly:

> *"Log the source type for every session, but suppress it for high-power sessions to protect protocol
> privacy."*

That makes `source_type == suppressed` a **self-interpreting one-bit flag for a high-power protocol** —
the `count > 0 && tick_ms == 0` cardiac-oracle shape, and worse than no redaction for the same reason:
the redaction pattern needs no correlation work, while the raw value would.

> **One fixed-shape marshaller, no field whose presence or value depends on the session's protocol,
> power class or tier**, on the `np_fault_latch_build_report()` precedent. **The correct design is not
> to redact the field conditionally; it is not to have the field.** The same applies to §6.7's
> entitlement: an SHDR record that appears only for T2-entitled sessions is a tier oracle.

---

## 16. IEC 62304 classification, argued architecturally

| Processor | Class | Can it see a rail? |
|---|---|---|
| i.MX RT1062 main | **B** | Negotiates PD, so it knows the contract. Owns the session runner and the governor |
| STM32G071 safety MCU | **C**, bare-metal, ~1,600 lines / 9 modules | **Owns every stimulation enable GPIO and has no representation of available power** (`OI-PWR-13`) |

**The tempting design puts arbitration in the safety MCU, and it is the wrong call.** PD contract
negotiation is a **stateful protocol** with timers, retries and vendor quirks; putting it in a
bare-metal Class C partition of ~1,600 lines is a large proportional increase, and every line is
Class C forever. And **arbitration must work when software does not** (§10.1), which hardware ORing
satisfies strictly better than software can.

| Element | Class | Where |
|---|---|---|
| Ideal-diode ORing, inrush, priority | **Hardware** — not software at all | Hub / base station |
| PD negotiation, budget summation, the governor, predicted dose | **B** | Main processor |
| **`PWR_ADEQUATE`** — one latched, fail-closed input asserted by hardware comparators when the summed rail supports the enabled stimulation set | **C** | Safety MCU, gating the existing enables |
| **Cumulative time-at-limit counter** (§5.7) | **C** | Safety MCU — a counter and a comparison, no model |
| **T2 entitlement redemption** (§6.7) | **C** to gate, **B** to present | Safety MCU owns the enables, so it must own the refusal — the `NP-FW-BENCH-001` split |

> **The total new Class C surface is: one latched input, one counter, one credential check.** No new
> Class C module; extend the existing one. **Each is a comparison against a measured or asserted
> value, never a model** — which is deliberate, because §5.2's constants are provisional and Class C
> is where provisional constants go to be frozen.

**One consequence worth stating:** `PWR_ADEQUATE` must be **latched**, not level-sensitive. A supply
oscillating around the threshold would otherwise chatter the enables, converting a brownout into
repeated uncontrolled starts and stops — worse than one clean halt. Clearing the latch takes the same
deliberate re-enable path as any other safety fault.

---

## 17. Cost

Costed against `NP-COST-001` Rev 2's own model — A-1 populations, A-2 full-L1, A-3 per-configuration
BOM→COGS multipliers — so the figures compose with §2.1 rather than sitting beside it. **All four T1
configurations are gross-margin negative before this study starts**, so a new part is not a rounding
error.

### 17.1 The recommended set costs nothing

| Recommendation | BOM delta |
|---|---|
| **T1: source set unchanged (S-1, S-3)** | **$0.00 at every configuration** |
| T2 mains path (S-4) | **$0.00 BOM** — an accessory, not a BOM line (§17.3) |
| **F4 tier gate by device-bound entitlement (§6.7)** | **$0.00** — firmware plus key management, no part |
| Thermal-dose check (§5.7) | **$0.00** — reuses the PD2 NTC the design already has |

> **A costed study whose recommendation costs nothing is a result, not an evasion.** The recommended
> architecture is the one already shipping, because no other source changes what the assembly can
> spend, and the tier gate that works is the one with no hardware in it. The alternative was a part
> that buys watts the helmet cannot use, on configurations already margin-negative.

### 17.2 What a second EPR PD sink *would* cost — sensitivity, not recommendation

**A finding first, and it is a gap in the cost model:**

> **The existing USB-C PD sink has no BOM line anywhere in the document set.** No sink controller, no
> receptacle, no ORing or inrush part appears in `NP-COST-001`, `NP-HW-HUB-001` or `NP-DRV-SHELL-002`,
> and no PD silicon is named anywhere in the tree. §2 costs the **charger** ($3–26); §2.1 costs the
> BOM; **the sink between them is uncosted**, so a second-sink delta cannot be derived by doubling
> anything. `OI-PWRSRC-18`.

Estimated from part classes. **Order-of-magnitude engineering estimates on the same footing
`NP-PWR-BUDGET-001` §4.1 gives its TMS energies — not quotes, and not design inputs until EE confirms:**

| Line | Estimate |
|---|---:|
| USB-C receptacle, 5 A rated, sealed | $0.60 – 1.20 |
| EPR-capable PD sink controller | $1.10 – 2.20 |
| Ideal-diode ORing controller + 2 FETs | $1.40 – 2.60 |
| Input TVS + common-mode choke + π filter (Layer 5 parity) | $0.80 – 1.60 |
| Hot-swap / inrush controller | $0.35 – 0.90 |
| Current sense + telemetry | $0.45 – 0.90 |
| **Total** | **$4.70 – 9.40** — **$7 mid** |

**GM% movement, using `NP-COST-001` A-3's own multipliers on a $7 BOM delta:**

| Config | Multiplier | COGS delta | Retail | GM% now | With the part | **Movement** |
|---|---:|---:|---:|---:|---:|---:|
| Core — EEG only | 1.537 | +$10.76 | $449 | −23 % … −45 % | −26 % … −47 % | **−2.4 pt** |
| Home Lite | 1.397 | +$9.78 | $599 | −50 % … −64 % | −51 % … −66 % | −1.6 pt |
| Home Standard ★ | 1.333 | +$9.33 | $849 | −41 % … −51 % | −42 % … −52 % | −1.1 pt |
| Home Premium | 1.352 | +$9.46 | $1,199 | −7 % … −14 % | −8 % … −15 % | −0.8 pt |
| Pro Entry | 1.639 | +$11.47 | $4,999 | +50 % … +52 % | +49.8 % … +51.8 % | −0.2 pt |
| Pro Full | 1.745 | +$12.22 | $13,999 | +73 % | +72.5 % … +73.3 % | −0.1 pt |

> **The cheapest configuration pays the most for the part, by a factor of twenty-four over Pro Full.**
> Core carries the highest multiplier (1.537) *and* the lowest retail ($449), so a $7 part costs it
> **2.4 gross-margin points.** That is the general shape of any part added to this line, and it is
> worth stating because intuition runs the other way.

Every figure inherits `NP-COST-001`'s floor status: term **U** excluded, `OI-COST-01` and `OI-COST-05`
open, `OI-COST-10` making `OI-HEXTILE-06` a precondition on any pricing action.

### 17.3 The base station and the entitlement, compared on cost

| | **S-4 base station** | **§6.7 device-bound entitlement** |
|---|---:|---|
| Certified medical-grade AC/DC, 500 W class | $180 – 320 | — |
| DC cable + locking connector (or PD source stage) | $12 – 25 | — |
| ORing / inrush / telemetry board | $18 – 35 | — |
| Enclosure, dock, cosmetics | **not costed** — needs tooling volume, which needs `OI-PWR-11` | — |
| **Unit cost, excl. enclosure and NRE** | **$210 – 380** | **$0.00** |
| Regulatory surface added | **NeurOne owns a mains isolation barrier**, `VE-11` scope grows | **None** |
| Ongoing | Service, spares, a failure mode that ends a clinic session | Key management |

**Two things this changes about the T2 commercial picture, pointing opposite ways:**

- **It fits the T2 margin without strain.** Pro Full is +$10,163/unit at price in force; $210–380 is
  2–4 % of that.
- **It is not free, because §2 already ships Pro Full two 65 W chargers at $26.** If `OI-PWR-11`
  resolves that those were the dual-inlet architecture, the base station **replaces** them and the
  delta is $184–354; if the second brick is a spare, it is purely additive. **`OI-PWR-11` moves this
  figure by $26 and the architecture by much more** — which is why `NP-PWR-BUDGET-001` §4.4.2 says to
  resolve it first, and this study agrees.

---

## 18. Decisions

**None is locked; all are proposals for design review.** Five are marked for principal decision.

| ID | Decision | Rationale | Reversible? |
|---|---|---|---|
| **D-1** *(principal)* | **The T1 helmet keeps a single USB-C PD inlet. Do not build a mains path for T1.** | §4: the sealed cavity supports 28–49 W and the existing brick already delivers 40 W. §7.0: no candidate source changes coverage at all | Yes — reversible on evidence, via `OI-PWR-01` |
| **D-2** | **`OI-PWR-01`'s multi-tile CFD stays the next step, and its purpose inverts:** sequence it because the current ceiling may be **optimistic and unsafe**, not because it may be pessimistic and expensive | §4.3: it can move a 4–8 tile estimate; it cannot move 28–49 W to 1,792 W. The asymmetry makes it a safety item | Yes |
| **D-3** | **A coverage figure is a necessary condition, never a capability claim.** Every table stating what a source "unlocks" carries the thermal bound beside it | §7.0. Without this a true electrical statement reads as a false capability statement | Yes — a documentation convention |
| **D-4** *(if S-4 is built)* | **Mains hardware lives in a separate attached base station, and its output is a USB-C PD source** | §8: keeps mains entry, the barrier, the converter and its heat outside the measured shielded volume. One inlet type is one governor input, one filter qualification, one arbitration case. §6.7 reaches the same conclusion from the tier direction | Yes |
| **D-5** | **`NP-PWR-BUDGET-001` §4.4.1's isolation row is decisive about the wrong variable — corrected. Its conclusion is retained for different reasons** | §9.1: isolation ownership follows **mains entry**, not connector form. The recommendation survives on the Layer 5 qualification, arbitration simplicity, the EU stance, and §6.7 | Yes |
| **D-6** *(principal / safety)* | **Arbitration in hardware. The safety MCU gains three small Class C elements: `PWR_ADEQUATE` (latched), a time-at-limit counter, and the entitlement check** | §16: PD negotiation is stateful protocol code and would be Class C forever; arbitration must work when software does not; and each new element is a comparison against a measured or asserted value, never a model | Yes, but the classification is the point |
| **D-7** | **A source *change* is not in the module-change class; a source *loss* is a fault, not a change** | §10.2: a module change alters what is connected to the patient; a source change alters what feeds it. A power cut mitigates the first and *causes* the second | Yes |
| **D-8** *(new, from §5)* | **The design needs a cumulative thermal-dose check, because the interlock caps temperature and says nothing about duration at that temperature.** Predicted dose Class B at admission; a measured time-at-limit counter Class C as backstop | §5.5: cascading — proposed as the remedy for the *power* problem — puts three of twenty protocols past 40 CEM43 in a single session, and nothing looks at it. §5.3/§5.4 are clean nulls, so the check is cheap to satisfy | Yes |
| **D-9** *(principal)* | **Each failure class gets the mechanism that matches its remedy. Power gates F3 — it already does. F4 takes a device-bound signed entitlement, not a power limit and not a proprietary supply** | §6.5: total watts rank-correlate 0.849 with irradiance but only 0.582 with dose, and any cap above 200 W passes the library's highest-dose protocol. §6.6: §2 is locked and makes power advisory. §6.7: the entitlement wins on five of seven axes and adds no regulatory surface | Yes |
| **D-10** | **The governor's budget is the summed contract of sources *presently supplying*, recomputed on every arbitration event, raised only at a protocol boundary, bounded by `min(electrical, thermal, dose)`** | §11. Raising mid-stimulus silently changes delivered dose inside a session the protocol did not declare | Yes |
| **D-11** | **Four failure classes, two prohibitions: no F3 presented as F2; no F3-b presented as F3-a; no F4 presented as any of them** | §12.2. F3-b is currently every case and F4 is fixable by no purchase the user can make alone — the naive implementation would be wrong for the whole library and would take money for nothing | Yes |
| **D-12** *(principal)* | **CLAUDE.md §2's charger key changes from *configuration* to *configuration floor + protocol-set choice*. Raised, not changed** | §14.1: one configuration spans a 123× demand range | Yes |
| **D-13** | **No per-session, per-source record reaches SHDR.** Per-port lifetime counters and PD contract logs only | §15.1: a per-source session count is a low-cardinality projection of the protocol library, and at the top rung it names one protocol | Yes, but the classification is the point |
| **D-15** *(principal)* | **Do not buy an IEC 60601-1 isolation barrier to obtain a tier gate.** The power gate's benefit set is empty after §7.0: zero net protocols unlocked, and none of the thirteen risky ones. **The decision was correct on its stated premise; the premise did not survive §4** | §6.11. What the direction wants is delivered by §6.7's device-bound entitlement at $0.00 BOM, no new regulatory surface, and with the duration and dose axes a supply cannot see | Yes |
| **D-16** | **Information for safety sits on top of the hardware interlocks, never instead of them.** No control is relaxed because a clinician is assumed present. Per-protocol clinician acknowledgement carries the most duty; the $1,800/yr annual calibration visit is the re-acknowledgement point; **CLAUDE.md §6.1's decision-support document is the wrong channel and must not be used** | §6.12. ISO 14971 ranks information for safety last, and §6.4 shows the device cannot verify the supervision the transfer assumes. §6.1's document is a consent artefact answering one question well | Yes, but the ranking is the point |
| **D-14** *(principal)* | **Device tier is a proxy for clinical supervision and must be stated as one.** The device cannot detect a professional; a Pro device used alone passes, a supervised T1 user fails | §6.4. The same discipline the programme already applies to `sLORETA` reachability and to zone-vs-site claims | Yes, but it is a claims rule |

---

## 19. Risk rows

In the shape of `NP-RISK-003` / `NP-RISK-004` §2. **Proposed rows, not accepted ones** — acceptance is
an owner-signed action on the owning register.

| ID | Sev | Hazard | Cause | Consequence | Control | Owner | Status |
|---|---|---|---|---|---|---|---|
| **RISK-PWRSRC-01** | **High** | Stimulation continues into a collapsing rail after the last source is removed | No battery; the 22 F supercapacitor is specified for duty-cycle transients, not hold-up; no hold-up requirement exists | Uncontrolled termination of an active tDCS/tACS/VNS output, bypassing the hardware-enforced 30 s ramp | `PWR_ADEQUATE` latched Class C input (D-6) + a **passive** discharge path independent of the rails. `OI-PWRSRC-11` must dimension the hold-up first | Safety + EE | **OPEN — BLOCKING for `OI-PWR-13`** |
| **RISK-PWRSRC-02** | **High** | A protocol is permitted that the assembly cannot thermally sustain | Governor checks watts against contract only; the thermal budget is not a computable number anywhere | Scalp reaches the applied-part limit during an otherwise valid session, on a source the buyer was told would run it | `min(electrical, thermal)`, thermal bound provisionally the **conservative 28 W** | Firmware + Thermal | **OPEN — `OI-PWRSRC-12`** |
| **RISK-PWRSRC-03** | **High** | Cumulative thermal dose accumulates unbounded at the interlock ceiling | The 42 °C interlock caps temperature and has no duration input; a cascaded protocol runs for hours at the ceiling — 292 CEM43 in one session | Thermal injury from a session in which every real-time interlock functioned correctly throughout | D-8: predicted-dose check at admission (B) + measured time-at-limit counter (C). `OI-PWRSRC-01` | Safety + Thermal | **OPEN — the §5 finding** |
| **RISK-PWRSRC-04** | Medium | Measured EMF shielding degraded by a new power inlet | Switched high current inside the occipital-arch assembly; Layer 5 qualified for USB-C only | The primary **measured** technical claim fails at qualification, after tooling | Mains outside the shielded volume (D-4); any new inlet bench-verified, not argued | EE + EMC | **OPEN — inherits `OI-PWR-12`** |
| **RISK-PWRSRC-05** | Medium | Buyer purchases a source that unlocks nothing | F3-b presented as F3-a, or F4 presented as F3; a 65 W upgrade sold as a protocol unlock when it unlocks zero | Money taken for a purchase that cannot fix the user's problem; consumer-protection exposure | D-11's class split; display blocked until items 3, 4 and 7 exist (`OI-PWRSRC-14`) | Product + Legal | **OPEN** |
| **RISK-PWRSRC-06** | Medium | Uncontrolled inrush on hot-plug into the 22 F supercapacitor | No inrush limiting specified for a second inlet | Connector damage, nuisance shutdown, and a conducted-emission event inside the shielded assembly | Per-inlet hot-swap controller with soft-start (§10.4) | EE | **OPEN** |
| **RISK-PWRSRC-07** | Medium | Chattering stimulation enables during a sagging supply | `PWR_ADEQUATE` implemented level-sensitive rather than latched | Repeated uncontrolled starts and stops — worse than one clean halt | Latch with a deliberate re-enable path (§16) | Safety FW | **OPEN** |
| **RISK-PWRSRC-08** | **High** | A per-source SHDR record identifies the wearer's protocol, and therefore indication | One protocol occupies the top source rung; source usage is a low-cardinality projection of the library | UHDR content reaches an SHDR store NeurOne can read — a boundary breach, not a leak of degree | D-13: no per-session per-source record; fixed-shape marshaller, no conditionally-present field (§15.2) | Privacy + FW | **OPEN — `OI-PWRSRC-17`** |
| **RISK-PWRSRC-09** | **High** | A supervision-gated protocol runs unsupervised, or a clinician is blocked from one | Device tier is the only proxy the device can evaluate, and it is loose in both directions | The stated justification for tier gating — professional monitoring — is not delivered by the mechanism that implements it | D-14: state the proxy as a proxy; `OI-PWRSRC-04` on whether a per-session assertion is wanted | Product + Clinical | **OPEN** |
| **RISK-PWRSRC-10** | Medium | A T2-gated protocol is bypassed by an app that sets the flag itself | `NP_PROTO_FLAG_T2_TIER` is app-computed and hub-ignored; a gate the app computes and the hub trusts is not a safety gate | Injury-risk protocol runs on an unentitled device | Device-bound signed entitlement redeemed in the safety MCU (§6.7, D-9), never an app flag | Safety FW | **OPEN — no mechanism exists today** |
| **RISK-PWRSRC-11** | Medium | A protocol silently under-doses because the thermal interlock throttled it | Eight cascaded protocols imply face temperatures above 42 °C; the interlock clamps, and nothing in the app, compiler or session record reports that it did | The protocol does not deliver its evidence-grade dose, and neither user nor clinician is told — *"under-dosing, not mechanism failure, explains most nulls"* | Surface throttle events in the session record and to the user; §5.7's predicted-dose check would catch it at admission | Firmware + Clinical | **OPEN — `OI-PWRSRC-21`** |
| **RISK-PWRSRC-12** | Medium | Duty for a gated protocol's hazard is transferred to a person with no channel to reach the person it lands on | The wearer is neither purchaser nor warranty owner; the programme has a consent surface for the wearer and a telemetry surface for the warranty owner, and no safety-information surface for either | The hazard information reaches the clinician and never the wearer | D-16's channel ranking; a distinct, stealth-proof annunciator state for a gated protocol (§6.12) | Product + Clinical | **OPEN — `OI-PWRSRC-23`** |

**Verification map** (`NP-RISK-004` §3 form) — **four of ten have no verification defined**, which is
itself the finding:

| Risk | Verified by |
|---|---|
| RISK-PWRSRC-01 | **None defined.** Needs a hold-up bench measurement that does not exist — `OI-PWRSRC-11` |
| RISK-PWRSRC-02 | Extends `OI-PWR-01`'s CFD + a governor unit test against the committed demand model |
| RISK-PWRSRC-03 | `scripts/check-thermal-dose.ts` in CI (flag-for-review) + THERM-1b bench once `OI-PWR-01` lands |
| RISK-PWRSRC-04 | EMI bench with EMF-1, per `OI-PWR-12`. **Analysis is not admissible** — the claim is measured |
| RISK-PWRSRC-05 | **None defined.** Needs an HFE formative on the purchase-time display |
| RISK-PWRSRC-06 | Bench: hot-plug inrush into a charged and a discharged supercapacitor |
| RISK-PWRSRC-07 | Bench: ramp a source down through the threshold; confirm one clean halt |
| RISK-PWRSRC-08 | CI assertion that no SHDR writer takes a source argument — the `firmware/shdr/tests/` precedent |
| RISK-PWRSRC-09 | **None definable by the device.** That is the content of the risk |
| RISK-PWRSRC-10 | **None defined.** Needs the entitlement mechanism to exist first (§6.7) |

---

## 20. Open items

Fresh family, append-only, never renumbered — `NP-CONV-001` §4.

| ID | Description | Owner / Blocking |
|---|---|---|
| **OI-PWRSRC-01** | **Cascading converts the power problem into a thermal-dose problem, and nothing checks it.** `NP-SES-PWR-001` §4 proposes cascading as the remedy for over-budget protocols; §5.5 finds three of twenty cascaded protocols past 40 CEM43 in a **single** session (Vascular Baseline 292), because the 42 °C interlock caps **temperature** and has no **duration** input | **Safety + Thermal. The §5 finding.** Sequence with `OI-SESPWR-04` — a cascade primitive must carry a dose bound as well as an admissibility flag |
| **OI-PWRSRC-02** | **Source CEM43 injury thresholds for scalp skin.** §5's isoeffect arithmetic is exact; the reference lines it prints (2, 40) are literature values and **this study asserts neither**. The derived quantity is the dose | Clinical. Blocks any pass/fail use of `check-thermal-dose.ts` |
| **OI-PWRSRC-03** | **Specify the runtime dose gate**: predicted dose at protocol admission (Class B, main processor) and a measured cumulative time-at-limit counter (Class C, safety MCU). **The counter needs no model**, which is why it is the Class C half (§5.7) | Firmware + Safety. Follows `OI-PWRSRC-02` |
| **OI-PWRSRC-04** | **Device tier is a proxy for clinical supervision, and the device cannot evaluate the thing itself.** A Pro device used alone passes; a supervised T1 user fails. Nothing in-tree is per-session — the service contract, clinic warranty registration and multi-patient dashboard all evidence a relationship at purchase, not at a session. Decide whether a per-session supervision assertion is wanted at all; `NP-FW-BENCH-001`'s physical-assertion pattern is the obvious shape (§6.4) | Product + Clinical. Feeds D-14 |
| **OI-PWRSRC-05** | **`check-thermal-dose.ts` is flag-for-review, not a gate, until `OI-PWR-01` lands.** Its constants are named exports precisely so the verification-grade CFD can replace them without rewriting the check. **No constant in it is a derived figure.** **RE-POINTED 2026-09-03 by `NP-THERM-CFD-N1-001` §10: it stays flag-for-review, but it now follows `OI-R1-02` (THERM-1b bench correlation), not `OI-PWR-01`.** The N-tile study re-derives the *network*, not the *tissue* constants (CEM43 reference, perfusion bounds, ambient sensitivity) this check actually carries — those still need bench correlation. Add `R_sink` to the constants it must carry once `OI-N1-02` closes | Thermal. **Follows `OI-R1-02`**, not `OI-PWR-01` |
| **OI-PWRSRC-06** | **Do not nominate protocols for T2 gating until `OI-SESPWR-01` resolves.** Most high-demand protocols are over-scoped rather than intrinsically high-power (`clinical-04` irradiates 37 sockets for a bilateral-DLPFC indication). **Tier-gating a scope defect freezes it into the product structure**, and it is a data edit to fix (§6.10) | Clinical + Product. **Sequencing item, and it binds** |
| **OI-PWRSRC-07** | **The transient thermal ceiling could justify buying one rung.** τ_face 35–45 min against 6–30-min sessions means ~39 % of steady rise is reached, implying ~70–125 W transiently for one isolated session. **Would make the 92–132 W rung defensible (4–6/23); does not rescue mains.** Caveats: τ was derived for the face lumped mass in the fan-off case, not the cavity, and §5.4's carry-over is real though mild. **REFUTED 2026-09-03 — `NP-THERM-CFD-N1-001` §7 (N1-D-5), recommended CLOSED as NOT SUPPORTED.** This row's own caveat is the defect: τ_face 35–45 min is R1 §4's **fan-off** value, built on `R_in ∥ R_out(off)` = 0.087 m²K/W. In the healthy state the via is in parallel too, giving **0.0056** — ~15× lower — so the module equilibrates in ~3 min and the slow node is the **scalp**, not the module. Measured on the N-tile model, a 30-min session reaches **75–91 % of steady rise, not 39 %**; ~39 % is approached only by a 6-min session at N = 80. **No transient allowance may be spent on a power rung.** Its one durable point: the credit grows with N, so any allowance must be a function of N and session length, never a flat multiplier | Thermal. Recommended CLOSED. **Does not gate S-2 or `OI-PWRSRC-12` in the direction it proposed** |
| **OI-PWRSRC-08** | **What PD contract can a power bank actually hold, for a full session, at end of charge?** CLAUDE.md §4 defines Mode 3 as running *"from any USB-C PD power bank"* and gives no envelope. **Mode 3 is CLAUDE.md §1's primary competitive moat, and its width is undocumented** (§7.2) | Product + EE. Blocks §13's Mode 3 column |
| **OI-PWRSRC-09** | **The 24 V emitter rail's converter topology inverts at EPR** — 20 V in is a boost, 48 V EPR is a buck, resized ~40 W → ~232 W with ~19 W of loss inside the hub (§7.3) | EE. Gates S-2 |
| **OI-PWRSRC-10** | **Is an IEC 62368-1 commodity PD brick adequate for a device with conductive applied parts under IEC 60601-1?** *"The certified charger is the barrier"* does not identify which standard certified it. **This applies to the inlet that ships today**, and CLAUDE.md §2's *"any PD-compliant charger must work"* is the stance under test (§9.3a) | **Regulatory + EE. BLOCKING for `VE-11`** |
| **OI-PWRSRC-11** | **Dimension the hub's hold-up.** No battery; the 22 F supercapacitor is specified for duty-cycle transients, and no controlled document gives its rail voltage or usable depth of discharge. CLAUDE.md §3's hardware-enforced **30 s tDCS ramp** is certainly longer than any plausible hold-up (§10.1) | **Safety + EE. BLOCKING for `OI-PWR-13`.** Pre-exists multi-source |
| **OI-PWRSRC-12** | **The thermal budget must become a number the governor can read.** §7.0 shows it binds below the electrical budget in every row. Until it exists, use the conservative **28 W**, marked provisional as `SR-FAN-03`'s constants are | **Firmware + Thermal. BLOCKING for `OI-PWRSRC-14`** |
| **OI-PWRSRC-13** | **One source of truth for `TILE_W`, the overhead constant and the thermal constants, emitted into both the TypeScript audits and the C runtime, with a CI check that they agree.** Three committed scripts and a future hub governor read the same model in two languages | Firmware + CI. `NP-CONV-001` §8 |
| **OI-PWRSRC-14** | **The purchaser-facing display is blocked on the thermal budget, the predicted dose and the tier/entitlement state** (§12.3 items 3, 4, 7). Not blocked on hardware. **Shipping earlier means shipping the F3-b-as-F3-a and F4-as-F3 defects deliberately** | Product + App. **Blocked, not pending** |
| **OI-PWRSRC-15** | **CLAUDE.md §2's charger policy is keyed to configuration and the direction keys it to protocol set.** One configuration spans a 123× demand range. Auto-include-by-serial has no trigger for a protocol choice; the intent signal inverts; and **the EU sentence that protects the buyer is the same one that disarms power-based tier gating** (§14, §6.6a). **§2 is locked and untouched** | **Principal.** D-12 |
| **OI-PWRSRC-16** | **Does the EU common-charger regime apply to a device with no battery?** Nothing in the box charges anything. If it does not apply, §2's EU note is self-imposed rather than legal — widening the options for S-4's connector and §6.7's accessory. If it does, it applies to the existing inlet too. **No position taken**, and nothing here depends on the answer | Regulatory counsel |
| **OI-PWRSRC-17** | **A per-source session record is UHDR, not SHDR.** Only one protocol occupies the top source rung, so a nonzero count there is very nearly that protocol's name — and *"protocol parameters used"* is UHDR unconditionally. **General rule: a per-source usage record is a low-cardinality projection of the protocol library** (§15.1) | Privacy + FW. Binds before any source telemetry is designed |
| **OI-PWRSRC-18** | **The existing USB-C PD sink has no BOM line anywhere in the document set** — no sink controller, no receptacle, no ORing or inrush part, and no PD silicon named anywhere in the tree. §2 costs the charger; §2.1 costs the BOM; the sink between them is uncosted, so a second-sink delta cannot be derived by doubling anything. §17.2's figures are part-class estimates, **not design inputs** | EE + Finance. Feeds `NP-COST-001` |
| **OI-PWRSRC-19** | **`NP_PROTO_FLAG_T2_TIER` is declared and unenforced, and so is `NP-NPPS-REF-001` §4's `intensity_milliamps` 0–1 (T1) / 0–4 (T2) split** — the validator applies `maxIntensityMilliamps` at scope `'global'` with no tier term. **Two declared-but-unenforced tier mechanisms.** Specify the F4 gate (§6.7) rather than adding a third | Firmware + App. Follows `OI-PWRSRC-06` |
| **OI-PWRSRC-20** | **`NP-PWR-BUDGET-001` and `NP-SES-PWR-001` are not registered in `NP-DHF-001`** — zero occurrences of either serial, against §5's own rule that the index *"must be updated before the new document is released."* Not backfilled here: registering another author's document is an owner-signed QMS action | Quality. Design-control traceability || **OI-PWRSRC-21** | **A throttled protocol under-doses silently.** Eight cascaded protocols imply face temperatures above the 42 °C limit; the Path B1 interlock clamps them, so the safety outcome is correct and **the efficacy outcome is not reported anywhere** — not in the app, not in the compiler, not in the session record. §5.5.1. The programme's own lesson is *"under-dosing, not mechanism failure, explains most nulls"* | Firmware + Clinical. Same silent-substitution objection `NP-SES-PWR-001` §3 makes for zone scope |
| **OI-PWRSRC-22** | **The power-based tier gate was decided against a coverage ladder §7.0 has invalidated, and its benefit set is now empty.** Net protocols unlocked by a T2 mains accessory after the cavity ceiling: **zero**. Risky protocols it would gate: **zero of thirteen** — they are already reachable on a stock Home Standard by cascading, because they are risky by *duration at the ceiling*, not by draw. **Cost of proceeding: NeurOne owns an IEC 60601-1 isolation barrier at the 510(k) tier with `VE-11` Open, plus a $210–380 accessory whose failure ends a clinic session.** §6.11 | **Principal. The mechanism is sound; the premise moved.** Recommend D-15 |
| **OI-PWRSRC-23** | **There is no wearer-facing safety-information channel.** The wearer is neither purchaser nor warranty owner, and CLAUDE.md §6's own invariant is that a clinic registering warranty has not consented for any patient. Gating a protocol transfers duty to a clinician and leaves the wearer with nothing. **CLAUDE.md §6.1's decision-support document is a consent artefact and is the wrong vehicle** (§6.12). The amber in-use annunciator is the one channel that already reaches the room | Product + Clinical. Follows D-16 |


---

## 21. Cross-references

`NP-PWR-BUDGET-001` §3.2 (the aggregate estimate §4 restates in watts), §3.3 (export efficiency is the
lever), §3.4 (efficacy floor), §3.6 (whole-vault mode — the low-irradiance counter-example in §6.5),
§4.1 (the TMS estimate, and the precedent for labelling an unsourced engineering figure), §4.4 (the
second-inlet assessment this study tests), §4.4.1 (the isolation row corrected by §9), §4.4.2
(`OI-PWR-11`, resolve first — §17.3), §4.4.4 (the conclusion confirmed and widened by §4.3), **D-4**
(the governor in watts — §11), `OI-PWR-01` (now carrying `OI-PWRSRC-05`/`-07`), `OI-PWR-08` (the
model-validity bound §4.2 honours), `OI-PWR-12` (EMF bench — §8.1), `OI-PWR-13` (arbitration — §10) ·
`NP-SES-PWR-001` **D-1** (the audit is a committed script — §1, §5.2), §2.1 (per-tile draw 1.3–25.0 W),
§2.3 / `OI-SESPWR-02` (the possible R-4 CW breach on the one protocol that would justify mains — §2.3),
§2.4 / `OI-SESPWR-03` (the 4× ambiguity, in heat as in power — §3.3, §5.6), §3 / `OI-SESPWR-01`
(lobe-scale zones: the demand-side fix that dominates every supply-side one, and the reason §6.10 does
not nominate protocols), §4 / `OI-SESPWR-04` (cascading — **the mechanism §5.5 finds carries an
unchecked dose**, and the admissibility field §10.3 reuses) · `NP-THERM-CFD-R1-001` §2 (Path A
rejected; the junction regulates the wrong node), §3 (`q_inward`, `R_face→core`, `SR-FAN-03`), §4
(τ_face — §5.2, §5.4, `OI-PWRSRC-07`), §5 (BN-boss export, the 11.3 °C margin, the 90 % export
fraction, the 30.7 °C calibration point — §4.1, §5.2) · `NP-DT-001` DI-REG-01, DI-SAFE-08, DI-SAFE-13,
VE-11 (Open — `OI-PWRSRC-10`), RISK-26 · `NP-ENV-OPRANGE-001` (the ambient gate §5.6 finds to be the
most load-bearing thermal control in the design) · `NP-REQ-FANHEALTH-001` SR-FAN-01…06 ·
`NP-HW-HEXTILE-001` §9.1/§9.2, R-4/R-5/R-6, `OI-HEXTILE-02`, `-20`, `-21` (the η_wp wall — the
canonical F2), `OI-HEXTILE-09` (the governor) · `NP-COST-001` §2 A-1/A-2/A-3, §4 ($11.53/tile,
$346/headset), `OI-COST-01`/`-05`/`-08` (which §6.9 feeds)/`-10` · `NP-TOOL-HUB-001` §2 (hub PCB at
the occipital arch), §3 F-02 / HUB-MDR-04/05, FAI-HTOOL-02 · `NP-FW-BENCH-001` (the device-bound
credential redeemed against a physical assertion — the shape §6.7 adopts) · `NP-FW-POE-001`
(MCU-table-authoritative `min()`) · `NP-NPPS-REF-001` §4 (`intensity_milliamps` 0–1/0–4 — the
unenforced tier precedent) · `NP-FW-HD-001` §2.3 (`NP_HD_TARGET_DEPTH_SURFACE`/`_DEEP` — the
reachability precedent behind D-14) · `NP-RISK-003` / `NP-RISK-004` (row and verification-map shape) ·
`NP-CONV-001` §4, §8 · `firmware/hub_control/include/np_hub_types.h:89,193`
(`NP_PROTO_FLAG_T2_TIER`) · `app/web/src/lib/hubCompiler.ts:158,333` (`T2_MODALITY_TYPES`) ·
`app/web/src/lib/protocolValidator.ts` (`maxIntensityMilliamps` at scope `'global'`) · CLAUDE.md §1
(wired-first USB-C, Mode 3, measured shielding, the two-tier structure §6.9 strengthens), §2 (charger
policy LOCKED — §14; the margin-negative configurations §17 costs against; `OI-COST-08`), §3 (the 30 s
tDCS ramp, the 40 µC/cm² limit, the sLORETA reachability rule), §4 (power table — §13; Layer 5; the
safety architecture §16 extends; no battery row), §5 (UHDR/SHDR — §15) ·
`docs/reference/regulatory-strategy.md` · `scripts/check-pbm-power.ts` ·
`scripts/check-power-source-coverage.ts` · `scripts/check-thermal-dose.ts`

---

## 22. Conclusion — the three sentences that matter

1. **The binding constraint is heat, and expressed correctly it is a wall rather than a limit.**
   `NP-PWR-BUDGET-001` §4.4.4 argued it as a tile count, where it read as a concurrency ceiling that a
   better CFD might relax. Restated in the unit **D-4** requires — **27.6 to 49.1 W of aggregate
   emitter power** — R-10's existing 40 W already sits at or above the conservative end, and **every
   rung of the coverage ladder above it buys watts the sealed cavity cannot reject**: 4.7–8.4× at
   PD 240 W EPR, 36–65× at 1,800 W mains. The wall-plug path for T1 is not merely premature; it is
   buying a quantity the architecture has no way to spend.

2. **The thermal-dose audit inverts the hazard, and this is the most valuable result in the study.**
   The protocols a T1 device can run today are nowhere near harm — **3.5 × 10⁻⁶ CEM43 over a full
   108-session course**, seven orders of magnitude clear. The exposure is created by **cascading**,
   which is `NP-SES-PWR-001` §4's own sanctioned remedy for insufficient power: it holds the cavity at
   the interlock ceiling for hours (Vascular Baseline, 40 groups, 20.0 h, **292 CEM43**), and **13 of
   20 cascaded protocols pass the conservative reference line.** *The accepted fix for insufficient
   power is what generates the thermal-injury exposure* — and the interlock cannot see it, because it
   caps temperature and has no duration input.

3. **The power gate is sound as a mechanism and rests on a premise §4 has invalidated.** It was
   decided against a ladder in which more watts unlocked 16, then 21, then 22 of 23 protocols; after
   the cavity ceiling every rung unlocks the same **two**, and the **thirteen** risky protocols are
   already reachable on a stock Home Standard with the brick in the box. **Net protocols unlocked by a
   T2 mains accessory: zero. Cost: NeurOne owning an IEC 60601-1 isolation barrier with `VE-11` open.**
   The direction's real objective — supervised-only protocols — is met in full by a device-bound
   signed entitlement at $0.00 BOM, which can also express the duration and dose axes a supply cannot
   see.

---

## 23. Revision history

| Rev | Date | Author | Change |
|---|---|---|---|
| **1** | **2026-08-27** | NeurOne Systems Engineering | **Initial release.** Costed design study of the helmet's power-source architecture, against principal direction of 2026-08-27 (multi-source supply; purchase-time selection; thermal injury as a time-temperature dose; a T1/T2 protocol split; and whether power supply can itself be the tier gate). **Headline: the wall-plug path is premature for T1, by a much wider margin than `NP-PWR-BUDGET-001` §4.4.4 states.** Restating §4.4.4's argument in the unit **D-4** requires gives a sealed-cavity ceiling of **27.6–49.1 W** of emitter power against an existing 40 W delivery, so PD 240 W EPR would supply 4.7–8.4× what the assembly can reject and an 1,800 W station 36–65×. **§7.0 is the central result: a coverage figure is a necessary condition, never a capability claim, and thermally achievable coverage is 2/23 under *every* candidate source — identical to today.** The route to the library is demand reduction (`OI-SESPWR-01`) and the T1/T2 split, not supply. **Premise corrections:** the commissioning demand list carries a phantom 40 W entry and a phantom 1,775 W maximum (the real maximum is **1,600 W**; 1,775 = 71 × 25.0 W is `OI-PBMCH-04`'s cross-platform compiler defect on an unmerged branch, so sizing against it would have been sizing against a bug); coverage at 40 W and 92 W is 2/23 and 4/23, not 3 and 5; **no source reaches 23/23 at any wattage**, because `clinical-09` is operator-scoped; and `NP-PWR-BUDGET-001` §4.4.1's decisive isolation row **is decisive about the wrong variable** — ownership follows mains entry, not connector form (**D-5**; its conclusion retained on the Layer 5 qualification and arbitration-simplicity grounds). **New §5 — the cumulative thermal-dose check the design does not have.** Every thermal control in-tree is a real-time reactive interlock; a repo-wide search finds **no CEM43 or cumulative-dose treatment on `main`**. `scripts/check-thermal-dose.ts` supplies it. **Two clean nulls and one finding:** protocols that fit the envelope accumulate ≤3.5 × 10⁻⁶ CEM43 over a 108-session course, and back-to-back sessions peak at 37.9 °C — **both negligible, stated plainly** — while **cascading, proposed in `NP-SES-PWR-001` §4 as the remedy for the *power* problem, puts three of twenty protocols past 40 CEM43 in a single session (Vascular Baseline 292)**, because the 42 °C interlock caps temperature and has no duration input. Ambient 25 → 35 °C moves a single session's dose ~500,000×, making `NP-ENV-OPRANGE-001`'s gate the most load-bearing thermal control in the design. **All thermal constants are provisional named exports — flag-for-review, not a pass/fail gate, until `OI-PWR-01` lands.** **New §6 — the T1/T2 split, F4, and whether power can be the gate.** Verified: `NP_PROTO_FLAG_T2_TIER` exists on the wire and **nothing in firmware reads it**; it is derived solely from the modality type set; and `NP-NPPS-REF-001` §4's `intensity_milliamps` tier split is **also unenforced** (`protocolValidator` applies one `'global'` limit) — **two declared-but-unenforced tier mechanisms**. The new requirement is the first tier gate with **no physical backing**. **On power-as-gate: partly right, and the numbers say where.** Total watts rank-correlate **0.849** with irradiance but only **0.582** with dose, and **any cap above 200 W passes Memory Boost, the highest-dose protocol in the library**, while blocking ten safer ones. It also cannot express milliamp-scale tier splits, collides with locked §2 (*"any PD-compliant charger must work"* makes it advisory), and **collides with the principal's own purchaser-choice direction — power can be the gate or the purchase, not both.** **D-9: each failure class gets the mechanism matching its remedy** — power gates F3 (it already does), and F4 takes a **device-bound signed entitlement**, which beats a proprietary authenticated supply on five of seven axes, adds **no** regulatory surface, and does not take out the session when an accessory fails in a clinic. Fourteen decisions **D-1…D-14** (five to principal), ten risk rows of which **four have no verification defined**, twenty open items **`OI-PWRSRC-01…20`** (four BLOCKING). **Recommended T1 BOM delta $0.00** — the recommended source set is the one already shipping and the tier gate that works has no hardware in it. Adds `scripts/check-power-source-coverage.ts` and `scripts/check-thermal-dose.ts`, both importing `analyse()` from `check-pbm-power.ts` so demand, coverage and dose cannot fork (D-1). **No locked section modified; no firmware, app or protocol changed.** `check-doc-filenames` and `check-section-refs` pass. |
