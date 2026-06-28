# 1064nm Smart Zone Module FPC Layout Variant

**Project:** NeuroPulse
**Document:** NP-HW-FPC-001
**Revision:** E
**Date:** 2026-05-13
**Status:** BASELINED
**Effective Date:** 2026-05-13
**Author:** Steve Hickman (CEO, interim Quality authority)
**Approved By:** Steve Hickman, CEO
**References:** NP-HW-FPC-001 Rev D; NP-FW-PBM1064-001 Rev A; NP-TOOL-ZM-SM-001 Rev A; NP-PROC-FPC-1064-001 Rev A
**Related Issues:** GitHub Issue #54
**Gate:** —
**IEC 62304 Class:** —
**Supersedes:** NP-HW-FPC-001 Rev D (base module pinout and FPC spec — unchanged)
**Parent Document:** NP-HW-FPC-001 Rev D

---

## 1. Scope

This document specifies the FPC layout variant for the 1064nm smart zone module accessory (SKU: NP-ZM-1064). It is a **variant document** layered on NP-HW-FPC-001 Rev D. All base module FPC decisions (connector, PD1/PD2 annular ring positions, PDMS bonding process, EEG channel routing separation, gasket groove geometry) are inherited unchanged. Only the differences are documented here.

The base module FPC (NP-HW-FPC-001 Rev D) is **not modified**. The smart module FPC is a separate part number: **NP-FPC-ZM-SM-01**.

---

## 2. Connector — Unchanged

**Hirose FH34S-20S-0.5SH** (20-pin, 0.5mm pitch, back-flip lever ZIF, ≥1,000 insertion cycles).

The smart module FPC mates to the same Hirose FH34S-20S-0.5SH ZIF receptacles on the hub PCB. No hub PCB connector change is required.

---

## 3. 20-Pin Connector Pinout — Smart Module Variant

Base module pinout (NP-HW-FPC-001 Rev D) is reproduced for comparison. Smart module differences are marked **[CHANGED]**.

| Pin | Base Module (Rev D) | Smart Module (NP-FPC-ZM-SM-01) | Notes |
|-----|---------------------|-------------------------------|-------|
| 1 | LED_A_ANODE | LED_A_ANODE | 660nm anode (CH_A) |
| 2 | LED_A_CATHODE | LED_A_CATHODE | 660nm cathode (CH_A) |
| 3 | LED_A_ANODE_2 | LED_A_ANODE_2 | 660nm anode string 2 |
| 4 | LED_A_CATHODE_2 | LED_A_CATHODE_2 | 660nm cathode string 2 |
| 5 | LED_B_ANODE | LED_C_ANODE | **[CHANGED]** 1064nm anode (CH_C) |
| 6 | LED_B_CATHODE | LED_C_CATHODE | **[CHANGED]** 1064nm cathode (CH_C) |
| 7 | LED_B_ANODE_2 | LED_C_ANODE_2 | **[CHANGED]** 1064nm anode string 2 |
| 8 | LED_B_CATHODE_2 | LED_C_CATHODE_2 | **[CHANGED]** 1064nm cathode string 2 |
| 9 | GND | GND_I2C | **[CHANGED]** I2C bus return; see §3.1 |
| 10 | LED_B_ANODE_3 | SDA | **[CHANGED]** I2C data (LPI2C3 hub) |
| 11 | LED_B_CATHODE_3 | SCL | **[CHANGED]** I2C clock (LPI2C3 hub) |
| 12 | LED_B_ANODE_4 | VCC_3V3 | **[CHANGED]** 3.3V supply to driver IC |
| 13 | LED_B_CATHODE_4 | LED_B_ANODE | **[CHANGED]** 808nm anode (CH_B) — note: remapped from base pins 5–8 |
| 14 | LED_B_ANODE_5 | LED_B_CATHODE | **[CHANGED]** 808nm cathode (CH_B) |
| 15 | LED_B_CATHODE_5 | LED_B_ANODE_2 | **[CHANGED]** 808nm anode string 2 |
| 16 | LED_B_ANODE_6 | LED_B_CATHODE_2 | **[CHANGED]** 808nm cathode string 2 |
| 17 | NTC_OUT | NTC_OUT | NTC thermistor (per-zone, hub ADC) — unchanged |
| 18 | ZONE_ID | ZONE_ID (3.3 kΩ) | **[CHANGED]** 3.3 kΩ to GND (vs 10–220 kΩ base ladder); see §3.2 |
| 19 | PD2_CATHODE | PD2_CATHODE | InGaAs PD2 (scalp-facing) cathode — unchanged position |
| 20 | GND | GND | Power and signal return — unchanged |

