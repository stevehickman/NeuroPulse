# The Power Envelope Against a Specified Rejection Path — the T1 Source, the Second Inlet, and the First Electrical Specification of the TMS Coil

**Project:** NeurOne
**Document:** NP-PWR-THERM-001
**Revision:** 1
**Date:** 2026-09-15
**Status:** DESIGN STUDY — resolves the five threads grouped in issue #336 as far as the evidence reaches, and says explicitly where it stops. **Modifies no locked section.** CLAUDE.md §2.2 and §4.5 are read, re-derived and analysed; §9 says what §4.5 has to become and routes the edit, and §6 does the same for §2.2. Every figure is produced by `scripts/check-power-envelope.ts`, not transcribed.
**Effective Date:** —
**Author:** NeurOne Systems Engineering
**Approved By:** — (pending design review)
**References:** NP-THERM-SINK-001 Rev 1 (§2 the terminus, §3 the two recoveries, §4 `SPEC-SINK-01…04`, §7 the spreader trade, §8 the ceiling, §8.2 `SINK-D-1`, §11 the occlusion fault case, `SINK-D-1…6`, `OI-SINK-01…08` — the input this whole document is built on); NP-THERM-CFD-N1-001 Rev 1 (§3 the N = ∞ boundary condition, §4a/§5 the `R_sink` sweep, §6 authored montage sizes, §6a the per-protocol table, §6b the per-tile drive wall, §11 the `OI-PWR-01` disposition, `N1-D-1…7`, `OI-N1-01…08`); NP-THERM-CFD-R1-001 Rev 1 (§2 `R_OUT_BASE`, §5 the BN-boss export study and its "perfect sink", `OI-R1-01/02`); NP-THERM-COOL-001 Rev 11 (the Rev 11 withdrawal, §7.4.3 the clamp, §7.4.4 the Class B refusal, §7.5.1 the Δ derivation, D-4, `OI-THCOOL-16/17/19`, `OI-POE-09`); NP-FW-POE-001 (§3 the POE block, §4 step 4, §6.1 the hysteresis latch, §8 Mode 3, `OI-POE-06`, `OI-POE-09`); NP-PWRSRC-001 Rev 1 (§4.1 the cavity wall in watts, §4.3 the stacked-not-coincident argument, §4.4 the one place watts bind, §7.0 coverage 2/23, §12.4, §13 the proposed power table, §14 the charger-policy impact, D-1/D-2, `OI-PWRSRC-10/11/12/15/16/22`); NP-PWR-BUDGET-001 Rev 3 (§3.2 the aggregate estimate, §3.4 the efficacy floor, §4.1 the TMS scaling estimate, §4.2 the field-target finding, §4.4 the second inlet, §4.4.2 the §2.2 discrepancy, §4.4.4, D-4, `OI-PWR-01/02/03/04/08/10/11/12/13`); NP-SES-PWR-001 Rev 1 (§2.1 the tile-count governor, the library floor and ceiling, §4 cascading, D-1); NP-HW-HEXTILE-001 Rev 8 (§9.1 overhead, §9.2 concurrency, R-4/R-5, `OI-HEXTILE-02/06/09/20/21`); NP-ENV-OPRANGE-001 (§1 the derate definition, §2 the ambient/duty envelope); NP-ENV-001 §4 (the hybrid class rule); NP-DT-001 Rev 2 (DI-REG-01 IEC 60601-1, DI-PERF-12/13, DI-SAFE-13, VE-11); NP-REQ-FANHEALTH-001 (Path B1, SR-FAN-05); NP-TOOL-HUB-001 Rev 1 (§2 the occipital-arch placement, §3 F-02/F-04, HUB-MDR-04, FAI-HTOOL-02); NP-COST-001 Rev 2 (`OI-COST-10`); NP-CONV-001 Rev 6 (§4 identifiers, §8 a convention worth writing down is worth a script); `docs/neuromod_neuro_protocols.md` (the TMS clinical protocol parameters); `cad/CAD_PARTS_LIST.md` (TMS-COIL-HOUSING, TMS-WINDOW); CLAUDE.md §1 (wired-first USB-C, Mode 3), §2.1 (margin-negative T1), §2.2 (charger policy, LOCKED), §3 (modality ceilings, the 0.1–0.5 T range), §4.2 (the 42 °C interlocks and the TMS EMF gate), §4.3 (the 4-layer stack, the non-conductive CFRP window), §4.5 (power), §4.6 (Mode 3); IEC 60601-1 (42 °C applied part); IEC 62304 §4.3 (segregation); `scripts/check-power-envelope.ts`; `scripts/check-thermal-sink.ts`; `scripts/check-thermal-multitile.ts`; `scripts/check-pbm-power.ts`
**Related Issues:** #336 (all five threads), #335 (the tile-ceiling consumer), #332 (artifact A15, the TMS coil)
**Gate:** No gate. Closes `OI-PWR-01` and `OI-SINK-02`/`OI-SINK-05`, answers `OI-PWR-11` and `OI-POE-09`, narrows `OI-PWR-02`/`OI-PWR-03`/`OI-PWR-04`, and raises `OI-PWRTH-01…09` of which two are BLOCKING.
**IEC 62304 Class:** — (analysis document; no code ships from it). §10 argues the class of the one firmware change it recommends.
**Supersedes:** None — specifies terms `NP-PWR-BUDGET-001` §4 left open and re-derives figures `NP-PWRSRC-001` §4.1 computed against a resistance that has since been specified.
**Parent Document:** NP-THERM-SINK-001 §4 (`SPEC-SINK-01`)

---

> **⚠ READ FIRST — the one-paragraph version.**
>
> Issue #336 groups five threads on the grounds that *"resolving any one of them alone just moves
> the unspecified quantity somewhere else."* That is exactly right, and this document's finding is
> the reason: **every one of the five terminates in heat, not in watts, and four of the five were
> being argued as supply questions.** With `SPEC-SINK-01` in hand the PBM ceiling is **31.4 W across
> the whole lattice and 8.9 W at the montages the library actually authors** — 3.1× below the bottom
> of the band `NP-PWRSRC-001` §4.1 argued the T1 source decision against, so that decision
> **strengthens and can be closed rather than deferred** (§5). CLAUDE.md §4.5's *T1 peak 45–50 W* is
> an electrical peak the assembly cannot spend (§3). The TMS coil now has an electrical
> specification — `SPEC-TMS-01…05`, derived from a coil geometry and landing on top of
> `NP-PWR-BUDGET-001` §4.1's independent scaling estimate — and it needs **176 W**, which is an
> ordinary PD contract; what is **not** ordinary is the **33 W of I²R inside a coil pressed against a
> scalp under the same 42 °C limit**, with no cooling path specified and the window under it required
> to be non-conductive (§8). And the coil at the top of CLAUDE.md §3's stated field range reaches
> **4.1× less cortical field than the protocols the evidence base cites need**, which does not
> change the power answer so much as put **two power answers 17× apart** behind a question that is
> not a power question at all (§7). **`OI-PWR-04`'s third option is assessed and declined for a
> fourth** (§11), and `OI-PWR-11` is decidable at last: 130 W against a 176 W requirement, so the
> Pro Full second brick is **not** a dual-inlet TMS provision (§12).

---

## 1. Headline

1. **The five threads are one thread (§2).** Four of the five were framed as supply questions —
   how many watts, through how many inlets, from what source. Given a specified rejection path and
   a specified coil, the supply half of every one of them is easy and the thermal half is the wall.
   The issue's own grouping rationale is the finding.
2. **`OI-PWR-01` is closed, not narrowed (§4).** `NP-THERM-CFD-N1-001` §11 recommended NARROWED on
   the grounds that a mesh CFD could not answer the question because the binding term was lumped and
   unspecified. It has since been specified. The aggregate-concurrency question it was opened for is
   answered; the intra-tile field and mesh-independence residue belongs to `OI-R1-01`, where it
   started, and nothing is left under `OI-PWR-01`'s own number.
3. **The PBM ceiling is N-dependent and a charger cannot see N (§3).** 31.4 W at the full lattice,
   **8.9 W at N = 6**, 17.9 W at the top of the ambient envelope. `NP-PWRSRC-001` §4.1's
   27.6–49.1 W is not wrong; it is the fully-distributed corner of a surface, quoted as a property
   of the assembly.
4. **The per-protocol table does not survive its own re-run (§4a).** Under `SPEC-SINK-01`,
   **22 of 22** zone-fixed protocols are thermally bound — the five `NP-THERM-CFD-N1-001` §6a called
   power-bound are not — and **21 of 22 hold no tiles at all on the bare shell that ships today.**
   `OI-SINK-01` was BLOCKING before this re-run and is more so after it.
