# NP-PROC-FPC-1064-001 Rev A
## 1064nm Smart Zone Module — Component Selection and Procurement Specification

**Document:** NP-PROC-FPC-1064-001 Rev A  
**Date:** 2026-05-13  
**Status:** Baselined — Issue #54  
**Supplements:** NP-PROC-FPC-001 Rev A (base module FPC procurement — unchanged)  
**Author:** NeuroPulse Hardware Engineering  
**References:** NP-HW-FPC-001 Rev E; NP-TOOL-ZM-SM-001 Rev A; NP-FW-PBM1064-001 Rev A; Issue #54

---

## 1. Scope

This document covers component selection and procurement specifications for the three non-standard components in the 1064nm smart zone module:

1. **On-module I2C LED driver IC combination** — selected and evaluated (§2)
2. **InGaAs photodiodes PD1 and PD2** — selected and evaluated (§4)
3. **1064nm LED emitter binning specification** — procurement spec (§3)

It also covers the TIA gain compatibility analysis (§5) and full smart module BOM summary (§6).

This document supplements NP-PROC-FPC-001 Rev A. The existing 660nm and 808nm LED emitter binning specs in NP-PROC-FPC-001 Rev A are unchanged and apply to the 660nm and 808nm emitters in the smart module's CH_A and CH_B channels.

---

## 2. On-Module I2C LED Driver IC Selection

### 2.1 Requirements

From NP-FW-PBM1064-001 Rev A §3.3 and NP-HW-FPC-001 Rev E §6:

| Requirement | Value |
|-------------|-------|
| Interface | I2C, address 0x30, 100 kHz |
| Channels | 3 independently controlled |
| Per-channel current | 80–200 mA, ≥ 1 mA resolution |
| Per-channel PWM frequency | 0.5–100 Hz programmable |
| Duty cycle resolution | ≥ 8-bit |
| Per-channel enable/disable | Required |
| Status register | Thermal fault, OCP, open-LED flags |
| Startup time | ≤ 5 ms to I2C ACK |
| Package | Fits on 22 × 14 mm rigidizer PCB |
| BOM target | ≤ $2.00 driver IC + passives + FETs |

### 2.2 Candidate Evaluation

#### Option A: Direct-Drive Dedicated LED Driver IC

Candidates: ROHM BD2606MVV (3-ch I2C), NXP PCA9956B (24-ch), TI TPS92518 (2-ch).

**ROHM BD2606MVV:**
- 3-channel I2C LED driver ✓
- Max per-channel output: ~35 mA — **fails 80–200 mA requirement** ✗
- PWM frequency: internal fixed, not programmable to 0.5 Hz ✗

**NXP PCA9956B:**
- 24-channel, I2C ✓
- Max per-channel: 57 mA direct drive — **fails 80–200 mA** ✗
- PWM: 97 Hz fixed — **fails 0.5–40 Hz therapeutic pulsing** ✗

**TI TPS92518:**
- 2-channel only — **fails 3-channel requirement** ✗

**Conclusion:** No commercially available dedicated LED driver IC meets all three simultaneous requirements (80–200 mA, 0.5–40 Hz, 3 channels). Direct-drive approach is **not viable**.

#### Option B: I2C PWM Controller + External FETs

Candidates: NXP PCA9685 (16-ch 12-bit PWM controller) + IRLML6344 N-FETs.

**NXP PCA9685:**
- 16-channel 12-bit PWM ✓
- I2C, address configurable ✓
- PWM frequency: 24–1526 Hz via pre-scaler — **minimum 24 Hz; cannot generate 0.5–10 Hz** ✗
- External FETs would handle current ✓

**Conclusion:** PCA9685 fails the 0.5–10 Hz therapeutic pulsing frequency requirement. Not suitable.

#### Option C: I2C Slave MCU + External FETs (Recommended)

A small microcontroller programmed as an I2C slave precisely implements the register map from NP-FW-PBM1064-001 Rev A §5.1. Three MCU PWM outputs drive N-channel MOSFET gates; the FETs switch LED current through series sense resistors.

