# EEG Electrode Net — Architecture, Sizing, and Interference Control

**Project:** NeurOne
**Document:** NP-HW-EEGNET-001
**Revision:** 8
**Date:** 2026-08-31
**Status:** DRAFT
**Effective Date:** —
**Author:** NeurOne Systems Engineering
**Approved By:** — (new document)
**References:** NP-HEX-ZM-001 §3.1/§3.2/§3.3/§4a/§5.3, NP-HELMET-GEOM-001 §0/§2/§3.1/§5, NP-DRV-SHELL-002 §3.5/§5.1.4–§5.1.6/§9.1–§9.6/§10.1, NP-HW-HUB-001 §4.5/§5/§7.2/§7.4, NP-HW-HEXTILE-001 §1/§4.5/§6.4/§7.1/§7.2/D-5, NP-THERM-BEZEL-001, NP-RISK-002 (RISK-21), NP-RISK-004, NP-COST-001 §2/§6, NP-PWR-BUDGET-001 §3.4–§3.7, NP-SES-PWR-001 §8, NP-FEAS-FNIRS-001, NP-HFE-002 §2.3/§2.5/§3/§5/§7.1/§7.3/§7.4, NP-OPT-PSF-001, NP-ENV-OPRANGE-001 §4, NP-CONV-001 Rev 6, CLAUDE.md §3/§4.2/§4.3/§4.4/§5.1
**Related Issues:** PR #283 (Rev 3); Rev 4 reconciles against PR #284; Rev 7 scopes the Rev 6 result; Rev 8 records the VNS-clip precedent on OI-EEGNET-20 and re-traces ±10 mm per the PR defect retrospective §2.4
**Gate:** NET-1 (strain-tracking fidelity), NET-2 (placement-verification qualification); interacts with REG-1, MECH-2, THERM-1, SEAL-1
**IEC 62304 Class:** N/A — the net is hardware. It is a risk control with no software class; the firmware that reads its impedance matrix and gates tES/visual stimulation carries the class (Class C where it owns an enable line, per CLAUDE.md §4.2).
**Supersedes:** None. Contests NP-HEX-ZM-001 §4a's T1-B tile type; see §0 and §7.
**Parent Document:** NP-HEX-ZM-001

---

> **⚠ Read §0 before anything else, and do not read this document as a recommendation to adopt.**
> It specifies a net because the request was to specify one, and the net does fix something no
> parameter change can fix. It also makes four things worse, one of them a safety-gate determinism
> problem and one of them a BOM regression on the exact term `NP-COST-001` already calls the largest
> uncosted risk. Both halves are in §7. **Nothing in this document should be quoted as a cost saving.**

---

> ## Rev 8 — the design already has a precedent for "one location, two functions", and it is not dual-rating.
>
> Two additions, no figure changed and nothing withdrawn.
>
> **1. The VNS clip is the precedent `OI-EEGNET-20` was missing.** §0's fork exists because the T1-B pad
> is *dual-rated* — one contact carrying both µV recording and tES current. The auricular clip faces the
> same problem at the same location and solves it the other way: **A1/A2 EEG references ride 2 spare
> conductors in the existing 6-pin cable** (CLAUDE.md §3 modality 6), so VNS stimulation and EEG
> reference are **co-located but not dual-rated**. One location, two functions, one conductor each.
> **The clip could do that because it had spare conductors. The socket has none** — `NP-DRV-SHELL-002`
> §5.1.4 dropped its last two reserved positions. That is the whole of `OI-EEGNET-20` in one sentence.
>
> **2. ±10 mm re-traced to source**, per `docs/status/pr-defect-retrospective.md` §2.4's rule that *"before a candidate is
> eliminated on a numeric ground, that number is re-traced to source"* — Class 2 being where #273
> eliminated a design candidate on a constraint that did not exist. §1.10.5 eliminated pod patterns on
> ±10 mm, so the trace is owed. **It holds, with a caveat now stated inline:** §3.4 derives it from the
> ~33 mm 10-20 line spacing and labels it *"a design tolerance, not a clinical claim"*. It is a
> **`[design-target]`**, not a measurement, and §1.10.5's pass/fail column now says so.
>
> ---
>
> ## Rev 7 — Rev 6's result is a **T1, per-target** result. Scoped, and the term it omits partly reverses it for T2.
>
> Rev 6 evaluated the lattice covering radius against every point on the scalp and concluded *"the
> finding is against discrete pod selection generally."* Asked whether the analysis covered T2 as well as
> T1, three things came out — the first two narrow the claim, the third cuts against it.
>
> | | Was (Rev 6) | Is (Rev 7) | Cause |
> |---|---|---|---|
> | The *"meets ±10 mm"* column | Unqualified | **A T1 answer.** `OI-EEGNET-15` already holds that tolerance is modality-dependent — *"tighter for T2 sLORETA source localisation and HD-tDCS 4×1 targeting."* On a tighter T2 budget **every row fails, including centre+ring7** | §1.10.5 |
> | The covering radius | Read as a whole-montage guarantee | **A per-target bound.** It assumes each target may claim whichever socket suits it best. A montage's sites **compete** for sockets, and one socket carries one tile | §1.10.6 |
> | *"Against discrete pod selection generally"* | Stated generally | **Sound for a sparse montage, overstated for a dense one.** Resolving contention is exactly what multi-pod tiles do, and Rev 6's single-target framing gave them no credit for it | §1.10.6 |
>
> **Measured, and it is stark at the current design:** with today's single centre pod, **~100 % of scalp
> points are reachable from exactly one socket** at ±10 mm. There is no assignment slack at all, so any
> two montage sites contending for a socket means one of them cannot be placed. Multi-pod raises it —
> centre+ring7 reaches 1.33 sockets per point, 69 % singly-served.
>
> **Not withdrawn:** the covering-radius numbers, the two Rev 5 corrections, and the model validation
> (a single centre pod gives 20.0 mm, half the 40 mm pitch). Rev 7 bounds what they answer.
>
> ---
>
> ## Rev 6 — `OI-EEGNET-25` computed. It corrects two Rev 5 claims and weakens the case for discrete pod selection.
>
> `scripts/pod-pattern-coverage.ts` computes the **lattice covering radius** — for every point on the
> lattice-covered scalp, the distance to the nearest pod position available *anywhere* on the lattice,
> assuming the electrode tile may be placed at whichever socket suits best. That is montage-independent,
> so unlike a montage-specific fit **it does not wait on REG-1**.
>
> | | Was (Rev 5) | Is (Rev 6) | Cause |
> |---|---|---|---|
> | §1.10.4's quantisation figures | The residual | **A lower bound on it.** The formula counts angular error at ring radius r only, ignoring radial mismatch and assuming a target is served by its own tile alone. Measured: ring4 11.1 → **12.7 mm**, ring5 9.0 → **12.5**, ring6 7.5 → **11.6**, ring8 5.7 → **10.4** | §1.10.5 |
> | §1.10.5: centre-plus-ring is *"strictly better than a regular pentagon at the same count"* | Asserted | **Wrong.** At N=4 centre-plus-ring is far *worse* (17.3 vs 12.7 mm); at N=5 it ties (12.6 vs 12.5); it wins only from N≈6, decisively at N=8 | Computed, not argued |
> | Whether pattern optimisation rescues five pods | Open — *"could make five behave like six or better"* | **It does not.** No pattern at any count meets ±10 mm worst-case except **centre+ring7 (8 pods, 9.4 mm)**, which keeps 34/90 emitters — and that is covering error *alone*, before shape, seating or landmark terms | §1.10.5 |
>
> **The consequence is against discrete selection generally**, not just against five: a scheme that picks
> the nearest of N fixed positions cannot meet the placement budget at any pod count that leaves useful
> emitters. §1.4's continuous per-site offset has **no** quantisation term — which returns the argument
> to §1.10.1, where §1.4 was already shown to imply position-keyed parts.
>
> **The model validates against two known figures:** a single centre pod (today's T1-B) gives a covering
> radius of **20.0 mm**, exactly half the 40 mm socket pitch, consistent with §1.3's 18 mm Oz defect.
>
> ---
>
> ## Rev 5 — variable pod population, and a correction §1.4 has owed since Rev 2.
>
> | | Was | Is (Rev 5) | Cause |
> |---|---|---|---|
> | §1.4's *"per-site placement file"* | *"a zero-tooling, zero-BOM change"*; §1.5 says the §0 fork *"dissolves"* | **It already means one part per electrode site.** §1.3's own examples need *different* offsets at different sockets (Oz ~18 mm posterior; Fp1/Fp2 ±18 mm in adjacent tiles), so `R-2` was broken by §1.4 and Rev 3 did not say so | §1.10.1. The variable-pod proposal inherits position-dependence rather than introducing it |
> | Pod count per tile | One number, to be chosen (§1.7) | **Possibly a per-socket variable** — either manufactured variants or one universal tile with pods selected on-tile | §1.10.2 |
> | §1.7.2's rejection of an on-tile mux | Applied generally | **Does not transfer to static selection.** The rejection was about time-sharing destroying simultaneity; selection here is set once at fit | §1.10.3 |
>
> **Nothing decided.** §1.10 raises `OI-EEGNET-23…26`. The pod-pattern optimisation (`OI-EEGNET-25`) is
> the deciding input and does not exist yet; every quantisation figure in §1.10.4 assumes a single ring,
> which §1.10.5 argues is the wrong pattern to assume.
>
> ---
>
> ## Rev 4 — reconciled against `NP-PWR-BUDGET-001` Rev 3 / `NP-SES-PWR-001` Rev 1. One citation was stale; two arguments got stronger.
>
> Rev 3 was written before PR #284 landed. Reconciling, per `NP-CONV-001` §7:
>
> | | Was (Rev 3) | Is (Rev 4) | Cause |
> |---|---|---|---|
> | The concurrency ceiling §1.8.1 quotes | *"~6 concurrent tiles at 25 % duty"*, taken as the settled figure | **~6 is an artefact of one operating point no authored protocol uses.** Real per-tile draw is 1.3–20.0 W, so concurrency spans **2–32 tiles** | `NP-SES-PWR-001` §2, measured against `protocols/predefined/` rather than argued |
> | §1.8's argument that the ceiling does not reach electrodes | Our inference from the ceiling being a power figure | **Now stated directly in the source**: `NP-PWR-BUDGET-001` §3.5 *"records that populated != driven"*, and §9.3's new fourth consequence retires *"placement options, not capability"* | The argument is corroborated, not disturbed |
> | `OI-EEGNET-21`'s open question (T1-E's PBM cost) | *"an NP-OPT-PSF-001 coverage question, not answered here"* | **Specified**: §3.7 shows coverage is the *only* quantity that scales with tile count, so T1-E costs coverage area and nothing else — total optical output is envelope-capped at ~13–14 W regardless | `NP-PWR-BUDGET-001` §3.7 |
> | `OI-EEGNET-20`'s reopening of the contact count | Requested on force grounds alone | **Independently forced.** `OI-HEXTILE-20` finds §8.1's 25.0 W/tile peak may be illegal against R-5; at 18.6 W the per-pin current that set `VLED` at 3 contacts changes. Same tooling-blocking decision, two callers | `NP-HW-HEXTILE-001` Rev 7 |
>
> **New at Rev 4:** §1.9 answers what else a four-pod tile can hold, and raises `OI-EEGNET-22`.
> §1.6 gains a guard against a misreading PR #284 makes newly available (see the note at §1.6).
>
> ---
>
> ## Rev 3 — three questions added ahead of the net. Nothing decided, nothing reversed.
>
> Rev 3 adds **§1.6** and **§1.7** and raises `OI-EEGNET-18/19/20`. It reverses no position in Rev 2
> and decides nothing. It replaces **one framing**, per `NP-CONV-001` §7:
>
> | | Was (Rev 2) | Is (Rev 3) | Cause |
> |---|---|---|---|
> | Electrode count per tile | Treated as given at **one**, with §1.4 varying only the electrode's *offset* inside the tile | An **open variable**, and the input §1.6's density argument needs | §1.4 established that electrode pitch ≠ tile pitch. Electrode *count* is the same class of assumption and was never examined |
> | What the placement budget is *for* | A mechanical tolerance (±10 mm, §3.4) to be held by geometry | Possibly the wrong instrument for **recording** modalities — density plus pose measurement may substitute | §1.6. Absent from the entire document set; no interpolation or spatial-sampling argument exists anywhere in `docs/` or `firmware/` |
> | Why more socket contacts are hard | Read as a geometry limit | An **accessibility-force** limit, exactly linear in contact count, on a question `OI-SHELL2-03(b)` has not answered | §1.7.2. `REQ-SKT-01`'s two rows were *"Forced, not preferred"* — forced by row **length**. Three rows hold 30 contacts at the same 18 mm span, and an edge-following run holds ~48 |
> | Emitter count under multiple pods | — | **More pods inflate emitters *less*, not more** — 4 pods +7.5 % vs 2 pods +13.0 % | §1.7.1. Each pod removes ~7 emitters, so the cost axis runs opposite to the intuition. This corrects a first draft of §1.7.1 that read the sign the wrong way |
>
> | Staggered seating heights | Dismissed as *"not a lever — total spring energy is unchanged"* | **Half right.** Naive load-spreading is not a lever and is *counterproductive* against an over-centre; **spatial force-spread cancellation is** — 5.7 N per 0.05 N of spread removed | §1.7.2. Energy was the wrong quantity; the peak of L(x)/MA(x) is the right one |
> | Why electrode sites are scarce | Taken as given | **An undecided assumption whose stated basis is an emitter-power argument that does not reach an electrode** | §1.8. `OI-COST-01`: per-configuration tile population *"has never been decided"* |
>
> **Three things this revision is careful not to claim.** (i) It does **not** contest `REQ-SKT-01`, the
> 19-contact count, or `SMART-1` — each is *raised* into §7.3 and routed to its owning document, in the
> scope discipline `NP-HW-HEXTILE-001` Rev 2 set. (ii) §1.6's ~20 mm sampling target is a **literature
> estimate, not a project figure**, and fixing it is half of `OI-EEGNET-18`. (iii) The mixed-type variant
> in §1.7.4 is **priced, not dismissed** — `NP-HFE-002` §7.3's L3 tactile marking already distinguishes
> module types, so "the types are indistinguishable" is not an available objection and is not made.
>
> **Sequencing is unchanged.** `OI-EEGNET-14` (seating concentricity) is still the next action. What
> Rev 3 adds is that **`OI-EEGNET-20` is time-boxed by tooling** — `OI-SHELL2-09(i)` blocks socket
> tooling on the pinout co-revision, and D-5 marks pin count *"load-bearing and now tooling-blocking."*
> The contact count reopens once or not at all.

---

> ## ⚠ Rev 2 — the central premise of Rev 1 was WRONG. Read this before §1.
>
> **Rev 1 claimed the tangential 10-20 registration error reaches ~50 mm at Oz across 520–620 mm
> heads, and that the hex-tile architecture therefore cannot be rescued by any parameter change.
> Both halves are wrong.** The error is **~1.6 mm from size and ~3.4 mm from size plus shape**, and
> the two named defects are both repairable inside the existing tile.
>
> | | Was (Rev 1) | Is (Rev 2) | Cause |
> |---|---|---|---|
> | Tangential error, size | ~50 mm at Oz | **~1.6 mm** | Rev 1 mapped shell→head by **arc length**. A conforming cap does that. **A standoff shell with radial pods maps by normal projection**, and for concentric similar surfaces normal projection preserves *angle* — and a 10-20 fraction is an angular invariant across geometrically similar heads. **Size very nearly cancels.** |
> | Tangential error, shape | not separated | **~3.4 mm** (±10 % vault height, sagittal) | Shape does not cancel. It is now the larger of the two. |
> | Dominant term | head size | **seating concentricity — ~0.9 mm of landing error per 1 mm of head displacement** | Never modelled in Rev 1, and never specified anywhere in the document set (`OI-EEGNET-14`). |
> | Fp1/Fp2 and Oz | "unfixable inside the tiled architecture" | **Both fixable in-tile** | Rev 1 assumed the electrode must sit at tile centre. That is a convention (`NP-HW-HEXTILE-001` D-1 "site 0 reserved"), not a physical constraint. See §1.4. |
> | `NP-HEX-ZM-001` §3.2's claim that the lattice "registers to anatomy by construction" | refuted by Rev 1 | **correct; Rev 1's refutation is withdrawn** | It follows from normal projection. |
>
> **Consequence for this document.** The net is **demoted from recommendation to fallback.** §1.4
> specifies the cheaper option that Rev 1 missed, and it addresses **every** adverse consequence in
> §7.2 as well as the §0 fork. §§2–6 remain valid as a net specification and are retained for the
> case where §1.4 proves insufficient — principally **T2**, whose sLORETA / HD-tDCS tolerance is
> tighter than T1 neurofeedback's and is not established here (`OI-EEGNET-15`).
>
> **What survives Rev 1 unchanged:** the radial shortfall (~−4 mm at the smallest head, §1.1) is real
> and untouched by any of this; §5's modality matrix; §6's reframing of the antenna question onto
> near-field coupling; and every consequence in §7 *conditional on the net being adopted*.

---

## 0. The decision that has to come first — T1-B's electrode is dual-rated

The T1-B tile's Ag/AgCl contact is typed `NP_ELEM_DUAL_ELECTRODE`. It **records EEG and delivers
BES / tACS / tDCS from the same contact** (`NP-HEX-ZM-001` §4a; `NP-DRV-SHELL-002` §5.1.4 pin 13
`ELEC`, "dual-rated: EEG record **and** tES drive"). "Move the EEG electrodes to a net" is therefore
not one change. It is three mutually exclusive ones, and they have very different value:

| Option | What it means | Consequence |
|---|---|---|
| **(a)** Net carries EEG only; T1-B survives as a tES tile | Two electrode systems on one head | **Delivers almost none of the benefit.** The socket keeps pins 13/14/15, network N4 stays, `REG-1` stays (tES placement is a 10-20 problem too), the spring pod stays, `OI-HEXTILE-05` stays. Pure additive cost. |
| **(b)** Net carries EEG **and** tES ★ | One electrode system, off-lattice | **The only variant with leverage.** Everything in §7.1 follows from this and only this. Also the only variant that re-homes two safety interlocks onto a soft part (§7.2.4). |
| **(c)** Net carries EEG; tES deleted from T1 | Product change | CLAUDE.md §3 modalities 4 and 5 are locked. Out of scope for an engineering document. |

> **A precedent worth reading before choosing (Rev 8).** The same "one location, two functions" problem
> is already solved elsewhere in the product, and not by dual-rating: the auricular clip carries VNS
> stimulation *and* the A1/A2 EEG reference, on separate conductors in one cable (CLAUDE.md §3 modality
> 6). Dual-rating here is not a considered preference over that approach — it is what the socket's
> conductor budget leaves. See `OI-EEGNET-20`.

**This document specifies option (b).** Every claim below is conditional on it. If the principal
selects (a), most of §7.1 evaporates and the net becomes a cost line with a registration benefit
confined to recording.

---

## 1. Where the registration error actually comes from (REWRITTEN AT REV 2)

The request framed the failure as insufficient pod travel. That is **right about the conclusion and
wrong about the mechanism**, and the correction is load-bearing rather than pedantic: it is the
reason no amount of tuning inside the tiled architecture can succeed.

### 1.1 There are two errors, not one

**Radial (the axis travel actually addresses).** The one adult SKU covers 520–620 mm circumference
(CLAUDE.md §4.4; `NP-HEX-ZM-001` §3.3 `HEAD_CIRCUMFERENCE_MM = 620`). Mean radius runs 82.8 → 98.7 mm,
a delta of **15.9 mm**. Spring-decoupled pods travel **±12 mm** (CLAUDE.md §4.4). If nominal is flush
to the 620 mm datum — which it must be, because the bowl is cut to the largest skull — the smallest
head demands ~15.9 mm of inward travel and receives 12.

> **Radial margin ≈ −4 mm, before any cephalic-index or local-curvature deviation is added.**
> Marginal, and in principle fixable by more travel.

**Tangential — and the mechanism is normal projection, not arc length.** The pod extends along the
**shell's local inward normal** until it meets the scalp. That is the operation that decides where an
electrode lands, and it is not the same operation as laying a tape measure along both surfaces.

For two **concentric** surfaces of the same shape, the inward normal at shell angle θ meets the head
at the same angle θ. A 10-20 site is defined as a fraction of the nasion→inion arc, and for
geometrically similar heads that fraction is an **angular invariant** — it does not depend on size.
So normal projection is registration-preserving and **size cancels**.

Modelled as concentric sagittal ellipses (shell semi-length 110.5 mm, vault 100 mm per
`NP-HEX-ZM-001` §3.1), landing error along the head's own arc:

| Case | Fp (f=0.10) | F (0.31) | C (0.52) | P (0.73) | O (0.94) |
|---|---|---|---|---|---|
| **Size only** — 520 mm head, similar shape | −1.1 | −1.5 | +0.2 | +1.6 | +0.7 |
| **Shape only** — 620 mm head, vault 110 mm | −1.5 | −2.5 | +0.3 | +2.6 | +0.9 |
| **Size + shape** — 520 mm head, +10 % vault | −2.3 | −3.1 | +0.4 | **+3.4** | +1.4 |
| **Seating failure** — 520 mm head sunk 10 mm | **+8.3** | +3.5 | −0.4 | −4.4 | **−9.1** |

Three results, and the ordering is the point:

1. **Size contributes ~1.6 mm.** Not 50 mm. `NP-HEX-ZM-001` §3.2's claim that the lattice "registers
   to anatomy by construction" is **correct**, and follows from exactly this.
2. **Shape does not cancel** and is the larger intrinsic term at ~3.4 mm — because a taller or
   flatter vault is not a scaled copy, so no angular invariance protects it.
3. **Seating concentricity dominates everything: ~0.9 mm of landing error per 1 mm of head
   displacement.** The 5-position forehead bridge, Boa dial and temporal wings exist to centre the
   head, but **no document in the set states a concentricity requirement, and none measures one.**
   The registration budget is therefore held by an unspecified property of the fit system
   (**`OI-EEGNET-14`**).

> **Model limits, stated.** This is a 2D sagittal, concentric, ellipse model. It brackets the truth
> from the opposite side to Rev 1's arc-length model, and it is the better of the two because it
> matches the actual kinematics — but the coronal plane (where cephalic index 0.70–0.85 acts) has not
> been computed, and the concentricity assumption is the very thing `OI-EEGNET-14` says is unverified.
> **Neither model should be quoted as the answer; the deciding measurement is seating concentricity.**

### 1.2 What travel can and cannot do

Pod travel remains a **radial** degree of freedom and still contributes nothing to a tangential
error. That part of Rev 1 stands. What changed is the size of the tangential error it was being
asked to fix — and, more importantly, that **tangential compliance is available inside the tile**.

### 1.3 The two named defects, revisited

- **Oz is ~18 mm anterior.** Socket 74 sits at 86.2 % of arc against a true O line at ~94 %
  (`NP-HEX-ZM-001` §3.2). But the tile's **active-field circumradius is 20.21 mm**
  (`NP-HW-HEXTILE-001` §4.1). **18 mm < 20.21 mm — true Oz lies inside socket 74's own active
  field.** An electrode offset 18 mm posterior within that one tile lands on Oz exactly.
- **Fp1 and Fp2 cannot each have a socket.** True — *if each tile's electrode must sit at tile
  centre*. Fp1 and Fp2 are ~35–40 mm apart on an adult, which is inside a 40 mm tile's 46 mm vertex
  span and well within two adjacent tiles' combined reach at ±18 mm offsets.

Rev 1 asserted that "no pitch inside the 34–46 mm workable window resolves adjacent 10-20 lines."
That is true of the **tile pitch** and false of the **electrode pitch**, and Rev 1 conflated them.

### 1.4 The cheaper option — move the electrode inside the tile

**The electrode sits at tile centre by convention, not by physics.** `NP-HW-HEXTILE-001` D-1 reserves
site 0 of the 91-site emitter lattice on every tile type — PD1 aperture on T1-A/T1-C, "the electrode
pod axis" on T1-B. §4.5 is explicit about what T1-B actually *is*:

> *"T1-B needs no new FPC outline, no new lattice, and no new socket interface — **only a different
> placement file**."*

**An off-centre electrode is another placement file.** D-1's own reversibility column reads "Yes —
FPC artwork only, pre-tooling."

The available offset range is the active-field circumradius minus the pod radius, and the pod
diameter is **`OI-HEXTILE-05` — unspecified anywhere in the document set**:

| Rings masked | Pod ⌀ | In-tile offset range | Emitters/tile | Home Standard emitters | vs 2,286 |
|---|---|---|---|---|---|
| 0–1 | 11.4 mm | **±14.5 mm** | 84 | 2,646 | +15.7 % |
| 0–2 | 19.0 mm | **±10.7 mm** | 72 | 2,538 | +11.0 % |
| **0–3** (≈ the current "~half LED count") | 26.6 mm | **±6.9 mm** | 54 | 2,376 | +3.9 % |
| 0–4 | 34.2 mm | ±3.1 mm | 30 | 2,160 | −5.5 % |

**`OI-HEXTILE-05` is the tangential registration budget in disguise.** It is currently framed as
"how many emitter rings does the pod displace" — a PBM-coverage question. It is also, and more
importantly, the question of how far an electrode can be moved to meet anatomy. Nobody has been
answering the second question because nobody noticed it was the same question (**`OI-EEGNET-16`**).

**At the corrected error magnitude, the existing pod may already be sufficient.** ±6.9 mm covers the
~3.4 mm intrinsic size+shape term with margin, and is exceeded only when seating concentricity fails
by more than ~7 mm. That makes the leading candidate a **zero-tooling, zero-BOM change**: a
per-site placement file, plus a concentricity specification on the fit system.

**Two things it does not do.** A manufactured offset is **fixed**, so it cannot track per-user
seating or per-user shape; the residual is whatever `OI-EEGNET-14` turns out to allow. And it does
not touch the **radial** −4 mm shortfall at the smallest head, which is a separate problem with a
separate fix (more travel, which on this axis genuinely does work).

**If fixed offsets prove insufficient**, the next step is still not a net: it is a **tangentially
adjustable pod carrier** — a one-time, app-guided eccentric or slider set at fitting and locked.
That keeps the socket electrical interface, the discrete `SEAT#` presence detect, and every §7.2
consequence intact. The net (§§2–6) is the option after that.

### 1.5 Consequences addressed, relative to the net

| §7.2 adverse consequence | In-tile offset (§1.4) |
|---|---|
| +18 % emitters into term **U** | **Avoided at ±6.9 mm (+3.9 %); tunable, and strictly better than the net at every setting** |
| `SH2-DRC-16` artifact may regress | **Avoided** — electrodes stay on L1 behind the DRL guard plane; N4 unchanged |
| `RISK-21` reverts; `EEG-ROUTE-CHANNEL` returns | **Avoided** — no cables, no tail, no connector |
| Safety gates go discrete → continuous | **Avoided** — keyed socket, `SEAT#`, `check_placement()` all unchanged |
| §0 dual-rated T1-B fork (`OI-EEGNET-01`) | **Dissolves** — T1-B keeps both roles |

---

### 1.6 The other way to spend the budget — sample densely and interpolate

**Raised at Rev 3. Not adopted, and not costed here.** §§1.1–1.5 treat placement as a mechanical
tolerance problem: put the electrode where the anatomy is, to within ±10 mm (§3.4). There is a second
framing that the whole document set omits, and it changes what the budget is *for*.

Scalp potential is spatially band-limited — the skull is a volume-conduction low-pass filter. A field
sampled above its spatial Nyquist rate can be reconstructed and evaluated at **any** coordinate,
including a nominal 10-20 site no electrode physically occupies. Placement tolerance then stops being
a mechanical requirement and becomes two software requirements: **sample densely enough**, and **know
where the array actually sits**.

**The arithmetic, at this lattice.** One electrode per 40 mm hex is one per 1,385 mm² of tile face,
which is an equivalent hexagonal-packing spacing of 40 mm — the tile pitch, as it must be. Doubling
electrode population divides the area per electrode and scales the spacing by 1/√2:

| Electrodes per tile | Area per electrode | Equivalent spacing |
|---|---|---|
| 1 (today) | 1,385 mm² | 40.0 mm |
| 2 | 693 mm² | 28.3 mm |
| 3 | 462 mm² | 23.1 mm |
| 4 | 346 mm² | 20.0 mm |

**The table describes the lattice's ceiling, not any shipped configuration — and this is the finding,
not a footnote.** It assumes *every* tile carries the stated count. Home Standard populates 9 T1-B
tiles of 30, and only **N** electrodes are simultaneously recordable, where N is 8 (T1) or 21 (T2),
set by the ADS1299 bank (§1.7.5). Equivalent spacing over the 30-tile field:

| Electrodes | Equivalent spacing | What this is |
|---|---|---|
| 9 | 73.0 mm | Home Standard today — 9 T1-B tiles, one electrode each |
| 8 | 77.4 mm | T1's simultaneous channel count |
| 21 | 47.8 mm | T2's simultaneous channel count |
| **120** | **20.0 mm** | **4 per tile on all 30 tiles — what the ~20 mm row actually requires** |

> **Sampling density and ADC channel count are the same constraint seen twice.** Reaching 20 mm needs
> ~**15×** T1's channel count and ~**5.7×** T2's. No electrode-per-tile decision reaches the
> reconstruction regime on its own; `OI-EEGNET-19` and the N in §1.7.5 must be decided together or
> neither means anything.

**One consequence for the study design.** Four electrodes on nine tiles is **not** a 20 mm field — it
is dense patches at ~20.5 mm intra-tile spacing (§1.7.1) separated by the 40 mm tile pitch. Non-uniform
sampling does not behave like uniform sampling at the mean density, and `OI-EEGNET-18` must treat the
patch structure explicitly rather than quoting an average.

> **Guard against one misreading, added at Rev 4.** `NP-PWR-BUDGET-001` §3.7 concludes *"do not state
> the emitter count as a capability claim"*, citing `NP-OPT-PSF-001` §3.3's **26.2 mm cortical
> resolution floor** and *"there is little spatial selectivity left to buy below module granularity."*
> **That is an optical result and it does not transfer to this section.** It bounds where *photons*
> land after scattering through skull and cortex; §1.6 is about spatial sampling of a *potential
> field* by volume conduction. The two have different physics and different length scales — 10-20 lines
> sit ~33 mm apart, and the sampling question is whether the scalp field is reconstructable, not
> whether a beam can be aimed. Quoting §3.7 against §1.6 would retire this question for a reason that
> does not apply to it.

**Four limits, stated rather than buried.**

1. **The sampling target is not a project figure.** The commonly cited estimate for adequate scalp-field
   sampling is on the order of 20 mm or finer. Nothing in this document set has established it for this
   geometry, this electrode type or these measures, and it must not be quoted as though it had. Fixing
   it is the first half of `OI-EEGNET-18`.
2. **Reconstruction needs coordinates, and this device does not have them.** Interpolating a virtual
   Fp1 requires knowing where the electrodes sit relative to *nasion and inion*, not relative to the
   helmet. §4.2's first honest limit applies unchanged: the PD2 albedo scheme resolves net-to-helmet
   pose, not net-to-head pose. A dense array does improve the *shape* of the problem — N independent
   placement errors collapse to a single over-determined 6-DOF rigid-body unknown — but it does not
   remove it, and the residual is `OI-EEGNET-14` either way.
3. **It covers recording only.** A virtual sensor can be interpolated; a virtual **current source**
   cannot. The T1-B contact is dual-rated (§0), so for CLAUDE.md §3 modalities 4 and 5, and for T2's
   4×1 HD-tDCS rings, the metal must be where the current is meant to go. The Oz photoparoxysmal gate
   is in the same class — it needs contact impedance at a physical site through a deterministic
   presence primitive (§7.2.4, `OI-EEGNET-06`).
4. **"Different calculations" is a validation burden, not a maths burden.** An interpolated virtual
   channel compared against a normative database built from physically-placed electrodes is an
   equivalence claim requiring evidence. Interpolation error is also worst at array edges — which here
   means Fp and Oz, the two sites carrying the safety gate and the prefrontal montage.

**Why it is recorded even though it is not adopted.** `OI-EEGNET-15` already suspects the ±10 mm
tolerance is modality-dependent. This section says something stronger: for recording modalities the
tolerance may be the wrong *instrument*, and density plus pose measurement may substitute for it. That
is a different question from how tight a mechanical tolerance can be held, and it has never been asked.

### 1.7 Electrodes per tile, and the socket contact budget that gates it

§1.4 established that electrode pitch and tile pitch are different quantities, and that an electrode
may be moved inside its tile. It then treats **one electrode per tile** as given. That is a convention
too, and it is the input §1.6 needs.

#### 1.7.1 What two electrodes per tile would buy

Density is not the strongest argument — §1.6's table shows 2/tile reaches 28.3 mm, short of a ~20 mm
target that would need 4/tile. The stronger argument is registration. `NP-HEX-ZM-001` §3.2 records the
defect as *"the lattice registers to alternate lines at best"*, with 10-20 lines ~33 mm apart. Two pods
at the ±14.5 mm offset that ring-0–1 masking allows (§1.4 table) sit **29.0 mm apart** — 4 mm short of
that line spacing, and closable by the half-module row parity offset. One tile row then reaches two
10-20 rows, which attacks the recorded defect directly rather than by brute sampling.

**Four pods fit, and the intra-tile spacing lands on the target.** Four ⌀11.4 mm pods on a square at
radius 14.51 mm inside the 20.21 mm active field sit **20.5 mm apart** (29.0 mm on the diagonal), well
clear of collision. That intra-tile figure meets §1.6's ~20 mm target locally; what fails is the
40 mm gap *between* tiles.

**The emitter trade has opposite signs on two axes, and the cost axis is the one §1.4/§1.5 use.**
More, smaller pods retain **more** emitters than one large pod — better PBM coverage at electrode
sites — and *more emitters is more BOM*, which inflates term **U**. Against T1-B's current ~44
emitters (one ⌀26.6 mm pod, rings 0–3, `NP-HW-HEXTILE-001` §4.5):

| Pods per T1-B tile | Emitters/tile | Home Standard total | vs 2,286 |
|---|---|---|---|
| 1 large (⌀26.6, today) | ~44 | 2,286 | baseline |
| 1 small (⌀11.4) | ~84 | 2,646 | +15.7 % |
| 2 small | ~77 | 2,583 | +13.0 % |
| 3 small | ~70 | 2,520 | +10.2 % |
| **4 small** | **~63** | **2,457** | **+7.5 %** |
| *(net — T1-B deleted, all T1-A)* | *90* | *2,700* | *+18.1 %* |

> **More electrodes per tile is *cheaper* than fewer**, on the axis that binds. This is the opposite of
> the intuition and it is the strongest single argument for carrying the study to 4: every added pod
> removes ~7 emitters, so 4 pods (+7.5 %) costs less than 2 (+13.0 %) and under half the net (+18.1 %).
> It remains worse than §1.4's in-tile offset (+3.9 %), which adds no pod at all.

All of it is contingent on `OI-HEXTILE-05` and inherits `OI-EEGNET-16` — pod diameter is the
registration budget, the emitter budget *and* the achievable pod count in one unspecified number.

**Recording and stimulating sites can be decoupled above 1/tile.** At one electrode per tile the two
roles are forced onto one dual-rated pad (§0). At 2–4 they need not be: a tile can carry several
recording-only pads and one dual-rated pad. This matters twice — it holds the safety-MCU stimulation
channel count flat while multiplying recording density (§1.7.5), and it is the clean answer to §1.6's
third limit, since the density argument applies only to recording and tES keeps its physical placement.

#### 1.7.2 The gate is the socket contact budget, and it is a force budget