5. **CLAUDE.md §4.5's T1-peak row is unreachable (§3b).** ~45–50 W device draw against a
   **38.4 W** thermal ceiling at the most favourable montage there is. A 45 W brick already covers
   it. The 65 W rung is a T2 *intent* signal, which §2.2 says it is, and nothing else.
6. **`SPEC-TMS` exists (§6).** Figure-8, two 25 mm wings in series, 10 turns each, **L = 18.2 µH**,
   a **28 µF / 1,600 V** bank, **36.0 J/pulse** at 0.5 T, a **142 µs** ring, **1,989 A** peak.
   Derived from geometry, and it reproduces `NP-PWR-BUDGET-001` §4.1's E ∝ B² estimate at both ends
   of the field range from inputs that estimate shares none of. `OI-PWR-03` asked for exactly this
   substitution.
7. **The supply figure is 176 W, and it is the easy half (§8).** `SPEC-TMS-04`: 169 W of recharge
   at 10 Hz biphasic-with-recovery plus ~7 W of device. A 240 W EPR contract covers it. **33 W of
   winding dissipation at the scalp does not have a home**, and the adiabatic pulse budget of a
   200 g coil is **516 pulses** against a 3,000-pulse protocol.
8. **The field range is the gating question, and it is not a power question (§7).** At 0.5 T this
   coil delivers **17 % of a clinical figure-8's maximum cortical field** at 20 mm and is **4.1×
   short** of 120 % RMT. The shortfall is **depth falloff of a 25 mm wing**, which
   `NP-PWR-BUDGET-001` §4.2 does not charge it for at all, and not the peak-field comparison §4.2
   does make. Closing it by field alone needs 2.06 T → 613 J/pulse → **2,886 W**.
9. **`OI-PWR-04`'s option (c) is assessed and declined for an option (d) (§11).** A second PD sink
   on the head-worn hub was the lead candidate and its whole case was *scoping the Mode 3 loss*.
   At 176 W nothing scopes it — no power bank sources 176 W — so the case evaporates while
   `OI-PWR-12`'s cost (a 5 A switched aggressor inside the measured-shielding assembly) stays.
   **Put the inlet on the TMS driver, where the bank and the 33 W already are.**
10. **`OI-PWR-11` is decidable (§12).** 2 × 65 W = 130 W against a 176 W requirement. The reading
    in which §2.2's second Pro Full brick encodes a dual-inlet TMS architecture is **refuted by
    36 %**.
11. **`OI-POE-09` is answered in both halves (§10).** (a) **Two latches, one per class** — the
    Class C object stays unwritable by Class B code and keeps §6.1's "one boolean and one timer"
    property on the edge that has safety content. (b) The band is **chosen in dose**: re-admit only
    at ≥ 2.0× the efficacy floor, composed by `max()` with the sense path's 1.0 °C resolution
    floor, which is how §6.1 already requires margins to enter.

---

## 2. Why these five were one issue

The issue groups five threads and gives the grouping a reason: resolving one alone *"just moves the
unspecified quantity somewhere else."* That held while the unspecified quantity was a rejection
resistance. With `SPEC-SINK-01` specified it stops being true in the way the issue meant, and
becomes true in a stronger way:

| # | Thread | Framed as | Terminates in |
|---|---|---|---|
| 1 | `OI-PWR-01` / `OI-N1-02` — the tile ceiling | a modelling question (multi-tile CFD) | a **lumped resistance nobody owned**, now `SPEC-SINK-01`, and a **spreader BOM line** (`OI-SINK-01`) |
| 2 | `OI-POE-09` — a withdrawn Rev 10 claim | an IEC 62304 class question | a class question, genuinely — **the one thread that is not about heat** (§10) |
| 3 | the T1 power source | *which supply?* | **the cavity cannot reject what any supply delivers** (§5) |
| 4 | `OI-PWR-04` — the second inlet | *how many inlets?* | **where the 33 W and the capacitor bank live** (§11) |
| 5 | TMS has no electrical spec | a power-budget gap | **176 W of supply, which is easy, and 33 W at the scalp, which is not** (§8) |

Three of the five were being argued in watts and answered in kelvin. That is not a coincidence of
this document set; it is the shape `NP-PWR-BUDGET-001` D-4 and `NP-THERM-CFD-N1-001` N1-D-4 both
arrived at independently — **the governed quantity is watts, and what bounds the watts is
rejection** — and #336's five threads are five places the same substitution had not been made yet.

**What this document does not claim.** It does not close `OI-SINK-01` (the spreader is still a BOM
line and an EMF clearance nobody has), `OI-R1-01` (no mesh CFD), `OI-R1-02` (no bench), `OI-PWR-02`
(the field target is quantified, not decided) or `OI-HEXTILE-02` (no emitter is selected, so every
PBM cost and every η_wp inherits that). Where it specifies, it specifies at design-study grade with
the inputs named, and `scripts/check-power-envelope.ts --validate` is what falsifies it.

---

## 3. The PBM ceiling in the unit the governor needs

`NP-PWRSRC-001` §4.1 inverted `NP-PWR-BUDGET-001` §3.2 into watts and got one number for the whole
assembly: **27.6 W at R″ 0.41, 49.1 W at R″ 0.23.** That inversion is arithmetically correct and it
was the right move — it is what let §4 reach the reductio that a 240 W EPR contract asks for a
scalp interface in the boiling-water band. But the model behind it has **no N in it**, and under
`SPEC-SINK-01` N is the whole story:

| N | bare shell S0, W/tile | S0 total | with spreader S3, W/tile | S3 total |
|---:|---:|---:|---:|---:|
| 1 | 1.33 | 1.3 W | 2.46 | 2.5 W |
| 2 | 1.31 | 2.6 W | 2.35 | 4.7 W |
| **6** | 0.78 | **4.7 W** | 1.49 | **8.9 W** |
| 12 | 0.72 | 8.7 W | 1.20 | 14.4 W |
| 20 | 0.62 | 12.3 W | 0.88 | 17.6 W |
| 37 | 0.49 | 18.1 W | 0.64 | 23.6 W |
| 80 | 0.39 | 31.4 W | 0.39 | 31.4 W |

25 °C ambient, distributed montage, max `T_face` ≤ 42.0 °C. **17.9 W** aggregate at the +35 °C top of
`NP-ENV-OPRANGE-001` §2's envelope; **7.8 W at N = 6 occluded** (`SPEC-SINK-02`).

**§4.1's band is the fully-distributed corner of this surface, quoted as a property of the
assembly.** `NP-THERM-CFD-N1-001` §6 puts authored montages at **N = 5–37**, and at N = 6 the
admissible total is **3.1× below the bottom of §4.1's band with a spreader and 5.9× below it
without.**

> **This does not overturn §4.1's conclusion; it removes the last way of reading around it.** §4.1
> argued no source above ~57 W buys anything, with the escape clause that `OI-PWR-01`'s CFD might
> return a *materially higher* ceiling. It did not. **The ceiling fell when it was specified, and it
> acquired two arguments — N and ambient — that a charger keyed to "peak draw of configuration"
> cannot see** (§6).

**The budget rises with N**, which `NP-THERM-SINK-001` §8.1 states and `--validate` asserts as a
monotone sequence rather than an assumption. That is the exact opposite of a concurrency *ceiling*,
and it is the arithmetic behind #335's task 1: a tile-count governor gets the sign of the
relationship wrong, and the quantity to hand the compiler is this table, not a tile count.

### 3b. What it does to CLAUDE.md §4.5's T1-peak row

§4.5 reads *"T1 peak ~45–50 W · 20 V/3 A (65 W)"*. Net of `NP-HW-HEXTILE-001` §9.1's ~6–8 W non-PBM
overhead that is ~38–43 W to emitters, and `scripts/check-pbm-power.ts` takes it as
`AVAILABLE_W = 40.0`.

| | to emitters | device draw |
|---|---:|---:|
| thermally admissible, fully distributed (N = 80) | 31.4 W | **38.4 W** |
| thermally admissible at an authored montage (N = 6) | 8.9 W | **15.9 W** |

**The T1-peak row is an electrical peak the assembly cannot spend.** A 45 W brick covers the
fully-distributed ceiling with 6.6 W to spare, and covers the realistic montage nearly three times
over. This is `NP-PWRSRC-001` §12.4's *"a 65 W source unlocks zero protocols"* with a resistance
behind it rather than a coverage count.

### 3c. The per-protocol table, re-run — `OI-SINK-05` discharged

