# PBM Irradiance and Temperature — Regulatory Limits and Safety Evidence Record

**Project:** NeurOne
**Document:** NP-BIB-PBMIRR-001
**Revision:** 3
**Date:** 2026-10-04
**Status:** DRAFT — literature and regulatory review; asserts no measurement and sets no limit
**Effective Date:** 2026-10-04
**Author:** NeurOne Regulatory Affairs
**Approved By:** Pending — outside regulatory counsel review under RISK-03 (`NP-REG-PBM1064-001` Rev 2 §6A)
**References:** CLAUDE.md §3, §4.2; `NP-REG-PBM1064-001`; `NP-HW-HEXTILE-001` §2 (R-4, R-5), §4.3.1, `OI-HEXTILE-20`; `NP-SES-PWR-001` §2.3, `OI-SESPWR-02`; `NP-FEAS-PBMCH-001` `OI-PBMCH-07`; `NP-RISK-002` RISK-03, `OI-RISK2-04`; `docs/status/pending-decisions.md` §13.1a; `NP-BIB-1064-001`; FDA draft guidance *Photobiomodulation (PBM) Devices — Premarket Notification [510(k)] Submissions* (2023-01); FDA *General Wellness: Policy for Low Risk Devices* (2016, revised 2026-01-06); ICNIRP 2013 laser guidelines; ICNIRP 2013 incoherent visible/IR guidelines; IEC 60825-1; ANSI Z136.1; IEC 62471; IEC 60601-1; IEC 60601-2-57
**Related Issues:** GitHub Issue #5 (RISK-03); GitHub Issue #56
**Gate:** —
**IEC 62304 Class:** — (evidence record, not device software)

---

## 1. Why this document exists

CLAUDE.md §3 sets the PBM scalp ceiling at **400 mW/cm² peak pulsed (≤ 25 % duty) · 200 mW/cm² CW ·
42 °C**, and `NP-HW-HEXTILE-001` §2 carries it as R-4, with a 600 mW/cm² three-channel aggregate as
R-5. `docs/status/pending-decisions.md` §13.1a traced both to firmware constants marked *"pending
RISK-03 regulatory opinion"* and found **no IEC 62471 derivation, no FDA position and no ICNIRP limit
anywhere in the chain.**

This document is the missing literature half. It records what the published regulatory and safety
record says about irradiance and temperature boundaries for PBM from **600 nm to 1100 nm**, and
compares the NeurOne ceilings with it. **It does not replace counsel's opinion, set a limit, or
change a requirement.** `NP-REG-PBM1064-001` Rev 2 §6A carries the questions it raises to counsel.

### 1.1 Source quality, stated once

This review was run through web search. The session's network policy blocked the primary texts
(icnirp.org, fda.gov and the paywalled ANSI and IEC standards). Every figure below is labelled by
where it came from:

| Label | Meaning |
|---|---|
| **[P]** | Confirmed against the primary text, read in full on 2026-10-04 (Rev 2): ICNIRP 2013 laser, ICNIRP 2013 incoherent, FDA 2023 draft guidance |
| **[S]** | Confirmed against a search-result extract of the named source |
| **[K]** | The standard's published formula, from working knowledge; not re-read in this review |
| **[G]** | Grey literature (vendor or consumer material). Recorded because it states a number, not because it carries weight |

**No [K] figure may be cited as verified until it is checked against the purchased standard**
(`OI-BIBPBM-01`).

**Rev 2 re-read the three freely available primary texts** (the ICNIRP PDFs and the FDA draft
guidance) and relabelled what they confirm **[P]**. They are ICNIRP's documents, **not** the
IEC or ANSI standards that adopt them, so a [P] row verifies ICNIRP's wording and says nothing
about the standard's. The paywalled standards are still unread (§6, `OI-BIBPBM-01`).

---

## 2. Finding: no statute or regulation sets a PBM irradiance limit

