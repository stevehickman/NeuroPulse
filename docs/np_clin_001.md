# Clinical Trials Strategy

**Project:** NeurOne
**Document:** NP-CLIN-001
**Revision:** 2
**Date:** 2026-11-25
**Status:** ACTIVE — pre-decisional strategy document. It sets no requirement, and nothing in it is a product claim.
**Effective Date:** —
**Author:** NeurOne Clinical
**Approved By:** — (pending Clinical review of the Rev 2 evidence ratings)
**References:** `docs/reference/clinical.md` (evidence summary; the HOPE read-out and its sources); `NP-ACC-PRIORITY-001` §8 `OI-ACC-10` (mastoid pad release gate); `NP-FW-HUB-001` §13 `OI-FWHUB-37` (40 Hz photic driving and seizures); `NP-TOOL-SHELL-002` S2-F01; `docs/np_bib_001.docx` (bibliography)
**Related Issues:** `OI-CLIN-01`; GitHub #30 (HOPE Phase 3, closed), #31 (mastoid pad)
**Gate:** None.
**IEC 62304 Class:** N/A
**Supersedes:** `docs/superseded/np_clin_001.docx`, the Word issue of this document's Rev 1 (written as Rev A, May 2026), carried into Markdown at Rev 2.

> **Rev 2 (2026-11-25): the 40 Hz evidence is revised after HOPE Phase 3 (`OI-CLIN-01`), and the document moves from Word to Markdown.**
> Cognito's pivotal HOPE trial missed its primary endpoint (CTAD, 2026-11-16). §2 rates 40 Hz visual stimulation for Alzheimer's **Mixed**, no longer *Very Strong*. §3.2 and §3.6 give the result and stop calling NeurOne's protocol a replication of GENUS. §5.4 drops the conditional on a positive HOPE result. §6 priority 5 gains a post-HOPE design note, and its rank is not changed. §7 adds the HOPE entry. Everything else is the Rev 1 text, carried over verbatim from the Word file, with one exception: §7's bibliography filename is corrected to `np_bib_001.docx`. **Not re-checked in this revision:** the non-40 Hz evidence, the researcher table and the trial budgets. Read them as of May 2026.
>
> **Why Markdown.** Revision numbers are assigned at merge (`docs/revisions/README.md`), and the assigner numbers only Markdown documents. A Word document therefore has no path to a new revision. The Word file is kept in `docs/superseded/` as the record of Rev 1. `editscripts/generate_clinical_trials_doc.py` no longer produces this document and must not be re-run for it.

---

## 1. Commercial Rationale — The All-in-One Proposition

NeurOne does not require multi-modal synergy claims to succeed commercially. Each of the 8 T1 modalities (11 in T2) carries independent clinical evidence. The primary value proposition is straightforward:

- **Lower cost:** One device replaces up to 8 separate devices (PBM headset, neurofeedback system, tDCS/tACS stimulator, VNS device, audio entrainment player, visual stimulation goggles, intranasal probe, HRV monitor). Cumulative retail cost of equivalent individual devices: $8,000–$25,000+. NeurOne Home Standard: $849.
- **Less clutter:** One wearable platform, one charging cable, one app, one subscription. No device management overhead for clinicians or patients.
- **Modality choice:** Users and clinicians choose which modalities to use. Someone familiar only with tDCS uses only tDCS. A neurofeedback practitioner adds PBM on top without replacing their workflow. No multi-modal protocol required.
- **Multi-modal upside:** If simultaneous multi-modal use produces synergistic effects — and early evidence suggests it does for some combinations — NeurOne is the only consumer platform positioned to demonstrate and deliver those effects at scale.

### 1.1 Why Vielight's 35+ RCTs Partially Support NeurOne

Vielight has published or supported more than 35 clinical trials of transcranial and intranasal PBM. These trials are directly relevant to NeurOne — but with important distinctions that determine how they can be used in marketing and regulatory contexts.

**WHAT TRANSFERS:**

- The Vielight trials establish that transcranial + intranasal PBM at 810nm is safe, feasible for home use, and produces measurable neurological effects across cognition, PTSD/TBI, and sleep outcomes. NeurOne's bibliography cites these trials to establish that PBM is a legitimate, evidence-backed modality.
- For FDA general wellness positioning and FTC claims substantiation, the Vielight body of evidence is relevant prior art. NeurOne can cite it as background scientific context supporting the modality category.
- The Vielight Neuro EEG study (2025) — proving that itPBM reorganises neural oscillations and functional network connectivity — directly supports NeurOne's combined PBM + EEG neurofeedback architecture.

**WHAT DOES NOT TRANSFER:**