### 3.1 I2C Bus Assignment (Pins 9–12)

Pins 9–12 are repurposed from LED_B drive current lines to I2C bus + power:

- **Pin 9 (GND_I2C):** Common return for I2C pull-up resistors. Tied to pin 20 GND on module FPC.
- **Pin 10 (SDA):** I2C data. 4.7 kΩ pull-up to 3.3V on hub PCB per slot (see §6).
- **Pin 11 (SCL):** I2C clock. 4.7 kΩ pull-up to 3.3V on hub PCB per slot.
- **Pin 12 (VCC_3V3):** 3.3V supply to on-module driver IC. Current draw: ≤50 mA (ATtiny402 + gate drive). Hub PCB 3.3V rail must be verified for ×5 simultaneous smart module load.

The hub firmware enables I2C GPIO mux (LPI2C3) for a slot only after ADC detects ZONE_ID < 1100 counts. For base module slots (ZONE_ID ≥ 1100 counts), pins 9–12 remain in LED drive configuration and I2C mux is disabled.

### 3.2 ZONE_ID Resistor (Pin 18)

| Module | Resistor | ADC counts (12-bit, 3.3V ref, 10 kΩ pull-up) | Threshold |
|--------|----------|----------------------------------------------|-----------|
| Smart 1064nm | 3.3 kΩ, 1%, 0402 | ~1016 | < 1100 → smart |
| ZM-01 (base) | 10 kΩ | ~2048 | ≥ 1100 → base |
| ZM-02 (base) | 22 kΩ | ~2818 | |
| ZM-03 (base) | 47 kΩ | ~3378 | |
| ZM-04 (base) | 100 kΩ | ~3723 | |
| ZM-05 (base) | 220 kΩ | ~3918 | |
| No module | open | ~4095 | |

Margin from smart module nominal (1016 counts) to nearest base module threshold (1100 counts): **84 counts**, equivalent to ±0.8% ADC linearity margin. This is above the ±0.5% (±20 counts) required by RISK-18 debounce analysis.

**1% tolerance resistor is mandatory.** A 5% 3.3 kΩ could shift ADC reading by ±51 counts, approaching the threshold margin. Only 1% 0402 (e.g., Yageo RC0402FR-073K3L) is acceptable.

---

## 4. LED Array — Smart Module

### 4.1 LED Count and Wavelength Distribution

The smart module carries **three wavelength channels** on the same 66 × 78 mm FPC footprint as the base module.

| Channel | Wavelength | LED count | % of array | Driver IC channel |
|---------|-----------|-----------|-----------|------------------|
| CH_A | 660–670 nm | 200 | 36.4% | CH_A |
| CH_B | 808–830 nm | 200 | 36.4% | CH_B |
| CH_C | 1064 nm ± 5 nm | 150 | 27.3% | CH_C |
| **Total** | | **550** | | |

The reduction from 300 to 200 LEDs per 660/808nm channel (-33%) is required to accommodate 1064nm emitters. Session irradiance for CH_A and CH_B is proportionally reduced; this is offset by the addition of CH_C therapeutic benefit.

### 4.2 LED Pitch and Layout

- **Base module:** 300+300 = 600 LEDs, 6mm inter-LED pitch (interleaved 660/808nm pairs), ±15–25% irradiance variation.
- **Smart module:** 550 LEDs in a tri-wavelength interspersed pattern. Primary grid pitch: 6 mm. LED triplets (660nm + 808nm + 1064nm) arranged in repeating unit cells across the FPC strip.

Unit cell (approximate): 12 mm × 12 mm containing 2× 660nm + 2× 808nm + 1.5× 1064nm (rounded to integer per cell).

Irradiance variation: ±20–30% (slightly higher than base module due to lower 1064nm WPE and sparser distribution). This is within the therapeutic dose tolerance for PBM at 1064nm (dose accumulated over session compensates for irradiance variation).

### 4.3 1064nm LED Package Compatibility

1064nm LED emitters (EPITEX or equivalent per NP-PROC-FPC-1064-001 Rev A) use the same SMD package footprint as the 660/808nm emitters in the base module. The mould variant (NP-TOOL-ZM-SM-001 Rev A) confirms the same LED array window geometry is compatible.