No US or EU law or regulation found in this review sets a peak or average irradiance limit for
photobiomodulation anywhere from 600 to 1100 nm. The boundaries that do exist are of three kinds:
a **definitional** one (FDA: "non-heating"), **exposure-limit standards** written for laser and lamp
safety, and a **contact-temperature** standard.

| Source | What it says | Numeric limit |
|---|---|---|
| FDA draft guidance, *PBM Devices — 510(k) Submissions* (2023-01, **not final**) | Defines PBM as *"the application of light at an irradiance that does not induce heating with the goal of altering biological activity"* **[P]**. Full text read in Rev 2. §J (thermal safety) names **masks and helmets** as devices that may raise tissue temperature to dangerous levels, and recommends skin-temperature measurements, built-in time limits (especially over-the-counter) and **tissue temperature sensors that shut off output**. §K (eye) says to measure output against the ocular MPE. Its listed test standards are ES60601-1 and 60601-1-2; it cites neither IEC 62471 nor IEC 60601-2-57 | **None.** The text contains no irradiance, fluence or temperature figure. "Non-heating" is never quantified |
| FDA *General Wellness* guidance (2016; revised 2026-01-06) | A product is **not** low risk if it involves *"technologies, like lasers or radiation, that could pose safety risks without regulatory controls"*. Its worked example is a skin-rejuvenating laser **[S]** | None. An eligibility gate, not a limit |
| 21 CFR 890.5500 (infrared lamp, code ILY); 21 CFR 878.5400 (low-level laser, aesthetic, code OLI) | Classification regulations **[S]** | None |
| 21 CFR 1040.10 | Laser product performance standard | Applies to lasers only; not to LED arrays |
| IEC 60601-2-57 (2023 ed.; EN IEC 60601-2-57:2026 adds RG-1C) | Non-laser therapeutic light sources, 200–3000 nm; classifies by IEC 62471 risk group and permits RG3 with trained operators **[S]** | Only through IEC 62471 (§3.2) |
| EU MDR Annex XVI, group 5 | High-intensity optical radiation equipment *"for skin treatment"* | None of its own. Whether a scalp PBM wellness product is in scope turns on its claims |

**The General Wellness exclusion is the finding that matters most for T1.** CLAUDE.md §1 classes
T1 as FDA-exempt wellness. The 2026 revision kept the exclusion of risky light technologies. It
names lasers, not LEDs, and nothing found places a 400 mW/cm² LED array on either side of it.
That is `NP-REG-PBM1064-001` Q14.

---

## 3. Exposure-limit standards

### 3.1 Laser skin exposure limit (ICNIRP 2013 / IEC 60825-1 / ANSI Z136.1)

For 400–1400 nm and exposures from 10 s to 30,000 s, the skin limit is **E = 200 × C_A mW/cm²**
(2000 × C_A W/m²) **[P]** (ICNIRP 2013 laser, Table 7), averaged over a **3.5 mm limiting aperture**
(for beams under 1 mm the actual, unaveraged exposure is compared). Below 10 s the limit is
11 × C_A × t^0.25 kJ/m² (100 ns–10 s). C_A is **[P]** (ICNIRP 2013 laser, Table 3):

- C_A = 1 for 400 ≤ λ < 700 nm
- C_A = 10^(0.002 × (λ − 700)) for 700 ≤ λ < 1050 nm
- C_A = 5 for 1050 ≤ λ ≤ 1400 nm

The 30,000 s (8.3 h) upper duration is the table's end; no limit is stated beyond it.

| λ | Skin limit, CW or time-averaged |
|---|---|
| 600–700 nm (incl. 660–670) | **200 mW/cm²** |
| 808 nm | ≈ 329 mW/cm² |
| 830 nm | ≈ 364 mW/cm² |
| 850 nm | ≈ 399 mW/cm² |
| 1050–1100 nm (incl. 1064) | **1000 mW/cm²** |