- Device equivalence: Vielight RCTs used Vielight devices at specific irradiance, duty cycle, and LED count. NeurOne (dual-wavelength, 400 mW/cm² pulsed, individually addressable hex-tile PBM modules across the socket lattice with real-time per-module J/cm² dose metering) has no published comparator. Any marketing claim implying NeurOne produces outcomes 'at least as good as Vielight' requires head-to-head data or regulatory counsel opinion.
- Multi-modal interaction: No published trial tests PBM combined with BES, tDCS, VNS, audio entrainment, and visual stimulation simultaneously. Vielight trials are single-modality (PBM only). If NeurOne makes outcome claims implying benefit from the combined protocol, those require NeurOne-specific data.
- 400 mW/cm² irradiance claim: RISK-03 (regulatory opinion pending) — this specific claim cannot appear in any public material until regulatory counsel clears it, regardless of what Vielight claims for their own devices.

*PRACTICAL RULE: Cite Vielight trials freely to establish that 'PBM is clinically studied and evidence-supported.' Do not cite them to claim that NeurOne produces equivalent or superior outcomes — that requires NeurOne-specific data or regulatory opinion.*

## 2. Individual Modality Clinical Evidence

| **Tier** | **Modality** | **Evidence Strength** | **Primary Indications** | **Key Trials (n, result)** | **Key Researchers** |
|---|---|---|---|---|---|
| T1 | Transcranial PBM <br> 660nm + 810nm | Moderate-Strong | MCI/Dementia, TBI/PTSD, Depression, Healthy cognition | Rashidi-Ranjbar 2025 (n=15, MCI, multimodal neuroimaging, AJGPsych); Brazilian BDNF RCT 2024 (n=93, MoCA +3.20 vs +1.97, BDNF↑, p=0.03); Naeser 2023 (n=4, CTE/PTSD, 2 SD improvement); Chao/UCSF 2019 (n=8 dementia, DMN connectivity improved); Yao et al. Science Advances 2022 (1064nm, working memory capacity) | Rashidi-Ranjbar (Toronto); Naeser (VA Boston); Hamblin (MGH); Jeffery (UCL, retinal) |
| T1 | Intranasal PBM <br> 660nm + 810nm | Moderate <br> (always combined with transcranial in trials) | MCI/Dementia, Healthy cognition | Japanese RCT Protocol 2024 (Frontiers Neurology, 12-week sham-controlled, intranasal + transcranial, MCI/mild AD); Vielight EEG study 2025 (proves PBM reorganises neural oscillations — supports PBM+neurofeedback combination); Chun et al. 2025 (Korean RCT, MCI/mild AD) | Lim (Vielight); Rashidi-Ranjbar; Hamblin |
| T1 | EEG Neurofeedback | Strong (ADHD) <br> Moderate (PTSD, anxiety) <br> Emerging (depression) | ADHD, PTSD, Anxiety, Depression, Cognitive enhancement | Wu et al. Brain & Behavior 2024 (13 RCTs, n=1,370 children, TBR/SMR/SCP/HEG); Meta-analysis (21 RCTs, n=1,261, ADHD); Frontiers Psychiatry 2024 (PTSD moderate-to-large effect); MDPI Signals 2024 (EEG-adaptive closed-loop binaural beats, n=24, 96% reached target vs 0% sham) | Arns (Brainclinics NL); Strehl (Tübingen); Stinear (Auckland) |
| T1 | tACS / BES <br> (Brainwave Entrainment Stimulation) | Moderate <br> (contested by pub. bias analyses) | Working memory, Depression, MCI/AD (40Hz) | STM meta-analysis 2023 (102 studies, n=2,893, modest-to-moderate effect); Frohlich tACS-MDD RCT 2022 (n=255, remote, rapid improvement wk1, p=0.022); Frontiers Psychiatry meta-analysis 2023 (4 RCTs, n=224 MDD); Gamma-tACS review Transl Neurodegeneration 2024 (40Hz modifies AD progression) | Frohlich (UNC Chapel Hill, depression); Reinhart (BU, WM); Herrmann (Oldenburg) |
| T1 | tDCS <br> (Cortical Priming Stimulation) | Strong <br> (depression, large RCTs) | Depression (esp. MDD), MCI/AD cognition, Motor rehabilitation | Brunoni et al. Nature Medicine 2024 (n=174 home tDCS, 44.9% remission vs 21.8% sham); Jog/UCLA 2025 (n=71, personalized HD-tDCS, significant mood improvement + gray matter changes, PMC12426800); JCM meta-analysis 2025 (14 RCTs, tDCS for AD cognition); DepressionDC Lancet 2023 (n=150, null result — add-on SSRIs) | Jog (UCLA); Brunoni (São Paulo); Bikson (CCNY, safety/dosimetry) |
| T1 | Auricular VNS + HRV <br> (taVNS) | Strong <br> (insomnia, d=1.2) <br> Moderate <br> (epilepsy, post-stroke) | Insomnia, Epilepsy, Post-stroke depression, Working memory | JAMA Network Open 2024 (n=72, insomnia RCT, PSQI −8.2 vs −3.9, d=1.2, sustained 20 weeks); Yang et al. (n=150, drug-resistant epilepsy, seizure↓); Pan et al. CNS N&T 2024 (n=28, working memory, EEG network stabilisation); Scoping review 2024 (109 studies, 21 clinical populations) | Multiple groups; rapidly growing field |
| T1 | Neural Audio Entrainment <br> (binaural beats + bone conduction) | Moderate <br> (anxiety, sleep) <br> Weak <br> (cognition — entrainment inconsistent) | Anxiety, Stress, Sleep, Cognitive enhancement (closed-loop) | PLOS ONE systematic review 2023 (22 studies, 36% confirmed entrainment, medium effect cognition/anxiety/pain); MDPI Signals 2024 (first EEG-adaptive binaural beats RCT, n=24, 96% target); 8-RCT meta-analysis (acoustic stimulation improves insomnia severity); Stress RCT (salivary cortisol + alpha-amylase, significant in 2 well-designed studies) | Emerging; no dominant research group |
| T1 | Visual Stimulation <br> (40Hz gamma / EMDR / photic driving) | **Mixed (40Hz AD):** the pivotal trial missed its primary endpoint, and only a mild subgroup was positive, on nominal statistics <br> Strong (EMDR/PTSD) <br> Limited (general photic driving) | Alzheimer's disease, PTSD, Cognitive entrainment | Cognito/OVERTURE Phase 2 2024 (n=76, mild-to-moderate AD, 76% ↓cognitive decline, 69% ↓brain atrophy, 77% ↓functional decline); Chan et al. Phase 2A (n=15 mild AD, ↓ventricular dilation, ↑DMN, ↑memory); **HOPE Phase 3 (n=673, 12 months): primary endpoint missed**, with no difference from sham on iADMARS. A prespecified mild subgroup (MMSE 21–28, n=395) was positive on nominal, multiplicity-unadjusted statistics. Seizures 0.9% vs 0.3% sham (§3.2). The Phase 2 figures above came from a 76-person trial, and the pivotal trial did not reproduce them on its primary endpoint; >30 EMDR RCTs; WHO-recommended for PTSD | Tsai (MIT Picower, GENUS; SAB candidate); Cognito Therapeutics (Phase 3) |
| T2 | TMS <br> (Transcranial Magnetic Stimulation) | Very Strong <br> (FDA-cleared, CPT codes) | Treatment-resistant depression, MCI, OCD (FDA-cleared) | George/Neuronetics pivotal RCT 2008 (n=301, FDA clearance); Meta-analysis JAMA Psychiatry 2017 (Cohen d=0.40); BRIGhTMIND Nature Medicine 2024 (n=255 TRD, connectivity-guided iTBS); George/MUSC 2023 (first EEG-synchronized rTMS RCT, alpha phase-locked stimulation); Pooled data: 46.81% response, 28.25% remission, Cohen d=1.4 | George (MUSC); Williams (Stanford, accelerated iTBS); Blumberger (CAMH, theta burst) |
| T2 | 1170nm Deep PBM <br> (Laser diodes) | Preclinical only <br> (no human RCT at 1170nm) | Subcortical / deep structure (theoretical) | Theoretical depth advantage (35–40mm): no published human RCT at 1170nm specifically. 1064nm cognition data (Yao/Science Advances 2022) is closest. First-mover opportunity for NeurOne-sponsored human pilot. | Hamblin (tPBM review author); Yao/UT Dallas (1064nm cognition) |