`NP-THERM-CFD-N1-001` §6a was computed at `R_sink` 0.5 K/W with the external film counted twice, and
reported **4 inadmissible at any heatsink / 14 heatsink-recoverable** over 23 authored protocols.
Re-run against `SPEC-SINK-01` on the network that restores the film to both paths (22 rows; the
23rd is clinician-selected, so its montage is not a property of the protocol):

| | count |
|---|---:|
| inadmissible at **any** rejection resistance | **3** |
| recovered by the **spreader**, not a heatsink | **4** |
| hold ≥ 1 tile on the **bare shell as adopted** | **1** |
| **power**-bound rather than thermally bound | **0** |

Two differences from §6a, stated rather than reconciled away. **(i)** The "perfect" column here pins
the exterior skin at ambient, which is *strictly more generous* than N1's ideal — N1 zeroed `R_sink`
but still charged the cavity leg its full 0.18 with the film in it. So it is an upper bound on N1's
column, and a protocol inadmissible here at a perfect exterior is inadmissible under any reading.
**(ii)** N1 counted 23 rows including the clinician-selected one.

**Three things this re-run says that §6a did not.**

- **The library is thermally bound end to end.** §6a found five protocols power-bound. Under the
  specified rejection path there are **none**. `NP-HW-HEXTILE-001` §9.3 consequence 2 — that
  aggregate thermal risk *"cannot"* bind because the power envelope permits ~6 tiles — is now wrong
  for every authored protocol, not merely for most of them.
- **"Recoverable on the heatsink alone" has no referent.** `SINK-D-2` says there is no heatsink to
  improve and `R_sink` is not a purchasable parameter. What those protocols are recoverable *on* is
  `SPEC-SINK-04` — a lamination process and a BOM line against a margin-negative T1 — which is
  `OI-SINK-01`, still **BLOCKING**.
- **On the shell that ships today, 21 of 22 hold no tiles at all.** `SINK-D-4` already calls the
  bare-shell column "not an operating design". This is the same statement counted over the library.

---

## 4. `OI-PWR-01` — closed, not narrowed

`NP-THERM-CFD-N1-001` §11 recommended `OI-PWR-01` **NARROWED, not closed**, on a stated reason: the
concurrency and clustering questions were answered, the intra-tile field and mesh-independence
questions belonged to `OI-R1-01` where they started, and *"a mesh CFD could not have answered the
question because the binding term is lumped and unspecified."* It added the condition that
`OI-PWR-01` *"should not gate anything that `OI-N1-02` does not gate first."*

**Both halves of that recommendation have since been satisfied by events.** `OI-N1-02` closed on
2026-09-08. The binding term is specified. What `OI-PWR-01` asked for — *a verification-grade
multi-tile CFD* — would now be a refinement of terms that are no longer the ones that move the
answer, and the two items that do gate the tile field are named and owned elsewhere:

| What `OI-PWR-01` was carrying | Where it lives now |
|---|---|
| aggregate concurrency; the "~6 tiles" rule | **Answered.** `NP-THERM-CFD-N1-001` §3/§6b, `NP-THERM-SINK-001` §8.1, §3 above. The governor is watts (`D-4`, `N1-D-4`, `SINK-D-1`) |
| clustering (`OI-PWR-10`) | **Answered.** ≤ 0.22 K across every authored montage; dropped at `N1-D-2` |
| intra-tile field resolution, mesh independence | `OI-R1-01` — never `OI-PWR-01`'s, and returned |
| bench correlation | `OI-R1-02` (THERM-1b) |
| the thing that actually gates the ceiling | **`OI-SINK-01`** — the spreader. BLOCKING |

> **PWRTH-D-1 (principal): `OI-PWR-01` is CLOSED.** Nothing remains under its number that is not
> carried, by name, by `OI-R1-01`, `OI-R1-02` or `OI-SINK-01`. Leaving it open as NARROWED keeps a
> fourth name pointing at three owned items and invites a mesh CFD to be sequenced against a term
> that no longer binds. `NP-THERM-CFD-N1-001` §11's analysis is adopted in full; only its
> disposition changes, and it changes because its own stated precondition was met.

### 4a. `OI-SINK-02` — discharged by pairing, not by overwrite

`OI-SINK-02` asked for `scripts/check-thermal-multitile.ts`'s `R_SINK_DEFAULT` to move 0.5 → 1.08
and be marked specified, and recorded why `NP-THERM-SINK-001` would not do it: *"it would silently
move every published figure in `NP-THERM-CFD-N1-001`."* That objection is correct and it does not
go away by being deferred to a later document.

**Both requirements are met by carrying the two values side by side.** `R_SINK_DEFAULT = 0.5` stays,
and keeps reproducing the document the script was written for. `R_SINK_SPECIFIED = 1.08` is added
next to it as the value **any new figure must be computed at**. And `rSinkNote()` prints, at every
report line that names either, that 0.5 is N1's published value and is superseded by `SPEC-SINK-01`.

> The defect `OI-SINK-02` was raised against is a **superseded number being readable without its
> label**, not a constant having the wrong value. A rename fixes the constant and leaves N1's tables
> unreproducible; this fixes the readability and leaves both documents intact. `OI-SINK-02` is
> **CLOSED**.

---

## 5. The T1 power source, re-decided against the specified ceiling

`NP-PWRSRC-001` Rev 1 recommended keeping the T1 source set as it ships and stated the recommendation
conditionally, because §4.3's own escape clause left it open: *"`OI-PWR-01`'s multi-tile CFD remains
the correct next step and this study does not displace it — but its result cannot make the wall-plug
path timely."* **`OI-PWR-01` has now returned, and it returned worse than the number the
recommendation was made against.**

| | ceiling used | source of it |
|---|---:|---|
| `NP-PWR-BUDGET-001` §3.2 (2026-08-02) | 4–8 tiles | single-tile CFD, extrapolated |
| `NP-PWRSRC-001` §4.1 (2026-08-27) | 27.6–49.1 W | the same, inverted into watts |
| **this document, §3** | **31.4 W distributed / 8.9 W at N = 6 / 17.9 W at +35 °C** | `SPEC-SINK-01`, two independent recoveries |

> **PWRTH-D-2 (principal): the wall-plug path for T1 PBM is CLOSED, not deferred.** §4.3's escape
> clause required a *materially higher* thermal ceiling to make even one rung of the supply ladder
> spendable — five to eight times higher for a 240 W contract, thirty-six to sixty-five times for
> mains. The specified ceiling is **lower** than the band that clause was written against, and it is
> lower in the direction that matters most: at the montages the library authors, not at the
> fully-lit corner nobody runs. `NP-PWRSRC-001` D-1, D-2 and §7's source set are **adopted as
> decided** rather than carried as recommendations.

**What this decides and what it does not.** It decides the *source* question. It does not touch
`OI-PWRSRC-10` (which standard certified the brick that is the isolation barrier — BLOCKING, and it
applies to the inlet that ships today), `OI-PWRSRC-11` (the hub hold-up is undimensioned — BLOCKING
for `OI-PWR-13`), or `OI-PWRSRC-12` (the thermal budget is not a number the governor can read). **§3
is the number `OI-PWRSRC-12` asks for**, and §13 routes it.

### 5a. The one place the ceiling being lower changes an answer rather than confirming it

`NP-PWRSRC-001` §7.0 records thermally achievable coverage as **2/23 under every candidate source —
identical to today**, and used that to show no source buys protocols. §3c's re-run says the
denominator moved: under `SPEC-SINK-01` on the **bare shell**, 21 of 22 zone-fixed protocols hold no
tiles at all. **§7.0's conclusion survives — no *source* changes it — but the figure it is stated
with is now a spreader-conditional one**, and quoting "2/23" without the spreader state attached is
the error `SINK-D-1` exists to prevent. `OI-PWRTH-02` routes the re-statement to that document.

---

## 6. CLAUDE.md §2.2's charger policy, reconciled

**§2.2 is LOCKED and is not modified here.** What follows is the reconciliation the issue asks for
and what §2.2 would have to become; the edit is routed as `OI-PWRTH-03`, alongside
`NP-PWRSRC-001`'s own `OI-PWRSRC-15`, which asks the adjacent question.

§2.2's rule is *"charger scaled to **peak draw** of the configuration"*, and the issue's premise is
that a changed thermal ceiling changes the ladder because the ladder is keyed to peak draw. **The
premise is right and the consequence is the opposite of what it sounds like: the ladder does not
need more rungs, it needs one fewer justification.**

| §2.2 row | Charger | Peak draw it is keyed to | Under §3 |
|---|---|---|---|
| Core | 15 W | EEG + hub, no PBM | unaffected — no PBM, no cavity |
| Home Lite | 30 W | partial PBM | inside the thermal ceiling at every N |
| Home Standard ★ | 45 W | ~17–20 W standard, 45–50 W peak | **45 W already exceeds the 38.4 W fully-distributed thermal ceiling** |
| Home Premium | 45 W | same | same |
| Pro Entry | 65 W | T2 standard 44–46 W | 1170 nm path, separate budget — unaffected |
| Pro Full | 65 W × 2 | T2 peak 70–74 W | **§12** |

