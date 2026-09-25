# Commercial model — retail ladder, charger policy, consumables, clinician tiers

> Relocated from CLAUDE.md §2.1a, §2.2, §2.3 and §6.1 (Rev 40), from §2.1 — the BOM / COGS / GM%
> columns and the Home Standard box contents — (Rev 43), and **(Rev 47) the remainder of §2: the
> configuration table's Tier and Retail columns, the charger policy's mechanics, and the consumable
> roster.** CLAUDE.md §2 now keeps no §2 figure at all — only the pricing guardrail, the two charger
> rules, the two consumable rules, §2.1a's finding, and a one-line list of the six configuration
> names by tier. **This file is therefore the only place any §2 number can be read.**
> Content is verbatim; section numbers are unchanged so inbound `CLAUDE.md §2.2`-style citations
> still name the right block.
>
> **Read this file when:** quoting or setting a price, sizing a charger, working on consumables or
> subscription revenue, or answering any margin question. The cost *derivation* behind §2.1a is
> `docs/np_cost_001.md` (NP-COST-001) §8 — that document, not this one, is the authority on how the
> figures were produced and on what they exclude (term **U**).
>
> **Binding constraint carried over from §2.1a:** no price may be set before `OI-HEXTILE-06` is
> decided (`OI-COST-10`).

### 2.1 Configuration cost table — BOM / COGS / GM% and the full caveats

**The whole table lives here, not in CLAUDE.md §2.1 (cost columns Rev 43; Tier, Retail and
Modalities Rev 47).** The core keeps only the six configuration *names* grouped by tier — the
mapping its §1 regulatory split and 231 inbound name references resolve against — plus the invariant
that every T1 row is gross-margin negative and every figure is a floor. It keeps no figure, because by
the rule stated in the caveat below **no figure here may be quoted, cited or acted on without first
reading `docs/np_cost_001.md`**: a number visible in the always-loaded core could never be *used*
from there, only misremembered, and the cost model is the most-revised content in the document set
(Rev 38 replaced every figure; `OI-HEXTILE-06` will move them again).

> **⚠ RETAIL IS UNLOCKED (principal, 2026-08-16). BOM / COGS / GM% are re-derived against the
> hex-tile architecture and are FLOORS, not estimates.** The pre-hex figures (Core $168–169 /
> $258–260 / 42% … Pro Full $1,506 / $2,628 / 81%) were built on the retired five-zone-module design
> and are superseded. **Every T1 configuration is gross-margin negative at the prices below.** Read
> `docs/np_cost_001.md` before quoting, citing or acting on any number here — it carries the
> derivation, the three assumptions that are not sourced anywhere in the document set, and the
> uncosted term **U** that every row excludes.
>
> **Retail prices in the table are the prices currently in force, not a decision.** Unlocking the
> constraint does not set a price — see §2.1a for the ladder it implies, and note **`OI-HEXTILE-06`
> must be decided before any price is set** (`OI-COST-10`).

**Three things about this table that are not optional to know:**

1. **GM% above is an output, not a target.** It was derived under the lock, which is what made it
   evidence rather than an assumption. No figure was adjusted to preserve the old 36–81% band.
2. **Every row is a floor.** Each excludes term **U** — the emitter-count delta (Home Standard goes
   from 600 emitters to ~2,286) net of the retired hub-side LED drive stage. **U is uncosted in both
   directions and is very likely large and positive:** the 660/808 nm emitters are *not selected*
   (`OI-HEXTILE-02`), so the dominant BOM line has no unit price anywhere in the document set.
   **OI-HUB-C08 therefore cannot be closed**, and the gap is wider than OI-HUB-C08 states — it
   scopes only the *drive electronics*, not the emitters.
3. **The dominant recoverable term is the InGaAs photodiode pair**, ~$10 of the $11.53/tile. That is
   `OI-HEXTILE-06`, and it is the decision that determines whether T1 can close against locked
   retail at all. `docs/np_cost_001.md` §6 runs its three options: **none of them, alone or
   combined, restores a positive T1 margin.**

**Cost table (floors; cost columns relocated verbatim from CLAUDE.md §2.1 at Rev 43; Tier column
added at Rev 47, when the core stopped holding a configuration table of its own).** The core retains
none of these columns — this table is the only copy.

