# Configuration Cost Model — Hex-Tile Re-derivation

**Project:** NeurOne
**Document:** NP-COST-001
**Revision:** 2
**Date:** 2026-08-16
**Status:** ACTIVE
**Effective Date:** 2026-08-16
**Author:** NeurOne Systems Engineering
**Approved By:** — (new document)
**References:** CLAUDE.md §2.1, §2.2, §3, §4.5; `NP-DRV-SHELL-002` Rev 4 §10.1, §10.2; `NP-HW-HEXTILE-001` Rev 6 §4.2, §4.3, §6.4, §8.2.1, §9; `NP-HW-HUB-001` Rev 6 §8, §8.1, §8.2, §8.4; `NP-HEX-ZM-001` §4a, §3.2; `NP-ART-001` §5; `NP-DB-005` Rev 5 §4; `docs/reference/competitive-position.md`
**Related Issues:** OI-HUB-C08, OI-HEXTILE-02, OI-HEXTILE-06, OI-SHELL2-05, OI-COST-01…10
**Gate:** REG-1 / ACT-1 (socket count is PROVISIONAL — every figure here inherits that)
**IEC 62304 Class:** N/A (commercial model; no software)
**Supersedes:** The BOM / COGS / GM% columns of CLAUDE.md §2.1 (Rev 35 and earlier) and `NP-DB-005` Rev 5 §4
**Parent Document:** None

---

> **⚠ READ THIS FIRST.** This document re-derives the cost columns of CLAUDE.md §2.1 against the
> hex-tile architecture. **Retail was locked when this analysis was commissioned, and is UNLOCKED as
> of Rev 2** (principal direction, 2026-08-16 — see §8). **§1–§7 are preserved exactly as derived
> *under the lock*, because that is what made margin an output rather than an assumption.** The
> result is that **every T1 configuration is gross-margin negative** at the prices in force, and the
> figures given are **floors** — they exclude a term (§5, **U**) that is uncosted in the record and
> very likely large and positive.
>
> **§8 does not set a price.** Unlocking the constraint yields a *ladder*, not a number: break-even
> for Home Standard is ~$1,196–1,278 and target margin needs $1,869–1,997, a 2.20–2.35× increase.
> Choosing the actual price is a separate commercial decision, and **`OI-HEXTILE-06` must be decided
> first** (OI-COST-10) or the price is set against a cost that decision invalidates.
>
> **This document does not close `OI-HUB-C08`.** It establishes *why* it cannot be closed, which is
> a stronger result than the estimate it was asked for: the netting OI-HUB-C08 describes is not
> merely unperformed, it is **not performable from the record**, and the reason also invalidates a
> larger term that OI-HUB-C08 does not scope. See §5.

---

## 1. Scope and what changed

The CLAUDE.md §2.1 cost columns were set against the **retired five-zone-module** design: five
position-unique 66 × 78 mm modules, 600 emitters total, 20-pin Hirose FH34S tails, hub-side drive.
The 2026-07-15 hex-tile decision (`NP-HEX-ZM-001`) replaced that with one universal 40 mm tile SKU
over an ~80-socket lattice. Six things changed that bear on cost:

| # | Change | Source |
|---|---|---|
| 1 | Cluster-controller tier introduced — 18 boards at n = 80 | `NP-HW-HEXTILE-001` §8.2.1 |
| 2 | L1 becomes an **electro-mechanical carrier**, not a moulded part | `NP-DRV-SHELL-002` §10.2 |
| 3 | Socket spring-contact arrays — 80 sockets × **19** contacts | `NP-DRV-SHELL-002` §5.1.4 |
| 4 | 24 V rail — N1 conductors and gate part classes re-rated | `OI-HUB-C17b` ADOPTED, `NP-DRV-SHELL-002` §5.4 |
| 5 | **Every tile carries its own driver + metering** (D-3) | `NP-HW-HEXTILE-001` §6.4 |
| 6 | Tile count is **configuration-dependent** — no fixed 600 LEDs / 5 modules | CLAUDE.md §3 |

Change 5 is the one that dominates, and it was not in the brief that commissioned this work.