Verify: **Vf binning** for 1064nm emitters must be ≤ ±0.1 V within the smart module population (same requirement as 660/808nm in NP-PROC-FPC-001 Rev A §4.2). See NP-PROC-FPC-1064-001 Rev A §3 for full binning specification.

---

## 5. Photodiode Footprints (PD1, PD2)

### 5.1 PD1 — Forward-Emission Photodiode

- **Position:** Same as NP-HW-FPC-001 Rev D. Geometric centre of 66 × 78 mm LED array (X = 33.0 mm, Y = 39.0 mm from module reference corner).
- **Pad geometry:** 1.6 mm annular ring, PCB-facing copper layer (emitter side), hard gold ≥ 0.5 µm cobalt-alloyed.
- **Component:** InGaAs PD per NP-PROC-FPC-1064-001 Rev A §4 (Hamamatsu G12180-010A or qualified equivalent).
- **Note:** InGaAs is broadband (600–1700 nm). PD1 measures aggregate forward emission across all three wavelengths. Per-wavelength dose separation is performed in firmware via calibrated K coefficients (NP-FW-PBM1064-001 Rev A §6.2).

### 5.2 PD2 — Scalp-Facing Reference Photodiode

- **Position:** X = 33.0 mm, Y = 39.0 mm (co-located in XY with PD1, opposite FPC face). **This position is identical to NP-HW-FPC-001 Rev D F-04.** The mould F-04 aperture (NP-TOOL-ZM-001 Rev A) is compatible without modification.
- **Pad geometry:** 1.6 mm annular ring, scalp-facing copper layer, hard gold ≥ 0.5 µm. Pin 19 (PD2_CATHODE) per pinout table above.
- **Component:** Same InGaAs PD as PD1 (Hamamatsu G12180-010A or qualified equivalent).

### 5.3 TIA Gain Compatibility

The hub PCB TIA front-end for PD1/PD2 ADC channels was sized for silicon photodiodes (responsivity ≈ 0.45–0.50 A/W at 808 nm). InGaAs responsivity at 1064 nm ≈ 0.85–0.95 A/W (Hamamatsu G12180-010A datasheet), approximately **2× higher**.

**Analysis:**

At representative dose-metering irradiance levels (PD1 behind PDMS: ~2–8 mW/cm²; PD2 backscattered: ~0.5–3 mW/cm²) with 1 mm² active area:

| Condition | Silicon (810 nm, 0.47 A/W) | InGaAs (1064 nm, 0.90 A/W) |
|-----------|---------------------------|---------------------------|
| PD1 at 8 mW/cm² | 37.6 µA | 72.0 µA |
| PD2 at 3 mW/cm² | 14.1 µA | 27.0 µA |

If TIA Rf = 47 kΩ (baseline):
- Silicon PD1 max: 37.6 µA × 47 kΩ = **1.77 V** (safe, 3.3V rail)
- InGaAs PD1 max: 72.0 µA × 47 kΩ = **3.38 V** — **exceeds 3.3V rail; saturates**

**Resolution (BLOCKING):** Hub PCB requires a **gain selection resistor per slot** (see §6, OI-PBM-HW-01). For smart module slots, Rf must be reduced to ≤ 22 kΩ:
- InGaAs PD1 at 8 mW/cm² with Rf = 22 kΩ: 72.0 µA × 22 kΩ = **1.58 V** — safe margin ✓
- Silicon PD1 at 8 mW/cm² with Rf = 22 kΩ: 37.6 µA × 22 kΩ = **0.83 V** — acceptable ✓ (firmware uses per-slot K coefficients)

The ZONE_ID threshold detection (ADC < 1100 counts) must trigger the TIA gain switch before the slot I2C is enabled. This requires a GPIO-controlled analog switch on the hub PCB per slot (e.g., Vishay DG2788 or similar single-supply SPDT analog switch, $0.15–0.25/switch × 5 slots = $0.75–1.25 BOM delta on hub PCB).

---

## 6. On-Module Driver IC Footprint

### 6.1 Rigidizer Sub-Board

The on-module I2C LED driver IC assembly mounts on a **PCB rigidizer** bonded to the FPC within the zone module shell cavity. This is a small FR4 PCB daughter board (dimensions: ≤22 × 14 mm, 0.8 mm thickness) soldered to FPC pads via ZIF-compatible footprint or direct pad bonding.

The rigidizer occupies the FPC area nearest the 20-pin Hirose connector, in the zone module cavity behind the Hirose end of the FPC strip (outside the LED array window). The mould variant (NP-TOOL-ZM-SM-001 Rev A §4) provides a dedicated 24 × 16 × 3 mm cavity for the rigidizer sub-board.