The socket interface carries exactly one electrode net: `ELEC` (pin 13) and `ELEC_SHLD` (pin 14). A
second electrode needs a second pair, and the count is closed at 19 with the 2 reserved positions
explicitly dropped (`NP-DRV-SHELL-002` §5.1.4).

**The obstacle is not area.** `REQ-SKT-01`'s two staggered rows were *"Forced, not preferred"* — forced
by **row length**, not by tile face area. A 19-position single row spans 36 mm; two rows put the longest
row at 10 pads = 18 mm, inside the 20.0 mm budget. Three rows of 10 hold **30 contacts at the same 18 mm
row span**, and a 30-contact three-row array occupies roughly 62 mm² of a 1,385 mm² tile face — about
4.5 %. Twenty-one contacts in *two* rows spans 20 mm, i.e. sits exactly at the limit with no margin;
the same 21 in three rows does not.

**Nor do the rows have to be straight.** A run of contacts may follow the hex — an L, a chevron, or a
polyline at constant offset from the edge. This is worth more than a third row: a chord is the *worst*
path across a hexagon (it is what *"tapers into the corners exactly where the ±0.4 mm lateral tolerance
is hardest to hold"*), whereas an edge-parallel polyline holds constant edge margin over its whole
length, and a hexagonal ring at 16 mm radius offers ~96 mm of path — **~48 positions at 2.00 mm pitch
on a single run.** Row *count* and row *straightness* are both free variables; only contact count is not.

**The obstacle is clamp force, and it is exactly linear in contact count.** `NP-HW-HEXTILE-001` §7.1
gives 0.3–0.5 N per contact → 5.7–9.5 N per module → 34.2–57.0 N per 6-tile plate. Each electrode adds
`ELEC`, and adds `ELEC_SHLD` only if the shield is per-electrode rather than one DRL-driven shield
shared across the tile — a question this document does not answer, and worth 3 contacts at 4 electrodes:

| Electrodes/tile | Contacts (shared shield) | Contacts (per-electrode shield) | **Plate load, shared** | **Plate load, per-electrode** |
|---|---|---|---|---|
| **1 (adopted)** | 19 | 19 | **34.2–57.0 N** | **34.2–57.0 N** |
| 2 | 20 | 21 | 36.0–60.0 N | 37.8–63.0 N |
| 3 | 21 | 23 | 37.8–63.0 N | 41.4–69.0 N |
| **4** | **22** | **25** | **39.6–66.0 N** | **45.0–75.0 N** |

Twenty-five contacts in three rows put the longest row at 9 pads = 16 mm, *inside* today's 18 mm span.
Area never binds; force always does.

**34.2–57.0 N is already the open question**, not a cleared bar: `OI-SHELL2-03(b)` asks whether it is
one-handed-achievable through the §5.4a over-centre actuator at Parkinson's H&Y II–III, against
`RISK-22`. Every added contact pushes further into a question that has not been answered. The design
has also just spent contacts in that currency deliberately — `VLED`/`PGND` went 4+4 → 3+3 explicitly
*"at the cost of two extra contacts on an interface with a live RISK-22 one-handed-force constraint."*

**Two findings about the force question itself, before any lever.**

1. **34.2–57.0 N is contact force only.** `NP-HW-HEXTILE-001` §7.1 computes it as 0.3–0.5 N × 19 × 6
   and nothing else. The clamp additionally compresses the **ejector springs** (`NP-HEX-ZM-001` §5.4a:
   *"releasing it lifts the plate → modules pop on their own ejector springs"*), the **per-tile
   perimeter gaskets** (30 co-moulded gaskets carrying IPX4), and any **plunger preload**. The true
   plate load is higher by an unstated amount.
2. **There is no input-force acceptance number.** §5.4a's ≤1 N is the *retired per-module eject-lever*
   figure, explicitly restated for the cluster actuator as *"low input force via mechanical advantage
   (the RISK-22 low-force intent restated as input force, not per-module extraction)"* — with no value.

> Required MA = plate load ÷ input target, and **neither end of that ratio exists.** Closing both ends
> precedes exploring any lever, and belongs to MECH-2 alongside `OI-SHELL2-03(b)`.

**Levers that buy force headroom without giving up contacts:**

| Lever | Mechanism | Assessment |
|---|---|---|
| **§5.4a actuator mechanical advantage** | §5.4a already gives *"high mechanical advantage near close"* | **Primary.** But MA is high only *near close*, and peak hand force occurs **before** that — the figure that matters is MA at the peak of L(x)/MA(x), not MA at top-dead-centre where it diverges and hand force → 0. Available: link geometry; handle length (package-limited — `OI-SHELL2-11` inter-bowl space, 1.0 mm bezel); a compound two-stage toggle (parts ×18 clusters); and a **shaped profile on the push/pull lever**, which is *not* the twist cam §5.4a prohibits on accessibility grounds — a distinction that must be stated explicitly or the proposal reads as violating a requirement |
| **Force-spread cancellation** ★ | Every contact must meet a minimum force for ≤50 mΩ and fretting resistance. Force varies across the plate, so the **mean** must be raised by the spread to hold the minimum at the worst location: plate load = N × (F_min + spread) | **Real, and the lever this document previously missed.** At 114 contacts, **every 0.05 N of spread removed takes 5.7 N off the plate — 10–17 % of the stated figure, at zero BOM.** The spread is not hypothetical: plate deflection is already a CLUSTER-1 metric (×3.07 for the rejected 8-tile patch), and CONTIG-1 rejects the pendant petal precisely because a compliant arm *"seats at lower force and defeats this section's 'individual, controlled force' property"* |
| **Release force, not throw force** | Over-centre latches must be pushed back over centre | **Possibly the binding case.** `OI-HFE2-05` records that L2's confirm-and-correct loop *"assumes cheap re-opening of a cluster."* Ejector springs ease release and *add* to throw — **a trade that sets RISK-22's two halves against each other and is stated nowhere** |
| Smaller clamp plates | Load ∝ contacts/tile × tiles/plate. This is how 19 contacts came in *below* Rev 1's 18, once the 18-cluster partition capped plates at 6 | Real but expensive: more clusters than 18, which is provably minimal at n=80 under CLUSTER-1/SYM-1/CONTIG-1 — so cluster-controller BOM at $114.12/tier |
| Lower per-contact force | Direct | **Bad lever.** 0.3–0.5 N is already low; below it, fretting and contact resistance run into `SH2-DRC-09`'s resistance→heating bound and `OI-HEXTILE-11`'s unquantified µV-path contact noise |
| ~~Staggered seating heights, in the naive sense~~ | Spread engagement over the stroke to lower the peak | **Not a lever, and for an over-centre it is counterproductive.** Mean-preserved stagger leaves the *endpoint* load identical, and the peak of a monotonic spring load is at the endpoint. Worse: §5.4a's MA is high *near close*, so moving load earlier in the stroke moves it into the **low**-MA region and *raises* peak hand force. The useful stagger is spatial, not temporal — see below |

**Stagger axes, assessed.** The mechanism that works is force-spread cancellation above; the axes
differ in what spread they can cancel and at what scale.

| Axis | Scale | Assessment |
|---|---|---|
| Row-to-row within a tile | ~1.7 mm | **Not a force lever** — deflection over 1.7 mm is negligible. A real **sequencing** lever: `REQ-SKT-01` already places `SEAT#` last-seating, and explicit make/break ordering (AGND and shield first, power next, µV electrode last) guarantees reference before signal and protects the ADS1299 input |
| Centre-to-edge within a tile | ≤18 mm | **Real, at the right scale for tile tilt** — cancels off-centre plunger loading and socket-floor non-parallelism. Magnitude depends on residual tilt after the plunger, which is unstated |
| Tile-to-tile across a cluster | 3–6 tile span — where the largest spread lives | **Rejected as module pad height** — position-specific pads break `R-2`/`SMART-1`; a tile must work in any socket. The per-module plunger already exists for this. The live question is whether **plunger preload should be graded by position within the plate** |
| **Spring-rate grading instead of height** ★ | any | **The strongest variant, and it is absent from the document set.** Springs are on the **socket** (D-5: spring pins on socket, flat pads on module), so grading pogo spring rate is entirely socket-side and leaves `R-2` untouched. Cost: multiple pogo part numbers, position-sensitive socket assembly, and interaction with the force-dependent ≤50 mΩ / ≥500-cycle spec |
| Graded vs stepped | — | Neutral on peak force; a smoother curve is more controllable, which for tremor may matter more than peak, and the formative measures usability rather than force alone |
| Progressive-rate contacts | — | Not stagger, but addresses the same thing: soft early travel, stiff at the end. Part cost across ~1,520 contacts |

> **The two levers are complementary, not alternatives.** Spread cancellation makes the load profile
> **lower and deterministic**; a shaped push/pull profile then **flattens hand force** against that
> known curve. Neither works alone — MA cannot be shaped against an unknown load, and a lower load does
> not help if MA is mismatched to its shape.

#### 1.7.3 What a third row — or a non-straight one — changes, in both directions

**Better — µV siting, and a non-straight array is much better than a third row.** Today
`ELEC`/`ELEC_SHLD` share a two-row array with 3 × `VLED` at 24 V (0.35 A/pin nominal, 0.52 A on loss of
one contact). A third row buys ~1.7 mm of separation; an edge-following array buys up to the tile's full
width, letting the µV pins sit on one hex face and the power contacts on the opposite one. Given
`OI-HEXTILE-11` — pogo contact noise in a µV chain is *not* covered by the ≤50 mΩ resistance spec —
this is an argument for re-shaping the array **at constant contact count**, independent of any
electrode-count decision.

**Worse — spreading the array tightens the *rotational* tolerance, and nothing has priced this.** The
±0.4 mm lateral blind-mate budget is a displacement, and a tile rotation θ displaces a contact by R·θ at
radius R from the array centroid. Today's ~18 × 1.7 mm array has R ≲ 9 mm, so 0.4 mm absorbs ~2.5° of
rotation; an edge-following ring at R = 16 mm absorbs only ~1.4°. **Spreading the array roughly halves
the angular tolerance the mechanical key must hold**, and no document in the set states what that key
actually holds. This is the cost of the µV-siting benefit above, it is new, and it applies to any
non-compact array whether or not the contact count changes.

**Mixed — the mis-key asymmetry stops being free, but a shaped array can do better than a count.** `NP-HW-HEXTILE-001` §7.1 notes that the two-row
layout *"makes this easier, not harder: a row-length difference (9 vs 10) is itself an asymmetry"*,
carrying the fail-open geometry that stops a mis-keyed insertion making contact. A symmetric 10+10+10
loses that; 9+10+11 keeps it, and uneven row spacing or an offset middle row provides a *second*
asymmetry axis that two rows do not have. A deliberately **chiral** run — an L with unequal legs, or a
chevron — carries the fail-open geometry far more robustly than any row-length difference, because it
fails under rotation rather than under a miscount. `HT-DRC-22` and `SH2-DRC-05a` would both need re-verification,
and so would the claim that the two-row layout is what keeps the ±0.4 mm lateral blind-mate tolerance
holdable.

Note this is the **fail-open backstop**, not the primary control. Orientation is enforced mechanically
(`NP-HFE-002` §7.4, `HFE-R-05`: a wrongly-oriented module must *fail to seat*, not seat poorly). Both
layers are deliberate and both must survive any row-count change.

#### 1.7.4 Uniform or mixed — and the HFE cost of mixing, priced

Two variants must not be conflated:

| Variant | What it is | Verdict |
|---|---|---|
| **Uniform** ★ | Every T1-B carries two electrodes | **The one to evaluate.** Type set stays at 4; `R-2` intact — any T1-B in any socket; no user instruction |
| **Mixed** | Distinct 1-electrode and 2-electrode types, placed per instruction | Viable, but pays two nameable HFE costs below. Not dismissed — priced |

**Module-type identification already exists and is not the obstacle.** `NP-HFE-002` §7.3 specifies L3
module-type tactile marking on the outward non-emitting face — 1/2/3/4 raised bars for
T1-A/T1-B/T1-C/T2-D, with *"ISO 17049 braille … co-moulded in addition, never instead"*. `C-1` forbids
type-differentiating **mechanical keying** (mating geometry that blocks insertion); a marking on a
non-mating face is expressly not a key. So "the types are indistinguishable" is **not** an available
objection to mixing, and this document should not make it.

The two real costs are:

1. **A fifth type sits at the edge of the stated reliable range — but this cost is contingent on the
   L3 *encoding*, not on the type count.** §7.3 rests on *"bar count to 4 is well inside the reliable
   range"*, and §2.3(a) puts tactile counting at *"reliable to about 4–5 and degrades fast beyond that"*.
   That ceiling belongs to **counting**. A shape encoding is not counted, and `NP-HFE-002` Rev 2 §7.3
   now carries one — a nested figure reaching 6 types with a two-stage decision of branching ≤3.
   **Under that encoding this cost largely dissolves**, leaving cost 2 as the real one. `OI-HFE2-10`.
2. **L1(d) stops being one rule — this is the larger cost.** §7.1(d) is a *single uniform* feature at
   ~9 instances, deliberately not identifying which site, whose entire value is collapsing the common
   build to *"place an EEG module wherever you feel the dimple, base modules everywhere else."* Two
   electrode-bearing types break that rule and push work back onto §7.1(a)–(c) counting. **Uniform
   preserves L1(d) exactly.**

**Direction conflict worth recording.** §7.1.4 counts deleting T1-B as an accessibility gain — L1(d)
deleted, *"9 positions that matter"* → zero. §1.7 pushes the opposite way and makes L1(d) more
load-bearing. Both cannot be pursued; whichever is adopted must say so explicitly rather than leaving
`NP-HFE-002` §7.1(d) pointing at a premise that no longer holds.

#### 1.7.5 Consequences that must be priced by their owning documents, not here

| Consequence | Owner | Note |
|---|---|---|
| **ADC channel count is the true ceiling, and it is the same constraint as sampling density** | `NP-HW-HUB-001` §5 | N4 muxes any socket's `ELEC` onto **N** channels — 8 (T1) / 21 (T2), set by the ADS1299 bank. §1.6 shows the ~20 mm target needs ~120 electrodes: **~15× T1's N and ~5.7× T2's.** More electrodes buy nothing without more N, and N is the more expensive half. 16-channel T1 is 2 × ADS1299. `OI-EEGNET-19` is undecidable without a position on N |
| Safety-MCU load depends on how many electrodes are **dual-rated**, not how many exist | `NP-HW-HUB-001` §7.2 | §1.7.1's decoupling lever: recording-only pads cost the safety MCU nothing. Four electrodes per tile with one dual-rated pad leaves the stimulation channel count unchanged |
| Per-cluster electrode mux width doubles | `NP-DRV-SHELL-002` §3.5 | 6 tiles → up to 12 electrode inputs per cluster. Tail conductor count is set by shared-lane count, not by socket count, so the tail impact is modest — but it is not zero and is not derived here |
| Safety-MCU frame growth | `NP-HW-HUB-001` §7.2 | Enable-bit position ≡ charge-monitor channel index, and the 38-byte extended heartbeat has **zero spare bytes**. Doubling *stimulating* electrodes moves `NP_SAFETY_RX_EXT_FRAME_LEN`, both checksum spans, the size assertion and eight `offsetof` assertions — a Class C wire-format change |
| Socket array BOM | `NP-DRV-SHELL-002` §10.1 | $0.40–0.80 per **socket array** of 19 contacts, $32–64 per headset at 80 sockets. If price scales with contact count, 21 contacts is **+$3–7** and 30 contacts **+$19–37** per headset — against a configuration at −41 % to −51 % gross margin |
| Tile-face area is *not* a constraint | — | Recorded so it is not re-raised: a 30-contact three-row array is ~4.5 % of the tile face |

> **Nothing in §1.6 or §1.7 is a cost saving, and no figure here may be entered into `NP-COST-001`.**
> Same rule as §7.2.2 — that document owns the re-derivation and must do it as a whole.

### 1.8 Why so few sockets are populated — and the tile type that is missing

**The premise of §1.6 and §1.7 is that electrode sites are scarce. They are scarce by assumption, not
by decision.** `NP-COST-001` §2 A-1 is unambiguous:

> *"There is **no per-configuration tile population** in the document set. Searching every controlled
> document for the configuration names returns two hits, both of which merely cite the $405 Home
> Standard BOM in passing. Neither allocates tiles to a configuration. **This is the input the whole
> model depends on, and it has never been decided."* — `OI-COST-01`

Home Standard's 30 tiles is a figure a cost model adopted because it needed one, assembled from
`NP-HEX-ZM-001` §4a (*"~8–9 × T1-B … + the balance in T1-A"*) and `NP-HW-HEXTILE-001` §6.4 option 1
(*"a build populating 20–30 tiles retains full protocol flexibility"*).

#### 1.8.1 The only argument on record against full population is about emitters

`NP-HW-HEXTILE-001` §6.4 option 1:

> *"**Do not populate all sockets.** §9 shows the power envelope permits only ~5–6 tiles to run
> concurrently regardless; the lattice's value is placement freedom, not simultaneous activation."*

**⚠ The "~6 tiles" figure is superseded, and the correction strengthens this section.**
`NP-SES-PWR-001` §2, auditing `protocols/predefined/` rather than arguing from one operating point,
finds ~6 is *"an artefact of one operating point that no authored protocol uses"* — real per-tile draw
spans **1.3–20.0 W**, so concurrency spans **2–32 tiles**, and a tile-count governor is wrong in both
directions (it forbids 80 tiles at 30 W and permits 6 at 150 W). `NP-PWR-BUDGET-001` §3.5 now
**states directly what §1.8 had to infer** — *"records that populated != driven"* — and
`NP-HW-HEXTILE-001` §9.3's fourth consequence retires *"placement options, not capability"*, since all
80 tiles can be lit at ~1.5 % drive for ~30 W. The original figure is retained below because it is what
§6.4's argument was built on.

That is a **power** argument and it is sound on its own terms — §9 gives T1 peak 45–50 W, ~6–8 W of
non-PBM overhead, ~38–42 W available to emitters, and 25.0 W for one T1-A at full dual-channel drive,
hence ~6 concurrent tiles at 25 % duty.

> **An electrode draws microamps.** The concurrency ceiling — the *only* stated reason not to fill the
> lattice — does not reach the recording half at all. An argument made about emitters has been
> inherited by a decision about electrodes, and §1.6/§1.7 inherited it in turn.

#### 1.8.2 The lattice is paid for in every configuration regardless

`NP-COST-001` §2 A-2: the L1 carrier is laminated with **all ~80 sockets and all 18 cluster
controllers in every configuration, including Core.** The socket spring-contact arrays ($32–64) and
the cluster-controller tier ($114.12) are sunk whether 4 tiles are populated or 80. Configurations
differ *only* in tiles.

**So the marginal cost of filling an empty socket is exactly one tile** — and per-tile cost is
dominated by things an electrode does not use: `NP-HW-HEXTILE-001` §6.4's **$11.53/tile of driver and
metering, ~$10 of it two InGaAs photodiodes** (`OI-HEXTILE-06`), plus emitters, which are uncosted and
unselected (term **U**, `OI-HEXTILE-02`).

#### 1.8.3 T1-B is a PBM tile with a hole in it

Every electrode site today drags a full PBM tile's cost behind it. T1-B is *"a masking derivation"*
(§4.5) — ~44 emitters, an LED driver, an InGaAs pair, dose metering and an NTC throttle, all to host
one Ag/AgCl pad. **There is no electrode-only tile type in the taxonomy.**

Such a type — call it **T1-E** — would carry the pad, the spring pod, and a cents-level UID responder
so `np_module_map` can inventory it. It preserves `NP-HEX-ZM-001` §4a's invariant *more* easily than
T1-B does: one mould, one outline, one socket interface, *"only a different placement file"* — with
nothing to mask, because there is nothing to remove.

| Build | Tiles | Emitters | vs 2,286 | Electrode sites | Driver + metering on electrode tiles |
|---|---|---|---|---|---|
| **Today** — 21 T1-A + 9 T1-B | 30 | 2,286 | baseline | 9 | 9 × $11.53 = **$104** |
| 21 T1-A + 9 T1-E | 30 | **1,890** | **−17.3 %** | 9 | ~$0 |
| 21 T1-A + 20 T1-E | 41 | **1,890** | **−17.3 %** | **20** | ~$0 |

> **This is the only option examined anywhere in §§1.6–1.8 that moves term U in the right direction.**
> Every other variant inflates it — 2 electrodes/tile +13.0 %, 4/tile +7.5 %, the net +18.1 %, even
> §1.4's in-tile offset +3.9 %. T1-E cuts it **17 %** while more than doubling electrode sites, and at
> constant electrode count it strictly dominates T1-B on both cost terms.

#### 1.8.4 Four costs, stated

1. **PBM coverage at electrode sites falls to zero — and Rev 4 can now say exactly what that costs.**
   T1-B delivers ~44 emitters, about 49 % of a T1-A; T1-E delivers none. `NP-PWR-BUDGET-001` §3.7
   supplies the frame Rev 3 lacked: **coverage is the only one of the three quantities that scales with
   tile count.** Local irradiance is set by the tile and total optical output is capped by the PD
   envelope at ~13–14 W optical *whatever the population*. So T1-E costs **illuminable area at those
   sites and nothing else** — it cannot reduce deliverable dose, because dose was never population-bound.
   The trade is therefore specifically against §3.6's whole-vault **coverage** mode (all 80 tiles,
   ~1.5 % drive, ~30 W), not against focal protocols, where irradiance is the whole ballgame and an
   unlit electrode site is not in the montage anyway. Still `OI-EEGNET-21`, and still a proposal —
   but the question is now quantitative rather than open-ended.
