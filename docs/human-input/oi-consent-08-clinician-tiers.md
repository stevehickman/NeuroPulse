# Principal review: OI-CONSENT-08, the clinician tiers and what each one reaches

**Status:** DRAFT, ready to send; not yet sent (no addressee named). Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Software (`OI-CONSENT-08` in `docs/status/pending-decisions.md`; register row `UC-069`)
**To:** The principal, with a clinical adviser (name to be assigned; the item's owner column reads "principal, with clinical advisor input")
**Response needed:** a ruling on each of Q1 to Q5, a sign-off of the two tables (or the changes), and the date.
**Why this is a human decision and not a code one:** a tier set decides what a clinician grant reaches, and so what a user is told a clinician can learn (`CLAUDE.md` §6.1). The tables have never been reviewed as a whole, and the prices have no derivation.

## 1. The question

Which of the 11 UHDR (User Health Data Record) elements should each clinician tier reach, and are the four
tiers and their prices what you want? Today's assignments were written by hand in both apps from the first
consent-engine commit with no recorded derivation, and one element could not be reached by any tier until
2026-10-07.

## 2. What is there today

The tier table (`app/NeurOneShared/Resources/clinician-tiers.json`) and the element table
(`app/NeurOneShared/Resources/uhdr-elements.json`) are the single sources for both apps. A tier reaches every
element whose `minimumTier` ranks at or below it, less any `withheldElements` (none today).

| Tier | Rank | Price (`CLAUDE.md` §6.1, `commercial-model.md`) | Aimed at |
|---|---|---|---|
| Monitor | 0 | $49 per month per patient | Primary care, wellness, coordinators |
| Assess | 1 | $149 per month per patient | Neurologists, psychiatrists |
| Full Clinical | 2 | $299 per month per patient | TMS clinics, neuromodulation programmes |
| Research | 3 | $599 per month per study | Academic trials; elements are IRB-defined per study descriptor, not from this table |

| Element | `minimumTier` | In the library's use cases |
|---|---|---|
| SESSION_TIMESTAMPS, SESSION_DURATION, PROTOCOL_PARAMETERS | Monitor | `adherence_monitoring` and all above |
| EEG_WAVEFORMS, NEUROFEEDBACK_SCORES, PBM_DOSE_LOGS | Assess | `eeg_review` (Assess) and `hrv_outcomes` |
| HRV_TIME_SERIES, PPG_OPTICAL_SIGNAL, CLOSED_LOOP_EVENTS, OUTCOME_LOGS | Full Clinical | `hrv_outcomes` only |
| EYE_STATE_LOGS | Full Clinical | **none** |

**What prompted the review.** `EYE_STATE_LOGS` was marked Full Clinical but was in no tier's set, so no
grant could ever reach it. Nothing in the history says that was a decision. It was added to Full Clinical on
2026-10-07 at the principal's direction, which makes Full Clinical 11 elements and every element reachable.
The existing test (ISC-137) checks that no tier holds an element below its minimum, not that every element
is reachable.

**Three things in the tables worth a second look** (the sender's reading, to be confirmed)
- Raw **EEG waveforms** (brain activity) sit at Assess, below **PPG** and **HRV**, which sit at Full
  Clinical. If sensitivity is the test, that ordering may be the reverse of what the test would give.
- The library has **three use cases**, one per tier. CLOSED_LOOP_EVENTS, HRV_TIME_SERIES, PPG_OPTICAL_SIGNAL and
  OUTCOME_LOGS are reached only through `hrv_outcomes`, and EYE_STATE_LOGS by no use case at all. A
  clinician selects use cases and never elements (`CLAUDE.md` §6.1), so an element no use case names is
  unreachable in practice even though a tier's set contains it.
- The price unit in the tier file is `PATIENT` or `STUDY`; the app renders it as "per month". The monthly
  period is in the commercial model and the locale string, not the table.

## 3. Questions

**Q1. Is each element in the right tier?** Sensitivity is the test (eye state is infrared-derived and near
physiological; HRV and PPG are physiological; EEG is brain activity), not convenience. For each of the 11
elements, say Monitor, Assess, Full Clinical, or withheld from clinician grants entirely.

**Q2. Should tier sets stay derived from `minimumTier`?** The generator does this today, so the two cannot
disagree again. The alternative is a hand-written set per tier, which is what caused the eye-state gap.
`withheldElements` exists for the exceptions.
- **A. Keep derived.**
- **B. Hand-written sets**, with a test that every element is reachable.

**Q3. Do the elements each use case needs match the tier that offers it?** No library use case names eye
state. Should there be a use case for it, or should the element be removed from clinician reach?

**Q4. Should any element be reachable only through a Research study descriptor (§6.3)?** For example,
elements judged too sensitive for a routine clinician grant.

**Q5. Are the prices decided?** They have no derivation (`UC-069`). Note `CLAUDE.md`'s preamble says no new
price is set, and none may be set before `OI-HEXTILE-06` (`OI-COST-10`). Whether that reaches clinician
subscription prices, or only retail device prices, is a question for you. Options: confirm the present
prices as they stand, change them (which depends on that answer), or leave them unreviewed and record that.

## 4. What we need back

1. A ruling on each of Q1 to Q5.
2. For any element that moves tier, the sensitivity rationale in a sentence (`CLAUDE.md` §18).
3. A sign-off of the two tables as they will stand, or the list of edits.
4. The date and who advised.

## 5. What happens next

The outcome is recorded in `docs/status/completed-decisions.md`, the two tables are edited, and
`bun scripts/sync-clinician-tiers.ts` and `bun scripts/sync-uhdr-elements.ts` regenerate the app files.
Any change to a tier set changes what a clinician grant reaches and what a user is told a clinician can
learn, so it also needs the plain-language statement and the tests that cite it reviewed. A grant already
made keeps the use-case IDs it was made from. Nothing in the repository acts as if an answer has been given.
