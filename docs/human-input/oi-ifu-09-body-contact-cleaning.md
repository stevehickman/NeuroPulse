# Regulatory and risk question for review: OI-IFU-09, cleaning and disinfection of the parts that touch the person

**Status:** DRAFT, ready to send; not yet sent (no addressee named). Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Systems Engineering (`OI-IFU-09` in `docs/np_ifu_001.md` §2.13)
**To:** Regulatory lead and risk lead, with the principal for question 1 (names to be assigned; the item's owner column reads "Regulatory + Systems Engineering")
**Response needed:** a ruling on each of the four questions in section 4, a short rationale for each, conditions that would reopen it, and the reviewer's name, role and date.
**Why now:** `IFU-28…31` tell a user how to clean the headset's non-contact parts. Nothing yet tells anyone how to clean the parts that touch them, or whether a headset may pass from one person to another. A user will do something with that gap, whether or not we write it down.

## 1. The question

May a NeurOne headset be used by more than one person, and if so, what cleaning and disinfection between people
has to be validated before we say so?

Plain form: the data layer already assumes a clinic can own one device and many patients can use it
(`CLAUDE.md` §6.0: each patient has their own `ConsentStore` on a clinic device). The hardware documents say
nothing about what happens to the electrode pods, ear pads, ear clip, goggle rim, cap and nasal probe between
two wearers. Either the product is one person's device, or it needs a reprocessing regime. Today it is
neither, because nobody has decided.

## 2. What is known

**Which parts touch whom** (from the documents read; a blank means no document states it)

| Part | Touches | Repo basis for the material and the contact | Hygiene control that exists | Gap |
|---|---|---|---|---|
| EEG electrode pods and hydrogel tips | Scalp, through hair; repeated | Skin-contact elements are to meet ISO 10993-5 and -10 (`NP-HELMET-GEOM-001` §0), a requirement and not evidence; tips are a consumable (`commercial-model.md` §2.3) | "No shared electrode tips" (`NP-RM-001`); the tip count prompt (placeholder, `OI-ACC-09`) | No cleaning regime for the pod under the tip; the tip's count is not a hygiene limit |
| Light windows and bezels | Hair and scalp at a 1.0 mm standoff | PDMS windows listed for ISO 10993 assessment (`NP-QMS-DC-001`) | `IFU-28` (dry wipe, proposed, held) | Proposed method is not a disinfection step |
| Forehead bridge, occipital dial, temporal wings, headband surfaces | Skin of forehead and head | Not stated | None | Material and contact not stated in any document read |
| Goggle rim, rim guards, shades | Skin around the eyes | Not stated | None | As above |
| Audio ear pads, foam, bone-conduction pad | Skin around the ear; mastoid; prolonged | No material of record (`OI-AUDIOHW-06`) | Foam prompt on loss of seal only, by decision (`commercial-model.md` §2.3) | Hygiene replacement is at the user's discretion by design |
| Auricular ear clip and its PDMS hydrogel pads | Ear skin | No biocompatibility basis of record (`OI-VNSCLIP-07`) | Pad session count (placeholder) | As above |
| Intranasal Y-probe | **Nasal mucosa** | No material of record (`NP-HW-NASAL-001` §7) | Single-use sleeve (`OI-NASAL-04`: unenforceable as specified); `NP-RM-001` rates infection from a shared probe | No regime for probe or dock (`OI-NASAL-08`) |
| Cervical gel pads (T2) | Neck skin | Biocompatibility basis must cover reuse (`OI-CVNSHW-07`) | Contact check; "do not share pads" in the proposed text (`NP-REG-CVNS-001` §7.4) | Contamination by one person's reuse is outside what the check sees |
| T2 qEEG wet-gel cap | Scalp, wet gel | `NP-HW-TCAP-001` is silent on cleaning and on reuse | None | Not stated |
| Hub | None, except the dock that touches the probe tips | n/a | n/a | The dock is part of `OI-NASAL-08` |

**What the FDA guidance says** (read from the guidance itself, "Reprocessing Medical Devices in Health Care Settings:
Validation Methods and Labeling", final, March 2015, docket FDA-2011-D-0293; the full PDF was fetched on 2026-10-30
and its text extracted locally. The document marks itself as **nonbinding recommendations**.)
- **Its scope is not limited to devices shared between people.** It covers four situations, including "reusable
  medical devices intended to be reused only by a single patient and intended to be reprocessed between each use"
  (§III, item 3). So a single-person headset does not by itself take a device outside the idea of reprocessing
  instructions; what matters is whether the device is *intended to be reprocessed between uses*.
- **Reprocessing instructions are part of the labelling.** The guidance interprets adequate reprocessing instructions
  as part of "adequate directions for use" under 21 CFR 801.5 and a condition of the exemption under 21 CFR 801.109.
- **Lay use has its own item (§VI, Criterion 5, item N):** instructions for a device kept by a patient or lay carer "should be
  understandable to a lay person and can be performed at home", with the equipment readily available where it is
  used.
- **Contact decides the level (Spaulding, as FDA modifies it, §VI, Criterion 3).** *Critical* devices enter sterile tissue.
  *Semi-critical* devices contact **intact mucous membranes or non-intact skin**: "thoroughly clean these devices and
  then reprocess them by sterilization", or **high-level disinfection** if the design cannot be sterilised.
  *Non-critical* devices contact **only intact skin**: "thorough cleaning, then intermediate or low level
  disinfection", and the guidance names **skin electrodes** among its examples.
- **Applied here (reviewer to confirm):** every part that touches only intact skin is non-critical. The **nasal
  probe touches mucosa and is semi-critical**, so the expected reprocessing between uses is heavier than a sleeve
  and a wipe. A cervical pad used on broken skin would also be semi-critical, which is a reason the "do not use over
  broken or irritated skin" warning (`NP-REG-CVNS-001` §7.3) matters.
- **Which NeurOne products it reaches is the reviewer's call.** The guidance addresses submissions for devices
  under 510(k), PMA, HDE, de novo and IDE. T2 is a 510(k) target. T1 is positioned as FDA-exempt wellness
  (`CLAUDE.md` §1), and whether a wellness product is inside this guidance at all depends on that status.

**What is still unverified** (outside sources read only as search summaries; the pages were on hosts this session
could not reach): ISO 17664-1:2021's exact scope, IEC 62366-1's role in validating reprocessing instructions, and the
IEC 60601-1 clause numbers (a secondary source places cleaning and disinfection at clause 11.6). Read the standards
before relying on any of them.

**What is assumed, not known.** Whether the guidance's "intended to be reprocessed between each use" describes a
NeurOne headset. That follows from the intended-use statement, which is the reviewer's and the principal's.

## 3. Why the answer is not ours to give

A cleaning claim for a body-contact device says that a person who follows it will not be infected or irritated.
That is a risk-management decision (ISO 14971) and a regulatory one (what is the intended use, and is the device
reusable between people), and either one can add testing that costs time and money. Nothing in this file or in
`docs/np_ifu_001.md` may act as if it has been answered.

## 4. The four questions

**Q1 (principal, with Regulatory). Is each headset a single-person device, and is it intended to be reprocessed between uses?** Note that the FDA guidance reaches single-patient reusable devices too, so answering *single-person* does not on its own remove the need for validated cleaning instructions for a 510(k) device (T2). It may remove the need for *disinfection between people*.
- **A. Yes, for T1 and for every accessory.** The IFU says so and makes no shared-use claim. Cheapest, but cleaning instructions are still owed for what one person reuses. It sits
  badly with `CLAUDE.md` §6.0, which allows a clinic to own a device that many patients use, so a clinic that
  buys T1 would be outside its labelling.
- **B. Yes for T1; T2 is allowed to be shared with a validated regime.** One regime to build, for the clinic
  product only.
- **C. Shared use is allowed on both tiers with a validated regime.** Most expensive.
We lean to **B**, because T2 is the clinical product (multi-patient dashboard, anonymised session tag) and T1 is
sold to individuals. This is a recommendation, not a decision.

**Q2 (Regulatory). If any shared use is allowed, which levels of disinfection, for which parts?** Scalp- and
skin-contact surfaces, the goggle rim, and the cap are the candidates. Which agents are named in the IFU must
come from a compatibility test on the real materials (PDMS windows, the Ag/AgCl pods, silicone, the hard-gold
contacts), which does not exist yet.

**Q3 (Risk). The nasal probe.** It is the one mucosal contact, so semi-critical in FDA's scheme (sterilisation, or high-level disinfection if the design cannot be sterilised, between uses), with a single-use sleeve that cannot be enforced
as specified (`OI-NASAL-04`). Does the sleeve count as the barrier that changes the class? Pick one: A. the probe is for one person only on every tier, with a sleeve per
session; B. the probe is allowed to be shared, with a validated regime for the probe and the dock; C. the
probe leaves T2.

**Q4 (Regulatory). Is the cervical gel pad's reuse by one person acceptable without a limit?** The pad is used
until the contact check fails, then thrown away (`NP-REG-CVNS-001` §7.4). The check cannot see skin
contamination, which is the residual hazard (`OI-CVNSHW-07`). Options: A. keep as written; B. add a use
limit; C. return the pad to single use.

## 5. What we need back

For each question: the ruling or the option chosen; a short rationale (`CLAUDE.md` §18 wants a derivation);
conditions that would reopen it; and your name, role and date. If a ruling needs a test first (for example a
compatibility test of the disinfectant on the PDMS windows), say which test and what result would be enough.

## 6. What happens with the answer

It is recorded in `docs/np_ifu_001.md` §2.13, in `docs/status/completed-decisions.md` and against `OI-IFU-09`,
and then `IFU-32` is extended or the contact-part statements are drafted to match. Until then no statement
about cleaning or disinfecting a part that touches a person is written, and `IFU-32` says only what is already
sourced.