### 6.2 Selected Driver IC: ATtiny402 + 3× IRLML6344

**Selected combination: Microchip ATtiny402 (I2C slave) + 3× Infineon IRLML6344 N-channel MOSFETs**

Rationale: See NP-PROC-FPC-1064-001 Rev A §2 for full evaluation. Summary:

- No commercially available single LED driver IC meets all requirements simultaneously: 3 independent channels, 80–200 mA per channel, programmable 0.5–40 Hz pulsing, 8-bit duty cycle, I2C at 0x30, internal thermal fault, OCP, open-LED detection.
- The ATtiny402 programmed as an I2C slave implements the exact register map defined in NP-FW-PBM1064-001 Rev A §5.1. The three PWM outputs drive N-FET gates; the FETs switch LED current via series sense resistors.

**Component list for rigidizer sub-board:**

| Ref | Component | Value / Part | BOM |
|-----|-----------|-------------|-----|
| U1 | I2C slave MCU | Microchip ATtiny402, SOT-23-8 | $0.38 |
| Q1 | N-MOSFET CH_A (660nm) | Infineon IRLML6344TRPBF, SOT-23 | $0.12 |
| Q2 | N-MOSFET CH_B (808nm) | Infineon IRLML6344TRPBF, SOT-23 | $0.12 |
| Q3 | N-MOSFET CH_C (1064nm) | Infineon IRLML6344TRPBF, SOT-23 | $0.12 |
| R1 | Current sense, CH_A | 1.0 Ω, 0402, 1%, 0.25W | $0.02 |
| R2 | Current sense, CH_B | 1.0 Ω, 0402, 1%, 0.25W | $0.02 |
| R3 | Current sense, CH_C | 1.0 Ω, 0402, 1%, 0.25W | $0.02 |
| R4–R6 | Gate resistors | 10 Ω, 0402 | $0.01 |
| C1 | Decoupling, VCC | 100 nF, 0402, X5R | $0.01 |
| C2 | Bulk cap | 10 µF, 0402, X5R | $0.03 |
| — | PCB (FR4, 22×14mm, 0.8mm) | 2-layer | $0.15 |
| **Total** | | | **$1.00** |

IRLML6344 key specs: VGS_th = 0.4–1.0 V (fully enhanced at 3.3V drive from ATtiny402), ID = 5 A, RDS(on) = 27 mΩ, SOT-23. Thermal dissipation at 180 mA: P = I² × RDS(on) = 0.18² × 0.027 = **0.87 mW** — negligible.

**Total smart module driver BOM (rigidizer): $1.00 — within $2.00 target.**

### 6.3 ATtiny402 Firmware Interface

The ATtiny402 is programmed via UPDI before module assembly. It implements:
- I2C slave address: 0x30 (factory-burned, not field-configurable)
- Register map: NP-FW-PBM1064-001 Rev A §5.1, registers 0x00–0x0D
- PWM generation: Timer/Counter A (TCA) in split mode, 3 independent 8-bit PWM channels at configurable frequency (0.5–100 Hz via period register; firmware maps codes in §5.2)
- Thermal sensing: Internal temperature sensor (ATtiny402 ADC channel TEMPSENSE, factory-calibrated ±10°C). Thermal fault flag at 62°C threshold (register 0x0B, default 62°C)
- OCP detection: ADC measures voltage across each current-sense resistor; OCP threshold at 220 mA (registers OCP bits in STATUS 0x00)
- Open-LED detection: ADC measures LED string voltage; detects open circuit (voltage > VCC on anode pin when enabled)
- Startup time: < 2 ms from VCC stable to I2C ACK (ATtiny402 power-on reset: 100 µs; I2C slave init: < 1 ms total)

**ATtiny402 firmware part number: NP-FW-ZM-TINY402-001** (to be authored by firmware team; OI-PBM-08, blocking FAI-SM-02).

### 6.4 I2C Pull-ups on Hub PCB

Each smart module slot requires 4.7 kΩ pull-ups on SDA and SCL (pins 10 and 11) to 3.3V on the hub PCB. At 5 slots × 2 resistors = **10 × 4.7 kΩ, 0402** on hub PCB.

Hub PCB designer must verify space for 10 pull-up resistors. If space-constrained, a single 4.7 kΩ per line per bus segment (if all 5 slots share one I2C bus with switchable GPIO mux) reduces to 2 pull-ups per bus. **Preferred: per-slot I2C bus segment switching via GPIO mux (eliminates I2C address conflicts since all smart modules share 0x30).**