2. **It is a fifth tile type**, which lands on `NP-HFE-002` §7.3's L3 marking at exactly the point
   §2.3(a) puts tactile counting at *"reliable to about 4–5"*. It therefore depends on the L3 encoding
   question raised at §1.7.4 and now carried by `OI-HFE2-10`.
3. **It does not fix N.** Twenty electrode sites with N = 8 still records 8 (§1.7.5). T1-E makes
   *sites* cheap, not *channels*, so §1.6's ceiling stands and becomes unambiguously the binding one.
4. **Mass, assembly time and per-tile gasket seam length** all scale with population and none is
   stated anywhere — the same gap `OI-COST-01` records for population itself.

> **No figure in §1.8 may be entered into `NP-COST-001`.** Same rule as §7.2.2 and §1.7.5: that
> document owns the re-derivation, and `OI-COST-01` must be decided there, not inferred here.

### 1.9 What else a four-pod tile can carry (NEW AT REV 4)

§1.7 asks how many electrodes a tile should hold. This section asks the complement: **at four pods,
what is left, and what should occupy it?** It is answered against the two constraints that decide it —
what the protocol library actually demands (`NP-SES-PWR-001`) and what the power envelope affords
(`NP-PWR-BUDGET-001` Rev 3) — both of which post-date Rev 3.

#### 1.9.1 What remains, measured

| | Value | Basis |
|---|---|---|
| Emitter sites remaining | **62 of 90** (~69 % of a T1-A) | 91-site lattice − site 0 reserved − 4 × 7-site pod neighbourhoods (`NP-HW-HEXTILE-001` D-1, §4.5) |
| Pod footprint | 408 mm² of a 1,385 mm² tile face | 4 × ⌀11.4 mm, contingent on `OI-HEXTILE-05` |
| Free socket contacts | **zero** | 19 positions, closed, *"2 reserved dropped"* (`NP-DRV-SHELL-002` §5.1.4) |
| Free power | effectively **all of it** | An electrode draws µA; a photodiode is passive |

**The binding constraint is not area, and not power. It is the socket contact budget** — the same wall
§1.7.2 hits, and for the same reason: anything that produces a signal has to leave the tile.

#### 1.9.2 What the protocol library asks for — and it is not more emitters

`NP-SES-PWR-001`'s largest finding is *"not a power finding"*: protocols **target lobe-scale zones
where their own cited evidence specifies electrode-scale sites** — the depression protocol irradiates
37 sockets in service of a *bilateral DLPFC (F3/F4)* indication, and *"no DLPFC, F3/F4 or single-site
zone exists."* The remedy is a data edit gated on **REG-1** (`OI-SESPWR-01`), and it takes the
depression protocol from 740 W to ~50 W.

**Two consequences for this section.** First, the library's unmet need is *finer targeting*, which a
four-pod tile does **not** supply — `NP-OPT-PSF-001` §3.2 gives one 40 mm tile a 40.0 mm FWHM at
cortex, so the tile is already approximately the optical resolution unit and *"targeting below one tile
buys nothing."* Second, `OI-SESPWR-01` and `OI-EEGNET-14` are **the same REG-1 dependency reached from
opposite directions** — PBM zone authorship and EEG electrode registration both wait on socket-to-10-20
registration. Neither document says so.

#### 1.9.3 Candidates, assessed

| Candidate | Power | Verdict |
|---|---|---|
| **More emitters** | envelope-capped | **No.** §3.7: adding emitters buys *irradiance*, not precision, and total optical output is ~13–14 W *"whatever the tile population."* The 28 sites the pods take cannot be bought back by populating others |
| **1064 nm (T1-C function) on the same tile** | — | **No, and this is now settled.** `OI-HEXTILE-21`: CH_C reaches 28 mW/cm² against its own Grade A protocol's 0.25 W/cm² — **9× short**, and a hypothetical 90-site 1064-only tile still falls 3× short. *"An η_wp ≈ 4.8 % emitter wall, not a power or layout shortfall."* Fewer sites on a shared tile makes it worse |
| **A second NTC / thermal sensing** | negligible | **No need.** The per-tile NTC and the 62 °C junction throttle already own local thermal, and `NP-HW-HEXTILE-001` §9.3 established the risk is local, not aggregate |
| **Long-separation fNIRS detector** ★ | passive | **The one candidate the geometry favours — see §1.9.4** |
| **Nothing — leave the sites unpopulated** | — | **A serious answer, not a null one.** Every T1 configuration is gross-margin negative and term **U** is the dominant uncosted risk (`NP-COST-001` §2). Depopulating is the only option here that helps it |

#### 1.9.4 The four-pod geometry lands an fNIRS source–detector pair in its window, for free

`NP-FEAS-FNIRS-001` finds the modality plausible on existing optics and names the blocker as
**geometry**: fNIRS needs a source–detector separation of **2.5–3.5 cm**, and *"PD1/PD2 are co-located
with the LED array to measure near-field backscatter (mm depth) — exactly the wrong geometry."* Its
proposed bench step is to drive one tile and read a **neighbouring** tile's scalp-facing PD.

Put the §1.7.1 pod arithmetic against that window:

| Separation available | Distance | In the 25–35 mm window? |
|---|---|---|
| Adjacent pods, four-pod tile | 20.5 mm | No — too short |
| **Diagonal pods, four-pod tile** | **29.0 mm** | **Yes** |
| Nearest neighbouring tile (lattice pitch) | 40.0 mm | No — too long |

> **The four-pod layout is the first geometry in the design that places a source and a detector at an
> fNIRS-appropriate separation *within a single tile*.** The cross-tile separation the feasibility study
> proposes overshoots the window; the intra-tile diagonal sits in it. Nothing was designed for this —
> it falls out of a pod placement chosen for 10-20 registration.

**Three reasons this is a candidate and not a recommendation.**

1. **D-2 conflicts.** PD1/PD2 co-location is required *"for the PD1/PD2 fouling-vs-ageing ratio to be
   valid"* (`NP-HW-HEXTILE-001` D-2). An fNIRS detector at a pod position is therefore a **third** PD,
   not a relocated one.
2. **A third PD needs conductors that do not exist.** Back to §1.9.1's binding constraint and to
   `OI-EEGNET-20` / `OI-HEXTILE-20`. This is the third distinct consumer of the same closed contact
   budget, after the second electrode and its shield.
3. **The wavelength objection is untouched by geometry.** `NP-FEAS-FNIRS-001` Risk A is that
   808–830 nm *"sits on/near the isosbestic point — the worst place"* for oxy/deoxy separation. A
   better separation distance does not fix a chromophore problem, and nothing here claims it does.

**One convergence worth recording rather than claiming.** `NP-PWR-BUDGET-001` §3.6's whole-vault mode —
all 80 tiles at ~1.5 % drive, ~30 W — is explicitly *coverage, not dose*, delivering 7.2 J/cm² against
a ≥10 J/cm² threshold. That is a limitation for PBM. For **monitoring**, sub-therapeutic whole-vault
illumination is not a limitation but the desired condition. Whether the two uses can share one mode is
`OI-EEGNET-22`, and `OI-PWR-07` already asks whether the mode is a product feature at all.

#### 1.9.5 The answer, stated plainly

**Space is not the scarce resource on a four-pod tile; socket contacts are.** 62 emitter sites and
~977 mm² remain, and the power envelope is indifferent to everything proposed here because electrodes
and photodiodes are microamp-and-passive. Of the five candidates, three are ruled out by the physics
already documented, one — depopulation — is the only one that helps the margin problem, and one —
fNIRS — has a geometry the four-pod layout supplies for free and a **wiring** problem it does not.

> **Every additional signal-producing element on a tile is a claim on the same 19 closed contacts.**
> The second electrode (§1.7.2), its shield, and an fNIRS detector are three claimants on a budget that
> `OI-HEXTILE-20` has independently reopened and `OI-SHELL2-09(i)` will close at socket tooling. They
> should be counted **once, together**, not discovered one at a time.

### 1.10 Variable pod population — manufactured variants vs one selectable tile (NEW AT REV 5)

§1.7 asks how many pods a tile should carry, on the assumption the answer is one number. It need not
be: pod count and pod position could vary by socket. This section works that through, and it opens by
correcting something §1.4 has asserted since Rev 2.

#### 1.10.1 The correction §1.4 owes — per-site placement files already mean position-keyed parts

§1.4 presents the in-tile offset as *"a zero-tooling, zero-BOM change: a per-site placement file"*, and
§1.5 concludes the §0 dual-rated fork *"dissolves"*. **Both under-state what a per-site placement file
is.** §1.3's own worked examples require **different** offsets at different sockets — Oz displaced
~18 mm posterior inside socket 74, Fp1 and Fp2 displaced ±18 mm in two adjacent tiles. A placement file
that differs by site means **T1-B is not one part; it is one part per electrode site.**

> **`R-2` — "any type in any socket" — was already broken by §1.4, and Rev 3 did not say so.** The
> variable-pod proposal in this section does not introduce position-dependence into the design. It
> inherits it, makes it explicit, and adds a count axis. That is a change in degree, not in kind, and
> §1.7.4's costing of the "mixed" variant was written as though the alternative were position-free.

#### 1.10.2 Two ways to get variable pod placement, and they are not close

| | **A — manufactured variants** | **B — one universal tile, pods selected** |
|---|---|---|
| Parts | one per (count, position) combination | **one** |
| Tile types in the taxonomy | **7–8** (T1-B1…B4 + A/C/D/E) | **4–5** — unchanged from today |
| `NP-HFE-002` §7.3 encoding | **exceeds** the nested figure's 6 | fits with headroom |
| `R-2` (any type in any socket) | broken, per-position | **intact** |
| User placement instruction | required, per socket | **none** |
| Registration | continuous within ±14.5 mm | **discrete** — best of N (§1.10.4) |
| Socket contacts | pays only for pods present | **1–2 regardless of N**, but only under §1.10.3 |

**B is the better structure on every axis except registration precision**, and the type-count column is
the decisive one: A pushes the taxonomy past the encoding `NP-HFE-002` Rev 2 just adopted, while B
leaves it where it is.

#### 1.10.3 Why the on-tile selector objection does not carry here

§1.7.2 rejected an on-tile mux, and that rejection was about a different mechanism. It read: a mux
gives *N sites time-shared on one channel*, destroying simultaneity for coherence, phase and sLORETA.

**Selection here is static per build** — set at fit, held for the session. No switching in the µV band,
no simultaneity loss, and `D-3` already fits a driver MCU to every tile, so an I2C-addressable latch
exists. The earlier objection does not transfer.

What does transfer, and must be characterised rather than assumed: **switch Ron and leakage sit in the
µV path in the worst EMI location on the tile** (`SH2-DRC-16`). Note the test already exists one level
up — `SH2-DRC-27` is *"electrode mux leakage and Ron mismatch impact on EEG CMRR"* for the per-cluster
mux — so this is a re-run at a new location, not a new characterisation.

**Wiring every pod out instead is the worst option** and should be recorded as rejected: it pays the
maximum contact cost at every electrode site whether or not the pods are used, against a 19-position
budget with *"2 reserved dropped"*.

#### 1.10.4 Pod count is set by angular quantisation, and four is too few

Selecting the nearest of N pods on a ring of radius r = 14.51 mm quantises position; worst case is a
target landing between two pods, at 2·r·sin(π/2N). Decomposed against §3.4's ±10 mm and §1.1's 3.4 mm
size-plus-shape term:

| N | Quantisation | Left for all else | After shape | ≈ seating tolerance | Emitters | Scalp load ×9 tiles |
|---|---|---|---|---|---|---|
| 4 | **11.1 mm** | 0.0 mm | 0.0 mm | **0.0 mm** | 62/90 | 2.9–4.3 kg |
| **5** | **9.0 mm** | 4.4 mm | 2.8 mm | **3.1 mm** | 55/90 | 3.6–5.4 kg |
| **6** | 7.5 mm | 6.6 mm | 5.7 mm | **6.3 mm** | 48/90 | 4.3–6.5 kg |
| 8 | 5.7 mm | 8.2 mm | 7.5 mm | **8.3 mm** | 34/90 | 5.8–8.6 kg |

> **⚠ Corrected at Rev 6 — every figure in this table is a LOWER BOUND, not the residual.** The formula
> counts angular error at ring radius r and nothing else: it assumes the target sits *at* radius r
> (ignoring radial mismatch) and that a target is served only by its own tile's pods. §1.10.5 computes
> the honest quantity and it is worse throughout — ring4 **12.7 mm**, ring5 **12.5**, ring6 **11.6**,
> ring8 **10.4**. The table is retained because the *shape* of the argument — quantisation rises sharply
> as N falls — survives, and because it is what Rev 5 reasoned from.

**Four evenly-spaced pods blows the budget on quantisation alone**, before seating, shape or landmark
error. **Five clears it at 9.0 mm but spends 90 % of the budget doing so**, leaving ~3.1 mm of seating
tolerance against §1.1's own worked example of a 10 mm sunk head producing 9.1 mm at Oz.

> **Five is the count that makes the structure contingent on a measurement nobody has taken.** Six has
> margin whichever way `OI-EEGNET-14` resolves; five does not. That is an argument for sequencing, not
> against five — five cannot be *chosen* before concentricity is measured, and six can.