## 3. Multi-Modal Combination Evidence

The central research gap: no published RCT has tested PBM + BES + VNS + audio + visual simultaneously in adults. However, several two-modality combinations have published evidence, and the trajectory of the field is clearly toward combination protocols. NeurOne is the only consumer platform positioned to run these studies at scale.

### 3.1 tDCS + taVNS — Simultaneous Synergy Demonstrated

**This is the strongest published multi-modal evidence directly applicable to NeurOne.**

- **Synergy confirmed:** Aghajani et al., Brain Stimulation 2021 (PMID 33621676) — First simultaneous tDCS + taVNS fMRI study. Synergistic activation in bilateral thalamus, pallidum, parahippocampal gyrus, dorsal raphe, substantia nigra, periaqueductal gray. Combined effect > sum of individual effects.
- **RCT:** Frontiers in Neuroscience 2022 (PMC9344917) — First RCT, simultaneous tDCS + taVNS, working memory in healthy adults. Three-arm: tDCS alone / taVNS alone / combined. Combined > either alone; more consistent improvements in 3-back spatial accuracy. Effect depended on task difficulty, not stimulation site — suggests an attentional or arousal mechanism rather than purely local cortical effect.
- **Active RCT:** Post-stroke cognitive impairment RCT (PMC11015246, 2024) — 3-arm protocol (tDCS alone / taVNS alone / combined) recruiting at First Affiliated Hospital Yangtze University (n=66 target). Primary: MoCA. Results expected 2025.
- **NeurOne relevance:** NeurOne delivers tDCS (Cortical Priming Stimulation) and taVNS (auricular VNS) simultaneously from a single platform. This is the only published synergistic combination NeurOne can claim today, with RCT support.

### 3.2 40Hz Audiovisual (GENUS) — Pivotal Trial Missed Its Primary Endpoint

