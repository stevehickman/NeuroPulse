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

> **⚠ READ FIRST — the two answers, before the analysis that produced them.**
>
> **1. The wall-plug path is premature for T1, and the margin is far wider than `NP-PWR-BUDGET-001` §4.4.4 states.** §4.4.4 argues it in tiles — the thermal ceiling (4–8) and the power ceiling (~6) are *"coincident"*, so relieving power moves the binding constraint to heat. Restated in the unit **D-4** says the argument must use, the finding is much sharper: **the sealed cavity supports 28–49 W of emitter power, and the existing 45 W brick already delivers 40 W of it** (§4.1). A USB-C PD 240 W EPR contract would deliver 232 W — **4.7× to 8.4× the heat the assembly can reject.** A 1,800 W mains station would deliver 1,792 W, **36× to 65×.** No inlet, connector, brick or base station changes that number, because the residual terminates in a cavity that cannot be ventilated without breaching the EMF shield (`NP-PWR-BUDGET-001` §3.3). **Do not build a mains path for T1 PBM.**
>
> **2. The scope addition attacks that finding at its root, and the finding survives — quantified.** If the 42 °C applied-part limit moves, does the power ceiling become binding again? **No.** §5.4 runs the sensitivity: raising the scalp-contact limit from 42 °C to **50 °C** — an enormous clinical and regulatory ask — moves the aggregate ceiling from 28–49 W to **47–84 W**. That buys **two** more of the 23 protocols. To make a 240 W EPR contract thermally spendable you would need a scalp-contact temperature in the **84–126 °C** band. Water boils at 100 °C.
>
> **And the deliberate-heat proposition needs *less* power, not more.** §5.5 derives it from the same thermal network the safety case rests on: the total heat that can be delivered *into* the head at any defensible surface temperature is **4.8 W at 42 °C, 12.4 W at 50 °C**. The existing 45 W brick supplies that with 30 W to spare. **The sauna proposition is not an argument for a wall plug. It is an argument for a $4 resistive heater** (§5.9) — and, before either, for the mechanism question in §5.3, which may end it entirely.
>
> **What this study does recommend.** A **three-source set** (§6), a **base-station accessory** rather than a helmet-borne mains inlet (§7), an **arbitration architecture that keeps the Class C surface to one latched input** (§9, §15), a governor denominated in the **sum of present contracts** (§10), and an **F3 failure class that splits** so a buyer is never sold a source that changes nothing (§11). Total T1 BOM delta for the recommended set: **$0.00**, because the recommended T1 source set is the one already shipping (§16).
>
> **What it does not do.** It does not set a price, edit CLAUDE.md, resolve `OI-PWR-11`, or size TMS. It records where each of those blocks.

---

## 1. Method, and the two scripts

Per `NP-SES-PWR-001` **D-1** — *the audit is a committed script, not a table in a document* — every
demand and coverage figure below is produced by code committed with this study, not transcribed:

```bash
bun scripts/check-pbm-power.ts               # per-protocol demand (pre-existing)
bun scripts/check-power-source-coverage.ts   # source coverage (new, this study)
```

The second imports `analyse()` from the first. That is deliberate and is the whole reason the
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
| **As `bun scripts/check-power-source-coverage.ts` reports on `main`** | `13 22 68 82 98 124 133 143 150 162 163 173 178 185 200 200 330 355 375 375 400 1600` |

Three differences:

1. **There is no protocol at 40 W.** The supplied list carries one; the library does not.
2. **There are two protocols at 200 W, not one** — `Memory Boost` and `PBM — Cognitive Enhancement
   1064nm`, both 8 sockets at 25.0 W/tile.
3. **The maximum is 1,600 W, not 1,775 W.** `Vascular Baseline`, 80 sockets × 20.0 W/tile.

**Where 1,775 W comes from, since the number is not invented.** 1,775 = 71 × 25.0 W — the
`Vault (excl. Occipital)` zone at full two-channel drive. No protocol on `main` has that shape.
`clinical-03-pbm-alzheimers-1064.npps` on the **unmerged** branch `claude/clinical-03-alzheimers-band`
targets that zone, and on iOS and Windows its `wavelength: "1064nm"` field never reaches the wire
(`OI-PBMCH-04`), so on those two runtimes it compiles to a `660_808nm` tile: **71 × 25.0 = 1,775 W.**
On `main`, and on the web runtime anywhere, it does not exist.

