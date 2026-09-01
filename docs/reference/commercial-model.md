# Commercial model — retail ladder, charger policy, consumables, clinician tiers

> Relocated from CLAUDE.md §2.1a, §2.2, §2.3 and §6.1 (Rev 40) to slim the always-loaded core.
> Content is verbatim; section numbers are unchanged so inbound `CLAUDE.md §2.2`-style citations
> still name the right block. CLAUDE.md keeps the invariant of each section and points here.
>
> **Read this file when:** quoting or setting a price, sizing a charger, working on consumables or
> subscription revenue, or answering any margin question. The cost *derivation* behind §2.1a is
> `docs/np_cost_001.md` (NP-COST-001) §8 — that document, not this one, is the authority on how the
> figures were produced and on what they exclude (term **U**).
>
> **Binding constraint carried over from §2.1a:** no price may be set before `OI-HEXTILE-06` is
> decided (`OI-COST-10`).

### 2.1 Configuration table — the full caveats (condensed in CLAUDE.md §2.1)

The configuration table itself stays in CLAUDE.md §2.1 because a cost or margin question usually
starts there. Its two qualifying blocks are reproduced here in full; CLAUDE.md carries a condensed
form of them.

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