**Selected: Microchip ATtiny402 + 3× Infineon IRLML6344**

**ATtiny402 evaluation:**

| Requirement | ATtiny402 capability | Pass/Fail |
|-------------|---------------------|-----------|
| I2C slave @ 0x30 | TWI peripheral, address configurable in firmware | ✓ |
| 3× PWM channels, 0.5–100 Hz | TCA split mode + TCB: 3 independent 8-bit PWM channels; any frequency from ~0.03 Hz to 10 MHz | ✓ |
| 8-bit duty cycle resolution | TCA 8-bit timer | ✓ |
| Status register + flags | Firmware-implemented in I2C register map | ✓ |
| Thermal fault @ 62°C | Internal TEMPSENSE ADC, factory-calibrated ±10°C | ✓ |
| OCP detection | ADC measures sense resistor voltage; threshold in firmware | ✓ |
| Open-LED detection | ADC measures LED string voltage via divider | ✓ |
| Startup to I2C ACK ≤ 5 ms | POR reset: 100 µs; firmware init: < 1 ms; total < 2 ms | ✓ |
| Package (22 × 14 mm rigidizer) | SOT-23-8: 2.9 × 2.8 mm | ✓ |
| Supply voltage: 3.3V (from FPC pin 12) | ATtiny402 VCC: 1.8–5.5 V | ✓ |

**IRLML6344 evaluation:**

| Requirement | IRLML6344 | Pass/Fail |
|-------------|-----------|-----------|
| Gate drive 3.3V fully enhanced | VGS_th = 0.4–1.0 V; RDS(on) = 27 mΩ at VGS = 2.7 V | ✓ |
| ID ≥ 200 mA (pulsed) | ID = 5 A continuous, 20 A pulsed | ✓ |
| SOT-23 package | SOT-23 ✓ | ✓ |
| Thermal: 3 FETs at 180 mA max | P = 0.18² × 0.027 = 0.87 mW per FET | ✓ |

**BOM breakdown:**

| Component | Part | Qty | Unit price (volume) | Total |
|-----------|------|-----|---------------------|-------|
| I2C slave MCU | ATtiny402-SSN (SOT-23-8) | 1 | $0.38 | $0.38 |
| N-MOSFET CH_A | IRLML6344TRPBF (SOT-23) | 1 | $0.12 | $0.12 |
| N-MOSFET CH_B | IRLML6344TRPBF (SOT-23) | 1 | $0.12 | $0.12 |
| N-MOSFET CH_C | IRLML6344TRPBF (SOT-23) | 1 | $0.12 | $0.12 |
| Current sense R, CH_A–C | 1.0 Ω, 0.25W, 1%, 0402 | 3 | $0.02 | $0.06 |
| Gate resistors | 10 Ω, 0402 | 3 | $0.005 | $0.015 |
| Decoupling 100 nF | 0402, X5R, 10V | 1 | $0.01 | $0.01 |
| Bulk cap 10 µF | 0402, X5R, 6.3V | 1 | $0.03 | $0.03 |
| PCB rigidizer (FR4, 22×14mm) | 2-layer, 0.8 mm | 1 | $0.15 | $0.15 |
| **Total** | | | | **$1.00** |

**Total driver IC BOM: $1.00 — 50% below $2.00 target.**

### 2.3 Current Sense Resistor Value

Series sense resistors at 1.0 Ω, 1%:
- At 80 mA: Vsense = 80 mV
- At 180 mA: Vsense = 180 mV
- At 200 mA (OCP limit): Vsense = 200 mV

ATtiny402 ADC (VREF = internal 2.5 V, 10-bit) resolution: 2.5 V / 1024 = 2.44 mV/count. At 180 mA: 180 mV / 2.44 = 74 counts — adequate for current monitoring and OCP detection.

