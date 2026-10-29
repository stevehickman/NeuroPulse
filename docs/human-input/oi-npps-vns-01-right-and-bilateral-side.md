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

**Status of this section: not verified against the full texts.** The session that drafted it could
read abstracts and search snippets only; the full-text hosts were outside its network allowance.
A reader must check each point against its source before relying on it.

Recorded in the repository (from `OI-NPPS-VNS-01` (f), quoting Matsuoka 2025 pp. 4, 5, 11):
- Most taVNS studies stimulate the left ear for the cardiac reason above. The row records "the eight
  stroke studies that state a side used left, one bilateral, none right"; the counts are ambiguous as
  written and need re-checking against the paper. No stroke study used right alone.
- Right and bilateral stimulation are also reported as safe and effective.
- Bilateral cardiac effects were unknown until a study cited as "Hatik 2023" found no bradycardia or
  hypotension. **That citation could not be found by search and needs checking.**

Found by search (abstract level only):
- A 2022 systematic review and meta-analysis of 177 taVNS studies reports a low adverse-event rate,
  mostly ear pain, headache and tingling, with no difference from controls. The abstract does not
  split results by side or report bradycardia separately.
- Percin et al. 2024 (J Clin Med): bilateral stimulation, 20 minutes at 25 Hz, 76 healthy adults.
  Heart rate fell in the in-ear group only; blood pressure and RMSSD improved in both active groups.
- One study reports a rise in a single HRV measure (SDNN) after right-ear stimulation against sham.
- A trial registered as NCT05370027 compares left, right and bilateral taVNS on heart rate, blood
  pressure and HRV in healthy people. No results were seen.
- A 2025 review says comparative left-versus-right studies are still needed.

What the search did **not** establish:
- Any trial of right-ear taVNS in people with heart disease, or any report of bradycardia, asystole or
  syncope from right-ear taVNS.
- Whether any trial combined right or bilateral stimulation with the high currents the stroke
  protocols use. Our ceiling is now 40 mA, so 2.5–9 mA doses are authorable.

The concern is mechanistic, from invasive cervical VNS, where the stimulus is at the nerve trunk. Whether
it carries over to the auricular branch is not shown by the evidence we have.

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