| Config | Tier | BOM (floor) | COGS (floor) | Retail (in force, 🔓 unlocked) | GM% (floor) | Modalities included |
|--------|------|-----|------|--------|-----|---------------------|
| Core — EEG only | T1 | $360–423 | $554–650 | $449 | **−23% to −45%** | 4-ch EEG · all connectivity · EMF shielding · processor stack · 8GB eMMC |
| Home Lite | T1 | $642–705 | $896–984 | $599 | **−50% to −64%** | Core + PBM tiles (660+810nm) · 8-ch EEG · VNS+HRV clip |
| Home Standard ★ (flagship) | T1 | $897–959 | $1,196–1,278 | $849 | **−41% to −51%** | All T1 modalities (see §3) |
| Home Premium | T1 | $952–1,014 | $1,287–1,371 | $1,199 | **−7% to −14%** | All T1 + EC lens (+$89 value) · 2yr warranty · priority support |
| Pro Entry | T2 | $1,463–1,525 | $2,398–2,500 | $4,999 | **+50% to +52%** | All T1 + 21-ch qEEG · 1170nm deep PBM · clinical tACS · HIPAA cloud · sLORETA |
| Pro Full | T2 | $2,136–2,198 | $3,728–3,836 | $13,999 | **+73%** | All T2 + TMS hub · multi-patient dashboard · scripting API · FHIR R4 · $1,800/yr service |

**★ Home Standard box contents:** All T1 modules · hard clamshell case · braided aramid USB-C cable (spare in box) · **45W NeurOne branded GaN charger** · S1 opaque shade · interface covers (installed + spare set each type) · mesh cleaning brush · Boa replacement cable + hook tool · moisture-barrier electrode tip hydration caps · humidity indicator card · pre-impregnated cleaning cloth packets

### 2.1a Implied retail ladder (retail unlocked 2026-08-16 — implied, NOT set)

Retail = COGS ÷ (1 − original GM target). **These are the prices the current costs imply, not prices
that have been decided.** Every figure inherits the §2.1 floor status — term **U** is still excluded,
so this is the *least* retail would have to move. Full derivation and consequences: `NP-COST-001` §8.

| Config | **Break-even** | **At original GM target** | Multiple of price in force | Per-unit result at price in force |
|--------|---|---|---|---|
| Core — EEG only | $554–650 | **$955–1,121** (42%) | 2.13–2.50× | **−$105 to −$201** |
| Home Lite | $896–984 | **$1,445–1,587** (38%) | 2.41–2.65× | **−$297 to −$385** |
| Home Standard ★ | $1,196–1,278 | **$1,869–1,997** (36%) | 2.20–2.35× | **−$347 to −$429** |
| Home Premium | $1,287–1,371 | **$2,475–2,637** (48%) | 2.06–2.20× | **−$88 to −$172** |
| Pro Entry | $2,398–2,500 | $8,881–9,259 (73%) | 1.78–1.85× | *+$2,499 — profitable today* |
| Pro Full | $3,728–3,836 | $19,621–20,189 (81%) | 1.40–1.44× | *+$10,163 — profitable today* |

**Four things to weigh before setting any price:**

1. **Break-even binds before margin does.** Home Standard cannot be sold below **~$1,196** at any
   margin — already 1.4× its current $849.
2. **The T1 ladder collides with the T2 ladder.** Home Premium at $2,475–2,637 against a Pro Entry
   at $4,999 makes §1's two-tier structure hard to sustain — one tier with a regulatory footnote.
   **`OI-COST-08`.**
3. **Pro's rows are where the *target*, not the cost, should be questioned.** Both are profitable
   today; holding 73%/81% is what demands $9K and $20K. This is not a mandate to raise T2 pricing.
4. **Decide `OI-HEXTILE-06` first (`OI-COST-10`).** Silicon PD + a 20-tile build moves Home
   Standard's target-margin retail **$1,997 → ~$1,383**. Pricing before that decision prices against
   a cost it invalidates.

**★ Home Standard box contents:** All T1 modules · hard clamshell case · braided aramid USB-C cable (spare in box) · **45W NeurOne branded GaN charger** · S1 opaque shade · interface covers (installed + spare set each type) · mesh cleaning brush · Boa replacement cable + hook tool · moisture-barrier electrode tip hydration caps · humidity indicator card · pre-impregnated cleaning cloth packets

### 2.2 Charger policy (locked)

**Two rules the core also keeps, because they constrain work that never opens this file** (→
CLAUDE.md §2.2): the ladder is **keyed to peak draw, not price**, so the 2026-08-16 retail unlock
does not touch it — the two peak-draw figures it is keyed to are CLAUDE.md §4.5, not this table —
and the **EU note** below is a hard app-behaviour rule, not a marketing preference.

Charger scaled to peak draw of configuration. Auto-included at every upgrade by serial number tracking. Upfront 65W upgrade option ($19 at-cost) offered at checkout as intent signal.