OCP threshold register (NP-FW-PBM1064-001 Rev A §5.1, STATUS byte OCP bits): set in ATtiny402 firmware at ADC count corresponding to 220 mA (226 mV → 93 counts). Safety margin: 22% above 180 mA maximum operating current.

Power dissipation in sense resistor at 180 mA: P = 0.18² × 1.0 = **32.4 mW**. At 0.25W rated 0402 resistor: 13% derating — acceptable.

### 2.4 ATtiny402 Firmware (OI-PBM-HW-05)

Firmware document: **NP-FW-ZM-TINY402-001** (to be authored). Firmware shall implement:
- I2C TWI peripheral in slave mode, fixed address 0x30, 100 kHz
- Register map exactly per NP-FW-PBM1064-001 Rev A §5.1 (registers 0x00–0x0D)
- TCA split mode: WO0/WO1/WO2 → Q1/Q2/Q3 gate drive (channels CH_A/CH_B/CH_C)
- TCB in single-shot mode: optionally used for OCP sampling timing
- PWM frequency calculation: PERIOD register = (F_CPU / (prescaler × freq_hz)) − 1; F_CPU = 20 MHz (internal oscillator, ±3%)
- TEMPSENSE ADC sampling: 1 Hz background; raise THERMAL flag when temperature > THERMAL register value
- OCP ADC sampling: per channel, 1 kHz background; raise OCP flag when threshold exceeded for ≥3 consecutive samples

UPDI programming: performed on the rigidizer sub-board before assembly into module shell. Programming fixture must access UPDI pad on rigidizer PCB. Firmware is locked (read-out protection enabled) after programming.

---

## 3. 1064nm LED Emitter Binning Specification

### 3.1 Overview

This section supplements NP-PROC-FPC-001 Rev A with procurement requirements for 1064nm LED emitters used in the smart module CH_C channel. The structure mirrors the existing 660nm and 808nm sections of NP-PROC-FPC-001 Rev A.

### 3.2 Wavelength Specification

| Parameter | Requirement | Rationale |
|-----------|-------------|-----------|
| Peak emission wavelength | 1064 nm ± 5 nm | CCO absorption secondary peak; matches 1064nm Nd:YAG literature wavelength for PBM |
| Spectral half-width (FWHM) | ≤ 30 nm | Narrow enough to avoid broadband IR noise; wider than laser but sufficient for LED-based PBM |
| Operating temperature shift | ≤ +0.3 nm/°C | At 42°C junction limit (IEC 60601), shift < 6 nm — remains within ±5 nm band |

### 3.3 Electrical and Optical Specifications

| Parameter | Requirement | Notes |
|-----------|-------------|-------|
| Forward voltage Vf | 1.9–2.3 V nominal at 150 mA | 1064nm GaAs/AlGaAs emitters; Vf lower than 660nm (1.8–2.0 V) and 808nm (1.6–1.8 V) |
| Vf binning tolerance | ±0.1 V max spread within a single module lot | For uniform parallel string current sharing on FPC. Wider spread causes current hot-spots. |
| Drive current: continuous | 80–180 mA | Nominal drive range per LED |
| Drive current: pulsed | 200 mA at ≤ 25% duty cycle, ≤ 10 ms pulse width | Firmware-enforced ceiling (NP-FW-PBM1064-001 Rev A §5.5) |
| Wall-plug efficiency (WPE) | ≥ 10% at 150 mA | 1064nm GaAs emitters are less efficient than 808nm (WPE ~30–40%). Minimum 10% acceptable for this application. |
| Radiant flux at 150 mA | ≥ 45 mW per LED | From 10% WPE × 150 mA × 2.1 V = 31.5 mW minimum; specify 45 mW as procurement floor |
| L70 lifetime | ≥ 80,000 hours at rated drive | Same requirement as 660nm/808nm emitters in NP-PROC-FPC-001 Rev A §4.3 |
| Lumen maintenance binning | ≤ 15% flux spread within a single module lot | Consistent irradiance across zone |

### 3.4 Package Requirements

