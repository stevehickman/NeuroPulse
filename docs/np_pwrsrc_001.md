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
