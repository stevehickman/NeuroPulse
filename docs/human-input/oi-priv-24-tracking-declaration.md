# Question for the IRB adviser: OI-PRIV-24, a study that declares tracking over time

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** none yet.
**From:** NeurOne Software Engineering (`OI-PRIV-24` in `docs/status/pending-decisions.md`; the principal's ruling of 2026-11-20)
**To:** the IRB adviser (unnamed)
**Response needed:** an answer to Q1 and Q2, a short rationale, conditions that would reopen it, and your name, role and the date.

## 1. Background

Research extracts are made on the participant's device and anonymised there (k at least 10, dates rounded to at least a week) before anything is sent. By default an extract carries no identifier and a study asks once. The principal ruled that a study may declare, up front in its signed descriptor and for good, that it needs to follow participants over time (for example a protocol changed after a first tranche). Such a study's extracts carry an identifier unique to the person and that study, never the same across studies, derived on the device. The person is told plainly that their releases can be linked and consents to that study. k-anonymity is applied to the linked series.

`NP-IRB-001` §5 bases the 45 CFR 46.104(d)(4) exemption for Type B studies on the data being de-identified before NeurOne or the researcher receives it. A coded identifier is not de-identified.

## 2. Questions

**Q1. What must a researcher show to declare tracking?** Options: (a) an IRB approval that names the tracking; (b) a written justification reviewed by NeurOne; (c) both. Cost of each: (a) puts the check on the researcher's IRB, (b) puts a review on NeurOne, (c) both.

**Q2. Does a tracking study stay inside the Type B exemption argument?** Options: (a) yes, because the identifier is unique per study, derived by the person's device, and meaningless outside that study; (b) no, tracking studies are a separate type with full review and their own consent text; (c) depends on a stated condition (name it).

## 3. What is known and assumed

Known: the declaration is fixed for a study; the identifier is not shared across studies; withdrawal stops future releases but cannot unlink those already sent (disclosed at consent). Assumed: that researchers hold the identifier and NeurOne's research infrastructure may or may not see it (a separate ruling).

## 4. What I need back

1. Q1 and Q2 answers. 2. A short rationale (`CLAUDE.md` §18). 3. A condition that would reopen it. 4. Your name, role and the date.