| Parameter | Requirement |
|-----------|-------------|
| Package family | SMD, reflow-compatible; same footprint as 660nm/808nm emitters used in base module (NP-PROC-FPC-001 Rev A §3.1) |
| Footprint compatibility | Must match existing LED pads on NP-FPC-ZM-SM-01 artwork |
| Thermal pad | Exposed thermal pad or bottom-side pad preferred for FPC heat spreading |
| Window material | Epoxy lens or flat-top; PDMS optical window above LED (per zone module design) provides diffusion |
| RoHS / REACH | Compliant; IPC/JEDEC J-STD-020E MSL rating ≤ 3 |

### 3.5 Irradiance Budget

At 150 LEDs per zone module at 150 mA nominal, 45 mW per LED:
- Total optical power: 150 × 45 mW = 6,750 mW per zone
- Zone area: 66 × 78 mm = 51.48 cm²
- Peak irradiance (CW): 6,750 mW / 51.48 cm² = **131 mW/cm²**
- At 25% duty cycle (pulsed): **131 mW/cm² peak pulsed**
- Average irradiance at 25% duty: 33 mW/cm²

This is lower than the 400 mW/cm² base module spec for 660/808nm (which has 300 LEDs per wavelength at higher WPE). The reduced irradiance reflects the lower WPE of 1064nm GaAs emitters.

Session dose at 131 mW/cm² peak, 25% duty, 20-minute session:
- Average irradiance: 32.75 mW/cm²
- Dose: 32.75 mW/cm² × 0.001 W/mW × 1200 s = **39.3 J/cm²**

This exceeds the 36 J/cm² per-session limit in NP-FW-PBM1064-001 Rev A §6.5. The firmware dose limit will gate the session at 36 J/cm², which corresponds to: 36 J/cm² / (0.13075 W/cm²) = **275 seconds = 4.6 minutes** of CW-equivalent dose time, or approximately **18.4 minutes at 25% duty cycle**. This is consistent with the 20-minute session target with a ~2-minute ramp-down buffer.

If higher irradiance emitters become available (WPE ≥ 15%), reconfirm dose limit with regulatory opinion (OI-SES-01).

### 3.6 Supplier Candidates and Selection

| Supplier | Product | λ (nm) | Vf at 150mA | Flux at 150mA | WPE | Package | Availability |
|----------|---------|---------|-------------|---------------|-----|---------|-------------|
| **EPITEX (Marubeni America)** | W1064W50C or similar 1064nm series | 1064 ± 5 | ~2.1 V | 45–60 mW | ~14–19% | SMD-5050 compatible | Volume: contact Marubeni America (primary source) |
| Roithner Laser Technik | H2O-1064L | 1064 | ~2.0 V | ~35–50 mW | ~10–15% | TO-18 (non-preferred) | Small quantities; SMD variant availability TBC |
| OSI Optoelectronics | LD-1064-SMD (custom) | 1064 ± 10 | ~2.2 V | ~30–45 mW | ~8–12% | SMD | Custom order; long lead time |
| Jenoptik | S0 series 1064nm | 1064 | ~1.9–2.1 V | ~50–80 mW | ~15–20% | Custom — not SMD standard | High cost; laser-adjacent |

**Selected primary supplier: EPITEX via Marubeni America.**

Rationale:
- Purpose-built NIR LED series covering 950–1300nm including 1064nm
- SMD package compatible with FPC reflow process
- WPE ≥ 14% — meets ≥10% requirement with margin
- Established supplier with volume pricing; accessible via Marubeni America North American distribution
- Binning programs available per customer specification

**Qualification requirements:**
- Submit Vf binning request: ±0.1 V maximum spread within a lot of 10,000 units minimum
- Submit optical flux binning request: ±15% maximum spread
- Obtain application note or test data for L70 lifetime at 150 mA drive; target ≥ 80,000 hours
- Confirm footprint compatibility with NP-FPC-ZM-SM-01 pad layout before FPC artwork release (OI-PBM-HW-04)
- Order ≥ 500 units for bench qualification (FAI-SM-04, FAI-SM-06)