- **Phase 2A:** Cognito Therapeutics Phase 2A (n=15 mild AD, Chan et al. 2022/2025): combined 40Hz visual flicker + 40Hz audio click trains daily. Outcomes: ↓ventricular dilation, ↓hippocampal atrophy, ↑DMN connectivity, ↑face-name delayed recall.
- **Phase 2 (OVERTURE):** OVERTURE Phase 2 (n=76, Frontiers Neurology 2024): 76% reduction in cognitive decline, 77% slowing of functional decline, 69% reduction in whole-brain atrophy. OLE (18 months): ~9 months time saved in cognitive function.
- **Phase 3 (HOPE):** read out on 2026-11-16 (Cognito Therapeutics at CTAD, 2026-11-16, as reported by NeurologyLive, Medscape and Fierce Biotech; not yet checked against a peer-reviewed publication). 673 people with biomarker-confirmed mild-to-moderate AD were randomised (663 analysed) to 12 months of Spectris (40Hz light + sound) or sham. **Primary endpoint missed:** iADMARS (MMSE + ADCS-ADL) did not differ from sham at month 12. A prespecified mild subgroup (MMSE 21–28, n=395) favoured the device on all five measures (ADCS-ADL 56% and CDR-SB 47% slower decline; composite nominal p=.04). Those figures are not adjusted for multiplicity, so they point to a next trial and support no claim. Cognito's 82% figure comes from a per-protocol group of 169 high-adherence users and cannot show causation. Seizures: 0.9% on the device vs 0.3% on sham, a handful of events in a population whose baseline risk is raised (`NP-FW-HUB-001` `OI-FWHUB-37`).
- **What the Phase 2 figures are worth now:** OVERTURE's 76% / 77% / 69% come from 76 people. The pivotal trial, at nearly nine times the size, did not reproduce them on its primary endpoint. Cite them only alongside the HOPE result, never alone.
- **NeurOne relevance:** The goggles (108 LEDs/lens, 6 zones/eye) can drive 40Hz visual flicker alongside 40Hz audio (binaural beats + bone conduction), the same two senses GENUS stimulates, but today they do not (see **Safety** below). **It is not shown to be equivalent to Spectris:** the light source, the audio stimulus and the dose all differ, so no Cognito result, positive or negative, transfers to NeurOne. T1 is FDA-exempt wellness, so no Alzheimer's claim is made in any case. EEG-adaptive closed-loop delivery is an architectural difference from Cognito's fixed schedule. Whether it improves outcomes is untested (§6, priority 5). **Safety:** the helmet refuses photic driving at 3–60Hz until its Oz photoparoxysmal detector is built and validated (`NP-FW-HUB-001` `OI-FWHUB-37`, closed 2026-11-25; HOPE's seizure imbalance was one input, the photosensitivity literature the stronger one), so 40Hz visual flicker is not delivered at all. The shipped gamma protocols run 40Hz audio and PBM without the goggles (the principal, 2026-11-26, `OI-NPPS-LIMITS-01`); a 40Hz audiovisual protocol in the GENUS sense waits on the detector (`OI-FWHUB-29`).
- **Emerging:** Tsai lab (MIT, 2023): 40Hz tactile vibration (wristband) also reduces AD pathology in mice. Multi-sensory (visual + audio + tactile simultaneously) is the next preclinical direction. No human RCT of 40Hz tactile stimulation exists. NeurOne does not currently support 40Hz tactile vibration. The mastoid pad's release gate is `NP-ACC-PRIORITY-001` `OI-ACC-10`, because HOPE never tested a tactile channel.

### 3.3 PBM + qEEG Neurofeedback + HRV Biofeedback — Published RCT (2025)

- **First published three-modality RCT:** ResearchGate January 2025: 'Efficiency of a Multimodal Neurotechnological Intervention (Cerebral Photobiomodulation, qEEG Neurofeedback, and Heart-Brain Biofeedback) in Enhancing Attention and Academic Performance in Middle School Students.' Nationally conducted RCT. Three modalities simultaneously — directly relevant to NeurOne's PBM + EEG + VNS/HRV stack. Population is children; adult replication needed.
- **Mechanistic foundation:** Vielight 2025 publication (high-level journal): itPBM reorganises neural oscillations and functional network connectivity. Confirms that PBM directly changes the EEG signals that neurofeedback trains — making PBM + EEG neurofeedback mechanistically coherent, not merely additive.
- **Review framework:** PubMed 31647776, 2019: 'Treatment of Neurodegeneration: Integrating Photobiomodulation and Neurofeedback in Alzheimer's Dementia and Parkinson's' — review establishing the theoretical framework. PBM builds better neural infrastructure; neurofeedback trains connectivity within it.

### 3.4 tDCS + EEG Neurofeedback — Emerging Evidence

- **Published RCT:** Frontiers in Neuroscience 2023: 'tDCS + Motor Imagery Neurofeedback — RCT.' 10 min tDCS before NFB session. NFB+tDCS group showed improved motor imagery vs NFB alone. Priming cortical excitability enhances subsequent neurofeedback learning window.
- **Meta-analysis:** ScienceDirect meta-analysis 2022: simultaneous tDCS + working memory training produces significant transfer effects vs training alone.
- **Active RCT:** JMIR Research Protocols 2022: double-blind RCT (tDCS + NFB for declarative memory) enrolled July 2022–2023. Results expected.
- **Mechanism:** Mechanism: tDCS primes neurons to be more responsive during the neurofeedback training window; timing (tDCS before or concurrent with NFB) is the key variable. NeurOne can sequence or layer these modalities under firmware control.

### 3.5 PBM + tDCS — Research Gap; High-Priority Opportunity

- **Current status:** JCM 2025 systematic review (17 RCTs, 2020–2024): tDCS and PBM each independently improve cognitive abilities in AD. Authors explicitly call for standardisation to enable future combination trials. No published RCT of PBM + tDCS together exists.
- **Mechanistic rationale:** PBM increases mitochondrial ATP and BDNF. tDCS modulates cortical excitability via LTP/LTD-like neuroplasticity. These are complementary, non-overlapping mechanisms. Better-energized neurons may respond more strongly to tDCS-induced plasticity.
- **Research opportunity:** NeurOne is uniquely positioned to run the first PBM + tDCS combination RCT for MCI/AD cognition. Rashidi-Ranjbar (Toronto) and Jog (UCLA) are natural co-PIs.

### 3.6 Multi-Modal Combination Evidence Summary

| **Combination** | **Evidence Level** | **Best Citation(s)** | **NeurOne Relevance** |
|---|---|---|---|
| tDCS + taVNS (simultaneous) | Synergy demonstrated <br> (fMRI + WM RCT) | Brain Stimulation 2021; Frontiers Neuroscience 2022; Post-stroke RCT enrolling 2024 | High — cite today |
| 40Hz visual + audio (GENUS) | **Mixed** <br> (Phase 2 positive; Phase 3 missed its primary endpoint; mild subgroup nominal only) | Cognito/Chan 2022; OVERTURE 2024 (n=76); HOPE Phase 3 (n=673, CTAD 2026-11-16) | Low for any outcome claim. The pivotal trial missed, and NeurOne's stimulus is not shown equivalent to Spectris. Research interest in the mild subgroup and in EEG-adaptive delivery (§6, priority 5) |
| PBM + qEEG NFB + HRV biofeedback | Moderate <br> (One 2025 RCT; children) | ResearchGate 2025 (nationally conducted RCT) | Moderate — adult replication needed |
| tDCS + motor imagery neurofeedback | Emerging <br> (Published RCT 2023) | Frontiers Neuroscience 2023; JMIR protocol 2022 | Moderate — motor rehabilitation context |
| tDCS + cognitive training (WM) | Moderate <br> (Meta-analysis) | ScienceDirect meta-analysis 2022 | Moderate |
| PBM + tDCS | Gap — no RCT <br> (review only) | JCM 2025 review; mechanistic rationale strong | High research priority — NeurOne first-mover |
| PBM + tACS | Gap — no data | Hypothesis only | Research priority |
| PBM + taVNS | Gap — no data | Hypothesis only | Research priority |
| tACS + EEG neurofeedback | Framework only <br> (Bikson closed-loop) | bioRxiv 2023; tACS EEG personalization paper 2025 | Research priority — EEG personalises tACS |
| taVNS + EEG neurofeedback | Implicit overlap <br> (EEG signal change) | Pan et al. 2024 (taVNS changes EEG during WM) | Research priority |

## 4. Key Researchers by Modality

| **Researcher** | **Institution** | **Contact** | **Focus Area(s)** | **Why NeurOne** | **Role** | **Priority** |
|---|---|---|---|---|---|---|
| Neda Rashidi-Ranjbar | St. Michael's Hospital / Unity Health Toronto | neda.rashidi-ranjbar@unityhealth.to | tPBM (intranasal + transcranial), MCI/AD, multimodal neuroimaging | PI of 2025 pilot RCT (n=15, AJGP). CIHR pathway. Most current PBM RCT PI in Canada. Early-career — motivated collaborator. Natural first contact. | TRIAL PI | 1 |
| Mayank Jog | UCLA Brain Mapping Center | mjog@mednet.ucla.edu | HD-tDCS, personalized stimulation, depression, structural neuroplasticity | K99/R00 NIH MH128572 active. PI of n=71 personalized HD-tDCS RCT (PMC12426800, 2025). Gray matter plasticity data (Sci Reports 2023). JAMA Network Open 2025 RCT PI. | TRIAL PI | 2 |
| Mark George | MUSC Brain Stimulation Lab | georgem@musc.edu <br> 843-876-5142 | TMS (all forms), EEG-synchronized TMS, accelerated rTMS, MCI | 55 active studies, 15 TMS machines. Pioneer: helped achieve FDA TMS clearance 2008. First-in-human EEG-synchronized rTMS RCT (Brain Stimulation 2023) — directly aligned with NeurOne EEG-adaptive architecture. MUSC MCI accelerated TMS trial 2023. | TRIAL PI (T2) | 3 |
| Margaret Naeser | VA Boston / BU Neurology | mnaeser@bu.edu | tPBM (TBI, PTSD, CTE, stroke, aphasia), veteran cohort | 47 years VA-funded. 2023 publication: n=4 ex-football players (CTE/PTSD), significant improvement PTSD/depression/pain/sleep. Virtual care TBI/PBM trial active. Existing veteran patient cohort = ready recruitment pool. | TRIAL PI | 4 |
| Li-Huei Tsai | MIT Picower Institute | 617-324-0305 | 40Hz GENUS (visual + audio + tactile), Alzheimer's, multi-sensory gamma | Founder of GENUS field (Nature 2016). Director MIT Aging Brain Initiative. SAB role ONLY — Cognito Therapeutics conflict prevents PI role. Expanding 40Hz multi-sensory to tactile vibration. Key scientific credentialing for NeurOne visual + audio modalities. | SAB (highest priority) | 5 |
| Glen Jeffery | UCL Institute of Ophthalmology | g.jeffery@ucl.ac.uk | Retinal PBM (670nm), age-related retinal decline, AMD | World's leading retinal PBM researcher. Shinhmar 2021: single 3-min 670nm = 20% cone-mediated color vision improvement in older adults. AMD pilot 2020 (n=42). Natural SAB candidate for NeurOne Mode F (invisible NIR retinal PBM) scientific credentialing. | SAB | 6 |
| Marom Bikson | CCNY Neural Engineering Group | neuralengr.org/bikson | tDCS (safety, dosimetry, HD-tDCS), closed-loop neuromodulation architecture | 2023 bioRxiv: scalable closed-loop neuromodulation framework (DL + EEG), modality-agnostic (tACS/tDCS/tFUS/TMS). tDCS safety data cited by FDA. Technical advisory role for NeurOne's closed-loop architecture. | TECHNICAL ADVISOR | 7 |
| Robert Reinhart | Boston University | reinhart@bu.edu | tACS (theta/gamma), working memory, depression, aging | Corresponding author: STM 2023 tACS meta-analysis (102 studies, n=2,893). Phase-specific tACS for working memory and depression. | TRIAL PI | 8 |
| Flavio Frohlich | UNC Chapel Hill | frohlichlab.org | tACS (alpha oscillations), depression, remote clinical trials | Remote tACS RCT (n=255 MDD, JCP 2022). Phase-specific alpha tACS. Infrastructure for large remote tACS trials. | TRIAL PI | 9 |

## 5. Researcher Proposals and Speculation on Multi-Modal Synergy

The following represents published proposals, theoretical frameworks, and expert speculation about combining modalities — not yet tested in clinical trials.

### 5.1 Closed-Loop Architecture as the Unifying Framework

Bikson et al. (bioRxiv 2023) propose a deep-learning EEG-adaptive closed-loop framework agnostic to stimulation modality. The system reads EEG state in real time and adjusts stimulation parameters (frequency, phase, amplitude) across any modality. The key insight: the stimulation modality (tACS vs tDCS vs TMS) is less important than the real-time EEG guidance that ensures stimulation is delivered at the neurologically optimal moment.

*NeurOne's autonomous closed-loop EEG architecture is the consumer implementation of exactly this framework. The value is not that any single modality is better — it is that all modalities can be guided by the same real-time neural state.*

### 5.2 PBM as a Neuroplasticity Primer for Other Modalities

Proposed mechanism (PubMed 31647776, 2019; JCM 2025 review): PBM increases mitochondrial ATP production, BDNF, and synaptic density. This creates 'better neural infrastructure' that makes other plasticity-inducing modalities (tDCS, tACS, neurofeedback) more effective. Analogous to exercise priming BDNF before a learning session.

*If true: PBM delivered first in a session sequence, followed by tDCS or neurofeedback, would produce greater neuroplastic change than either modality alone. This is testable with NeurOne's sequencing firmware capabilities. No human RCT exists yet — NeurOne could commission the first.*

### 5.3 taVNS as a Neuromodulatory Primer (Noradrenergic Enhancement)

Proposed mechanism: taVNS activates the locus coeruleus (noradrenaline) and basal forebrain (acetylcholine), both of which modulate cortical plasticity and memory consolidation. Running taVNS concurrently with tDCS, PBM, or neurofeedback could enhance plasticity via neuromodulator gating — the same mechanism underlying the tDCS + taVNS synergy demonstrated in the 2021 Brain Stimulation fMRI study.

*The confirmed tDCS + taVNS synergy (thalamus, parahippocampal gyrus) provides proof-of-concept that taVNS amplifies effects of concurrent stimulation modalities. Extending this to PBM + taVNS or neurofeedback + taVNS is a logical next step that only NeurOne can test at consumer scale.*

### 5.4 Multi-Sensory 40Hz — From Two Senses to Three

Tsai lab (MIT, 2023): 40Hz whole-body vibrotactile stimulation reduces amyloid and tau in mouse models. The Tsai group is exploring whether combining visual, auditory, and tactile 40Hz stimulation simultaneously produces stronger effects than any two-sense combination. HOPE Phase 3 (n=673) tested visual + auditory only and missed its primary endpoint (§3.2); tactile remains preclinical.

*NeurOne currently delivers 40Hz visual and auditory simultaneously, the same two senses as GENUS but not shown equivalent to it (§3.2). The condition this paragraph used to rest on, a positive HOPE result, did not occur. A 40Hz vibrotactile component (the mastoid pad) now waits on human evidence for tactile stimulation itself (`NP-ACC-PRIORITY-001` `OI-ACC-10`). The anchor boss for it stays in the shell tooling, inactive (`NP-TOOL-SHELL-002` S2-F01).*

### 5.5 EEG-Synchronized TMS (George, MUSC 2023) — Closed-Loop Precision

George et al. (Brain Stimulation 2023): first-in-human RCT of EEG phase-locked rTMS. When rTMS pulses are synchronized to the patient's prefrontal alpha phase: (a) strong phase entrainment develops in DLPFC, (b) anterior cingulate cortex is more reliably engaged, (c) inter-individual variability in response is reduced. The EEG phase at the moment of stimulation — not the average EEG — determines outcome.

*NeurOne's EEG-adaptive architecture can implement this for all non-TMS modalities in T1 (tDCS, tACS, PBM sequencing) and for TMS in T2. This is the evidence base for NeurOne's core architectural claim: EEG-guided delivery is better than fixed-schedule delivery.*

## 6. Recommended Clinical Trial Priority

| **Priority** | **Trial** | **Lead Researcher** | **Design** | **Budget / Timeline** | **Start** |
|---|---|---|---|---|---|
| 1 | SBIR Phase I <br> PBM for MCI/Cognition | Rashidi-Ranjbar (Toronto) <br> or Naeser (VA Boston) | NeurOne tPBM vs sham. n=20–30, 12 weeks. Replicate Vielight design with NeurOne hardware. Generates first NeurOne-specific PBM data; enables comparative claims. | $150K–$300K \| 12–18 months \| SBIR Phase I funding vehicle | NOW |
| 2 | tDCS for Depression <br> (Home-Based Replication) | Jog (UCLA) or Brunoni group | NeurOne tDCS (CPS) vs sham, MDD, home-based. n=40–60. Replicate Brunoni Nature Medicine 2024 design. Establishes NeurOne tDCS evidence; leverages existing protocol. | $200K–$400K \| 12–18 months | YEAR 1 |
| 3 | tDCS + taVNS Synergy <br> for Cognitive Impairment | Any MCI research center; <br> University collaboration | Three-arm: NeurOne tDCS alone / taVNS alone / combined. n=45–60. Replicate Frontiers Neuroscience 2022 design in MCI population. First NeurOne-specific multi-modal synergy data. | $250K–$400K \| 18 months | YEAR 1–2 |
| 4 | PBM + tDCS Combination <br> for MCI (First-in-Class) | Rashidi-Ranjbar + Jog <br> (multi-site) | Four-arm: PBM alone / tDCS alone / combined / sham. n=60–80. First human RCT of PBM + tDCS simultaneously. Genuine research first; high publication impact. JCM 2025 review explicitly calls for this trial. | $400K–$600K \| 18–24 months \| R21 mechanism | YEAR 2 |
| 5 | EEG-Adaptive 40Hz AV <br> (GENUS Replication + EEG) | Tsai lab (SAB, not PI) <br> or Rashidi-Ranjbar | NeurOne 40Hz visual + audio (GENUS protocol) + EEG-adaptive closed-loop vs Cognito fixed-schedule. Mild AD, n=30–50. Tests whether EEG guidance improves on fixed GENUS delivery. **After HOPE (2026-11-16):** the only signal HOPE found was in its mild subgroup (MMSE 21–28), so enrolment should be limited to that range. The study needs seizure monitoring and a stopping rule (`OI-FWHUB-37`). A 30–50 person study cannot confirm a subgroup effect that a 673-person trial could not establish on its primary endpoint. Its rank is not changed here; that is Clinical's decision | $350K–$500K \| 18 months | YEAR 2 |
| 6 | T2 — EEG-Synchronized <br> TMS for TRD | George (MUSC) <br> Target: SBIR Phase II | EEG phase-locked rTMS via NeurOne T2 platform vs standard rTMS. Extends George's 2023 first-in-human RCT with larger n and NeurOne-specific hardware. Generates T2 TMS data. | $800K–$2M \| 24–36 months \| SBIR Phase II | YEAR 3 |

## 7. Key Bibliography

*Cite-ready format. See `np_bib_001.docx` for the full 33-entry bibliography.*

**[tPBM — MCI RCT]** Rashidi-Ranjbar N et al. 'tPBM for MCI.' Am J Geriatric Psychiatry 2025; preprint medRxiv 2025.08.19.25333989. (n=15, multimodal neuroimaging)

**[tPBM — BDNF RCT]** Brazilian group. 'tPBM increases cognition and BDNF in adults over 50.' Photobiomodulation Photomedicine Laser Surg 2024. (n=93, MoCA Δ3.20 vs 1.97, p=0.03)

**[tDCS — Depression (large)]** Brunoni AR et al. 'Home tDCS for MDD.' Nature Medicine 2024. (n=174, 44.9% remission vs 21.8% sham)

**[tDCS — Depression (null)]** Brunoni AR et al. 'DepressionDC tDCS + SSRIs.' The Lancet 2023. (n=150, null result)

**[tDCS — Personalized HD]** Jog MA et al. 'Personalized HD-tDCS for depression.' PMC12426800, 2025. (n=71, significant, gray matter changes)

**[40Hz AV — Phase 2A]** Chan D et al. 'GENUS 40Hz audiovisual, mild AD.' Alzheimer's & Dementia 2025. (n=15, ↓atrophy, ↑DMN)

**[40Hz AV — Phase 2 (OVERTURE)]** Cognito Therapeutics. 'OVERTURE.' Frontiers Neurology 2024. (n=76, 76% ↓cognitive decline, 69% ↓brain atrophy; not reproduced on the Phase 3 primary endpoint; cite only with HOPE)

**[40Hz AV — Phase 3 (HOPE)]** Cognito Therapeutics. HOPE pivotal results, presented at CTAD, 2026-11-16. (n=673 randomised, 663 analysed, biomarker-confirmed mild-to-moderate AD, 12 months, sham-controlled; primary endpoint iADMARS missed; prespecified mild subgroup MMSE 21–28, n=395, nominal benefit; seizures 0.9% vs 0.3%.) Conference presentation and press coverage only (NeurologyLive, Medscape, Fierce Biotech); cite the peer-reviewed paper once it appears.

**[taVNS — Insomnia]** JAMA Network Open 2024. 'taVNS for chronic insomnia.' (n=72, d=1.2, PSQI −8.2 vs −3.9)

**[taVNS — Working memory]** Pan et al. 'taVNS + working memory in epilepsy.' CNS Neurosci Ther 2024. (n=28, 20 weeks, EEG network stabilisation)

**[tDCS+taVNS — fMRI synergy]** Aghajani H et al. Brain Stimulation 2021. PMID 33621676. (fMRI synergy: thalamus, pallidum, parahippocampal gyrus)

**[tDCS+taVNS — WM RCT]** Frontiers Neuroscience 2022 (PMC9344917). 'Simultaneous tDCS + taVNS on working memory.' RCT, combined > either alone.

**[PBM+NFB — Review]** PubMed 31647776, 2019. 'Integrating PBM and Neurofeedback in AD and Parkinson's.'

**[PBM+NFB+HRV — RCT]** ResearchGate January 2025. 'Multimodal PBM + qEEG NF + HRV biofeedback for attention.' Nationally conducted RCT (middle schoolers).

**[EEG NF — ADHD meta-analysis]** Wu et al. Brain & Behavior 2024. 13 RCTs, n=1,370. TBR/SMR/SCP/HEG.

**[tACS — Meta-analysis]** Science Translational Medicine 2023. 102 studies, n=2,893, modest-to-moderate cognitive improvement.

**[TMS — EEG-synchronized]** George MK et al. Brain Stimulation 2023. (PMC10872322). First EEG phase-locked rTMS RCT. Alpha-synchronized = better ACC engagement.

**[TMS — Connectivity-guided iTBS]** BRIGhTMIND. Sackeim HA et al. Nature Medicine 2024. (n=255 TRD)

**[EMDR]** Systematic review PMC11111257, 2024. >30 RCTs. WHO-recommended for PTSD.

**[Closed-loop framework]** Bikson M et al. bioRxiv 2023. 'Scalable closed-loop neuromodulation with deep learning.' Modality-agnostic EEG-adaptive architecture.

**[tPBM + tDCS review]** JCM 2025. 'Efficacy of tDCS and PBM in AD.' 17 RCTs 2020–2024. Calls for combination trial.

---

## Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-05-02 | NeurOne Clinical | **Initial issue, as a Word document** (written as Rev A, May 2026; generated by `editscripts/generate_clinical_trials_doc.py`). Kept at `docs/superseded/np_clin_001.docx`. |
| 2 | 2026-11-25 | NeurOne Clinical | **Carried from Word into Markdown, the Rev 1 text kept verbatim apart from the 40 Hz content (OI-CLIN-01). §2: 40 Hz visual for Alzheimer's rated Mixed, no longer Very Strong. §3.2, §3.6: HOPE Phase 3 missed its primary endpoint (iADMARS, n = 673), the mild subgroup is nominal, seizures 0.9 % vs 0.3 %, and NeurOne is no longer called a replication of GENUS. §5.4: drops the positive-HOPE conditional and points the tactile pad at OI-ACC-10. §6 priority 5: post-HOPE design note, rank unchanged. §7: HOPE entry added, bibliography filename corrected.** |
