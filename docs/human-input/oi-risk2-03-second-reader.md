# Second-reader request: OI-RISK2-03 (RETIRED dispositions in NP-RISK-002)

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Quality (`NP-RISK-002` §3, §6 `OI-RISK2-03`)
**To:** A second reader who did **not** write or approve the dispositions. Quality owns the item. No reader is named, and the choice is yours.
**Response needed:** for each row in section 2, **confirm**, **challenge** or **cannot tell**, with the evidence you checked. Then your name, role and date.

## 1. What you are being asked to do

`NP-RISK-002` retired five risks from the old register (`NP-RISK-001`) when the hex-tile architecture
replaced the earlier one. Each retirement rests on one claim: **the mechanism of the hazard no longer
exists in the design.** The item's own warning is the thing to test:

> The failure mode of this document is a hazard retired because its *old description* stopped
> matching, while the hazard itself moved somewhere nobody looked.

For each row, read the claim, open the cited source, and look for **where the hazard could have
gone**. "The part is gone" is not enough. Ask what now does the part's job.

## 2. The five rows

Quotes are shortened. The full rows are in `docs/np_risk_002.md` §3, lines 88–102.

| Risk | The retirement claim | What to check |
|------|----------------------|---------------|
| **RISK-11** (retest hardest) | Fatigue cracking of a flexed FPC over 1,000 module swaps. Retired because `NP-DRV-SHELL-002` §8.2 says the interconnect has **no dynamic-flex path**, so REQ-BR2-02's dynamic set is empty. The static bend is "a different, milder hazard" covered by REQ-BR2-01/03/04 and SH2-DRC-04/05. | Read §8.2 yourself. Is the set really empty, including **strain relief at the clip, the hub cable and any tile-to-tile jumper**? Do REQ-BR2-01/03/04 and SH2-DRC-04/05 exist, and do they cover the static bend? |
| **RISK-15** (retest hardest) | Wrong module in wrong slot. Retired because tiles are type-agnostic with an orientation-only key, and identity is socket (position) × module UID (type). "There is no wrong socket." | Type-agnostic tiles move the hazard from **mechanical** to **identity**. Check that the identity check (UID read, tier gating, `NP_SAFETY_EN_*` grants) exists and fails closed, and that a **wrong orientation** cannot be seated. Also check that `NP-TOOL-ZM-SM-001`'s F-SM-03 smart-module key went away without leaving 1064 nm modules interchangeable with others. |
| **RISK-01** | Wrong connector family (Molex SlimStack). Retired because hex tiles have no connector and a back-face compression pad array gives zero mating cycles (`NP-DRV-SHELL-002` §8.2). | Does a **compression pad array** have its own cycle, wear or contact-resistance hazard over 1,000 swaps? Is it carried in any register (`NP-RISK-003` / `-004`)? |
| **RISK-09** | 0.35 mm pitch connector current rating. "Same basis as RISK-01." | A one-line retirement that leans on RISK-01. If RISK-01 fails the check, this one does too. Check the **pad current rating** at 24 V / 1.04 A per tile (`NP-RISK-003` RISK-02). |
| **RISK-07** | BCR421U 150 mA rating insufficient for 180 mA. Retired because D-3 replaced the shared-FPC linear driver with an on-module driver. The generic hazard "a driver operated beyond its rating" is now inside `OI-HEXTILE-07`. | Open `OI-HEXTILE-07` (closed 2026-09-28) and `NP-HW-HEXTILE-001` D-3. Is "driver beyond rating" **actually stated** there, with a limit and a test, or only gestured at? A hazard moved into a closed item is not carried. |

## 3. Things the reader should know

- **The count was corrected in `NP-RISK-002` Rev 15 (2026-10-05).** The item used to say "nine" RETIRED
  dispositions, a figure from the first draft. §3's table has **five** (RISK-01, -07, -09, -11, -15), and
  §3 itself says the first draft's counts were wrong. **Please still check the count yourself**: parse
  the Disposition column and confirm five.
- **RISK-24 is not retired** (CLOSED-CONFIRMED: the risk was real, and the design changed). It is outside
  the item's scope, but you may note whether it was filed under the right disposition.
- **Retirement by "structurally eliminated" is the claim most easily wrong.** The same document found the
  RISK-25 residual had rested on a control that could not exist (`NP-RISK-002` §4.3.1).
- **Reading, not re-scoring.** You are not asked to rate anything or to decide whether a hazard is
  acceptable. You are asked whether it was right to stop tracking it.

## 4. What counts as a challenge

Mark a row **CHALLENGE** if you find any of these, and say which:

1. The cited section does not say what the row says it says.
2. The mechanism is gone but the **function it served** is now done by something with no hazard entry.
3. The hazard was moved into another item that is closed, or never stated the same limit.
4. The retirement depends on another retired row that fails its own check.
5. The row cites a **superseded** document (`NP-RISK-001` is in `docs/superseded/`) where a current one exists.

## 5. What we need back

1. Per row: **CONFIRM**, **CHALLENGE** or **CANNOT TELL**, with the file and line you checked.
2. For each challenge: the hazard you think moved, and where it should be carried.
3. Your result on the count (five, or not).
4. Reader name, role, date, and a line saying you did not write or approve the dispositions.

## 6. What happens after

Quality records the outcome in `NP-RISK-002` §6 and the decisions log. A challenged row reopens as a
hazard entry in `NP-RISK-003` or `-004`, scored by the Quality Lead (`NP-RM-001` §8.2). Do **not** edit the disposition table to match
your reading. Silent edits to a retired row hide exactly the failure this review looks for.

## References
`docs/np_risk_002.md` §3 (lines 79–119), §6 `OI-RISK2-03` · `docs/np_drv_shell_002.md` §8.2 ·
`docs/np_hw_hextile_001.md` D-3, `OI-HEXTILE-07` · `docs/np_risk_003.md` · `docs/np_risk_004.md` ·
`docs/superseded/README.md` (`NP-RISK-001`)
