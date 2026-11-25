# Question for the principal: OI-PRIV-22 (NP-PRIV-ANALYSIS-004), does "safety interlock log → SHDR" still cover interlocks that read the wearer

**Status:** ANSWERED 2026-11-23 by the principal: **option B**, and SHDR learns that a stop happened but never why. Not a controlled document; see `docs/human-input/README.md`.

> **The ruling (the principal, 2026-11-23).** The line now reads: a safety interlock that reports the device → SHDR; one that trips on a reading of the wearer → UHDR.
> Scope, also ruled: the cardiac cutoff (`0xC1`) **and** the cervical VNS SHDR session summary (impedance passed, cutoff occurred, fault reason) leave SHDR.
> On surveillance: "SHDR should know that a cutoff occurred because it causes a change in power / usage and thus affects hardware life. It doesn't need to know why
> the cutoff occurred. There may be other kinds of stops (hardware failure, panic, etc.) that look identical in terms of hardware usage changes."
> Implemented as one cause-free `stopped_early` byte in the SHDR session-end record (`REQ-FWHUB-89` (c)), set alike by any abort, any cervical VNS fault and any
> grant the safety MCU withdraws. It also overtakes Q2 of `oi-priv-41-session-ordinal-and-cardiac-cause.md` (ruled option A the same day, one cause-neutral event): SHDR says nothing about a cutoff's cause, because it has no cutoff event.
**GitHub issue:** [#950](https://github.com/stevehickman/NeuroPulse/issues/950)
**From:** NeurOne Firmware Engineering (`OI-PRIV-22` of NP-PRIV-ANALYSIS-004 in `docs/status/pending-decisions.md`; firmware privacy review, Finding H2)
**To:** the principal (Safety consulted on option B, below)
**Response needed:** an option; a short rationale; anything that would reopen it; your name, role and the date.

## 1. The question

`docs/reference/data-architecture-detail.md` §5.1 has a one-line boundary resolution: *"IR eye state during sessions → UHDR; safety interlock log → SHDR"*. It was written before the photoparoxysmal detector existed and before `CLAUDE.md` §5.1 rule 2 (a conditional redaction leaks its predicate, 2026-08-12).

Some interlocks report the **device** (a goggle lifted off its Hall sensor, a hot tile throttled). Others trip on a **reading of the wearer**: the photoparoxysmal detector reads the wearer's EEG, and the cervical VNS cardiac cutoff reads the wearer's heart rate. A log entry from the second kind says *this person had a photoparoxysmal discharge* or *this person's heart rate jumped during stimulation*, under the device token.

Plain form: **should the line read "a safety interlock that reports the device → SHDR; one that trips on a reading of the wearer → UHDR"?**

## 2. What is already done, without a ruling

- **The photoparoxysmal halt is now UHDR** (this change, `REQ-FWHUB-88`). The SHDR write was conditional on the halt (rule 2), and moving a field from SHDR to UHDR raises its protection, so it follows the existing rules. `data-architecture-detail.md` §5.1 has a row for it, and line 49 now points to that row.
- **Nothing else is moved.** The question below is only about the general line, and the one field it still carries that reads the wearer.

## 3. What the general line still carries

| Interlock | What trips it | Where it goes today | Source |
|---|---|---|---|
| Cervical VNS cardiac cutoff | the wearer's heart-rate step | SHDR: `NP_CVNS_SHDR_EV_CUTOFF` (`0xC1`) → `fault_log.fault_type = 'CVNS_HR_CUTOFF'`, published on purpose | `data-architecture-detail.md` §5.1 (fault-latch row); `np_hub_config.h:242` |
| Cervical VNS re-enable lifecycle after a cutoff (`0xC2`–`0xC6`) | what the wearer did next | **nowhere**: retired by `OI-PRIV-28` (NP-PRIV-ANALYSIS-004), `REQ-FWHUB-91`, which keeps only the cutoff flag `0xC1` in SHDR | `data-architecture-detail.md` §5.1 (re-enable lifecycle row) |
| Goggle Hall cut, PBM thermal throttle, watchdog | the device | SHDR | the line itself |

The DHF entry of 2026-08-12 (DHF Rev 29, `np-dhf-001-revision-history.md`) already reported, and left open, *"whether publishing `CVNS_HR_CUTOFF` to SHDR at all is correct (a separate locked decision)"*. This is that question, made general.

The cardiac cutoff is not a conditional redaction in the rule-2 sense when it is written for every cutoff: it is a deliberate disclosure. But it is a disclosure about a person, which the defining test of §5.1 would put in UHDR if the field were new.

## 4. Options

| Option | What changes | Cost |
|---|---|---|
| **A. Keep the line; decide each field by its own row** | Nothing more. The photoparoxysmal halt stays an exception by its row; the cardiac cutoff stays in SHDR. | None now. The line keeps reading as if it covered every interlock, so the next detector-driven interlock is likely to be logged to SHDR again by default, as this one was. |
| **B. Narrow the line: wearer-read interlocks → UHDR** | Line 49 is reworded. The cardiac cutoff (`0xC1`) leaves SHDR and stays in the UHDR session record (which already holds `cutoff_time_offset_s`). `fault_log` loses the `CVNS_HR_CUTOFF` type. | Fleet surveillance can no longer count cardiac cutoffs per device. Whether post-market surveillance needs that count is an ISO 14971 question for Safety, which is why Safety is consulted. Nothing ships today, so no stored record migrates. |
| **C. Narrow the line, and give surveillance an unconditional-shape counter** | As B, and if Safety needs a fleet signal, it is a count in a record written for **every** device at **every** upload whatever the value (the fault-latch `count` is the model), never a record that exists only when an interlock tripped. | As B, plus one fixed-shape field. A per-device count of cardiac cutoffs is still a fact about the wearers of that device; on a single-user helmet it is a fact about one person, so the count would need its own §5.1 row and rule-1 argument. |

## 5. Recommendation

**B**, with Safety asked whether surveillance needs a fleet signal; if it does, **C** for that signal only. The line was written for device faults, the detector made it reach the wearer's nervous system, and §5.1 rule 1 puts the burden on SHDR, not on UHDR. Option A leaves the default pointing the wrong way.

## 6. What this does not decide

- The photoparoxysmal halt: already UHDR by its own row (§2).
- The other SHDR leaks the review found in the same file (`OI-PRIV-21` of NP-PRIV-ANALYSIS-004) and the redaction-shape gate's coverage (`OI-PRIV-40`): separate items.
- Any interlock threshold, timing or enable path. This is about what is logged, not about what the interlock does.
