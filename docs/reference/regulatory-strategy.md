# Regulatory Strategy

> Relocated from CLAUDE.md Rev 32 §10 to slim the always-loaded core. Authoritative content for regulatory strategy. Referenced from CLAUDE.md → Document Map.

## T1 — FDA-exempt wellness pathway
- General wellness device (same category as Muse, sens.ai, Apollo Neuro)
- Consumer naming: "Brainwave entrainment stimulation" (not tACS), "Cortical priming stimulation" (not tDCS)
- Required standards: IEC 60601-1, IEC 60601-2-10, IEC 62471, IEC 62133, FCC Part 15
- Cybersecurity: SBOM, vulnerability disclosure policy, documented OTA update approach
- FTC claims substantiation: 33-entry bibliography maps each marketing claim to supporting citations
  - **The EMF shielding claim is not in that bibliography.** CLAUDE.md §1 calls the 5-layer stack the
    product's primary technical differentiator and `competitive-position.md` carries published copy for
    it, but no entry here, in `clinical.md` or in the 33-entry set covers it. The evidence record is
    **`docs/np_bib_emf_001.md` (NP-BIB-EMF-001)**, added 2026-09-15. Read it before substantiating any
    shielding claim: it establishes that a **dB attenuation figure is a product-performance claim and
    never an efficacy claim**, that no shielded-vs-unshielded outcome evidence exists for any modality,
    and that the FTC has enforced against unsubstantiated EMF-protection claims (WaveShield 2003;
    sticker settlements 2011; 5G-protection warnings 2020).
  - **⚠ IEC 60601-1-2 (EMC) is missing from the required-standards line above** and is named for the
    cervical VNS accessory in `NP-REG-CVNS-001` §192. Its IEC 61000-4-8 power-frequency magnetic
    immunity clause may be what actually requires shielding Layer 2 — `OI-BIBEMF-04`, which **gates any
    proposal to remove that layer**. Resolve before the standards list is relied on.

## T2 — FDA 510(k)
- Modular predicate: TMS (NeuroStar K083538, BrainsWay K122288) + tACS (Soterix K142485, Neuroelectrics K173185) + taVNS (electroCore K163334, K173323)
- Timeline: 18–36 months from T1 launch, $2–5M budget
- **QMS (21 CFR Part 820 / ISO 13485:2016): ESTABLISHED at company formation 2026-05-13** — NP-QMS-001 Rev 1 (manual), NP-DHF-001 Rev 1 (DHF index), NP-QMS-DC-001 Rev 1 (design controls), NP-RM-001 Rev 1 (risk management), NP-SW-001 Rev 1 (IEC 62304 SW plan), NP-QMS-CAPA-001 Rev 1 (CAPA procedure). All pre-formation design documents retroactively entered under change control. See Issue #33.
- Pre-Submission (Q-Sub) meeting with FDA at ~Month 20: free, prevents filing on avoidable grounds
- IEC 62304 software classification: Safety MCU → **Class C** · Main processor → **Class B** · App → **Class B** — formalized in NP-SW-001 Rev 1
- Clinical data: required for TMS modality; seeded T2 units into research institutions (Years 2–3) generate this data
- Human factors engineering (FDA 2016 HFE Guidance): URRA + formative + summative testing — governed by NP-HFE-001 (planned Month 9)
