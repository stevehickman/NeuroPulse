# 1064nm Smart Zone Module — Component Selection and Procurement Specification

**Project:** NeurOne  
**Document:** NP-PROC-FPC-1064-001  
**Revision:** 2
**Date:** 2026-09-21  
**Status:** BASELINED  
**Effective Date:** 2026-05-13  
**Author:** NeurOne Hardware Engineering  
**Approved By:** Steve Hickman, CEO  
**References:** NP-HW-FPC-001 Rev 5; NP-TOOL-ZM-SM-001 Rev 1; NP-FW-PBM1064-001 Rev 1  
**Related Issues:** GitHub Issue #54  
**Gate:** —  
**IEC 62304 Class:** —  
**Supersedes:** —  
**Parent Document:** NP-PROC-FPC-001 **Rev 4** (base module FPC procurement — the 660/808 nm binning specifications this document inherits are unchanged by Rev 4; *the "Rev 1" this field carried was the parent's stale header read through* `NP-CONV-001` *§4.1, see that document's Rev 4 banner*)

---

> **⚠ Rev 2 (2026-09-21) — §3.2 and §3.7's thermal compliance argument does not hold, and the wavelength row contradicts itself on its face. `OI-PBM-HW-08` raised. No requirement is changed and no part is re-selected.**
>
> Found while tracing `NP-PROC-FPC-001` §2.3's `T_j` requirement for GitHub #333. **The junction temperature both sections compute against is wrong, and the band-compliance conclusion fails even at the temperature they use.**
>
> **1. 42 °C is the scalp limit, not the junction limit.** §3.7 reads *"At maximum junction temperature 42°C (IEC 60601 scalp surface limit + 20°C junction-to-case)"* — the parenthetical describes `42 + 20 = 62` and then labels the result **42**. `CLAUDE.md` §4.2 and `NP-THERM-BEZEL-001` §4.1 are unambiguous: **42 °C is the scalp face (IEC 60601-1 applied part) and 62 °C is the junction throttle**, and §4.1 states the 20 °C gap is deliberate — *"keep the scalp cool while letting the junction run warm."* The sentence contains its own correction.
>
> **2. The arithmetic does not match either number.** Both sections use **ΔT = 20 °C** from 25 °C nominal, i.e. an effective junction of **45 °C** — neither the 42 °C they state (ΔT = 17) nor the 62 °C the interlock permits (**ΔT = 37**).
>
> **3. The V_f conclusion survives; the wavelength conclusion does not.** At the real ΔT = 37 °C the V_f drift is `37 × 1.5 mV/°C` = **55.5 mV**, not 30 mV — larger, still small, and §3.7's *"no thermal derating of current setpoint is required"* stands on a constant-current driver. **But the wavelength row fails twice over.** §3.2 states *"shift < 6 nm — remains within ±5 nm band"*: **6 nm is not within ±5 nm.** §3.7 states *"+6 nm over 20°C range → 1070 nm maximum. Remains within ±5 nm therapeutic band … (1059–1069 nm range allows the shift)"*: **1070 nm is above the 1069 nm ceiling it names in the same sentence.** Both are self-contradicting before any temperature correction. At the correct ΔT = 37 °C the shift is `37 × 0.3 nm/°C` = **11.1 nm → 1075 nm**, which is **6 nm outside** the ±5 nm band — roughly twice the excursion the band allows.
>
> **What this probably does NOT mean, stated so the correction is not over-read.** The ±5 nm band in §3.2 is justified as *"matches 1064nm Nd:YAG literature wavelength"*, and the 1064 nm claim has since **moved**: `OI-HEXTILE-21` and commit `f8806dd` re-authored `clinical-03` against the **Alzheimer's protocol at 1060–1080 nm** (Grade A, `docs/pbm_neuro_protocols.md`) rather than the cognitive one. **1075 nm sits inside 1060–1080 nm with margin.** So the likely resolution is that the ±5 nm procurement band is tighter than the therapeutic target now requires and should be **re-derived against the band the claim is actually made in** — not that the emitter is unusable. That re-derivation is a decision, and this revision does not take it.
>
> **Nothing in §3.3, §3.4, §3.5, §3.6, §4, §5 or §6 changes. `OI-PBM-HW-07` (part confirmation) is untouched.** The 2026-07-28 supersession below is unaffected and still governs module count, BOM and pricing.

