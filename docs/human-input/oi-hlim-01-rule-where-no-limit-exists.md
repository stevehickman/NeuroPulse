# Question for the principal: OI-HLIM-01, what the acknowledgement rule does where no health limit exists

**Status:** ANSWERED 2026-11-13 by the principal: **Q1 B, Q2 A, Q3 A**. Recorded in `NP-SW-001` §6.3.6 `SWR-SS-02`, `docs/status/completed-decisions.md` and `docs/reference/health-limits.md` §8.1. Not a controlled document; see `docs/human-input/README.md`.

> **Answer, 2026-11-13 (the principal):** Q1: B. Q2: A. Q3: A. The principal also directed that which limits are which comes from the documents, and that whether a limit exists for a modality, per day or per session, and whether doses combine across sites, comes from the research: a limit is added where the research states one, and where it states none or the matter is unknown there is none, with the rationale that no research-established limit was found.
**GitHub issue:** [#563](https://github.com/stevehickman/NeuroPulse/issues/563)
**From:** NeurOne Software Engineering (`OI-HLIM-01` in `docs/status/pending-decisions.md`; GitHub #563; `NP-SW-001` §6.3.6 `SWR-SS-02`)
**To:** the principal
**Response needed:** an option for each of Q1 to Q3, a short rationale, anything that would reopen it, and the date.

## 1. The question

Your rule (2026-10-08, `OI-UI-KMP-09`) is that a long session is admissible, that nothing beyond a health safety limit runs without the user's acknowledgement, and that nothing beyond a hardware or system limit runs at all. The survey for #563 (`docs/reference/health-limits.md`) finds that for most modalities there is no health limit on session length or cumulative dose to compare a session against, and that no per-day limit is enforced for any modality.

Plain form: when a modality has no health limit, what should the app do with a very long session of it? And if a daily limit is ever set, which part of the system keeps the day's total?

## 2. What is known

- **A per-session figure exists** for PBM transcranial (60, 60 and 36 J/cm² by wavelength), 1170 nm (60 J/cm²), tDCS and HD-tDCS (150 mC/cm², provisional, `OI-CHARGE-06`) and cervical VNS (60–120 s). None of the PBM or 1170 nm figures has a recorded source. Only the electrical ones reach the acknowledgement path that is built (the zone model, `docs/reference/safety-zones.md`).
- **No figure exists** for PBM intranasal, EEG and qEEG, BES/tACS and clinical tACS duration, auricular VNS duration, visual stimulation duration, TMS, or the vibrotactile pad.
- **Audio has none by your decision** of 2026-11-09 (`OI-AUDIOHW-01`): an 85 dB SPL level ceiling, with no exposure-duration or daily-dose model, because the device cannot know what the wearer hears outside a session.
- **Per day:** nothing counts sessions, pulses or dose across sessions. The safety MCU's charge accumulator is zeroed at each session start. The headset has no RTC backup (`CLAUDE.md` §2.3, §4.5), so it cannot tell that a day has passed across a power cycle. What a person received today is session history, which is UHDR and is held by the app (`CLAUDE.md` §5.1). In Mode 3 (autonomous) there is no app.
- **A general advisory already exists:** the validator warns on a session over 120 minutes, for every modality, with no acknowledgement. The figure has no derivation (`UC-067`).

**Assumed, not established:** that the clinical questions in `docs/human-input/oi-hlim-02-health-or-hardware-limit.md` will leave at least some modalities with no duration limit, so that Q1 needs an answer whatever that review returns.

## 3. Questions

**Q1. A session of a modality that has no defined health limit on duration or dose: what does `SWR-SS-02` do?**
- **A. Nothing.** The rule applies only where a limit is defined. A long session of such a modality runs without a word. Cost: a typo in a duration (the case the rule was written for) is not caught for those modalities.
- **B. A general long-session acknowledgement.** Above one stated duration the app asks for acknowledgement for any modality, saying that no health limit is defined and the session is long. Cost: the threshold is a choice with no health basis (it would have a row in `docs/status/unjustified-choices.md`), and it must not be presented as a health limit.
- **C. Hold the modality.** A modality with no defined health limit does not ship, or does not run beyond a stated length, until one is defined. Cost: it conflicts with `SWR-SS-01` (length alone is not a fault) unless you accept that for the interim, and it holds most of the T1 roster.

**Q2. Audio.** Given your decision that the audio ceiling is a level and not a dose:
- **A. Audio is outside `SWR-SS-02`.** The level ceiling is the whole control.
- **B. Audio falls under whatever Q1 decides** for modalities with no limit.

**Q3. If a per-day limit is adopted for any modality, where is the day's total kept?**
- **A. In the app, from session history.** Cost: the total is UHDR and stays on the user's device, which is where it belongs; it does not hold in Mode 3 or on a second phone, and the acknowledgement is app-side only.
- **B. On the headset, as a count since power-on or within a monotonic window.** Cost: with no RTC backup it is not a calendar day and a power cycle resets it unless it is stored; a stored per-person total on the device needs a ruling on its record (`CLAUDE.md` §5: when in doubt, UHDR).
- **C. No per-day limits for now.** Per-session limits only; daily totals are advice in the instructions for use. Cost: consecutive sessions are unbounded by the device, which is the present state.

## 4. What is not being asked

No figure is proposed, and none may be inferred from this file. Which limits are health limits, and which modalities need one, is the clinical question (`OI-HLIM-02`). No locked decision of `CLAUDE.md` is reopened.

## 5. What to send back

For each of Q1 to Q3: the option, a short rationale (`CLAUDE.md` §18 wants what fails without it and where that traces), any condition that would reopen it, and the date. The answer is recorded in `NP-SW-001` §6.3.6 and `docs/status/completed-decisions.md`, and this file's status line is then set to `ANSWERED`.