**Three consequences, and none of them is "add a rung".**

1. **The T1 half of the ladder is already above its thermal ceiling at Home Standard.** Keying to
   peak draw is what put it there, and keying to peak draw is not wrong — a supply must cover the
   *electrical* peak the hardware can momentarily present, which is a transient the 22 F hub
   supercapacitor exists to absorb. What changed is that the **sustained** figure is now known and
   is far below it. §2.2 needs no new row; **§4.5 needs a thermal-bound column** so the two can be
   read together, which is `NP-PWRSRC-001` §13's proposal and §9's here.
2. **The $19 65 W upgrade must not be sold as a T1 protocol unlock.** §2.2 already scopes it
   correctly — *"any buyer selects 65 W upfront → **T2 intent**"* — and `NP-PWRSRC-001` §14.2 already
   found that telling a buyer what it unlocks inverts it from a signal into a statement. §3b adds
   the reason it would be a false statement: at T1 the 65 W rung is above the ceiling twice over.
   **No change to §2.2. A change to any marketing copy that reads the ladder as a capability
   ladder** — `OI-PWRTH-04`, routed to `docs/reference/marketing-notes.md` and
   `docs/reference/competitive-position.md`.
3. **The EU note survives untouched and is load-bearing in a new place.** *"Any PD-compliant charger
   must work; the app informs, never blocks."* A user who arrives with a 30 W brick on a Home
   Standard loses nothing at any authored montage, because the montage is thermally bound below
   what 30 W delivers. The note was written as a consumer-rights concession; §3 makes it nearly
   free.

> **PWRTH-D-3: `CLAUDE.md` §2.2 requires no change from the specified thermal ceiling, and the
> reason is worth recording rather than leaving as an absence.** A charger ladder keyed to peak draw
> and a governor denominated in sustained watts are **different quantities that were never in
> conflict**; the appearance of conflict came from §4.5 carrying only one of them. The reconciliation
> belongs in §4.5's table, not in §2.2's.

---

## 7. `OI-PWR-02` — does 0.1–0.5 T reach the protocols the evidence base cites?

This section is sequenced before the specification it constrains, because it decides which figure
the specification is written at.

`NP-PWR-BUDGET-001` §4.2 compares CLAUDE.md §3's **0.1–0.5 T** against the ~1.5–2.0 T a clinical
figure-8 reaches and concludes the target *"may be too low"*, flagging it as a coil-EM question out
of its scope. **That comparison is not like-for-like in either direction, and both corrections
matter.**

- **In NeurOne's favour:** what depolarises an axon is the induced field, and that goes as
  **dB/dt = B·ω**, with ω set by L·C. `SPEC-TMS-03` rings in **142 µs** against a clinical
  stimulator's ~250 µs, so this coil converts **1.8× more of each tesla** into induced field. §4.2
  charges it for a factor it does not owe.
- **Against it, and larger:** a **25 mm wing falls off far faster with depth** than a 45 mm one —
  0.48 against 0.76 at 20 mm on a first-order loop falloff. **§4.2 does not charge it for that at
  all.**

Both coils are put through the same first-order expression, so the geometry factors common to both
cancel and **only the ratio is quoted — no absolute V/m appears anywhere in this document.** Target:
the reference coil at 1.2 × RMT, RMT being ~60 % of its maximum output → **0.72 of maximum**.

| NeurOne B (T) | fraction of reference max | vs the 0.72 target |
|---:|---:|---|
| 0.10 | 0.035 | 20.6× short |
| 0.20 | 0.070 | 10.3× short |
| 0.30 | 0.105 | 6.9× short |
| 0.40 | 0.139 | 5.2× short |
| **0.50** | **0.174** | **4.1× short** |

**§4.2's concern is confirmed, for a reason §4.2 did not give.** And the correction §4.2 got wrong
matters, because it is what makes the conclusion robust rather than an artifact of a crude
comparison: the 1.8× this coil gains on pulse width is already credited in the table, and it is
still 4.1× short.

**Closing it by field alone is not available.**

| | at 0.5 T | at clinical equivalence |
|---|---:|---:|
| field | 0.50 T | **2.06 T** — outside the stated range |
| E_pulse (∝ B²) | 36.0 J | **613 J** |
| supply at 10 Hz, biphasic | 169 W | **2,886 W** |

**17×**, and there is no PD class, no mains accessory and no head-worn thermal path in the programme
that reaches it. The other lever is a **larger wing**, which is a coil-design decision belonging to
`OI-PWR-02` and not to this document — and which trades against `SPEC-TMS-03`'s ring period, since
a larger wing raises L and lengthens the pulse, giving back some of the 1.8×.

> **PWRTH-D-4 (principal): the TMS power requirement has two answers 17× apart, and which one is
> right is `OI-PWR-02`'s to settle.** `SPEC-TMS-04`'s 176 W is the figure **if CLAUDE.md §3's
> 0.1–0.5 T stands**. §9's table carries it **with that condition attached**, written at the lower
> answer and labelled — never averaged between them, and never quoted without the label.
> **`OI-PWR-02` is not resolved here; it is quantified, and it is now the gating item for every
> T2 power, thermal and mechanical decision.** BLOCKING.

**Softest input.** RMT at 60 % of maximum output. At the clinical 50–70 % ends the target moves to
0.60–0.84 and the shortfall to **3.4–4.8×**. The verdict does not turn on it.

---

## 8. `SPEC-TMS` — the first electrical specification of the TMS coil

`NP-PWR-BUDGET-001` §4 found TMS has **zero** power or energy specification anywhere in the document
tree, put an order-of-magnitude bound on it by scaling clinical stimulator energies as E ∝ B², and
raised `OI-PWR-03`: *replace that scaling with an actual coil-inductance and capacitor-bank
calculation.* This is that calculation.

### 8.1 The specification

| Ref | Quantity | Value |
|---|---|---|
| **SPEC-TMS-01** | coil | figure-8, **2 wings in series**, wing mean radius **25 mm**, **10 turns/wing**, 6 × 1.5 mm (9 mm²) copper strip. **L = 18.2 µH**, `R_ac` = 11.7 mΩ, Q = 69, 0.251 mT per amp-turn at the wing centre |
| **SPEC-TMS-02** | energy store | **28 µF at 1,600 V**, **biphasic with bank recovery** — not a topology preference; see §8.3 |
| **SPEC-TMS-03** | pulse | series-LC ring **T = 142 µs** (monophasic ~71 µs, biphasic ~142 µs); `I_peak` = **1,989 A**; dB/dt = **22.1 kT/s** at the design point |
| **SPEC-TMS-04** | supply | **169 W** average recharge at 10 Hz / 0.5 T / biphasic-recovered (η_net 0.40, η_psu 0.85). **423 W** monophasic. With ~7 W of device: **176 W** |
| **SPEC-TMS-05** | design point | **0.5 T**, the **top** of CLAUDE.md §3's range — §7. 0.1–0.5 T is an *adjustment* range whose bottom is not an operating point |

**Design point: 0.5 T.** §7 shows the bottom of the range is 20.6× short of threshold-level field,
so no session would be run there. Sizing §4.5 at a mid-range value (0.3 T → 61 W) would size it at a
field nobody uses.

### 8.2 It reproduces the estimate it replaces

| B (T) | I_peak (A) | E_pulse (J) | `NP-PWR-BUDGET-001` §4.1 |
|---:|---:|---:|---|
| 0.10 | 398 | **1.4** | ~1–2 J |
| 0.20 | 796 | 5.8 | — |
| 0.30 | 1,194 | 12.9 | — |
| 0.40 | 1,592 | 23.0 | — |
| 0.50 | 1,989 | **36.0** | ~16–40 J |

| train | rate | monophasic | biphasic + recovery | §4.1 |
|---|---:|---:|---:|---|
| rTMS 10 Hz | 10.0 Hz | **423 W** | 169 W | ~400 W |
| rTMS 1 Hz | 1.0 Hz | 42 W | 17 W | — |
| iTBS (600 pulses / 190 s) | 3.16 Hz | **134 W** | 53 W | ~130 W |

**Both of §4.1's published rows are reproduced from a coil geometry that shares no input with the
scaling argument**, and the monophasic column is the one to compare, because §4.1's estimate assumed
no energy recovery. `--validate` asserts these four correspondences and fails on drift, so
`SPEC-TMS` cannot quietly stop being the thing §4.1 estimated. An internal consistency check is
asserted alongside them: `I_peak` computed from the bank (`V√(C/L)`) must equal `I_peak` computed
from the field (`B/(µ₀N/2a)`), and it does to < 1 A.