> **⚠ SUPERSEDED (2026-07-28) — module-count and BOM figures retired, do not use for BOM/pricing work.** This spec prices and sizes the smart module against the retired large-format (66×78mm), 5-zone-slot architecture: 150× 1064nm LEDs on a single 550-LED module, and retail pricing keyed to "all-5-zone smart module kit." `NP-HEX-ZM-001` replaced that with a universal 40mm hex tile (~90 elements max per tile — see `docs/np_hex_zm_001.md` §3.1) tiling ~30–80 sockets; per-module LED count, BOM, and kit pricing all need re-derivation against the new tile size, not this document's numbers.
>
> **Still-reusable:** the component *selections* — 1064nm emitter (EPITEX via Marubeni), driver IC (ATtiny402 + IRLML6344 FETs), InGaAs PD (Hamamatsu G12180-010A) — and the Vf-binning specification methodology (§3). These are part-level decisions independent of how many go on a tile or how many tiles exist. The mould-variant reference (NP-TOOL-ZM-SM-001) is itself superseded — see that document's own supersession note.

---

## 1. Scope

This document covers component selection and procurement specifications for the three non-standard components in the 1064nm smart zone module:

1. **On-module I2C LED driver IC combination** — selected and evaluated (§2)
2. **InGaAs photodiodes PD1 and PD2** — selected and evaluated (§4)
3. **1064nm LED emitter binning specification** — procurement spec (§3)

It also covers the TIA gain compatibility analysis (§5) and full smart module BOM summary (§6).

This document supplements NP-PROC-FPC-001 Rev 4. The existing 660nm and 808nm LED emitter binning specs in NP-PROC-FPC-001 §2.1 are unchanged and apply to the 660nm and 808nm emitters in the smart module's CH_A and CH_B channels — **and Rev 4 §2.1 adds that the ±0.10 V within-order bin is load-bearing for string construction, not only for RISK-08 current matching. That consequence carries to CH_C**: the ±0.1 V requirement in §3 below is a string-length premise as well as a current-matching one.

---

## 2. On-Module I2C LED Driver IC Selection

### 2.1 Requirements

From NP-FW-PBM1064-001 Rev 1 §3.3 and NP-HW-FPC-001 Rev 5 §6:

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

A small microcontroller programmed as an I2C slave precisely implements the register map from NP-FW-PBM1064-001 Rev 1 §5.1. Three MCU PWM outputs drive N-channel MOSFET gates; the FETs switch LED current through series sense resistors.

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

OCP threshold register (NP-FW-PBM1064-001 Rev 1 §5.1, STATUS byte OCP bits): set in ATtiny402 firmware at ADC count corresponding to 220 mA (226 mV → 93 counts). Safety margin: 22% above 180 mA maximum operating current.

Power dissipation in sense resistor at 180 mA: P = 0.18² × 1.0 = **32.4 mW**. At 0.25W rated 0402 resistor: 13% derating — acceptable.

### 2.4 ATtiny402 Firmware (OI-PBM-HW-05)