Two structural properties of odd N were checked and are not objections. A regular pentagon has no
antipodal pair, but its longest chord is **27.6 mm**, still inside `NP-FEAS-FNIRS-001`'s 25–35 mm
window, so §1.9.4 survives at five (the hexagon's antipodal pair is 29.0 mm). And because tiles mount
in one fixed orientation, left/right pod positions are *translated*, not mirrored — bilateral symmetry
for F3/F4, C3/C4, P3/P4 is obtained at either parity by fixing the pattern's phase so a mirror axis
runs sagittally.

#### 1.10.5 Centre-plus-ring — and the optimisation that actually decides this

**Every figure in §1.10.4 assumes one ring, and that is the wrong pattern to assume.** `D-1` already
reserves **site 0 at the tile centre**, described on T1-B as *"the electrode pod axis"* — the current
design's single pod is a centre pod. The natural five is therefore **centre + four on a ring**, which
reuses the reserved site and changes the problem qualitatively: a target near the tile centre is served
*exactly*, and quantisation applies only to off-centre targets. It is a 2D covering problem, not a 1D
angular one, and it is strictly better than a regular pentagon at the same count.

**Computed at Rev 6 — and it does not do what this section expected.** `scripts/pod-pattern-coverage.ts`
evaluates the **lattice covering radius**: for every point on the lattice-covered scalp, the distance to
the nearest pod position available anywhere on the lattice, assuming the electrode tile may go at
whichever socket suits best. This is montage-independent, so **it does not wait on REG-1** — which a
fit against actual 10-20 coordinates would. Ring radius is optimised per pattern.

| Pattern | Pods | Best r | **Worst case** | p95 | Emitters | Meets ±10 mm? **(T1 only — see §1.10.6)** |
|---|---|---|---|---|---|---|
| *centre only (today)* | *1* | — | *20.0 mm* | *19.0* | *83/90* | *no* |
| ring4 | 4 | 12.5 mm | 12.7 mm | 10.3 | 62/90 | no |
| centre+ring3 | 4 | 10.0 mm | **17.3 mm** | 13.2 | 62/90 | no |
| ring5 | 5 | 12.0 mm | 12.5 mm | 9.7 | 55/90 | no |
| centre+ring4 | 5 | 14.0 mm | 12.6 mm | 10.7 | 55/90 | no |
| ring6 | 6 | 11.5 mm | 11.6 mm | 9.4 | 48/90 | no |
| centre+ring5 | 6 | 14.5 mm | 11.9 mm | 8.9 | 48/90 | no |
| ring8 | 8 | 10.0 mm | 10.4 mm | 9.6 | 34/90 | no |
| **centre+ring7** | **8** | 14.5 mm | **9.4 mm** | 7.4 | **34/90** | **yes** |

**Three results, and two of them contradict what this section assumed.**

1. **Centre-plus-ring is not strictly better.** At N = 4 it is far *worse* than a plain ring
   (17.3 vs 12.7 mm) — the centre pod is redundant against neighbouring tiles while the ring it thins
   is what covers the gaps between them. It ties at N = 5 and wins only from N ≈ 6, decisively at N = 8.
   **The Rev 5 claim that it is "strictly better at the same count" is withdrawn.**
2. **Pattern optimisation does not rescue five pods.** Five is 12.5–12.6 mm however the pods are
   arranged, against a ±10 mm budget — worse than the 9.0 mm §1.10.4 predicted, not better.
3. **Only 8 pods meet ±10 mm worst-case, and only in the centre+ring form**, at 34/90 emitters —
   *and that is covering error alone*, before §1.1's shape term, seating concentricity or landmark
   error are added. On an RSS basis nothing in the table survives the full budget.

> **⚠ Provenance, added at Rev 8 (`docs/status/pr-defect-retrospective.md` §2.4).** The ±10 mm these patterns are eliminated
> against is a **`[design-target]`**, not a measurement: §3.4 derives it from the ~33 mm 10-20 line
> spacing (`NP-HEX-ZM-001` §3.2) and states it is *"a design tolerance, not a clinical claim."* The
> covering-radius figures themselves are **`[measured]`** — computed by `scripts/pod-pattern-coverage.ts`
> — but over geometry that is **`[estimate]`**, the interim ellipsoid `hardware/np_socket_map.json`
> calls *"a description, not a fact."* **No pattern here is eliminated by a datasheet or a measured
> anatomical bound.**

> **⚠ Scoped at Rev 7.** The ±10 mm column is a **T1** answer, and the whole table is a **per-target**
> bound. §1.10.6 supplies both qualifications and one term that runs the other way for T2.

> **The finding is against discrete selection generally, not against five.** Picking the nearest of N
> fixed positions carries an irreducible quantisation term, and it does not fall below the budget at any
> pod count that leaves a useful emitter population. **§1.4's continuous per-site offset has no such
> term** — which returns the question to §1.10.1, where §1.4 was already shown to imply position-keyed
> parts. The real trade is *quantisation error* against *part-count*, and Rev 6 prices the first side.

**Two honest limits.** The geometry is the **interim ellipsoid**, which its own generator calls *"a
description, not a fact"* and marks PROVISIONAL pending REG-1/ACT-1, so absolute residuals inherit that
status — though the *ranking* of patterns is far more robust than the absolute numbers. And the covering
radius answers *"any target anywhere"*; a **montage-specific** fit against nine known sites would do
better, and the p95 column (7.4–10.3 mm) indicates how much better. That fit needs REG-1 and is
**`OI-EEGNET-27`**.

#### 1.10.6 Two scope limits on §1.10.5, and the term that runs the other way (NEW AT REV 7)

§1.10.5 evaluated coverage against **every point on the lattice-covered scalp** and used no montage at
all. That was deliberate — montage-independence is what let it run without REG-1 — and it means the
bound holds for T1's nine sites, T2's twenty-one, and any montage authored later. But two things it
does not say were read into it, and a third was not modelled.

**(a) The ±10 mm column is a T1 answer.** `OI-EEGNET-15` already records that placement tolerance is
*"plausibly modality-dependent — looser for T1 8-channel wellness neurofeedback, tighter for T2 sLORETA
source localisation and HD-tDCS 4×1 targeting."* §1.10.5 evaluated against a single ±10 mm figure, which
is §3.4's design tolerance. **On any tighter T2 budget every row of that table fails, centre+ring7
included** — its 9.4 mm has 0.6 mm of margin against a number that was never claimed to be T2's.

**(b) The covering radius is a per-target bound, not a whole-montage guarantee.** It asks: for this
point, is there *some* socket whose tile can reach it. A montage asks a harder question — can **all**
sites be served **at once**, when each socket carries one tile. That is a matching problem, not a
covering one. T2's 21 sites sit ~33 mm apart on a 40 mm lattice, so neighbouring 10-20 sites frequently
fall nearest the **same** socket.

**(c) Contention is exactly what multi-pod tiles resolve — and §1.10.5's framing cannot see it.** A
1-pod tile serves one of two contending sites; a multi-pod tile serves both from one socket. What is
computable without 10-20 coordinates is the **assignment slack**: how many *distinct* sockets can serve
a given scalp point (`scripts/pod-pattern-coverage.ts`, second table):

| Pattern | Mean sockets per point (±10 mm) | Served by **one** socket only | at ±15 mm |
|---|---|---|---|
| **centre only (today)** | **1.00** | **~100 %** | 98.0 % |
| ring5 | 1.17 | 83.9 % | 48.8 % |
| centre+ring5 | 1.26 | 75.4 % | 35.3 % |
| centre+ring7 | 1.33 | 68.7 % | 30.2 % |

> **Today's single centre pod has no assignment slack whatever.** Essentially every scalp point is
> reachable from exactly one socket, so any two montage sites contending for that socket means one of
> them **cannot be placed at all** — not placed imprecisely, not placed. That is `§1.3`'s Fp1/Fp2 defect
> stated as a general property rather than as one anomaly: *"Fp1 and Fp2 cannot each have their own
> socket."*

**Consequence for Rev 6's conclusion.** *"The finding is against discrete pod selection generally"* is
**sound for a sparse montage and overstated for a dense one.** Multi-pod loses on covering radius and
wins on contention, and the second effect grows with montage density — so the balance is least
favourable at T1's nine sites and most favourable at T2's twenty-one. Rev 6 measured only the axis where
multi-pod loses.

**What is out of scope of the analysis, as the design currently stands.** A1/A2 sit on the VNS clip's
contact pads over the existing 6-pin cable (CLAUDE.md §3 modality 6), not on the lattice, so they were
never candidates in either computation. **That is a property of the present design, not a necessity** —
whether the lattice should extend to cover the A1/A2 positions is a live question and is not settled
here.

**What is still not computed.** The whole-montage assignment problem — can all N sites be served
simultaneously, and at what worst-case residual — is a matching problem over (site → socket → pod) and
**does** need 10-20 coordinates. **`OI-EEGNET-28`**, paired with `OI-EEGNET-27`; both wait on REG-1, and
both would be answered by the same fixture.

#### 1.10.7 What universality does not make cheaper

Two costs are *worse* under option B than under manufactured variants, and both are paid at every
electrode site:

- **Emitter loss becomes uniform.** A 1-pod variant keeps 84 emitters; a universal N-pod tile keeps
  90 − 7N at every electrode socket regardless of how many pods that socket uses. Term **U** again.
- **Scalp contact load multiplies.** Each pod contacts at 80–120 g (CLAUDE.md §4.4) whether selected or
  not — **4.3–6.5 kg across nine electrode tiles at N = 6**, against 0.7–1.1 kg today. This is a
  fit-system and comfort question that has not been asked of anyone. Retracting unused pods would fix
  it and would reintroduce a mechanical variant, defeating the point.

> **At N ≥ 6 a universal electrode tile retains ≤53 % of its emitters, and the PBM value at those sites
> becomes marginal.** That is `OI-EEGNET-21`'s premise reached from the opposite direction: if this
> option is adopted at six or more, the question stops being *how much PBM do we keep at electrode
> sites* and becomes *why keep any*. **`OI-EEGNET-21` and `OI-EEGNET-23` should be decided together.**

#### 1.10.8 Identification, the placement gate, and the build map

**Pod count needs no marking — it is directly palpable.** The pods are physical objects on the
scalp-facing face, and counting to four is inside `NP-HFE-002` §2.3(a)'s reliable range. It is
*self-demonstrating* rather than coded, so nothing is learned and nothing is added to L3, whose nested
figure keeps encoding **family**. This is what keeps option A's 7–8 types from being an encoding
problem — and it does **not** rescue option A, because pod *position* is not palpable: two 2-pod tiles
with different offsets are indistinguishable by touch and by eye. Under option B the question does not
arise, since there is one part.

**The placement gate cannot express any of this today.** `np_module_map_check_placement()` filters on
`type_mask` — *"at least one element whose type is in `type_mask`"* — which is an **element-type**
predicate. A 1-pod and a 4-pod tile both satisfy *"dual electrode at this socket"*, so the gate would
**pass a wrong build silently**, including at socket 74 where the photoparoxysmal halt depends on it.
The fix is a count/geometry field in the requirement, not new entries in the element enum, which would
pollute a type system to carry a quantity. This is the same weakness §7.2.4 identifies for the net,
reached from a different direction. **`OI-EEGNET-26`**, routed to `NP-HEX-ZM-001` §4a as the owner of
the identity model.

**A required *build* map is a third kind of data and has no home.** `hardware/np_socket_map.json` is
geometry and says so — *"There is deliberately no lobe and no side here. A socket's anatomical meaning
is its ZONE MEMBERSHIP… not a property of the hardware."* `00-zones.npps` is zone membership. Neither
is *"which module belongs in which socket for this build"*. The simulator can render it — the
generator already runs the real parser against the real sources — but it has nothing to render yet, and
the app's live inventory is still the retired 5-slot array (`OI-HFE2-02`). **The rendering and the app
pipeline want the same socket-indexed structure and should be cut once. `OI-EEGNET-24`.**

---

## 2. Net architecture

### 2.1 Topology — equal-tension geodesic strut network (lead)

The net is a tessellated network of **equal-length, equal-tension elastic struts** with electrode
pucks at vertices, not a fabric cap with electrodes sewn on.

**Mechanism, stated as a mechanism rather than a claim.** If every strut carries the same tension
and every strut has the same unstretched length, every strut experiences the same strain. Uniform
strain is a similarity transform: all geodesic distances scale by a common factor λ, and any point
defined as a *fraction* of a geodesic path stays at that fraction. Placement is then **scale-
invariant by construction** rather than correct-within-tolerance at one size.

Real nets deviate from this. The deviation is quantified as η in §3 and is the whole sizing question.

**Fallback: low-stretch fabric in discrete sizes** (the `actiCAP` pattern, ~2 cm circumference per
size). Cheaper to make, worse per-size coverage — a 520–620 mm range at 2 cm/size is 5–6 SKUs, with
5–6 sets of inventory, tooling, and consumable-fit permutations. Named so the trade is on the record.

### 2.2 Registration to the head — three landmark anchors

The net registers to **anatomy**, not to the helmet:

| Anchor | Landmark | Why it is usable by an untrained wearer |
|---|---|---|
| A1 | Nasion | Palpable depression; the standard 10-20 origin |
| A2 | Inion | Palpable occipital protuberance |
| A3/A4 | Left / right preauricular points | Palpable notch anterior to the tragus |

The wearer seats four features they can feel. Everything else follows from strut geometry. This is
the same registration procedure a clinical EEG technician performs, reduced to four tactile targets.

**Landmark identification error is a real budget line, not zero** — allocated 4 mm in §3.4.

### 2.3 Contact force lives in the puck, not in net tension

Contact force is specified unchanged at **80–120 g** (CLAUDE.md §4.4), but it is produced by a
**compliant element inside each puck** (Shore 30A silicone dome, ~3–5 mm working travel), not by
global net tension.

**Why local rather than global.** Global tension couples contact force to size — a large head in a
small net gets more force everywhere. Local springs decouple them, so net tension can be chosen for
comfort and placement fidelity alone, which are the two things it must serve.

> **Rejected alternative, recorded because it was the first idea and it is wrong.** Let the net
> position the electrode and let the helmet's existing calibrated spring pod press through the net
> onto the puck's back face. This reuses a designed, specified spring. It fails because the pod
> plunger must then *find* the puck, and the puck's tangential position is precisely the quantity
> that varies by up to 27.8 mm. The proposal reintroduces the registration problem the net exists to
> delete. Contact force moves into the puck.

### 2.4 Radial stack budget

| Element | Nominal | Note |
|---|---|---|
| Net strut, crossing a tile aperture | ≤0.5 mm | Occlusion and thermal budget, §5 |
| Puck body, proud of the strut plane | 5.0 mm | Uncompressed |
| Puck working travel | 3.0–5.0 mm | Delivers 80–120 g |
| Puck hard stop | at 5.0 mm compression | Prevents the helmet bottoming a puck on the largest head |
| Optical-module bezel (unchanged) | 1.0 mm | `NP-THERM-BEZEL-001` |

**Required CAD check (`NET-CAD-01`):** ≥7.0 mm clearance between the L0 module face plane and the
scalp at every candidate electrode site, for the 620 mm head. `NP-HELMET-GEOM-001` §5 states modules
"normally make no structural contact with anything inboard," so clearance likely exists — but it has
never been checked *at electrode sites for the largest head*, which is the only case that binds.

### 2.5 Materials

| Element | Lead | Reason | Fallback / risk |
|---|---|---|---|
| Struts crossing a tile aperture | **PTFE or FEP monofilament**, undyed | **No C–H bonds**, so no C–H overtone absorption anywhere in 660–1170 nm; hydrophobic; non-magnetic; ISO 10993 precedent as surgical mesh; low friction on scalp | Undyed PET — cheaper, but carries a C–H second-overtone band near 1150–1200 nm, i.e. **directly on the 1170 nm T2 laser line**. Must be measured, not assumed (§5.2). |
| Elastic take-up | LSR silicone elements, sited off the optical path where routing allows | Elastic recovery; fluoropolymers creep | Silicone has C–H bands; keeping it off apertures is the mitigation |
| Puck body | PTFE dome over PEEK core | PTFE is the best diffuse NIR reflector available (~98 %), which the puck needs anyway for §4 | — |
| Electrode | **Sintered Ag/AgCl, unchanged**, with the existing snap-off bayonet hydrogel tip | The CLAUDE.md §2.3 consumable model (electrode tips, 60–72 % GM) must survive untouched, and does | — |
| Recording conductor | Carbon-loaded resistive polymer filament | Eddy-current suppression (§5.4), lead-current limiting (§6) | — |
| Stimulation conductor (T2 only) | Fine nickel-free Cu, discrete film resistor at the puck | Must carry mA | — |
| Guard | Carbon-loaded polymer sheath, DRL-driven | Guards by potential, not by conductivity (§6.5) | — |

**Hydrophobicity does double duty** — it is the NIR-transparency choice *and* the wet-shunt
suppression choice (§5.5). That coincidence is why the fluoropolymer lead is strong.

### 2.6 Service and life

User-removable and washable without tools. **Creep/compression set is budgeted at 3 mm** of
equivalent placement drift over service life (§3.4) and is a stated qualification item, not an
assumption — fluoropolymers in particular creep, which is exactly why the *geometry-defining* struts
and the *elastic take-up* are different materials (§2.5).

---

## 3. Sizing — and why "how stretchable" is the wrong question

### 3.1 The model

For a net of nominal circumference C₀ worn on a head of circumference C_h, with λ = C_h/C₀:

> **e(f) = η · f · L · |λ − 1|**

| Term | Meaning |
|---|---|
| **e** | placement error at the site, mm |
| **η** | **strain-tracking fidelity** — the *fraction of geodesic displacement that fails to track*. Dimensionless, 0 = perfect similarity transform |
| *f* | the site's arc fraction (0.94 at Oz — the worst case, so *f* = 0.94 governs) |
| *L* | nasion→inion arc at nominal, 331 mm at 620 mm (`NP-HEX-ZM-001` §3.1) |
| λ − 1 | the strain the net is being asked to absorb |

### 3.2 The counterintuitive consequence

η and |λ − 1| **multiply**. So:

> **A very stretchy net with poor fidelity needs MORE sizes than a modestly stretchy net with good
> fidelity.** Stretchiness buys accommodation range; fidelity is what determines whether the
> accommodation lands the electrodes anywhere useful. They are different properties, and only the
> second is a placement property.

η is a **topology** property far more than a material property — which is why §2.1 specifies a strut
network first and a material second. A plain elastic cap concentrates strain near its loaded rim and
leaves the unloaded crown nearly unstrained; that is the physical content of a large η, and it is
why elastic caps drift at the vertex.

### 3.3 Size count is an output of a measurement

Nets are worn **in tension only** (λ ≥ 1, nominal at the smallest head in the band) — a slack net
slips, and a slipped net has unbounded error. Allocating 5 mm of the error budget to strain (§3.4),
the per-size stretch a band may absorb is Δ = 5/(0.94 · 331 · η), and

> **N = ⌈ ln(620/520) / ln(1 + Δ) ⌉ = ⌈ 0.1759 / ln(1 + Δ) ⌉**

Inverted into a decision rule on the measured value:

| Measured η | Per-size stretch | **Sizes required** |
|---|---|---|
| ≤ 0.084 | 19.2 % | **1** |
| ≤ 0.175 | 9.2 % | **2** |
| ≤ 0.266 | 6.0 % | **3** |
| ≤ 0.357 | 4.5 % | **4** |
| ≤ 0.449 | 3.6 % | **5** |

**Design baseline: 3 sizes**, which is satisfied by any η ≤ 0.266 — comfortably above a competent
geodesic strut network and below a plain elastic cap. **2 sizes** if `NET-1` returns η ≤ 0.175.

Two things bound a band, and they are different: the **lower** bound is slack/slip, the **upper**
bound is whichever of placement fidelity and comfort pressure binds first. At 19 % stretch the
single-size case is almost certainly pressure-limited before it is placement-limited, which is why
1 size is in the table but is not the baseline.

> **Tooling consequence, and it is the important one.** Sizes are specified as a **uniform scale
> factor on one topology** — one mould family, N cavity scalings, one strut count, one puck part, one
> tail. The size count can therefore change after `NET-1` returns without re-architecting anything.
> Nothing downstream may encode N.

### 3.4 Error budget

| Contributor | Allocation | Basis |
|---|---|---|
| Landmark identification by wearer | 4 mm | Untrained palpation of nasion/inion/preauricular |
| Strain-tracking (η) | 5 mm | §3.3, drives the size count |
| Cephalic-index / shape mismatch | 5 mm | §3.5 |
| Creep / compression set over life | 3 mm | §2.6 |
| **RSS total** | **8.7 mm** | **Within the ±10 mm tolerance** |

**Where ±10 mm comes from, rather than being asserted:** 10-20 lines are ~33 mm apart
(`NP-HEX-ZM-001` §3.2), so ±16.5 mm is the point at which a site becomes *ambiguous with its
neighbour*. ±10 mm holds ~40 % margin against that ambiguity limit and is consistent with the 5–10 mm
error of routine manual 10-20 placement. It is a design tolerance, not a clinical claim.

### 3.5 Cephalic index is a shape axis, and size cannot touch it

Circumference does not determine the nasion→inion arc. Two 580 mm heads at CI 0.72 and CI 0.85 have
materially different sagittal-to-coronal arc ratios. `NP-HEX-ZM-001` §3.3 assumes `CEPHALIC_INDEX =
0.78`; the adult human range is roughly 0.70–0.85.

Scaling a net uniformly changes size, not shape, so **no number of sizes addresses CI.**

> **This answers the request's either/or — adjustable net *or* several sizes — as *both, on
> orthogonal axes*, and the reason is anthropometric rather than a compromise.** The net is **sized**
> on the coronal/overall scale, where fidelity limits reach, and **adjustable** on an independent
> **sagittal take-up** (nasion→Cz→inion chain), where scale cannot help because the variation is a
> shape change. One graduated, detented take-up; the app states the setting during guided fit.

### 3.6 Gate NET-1 — the experiment that sets N

**Question.** What is η for the candidate net, over 520–620 mm, at 10-20 sites?

**Hypotheses (plural, and each falsifiable):**
- **H1** — Equal-tension geodesic strut topology achieves η ≤ 0.175 → 2 sizes.
- **H2** — η is in 0.175–0.266 → 3 sizes (the baseline).
- **H3** — η is site-dependent rather than a scalar, and the sagittal chain tracks worse than the
  coronal, because sagittal struts are loaded through fewer junctions. *If H3 holds, the scalar model
  in §3.1 is wrong and the size count must be derived per-axis.* **H3 is the hypothesis most likely
  to be true and the one the protocol is designed to expose.**

**Method.** Eleven rigid head-forms, 520–620 mm in 10 mm steps, at CI 0.72 / 0.78 / 0.85 (33 forms).
Ground-truth 10-20 sites marked on each form by measured arc fraction. Fit each net size, photograph
under calibrated stereo or structured light, and measure puck-centre to marked-site distance.
η is recovered per site by regressing measured error on *f* · L · |λ − 1|.

**Sample size.** With between-specimen SD of η ≈ 0.05, resolving η to ±0.04 — the precision needed
to separate the 2-size and 3-size bands — requires **~25 fit trials per net size**. At SD ≈ 0.03 it
falls to ~9. Protocol specifies **n = 25 per size**, reducible on a measured SD from the first 10.

**Decision rule.** Enter the measured 95 % upper confidence bound on η into the §3.3 table. Use the
**upper bound**, not the point estimate — a size count chosen on a point estimate is wrong half the
time by construction.

**Falsification condition.** If no size count ≤ 5 satisfies ±10 mm, the equal-tension geodesic
approach has failed and the fallback in §2.1 (low-stretch fabric, ~2 cm bands) is taken instead.

**NET-1 gates:** the size count, the tooling cavity count, packaging, the fit SKU matrix, and any
placement-accuracy claim in marketing or regulatory material.

---

## 4. Placement verification — gate NET-2

A net that is correct by construction is still worn by a person who may have put it on crooked.
Registration must be **measured per session**, not assumed per manufacture.

### 4.1 The scheme uses hardware that already exists

Every PBM tile carries **PD1** (behind the window) and **PD2** (scalp-facing, measuring backscattered
tissue power) — the RISK-14 Option B dual-photodiode dose-metering pair, CLAUDE.md §3 modality 1.

Each electrode puck carries a **PTFE annulus** around the Ag/AgCl contact, ~98 % diffuse NIR
reflectance. Scalp under hair returns far less. The contrast is large.

**Contract.**

| | |
|---|---|
| **Input** | Sequential low-power illumination of each tile; PD2 return read on that tile and its ring-1 neighbours |
| **Intermediate** | Per-tile albedo excess → puck-occupancy map at tile resolution (~±17 mm) |
| **Solve** | Least-squares rigid-body pose of the net against its *known* internal geometry. Because the net's inter-puck geometry is known to ±5 mm, the pose fit is over-determined and resolves better than any single tile reading |
| **Output** | Either "seated" with a residual, or a specific correction: *"shift 12 mm posterior"* |
| **Failure** | **Fail closed.** Pose unresolved → no session start; the wearer is asked to reseat. Never a silent best-guess pose |

### 4.2 Three honest limits, stated rather than buried

1. **It measures net-to-*helmet* pose, not net-to-*head* pose.** Head-to-helmet is set separately by
   the 5-position forehead bridge and the Boa dial. The scheme closes one half of the loop; §2.2's
   landmark anchors close the other. Only together do they constitute registration, and neither alone
   should be described as verifying placement.
2. **The ranging flash is a dose.** It is NIR energy delivered to the scalp and **must be metered into
   the J/cm² per-zone record**, which is UHDR under CLAUDE.md §5.1. A verification feature that
   silently accrues unlogged dose would defeat the dose-metering differentiator.
3. **NIR albedo depends on skin tone and hair.** Contrast against a 98 % PTFE annulus is large, but
   "large" is an assumption until measured. **Validation across Fitzpatrick I–VI and across hair
   density and colour is required, not suggested.** The failure mode is not merely reduced accuracy —
   it is a feature that *works better for some users than others*, which is a validity problem before
   it is an accuracy problem, and shares a root with the known skin-tone dependence of PPG and PBM.

### 4.3 Data classification

Follows the existing boundary rule (CLAUDE.md §5.1) by direct analogy with *"raw VNS impedance →
UHDR; contact resistance trend → SHDR"*:

| Element | Class | Reason |
|---|---|---|
| Per-tile PD2 albedo readings | **UHDR** | Scalp optical properties are user biology, and hair/skin phototype is inferable from them |
| Resolved net pose, per session | **UHDR** | Derived from the above; head geometry is user biology |
| Ranging-flash dose (J/cm²) | **UHDR** | Already the class of all PBM dose |
| Coarse boolean `net_pose_resolved` | **SHDR** | Device-condition signal with no user biology, and it must be a bare boolean — a *count* of failures weakly signals an atypical head, exactly the pattern already caught for the anonymisation-pipeline `failed_step` |

### 4.4 What NET-2 does and does not close

**NET-2 qualifies the verification scheme** — discrimination across phototypes, pose accuracy against
a ground-truth fixture, fail-closed behaviour, and dose accounting.

> **It does not close `REG-1`, and no claim to that effect is made here.** `REG-1` is about where the
> *socket lattice* sits relative to anatomy. §7.1.1 argues that question shrinks; it does not vanish,
> and it is not answered by this scheme.

---

## 5. Modality interference

Every T1 modality and every T2 addition sharing the cranial vault gets a verdict **and a falsifier** —
a measurement that could show the verdict wrong. **No cell says "none" without one.**

| # | Modality | Interaction | Verdict | Falsifier |
|---|---|---|---|---|
| 1 | PBM transcranial | Occlusion, NIR absorption, standoff, PD fouling discrimination | **Material — controlled by §5.1–5.3** | `NET-DRC-01/02/03` |
| 2 | PBM intranasal | Not in the net envelope | **None** | Physical fit check with net donned |
| 3 | EEG neurofeedback | This *is* the net | **Improved registration, possibly degraded artifact** | `SH2-DRC-16` (§7.2.1) |
| 4 | BES / tACS | Net carries the drive (option b) | **Material — wet shunt, §5.5** | `NET-DRC-06/07` |
| 5 | tDCS | As above, plus 40 µC/cm² | **Material; charge limit unaffected** | `NET-DRC-07` |
| 6 | VNS + HRV | Auricular clip, below the lattice ear cut-out | **None** | Clip fit + A1/A2 continuity with net donned |
| 7 | Neural audio | Ear cups are a rim-mounted subassembly; net terminates above the ear cut-out | **None, by exclusion** | Acoustic seal + 40 dB mesh RF check with net donned |
| 8 | Visual stimulation | Goggle/lens, outside the vault | **None**, but §7.2.4 re-homes its Oz safety gate | `NET-DRC-09` |
| T2 | 21-ch qEEG | Separate net part, same architecture | **Out of scope here** | — |
| T2 | 1170 nm deep PBM | Laser line sits on PET's C–H overtone | **Material — drives the fluoropolymer choice** | `NET-DRC-02` |
| T2 | TMS | 0.1–0.5 T through the net | **Material — §5.6** | `NET-DRC-10/11` |

### 5.1 PBM occlusion — `NET-DRC-01`

Strut routing cannot be guaranteed to avoid tile apertures, because strut geometry scales with the
head and the tile lattice does not — the same divergence as §1.1. Occlusion must therefore be handled
**by material, not by geometry**.

> **`REQ-NET-01`** — Areal occlusion of any tile aperture by net material ≤ **8 %**, at every size and
> at both ends of the sagittal take-up range.

### 5.2 NIR absorbance — `NET-DRC-02`

> **`REQ-NET-02`** — Single-pass transmittance of any net element that can lie in a tile aperture
> ≥ **97 %** at 660, 810, 1064 and **1170 nm**, measured on production material at production
> thickness.

1170 nm is called out because it is where the fallback material is weakest. This requirement is what
makes the PET fallback a *measurement* rather than a guess.

### 5.3 Dose metering — `NET-DRC-03`

PD1/PD2 ratio separates PDMS fouling from LED aging (CLAUDE.md §3 modality 1). **A strut in the
optical path perturbs that ratio and can read as fouling.** Two consequences:

> **`REQ-NET-03`** — The fouling/aging discriminator must be re-characterised with the net installed.
> **`OI-EEGNET-04`** — Whether dose-metering calibration coefficients become net-size-dependent is
> **open**. If they do, the coefficients are no longer purely module property, which contradicts
> `NP-HW-HUB-001` §9.5. Not resolved here.

### 5.4 Fluxgates and Helmholtz cancellation

The active cancellation depends on 3-axis fluxgate magnetometers mounted on **L1**
(`NP-HEX-ZM-001` §5.3c), and `REQ-EMI-10` forbids conductive additions to L1 without re-qualification.
The net is not on L1, but it sits millimetres inboard of it, inside the sensing volume.

> **The net is held to a stricter standard than any fixed part, and here is the reason.** A fixed
> ferromagnetic or conductive mass is *calibrated out* — `REQ-EMI-11` already re-triggers calibration
> on `np_module_map` rebuild. **The net is the only magnetically-relevant mass in the assembly whose
> position changes between sessions, which stretches, and which does not appear in `np_module_map` at
> all.** It cannot be calibrated out by any existing mechanism.

> **`REQ-NET-04`** — Zero ferromagnetic content. Total remanent moment ≤ **1 nAm²**. Nickel underplate
> is **prohibited**, including inside the connector; specify gold over palladium or gold over a
> Pd-barrier copper.
> **`REQ-NET-05`** — Every net conductor ≥ **1 kΩ** end-to-end, so eddy currents are negligible at ELF.
> **`REQ-NET-06`** — No continuous metallic shield film anywhere in the net.
> **`REQ-NET-07`** — The net adds no conductive element to L1.

### 5.5 Wet shunt — a burn hazard, not a signal-quality nuisance

Sweat, hydrogel and cleaning residue make textile conductive. Under tES a surface shunt between
anode and cathode concentrates current at the shunt's entry point — a documented tDCS burn mechanism.
It is the reason hydrophobic yarn is not a nicety.

> **`REQ-NET-08`** — Creepage between any two stimulating pucks ≥ **25 mm** along every net surface path.
> **`REQ-NET-09`** — Inter-electrode leakage ≥ **10 MΩ** dry and ≥ **100 kΩ** after a specified
> sweat-soak conditioning.
> **`REQ-NET-10`** — Before any tES enable, the **safety MCU** measures the inter-electrode impedance
> matrix and inhibits if any off-diagonal element falls below threshold. Assigned to the safety MCU
> because CLAUDE.md §4.2 gives it ownership of every stimulation enable GPIO; this is the same pattern
> as the existing VNS contact-confirmation interlock.

The 40 µC/cm² hardware charge-density limit is unaffected — it is enforced in the safety MCU
independent of the electrode carrier.

### 5.6 TMS (T2) — `NET-DRC-10/11`

At 0.5 T in ~100 µs, dB/dt ≈ **5 × 10³ T/s** (assumption stated; the real waveform is a damped
sinusoid and the peak should be taken from the coil driver spec). Induced EMF = A · dB/dt, so a 2 cm²
enclosed loop sees ~1 V.

> **`REQ-NET-11`** — Electrode temperature rise under the worst-case rTMS train ≤ **2 °C**.
> **`REQ-NET-12`** — No ferrite and no magnetically saturable component anywhere in the net.
> **`REQ-NET-13`** — Enclosed loop area ≤ **2 cm²** between any recording lead and the reference lead
> over the whole run (§6.2).

---

## 6. Wiring — the antenna question, answered on the right physics

### 6.1 "Antenna" is the wrong model here, and saying so changes the fix

The request asks that the wiring not act as an antenna, alone or through modality interaction. The
concern is exactly right; the mechanism is not radiative.

The net lives **inside** the Faraday envelope (`NP-DRV-SHELL-002` §9.6), and the BT/Wi-Fi radios are
in the control hub, **not** the headset (CLAUDE.md §4.1). The emitters that remain are internal and
slow: I2C at ≤400 kHz, LED PWM at ≤40 Hz with fast edges, ELF Helmholtz drive. At 400 kHz, λ ≈ 750 m.
A 300 mm lead is **λ/2500**.

> **A structure that small cannot resonate and cannot radiate meaningfully. It is not an antenna at
> any frequency present inside the shell.** What it can do is far worse and far more likely:
> **near-field mutual inductance** (enclosed loop area × dB/dt) and **mutual capacitance** (displacement
> current onto a high-impedance node). Those two are the design targets. Ferrites, RF absorbers and
> shield cans — the reflexive antenna fixes — buy nothing here and are prohibited anyway by
> `REQ-EMI-10` and `REQ-NET-04`/`-06`.

This is the same finding `NP-DRV-SHELL-002` §9.6 reached from the other direction: *"A Faraday cage
does not protect the EEG electrodes and fluxgates that share the enclosure with the source."*

### 6.2 Topology — tree, co-routed, single tail

> **`REQ-NET-14`** — Conductor topology is a **tree** terminating at one posterior connector. No closed
> conductive ring at any scale. (Inherits `REQ-EMI-09`.)
> **`REQ-NET-15`** — All leads are co-routed in one flat bundle along a common geodesic to the
> posterior termination, so the loop between any lead and the reference is a thin ribbon rather than a
> bowl-scale circuit. This is what makes `REQ-NET-13`'s 2 cm² achievable.
> **`REQ-NET-16`** — The tail terminates at the **posterior aggregation node**, where the ADS1299 bank
> already sits (`NP-DRV-SHELL-002` §3.5). The µV path therefore never enters L1, never crosses the
> parting-plane boss, and passes through **no pogo contact and no analog mux**.

### 6.3 Series resistance at the puck

> **`REQ-NET-17`** — Series resistance is integrated **in the puck**, not at the connector. A resistor
> at the far end limits nothing: the induced EMF appears along the lead, and only a resistance in
> series with the lead's own loop bounds the resulting current.

**Noise cost, computed rather than waved away.** Johnson noise over a 100 Hz band:

| Series R | Noise | Against ADS1299 ≈140 nV rms |
|---|---|---|
| 1 kΩ | 41 nV | +4 % total |
| 5 kΩ | 93 nV | +20 % |
| 10 kΩ | 131 nV | +37 % |

Not free at 5–10 kΩ. **What makes it affordable is that the semi-dry electrode's own source impedance
is already 5–50 kΩ**, so the added resistor shifts an already-dominated budget by a fraction rather
than a factor. Stated as a real cost because pretending it is zero would be wrong.

The series R also forms a low-pass with lead capacitance — an incidental benefit against fast PWM
edges, and one that must be bounded so it does not encroach on the EEG passband.

### 6.4 The tES conflict, resolved differently for T1 and T2

A 10 kΩ series resistor and a 2 mA stimulation current cannot share a conductor: 20 V of drop and
40 mW in the puck.

**T1 — one conductor per site, because T1 has no TMS.** Series R is needed only for fault-current
limiting and edge filtering, both satisfied at **1 kΩ**. At 2 mA that is 2 V of compliance — trivial —
and it doubles as a genuine tDCS safety feature, limiting fault current if a driver fails. Noise cost
+4 %. Count: **8 sites + reference + DRL = 10 conductors + guard.**

**T2 — two conductors per dual-rated site, because TMS is present.** Recording needs ≥5 kΩ in the lead
during a pulse; stimulation needs <50 Ω. Count: **21 × 2 + reference + DRL = 44 conductors + guard.**

> **Rejected: a parallel inductor to pass DC stimulation while blocking the TMS-induced transient.**
> Two independent failures. (i) At TMS spectral content (~3–10 kHz) a practical inductance gives an
> impedance comparable to the resistor, so it only partly helps. (ii) Any inductance large enough
> requires a ferrite core — ferromagnetic, prohibited near the fluxgates by `REQ-NET-04`, and it would
> saturate in the TMS field regardless.

> **`OI-EEGNET-07`** — Whether concurrent TMS **and** full 21-channel tES capability is actually a
> required protocol is **open**, and it is the single requirement that doubles the T2 tail. Worth
> asking before it is built.

### 6.5 Guarding — resistive, not braided

> **`REQ-NET-18`** — The guard is a **carbon-loaded polymer sheath driven from the DRL**, not a metallic
> braid.

A guard works by holding the surrounding conductor at the signal's potential so no displacement
current flows into it. **That is a statement about potential, not about conductivity** — a resistive
guard guards. Its limit is its RC: at ~10 kΩ/m and ~100 pF/m, τ ≈ 1 µs, effective to roughly 150 kHz.
That covers the EEG band with four orders of margin. Residual coupling from fast PWM edges above that
is handed to the existing deterministic-artifact machinery (`REQ-EMI-03` sense-quiet windows,
`REQ-EMI-04` dithering prohibition), which is precisely what those requirements exist for.

### 6.6 Reference

> **`REQ-NET-19`** — A net-borne reference at FCz plus a net-borne DRL is the baseline. **A1/A2 linked
> ears are taken from the VNS clip's contact pads over the existing 6-pin cable** (CLAUDE.md §3
> modality 6) whenever the clip is worn — this provision already exists and is not reinvented here.