## 2. Three assumptions this model rests on, none of which is sourced

**A model is only as good as its inputs are honest.** These three are stated here rather than buried
because **not one of them exists anywhere in the document set**, and each materially moves the
answer.

### A-1 — Per-configuration tile population (NOT WRITTEN DOWN ANYWHERE)

> **Finding.** There is **no per-configuration tile population** in the document set. Searching every
> controlled document for the configuration names returns two hits, both of which merely *cite* the
> $405 Home Standard BOM in passing (`NP-HEX-ZM-001:751`, `NP-HW-HEXTILE-001:296`). Neither
> allocates tiles to a configuration. **This is the input the whole model depends on, and it has
> never been decided.** Raised as **OI-COST-01**.

Adopted assumption, built from the two nearest statements in the record — `NP-HEX-ZM-001` §4a
(*"~8–9 × T1-B … + the balance in T1-A"*) and `NP-HW-HEXTILE-001` §6.4 option 1 (*"a build
populating 20–30 tiles retains full protocol flexibility"*):

| Config | T1-B (EEG/electrode) | T1-A (base PBM) | **Total tiles** | Basis |
|---|---|---|---|---|
| Core — EEG only | 4 | 0 | **4** | 4-ch EEG, no PBM. §3.2 notes Fp1/Fp2 may share one socket — see OI-COST-02 |
| Home Lite | 8 | 12 | **20** | 8-ch EEG; "first meaningful PBM configuration" at the bottom of §6.4's 20–30 band |
| Home Standard ★ | 9 | 21 | **30** | 8 neurofeedback sites + Oz; top of §6.4's band |
| Home Premium | 9 | 21 | **30** | identical hardware to Standard; EC lens is the delta |
| Pro Entry | 21 | 21 | **42** | 21-ch qEEG montage |
| Pro Full | 21 | 21 | **42** | as Pro Entry |

**T1-C (1064 nm) is at zero in every row** — CLAUDE.md §3 makes it a snap-in *accessory* upgrade
($149–199/zone), not base content. **T2-D (1170 nm laser tile) is not counted**: its per-headset
count is unspecified anywhere (**OI-COST-03**), so the Pro rows are floors even within this model.

### A-2 — Every configuration carries the full L1 carrier

The L1 carrier is laminated with all ~80 sockets and all 18 cluster controllers **in every
configuration**, including Core. Configurations differ only in how many *tiles* are populated.

**Why this assumption and not the cheaper one.** It follows the founding principle *"Shared platform
(one production line, two markets)"* and `NP-HW-HUB-001` §8.2's *"board cost is flat regardless of
how many of its 8 channels are populated"*. The alternative — populating only the clusters that have
tiles — is a real and unexercised lever (`NP-HW-HEXTILE-001` §8.2.2 option 5, `OI-HEXTILE-06`
option 1), but it trades D-7's stated principle of *"one physical grouping serving mechanics, power,
safety and addressing"* and would require a configuration-specific L1 lamination. **It has not been
decided.** Raised as **OI-COST-04**. Under the conservative-claim rule the dearer assumption is
carried.

### A-3 — BOM→COGS is a per-configuration multiplier, held constant

The task of carrying COGS through required deriving the existing relationship rather than inventing
one. **It is not a single rule.** Reading it off the six published pairs:

| Config | BOM (mid) | COGS (mid) | **Multiplier** | Uplift $ |
|---|---|---|---|---|
| Core | $168.50 | $259 | **1.537** | $90.50 |
| Home Lite | $265.50 | $371 | **1.397** | $105.50 |
| Home Standard | $405 | $540 | **1.333** | $135 |
| Home Premium | $460 | $622 | **1.352** | $162 |
| Pro Entry | $833 | $1,365 | **1.639** | $532 |
| Pro Full | $1,506 | $2,628 | **1.745** | $1,122 |

> **The multiplier is neither constant nor monotonic** — it falls 1.537 → 1.333 across Core → Home
> Standard, then climbs to 1.745 at Pro Full. No fixed markup, no fixed adder, and no linear fit
> reproduces the six pairs (successive slopes run 1.155, 1.21, 1.49, 1.99, 1.88). **So no single
> BOM→COGS rule can be recovered from the published figures**, and inventing one would silently
> re-price five configurations.

**Adopted:** each configuration keeps **its own** published multiplier. This is the only choice that
reproduces all six original pairs exactly and therefore the only one that changes nothing except
what the architecture changed. Raised as **OI-COST-05** — the multipliers encode non-BOM COGS
(assembly labour, test, packaging, freight, warranty reserve) that plausibly does **not** scale
linearly with a BOM that has tripled, so Finance should confirm they still hold at the new BOM
level. **If they do not, they will move up, not down** — these floors get worse, not better.

**GM% is confirmed as `(Retail − COGS) / Retail`** — it reproduces all six published percentages
(42, 38, 36, 48, 73, 81) to the rounding.

## 3. The delta, netted

**Correction to the commissioning brief.** The brief listed the cluster tier ($114.12), the L1
carrier ($125–216) and the socket contact arrays ($32–64) as three separate deltas to be added.
**They are not additive — that would treble-count.** `NP-DRV-SHELL-002` §10.1's
**"Estimated total, all lines — $175–225"** already contains all three, and its Rev 2 banner
supersedes the $125–216 figure the brief quotes (Rev 1's, computed at 12 passive carriers).

| Line | Value | Source |
|---|---|---|
| L1 electro-mechanical carrier, **all lines** | **$175–225** | `NP-DRV-SHELL-002` §10.1 — includes the $114.12 controller tier **and** the $32–64 socket arrays **and** the 24 V-rated high-side switches |
| Retired architecture removed | **−$18–30** | same table — *explicitly interconnect only*: 5 × FPC tails + 5 × Hirose FH34S + EEG harness |
| Hub PCB net | **+$1.09** | `NP-HW-HUB-001` §8.2 |
| **L1 net, per headset, all configurations** | **$146–208** | |

Per tile, from `NP-HW-HEXTILE-001` §6.4:

| Item | Per tile |
|---|---|
| U1 tinyAVR 2-series · Q1–Q3 N-FET · U2 dual TIA · passives · rigidizer PCB | $1.53 |
| **D1/D2 InGaAs PD (×2)** | **$10.00** |
| **Driver + metering per tile** | **$11.53** |

> **Source revisions re-verified 2026-08-18.** `NP-DRV-SHELL-002`, `NP-HW-HEXTILE-001` and
> `NP-HW-HUB-001` have each advanced several revisions since this model was built (to Rev 4, Rev 6
> and Rev 6 respectively), including `OI-HEXTILE-14`'s closure at 18 clusters / 20 connector
> positions. **Every figure above was re-checked against the current text and none has moved** —
> `NP-DRV-SHELL-002` §10.1's table is unchanged line for line, and its Rev 3 note states
> *"editorial only — no design decision, count or figure changed"*. Revision numbers are therefore
> omitted from the citations above: the figures are stable across them, and pinning a revision that
> keeps moving would make this table look stale when it is not.

**All figures are estimates pending EE Lead confirmation (`OI-SHELL2-05`), and are labelled as such
wherever they appear.** They are architecture-level derivations from component classes, not
quotations. A re-costing that presented them as settled would be worse than the caveat it replaces.

**One conflict of record, resolved and flagged.** `NP-HW-HUB-001` §8 carries a banner reading
*"⚠ EVERY FIGURE IN §8 IS VOID — do not quote"*, on the grounds that HEXTILE **D-4** moved the TIA
and ADC on-module. **That banner is stale**: `OI-HUB-C17c` was subsequently resolved **against
D-4** — the TIA, PD mux, NTC mux and ADC stay on the cluster controller, which is why the socket
went to 19 contacts. `NP-DRV-SHELL-002` Rev 2 (later than HUB Rev 3) accordingly re-uses §8.1 × 18
and states *"the two documents agree"*. **§8.1 is used here on that basis, and the stale banner is
raised as `OI-COST-06`** — it should be narrowed to the LED-drive line that D-3 genuinely deleted.

## 4. Result

**BOM floor** = old BOM + L1 net + ($11.53 × tiles). **COGS floor** = BOM floor × A-3 multiplier.
**GM%** = (locked retail − COGS floor) / locked retail. Low end pairs the cheaper L1 with the lower
old BOM; high end is the conservative pairing.

| Config | Tiles | Old BOM | **BOM floor** | **COGS floor** | Retail 🔒 | **GM% floor** | *was* |
|---|---|---|---|---|---|---|---|
| Core — EEG only | 4 | $168–169 | **$360–423** | **$554–650** | $449 | **−23% to −45%** | *42%* |
| Home Lite | 20 | $265–266 | **$642–705** | **$896–984** | $599 | **−50% to −64%** | *38%* |
| Home Standard ★ | 30 | $405 | **$897–959** | **$1,196–1,278** | $849 | **−41% to −51%** | *36%* |
| Home Premium | 30 | $460 | **$952–1,014** | **$1,287–1,371** | $1,199 | **−7% to −14%** | *48%* |
| Pro Entry | 42 | $833 | **$1,463–1,525** | **$2,398–2,500** | $4,999 | **+50% to +52%** | *73%* |
| Pro Full | 42 | $1,506 | **$2,136–2,198** | **$3,728–3,836** | $13,999 | **+73%** | *81%* |

**Reported plainly, as required: all four T1 configurations are gross-margin negative, and Home Lite
is the worst at −50% to −64%.** T2 survives because its retail is 3–6× its cost; it absorbs the same
absolute delta against a far larger price. **No assumption was adjusted to preserve a target GM%.**

**Where the money went.** For Home Standard the increase is $492–554 over the old $405, of which
**$346 (63–70%) is tile driver + metering** and **$146–208 is the L1 tier**. Of the $346, **$300 is
sixty InGaAs photodiodes.**

## 5. Term U — what is NOT in the table above, and why OI-HUB-C08 cannot be closed

Every figure in §4 is a **floor** because it excludes:

| Component of U | Status |
|---|---|
| **Emitter count delta** | **Uncosted.** Home Standard goes from 600 emitters to 21 × 90 + 9 × 44 = **2,286** (T1-A = 90, T1-B ≈ 44 per `NP-HW-HEXTILE-001` §4.2). Core 0 → 176; Home Lite 600 → 1,432; Pro 600 → 2,814. |
| **Retired hub-side LED drive stage** | **Uncosted.** The −$18–30 above is interconnect only, by the source table's own wording. |
| **Retired 5 × module shell / PDMS / FPC substrate** vs **new ~80 tile shells / PDMS** | **Uncosted on both sides.** |

> **This is the finding that blocks `OI-HUB-C08`.** OI-HUB-C08 asks for the retired drive
> electronics to be netted out before the cluster tier is netted in. That netting **cannot be
> performed from the record** — and the reason is worse than a missing line item:
>
> **`NP-ART-001` §5 records that the 660–670 nm and 808–830 nm emitters are *not selected*
> (`OI-HEXTILE-02`), so there is "no incoming-inspection criterion for _the dominant BOM line_."**
> No unit price for an emitter exists anywhere in the document set — not for the new tiles, and not
> for the retired 600. Both sides of the largest term in the subtraction are unpriced.
>
> **So OI-HUB-C08 stays open, and it is under-scoped.** It names only the *drive electronics*; the
> emitter delta is a separate and larger term that no open item currently owns. **Raised as
> `OI-COST-07`.** The result is presented as a bounded floor, per instruction, rather than guessing
> the subtraction.

**Direction of U.** Emitter count rises 3.8× on Home Standard while the retired drive stage served
only five slots. **U is therefore very likely positive and large** — every row in §4 is more likely
to get worse than better. At a merely illustrative $0.10/emitter (a number that is **not in the
record** and must not be quoted), Home Standard's floor would deepen by a further ~$169.

## 6. Does OI-HEXTILE-06 rescue it? No.

`NP-HW-HEXTILE-001` §6.4 offers three options. Run against Home Standard (old BOM $405, L1 $208
conservative, multiplier 1.333, retail $849):

| Option | Tile cost | BOM | COGS | **GM%** |
|---|---|---|---|---|
| — as modelled (30 tiles, InGaAs) | $11.53 | $959 | $1,278 | **−51%** |
| **1** — populate 20 not 30 | $11.53 | $844 | $1,125 | **−33%** |
| **2** — silicon PD on T1-A/T1-B (saves ~$9/tile) | $2.53 | $689 | $918 | **−8%** |
| **1 + 2 combined** | $2.53 | $664 | $885 | **−4%** |
| *for reference: old figure* | — | *$405* | *$540* | *+36%* |

> **Even the best combination of OI-HEXTILE-06's own options leaves Home Standard negative**, and
> that is *before* term U. The residual is structural: **the L1 tier alone ($146–208) is 36–51% of
> the entire old $405 BOM**, and no PD decision touches it. Option 3 (one PD pair per cluster) is
> excluded here because `NP-HEX-ZM-001` §4a explicitly protects the per-tile *"each tile meters
> itself"* claim, and CLAUDE.md §3 sells real-time per-zone J/cm² dose metering as the primary
> differentiator over Vielight.

**This is a programme-level finding, not a BOM-maintenance one.** Either retail unlocks, or tile
population and PD architecture change substantially, or T1 ships at negative margin. That is a
principal decision and is not taken here.

## 7. What is NOT affected

> **⚠ One item in this section was invalidated at Rev 2.** It previously also recorded that
> competitive **price comparisons** were safe to use *because retail was locked*. Retail is
> now unlocked (§8), so that justification is void and the claims are live. The charger
> finding below is unaffected — it never depended on price.

**CLAUDE.md §2.2 (charger policy) is unaffected — confirmed, not assumed.** The charger table is
keyed to *peak draw per configuration*, and the hex-tile change does not move peak draw: CLAUDE.md
§4.5's envelope is unchanged, and `NP-HW-HEXTILE-001` §9 holds concurrency to ~5–6 tiles regardless
of how many are populated — the lattice buys placement freedom, not simultaneous activation. Charger
BOM lines ($3–4 … $26) are unchanged.

## 8. Retail unlocked — principal decision, 2026-08-16

> **Retail pricing is UNLOCKED.** Principal direction, taken against §4's finding. The
> `**Retail 🔒**` column in CLAUDE.md §2.1 and `NP-DB-005` §4 is no longer a fixed input.

### 8.1 What was unlocked, and what that does and does not decide

**Unlocking the constraint is not the same as setting a price.** Retail was the one input this model
was forbidden to move, which is why §4 reported margin as an output. With it unlocked, the arithmetic
runs the other way — GM% becomes the input and retail becomes the output — and it yields a *price
ladder*, not a price. **The prices below are implied, not set.** Choosing the actual number is a
commercial decision about market position, elasticity and competitive framing, and it is not taken
in this document.

**The six configurations retain their current prices until that decision is made.** Nothing ships at
a new price on the strength of this section.

### 8.2 The implied ladder

Retail = COGS ÷ (1 − GM target), holding each configuration's **original** GM target. Low end pairs
the cheaper L1 with the lower old BOM; high end is the conservative pairing. **Every figure inherits
§5's floor status** — term **U** is still excluded, so these are the *least* retail would have to be.

| Config | COGS floor | **Break-even retail** | **Retail at original GM target** | Multiple of current | Current | Per-unit loss at current price |
|---|---|---|---|---|---|---|
| Core — EEG only | $554–650 | **$554–650** | **$955–1,121** (42%) | 2.13–2.50× | $449 | **−$105 to −$201** |
| Home Lite | $896–984 | **$896–984** | **$1,445–1,587** (38%) | 2.41–2.65× | $599 | **−$297 to −$385** |
| Home Standard ★ | $1,196–1,278 | **$1,196–1,278** | **$1,869–1,997** (36%) | 2.20–2.35× | $849 | **−$347 to −$429** |
| Home Premium | $1,287–1,371 | **$1,287–1,371** | **$2,475–2,637** (48%) | 2.06–2.20× | $1,199 | **−$88 to −$172** |
| Pro Entry | $2,398–2,500 | **$2,398–2,500** | **$8,881–9,259** (73%) | 1.78–1.85× | $4,999 | *+$2,499 (profitable)* |
| Pro Full | $3,728–3,836 | **$3,728–3,836** | **$19,621–20,189** (81%) | 1.40–1.44× | $13,999 | *+$10,163 (profitable)* |

**Break-even is the number that matters first.** It is the floor below which a configuration cannot
be sold at any margin. **Home Standard cannot be sold below ~$1,196** — 1.4× its current price —
before any margin is earned at all.

### 8.3 Three consequences the unlock creates, which the lock was concealing

**1. The T1 price ladder collides with the T2 ladder.** At target margin, Home Premium reaches
**$2,475–2,637** against a Pro Entry currently at $4,999. A consumer wellness configuration at
$2,600 and a clinical 510(k) configuration at $5,000 are not two tiers; they are one tier with a
regulatory footnote. **The two-tier structure of CLAUDE.md §1 is a casualty of this ladder** and
should be re-examined before any price is set. Raised as **OI-COST-08**.

**2. Pro's prices are also implicated, in the opposite direction from intuition.** Pro Entry and Pro
Full are *profitable today* (+$2,499 and +$10,163 per unit) yet still appear in the ladder, because
holding a **73%/81% margin target** against a risen COGS requires $8,881–9,259 and
$19,621–20,189. **These are the two rows where the target, not the cost, is the thing to question**
— T2 can absorb the delta and stay healthy at its current price, just at ~50%/~73% rather than
73%/81%. Unlocking retail should not be read as a mandate to raise T2 pricing.

**3. Every competitive price claim is now live.** §7 of the previous revision recorded that price
comparisons in `docs/reference/competitive-position.md` were *"factually correct and safe to use"*
**because retail was locked**. That justification is gone. At $1,869–1,997 Home Standard is **~40% of
a ~$5K Vielight Neuro Pro 2, not 17%**, and it lands directly on Sens.ai's $1.5–2K band — turning a
distant competitor into a direct price peer. The claims are flagged in that file rather than
rewritten, because they cannot be recomputed until a price is chosen. Raised as **OI-COST-09**.

### 8.4 The unlock does not retire the other two levers

§6 showed that `OI-HEXTILE-06`'s options do not restore positive margin **on their own**. They are
not thereby irrelevant — they set how far retail has to move. Silicon PD on T1-A/T1-B plus a 20-tile
build takes Home Standard's COGS from $1,278 to ~$885, and its target-margin retail from **$1,997 to
~$1,383** — a $614 reduction, and the difference between a 2.35× and a 1.63× price increase.
**Sequence matters: decide `OI-HEXTILE-06` before setting a price, or the price will be set against
a cost that the PD decision then invalidates.** Raised as **OI-COST-10**.

## 9. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-COST-01** | **Per-configuration tile population is not defined anywhere.** §2 A-1 adopts one; it is an assumption, not a decision. Every figure in §4 scales directly with it | Principal + Product | **All costing.** Decide with OI-HEXTILE-06 |
| **OI-COST-02** | Core is specified as 4-ch EEG, but `NP-HEX-ZM-001` §3.2 notes Fp1/Fp2 may share one socket at 40 mm pitch — Core may need 3 tiles, not 4 | Systems | Minor; REG-1 |
| **OI-COST-03** | **T2-D (1170 nm laser tile) per-headset count is unspecified**, so both Pro rows are floors even within this model | Hardware | T2 costing |
| **OI-COST-04** | Whether every configuration carries the full 18-cluster L1 (§2 A-2) or only populated clusters. Worth up to ~$140 on Core | ME + EE Lead | L1 lamination; **see `NP-HW-HEXTILE-001` §8.2.2 option 5** |
| **OI-COST-05** | Confirm the six A-3 BOM→COGS multipliers still hold at a ~2.4× larger BOM. Non-BOM COGS may not scale linearly — **if it moves it moves up** | Finance | Costing accuracy |
| **OI-COST-06** | **`NP-HW-HUB-001` §8's blanket "every figure is VOID" banner is stale** — `OI-HUB-C17c` resolved against D-4, so the TIA / PD mux / NTC mux / ADC lines survive and `NP-DRV-SHELL-002` Rev 2 relies on them. Narrow the banner to the LED-drive line D-3 actually deleted | Hardware | Documentation consistency; §8.1 is load-bearing for this document |
| **OI-COST-07** | **`OI-HUB-C08` is under-scoped.** It nets *drive electronics*; the **emitter-count delta** (600 → up to 2,814) is larger and unowned. Both sides unpriced because `OI-HEXTILE-02` has not selected an emitter | BOM sign-off | **Closing OI-HUB-C08** |
| **OI-COST-08** | **The T1 and T2 price ladders collide under the unlock.** At target margin Home Premium reaches $2,475–2,637 against a Pro Entry at $4,999 — a consumer configuration and a clinical one at the same price point. **CLAUDE.md §1's two-tier structure should be re-examined before any price is set** | Principal + Product | Pricing decision |
| **OI-COST-09** | **Every competitive price claim is live again.** `docs/reference/competitive-position.md`'s comparisons were safe *because retail was locked*; that justification is gone. At $1,869–1,997 Home Standard is ~40% of a ~$5K Vielight, not 17%, and lands on Sens.ai's $1.5–2K band. Cannot be recomputed until a price is chosen | Marketing | External claims |
| **OI-COST-10** | **Sequence: decide `OI-HEXTILE-06` BEFORE setting a price.** Silicon PD + a 20-tile build moves Home Standard's target-margin retail $1,997 → ~$1,383. Pricing first means pricing against a cost the PD decision then invalidates | Principal | **Blocks the pricing decision** |

## 10. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| **2** | **2026-08-16** | NeurOne Systems Engineering | **Retail pricing UNLOCKED by principal direction — new §8.** §1–§7 preserved verbatim as derived *under the lock*, since that is what made margin an output. §8.2 publishes the implied ladder (retail = COGS ÷ (1 − GM target), original targets held): **break-even Home Standard ~$1,196–1,278 and target-margin $1,869–1,997, a 2.20–2.35× increase**; Core $955–1,121; Home Lite $1,445–1,587; Home Premium $2,475–2,637. **These are implied, not set** — unlocking a constraint is not choosing a price, and the six configurations keep their current prices until that separate commercial decision is taken. §8.3 records three consequences the lock was concealing: the T1 ladder **collides with the T2 ladder** (Home Premium $2,637 vs Pro Entry $4,999 — the two-tier structure is a casualty, **OI-COST-08**); Pro's rows are the ones where **the margin target, not the cost, is the thing to question**, since both are profitable today and the unlock is not a mandate to raise T2 prices; and **every competitive price claim is live again** because Rev 1 §7's "safe to use" rationale rested entirely on the lock (**OI-COST-09**) — at $1,997 Home Standard is ~40% of a Vielight, not 17%, and becomes a direct Sens.ai price peer. §8.4 establishes the **binding sequence: decide `OI-HEXTILE-06` before setting a price** (silicon PD + 20 tiles moves the target-margin retail $1,997 → ~$1,383), else the price is set against a cost the PD decision invalidates (**OI-COST-10**). All §8 figures inherit §5 floor status — term U still excluded, so these are the *least* retail would have to be. |
| **1** | **2026-08-16** | NeurOne Systems Engineering | Initial release. Re-derives CLAUDE.md §2.1 BOM / COGS / GM% against the hex-tile architecture; retail untouched. **Result: all four T1 configurations gross-margin negative**, Pro unaffected. Corrects the commissioning brief's additive framing — `NP-DRV-SHELL-002` Rev 2 §10.1's $175–225 already contains the $114.12 controller tier and the $32–64 socket arrays, and supersedes Rev 1's $125–216. Establishes that **`OI-HUB-C08` cannot be closed from the record** and is under-scoped (OI-COST-07): `OI-HEXTILE-02` has selected no emitter, so the dominant BOM line is unpriced on both sides of the subtraction. Records three unsourced assumptions (tile population, full-L1-per-config, per-config COGS multiplier) as OI-COST-01/04/05 rather than burying them, and the finding that **no single BOM→COGS rule is recoverable** from the six published pairs. Confirms §2.2 charger policy unaffected. Raises OI-COST-01…07. |
