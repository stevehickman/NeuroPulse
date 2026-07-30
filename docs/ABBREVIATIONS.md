# NeurOne Design Abbreviations Glossary

## Product & Regulatory

| Abbr. | Expansion | Context |
|-------|-----------|---------|
| **T1** | Tier 1 (NeurOne Home) | FDA-exempt wellness device |
| **T2** | Tier 2 (NeurOne Pro) | FDA 510(k) clinical device |
| **FDA** | Food and Drug Administration | U.S. regulatory authority |
| **510(k)** | Section 510(k) premarket notification | FDA pathway for substantial equivalence |
| **DHF** | Design History File | Design control documentation for medical devices |
| **QMS** | Quality Management System | ISO 13485 medical device compliance |

## Neuromodulation & Clinical Terminology

| Abbr. | Expansion | Context |
|-------|-----------|---------|
| **PBM** | Photobiomodulation | Light therapy (660–1170nm) |
| **EEG** | Electroencephalography | Brain electrical activity recording (8–21 channels) |
| **qEEG** | Quantitative EEG | Digital analysis of EEG data; T2 includes sLORETA source imaging |
| **VNS** | Vagal Nerve Stimulation | Stimulation via auricular branch (CN X) |
| **tcVNS** | Transcutaneous Cervical VNS | T2 accessory: stimulation at carotid sheath (cervical vagus trunk) |
| **HRV** | Heart Rate Variability | Autonomic nervous system marker; measured via PPG in VNS clip |
| **BES** | Brainwave Entrainment Stimulation | Oscillating current stimulation (0.5–40Hz) |
| **tACS** | Transcranial Alternating Current Stimulation | Sinusoidal current (0–40Hz, ≤1mA T1; 16-ch ≤4mA T2) |
| **tDCS** | Transcranial Direct Current Stimulation | Direct DC stimulation (0.1–2mA, 40µC/cm² limit) |
| **HD-tDCS** | High-Definition tDCS | 4×1 sLORETA-guided montage (T2 only) |
| **TMS** | Transcranial Magnetic Stimulation | Magnetic pulse stimulation (0.1–0.5T, T2 only) |
| **rTMS** | Repetitive TMS | Pulsed TMS protocol |
| **TBS** | Theta Burst Stimulation | 3-pulse theta-frequency TMS pattern |
| **sLORETA** | Standardized Low Resolution Brain Electromagnetic Tomography | Source localization imaging (T2 clinical tool) |
| **EMDR** | Eye Movement Desensitization and Reprocessing | Bilateral eye stimulation protocol |
| **CN X** | Cranial Nerve X | Vagus nerve |

## Neuroscience & Physiology

| Abbr. | Expansion | Context |
|-------|-----------|---------|
| **RMSSD** | Root Mean Square of Successive Differences | HRV metric (parasympathetic tone) |
| **LF** | Low Frequency | HRV band (0.04–0.15 Hz) |
| **HF** | High Frequency | HRV band (0.15–0.4 Hz) |
| **PPG** | Photoplethysmography | Optical blood flow detection (infrared) |
| **DRL** | Driven Right Leg | Active EEG shield reference electrode |

## Hardware & Components

| Abbr. | Expansion | Context |
|-------|-----------|---------|
| **BOM** | Bill of Materials | Component cost list per configuration |
| **COGS** | Cost of Goods Sold | Manufacturing cost per unit |
| **USB-C** | Universal Serial Bus Type-C | Connector and power delivery standard |
| **PD** | Power Delivery | USB-C power negotiation standard |
| **Boa** | Boa Fit System | Dial-based occipital closure (brand: Boa Technology) |
| **LED** | Light Emitting Diode | Photon emitter (660, 808–830, 1064, 1170nm variants) |
| **PD** | Photodiode | Optical sensor for dose metering |
| **InGaAs** | Indium Gallium Arsenide | Long-wavelength infrared photodiode material (for 1064–1170nm) |
| **N42, N52** | Neodymium magnet grade | Permanent magnet specifications (N42 used in NeurOne for impact tolerance) |
| **VLT** | Visible Light Transmission | Optical transmission percentage (5–75% in EC lens) |
| **PDMS** | Polydimethylsiloxane | Optical window material with plasma-activated anti-fouling |
| **SiO₂** | Silicon Dioxide | Interlayer in PDMS–PI bonding (75nm RF-sputtered) |
| **AgNW** | Silver nanowire | Conductive coating (replaces ITO; tolerates 5–10% flex) |
| **ITO** | Indium tin oxide | Conductive coating (0.5% strain-to-failure; replaced by AgNW) |
| **EC** | Electrochromic | Smart tinting technology (5–75% VLT, 2s transition) |
| **NTC** | Negative Temperature Coefficient | Thermistor for temperature monitoring |
| **MCU** | Microcontroller Unit | Embedded processor (i.MX RT1062 main, STM32G071 safety) |
| **FPC** | Flexible Printed Circuit | Interconnect cable between modules |