---

## 7. What this change actually does — both halves

### 7.1 Genuine leverage

**7.1.1 `REG-1`'s content largely dissolves, and one unfixable defect is fixed.** With no electrode on
any socket, the lattice only has to cover scalp *optically*, against `NP-OPT-PSF-001`'s resolution
floor rather than a 10-20 tolerance. The Fp1/Fp2 shared-socket defect and the Oz-18 mm-anterior caveat
(§1.3) both dissolve. **This is the only item on either list that no parameter change can deliver.**

`REG-1` does not vanish: PBM zone naming still needs it. Its scope collapses; its tight-tolerance half
leaves. This may unblock the shell-tooling first cut via `OI-REVSH-01` — *may*, because
`NP-DRV-SHELL-002` also needs `ACT-1`.

**7.1.2 The socket-pinout union rule stops paying for a type that no longer exists.**
`NP-HW-HEXTILE-001` §1 makes the socket pinout *"the union of every type's needs,"* which is why pins
13 `ELEC` / 14 `ELEC_SHLD` / 15 `AGND` exist at **all ~80 sockets** for a T1-B that might land
anywhere. Delete the type and network **N4 leaves the socket interface entirely**, taking the
per-cluster electrode mux, the L1 scalp-facing DRL guard plane, and the cluster-tail feed with it.
`OI-SHELL2-10` (µV path through a pogo contact plus a mux) is deleted rather than mitigated.

