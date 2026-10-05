# Counsel engagement request: OI-RISK2-04 (RISK-03, PBM irradiance ceilings)

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Safety / Quality (`NP-RISK-002` §6 `OI-RISK2-04`; `NP-RISK-003` RISK-03)
**To:** CEO (the item's owner), who commissions outside regulatory counsel. No counsel is named.
**Response needed:** a decision to commission counsel (or a stated reason not to), the counsel's name and date engaged, and a go/no-go on the open choices in section 5.
**Nothing here goes to counsel as written.** The brief counsel receives is `NP-REG-PBM1064-001` (`docs/np_reg_pbm1064_001.md`). This file is for the person who decides to send it.

## 1. What is being asked of you

RISK-03 has been **open and externally blocked since 2026-05-06**, and **no counsel has been
commissioned**. It is the only risk in the register whose owner is the CEO. The question is whether to
commission outside regulatory counsel (PBM or digital-health specialist) now, on the scope in section 3.

## 2. Why it matters

The two PBM scalp ceilings have **no regulatory opinion behind them**:

- the **400 mW/cm² peak**, and
- the **average term** Σ Ēᵢ / (200 × C_A(λᵢ)) ≤ 1, adopted **at zero margin** to the laser skin reference
  (CLAUDE.md Rev 60, `NP-HW-HEXTILE-001` D-10).

**Either answer is possible, and they are not symmetric:**

| Counsel says | Effect |
|---|---|
| A **lower** ceiling is required | A pre-signing constant changes. D-10 reverses with no hardware change. Easier electrical spec, but a lower dose claim. |
| The adopted reference is **too high** | The risk is **scalp over-exposure**, not a business risk. With the 25 % duty cap retired (CLAUDE.md Rev 59), one channel may average 200 mW/cm² at 660 nm, about 329 at 808 nm and 400 at 1064 nm. |

`NP-RISK-003` Rev 3 corrects an earlier reading that "any lower ceiling makes every spec easier", which
held only while the duty cap held the average to 100 mW/cm².

**What it blocks:** the irradiance ceiling and the RSET values (`NP-RISK-002` §6). Marketing claims for
irradiance are gated until the opinion arrives (`NP-REG-PBM1064-001` §7).

## 3. Scope counsel would be asked to cover

Already written, in `NP-REG-PBM1064-001` Rev 2 (status DRAFT; "Approved By" names the CEO):

| Questions | Topic |
|---|---|
| Q1–Q12 | 1064 nm module, three-channel aggregate, T2 combined session, FTC substantiation |
| **Q14–Q18** | **The RISK-03 core:** wellness eligibility and "non-heating" (Q14); governing exposure framework for an LED array worn over 10 s (Q15); the 660 nm CW zero margin (Q16); weighted sum of averages versus sum of peaks (Q17); governing temperature limit and skin-type duty (Q18) |

Q14–Q18 are the part this item turns on. The brief says to **add this scope to the existing engagement
(GitHub Issue #5), not open a parallel one**. Because no engagement exists, "the existing instruction"
is a draft only.

## 4. Known gaps in what counsel would receive

- **Q13 is reserved and unwritten.** It would cover Mode F retinal PBM (808–830 nm bilateral). The
  DHF index notes a further revision is still needed (`docs/np_dhf_001.md`). Decide whether to write it
  before sending, or send without it and accept a second round.
- **The C_A constants are unverified** (`OI-BIBPBM-01`), so the average term is not final even if
  counsel agrees with its form.
- **The 42 °C scalp limit may be misattributed.** IEC 60601-1 Table 24 states 43 °C, with labelling
  above 41 °C (`OI-BIBPBM-03`). Counsel is asked in Q18, but the 400 peak at 1064 nm is a hazard
  control that may not be retired while this is open (CLAUDE.md §3).
- **Intranasal PBM is not in RISK-03's scope.** The probe has no exposure ceiling and no temperature
  sensor (`OI-NASAL-02`, blocking). Counsel would not cover it unless it is added.
- **No skin-type term exists in any NeurOne model.** Q18 asks whether one is required.

## 5. Decisions needed

1. **Commission counsel now?** Yes, no, or not until the items in section 4 are closed. Waiting keeps
   the ceilings unreviewed. Sending early risks a second round.
2. **Q13 (Mode F):** write it first, or defer to a second round?
3. **Intranasal scope:** add it to this engagement, or leave it to `OI-NASAL-02`?
4. **Who is the contact for counsel**, and does the CEO or Regulatory Affairs sign the instruction
   letter?
5. **Budget and timing**, if any are fixed. None is recorded in the repo.

## 6. What happens after

On engagement: record counsel and date in `NP-RISK-002` §6 and `NP-RISK-003`, and in the pending-decisions
log. On the opinion letter: apply it through the owning files, and lift the marketing gates
independently (`NP-REG-PBM1064-001` §7). Any change to a ceiling is a constants change before signing,
plus a derivation under CLAUDE.md §18. **Counsel's answer is not a decision until the CEO records it.**

## References
`docs/np_risk_002.md` §6 `OI-RISK2-04` · `docs/np_risk_003.md` RISK-03 · `docs/np_reg_pbm1064_001.md`
§6A, §7, §8 · `docs/np_bib_pbmirr_001.md` · `docs/np_hw_hextile_001.md` D-10