## Processor & Software

| Abbr. | Expansion | Context |
|-------|-----------|---------|
| **NXP** | NXP Semiconductors | Main processor vendor (i.MX RT1062 at 600MHz) |
| **STM32** | STMicroelectronics 32-bit MCU | Safety MCU (STM32G071) |
| **SRAM** | Static Random Access Memory | Volatile fast memory (32MB LPSDR4 + 1MB on-chip) |
| **eMMC** | Embedded MultiMediaCard | On-device flash storage (8GB industrial SLC) |
| **I2C** | Inter-Integrated Circuit | Serial communication bus (400kHz fast mode for zone modules) |
| **SPI** | Serial Peripheral Interface | Main-to-safety MCU heartbeat (200ms interval) |
| **GPIO** | General Purpose Input/Output | Digital control lines (safety MCU owns all stimulation enables) |
| **ADC** | Analog-to-Digital Converter | Converts analog signals to digital (ADS1299 for EEG) |
| **FreeRTOS** | Free Real-Time Operating System | Kernel (V11.3.0 LTS 202604.00, on main processor) |
| **LittleFS** | Lightweight File System | On-device filesystem for UHDR/SHDR partitions |
| **AES-256** | Advanced Encryption Standard 256-bit | UHDR partition encryption (biometric-derived key) |
| **E2E** | End-to-End | Encryption (research data backup) |
| **OTA** | Over-The-Air | Firmware/model updates via USB-C |
| **CSPRNG** | Cryptographically Secure Pseudo-Random Number Generator | Protocol session signing |

## Data & Privacy

| Abbr. | Expansion | Context |
|-------|-----------|---------|
| **UHDR** | User Health Data Record | User's personal data (EEG, HRV, dose, outcomes) — never accessed by NeurOne |
| **SHDR** | System Health Data Record | Device telemetry (LED ratio, temperature, impact, session count) — NeurOne property only |
| **TRNG** | True Random Number Generator | Warranty token generation (device-linked, no user identity) |
| **EDF+** | European Data Format extended | Standard for EEG waveform export |
| **HIPAA** | Health Insurance Portability and Accountability Act | U.S. healthcare privacy standard |
| **FHIR** | Fast Healthcare Interoperability Resources | Standard for health data exchange (R4 version, T2 only) |
| **LSL** | Lab Streaming Layer | Real-time protocol for research data (T2 API) |
| **IRB** | Institutional Review Board | Ethics review body for human research |
| **k-anonymity** | k-anonymity principle | Research anonymization (k≥10 minimum NeurOne policy) |
| **GDPR** | General Data Protection Regulation | EU data protection regulation |
| **MHMD** | Mental Health/Mental Disability Law | Washington state statute (Article 9 analogue in U.S. context) |

## Regulatory & Compliance

| Abbr. | Expansion | Context |
|-------|-----------|---------|
| **IEC 60068-2-14** | IEC standard thermal cycling test | 200 cycles for PDMS–PI anti-fouling qualification |
| **IEC 62304** | Software lifecycle for medical devices | Class C (safety MCU) + Class B (main processor) |
| **IEC 62471** | Photobiological safety of lamps and lamp systems | Hardware MPE (50% of exempt group threshold) |
| **IEC 60601** | Medical electrical equipment safety | Temperature limit 42°C (IEC 60601-1 limits) |
| **PBM optical resolution floor** | Not an abbreviation | Boundary modeling for zone sizing (see `docs/np_opt_psf_001.md`) |
| **HIPAA** | Health Insurance Portability and Accountability Act | Covered entity definition for clinical tier |
| **Common Rule** | 45 CFR 46 | Federal human research protection regulations; k-anonymity irreversibility disclosure required |

## Financial & Operational

| Abbr. | Expansion | Context |
|-------|-----------|---------|
| **GM%** | Gross Margin percentage | Profitability per configuration (36–81% range) |
| **MRR** | Monthly Recurring Revenue | Consumables + service contracts (primary revenue driver) |
| **GaN** | Gallium Nitride | Charger technology (high efficiency, compact) |
| **PBM** | Power Bank Multimodal | (Not an abbreviation; see "power bank runtime" in power specs) |

## Research & Clinical

| Abbr. | Expansion | Context |
|-------|-----------|---------|
| **NIH** | National Institutes of Health | U.S. research funding agency |
| **R21** | Exploratory/Developmental Research | NIH grant mechanism for pilot trials |
| **SBIR** | Small Business Innovation Research | NIH program for small companies |
| **POA** | Power of Attorney | Healthcare proxy for consent capacity assessment |
| **K163334, K173323** | FDA 510(k) clearance numbers | Predicates for electroCore gammaCore (cervical VNS regulatory pathway) |
| **RUL** | Remaining Useful Life | Predictive maintenance algorithm output |
| **LSTM** | Long Short-Term Memory | Neural network architecture for fleet predictive models |