**Backup supplier:** Roithner Laser Technik H2O-1064L (SMD variant if available). Spec risk: OSI Optoelectronics SMD variant has unconfirmed availability; Jenoptik excluded due to package incompatibility and cost.

### 3.7 Thermal Derating

1064nm GaAs LED Vf has a temperature coefficient of approximately −1.5 mV/°C (typical for GaAs). At maximum junction temperature 42°C (IEC 60601 scalp surface limit + 20°C junction-to-case), Vf reduces by: 20°C × 1.5 mV/°C = 30 mV from 25°C nominal. This has negligible effect on string current (< 2% change across 1.0 Ω series resistance FET configuration). No thermal derating of current setpoint is required.

Wavelength shift: +6 nm over 20°C range → 1070 nm maximum. Remains within ±5 nm therapeutic band for 1064nm target (1059–1069 nm range allows the shift).

---

## 4. InGaAs Photodiode Selection (PD1 and PD2)

### 4.1 Requirements

| Parameter | Requirement | Rationale |
|-----------|-------------|-----------|
| Wavelength sensitivity | Peak responsivity at 1064 nm ≥ 0.70 A/W | 1064nm LED emission; silicon PDs are blind beyond ~1100 nm |
| Spectral range | 900–1700 nm minimum | Covers 808nm CH_B, 1064nm CH_C, and 1170nm T2 reference |
| Active area | ≥ 0.5 mm² | Sufficient photocurrent at dose-metering irradiance levels (§4.3) |
| Package | SMD or compatible with 1.6 mm annular ring pad on FPC | Must match NP-HW-FPC-001 Rev E §5 footprint |
| Response time | < 1 ms | 10 Hz dose accumulation tick (100 ms period) |
| Operating temperature | −20°C to +70°C | Storage and in-use range |
| Dark current | < 10 nA at 0V reverse bias | Low bias preferred; TIA front-end designed for low dark current |
| RoHS | Compliant | |

### 4.2 Candidate Evaluation

| Candidate | λ peak | Resp at 1064nm | Active area | Package | Dark Icurrent | Cost (ea) |
|-----------|--------|---------------|-------------|---------|--------------|-----------|
| **Hamamatsu G12180-010A** | 1550 nm | ~0.90 A/W | 1.0 mm² | SMD (ceramic, 5.0×5.0 mm) | < 1 nA @ 0V | ~$9–12 |
| Hamamatsu G12183-010A | 1550 nm | ~0.90 A/W | 1.0 mm² | TO-18 | < 1 nA | ~$8–10 |
| OSI Optoelectronics PIN-10D | 1650 nm | ~0.85 A/W | 1.0 mm² | TO-46 | < 1 nA | ~$6–8 |
| Excelitas C30724EH | 1650 nm | ~0.80 A/W | 0.5 mm² | SMD | < 5 nA | ~$5–7 |
| Thorlabs FGA01FC | 1800 nm | ~0.90 A/W | 0.03 mm² (fiber-coupled) | FC/PC fiber — **excluded** | n/a | ~$180 |

**Selected: Hamamatsu G12180-010A for both PD1 and PD2.**

Rationale:
- SMD package (5.0 × 5.0 mm ceramic) — compatible with FPC reflow and 1.6 mm annular ring pad approach (package mounted on annular ring pad with wire bond or direct SMD pad if footprint matched)
- Responsivity 0.90 A/W at 1064 nm — exceeds 0.70 A/W requirement by 29% margin
- 1.0 mm² active area — same as base module silicon PDs; calibration coefficients can use consistent area normalisation
- Dark current < 1 nA at 0V — zero-bias TIA design is optimal; no reverse bias required
- Hamamatsu is a qualified NeuroPulse supplier (used for reference photodiodes in other applications); qualification path is shorter
- Rise time: < 10 ns (datasheet); far exceeds < 1 ms requirement