GPIO mux IC for I2C bus switching: **NXP PCA9546A** (4-channel I2C switch, I2C address 0x70–0x73, 3.3V, TSSOP-16) covers 4 of 5 slots; a second switch or direct connection handles the 5th. Alternative: **TI TCA9548A** (8-channel). BOM: ~$0.50/switch, 1–2 switches needed on hub PCB.

---

## 7. FPC Material and Construction — Unchanged

All FPC material, trace width/spacing, copper weight, RA copper specification, coverlay, PDMS bonding process (SiO₂ 75 nm interlayer + O₂ plasma, NP-FAI-ZM-001 §3e), and thermal cycling qualification requirements (200-cycle IEC 60068-2-14, BLOCKING) are inherited unchanged from NP-HW-FPC-001 Rev D.

The smart module FPC part number is **NP-FPC-ZM-SM-01**. It is manufactured on the same FPC production line as NP-FPC-ZM-01 (base module). The primary differences requiring separate artwork files:
1. Pin 5–8 rerouted from LED_B strings to LED_C (1064nm) strings
2. Pins 9–12 rerouted from LED_B drive lines to I2C bus + 3.3V supply
3. LED array pattern updated for 3-wavelength distribution (§4)
4. Rigidizer sub-board pads added at Hirose end (§6.1)

---

## 8. Open Items

| ID | Description | Blocking |
|----|-------------|---------|
| OI-PBM-HW-01 | Hub PCB TIA gain selection: add analog switch (Vishay DG2788 or equiv) per slot; GPIO controlled from ZONE_ID threshold detection | FAI-SM-06/07/08; **BLOCKING for hardware build** |
| OI-PBM-HW-02 | Hub PCB I2C bus switch (NXP PCA9546A or TI TCA9548A): 5-slot GPIO mux to prevent address conflict between smart modules | FAI-SM-02/04 |
| OI-PBM-HW-03 | Hub PCB 3.3V supply current budget: verify 5× smart modules simultaneous draw (5 × 50 mA = 250 mA) against 3.3V regulator spec | Pre-prototype |
| OI-PBM-HW-04 | FPC artwork NP-FPC-ZM-SM-01: release Gerbers with updated pin routing and 3-wavelength LED array; IPC-2223 Class 3 | FAI-SM-01 |
| OI-PBM-HW-05 | ATtiny402 firmware NP-FW-ZM-TINY402-001: implement register map, PWM, thermal fault, OCP, open-LED detection | FAI-SM-02/04 |

---

## 9. Design Review Checklist

| Item | Description | Status |
|------|-------------|--------|
| SM-DRC-01 | ZONE_ID 3.3 kΩ 1% tolerance confirmed | Open |
| SM-DRC-02 | ADC margin ≥ 50 counts confirmed at 3.3 kΩ ±1% | ✓ (84 counts, §3.2) |
| SM-DRC-03 | InGaAs PD footprint = 1.6 mm annular ring, hard gold ≥ 0.5 µm | ✓ (§5) |
| SM-DRC-04 | PD2 position X=33.0, Y=39.0 mm — compatible with NP-TOOL-ZM-001 Rev A F-04 aperture | ✓ (§5.2) |
| SM-DRC-05 | TIA gain saturation analysis completed | ✓ (§5.3) — hub PCB change required (OI-PBM-HW-01) |
| SM-DRC-06 | Driver IC BOM ≤ $2.00 | ✓ ($1.00, §6.2) |
| SM-DRC-07 | ATtiny402 startup time ≤ 5 ms | ✓ (< 2 ms, §6.3) |
| SM-DRC-08 | Rigidizer cavity dimensions confirmed with mould variant | Open — NP-TOOL-ZM-SM-001 Rev A §4 |
| SM-DRC-09 | I2C pull-up count on hub PCB confirmed | Open — hub PCB designer (OI-PBM-HW-02) |
| SM-DRC-10 | LED array FPC artwork released (NP-FPC-ZM-SM-01) | Open — OI-PBM-HW-04 |
| SM-DRC-11 | IRLML6344 thermal dissipation < 1 W per FET | ✓ (0.87 mW, §6.2) |
| SM-DRC-12 | 1064nm LED Vf binning spec added to NP-PROC-FPC-1064-001 Rev A | ✓ (§3 of that document) |
