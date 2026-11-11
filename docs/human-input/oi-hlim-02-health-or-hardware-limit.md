# Clinical question for review: OI-HLIM-02, which limits are health limits, and which modalities need a limit on session length or daily dose

**Status:** DRAFT, ready to send; not yet sent (no addressee named). Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Systems Engineering (`OI-HLIM-02` in `docs/status/pending-decisions.md`; GitHub #563; survey in `docs/reference/health-limits.md`)
**To:** a clinical reviewer, with Risk and Regulatory (names to be assigned)
**Response needed:** a ruling per row of the two tables in §3, with rationale, the reviewer's name and role, and the date.
**Out of scope:** the value of the 150 mC/cm² ceiling (`docs/human-input/oi-charge-06-dc-charge-ceiling.md`), the auricular current ceiling (`oi-vnsclip-09-current-ceiling.md`), the zone boundaries (`oi-zone-01-safety-zones.md`) and the T1 contraindications (`oi-ifu-07-t1-contraindications.md`). Those have their own requests.

## 1. The question

The software rule is: a session beyond a **health** limit runs only after the user acknowledges it; a session beyond a **hardware or system** limit does not run at all (`NP-SW-001` `SWR-SS-02`, `SWR-SS-03`). To apply it we need to know, for each limit the device has, which kind it is, and for each modality whether a limit on session length or cumulative dose (per session, per day) is needed at all.

Plain form: for each limit listed below, is exceeding it a risk to the person, a risk to the equipment, or both? And for each modality with no duration or daily limit, does it need one, and from which source?

## 2. What is known

The full table, with where each figure is stated and enforced, is `docs/reference/health-limits.md` §3 and §4. In short:

- Per-session figures exist for PBM transcranial, 1170 nm, tDCS and HD-tDCS, and cervical VNS. The PBM and 1170 nm figures have no recorded source.
- No per-day limit is enforced for any modality.
- Several modalities have no limit on session length or cumulative dose at all.
- Citations were checked where they could be reached on 2026-11-11 (§6 of the survey). One difference was found: the rat DC figure cited for tDCS (Liebetanz et al. 2009, 52,400 C/m²) is described in a published commentary as an extrapolated zero-lesion intercept for cathodal stimulation, where our design-input document calls it a measured lesion threshold. The 40 µC/cm² per-phase figure could not be confirmed against Shannon 1992 or McCreery 1990 from what was reachable. Both are `OI-HLIM-03`.

**Assumed, not established:** nothing. Where a cell below says "none found", it means a search of the repository found none.

## 3. Questions

**Table A. For each existing limit: health limit, hardware limit, or both?** Answer H (acknowledge and proceed is acceptable), S (never exceed, no acknowledgement), or B (both, in which case say at what level each applies).

| # | Limit | Where it is stated | What the device does today |
|---|---|---|---|
| A1 | PBM transcranial dose per session: 60 J/cm² (660 nm), 60 J/cm² (808 nm), 36 J/cm² (1064 nm) | `NP-FW-PBM1064-001` §6.5 | The hub switches the channel off in mid-session; no acknowledgement, no refusal before the session |
| A2 | 1170 nm dose per session: 60 J/cm² | `NP-SES-1064-001` §7 | Nothing found in code |
| A3 | tDCS and HD-tDCS: 150 mC/cm² per session | `NP-DT-001` DI-SAFE-01 | Safety MCU cuts off; the app asks for acknowledgement above 100 mC/cm² (placeholder) |
| A4 | Charge-balanced channels: 40 µC/cm² per phase | `NP-DT-001` DI-SAFE-01a | Safety MCU cuts off; the app asks for acknowledgement above 10 µC/cm² (placeholder) |
| A5 | Cervical VNS session: 60 to 120 s | `NP-HW-CVNS-001` `REQ-CVNS-04` | The hub refuses a longer session and stops at 120 s. Stated as the predicate's protocol, not as a health limit |
| A6 | Audio level: 85 dB SPL at the ear | `NP-HW-AUDIO-001` `REQ-AUDIO-19` | Refused above it. Decided as a level ceiling with no duration model (principal, 2026-11-09); listed for completeness, not for reopening |
| A7 | Applied-part temperature: 42 °C | `CLAUDE.md` §3 | Safety MCU throttles and cuts off. It caps temperature and has no duration input (`NP-PWRSRC-001` §5) |

**Table B. For each modality with no limit on session length or cumulative dose: is one needed?** Answer N (none needed, with the reason, so that it can be recorded as a decision and not as a gap), L (a limit is needed: name the quantity, per session or per day, and the source), or U (unknown: say what evidence would settle it).

| # | Modality | What exists | Per session | Per day |
|---|---|---|---|---|
| B1 | PBM transcranial | A1 above | Is a session-duration limit needed besides the dose? | None found |
| B2 | PBM intranasal | A duty ceiling only (`OI-NASAL-02`) | None found | None found |
| B3 | EEG neurofeedback, 21-ch qEEG (recording only) | Nothing | Is a limit on continuous wear, contact pressure or gel contact needed? An overnight session is admissible under `SWR-SS-01` | None found |
| B4 | BES / tACS (≤ 1 mA) and clinical tACS (≤ 4 mA) | A4 | None found | None found |
| B5 | tDCS | A3 | Covered by A3 | None enforced. A risk-management line mentions a "3-session daily limit" with no source (`UC-171`) |
| B6 | Auricular VNS, with HRV biofeedback | A4, and a 40 mA stop-gap | None found | None found |
| B7 | Visual stimulation (photic, EMDR, retinal PBM mode) | "IEC 62471 MPE at 50 % of exempt-group threshold"; no exposure time stated | None found | None found |
| B8 | Mode F (NIR retinal, during ordinary wear) | Blocked; the daily limit is a zero placeholder awaiting the RISK-03 opinion (Q-13) | Not applicable | To be set |
| B9 | TMS | Nothing patient-facing (`OI-TMS-03`) | None | None |
| B10 | 1170 nm deep PBM | A2 | As A2 | None found |
| B11 | HD-tDCS | A3, which a normal session cannot meet on the declared electrode (`OI-CHARGE-06` Q2, `OI-TCAP-01`) | Unresolved | None found |
| B12 | Cervical VNS | A5 | As A5 | None found. The predicate's instructions for use are reported to state a daily maximum; we could not read them and take nothing from them |
| B13 | Vibrotactile 40 Hz pad (provisional) | An acceleration range only | None found | None found |

**Q-C. Combination.** Sessions run several modalities at once and people may run several sessions a day. Is there any limit that must be stated across modalities (for example optical dose from the scalp tiles and the nasal probe together, or electrical channels together) and not per modality?

## 4. What each answer costs

- **H** on a limit means the software path of `SWR-SS-02` is built for it: a pre-signing comparison and an acknowledgement. Where the device today cuts off in mid-session (A1), that behaviour changes.
- **S** means refusal before signing with no way through; where the figure has no source it stays in `docs/status/unjustified-choices.md` until one is given.
- **L** on a per-day quantity depends on where the day's total is kept, which is the principal's question (`docs/human-input/oi-hlim-01-rule-where-no-limit-exists.md`).
- **N** is recorded as a decision with its reason, and the row stops being a gap.
- No answer changes a limit by itself: removing or weakening a safety control is an ISO 14971 decision (`CLAUDE.md` §18).

## 5. What to send back

For each row of Tables A and B and for Q-C: the answer, a short rationale with its source (a standard, a paper, a derivation), conditions that would reopen it, and the reviewer's name, role and date. Answers are recorded in the owning documents named in `docs/reference/health-limits.md` and in `docs/status/completed-decisions.md`.