Three properties of this limit decide how it may be used:

1. **Pulsed exposure is bounded by its average, and by every sub-group of pulses.** ICNIRP's
   rules for repetitive exposure **[P]**: (1) each pulse must meet the single-pulse limit; (2) **the
   exposure from any group of pulses delivered in time T must not exceed the limit for time T**,
   with T varied from the pulse duration to the whole exposure; (3) a C_P factor for **retinal**
   thermal limits only. Rev 1 read this as "the average may not exceed the CW limit, and there is
   no peak ceiling". **That was incomplete.** Rule 2 uses the *time-dependent* skin limit for
   T ≤ 10 s, so a long ON burst is bounded: radiant exposure ≤ 1.1 × C_A × T^0.25 J/cm². See §5.
2. **Multiple wavelengths add, qualitatively.** ICNIRP states that exposures are *"considered
   spectrally additive"* where the absorption site and the injury mechanism are the same, and that
   the assumption is conservative for thermal injury **[P]**. **It does not print the
   Σ Eᵢ / ELᵢ ≤ 1 formula.** The weighted-sum form used by R-4 and `OI-HEXTILE-31` is therefore
   still **[K]** (it comes from the IEC and ANSI texts), and so are the C_A weights as applied
   to it.
3. **The large-area reduction does not apply in this band.** ICNIRP's reduction to 100 W/m²
   (10 mW/cm²) for exposed skin above 0.1 m², and the inverse-area rule from 0.01 to 0.1 m², apply
   **above 1400 nm only** **[P]** (Table 7 note c and the text). ANSI Z136.1's corresponding footnote
   is still unchecked (`OI-BIBPBM-01`).

These are **laser** limits. Applying them to an LED array is the conventional conservative
reference (it is how Barrett & Gonzalez-Lima justified their dose, §4), not a legal requirement.

### 3.2 Lamp safety (IEC 62471 / ICNIRP 2013 incoherent)

- **Skin thermal, 380–3000 nm:** limited only for exposures **under 10 s** (radiant exposure
  2 × 10⁴ · t^0.25 J/m²) **[P]** (ICNIRP 2013 incoherent, eq. 22). Beyond 10 s the framework relies on **pain avoidance** (it says so: *"No limit is provided for longer exposure durations"*): the person
  moves away from a source that feels too hot. That assumption is weak for a device strapped to the
  head, worn by someone who may be asleep (`Deep Sleep` is in the library) or instructed to stay
  still.
- **Eye, infrared, 780–3000 nm:** **10 mW/cm²** (100 W/m²) for exposures of 1000 s or more, and 18 × t^−0.75 × 10³ W/m² below that **[P]** (ICNIRP 2013 incoherent, eq. 20–21; the source is weighted by an IR action spectrum, eq. 19).
  This matters for any scalp-array light reaching the eyes near the forehead, and is separate from
  the goggle interlocks of CLAUDE.md §4.2.

ICNIRP's incoherent guideline gives the same repetitive-exposure principle as the laser one:
any exposure within the anticipated duration T must be under the limit for T, which for a pulse
train is equivalent to comparing the **average over T** with the limit for T, and the analysis
window is slid along the time axis so the worst position governs **[P]**.

### 3.3 Temperature limits

| Source | Limit | Label |
|---|---|---|
| IEC 60601-1 (3rd ed.) Table 24, applied parts in contact **≥ 10 min**, all materials | **43 °C** | **[S]** |
| IEC 60601-1 (3rd ed.), any applied part **> 41 °C** | Must be **disclosed in labelling and instructions for use** | **[S]** |
| IEC 60601-1 (2nd ed.), applied parts not intended to supply heat | **41 °C** | **[S]** |
| IEC 60601-2-37 (ultrasound; cited as a precedent of the same figure) | 43 °C for contact ≥ 10 min | **[S]** |
| NeurOne, CLAUDE.md §3 and §4.2 | **42 °C** skin limit, attributed to IEC 60601; NTC per zone → hardware current throttle at **62 °C junction** | repo |
| Grey literature, red-light consumer guidance | If skin stays **> 40 °C for more than 5 min** after treatment, reduce power or session length | **[G]** |

