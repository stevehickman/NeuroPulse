# Owner request: OI-RISK2-02 (commission the hazard analysis for A11, A12, A13 and A15)

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Quality (`NP-RISK-002` §4, §4.1, §6 `OI-RISK2-02`)
**To:** Quality and Systems owners (the item's owner column reads "Quality + Systems"). No names are assigned.
**Response needed:** the decisions in section 4, named owners, and a date for the first scored draft.
**This is a request to do work, not a question with a yes or no answer.** Nobody has performed the analysis. The specifications that would feed it now exist.

## 1. What is owed

Three T1 modalities that ship have **never had a hazard analysis in any revision of the risk file**:

| Artifact | Modality | Owning specification |
|----------|----------|----------------------|
| A11 | Neural audio entrainment (audio cup) | `NP-HW-AUDIO-001` |
| A12 | Intranasal PBM (Y-probe) | `NP-HW-NASAL-001` |
| A13 | Auricular VNS and HRV (clip) | `NP-HW-VNSCLIP-001` |
| A15 | TMS coil (**added** to the item) | `NP-HW-TMS-001` |

A15 has no register entry of any kind. Its only two hazard rows, `RISK-PWRTH-01` and `-03`, sit in a
thermal study (`NP-PWR-THERM-001` §15), not in the risk file.

`NP-RISK-002` §4 calls this **"a gap in the ISO 14971 file, not merely in the documentation."**
**Blocking:** ISO 14971 completeness and T1 release.

A14 (cervical VNS) is **not** in scope. RISK-25 covers it.

## 2. Where it stands

The old block ("no specification to analyse") was lifted on 2026-09-20 (GitHub #332). Each
specification now carries an **unscored** §7 input list. The item stays open because
**a hazard analysis is an act, not a consequence of a document existing** (`NP-RISK-002` §4.1).

| Artifact | Spec status | Candidate hazards listed in §7 |
|----------|-------------|-------------------------------|
| A11 | DRAFT, Rev 4 | 9 live (one more is retired in place, option A not taken) |
| A12 | DRAFT, Rev 2 | 9 |
| A13 | DRAFT, Rev 1 | 8 |
| A15 | DRAFT, Rev 1, **gated** on `OI-PWRTH-01` and `OI-PWR-02` | 8 |

The specifications are **requirements-grade drafts**. Every one of them says which values are absent
instead of inventing one. That matters to the analysis, because the inputs a scored analysis needs are
often exactly the missing ones:

| Missing input | Bears on | Item |
|---------------|----------|------|
| No SPL ceiling for planar or bone-conduction audio | A11 overexposure (two hazards) | `OI-AUDIOHW-01`, `-02` |
| No exposure ceiling and **no temperature sensor** in the intranasal probe | A12 mucosal injury | `OI-NASAL-02` (BLOCKING) |
| Unmeasured **provisional** electrode areas (0.5 cm² clip) behind the per-phase charge ceiling | A13 charge density | `OI-CHARGE-07` |
| Coil temperature: no verification defined | A15 scalp contact above 42 °C | `OI-PWRTH-01` (BLOCKING) |
| No material of record for any skin- or mucosa-contacting part | A11, A12, A13 biocompatibility | specs §8 |

## 3. What is known about cross-artifact links

- **A14's cardiac interlock takes its R-peaks from A13's PPG sensor** (`OI-CVNSHW-03`). That is a
  dependency stated in no device description or IFU. An A13 analysis that scores the clip alone would
  miss the one A13 failure that reaches RISK-25.
- `NP-ART-001` showed A13's risk-register column as covered (✅) when the cited row said the opposite.
  It was corrected in that register's Rev 3. **Do not read a pointer to §4 as coverage.**
- The `NP-RISK-002` §3 note on disposition versus rating applies: **list, then score, as separate acts**
  (`OI-RISK2-01`).

## 4. Decisions needed

1. **Who performs the analysis, and who reviews it?** One named owner each for Quality and Systems.
   The reviewer should not be the author.
2. **Where do the entries live?** Options: new rows in `NP-RISK-002` §4, or per-artifact registers like
   `NP-RISK-003` and `-004`. This file does not say which, and I have not proposed a document ID.
3. **Scope.** Confirm A15 stays in. Confirm A14 stays out, apart from the A13-to-A14 link in section 3.
4. **Order.** All four at once, or the highest-severity first? The inputs with no ceiling
   (A12 mucosal exposure, A11 acoustic overexposure) are the likeliest to be severe. This is a
   suggestion for you to correct, not a finding.
5. **Do you analyse against DRAFT specs?** Scoring a hazard whose controlling limit does not exist yet
   gives a provisional score. Choose between: (a) score now and mark every unset limit as an
   open item, or (b) hold each artifact until its blocking item closes. Option (a) gets entries into the
   file sooner. Option (b) avoids scoring twice.
6. **Date** for the first scored draft, if one is fixed. None is recorded in the repo.

## 5. What this does not decide

- It does not score or dispose of any hazard. Nothing here changes a rating.
- It does not set any limit. SPL, mucosal exposure and electrode area belong to their own items.
- Completing the analysis does not make the spec drafts final.
- It does not replace the T1 release decision. It removes one blocker to it.

## References
`docs/np_risk_002.md` §4, §4.1, §6 `OI-RISK2-02`, §8 Rev 3 · `docs/np_hw_audio_001.md` §7 ·
`docs/np_hw_nasal_001.md` §7 · `docs/np_hw_vnsclip_001.md` §7 · `docs/np_hw_tms_001.md` §7 ·
`docs/np_art_001.md` §2.4 · `docs/np_pwr_therm_001.md` §15
