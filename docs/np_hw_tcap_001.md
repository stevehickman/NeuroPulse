# T2 Clinical Electrode Cap — Wiring and Driver Channel Assignment

**Project:** NeurOne
**Document:** NP-HW-TCAP-001
**Revision:** 1
**Date:** 2026-09-09
**Status:** DRAFT
**Effective Date:** 2026-09-09
**Author:** NeurOne Systems Engineering
**Approved By:** — (new document)
**References:** CLAUDE.md §3 T2 additions, §4; `NP-FW-HD-001` Rev 4 §4.1, §6.2, §6.3, §6.4, §7.1; `NP-HW-EEGNET-001` Rev 7 §5.5, §6.2, §6.4, §6.6; `NP-NPPS-REF-001` Rev 15 §4.12, §4.13; `NP-COST-001` Rev 2 §2, §4, §5; `NP-DT-001` DI-PERF-08, DI-SAFE-01; `NP-ART-001` Rev 1 §2, §6; `NP-CONV-001` Rev 6 §4, §5, §6, §8; Jurcak et al. (2007) *NeuroImage* 10-20 scalp-to-MNI atlas
**Related Issues:** OI-TACS-02 (this document closes half (i)); OI-TACS-01 (closed 2026-09-07); OI-EEGNET-07; OI-ART-06
**Gate:** — (§3 binds immediately, as the map firmware already depends on. The cap *artifact* has no assigned gate because it has no register entry — OI-TCAP-06)
**IEC 62304 Class:** N/A (hardware interface specification). It constrains SW-02 Class B code (`firmware/sloreta_hdtdcs/`) and the Class C charge-density interlock's operating assumptions.
**Supersedes:** None — first issue. It removes `k_driver_channel[]` (`firmware/sloreta_hdtdcs/src/np_hd_montage.c`) from the role of interim authority for the electrode-to-channel map.
**Parent Document:** None

---

> **Why this document exists.** The 2026-08-05 decision put one tACS driver channel on each of the
> 21 T2 cap electrodes. The document that would have pinned which electrode reaches which channel —
> this one — was named in that decision and never written, so a C array in Class B firmware became
> the authority for a hardware contract. `OI-TACS-01` then widened the hub wire format to carry all
> 21 channels (2026-09-07), which made the absence load-bearing rather than aspirational: **the wire
> format now commits to 21 channels that no controlled hardware document had ever specified.**
> `OI-TACS-02` half (i) is that gap. This document closes it.
>
> **What it does not do is grow into a cap specification.** Its subject is the *wiring* — which
> electrode, which conductor, which channel, carrying what current. The cap's mechanical
> construction, sizing across head circumference, materials and connector are a different artifact
> and a different document, and §8 says so explicitly rather than leaving the boundary to be
> inferred. **§4 is the section to read before treating any current figure here as permissible:**
> specifying what the conductor must carry surfaced three limits in the existing record that cannot
> all hold at once, and this document states the arithmetic rather than picking a winner.

---

## 1. Scope

**In scope, and binding:**

| § | What it pins |
|---|---|
| §2 | The electrode set, its ordinal numbering, and the fact that the ordinal *is* the wire ordinal |
| §3 | The electrode-to-driver-channel map, and the constraint any future map must satisfy |
| §4 | The per-conductor electrical envelope — the current the wiring must carry, and the three limits that disagree about whether it may be delivered |
| §5 | Conductor topology and count, to the extent the record determines it |
| §6 | How §3 is verified against firmware — mechanically, per `NP-CONV-001` §8 |

**In scope, and reported rather than decided:** §7 (BOM), which answers `OI-TACS-02` half (ii) with
a bound rather than a price, and finds the half under-scoped.

**Out of scope:** cap mechanics, electrode-to-scalp interface, sizing, materials, cable and
connector, and the 21-channel driver's own silicon selection. §8 enumerates these with the reason
each is out, so the boundary is a statement and not an omission.

---

## 2. The electrode set