**Package note:** The G12180-010A is a ceramic SMD package (5.0 × 5.0 × 1.5 mm). The FPC annular ring pad (1.6 mm diameter) is sized for the wire-bonded active area contact. For SMD mounting on the FPC rigidizer/zone module FPC, the pad footprint must be updated in the FPC artwork to match the G12180-010A SMD land pattern (Hamamatsu application note for G121xx series provides recommended PCB footprint). **OI-PBM-HW-06:** Update NP-FPC-ZM-SM-01 FPC artwork (OI-PBM-HW-04) to include Hamamatsu G12180-010A SMD footprint for PD1 and PD2 positions. Maintain annular ring centre position at X=33.0, Y=39.0 mm.

### 4.3 TIA Gain Compatibility

Detailed analysis is in NP-HW-FPC-001 Rev E §5.3. Summary of findings:

- InGaAs responsivity at 1064nm (~0.90 A/W) is approximately 2× higher than silicon responsivity at 808nm (~0.45 A/W)
- With base module TIA gain (Rf = 47 kΩ assumed), InGaAs PD1 output saturates at the 3.3V ADC rail at moderate irradiance levels
- **Required action (BLOCKING):** Hub PCB must add gain selection per slot (analog switch, GPIO-controlled from ZONE_ID threshold detection): Rf_smart = 22 kΩ for smart module slots vs Rf_base = 47 kΩ for base module slots
- Recommended analog switch: Vishay DG2788A (single-supply SPDT, Rds(on) < 5 Ω, SOT-23-6) in series with the gain resistor; ZONE_ID < 1100 counts closes the gain-reduction path; OI-PBM-HW-01

At Rf = 22 kΩ for smart module slots:
- PD1 at 8 mW/cm² irradiance (max PBM1064 forward emission estimate): 1.58 V — safe ✓
- PD2 at 3 mW/cm² (max backscatter): 0.59 V — safe ✓
- ADC dynamic range (12-bit, 3.3V ref): 1.58V / 3.3V = 48% of range — good linearity margin

Calibration K coefficients per NP-FW-PBM1064-001 Rev A §6.2 absorb the gain change into factory calibration.

### 4.4 Per-Unit Cost and BOM Impact

| Component | Qty per module | Unit cost | Total |
|-----------|---------------|-----------|-------|
| Hamamatsu G12180-010A (PD1) | 1 | $10.00 | $10.00 |
| Hamamatsu G12180-010A (PD2) | 1 | $10.00 | $10.00 |
| **Total InGaAs PD BOM** | **2** | | **$20.00** |

Volume pricing: at 10,000+ units/year, Hamamatsu G12180-010A is typically $8–10 in volume. Target NRE-free production pricing: < $20.00 for the pair.

Silicon PD pair in base module: ~$1.50–3.00 total. InGaAs PD pair adds **+$17–18** to base module BOM.

---

## 5. Summary: Smart Module BOM Delta vs. Base Zone Module

| Item | Base module | Smart module delta | Smart module total |
|------|-------------|-------------------|-------------------|
| 660nm LED emitters (200 pcs) | 300 pcs (per λ) | −100 pcs @ $0.08 = −$8.00 | $16.00 |
| 808nm LED emitters (200 pcs) | 300 pcs (per λ) | −100 pcs @ $0.08 = −$8.00 | $16.00 |
| 1064nm LED emitters (150 pcs) | — | +150 pcs @ $0.10 = +$15.00 | $15.00 |
| Silicon PD pair | $1.50 | Replaced by InGaAs: +$18.50 | $20.00 |
| Driver IC (BCR421W + passives) | $0.60 | Replaced by ATtiny402 combo: +$0.40 | $1.00 |
| FPC artwork NP-FPC-ZM-SM-01 | NP-FPC-ZM-01 | +$3.00 (new artwork, same process) | $3.00 |
| Mould variant NP-MOULD-ZM-SM-01 | Included in ZM-001 | Separate mould amortised at 5,000 units: ~$5.00/unit | ~$5.00 |
| Rigidizer PCB | — | +$0.15 | $0.15 |
| ZONE_ID 3.3kΩ 1% | 10kΩ–220kΩ per zone | Minimal delta | $0.02 |
| **Module BOM sub-total** | **~$18–22** | **+$23–28** | **~$41–50** |
| Hub PCB delta (amortised, OI-PBM-HW-01/02) | — | ~$1–2/unit at scale | ~$2 |
| **Total per smart module** | | | **~$43–52** |

