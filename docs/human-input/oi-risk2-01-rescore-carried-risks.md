# Owner request: OI-RISK2-01 (re-score the carried risks at the new architecture's numbers)

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Quality (`NP-RISK-002` §2, §6 `OI-RISK2-01`)
**To:** Quality and Mechanical Engineering owners (the item's owner column reads "Quality + ME"). The Quality Lead approves any re-score (`NP-RM-001` §8.2).
**Response needed:** the decisions in section 4, named owners, and a date for the first scored batch.
**This is a request to do work.** Nothing here proposes a score. Scoring is the Quality Lead's act, and I have not made any.

## 1. What is owed

`NP-RISK-002` re-baselined the risk file after the hex-tile change and **deliberately re-scored
nothing**, because disposition and re-scoring are separate acts and mixing them hides which judgements
changed. The re-scoring was left as this item:

> Re-score the carried risks against the `NP-RM-001` §4 severity × probability scales **at the new
> architecture's numbers.** RISK-16 (5 → ~30 seals) and RISK-22 (≤ 1 N lever → 34.2–57.0 N plate) are
> the two whose scores most plainly no longer hold.

**Blocking:** ISO 14971 file currency.

## 2. Where it stands

Twenty risks are carried. Ten sit in `NP-RISK-003` (tile), nine in `NP-RISK-004` (shell, socket, hub),
and RISK-25 is held in `NP-RISK-002` §4.

| Group | Risks | State |
|-------|-------|-------|
| **Already scored on S × P** | RISK-25 | Re-scored 2026-09-23 and again 2026-10-01 (`NP-RISK-002` §4.3, §4.3.5). Residual S5 × P2 = ALARP. **Not part of this item.** |
| **Re-scored on its own item** | `RISK-SHELL-03` (new, not carried) | CRITICAL to HIGH, 2026-09-25 (`NP-RISK-004` §2.3, `OI-RISK4-06` closed) |
| **Split out, still open** | RISK-16 → `OI-RISK3-04` · RISK-03 → `OI-RISK3-06` · RISK-22 → `OI-RISK4-03` (accessibility question) | Each has its own item, so they should not wait for the rest. Confirm that split is right. |
| **Everything else carried** | RISK-02, -04, -05, -06, -08, -14, -19, -23 (tile) · RISK-10, -12, -13, -17, -18, -20, -21, -26 (shell, socket, hub) | Carried at the old label with no S × P re-score |

**What the registers show.** The rows I read in `NP-RISK-003` and `-004` carry a one-word label (HIGH or
MEDIUM), not the S × P and acceptability result that `NP-RM-001` §4.3 produces. A label is not a
rating. I did not check whether any row elsewhere holds an S × P pair, so confirm before assuming none do.

**Why the label can mislead.** Several carried risks changed mechanism, not just number:
- RISK-16: perimeter seals rose from 5 to about 30, and the severity was raised LOW to HIGH on that
  reasoning (`NP-RISK-003` §1.3). That was the one deliberate exception. It is "carried at HIGH", not scored.
- RISK-22: the lever (≤ 1 N at the tip) is gone, and the plate load is 34.2–57.0 N. The old basis does
  not apply, and there is no equivalent number yet.
- RISK-08: its V_f-binning control was written against emitters that `OI-HEXTILE-02` has since
  un-selected, so the mitigation **regressed**.
- RISK-03: scope widened (duty cap retired, CLAUDE.md Rev 59–60), so a "lower approved ceiling is a
  business risk" reading no longer holds (`NP-RISK-003` Rev 3).

## 3. How the diff earned its cost (use it again)

The re-baseline compared labels mechanically against the old `.docx` and **caught three silent
re-scores** (RISK-18 and -21 drifted HIGH to MEDIUM, RISK-22 MEDIUM to HIGH) during transcription.
`NP-CONV-001` §8 says cross-document agreement is verified by mechanical diff, never by review. A
re-score batch should be diffed the same way against the register it changes, so a changed rating is
one somebody chose.

Also, `NP-RISK-002` §2 still says "**Nine** risks lose their referent". The §3 table has five
(`OI-RISK2-03` has the same stale count). That sentence is worth fixing in the same revision.

## 4. Decisions needed

1. **Scope.** All 19 remaining carried risks, or only those whose basis changed? "Basis unchanged" is a
   claim that needs the same evidence as a retirement, so it should be checked, not assumed.
2. **Order.** Suggested: the ones whose control or mechanism changed first (RISK-08, -16, -22, -03),
   then the rest. This is a suggestion for you to correct.
3. **Who scores and who approves.** `NP-RM-001` §8.2 makes the re-score the Quality Lead's act. Name
   the scorer (Quality + ME) and the approver. The approver should not be the only person who read
   the evidence.
4. **Inputs.** Several risks cannot be scored on S × P without a missing figure: the RISK-16 seam
   budget (`OI-RISK3-03`), the RISK-22 grip-force result (`OI-SHELL2-03(b)`, HFE formative), the RISK-03
   counsel opinion, the RISK-20 supplier confirmation. Score now and mark the basis provisional, or wait
   for the input? The RISK-25 re-scores used "provisional" and kept a P1 target.
5. **Record form.** Add S, P and the §4.3 result as columns in `NP-RISK-003` and `-004`, or keep the
   label and add a table. Either needs a revision of both registers.
6. **Date** for the first scored batch, if any is fixed. None is recorded in the repo.

## 5. What this does not decide

- It does not score any risk or change any rating.
- It does not replace the per-risk items listed above. They stay open until each closes.
- It does not review dispositions (that is `OI-RISK2-03`) or add missing analyses (`OI-RISK2-02`).

## References
`docs/np_risk_002.md` §2, §3, §6 `OI-RISK2-01` · `docs/np_risk_003.md` §1.3, §4 `OI-RISK3-03`, `-04`,
`-06` · `docs/np_risk_004.md` §2.3, `OI-RISK4-03` · `docs/np_rm_001.md` §4, §8.2 ·
`docs/np_conv_001.md` §8
