# OI-PRIV-54 — how long the warranty registry keeps an owner's data, and what erasure and transfer remove

**Status:** DRAFT, not sent. **From:** Software (Claude Code session, 2026-11-25). **To:** the principal, with the Privacy Lead (interim: the principal). The periods themselves may need regulatory counsel per market; that is noted where it applies.
**GitHub issue:** none yet. One is to be opened when this brief is sent (rule 7).

**What is asked.** Five decisions about the warranty registry (`ci/warranty/warranty_registration_schema.sql`), the one database in the design that holds a person's name, email and postal address. Each question below has options with costs and a recommendation. Nothing here is a decision, and nothing in the repository acts as if one has been made. `OI-PRIV-54` stays open until Q1 to Q3 are answered; Q4 and Q5 can follow.

## 1. What is known

- **What the registry holds.** `warranty_registrations`: the warranty token, serial number, model, the owner's name, email and address, the registration and purchase dates, retailer, tier, warranty expiry and whether the owner agreed to support contact. `warranty_claims`: a claim's kind, status and the day it was opened. `warranty_claim_notes`: free-text notes on a claim. The token is the only link to the SHDR fleet database, and joining the two is prohibited (`NP-FW-EMMC-002` §A.3 to §A.5).
- **What is already built (no decision needed, `NP-PRIV-ANALYSIS-005` F4).** Support contact defaults to no (`REQ-WARR-01`). The free-text `claim_description` is gone: a claim's kind is a fixed list the database enforces, and notes are in their own table, with no identity field, under a role of their own (`REQ-WARR-02`). No column holds a time finer than a day (`REQ-WARR-03`). Every table has a `retain_until DATE` column (`REQ-WARR-04`). `ci/test_warranty_schema.py` checks all of it. Requirements: `docs/reference/data-architecture-detail.md` §5.7.
- **What is deliberately not built.** `retain_until` has no default and may be NULL; nothing computes it. No deletion job, no erasure path and no transfer rule exists. The foreign keys keep PostgreSQL's default (`NO ACTION`), so today a registration with claims, or a claim with notes, cannot be deleted at all. That is the default left in place, not a choice, and the answers below decide what replaces it.
- **What happens on transfer today.** A factory reset makes a new warranty token, and the new owner registers it (`NP-FW-EMMC-002` §A.6). The SHDR record under the old token is archived, not deleted, and flagged `device_transferred`. Nothing says what happens to the previous owner's registration: it stays, with their name and address, with no end date. The registry can see a transfer: a new registration arrives for a serial number that is already registered.
- **The rules that bear on it.** GDPR Art. 5(1)(e) (keep personal data no longer than the purpose needs), Art. 17 (erasure, with the exceptions in Art. 17(3), including a legal obligation and legal claims), Art. 9 (health data, which notes can still collect); CCPA §1798.105 (deletion).

## 2. What is assumed, and not checked

- That no regulation obliges NeurOne to keep an owner's contact details beyond the warranty, for example to reach them with a recall or safety notice. T1 is a wellness product and T2 a 510(k) target; whether any such duty applies, in which market and for how long, is a question for regulatory counsel that this brief does not answer. **If one applies, it sets a floor under Q1 and an exception in Q3.**
- That the limitation period for a warranty or consumer claim is what decides how long a registration is useful after expiry. It differs by market. The number is counsel's; the shape of the rule is the principal's.
- That claim kinds and dates, with no link to a person, are useful as product-quality statistics. Nobody has asked the quality owner.

## 3. The questions

### Q1. How long is a registration kept?