### 8.3 Why the topology is specified rather than left open

A monophasic stimulator at the design point needs **430 W** of device draw. **No USB-C PD class
delivers it** — 240 W EPR is the top of the standard. Biphasic-with-recovery brings it to 176 W,
which a single 240 W EPR contract covers with margin. **`SPEC-TMS-02`'s topology is what makes the
§4.5 row expressible as a PD contract at all**, so it belongs in the specification and not in a
later trade study.

### 8.4 The coil's own thermal wall — §3's finding at the other tier

Q = 69 at the ring frequency, so **9.2 % of the stored energy is dissipated in the winding per
cycle**: **3.30 J/pulse** at 0.5 T, **33 W** during a 10 Hz train.

Adiabatic headroom of a 200 g coil assembly (≈ 100 J/K, copper-dominated and epoxy-potted) from
25 °C to the **same 42 °C applied-part limit the PBM tiles are held to** (IEC 60601-1, CLAUDE.md
§4.2): **1,700 J.**

| protocol | pulses | coil energy | vs budget |
|---|---:|---:|---:|
| rTMS 10 Hz standard (`docs/neuromod_neuro_protocols.md`) | 3,000 | 9,888 J | **5.8×** |
| iTBS | 600 | 1,978 J | **1.2×** |
| iTBS ×2 | 1,200 | 3,955 J | 2.3× |

**Admissible pulse count on the adiabatic budget alone: 516.**

> **This is §3's finding at the other tier, and it is why #336's grouping was right.** The TMS power
> path was argued as a supply question — shared rail, mains tether, second inlet — across two
> revisions of `NP-PWR-BUDGET-001`. Given a number, **the supply is the easy half**: 176 W is an
> ordinary PD contract. What is not ordinary is **33 W of I²R inside a coil pressed against a scalp
> under a 42 °C limit**, with no cooling path specified anywhere and with the window beneath it
> required by CLAUDE.md §4.3 to be a **non-conductive CFRP window** — which forecloses the obvious
> metallic spreader, exactly as `SPEC-SINK-04`'s graphite film has to be cleared against the same
> stack (`OI-SINK-01`). **`OI-PWRTH-01`, BLOCKING.**

The 200 g thermal mass and the adiabatic treatment are both first-order and both stated: a real coil
has a housing, a potting compound and a several-minute inter-train gap, and the number to design
against is a duty-cycle-aware model, not this one. What the adiabatic bound establishes is the
**sign and the order**: a 3,000-pulse protocol is not 10 % over, it is 5.8× over, and no plausible
refinement of the thermal mass closes that.

### 8.5 One consequence that falls out of CLAUDE.md §4.2 alone

§4.2's TMS interlock reads: *"EMF cancellation gated off 5 ms before pulse, 50 ms hold."* That is
**55 ms per pulse**. At 10 Hz the cancellation is off for **55 % of the train**; above **18.2 Hz** it
is off **continuously**; and iTBS's intra-burst rate is **50 Hz**, so for the whole of every burst it
is off.

Nothing here is a safety finding — the interlock gates cancellation off *because* the Helmholtz
coils must not fight the pulse, and that is correct. It is a **claim** finding: CLAUDE.md §1 lists
*"4-layer EMF shielding + active Helmholtz cancellation"* (*5-layer* when this was written; Layer 4 deleted 2026-09-23) as a founding principle and the *"only
consumer brain wearable with measured shielding"* as the primary technical claim, and during a TMS
train the active half of it is switched off more than half the time. The passive stack is unaffected.
**Nothing in the document set says this**, and a claim that is qualified nowhere is a claim that will
be made without the qualification. `OI-PWRTH-05`, routed to
`docs/reference/competitive-position.md` and `docs/reference/regulatory-strategy.md`.

---

## 9. CLAUDE.md §4.5 re-derived

**§4.5 is not edited by this document.** `docs/reference/hardware-detail.md` §4.5 is the owning file
and `OI-PWRTH-06` routes the edit; what follows is what the table has to become.

| Mode | Draw | Min contract | **Thermal bound** | Mode 3 |
|---|---|---|---|---|
| Standby | 1 W | 5 V/0.5 A | n/a | yes |
| EEG only | 2.5 W | 5 V/1 A | n/a | yes |
| Standard T1 ★ | ~17–20 W | 15 V/2 A (45 W) | inside | yes |
| T1 peak | ~45–50 W | 20 V/3 A (65 W) | **NOT REACHABLE — 38.4 W distributed, 15.9 W at N = 6 (`SPEC-SINK-01`)** | yes |
| T2 standard | ~44–46 W | 20 V/3 A (65 W) | 1170 nm TEC path, separate | yes |
| T2 peak (1170 nm) | ~70–74 W | 20 V/5 A (100 W EPR) | 1170 nm TEC path, separate | unverified (`OI-PWRSRC-08`) |
| **T2 + TMS †** | **~176 W** | **48 V/5 A (240 W EPR)** | **coil-limited to 516 pulses (§8.4)** | **no** |

**The T2 + TMS row is new and it is the point.** §4.5's 70–74 W was derived for the 1170 nm laser
zone specifically and is cited as an input by five other documents; `NP-PWR-BUDGET-001` §4 found TMS
absent from the electrical model entirely. At the design point it is **2.4× the T2-peak figure**.

**TMS and 1170 nm are not summed.** They are different applicators and no authored session runs both,
so the row is `max()`, not `sum()`. If that ever changes the contract becomes 243 W and there is no
PD class for it — which is a design constraint worth stating before someone writes a protocol that
assumes otherwise.

> **† Conditional on `OI-PWR-02`, and the condition is not a footnote.** §7 puts the 0.5 T design
> point 4.1× short of the cortical field 120 % RMT needs at this wing radius. If `OI-PWR-02` keeps
> 0.1–0.5 T, this row is the row. If it raises the field to clinical equivalence at this geometry,
> the requirement is **~2,886 W** and the modality is not head-worn at all. **The row is written at
> the lower answer and labelled, never averaged between them** — which is `NP-CONV-001` §4.0's *a
> document can be named before it exists* applied to a table row, the same move
> `NP-PWRSRC-001` §13 makes for the "not specified" row this one replaces.

**Not proposed:** any change to the T1 or T2 *draw* figures. §3 changes what they mean, not what they
are.

---

## 10. `OI-POE-09` — answered in both halves

This is the one thread in #336 that is not about heat. `NP-THERM-COOL-001` Rev 11 withdrew a Rev 10
claim — that an efficacy-floor refusal *"sets the §6.1 latch … exactly as a thermal denial does"* —
because that mechanism is specified nowhere, and `NP-FW-POE-001` §4 step 4 now records the gap.
**The issue asks either to close the item or to record what replaces the withdrawn claim. Both are
done here; the ownership half goes to principal because it names an IEC 62304 boundary.**

### 10.1 (a) Ownership — two latches, one per class

`NP-FW-POE-001` §6.1 states plainly that *"the latch is SW-01 state"*, written when every hard edge
was Class C. `OI-THCOOL-17` then sited the efficacy-floor refusal in **SW-02 at Class B**
(`NP-THERM-COOL-001` §7.4.4). One latch, two candidate owners, two classes.

| | shared latch, SW-02 signals SW-01 | **two latches, one per class** |
|---|---|---|
| IEC 62304 §4.3 | Class C state writable by Class B code — needs a segregation argument | boundary untouched |
| §6.1 property 2 ("one boolean and one timer") | preserved — one of each | **costs it: two of each** |
| What re-opens the Class C boundary | every revision of `NP-PWR-BUDGET-001` §3.4's efficacy floor | nothing |
| Anti-chatter | §6.1's argument | §6.1's argument holds for each edge **independently** — `OI-POE-09` says so itself |

> **PWRTH-D-5 (principal): two independent latches.** The shared-latch option buys §6.1's
> "one boolean and one timer" property and pays for it by making a **certified** object writable
> from an **efficacy** computation. `NP-THERM-COOL-001` §7.4.4's own argument for siting the clamp
> in SW-02 is that putting an efficacy computation inside the Class C boundary *"re-opens that
> boundary every time the floor is revised, for a quantity that cannot injure anyone"* — and a
> **write** into Class C state is that same coupling through a narrower aperture, not an escape from
> it. The cost is exactly two booleans and two timers, bounded and known. The benefit is that
> `NP-PWR-BUDGET-001` §3.4's floor can move without a Class C re-argument, which it will.
>
> **What replaces the withdrawn claim, stated so it is not assumed a second time:** *a floor refusal
> sets the **SW-02 efficacy latch**, a distinct object from the §6.1 Class C hysteresis latch. The
> two never write to each other. Admission requires **both** to be clear, and the composition is at
> the admission test, not in either latch's state.* That composition is `min()`-shaped, which is
> `NP-FW-POE-001` §5's existing rule for how the clamps combine — so it introduces no new
> composition semantics.