**Finding.** The 42 °C in CLAUDE.md §3 is **not** the IEC 60601-1 figure, which is 43 °C (≥ 10 min)
with a labelling duty above 41 °C. 42 °C is more conservative than the standard, so it is not
unsafe, but it is **attributed to a standard that does not state it**. Either the attribution is
wrong or a derivation exists and is uncited. Under CLAUDE.md §18 that raises an open item and does
not license any change to the value (`OI-BIBPBM-03`).

The 62 °C figure is a **junction** temperature, a component protection, and bounds no skin
temperature on its own. The thermal chain from junction to skin is `NP-THERM-*`'s, not this
document's.

---

## 4. Literature: heating thresholds and safety cases

| Finding | Source | Label |
|---|---|---|
| Unacceptable tissue heating begins at about **300 mW/cm² at 600–700 nm**, about **750 mW/cm² at 800–900 nm**, and as low as 100 mW/cm² at 400–500 nm | Zein, Selting & Hamblin, *J Biomed Opt* 23(12):120901, 2018 | **[S]** |
| Commonly recommended dose at the target tissue: **< 100 mW/cm²**, 4–10 J/cm²; reciprocity holds roughly over 1–100 mW/cm² and 1–100 J/cm² | Zein, Selting & Hamblin 2018 | **[S]** |
| Biphasic dose response: beyond an optimum, more light inhibits. A bound on efficacy, not safety | Huang, Sharma, Carroll & Hamblin, *Dose-Response* 2011 | **[S]** |
| **1064 nm, 250 mW/cm², 60 J/cm², 4 min, 4 cm spot, right frontal pole**; justified as one quarter of the skin limit (1000 mW/cm²); *"negligible heat"*. The dose most later 1064 nm work reuses | Barrett & Gonzalez-Lima, *Neuroscience* 2013 | **[S]** |
| At **≤ ~250 mW/cm²** transcranial laser/thermal stimulation is reported safe and non-painful; a practical unmonitored starting range is 30–100 mW/cm² average, staying below 250 mW/cm² unless temperature is monitored | Summarised from the transcranial PBM literature (Sci Rep 2021 and later) | **[S]** |
| Modelled at 1064 nm: scalp **+0.38 °C at 100 mW/cm²** and **+3.76 °C at 1000 mW/cm²**; brain +0.06 °C and +0.57 °C; temperature rise **asymptotes after ~10 min**; 1000 mW/cm² **exceeds scalp and brain temperature safety limits** | High-resolution computational modelling of tPBM, medRxiv 2025.01.18.25320758 (published *Neuromodulation* 2025) | **[S]** |
| People with **darker skin** had about **3× the risk of heating or thermal injury**; the excess is concentrated in the red band, where melanin absorbs more than in NIR, and is worst at high irradiance and dual irradiation | *Photodermatol Photoimmunol Photomed* 2025 (PMC12336633) | **[S]** |

**One inference, labelled as such.** The modelled scalp rise is close to linear (≈ 0.38 °C
per 100 mW/cm² at 1064 nm). From a 33–34 °C resting scalp, 43 °C is ~9 °C away, which the linear
figure reaches only well above 1000 mW/cm². **This is not a basis for any limit**: it is one model,
at one wavelength, with no contact heating from the applicator and no melanin term, and a closed
helmet adds conducted heat from the emitters that an open-air laser spot does not.

---

## 5. NeurOne's ceilings against this record

