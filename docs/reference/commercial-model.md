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

> **⚠ One row below contradicts that second rule and is not resolved here.** The audio cup foam
> names *"Calendar reminder"* as its trigger, which the invariant forbids; `docs/np_acc_priority_001.md`
> raises it and declines to resolve it, on the ground that inventing a seal or compression proxy is
> the design work the invariant exists to protect. Either the foam gains a measurement trigger or the
> invariant gains an explicit, reasoned exception — a principal decision, Product + FW.

| Item | Price | Interval | GM% | Notes |
|------|-------|----------|-----|-------|
| Intranasal sleeves (30-pack) | $19/pack or $19/mo sub | Single use | 68–79% | Only authenticated consumable. COGS $4–6. Primary MRR driver. |
| Electrode hydrogel tips (8-pack) | $12–16 or $9.99/mo sub | 30–60 sessions | 60–72% | App impedance trend prompts. Bayonet snap, zero training. |
| VNS clip pads (2-pack) | $8/pack | 20–40 sessions | 65% | Electrochemical degradation from VNS current. |
| Audio cup foam (set) | $24/set | 6–12 months | 58% | Calendar reminder. |
| Audio cup mesh frame (pair) | $9.99/pair | Annual | 62% | App driver impedance flags fouling. Snap-in, user-replaceable. |
| Interface protection covers (complete kit) | $22.99 or $19.99/yr bundle | Annual / as lost | 70% | All tethered — loss prevention by design. |
| S3 prescription Rx insert | $49–139 | 12–24 months | Variable | Optician partner network. Zero marginal marketing cost per renewal. |
| T2 service contract | $1,800/yr | Annual | ~75% | Same-day loaner, priority support, annual calibration. |

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

