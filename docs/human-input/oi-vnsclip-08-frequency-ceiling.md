# Clinical and risk question for review: OI-VNSCLIP-08, the 25 Hz auricular VNS ceiling

**Status:** DRAFT, ready to send; not yet sent (no addressee named). Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Software / Safety (`OI-VNSCLIP-08` in `docs/status/pending-decisions.md`; register row `UC-070`)
**To:** Clinical reviewer and Risk reviewer (names to be assigned; the item's owner column reads "Clinical + Risk")
**Response needed:** a ruling (A, B or C below) with rationale, the reviewer's name and role, and the date.
**Related:** `OI-NPPS-LIMITS-01` (`oi-npps-limits-01-stroke-protocol-30hz.md`) cannot be answered until this is.

## 1. The question

Auricular VNS frequency is capped at 25 Hz (`CLAUDE.md` §3, hard-limits table; `REQ-VNSC-03`). Is that
number a **hazard control** (it prevents a named harm) or an **equivalence bound** (it was carried over
from a predicate device and nothing here depends on it for safety)?

Plain form: what bad thing happens at 26 Hz or 30 Hz through this clip that does not happen at 25 Hz?

## 2. Why we are asking

`CLAUDE.md` §18 allows a figure to stay in a controlled document only if something fails without it
and the failure is traceable. Nothing recorded does either for this figure. `§3` calls it a hard limit,
the validators reject anything above it, and the safety MCU would never grant the channel. A shipped
stroke-rehab protocol authors 30 Hz and is blocked by it (`OI-NPPS-LIMITS-01`).

## 3. What the device does today

| | |
|---|---|
| Frequency range | 1–25 Hz (wire field `freq_mhz` 1000–25000) |
| Charge ceiling on this channel | 40 µC/cm² per phase. It is amplitude × phase width and **contains no frequency term** |
| Safety MCU charge monitor | has no frequency, duty or average-current term |
| Current ceiling | 40 mA, a stop-gap derived from the charge limit on a provisional 0.5 cm² pad (`UC-079`, `OI-VNSCLIP-09`) |
| Cardiac interlock on the auricular clip | none |

## 4. What is known

**From the repository.** The only traceable origin of 25 Hz is the cervical device: `NP-REG-CVNS-001` §3
takes it from the gammaCore predicate (K163334, fixed 25 Hz), a substantial-equivalence argument, not a
safety derivation. No document says it was carried over to the auricular clip. For the worst rated case
the mean rectified current is 25.5 µA at 25 Hz and 30.6 µA at 30 Hz, and nothing in the repository
relates either figure to a hazard.

**From the literature** (checked 2026-10-29 unless noted):
- Cogan, Ludwig, Welle, Takmakov 2016, *J Neural Eng* 13:021001 (read in full 2026-10-07, per the
  open item): frequency is a co-factor in tissue damage that the Shannon equation does not capture
  ("lower frequency stimulation invariably resulted in reduced nerve damage"). The evidence is from
  electrodes on nerve and cortex. It gives **no frequency boundary** and says nothing about skin.
- Matsuoka, Yamaguchi, Fujiwara 2025, *Front Physiol* 16:1693907 (full text read): 25 Hz is the most
  common taVNS frequency, offered only as a "starting point"; 30 Hz was used in 11 studies and 100 Hz in
  9 of the healthy-participant counts. It reports no serious adverse event and says the relationship
  between parameters and adverse events "remains unknown."
- Safety meta-analysis of taVNS, 2022 (177 studies, full text read): "no standardized stimulation
  parameters elicit optimum efficacy and safety"; it reports no frequency-dependent adverse-event
  finding.
- Brain Sci 2022 (35 healthy adults, left cymba, 250 µs; **abstract only**): 100, 25, 10 and 1 Hz at
  3.0 mA all reduced heart rate, with 100 Hz the largest effect. A physiological effect, not an
  adverse-event comparison.
- Biomedicines 2025 (78 healthy adults, 10 and 25 Hz, 100–500 µs; **abstract only**): the authors
  conclude taVNS is safe in healthy adults. It tested nothing above 25 Hz.
- Chang 2021 (30 Hz, bursts of 500 ms, 0.1–5.0 mA, 36 chronic-stroke patients): no serious adverse
  events; it gives no rationale for 30 Hz and no frequency-dependent safety discussion.

**What no source states.** A safety or efficacy reason for stopping at 25 Hz. A frequency-dependent
skin or tissue boundary for surface electrodes. I found no study that measures harm at 30 Hz or above
through skin electrodes, and the ones above were not designed to.

## 5. Options and what each costs

**A. It is an equivalence bound.** Record that, and move 25 Hz out of the `CLAUDE.md` §3 hard-limits
table (a locked-section change, so the principal's). Validators and the MCU keep or drop it as a
protocol-level limit afterwards. Unblocks the 30 Hz stroke protocol (`OI-NPPS-LIMITS-01`). Cost: the
platform no longer claims a hard frequency limit and relies on the 40 µC/cm² per-phase limit and the
interlocks that already exist.

**B. It is a hazard control.** Name the hazard and mechanism (for example mean current or skin heating
at the pad), then state the limit in that quantity, since frequency alone does not bound it. This needs
a derivation or a measurement on the real pad (area is provisional, `UC-006`). The 25 Hz stays until
that exists. Cost: new engineering work, and possibly a mean-current term in the MCU charge monitor (Class C).

**C. Keep 25 Hz as a provisional precaution** with no claim either way, and revisit when a pad
measurement exists. No change; the stroke protocol stays blocked. Cost: the gap remains open and `UC-070`
stays in the unjustified-choices register.

## 6. What we need back

1. Ruling: A, B or C.
2. The rationale in two or three sentences, recorded as the derivation (`CLAUDE.md` §18). Under B, the
   hazard and mechanism.
3. Conditions that would reopen the ruling (for example, a skin-heating measurement or a published
   tolerability comparison above 25 Hz).
4. Reviewer name, role and date.

## 7. What happens next

The ruling is recorded in `docs/status/pending-decisions.md` (`OI-VNSCLIP-08`), logged in
`docs/status/completed-decisions.md`, and applied to the owning documents and `UC-070`. Nothing in the
repository acts as if an answer has been given.
