# OI-SIMFW-35: after a compaction that lost its COMMIT, is withholding cervical VNS from everyone intended?

**Status:** DRAFT, not sent. **From:** a Claude Code session working for the principal (2026-11-11). **To:** the safety
**GitHub issue:** [#826](https://github.com/stevehickman/NeuroPulse/issues/826)
MCU's owner (not yet named), with Risk and Clinical/HFE for §2 Q2. **Owning item:** `OI-SIMFW-35`
(`docs/np_sim_fw_001.md` §22). Related: `OI-SIMFW-27` (Q8: the new latch store shares this code), `OI-FMEA-10`,
`OI-RISK2-07`, GitHub #826, #467.

**This is a proposal, not a decision.** No firmware was changed for it.

## 1. What happens today

`np_nv_state.c` keeps the per-user cervical VNS cardiac cutoff in two flash pages. When the live page fills, the whole
state is written to the other page as RESET … COMMIT, and the full page is erased afterwards. If power is lost before
the COMMIT is written, the next boot finds an uncommitted snapshot beside the old page. It merges the snapshot's records
on top of the old state and sets `NP_SAFETY_USER_ANY`, which withholds cervical VNS from every user. In the simulator
run a user who had never had a cutoff read `USER_BLOCKED`, and the next write completed the compaction with the block
kept.

**The documents say this is the design:**

| Source | What it says |
|---|---|
| `np_nv_state.c` header | "An UNCOMMITTED snapshot is never trusted to replace the state: its records are merged in on top of what came before, and ANY is set, because the write it was carrying may be missing" |
| `NP-SW-001` `REQ-SW01-03` | "a snapshot that was never committed … never replaces committed state and fails closed" |
| `NP-FMEA-001` FMEA-M05-06 | "a torn record, an uncommitted snapshot, an overflow or a write failure all read as *blocked*" |

**And there is an exit.** The header says an acknowledgement, the re-enable confirmation run by a named person, "clears
that person's entry and both unattributable kinds". So the first named user who completes the cardiac re-enable path
(30 s lockout, app confirmation, repeat impedance check) clears `ANY` for everyone. The row's "no user is named" is true
of the block and not of its release.

## 2. The questions

**Q1. Is `ANY` after a lost COMMIT intended, as against reading the old page alone?**

| Option | What it means | Cost |
|---|---|---|
| **A. Intended (as built)** | The row is closed as "by design", citing the three sources above | None in code. A power loss in the compaction window costs every user of that device one re-enable confirmation |
| **B. Read the old page alone when it is intact** | An uncommitted snapshot is discarded if the old page still decodes completely | **This can lose a cutoff.** The compaction is triggered by a write that did not fit in the full page, so the record that write carried (which can be a new cutoff) may exist only in the uncommitted snapshot. Discarding the snapshot discards it. To be safe, B would have to prove the triggering write is in the old page, which it is not by construction |
| **C. Merge without `ANY` when the snapshot is complete but for its COMMIT** | If every record the snapshot should hold is present and only the COMMIT is missing, trust the merge and do not set `ANY` | The MCU cannot know how many records the snapshot should hold without the COMMIT, which is what carries the count. Needs a format change (the count written first), and a new compaction path to certify |

**Recommendation: A.** The behaviour is documented in three places, fails closed, and has an exit. B weakens the control
it is meant to tidy. C is a format change to remove one confirmation after a rare power loss. No option here may make a
cutoff easier to lose.

**Q2 (Clinical/HFE, optional). Is the person told why?** The reply already carries `NP_SAFETY_NV_FLAG_OUTSTANDING` for
the blanket warning, and never says which user. A person blocked by `ANY` after a power loss has had no cardiac event.
Whether the app's message for that case should differ from the message after a real cutoff is a wording question for
the app (CLAUDE.md §17), and it must not reveal whether anyone on the device had a cardiac event (§5.1 rule 2: the
message must not depend on that). Not asked of the safety MCU's owner.

## 3. What is known, and what is not

Known from the run (`np_sim_safety_tests.c`): §1. Known from the code: the exit path, and that `OI-SIMFW-27` Q8 C will
reuse this log, commit and compaction code for a device-wide latch store, so whatever is decided here applies there too.
Not known: how wide the power-loss window is on silicon (one double-word write; the flash timing is unmeasured), and so
how often a user would meet this.

## 4. What I need back

For Q1 the option, a short reason, and your name, role and date. If A, the row closes as intended behaviour and the
scenario that records it stays as its regression check.
