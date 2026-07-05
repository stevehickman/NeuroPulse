# Electrical & Magnetic Neuromodulation Protocols for Human Neurological Conditions
## TMS · taVNS/VNS · tDCS · tACS

**Sources:** NeuroPulse curated research databases (PRs #165–167) — `docs/NeuroPulse_TMS_database.tsv` (236 studies), `docs/NeuroPulse_VNS_database.tsv` (217), `docs/NeuroPulse_tACS_database.tsv` (108), `docs/tdcs_database_full.csv` (237). Result legend across all four: green ☺ = positive, yellow ☺ (modest)/mixed = unclear, red ☹ = negative/null; ⚔ ref = methodology/guideline.
**Method:** Human studies extracted with full stimulation parameters and outcome. Protocols reflect what converges across the *higher-quality* studies (pivotal RCTs, FDA-cleared paradigms, meta-analyses) and explicitly down-weight or flag the well-documented negative trials. Companion to `docs/pbm_neuro_protocols.md` (photobiomodulation).
**Author:** compiled 2026-07-03.

---

## Evidence-grade key
- **A** — FDA-cleared and/or multiple pivotal RCTs + meta-analytic support; parameters standardized.
- **B** — ≥1 positive RCT plus supporting trials; some negatives attributable to targeting/dose/pairing.
- **C** — Open-label/pilot/mechanistic positive; RCTs absent, mixed, or under-powered.
- **D** — No human clinical trial; projection from mechanism.

## The quality lesson each modality teaches

Each modality has a distinct failure mode that its database makes visible. These drive every protocol below.

- **TMS — target and directionality beat raw dose.** The workhorse (figure-8, left DLPFC, 10 Hz, 120% resting motor threshold, 3000 pulses) is FDA-cleared for depression, but the *non-depression* wins came only when the target moved: OCD failed repeatedly over DLPFC and succeeded over dmPFC/ACC (deep) or SMA/OFC. Excitatory (HF / iTBS) is used over *hypo*active cortex; inhibitory (1 Hz / cTBS) over *hyper*active (contralesional M1 in stroke, left temporoparietal in hallucinations, right IFG in aphasia). iTBS (600 pulses, 3 min) is non-inferior to 37-min 10 Hz (Blumberger 2018); fMRI-guided accelerated dosing (SAINT/SNT: 10 sessions/day × 5 days) reaches ~79–90% remission.
- **VNS — it is a slow, cumulative therapy, and the ear target is specific.** The pivotal *acute* depression RCT was **negative** (Rush 2005) yet long-term registries are positive (Aaronson 2017) — efficacy accrues over months to years (epilepsy: 24% → 44% seizure reduction by 3 yr). For non-invasive taVNS, **cymba conchae beats tragus** (strongest brainstem/NTS activation, Yakunina 2017). "Paired" VNS — timing stimulation to movement (stroke), tones (tinnitus), or extinction trials (PTSD) — is where the mechanism lives.
- **tDCS — it is a primer/modulator, not a driver, and polarity + dose are non-linear.** Anodal = excitatory, cathodal = inhibitory (Nitsche & Paulus 2000), but 2 mA cathodal can flip to excitatory (Batsikadze 2013) — more is not always more. tDCS works best *paired with a task or therapy* (aphasia + speech therapy, stroke + rehab, cognition + training); alone its clinical signal is weak and its depression evidence is genuinely split (Brunoni positive, Loo 2018 null). Epilepsy uniquely uses **cathodal** stimulation over the focus.
- **tACS — frequency-specific, state-dependent, and translation is early.** It entrains endogenous rhythms, so the *frequency* is the dose: 40 Hz gamma for Alzheimer's/cognition (converges with PBM/GENUS), tremor-frequency phase-locked for Parkinson's/essential tremor, alpha for depression, slow-oscillation during sleep for memory. Landmark durable results exist (Reinhart 2019, Grover 2022 — effects lasting ~1 month). But ~75% of scalp current is shunted (Vöröslakos 2018), occipital montages carry retinal-phosphene/peripheral-nerve confounds, and replication failures are real (Klink/Veniero WM null, Clayton attention null). Clinical use is mostly still mechanistic.

**NeuroPulse mapping:** all four modalities exist in the platform — BES/tACS (0.5–40 Hz, ≤1 mA), tDCS/HD-tDCS (0.1–2 mA, safety-MCU charge limit), taVNS (auricular clip) + tcVNS (T2 cervical accessory), and T2 focal TMS. The parameters below are directly loadable as protocol presets, subject to the RISK-03 / 510(k) claims gates.

---

## CROSS-MODALITY: which modality for which condition (best-supported first)

| Condition | Strongest modality | Also supported | Grade of strongest |
|---|---|---|---|
| Depression (MDD) | **TMS** (FDA-cleared) | tDCS (adjunct), taVNS, tACS (alpha) | A |
| OCD | **TMS** (dTMS, FDA-cleared) | tDCS (cathodal SMA/OFC) | A |
| Epilepsy | **VNS** (implanted, FDA-cleared) | tDCS (cathodal focus), taVNS | A |
| Smoking cessation | **TMS** (dTMS, FDA-cleared) | tDCS (DLPFC) | A |
| Migraine / cluster headache | **VNS** (tcVNS gammaCore, FDA-cleared) | TMS (sTMS), tDCS, taVNS | A |
| Stroke motor rehab | **VNS-paired** (Vivistim, FDA-cleared) | TMS (M1), tDCS (M1)+rehab, tACS | A |
| Neuropathic / chronic pain | **TMS** (HF-M1, Level A) | tDCS (M1), tACS (alpha) | A |
| Cognitive enhancement / WM | **tACS** (theta/gamma) / **tDCS** (anodal F3) | TMS (DLPFC) | A/B |
| Post-stroke aphasia | **TMS** (1 Hz right IFG) / **tDCS** (anodal L-IFG)+therapy | — | B |
| Schizophrenia (hallucinations) | **tDCS** (F3+TP) / **TMS** (1 Hz L-TPC) | tACS | B |
| PTSD | **TMS** (right DLPFC) | taVNS (extinction), tDCS | B |
| Parkinson's (motor) | **TMS** (HF-M1) | tACS (tremor/gamma), tDCS+gait, taVNS | B |
| Alzheimer's / dementia | **TMS** (multisite+training) / **tACS** (40 Hz) | tDCS (temporal), VNS | C |
| ADHD | **tDCS** (DLPFC) | TMS, tACS (gamma) | B |
| Fibromyalgia | **TMS** (HF-M1) / **tDCS** (M1) | tACS, taVNS | B |
| Multiple sclerosis (fatigue) | **tDCS** (S1/DLPFC) | TMS (spasticity) | B/C |
| Tinnitus | **VNS-paired**+tones / **TMS** (1 Hz) / **tDCS** | tACS | B/C |
| Anxiety / GAD | **TMS** (right DLPFC) | taVNS, tDCS | B |
| Disorders of consciousness | **tDCS** (anodal L-DLPFC) | taVNS, TMS | B |
| Dysphagia (stroke) | **TMS** / **tDCS** (pharyngeal M1) | — | B |
| Essential tremor / ataxia | **tACS** (phase-locked) / **tDCS** (cerebellar) | TMS (cerebellar) | B/C |
| Insomnia / sleep-memory | **tACS** (slow-osc) / **tDCS** (slow-osc) | TMS, taVNS | B/C |
| Autism | **tDCS** (anodal F3) | TMS (dmPFC) | B |
| Addiction (multiple) | **TMS** (DLPFC/dTMS) / **tDCS** (bilateral DLPFC) | taVNS (craving) | B |

---

# 1. TMS (Transcranial Magnetic Stimulation)

## Master table

| Condition | Grade | Coil | Target | Type / freq | Intensity | Pulses/session | Sessions |
|---|---|---|---|---|---|---|---|
| Depression (MDD) | **A** | Figure-8 / H1 | Left DLPFC | 10 Hz rTMS **or** iTBS 50 Hz | 120% RMT | 3000 (rTMS) / 600 (iTBS) | 20–30 |
| Depression — accelerated (SAINT/SNT) | **A** | Figure-8 | Left DLPFC (fMRI-guided) | iTBS 50 Hz | 90% rMT | 1800 ×10/day | 50 over 5 days |
| Depression — deep | **A** | H1 | Bilateral PFC (L>R) | dTMS 18 Hz | 120% RMT | 1980 | 20–30 |
| OCD | **A** | H7 deep | dmPFC + ACC | dTMS 20 Hz | 100% MT | 2000 | ~29 (6 wk) |
| OCD — alt | **B** | Figure-8 | Pre-SMA / OFC | 1 Hz (inhibitory) | 100% MT | 1200 | 15–20 |
| Smoking cessation | **A** | H4 deep | Bilat lateral PFC + insula | dTMS 10 Hz | 120% MT | 1800 | 18 |
| Neuropathic / chronic pain | **A** | Figure-8 | M1 (somatotopic, contralat.) | HF 5–20 Hz | 80–90% RMT | 1500–2000 | 5–10+ |
| Migraine — acute | **A** | Handheld sTMS | Occipital | single-pulse (×2) | fixed | 2 | PRN/attack |
| Migraine — prophylaxis | **B** | Figure-8 | M1 | 10 Hz | 80–90% RMT | 600 | 3+ |
| PTSD | **B** | Figure-8 | Right DLPFC | 1 Hz or 10–20 Hz | 80–110% MT | 400–1600 | 10–15 |
| Stroke — motor | **B** | Figure-8 | Contralesional M1 (1 Hz) / ipsilesional (HF) | 1 Hz or 3–10 Hz | 90–120% RMT | 300–1200 | 5–10 + rehab |
| Stroke — aphasia | **B** | Figure-8 | Right IFG | 1 Hz (inhibitory) | 90% RMT | 1200 | 10 |
| Schizophrenia (AVH) | **B** | Figure-8 | Left temporoparietal | 1 Hz | 90% MT | ~1000 | 9–12 |
| Parkinson's (motor) | **B** | Figure-8 | M1 (bilateral) | HF 5–25 Hz | 90–110% RMT | 1000 | 8+ |
| Fibromyalgia | **B** | Figure-8 | Left M1 | 10 Hz | 80% RMT | 1500–2000 | 10 + maint. |
| Alzheimer's | **C** | Figure-8 | Multisite (DLPFC+language+parietal) | 10 Hz + cog. training | 90–110% RMT | ~1 hr | 30 |
| Tinnitus | **C** | Figure-8 | Left auditory cortex | 1 Hz | 110% RMT | 2000 | 10 |
| Addiction (cocaine/alcohol/meth) | **B/C** | Figure-8 / H | DLPFC / mPFC | HF 10–15 Hz | 100–110% RMT | 1000–2400 | 10+ |

## Detailed protocols

**Depression (MDD) — Grade A.** 49 studies; foundation of clinical TMS. Standard: figure-8, **left DLPFC, 10 Hz, 120% RMT, 3000 pulses/session (~37 min), 20–30 sessions over 4–6 wk** (O'Reardon 2007 n=301, George 2010 n=190; meta-analysis Berlim 2014 response OR ~3.3). **iTBS** (50 Hz triplets @ 5 Hz, 600 pulses, ~3 min) is non-inferior (Blumberger 2018 n=414) and now preferred for throughput. **Accelerated SAINT/SNT**: fMRI-guided (sgACC-anticorrelated) L-DLPFC, iTBS 90% rMT, 1800 pulses × 10/day × 5 days → remission 78.6% vs 13.3% sham (Cole 2022). **Deep TMS** (H1, bilateral PFC, 18 Hz, 1980 pulses) FDA-cleared (Levkovitz 2015). Right-DLPFC 1 Hz is the low-frequency alternative (Klein 1999). Valid negatives (Yesavage 2018 VA n=164, Herwig 2007 add-on) exist but are outweighed. Durable with taper/maintenance.

**OCD — Grade A.** *Target is everything.* FDA-cleared: **deep TMS H7 over dmPFC + ACC, 20 Hz, 100% MT, 2000 pulses, ~29 sessions** → response 38% vs 11% (Carmi 2019). Alternative: **1 Hz (inhibitory) over pre-SMA or OFC** (Mantovani 2010, Gomes 2012). **DLPFC targets failed repeatedly** (Alonso 2001, Sachdev 2007, Sarkhel 2010, Prasko 2006 — all valid negatives): do not target DLPFC for OCD.

**Smoking cessation — Grade A.** FDA-cleared (first addiction indication): **deep TMS H4, bilateral lateral PFC + insula, 10 Hz, 120% MT, 1800 pulses, 18 sessions** (15 daily + 3 weekly) → quit rate 28% vs 12% (Zangen 2021 n=262). HF L-DLPFC + cue (Amiaz 2009) also works.

**Neuropathic & chronic pain — Grade A (Level A recommendation).** **HF (5–20 Hz) rTMS over M1**, somatotopic to pain area, contralateral, 80–90% RMT, 1500–2000 pulses, ~20 min, repeated sessions (Lefaucheur 2008/2020, Khedr 2005, Hosomi 2013 n=70). Central pain responds better than peripheral. Also fibromyalgia (Passard 2007), phantom limb, CRPS, trigeminal, SCI pain.

**Migraine — Grade A (acute) / B (prevention).** FDA-cleared **single-pulse TMS (handheld, occipital, 2 pulses per attack)** → 2-h pain-free 39% vs 22% (Lipton 2010); also prevention (Starling 2018). HF-M1 rTMS (10 Hz, 600 pulses) for prophylaxis (Misra 2013).

**PTSD — Grade B.** **Right DLPFC**, either HF (10–20 Hz, Cohen 2004, Boggio 2010) or 1 Hz (Watts 2012), 10–15 sessions; augmenting exposure/CPT therapy improves outcomes (Kozel 2018/2019). Deep TMS over mPFC + exposure (Isserles 2013).

**Stroke — Grade B, adjunct to rehab.** Two directional strategies: **1 Hz (inhibitory) over contralesional M1** (Fregni 2006) or **HF over ipsilesional M1** (Khedr 2005, 3 Hz); pair with therapy. Note the multisite **NICHE RCT was negative** (Harvey 2018 n=199) — TMS adds little over intensive rehab alone, so position as adjunct. **Aphasia:** 1 Hz over right IFG (pars triangularis), 1200 pulses, 10 sessions (Thiel 2013, NORTHSTAR 2021). **Neglect:** cTBS over contralesional PPC. **Dysphagia:** HF pharyngeal M1 (Khedr 2009).

**Schizophrenia — Grade B (hallucinations only).** **1 Hz over left temporoparietal cortex, 90% MT, ~1000 pulses, 9–12 sessions** for auditory hallucinations (Hoffman 2003/2005; meta-analysis Slotema 2014, small-moderate). Bilateral DLPFC HF for negative symptoms is weaker (RESIS/Wobrock 2015 negative). 

**Parkinson's (motor) — Grade B.** **HF rTMS over M1 (bilateral), 5–25 Hz, 90–110% RMT, 1000 pulses, ≥8 sessions** improves UPDRS-III (meta-analysis Chou 2015; Khedr 2003). 1 Hz SMA for dyskinesia; DLPFC for PD depression (Pal 2010). iTBS negative (Benninger 2011).

**Alzheimer's — Grade C.** Multisite rotating (DLPFC + Broca + Wernicke + parietal) 10 Hz + cognitive training, ~1 hr, 30 sessions (neuroAD; Rabey 2013, Bentwich 2011) — modest, primary endpoint often missed (Sabbagh 2020). Precuneus 20 Hz for episodic memory (Koch 2018).

**Others (Grade C, brief):** Tinnitus (1 Hz auditory, Folmer 2015 — but large Landgrebe 2017 null); addiction cocaine/alcohol/meth (DLPFC/mPFC HF); Tourette (1 Hz SMA); GAD/panic/social anxiety (right DLPFC); essential tremor & cerebellar ataxia (cerebellar 1 Hz); MS spasticity (iTBS M1) & fatigue; eating disorders, insomnia, disorders of consciousness (DLPFC HF, transient). **ALS negative** (Di Lazzaro 2009).

**Safety (all TMS):** observe Rossi 2009/2021 seizure-risk parameter tables; dose to individual motor threshold; screen for seizure risk factors. Very low seizure incidence with guideline adherence (Lerner 2019).

---

# 2. VNS (Vagus Nerve Stimulation) — all modalities & indications

Unlike the other three modalities, VNS is a **systemic autonomic therapy** whose evidence base extends well beyond neurology into cardiac, gastrointestinal, inflammatory, and metabolic disease. This section covers the **entire** VNS database (217 studies), across all four delivery modalities, not just the auricular taVNS channel.

## The four VNS delivery modalities

| Modality | Route | Typical target | Signature parameters | NeuroPulse fit |
|---|---|---|---|---|
| **Implanted cervical VNS** | Surgical | Left cervical vagus | 20–30 Hz, 0.25–3.5 mA, 250–500 µs, 30 s on / 5 min off, chronic | Reference (predicate for T2 claims) |
| **tcVNS** (e.g. gammaCore) | Transcutaneous neck | Cervical vagus trunk | 25 Hz in **5 kHz bursts**, 1 ms bursts, to tolerance, 120 s doses | **T2 cervical accessory** |
| **taVNS** | Transcutaneous auricular | **Cymba conchae** (> tragus) | 20–25 Hz (1 Hz for migraine), 0.5–6 mA, 200–500 µs, 30 min ×1–2/day or up to 4 h/day | **NeuroPulse-native auricular clip** |
| **VBLOC** (vagal blocking) | Implanted intra-abdominal | Subdiaphragmatic vagal trunks | **5 kHz high-frequency BLOCK** (inhibitory, not stimulation), up to 8 mA, ~12 h/day | Reference only |

Two paradigm distinctions matter throughout: (1) **cervical/implanted stimulation** engages both afferent and efferent fibers and is FDA-cleared for several indications; **auricular taVNS** engages afferents only and is the non-invasive analogue. (2) **VBLOC is not stimulation** — it is a high-frequency conduction *block* used to reduce vagal signaling (obesity), the opposite intent from every other row here.

## Master table — ALL VNS indications

### Neurological & psychiatric

| Condition | Grade | Best modality | Target | Freq | Current | PW | Duty / duration | Course |
|---|---|---|---|---|---|---|---|---|
| Epilepsy | **A** (impl.) / **B** (taVNS) | Implanted / taVNS | L cervical / cymba conchae | 20–30 Hz | 0.25–3.5 / 0.5–1.5 mA | 250–500 µs | 30s on/5min off; ta ≥1–4 h/day | chronic / ≥months |
| Depression (MDD) | **A** (impl.) / **B** (taVNS) | Implanted / taVNS | L cervical / cymba conchae | 20 Hz | titrated / 4–6 mA | 200–500 µs | ta 30 min ×2/day | ≥8–12 wk (accrues over yr) |
| Stroke motor rehab | **A** (paired) | Implanted (Vivistim) / taVNS | L cervical / auricular | 30 Hz | 0.8 mA | 100 µs | 0.5 s bursts **paired to movement** | 6 wk clinic + home |
| Migraine / cluster (acute+prev) | **A** | tcVNS (gammaCore) | Cervical (neck) | 25 Hz (5 kHz bursts) | to tolerance | 1 ms bursts | 120 s ×2–3/day or PRN | per attack / 4–12 wk |
| Migraine — taVNS prophylaxis | **B** | taVNS | cymba conchae | **1 Hz** (not 25) | to tolerance | 250–500 µs | 4 h/day or 30 min ×2 | 12 wk |
| Anxiety / GAD | **B** | taVNS | cymba conchae | 20 Hz | 4–6 mA | 200–300 µs | 30 min ×1–2/day | 8 wk |
| Insomnia | **B** | taVNS | cymba conchae | 20 Hz | 4–6 mA | 200–300 µs | 30 min ×2/day | 8 wk |
| PTSD / fear extinction | **B** | taVNS/tcVNS (paired) | concha / neck | 25 Hz | 0.5 mA / to tol. | 200–500 µs | paired to extinction | task-based |
| Tinnitus | **B/C** | Implanted/taVNS + tones | cervical/auricular | 25–30 Hz | 0.5–0.8 mA | 100–250 µs | paired to tones | 6–12 wk |
| Disorders of consciousness | **C** | taVNS | cymba conchae | 25 Hz | to tolerance | 250 µs | min/day | weeks |
| Pain (acute/experimental/pelvic) | **C** | taVNS | concha / RAVANS | 25 Hz | to tolerance | 250 µs | continuous / resp-gated | perioperative–weeks |
| Parkinson's (gait/balance/cog) | **C** | taVNS | cymba conchae | 25 Hz | to tolerance | 250 µs | min/day | weeks |
| Alzheimer's | **C** | Implanted | L cervical | 20–30 Hz | titrated | 250–500 µs | 30s on/5min off | chronic (6–12 mo) |
| Cognition (memory/executive/arousal) | **C** | taVNS (paired) | concha / cymba | 25 Hz | 0.5 mA | 200–500 µs | task-paired | acute |
| Fibromyalgia | **C** | taVNS | cymba conchae | 20 Hz | to tolerance | 200–300 µs | 30 min/day | weeks |
| Schizophrenia | **C (null)** | taVNS | cymba conchae | 25 Hz | 0.5–1.0 mA | 250 µs | min/day | 26 wk (Hasan 2015 null) |
| Addiction (craving) | **C** | taVNS (cue-paired) | cymba conchae | 20–25 Hz | to tolerance | 200–300 µs | cue-paired | pilot |

### Cardiovascular & autonomic

| Condition | Grade | Best modality | Target | Freq | Current | Duty / duration | Course |
|---|---|---|---|---|---|---|---|
| Autonomic / HRV modulation | **B** (mech.) | taVNS | tragus / cymba | 20–30 Hz | to tolerance | 15–30 min | acute–2 wk |
| Atrial fibrillation | **B** | taVNS | tragus | 20 Hz | 1 mA below discomfort | 1 h/day | acute–6 mo |
| Heart failure | **C / failed (impl.)** | Implanted / taVNS | cervical | 1–10 Hz | titrated | closed-loop / 1 h/day | chronic |
| POTS / dysautonomia | **C** | taVNS | tragus | 20 Hz | to tolerance | 1 h/day | 2 mo |

### Inflammatory & autoimmune

| Condition | Grade | Best modality | Target | Freq | Current | Duty / duration | Course |
|---|---|---|---|---|---|---|---|
| Rheumatoid arthritis | **A/B** (impl.) | Implanted (SetPoint) | L cervical | **10 Hz** | 0.25–2.0 mA | 60 s, 1–4×/day | ≥12 wk |
| Crohn's disease / IBD | **C** | Implanted | L cervical | 10 Hz | 0.25–1.5 mA | 30s on/5min off | 6–12 mo |
| Sjögren / lupus (fatigue) | **C** | taVNS | auricular | — | to tolerance | 4–5 min/day | weeks |
| COVID-19 / long-COVID | **C** | tcVNS / taVNS | neck / cymba | 25 Hz (5 kHz) / 20–25 Hz | to tolerance | 2 min ×2–3/day | days–weeks |
| Systemic inflammation (mech.) | — | Implanted / taVNS | cervical / cymba | 1–10 Hz | 1–5 mA | continuous | — |

### Metabolic & gastrointestinal

| Condition | Grade | Best modality | Target | Freq | Current | Duty / duration | Course |
|---|---|---|---|---|---|---|---|
| Obesity / weight loss | **C / mixed** | VBLOC (block) | subdiaphragmatic trunks | **5 kHz block** | up to 8 mA | ~12 h/day | 12–24 mo |
| Functional dyspepsia / gastroparesis / IBS | **C** | taVNS | cymba conchae | 25 Hz | to tolerance | 300–500 µs, min/day | 4 wk+ |
| Glucose tolerance / metabolic | **C** | taVNS | concha | 20 Hz | to tolerance | 20 min ×2/day | 12 wk |

## Detailed protocols

### Neurological & psychiatric

**Epilepsy — Grade A (implanted) / B (taVNS).** Implanted VNS FDA-cleared: L cervical vagus, 20–30 Hz, 500 µs, 30 s on / 5 min off, chronic — ~50% of patients reach ≥50% seizure reduction, **efficacy increases over 2–3 years** (Handforth 1998 pivotal n=196, Englot 2011 meta n=3321), and long-term VNS is associated with reduced SUDEP. Responsive (cardiac-triggered) stimulation can abort seizures (AspireSR, Boon 2015). **taVNS** (cymba conchae, 20–30 Hz, 0.5–1.5 mA, ≥1–4 h/day) reduced seizures vs sham-ear (Rong 2014 n=98). Caveat: Bauer 2016 found 25 Hz not superior to 1 Hz control — interpret taVNS dosing cautiously.

**Depression (MDD) — Grade A (implanted) / B (taVNS).** Implanted VNS FDA-cleared as a *long-term* adjunct: the acute RCT was **negative** (Rush 2005 n=235) but the 5-yr registry shows cumulative response ~68% and lower mortality vs treatment-as-usual (Aaronson 2017 n=795) — **this is a chronic therapy, judged over months to years, not acutely.** **taVNS** protocol (NeuroPulse-relevant): **cymba conchae, 20 Hz, 4–6 mA to tolerance, 200–300 µs, 30 min twice daily, 8–12 wk** → reduced HAM-D vs sham-ear across multiple RCTs (Rong 2016 n=160, Zhang 2021, Song 2024 multicenter), with limbic/DMN connectivity normalization. Extends to perinatal, adolescent, and bipolar depression (pilots, low manic-switch risk).

**Stroke motor rehabilitation — Grade A (paired).** FDA-cleared **Vivistim**: implanted L cervical vagus, 30 Hz, 0.8 mA, 100 µs, **0.5 s bursts time-locked to rehab movements**, 6 wk in-clinic + home → clinically meaningful arm-function response 47% vs 24% (VNS-REHAB, Dawson 2021 n=108). **taVNS-paired** rehab (auricular, movement-triggered) shows the same direction non-invasively (Capone 2017, Wu 2020, meta Li 2022) — directly relevant to a NeuroPulse auricular + rehab pairing.

**Migraine / cluster headache — Grade A.** FDA-cleared **tcVNS (gammaCore)**: cervical vagus, 25 Hz in 5 kHz bursts, 1 ms bursts, 120 s doses — acute cluster (ACT1/ACT2), acute migraine (PRESTO), and prevention (PREVA, Gaul 2016 chronic cluster). **taVNS prophylaxis paradox:** here **1 Hz beats 25 Hz** (Straube 2015, Zhang 2019, Song 2023) — opposite of the depression/epilepsy frequency. Frequency is condition-specific.

**Anxiety / GAD — Grade B.** taVNS cymba conchae, 20 Hz, 4–6 mA, 30 min ×2/day, 8 wk → reduced HAMA vs sham (Tan 2022, Zhu 2022); emotion-regulation enhancement (Bramson 2023); implanted VNS pilot in refractory anxiety (George 2008). **Insomnia — Grade B:** same parameter set improves PSQI (Jiao 2020, Luo 2017, Zhao 2020).

**PTSD / fear extinction — Grade B (paired).** taVNS (concha, 25 Hz, 0.5 mA) **paired to extinction trials** enhances extinction memory and dampens defensive responses (Burger 2016, Szeska 2020); tcVNS blunts sympathetic/inflammatory stress reactivity (Gurel 2020, Lamb 2017). Translational rationale from rat paired-VNS (Noble 2017).

**Tinnitus — Grade B/C (paired + tones).** Implanted VNS + tone pairing (30 Hz, 0.8 mA, 100 µs, tone-paired, 2.5 h/day; Tyler 2017 subgroup, from Engineer 2011 animal work) reverses maladaptive auditory plasticity; taVNS + tailored sound is the non-invasive analogue (Suk 2018, Ylikoski 2017). The *pairing* is essential.

**Disorders of consciousness — Grade C.** Implanted VNS transitioned a vegetative patient toward minimally conscious (Corazzol 2017 case); taVNS (cymba conchae, 25 Hz) produced CRS-R gains in subsets (Yu 2017, Vitello 2023 RCT crossover). **Pain — Grade C:** taVNS raises pain thresholds and reduces postoperative/pelvic pain (Usichenko 2017, Napadow 2012 respiration-gated), though some experimental-pain studies are null (Janner 2018). **Parkinson's — Grade C:** taVNS improves gait/balance acutely (Sigurdsson 2021, Mondal 2021). **Alzheimer's — Grade C:** implanted VNS stabilized/improved cognition at 6–12 mo (Sjögren 2002, Merrill 2006, uncontrolled). **Fibromyalgia — Grade C:** taVNS reduced pain/fatigue (Yang 2023 RCT).

**Cognition — Grade C, parameter-sensitive.** Foundational: post-learning implanted VNS improved word recognition (Clark 1999); taVNS during encoding improves memory and modulates LC-NE markers (Ventura-Bort 2018, Jacobs 2015 older adults), executive control (Beste 2016, Sellaro 2015), and pupil/arousal (Sharon 2021). **But many null RCTs exist** (Jongkees 2018 WM, Pihlaja 2020 attention, Keute 2019 inhibition, Mertens 2020 memory) — acute cognitive benefit is real but fragile and dose/site-dependent; do not over-claim.

**Schizophrenia — Grade C (largely null).** taVNS did not beat sham on symptoms in a 26-wk RCT (Hasan 2015); only pilot cognitive signals (Zhu 2019). **Addiction — Grade C:** cue-paired taVNS reduced craving (Cai 2022 pilot).

### Cardiovascular & autonomic

**Autonomic / HRV — Grade B (mechanistic).** taVNS (tragus or cymba conchae, 20–30 Hz) acutely **increases HRV and shifts toward parasympathetic predominance**, reduces muscle sympathetic nerve activity, and improves baroreflex sensitivity (Clancy 2014, Machetanz 2021, Bretherton 2019 in aging). Right-ear and exhalation-gated delivery enhance cardiovagal engagement (De Couck 2017, Sclocco 2019). **Note parameter sensitivity:** Keute 2018 found no reliable acute HRV change at 0.5 mA — dose matters. *This is the mechanistic backbone of the NeuroPulse VNS+HRV clip and HRV-biofeedback protocols.*

**Atrial fibrillation — Grade B.** taVNS tragus, 20 Hz, 1 mA below discomfort, 1 h/day → reduced AF burden and inflammatory cytokines (Stavrakis 2015 crossover n=40, TREAT-AF 2020 n=53), and reduced post-ablation recurrence (Yu 2017). One acute electrophysiology study was null (Gauthey 2020).

**Heart failure — Grade C / failed for implanted.** Open-label implanted VNS improved ejection fraction and NYHA class (ANTHEM-HF, De Ferrari 2011), but **two pivotal RCTs were negative** (INOVATE-HF n=707 — no reduction in death/HF events; NECTAR-HF — no remodeling benefit). A well-documented implanted-VNS failure. taVNS pilot only (Zhou 2019). **POTS — Grade C:** taVNS tragus reduced postural tachycardia (Stavrakis 2021).

### Inflammatory & autoimmune (the "inflammatory reflex")

Mechanistic basis: the vagus efferent arm suppresses TNF and systemic inflammation (Tracey 2002 inflammatory reflex; Borovikova 2000 animal — efferent VNS blocked endotoxic shock).

**Rheumatoid arthritis — Grade A/B (implanted).** Implanted L cervical VNS, **10 Hz** (note: lower than epilepsy/depression), 0.25–2.0 mA, 250 µs, **60 s, 1–4×/day** (very low duty) → reduced TNF and DAS28 (Koopman 2016 n=17, first human proof of the inflammatory reflex; SetPoint micro-regulator Genovese 2020) and **positive pivotal RCT** in refractory RA (Marsal 2025). taVNS analogue reduced TNF in a small cohort (Addorisio 2019).

**Crohn's disease / IBD — Grade C.** Implanted L cervical VNS, 10 Hz, 0.25–1.5 mA, achieved clinical + endoscopic remission in most patients (Bonaz 2016 n=7, Sinniger 2020 12-mo n=9). **Sjögren / lupus — Grade C:** taVNS reduced fatigue and inflammatory markers (Tarn 2019, Aranow 2021). **COVID-19 / long-COVID — Grade C:** tcVNS trended to faster recovery and lower CRP (SAVIOR, Tornero 2022); taVNS reduced long-COVID fatigue/dysautonomia (Baptista 2023).

### Metabolic & gastrointestinal

**Obesity — Grade C / mixed (VBLOC vagal *blocking*).** Intra-abdominal **high-frequency (5 kHz) vagal block**, up to 8 mA, ~12 h/day → weight loss that met a co-primary but not the superiority margin (ReCharge, Ikramuddin 2014 n=239; maintained at 24 mo, Apovian 2017). The earlier EMPOWER RCT was **negative** (high sham response). Distinct paradigm — *reducing* vagal signaling. **Glucose tolerance — Grade C:** taVNS concha, 20 Hz, 20 min ×2/day, 12 wk improved glucose tolerance (Huang 2014).

**Functional GI — Grade C.** taVNS (cymba conchae, 25 Hz) increases vagal tone and gastric motility, improving functional dyspepsia + gastric accommodation (Zhu 2021 RCT), gastroparesis symptoms (Gottfried-Blackmore 2020), and IBS pain (Hong 2022); also modulates appetite/eating behavior (Teckentrup 2020, Kozorosky 2022).

## VNS cross-cutting notes

1. **Frequency is indication-specific:** 20–30 Hz (epilepsy, depression, stroke, most taVNS); **1 Hz** (taVNS migraine prophylaxis); **10 Hz, very low duty** (RA, IBD — anti-inflammatory); **5 kHz bursts** (tcVNS gammaCore); **5 kHz continuous block** (VBLOC obesity — inhibitory).
2. **Judge on the right timescale:** VNS efficacy accrues over months–years (epilepsy, depression) — the negative *acute* depression RCT and the positive 5-yr registry are the same therapy.
3. **Respect the documented failures:** heart failure (INOVATE-HF, NECTAR-HF both negative), obesity EMPOWER (negative), schizophrenia (Hasan 2015 null), and many null acute taVNS cognition/HRV/attention studies. Parameter and timing precision separate signal from noise.
4. **taVNS optimization:** target **cymba conchae** (not tragus) for strongest brainstem/NTS engagement (Yakunina 2017); right-ear or exhalation/inspiration-gated delivery enhances cardiovagal effects (Sclocco 2019). NeuroPulse HRV-synchronized taVNS (stimulation phase-locked to respiration/inspiration) aligns directly with this respiration-gated evidence.
5. **NeuroPulse channels:** the auricular clip covers the taVNS indication set (depression, epilepsy, anxiety, insomnia, HRV/autonomic, GI, inflammatory); the T2 cervical accessory covers the tcVNS set (migraine, cluster, PTSD). The implanted-VNS and VBLOC rows are predicate/reference for 510(k) argumentation, not device functions.

---

# 3. tDCS / HD-tDCS (Transcranial Direct Current Stimulation)

**Universal parameters:** 1–2 mA, 20–30 min, sponge electrodes ~25–35 cm² (density ~0.03–0.08 mA/cm²) or HD 4×1 ring for focality. **Anodal = excitatory, cathodal = inhibitory.** Best paired with a task/therapy.

## Master table

| Condition | Grade | Polarity | Anode | Cathode | mA | min | Sessions |
|---|---|---|---|---|---|---|---|
| Cognition (healthy WM) | **A** | Anodal | Left DLPFC (F3) | R supraorbital | 1–2 | 10–20 | 1+ (with task) |
| Depression | **B** | Anodal | Left DLPFC (F3) | F4 / F8 / R supraorbital | 2 | 20–30 | 10–22 |
| Chronic pain / fibromyalgia | **B** | Anodal | M1 (C3/C4) | Contralat. supraorbital | 2 | 20 | 5–10 |
| Stroke motor rehab | **B** | Anodal (or dual) | Ipsilesional M1 | Contralesional M1 / supraorbital | 1–2 | 20–40 | 5–10 + rehab |
| Aphasia (post-stroke) | **B** | Anodal | Left IFG / perilesional | R supraorbital | 1 | 20 | 5–15 + therapy |
| Schizophrenia (AVH) | **B** | Anodal+cathodal | Left DLPFC (F3) | Left temporoparietal | 2 | 20 | 10 |
| Addiction / craving | **B** | Bilateral | Right DLPFC (F4) | Left DLPFC (F3) | 2 | 20 | 1–5+ |
| Epilepsy | **B** | **Cathodal** | Epileptogenic focus | Contralateral | 1–2 | 20–30 | 1–14 |
| ADHD | **B** | Anodal | L-DLPFC (F3) or R-DLPFC (F4) | contralat. | 1–2 | 20–30 | 5–28 |
| MS fatigue | **B** | Anodal | S1 (bilateral) or L-DLPFC | occipital / supraorbital | 1.5–2 | 15–20 | 5–20 |
| Alzheimer's / dementia | **C** | Anodal | Bilateral temporal / L-DLPFC | deltoid / supraorbital | 1.5–2 | 15–30 | 1–10 |
| Parkinson's (motor/gait/cog) | **C** | Anodal | M1 leg area / DLPFC | contralat. supraorbital | 2 | 13–20 | 1–10 |
| Dysphagia (stroke) | **B** | Anodal | Pharyngeal M1 | contralat. supraorbital | 1–2 | 20–30 | 5–10 |
| Disorders of consciousness | **B** | Anodal | Left DLPFC (F3) | R supraorbital | 2 | 20 | 1–5 |

## Detailed protocols

**Cognitive enhancement (healthy) — Grade A.** **Anodal over left DLPFC (F3), cathode right supraorbital, 1–2 mA, 10–20 min, applied during a working-memory task** improves WM accuracy/speed (Fregni 2005, Keeser 2011). Polarity-specific (anodal ↑, cathodal ↓; Zaehle 2011). Also language/word-retrieval (anodal left IFG/temporoparietal, Floel 2008), math, planning. Effects require concurrent engagement.

**Depression — Grade B (genuinely mixed).** **Anodal F3 / cathode F4 (or F8), 2 mA, 20–30 min, 10–22 sessions.** Positive: Boggio 2008 (n=40), Brunoni 2013 factorial (tDCS + sertraline best), home-based Woodham/Fu 2024 (n=174). But **tDCS < escitalopram** (Brunoni 2017 ELECT-TDCS) and several outright nulls (Loo 2018 n=120, Palm 2012, Blumberger 2012). Position as an accessible **adjunct**, not monotherapy. Higher density (25 cm² pads, 0.08 mA/cm²) in the positive trials.

**Chronic pain / fibromyalgia — Grade B.** **Anodal M1 (C3/C4), cathode contralateral supraorbital, 2 mA, 20 min, 5–10 sessions** (Fregni 2006 fibromyalgia — M1 > DLPFC; Valle 2009 effect to ~60 days). HD-tDCS 4×1 ring also effective (Villamar 2013). Extends to SCI central pain, migraine, phantom limb, trigeminal neuralgia. Some nulls (Luedtke 2015 back pain, Wrigley 2013 SCI).

**Stroke motor rehab — Grade B (with therapy).** **Anodal ipsilesional M1**, or **dual (anode ipsilesional / cathode contralesional)**, or **cathodal contralesional**, 1–2 mA, 20–40 min, 5–10 sessions **paired with rehab/CIMT/robot** (Hummel 2005, Lindenberg 2010 dual, Bolognini 2011 + CIMT). Weak alone (Hesse 2011 robot-only null, Rossi 2013 acute null). **Aphasia:** anodal left IFG + speech therapy, 1 mA, 20 min, up to 15 sessions (Fridriksson 2018 n=74). **Dysphagia:** anodal pharyngeal M1 (Kumar 2011). **Neglect:** anodal right PPC.

**Schizophrenia (hallucinations) — Grade B.** **Anode left DLPFC (F3) + cathode left temporoparietal, 2 mA, 20 min ×2/day, 10 sessions** → robust hallucination reduction lasting 3 mo (Brunelin 2012); also negative symptoms (anodal F3, Valiengo 2020 n=100). Multisite Kantrowitz 2019 mixed.

**Addiction / craving — Grade B.** **Bilateral DLPFC, right-anodal / left-cathodal (or reverse), 2 mA, 20 min** reduces craving across alcohol (Klauss 2014 ↓relapse), smoking (Fecteau 2014), cocaine (Batista 2015), cannabis, methamphetamine, and food craving (Fregni 2008). Repeated sessions for durability.

**Epilepsy — Grade B (cathodal).** Uniquely **cathodal (inhibitory) over the epileptogenic focus, cathode contralateral, 1–2 mA, 20–30 min** reduces seizures ~44% (Fregni 2006 n=19; San-Juan 2017 effect ≤2 mo; Yang 2020 multicenter n=70). Also pediatric focal (Auvichayapat 2013) and LGS spasms.

**ADHD — Grade B.** Anodal left DLPFC (F3) or right DLPFC (F4), 1–2 mA, 20–30 min; adult inattention (Leffa 2022 n=64, Cachoeira 2017), children WM/inhibition (Soff 2017, Breitling 2016 right-IFG). Single-session can be null (Cosmo 2015) — repeated dosing needed.

**MS — Grade B (fatigue/pain).** Anodal S1 or whole-somatosensory (Ferrucci 2014, Tecchio 2014) or L-DLPFC (Chalah 2017), 1.5–2 mA, 15–20 min for fatigue; anodal M1 for MS neuropathic pain (Mori 2010). Home/remote feasible (Charvet 2018).

**Others (Grade C):** Alzheimer's (anodal bilateral temporal or L-DLPFC → ↑MMSE/recognition memory, Khedr 2014/2019, but Bystad 2016 home null); Parkinson's (anodal M1-leg for gait/FOG, DLPFC for executive/depression; multitarget > single, Dagan 2018); disorders of consciousness (anodal L-DLPFC → CRS-R gains in MCS not VS, Thibaut 2014 n=55); autism (anodal F3 → ↓CARS, Amatachaya 2014); cerebellar ataxia (anodal cerebellar + spinal, Benussi 2018 persisted 3 mo); PTSD, tinnitus (bifrontal), sleep/insomnia (slow-oscillating anodal frontal → ↑N3/memory, Marshall 2006), cerebral palsy, Tourette (cathodal M1/SMA), stuttering (anodal left IFC), dyslexia (children).

**Dose/safety notes:** dose is **non-linear** (2 mA cathodal can flip excitatory — Batsikadze 2013); HD-tDCS (4×1) gives more focal, longer after-effects; NeuroPulse safety-MCU 40 µC/cm² charge-density ceiling and per-electrode limits bound all of the above (T2 HD-tDCS uses the sLORETA-guided 4×1 ring).

---

# 4. tACS / tRNS (Transcranial Alternating Current Stimulation)

**Frequency is the dose** — tACS entrains endogenous rhythms. Typical: 1–2 mA, ~16 cm² sponges (or HD ring), 10–40 min. Translation is earliest-stage of the four; strongest evidence is cognitive/mechanistic, with emerging clinical use in AD, PD, and depression.

## Master table

| Application | Grade | Montage / target | Frequency | Intensity | Duration | Notes |
|---|---|---|---|---|---|---|
| Working memory (theta) | **A/B** | Frontoparietal F3+P3 / F4+P4, **in-phase** | 6 Hz theta | 1 mA | 20 min | in-phase ↑, anti-phase ↓ (Polania 2012) |
| WM restoration (older adults) | **A** | Frontotemporal HD, phase-tuned | individual theta | HD | 25 min | Reinhart 2019 — lasted ~50 min |
| WM + long-term memory | **A** | Parietal (theta) + PFC (gamma), HD | theta / gamma | HD | 20 min ×4 days | Grover 2022 — lasted 1 month |
| Fluid intelligence / cognition | **B** | Left prefrontal (F3-Cz) | 40 Hz gamma | 1 mA | — | Santarnecchi 2013 |
| Alzheimer's | **C** | Temporal / temporoparietal | **40 Hz gamma** | 2–3 mA | 60 min | ↑ episodic memory, ↑ hippocampal CBF (Benussi 2021) |
| Parkinson's — tremor | **B** | M1 contralateral, **phase-locked** | tremor freq (4–6 Hz) | ≤2 mA | short | ~50% tremor suppression (Brittain 2013) |
| Parkinson's — bradykinesia | **C** | M1-orbit | **gamma** (60–70 Hz; not beta) | 1–2 mA | — | gamma ↑, beta ↓ (frequency-specific) |
| Depression (MDD) | **C** | Bifrontal / F3-F4 | 10 Hz alpha | 2 mA | 40 min | 5+ sessions; ↑ remission (Alexander 2019) |
| Essential tremor | **C** | Cerebellum, phase-locked | tremor freq | ≤2 mA | short | transient suppression (Schreglmann 2021) |
| Sleep / memory consolidation | **C** | Frontal, SO-locked | 0.75 Hz slow-osc | ~1.5 mA | during sleep | ↑ spindles + memory (Ketz 2018) |
| Schizophrenia (AVH) | **C** | Left frontotemporal (F3+T3) | 10 Hz alpha | 2 mA | 20 min | 5 daily; trend ↓ hallucinations |
| ADHD | **C** | Frontal | 40 Hz gamma | 1–2 mA | — | ↑ attention (Dallmer-Zerbe 2020 pilot) |
| Chronic pain / fibromyalgia | **C** | S1 / somatosensory | 10 Hz alpha | 1–2 mA | 20–40 min | ↓ pain sensitivity (Ahn 2019) |

## Detailed protocols

**Working memory & cognition — Grade A/B (strongest tACS evidence).** **Theta (~6 Hz) frontoparietal tACS delivered in-phase** improves WM; anti-phase impairs it — causal phase-coupling (Polania 2012, Violante 2017). Two landmark durable results: **Reinhart & Nguyen 2019** (frontotemporal phase-tuned theta restored WM in 60–76-yr-olds to young-adult level) and **Grover 2022** (parietal-theta for WM + prefrontal-gamma for long-term memory, HD, 20 min × 4 days, **benefit lasted 1 month**). Gamma (40 Hz) prefrontal improves fluid reasoning (Santarnecchi 2013). Caveat: replication failures exist (Klink/Veniero 2020 theta null) — effects are state- and phase-dependent.

**Alzheimer's — Grade C (converges with 40 Hz GENUS/PBM).** **40 Hz gamma tACS over temporal/temporoparietal cortex, 2–3 mA, 60 min** improved episodic memory + cholinergic markers (Benussi 2021 crossover), increased hippocampal CBF (Sprugnoli 2021), and suggested reduced tau PET (Dhaynaut 2022). This is the same 40 Hz gamma target as PBM Mode-40Hz and the mastoid vibrotactile GENUS accessory — a **multi-modal 40 Hz convergence** worth exploiting.

**Parkinson's — Grade B (tremor) / C (bradykinesia).** **Phase-locked tACS at the tremor frequency over M1** suppresses resting tremor ~50% by phase cancellation (Brittain 2013) — a closed-loop, EEG/accelerometer-driven paradigm ideal for NeuroPulse's adaptive architecture. For bradykinesia, **gamma (60–70 Hz) helps and beta (20 Hz) worsens** (Krause 2014, Guerra 2020) — frequency choice is directional.

**Depression — Grade C.** **Alpha (10 Hz) bifrontal tACS, 2 mA, 40 min, 5+ daily sessions** reduced left-frontal alpha and raised remission at follow-up (Alexander 2019, Riddle 2020). A larger 77.5 Hz montage RCT (Wang 2022) also reduced scores.

**Sleep / memory & essential tremor & others (Grade C):** slow-oscillation (0.75 Hz) tACS time-locked during NREM sleep boosts spindles and declarative memory (Marshall 2006 precursor, Ketz 2018); phase-locked cerebellar tACS transiently suppresses essential tremor (Schreglmann 2021); alpha S1 tACS reduces pain (Ahn 2019 fibromyalgia); 10 Hz left-frontotemporal for schizophrenia hallucinations (Mellin 2018); 40 Hz frontal for ADHD attention (Dallmer-Zerbe 2020). **Temporal interference (TI)** — kHz carriers with a low-frequency beat — is an emerging way to reach deep targets (hippocampus) non-invasively (Grossman 2017, Violante 2023).

**Caveats (all tACS):** ~75% scalp-current shunting means 1–2 mA may be subthreshold for deep targets (Vöröslakos 2018); occipital montages produce **retinal phosphenes** and peripheral-nerve co-stimulation confounds (Kar & Krekelberg 2012, Asamoah 2019); tACS-EEG artifacts mimic entrainment (Noury 2016). Keep intensity ≤4 mA (safety consensus, Antal 2017).

---

## CROSS-CUTTING PRINCIPLES

1. **Match directionality to the target's state.** Excite hypoactive cortex (TMS HF/iTBS, tDCS anodal, tACS resonant frequency); inhibit hyperactive cortex (TMS 1 Hz/cTBS, tDCS cathodal). Epilepsy is the clearest case: TMS/tDCS both go *inhibitory* over the focus.
2. **Target > frequency for non-depression TMS.** OCD's DLPFC failures vs dmPFC/SMA successes are the canonical lesson.
3. **Pairing is a mechanism, not a garnish.** VNS-movement (stroke), VNS-tones (tinnitus), VNS-extinction (PTSD), tDCS-task (cognition), tDCS-therapy (aphasia/rehab) — the paired modalities carry the effect.
4. **VNS accrues; don't judge it acutely.** The negative acute depression RCT and positive 5-yr registry are the same therapy at different timescales.
5. **tDCS dose is non-linear and tACS is state/phase-dependent** — higher/longer is not automatically better; both have real replication failures. Weight FDA-cleared TMS/VNS paradigms accordingly.
6. **40 Hz gamma is a cross-modality convergence** (tACS, PBM, GENUS audio/visual/vibrotactile) for Alzheimer's and cognition — NeuroPulse can stack these on one clock.
7. **Respect documented nulls:** TMS — no DLPFC for OCD, acute-stroke TMS adds little over rehab (NICHE), large tinnitus RCT null; VNS — acute depression RCT null, many null taVNS cognition/HRV studies; tDCS — depression < SSRIs, several outright nulls; tACS — occipital phosphene/peripheral confounds, WM/attention replication failures.
8. **Closed-loop is the frontier.** fMRI-guided accelerated TMS (SAINT), phase-locked tACS (tremor cancellation), respiration-gated taVNS, and EEG-adaptive dosing all outperform fixed open-loop delivery — exactly NeuroPulse's autonomous EEG-adaptive design point.

*These are research-derived design/scientific protocols, not clinical prescriptions or regulatory-cleared claims. Device deployment remains gated by RISK-03 (T1 wellness) and the T2 510(k) pathway.*