### 10.2 (b) Sizing — a band chosen in dose

Δ = 1.0 °C was derived in `NP-THERM-COOL-001` §7.5.1 from ADC representation, room-thermostat
differential and re-arm wait — **three grounds, none about dose.** Carried onto the floor edge:

| full dose | blocks at | re-arm @ 1.0 °C | dose there | × floor |
|---:|---:|---:|---:|---:|
| 40 J/cm² | 33.8 °C | 32.8 °C | 18 J | 1.8× |
| 60 J/cm² | 34.2 °C | 33.2 °C | 22 J | 2.2× |
| 90 J/cm² | 34.4 °C | 33.4 °C | 28 J | 2.8× |
| 120 J/cm² | 34.6 °C | 33.6 °C | 34 J | 3.4× |

Defensible in every row, chosen in none. **The quantity this edge guards is how far above the floor
a re-admitted session must land, and that is a dose.** Denominate it there: re-admit only at
**≥ 2.0× the efficacy floor (20 J/cm²)**.

| full dose | band for 2.0× | expressible? | **POE-D band = max(1.0, ·)** |
|---:|---:|---|---:|
| 40 J/cm² | 1.25 °C | yes | **1.25 °C** |
| 60 J/cm² | 0.83 °C | **no — below 1.0 °C** | 1.00 °C |
| 90 J/cm² | 0.56 °C | **no** | 1.00 °C |
| 120 J/cm² | 0.42 °C | **no** | 1.00 °C |

**A dose rule alone is unimplementable above 50 J/cm².** `OI-POE-06` records that the shipped
`adc_to_celsius()` returns whole degrees, so **1.0 °C is the finest band the sense path can express**
— and a rule that specifies 0.42 °C against a whole-degree sensor does not degrade, it silently
becomes 1.0 °C anyway with nobody having chosen it.

> **PWRTH-D-6: the efficacy-floor band is `Δ = max(1.0 °C, 50/D °C)` for a protocol of full dose
> `D` J/cm².** Composed by `max()`, which is how `NP-FW-POE-001` §6.1 already requires margins to
> enter. It delivers **≥ 2.0× the floor on every row**, it is chosen where the sense path can
> express the choice and falls back to the representation floor where it cannot, and it changes
> behaviour **only for protocols at or below 50 J/cm²** — a bounded change to an Efficacy-class
> refusal in SW-02 that touches no Class C state and cannot widen any thermal bound, because +35 °C
> is not SW-02's to move (`NP-FW-POE-001` §1).
>
> **`OI-POE-09` is CLOSED.** `OI-POE-06` is not, and PWRTH-D-6 makes it slightly more valuable than
> it was: fixing the ambient source to 0.1 °C would let the dose rule bind unaided at every dose,
> and the `max()` would become inert rather than load-bearing.

**One consequence to state.** Under PWRTH-D-6 a 40 J/cm² protocol's re-arm moves 32.8 → 32.5 °C.
That is *more* conservative, not less, and it is in the direction `NP-THERM-COOL-001` §7.4.5 already
warns reads backwards: the lighter protocol is the one held out longer. §7.4.5's advice — *"a
higher-dose protocol may still run,"* never *"try a shorter one"* — is unchanged and is now true by
construction rather than by arithmetic accident. `OI-THCOOL-18` is unaffected.

---

## 11. `OI-PWR-04` — the option set, priced against a real number

`NP-PWR-BUDGET-001` §4.4 added a third option to `OI-PWR-04`'s recorded two and called it the lead
candidate. **The issue asks for it to be assessed explicitly. Here it is, and the assessment
declines it for a fourth.**

Requirement, for the first time a number: **176 W sustained during a train** (`SPEC-TMS-04`,
conditional per §7).

| | capacity | verdict |
|---|---|---|
| **(a)** shared rail, one inlet | 240 W EPR covers 176 W | **feasible now, and was not before.** The reason §4.3 rejected it — "a purpose-built capacitor bank and a contract likely well above 100 W EPR" — was an unbounded requirement, not a large one. The bank is `SPEC-TMS-02` and is 28 µF |
| **(b)** mains-tethered | unbounded | NeurOne owns an IEC 60601-1 isolation barrier on a device with conductive applied parts, with `VE-11` open; costs Mode 3 **device-wide** |
| **(c)** second PD sink on the hub | 2 × 100 W = 200 W | 13 % margin. `OI-PWR-12`'s EMF bench is still owed, **inside** the assembly whose measured shielding is the primary claim |
| **(d)** inlet on the TMS driver | 176 W where the load is | **no second inlet on the head-worn hub at all** |

**(c)'s whole case was scoping**, and the case is stated in §4.4 precisely: it *"scopes the Mode 3
loss to the modality that forces it"*, where (b) excludes the whole device. **With 176 W in hand
that case evaporates.** No power bank sources 176 W; TMS is outside Mode 3 by arithmetic, whatever
the inlet count is. What (c) still costs is unchanged and real: `OI-PWR-12` puts a 5 A switched
aggressor at or adjacent to the occipital arch, and `NP-PWR-BUDGET-001` §4.4.3 is right that it
cannot be closed on paper.

**(d) pays none of that.** §8.4 puts 33 W of dissipation and a 28 µF / 1,600 V bank in the applicator
regardless of where the inlet is, and `cad/CAD_PARTS_LIST.md` already describes TMS-COIL-HOUSING as
*"a dedicated applicator, not a socket-mounted module"*. Putting the inlet where the energy store and
the heat already are:

- keeps the head-worn hub **single-inlet**, so CLAUDE.md §1's wired-first USB-C and T1 Mode 3 are
  untouched and `OI-PWR-12` does not arise;
- stays **SELV behind a certified brick**, so `OI-PWRSRC-10`'s open question about *which* standard
  certified it is not made worse — (b) is the option that changes that answer;
- leaves `OI-PWR-13`'s dual-source arbitration unneeded for the head-worn assembly, since the two
  supplies are never both feeding the same rail;
- and puts 1,600 V a cable-length away from the scalp rather than in the enclosure on it.

> **PWRTH-D-7 (principal): `OI-PWR-04` resolves to (d) — the TMS drive assembly carries its own PD
> inlet, on its own enclosure.** (a) is the fallback if the applicator must be passive, and is
> feasible; (c) is declined because its only advantage over (a) was a Mode 3 scoping that 176 W
> removes, while its `OI-PWR-12` cost stands; (b) is declined for the isolation ownership §4.4.1
> already priced. **`OI-PWR-04` is NARROWED to "(d), or (a) if `OI-PWRTH-07` finds the applicator
> must be passive", and it is no longer blocked on `OI-PWR-11`** — §12 answers that item and the
> answer does not bear on (d).
>
> **What this does not decide:** whether the applicator is passive (coil + cable) or active (coil +
> bank + charger). §8.4's 33 W and `SPEC-TMS-02`'s bank push toward active; the mass on a head-worn
> applicator pushes back. **`OI-PWRTH-07`**, ME + EE, and it is the question that decides (d) vs (a).

---

## 12. `OI-PWR-11` — 130 W against the requirement

CLAUDE.md §2.2 ships Pro Full **"65 W NeurOne GaN (branded) × 2"**, $26 BOM, against a §4.5 row
negotiating **one** 100 W EPR contract. `NP-PWR-BUDGET-001` §4.4.2 raised three readings the document
set could not distinguish and routed the item to principal with the note that *"it should be resolved
before any design work starts — the answer determines whether §4.4 is a proposal or a
specification."*

**It is decidable now, because the requirement has a number.**

| reading | against `SPEC-TMS-04` |
|---|---|
| **2 — a dual-inlet architecture was assumed** | 2 × 65 = **130 W** against a **176 W** requirement. **Short by 36 %. REFUTED as a TMS provision.** Two 65 W bricks do not power this coil at the design point under any topology; the monophasic figure is 430 W |
| **3 — a separate T2 accessory** | 65 W covers a 1170 nm applicator (44–46 W) comfortably and covers **nothing** at TMS. **Survives only for 1170 nm** |
| **1 — an undocumented spare** | **survives**, and is now the only reading consistent with both §4.5 and `SPEC-TMS-04` |

