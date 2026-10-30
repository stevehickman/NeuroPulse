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

**What the standards and guidance would ask** (outside sources; read as search summaries, so each needs the text
checked before it is relied on)
- FDA's guidance on reprocessing reusable devices (March 2015) asks manufacturers to validate cleaning and
  disinfection or sterilisation instructions for a reusable device used on more than one patient, and cites
  21 CFR 801.109 ([FDA guidance page](https://www.fda.gov/regulatory-information/search-fda-guidance-documents/reprocessing-medical-devices-health-care-settings-validation-methods-and-labeling)).
- ISO 17664-1:2021 sets what processing information a manufacturer must supply
  ([AFNOR listing](https://www.boutique.afnor.org/en-gb/standard/iso-1766412021/processing-of-health-care-products-information-to-be-provided-by-the-medica/xs139592/264264)).
- IEC 62366-1 is the usual frame for validating that people can follow reprocessing instructions.
- IEC 60601-1 has a clause on cleaning, disinfection and compatibility with the agents used; a secondary source
  places it at clause 11.6, **unverified until the standard's text is read**.

**What is assumed, not known.** That a device used on intact skin needs a lower level of disinfection than one
used on mucosa (the usual grading of devices by contact). That is the reviewer's to confirm. The nasal probe
is the only part that touches mucosa.

## 3. Why the answer is not ours to give

A cleaning claim for a body-contact device says that a person who follows it will not be infected or irritated.
That is a risk-management decision (ISO 14971) and a regulatory one (what is the intended use, and is the device
reusable between people), and either one can add testing that costs time and money. Nothing in this file or in
`docs/np_ifu_001.md` may act as if it has been answered.

## 4. The four questions

**Q1 (principal, with Regulatory). Is each headset a single-person device?**
- **A. Yes, for T1 and for every accessory.** The IFU says so and makes no shared-use claim. Cheapest. It sits
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

**Q3 (Risk). The nasal probe.** It is the one mucosal contact, with a single-use sleeve that cannot be enforced
as specified (`OI-NASAL-04`). Pick one: A. the probe is for one person only on every tier, with a sleeve per
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