> **Stated in the form this programme requires.** *1,775 W is not on `main`.* It is not claimed here
> that it exists nowhere — `git branch -a` shows concurrent work, and asserting a negative across
> unmerged branches is not a checkable claim. The figure to design against today is **1,600 W**.

**This does not weaken the brief's framing; it sharpens it.** The principal's point — *the top figure
is a requirement, not a defect* — stands at 1,600 W exactly as it stood at 1,775 W. What changes is
that the 1,775 W reading is a **cross-platform compiler defect masquerading as a power requirement**,
and sizing a supply against it would have been sizing against a bug.

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
13  22  68  82  98  124  133  143  150  162  163  173  178  185  200  200  330  355  375  375  400        1600
                                                                                                    └── 4.0× ──┘
```

**The largest single step in the library is 400 W → 1,600 W, a factor of four, and nothing asks for
anything in between.** That is the single most useful fact for a sizing decision, because it means the
choice is not a continuum. There are exactly four rungs worth considering:

| Rung | To emitters | Coverage | What it is |
|---|---:|---:|---|
| ~40 W | 40 | 2/23 | today |
| ~92–132 W | 92–132 | 4–6/23 | PD 100 W / PD 140 W EPR |
| **~232 W** | 232 | **16/23** | **PD 240 W EPR — one connector, one contract, Mode 3 intact** |
| ~470–500 W | 472–492 | 21/23 | dual EPR **or** a 500 W mains station — indistinguishable |
| ~1,650 W+ | 1,792 | 22/23 | mains only, for one protocol |

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

## 5. The deliberate-heat proposition (scope addition, principal direction 2026-08-27)

**Direction, in substance:** heat-shock proteins may help heal neurological problems (Frontiers in
Physiology 2019, DOI `10.3389/fphys.2019.01556`); reconsider the 42 °C limit; treat heat as a *sauna
for the head* alongside the other modalities; establish the upper limit before burns, and the upper
limit before hardware malfunction.

This lands here rather than in a thermal document because it attacks §4 at its root. If the thermal
ceiling moves, the power ceiling becomes binding again and the wall plug stops being premature.
**§5.4 runs exactly that sensitivity. The finding survives.**

### 5.1 The citation — recorded, not characterised

**DOI `10.3389/fphys.2019.01556` is a principal-supplied citation whose content could not be
verified from this environment.** `www.frontiersin.org` and `doi.org` both return
`CONNECT tunnel failed, 403` through the egress proxy; one attempt was made against each.

> **No finding, method, temperature, exposure duration, species, tissue or evidence grade from that
> paper is characterised anywhere in this document.** Everything in §5 is derived from the
> repository's own thermal model, from the definition of the CEM43 metric, or from the standards this
> programme has already made binding. Where a claim would need the paper, it is marked as needing it
> and routed to `OI-PWRSRC-02`.

### 5.2 The mechanism question comes before every engineering question

The proposition is *local heating at the scalp surface*. The literature the citation belongs to
concerns systemic heat therapy — sauna, immersion, whole-body hyperthermia — which raises **core body
temperature**, with heat-shock response following cellular temperature elevation *in the target
tissue*. NeurOne's actuators sit outside the skull, on tissue that is not the target.

The chain that must hold is:

> scalp surface at T → conduction through scalp, skull, CSF → **neural tissue** elevated enough, for
> long enough, to induce a heat-shock response

**The repository already contains the model that evaluates the second arrow, and it is the model the
entire safety case rests on.** `NP-THERM-CFD-R1-001` §2 and §3:

- The inward path from the tile face to the **perfused scalp core** is `R_face→core ≈ 0.105 m²K/W`,
  against a core pinned at **37 °C** by a Pennes perfusion term.
- Fan-off, that inward path is **3.7× less resistive** than the outward path, so *"~83 % of junction
  heat dumps into the patient"* — and §5.3 of that document records what becomes of it:
  *"~240 W/m² leaks straight to scalp, **removed by perfusion**, but leaving the tissue hot."*

**Perfusion is not a detail of the model; it is the model's dominant term, and it is a sink, not a
conductor.** Heat entering at the scalp is carried away by scalp blood flow into a 37 °C systemic
pool. To raise brain temperature the blood itself must be warmer — which is what *systemic* heating
does, and what a head-worn surface heater acting on a few hundred cm² of skin, over a body it cannot
warm, does not.

#### 5.2.1 The two claims are mutually exclusive, and that is the whole argument

This is the form the finding should be read in, because it needs no external literature:

| If… | Then… |
|---|---|
| The perfusion model is **right** | The perfused scalp shorts delivered heat to a 37 °C core. Brain temperature does not move measurably. **The proposition fails on physiology, before any power budget.** |
| The perfusion model is **wrong** | Heat does reach deep tissue. But `DI-SAFE-13`, `SR-FAN-03`'s 4.5 mW/cm² fan-off ceiling, `NP-ENV-OPRANGE-001`'s whole ambient/duty envelope and `NP-THERM-CFD-R1-001`'s rejection of Path A are **all** computed from that same model. **That is a first-order safety finding, not a feature opportunity.** |

> **There is no third branch in which the model is right for safety and wrong for efficacy.** Any
> proposal to heat the brain from the scalp is simultaneously a claim that the device's thermal safety
> analysis is unconservative in the same direction. Whoever argues for the modality inherits the
> obligation to re-open `DI-SAFE-13`.

#### 5.2.2 This is the sLORETA precedent, exactly

CLAUDE.md §3 (T2 additions, HD-tDCS) establishes the house rule: *"Localization ≠ reachability…
sLORETA resolves deep sources, but a 4×1 ring is focal only for cortical-surface targets… ACC sits
47.1 mm from its nearest scalp electrode and is not focally reachable from any electrode position…
deep targets must never be presented as focal stimulation."*

**A heat-shock target in neural tissue is real. That does not make it reachable by a scalp-surface
heater.** The programme already refuses to present the two as the same thing for electric fields, and
`NP-FW-HD-001` §2.3 enforces the refusal in a data field
(`NP_HD_TARGET_DEPTH_SURFACE`/`_DEEP`). The same discipline applies here; §17 D-8 records it.

> **`OI-PWRSRC-01` — BLOCKING for the entire proposition.** Does scalp-surface heating raise neural
> tissue temperature measurably at any surface temperature below the injury threshold? **Do not assume
> the answer in either direction.** What would settle it: (a) a Pennes-perfusion model extended
> through skull, CSF and brain with a *deliberate* surface source rather than a leaked one — a direct
> extension of `OI-PWR-01`'s CFD scope; (b) the citation's actual mechanism and route of heating,
> which requires reading it (`OI-PWRSRC-02`); (c) the selective-brain-cooling literature, in which
> surface head cooling has repeatedly been found insufficient to change brain temperature — **offered
> here as general physiological engineering knowledge, not from a project source, and requiring a real
> citation before it is relied on**, on the same footing `NP-PWR-BUDGET-001` §4.1 puts its TMS energy
> figures and `OI-PWR-06` tracks. **Clinical + Thermal. No engineering work on this modality should
> start before it resolves.**

### 5.3 Question 1 — the upper limit before burns: there isn't one

**Thermal injury is a dose, not a threshold**, and the honest answer to *"what temperature is safe"*
is that the question is malformed. The standard metric is **CEM43** — cumulative equivalent minutes at
43 °C (Sapareto & Dewey):

> CEM43 = t · R^(43 − T),  R = 0.25 for T < 43 °C,  R = 0.5 for T ≥ 43 °C

The formula is a definition and is used here as one. **The injury *thresholds* expressed in CEM43 are
literature values, and none is asserted in this document** — `OI-PWRSRC-03`.

#### 5.3.1 The isoeffect curve, which is arithmetic and is the deliverable

Thermal dose delivered by one 20-minute session:

| Face temperature | CEM43 for a 20-min session | vs. today's 42 °C session |
|---:|---:|---:|
| 40 °C | 0.31 | 0.06× |
| 41 °C | 1.25 | 0.25× |
| **42 °C** (today's limit) | **5** | **1×** |
| 43 °C | 20 | 4× |
| 44 °C | 40 | 8× |
| 45 °C | 80 | **16×** |
| 47 °C | 320 | 64× |
| 50 °C | 2,560 | **512×** |

Read the other way — **how long a session may run to deliver the same dose as today's 42 °C limit for
20 minutes:**

| Face temperature | Iso-dose session length |
|---:|---:|
| 42 °C | 20 min |
| 43 °C | 5 min |
| 44 °C | 2.5 min |
| **45 °C** | **75 seconds** |
| 46 °C | 38 seconds |
| 47 °C | 19 seconds |
| 50 °C | **2.3 seconds** |

> **This is the number that decides the proposition, and it is not a power number.** *"Raise the limit
> to 45 °C"* sounds like a 3 °C engineering concession. In thermal dose it is a **16× increase**, and
> holding today's dose at 45 °C means a **75-second** session. The efficacy band the programme already
> accepts (`NP-PWR-BUDGET-001` §3.4: 6–30 min) is not compatible with it.

#### 5.3.2 Four things the curve above does not capture

1. **Brain hyperthermia is a different and more serious hazard than skin burn, and nobody here has
   assessed it.** Neural tissue is injured at temperatures well below skin-burn thresholds. §5.2 says
   the brain probably cannot be reached; **if §5.2 resolves the other way, this becomes the governing
   hazard, not the scalp.** `OI-PWRSRC-04`.
2. **These are daily protocols.** CEM43 was developed for single hyperthermia treatments.
   `clinical-01`-class Alzheimer's dosing is **108 sessions over 56 days**. Twenty minutes a day at
   today's 42 °C limit is 5 CEM43 per session and **540 CEM43 cumulative** if nothing repairs — and
   the repair/thermotolerance term is not in the metric, not in this repository, and not something
   this study will invent. **Applying CEM43 to a daily-repeated, months-long course is outside its
   validated domain.** `OI-PWRSRC-05`.
3. **The therapeutic mechanism and the injury mechanism are the same mechanism.** Heat-shock proteins
   are induced by proteotoxic stress — they are the cell's response to protein denaturation, which is
   what CEM43 measures the accumulation of. There is no wall between "HSP induction" and "thermal
   injury"; there is one Arrhenius dose axis, and any therapeutic window is a ratio of repair to
   damage, not a temperature. **A device that deliberately induces HSPs is deliberately operating on
   the injury curve.** Stated as a mechanism observation requiring clinical review, not as a finding:
   `OI-PWRSRC-06`.
4. **Population variability, and a hazard specific to this form factor.** Impaired sensation (diabetic
   neuropathy, post-stroke, medication), age, scalp condition and hair density all move the response,
   and none is screened. **The device is worn on a head, retained by a Boa dial, by a user who may
   have impaired cognition** — three of the nine L2 research categories are AD/dementia, TBI and
   Parkinson's. *"The user will notice and take it off"* is not available as a mitigation here.
   `OI-PWRSRC-07`.

### 5.4 Does a raised limit make the power ceiling binding again? No — here is the sensitivity

This is the question the scope addition exists to force, so it is answered numerically. Re-run §4.1
with the applied-part limit as a free variable. Margin = T_limit − 30.7 °C (the single-tile face
temperature at nominal 25 °C ambient); ceiling = margin ÷ R.

| Applied-part limit | Margin | **Aggregate emitter ceiling** | Coverage at that ceiling | Δ vs today |
|---:|---:|---:|---:|---:|
| **42 °C** (today) | 11.3 °C | **28 – 49 W** | 2 / 23 | — |
| 43 °C | 12.3 °C | 30 – 54 W | 2 / 23 | **+0** |
| 45 °C | 14.3 °C | 35 – 62 W | 2 / 23 | **+0** |
| 47 °C | 16.3 °C | 40 – 71 W | 2 / 23 | +0 |
| **50 °C** | 19.3 °C | **47 – 84 W** | **4 / 23** | **+2** |
| 60 °C (indefensible) | 29.3 °C | 71 – 127 W | 4 / 23 | +2 |

> **Raising the scalp-contact limit by 8 °C — a 512× increase in per-session thermal dose (§5.3.1) —
> buys two protocols out of twenty-three.** To reach the 232 W of a single PD 240 W EPR contract, the
> limit would have to go to the 84–126 °C band derived in §4.2.
>
> **`NP-PWR-BUDGET-001` §4.4.4's conclusion therefore survives its strongest available attack, and the
> wall-plug path stays premature under every reading of the principal's direction.** The two ceilings
> are not in a delicate balance that a physiological argument can tip. At the top of the library they
> are three orders of magnitude apart.

### 5.5 The sauna's actual power budget — smaller than what already ships

If heat is the product rather than the waste, the quantity to size is not source watts but **watts
delivered inward**, and `NP-THERM-CFD-R1-001` §3 has already derived that network: the binding
constraint is inward flux against a 37 °C perfused core through `R_face→core ≈ 0.105 m²K/W`.

> q_inward,max = (T_face − 37 °C) ÷ 0.105  [W/m²],  over ~0.1 m² of vault

| Face temperature | q_inward | **Total heat delivered into the head** |
|---:|---:|---:|
| **42 °C** | 47.6 W/m² | **4.8 W** |
| 43 °C | 57.1 W/m² | 5.7 W |
| 45 °C | 76.2 W/m² | 7.6 W |
| 47 °C | 95.2 W/m² | 9.5 W |
| 50 °C | 123.8 W/m² | **12.4 W** |

*(Cross-check: §3 of `NP-THERM-CFD-R1-001` states `q_inward ≤ 47.4 W/m²` at the 42 °C limit. The first
row reproduces it to rounding, which is the intended validation of this arithmetic.)*

> **The entire deliberate-heat proposition, at any surface temperature anyone could defend, needs
> between 4.8 W and 12.4 W.** The 45 W brick in every Home Standard box supplies that with 30 W to
> spare. **The scope addition does not create demand for a wall plug. It creates demand for about ten
> watts, and the device already has them.**

**Does §3.3's "export efficiency, not supply size" lever invert, as the direction anticipated?**
Partly — and the part that inverts is not the useful part. If heat is wanted, the BN boss exporting
90 % of it is working against you, so **yes**, the export path becomes the thing to change rather than
the thing to improve. But the ceiling on what can be *delivered inward* is set by the face-temperature
limit and by perfusion, not by how much heat is available to deliver. Turning the export off does not
raise the 4.8 W; it only wastes less on the way there. **`NP-PWR-BUDGET-001` §3.3's conclusion — that
supply size is not the lever — holds in both regimes**, which is a stronger result than it had.

**And it breaks the thing it would improve.** Defeating the BN-boss export means the junction runs hot
again — which `NP-THERM-CFD-R1-001` §5 adopted the export specifically to prevent — while the sealed
cavity becomes the only place the heat can go, and §4.1 says that cavity is already at its limit.

### 5.6 Question 2 — the regulatory answer, which is harder than the physiological one

**42 °C is not a NeurOne preference.** `NP-DT-001` **DI-SAFE-08** records it as *"IEC 60601-1 scalp
surface ≤42 °C compliance required"*, **DI-REG-01** makes IEC 60601-1 binding, and **VE-11**
(accredited-lab standards testing) is **Open**. Physiology may permit more; the standard still says
42, and exceeding it is a **compliance decision, not an engineering one**.

**The structural point, which is the one that matters.** IEC 60601-1 treats an applied part that
merely gets warm and an applied part **intended to supply heat** as different things, with different
requirements — the latter carrying its own justification, labelling and clinical-evidence obligations.
The principal's direction does not ask to tolerate more incidental heat. It asks to make heat a
**modality**. That is a **reclassification of the applied part**, not a limit adjustment.

> **The exact clause, the exact permitted temperatures and the exact justification format are not
> asserted here.** They belong to accredited-lab and regulatory-counsel scope, and this programme has
> a live precedent of exactly this shape: `RISK-03`, where two firmware irradiance governors (R-4's
> 400/200 mW/cm² and R-5's 600 mW/cm²) have waited since 2026-05-06 on an opinion nobody has
> commissioned. **`OI-PWRSRC-08` — add the applied-part heating question to the existing `RISK-03`
> instruction rather than opening a parallel engagement**, which is what `NP-REG-PBM1064-001` §2
> explicitly directs. Same counsel, same device, same standard.

Three routes exist; each has a cost, none is free and none is quick:

| Route | What it requires | Cost |
|---|---|---|
| Justified deviation from the applied-part limit | ISO 14971 benefit-risk with **clinical benefit evidence** — which §5.2 says may not exist for this route of heating | Blocked behind `OI-PWRSRC-01` |
| Reclassify as an applied part intended to supply heat | New clause path, new type testing at VE-11, labelling, user-warning architecture | Reopens VE-11 scope; schedule impact |
| Leave 42 °C alone; treat HSP as out of scope | Nothing | **$0** |

#### 5.6.1 The landmine: this very likely moves T1 out of the wellness exemption

`docs/reference/regulatory-strategy.md` puts T1 on the **FDA-exempt general-wellness** pathway, in the
same category as Muse, Sens.ai and Apollo Neuro. CLAUDE.md §1 makes that one of the two tiers the
whole product structure rests on. The general-wellness policy has **two** gates, and this direction
strains both:

1. **The claim must be a general-wellness claim, not a disease claim.** The direction's own stated
   rationale is *"heat shock proteins can help heal neurological problems."* Healing a neurological
   condition is a disease claim. The existing T1 modalities survive on wellness framing precisely
   because their consumer names avoid it — *"Brainwave Entrainment Stimulation"* for tACS, *"Cortical
   Priming Stimulation"* for tDCS (CLAUDE.md §3, which states the regulatory-naming purpose outright).
   **There is no comparable renaming available for "deliberately heating the head to induce a
   stress-protein response."**
2. **The product must be low risk.** A device that deliberately drives an applied part **past a safety
   limit set by the binding standard**, on a population that includes dementia, TBI and Parkinson's
   users, is a poor candidate for a low-risk determination — and §5.3.2's fourth point removes the
   usual mitigation.

> **`OI-PWRSRC-09` — BLOCKING, and a bigger commercial fact than any wattage in this study.**
> **Adopting deliberate scalp heating as a T1 modality plausibly moves T1 from a 12–18-month
> FDA-exempt wellness launch to a 510(k) pathway.** CLAUDE.md §1's two-tier structure — one chassis,
> two markets, two timelines — is the thing at stake, and it is already under strain from `OI-COST-08`
> (the T1 and T2 price ladders colliding). **This must be answered by regulatory counsel before any
> engineering, and it must not be buried under the thermal analysis.**

### 5.7 Question 3 — the hardware ceiling, verified rather than assumed

Each figure below was checked in-tree; the brief's list was accurate except where noted.

| Element | Ceiling / status | Verified at |
|---|---|---|
| LED junction throttle / cutoff | **62 °C throttle, 65 °C cutoff** | `NP-DT-001` DI-SAFE-08 — confirmed verbatim |
| Junction throttle as a face-temperature control | **Rejected.** At T_j = 62 °C the face reaches **60.2 °C** and scalp 52.2 °C — 14–21 °C over the limit | `NP-THERM-CFD-R1-001` §2, Path A NO-GO |
| Face ≤42 °C under single-fault loss of forced convection | `DI-SAFE-13`, via scalp-facing NTC at PD2 (Path B1) + `SR-FAN-03` derate to **~4.5 mW/cm² at 43.3 °C ambient**. Status **Open**; risk row **RISK-26** open | `NP-DT-001` §5; `NP-THERM-CFD-R1-001` §3 |
| Base thermal design | BN-boss conductive export, **shielded interior un-ventilated** — ventilating it breaches the EMF shield | `NP-THERM-CFD-R1-001` §5 |
| Emitter drive window | 120–180 mA for **L70 80,000–100,000 h** (R-6) | `NP-HW-HEXTILE-001` §2 R-6 |
| Ambient envelope | PBM full ≤ +35 °C, derate +35→+43, **block > +43** — because *"even minimum useful dose cannot hold scalp ≤42 °C"* | `NP-ENV-OPRANGE-001` |

**Two corrections to the brief's list, and one item it did not contain.**

- **`NP-THERM-CFD-R1-001`'s Path-A rejection does *not* become moot if 42 °C moves.** The brief
  suggests noting what changes if 42 °C is not the constraint. Less than it looks: Path A was rejected
  because the junction throttle **regulates the wrong node** — fan-off, ~83 % of junction heat goes
  *inward*, so the junction is not a proxy for the face at **any** face limit. A higher limit changes
  the number Path B1's NTC compares against; it does not make the junction a valid proxy.
  **`DI-SAFE-13` and the Path B1 architecture stand unchanged under any limit.**
- **The L70 claim has no junction-temperature qualifier anywhere.** R-6 states 80,000–100,000 h against
  a *drive-current* window only. LED lifetime is a strong function of junction temperature, and every
  published L70 figure is quoted at a stated T_j or T_case. **CLAUDE.md §3's L70 claim is therefore
  unqualified in the one variable that dominates it** — which matters most in exactly the regime this
  direction proposes. `OI-PWRSRC-10`. (A pre-existing gap, not one the direction creates, but a
  deliberate-heat mode is where it would first become a warranty problem.)
- **The device has no battery, and that is a genuine advantage worth stating.** CLAUDE.md §4 has no
  battery row; `NP-FW-EMMC-002` Rev 2 §H records *"this device has no battery or coin cell"* as the
  reason its characterisation window is denominated in records rather than calendar time. **Thermal
  runaway of a lithium cell — the dominant thermal hazard class for head- and body-worn electronics —
  is structurally absent.** Any raised thermal setpoint is bounded by component and tissue limits only,
  with no stored-energy failure mode behind it.

**Elements with no stated thermal ceiling anywhere in the document set.** Each needs its own number
before any setpoint moves; none is invented here.

| Element | Why it binds |
|---|---|
| PDMS optical window; **PDMS–PI bond via 75 nm SiO₂ interlayer** (174–860 N/m peel) | The 200-cycle IEC 60068-2-14 thermal-cycling qualification is **BLOCKING** and unrun. A raised operating temperature changes the cycling profile the qualification must run against — **it invalidates the test plan, not merely the margin** |
| AgNW lens coating; 3–5 µm hard coat | Adhesion and sheet-resistance drift with temperature |
| Tile MCU (tinyAVR 2-series, `U1`) and its 128 B EEPROM | EEPROM **retention** is strongly temperature-dependent, and `NP-FW-NVRAM-001` Map 4 puts the dose-calibration record there |
| Hub 8 GB eMMC | Industrial parts are rated to 85 °C ambient and retention derates with temperature. Holds both UHDR and SHDR partitions |
| 22 F supercapacitor | Electrolytic lifetime roughly halves per 10 °C. CLAUDE.md §4 already logs its NTC for aging estimation — that model is calibrated for the current envelope |
| Mu-metal liner + PETG laminate encapsulation; silicone RTV at cutouts | PETG's glass transition is low. **This is a shielding element, so its degradation attacks the product's primary measured claim** |
| Palladium-coated polyester inner liner | The permanence claim in CLAUDE.md §4 is made for *"device lifetime"* at the current envelope |

`OI-PWRSRC-11`, and it is the reason §5.9 recommends a separate study: **seven component ceilings, none
of which exists, is not a section — it is a work package.**

### 5.8 Efficiency moves against you, and the actuator is probably wrong

LED radiant output droops as junction temperature rises, so a hotter device delivers **less optical
dose per watt**. In a deliberate-heat mode the two outputs of the same actuator are in direct
opposition: driving harder for heat reduces therapeutic optical output, while `OI-HEXTILE-21` already
records that the 1064 nm channel is **9× below** the irradiance its own Grade A protocol specifies, at
η_wp ≈ 4.8 %.

> **So the question the brief raises is the right one, and the answer looks clear: the PBM emitters
> are the wrong heater.**
>
> - A resistive element converts ~100 % of input to heat. A 660/808 nm LED array converts 55–70 % to
>   heat (η_wp 0.30–0.45) and spends the rest on light nobody asked for in this mode.
> - The heat requirement is **4.8–12.4 W** (§5.5). Distributed polyimide film heating elements at that
>   power are a **$3–6 BOM class**, against the **$11.53/tile driver-plus-metering line — $346/headset
>   at 30 tiles** (`NP-COST-001` §4) that a PBM tile carries.
> - A separate heater **decouples the two modalities**, so heat and dose become independently
>   commandable instead of two readings of one current setting — the same argument
>   `NP-FEAS-PBMCH-001` makes for separating 660 nm from 808 nm.
> - It also **relocates the actuator**. A resistive element needs no optical window, no dose metering,
>   no photodiode pair and no 6 mm emitter pitch, so it can sit where heat should be delivered rather
>   than where light must be.
>
> **`OI-PWRSRC-12` — if the modality survives `OI-PWRSRC-01` and `OI-PWRSRC-09`, evaluate a
> purpose-built resistive heating element before assuming the PBM emitters do both jobs.** Nobody has
> asked this, and it carries a large BOM and architecture consequence in both directions.

### 5.9 Recommendation on scope: this needs its own study, and here is why rather than padding

The direction instructed that if the analysis grows past what a section can carry honestly, that
should be said. **It has.** What §5 can carry — and does — is the part that gates everything else.

**Settled here:** the mechanism objection in its mutually-exclusive-claims form (§5.2); the sensitivity
showing a raised limit does not rescue the power case (§5.4); the sauna's actual budget of 4.8–12.4 W
(§5.5); the CEM43 isoeffect arithmetic (§5.3.1); the regulatory reclassification and the wellness
landmine (§5.6); the verified hardware ceilings that do exist (§5.7); and the actuator question (§5.8).

**Not settled here, and not honestly settleable in a section:** seven component thermal ceilings that
do not exist (`OI-PWRSRC-11`); brain-hyperthermia hazard analysis (`OI-PWRSRC-04`); cumulative thermal
dose over a 108-session course with a repair term (`OI-PWRSRC-05`); re-derivation of `NP-ENV-001` and
`NP-ENV-OPRANGE-001`, whose entire operating envelope is expressed as headroom to 42 °C; and the
re-argued interlock architecture.

> **Recommended: a separate thermal-modality study, `NP-THERM-HSP-001`, gated on `OI-PWRSRC-01`
> (mechanism) and `OI-PWRSRC-09` (wellness) resolving *first*.** Both are cheap relative to the study,
> and either can end the proposition. **Commissioning seven component-ceiling investigations before
> asking whether the heat reaches the brain, and whether the modality is sellable without a 510(k),
> would be work in the wrong order.** §5 is deliberately the part that can be done before that money is
> spent.

### 5.10 If it is adopted anyway: what the interlock must become

Recorded so a decision to proceed does not have to rediscover it. A deliberately-heating device needs
its thermal interlock **re-argued, not relaxed**:

1. **The limit stays hardware-enforced and fail-closed.** CLAUDE.md §4 lists the 42 °C limit as a
   modality interlock with hardware enforcement (*"NTC per zone → hardware current throttle"*). Any
   raised setpoint is still a setpoint the safety MCU owns, still compared in hardware, still
   defaulting to the lower value on any fault, sensor faults included. **A therapeutic setpoint must
   never be writable by the application processor to a value above the safety table's own maximum** —
   the `NP-FW-POE-001` MCU-table-authoritative `min()` pattern, which exists for exactly this reason
   and must be reused rather than reinvented.
2. **Two setpoints, not one raised setpoint.** A *therapeutic* ceiling for an active heat session and
   an *incidental* ceiling for every other modality. Collapsing them silently raises the limit for PBM,
   tACS and VNS sessions that never asked for it.
3. **Defined behaviour when the user cannot remove the device.** §5.3.2's fourth point. The exit
   condition cannot be user action.
4. **It interacts with the head-presence gate.** `NP-FW-BENCH-001` (Rev 1, on `main`) gates stimulation
   on head presence with a designed bench/service bypass. A heat modality makes that gate
   safety-relevant in a new way: heating an unattended helmet on a bench is a fire question rather than
   a burn question, and the bypass exists precisely to allow bench operation. **The bypass must not
   extend to a therapeutic heat setpoint.** `OI-PWRSRC-13`.
5. **What becomes Class C:** the therapeutic-setpoint comparison, the two-setpoint selection, and the
   bypass exclusion in (4). All three sit inside the existing safety-MCU partition and extend
   `SW01-M04` rather than creating a new module. **The therapeutic decision to heat is Class B; the
   ceiling that bounds it is Class C.** The same split §15 argues for source arbitration.

### 5.11 UHDR/SHDR for thermal data

Applying CLAUDE.md §5's own test — *does this tell us something about the person?*

| Datum | Class | Reasoning |
|---|---|---|
| NTC temperature profiles (existing) | **SHDR** | Already classified. Device condition |
| **Per-session thermal exposure record** (measured face temperature, duration, cumulative CEM43) | **UHDR** | A *physiological measurement of the wearer's scalp*, taken during a session and tied to session duration — the same footing as raw EEG impedance and IR eye state, both of which §5's boundary list puts in UHDR during sessions |
| Cumulative CEM43 per user | **UHDR** | A cumulative injury-risk dose is a health record about a person; its whole purpose is to bound that person's exposure |
| Count of thermal-limit trips (unsigned integer, no timestamps) | **SHDR** | Matches the existing *"device session count → SHDR; session timestamps → UHDR"* boundary exactly |
| Thermal **interlock** trip → safety interlock log | **SHDR** | The locked *"safety interlock log → SHDR"* rule |

> **And the 2026-08-12 rule binds here.** A redaction applied conditionally on a sensitive predicate
> leaks that predicate. If a thermal interlock record suppressed its timing only for
> *therapeutic-heat* sessions, the redaction pattern would identify a therapeutic-heat session — and
> therefore that the wearer is on a heat protocol, which is a condition proxy. **The marshaller must be
> fixed-shape**, on the `np_fault_latch_build_report()` precedent: one record layout, no field whose
> presence or value depends on the session type. `OI-PWRSRC-14`.
