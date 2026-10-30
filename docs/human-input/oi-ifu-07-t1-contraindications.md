# Clinical and regulatory question for review: OI-IFU-07, who must not use NeurOne Home, and what the warnings say

**Status:** DRAFT, ready to send; not yet sent (no addressee named). Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Systems Engineering (`OI-IFU-07` and `IFU-33` in `docs/np_ifu_001.md`)
**To:** Clinical adviser and Regulatory lead (names to be assigned; the item's owner column reads "Clinical + Regulatory")
**Response needed:** for each of the five questions in section 4, a ruling or the list to use, a short rationale, conditions that would reopen it, and the reviewer's name, role and date.
**Why now:** `IFU-19…22` tell a user how to use the goggles, the electrical modalities, the audio cups and the intranasal probe, and they are not issued until the warnings exist. The only T1 list in the repository is five lines in a risk plan. Nobody with clinical standing has reviewed it, and it says nothing per modality.

## 1. The question

Is the T1 "not intended for" list complete and right, and what else must a lay user of NeurOne Home be told
before using each modality?

Plain form: the headset delivers red and near-infrared light to the scalp and the eyes, flashing light, weak
electrical currents through the scalp and the ear, and sound through the ear and the bone behind it. A person
who should not have one of those has to be told so before they use it. We have a short list and no clinical
review of it.

## 2. What is known

**The only T1 list in the repository** (`NP-RM-001` §6.1, Rev 1, dated 2026-05-13, ACTIVE)
- Intended users are adults aged 18 and over, for wellness (focus, relaxation, sleep, general cognitive function),
  at home, without clinical training.
- **Not intended for:** diagnosis, treatment or monitoring of any medical condition; users under 18; users with
  active implanted electronic devices; users with known seizure disorders ("photoparoxysmal EEG detection required
  before visual stimulation use"); users who are pregnant.
- `NP-RM-001` §9 (risk-control hierarchy) lists the implanted-cardiac-device warning as an *information for safety*
  control, "level 3 (supplementary only)": the last and weakest control in ISO 14971.
- Severity examples in `NP-RM-001` §4.1 include transient seizure from photostimulation (S3), sustained seizure and
  permanent hearing damage (S4), and temporary tinnitus from bone conduction (S2).

**The cervical list, for comparison** (`NP-REG-CVNS-001` §7.2, T2, proposed): active implanted electronic device
(pacemaker, implanted defibrillator, cochlear implant, drug pump), carotid artery disease, cervical vagotomy,
pregnancy (precautionary), under 18.

**What the device does, by modality** (`CLAUDE.md` §3 and §4.2; `docs/reference/modality-stack.md`)

| Modality | Delivers | Device-side controls that exist |
|---|---|---|
| PBM scalp and intranasal | Red (660 nm) and near-infrared (808–830 nm, 1064 nm optional) light on the scalp and in the nostrils | Weighted-sum average ceiling, 400 mW/cm² peak, 42 °C limit; NTC throttle. The ceilings are unverified (`OI-BIBPBM-01`, `-03`); no skin-type term exists (`OI-BIBPBM-04`), and darker skin showed about three times the heating risk in the literature (`NP-BIB-PBMIRR-001` §4) |
| Visual | 108 micro-LEDs per lens, photic driving 0.5–100 Hz, EMDR alternation; Mode F invisible near-infrared on the retina during normal wear | IEC 62471 limit at 50 % of the exempt-group threshold; three hardware layers (proximity sensor, lift sensor, current limit); photoparoxysmal halt in under 200 ms from the Oz electrode, with a clinician unlock to re-enable 3–30 Hz; Mode F off by default, separate consent, and gated off in firmware until the opinion letter arrives |
| EEG neurofeedback | Nothing delivered; records scalp signals | Skin contact only |
| BES / tACS | 0.5–40 Hz, up to 1 mA, biphasic, scalp electrodes | 40 µC/cm² per phase, enforced in hardware; per-electrode impedance monitoring |
| tDCS | 0.1–2 mA direct current, up to 3 electrode pairs | 150 mC/cm² per session (provisional, `OI-CHARGE-05`/`-06`), 30 s ramp, in hardware |
| Auricular VNS | Biphasic current to the ear, up to 40 mA as a stop-gap, no frequency ceiling | 40 µC/cm² per phase; contact confirmation; the 40 mA is not a sensory, heating or cardiac limit (`OI-VNSCLIP-09`) |
| Audio | Binaural beats, isochronic tones, noise through over-ear drivers and bone conduction | **No safe level is set in any controlled document** |

**What is not known.** Whether the list above is complete for these exposures. Nothing in the repository covers:
photosensitivity, medication that sensitises skin or eyes to light, eye conditions, skull defects, metal in or on
the head, skin conditions at electrode sites, tinnitus and hearing conditions, a history of migraine triggered by
light, or use while driving. These are examples of what a reviewer would be asked about, not a claim that each
applies.

## 3. Why the answer is not ours to give

A contraindication list says who may be harmed. A list that is too short injures someone, and one that is too
long makes the product unusable for people it would help. Both are clinical and regulatory judgements, and the
list for a wellness device may differ from the list for a prescription one. Nothing in this file or in
`docs/np_ifu_001.md` may act as if it has been answered. `IFU-33` carries only what `NP-RM-001` §6.1 already
states.

## 4. The five questions

**Q1 (Clinical). Is the `NP-RM-001` §6.1 list right and complete for NeurOne Home as a whole?** Add, remove or
reword. In particular: is "known seizure disorder" an absolute exclusion, given that the headset can detect a
photoparoxysmal response at the Oz electrode and halt the goggles, and what does the parenthetical "detection
required before visual stimulation use" mean for a user's instruction?

**Q2 (Clinical). Per modality, what must a user be told before use?** For the light modalities (scalp, nose, eyes,
Mode F), flashing light, the electrical modalities (scalp, ear), and sound. If the answer is "the same list",
say so.

**Q3 (Clinical, with Risk). Is there a safe listening level to state for the audio modality?** None is set. Pick
one: A. a level is derived and stated; B. the audio output is capped in the device and no level is stated;
C. the text warns without a number.

**Q4 (Regulatory). Is the instruction a contraindication list, or a warning, or both?** For a wellness device
under `CLAUDE.md` §1 the answer may differ from the T2 labelling. Does "not intended for … users who are pregnant"
belong in the same register as "under 18"?

**Q5 (Clinical). Skin type.** Is the heating risk in darker skin under red light something the text must state
(`NP-BIB-PBMIRR-001` §4; `OI-BIBPBM-04`), and if so in what words?

## 5. What we need back

For each question: the ruling or the list; a short rationale (`CLAUDE.md` §18 wants a derivation); conditions that
would reopen it; and your name, role and date. If a ruling needs a measurement first, say which and what result
would be enough.

## 6. What happens with the answer

It is recorded in `docs/np_ifu_001.md`, in `docs/status/completed-decisions.md` and against `OI-IFU-07`, and
`IFU-33` is extended to match. `IFU-19…22` are issued only after `OI-IFU-07` closes.