| Config | Charger included | BOM |
|--------|-----------------|-----|
| Core | 15W USB-C (unbranded) | $3–4 |
| Home Lite | 30W GaN (unbranded) | $5–6 |
| Home Standard ★ | 45W NeurOne GaN (branded) | $10 |
| Home Premium | 45W NeurOne GaN (branded) | $10 |
| Pro Entry | 65W NeurOne GaN (branded) | $13 |
| Pro Full | 65W NeurOne GaN (branded) × 2 | $26 |

> **Note 2026-09-23 (`OI-TACSDRV-06`) — scope of "upgrade" above, not a change to the policy.** The
> auto-include rule serves **configuration upgrades** (`NP-PWRSRC-001` §14.2: *"Fitting a T1-C module
> is a serial event"*). It does not assume that a T1 unit is converted into a T2 unit. `NP-REG-UPG-001`
> recommends, but does not decide, that T2 is always a new unit, which ships with its own
> configuration's charger under this same rule. The policy text and table are unchanged.
>
> **DECIDED 2026-09-23 (`NP-REG-UPG-001` Rev 2 §7.0): T2 is always a new unit, and a T1 owner's modules
> carry over to it.** The charger rule and table above are unchanged. A T2 ships its own configuration's
> charger, and the T1 keeps its own. **Three commercial consequences, none of them a price:**
> (1) **No rolling upgrade.** A T1 unit refuses every T2 modality and T2 software feature
> (`REQ-UPG-01`), so a T2-D tile, the cervical VNS accessory or the qEEG cap sold to a T1-only owner
> does nothing on that unit. Whether such parts are sold to T1-only owners at all is Product's call.
> (2) **§2.1 has no T2 offering for a buyer who already owns modules.** Every configuration row ships
> its own box contents. Defining that offering is Product's (`OI-UPG-04`). The differential the
> decision protects is the **unit's**: base hardware plus T2 content. Carried modules lower the
> owner's cost of the move by exactly what they already paid. (3) **No price is set before
> `OI-HEXTILE-06`** (`OI-COST-10`), and that includes any trade-in value or T2-without-modules offering.
> The T1 and T2 ladder collision (`OI-COST-08`) is unchanged by this decision.

**Charger upgrade intent signals:**
- Core buyer selects 30W upfront → PBM intent → 14-day follow-up
- Core buyer selects 45W upfront → Full T1 intent → 7-day completion bundle offer
- Any buyer selects 65W upfront → T2 intent → human clinical sales call within 48 hours

**EU note:** Chargers are branded recommendations, not proprietary requirements. Any PD-compliant charger must work. App displays "power level: reduced" informatively, never blocks.

### 2.3 Consumables + recurring revenue

**Two rules the core also keeps** (→ CLAUDE.md §2.3): intranasal hygiene sleeves are the **only
authenticated consumable** and the primary MRR driver; and **every consumable prompt is
measurement-triggered (CLAUDE.md §5.2), never calendar-triggered** — a consumable with no
measurement gets no prompt, and inventing a trigger for one is design work, not a documentation
edit.

**What counts as a measurement** (CLAUDE.md Rev 48, `OI-ACC-02`; this full statement was moved here
from the core in Rev 56). A calendar prompt is not only forbidden here, it is **unimplementable on
this device**. There is no battery, coin cell or `VBAT` rail (CLAUDE.md §4.5), so the RT1062's SNVS
RTC has no backup domain and wall time is lost on every disconnect. A replacement prompt therefore
qualifies in exactly one of two ways, and the row must say which:

1. **Condition measurement**: a sensed quantity of the part itself (hydrogel tips: impedance trend).
2. **Exposure count**: a count the device already takes, of the quantity that drives the part's
   degradation, **with that mechanism named** (VNS clip pads: electrochemical degradation from VNS
   current). An exposure count is weaker than a condition measurement, and the difference matters.
   It cannot see a part that failed early, was damaged, or degraded off the device. A row taking
   this route says what its count cannot see.

**A threshold back-derived from a calendar interval is a calendar prompt wearing a session count**,
and is the thing this invariant forbids. The trigger *kind* and the threshold's *provenance* are two
claims, and satisfying the first does not satisfy the second. A threshold that no measurement
supports is an unvalidated placeholder and is labelled one, per `NP-FW-EMMC-002` §G.2.

**Scope: consumable replacement prompts only.** Service-network and calibration intervals are
deliberately outside it and stay calendar-denominated. The 3–5 year Tier B fluxgate visit
(`docs/reference/durability-maintenance.md`; scale-factor drift is not self-detectable) and the
$1,800/yr T2 calibration visit (`NP-PWRSRC-001` D-16, the ISO 14971 re-acknowledgement point) have
no measurement to substitute. Reading this rule onto them would delete two controls.

> **⚠ `OI-ACC-02` RESOLVED (principal, 2026-09-15; CLAUDE.md Rev 48) — the foam's trigger was never
> a calendar, and CLAUDE.md §2.3 needed no exception.**
>
> **This block replaces the unresolved flag Rev 47 (#360) left here.** That revision carried §2.3's
> invariant into this file and set the row beside it, which is what put the rule and the row in one
> reader's view at once; it recorded the conflict as surfaced, not fixed, because choosing between a
> new measurement trigger and a reasoned exception is a principal decision. This is that decision.
>
> The item offered two outcomes: build the foam a seal or
> compression proxy, or write a reasoned exception into the invariant. **Neither was taken, because
> the item's framing rested on this table rather than on the code.** The shipped app already triggers
> the foam on a **session count** read from the hub's SHDR-class `CONSUMABLE_STATUS` characteristic
> (`ConsumableInventory.swift`, `ConsumableModels.kt`) — a measurement the device takes. The calendar
> existed only in this row's prose, and it could never have been anything else: the headset has no
> battery, coin cell or `VBAT` rail, so wall time is lost on every disconnect and a device-side
> calendar trigger is **unimplementable**, not merely forbidden (`NP-FW-EMMC-002` §H.3.1).
>
> **What was actually defective was the threshold's provenance.** `150` was back-derived from
> "6–12 months" through an unvalidated *assuming daily use* assumption, and did not match its own
> stated derivation (`~180`) on either platform. That is the `NP-FW-EMMC-002` §G.2 defect class —
> `NP_ACCEL_DROP_THRESHOLD_G` and `NP_ACCEL_MAINT_THRESHOLD`, *"chosen before any hardware existed
> and … never compared against a device that failed"* — and it takes that document's remedy: the
> number is **relabelled an unvalidated placeholder** and carried by `OI-ACC-04`. **The value is
> deliberately unchanged.** Re-deriving it (the file's own midpoint convention gives 270 from the
> calendar-implied 180–360) would re-derive from a derivation just declared void, and would move
> both platforms' test suites and the prompt frequency of a revenue-generating consumable on
> reasoning that has been withdrawn. Overriding to any other number costs one edit.
>
> **No seal or compression measurement is obtainable from the current sensor set**, and that is a
> physical finding rather than a budget one: the foam is non-conductive and sits in no circuit, there
> is **no microphone anywhere in the design** (so no acoustic seal test exists), and the one audio
> impedance channel — `np_mod_audio_hal_mesh_impedance()`, itself an unimplemented HAL stub
> (`OI-AUDIO-08`) whose return value is discarded at its only call site — measures a **conductive
> mesh**, not the foam beside it. This null is bounded by what was searched: the four candidate
> channels enumerated in `NP-FW-BENCH-001` §4.1 plus `np_sw02_platform_hal.h` as of this revision.
> A part added to either list reopens the question.
>
> **Reopened 2026-09-24:** `OI-AUDIOHW-08` was decided for an in-cup microphone per side, used for
> measurement only (`NP-HW-AUDIO-001` Rev 4 `REQ-AUDIO-14`), so an acoustic seal test becomes
> obtainable once that microphone is specified and implemented (`OI-AUDIOHW-09`, `-10`).
>
> **What the exposure count cannot see:** a foam torn, contaminated, or compression-set in storage or
> by a second user; and any wearer whose sessions are much longer or shorter than whatever session
> length `OI-ACC-04` eventually assumes. The mechanism is time-under-compression, and session count
> is a proxy for it, not a measure of it.
>
> **Two corrections to claims made when the item was raised**, both false before this revision and
> not created by it. (i) `NP-ACC-PRIORITY-001` §8 and `docs/status/pending-decisions.md` state *"every
> other row in that table names a measurement"* — the interface-cover, S3 Rx and T2 service rows name
> none, and no Notes cell anywhere in the table names a session count, because the session counts live
> in the **Interval** column. (ii) §5 row 14 calls the foam *"the one calendar-triggered consumable
> reminder in the document set"* — the mesh frame ("Annual"), the covers ("Annual / as lost") and the
> S3 insert ("12–24 months") are all calendar-denominated in the Interval column, and the mesh frame
> is the worse case, because its claimed measurement is unimplemented (`OI-ACC-05`).
>
> **Read Interval and Notes together.** No row states an admissible trigger inside one cell: Interval
> supplies the exposure count, Notes supplies the mechanism or the condition measurement. Three rows
> below satisfy neither half — covers (loss is not degradation), S3 Rx (its real trigger is refraction
> change, which is user biology and therefore UHDR, permanently outside NeurOne's reach) and the T2
> service contract (not a consumable prompt). They are recorded here rather than silently exempted.
> **This rule is enforced by reading, not by a gate** — unlike §5.1's redaction shape, §6.2's
> reachability and §17's locale rule, which each have one. That gap is `OI-ACC-06`.
>
> *(2026-09-24: `OI-ACC-06` closed — the rule now has a gate. See the next block.)*

> **`OI-ACC-03` CLOSED (2026-09-22, principal) — the cervical VNS gel pad has a row, is replaced
> on a measured failure, and is the only row whose price is deliberately empty.** The pad was a
> specified T2 consumable (`NP-REG-CVNS-001` §2.4) that appeared in no consumables table.
>
> - **"Single use" is a performance guarantee, not a reuse prohibition** (principal, 2026-09-22).
>   The pad is guaranteed to work for one application; it is not required to be discarded after
>   one. **Single use is therefore not enforced**, and needs no authentication, because a failing
>   pad is detected. `NP-REG-CVNS-001` §2.4 (Rev 2) now says so, since *"single-use"* on a medical
>   device reads as *"do not reuse"* by default.
> - **Trigger: a condition measurement, the first of CLAUDE.md §2.3's two admissible kinds.** Every
>   cervical session is preceded by a per-electrode impedance check against **≤ 5.0 kΩ at 1 kHz**
>   (`NP-HW-CVNS-001` `REQ-CVNS-06`, `NP_CVNS_IMPEDANCE_MAX_KOHM`), and the safety MCU withholds
>   enable when it fails (CLAUDE.md §4.2); a pad that lifts mid-session is caught within 500 ms
>   (`REQ-CVNS-08`). The threshold is a safety gate with its own provenance, not one back-derived
>   from a calendar. This is the blocking mechanism `NP-ACC-PRIORITY-001` §5 row 9 already named.
> - **Interval: none — replaced on failure.** Not a session count, and **not** the 20–40 sessions
>   of the auricular clip pads: the `OI-ACC-02` record called this item cheaply closable on the
>   VNS-clip template, `modality-stack.md` *"already recording 20–40 sessions for the pads"*, but
>   that figure belongs to the auricular PDMS pads (modality ⑥). A pad with a condition measurement
>   needs no exposure count and no validated threshold.
> - **A worn pad that still passes cannot overdose.** The per-phase charge ceiling is checked
>   against the declared 2 cm² (`NP_CVNS_ELECTRODE_AREA_MCM2`), and at `REQ-CVNS-04`'s maxima
>   (2 mA × 1000 µs = 2 µC) the commanded density is 1 µC/cm² against 40 µC/cm², so contact would have
>   to shrink below 0.05 cm² before the ceiling is at stake — far past any impedance that passes
>   5.0 kΩ. The residual reuse hazard is **skin**, not dose: the pad's biocompatibility basis must
>   cover repeat application (`NP-HW-CVNS-001` `OI-CVNSHW-07`).
> - **The detection reaches the user by location — app side done, hub side open (`OI-ACC-07`).**
>   A refusal with no words is not a prompt (CLAUDE.md §5.2), and the principal requires the
>   message to say **which pad location** failed. iOS and Android now read `CVNS_PAD_STATUS` and
>   show *"a gel pad on the left side of your neck failed its contact check"*, over a neck diagram
>   that lights that side. The side is reported by the hub, which knows the montage; the app never
>   guesses it from an electrode number. No `ConsumableKind` is needed — the trigger is a
>   per-session refusal, not a count. Still open: the hub does not publish the characteristic
>   yet, its side mapping waits on the cable pin assignment (`OI-CVNSHW-01`), and a Mode 3
>   wearer is sent to the phone by the user doc (`NP-REG-CVNS-001` §7.4).
> - **Price and GM%: not set.** `OI-COST-10` forbids setting any price before `OI-HEXTILE-06`, and
>   the pad has no COGS estimate anywhere. The empty cells are the record, not an omission.
> - **Pack size is a commercial choice, not a requirement** (CLAUDE.md §18). The assembly checks
>   two electrodes every session (`NP_CVNS_ELECTRODE_COUNT`); how many sessions a pack serves now
>   depends on how long pads last, which nothing yet measures.

> **`OI-ACC-06` CLOSED (2026-09-24, GitHub #387) — CLAUDE.md §2.3's trigger rule is enforced by
> `scripts/check-consumable-triggers.ts`, and the table below gained the Trigger column it reads.**
>
> - **The Trigger column is the row's declaration.** It opens with one of three kinds —
>   **Condition measurement**, **Exposure count**, or **No prompt** — followed by `·`-separated
>   fields. A prompting row names its `producer:`. An exposure count adds `threshold:` with a
>   provenance, `mechanism:` and `cannot see:`. The gate reads that cell. It does not read the
>   Notes cell, because the mesh frame showed that a Notes cell can name a measurement no code takes.
> - **What the gate checks against the code.** The producer exists on both platforms: a
>   `ConsumableKind` case plus the `CONSUMABLE_STATUS` characteristic, or a registered
>   characteristic such as `CVNS_PAD_STATUS`. Each `ConsumableKind` is claimed by exactly one row. A
>   stated threshold equals `sessionLimit` on iOS and Android. A session count is never labelled a
>   condition measurement. A prompting row has no calendar interval, and the reminder engines
>   contain no calendar-shaped trigger. It fails in both directions: a kind added in code with no
>   row fails, and so does a row whose producer is not in the code.
> - **Declared absences are printed on every run and fail once they stop being true.** Two hub
>   producers are unpublished: `CONSUMABLE_STATUS` (`OI-ACC-08`, #381) and `CVNS_PAD_STATUS`
>   (`OI-ACC-07`). The gate flips when firmware code references either one. One row is exempted
>   from one rule: the hydrogel tips' mechanism (`OI-ACC-09`). The exemption fails once the row
>   names a mechanism.
> - **Found while declaring the rows, and not fixed here: the hydrogel tip row named a trigger that
>   does not exist.** Its Notes said *"App impedance trend prompts"*, and `NP-DT-001` `DI-USE-07`
>   says the same. Neither platform computes an impedance trend. The shipped prompt is a 45-session
>   count, the midpoint of a 30–60 range whose source nothing states. That is the mesh-frame defect
>   (`OI-ACC-05`) with a count behind it. The row now states what ships, and `OI-ACC-09` carries the
>   fix. The **VNS clip pads'** 30 is labelled an unvalidated placeholder under `OI-VNSCLIP-07` for
>   the same reason. It is the midpoint of a quoted range, not a derived life.
> - **What the gate cannot check:** whether a cited derivation is right, whether a named mechanism
>   is the true one, or whether a producer works. It checks that a trigger is declared in an
>   admissible shape and that its producer exists. Price, GM% and pack size are outside it.
> - **Nothing else in #387 is closed by this.** `OI-ACC-04` and `OI-AUDIOHW-08` wait on the
>   decision about the in-cup microphone (*taken later the same day — `OI-AUDIOHW-08` option B;
>   `OI-ACC-04` now waits on `OI-AUDIOHW-09` and `-10`*). `OI-ACC-05` waits on a measurement. `OI-VNSCLIP-07` needs
>   characterisation. `OI-NASAL-04` needs a Product + EE + Privacy decision on what sleeve
>   authentication asserts. Each is now visible as a declared row or exemption, not as prose.

| Item | Price | Interval | GM% | Trigger | Notes |
|------|-------|----------|-----|---------|-------|
| Intranasal sleeves (30-pack) | $19/pack or $19/mo sub | Single use | 68–79% | **Exposure count** · producer: `ConsumableKind.intranasalSleeves` · threshold: 1 — single use by design (`NP-HW-NASAL-001` §6.1) · mechanism: contamination of a hygiene barrier by use · cannot see: a sleeve refitted after *Mark replaced* — authentication checks the sleeve type, not the unit (`OI-NASAL-04`) | Only authenticated consumable. COGS $4–6. Primary MRR driver. Blocks session start when exceeded (safety-blocking). |
| Electrode hydrogel tips (8-pack) | $12–16 or $9.99/mo sub | 30–60 sessions | 60–72% | **Exposure count** · producer: `ConsumableKind.electrodeHydrogel` · threshold: 45 — unvalidated placeholder, the midpoint of a range with no stated source (`OI-ACC-09`) · mechanism: not named (`OI-ACC-09`) · cannot see: a tip dried, damaged or contaminated off the device, or one that fails early | Bayonet snap, zero training. **Was "App impedance trend prompts"** — no impedance-trend trigger exists on either platform; the shipped prompt is the session count in the Trigger cell. The impedance trend is the target condition measurement (`OI-ACC-09`). |
| VNS clip pads (2-pack) | $8/pack | 20–40 sessions | 65% | **Exposure count** · producer: `ConsumableKind.vnsPads` · threshold: 30 — unvalidated placeholder, the midpoint of a quoted range, not a derived life (`OI-VNSCLIP-07`) · mechanism: electrochemical degradation from VNS current · cannot see: a pad damaged, dried or degraded off the device, or a poor linked-ear EEG reference on a pad that still passes (`OI-VNSCLIP-01`) | Electrochemical degradation from VNS current. |
| Cervical VNS gel pads (5-pack) — T2 | **Not set — `OI-COST-10`** | On failure — performance guaranteed for one use; reuse permitted (`NP-REG-CVNS-001` §2.4) | **— (no price, no COGS)** | **Condition measurement** · producer: `CVNS_PAD_STATUS` · per-electrode impedance ≤ 5.0 kΩ at 1 kHz before every session (`REQ-CVNS-06`) · cannot see: skin reaction to reuse (`OI-CVNSHW-07`) | Enable refused by the safety MCU when the check fails; lift mid-session caught in 500 ms (`REQ-CVNS-08`). App names the failing pad's side and lights it on a neck diagram; hub side open (`OI-ACC-07`). `OI-ACC-03`, closed above. |
| Audio cup foam (set) | $24/set | **150 sessions — unvalidated placeholder (`OI-ACC-04`), pending the seal measurement** | 58% | **Exposure count** · producer: `ConsumableKind.audioCupFoam` · threshold: 150 — unvalidated placeholder (`OI-ACC-04`) · mechanism: compression set under wear (time-under-compression; sessions proxy it) · cannot see: tear, contamination, or storage set; the hub counts sessions that drove audio, and cannot sense the cups, so a session with the cups worn and no audio is not counted | **Principal decision 2026-09-23:** the prompt covers loss of function (loss of seal) only; comfort and hygiene replacement is at the user's discretion by design. Target trigger is a condition measurement: seal loss read by the in-cup microphone, measurement only, no noise cancelling (`OI-AUDIOHW-08` decided 2026-09-24; not yet specified or implemented — `OI-AUDIOHW-09`, `-10`). Was "6–12 months / Calendar reminder" — `OI-ACC-02`, resolved above. |
| Audio cup mesh frame (pair) | $9.99/pair | Annual (purchase cadence — no prompt) | 62% | **No prompt** · the named measurement, driver impedance, is a HAL stub whose value is discarded (`np_mod_audio_hal_mesh_impedance()`, `OI-AUDIO-08`), and there is no `ConsumableKind` case, so nothing prompts; `OI-ACC-05` | Snap-in, user-replaceable. **Was "App driver impedance flags fouling"** — a row that named a measurement no code takes. Target trigger: a condition measurement of fouling, either driver impedance (`REQ-AUDIO-07`) or the in-cup microphone of `OI-AUDIOHW-08`. |
| Interface protection covers (complete kit) | $22.99 or $19.99/yr bundle | Annual / as lost | 70% | **No prompt** · loss is not degradation, so there is nothing to measure; replacement is the user's call | All tethered — loss prevention by design. |
| S3 prescription Rx insert | $49–139 | 12–24 months | Variable | **No prompt** · the real trigger is a change in the wearer's refraction, which is user biology — UHDR, permanently outside NeurOne's reach (CLAUDE.md §5.1) | Optician partner network. Zero marginal marketing cost per renewal. |
| T2 service contract | $1,800/yr | Annual | ~75% | **No prompt** · a service and calibration interval, not a consumable replacement prompt; CLAUDE.md §2.3 keeps it calendar-denominated by scope | Same-day loaner, priority support, annual calibration. |

**Where the counts come from (`OI-ACC-08`, decided 2026-09-25, GitHub #381).** Every *Exposure count* row above reads the hub's `CONSUMABLE_STATUS` (0x0007), produced by `firmware/hub_control/src/np_consumables.c`. **Increment:** at the end of each session, a kind advances by one if the session *drove* its modality — intranasal PBM for the sleeves, EEG for the hydrogel tips, auricular VNS for the clip pads, audio for the cup foam — i.e. the runner accepted a drive command for it; a refused or stop-only command counts for nothing, and an aborted session counts if the modality was driven before the abort. **Epoch:** a count is sessions since replacement; *Mark replaced* writes the kind index to 0x0007, the hub zeroes it and notifies the zero, and a replacement marked with no hub connected is queued in the app and written at the next connect. **Owner:** the hub, persisted in the Config partition, so a count survives an app reinstall, a second phone and a power cycle. A Config record that is present but unreadable is never read as zero — that would silently clear a pending sleeve block.

---

### 6.1 Use case subscription tiers

| Tier | Price | Use cases | UHDR elements | Target clinician |
|------|-------|-----------|---------------|-----------------|
| Monitor | $49/mo/patient | Adherence monitoring, protocol compliance | Session timestamps, duration, protocol parameters | Primary care, wellness, coordinators |
| Assess | $149/mo/patient | All Monitor + EEG review, neurofeedback, efficacy | Adds EEG waveforms, neurofeedback scores, dose logs | Neurologists, psychiatrists |
| Full Clinical | $299/mo/patient | All Assess + HRV, closed-loop events, outcomes | Adds HRV, PPG, adaptation events, outcome logs | TMS clinics, neuromodulation programmes |
| Research | $599/mo/study | IRB-defined custom (NeurOne review required) | IRB-approved minimum, k≥10 anonymization, no IDs | Academic trials, observational studies |

**Key principle:** Clinicians select **use cases** (not data elements). System determines minimum necessary UHDR elements. Users receive plain-language decision support document listing what clinician CAN learn, CANNOT learn, and privacy implications per element.

**Expansion workflow:** Differential consent document → persistent user notification → user approves/denies/asks questions → retroactive access is a separate decision. Retroactive and prospective access presented as separate consent decisions even if made simultaneously.

**Expansion workflow as implemented (2026-09-09, `OI-CONSENT-02`).** The specification above is
unchanged; what follows is how it is built, because two of its clauses turn out to constrain the
data model and not only the screens.

- **A grant is a sequence of access decisions, not a tier.** `ClinicianAccessScope` carries an
  element set, the point it takes effect from, and — separately — whether it also reaches data
  recorded before then. `tier.uhdrElements` is timeless, so a tier alone can only ever express
  *"yes, including everything already recorded"*; the position **"widen it going forward, leave my
  earlier sessions alone"** is unrepresentable without the scope. The tier still moves on
  expansion — it is what the subscription and the price are keyed to — but it is no longer the only
  thing deciding what a clinician can see. A grant with no scopes means the tier's elements from
  the grant date with prior data included, which is what a bare tier has always meant.
- **The differential document refuses three changes** rather than describing them:
  one that adds nothing; one that would *remove* an element (a re-scope, which an expansion
  document would narrate only in terms of what is gained); and any change touching the **Research**
  tier, whose element set is IRB-defined per study descriptor, so its emptiness is an absence of
  information and never a set. Research access changes go through §6.3's per-study path.
- **The two decisions are on separate steps, not one screen.** Approving answers the
  forward-looking question only; the retroactive question is then asked on its own, states that
  either answer is available, and offers both as controls of equal weight. An Approve button with a
  history checkbox beside it is one decision with a modifier on it, which is what
  *"presented as separate consent decisions"* rules out.
- **Asking a question leaves the request pending.** §6.1's third response is not a decision, so it
  must not clear the notification.
- **A "from today onwards only" answer stays visible on the grant afterwards**, or the user cannot
  tell later which of the two answers they gave.

**Initial-grant workflow as implemented (2026-09-13, `OI-CONSENT-04` + `OI-CONSENT-06`).** The
same two clauses constrain the *first* grant, and until now neither reached it.

- **The use-case selection decides the grant.** The form collected one and built a grant from the
  tier, so the key principle above was a question the user answered and nothing consumed. A grant
  now carries the use-case IDs *and* the element set derived from them, clamped to the tier — which
  stays the ceiling and the thing the price is keyed to. Most grants are therefore **narrower than
  their tier**, which is what *minimum necessary* meant all along.
- **The elements are frozen at grant time, not re-derived from the IDs on read.** Both are stored
  because they answer different questions: the IDs are what the user consented to in their own
  terms, and the scope is what that came to on the day. Re-deriving would mean a later edit to a
  library entry silently widens every grant already made, retroactively, with nobody asked — the
  same failure the timeless tier had, arriving by a different route.
- **The library holds keys, not text**, because it is one table read by both platforms and
  Android's `:core` cannot reference `R.string` at all (CLAUDE.md §17).
- **The retroactive question is asked on the first grant too**, on its own step, with both answers
  as controls of equal weight and neither preselected. Defaulting it to *prospective only* was the
  cheaper and more conservative option and was rejected for that reason: §6.1 does not ask for
  retroactive access to be narrow, it asks for the two decisions to be **presented separately**,
  and a default presents neither.

One half remains absent and tracked rather than implied: the clinician-portal channel is specified
(`NP-SW-PORTAL-API-001`) and represented in code by a port that refuses by default
(`ClinicianPortalChannel`), but the clinician-identity anchor it needs does not exist, and §6.1's
third response — *asks questions* — has nowhere to send a question that NeurOne is allowed to carry
(the text is the user's own, hence UHDR, hence end-to-end to the clinician or nothing).