| NeurOne figure | Against the record | Reading |
|---|---|---|
| **200 mW/cm² CW** (R-4) at **660 nm** | = laser skin limit (200 × 1) for T > 10 s. **Rev 2:** the sub-group rule (§3.1 item 1) applies the 10 s limit to a window of T ≤ 10 s, 1.1 × T^0.25 J/cm², which is **195.6 mW/cm² at T = 10 s** | **Zero margin** at 660 nm, and on a strict reading of ICNIRP's rule 2 a CW 200 mW/cm² exceeds the limit for windows of about 9.7–10 s by up to 2.2 %. The step at 10 s is the table's own (2.0 × C_A kW/m² against 11 × C_A × t^0.25 kJ/m² = 19.6 kJ/m²). Whether IEC 60825-1 and ANSI Z136.1 print the same step is unread (`OI-BIBPBM-05`). Below the limit at 808 nm and above |
| **400 mW/cm² peak, ≤ 25 % duty** (R-4) | Averages ≤ 100 mW/cm². **Rev 2:** ICNIRP's sub-group rule bounds an **uninterrupted ON burst** at 400 mW/cm² to the T where 0.4 × T = 1.1 × C_A × T^0.25 J/cm²: **3.9 s at 660 nm, 7.5 s at 808 nm, 8.6 s at 830 nm**, and not binding at 1064 nm (above 10 s the limit is 1000 mW/cm² average). A burst is the longest run of ON pulses with no off-time between them | The average is within the rule at every wavelength. Rev 1's "the standards bound the average, not the peak" is **corrected**: they bound the average over every window, which limits burst length at 400. The 400 figure itself still has no recorded derivation (RISK-03). Whether any signed protocol has a burst this long is not checked here (`OI-BIBPBM-05`) |
| **600 mW/cm² aggregate peak** (R-5) | 150 mW/cm² average at 25 % duty. Weighted sum at the §3.1 worst case (all at 660 nm) = 0.75; at full three-channel operation (100 average each at 660/808/1064) = 0.5 + 0.30 + 0.10 = **0.90** | Within the weighted-sum rule **if** duty holds. The standards' natural form for R-5 is a **weighted sum of averages**, not a sum of peaks (`OI-BIBPBM-02`) |
| Two T1-A channels at 403 mW/cm² each (`OI-HEXTILE-20`), same spot, CW | 806 mW/cm² vs 200 (660) + 329 (808) | **Would exceed** both the skin limit and Hamblin's heating onset. Only reachable if CW and full current coincide |
| `07-vascular-baseline`, `660_808nm`, **80 % CW** → **~322 mW/cm²** (`OI-SESPWR-02`) | Above the 660 nm skin limit (200) and above Hamblin's 600–700 nm heating onset (~300) | **If the CW clamp does not exist, this protocol exceeds the conventional skin reference at 660 nm**, not only R-4. Raises the priority of `OI-SESPWR-02`; decides nothing here |
| **42 °C** skin limit (CLAUDE.md §3) | IEC 60601-1: 43 °C (≥ 10 min), labelling above 41 °C | More conservative than the standard; attribution unsupported (`OI-BIBPBM-03`) |
| Fitzpatrick V–VI wearers | 3× thermal-injury risk, red-weighted | No skin-type term exists in any NeurOne dose or thermal model found (`OI-BIBPBM-04`) |
| FDA 2023 PBM draft guidance | Not cited anywhere in the document set before this revision | Now carried into `NP-REG-PBM1064-001` Rev 2 §6A |

### 5.1 The sub-group rule against the predefined protocol library (Rev 3, `OI-BIBPBM-05`)

**Method** (reproducible by hand; no script is committed). Skin limit for a window T:
EL(T) = 1.1 × C_A × T^0.25 J/cm² for T ≤ 10 s, and 200 × C_A × T mW·s/cm² above. For each
`pbm_transcranial` block in `protocols/predefined/`, peak irradiance = `intensity` × **403 mW/cm²**
(the `NP-HW-HEXTILE-001` §4.3.1 full-drive anchor, as `NP-SES-PWR-001` §2.3 scales it), both
channels of a `660_808nm` block on together, the periodic pulse train slid over every window from
1 ms to 10 s, and the score taken as Σ over the two wavelengths of (worst exposure in the window ÷
EL(T)). A score above 1 breaches the rule. Inherits `OI-HEXTILE-20` (403 is a design target) and
the unverified C_A weights (`OI-BIBPBM-01`). **Reading, not measurement.**

