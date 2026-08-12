# NeurOne Design Abbreviations Glossary

> **Naming and notation conventions live in `docs/np_conv_001.md` (NP-CONV-001)** — signal
> naming (active-low signals terminate with `#`), index notation, document IDs, `§N` section
> marking, and identifier families. This file defines *what terms mean*; NP-CONV-001 defines
> *how names are formed*.

## Product & Regulatory

### [510(k)](#510k)
**510(k)** — [Section 510(k) premarket notification](https://www.fda.gov/medical-devices/premarket-submissions/premarket-notification-510k)  
FDA pathway for demonstrating substantial equivalence to a predicate device. NeurOne T2 uses electroCore gammaCore as predicate for cervical VNS indication (K163334, K173323). See `docs/reference/regulatory-strategy.md` ("T2 — FDA 510(k)").

### [DHF](#dhf)
**DHF** — Design History File  
Comprehensive design control documentation required by IEC 62304. NeurOne formal index at `docs/np_dhf_001.md`; contains all design rationales, risk analyses, and verification records. See docs/status/document-register.md.

### [FDA](#fda)
**FDA** — [Food and Drug Administration](https://www.fda.gov)  
U.S. federal regulatory authority for medical devices (Center for Devices and Radiological Health — CDRH). NeurOne T1 regulated as exempt; T2 targets 510(k) pathway.

### [QMS](#qms)
**QMS** — Quality Management System  
Regulatory compliance framework for medical devices. NeurOne implements [ISO 13485](https://www.iso.org/standard/59752.html) requirements (ISO 13485:2016).

### [T1](#t1)
**T1** — Tier 1 (NeurOne Home)  
FDA-exempt wellness tier. 8 modalities, no 510(k) required, $449–$1,199 retail, 12–18 month launch timeline. See CLAUDE.md §1–2.

### [T2](#t2)
**T2** — Tier 2 (NeurOne Pro)  
FDA 510(k) clinical tier. 11 modalities (adds 21-ch qEEG, TMS, deep PBM, clinical tACS, sLORETA-guided HD-tDCS), HIPAA cloud, $4,999–$13,999 + $1,800/yr, 18–36 month post-T1 timeline. See CLAUDE.md §1–2.

## Neuromodulation & Clinical Terminology

### [BES](#bes)
**BES** — Brainwave Entrainment Stimulation  
Oscillating current stimulation (0.5–40Hz, ≤1mA, charge-balanced biphasic). NeurOne consumer name for EEG-synchronized frequency-following modality. Regulatory naming avoids medical device classification trigger. See CLAUDE.md §3 (modality 4).

### [CN X](#cn-x)
**CN X** — [Cranial Nerve X](https://en.wikipedia.org/wiki/Vagus_nerve)  
Vagus nerve (10th cranial nerve). NeurOne targets auricular branch (auricular vagus) for T1 VNS and cervical trunk for T2 tcVNS accessory. See CLAUDE.md §3 (modalities 5–6).

### [EEG](#eeg)
**EEG** — [Electroencephalography](https://en.wikipedia.org/wiki/Electroencephalography)  
Non-invasive measurement of electrical brain activity via scalp electrodes. NeurOne T1: 8-ch semi-dry hydrogel (Fp1/2, F3/4, C3/4, P3/4). T2: 21-ch wet gel qEEG with sLORETA. See CLAUDE.md §3 (modality 3) and §4.2 (safety interlocks).

### [EMDR](#emdr)
**EMDR** — [Eye Movement Desensitization and Reprocessing](https://en.wikipedia.org/wiki/Eye_movement_desensitization_and_reprocessing)  
Psychotherapy using bilateral alternating stimulation. NeurOne visual modality supports L/R alternation for EMDR protocols. See CLAUDE.md §3 (modality 8).

### [HD-tDCS](#hd-tdcs)
**HD-tDCS** — High-Definition tDCS  
Spatially-focused tDCS using 4×1 ring montage (center anode + 4 return cathodes). NeurOne T2 implementation: sLORETA-guided electrode positioning, ≤2mA per electrode, 40µC/cm² total charge density safety limit. See CLAUDE.md §3 (T2 additions) and §4.2.

### [HRV](#hrv)
**HRV** — [Heart Rate Variability](https://en.wikipedia.org/wiki/Heart_rate_variability)  
Autonomic nervous system marker (parasympathetic tone indicator). NeurOne measures via PPG in VNS auricular clip; biofeedback protocol targets resonance frequency (4–7 breaths/min range). See CLAUDE.md §3 (modality 6).

### [PBM](#pbm)
**PBM** — [Photobiomodulation](https://en.wikipedia.org/wiki/Photobiomodulation)  
Light therapy with wavelengths 660–1170nm promoting cellular ATP production. NeurOne: transcranial PBM (base 660+808nm + 1064nm smart modules), intranasal bilateral, and retinal 808–830nm modes. See CLAUDE.md §3 (modalities 1–2, 8).

### [qEEG](#qeeg)
**qEEG** — Quantitative EEG  
Digital analysis and spectral decomposition of EEG data. NeurOne T2 includes sLORETA source localization for HD-tDCS targeting. See CLAUDE.md §3 (T2 additions).

### [rTMS](#rtms)
**rTMS** — [Repetitive Transcranial Magnetic Stimulation](https://en.wikipedia.org/wiki/Transcranial_magnetic_stimulation)  
Pulsed TMS protocol (typically 0.5–20Hz repetition rate). NeurOne T2 supports rTMS via focal figure-8 coil. Regulatory classification: non-significant modification over standard rTMS systems.

### [sLORETA](#slorета)
**sLORETA** — Standardized Low Resolution Brain Electromagnetic Tomography  
Source localization algorithm for EEG (inverse problem solution via Laplacian weighting). NeurOne T2: automatically maps cortical target regions to 10-20 electrode positions for sLORETA-guided HD-tDCS montage setup. See CLAUDE.md §3 (T2 additions).

### [tACS](#tacs)
**tACS** — [Transcranial Alternating Current Stimulation](https://en.wikipedia.org/wiki/Transcranial_alternating_current_stimulation)  
Sinusoidal brain stimulation (0–40Hz). NeurOne T1: ≤1mA per electrode. T2: 16-channel arbitrary waveform, ≤4mA. Charge-balanced biphasic, per-electrode impedance monitoring. See CLAUDE.md §3 (modality 4).

### [TBS](#tbs)
**TBS** — Theta Burst Stimulation  
3-pulse theta-frequency TMS pattern (typically 50Hz triplet bursts at 5Hz inter-burst interval). Evidence-based protocol for depression, PTSD; faster than standard rTMS. NeurOne T2 TMS hub supports TBS. Regulatory pathway: identical to rTMS (K163334, K173323 predicates).

### [tcVNS](#tcvns)
**tcVNS** — Transcutaneous Cervical Vagal Nerve Stimulation  
Stimulation at cervical vagus trunk (carotid sheath level). NeurOne T2 accessory targeting cluster headache, migraine, depression, PTSD, post-stroke rehabilitation. Separate 510(k) required post-T1. Cardiac rhythm interlock: safety MCU monitors HR; cutoff if >15 BPM change within 5s. See CLAUDE.md §3 (T2 additions) and §4.2.

### [tDCS](#tdcs)
**tDCS** — [Transcranial Direct Current Stimulation](https://en.wikipedia.org/wiki/Transcranial_direct_current_stimulation)  
Direct low-current brain stimulation (0.1–2mA DC, charge-balanced biphasic pulses). NeurOne: 40µC/cm² hardware-enforced limit (safety MCU, app cannot override). 30s ramp up/down, ≤3 electrode pairs. See CLAUDE.md §3 (modality 5).

### [TMS](#tms)
**TMS** — [Transcranial Magnetic Stimulation](https://en.wikipedia.org/wiki/Transcranial_magnetic_stimulation)  
Magnetic pulse stimulation of cortical neurons. NeurOne T2 only: focal figure-8 coil (0.1–0.5T), rTMS + TBS capable, non-conductive CFRP window at coil site (prevents eddy current field loss), TMS-gated EMF cancellation (Helmholtz off 5ms pre-pulse, 50ms post-pulse hold). See CLAUDE.md §3 (T2 additions) and §4.2.

### [VNS](#vns)
**VNS** — [Vagal Nerve Stimulation](https://en.wikipedia.org/wiki/Vagus_nerve_stimulation)  
Electrical stimulation of vagus nerve for autonomic modulation. NeurOne T1: auricular branch (CN X, 1–25Hz, ≤2mA, biphasic charge-balanced) in VNS auricular clip with PPG HRV measurement. See CLAUDE.md §3 (modality 6).

## Neuroscience & Physiology

### [DRL](#drl)
**DRL** — Driven Right Leg  
Active EEG shield reference electrode technique (lowers baseline noise by fed-back subtraction). NeurOne EEG front-end (ADS1299) uses DRL electrode bonded to CFRP outer shell. Improves common-mode rejection ratio (CMRR) >90dB. Standard clinical EEG practice per ANSI/AAMI EC12.

### [HF](#hf)
**HF** — High Frequency  
HRV frequency band (0.15–0.4 Hz), primarily parasympathetic (vagal) contribution. NeurOne reports HF power and LF/HF ratio. See CLAUDE.md §3 (modality 6).

### [LF](#lf)
**LF** — Low Frequency  
HRV frequency band (0.04–0.15 Hz), mixed sympathetic and parasympathetic input. NeurOne reports LF power and LF/HF ratio for autonomic balance assessment. See CLAUDE.md §3 (modality 6).

### [PPG](#ppg)
**PPG** — [Photoplethysmography](https://en.wikipedia.org/wiki/Photoplethysmography)  
Optical measurement of blood volume changes (infrared absorption by hemoglobin). NeurOne: 808–830nm PPG in VNS auricular clip for HRV extraction and contact confirmation. Extractable time series in UHDR. See CLAUDE.md §3 (modality 6) and §5.1.

### [RMSSD](#rmssd)
**RMSSD** — Root Mean Square of Successive Differences  
HRV time-domain metric: root mean square of inter-beat intervals (RR intervals). Reflects parasympathetic nervous system tone; high RMSSD associated with better vagal tone and cardiovascular health. NeurOne displays per-session RMSSD and 30-session trend. See CLAUDE.md §3 (modality 6).

## Hardware & Components

### [AgNW](#agnw)
**AgNW** — Silver Nanowire  
Conductive coating material replacing ITO in NeurOne visual lens. AgNW tolerates 5–10% flex strain (vs. ITO: 0.5% strain-to-failure). Lifetime conductive claim verified by fleet SHDR attenuation monitoring. See CLAUDE.md §4.2 (visual stimulus outer conductive coating).

### [Boa](#boa)
**Boa** — [Boa Fit System](https://www.boatechnology.com)  
Dial-based occipital closure mechanism (brand: Boa Technology). NeurOne headset: 10cm range, 0.5mm/click, 50,000-cycle rated, enclosed PTFE-lined cable channel (prevents hair entanglement). Replacement cable + tool in box. See CLAUDE.md §4.4.

### [BOM](#bom)
**BOM** — Bill of Materials  
Component cost list per configuration. NeurOne configurations span $168–1,506 total BOM (Core to Pro Full). Used for COGS calculation and cost tracking. See CLAUDE.md §2 (configurations).

### [COGS](#cogs)
**COGS** — Cost of Goods Sold  
Manufacturing cost per unit (includes BOM + labor + overhead allocation). NeurOne ranges $258–2,628 depending on configuration tier. Drives gross margin % (36–81% range). See CLAUDE.md §2.

### [EC](#ec)
**EC** — Electrochromic  
Smart tinting technology (bistable liquid crystal or electrochromic film). NeurOne premium lens upgrade (+$89): 5–75% VLT, 2s transition, 3–5µm hard coat over film (scratch protection), 15mW hold current, clears to 75% on power restore (safety failsafe). See CLAUDE.md §3 (visual stimulus EC lens).

### [FPC](#fpc)
**FPC** — [Flexible Printed Circuit](https://en.wikipedia.org/wiki/Flexible_electronics)  
Flexible interconnect cable between modules. NeurOne: 20-pin FPC connects hub to zone modules (I2C + power rails). Specification: NP-HW-FPC-001 Rev 5. Bandwidth: 400kHz I2C fast mode. See CLAUDE.md §4.1 and §3 (smart zone modules).

### [InGaAs](#ingaas)
**InGaAs** — Indium Gallium Arsenide  
Long-wavelength infrared photodiode material. NeurOne: Hamamatsu G12180-010A sensors in 1064nm smart zone modules for dose metering (InGaAs peak sensitivity 1000–1700nm). See CLAUDE.md §3 (1064nm smart zone module).

### [ITO](#ito)
**ITO** — [Indium Tin Oxide](https://en.wikipedia.org/wiki/Indium_tin_oxide)  
Standard transparent conductive coating (0.5% strain-to-failure). Replaced in NeurOne visual lens by AgNW for flex tolerance. Legacy reference in competitive analysis only. See CLAUDE.md §3 (visual stimulus AgNW coating).

### [LED](#led)
**LED** — [Light Emitting Diode](https://en.wikipedia.org/wiki/Light-emitting_diode)  
Semiconductor photon emitter. NeurOne uses LED wavelengths: 660nm (red, superficial PBM), 808–830nm (near-infrared, intermediate PBM + PPG), 1064nm (smart zone modules, cortical PBM). Retinal: 660nm + 808–830nm (108 µ-LEDs per lens). L70 lifespan: 80,000–100,000 hours @ 120–180mA. See CLAUDE.md §3–4.

### [MCU](#mcu)
**MCU** — [Microcontroller Unit](https://en.wikipedia.org/wiki/Microcontroller)  
Embedded processor. NeurOne: NXP i.MX RT1062 (Cortex-M7, 600MHz, main processor) + STM32G071 (Cortex-M0+, 64MHz, safety MCU). See CLAUDE.md §4.1.

### [N42, N52](#n42-n52)
**N42, N52** — Neodymium Magnet Grade  
Permanent magnet strength classification ([energy product metric](https://en.wikipedia.org/wiki/Neodymium_magnet)). NeurOne uses N42 (not N52) in lens rim for improved impact tolerance (−$0.80 BOM). See CLAUDE.md §3 (visual stimulus shade system).

### [NTC](#ntc)
**NTC** — [Negative Temperature Coefficient](https://en.wikipedia.org/wiki/Thermistor)  
Thermistor with resistance decreasing as temperature increases. NeurOne: hub NTC monitors supercapacitor aging; logged in SHDR for predictive maintenance. PBM zone NTC per-zone prevents >42°C (IEC 60601 limit) → hardware current throttle at 62°C junction. See CLAUDE.md §4.5 and §5.2.

### [PD (Power Delivery)](#pd-power-delivery)
**PD** — [Power Delivery](https://en.wikipedia.org/wiki/USB_Power_Delivery)  
USB-C power negotiation standard (5V, 9V, 15V, 20V). NeurOne: 15V/2A (45W) typical T1, 20V/3A (65W) typical T2. Charger policies per configuration ensure adequate peak current margin. See CLAUDE.md §2.2 and §4.5.

### [PDMS](#pdms)
**PDMS** — [Polydimethylsiloxane](https://en.wikipedia.org/wiki/Polydimethylsiloxane)  
Silicone optical window material. NeurOne: plasma-activated anti-fouling coating on PBM/retinal optical windows. Bond to PI substrate via 75nm SiO₂ interlayer (RF magnetron sputter). Achieves 174–860 N/m peel force; 200-cycle IEC 60068-2-14 thermal cycling qualification required before production. See CLAUDE.md §3 (optical windows).

### [SiO₂](#sio2)
**SiO₂** — Silicon Dioxide  
Thin interlayer in PDMS–PI bonding (75nm RF-sputtered). Improves adhesion: 174–860 N/m peel force after plasma activation. See CLAUDE.md §3 (PBM optical windows).

### [USB-C](#usb-c)
**USB-C** — [Universal Serial Bus Type-C](https://en.wikipedia.org/wiki/USB-C)  
Connector standard and power delivery protocol. NeurOne default: wired-first USB-C (zero RF at scalp), <1ms latency streaming, firmware download, SHDR telemetry upload. Also supports BT 5.3 LE and Wi-Fi 6 (antennas in hub, not headset). See CLAUDE.md §4.1.

### [VLT](#vlt)
**VLT** — Visible Light Transmission  
Optical transmission percentage in lens. NeurOne shade options: S1 opaque (<0.5% VLT), S2 polarising (~12% VLT), S3 prescription (~12–50% depending on Rx lens), EC smart lens (5–75% VLT, variable). See CLAUDE.md §3 (visual stimulus shade system).

## Processor & Software

### [ADC](#adc)
**ADC** — [Analog-to-Digital Converter](https://en.wikipedia.org/wiki/Analog-to-digital_converter)  
Converts analog signals to digital representation. NeurOne: Texas Instruments ADS1299 (24-bit, 500Hz sample rate, 8-channel EEG front-end with integrated amplification). ADS1299 includes DRL (driven right leg) active shield for noise rejection. See CLAUDE.md §4.1.

### [AES-256](#aes-256)
**AES-256** — [Advanced Encryption Standard 256-bit](https://en.wikipedia.org/wiki/Advanced_Encryption_Standard)  
Symmetric-key encryption (256-bit key, 128-bit block size). NeurOne: UHDR partition encryption via biometric-derived key (user holds; NeurOne cannot access). On-device encryption at rest. See CLAUDE.md §5.1.

### [CSPRNG](#csprng)
**CSPRNG** — Cryptographically Secure Pseudo-Random Number Generator  
High-entropy PRNG for cryptographic applications. NeurOne: session protocol CSPRNG signing prevents forged or corrupted protocol injection. Safety-critical for closed-loop EEG-adaptive operation without phone. See CLAUDE.md §4.2.

### [E2E](#e2e)
**E2E** — End-to-End (Encryption)  
User-held encryption key for data in transit and at rest. NeurOne: automated nightly UHDR incremental backup to USB-C local or E2E encrypted cloud (user-held key, NeurOne cannot decrypt). See CLAUDE.md §5.1.

### [eMMC](#emmc)
**eMMC** — [Embedded MultiMediaCard](https://en.wikipedia.org/wiki/MultiMediaCard)  
On-device flash storage standard. NeurOne: 8GB industrial SLC eMMC (30,000+ P/E cycles), LittleFS filesystem, firmware partition write-protected, separate UHDR/SHDR partitions from first firmware line. See CLAUDE.md §4.1.

### [FreeRTOS](#freertos)
**FreeRTOS** — [Free Real-Time Operating System](https://www.freertos.org)  
Open-source RTOS kernel for embedded systems. NeurOne: FreeRTOS-Kernel V11.3.0 LTS 202604.00 (vendored in `firmware/vendor/freertos/`). Runs on NXP i.MX RT1062 main processor. Provides task scheduling, mutexes, semaphores for deterministic closed-loop operation. See CLAUDE.md §4.1.

### [GPIO](#gpio)
**GPIO** — [General Purpose Input/Output](https://en.wikipedia.org/wiki/General-purpose_input/output)  
Digital control lines for microcontroller. NeurOne: safety MCU (STM32G071) physically owns all stimulation GPIO enable lines. App crash cannot cause unsafe stimulation (hardware isolation). See CLAUDE.md §4.2.

### [I2C](#i2c)
**I2C** — [Inter-Integrated Circuit](https://en.wikipedia.org/wiki/I%C2%B2C)  
Serial communication bus (Philips/NXP standard). NeurOne: modules sit behind a cluster controller's PCA9548A, reached over a single differential cluster bus — there is no per-socket I2C peripheral. The earlier "dedicated LPI2C3 bus per smart zone module slot, ZONE_ID 3.3kΩ resistor on pin 18 for detection" scheme is **retired** (superseded by NP-HW-HUB-001 Rev 3 §5 cluster-controller fan-out + `np_module_map` UID-based auto-inventory). See CLAUDE.md §3 (1064nm smart zone modules).

### [LittleFS](#littlefs)
**LittleFS** — [Lightweight File System](https://github.com/littlefs-project/littlefs)  
Flash filesystem optimized for embedded systems (wear-leveling, power-loss safe). NeurOne: LittleFS manages eMMC UHDR/SHDR partitions. Separate encryption per partition. See CLAUDE.md §4.1 and §5.1.

### [NXP](#nxp)
**NXP** — [NXP Semiconductors](https://www.nxp.com)  
Semiconductor manufacturer (main processor vendor). NeurOne: i.MX RT1062 (Cortex-M7, 600MHz, 1MB on-chip SRAM + 32MB LPSDR4), USB-HS OTG, FPU+DSP+SIMD. Cost-optimized for consumer wearables. See CLAUDE.md §4.1.

### [OTA](#ota)
**OTA** — Over-The-Air  
Firmware/ML model update delivery via USB-C or cloud. NeurOne: versioned OTA packages signed by CSPRNG for session protocol integrity. Predictive maintenance models deployed to fleet via OTA (competitive moat grows with fleet size). See CLAUDE.md §5.2.

### [SPI](#spi)
**SPI** — [Serial Peripheral Interface](https://en.wikipedia.org/wiki/Serial_Peripheral_Interface)  
Synchronous serial communication bus. NeurOne: SPI heartbeat from main processor to safety MCU every 200ms; 1.5s watchdog → all-stimulation cutoff <50ms if main crashes. See CLAUDE.md §4.2.

### [SRAM](#sram)
**SRAM** — [Static Random Access Memory](https://en.wikipedia.org/wiki/Static_random-access_memory)  
Volatile fast memory (no refresh required). NeurOne: 1MB on-chip SRAM + 32MB LPSDR4 external on i.MX RT1062. Used for real-time signal processing buffers (EEG, PBM dose metering, adaptive frequency control). See CLAUDE.md §4.1.

### [STM32](#stm32)
**STM32** — [STMicroelectronics 32-bit Microcontroller](https://www.st.com/en/microcontrollers-microprocessors/stm32-32-bit-arm-cortex-mcus.html)  
ARM Cortex-based MCU family. NeurOne: STM32G071 (Cortex-M0+, 64MHz, 36KB SRAM, 128KB flash) used as safety MCU (NOT G031 — G031 has only 8KB SRAM, insufficient for EMF firmware). Bare-metal firmware controls all stimulation GPIO. See CLAUDE.md §4.1.

## Data & Privacy

### [EDF+](#edf)
**EDF+** — [European Data Format Extended](https://www.edfplus.info)  
Standard for EEG waveform export (ISO 14977:2004 compliant). NeurOne: T1/T2 export all 8–21 EEG channels to EDF+ format for clinician review and external analysis (e.g., third-party qEEG software). Time-stamped waveforms; preserves all metadata (sampling rate, filter settings, electrode impedance). See CLAUDE.md §5.1 (UHDR export).

### [FHIR](#fhir)
**FHIR** — [Fast Healthcare Interoperability Resources](https://www.hl7.org/fhir)  
Standard for health data exchange (HL7 v4 successor). NeurOne T2 only: FHIR R4 REST API for EHR integration, multi-patient dashboard data export, research dataset generation. Structured observation resources for EEG, HRV, session parameters. See CLAUDE.md §3 (T2 HIPAA cloud).

### [GDPR](#gdpr)
**GDPR** — [General Data Protection Regulation](https://gdpr-info.eu)  
EU data protection framework. NeurOne compliance: UHDR AES-256 encryption (user-held key), no profiling without explicit consent (article 22), data portability (EDF+ export), right to erasure (on-device deletion only; published anonymized extracts irreversible per law). Consent withdrawal blocks future data flows (irreversibility notice mandated). See CLAUDE.md §6.

### [HIPAA](#hipaa)
**HIPAA** — [Health Insurance Portability and Accountability Act](https://www.hhs.gov/hipaa)  
U.S. healthcare privacy/security standard. NeurOne T2: HIPAA-compliant cloud infrastructure (SHDR telemetry linked to warranty token, not user identity; clinic workflows via BAA). T1 wellness tier exempt from HIPAA (no protected health information in NeurOne possession). Clinicians acting as business associates via Data Use Agreements. See CLAUDE.md §6 (clinical consent engine).

### [IRB](#irb)
**IRB** — [Institutional Review Board](https://en.wikipedia.org/wiki/Institutional_Review_Board)  
Ethics review body for human subject research (45 CFR 46). NeurOne research consent L3 gates on IRB approval; all studies require NeurOne secondary review for anonymization parameters (k≥10 minimum). See CLAUDE.md §5.3 and §6.

### [k-anonymity](#k-anonymity)
**k-anonymity** — [k-anonymity Principle](https://en.wikipedia.org/wiki/K-anonymity)  
Data privacy model: each record indistinguishable from k–1 others on quasi-identifiers. NeurOne minimum: k≥10 per study descriptor. Enforced on-device before data leaves device. Irreversibility notice (45 CFR 46 Common Rule): once k-anonymized data published, individual withdrawal from that dataset impossible (dataset property, not record property). See CLAUDE.md §5.3.

### [LSL](#lsl)
**LSL** — [Lab Streaming Layer](https://github.com/sccn/labstreaminglayer)  
Real-time data streaming protocol (primarily research use). NeurOne T2 API: LSL outlet for 21-ch qEEG, HRV, session parameters; enables live integration with EEGLAB, BCI2000, Python ML pipelines. Low-latency, network-transparent. See CLAUDE.md §3 (T2 clinical APIs).

### [MHMD](#mhmd)
**MHMD** — Mental Health/Mental Disability Law  
Washington state [RCW 71.34](https://app.leg.wa.gov/RCW/default.aspx?cite=71.34) statute (GDPR Article 9 analogue in U.S. context). Defines heightened protection for mental health data; NeurOne anonymization pipeline flags when UHDR contains mental health quasi-identifiers (suppresses timestamps, location data). See CLAUDE.md §5.1.

### [SHDR](#shdr)
**SHDR** — System Health Data Record  
NeurOne-owned device telemetry: LED output ratio, NTC temperature profiles, EMF shielding attenuation, session count (unsigned integer), consumable cycles, USB-C insertion counter, PD negotiation log, impact events (off-device only), fan RPM, supercap cycles, firmware version history, calibration coefficients. Linked to opaque TRNG warranty token (no user identity). Consent subject: warranty owner (separate from user consent). Upload to NeurOne fleet database on USB-C connect. Drives predictive maintenance; no user biology content. See CLAUDE.md §5.1.

### [TRNG](#trng)
**TRNG** — True Random Number Generator  
High-entropy random generation for cryptographic tokens. NeurOne: TRNG warranty token (device-linked opaque identifier, no user identity) generated at factory. Enables SHDR fleet analysis without user PII. Distinct from session protocol CSPRNG. See CLAUDE.md §5.1.

### [UHDR](#uhdr)
**UHDR** — User Health Data Record  
User-owned encrypted health data: EEG waveforms (all channels), HRV time series, PPG optical signal, neurofeedback scores, session timestamps/duration, protocol parameters, closed-loop adaptation events, PBM dose (J/cm²), user-entered symptom/outcome logs, eye-open/closed state. Stored on-device eMMC UHDR partition, AES-256 encrypted with user biometric-derived key (NeurOne does not hold decryption key). Automated nightly incremental backup to USB-C local or E2E encrypted cloud (user-held key). NeurOne never accesses UHDR (not for support, engineering, research, or regulatory submission). Per-element, per-use-case, time-limited, audited, revocable clinician access. See CLAUDE.md §5.1.

## Regulatory & Compliance

### [Common Rule](#common-rule)
**Common Rule** — [45 CFR 46](https://www.ecfr.gov/current/title-45/part-46) (Federal Code of Regulations)  
U.S. federal human research protection regulations. NeurOne k-anonymity irreversibility notice mandated by Common Rule: "Once your anonymized data has been included in a published study, it cannot be individually withdrawn from that dataset." Triggers informed consent requirement for all research participation. See CLAUDE.md §5.3 and §6.2.

### [IEC 60068-2-14](#iec-60068-2-14)
**IEC 60068-2-14** — [Test methods for electrical and electronic equipment, thermal cycling](https://www.iec.ch)  
Thermal cycling qualification standard. NeurOne requirement: 200-cycle IEC 60068-2-14 thermal cycling qualification for PDMS–PI anti-fouling bond integrity (75nm SiO₂ interlayer). Qualification required before production (blocking item). See CLAUDE.md §3 (optical windows).

### [IEC 60601](#iec-60601)
**IEC 60601** — [Medical electrical equipment safety](https://www.iec.ch/webstore/publication/24157)  
International standard for medical device electrical safety. NeurOne uses IEC 60601-1 temperature limit (42°C) for PBM zone hardware current throttle (NTC monitors junction; throttles at 62°C for safety margin). Safety-critical hardware enforcement. See CLAUDE.md §4.2.

### [IEC 62304](#iec-62304)
**IEC 62304** — [Medical device software lifecycle processes](https://www.iec.ch/webstore/publication/30515)  
Software lifecycle standard for medical devices. NeurOne certification: Class C (safety MCU, ~500 lines bare-metal) + Class B (main processor, ~10K lines firmware). Separate software safety classification reflecting dual-processor architecture. See CLAUDE.md §4.2.

### [IEC 62471](#iec-62471)
**IEC 62471** — [Photobiological safety of lamps and lamp systems](https://www.iec.ch/webstore/publication/27102)  
Standard for photobiological safety of optical radiation. NeurOne visual modality: hardware MPE (Maximum Permissible Exposure) ceiling set to 50% of exempt group threshold (conservative safety margin). IR proximity sensors + Hall sensor (goggle lift = instant LED cutoff) + photoparoxysmal EEG detection (<200ms halt). Three independent safety layers. See CLAUDE.md §3 (visual stimulus) and §4.2.

## Financial & Operational

### [GaN](#gan)
**GaN** — [Gallium Nitride](https://en.wikipedia.org/wiki/Gallium_nitride)  
Wide-bandgap semiconductor material. NeurOne: GaN chargers provide higher efficiency and compact size (vs. silicon). Branded 45W/65W NeurOne GaN chargers included in Home Premium / Pro configurations; unbranded option for Core/Lite tiers. See CLAUDE.md §2.2 (charger policy).

### [GM%](#gm)
**GM%** — Gross Margin Percentage  
Profitability per configuration: (Retail − COGS) / Retail × 100%. NeurOne range: 36% (Home Standard) to 81% (Pro Full). Gross margin drives long-term sustainability and R&D reinvestment. See CLAUDE.md §2 (configurations).

### [MRR](#mrr)
**MRR** — Monthly Recurring Revenue  
Subscription revenue from consumables + service contracts (primary revenue driver). NeurOne: intranasal sleeves ($19/pack or $19/mo subscription), electrode hydrogel tips ($12–16 or $9.99/mo), VNS pads, audio cups, interface covers, T2 service contracts ($1,800/yr). Projected 60–80% of total revenue by year 3. See CLAUDE.md §2.3.

## Research & Clinical

### [K163334, K173323](#k-clearances)
**K163334, K173323** — FDA 510(k) Clearance Numbers  
Predicates for electroCore gammaCore (cervical VNS regulatory pathway). NeurOne T2 tcVNS (cervical transcutaneous VNS) targets cluster headache (K163334 → electroCore cleared for cluster 2022) and migraine (K173323 → electroCore cleared for migraine). NeurOne separate 510(k) required post-T1; uses electroCore predicate as precedent. See CLAUDE.md §3 (T2 cervical VNS accessory).

### [LSTM](#lstm)
**LSTM** — [Long Short-Term Memory](https://en.wikipedia.org/wiki/Long_short-term_memory)  
Recurrent neural network architecture (handles sequential data with long-range dependencies). NeurOne fleet predictive maintenance (Phase 2, years 2+): LSTM trained on SHDR sensor trajectories predicts time-to-failure with personalized Bayesian updates as fleet grows. See CLAUDE.md §5.2.

### [NIH](#nih)
**NIH** — [National Institutes of Health](https://www.nih.gov)  
U.S. federal biomedical research funding agency. NeurOne target: pilot data (n=20–30 via research suggestion portal + crowdfunding) supports NIH SBIR/R21 grant applications. Research calendar syncs with NIH funding cycles. See CLAUDE.md §6.3 (patient research agenda).

### [POA](#poa)
**POA** — [Power of Attorney](https://en.wikipedia.org/wiki/Power_of_attorney)  
Legal authority granted by one person (principal) to another (agent) to act on their behalf. NeurOne research consent L1: healthcare POA holder uploads executed POA → human review 3 business days → jurisdiction flagged → scope limited → annual re-verification. Proxy consent workflows trigger when patient capacity questioned. See CLAUDE.md §6.2.

### [R21](#r21)
**R21** — [NIH Exploratory/Developmental Research Grant](https://grants.nih.gov/grants/funding/r21.htm)  
Small NIH grant mechanism for high-risk pilot studies (≤$275K direct costs, 2 years). NeurOne crowdfunded pilot data (n=20–30) + patient research agenda votes enable R21 applications. Cost-effective pathway to larger R01 funding. See CLAUDE.md §6.3 (crowdfunding catalyst).

### [RUL](#rul)
**RUL** — Remaining Useful Life  
Predictive maintenance metric: estimated time until component failure. NeurOne Phase 2 (years 2+): fleet-trained LSTM models predict RUL for LEDs, supercapacitor, consumables. Continuously revised Bayesian predictions improve with fleet size. Enables proactive reminders + targeted supply chain optimization. See CLAUDE.md §5.2.

### [SBIR](#sbir)
**SBIR** — [Small Business Innovation Research](https://www.sbir.gov)  
U.S. federal program funding high-tech startups on R&D contracts. NeurOne eligible: Phase I ($50K–150K pilot), Phase II ($500K–750K, 2 years), Phase IIB commercialization. Patient research crowdfunding accelerates SBIR proposal competitiveness. See CLAUDE.md §6.3.

## Standards & Metrics

### [10-20 system](#10-20-system)
**10-20 system** — [International 10-20 EEG Electrode Placement System](https://en.wikipedia.org/wiki/10%E2%80%9320_system)  
Standardized scalp electrode positioning for EEG (inter-electrode distances 10% or 20% of nasion-inion distance). NeurOne T1: 8-ch subset (Fp1/2, F3/4, C3/4, P3/4). T2: 21-ch full system (adds FC3/FC4 for M1 TMS targeting, Oz for photoparoxysmal detection, A1/A2 on VNS clip). Enables normative comparison and clinical handoff to third-party EEG analysis. See CLAUDE.md §3–4.

### [FWHM](#fwhm)
**FWHM** — Full Width at Half Maximum  
Optical irradiance distribution metric (spatial resolution). NeurOne PBM: ±15–25% irradiance variation across 6mm inter-LED pitch field → near-uniform coverage. FWHM characterization in `docs/np_opt_psf_001.md`. Precision optical modeling informs zone sizing for lateralized protocols (e.g., unilateral DLPFC targeting). See CLAUDE.md §3 (PBM transcranial).

### [L70](#l70)
**L70** — [Lumen Maintenance to 70%](https://en.wikipedia.org/wiki/LED_lamp#Lifespan)  
LED lifespan metric: hours until output drops to 70% initial brightness (standard for LED lifetime rating). NeurOne: 80,000–100,000 hours L70 @ 120–180mA per LED. Thermal throttling (NTC monitoring) extends L70 under heavy use. Commercial LEDs (Cree/Samsung) selected for >95% consistency across wavelengths. See CLAUDE.md §4.4 (power/operating modes).

### [P/E cycles](#pe-cycles)
**P/E cycles** — Program/Erase Cycles  
Flash memory endurance metric (NAND degradation per write cycle). NeurOne eMMC: industrial SLC (Single Level Cell) with 30,000+ P/E cycles (consumer MLC: 3,000–5,000; industrial SLC: 30,000+). Firmware partition write-protected; UHDR/SHDR use cyclic wear-leveling (LittleFS). See CLAUDE.md §4.1.

### [WVTR](#wvtr)
**WVTR** — Water Vapor Transmission Rate  
Moisture barrier specification (g/m²/day). NeurOne electrode hydration caps: WVTR <0.5 g/m²/day (extends storage life to 24+ months). Measured per ASTM E96 desiccant method. Silicone over-mold at Y-junction (intranasal PBM) prevents water ingress during use. See CLAUDE.md §3 (PBM intranasal and EEG modalities).

## EEG Electrode Positions (10-20 System)

### [A1/A2](#a1-a2)
**A1/A2** — Auricular 1/2 Reference Electrodes  
Linked-ear reference position (standard in clinical EEG). NeurOne T2: A1/A2 co-located on VNS auricular clip contact pads (2 spare conductors in existing 6-pin cable, +$15 BOM). Enables single unified electrode array (no separate ear clips). Linked-ear normative reference improves source localization accuracy for sLORETA. See CLAUDE.md §3 (VNS clip + qEEG integration).

### [C3/C4](#c3-c4)
**C3/C4** — Central 3/4 Electrodes  
Primary motor/sensory cortex representation (Brodmann area 1/3/4). NeurOne T1/T2: bilateral recording site for motor imagery, sensorimotor feedback, resting-state connectivity. C3/C4 tDCS montage targets motor cortex (stroke rehabilitation, Parkinson's tremor). See CLAUDE.md §3 (EEG neurofeedback modality).

### [F3/F4](#f3-f4)
**F3/F4** — Frontal 3/4 Electrodes  
Prefrontal cortex (dorsolateral prefrontal cortex — DLPFC region). NeurOne T1/T2: depression/mood feedback site (neurofeedback targets mood-related alpha suppression). F3/F4 tDCS predicate for depression treatment. See CLAUDE.md §3 (EEG neurofeedback, tDCS modalities).

### [FC3/FC4](#fc3-fc4)
**FC3/FC4** — Frontocentral 3/4 Electrodes  
M1 (primary motor cortex) neighboring sites (used for motor co-registration). NeurOne T2 only: added to 21-ch qEEG for TMS coil targeting (M1 TMS for depression/anxiety has strongest evidence base). Enables automated electrode-to-neuronavigation co-registration. See CLAUDE.md §3 (T2 additions — TMS).

### [Fp1/Fp2](#fp1-fp2)
**Fp1/Fp2** — Frontopolar 1/2 Electrodes  
Frontopolar cortex (anterior prefrontal region, above eyebrows). NeurOne T1/T2: prefrontal baseline recording for executive function, decision-making feedback. Highest impedance site (hairline proximity); critical for proper gel application training. See CLAUDE.md §3 (EEG neurofeedback modality).

### [Oz](#oz)
**Oz** — Occipital Midline Electrode  
Visual cortex (V1/V2 representation, midline). NeurOne T1/T2: photoparoxysmal seizure detection site (Oz is most sensitive for photic driving). Photoparoxysmal EEG detection at Oz → goggle halt <200ms (safety interlock). Clinician-unlock option for 3–30Hz protocols only. See CLAUDE.md §4.2 (visual stimulus safety interlocks).

### [P3/P4](#p3-p4)
**P3/P4** — Parietal 3/4 Electrodes  
Posterior parietal cortex (attention/spatial processing). NeurOne T1/T2: recording site for parietal alpha (10Hz) and theta (4–7Hz) rhythms. P3/P4 neurofeedback for attention training, sleep spindle detection. Posterior cingulate connectivity analysis (default mode network). See CLAUDE.md §3 (EEG neurofeedback modality).

## NeurOne Part Numbers

Internal tracking identifiers for design documents, hardware specifications, and firmware modules. All NP-* and OI-* codes link to locked design decisions in CLAUDE.md and subsidiary documents.

### [00-zones.npps](#00-zones-npps)
**`00-zones.npps`** — NeurOne Protocol File (Zone Definitions)  
Protocol file specifying zone (LED cluster) configurations. Zones are software-defined sets of sockets in the hex-socket lattice (NP-HEX-ZM-001), not fixed hardware slots. Total LED count scales with populated T1-A (base PBM) modules per configuration. See CLAUDE.md §3 (PBM transcranial tiling).

### [NP-DHF-001](#np-dhf-001)
**NP-DHF-001** — Design History File (Formal Design Control Index)  
Master design control documentation index for IEC 62304 medical device compliance (Class B/C hybrid). Source of truth for all design rationales, risk analyses, verification records, traceability matrix, change control logs. Referenced in `docs/np_dhf_001.md`. See CLAUDE.md §1 (Document Map).

### [NP-FW-PBM1064-001](#np-fw-pbm1064-001)
**NP-FW-PBM1064-001** — Firmware Spec for 1064nm Smart Zone Modules (Rev A)  
Firmware specification for 1064nm smart module control logic. Defines: on-module Microchip ATtiny402 I2C slave + Infineon IRLML6344 N-FETs for independent channel drive (660nm CH_A, 808nm CH_B, 1064nm CH_C). Three-tier PBM penetration stack orchestration (660nm surface → 1064nm cortical → 1170nm deep). Thermal throttle priority: 1170nm first, then 1064nm CH_C, then CH_B. See CLAUDE.md §3 (1064nm smart zone module architecture).

### [NP-HEX-ZM-001](#np-hex-zm-001)
**NP-HEX-ZM-001** — Hex-Socket Lattice for Zone Module Placement  
Hardware socket array specification for snap-in zone module placement. Defines physical lattice geometry and I2C addressing scheme. Zones (LED clusters) are protocol-defined via `00-zones.npps`, not fixed to hardware slots — enables flexible scalability across configurations. See CLAUDE.md §3 (PBM transcranial modality).

### [NP-HW-FPC-001](#np-hw-fpc-001)
**NP-HW-FPC-001** — 20-Pin FPC Connector Specification (Rev E)  
Flexible printed circuit connector specification between hub and zone modules. 20-pin FPC carries I2C data/clock + 3.3V power rails. The pin-18 ZONE_ID 3.3kΩ detection resistor and the per-slot LPI2C3 addressing it served are **retired** — modules are identified by UID-based auto-inventory (`np_module_map`) over NP-HW-HUB-001 Rev 3's cluster-controller fan-out. Bandwidth supports real-time zone control during adaptive EEG-driven sessions. See CLAUDE.md §4.1.

### [NP-OPT-PSF-001](#np-opt-psf-001)
**NP-OPT-PSF-001** — Optical Point-Spread Function Characterization  
Detailed optical modeling document for PBM irradiance field distribution. Specifies FWHM (±15–25% variation across 6mm inter-LED pitch), penetration depth profile (wavelength-dependent), near-field/far-field transition distances. Informs zone sizing for lateralized protocols (e.g., unilateral DLPFC targeting with measurable precision). Referenced in CLAUDE.md Document Map. See `docs/np_opt_psf_001.md`.

### [NP-SES-1064-001](#np-ses-1064-001)
**NP-SES-1064-001** — Session Orchestrator for 1064+1170nm Combined Protocol (Rev A)  
Firmware specification for multi-wavelength PBM session coordination (T2 only). Orchestrates simultaneous/sequential 1064nm smart zone modules (cortical depth) + existing 1170nm laser system (subcortical depth). Thermal throttle hierarchy, real-time adaptive frequency control, dose metering across three penetration tiers. See CLAUDE.md §3 (T2 1064nm + 1170nm combined session).

### [OI-CVNS-HUB-11](#oi-cvns-hub-11)
**OI-CVNS-HUB-11** — Hub-to-MCU Cervical VNS Impedance Cross-Validation  
Specification for cervical VNS safety MCU impedance monitoring. Hub measures electrode contact impedance; safety MCU reads per-electrode impedance for cross-validation. Divergence flag (`NP_CVNS_SHDR_EV_IMP_CROSSVAL`, no kΩ values, suppressed timestamp) logged to SHDR to detect contact degradation. Cardiac rhythm interlock: safety MCU monitors R-peak; HR change >15 BPM within 5s → GPIO cutoff <100ms. See CLAUDE.md §3 (T2 cervical VNS) and §5.1 (SHDR boundary cases).

### [OI-PBM-HW-01](#oi-pbm-hw-01)
**OI-PBM-HW-01** — Base PBM Module Hardware Specification (Rev B pending)  
Hardware specification for base (660nm + 808nm) transcranial PBM module. Defines: 550 LEDs per zone (split across wavelengths), photodiode duo (forward + backscattered), dual PD PDMS fouling detection, Microchip ATtiny402 I2C interface. The Hub PCB Rev B **per-slot** Vishay DG2788A TIA gain switch is **superseded**: SMART-1 requires every socket to be I2C/TIA-capable, which reopened the hub TIA-gain design as an NP-HW-HUB-001 Rev 3 item rather than a five-slot Rev 2 addition. BOM delta +$23–28, retail $149–199/zone. See CLAUDE.md §3 (PBM 1064nm smart zone module architecture).

## Wavelengths

Optical wavelengths by penetration depth and application.

### [660nm](#660nm)
**660nm** — Red Light (Photobiomodulation)  
Visible red wavelength (superficial penetration, ~2–5mm). NeurOne PBM: transcranial 660nm LEDs in base modules and 1064nm smart zones; intranasal bilateral 660nm probes; retinal 660nm µ-LEDs (108 per lens). L70 lifespan 80,000–100,000 hours @ 120–180mA. Dose metering via dual photodiodes (forward + backscattered) detects PDMS fouling independently from LED aging. See CLAUDE.md §3 (PBM modalities).

### [808–830nm (PBM)](#808-830nm-pbm)
**808–830nm** — Near-Infrared Photobiomodulation  
Near-infrared wavelength range (intermediate penetration, ~5–10mm, higher absorption in hemoglobin). NeurOne: transcranial 808–830nm LEDs (base modules + smart zones), intranasal bilateral, retinal 808–830nm µ-LEDs. Therapeutic window overlaps cytochrome c oxidase absorption peak. Commonly paired with 660nm for dual-wavelength efficacy. See CLAUDE.md §3 (PBM transcranial, intranasal, retinal modalities).

### [808–830nm (PPG)](#808-830nm-ppg)
**808–830nm** — Near-Infrared Heart Rate Variability  
Near-infrared wavelength for PPG (photoplethysmography) optical blood volume measurement. NeurOne VNS auricular clip: 808–830nm PPG for HRV extraction via hemoglobin absorption changes. Real-time RMSSD and LF/HF biofeedback protocols. Extractable time series in UHDR. See CLAUDE.md §3 (VNS + HRV modality 6).

### [940nm](#940nm)
**940nm** — Infrared Proximity Sensor  
Near-infrared wavelength for proximity sensing (eye-open detection). NeurOne visual modality: IR proximity sensors (940nm) in goggle housing detect eye proximity → instant LED cutoff on goggle lift (safety interlock). Prevents accidental exposure. See CLAUDE.md §4.2 (visual stimulus safety).

### [1064nm](#1064nm)
**1064nm** — Neodymium Laser (Cortical Penetration)  
Neodymium laser wavelength (cortical penetration, ~10–20mm). NeurOne T2 only: smart zone modules with on-module Microchip ATtiny402 controller + Infineon N-FETs drive independent 1064nm channel (CH_C). InGaAs photodiodes for dose metering (peak sensitivity 1000–1700nm). Coordinates with 660nm (CH_A) and 808nm (CH_B) for three-tier penetration stack. See CLAUDE.md §3 (1064nm smart zone modules, T2 combined protocol).

### [1170nm](#1170nm)
**1170nm** — Erbium Laser (Deep Subcortical Penetration)  
Erbium laser wavelength (deep subcortical penetration, >20mm). NeurOne T2 only: laser diodes with TEC stabilisation, ≤1,000 mW/cm² intensity limit, ≤35–40mm depth targeting (thalamus, striatum, amygdala). Three-tier thermal throttle priority: 1170nm first, then 1064nm CH_C, then CH_B. Separate 510(k) required for T2 launch. See CLAUDE.md §3 (T2 deep PBM).

## Regulatory Pathways & Strategy

### [T1 wellness](#t1-wellness)
**T1 wellness** — FDA-Exempt Pathway  
NeurOne Home tier: FDA wellness classification (no 510(k) required). Consumer marketing begins 12–18 months post-tooling. Modalities (8): PBM transcranial + intranasal, 8-ch EEG, VNS + HRV, BES/tACS/tDCS, neural audio, visual stimulation. No mandatory medical claims; position as wellness wearable. Regulatory strategy allows rapid market entry with later FDA submission for T2. See CLAUDE.md §1–2 and `docs/reference/regulatory-strategy.md`.

### [T2 510(k)](#t2-510k)
**T2 510(k)** — FDA Substantial Equivalence Pathway  
NeurOne Pro tier: 510(k) premarket notification targeting "Substantial Equivalence" to FDA-cleared predicate devices. Predicates identified: electroCore gammaCore (K163334 cluster headache, K173323 migraine) for cervical VNS indication. Separate 510(k) required post-T1 for multi-modality clinical claims (depression, anxiety, PTSD, TBI, sleep). Timeline: 18–36 months post-T1. Full design control (IEC 62304 Class B/C hybrid). See CLAUDE.md §1–2 and `docs/reference/regulatory-strategy.md` §3.

## Personal AI Infrastructure (PAI Context)

### [DA](#da)
**DA** — Digital Assistant  
AI agent in PAI (Personal AI Infrastructure) framework. NeurOne design context: SmartyPants (Steve's specific DA instantiation) assists with technical specifications, design reviews, documentation. Operates in NATIVE, ALGORITHM, or MINIMAL modes depending on task complexity. See `~/.claude/PAI/USER/DA_IDENTITY.md`.

### [E1–E5](#e-levels)
**E1–E5** — Effort Levels 1–5  
PAI task complexity/scope classification. E1: Standard+fast-path (simple edits, refactors). E2: Extended (moderate features, multi-file changes). E3: Advanced (architecture design, deep debugging). E4: Deep (cross-system refactors). E5: Comprehensive (full rewrites, new subsystems). NeurOne ABBREVIATIONS.md creation = E3 (multi-section alphabetization + link research). See `~/.claude/CLAUDE.md` (effort shortcuts).

### [ISA](#isa)
**ISA** — Implementation Specification Artifact  
PAI format for design decisions and technical specifications. Executable document: Current State → Ideal State via verifiable iteration (ISC). NeurOne CLAUDE.md sections use ISA format for locked decisions (§ notation indicates design-control registry). Version-stamped; change log at `docs/status/completed-decisions.md`. See `~/.claude/PAI/DOCUMENTATION/IsaFormat.md`.

### [PAI](#pai)
**PAI** — [Personal AI Infrastructure](~/.claude/PAI/DOCUMENTATION/LifeOs/LifeOsThesis.md)  
Steve's Life Operating System — scaffolding for AI assistance across all projects and personal systems. Comprises: constitutional rules (system prompt), operational procedures (CLAUDE.md), context routing (memory + documentation), skill/agent/hook system, observability (logging), notification (voice), dashboard (Pulse). NeurOne design uses PAI algorithms for specification, verification, and cross-project decision consistency. See `~/.claude/PAI/DOCUMENTATION/PAISystemArchitecture.md`.

### [RTK](#rtk)
**RTK** — Runtime Tools Kit  
PAI internal tooling for code optimization and compression. PreToolUse hook rewrites Bash commands through RTK for 60–90% token reduction. Gains reported via `bun TOOLS/Inference.ts gain`. Used to manage context window in long conversations. See `~/.claude/CLAUDE.md` (Context reduction section).

---

## Usage Notes

### Cross-references
- **Internal links:** Each section uses markdown anchors (`#anchor-id`) for deep linking within this document. Jump to any definition via sidebar or direct `#` link.
- **External links:** Industry standard terms (FDA, IEC, IEEE, GDPR, NIH, etc.) link to authoritative sources (government sites, standards bodies, Wikipedia).
- **NeurOne documents:** Internal references (CLAUDE.md § notation, `docs/` files) link to project-specific specifications and decision logs.

### Alphabetical organization
- **Each section sorted A–Z by acronym** for quick lookup.
- Acronyms appear **in BOLD** with anchor IDs in section headers.
- Context includes regulatory pathway, hardware component, or data field affected.

### Dual meanings
- **PD (Power Delivery vs. Photodiode):** Context distinguishes (power specs vs. optical sensing).
- **EC (Electrochromic vs. elsewhere):** Visual modality context clarifies (lens tinting vs. other meanings).
- **808–830nm (PBM vs. PPG):** Separate entries by application (photobiomodulation vs. heart rate sensing).

### Standards references
- **IEC standards:** All medical device standards (IEC 60601, 62304, 62471) link to ISO/IEC catalog.
- **FDA pathways:** 510(k), K-numbers, regulatory precedents linked to official FDA database.
- **Common Rule (45 CFR 46):** U.S. human research protection regulations — ECFR link.

### Part numbers
- **NP-* and OI-* codes:** Internal NeurOne design tracking identifiers.
- **Versioning:** Many include revision (Rev A, Rev 2, Rev 5) — always reference current revision in CLAUDE.md locked decisions.
- **Status flags:** Some part numbers are "pending", "on hold" or "superseded" (e.g., OI-PBM-HW-01's per-slot TIA gain switch — superseded, reopened as an NP-HW-HUB-001 Rev 3 item). See `docs/status/pending-decisions.md` for current status.

### Wavelengths
- **Always with "nm" (nanometers)** to avoid ambiguity.
- **Ranges (e.g., 808–830nm)** indicate LED spectral width (not a single wavelength).
- **Penetration depths approximate:** Vary by skin type, pigmentation, hair density; `docs/np_opt_psf_001.md` contains detailed optical characterization.

---

## Cross-Reference Index by Document

| Category | Primary Document | Related Files |
|----------|------------------|---------------|
| **Product & Pricing** | CLAUDE.md §1–2 | `docs/reference/competitive-position.md` |
| **Modalities** | CLAUDE.md §3 | `docs/np_opt_psf_001.md` (optical characterization) |
| **Hardware** | CLAUDE.md §4 | NP-HW-*, OI-* part specifications |
| **Data Architecture** | CLAUDE.md §5 | `docs/np_dhf_001.md` (DHF master index) |
| **Clinical Consent** | CLAUDE.md §6 | `docs/reference/regulatory-strategy.md` |
| **Regulations** | `docs/reference/regulatory-strategy.md` | CLAUDE.md §4.2 (IEC requirements) |
| **Design Status** | `docs/status/pending-decisions.md` | `docs/status/completed-decisions.md` |

---

*Last updated: 2026-07-30*  
*Format: Alphabetical by section, anchor links to external/internal references*  
*Governance: Locked decisions (CLAUDE.md) supersede all entries; pending status in `docs/status/pending-decisions.md`*
