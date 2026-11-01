# Principal decision for review: OI-PWRSRC-15, the charger policy is keyed to configuration and the power study keys it to protocol set

**Status:** DRAFT, ready to send; not yet sent (no addressee named). Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Systems Engineering (`OI-PWRSRC-15` and decision D-12 in `docs/np_pwrsrc_001.md` §14; related D-6 and D-9)
**To:** The principal (the item's owner column reads "Principal"; D-6 is "principal / safety")
**Response needed:** a ruling on each question in section 3, a short rationale, conditions that would reopen it, name, role and date.
**Why now:** nothing is blocked. `CLAUDE.md` §2.2 is **locked and has not been changed**; this item asks whether it should change.

## 1. The question

`CLAUDE.md` §2.2 keys the charger to the **configuration's peak draw**. `NP-PWRSRC-001` finds that one configuration spans a very wide range of
protocol demand, so a configuration key cannot express a choice that depends on which protocols the buyer wants. Should §2.2 keep its single
key, or become a **configuration floor plus a protocol-set choice**?

## 2. What is known

From `NP-PWRSRC-001` §14 (a study; its figures are model outputs):

- A Core buyer runs no PBM, so nothing above 15 W matters for them. A Home Standard buyer's library spans **13 W to 1,775 W, a 137x range inside one configuration** (the document says 123x elsewhere in the same text; the sender has not reconciled them).
- What §2.2 would become: a **floor** keyed to configuration (the device must always run its fitted hardware, which is what §2.2 already is) plus a **choice** keyed to the protocol set.
- Auto-include by serial number survives for configuration upgrades. It has no trigger for a protocol choice, because deciding to run a protocol is not a serial event.
- The upfront upgrade as an intent signal inverts: it works today because the buyer does not know what it unlocks. The study finds a 65 W source unlocks **zero** protocols, so selling it as a protocol unlock would be false. Its value as a T2-intent signal is unaffected.
- The EU sentence, "any PD-compliant charger must work", survives, but it is the same sentence that disarms power-based tier gating (`NP-PWRSRC-001` §6.6a).
- `OI-PWRSRC-16` (regulatory counsel) asks whether the EU common-charger regime applies at all, since the device has no rechargeable battery. It is a separate question and nothing here depends on it.

**Not known:** the commercial effect of a protocol-keyed charger (no price is set, `OI-COST-10`), and the answer to `OI-PWRSRC-16`.

## 3. Questions

**Q1 (principal, D-12). Should `CLAUDE.md` §2.2 change from "configuration" to "configuration floor + protocol-set choice"?**
- **A. No. Keep §2.2 as locked.** The study stays advisory. Cost: a configuration key remains a weak predictor of demand, and the app keeps telling a buyer "power level: reduced" truthfully without a purchasable remedy keyed to protocols.
- **B. Yes. Adopt the two keys.** Cost: a locked section changes, so `CLAUDE.md` Rev bump, the commercial model, the upgrade logic and the app copy follow, and no price may be set before `OI-HEXTILE-06`.
- **C. Not yet.** Wait for `OI-PWRSRC-16`, since its answer changes which constraints are legal rather than architectural.

**Q2 (principal / safety, D-6). Is the arbitration in hardware decision taken?** D-6 proposes that the safety MCU gains three small Class C elements: a latched `PWR_ADEQUATE`, a time-at-limit counter and the entitlement check. The sender found no logged ruling on it in `docs/status/completed-decisions.md`. If it was decided, where? If not, it is the safety-MCU change that `OI-PWRSRC-11` (hub hold-up) also depends on.

**Q3 (principal, D-9). Is the device-bound signed entitlement decided as the mechanism for the failure class that power cannot gate?** `NP-PWRSRC-001` §6.7 argues it wins on five of seven axes. The sender found no logged ruling. It overlaps the signed tier identity of `REQ-UPG-02`, which is **not in force** (`OI-UPG-08`).

The sender assumes none of these answers.

## 4. What the sender needs back

1. A, B or C for Q1, and yes, no or "decided at (location)" for Q2 and Q3.
2. A rationale that names what fails if the key is wrong (CLAUDE.md §18).
3. Conditions that would reopen each, for example the result of `OI-PWRSRC-16`.
4. Name, role and date.

A change to §2.2 is the principal's decision and is not made as a detail; the sender changes nothing until the answer is recorded in `docs/status/completed-decisions.md`.
