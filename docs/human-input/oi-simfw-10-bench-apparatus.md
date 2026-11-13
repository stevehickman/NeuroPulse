# Bench and first-article question for review: OI-SIMFW-10, is there a bench rig, and what runs on it

**Status:** DRAFT, ready to send; not yet sent (no addressee named). Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#825](https://github.com/stevehickman/NeuroPulse/issues/825)
**From:** NeurOne Software Engineering (`OI-SIMFW-10`, `NP-SIM-FW-001` §11)
**To:** Whoever owns bench and first-article test (name and role to be assigned)
**Response needed:** an answer to each of the four questions in section 3, with name, role and date.
**Why now:** `NP-SIM-FW-001` §11 layers the simulator so that scenarios and the bridge protocol could also drive a bench rig. That is only a layering until someone says a rig exists. Nothing here is built for a rig, and nothing in the repository acts as if these answers were given.

## 1. What is known

- No document specifies a bench apparatus. `NP-FW-BENCH-001` covers the head-presence gate and what bench activity needs from it. Its D-7: a rig legitimately has modules out, and the answer is a protocol that requires only the modules present. `SEAT#` and `np_module_map_check_placement()` are never relaxed.
- First-article tests (`NP-FAI-001` and the per-artifact lists) are bench tests run by hand today.
- Simulator runs are excluded from verification evidence (`REQ-SIMFW-06`). A scenario run on a rig could be evidence, and then `REQ-SIMFW-10` applies: the runner, bridge and module models are validated under the quality system, and the evidence names their versions.
- `REQ-SIMFW-09`: a scenario on real hardware may inject only modules the rig does not have. A rig provider refuses an injection aimed at real hardware.

## 2. What is assumed

That the scenario file, the bridge protocol and the module models are shared with a rig and only the platform provider changes. That is the §11 proposal. It has not been reviewed by anyone who runs a bench.

## 3. Questions

**Q1. Does a bench rig exist, or is one planned?** Options: (A) exists, (B) planned with an owner and a date, (C) neither. On C, `OI-SIMFW-10` stays open and §11 stays a layering.

**Q2. What is the hub on the rig?** (A) the real i.MX RT1062 on the production board, (B) a development board, (C) a host process. On A, only the scenario and bridge layers are shared; the firmware is the cross-compiled image. On C, the rig is the simulator and `REQ-SIMFW-06` applies.

**Q3. Which first-article items would be scripted?** Name them by item ID (for example `FAI-TC05`). Say which stay manual.

**Q4. Is evidence from a scripted run acceptable in place of manual steps?** Yes with `REQ-SIMFW-10`'s validation, yes with other conditions, or no. If yes, name who validates the runner and module models, and confirm the ISO 13485:2016 §4.1.6 citation in `REQ-SIMFW-10` (marked there "to be confirmed by Quality").

## 4. What to send back

The answer to each question, a short rationale (CLAUDE.md §18 wants a derivation), conditions that would reopen it, and your name, role and date. Include no personal data and no UHDR content.

## 5. When answered

Record the answers in `NP-SIM-FW-001` §11, close `OI-SIMFW-10` there, log it in `docs/status/completed-decisions.md`, and set the status line above to `ANSWERED`.