**Estimated retail pricing:** $149–199 per smart zone module position (single zone); $599–699 for all-5-zone smart module kit. Gross margin ~40–55% at scale.

---

## 6. Supplier and Procurement Action Items

| Action | Supplier | Contact | Timeline | Blocking |
|--------|----------|---------|----------|---------|
| Request Vf + flux binning program | EPITEX / Marubeni America | Marubeni America NIR LED sales | 4–6 weeks | OI-PBM-HW-04 (FPC artwork) |
| Order 500 units for bench qualification | EPITEX / Marubeni America | Same | 8 weeks ARO | FAI-SM-04, FAI-SM-06 |
| Obtain L70 test data ≥ 80,000 hr | EPITEX | Supplier qualification | 2–4 weeks | NP-PROC-SUP-001 CAT-A (LED supplier) |
| InGaAs PD G12180-010A qualification | Hamamatsu Photonics K.K. | hamamatsu.com/us | 2 weeks | FAI-SM-06/07/08 |
| Order 200 units G12180-010A for bench qualification | Hamamatsu / distribution | Digi-Key, Mouser, or direct | 2–3 weeks | FAI-SM-06 |
| ATtiny402 firmware NP-FW-ZM-TINY402-001 | Internal firmware team | OI-PBM-HW-05 | 3–4 weeks | FAI-SM-02/04 |
| Hub PCB TIA gain switch (OI-PBM-HW-01) | Hub PCB designer | Internal | 3–4 weeks | FAI-SM-06/07/08 |
| Hub PCB I2C bus switch (OI-PBM-HW-02) | Hub PCB designer | Internal | 3–4 weeks | FAI-SM-02/04 |

---

## 7. Open Items

| ID | Description | Blocking |
|----|-------------|---------|
| OI-PBM-HW-01 | Hub PCB TIA gain selection switch per slot (Vishay DG2788A SPDT analog switch + Rf = 22 kΩ for smart slots) | FAI-SM-06/07/08 — BLOCKING |
| OI-PBM-HW-02 | Hub PCB I2C bus switch 5-slot (NXP PCA9546A or TI TCA9548A) | FAI-SM-02/04 |
| OI-PBM-HW-03 | Hub PCB 3.3V rail budget: verify 5 × 50 mA = 250 mA smart module simultaneous load | Pre-prototype |
| OI-PBM-HW-04 | NP-FPC-ZM-SM-01 FPC artwork: Gerbers with updated pinout, LED array, InGaAs PD footprint (G12180-010A land pattern), rigidizer pads | FAI-SM-01 |
| OI-PBM-HW-05 | ATtiny402 firmware NP-FW-ZM-TINY402-001 | FAI-SM-02/04 |
| OI-PBM-HW-06 | Update FPC artwork (OI-PBM-HW-04) to include G12180-010A SMD footprint at PD1/PD2 annular ring positions | OI-PBM-HW-04 |
| OI-PBM-HW-07 | 1064nm LED emitter: confirm Marubeni America EPITEX part number; obtain binning program confirmation; confirm SMD footprint match to FPC artwork | FAI-SM-04/06 |

---

*NP-PROC-FPC-1064-001 Rev A — Smart Zone Module Component Selection and 1064nm LED Emitter Procurement Specification. Addresses Issue #54 DoD: I2C driver IC selected, InGaAs PD selected, TIA gain compatibility confirmed, 1064nm LED binning spec added.*