**21 sites, 10-20 system, one Ag/AgCl sintered pellet each, dual-rated for qEEG recording and
stimulation current** (CLAUDE.md §3 T2). Scalp MNI coordinates are Jurcak et al. (2007) against the
MNI152 standard head; they are reproduced here because §3's non-aliasing constraint is a statement
about distances between them, and a constraint whose inputs live in another document is not
checkable.

> **`REQ-TCAP-01`** — The ordinal in §3's first column is the electrode's identity **everywhere**:
> the `np_hd_electrode_t` enum value, the `k_electrode_mni[]` / `k_electrode_names[]` index, the bit
> position in `np_mod_clin_tacs_params_t`'s three-byte channel mask, and the ordinal of the cap's
> physical conductor. There is no second numbering and no translation table.

The mask's three bytes divide the ordinals at 8 and 16 — `channel_mask_lo` 0–7, `channel_mask_hi`
8–15, `channel_mask_ext` 16–20 with bits 5–7 reserved and written clear (`NP-NPPS-REF-001` §4.12).
**Those boundaries are wire-format artifacts and carry no anatomical meaning.** Ordinals 16–20 are
Pz, P4, P8, O1, O2 — parietal and occipital sites that ended up in the third byte because the byte
ran out there, not because they form a group. Nothing may be inferred from which byte an electrode
falls in.

---

## 3. Electrode-to-driver-channel map (BINDING)

> **`REQ-TCAP-02`** — **The map is the identity: electrode *i* is driven by driver channel *i*, for
> *i* = 0…20. One electrode, one channel, no sharing in either direction.**

| Ordinal | Electrode | MNI (x, y, z) mm | Driver channel |
|---|---|---|---|
| 0 | Fp1 | (−21, 66, 5) | 0 |
| 1 | Fp2 | (21, 66, 5) | 1 |
| 2 | F7 | (−51, 26, −2) | 2 |
| 3 | F3 | (−35, 36, 64) | 3 |
| 4 | Fz | (0, 31, 75) | 4 |
| 5 | F4 | (35, 36, 64) | 5 |
| 6 | F8 | (51, 26, −2) | 6 |
| 7 | FC3 | (−40, 14, 67) | 7 |
| 8 | FC4 | (40, 14, 67) | 8 |
| 9 | T7 | (−70, −17, −2) | 9 |
| 10 | C3 | (−55, 0, 67) | 10 |
| 11 | Cz | (0, −10, 83) | 11 |
| 12 | C4 | (55, 0, 67) | 12 |
| 13 | T8 | (70, −17, −2) | 13 |
| 14 | P7 | (−51, −55, −2) | 14 |
| 15 | P3 | (−35, −55, 64) | 15 |
| 16 | Pz | (0, −65, 75) | 16 |
| 17 | P4 | (35, −55, 64) | 17 |
| 18 | P8 | (51, −55, −2) | 18 |
| 19 | O1 | (−21, −85, 5) | 19 |
| 20 | O2 | (21, −85, 5) | 20 |

**An identity map is a decision, not a default.** It is worth stating why, because the map it
replaced was also "obvious" to whoever wrote it. The retired 16-channel placeholder wrapped
electrodes 16–20 back onto channels 11–15 — an arithmetic convenience that aliased *geometric
neighbours*: Cz↔Pz, C4↔P4, T8↔P8, P7↔O1, P3↔O2. A 4×1 ring draws its four cathodes from the
electrodes nearest its anode, so aliasing neighbours guarantees collisions rather than risking them.
C4↔P4 made the M1_R ring undeliverable outright, because P4 is one of C4's four nearest electrodes
(`NP-FW-HD-001` §6.4).

**16 was never the binding number.** A bilateral 4×1 energises 10 electrodes, so 16 channels were
always sufficient in count; every observed collision came from the mapping. Going to 21 removes the
question rather than re-solving it.