Firmware document: **NP-FW-ZM-TINY402-001** (to be authored). Firmware shall implement:
- I2C TWI peripheral in slave mode, fixed address 0x30, 100 kHz
- Register map exactly per NP-FW-PBM1064-001 Rev 1 §5.1 (registers 0x00–0x0D)
- TCA split mode: WO0/WO1/WO2 → Q1/Q2/Q3 gate drive (channels CH_A/CH_B/CH_C)
- TCB in single-shot mode: optionally used for OCP sampling timing
- PWM frequency calculation: PERIOD register = (F_CPU / (prescaler × freq_hz)) − 1; F_CPU = 20 MHz (internal oscillator, ±3%)
- TEMPSENSE ADC sampling: 1 Hz background; raise THERMAL flag when temperature > THERMAL register value
- OCP ADC sampling: per channel, 1 kHz background; raise OCP flag when threshold exceeded for ≥3 consecutive samples

UPDI programming: performed on the rigidizer sub-board before assembly into module shell. Programming fixture must access UPDI pad on rigidizer PCB. Firmware is locked (read-out protection enabled) after programming.

---

## 3. 1064nm LED Emitter Binning Specification

### 3.1 Overview

This section supplements NP-PROC-FPC-001 Rev 4 with procurement requirements for 1064nm LED emitters used in the smart module CH_C channel. The structure mirrors the existing 660nm and 808nm sections of NP-PROC-FPC-001 Rev 5.

### 3.2 Wavelength Specification

| Parameter | Requirement | Rationale |
|-----------|-------------|-----------|
| Peak emission wavelength | 1064 nm ± 5 nm | CCO absorption secondary peak; matches 1064nm Nd:YAG literature wavelength for PBM |
| Spectral half-width (FWHM) | ≤ 30 nm | Narrow enough to avoid broadband IR noise; wider than laser but sufficient for LED-based PBM |
| Operating temperature shift | ≤ +0.3 nm/°C | ⚠ **Rationale WITHDRAWN Rev 2, requirement unchanged — `OI-PBM-HW-08`.** It read: *"At 42°C junction limit (IEC 60601), shift < 6 nm — remains within ±5 nm band."* **6 nm is not within ±5 nm**, so the conclusion contradicts its own premise; and **42 °C is the scalp-face limit, not the junction limit** — the junction throttle is **62 °C** (`CLAUDE.md` §4.2). At ΔT = 37 °C from 25 °C nominal the shift is **11.1 nm → 1075 nm**, **6 nm outside** the ±5 nm band. See the Rev 2 banner: the likely fix is re-deriving this band against the **1060–1080 nm** Alzheimer's target the 1064 nm claim now rests on, which 1075 nm satisfies |

### 3.3 Electrical and Optical Specifications

| Parameter | Requirement | Notes |
|-----------|-------------|-------|
| Forward voltage Vf | 1.9–2.3 V nominal at 150 mA | 1064nm GaAs/AlGaAs emitters; Vf lower than 660nm (1.8–2.0 V) and 808nm (1.6–1.8 V) |
| Vf binning tolerance | ±0.1 V max spread within a single module lot | For uniform parallel string current sharing on FPC. Wider spread causes current hot-spots. |
| Drive current: continuous | 80–180 mA | Nominal drive range per LED |
| Drive current: pulsed | 200 mA at ≤ 25% duty cycle, ≤ 10 ms pulse width | Firmware-enforced ceiling (NP-FW-PBM1064-001 Rev 1 §5.5) |
| Wall-plug efficiency (WPE) | ≥ 10% at 150 mA | 1064nm GaAs emitters are less efficient than 808nm (WPE ~30–40%). Minimum 10% acceptable for this application. |
| Radiant flux at 150 mA | ≥ 45 mW per LED | From 10% WPE × 150 mA × 2.1 V = 31.5 mW minimum; specify 45 mW as procurement floor |
| L70 lifetime | ≥ 80,000 hours at rated drive | Same requirement as 660nm/808nm emitters in NP-PROC-FPC-001 **§2.3** (*corrected 2026-09-21 — this cell cited §4.3, which does not exist; §4 of that document is the Hirose connector*) |
| Lumen maintenance binning | ≤ 15% flux spread within a single module lot | Consistent irradiance across zone |

### 3.4 Package Requirements