> **PWRTH-D-8 (principal, and §2.2 is not modified): whatever the second Pro Full brick is, it is
> not the TMS supply.** The charger ladder cannot be made to express TMS by adding 65 W rungs to it,
> and §4.4's second-inlet proposal was **never** a specification being rediscovered — §4.4.2's
> reading 2 is refuted, so §4.4 was a proposal, and §11 declines it on its merits rather than on
> this. **The decision §2.2 needs is between reading 1 and reading 3**, which is a commercial
> question (is it a spare, or is it the 1170 nm applicator's supply?) and not a power one.
> `OI-PWRTH-03` carries it to `docs/reference/commercial-model.md` §2.2 with `OI-PWRSRC-15`.

---

## 13. What this hands #335, and what it still cannot

#335 task 1 asks for the concurrent-power governor to be **re-expressed in watts** and for the
compiler to enforce it. §3's table is that quantity, and it is what `OI-PWRSRC-12` — *"the thermal
budget is not a number the governor can read, so a watts-only governor would permit a 232 W protocol
the assembly cannot reject"* — asks for:

```
P_thermal(N, ambient, spreader)   →   scripts/check-power-envelope.ts §1
```

with three properties the governor must carry rather than flatten:

1. **It rises with N.** A tile-count governor gets the sign wrong; a watts governor that ignores N
   gets the magnitude wrong by 12× between N = 1 and N = 80.
2. **It is spreader-conditional.** Per `SINK-D-1` a figure may be quoted only alongside the per-tile
   drive **and the spreader state**. Until `OI-SINK-01` closes, the bare-shell column is the design,
   and on it 21 of 22 protocols hold no tiles (§3c).
3. **It is ambient-dependent** in a way `NP-ENV-OPRANGE-001` §2's flat +35 °C gate cannot see —
   which is `OI-N1-08`, still open, and §3 makes it sharper rather than resolving it.

**What it cannot hand #335 is a number to compile against.** `OI-SINK-01` is BLOCKING and
`OI-HEXTILE-02` has selected no emitter, so `TILE_W`'s 25.0 W/tile and η_wp 0.35 are both provisional
and both enter §3 multiplicatively. **The governor's *form* is decidable today; its *constants* are
not.** `OI-PWRTH-08`.

---

## 14. Decisions

| Ref | Decision | Basis | Reversible |
|---|---|---|---|
| **PWRTH-D-1** *(principal)* | **`OI-PWR-01` is CLOSED**, not narrowed. Its residue is carried by name by `OI-R1-01`, `OI-R1-02` and `OI-SINK-01` | §4 | Yes |
| **PWRTH-D-2** *(principal)* | **The wall-plug path for T1 PBM is CLOSED, not deferred.** `NP-PWRSRC-001` D-1/D-2 and §7's source set are adopted as decided | §3, §5 | Yes — on a THERM-1b result that moves the ceiling by 5× |
| **PWRTH-D-3** | **CLAUDE.md §2.2 requires no change from the specified ceiling.** A ladder keyed to peak draw and a governor in sustained watts were never in conflict; the appearance of conflict was §4.5 carrying only one of them | §6 | Yes |
| **PWRTH-D-4** *(principal)* | **The TMS power requirement has two answers 17× apart and `OI-PWR-02` decides which.** §9's row is written at the lower one and labelled, never averaged | §7 | No — the two answers do not average |
| **PWRTH-D-5** *(principal)* | **`OI-POE-09`(a): two independent hysteresis latches, one per IEC 62304 class.** A floor refusal sets the SW-02 efficacy latch; admission requires both clear | §10.1 | Yes |
| **PWRTH-D-6** | **`OI-POE-09`(b): `Δ = max(1.0 °C, 50/D °C)`** — ≥ 2.0× the efficacy floor on re-admission, composed by `max()` | §10.2 | Yes |
| **PWRTH-D-7** *(principal)* | **`OI-PWR-04` resolves to (d)** — the TMS drive assembly carries its own PD inlet; (a) is the fallback | §11 | Yes — on `OI-PWRTH-07` |
| **PWRTH-D-8** *(principal)* | **§2.2's second Pro Full brick is not the TMS supply** (130 W vs 176 W). §4.4.2 reading 2 refuted | §12 | No — the arithmetic does not move |
| **PWRTH-D-9** | **`SPEC-TMS-01…05` is the TMS electrical specification**, at design-study grade, and `--validate` is what falsifies it | §8 | Yes — on `OI-PWR-02` or a coil-geometry change |

---

## 15. Risk rows

| Ref | Hazard | Current control | Verification |
|---|---|---|---|
| RISK-PWRTH-01 | The TMS coil reaches 42 °C mid-train; no cooling path is specified and the window beneath it must be non-conductive | None — §8.4 asserts no clearance | **No verification defined** — `OI-PWRTH-01`, BLOCKING |
| RISK-PWRTH-02 | A protocol is authorised against §4.5's T1-peak row, which §3b shows the assembly cannot sustain | `SINK-D-1`; `scripts/check-power-envelope.ts` §1 is the standing check | `OI-SINK-01` + THERM-1b (`OI-R1-02`) |
| RISK-PWRTH-03 | `SPEC-TMS` is built on a coil geometry this document chose, not one ME specified | Every constant is a named export and `--validate` pins the correspondences to `NP-PWR-BUDGET-001` §4.1 | `OI-PWRTH-09` — ME returns a coil envelope |
| RISK-PWRTH-04 | The shielding claim is made without §8.5's qualification, and a TMS train is where a reviewer would look for it | None — `docs/reference/competitive-position.md` carries no TMS caveat | `OI-PWRTH-05` |
| RISK-PWRTH-05 | A 1,600 V bank is sited in a head-worn applicator to keep the cable passive | PWRTH-D-7 prefers the inlet at the driver; the passive/active question is open | `OI-PWRTH-07` |

---

## 16. Open items

| Ref | Item | Owner |
|---|---|---|
| **OI-PWRTH-01** | **BLOCKING — the TMS coil has no thermal path.** 33 W of winding dissipation during a 10 Hz train, an adiabatic budget of 516 pulses against a 3,000-pulse protocol, a 42 °C applied-part limit, and a **non-conductive** window beneath (CLAUDE.md §4.3) that forecloses a metallic spreader. Specify a cooling path, a duty-cycle-aware thermal model, or a firmware pulse-count/train-gap governor — and note that the last is `NP-PWRSRC-001` §5.5's *time-at-ceiling* problem arriving at T2 | Thermal + ME + FW |
| **OI-PWRTH-02** | **`NP-PWRSRC-001` §7.0's "2/23 under every candidate source" is now spreader-conditional.** §3c re-runs the library under `SPEC-SINK-01`: on the bare shell 21 of 22 zone-fixed protocols hold no tiles. §7.0's *conclusion* survives — no source changes it — but the figure must not be quoted without the spreader state (`SINK-D-1`) | Thermal (owner of `NP-PWRSRC-001`) |
| **OI-PWRTH-03** | **`CLAUDE.md` §2.2 (LOCKED): decide between §12's readings 1 and 3** — is the second Pro Full brick a spare, or the 1170 nm applicator's supply? Reading 2 is refuted. Commercial question, not a power one. Route with `OI-PWRSRC-15` | Principal + Commercial |
| **OI-PWRTH-04** | **No marketing or positioning copy may read §2.2's charger ladder as a capability ladder.** §3b: at T1 the 65 W rung is above the thermal ceiling twice over, so "more watts, more protocols" is false. §2.2 itself scopes it correctly as a T2 *intent* signal | Marketing |
| **OI-PWRTH-05** | **The active-shielding claim is unqualified for TMS.** CLAUDE.md §4.2's gate puts Helmholtz cancellation off for 55 % of a 10 Hz train, continuously above 18.2 Hz, and for the whole of every iTBS burst. The passive stack is unaffected. Qualify the claim where it is made | Marketing + Regulatory |
| **OI-PWRTH-06** | **Apply §9's table to `docs/reference/hardware-detail.md` §4.5** — add the thermal-bound and Mode 3 columns and the conditional T2 + TMS row. §4.5 is a locked section; this is the edit `NP-PWRSRC-001` §13 proposed, now with the TMS row it could not supply | Principal |
| **OI-PWRTH-07** | **Is the TMS applicator passive or active?** §8.4's 33 W and `SPEC-TMS-02`'s 28 µF/1,600 V bank push toward siting the driver in the applicator; head-worn mass pushes back. **This decides PWRTH-D-7's (d) vs its (a) fallback** | ME + EE |
| **OI-PWRTH-08** | **§3's `P_thermal(N, ambient, spreader)` is the form #335's governor needs; its constants are not available.** `OI-SINK-01` (spreader) and `OI-HEXTILE-02` (no emitter selected, so `TILE_W` and η_wp are provisional) both enter multiplicatively. Specify the governor's **interface** now and its constants when they land | FW + Thermal |
| **OI-PWRTH-09** | **`SPEC-TMS-01`'s geometry is chosen here, not specified by ME.** Wing radius is the single largest lever — it sets depth falloff (§7), L, the ring period and the energy. A returned coil envelope re-runs everything downstream through one script. Co-decide with `OI-PWR-02`; `#332` records that artifact A15 has no owning document either | ME + Thermal |

**Closed here:** `OI-PWR-01` (§4, PWRTH-D-1) · `OI-SINK-02` (§4a) · `OI-SINK-05` (§3c) ·
`OI-POE-09` (§10, PWRTH-D-5/D-6) · `OI-PWR-11` (§12, PWRTH-D-8).

**Narrowed or re-pointed:** `OI-PWR-04` → (d) or (a), no longer blocked on `OI-PWR-11` (§11) ·
`OI-PWR-03` → answered by `SPEC-TMS-01…03`, residue is `OI-PWRTH-09`'s geometry · `OI-PWR-02` →
**quantified and BLOCKING**, now the gating item for every T2 power, thermal and mechanical decision
(§7) · `OI-PWR-13` → unneeded for the head-worn assembly under PWRTH-D-7 (§11) · `OI-POE-06` → more
valuable than it was (§10.2).

**Explicitly not closed:** `OI-SINK-01` (BLOCKING — the spreader) · `OI-R1-01` (mesh independence) ·
`OI-R1-02` (THERM-1b bench) · `OI-N1-08` (the flat ambient gate) · `OI-PWRSRC-10/11/12` (all three
BLOCKING and none touched here) · `OI-HEXTILE-02` (no emitter selected) · `OI-COST-10`.

---

## 17. Reproduction

```bash
bun scripts/check-power-envelope.ts             # full report — every figure above
bun scripts/check-power-envelope.ts --validate  # anchors only; exit 1 on drift
```

`--validate` asserts two families of correspondence, and the document is falsified if either drifts:

1. **That this report is reading the rejection specification it claims to.** `SPEC-SINK-01`'s
   `R_sink`, `h_ext` and exterior area; `NP-THERM-SINK-001` §8's published 10-tile and 2-tile
   ceilings; §8.1's 8.9 W at N = 6 and 31.4 W aggregate; and that the admissible **total** rises
   monotonically with N, which §8.1 claims and this script relies on to take the full lattice as the
   best N.
2. **That `SPEC-TMS` lands on `NP-PWR-BUDGET-001` §4.1's independent estimate** rather than
   replacing it with an unrelated number: E_pulse at both ends of the field range, and the
   monophasic 10 Hz and iTBS recharge figures. Plus one internal consistency check — `I_peak` from
   the bank must equal `I_peak` from the field.

The script imports the whole thermal model from `check-thermal-sink.ts`, which imports
`check-thermal-multitile.ts`, which imports `analyse()` from `check-pbm-power.ts`, so demand,
coverage, thermal and TMS cannot fork (`NP-PWRSRC-001` D-1). Per `NP-CONV-001` §8, a convention worth
writing down is worth a script — and a specification written against another document's constants is
worth a check that it still follows from them.

---

## 18. Revision history

| Rev | Date | Author | Change |
|---|---|---|---|
| 1 | 2026-09-15 | NeurOne Systems Engineering | **Initial release, against issue #336's five grouped threads.** **Central finding: every one of the five terminates in heat, and four were being argued as supply questions** — which is the issue's own grouping rationale, one level down. Takes `NP-THERM-SINK-001`'s `SPEC-SINK-01` as an input and re-derives what depends on it. **PBM ceiling in watts: 31.4 W at the full lattice, 8.9 W at N = 6, 17.9 W at +35 °C** — 3.1× below the bottom of `NP-PWRSRC-001` §4.1's 27.6–49.1 W band at the montages the library authors, because §4.1's model has no N in it (§3). **CLAUDE.md §4.5's T1-peak row is an electrical peak the assembly cannot spend**: 38.4 W thermal against ~45–50 W, so a 45 W brick already covers it and the 65 W rung is a T2 intent signal and nothing else (§3b). **`OI-SINK-05` discharged**: re-running N1 §6a under the specification leaves **22 of 22** zone-fixed protocols thermally bound (§6a found five power-bound), **3 inadmissible at any rejection resistance**, and **21 of 22 holding no tiles at all on the bare shell that ships** (§3c). **`OI-PWR-01` CLOSED rather than narrowed** — `NP-THERM-CFD-N1-001` §11's own stated precondition was met when `OI-N1-02` closed, and its residue is carried by name by `OI-R1-01`, `OI-R1-02` and `OI-SINK-01` (§4, PWRTH-D-1). **`OI-SINK-02` discharged by pairing not overwrite**: `R_SINK_SPECIFIED = 1.08` added alongside `R_SINK_DEFAULT = 0.5` with `rSinkNote()` printed at every report line naming either, so a superseded number cannot be read without its label and N1's tables stay reproducible (§4a). **The T1 wall-plug path is CLOSED, not deferred** — §4.3's escape clause needed a materially *higher* ceiling and the specified one is lower (§5, PWRTH-D-2). **CLAUDE.md §2.2 needs no change**, and the reason is recorded rather than left as an absence: a ladder keyed to peak draw and a governor in sustained watts were never in conflict (§6, PWRTH-D-3). **First electrical specification of the TMS coil, `SPEC-TMS-01…05`** — figure-8, two 25 mm wings, 10 turns each, **L = 18.2 µH**, **28 µF/1,600 V**, **36.0 J/pulse** at 0.5 T, **142 µs** ring, **1,989 A** peak, **169 W** recharge at 10 Hz biphasic-recovered — derived from geometry and reproducing `NP-PWR-BUDGET-001` §4.1's E ∝ B² estimate at both ends of the field range from inputs it shares none of (§8, `OI-PWR-03` answered). **Biphasic-with-recovery is specified rather than left open** because the monophasic figure is 430 W and no PD class delivers it (§8.3). **The coil's own thermal wall is §3's finding at the other tier**: 33 W of I²R at the scalp under the same 42 °C limit, **516 admissible pulses against a 3,000-pulse protocol**, no cooling path anywhere, and a non-conductive window beneath that forecloses a metallic spreader — `OI-PWRTH-01`, BLOCKING (§8.4). **`OI-PWR-02` quantified and confirmed**: at 0.5 T the coil reaches **17 %** of a clinical figure-8's maximum cortical field and is **4.1× short** of 120 % RMT, the shortfall being **depth falloff of a 25 mm wing** which `NP-PWR-BUDGET-001` §4.2 does not charge it for, not the peak-field comparison §4.2 does make — and the 1.8× this coil *gains* on pulse width is already credited. Closing it by field alone needs 2.06 T → 613 J → **2,886 W**, so the TMS supply question has **two answers 17× apart** and `OI-PWR-02` decides which (§7, PWRTH-D-4). **`OI-POE-09` CLOSED in both halves**: (a) **two independent latches, one per IEC 62304 class** — a Class B write into Class C state is the same coupling §7.4.4 refused, through a narrower aperture — with the withdrawn Rev 10 claim's replacement stated explicitly; (b) **`Δ = max(1.0 °C, 50/D °C)`**, ≥ 2.0× the efficacy floor on re-admission, composed by `max()` as §6.1 requires, chosen in dose where the sense path can express the choice and falling back to `OI-POE-06`'s whole-degree floor where it cannot (§10). **`OI-PWR-04`'s third option assessed and declined for a fourth**: a second PD sink on the hub was the lead candidate and its whole case was scoping the Mode 3 loss, which 176 W removes, while `OI-PWR-12`'s cost stands — the inlet belongs on the TMS driver where the bank and the 33 W already are (§11, PWRTH-D-7). **`OI-PWR-11` decidable at last**: 2 × 65 = 130 W against 176 W, short by 36 %, so §4.4.2's reading 2 is **refuted** and §4.4 was a proposal, not a specification being rediscovered (§12, PWRTH-D-8). Hands #335 the governor's **form** — `P_thermal(N, ambient, spreader)` — and states why its **constants** are not available (§13). Nine open items `OI-PWRTH-01…09` (one BLOCKING, plus `OI-PWR-02` re-marked BLOCKING), five risk rows, nine decisions PWRTH-D-1…9 (six to principal). Adds `scripts/check-power-envelope.ts`; adds `R_SINK_SPECIFIED` and `rSinkNote()` to `scripts/check-thermal-multitile.ts`. **No locked section modified; no firmware, app or protocol changed; no figure in a released document rewritten.** |