> **`REQ-TCAP-03`** — **If channel sharing is ever reintroduced under cost pressure, no two
> electrodes sharing a channel may lie within one ring radius (~60 mm) of each other**, measured on
> the §2 coordinates. This is the constraint the identity map satisfies trivially and the retired
> map violated five times.

`REQ-TCAP-03` is retained as a live requirement rather than as history because §7 finds the
per-channel cost unpriced: a future costing could reopen sharing, and this is the rule it would have
to be checked against. The check is mechanical — all pairwise distances on §2's table — not a
matter of reading the map and finding it reasonable. Every one of the five aliased pairs above
reads as reasonable.

**Distinctness is verified at run time as well as at design time.** `np_hd_montage_validate()`
re-checks that every electrode in a montage maps to a distinct channel, across both hemispheres of
a bilateral montage rather than within each ring (`NP-FW-HD-001` §6.3). Under `REQ-TCAP-02` that
check cannot fail on channels alone — one electrode is one channel — but it is retained deliberately,
because the *electrode* claim it also enforces is a statement about physics that holds under any
wiring: one 3.5 mm pellet carries one net current whatever the map says.

---

## 4. Per-conductor electrical envelope

### 4.1 What the conductor must carry

The cap conductor for a dual-rated site carries whichever modality demands most of it, not the
modality that named it. Two make claims on it:

| Modality | Per-electrode ceiling | Simultaneously live | Source |
|---|---|---|---|
| sLORETA HD-tDCS | **2 mA** DC, 30 s ramp | ≤5 (ring), 10 (bilateral) | `NP_HD_MAX_CURRENT_UA`; `NP-FW-HD-001` §7.1 |
| Clinical tACS | **4 mA** peak, 0.5–100 Hz, charge-balanced biphasic | up to 21, independent | `NP-NPPS-REF-001` §4.12; CLAUDE.md §3 |

> **`REQ-TCAP-04`** — Each of the 21 stimulation conductors, its termination, and every switch or
> contact in series with it is rated for **4 mA continuous**, not 2 mA. The 2 mA figure is
> HD-tDCS's own limit and is not the cap's.

> **`REQ-TCAP-05`** — The rating is **per conductor and concurrent on all 21**. Clinical tACS
> channels are independent (`NP-NPPS-REF-001` §4.12), so no thermal or compliance budget may assume
> the ≤5-of-21 concurrency that HD-tDCS happens to exhibit. Aggregate injected current is separately
> bounded by charge balance across the montage — the montage sums to zero — but that bounds the
> *sum*, not any individual conductor.

### 4.2 Three limits that cannot all hold, stated rather than resolved

Writing `REQ-TCAP-04` required knowing whether 4 mA into one of these electrodes is permissible.
It is not answerable from the record, because three figures that are each individually stated
disagree once the electrode area is substituted in. All three are in
`firmware/sloreta_hdtdcs/include/np_hd_config.h`, within fifteen lines of one another.

**Given** `NP_HD_ELECTRODE_AREA_CM2` = 0.0962 cm² (9.621 × 10⁻⁶ m², a 3.5 mm pellet):

| # | Stated limit | Where | What it permits on this electrode |
|---|---|---|---|
| L1 | 2 mA per electrode | `NP_HD_MAX_CURRENT_UA` = 2000 | — (this is the current itself) |
| L2 | 6.0 A/m² tissue current density | `NP_HD_MAX_ELECTRODE_DENSITY_A_M2` | **57.7 µA** |
| L3 | 40 µC/cm² charge per phase | `NP_HD_MAX_CHARGE_DENSITY_UC_CM2`; CLAUDE.md §3; `NP-DT-001` DI-SAFE-01 | frequency-dependent — see below |

**L1 against L2.** 2 mA over 9.621 × 10⁻⁶ m² is **207.9 A/m²** — **34.6×** L2. The tACS ceiling of
4 mA is **415.8 A/m²**, **69.3×**. Inverted: at 6.0 A/m², this electrode may carry 57.7 µA, and
delivering 2 mA within L2 needs 3.33 cm² — a **20.6 mm** diameter pellet, not 3.5 mm.