| Group | Blocks | Longest uninterrupted ON | Worst score (660 + 808 weighted) | Reading |
|---|---|---|---|---|
| Pulsed, 2–40 Hz at 25 % duty | 16 blocks across 14 files (Gamma Focus … Parkinson's) | **≤ 125 ms** (2 Hz, `03-deep-sleep`); 6.25 ms at 40 Hz | **≤ 0.64** (660 alone ≤ 0.40) | Pass. The rule never binds on a pulsed block at 25 % duty |
| CW, low irradiance (`clinical-04-cassano` 36, `clinical-05-maiello` 30, `clinical-06-tbi` 22 mW/cm²; the two 1064 nm blocks, whose CH_C channel gives 28 mW/cm², `NP-HW-HEXTILE-001` §4.3.2) | 5 | Whole session | ≤ 0.28 (1064 nm: far lower) | Pass, even read as CW |
| CW, single NIR channel (`clinical-04-schiffer` 250 at 810 nm, `clinical-05-wang` 310 at 820 nm; 4 min ON) | 3 blocks | 4 min | 0.77 and 0.91 (single wavelength) | Pass |
| CW, 80 % (`07-vascular-baseline`, ~322 mW/cm²) | 1 | 30 min | **2.56** (660 alone 1.59) | **Fails**, as it already fails the average term (`OI-SESPWR-02`) |
| `frequency: 0Hz` with `duty_cycle: 25 %` (`clinical-09-stroke-rehab`, 40 % ≈ 161 mW/cm²) | 1 | Undefined (`OI-SESPWR-03`) | **1.28 if read as CW**; passes if read as 25 % duty | Verdict follows the `OI-SESPWR-03` reading |

**Findings.**

1. **The sub-group rule changes no verdict in the library.** The two failing readings already fail
   the average term (§3.1, D-10). No block sits in the 195.6–200 mW/cm² band at 660 nm where the
   rule and the average disagree.
2. **The binding burst length is shorter than Rev 2 said.** Rev 2 gave 3.9 s for one channel at
   400 mW/cm² at 660 nm. On a `660_808nm` tile with **both channels on at 403 mW/cm²** (the
   `OI-HEXTILE-20` case), the weighted rule limits an uninterrupted burst to **about 2.0 s**; one
   channel alone, about **3.8 s**.
3. **Nothing in the library comes near it.** The longest pulsed ON run is 125 ms. Only a CW
   block can produce a burst of seconds, and the CW blocks are the ones scored above.
4. **What stays open is the pre-signing check's shape, not the library.** A check that
   compares only the time-averaged weighted sum (`OI-HEXTILE-30`, `OI-HEXTILE-31`) would pass a
   protocol of long, sparse bursts whose average is under 1 and whose bursts breach rule 2. The
   check should score the worst window as above. That is a design input to those items, and
   **it is not a new requirement** (CLAUDE.md §18): the standards' rule is the traceability, and it
   stays [P] for ICNIRP and [K] for IEC and ANSI until `OI-BIBPBM-01` closes. The hardware U4
   bound (≤ 50 % conduction over any window of 250 ms or more) already prevents continuous
   conduction at the tile today, and `OI-HEXTILE-30` proposes to redesign it.

---

## 6. Open items

| ID | Item | Owner · Blocking |
|---|---|---|
| **OI-BIBPBM-01** | **Verify every [K] figure against the purchased standards. OPEN, narrowed in Rev 2.** *Done against the ICNIRP texts and the FDA draft guidance (relabelled [P]):* C_A across 700–1400 nm (Table 3); the skin limit 2.0 × C_A kW/m² and its 3.5 mm aperture (Table 7); the large-area reduction applying above 1400 nm only; the ICNIRP repetitive-pulse rules (corrected, §3.1 item 1); the multiple-wavelength additivity statement (qualitative only); the incoherent skin and IR-eye limits; the FDA draft guidance, which has **no numeric heating or temperature criterion**. *Still open:* (a) the **Σ Eᵢ / ELᵢ ≤ 1 formula** as printed in IEC 60825-1 / ANSI Z136.1 (ICNIRP does not print it); (b) **ANSI Z136.1's** limiting aperture, repetitive-pulse rules and large-area footnote; (c) **IEC 60825-1** and **IEC 62471** as adopted, since ICNIRP's text is not theirs; (d) **IEC 60601-1 Table 24** and **IEC 60601-2-57** (2023 / EN 2026): any numeric heating or temperature criterion. (a)–(d) need the purchased standards | Regulatory Affairs · blocks citing any [K] row as verified |
| **OI-BIBPBM-02** | R-5's form: the exposure standards assess multiple wavelengths as a weighted sum of time-averaged irradiance (§3.1). R-5 is a sum of peaks. Whether R-5 should be restated is counsel's and the owner's call (`NP-REG-PBM1064-001` Q17, `OI-PBMCH-07`) | Firmware + Safety · counsel |
| **OI-BIBPBM-03** | The 42 °C skin limit of CLAUDE.md §3 / §4.2 is attributed to IEC 60601, which states 43 °C (≥ 10 min) and a labelling duty above 41 °C. Find the derivation or correct the attribution. **The value is not to be raised on this finding** (CLAUDE.md §18) | Safety · none |
| **OI-BIBPBM-05** | **Burst-length limit from the sub-group rule. Library check done in Rev 3 (§5.1): no predefined protocol breaches it that does not already breach the average term.** ICNIRP's repetitive-exposure rule 2 (§3.1 item 1) bounds any run of ON pulses to 1.1 × C_A × T^0.25 J/cm² for T ≤ 10 s; uninterrupted bursts are limited to about 3.8 s at 660 nm (one channel) and about 2.0 s for a `660_808nm` tile at 403 mW/cm² on both channels. *Remaining:* (a) read the same rule in IEC 60825-1 and ANSI Z136.1 (`OI-BIBPBM-01`); (b) decide with counsel whether R-4 states a maximum ON time, or whether the pre-signing check scoring the worst window is enough (`OI-HEXTILE-30`, `OI-HEXTILE-31`); (c) fix `OI-SESPWR-03`, because the stroke-rehab verdict turns on it. **A ceiling refuses a protocol and never reshapes one** (CLAUDE.md §3). Sets no limit and changes no requirement | Firmware + Safety · counsel |
| **OI-BIBPBM-04** | No skin-type (melanin) term found in any PBM dose or thermal model. The 2025 evidence puts darker-skinned users at ~3× thermal risk, concentrated at 660 nm. Decide whether dose, derating, or labelling addresses it | Safety + Clinical · counsel (Q18) |

---

## 7. Sources

- ICNIRP, *Guidelines on limits of exposure to laser radiation of wavelengths between 180 nm and
  1,000 µm*, Health Phys 105(3), 2013 — https://www.icnirp.org/cms/upload/publications/ICNIRPLaser180gdl_2013.pdf
- ICNIRP, *Guidelines on limits of exposure to incoherent visible and infrared radiation*, Health
  Phys 105(1), 2013 — https://www.icnirp.org/cms/upload/publications/ICNIRPVisible_Infrared2013.pdf
- ANSI Z136.1-2022 (sample) — https://assets.lia.org/s3fs-public/pdf/ansi-standards/samples/Sample%20-%20Z136.1%20-2022-Digital.pdf
- FDA, *PBM Devices — Premarket Notification [510(k)] Submissions*, draft guidance, Federal
  Register 2023-00422 — https://www.federalregister.gov/documents/2023/01/12/2023-00422/photobiomodulation-devices-premarket-notification-submissions-draft-guidance-for-industry-and-food ;
  full text https://www.fda.gov/media/164417/download
- FDA *General Wellness* 2026 revision, summaries — https://www.cov.com/news-and-insights/insights/2026/01/fda-issues-revised-guidance-on-general-wellness-products ;
  https://innolitics.com/articles/fda-guidance-general-wellness-policy-for-low-risk-devices/
- 21 CFR 878.5400 — https://www.law.cornell.edu/cfr/text/21/878.5400
- IEC 60601-2-57:2023 — https://webstore.iec.ch/en/publication/73147 ; EN IEC 60601-2-57:2026 —
  https://standards.iteh.ai/catalog/standards/clc/040badbc-062d-4d61-ac34-aa57b3f3c762/en-iec-60601-2-57-2026
- Intertek, photobiological compliance for medical devices — https://www.intertek.com/blog/2024/09-17-photobiological-compliance-for-medical-devices/
- IEC 60601-1 applied-part temperatures (MDDI) — https://www.mddionline.com/regulatory-quality/iec-60601-1-2005-a-revolutionary-standard-part-2
- Zein, Selting & Hamblin 2018 — https://pmc.ncbi.nlm.nih.gov/articles/PMC8355782/
- Huang et al., biphasic dose response — https://journals.sagepub.com/doi/10.2203/dose-response.09-027.Hamblin
- Barrett & Gonzalez-Lima 2013 context (Gonzalez-Lima & Barrett 2014 review) — https://www.ncbi.nlm.nih.gov/pmc/articles/PMC3953713/
- Transcranial PBM and thermal stimulation, Sci Rep 2021 — https://www.nature.com/articles/s41598-021-97987-w
- tPBM thermal modelling, medRxiv 2025 — https://www.medrxiv.org/content/10.1101/2025.01.18.25320758v1.full
- Skin colour and photosensitivity in PBM, 2025 — https://pmc.ncbi.nlm.nih.gov/articles/PMC12336633/

---

## 8. Revision history

| Rev | Date | Author | Change |
|---|---|---|---|
| 1 | 2026-09-28 | NeurOne Regulatory Affairs | First issue. Literature and regulatory review of PBM irradiance and temperature boundaries, 600–1100 nm. Finds no statutory irradiance limit; records the laser skin limit, IEC 62471, IEC 60601-1 temperature limits and the heating literature; compares R-4, R-5, the 42 °C limit and `OI-SESPWR-02` against them. Raises `OI-BIBPBM-01`…`04`. Sets no limit and changes no requirement |
| 2 | 2026-10-04 | NeurOne Regulatory Affairs | Reads the ICNIRP 2013 laser and incoherent guidelines and the FDA 2023 draft guidance in full (network policy now allows icnirp.org and fda.gov) and relabels the confirmed rows **[P]**. Confirms C_A, the skin limit and its aperture, the >1400 nm scope of the large-area reduction, the incoherent skin and IR-eye limits. **Corrects Rev 1 §3.1 item 1**: ICNIRP bounds every sub-group of pulses, not only the average. Finds the FDA guidance contains no numeric criterion. The weighted-sum formula stays [K]. Narrows `OI-BIBPBM-01` (still open: ANSI, IEC) and raises `OI-BIBPBM-05`. Sets no limit and changes no requirement |
| 3 | 2026-10-04 | NeurOne Regulatory Affairs | Runs the ICNIRP sub-group rule against every `pbm_transcranial` block in the predefined library (§5.1, `OI-BIBPBM-05`): it changes no verdict. Corrects Rev 2's burst bound for a two-channel tile (about 2.0 s, not 3.9 s). `OI-BIBPBM-05` narrowed, not closed. Sets no limit and changes no requirement |