> **This spends an option deliberately.** `NP-HW-HEXTILE-001` §7.2 warns that removing those pins
> *"would silently re-impose the type-restricted placement model that SMART-1 was decided to
> eliminate."* Removing them because no type needs them is legitimate — but `SMART-1`'s rationale
> covers a *future* electrode-bearing smart tile at any socket, including for the IRB custom studies
> in CLAUDE.md §6.3. That option is being spent, not overlooked.

**7.1.3 A cluster of open items closes by deletion.** The electrode-pod body diameter is unspecified
anywhere in the document set and is the input deciding T1-B's LED ring depopulation. Deleting T1-B
closes `OI-HEXTILE-05`, `RISK-HEX-03`, the ELECTRODE-POD part, the bezel's type-dependent `s = 0`
split (`NP-THERM-BEZEL-001`, `BEZEL-1b`), and the per-type operating-envelope split
(`NP-ENV-OPRANGE-001` §4 — T1-B's +5 °C gel low bound disappears and every tile becomes −10 → +43 °C).
`OI-COST-02` (Core 3-vs-4 tiles) dissolves.

**7.1.4 Taxonomy collapses 3 types → 2, with a real accessibility gain.** `NP-HFE-002` §5's *"9
positions that matter out of ~80"* becomes **zero positions that matter**; L1(d) — the only HFE
feature gated on `REG-1` — is deleted. For a blind user, positional counting leaves module placement
entirely.

### 7.2 What gets worse — read this before adopting

**7.2.1 `SH2-DRC-16` (<5 µVpp artifact) may REGRESS, not improve.** The tempting claim is that moving
electrodes off L1 restores the retired ≥15 mm PBM-to-EEG separation. **It does not, and the honest
reading is the opposite.** The tiles' *emitting face* is the scalp-facing face, so a net between scalp
and tile sits **closer** to the LED drive loop than L1's guarded face does. Two of the four
compensating mechanisms in `NP-DRV-SHELL-002` §9.1 weaken: mechanism 2 is lost outright (the DRL guard
plane is an L1 structure), and mechanism 1 lengthens (the analog path now runs from a floating net
across the bowl boundary to the PAN). §9.2's arithmetic is untouched — the therapeutic band still
cannot be filtered on the tile — so the **source is unchanged**. `SH2-DRC-16` is retained verbatim and
becomes harder again.

> Routing the net outside the outer bowl to recover geometric separation is **rejected**: it reopens
> exactly the shield-aperture problem `NP-HEX-ZM-001` §5.2 exists to avoid.

**7.2.2 BOM moves the wrong way, on the worst possible term.** `NP-COST-001` §5 gives Home Standard's
emitter count as an explicit formula: **21 × 90 + 9 × 44 = 2,286** (T1-A = 90, T1-B ≈ 44 per
`NP-HW-HEXTILE-001` §4.2 — T1-B is depopulated for pod clearance). Delete T1-B and every tile is
T1-A: **30 × 90 = 2,700. +414 emitters, +18.1 %.** This is exact arithmetic on the source formula,
not an estimate. That inflates term **U** — the emitter-count delta `NP-COST-001` §2 already
flags as *"very likely large and positive"* and the reason `OI-HUB-C08` cannot close — against a
configuration already at **−41 % to −51 % gross margin** with a ~$1,196 break-even. The net, its
contacts, connector, tooling and FAI are all net-new on top.

> **Nothing here is a cost saving.** The N4 mux deletion (~$11–22/headset, `NP-DRV-SHELL-002` §10.1)
> is real but is smaller than the emitter regression. **No figure in this document may be entered into
> `NP-COST-001`; that document owns the re-derivation and must do it as a whole.**

**7.2.3 Two tooling savings reverse.** `cad/CAD_PARTS_LIST.md` records `EEG-ROUTE-CHANNEL` as
**RETIRED**, its `REQ-ST-01..07` features *"deleted from shell tooling (a net simplification of the
shell tool — the complexity moves into L1 lamination)."* A net needs routing, strain relief and a
connector, so that channel or its equivalent returns — moving complexity back into the more expensive
tool. And **`RISK-21` reverts**: `NP-RISK-002` disposes of it (EEG electrode cable routing
unspecified, HIGH) on the ground that *"There are no EEG cables to route."* That becomes false.

**7.2.4 Two safety gates re-home onto a soft, user-placed part. This is the highest-consequence item
in this document.**

| Gate | Today | On a net |
|---|---|---|
| Photoparoxysmal halt | `check_placement({Oz, EEG\|DUAL})` against keyed socket 74 — **discrete, digital, deterministic**; <200 ms halt | **Continuous, analog**: net seated *and* Oz contact impedance acceptable |
| tES montage presence | Electrodes present at all montage sockets (HD-tDCS 4×1 = 5) | As above, across the montage |

`np_module_map_check_placement()` and its `type_mask` model do not generalise to a net. **A new
presence primitive is required**, and it must be at least as deterministic as the one it replaces for
a Class C <200 ms halt path. That is a firmware safety work item, not a documentation one.

**7.2.5 The identity model has no slot for a net.** *"Socket = position, module = type, all discovered
by UID auto-inventory"* has no representation for a cranial element that is **neither a socket nor a
module**. Code deletion is trivial — `NP_ELEM_EEG_ELECTRODE` (5) and `NP_ELEM_TES_ELECTRODE` (6)
already exist and are unused, so the enum anticipated the split — but adding the net is an **addition
to the architecture**, not a deletion from it.

**7.2.6 What does NOT improve, despite looking as though it should.** The 18-cluster tier does not
shrink to 12 and the $114.12 controller tier does not drop. `SYM-1`'s recorded justification is the
per-cluster safety cut domain, the I2C segment, inclusive-midline zone membership, EMDR L/R
alternation, hemispheric **PBM** targeting and `NP-OPT-PSF-001`'s lateralisation model. Only
"bilateral montages" is electrode-related, and it follows tES. **`SYM-1` and `CONTIG-1` survive
intact.**

### 7.3 Locked decisions touched — was → is → cause

Per `NP-CONV-001` §7. Nothing below is reversed by this document; each is *raised*.

| Decision | Was | Is (proposed) | Cause |
|---|---|---|---|
| `NP-HEX-ZM-001` §4a T1-B tile type | Dual-rated electrode tile placeable at any socket | Deleted; electrodes off-lattice | §1 — tangential registration is unachievable on a rigid lattice |
| Socket pinout (`NP-HW-HEXTILE-001` §7) | 16/19-position union including 13/14/15 | Two positions freed | §7.1.2 |
| `REG-1` scope | Lattice registers to 10-20 within tolerance | PBM zone naming only | §7.1.1 |
| `RISK-21` disposition | *"There are no EEG cables to route"* | **Reverts** | §7.2.3 |
| `EEG-ROUTE-CHANNEL` | RETIRED from shell tooling | Returns in some form | §7.2.3 |
| Single inner transparent shield (`NP-HELMET-GEOM-001` §0) | **ABANDONED**, on exactly one stated ground: *"electrodes must galvanically contact skin, which a continuous dielectric barrier physically blocks"* | **Reopened — NOT decided here** | That ground is removed when no electrode is in the lattice. Consequences for sealing (`SEAL-1`, 80 gaskets), cleaning and tooling are large and belong to a principal decision, not to this document |
| **Electrodes per tile** (`NP-HW-HEXTILE-001` §4.5, D-1) | One, at site 0, by convention | **Raised as an open variable — 1 vs 2, uniform vs mixed** | §1.7, `OI-EEGNET-19`. Not decided; §1.7.4 recommends uniform if it is pursued at all |
| **`REQ-SKT-01`** — pad array is two staggered rows | Binding, not advisory | **Raised — row count and row straightness are both free; neither should be assumed away** | §1.7.2. The two-row constraint follows from row *length*, not from tile area, and a chord is the worst path across a hexagon. §1.7.3 gives an argument for re-shaping at *constant* contact count, and a new counter-cost (angular tolerance) |
| **Socket contact count = 19** (`NP-DRV-SHELL-002` §5.1.4, D-5) | Closed by principal decision 2026-08-11; 2 reserved dropped | **Not reopened here — raised as a variable the MECH-2 / HFE force study should carry** | §1.7.2, `OI-EEGNET-20`. That study must run anyway to close `OI-SHELL2-03(b)`, and it is the only one that can price a contact |
| Per-configuration tile population (`NP-COST-001` §2 A-1) | 30 tiles for Home Standard, 9 of them T1-B | **Raised — it is an adopted assumption, not a decision, and §1.8 shows its stated basis is an emitter-power argument that does not reach electrodes** | §1.8, `OI-EEGNET-21`. Routed to `OI-COST-01`, which owns it |
| Tile taxonomy (`NP-HEX-ZM-001` §4a) | Three T1 types; every electrode site is a depopulated PBM tile | **Raised — an electrode-only T1-E is proposed, not adopted** | §1.8.3. Preserves the one-mould invariant; costs PBM coverage at electrode sites, which `NP-OPT-PSF-001` must price |
| `NP-HFE-002` §7.1(d) standard-electrode-site marker | Deleted along with T1-B (§7.1.4), *"9 positions that matter"* → zero | **Direction is now contested** — §1.7 makes L1(d) *more* load-bearing | §1.7.4. Both directions cannot be pursued; whichever is adopted must say so |

### 7.4 Blast radius, measured

**30 files, 102 references.** Code surface is 6 files: `np_module_map.h`, `np_module_map_tests.c`,
`np_zone_notify.h`, `np_zone_announce.c`, `ZoneModuleInfo.swift`, `shdr_fleet_schema.sql`; plus 2
editscripts and 2 CAD files. The remainder are controlled documents, each taking a revision event
under `NP-CONV-001`. Eight ISCs in `np_hex_zm_isa.md` (49, 51, 52, 54, 55, 56, 62, 68) are directly
falsified.

### 7.5 Two observations for the principal

**This is an oscillation, not a one-way improvement.** EEG was previously *outside* the tiles, on a
separate harness on the far side of the shell wall, and was moved *in*; the artifact problem came with
it and `NP-DRV-SHELL-002` §9.1 is the record of paying for that. This moves it back out, and the
constraint that reasserts is mechanical/tooling complexity. Each swing trades artifact integrity
against tooling simplicity without either being decisively dominant — **and that pattern is the signal
that neither is the binding constraint.** The binding constraint is the one thing that does not
oscillate: **a 40 mm hex lattice cannot resolve adjacent 10-20 lines.** That is what should decide
this, and it is the argument in favour.

**Sequencing.** `OI-HEXTILE-05` already blocks T1-B's layout, so leaving T1-B *undecided* costs nothing
today. `REG-1` is the urgent gate, and its scope depends entirely on this outcome. **Decide T1-B before
spending more effort on `REG-1`** — otherwise that effort may be spent establishing a registration the
architecture no longer needs.

---

## 8. Open items

