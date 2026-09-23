# Regulatory Strategy

> Relocated from CLAUDE.md Rev 32 §10 to slim the always-loaded core. Authoritative content for regulatory strategy. Referenced from CLAUDE.md → Document Map.

## T1 — FDA-exempt wellness pathway
- General wellness device (same category as Muse, sens.ai, Apollo Neuro)
- Consumer naming: "Brainwave entrainment stimulation" (not tACS), "Cortical priming stimulation" (not tDCS)
- Required standards: IEC 60601-1, IEC 60601-2-10, IEC 62471, IEC 62133, FCC Part 15
- Cybersecurity: SBOM, vulnerability disclosure policy, documented OTA update approach
- FTC claims substantiation: 33-entry bibliography maps each marketing claim to supporting citations
  - **The EMF shielding claim is not in that bibliography.** CLAUDE.md §1 calls the 4-layer stack (5-layer before the 2026-09-23 Layer 4 deletion) the
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

## T1 → T2 transition — how a unit crosses the tier line (added 2026-09-23, `OI-TACSDRV-06`)

**Nothing above said how a T1 unit becomes a T2 unit, and the answer is a regulatory one.** T1 sits
under FDA's general wellness policy, which is enforcement discretion keyed to **intended use**. It is
not a statutory exemption, whatever the shorthand "FDA-exempt" suggests. T2 is a 510(k) device. So
every upgrade model is an answer to *"when, and by whose act, does a wellness product become a device
intended for a medical purpose?"* The analysis is **`docs/np_reg_upg_001.md` (NP-REG-UPG-001) §6**.
It is an engineering reading, not a regulatory opinion:

| Model | Regulatory consequence, in one line |
|---|---|
| **(a) New unit** (trade-in or second purchase) — **RECOMMENDED, not decided** | No transition happens in the field. Each T2 is built, accepted, labelled and UDI-marked as T2. T1 promotion must not present T1 as upgradeable to a medical device. The residual question is whether T1-released parts may carry over into a T2 |
| **(b) Service conversion** | Manufacturing, not servicing: **depot only**, never a Tier B partner. The 510(k) must cover converted units, and the conversion needs process validation. **Every T1 unit would need DHR-grade build history from day one**, whether or not it is ever converted. It must be a Pre-Submission question |
| **(c) User-installed parts** | The user creates the medical device outside any manufacturing control. It cannot deliver T2 anyway: the tACS driver, the 21-channel ADS1299 bank, the T2 power path and the TMS window are factory-only |

**Seven questions for counsel** (`NP-REG-UPG-001` §6.6) join the **existing RISK-03 engagement**
(GitHub Issue #5), per `NP-REG-PBM1064-001` §1's single-engagement rule. That is `OI-UPG-02`. One of
them reaches beyond the upgrade question: **EU MDR Annex XVI** may bring non-medical brain-stimulation
equipment, and so T1, into MDR scope in the EU, and **Article 16** would make any partner that
converts units the manufacturer. The record's only EU position today is the charger note in
`commercial-model.md` §2.2.

**Needed under every model:** the device holds no tier identity. `NP_PROTO_FLAG_T2_TIER` is read by
no firmware, and T2-D tiles, the cervical VNS accessory and possibly the qEEG cap fit interfaces every
T1 unit has. A T1-labelled unit therefore cannot currently refuse a T2 modality. That is `OI-UPG-01`,
the extension of `NP-PWRSRC-001` D-9's device-bound entitlement to modality gating.
