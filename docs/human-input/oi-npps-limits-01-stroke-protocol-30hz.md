# Clinical question for review: OI-NPPS-LIMITS-01, the shipped stroke protocol authors 30 Hz

**Status:** DRAFT, ready to send; not yet sent (no addressee named). Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Software (`OI-NPPS-LIMITS-01` in `docs/status/pending-decisions.md`)
**To:** Clinical reviewer (name to be assigned; the item's owner column reads "Clinical")
**Response needed:** a ruling (A, B or C below) with rationale, the reviewer's name and role, and the date.
**Depends on:** `OI-VNSCLIP-08` (`oi-vnsclip-08-frequency-ceiling.md`). Evidence on the 25 Hz ceiling is there and is not repeated here. Option C below is only available if that item rules the ceiling an equivalence bound.

## 1. The question

The shipped protocol `clinical-21-vns-stroke-rehab.npps`, "taVNS — Stroke Motor Rehab (paired)", authors
`frequency: 30Hz` on the auricular VNS block. The hardware limit is 25 Hz, so the protocol cannot run.
What should the protocol be?

## 2. Why we are asking

The 30 Hz came from **Dawson 2021 (VNS-REHAB)**, which used the **implanted Vivistim** device on the
cervical vagus, not transcutaneous auricular stimulation. The protocol itself calls its version a
"non-invasive analogue." A source trial's parameter was copied across a change of device. We did not
change the number to 25 Hz, because writing a number into a cited protocol that the citation does not
support makes the check pass and the document wrong.

## 3. What is known

- The auricular stroke studies in Matsuoka 2025 (9 studies): 20 Hz in four, 25 Hz in four, 30 Hz in one.
  The 30 Hz study is Chang 2021 (36 chronic-stroke patients, left cymba, 500 ms bursts at 30 Hz,
  0.3 ms, 0.1–5.0 mA, triggered by a movement cue, 9 sessions of 256 stimulations). Active and sham did
  not differ on Fugl-Meyer. Capone 2017 used 20 Hz, 0.3 ms, 30 s trains every 5 min (a 10 % duty), at
  currents titrated above detection and below pain (up to 9 mA at the extremes).
  The Chang 2021 discussion attributes "30 Hz, 0.8 mA" to Capone; that attribution is wrong.
- The shipped protocol runs continuous 30 Hz for 30 minutes. Every cited auricular stroke study
  delivers bursts or trains, most of them paired with movement. The auricular VNS block now has fields
  for pulse width, burst and trigger, but the movement-cue trigger is refused by the validator and
  compiler until the hub has a cue input (`OI-NPPS-VNS-01` (a)).
- No source found gives a safety or efficacy reason for 25 Hz. See `OI-VNSCLIP-08`.
- Until this is decided the protocol is waived **by name** in `npps-predefined.test.ts`, which lists
  the exact failure string, so fixing the protocol or breaking another one makes the test fail.

## 4. Options and what each costs

**A. Re-cite the protocol to auricular evidence at or below 25 Hz** (for example Capone 2017 at
20 Hz, or the four 25 Hz studies) and set the frequency to match. Cost: the protocol's pattern should
follow the cited study too (trains or movement-paired bursts, not continuous 30 Hz), and the
description and citation change; the reviewer must say which study the protocol should follow.

**B. Keep 30 Hz and withdraw the protocol from the shipped library** until the ceiling is revisited.
Cost: no stroke protocol ships; the waiver goes; the protocol returns only after `OI-VNSCLIP-08` is
ruled and a replacement is authored.

**C. Establish that 25 Hz is therapeutically adequate here and keep the protocol at 25 Hz**, recording
that as the reason for the change. Cost: a clinical statement the reviewer must be willing to stand
behind, with evidence that goes beyond "the studies mostly used 20–25 Hz." If the ceiling is later
found to be an equivalence bound and moved, this option's premise changes.

If the reviewer prefers to wait for the `OI-VNSCLIP-08` ruling before choosing, say so; B is the holding
position.

## 5. What we need back

1. Ruling: A, B or C (or "wait for `OI-VNSCLIP-08`").
2. Under A, which study the protocol follows. Under C, the clinical rationale in two or three
   sentences.
3. Conditions that would reopen it.
4. Reviewer name, role and date.

## 6. What happens next

The ruling is recorded in `docs/status/pending-decisions.md` (`OI-NPPS-LIMITS-01`), logged in
`docs/status/completed-decisions.md`, and the protocol file, its citation and the test waiver are
changed to match. Nothing in the repository acts as if an answer has been given.