| | Option | Cost | What stays open |
|---|---|---|---|
| **A** | **Until the warranty expires plus a fixed period** (the claim limitation period, set by counsel per market), extended while a claim is open. `retain_until` is set at registration from `warranty_expires`. | A deletion job and the per-market period. Contact for anything after that date is lost. | The period per market. |
| **B** | **While the registered owner holds the device, then a fixed period** after a transfer or an erasure request. | The registry keeps identity for as long as the device is in use, possibly a decade. `retain_until` stays NULL until an event sets it. | Weak against Art. 5(1)(e): the end depends on an event that may never be reported. |
| **C** | **Until the warranty expires**, then deleted. | Shortest. A dispute raised just after expiry finds no record. | Whether that is acceptable to whoever handles disputes. |
| **D** | **No end** (today's state). | None now. | Conflicts with Art. 5(1)(e); listed only because it is the status quo. |

**Recommendation: A.** It is the only option with a date known at registration, so the column, the job and the notice to the owner can all state it.

### Q2. How long are claims and notes kept?

| | Option | Cost |
|---|---|---|
| **A** | **Notes are deleted a short period after their claim is resolved; claim rows live as long as the registration.** | A second, shorter period to set. Notes carry the most sensitive content and go first. |
| **B** | **As A, but at the registration's end each claim row is de-linked** (its token removed) and kept as an anonymous fault statistic: kind, status, day. | A schema change (the token becomes nullable, or claims move to a statistics table). Useful only if quality wants the statistic. |
| **C** | **Claims and notes live exactly as long as the registration** and go with it. | Simplest. Notes kept for years with nothing needing them. |

**Recommendation: A**, or **B** if the quality owner wants fault statistics. Either way notes should not outlive the claim they explain by long.

### Q3. What does an owner's erasure request remove?

| | Option | Cost |
|---|---|---|
| **A** | **Everything about them:** the registration, its claims and their notes. The owner is told the warranty registration ends with it. | The foreign keys cascade (registration to claims to notes). A delete-by-token path in the registry service. |
| **B** | **Identity only:** name, email and address are cleared, and the row (token, serial, dates, claims) stays. | The warranty can still be honoured on proof of purchase. The remaining row is still linkable to the device's SHDR record through the token. |
| **C** | **A, except records under a legal hold** (a claim in dispute, or a duty counsel identifies under §2), which keep the minimum and are deleted when the hold ends. | A hold flag and its review. This is what Art. 17(3) already permits, made explicit. |

**Recommendation: C.** It is A in the ordinary case, and it states the one reason a row may survive instead of leaving it to judgement. Not asked here, but it follows: whether an erasure request also reaches the SHDR record under the same token. After the registration is gone nothing links that record to a person, so the current design would keep it; say if that should change.

### Q4. What happens to the previous owner's registration when the device is transferred?

| | Option | Cost |
|---|---|---|
| **A** | **Deleted when the new owner registers** (with its claims and notes). | A dispute about a pre-transfer claim finds nothing. |
| **B** | **Identity removed when the new owner registers; the rest kept** for warranty history, as the SHDR record is archived, until Q1's date. | The trigger is a new registration for the same serial number, which the registry can see. |
| **C** | **Unchanged until Q1's date**, with the identity kept. | A previous owner's address stays on file after they have parted with the device. |

**Recommendation: B.** It matches what the design already does with the SHDR record (archive, do not delete) without keeping a name and address that no longer belong to the device. Not asked here: whether the warranty itself transfers to the new owner, which is commercial.

### Q5. Are free-text notes kept at all?

| | Option | Cost |
|---|---|---|
| **A** | **Keep `warranty_claim_notes` as built**, with Q2's short retention and its own role. | Notes can still hold health statements; the separation limits who reads them and for how long. |
| **B** | **Drop the table:** claims are the enumerated kind only. | Support staff will write the detail somewhere, most likely a ticketing tool with its own, uncontrolled retention. |

**Recommendation: A.** B does not remove the text, it moves it out of sight. Not asked here, but it follows: the claim list deliberately has no value for an injury or a symptom (`REQ-WARR-02`), so a warranty claim that reports one needs a route out of the registry to whatever complaint or adverse-event handling the product has. No document names that route today.

## 4. What to send back

- For each of Q1 to Q5, the option chosen (or another), and for Q1 and Q2 whether counsel sets the periods or the principal does.
- A short rationale for each (`CLAUDE.md` §18 wants a derivation; a period with no source would be a row in `docs/status/unjustified-choices.md`).
- Conditions that would reopen it (for example a regulatory traceability duty found under §2, or the registry moving to a third-party processor).
- Name, role and date.

**What the answer changes.** The schema sets `retain_until` from the chosen rule and the foreign keys take the chosen delete action; the registry service gains the deletion job and the erasure and transfer paths; `REQ-WARR-04` gains the rule and `ci/test_warranty_schema.py` checks it; the answer is recorded in `docs/reference/data-architecture-detail.md` §5.7 and `NP-FW-EMMC-002` §A.6 and logged in `docs/status/completed-decisions.md`. `OI-PRIV-54` closes when that is built and gated.