| Parameter | Requirement |
|-----------|-------------|
| Package family | SMD, reflow-compatible; same footprint as 660nm/808nm emitters used in base module (NP-PROC-FPC-001 **§2.3** — *corrected 2026-09-21 from §3.1, which does not exist; §3 of that document is the BCR421W driver IC*) |
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

This exceeds the 36 J/cm² per-session limit in NP-FW-PBM1064-001 Rev 1 §6.5. The firmware dose limit will gate the session at 36 J/cm², which corresponds to: 36 J/cm² / (0.13075 W/cm²) = **275 seconds = 4.6 minutes** of CW-equivalent dose time, or approximately **18.4 minutes at 25% duty cycle**. This is consistent with the 20-minute session target with a ~2-minute ramp-down buffer.

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

> **⚠ CORRECTED Rev 2 (2026-09-21) — `OI-PBM-HW-08`. The two paragraphs above are retained as the record of
> what was computed; neither is usable as written.**
>
> **The junction temperature is wrong.** *"maximum junction temperature 42°C (IEC 60601 scalp surface limit +
> 20°C junction-to-case)"* — the parenthetical describes `42 + 20 = 62 °C` and then labels it 42. **42 °C is
> the scalp face; 62 °C is the junction throttle** (`CLAUDE.md` §4.2; `NP-THERM-BEZEL-001` §4.1, which states
> the 20 °C gap is deliberate). **And the arithmetic matches neither**: ΔT = 20 °C from 25 °C nominal is an
> effective junction of **45 °C**. The correct figure is **ΔT = 37 °C**.
>
> | Quantity | As written | At ΔT = 37 °C | Consequence |
> |---|---|---|---|
> | V_f drift | 30 mV | **55.5 mV** | Conclusion **survives** — still negligible against a constant-current driver; no setpoint derating needed |
> | Peak wavelength | +6 nm → 1070 nm | **+11.1 nm → 1075 nm** | Conclusion **fails** — ±5 nm allows 1059–1069 nm, so 1075 nm is **6 nm outside** |
>
> **The wavelength claim was already self-contradicting before the temperature correction**: *"1070 nm
> maximum. Remains within ±5 nm therapeutic band … (1059–1069 nm range allows the shift)"* states a maximum
> above the ceiling it names in the same sentence.
>
> **Not necessarily a part problem.** The ±5 nm band is justified in §3.2 as matching the Nd:YAG literature
> wavelength, and the 1064 nm claim has since moved to the **1060–1080 nm** Alzheimer's band
> (`OI-HEXTILE-21`; `clinical-03` re-authored at commit `f8806dd`), which **1075 nm satisfies with margin**.
> Re-deriving §3.2's band against the target the claim is now made in is `OI-PBM-HW-08` and is not decided here.
>
> **The `< 2 % string current` claim is also not checkable as stated** and was not corrected: it cites *"1.0 Ω
> series resistance FET configuration"* without a string length, and per-emitter V_f drift enters the string
> multiplied by N. Restating it with N, or deleting it in favour of the driver-regulation argument that
> actually carries the conclusion, is part of the same item.

---

## 4. InGaAs Photodiode Selection (PD1 and PD2)

### 4.1 Requirements

| Parameter | Requirement | Rationale |
|-----------|-------------|-----------|
| Wavelength sensitivity | Peak responsivity at 1064 nm ≥ 0.70 A/W | 1064nm LED emission; silicon PDs are blind beyond ~1100 nm |
| Spectral range | 900–1700 nm minimum | Covers 808nm CH_B, 1064nm CH_C, and 1170nm T2 reference |
| Active area | ≥ 0.5 mm² | Sufficient photocurrent at dose-metering irradiance levels (§4.3) |
| Package | SMD or compatible with 1.6 mm annular ring pad on FPC | Must match NP-HW-FPC-001 Rev 5 §5 footprint |
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
- Hamamatsu is a qualified NeurOne supplier (used for reference photodiodes in other applications); qualification path is shorter
- Rise time: < 10 ns (datasheet); far exceeds < 1 ms requirement

