# Risk question for review: OI-NPPS-VNS-01 (f), right-ear and bilateral auricular VNS

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Software / Safety (`OI-NPPS-VNS-01` in `docs/status/pending-decisions.md`, item (f))
**To:** Risk reviewer, with a clinical adviser (names to be assigned; the item's owner column reads "FW Hub + App + Clinical")
**Response needed:** a ruling (A, B or C below) with rationale, the reviewer's name and role, and the date.

## 1. The question

An NPPS protocol can now author `side: right` or `side: bilateral` on the auricular VNS block
(`vns_hrv`, NP-NPPS-REF-001 Rev 19). Until Rev 19 the compiler always wrote the left ear. Should the
platform accept right and bilateral stimulation, and on what terms?

Plain form: is left-ear-only a hazard control that must stay, or a convention that need not bind us?

## 2. Why we are asking

The principal asked for the study parameters of the stroke taVNS papers to be authorable. Those
papers choose the ear for a stated cardiac reason (Capone 2017: only the left ear was stimulated
"to reduce the risk of cardiac side effects, … vagal fibers to the heart are supposed to originate from
the right side"). Our auricular clip has **no cardiac interlock**; only the cervical device has one
(`CLAUDE.md` §4.2). Today a hand-written `side: right` compiles and runs. Nothing in this repository
records a decision that it should.

## 3. What the device does today

| | Left (default) | Right | Bilateral |
|---|---|---|---|
| Authorable | Yes | Yes, ungated | Yes, ungated |
| Cardiac interlock on the auricular clip | None | None | None |
| Current ceiling | 40 mA (stop-gap, `UC-079`, `OI-VNSCLIP-09`) | same | same |
| Charge ceiling (safety MCU) | 40 µC/cm² per phase | same | same |

An absent `side` is left and is unchanged by any ruling here.

## 4. What the literature says, and how far we have checked it

**Status of this section:** the full texts of the three reviews below and of Hatik 2023 (primary
study) were read on 2026-10-29. **Kraus 2007 could not be read** (the publisher host is outside the
session's network allowance), and neither could Sinkovec 2021, Gauthey 2020 or the other primary
studies; every statement about them is a review's account of them. The repository's own
count of stroke studies by side (see the last bullet) was not re-checked.

**Reviews read in full**
- Matsuoka, Yamaguchi, Fujiwara 2025, *Front Physiol* 16:1693907, narrative review of 154 papers
  (139 healthy participants, 9 stroke, 6 Parkinson's). Left-ear stimulation is used "partly owing to
  theoretical concerns regarding cardiac parasympathetic innervation of the right vagus"; "right-sided
  and bilateral approaches were also reported as safe and effective." **Only 2 studies used right-sided
  taVNS** (Gauthey 2020, Sinkovec 2021). All six Parkinson's studies used the left ear. Adverse events
  were mainly transient ear pain, redness and itching, and no severe adverse effects were observed.
  The review itself warns of a "high rate of non-reporting" and underreporting bias, so "it was unclear
  whether any adverse events had occurred." It cites Kraus 2007 as concluding that bilateral taVNS
  effects were unknown because the heart's parasympathetic supply is from the right vagus, and Hatik
  2023 as reporting no bradycardia or hypotension with bilateral stimulation. It reports Sinkovec 2021
  as finding reduced heart rate, left ventricular contractility and output with right taVNS. It says
  effects "may differ between healthy individuals and patient populations."
- Dalgleish et al. cited in the same review: bradycardia and arrhythmia are concerns of implanted VNS,
  and cardiac side effects of taVNS are rare.
- Safety of taVNS: systematic review and meta-analysis (2022, PMC9772204): 177 studies, 6,322
  subjects. Overall adverse-event incidence 12.84 per 100,000 person-minutes-days, mostly ear pain,
  headache and tingling, with no difference from controls. **71.3% of studies stimulated the left
  side.** The authors say that although few studies used the right or bilateral side, "they carry no
  additional risk," that "bilateral taVNS did not significantly increase the likelihood of
  cardiovascular events," and that no severe cardiac adverse events were associated with taVNS,
  "except for very few patients with underlying heart diseases and recurrent symptoms." They note
  that right-side stimulation more often caused bradycardia in animal studies, and that *cervical*
  bilateral stimulation shows cardiovascular side effects. Limitations they state: most studies did
  not distinguish adverse effect from adverse event, so causality is hard to judge; 55.37% of studies
  did not report adverse events.
- Cardiovascular effects of auricular stimulation, systematic review and meta-analysis of RCTs
  (Front Neurosci 2023, 76 trials, quality "moderate to low"): auricular stimulation reduced heart rate
  against control, MD -1.23, 95% CI -1.74 to -0.72, and "no drop of HR in any of the trials" was
  regarded as an adverse event. Blood pressure, oxygen saturation and baroreflex sensitivity were
  not significantly changed; no serious side effect occurred in any study. The authors say the
  right-versus-left concern comes from direct cervical vagal stimulation in dogs (Ardell and Randall
  1986), and that transferring it to auricular stimulation "is not justified" because the
  neurocircuitry differs.

**Where it is thin, for this device**
- The reassurance on right and bilateral stimulation comes from few studies (2 right-sided in the
  taVNS review), mostly healthy volunteers, at the low currents and short sessions typical of those
  studies. The meta-analyses pool acupressure and other auricular methods with electrical stimulation
  (the cardiovascular review) or rely on poor adverse-event reporting (the safety review).
- Neither meta-analysis reports a controlled comparison of right against left for cardiac outcomes.
- Our ceiling is now 40 mA (`UC-079`). The stroke studies' doses (2.5 to 9 mA, per the repository
  row) are authorable, and no cited source tests right or bilateral stimulation at those doses in
  people with cardiac disease.
- The auricular clip has no cardiac interlock. In the literature above, the safety evidence is
  observational and relies on the absence of reports.

**Not checked**
- The row records "the eight stroke studies that state a side used left, one bilateral, none right".
  The counts are ambiguous as written, and the Matsuoka laterality table was not located in the text
  read. Re-check against the paper.
- Kraus 2007 (see above), Sinkovec 2021 and Gauthey 2020, the only two right-sided studies in
  Matsuoka's review.
- The registered trial NCT05370027 (left, right and bilateral in healthy adults) has no results in
  anything read.

**Primary study read: Hatik, Asrlan, Demirbilek, Ozden 2023, *Brain Behav* 13:e3332** (the paper the
repository row calls "Hatik 2023", a study of cycling recovery in healthy young adults, not a cardiac
safety study):
- 90 healthy adults aged 18 to 23, randomised to bilateral sham, unilateral left, or bilateral
  stimulation (30 per group), 20 minutes a day for 4 days, at the cymba concha and tragus, 10 Hz,
  biphasic, 300 µs, with the current set at each person's detection threshold (no mA figure is stated).
- Pulse and systolic and diastolic blood pressure were measured as side-effect monitors. The authors
  "did not encounter bradycardia and hypotension in any group" and "VNS (left or bilateral) did not
  cause bradycardia or hypotension." Seen as a safety result, this is a monitoring note, not a
  designed cardiac endpoint, with no stated power for it.
- **Exclusions included "existing cardiac system disease history and receiving treatment."** So it
  says nothing about people with heart disease. The authors' limitations: short follow-up, and a real
  sham may not have been fully formed.
- The authors argue right and left ear stimulation are cardiac-safe because auricular afferents end
  at the same brainstem nuclei (citing Chen 2015 and Murray 2016). This is their reasoning, not a
  finding of their trial.
- Matsuoka's other use of this paper, that bilateral stimulation caused "significantly less
  stimulation-related pain," matches the abstract (pain and fatigue improved with bilateral).

**Kraus 2007 (not read).** The reference list of Matsuoka 2025 gives it as "BOLD fMRI deactivation of
limbic and temporal brain structures and mood enhancing effect by transcutaneous vagus nerve
stimulation," *J Neural Transm* 114:1485, an fMRI mood study. Matsuoka cites it for the statement that
bilateral effects were unknown because the heart's parasympathetic supply is from the right vagus.
Nothing in the title suggests it was a cardiac or bilateral safety study, so the attribution should
be checked at the source before it is repeated.

## 5. Options and what each costs

**A. Refuse right and bilateral** until a cardiac mitigation exists. Validator and compiler change;
two stroke-literature configurations cannot be authored. Safest; rests on mechanism, not observed harm.

**B. Allow with a warning the clinician acknowledges.** This would reuse the zone model's
acknowledgement path (`OI-ZONE-01`), which is built but not run on a device and whose boundaries are
placeholders. New caution case, text and tests. Protocols stay authorable by a person who has been told.

**C. Allow ungated.** No change beyond removing the open item. Rests on healthy-volunteer evidence at
low current, which this device does not bound.

If the reviewer prefers a mix (for example, B for bilateral and A for right), say so.

## 6. What we need back

1. Ruling: A, B, C or the mix.
2. The rationale in two or three sentences, recorded as the derivation (`CLAUDE.md` §18).
3. Whether a ruling on right ear depends on current (for example, allowed up to a stated amplitude).
4. Conditions that would reopen it, such as the NCT05370027 result or a cardiac-history exclusion.
5. Reviewer name, role and date.

## 7. What happens next

The ruling is recorded in `docs/status/pending-decisions.md` (`OI-NPPS-VNS-01` (f)), logged in
`docs/status/completed-decisions.md`, and applied to the validator, compiler and `NP-NPPS-REF-001`.
Nothing in the repository acts as if an answer has been given.