| ID | Item | Owner | Gate |
|---|---|---|---|
| **OI-EEGNET-01** | **§0 fork not decided.** (a) EEG-only net / (b) EEG+tES net / (c) tES deleted from T1. Everything in this document assumes (b) | **Principal** | Blocks all |
| OI-EEGNET-02 | η unmeasured; size count is a baseline, not a result | ME | **NET-1** |
| OI-EEGNET-03 | Sagittal take-up detent count and range unspecified pending `NET-1` H3 | ME | NET-1 |
| OI-EEGNET-04 | Whether PD1/PD2 dose calibration becomes net-size-dependent, contradicting `NP-HW-HUB-001` §9.5's module-property rule | EE | NET-2 |
| OI-EEGNET-05 | PD2 albedo discrimination unvalidated across Fitzpatrick I–VI and hair density/colour | Systems + Clinical | **NET-2** |
| OI-EEGNET-06 | New presence primitive for the photoparoxysmal and tES gates — Class C, <200 ms | FW Safety | Blocks T2 visual + all tES |
| OI-EEGNET-07 | Is concurrent TMS + full 21-ch tES actually required? It doubles the T2 tail | Clinical | T2 |
| OI-EEGNET-08 | `NP-THERM-BEZEL-001` re-run with net material in the bezel gap and reduced evaporative scalp cooling | Thermal | **THERM-1** |
| OI-EEGNET-09 | `NP-COST-001` whole-document re-derivation. **Direction is likely adverse** (§7.2.2) | Finance + Systems | Precedes any pricing per `OI-COST-10` |
| OI-EEGNET-10 | Single inner transparent shield reopened by §7.3 — sealing, cleaning, tooling consequences unexplored | **Principal** | SEAL-1 |
| OI-EEGNET-11 | Net has no representation in the module-identity model (§7.2.5) | FW + App | — |
| OI-EEGNET-12 | Residual risk that a T1-B consumer exists which no grep pattern in §7.4 caught | Systems | Before tooling |
| OI-EEGNET-13 | Net risk register (`NP-RISK-005`?) and FAI do not exist. Per `NP-ART-001`, this would be a tenth artifact with no owning risk document | QA | Before tooling |
| **OI-EEGNET-14** | **Seating concentricity is unspecified and unmeasured, and §1.1 shows it is the DOMINANT registration term (~0.9 mm landing error per 1 mm of head displacement).** The fit system (5-position bridge, Boa, temporal wings) has never been given a concentricity requirement. This is the single measurement that decides whether §1.4 suffices. **Rev 4 — it is not only an EEG dependency:** `OI-SESPWR-01` needs the same socket-to-10-20 registration before evidence-faithful PBM zones (a DLPFC zone of 2 sockets per hemisphere) can be authored at all, and it is worth ~690 W on the depression protocol alone. Two consumers, one measurement; neither document names the other | **ME + Systems** | **Blocks the §1.4-vs-net choice** |
| **OI-EEGNET-15** | Placement tolerance is treated as one number (±10 mm). It is plausibly **modality-dependent** — looser for T1 8-channel wellness neurofeedback, tighter for T2 sLORETA source localisation and HD-tDCS 4×1 targeting. If so, T1 takes §1.4 and only T2 needs a cap. Not established here | Clinical | T2 |
| **OI-EEGNET-16** | **`OI-HEXTILE-05` (pod body diameter) is the tangential registration budget and is not being treated as one.** It is currently scoped as a PBM-coverage question only. Re-scope it before T1-B Rev 2 layout | Systems + ME | **T1-B layout** |
| **OI-EEGNET-17** | §1.1's model is 2D sagittal only. The coronal plane, where cephalic index 0.70–0.85 acts, has not been computed | Systems | With OI-EEGNET-14 |
| **OI-EEGNET-18** | **Spatial sampling density as an alternative to placement tolerance (§1.6).** Two halves: (a) establish the actual sampling requirement for this geometry and these measures — the ~20 mm figure is a literature estimate and is **not** a project number; (b) determine whether array pose can be recovered well enough to interpolate against anatomy, which is `OI-EEGNET-14` in a different currency. **Scope is recording only** — §1.6 limit 3 excludes tES and the Oz gate. Interacts with `OI-EEGNET-15`: if tolerance is modality-dependent, so is this | Systems + Clinical | With OI-EEGNET-14/15 |
| **OI-EEGNET-19** | **Electrodes per tile — study 1, 2, 3 and 4, uniform or mixed (§1.7).** The range is **not** 1–2: §1.6's density argument is only satisfied at 4/tile, so a study capped at 2 cannot answer the question that motivates it. Four ⌀11.4 mm pods fit at 20.5 mm intra-tile spacing (§1.7.1). Decide with `OI-HEXTILE-05`, not after it — pod diameter sets the emitter budget, the pod separation *and* the achievable pod count. **Note the cost axis runs the counterintuitive way**: 4 pods inflate emitters +7.5 % against 2 pods' +13.0 %, because each pod removes ~7 emitters. Three cross-cuts the study must carry: (a) shield per electrode or one shared DRL-driven shield — worth 3 contacts at 4 electrodes; (b) whether every electrode is dual-rated or only a subset (§1.7.1 — decoupling holds the safety-MCU channel count flat); (c) uniform vs mixed, where §1.7.4 recommends uniform. **Blocked by OI-EEGNET-20**: every electrode past the first needs socket positions that do not exist | Systems + ME + HFE | **T1-B layout; with OI-HEXTILE-05 and the N of §1.7.5** |
| **OI-EEGNET-20** | **Carry socket contact count as a variable in the MECH-2 / HFE force study, and evaluate a three-row array (§1.7.2–§1.7.3).** Force is exactly linear at 0.3–0.5 N per contact, and 34.2–57.0 N at 19 is **already the unanswered question** in `OI-SHELL2-03(b)`. Route to `NP-DRV-SHELL-002` §5.1.4 and `NP-HW-HEXTILE-001` D-5 — **not decided here**. **Row count and row straightness are both free variables** — an edge-following L, chevron or polyline offers ~48 positions on one run at 2.00 mm pitch and holds constant edge margin, where a chord does not (§1.7.2). Independent of any electrode decision, §1.7.3 gives a µV-siting argument for re-shaping the array at constant count (`OI-HEXTILE-11`), against a newly identified cost: a spread array roughly **halves the angular tolerance** the mechanical key must hold, which no document currently states. **Two prerequisites the study cannot skip (§1.7.2):** the stated 34.2–57.0 N is **contact force only** — ejector springs, 30 per-tile gaskets and plunger preload are excluded — and there is **no input-force acceptance number** for the cluster actuator, §5.4a's ≤1 N being the retired per-module eject-lever figure. Required MA = load ÷ target and neither end exists. **Force-spread cancellation is a real second lever** worth 5.7 N per 0.05 N of spread removed, best implemented as socket-side spring-rate grading (preserves `R-2`); naive load-spreading stagger is counterproductive against an over-centre. **Release force may bind before throw force** (`OI-HFE2-05`), and the ejector-spring trade between them is stated nowhere. **Rev 4 — the count is now reopened from a second direction, and the two must resolve together.** `OI-HEXTILE-20` finds §8.1's 25.0 W/tile peak may be illegal (806 mW/cm² against R-5's 600), and reading (b) puts the true peak at **18.6 W**, changing the rail current and *"the per-pin contact current that set `VLED` at 3 contacts"* — D-5, the same tooling-blocking count. **It does not free a contact:** at 18.6 W over 2 `VLED` pins the degraded case is 1.29× against the ≥2× rule, so 3+3 stands. What it does show is that at the *current* 25.0 W basis 3 pins give only 1.92× — the rule 3+3 exists to satisfy is met only under reading (b). The count must be re-derived either way, and §1.9.5 adds a third claimant on it. **Rev 8 — the design already solved this problem once, the other way, and the contrast is the argument.** The auricular clip carries **both** VNS stimulation and the A1/A2 EEG reference at one location, and it does *not* dual-rate a pad: the references ride **2 spare conductors in the existing 6-pin cable** (CLAUDE.md §3 modality 6, +$15 BOM **`[design-target]`**). Co-located, one conductor per function, so none of §0's dual-rated fork arises there. **The clip could do that because it had spares; the socket has none** — §5.1.4 closed at 19 with *"2 reserved dropped"*. So the socket's dual-rating in §0 is not a considered choice over the clip's approach, it is what remains when the conductors run out, and every claimant in §1.10.7 (second electrode, its shield, an fNIRS detector) is competing for conductors the clip simply had. **Whether the socket should be given spares is the question this item exists to put.** **Time-boxed:** `OI-SHELL2-09(i)` blocks socket tooling; after that cut the count is permanent at every socket by the union rule | ME + HFE + EE | **MECH-2; before socket tooling** |
| **OI-EEGNET-22** | **A four-pod tile places an fNIRS source–detector pair at 29.0 mm — inside `NP-FEAS-FNIRS-001`'s 2.5–3.5 cm window — where the cross-tile separation that study proposes (40.0 mm) overshoots it (§1.9.4).** The geometry is free; the wiring is not. Three gates, none opened here: **D-2** requires PD1/PD2 co-location for the fouling-vs-ageing ratio, so an fNIRS detector is a *third* PD, not a relocated one; a third PD is a third claimant on the closed 19-contact budget alongside the second electrode and its shield (`OI-EEGNET-20`); and `NP-FEAS-FNIRS-001` Risk A — 808–830 nm sitting on the isosbestic point — is a **chromophore** problem that no separation distance fixes. Also asks whether `NP-PWR-BUDGET-001` §3.6's sub-therapeutic whole-vault mode, a limitation for PBM, is the desired condition for monitoring (`OI-PWR-07`) | Systems + EE + Clinical | With `OI-EEGNET-19`/`-20`; `NP-FEAS-FNIRS-001` go/no-go |
| **OI-EEGNET-23** | **Manufactured pod-count variants, or one universal tile with pods selected on-tile? (§1.10.2)** Option A pushes the taxonomy to **7–8 types**, past the 6 `NP-HFE-002` Rev 2's nested figure reaches, and breaks `R-2` per position. Option B holds the taxonomy at today's 4–5, keeps `R-2`, needs no placement instruction, and costs 1–2 contacts regardless of pod count — **conditional on a static on-tile selector** (§1.10.3), whose Ron/leakage in the µV path is a re-run of `SH2-DRC-27` one level down. **Decide with `OI-EEGNET-21`:** at N ≥ 6 a universal tile keeps ≤53 % of its emitters and the PBM case at electrode sites becomes marginal, which is T1-E's premise from the other side | **Principal + Systems** | **T1-B layout; with `OI-EEGNET-19`/`-21`** |
| **OI-EEGNET-24** | **A required *build* map — "which module belongs in which socket" — is a third kind of data with no home, and the simulator has nothing to render (§1.10.7).** `hardware/np_socket_map.json` is geometry and says so explicitly; `00-zones.npps` is zone membership; neither is a build map. The simulator generator already runs the real parser against real sources, so rendering is cheap once the artifact exists. **Cut it once with `OI-HFE2-02`** — the app's live inventory is still the retired 5-slot `zoneModules: [UInt8] = [0,0,0,0,0]`, and both want the same socket-indexed structure | Systems + App | With `OI-HFE2-02` |
| ~~**OI-EEGNET-25**~~ | **✅ CLOSED at Rev 6 — computed, `scripts/pod-pattern-coverage.ts`; result at §1.10.5.** Reformulated as the montage-independent **lattice covering radius**, which needs no 10-20 coordinates and so did not wait on REG-1. **Answer: the optimisation does not rescue five pods, and centre-plus-ring is not strictly better at low N.** Only centre+ring7 (8 pods, 34/90 emitters) meets ±10 mm worst-case at 9.4 mm, and that is covering error alone. The finding is against discrete selection generally. Model validated: a single centre pod gives 20.0 mm, exactly half the 40 mm pitch. *Original text:* **Optimise the pod pattern against the real target set — this is the deciding input, and it does not exist.** Every figure in §1.10.4 assumes N pods evenly spaced on one ring at r = 14.51 mm; `D-1` already reserves the tile centre, so **centre-plus-ring** is the natural pattern and turns a 1D angular problem into a 2D covering one. Minimise worst-case residual over the actual 10-20 targets across all 80 socket positions (neither uniform nor centred) using `hardware/np_socket_map.json`. **It could make five behave like six or better**, at five's emitter and scalp-load cost | Systems | **CLOSED — Rev 6** |
| **OI-EEGNET-27** | **Montage-specific pod-pattern fit, once REG-1 lands.** §1.10.5's covering radius answers *"any target anywhere"* and is therefore a bound, not a design target. A fit against the **nine actual T1 sites** (Fp1/2, F3/4, C3/4, P3/4, Oz) would do materially better — the p95 column (7.4–10.3 mm vs 9.4–17.3 mm worst case) indicates roughly how much. It needs 10-20 coordinates on the shell, which is `REG-1`, and it shares that dependency with `OI-EEGNET-14` and `OI-SESPWR-01`. **Do not use it to reopen a pod count settled on the covering radius** — a montage-specific pattern is fragile to any montage change, and `NP-HEX-ZM-001` §4a's research mission is arbitrary montage design | Systems + Clinical | **REG-1** |
| **OI-EEGNET-28** | **The whole-montage assignment problem is not computed, and it is where multi-pod tiles earn their keep (§1.10.6).** §1.10.5's covering radius is a *per-target* bound — for this point, does *some* socket reach it. A montage asks whether **all** sites can be served **simultaneously**, when each socket carries one tile: a matching problem over (site → socket → pod), not a covering one. It matters because assignment slack at today's single centre pod is **~1.00 socket per point**, i.e. none — any two sites contending for a socket means one cannot be placed at all, which is §1.3's Fp1/Fp2 defect generalised. Multi-pod raises slack (centre+ring7 → 1.33), so **the multi-pod case is strongest exactly where Rev 6 measured nothing**: dense montages, i.e. T2's 21 sites. Needs 10-20 coordinates, so it waits on **REG-1** and pairs with `OI-EEGNET-27` — one fixture answers both | Systems + Clinical | **REG-1; with `OI-EEGNET-27`** |
| **OI-EEGNET-26** | **`np_module_map_check_placement()` cannot express a pod-count requirement, and would pass a wrong build silently (§1.10.7).** `type_mask` is an *element-type* predicate — a 1-pod and a 4-pod tile both satisfy *"dual electrode at this socket"*, **including at socket 74, where the photoparoxysmal halt depends on the gate.** The fix is a count/geometry field in the requirement, not new element-enum entries, which would make a type system carry a quantity. Same weakness §7.2.4 finds for the net, different cause. Routed to `NP-HEX-ZM-001` §4a as owner of the identity model | FW + Systems | **Safety-adjacent; with `OI-EEGNET-23`** |
| **OI-EEGNET-21** | **An electrode-only tile type (T1-E) does not exist, and the reason electrode sites are scarce does not survive inspection (§1.8).** Per-configuration tile population *"has never been decided"* (`OI-COST-01`); the only argument on record against full population is `NP-HW-HEXTILE-001` §6.4's concurrency ceiling, which is a **power** argument that does not reach an electrode. The lattice — all ~80 sockets, all 18 cluster controllers — is paid for in every configuration (`NP-COST-001` A-2), so the marginal cost of a populated socket is one tile, and tile cost is dominated by the $11.53 driver/metering (~$10 of it InGaAs) and by emitters, none of which an electrode uses. **T1-E is the only option in §§1.6–1.8 that moves term U the right way: −17.3 % emitters while doubling electrode sites, and it strictly dominates T1-B at constant electrode count.** **Rev 4 sharpens the open question rather than closing it:** per `NP-PWR-BUDGET-001` §3.7, coverage is the only quantity that scales with tile count, so T1-E costs **illuminable area at those sites and nothing else** — it cannot reduce deliverable dose, which is envelope-bound at ~13–14 W optical regardless of population. The trade is against §3.6's whole-vault coverage mode specifically. Decide it against `NP-OPT-PSF-001` and `OI-PWR-07`. Depends on `OI-HFE2-10` for the fifth-type marking; does **not** relieve the N ceiling of §1.7.5. **Note the sequencing this creates, because it is unusual and should be deliberate rather than inherited:** an HFE formative on tactile discrimination sits *upstream* of a tile-taxonomy decision. If `OI-HFE2-10` falls back to the bar row, the taxonomy caps at four types and T1-E needs either a re-encoding or a type it can displace. The dependency runs the right way — marking is cheap to change before the mould insert is cut, taxonomy is not — but nothing else in the document set has this shape | Principal + Product + Systems | **With OI-COST-01 and OI-HEXTILE-06** |

## 9. Cross-references

`NP-HEX-ZM-001` §3.1 (arc, tile count), §3.2 (10-20 rows, Fp/Oz defects), §4a (T1-B), §5.2 (shield
seam), §5.3 (fluxgate siting) · `NP-HELMET-GEOM-001` §0 (inner-shield abandonment), §2 (radial stack),
§5 (bezel, no inboard contact) · `NP-DRV-SHELL-002` §3.5 (N4, PAN), §5.1 (socket pins), §9.1–§9.6
(EMI, `REQ-EMI-01..11`), §10.1 (BOM) · `NP-HW-HEXTILE-001` §1 (pinout union), §7.2 (SMART-1 option) ·
`NP-DRV-SHELL-002` §5.1.4–§5.1.6 (contact count, force, plate load), §10.1 (socket BOM) ·
`NP-HW-HEXTILE-001` §4.5 (T1-B masking), §7.1 (pad array, row asymmetry, contact force), D-1/D-5 ·
`NP-HFE-002` §2.3 (discriminability), §2.5 (type vs position), §3 (C-1…C-9), §7.1(d) (site marker),
§7.3 (L3 type marking), §7.4 (orientation) · `NP-HW-HUB-001` §5 (N4 channel count), §7.2 (enable word) ·
`NP-HW-HUB-001` §9.5 (calibration is module property) · `NP-THERM-BEZEL-001` (bezel, THERM-1) ·
`NP-RISK-002` (RISK-21) · `NP-COST-001` §2 A-1/A-2 (tile population, L1 carrier), §2 (term U), §5 (emitter formula), §6 (`OI-HEXTILE-06`) ·
`NP-HW-HEXTILE-001` §6.4 (populate-all argument), §9 (concurrency ceiling), §9.3 (fourth consequence), D-2 (PD co-location), `OI-HEXTILE-20`/`-21` ·
`NP-PWR-BUDGET-001` §3.4 (efficacy floor), §3.5 (populated ≠ driven), §3.6 (whole-vault coverage mode), §3.7 (irradiance vs output vs coverage) ·
`NP-SES-PWR-001` §2 (measured concurrency 2–32), §8 (`OI-SESPWR-01` lobe-scale vs electrode-scale) ·
`NP-FEAS-FNIRS-001` (S-D separation window, Risk A isosbestic) ·
`NP-HFE-002` §2.3(a) (counting range), §7.3 Rev 2 (nested-figure encoding, 6 types) ·
`firmware/hub_control/include/np_module_map.h` (`type_mask`) · `hardware/np_socket_map.json` ·
`scripts/generate-simulator-data.ts` · `scripts/pod-pattern-coverage.ts` (covering radius + assignment slack) ·
`docs/status/pr-defect-retrospective.md` §2.4 (evidence-class tags; re-trace before eliminating on a number) — a process record, **not** a controlled document: no serial ·
`NP-HEX-ZM-001` §5.4a (cluster clamp, plunger, ejector springs, actuator intent) · `NP-OPT-PSF-001` ·
`NP-HFE-002` §5 · `NP-ENV-OPRANGE-001` §4 · `NP-CONV-001` Rev 6 · CLAUDE.md §3, §4.2, §4.3, §4.4, §5.1

---

*Rev 7 scopes Rev 6's result: its ±10 mm column is a T1 answer per `OI-EEGNET-15`, and its covering
radius is a per-target bound that omits socket contention — the one axis on which multi-pod tiles win,
and the one that grows with montage density. Nothing in Rev 6 is withdrawn; its reach is bounded.*

*Rev 5 adds §1.10 and corrects §1.4/§1.5: a per-site placement file already meant one part per
electrode site, so `R-2` was broken before this section proposed anything. The pod-count question now
waits on `OI-EEGNET-25` (pattern optimisation) and, as always, on `OI-EEGNET-14`.*

*Rev 4 reconciles Rev 3 against PR #284, adds §1.9 and `OI-EEGNET-22`, and corrects one stale citation
(the "~6 tiles" concurrency figure). Two Rev 3 arguments are corroborated rather than disturbed. The
next action is still `OI-EEGNET-14` — which Rev 4 shows has a second consumer in `OI-SESPWR-01`.*

*Rev 3 adds §1.6, §1.7 and `OI-EEGNET-18/19/20`; it decides nothing and reverses nothing. The next
action is unchanged — `OI-EEGNET-14`. The one item with a deadline is `OI-EEGNET-20`, because socket
contact count stops being free at the socket-tooling cut.*

*Rev 2 corrects Rev 1's central premise (see the Rev 2 banner) and demotes the net from
recommendation to fallback. The document remains a DRAFT and must not be baselined. **The next action
is not a net decision — it is `OI-EEGNET-14`, the seating-concentricity measurement**, because that
one number decides whether §1.4's zero-tooling in-tile offset is sufficient and therefore whether
§§2–6 are needed at all for T1. `OI-EEGNET-15` decides the same question for T2 independently. The
document is written to be decidable, not to be adopted.*