## Standards & Metrics

| Abbr. | Expansion | Context |
|-------|-----------|---------|
| **10-20 system** | 10-20 electrode placement system | EEG electrode standardization (Fp1/2, F3/4, C3/4, P3/4, A1/A2, Oz) |
| **L70** | Lumen maintenance to 70% | LED lifespan metric (80,000–100,000 hours at 120–180mA) |
| **FWHM** | Full Width at Half Maximum | Optical irradiance distribution metric (±15–25% variation) |
| **P/E cycles** | Program/Erase cycles | Flash memory endurance (30,000+ for industrial SLC) |
| **WVTR** | Water Vapor Transmission Rate | Moisture barrier specification for electrode caps (<0.5 g/m²/day) |

## EEG Electrode Positions (10-20 System)

| Abbr. | Expansion | Location |
|-------|-----------|----------|
| **Fp1/Fp2** | Frontopolar 1/2 | Above eyebrows, midline ±5cm |
| **F3/F4** | Frontal 3/4 | Prefrontal cortex |
| **C3/C4** | Central 3/4 | Primary motor/sensory cortex |
| **P3/P4** | Parietal 3/4 | Posterior parietal cortex |
| **Oz** | Occipital midline | Visual cortex (photoparoxysmal detection) |
| **A1/A2** | Auricular 1/2 | Linked-ear reference (on VNS clip pads in T2) |
| **FC3/FC4** | Frontocentral 3/4 | M1 TMS targeting co-registration |

## NeurOne Part Numbers

| Part Number | Description |
|-------------|-------------|
| **NP-FW-PBM1064-001** | Firmware spec for 1064nm smart zone modules |
| **NP-HW-FPC-001** | 20-pin FPC connector specification |
| **NP-HEX-ZM-001** | Hex-socket lattice for zone module placement |
| **NP-SES-1064-001** | Session orchestrator for 1064+1170nm combined protocol |
| **NP-DHF-001** | Design History File (formal design control index) |
| **NP-OPT-PSF-001** | Optical point-spread function characterization |
| **OI-PBM-HW-01** | Base PBM module hardware specification (Rev B pending) |
| **OI-CVNS-HUB-11** | Hub-to-MCU cervical VNS impedance cross-validation |
| **`00-zones.npps`** | NeurOne protocol file (zone definitions) |

## Wavelengths (Abbreviation by Context)

| Abbr. | Wavelength | Application |
|-------|-----------|-------------|
| **660nm** | Red | Photobiomodulation (superficial, ~2–5mm) |
| **808–830nm** | Near-infrared | Photobiomodulation (intermediate, ~5–10mm) |
| **1064nm** | Neodymium laser | Cortical penetration (~10–20mm) |
| **1170nm** | Erbium laser | Deep subcortical penetration (>20mm, T2 only) |
| **940nm** | Infrared (proximity) | Eye-open detection sensor |
| **808–830nm** | NIR (PPG) | Heart rate variability measurement |

## Regulatory Pathways & Strategy

| Abbr. | Expansion | Status |
|-------|-----------|--------|
| **T1 wellness** | FDA-exempt | No 510(k) required; marketing begins 12–18 months |
| **T2 510(k)** | Substantial equivalence | Full regulatory pathway; predicate identified (electroCore gammaCore for cervical VNS); ~18–36 months post-T1 |

## Personal AI Infrastructure (PAI Context)

| Abbr. | Expansion | Role |
|-------|-----------|------|
| **PAI** | Personal AI Infrastructure | Steve's Life Operating System |
| **DA** | Digital Assistant | AI agent (SmartyPants in this instance) |
| **ISA** | Implementation Specification Artifact | PAI format for design decisions and specifications |
| **RTK** | Runtime Tools Kit | PAI internal tooling for optimization |
| **E1–E5** | Effort levels 1–5 | Task complexity/scope classification |

---

## Notes

- **Dual meanings:** Some abbreviations (e.g., "PD" for both Power Delivery and Photodiode, "EC" for both Electrochromic and elsewhere) are context-dependent — check surrounding text.
- **Part numbers:** All NP-* and OI-* codes are internal NeurOne tracking identifiers for design documents and hardware specifications. They appear in CLAUDE.md as cross-references to locked design decisions.
- **Wavelengths:** Always stated with "nm" (nanometers) to avoid ambiguity. LED wavelengths vary slightly (e.g., 808–830nm is a range); precise values are in hardware specifications.
- **Regulatory:** T1/T2 classification drives which modalities are available and what consents are required. Both tiers share the same processor stack and chassis.

---

*Last updated: 2026-07-30 | Document: ABBREVIATIONS.md*
