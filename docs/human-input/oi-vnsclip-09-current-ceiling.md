# Clinical and risk question for review: OI-VNSCLIP-09, does the auricular VNS clip need a current ceiling?

**Status:** DRAFT, ready to send; not yet sent (no addressee named). Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Software / Safety (`OI-VNSCLIP-09` in `docs/status/pending-decisions.md`; register rows `UC-072` (retired) and `UC-079`; GitHub #554 and #559)
**To:** Clinical reviewer and Risk reviewer (names to be assigned; the item's owner column reads "Clinical + Risk")
**Response needed:** a ruling on each of Q1 to Q4, with rationale, the reviewer's name and role, and the date.
**Related:** `oi-vnsclip-08-frequency-ceiling.md` (the 25 Hz ceiling, answered by the principal on 2026-10-29). It asked the same question of the frequency, and its answer does not decide this one.

## 1. The question

The auricular VNS clip has a **40 mA current ceiling**. It is a stop-gap the principal set on 2026-10-08 in place
of a 2 mA ceiling that was removed, and it is derived only from the 40 µC/cm² per-phase charge limit. Does the clip
need a current ceiling at all, and if so, in what quantity, at what level, and enforced where?

Plain form: what bad thing happens at 10 mA, 20 mA or 40 mA through this clip that does not happen at 2 mA?

## 2. Why we are asking

`CLAUDE.md` §18 allows a figure to stand only if something fails without it and the failure is traceable. The 2 mA
ceiling had no derivation and every stroke taVNS dose in the three papers read exceeds it, so it was removed (Rev
67). The 40 mA figure that replaced it (Rev 68) is internal arithmetic, not a clinical, sensory, heating or cardiac
limit. **Removing a safety control is an ISO 14971 decision (`CLAUDE.md` §18), and Risk has not yet recorded one.**

## 3. What the device does today

| | |
|---|---|
| Current ceiling | 40 mA: 40 µC/cm² × 0.5 cm² ÷ 500 µs, the strictest current the per-phase limit permits at any allowed pulse width (`UC-079`) |
| The pad area behind it | 0.5 cm², **provisional and unmeasured** (`UC-006`). The figure scales with the real area; a smaller pad lowers it in proportion |
| Safety MCU | enforces only the 40 µC/cm² per-phase charge limit. It has no current, frequency or duty term |
| At 250 µs on that pad | the charge limit alone would permit about 80 mA, and at 50 µs about 400 mA, so 40 mA is the one figure that holds at every authored width |
| App-side zone model | advisory and placeholder (`UC-073` to `UC-077`): it refuses a danger-zone protocol at compile time and asks for an acknowledgement in the caution zone, through RMS and mean current density, not through peak current |
| Cardiac interlock | none on the auricular clip |
| The driver's real compliance limit | not stated in any document |

## 4. What is known

**Doses in the stroke taVNS studies** (as recorded in the open item)
- Capone 2017: individual levels spanned 1.2 to 9.0 mA for real stimulation, titrated "above the detection
  threshold and below the pain threshold"; the means read were 2.5 to 3.2 mA.
- Chang 2021: 0.1 to 5.0 mA set just below pain threshold, mean tolerated 4.5 mA.
- So **40 mA is about 4.4 times the highest Capone level and 8 times the highest Chang level.** No study read
  approaches it.

**From the literature read for this item** (full texts of the reviews; abstracts where marked)
- Matsuoka 2025 (narrative review, 154 papers): the most common amplitude basis is "below the pain threshold".
  Adverse events were mainly transient ear pain, redness and itching, with no severe adverse effects. It says
  the relationship between parameters and adverse events "remains unknown".
- The 2022 taVNS safety meta-analysis (177 studies): the most reported events were ear pain, headache and
  tingling; it found no difference in adverse-event risk against controls. Causality was hard to judge and
  55% of studies did not report adverse events.
- Gauthey 2020 (right cymba, 28 healthy men): mean current 1.5 and 1.2 mA at the cymba and 5.5 mA at the ear
  lobe control, set by sensation.
- A 2022 healthy-adult study (abstract only) used 3.0 mA at the left cymba, with 1.0 and 0.2 mA arms.
- gammaCore, the cervical predicate, delivers up to 24 mA (K163334). That is a different electrode and nerve site.

**What this suggests, and what it does not show.** In every study read, the working current was set by the
person's own sensation, below pain. The sender's reading is that the practical limit in the literature is
sensory, and that the 40 mA figure is far above anything studied. That does not show 40 mA is unsafe, and it
does not show it is safe: none of these studies tested currents near it, and none measured skin heating at the
electrode.

**What no source states.** A current at which this clip is hazardous, or the mechanism (pain, skin heating
under the pad, a burn, or a cardiac effect, `OI-NPPS-VNS-01` (f)). No document records which hazard a ceiling
here would be for.

## 5. Questions

**Q1. Does the clip need a current ceiling?**
- **A. No.** The dose is bounded by the per-phase charge limit, by titration to the person's own sensory
  threshold, and by the app-side zone model. Record the Risk decision to rely on those. The 40 mA stop-gap is
  removed.
- **B. Yes, as a hazard control.** Name the hazard and the mechanism, then state the limit in the quantity that
  is hazardous (see Q2). The 40 mA stands until that exists.
- **C. Not yet decided.** The 40 mA stop-gap stays, labelled provisional, until the pad area is measured
  (`OI-CHARGE-07`) and a hazard is named. This is the present state.

**Q2. If yes, in which quantity?** Peak current (mA); current density at the pad (mA/cm²), which depends on the
real pad area; mean rectified current or RMS current density, which bring pulse width and frequency in and give
a heating-type limit; or a combination. GitHub #559 asks whether the cap should depend on pulse width and
frequency and lists the design details.

**Q3. Where is it enforced?** On the safety MCU (Class C, so a requirement, a test and a Class C change) or in
the app and compiler only. The zone model is app-side today and advisory.

**Q4. What is acceptable in the meantime?** Is the 40 mA stop-gap acceptable for any session that could run
before this is decided, and should any stroke-study dose above a stated level need the zone model's
acknowledgement?

## 6. What we need back

1. A ruling on Q1 to Q4.
2. The rationale in two or three sentences, recorded as the derivation (`CLAUDE.md` §18). Under B, the hazard
   and mechanism, and who must supply a measurement of the real pad.
3. Conditions that would reopen it (for example a measured pad area, a skin-heating measurement, or a clinical
   adviser's view on titration).
4. Reviewer names, roles and dates.

## 7. What happens next

The ruling is recorded in `docs/status/pending-decisions.md` (`OI-VNSCLIP-09`) and
`docs/status/completed-decisions.md`, and `UC-079` is moved or updated. Under A or B, `CLAUDE.md` §3 and
`REQ-VNSC-03` change in a separate revision. Nothing in the repository acts as if an answer has been given.
