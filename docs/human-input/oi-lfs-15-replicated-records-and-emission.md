# OI-LFS-15: does `REQ-LFS-01` reach every `REPLICATED` Config record, and what about an owner setting that lifts a refusal?

**Status:** DRAFT, not sent.
**GitHub issue:** none yet. Rule 7 of `docs/human-input/README.md` asks for one; the session that wrote this did not open an issue unasked.
**Sender:** the session that worked on `OI-LFS-15` (a Claude agent), drafted 2026-11-26.
**Addressee:** Firmware (`NP-SOUP-LFS-001`'s owner), with the Quality Lead / Risk for Q2. No person is named.
**Owning open item:** `OI-LFS-15` (`docs/status/pending-decisions.md`; `NP-SOUP-LFS-001` §9, §13.5).
**Not a controlled document and not a design input** (`docs/human-input/README.md`): no serial, not in `NP-DHF-001`, and it writes no requirement (`CLAUDE.md` §18).

**How to read it.** **[E]** is established, with the file that shows it. **[P]** is proposed here.

---

## 1. What I need from you

| # | Question | Recommendation |
|---|----------|----------------|
| Q1 | Is `REQ-LFS-01` left as written ("no value that bounds an emission may be stored under a tail-additive policy"), with caller rule R8 holding the emission-limit consumers to writing `REPLICATED` records and never reading them? **A:** yes, as built. **B:** widen it to "no value that bounds an emission is stored under any policy but `REBUILD`", with R8 as its gate. | **B** in its text, because it says what §6.2 already relies on; it changes no code, since R8 already enforces it for the listed consumers. Either is honest; **A** leaves the scope implied |
| Q2 | The photoparoxysmal detector's MANUAL OVERRIDE bit (`NP_CFG_FILE_PPX_CONFIG`, `REPLICATED`) lifts a safety refusal (`REQ-FWHUB-65` / `-66`) once the evidence level is BENCH. A `REPLICATED` record can come back as its previous valid copy (E4, `NP-SOUP-LFS-001` §13.18), so an override the owner cleared can return. Is that a hazard to control before the evidence level is ever raised? **A:** yes: a stored override is not applied at boot, and must be set again in the session (or by a signed owner credential, `OI-FWHUB-32`). **B:** yes: keep it stored, and add it to `RISK-LFS-09` with a control to be chosen when evidence is raised. **C:** no: an owner setting is outside `REQ-LFS-01`, and its rollback is accepted. | **A** or **B**; not **C** without Risk's rating |

What to send back: the option for each, a short reason (`CLAUDE.md` §18 wants one), anything that would reopen it, and your name, role and date.

## 2. What was found

- **[E]** Map 3 (`NP_CFG_FILE_MODULE_USAGE`, `REPLICATED`) holds per-module running totals, including drive time, metered dose and throttle counts (`firmware/hub_control/src/np_module_usage.c`; `NP-FW-NVRAM-001` §3.3.1.6). **A real path would exist if a limit consumer used one as a bound,** for example a lifetime-dose cap or a derating keyed on accumulated drive time. A rolled-back, stale or lost replica gives a smaller total, so the bound would be met late or never. No integrity check sees it, because the older copy is valid (§13.18 E4).
- **[E]** **No limit consumer reads one today.** `np_session_runner.c` is the only limit consumer that touches Map 3, and it only feeds it (`np_mu_session_begin`, `_add_used`, `_add_dose`, `_note_throttle`, `_session_end` and others). Nothing in `firmware/` outside its tests calls `np_mu_snapshot()`, the read of the totals.
- **[E]** **Built in this change:** caller rule R8 in `scripts/check-lfs-caller-rules.ts`. An emission-limit consumer never calls `np_cfg_store_replicated_read()`, names a `REPLICATED` file id only to write it, and uses a name from a `REPLICATED` record's owner only where `REPLICATED_API_USES` lists it for that file, as a writer or as a reader with the reason the value bounds no emission. It is keyed on the policy each row names and on the owner module, never on Map 3's row layout. Its self-test shows it rejects a limit consumer reading Map 3's totals and allows the session runner's writes.
- **[E]** Two limit consumers read a `REPLICATED` record today, and R8 lists both with a reason:
  - `np_session_runner.c` reads the pause limit (`NP_CFG_FILE_PAUSE_LIMIT`, `SWR-SS-05`) when a pause begins, with every enable withdrawn. Its expiry ends a session that is emitting nothing.
  - `np_mod_cvns.c` relays the cardiac cutoff sensitivity (`NP_CFG_FILE_CARDIAC_SENS`, `REQ-FWHUB-74`) to the safety MCU. Level 0 is the validated cutoff and no level is less sensitive, so an older or lost value still leaves the cutoff at least as protective as validated.
- **[E]** **What R8 cannot see: a value an owner pushes into another module.** `np_ppx_config.c` reads its record at boot and calls `np_ppx_policy_set_config()`, which `np_mod_visual.c` consults. The override bit lifts the 3 to 60 Hz photic-driving refusal when the evidence level is BENCH (`firmware/hub_control/include/np_ppx_config.h`). It lifts nothing today because `NP_PPX_EVIDENCE_IN_FORCE` is NONE (`np_ppx_policy.h`), and a stored override is not applied while it is NONE.
- **[E]** The override cannot move to `REBUILD`, because nothing re-answers an owner's choice. A literal widening of `REQ-LFS-01` that counted it as a value that bounds an emission would forbid storing it at all. That is why Q2 is a separate question from Q1.
- **Assumed, not checked:** that `REQ-LFS-01`'s "bounds an emission" means a magnitude or a limit and not a permission. Q2 exists because the override is a permission.

## 3. Options and their costs

**Q1 A (as written).** No document changes. R8 enforces the replicated rows' "bounds no emission" comments for the listed consumers. *Cost:* the requirement still names only a policy that no longer exists, so its scope stays implied by §6.2 and R8.

**Q1 B (widen).** `NP-SOUP-LFS-001` §6.2 restates `REQ-LFS-01` for every policy but `REBUILD`. R8 is already its gate, and §6.1's Class B argument is unchanged, because it already assumed this. *Cost:* the wording changes in one controlled document. If Q2 is answered C, the widened text must say that a permission an owner sets is not a value that bounds an emission.

**Q2 A (not applied from storage).** The override lives in RAM for the session it is set in, or is stored but applied only after a signed owner credential checks it (`OI-FWHUB-32`). *Cost:* the clinician sets it again each session, which is a usability cost and a firmware change.

**Q2 B (risk row now, control later).** It is added to `RISK-LFS-09` as an instance that matters only once the evidence level is raised, and the evidence-raising change must close it. *Cost:* nothing now, and a dependency that change has to remember.

**Q2 C (accept).** *Cost:* a rolled-back override can lift a refusal the owner withdrew, with no integrity check that sees it. That needs a Risk rating, which this brief does not give.

## 4. What stays open until you answer

`OI-LFS-15` stays open on the scope decision (Q1) and on the override (Q2). The guard for Map 3 and the other `REPLICATED` records against the listed limit consumers is built (R8, `NP-SOUP-LFS-001` §13.5).