**L1 against L3.** L3 caps charge per phase at 40 µC/cm² × 0.0962 cm² = **3.85 µC**
(`NP_HD_MAX_CHARGE_PER_PHASE_UC`, and the arithmetic agrees). For a sinusoid of peak *I* at
frequency *f*, charge per half-cycle is *I*/(π*f*), so L3 binds *I* ≤ 3.85 µC × π*f* :

| Frequency | Peak current L3 permits | Against the 4 mA ceiling |
|---|---|---|
| 0.5 Hz (bottom of the tACS band) | **6.0 µA** | 660× over |
| 40 Hz (top of CLAUDE.md §3's band) | **0.48 mA** | 8.3× over |
| 100 Hz (top of `NP-NPPS-REF-001` §4.12's band) | **1.21 mA** | 3.3× over |

**So the ≤4 mA tACS ceiling and the 40 µC/cm² limit are jointly unreachable across the entire
frequency band, at either band's top end.** The binding constraint on a small electrode is charge
density, it is *frequency-dependent*, and **no constant in the system expresses that dependence** —
`NP_HD_MAX_CURRENT_UA` is a scalar, and the only run-time check is against cumulative session
charge, not per-phase charge at the authored frequency.

**What is enforced today, and what is only declared.** L1 is checked
(`np_hd_stim.c` rejects an anode current above `NP_HD_MAX_CURRENT_UA`). **L2 has no production
consumer at all** — the only reference to `NP_HD_MAX_ELECTRODE_DENSITY_A_M2` in the tree is a FAI
assertion that the constant's own value is ≤ 6.0, which is a tautology over a `#define` and not a
statement about any delivered current; `NP_HD_ELECTRODE_AREA_M2`, commented *"for A/m² computation"*,
is referenced by nothing. L3 is checked, but against accumulated session charge rather than per
phase, which is a different quantity from the one the limit names.

**This document does not pick a winner, and the reason is not caution.** The three resolutions are
not interchangeable: enlarging the electrode is a cap redesign, lowering the current is a clinical
capability reduction, and raising L2 is a safety-basis change requiring the Bikson-lab derivation
`np_hd_config.h` cites to be revisited against 3.5 mm HD electrodes. That is an EE-plus-clinical
decision with regulatory surface, and it is `OI-TCAP-01`. **It is not a live hazard today** — the
T2 module drivers are stubs (`np_mod_t2_stubs.c`), no cap exists, and the Class C interlock in the
safety MCU is an independent enforcement path that this analysis does not touch. It is a
specification defect that must not reach a cap drawing.

### 4.3 Impedance and verification, per site

Carried unchanged from `NP-FW-HD-001` §3 and recorded here because they are properties of the
wiring, not of the firmware that reads them: **≤10 kΩ** per electrode or stimulation aborts,
measured at **1 kHz AC**; a 30 s ramp on entry and exit; per-hemisphere impedance check before a
bilateral montage energises.

---

## 5. Conductor topology and count

`NP-HW-EEGNET-001` §6.2's topology rules bind the T2 cap as they do the T1 net — a **tree**
terminating at one posterior connector, **no closed conductive ring at any scale**, all leads
co-routed in one flat bundle so the lead-to-reference loop is a ribbon rather than a bowl-scale
circuit, and a **carbon-loaded polymer guard driven from the DRL** rather than a metallic braid
(§6.5). The reference is net-borne at FCz with a net-borne DRL (§6.6).

**The conductor count is not determined by the record, and the undetermined part is large.**
`NP-HW-EEGNET-001` §6.4 resolves the recording-versus-stimulation conflict differently for the two
tiers: a recording lead wants series resistance (≥5 kΩ during a TMS pulse) and a stimulation lead
cannot have it (10 kΩ at 2 mA is 20 V of drop and 40 mW in the puck), so where TMS is present the
site needs **two** conductors rather than one.

| If | Conductors |
|---|---|
| Concurrent TMS **and** full 21-channel tES is required | 21 × 2 + reference + DRL = **44**, plus guard |
| It is not | 21 + reference + DRL = **23**, plus guard |

That question is **`OI-EEGNET-07`**, open, and `NP-HW-EEGNET-001` §6.4 already names it as *"the
single requirement that doubles the T2 tail."* This document cannot settle it — it is a clinical
protocol question, not a wiring one — and records instead that it is the **larger** of the two cost
terms in this artifact's neighbourhood, considerably larger than the five current sources
`OI-TACS-02` half (ii) asks about (§7.4).

> **`REQ-TCAP-06`** — Whichever count `OI-EEGNET-07` yields, §3's ordinals are unaffected. A
> two-conductor site is one electrode with one ordinal and one driver channel; the second conductor
> is a recording path, not a second channel.

---

## 6. Verification — the map is diffed, not read

`NP-CONV-001` §8 requires cross-document interface agreement to be established by mechanical diff,
on the evidence that all three of the naming collisions that prompted it survived multiple human
readings. **A name mismatch reads as agreement, and so does an off-by-one in a 21-row map.**

> **`REQ-TCAP-07`** — §3's table is diffed against firmware by
> **`scripts/check-tcap-map.ts`**, gated in CI. It compares, row for row: the electrode name against
> `k_electrode_names[]`, the MNI triple against `k_electrode_mni[]`, and the driver channel against
> `k_driver_channel[]`, all in `firmware/sloreta_hdtdcs/src/np_hd_montage.c`; and the row count
> against **both** `NP_HD_DRIVER_CHANNELS` (`np_hd_config.h`) and `NP_CLIN_TACS_CHANNELS`
> (`firmware/hub_control/include/np_hub_types.h`). It also re-derives `REQ-TCAP-03` from §2's
> coordinates rather than trusting the map to be an identity.

The two channel-count constants are checked separately and deliberately. They are spelled in two
trees because the mask width is a wire contract and the hub does not include the sLORETA tree;
`np_protocol_tests.c` already asserts they are equal to each other, and this check adds the third
corner — that they equal what this document specifies. Two of the three agreeing is the failure
mode worth catching.

Per `NP-CONV-001` §8, the check was **falsified before being trusted**: `--self-test` drives the
real entry point against fixtures that perturb one channel value, one electrode name, one MNI
coordinate, the row count, and each constant independently, and confirms a non-zero exit and the
expected message for each. A probe that has only ever been run against a passing tree has
demonstrated nothing.

**What this changes about authority.** Until now `k_driver_channel[]` was the interim authority for
the map, which is to say Class B firmware was the source of truth for a hardware contract that
Class C limits are derived against. That inverts the intended direction. From this revision the
direction is: **this document specifies, firmware implements, and CI proves they agree** — and if
they disagree, the firmware is wrong.

---

## 7. `OI-TACS-02` half (ii) — the BOM delta

**The question as posed cannot be answered, can be bounded, and is under-scoped. All three matter.**

### 7.1 It cannot be priced

`OI-TACS-02` asks for the cost of 5 additional precision current sources and their sense paths.
There is no per-channel figure to multiply: the 2026-08-05 decision records in terms that the
16-channel configuration had *"no sourcing document, no BOM line, and no named silicon,"* and none
has been selected since. This is the analog counterpart of `OI-HEXTILE-02` on the emitter side, and
it produces the same result `NP-COST-001` §5 reached there — **a term that is not merely unpriced
but unpriceable from the record.** Inventing a per-channel figure would be worse than the bound
below, because it would look like a costing.

### 7.2 It can be bounded, and the bound settles the question

The item was raised because the Pro rows are where a cost surprise lands (`NP-COST-001` §4: Pro
Entry and Pro Full are the only two configurations carrying positive margin). That question is
answerable without a unit price, by inverting it.

**Materiality threshold.** Let *X* be the unpriced per-channel cost including its sense path. The
5-channel BOM delta is 5*X*, and it reaches COGS through that configuration's own A-3 multiplier
(`NP-COST-001` §2 A-3: Pro Entry 1.639, Pro Full 1.745):

| Config | COGS delta | Per-unit margin at price in force | *X* that would erase it |
|---|---|---|---|
| Pro Entry | 8.195 *X* | +$2,499 | **$305/channel** |
| Pro Full | 8.725 *X* | +$10,163 | $1,165/channel |

**Pro Entry binds, at $305 per channel.**

**Upper bound from the record.** The published pre-hex BOMs decompose: Home Standard $405, Pro Entry
$833. Pro Entry is Home Standard's hardware plus the T2 additions, so **every T2 hardware addition
together — the 21-channel qEEG cap, 1170 nm deep PBM, the clinical tACS driver and the rest — is at
most $428.** The tACS driver is one item inside that, so at 16 channels it is at most **$26.75 per
channel**, and the 5-channel delta is at most **$134 of BOM / $219 of COGS**.

**$26.75 against a $305 threshold is a factor of 11**, and the bound is absurdly conservative: it
credits the entire T2 hardware addition to the tACS driver alone. The delta cannot flip either Pro
row, and cannot make a material dent in either margin. **Stated so it is falsifiable:** if silicon
is ever selected above **$305 per channel including its sense path**, this conclusion is void and
`OI-TACS-02` half (ii) reopens. Nothing else about the finding needs re-derivation.

### 7.3 It is under-scoped — the same shape as `OI-COST-07`

§7.2's bound rests on the 16-channel baseline having been absorbed into the $833 old BOM. **The
record says it was not**: *"no BOM line"* is the 2026-08-05 wording. If the baseline was never
costed, then the uncosted quantity is not the 5-channel delta — **it is the whole 21-channel driver
stage**, and `OI-TACS-02` half (ii) names the smaller term.

This is precisely the finding `NP-COST-001` §5 made against `OI-HUB-C08`, which *"names only the
drive electronics"* while the larger emitter delta went unowned (`OI-COST-07`).

**The conclusion survives the re-scope, but not by the same argument, and the difference matters.**
§7.2's $26.75 ceiling was derived by locating the driver *inside* the $428 T2 envelope; under this
section's premise it is not inside it, so that derivation is unavailable and would be circular if
carried over. What survives is the threshold, which never depended on the decomposition: at
**$305 per channel**, a 21-channel stage is **$6,403** — **2.9–3.0× the entire Pro Full BOM floor**
($2,136–2,198) and 4.2–4.4× Pro Entry's. An analog driver stage costing three times the whole device
is excluded by inspection, so the whole stage is immaterial for the same reason the delta was.
**The conclusion does not change; the item's scope does.** Raised as `OI-TCAP-04`.

### 7.4 The larger uncosted term next door

If the concern is what the 21-channel commitment costs, the five current sources are not the place
to look. **`OI-EEGNET-07` decides whether the T2 cap tail is 23 conductors or 44** (§5) — a
doubling of the harness on a per-unit part, gated on a clinical question nobody has been asked. No
costing owns it: `NP-COST-001` §2 A-1's tile-population table has no row for the T2 cap at all, and
the cap does not appear in `NP-ART-001` §2's artifact register (`OI-TCAP-06`). It is not costed
here either, because the conductor count is undetermined and costing an undetermined count is the
error §7.1 declines to make. It is recorded so that the next costing pass looks at the term that is
large rather than the term that was noticed.

### 7.5 What this does not settle

The figures in `NP-COST-001` §4 are **floors** that already exclude term **U**, and §7.2's bound is
an addition to a floor, not a correction of one. Nothing here revises a published BOM, COGS or GM%
figure, and no price is set — `OI-COST-10`'s sequencing constraint is untouched. Board area,
connector pin count and the tES current rating of the analog crosspoint switch (`NP-HW-HUB-001`
§7.4's residual, sitting with `NP-DRV-SHELL-002`) are not BOM-line costs and are not bounded by any
of the above.

---

## 8. What this document does not specify, and why

Naming the boundary is the point of this section; `NP-ART-001` §4's finding was that nine artifacts
had no owning document and that the gap was invisible because nothing enumerated what was missing.

| Not specified here | Why | Where it must go |
|---|---|---|
| Cap mechanical construction, fabric/net, electrode housing | A different artifact from its wiring; no register entry exists to attach it to | `OI-TCAP-06`, then a cap specification |
| Sizing across head circumference | `NP-HW-EEGNET-001` §3.3 makes size count an *output of a measurement* (gate NET-1), and §5's interference table puts the T2 qEEG net explicitly **out of scope** of that document as a *"separate net part, same architecture"* | `OI-TCAP-05` |
| Cable, connector and hub-side termination | `NP-HW-EEGNET-001` §6.2 fixes the *topology* (tree, one posterior termination) but no connector is selected for the T2 cap | `OI-TCAP-05` |
| Conductor count | Undetermined — `OI-EEGNET-07` (§5) | `OI-EEGNET-07` |
| The 21-channel driver's silicon, compliance voltage and sense-path architecture | Never selected (§7.1) | `OI-TCAP-03` |
| Which of L1/L2/L3 governs (§4.2) | Clinical and safety-basis decision with regulatory surface | `OI-TCAP-01` |
| Electrode diameter | Follows from `OI-TCAP-01` — 3.5 mm is an EEG-electrode dimension carried into a stimulation duty | `OI-TCAP-01` |

**The cap is a manufactured artifact with no entry in `NP-ART-001` §2.** That register asks in
`OI-ART-06` whether it is complete; it is not, and this is one of the misses. It is recorded as
`OI-TCAP-06` and routed there rather than fixed here, because adding an artifact row is that
register's decision and it carries tooling, risk-register and FAI consequences this document is not
positioned to assess.

---

## 9. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **`OI-TCAP-01`** | **Three limits over the same electrode are mutually unsatisfiable (§4.2).** 2 mA on 0.0962 cm² is 207.9 A/m², **34.6×** the stated 6.0 A/m²; the 4 mA tACS ceiling exceeds the 40 µC/cm² per-phase limit at every frequency in the band (660× at 0.5 Hz, 8.3× at 40 Hz, 3.3× at 100 Hz). The three resolutions — enlarge the electrode to ~20.6 mm, lower the current, or revisit L2's safety basis for 3.5 mm HD electrodes — are a cap redesign, a capability reduction, and a regulatory-surface change respectively. **Not a live hazard** (T2 drivers are stubs, no cap exists, the Class C interlock is independent), but it must not reach a cap drawing | EE Lead + Clinical | **Electrode diameter; `REQ-TCAP-04`; any cap drawing** |
| **`OI-TCAP-02`** | **`NP_HD_MAX_ELECTRODE_DENSITY_A_M2` and `NP_HD_ELECTRODE_AREA_M2` have no production consumer.** The sole reference is a FAI assertion that the constant is ≤ 6.0 — a tautology over a `#define`, not a check on any delivered current. A declared limit nothing enforces reads as a control in a design review and is not one. Either give it a consumer or retire it; do not leave it stated | FW Safety | Follows `OI-TCAP-01` — the number may change first |
| **`OI-TCAP-03`** | **The 21-channel tACS driver has no named silicon**, no compliance-voltage specification and no sense-path architecture, so §7 can bound its cost but not price it. Analog counterpart of `OI-HEXTILE-02`. `REQ-TCAP-04`/`-05` (4 mA per conductor, concurrent on 21) are the input requirements for that selection | EE Lead | T2 BOM sign-off; `OI-TACS-02` half (ii) |
| **`OI-TCAP-04`** | **`OI-TACS-02` half (ii) is scoped to the 5-channel delta, but the 16-channel baseline had no BOM line either** (§7.3), so the uncosted quantity is the whole 21-channel stage. Same shape as `OI-COST-07` against `OI-HUB-C08`. The §7.2 bound holds either way; the item's scope is what is wrong | BOM sign-off | Scope of `OI-TACS-02` half (ii) |
| **`OI-TCAP-05`** | **The cap has no mechanical, sizing or connector specification** (§8). `NP-HW-EEGNET-001` §5 puts the T2 net explicitly out of its scope as a *"separate net part"*, so the exclusion is deliberate on both sides and the document simply does not exist. T2-blocking, not T1-blocking | Systems + ME | T2 cap tooling; `NP-ART-001` §3.2-class FAI |
| **`OI-TCAP-06`** | **The T2 cap is absent from `NP-ART-001` §2's artifact register.** `OI-ART-06` asks whether that register is complete; this is one miss, found by needing an artifact to attach `OI-TCAP-05` to. Adding the row carries tooling / risk-register / FAI consequences that belong to that register's owner | Systems | `NP-ART-001` completeness; `NP-COORD-001` G2 |
| **`OI-TCAP-07`** | **The tACS frequency band disagrees between two controlled sources** — CLAUDE.md §3 locks BES/tACS at **0.5–40 Hz**, `NP-NPPS-REF-001` §4.12 accepts **0.5–100 Hz** for clinical tACS, and validators enforce the latter. It changes §4.2's arithmetic (L3 permits 0.48 mA at 40 Hz, 1.21 mA at 100 Hz) but not its conclusion. Surfaced by needing one band to state the envelope in | Systems | Documentation consistency; input to `OI-TCAP-01` |

---

## 10. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-09-09 | NeurOne Systems Engineering | Initial release, closing **`OI-TACS-02` half (i)**. **§3 pins the electrode-to-driver-channel map as the identity map (`REQ-TCAP-02`)**, removing `k_driver_channel[]` from the role of interim authority — Class B firmware had been the source of truth for a hardware contract since 2026-08-05, and `OI-TACS-01`'s widening of the hub wire format to 21 channels (2026-09-07) made that load-bearing. `REQ-TCAP-03` retains the non-aliasing rule (~60 mm) as a live requirement rather than history, because §7 leaves per-channel cost unpriced and a future costing could reopen sharing. **§6 makes the map machine-checked** (`scripts/check-tcap-map.ts`, `REQ-TCAP-07`), diffing names, MNI coordinates, channels and both channel-count constants, and re-deriving `REQ-TCAP-03` from the coordinates rather than trusting the map; falsified against six injected perturbations before being wired in, per `NP-CONV-001` §8. **§4 is the section this document did not set out to write.** Specifying the conductor's rating surfaced that `NP_HD_MAX_CURRENT_UA` (2 mA), `NP_HD_MAX_ELECTRODE_DENSITY_A_M2` (6.0 A/m²) and the 40 µC/cm² per-phase limit are mutually unsatisfiable on a 0.0962 cm² electrode — 2 mA is **34.6×** the density limit, and the 4 mA tACS ceiling exceeds the charge-density limit at **every** frequency in the band because that limit is frequency-dependent and no constant expresses it. The arithmetic is stated and no winner is picked (`OI-TCAP-01`); L2 is additionally found to have **no production consumer** (`OI-TCAP-02`). **§7 answers `OI-TACS-02` half (ii) with a bound, not a price**: no silicon is named, so no per-channel figure exists, but the materiality threshold is **$305/channel** (Pro Entry binds) against a record-derived ceiling of **≤$26.75/channel** — a factor of 11 — so the delta cannot move either Pro row, stated falsifiably. **§7.3 finds the half under-scoped**: the 16-channel baseline had no BOM line either, so the uncosted term is the whole 21-channel stage, the same shape as `OI-COST-07` against `OI-HUB-C08`. **§7.4 names the larger term next door** — `OI-EEGNET-07` decides whether the cap tail is 23 or 44 conductors, and no costing owns it. §8 states the boundary rather than leaving it inferred, and finds the cap absent from `NP-ART-001` §2's register (`OI-TCAP-06`). No locked decision is changed and no published cost figure is revised. |
