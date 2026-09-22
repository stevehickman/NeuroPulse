# Accessory and Companion-Software Priority Set

**Project:** NeurOne
**Document:** NP-ACC-PRIORITY-001
**Revision:** 4
**Date:** 2026-09-22
**Status:** DRAFT — awaiting principal + product sign-off. Ranks nothing that is locked; sets no price.
**Effective Date:** —
**Author:** NeurOne Systems Engineering
**Approved By:** — (pending principal + product review)
**References:** NP-THERM-COOL-001 Rev 11 (§6.7 the two thermal accessories, §6.7.1 the binding-term argument, §8 D-3 the two-item precedent this generalises, §8/§9 the rejected options); NP-PWRSRC-001 Rev 1 (§5.5 CEM43 and time-at-ceiling, §8 mains siting, §12 the honest-source prohibition); NP-PWR-BUDGET-001 Rev 3 §3.4 (the efficacy floor); NP-APP-ROADMAP-001 Rev 3 (§3 core-app prerequisite, §4.2 the four Watch phases, §6 the regulatory declaration that sets the Watch app's class, §7/§10 OI-WA-02); NP-FMEA-GEOM-001 FMEA-G02-01 (electrode impedance cutoff); NP-FMEA-001 FMEA-M06 (pre-session and mid-session impedance); NP-DP-001 (partner optician network, Month 10); NP-TOOL-LENS-001 Rev 2 (lens and goggle-arm tooling); NP-TOOL-HUB-001 Rev 1 F-02 (port-cover anchor posts, OI-ART-07); NP-CONV-001 §4.0 (document naming), §8 (falsify before trusting); CLAUDE.md §1 (tier timelines), §2 (configurations, the cost floor and `OI-COST-10`), §3 (modality roster and the accessory-bearing modalities), §5 (measurement-triggered reminders), §6; `docs/reference/accessories-roadmap.md`; `docs/reference/modality-stack.md`; `docs/reference/commercial-model.md` §2.1/§2.2/§2.3; `docs/reference/service-network.md`; `docs/reference/hardware-detail.md`; `docs/status/pending-decisions.md` §13.2, §13.4; `docs/np_cost_001.md`; NP-HW-FITOVER-001 Rev 1 (the deferred fit-over assembly ranked at 19, and the decision that closes `OI-ACC-01`); NP-HFE-001 CT-03; NP-FW-BENCH-001 (`OI-VIS-03`/`-04`, `OI-BENCH-08`)
**Related Issues:** GitHub Issue #344 (this document); #24 (cervical tcVNS), #25 (S3 Rx / optician network), #30 (HOPE Phase 3), #31 (mastoid pad), #32 (Watch app), #73 (T2 programme)
**Gate:** No gate. Not tooling-blocking — with one exception recorded in §7: the mastoid-pad anchor boss is a first-cut provision whose window closes with shell tooling, independently of the ranking.
**IEC 62304 Class:** — (planning document; no code changed, no requirement altered)
**Supersedes:** None — new document.
**Change Summary:** Rev 4 (2026-09-22) — **`OI-ACC-03` is closed: the cervical VNS gel pad has a row in `docs/reference/commercial-model.md` §2.3.** By principal decision, *"single-use"* is a performance guarantee, not a reuse prohibition: the pad is replaced **on failure**, detected by a **condition measurement** — the per-electrode ≤ 5.0 kΩ impedance check the safety MCU gates enable on (`NP-HW-CVNS-001` `REQ-CVNS-06`). Price and GM% **deliberately not set** (`OI-COST-10`; no COGS exists). Row 9's C3a placement is confirmed rather than moved: the blocking mechanism the row already named is the pad's trigger. **Raised: `OI-ACC-07`** — the refusal must reach the wearer naming the failing pad's location; the app half (message + neck diagram) is done, the hub half is open. No rank moved; §3 untouched. Rev 3 (2026-09-15) — **`OI-ACC-02` is closed by principal decision, and two claims this document made are falsified by it.** The foam's prompt was never a calendar in the product: the shipped app triggers it on a session count from the hub's SHDR-class `CONSUMABLE_STATUS`, and the headset has no `VBAT` rail, so a device-side calendar trigger is unimplementable. **This document's refusal to invent a trigger is upheld, not overturned — none was invented**; what was defective was the threshold's provenance, now relabelled an unvalidated placeholder (`OI-ACC-04`), and CLAUDE.md §2.3 gained a definition rather than an exception (Rev 48). **Falsified here:** §5 row 14's *"the one calendar-triggered consumable reminder in the document set"* (the mesh frame, interface covers and S3 Rx insert are all calendar-denominated in the Interval column) and §8's *"every other consumable in the table names a measurement"* (three rows name none, and the session counts live in the Interval column, not in Notes). The mesh frame is the worse case, its Notes naming a measurement that does not exist (`OI-ACC-05`). No rank moved; no item was added or removed; §3's criterion is untouched. Rev 2 (2026-09-14) — **`OI-ACC-01` is closed by principal decision, and the ranking gains its first cross-class falsification.** Corrective-eyewear users buy prescription lenses *for* the goggle assembly (the S3 Rx path); there is no intent to build a goggle that fits over the user's own glasses. Rank 2's class is therefore **confirmed C1 and derived rather than judged** (§5), and the deferred fit-over assembly is placed at **rank 19, bottom of the set** (`NP-HW-FITOVER-001`), by §3.5's procedure — reproducing the principal's "very low priority" without being told it (§4). 18 items → 19; 17 ranks → 18. No other row moved, which is §3.5's property working. Rev 1 (2026-09-13): first issue, closing the general TODO raised 2026-08-30 in `docs/status/pending-decisions.md` §13.2 by `NP-THERM-COOL-001` §8 D-3.
**Review Cadence:** Re-run on the triggers in §6 — not on a calendar. A ranking re-run on a schedule invites re-litigation; a ranking re-run on a trigger does not.

---

## 1. Why this document exists

`NP-THERM-COOL-001` §8 **D-3** ranked two accessories against each other — TEC base-station chiller
above hip ice pack, on value delivered — and in doing so exposed a gap it could not itself fill:
`docs/reference/accessories-roadmap.md` lists accessories with **no relative ordering at all**, and
CLAUDE.md §2.3's consumables are priced but not sequenced. The gap was raised as a general TODO
(`docs/status/pending-decisions.md` §13.2, 2026-08-30, owner principal + product).

This document closes it, and deliberately delivers **two** things rather than one:

1. **§5 — the ordered set**, covering every accessory and companion-software item in the document set.
2. **§3 — the criterion**, written down first and applied second.

The criterion is the part that matters. A ranked list alone is a snapshot: the next accessory
raised has no place to go, and putting it somewhere re-opens every pair above and below it. A
criterion makes placement a lookup. §3 is therefore stated so that **an item added in 2027 can be
placed by someone who was not in the room in 2026**, and §6 says when the whole set is re-run rather
than extended.

**What this document does not do.** It sets no price, changes no locked decision, alters no
requirement, and commits no schedule. It is an ordering of value delivered, and it is input to
programme sequencing rather than the sequence itself.

---

## 2. Scope — what is and is not an item

**In scope:** anything a user can buy or install that is not the device, and any companion software
that is not the core app. That is: optional accessories, field-upgrade modules, consumables and
wear parts, shade and lens options, and companion applications.

**Out of scope, with the reason** — recorded so that "complete" in §5 is checkable rather than
asserted:

| Excluded | Why it is not an item |
|---|---|
| Core iOS / Android / web app | The platform, not a companion. Sequenced by `NP-APP-ROADMAP-001` §3, which the Watch app is downstream of. |
| T2 HIPAA cloud · FHIR R4 · LSL · scripting API · multi-patient dashboard · sLORETA | **Contents of the Pro Full configuration** (CLAUDE.md §2.1), sold with the tier. Nothing to sequence separately. |
| TMS hub · 21-ch qEEG cap · 1170 nm deep PBM · clinical tACS | Same — configuration contents (CLAUDE.md §2.1/§3), not accessories. |
| T2 service contract ($1,800/yr) | A service attached to a configuration, not a roadmap item. |
| 45 / 65 / 100 W chargers | CLAUDE.md §2.2 is **LOCKED** and keyed to peak draw; the charger is auto-included at every upgrade and is never an optional purchase. The $19 at-cost 65 W upgrade is an intent signal, not an accessory. |
| S1 opaque shade (<0.5% VLT) | In the box (`docs/reference/modality-stack.md`). |
| On-head TEC · scalp-gap ventilation · vapour compression | **Not recommended** — `NP-THERM-COOL-001` §8 and §9. Rejected options are not low-priority items. |
| Sealed pneumatic loop | Out of scope by `NP-THERM-COOL-001` §8 **D-2**; `OI-THCOOL-06` closed with it. |

---

## 3. The criterion

### 3.0 Governing rule — rank the item for what it does, not for what it could be sold as

This is D-3's marketing constraint, promoted to the rule that governs how every other test is
scored. D-3 found that the hip ice pack extends the **ambient envelope** and does **not** shorten
sessions on a 45 W brick, and bound copy, packaging, the store page and the in-app upsell to say
only the former — the same prohibition `NP-PWRSRC-001` §12 states: do not sell a source that changes
nothing.

The ranking consequence is that an item is scored on its **demonstrated** effect. Where a document
states what an item does and what it does not do, the ranking takes both halves. An item cannot buy
rank with a claim its own specification withholds.

### 3.1 The four tests, applied lexicographically

**Lexicographic, not weighted.** Test 1 decides; Test 2 only separates items that tie on Test 1; and
so on. This is not a stylistic choice — it is what D-3 actually did. D-3 did **not** trade the ice
pack's lower draw (~1–2 W against 56–188 W), its preserved Mode 3 autonomy, or its independence from
mains against the chiller's benefit. It asked *what does each one change*, found that one raises the
binding term and the other does not, and stopped. A weighted score would have let the ice pack's
real merits buy back rank and inverted a decision already taken. The criterion reproduces the
decision it generalises (§4).

---

#### Test 1 — Capability class (primary key)

*What does the item change about the therapy the platform delivers?*

| Class | Test | Reading |
|---|---|---|
| **C1 — Access** | Without the item, the therapy does not reach the user **at all** | Two forms, both C1: the platform has no other way to deliver the channel, **or** the user cannot physically receive a channel the platform does deliver |
| **C2 — Binding relief** | The item raises the term that is **currently** the binding `min()` | D-3's own test, stated generally |
| **C3 — Continuity** | Without the item, therapy already delivered **stops or degrades** | Split in §3.2 |
| **C4 — Envelope** | Same therapy, **wider conditions of use**. No change to dose, protocol or concurrency | The ice pack's honest description, stated generally |
| **C5 — Experience** | No change to delivered therapy, its dose, or its envelope | |

Three notes on applying Test 1, each of which decided a real placement below:

- **C1's second form is not a courtesy.** A channel the platform delivers but a given user cannot
  receive is, for that user, a channel the platform does not deliver. This is what puts the S3 Rx
  system in C1 — *conditionally*, because the document set does not currently answer whether it
  needs to (`OI-ACC-01`, §7).
- **C2 is a claim about the present, not about size.** An item is C2 only while the term it raises
  is the one that binds. `NP-THERM-COOL-001` §6.7.1 is the worked instance: on a 45 W brick the
  **electrical** term binds first at 6.4 tiles, so cooling alone — which raises only `thermal` —
  cannot be C2 no matter how much cooling it delivers. When the binding term changes, classes
  change; §6 makes that a re-run trigger.
- **A regulatory declaration can set a class.** Where the product's own filing position declares an
  item to be non-therapeutic, C1–C4 are foreclosed and the item is C5 by that declaration rather
  than by judgement. This is how the Watch app is placed, and it is the least arbitrary placement
  in §5.

#### Test 2 — Reach (first tie-break)

*Over how much of the shipped installed base is the item's function available?*

Counted over **configurations** (CLAUDE.md §2.1), because that is a fact the document set states,
not an estimate: an item usable in Core–through–Pro Full reaches more than one usable from Home
Standard up, which reaches more than one usable on T2 only.

**Boundary against §3.3, stated explicitly because it is the easiest place to cheat.** Reach counts
the **addressable population**, which follows from the locked tier definitions in CLAUDE.md §1–§2.
It does **not** count how soon a gate will lift. A T2-only item has narrow reach because T2 is a
smaller installed base for the whole horizon of this ranking (T2 is 18–36 months post-T1, CLAUDE.md
§1) — not because `#73` is unresolved. The first is a product fact; the second is an open question,
and §3.3 forbids open questions from setting order.

#### Test 3 — Irreversibility of omission (second tie-break)

*Does not shipping it now foreclose something, or merely defer it?*

An item that must be provisioned at a window that closes — a tooling first cut, a socket, a partner
contract — ranks above an equal-class, equal-reach item that can be added at any time. Deferral is
cheap; foreclosure is not.

This test carries one consequence that is **independent of rank**, recorded in §7: a foreclosing
provision should be made whether or not the item it serves is ever released, when the provision is
free and the retrofit is not.

#### Test 4 — Cost to ship (final tie-break)

*Engineering and qualification work to get it into a user's hands.*

> **⚠ No cost, COGS, BOM, margin or retail figure is an input to this ranking, at any test.**
>
> This is a constraint, not a preference. CLAUDE.md §2.1: every configuration cost figure in the
> document set is a **floor**, not an estimate — each excludes the uncosted term **U**, because
> `OI-HEXTILE-02` has selected no 660/808 nm emitter and `OI-HUB-C08` therefore cannot be closed.
> Retail is unlocked but **no new price is set, and none may be set before `OI-HEXTILE-06`**
> (`OI-COST-10`). A ranking that consumed those numbers would be asserting a cost comparison the
> document set cannot make, and would silently re-rank itself when term U lands.
>
> Test 4 is therefore *relative engineering effort against work already specified* — a BOM delta
> quoted in §5 is context for the reader, never an input. Where two items tie through Tests 1–3 and
> their effort is not distinguishable from the document set, §3.4 applies.

### 3.2 C3 is split, because "degrades" and "blocks" are not the same fact

Consumables and wear parts all sit in C3, and ordering nine of them on reach alone produced an
ordering that contradicts what the safety architecture actually does. The split is mechanical:

- **C3a — blocking.** Absence stops a session, because an authentication or an interlock says so.
  The evidence is a named mechanism: an authenticated consumable (CLAUDE.md §2.3), an impedance
  interlock (CLAUDE.md §4.2; `NP-FMEA-001` FMEA-M06; `NP-FMEA-GEOM-001` FMEA-G02-01's >5× baseline
  cutoff).
- **C3b — degrading.** Absence reduces quality, comfort or service life, with no mechanism that
  refuses the session.

An item is C3a only if a document names the mechanism. "It would probably be bad" is C3b.

### 3.3 Dependencies are recorded, never ranked

Every row in §5 carries a **Gate** column naming what the item waits on and whether the gate is
**EXTERNAL** (the result is outside the project's control) or **INTERNAL**.

**No gate is an input to any of the four tests.** Two reasons, and the second is the operative one:

1. A gate's *lifting date* is not evidence about an item's value. HOPE Phase 3 reading out in
   mid-2026 or mid-2027 does not change what a 40 Hz mastoid channel delivers if it reads out
   positive.
2. **A ranking that consumed gate dates would silently re-rank itself every time a trial slipped or
   a supplier moved** — which is precisely the re-litigation this document exists to prevent, arriving
   through the back door. Issue #344 asks for the dependencies to be *recorded* rather than left to
   set the order; §5's Gate column is that record.

The practical reading: **rank says what to build; the Gate column says what you can build now.**
They are read together and neither substitutes for the other.

### 3.4 Ties

If two items tie through all four tests, **they are recorded as peers and share a rank number.**
Inventing an order to avoid a tie is the failure mode this criterion exists to prevent — an
invented order carries no reasoning, so the next reader re-litigates it. §5 contains one such
peer pair, and it is a real one: the two lens options are mutually exclusive (S2 is standard-lens
only) and therefore serve disjoint populations.

### 3.5 Placement procedure for a new item

Four steps, no meeting required:

1. Read §2. If the item is out of scope, record it in §2's table with the reason and stop.
2. Apply Test 1. Cite the document that establishes the class. If the class is C3, apply §3.2 and
   cite the mechanism, or default to C3b.
3. Apply Tests 2, 3, 4 in order **only against the items already in that class**. The rest of §5
   does not move.
4. Record the gate and its EXTERNAL/INTERNAL kind. If the class cannot be settled from the document
   set, raise an `OI-ACC-nn` open item rather than guessing, place the item at its **lower** candidate
   class in the meantime, and say so in the row — as `OI-ACC-01` does for the S3 Rx system.

Step 3 is the property that makes the artifact durable: adding an item touches one class, not the
list.

---

## 4. Falsification — does the criterion reproduce D-3?

Per `NP-CONV-001` §8, a rule is not trusted until it has been run against the case it claims to
explain. D-3 is the only ranked pair the document set contains, so it is the only available test —
and it is a real one, because it was decided *before* this criterion existed.

| | TEC base-station chiller | Hip ice / PCM pack |
|---|---|---|
| **Test 1 — class** | **C2** — arrives with a mains base station, so it raises **both** the electrical and the thermal terms; `maxConcurrent` moves and cascades shorten | **C4** — raises only `thermal`; on a 45 W brick the **electrical** term binds first at 6.4 tiles, so nothing about delivered therapy changes |
| **Tests 2–4** | not reached — Test 1 separates them | not reached |
| **Criterion's result** | **above** | **below** |
| **D-3's result (2026-08-30)** | **above** | **below — "low"** |

Test 1 alone separates them, in the direction D-3 decided, for the reason D-3 gave
(`NP-THERM-COOL-001` §6.7.1). Tests 2–4 are never reached.

**Two checks that the reproduction is not circular:**

- **The criterion had to be able to get this wrong, and a weighted one would have.** The ice pack
  wins on three real axes — draw (~1–2 W vs 56–188 W), preserved Mode 3 autonomy (the chiller has
  none), and no mains dependency — and it is the *only* one of the two that works away from a wall.
  Any additive rubric weighting autonomy or draw meaningfully inverts D-3. Lexicographic ordering is
  what holds the decision, and §3.1 says so rather than leaving it to be rediscovered.
- **The magnitude claim survives too.** D-3 called the ice pack **low** priority, not merely second.
  §5 places it **15th of 19** with only the two lens options, the Watch app and the deferred fit-over
  goggle below it. That was
  derived, not fitted: the ice pack's rank falls out of C4 sitting below C1–C3, and nothing in the
  criterion mentions the ice pack.

**The second test, and it is the one Rev 1 said was missing (added Rev 2).** Rev 1 recorded that D-3
compared two items *within* one class-pair, so the C1-over-C2-over-C3 ordering was argued rather than
falsified, and named that the part of §3 most open to challenge.

**A cross-class case has now arrived on its own.** On 2026-09-14 the principal decided that
corrective-eyewear users buy prescription lenses for the goggle assembly, that no fit-over goggle is
intended, and — independently — that a fit-over assembly should be specified but held at **very low
priority**, unbuilt absent specific user requests. The criterion was applied to it afterwards, by
§3.5's four steps, with no reference to that judgement:

| | Fit-over goggle assembly |
|---|---|
| **Test 1 — class** | **C5.** Access is already delivered to this cohort by rank 2, so it unlocks nothing; dose, protocol, concurrency and envelope are unchanged |
| **Test 2 — reach** | Narrowest in the set — below rank 18 |
| **Tests 3–4** | Forecloses nothing by waiting; highest engineering cost in the table |
| **Criterion's result** | **rank 19 — bottom of the set** |
| **Principal's judgement (2026-09-14)** | **"very low priority"** |

Two things make this a real test rather than a restatement. **It crosses classes** — the placement
turns on C5 sitting below C1, which is exactly the ordering Rev 1 could not falsify. And **the
criterion had to be able to put it higher**: the naive reading is that a fit-over assembly serves the
*same* access need as rank 2, which would make it C1 and put it near the top. §3.1's C1 test is what
refuses that — access is about whether therapy reaches the user, and it already does.

**What is still not falsified:** the C2-to-C3 and C3-to-C4 boundaries have no decided case either
way. §6's triggers remain written so that challenging the class ordering is a re-run of §5 rather
than an argument about one item.

---

## 5. The ordered priority set

19 items, 18 ranks (one peer pair). **Rank is value delivered; the Gate column is what it waits on;
§3.3 governs how to read them together.** Figures are context, never inputs (§3.1 Test 4).

### C1 — Access · without the item the therapy does not reach the user

| # | Item | Class evidence | Reach | Gate |
|---|---|---|---|---|
| **1** | **40 Hz mastoid vibrotactile LRA pad** (#31) | The 40 Hz somatosensory channel exists nowhere else on the platform. `docs/reference/accessories-roadmap.md` states it directly and names what does *not* substitute: the Watch's Taptic output is uncharacterised, wrist-to-brain coupling is soft-tissue attenuated, and `NP-APP-ROADMAP-001` Rev 3 records that **watchOS has no continuous-waveform haptic API at all** (`CHHapticEngine` does not exist there). Mastoid placement couples to bone | T1 + T2, every 40 Hz / GENUS protocol | **EXTERNAL** — #30, HOPE Phase 3 (Cognito Therapeutics, n=670, mid-2026). Release within 6 months if positive. **See §7: the anchor-boss provision does not wait on this** |
| **2** | **S3 prescription clip + Rx insert** (#25) | **C1 confirmed — `OI-ACC-01` CLOSED 2026-09-14 (principal).** Corrective-eyewear users buy prescription lenses *for* the goggle assembly; there is **no intent to build a goggle that fits over the user's own glasses**, and `NP-HW-FITOVER-001` §1 records that as a positive design decision. So the platform provides **exactly one path** by which this cohort reaches the visual modality, which is §3.1's second form of C1 exactly — *a channel the platform delivers but a given user cannot receive*. Rev 1 held this rank on a flagged judgement; it is now **derived** | T1 + T2 (Home Standard and up — goggles), the corrective-eyewear cohort | **INTERNAL** — partner optician network contract (`docs/status/pending-decisions.md` §13.4; `NP-DP-001` Month 10). `docs/reference/service-network.md` records Tier A opticians as **already engaged via the S3 programme**, so this gate is further along than its rank implies |
| **3** | **1064 nm snap-in smart zone module** (field upgrade) | Adds the **cortical depth tier** of the three-tier penetration stack (660 surface → 1064 cortical → 1170 deep). No other part delivers it; base modules are unchanged | T1 + T2, opt-in per zone | **INTERNAL** — hub-side cluster fan-out unimplemented (`OI-HUB-C01…C19`); emitter selection (`OI-HEXTILE-02`). **Also regulatory:** 1064 nm irradiance is inside the single RISK-03 counsel engagement (Issue #5, `NP-REG-PBM1064-001` Rev 1 §8) — do not open a parallel engagement |
| **4** | **Cervical tcVNS accessory** (#24) | Cervical vagus trunk activation is higher than the auricular branch and is **not available on T1 at all** (T1 is auricular-only, deliberately: no carotid proximity) | **T2 only** — narrowest reach in C1 (§3.2's boundary note: this is the addressable population, not the gate) | **INTERNAL** — separate 510(k) required (predicate: electroCore gammaCore K163334 / K173323); T2 programme #73. Safety MCU owns the cardiac interlock (CLAUDE.md §4.2) |

### C2 — Binding relief · raises the term that currently binds

| # | Item | Class evidence | Reach | Gate |
|---|---|---|---|---|
| **5** | **TEC base-station chiller** | Arrives with a mains base station, so it raises **both** terms of `min(electrical, thermal, dose)`; `maxConcurrent` moves and cascades shorten. Cascading is the only real thermal-injury exposure in the document set (`NP-PWRSRC-001` §5.5, 292 CEM43 on Vascular Baseline), so the clinic argument is **dose, not time** | Clinic / high-concurrency sessions, both tiers. Mode 3 autonomy **not** possible | **INTERNAL** — anti-fog is the binding constraint, not cooling: the scalp gap's dew point is ~31 °C, the module face must stay in a ~32–42 °C band, and a thermostatic tempering valve is mandatory. `OI-THCOOL-11` tests whether the dual-PD fouling discriminator already detects condensation onset. Mains siting per `NP-PWRSRC-001` §8; planner must **detect** cooling state, not be told it (`SR-FAN-06`) |

> **C2 has exactly one member, and that is informative.** Every other candidate either changes what
> the platform can deliver (C1) or changes nothing about the binding constraint (C3–C5). If a second
> C2 item ever appears, §6's **second** trigger — the binding term changed — has probably already fired, and §5 is due a re-run rather than an insertion.

### C3a — Continuity, blocking · a named mechanism refuses the session

| # | Item | Blocking mechanism | Reach | Gate |
|---|---|---|---|---|
| **6** | **Electrode hydrogel tips** | Impedance interlock: `NP-FMEA-GEOM-001` FMEA-G02-01 cuts off above 5× baseline; `NP-FMEA-001` FMEA-M06 checks pre-session and at 1 Hz mid-session. App impedance-trend prompt (measurement-triggered, CLAUDE.md §5.2) | **Every configuration** — Core ($449) is EEG-only and still needs them. Widest reach of any item in §5 | Shipping |
| **7** | **VNS clip pads** | Safety MCU holds if contacts are not impedance-confirmed (CLAUDE.md §4.2). Electrochemical degradation from VNS current is the wear mechanism | Home Lite and up (VNS+HRV clip) | Shipping |
| **8** | **Intranasal hygiene sleeves** | **The only authenticated consumable** (CLAUDE.md §2.3) — no authenticated sleeve, no intranasal session | Home Standard and up (intranasal PBM is modality ②) | Shipping. *Its status as primary MRR driver is not a ranking input (§3.1 Test 4)* |
| **9** | **Cervical tcVNS gel pads** (5-pack) | Same interlock chain as the accessory it serves: impedance confirmation plus the cardiac-rhythm interlock | T2 only | T2 programme (#73). **`OI-ACC-03` CLOSED 2026-09-22** — row added to `docs/reference/commercial-model.md` §2.3. Replaced on failure: the impedance check in this row's mechanism column **is** its trigger (a condition measurement that refuses the session), which confirms C3a. Reuse permitted by decision. Price not set (`OI-COST-10`). The app now names the failing pad's side and lights it on a neck diagram; the hub side of that path is `OI-ACC-07` |

### C3b — Continuity, degrading · no mechanism refuses the session

| # | Item | What absence costs | Reach | Gate |
|---|---|---|---|---|
| **10** | **Interface protection covers** (complete kit) | Ingress protection across all interfaces. All tethered — loss prevention by design. Ranked above the port covers on coverage, not on kind; the two are near-peers | Every configuration | Shipping |
| **11** | **Accessory port covers** (3-pack) | Port ingress. Magnetic, tethered, 3 installed + 2 spare | Every configuration | Shipping — but the anchor posts conflict between `NP-TOOL-HUB-001` F-02 and the shell tooling item (**`OI-ART-07`**, `docs/status/pending-decisions.md` §13.4) |
| **12** | **Boa regrease kit** ($4.99) | Fit-system service life — 50,000-cycle-rated dial, PTFE-lined channel. Ranked below the covers on Test 3: an unprotected port is an irreversible ingress event, a dry dial is wear | Every configuration | Shipping |
| **13** | **Audio cup mesh frame** (pair) | Driver fouling; app driver-impedance flags it (measurement-triggered). Snap-in, user-replaceable | Home Standard and up (audio is modality ⑦) | Shipping |
| **14** | **Audio cup foam** (set) | Seal and comfort | Home Standard and up | Shipping — **`OI-ACC-02` CLOSED 2026-09-15** (principal; disposition in `docs/reference/commercial-model.md` §2.3 and `docs/status/completed-decisions.md`). Neither side was wrong about the *product*: the shipped code already triggers on a session count, and the calendar lived only in the table's prose. **Two claims made in this row were wrong** and are corrected there: the foam is **not** "the one calendar-triggered consumable reminder in the document set" — the mesh frame, the interface covers and the S3 Rx insert are all calendar-denominated in the Interval column, and the mesh frame's claimed measurement is unimplemented (`OI-ACC-05`) |

### C4 — Envelope · same therapy, wider conditions of use

| # | Item | Class evidence | Reach | Gate |
|---|---|---|---|---|
| **15** | **Hip ice / PCM pack** | **Ambient envelope only** — the device runs in a hot room instead of derating or blocking, and on a 45 W brick it does **not** shorten sessions, because electrical binds first. Preserves **Mode 3 autonomy**, which the chiller cannot (USB-C, ~1–2 W pump; runs off a power bank). Capacity is a depleting budget: 182 g ice per 30 min at 6 tiles | Every configuration | **INTERNAL** — same anti-fog band and tempering valve as rank 5; `OI-THCOOL-11`. **Binding marketing constraint (D-3):** the session-time claim belongs to the base station alone — copy, packaging, store page and in-app upsell alike |
| **16=** | **EC lens** (+$89 upgrade / $129 standalone) | Bistable 5–75 % VLT, 2 s transition, clears to 75 % on power restore. Widens the ambient light conditions in which a visual or Mode F wear session is tolerable. Secondary: the EC driver monitors transition time as a **contact-resistance proxy**, detecting rim corrosion. **Contestable at C5** — it is sold as a premium comfort upgrade; C4 is taken on the conditions-of-use test, and the C5 reading is recorded so it can be moved without re-running §5 | Home Standard and up, as an upgrade | Shipping |
| **16=** | **S2 polarising shade** (~12 % VLT) | Same envelope argument, fixed rather than variable. **Peer of the EC lens under §3.4, not below it:** S2 is *standard lens only*, so the two are mutually exclusive and serve disjoint populations. Neither is a rung of the other's ladder | Home Standard and up, standard lens only | Shipping |

### C5 — Experience · no change to therapy, dose or envelope

| # | Item | Class evidence | Reach | Gate |
|---|---|---|---|---|
| **18** | **Apple Watch sync app** (#32) | **Class set by the product's own regulatory declaration**, not by judgement: `NP-APP-ROADMAP-001` §6 and `docs/reference/accessories-roadmap.md` declare every Watch-delivered function to be session monitoring / user-interface aid, **not therapeutic delivery** — therapeutic claims attach to NeurOne hardware only. That declaration forecloses C1–C4. Highest reach in its class and $0 hardware BOM, but Test 1 is the primary key | Every iPhone + Watch user, all configurations | **INTERNAL** — downstream of the core iOS app (`NP-APP-ROADMAP-001` §3). Four phases; **Phase 3 rescoped** to a low-rate rhythmic cue, honestly labelled, because watchOS cannot render 40 Hz; **Phase 4 blocked on `OI-WA-02`** (screen brightness ≥100 nits at 40 Hz) |
| **19** | **Fit-over goggle assembly** (deferred) | **C5, and capability-*negative* against the committed path.** `NP-HW-FITOVER-001` Rev 1. Not C1: access to the visual modality is **already delivered** to the corrective-eyewear cohort by rank 2, so a second route unlocks nothing. Not C2–C4: dose, protocol, concurrency and envelope unchanged — and §5.2 of that document recommends **withdrawing Mode F** on such an assembly, because NIR transmission through an undisclosed, uncharacterised spectacle lens is a metered *emission*, not a metered dose. What it offers is that the user need not buy an Rx insert. Below rank 18 on **reach**; also the highest-engineering-cost item in this table on Test 4 (a second assembly, a second tooling family, and a new visual safety case) | Corrective-eyewear users who decline the S3 path — narrowest in the set | **EXTERNAL — demand.** Nothing starts without specific recorded user requests (`NP-HW-FITOVER-001` §2.2). **Forecloses nothing by waiting** (§3.1 Test 3): it is a separate assembly, so no first-cut provision keeps it possible |

---

## 6. When this set is re-run

Extending the list is §3.5 and touches one class. **Re-running §5 as a whole** is warranted on
exactly four triggers, and on nothing else — in particular, not on a calendar:

| Trigger | Why it re-ranks | Watch |
|---|---|---|
| **An item's class changes** | Class is the primary key, so a class change moves an item across bands, not within one | **Fired once and was absorbed, not re-run (Rev 2):** `OI-ACC-01` closed C1-confirming, so rank 2 did not move and no other row did either. A class change that *confirms* the held class costs nothing; one that flips it is the expensive case |
| **The binding term changes** | C2 is a claim about what binds *now* (§3.1). If the electrical term stops binding first, cooling-only items become C2 and rank 15 moves to rank 5's band | `NP-THERM-COOL-001` §6.7.1; `OI-PWR-11`; `OI-PWRSRC-22` |
| **A new capability class is needed** | An item that fits no class in §3.1 means the class set is incomplete, and the ordering argument in §4's last paragraph needs re-testing | — |
| **A locked configuration or tier definition moves** | Test 2 counts over configurations (CLAUDE.md §2.1) and tiers (§1). If those move, reach moves | `OI-COST-08` (the T1/T2 ladders collide) |

**Not triggers:** a gate lifting or slipping (§3.3); a price being set (§3.1 Test 4); term **U**
landing; a new BOM estimate.

---

## 7. Consequences that do not wait on the ranking

**One, and it is the reason §3.1 has a Test 3.**

The mastoid pad's hardware provision — the anchor boss in the temporal stability wing — is
**free at first tooling cut and expensive afterwards**, and `docs/reference/accessories-roadmap.md`
already records it as a zero-incremental-cost provision. Its window is the shell tooling first cut.
Its gate, #30 / HOPE Phase 3, is **EXTERNAL** and will not lift on that schedule.

**Provision it regardless of where HOPE lands.** The provision is not the accessory: if HOPE reads
out negative the boss is an unused feature in a moulding, and if it reads out positive without the
boss the pad needs new shell tooling. The asymmetry is total, and it is independent of the pad being
ranked 1st, 10th or excluded — which is exactly why it is recorded here rather than inferred from
rank 1.

**This is the document's only tooling-relevant output**, and it changes no tooling specification; it
is an argument for a provision already described.

---

## 8. Open items raised

`OI-ACC-01…03` were found by building §5 — each is a place where the document set could not answer a
question the ranking had to ask. None is tooling-blocking; `OI-ACC-01` is tooling-*relevant*.

| ID | Item | Owner |
|---|---|---|
| **OI-ACC-01** | ~~Can the goggle assembly be worn over corrective eyewear?~~ **CLOSED 2026-09-14 (principal).** **No, and by design.** Corrective-eyewear users buy prescription lenses *for* the goggle assembly — the S3 Rx path, already supported by the Tier A optician network. A goggle sized to be worn over the user's own glasses is specified as a deferred concept in `NP-HW-FITOVER-001` and held at rank 19 behind a demand gate. **Effect on §5:** rank 2's C1 class is confirmed and is now derived rather than judged. **Effect beyond §5:** the decision is load-bearing for safety, not only for sizing — `NP-HW-FITOVER-001` §5.1 finds that `NP-HFE-001` CT-03's *"safety is not shade-dependent"* argument does **not** transfer to a user-supplied lens, because shades snap on outboard of the lens while the 940 nm IR eye-open sensor looks inboard, so a spectacle lens would sit inside the interlock path and could read as a false eye-open — the permissive direction on a Class C interlock | **CLOSED** — was ME + Product |
| **OI-ACC-02** | **CLOSED 2026-09-15 (principal) — neither offered outcome; the framing had a false premise, and this row's refusal to invent a trigger was upheld rather than overturned (none was invented).** As raised: the foam's prompt is recorded "Calendar reminder" in `docs/reference/commercial-model.md` §2.3 while CLAUDE.md §2.3 forbids that, so either the foam needs a seal or compression proxy (design work) or the invariant needs an exception. **Reading the code found a third state already shipped:** `ConsumableInventory.swift` / `ConsumableModels.kt` trigger the foam on a **session count** from the hub's SHDR-class `CONSUMABLE_STATUS` — a measurement the device takes. The calendar existed only in this table's prose and could never have been anything else, the headset having no `VBAT` rail (`NP-FW-EMMC-002` §H.3.1). **The real defect was the threshold's provenance:** `150` was back-derived from "6–12 months" via an unvalidated daily-use assumption and did not match its own comment (`~180`) — the `NP-FW-EMMC-002` §G.2 class, taking that document's remedy (relabelled an unvalidated placeholder; value deliberately unchanged; carried by `OI-ACC-04`). **No seal measurement is obtainable** from the sensor set of `NP-FW-BENCH-001` §4.1 + `np_sw02_platform_hal.h` as of Rev 48: the foam is non-conductive and in no circuit, there is no microphone anywhere in the design, and the one audio impedance channel is an unimplemented stub measuring a conductive **mesh**. CLAUDE.md §2.3 gained a definition rather than an exception (Rev 48): condition measurement, or exposure count with its mechanism named. **Two claims in this document were falsified in the process** — see §5 row 14 and §8's sibling rows | Product + FW |
| **OI-ACC-03** | ~~The cervical tcVNS gel pad (5-pack) is a consumable with no consumables entry.~~ **CLOSED 2026-09-22 (principal).** `docs/reference/commercial-model.md` §2.3 now carries the row. *"Single-use"* (`NP-REG-CVNS-001` §2.4) is a **performance guarantee, not a reuse prohibition**: the pad is replaced when it fails, and single use is not enforced because failure is detected — per-electrode impedance ≤ 5.0 kΩ at 1 kHz before every session, enable refused by the safety MCU otherwise (`NP-HW-CVNS-001` `REQ-CVNS-06`, CLAUDE.md §4.2), a condition measurement under CLAUDE.md §2.3. **Price and GM% not set** (`OI-COST-10`; no COGS). **One premise in the `OI-ACC-02` record is corrected, not inherited:** its *"20–40 sessions for the pads"* is the **auricular** clip's figure; the cervical pad has no interval to count. **Successor: `OI-ACC-07`** | **CLOSED** — was Product |
| **OI-ACC-07** | **The cervical gel pad's trigger refuses the session but tells the user nothing.** Raised 2026-09-22 closing `OI-ACC-03`. **App half done 2026-09-22 (principal direction: the message must say which pad location has failed, ideally on a graphic).** iOS and Android parse a new optional characteristic, `CVNS_PAD_STATUS` (`4E455550-0013-…`, NOTIFY 4 B: failed-electrode mask, which check, and the **side of the neck** of each electrode's pad), and show an alert that names the side — *"a gel pad on the left side of your neck failed its contact check"* — above a neck diagram that lights that side (`CervicalPadAlertView.swift`, `SessionScreen.kt`; ten `CVNS_PAD_*` keys in all eleven locales). **The side comes from the hub, never from the electrode number:** the hub knows the session's montage (`electrode_config`: bilateral = one pad each side, unilateral = both on one side) and its own wiring. **There is no helmet graphic to light:** the pads sit on the neck module, not in a helmet socket, and neither app draws the helmet (sockets are a text list) — so the graphic is of the neck, drawn as the wearer sees themself in a mirror and labelled Left/Right in words. **Remaining (FW + EE):** (1) the hub publishes `CVNS_PAD_STATUS` — no GATT server exists in firmware yet (`OI-WA-03`), and `NP_CVNS_FAULT_IMPEDANCE` is defined in `np_cvns_types.h` and raised nowhere; (2) the bilateral electrode-to-side mapping the hub reports depends on the cable conductor assignment (`NP-HW-CVNS-001` `OI-CVNSHW-01`); (3) in a unilateral montage both pads share one side and nothing yet distinguishes them (no neck-module geometry, `OI-CVNSHW-06`), so a single failure there is named by side only — the message reads *"a gel pad on the left side"*, which stays true; (4) Mode 3 has no phone, so no message path — the device speaks in tones and LEDs (CLAUDE.md §17); (5) the matching IFU line (`NP-REG-CVNS-001` §7). | FW + EE (app half done) |

---

## 9. What this document asks for

**A sign-off on §3, not on §5.** §5 is an output — re-derivable by anyone with §3 and the document
set, and correctable one row at a time. §3 is the decision: it fixes what "value delivered" means,
that the tests are lexicographic rather than weighted, that no cost figure enters, and that
dependencies are recorded rather than ranked.

Sign off §3 and §5 maintains itself. Sign off only §5 and the next accessory re-opens the list —
which is the state Issue #344 was raised to end.

**One placement remains flagged** for the principal as judgement rather than derivation, and it is
the place to look first: **rank 16=**, the EC lens, C4 versus C5.

Rev 1 flagged two. The other — **rank 2**, the S3 Rx system — was resolved on 2026-09-14 by the
decision recorded in `NP-HW-FITOVER-001` §1, and resolved *in favour of the class the criterion was
already holding*. That is the outcome worth noting when weighing §3: the flagged judgement was
carrying the right answer, and closing the question changed nothing in §5 except the grounds it
stands on.