**Package note:** The G12180-010A is a ceramic SMD package (5.0 × 5.0 × 1.5 mm). The FPC annular ring pad (1.6 mm diameter) is sized for the wire-bonded active area contact. For SMD mounting on the FPC rigidizer/zone module FPC, the pad footprint must be updated in the FPC artwork to match the G12180-010A SMD land pattern (Hamamatsu application note for G121xx series provides recommended PCB footprint). **OI-PBM-HW-06:** Update NP-FPC-ZM-SM-01 FPC artwork (OI-PBM-HW-04) to include Hamamatsu G12180-010A SMD footprint for PD1 and PD2 positions. Maintain annular ring centre position at X=33.0, Y=39.0 mm.

### 4.3 TIA Gain Compatibility

Detailed analysis is in NP-HW-FPC-001 Rev 5 §5.3. Summary of findings:

- InGaAs responsivity at 1064nm (~0.90 A/W) is approximately 2× higher than silicon responsivity at 808nm (~0.45 A/W)
- With base module TIA gain (Rf = 47 kΩ assumed), InGaAs PD1 output saturates at the 3.3V ADC rail at moderate irradiance levels
- **Required action (BLOCKING):** Hub PCB must add gain selection per slot (analog switch, GPIO-controlled from ZONE_ID threshold detection): Rf_smart = 22 kΩ for smart module slots vs Rf_base = 47 kΩ for base module slots
- Recommended analog switch: Vishay DG2788A (single-supply SPDT, Rds(on) < 5 Ω, SOT-23-6) in series with the gain resistor; ZONE_ID < 1100 counts closes the gain-reduction path; OI-PBM-HW-01

At Rf = 22 kΩ for smart module slots:
- PD1 at 8 mW/cm² irradiance (max PBM1064 forward emission estimate): 1.58 V — safe ✓
- PD2 at 3 mW/cm² (max backscatter): 0.59 V — safe ✓
- ADC dynamic range (12-bit, 3.3V ref): 1.58V / 3.3V = 48% of range — good linearity margin

Calibration K coefficients per NP-FW-PBM1064-001 Rev 1 §6.2 absorb the gain change into factory calibration.

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
| **OI-PBM-HW-08** | **§3.2 / §3.7 thermal-and-wavelength compliance argument is unsound; re-derive the ±5 nm band against the target the 1064 nm claim now rests on** (raised Rev 2, 2026-09-21, found while tracing `NP-PROC-FPC-001` §2.3 for GitHub #333). Three defects: **(a)** both sections compute against a junction temperature of 42 °C, which is the **scalp-face** limit — the junction throttle is **62 °C**; **(b)** the arithmetic uses ΔT = 20 °C (effective junction 45 °C), matching neither 42 nor 62 — the correct figure is ΔT = 37 °C, at which the shift is **11.1 nm → 1075 nm**, **6 nm outside** the ±5 nm band; **(c)** both the §3.2 rationale and the §3.7 paragraph **contradict themselves before any correction** (*"shift < 6 nm — remains within ±5 nm band"*; *"1070 nm maximum … remains within … (1059–1069 nm range)"*). The V_f half of §3.7 survives at 55.5 mV. **Likely resolution is that the ±5 nm band is tighter than the claim now needs** — `OI-HEXTILE-21` and commit `f8806dd` moved the 1064 nm claim to the **1060–1080 nm** Alzheimer's band, which 1075 nm satisfies — but that is a decision, not a correction, and it is not taken here. Also restate or delete §3.7's *"< 2 % string current"* claim, which omits the string length the drift is multiplied by | §3.2 wavelength band on any 1064 nm PO; `OI-PBM-HW-07` part confirmation; 1064 nm claim wording |
