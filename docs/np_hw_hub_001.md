# Hub PCB Rev B — Vishay DG2788A TIA Gain Switch per Smart Module Slot

**Project:** NeurOne  
**Document:** NP-HW-HUB-001  
**Revision:** B  
**Date:** 2026-05-13  
**Status:** BASELINED  
**Effective Date:** 2026-05-13  
**Author:** NeurOne Hardware Engineering  
**Approved By:** Steve Hickman, CEO  
**References:** NP-HW-FPC-001 Rev E §5.3; NP-FW-PBM1064-001 Rev A §4; CLAUDE.md §3 PBM Transcranial (OI-PBM-HW-01); Issue #54; Issue #62  
**Related Issues:** GitHub Issue #62 (OI-PBM-HW-01 CLOSED)  
**Gate:** —  
**IEC 62304 Class:** —  
**Supersedes:** NP-HW-HUB-001 Rev A (hub PCB without TIA gain switch)  
**Parent Document:** —

---

## 1. Scope

This document specifies the Rev B changes to the NeurOne hub PCB required to support the 1064nm smart zone module accessory (NP-FPC-ZM-SM-01). All changes are additive; the base hub PCB architecture is unchanged.

**Rev B adds:**
1. **Five Vishay DG2788A analog switches** — one per zone slot — switching the TIA feedback resistor Rf from 47 kΩ (base module / no module: silicon PD responsivity) to 22 kΩ (smart module: InGaAs PD, 2× higher responsivity). This is **OI-PBM-HW-01**, the blocking item for FAI-SM-04 and FAI-SM-06.
2. **NXP PCA9546A I2C bus switch** — 4-of-5 slot I2C isolation, preventing address collisions (all smart modules share 0x30). Fifth slot via direct LPI2C3 GPIO mux. This resolves OI-PBM-HW-02.
3. **Per-slot 4.7 kΩ I2C pull-up resistors** (10 resistors × 4.7 kΩ, 0402) on SDA and SCL lines for each smart-module-capable slot.
4. **3.3 V supply budget verification** for ≤5 simultaneous smart modules.

---

## 2. Background — Why TIA Gain Switch Is Required

The hub PCB TIA front-end for PD1/PD2 dose-metering ADC channels was sized for the base module silicon photodiodes at 808 nm (responsivity ≈ 0.47 A/W). The smart module carries Hamamatsu G12180-010A InGaAs photodiodes with responsivity ≈ 0.90 A/W at 1064 nm — approximately **2× higher**.

**TIA saturation analysis (from NP-HW-FPC-001 Rev E §5.3):**

| Condition | PD current | Rf = 47 kΩ (Rev A) | Rf = 22 kΩ (Rev B) |
|-----------|-----------|---------------------|---------------------|
| Si PD1 at 8 mW/cm², 1 mm² | 37.6 µA | 1.77 V ✓ | 0.83 V ✓ |
| InGaAs PD1 at 8 mW/cm², 1 mm² | 72.0 µA | **3.38 V — saturates** | 1.58 V ✓ |
| InGaAs PD2 at 3 mW/cm², 1 mm² | 27.0 µA | 1.27 V ✓ | 0.59 V ✓ |

With Rf = 47 kΩ, the InGaAs PD1 output at typical irradiance levels exceeds the 3.3 V ADC rail. The gain switch is mandatory; no firmware workaround exists.

**Resolution:** Per-slot SPDT analog switch selects Rf = 47 kΩ (base module default) or Rf = 22 kΩ (smart module). Switch is asserted by hub firmware immediately after ZONE_ID debounce confirms a smart module, and **before** the I2C mux is enabled for that slot.

---

## 3. TIA Gain Switch — Vishay DG2788A

### 3.1 Component Selection

**Selected IC: Vishay DG2788A** (Vishay/Siliconix)

| Parameter | Value |
|-----------|-------|
| Configuration | Dual SPDT (2× single-pole double-throw) |
| Supply voltage | 2.7–5.5 V (single supply; 3.3 V rail) |
| RON (typical at 3.3 V) | ≤ 2.5 Ω |
| COFF | ≤ 5 pF |
| Control logic | TTL/CMOS compatible (VIH ≥ 2.0 V, VIL ≤ 0.8 V) |
| Package | SOT-23-8 or SC-74A |
| BOM cost | $0.15–0.25 per unit |
| BOM × 5 slots | **$0.75–1.25 total** |

One DG2788A per slot covers **both** PD channels (PD1 TIA and PD2 TIA) using the two independent SPDT sections (section A = PD1 gain, section B = PD2 gain). Both sections are driven by the same control signal (`GAIN_SEL[n]` GPIO from i.MX RT1062).

**Rationale for DG2788A over alternatives:**
- Single-supply operation at 3.3 V without negative rail
- RON ≤ 2.5 Ω negligible vs Rf values (47 kΩ, 22 kΩ)
- COFF ≤ 5 pF: no significant noise injection into TIA feedback at DC–100 Hz dose-metering bandwidth
- Dual SPDT in a single SOT-23-8 package covers both PD channels per slot with one component
- Vishay Siliconix preferred supplier already qualified for NeurOne analogue front-end

### 3.2 TIA Feedback Switching Topology

Each zone slot has an independent TIA op-amp circuit for PD1 and PD2. The DG2788A switches the feedback resistor between two values.

**Topology (per TIA channel):**

```
                  +-------[Rf_A = 47 kΩ]---+
                  |       (DG2788A COM→NO)  |
PD_in ─ [Rbias] ─ ─ (−) ─ [TIA op-amp] ─── output ─→ LPADC1
                  |                         |
                  +-------[Rf_B = 22 kΩ]---+
                          (DG2788A COM→NC)
```

- **GAIN_SEL = LOW (default):** DG2788A selects COM→NO path (Rf_A = 47 kΩ). Used for base modules, absent slot, and during debounce before type is confirmed.
- **GAIN_SEL = HIGH (smart module):** DG2788A selects COM→NC path (Rf_B = 22 kΩ). Used after ZONE_ID debounce confirms ADC < 1100 counts.

The DG2788A COM pin is the op-amp feedback node. NO = 47 kΩ to op-amp output. NC = 22 kΩ to op-amp output.

**Resistor values:**

| State | Rf selected | Target | Achieved |
|-------|-------------|--------|---------|
| Base/absent (GAIN_SEL = LOW) | Rf_A | 47 kΩ | 47.0 kΩ (standard E24, 1%) |
| Smart module (GAIN_SEL = HIGH) | Rf_B | 22 kΩ | 22.1 kΩ (standard E24, 1%) |

Both resistors: 1%, 0402, low-temperature-coefficient (≤ 50 ppm/°C). Matched per slot to ≤ 0.5% between PD1 and PD2 TIA channels on the same slot to preserve PD1/PD2 ratio accuracy.

### 3.3 One DG2788A per Slot, Section Assignment

| DG2788A section | Signal | Switched Rf pair |
|-----------------|--------|-----------------|
| Section A | PD1 TIA (forward emission, behind PDMS) | 47 kΩ (LOW) ↔ 22 kΩ (HIGH) |
| Section B | PD2 TIA (scalp-facing backscatter) | 47 kΩ (LOW) ↔ 22 kΩ (HIGH) |

Both sections share the single `GAIN_SEL[n]` control line — PD1 and PD2 gain are always switched together, maintaining the PD1/PD2 ratio calibration validity.

### 3.4 GPIO Assignment — GAIN_SEL[0..4]

| GPIO name | i.MX RT1062 pin | Zone slot | Default state |
|-----------|----------------|-----------|---------------|
| GAIN_SEL_0 | GPIO_B0_04 | ZM-01 (slot 0) | LOW (47 kΩ) |
| GAIN_SEL_1 | GPIO_B0_05 | ZM-02 (slot 1) | LOW (47 kΩ) |
| GAIN_SEL_2 | GPIO_B0_06 | ZM-03 (slot 2) | LOW (47 kΩ) |
| GAIN_SEL_3 | GPIO_B0_07 | ZM-04 (slot 3) | LOW (47 kΩ) |
| GAIN_SEL_4 | GPIO_B0_08 | ZM-05 (slot 4) | LOW (47 kΩ) |

GPIO pins are i.MX RT1062 GPIO2 bank (GPIO_B0_xx). Configured as push-pull output, no pull resistor required (DG2788A input draws < 1 µA). Default LOW at power-on reset (3.3 V GPIO2 bank default state = LOW).

**Critical: GAIN_SEL must default LOW on power-on reset.** The i.MX RT1062 GPIO2 bank defaults to input mode (tri-state) on reset. Hub firmware boot sequence must configure GAIN_SEL[0..4] as outputs driven LOW before the zone detection task starts. See §5 for sequencing.

### 3.5 TIA Op-Amp Selection Notes

The existing hub TIA op-amp must support stable operation across the full Rf range (22 kΩ to 47 kΩ) at the LPADC1 input bandwidth (≤ 100 Hz dose metering). The DG2788A RON ≤ 2.5 Ω adds negligible noise and offset at these values. No change to op-amp selection is required for Rev B; verify GBW and input bias current remain acceptable with Rf = 22 kΩ (verify output swing margin at maximum InGaAs PD current: 72 µA × 22 kΩ = 1.58 V < 3.0 V output swing limit for 3.3 V single-supply).

---

## 4. I2C Bus Isolation — NXP PCA9546A (OI-PBM-HW-02)

### 4.1 Problem Statement

All smart zone modules implement I2C address 0x30 (factory-programmed into ATtiny402, not field-configurable). Up to five smart modules can be inserted simultaneously. A single shared I2C bus would result in address collisions: all five ATtiny402 ICs would respond simultaneously, corrupting bus arbitration.

### 4.2 Solution — Per-Slot I2C Bus Isolation via PCA9546A

**Selected IC: NXP PCA9546A** (4-channel I2C switch)

| Parameter | Value |
|-----------|-------|
| Channels | 4 (expandable; one PCA9546A covers slots 0–3) |
| Control | I2C register at address 0x70 (or 0x71–0x73 via A0/A1 pins) |
| Supply | 2.3–5.5 V; 3.3 V hub rail |
| Package | TSSOP-16 |
| BOM cost | ~$0.50 per IC |

**Slot assignment:**

| Slot | I2C isolation method | Notes |
|------|---------------------|-------|
| Slot 0 (ZM-01) | PCA9546A channel 0 | Switched by hub firmware via PCA9546A I2C at 0x70 |
| Slot 1 (ZM-02) | PCA9546A channel 1 | |
| Slot 2 (ZM-03) | PCA9546A channel 2 | |
| Slot 3 (ZM-04) | PCA9546A channel 3 | |
| Slot 4 (ZM-05) | Direct LPI2C3 GPIO mux (LPSPI_MUX GPIO) | i.MX RT1062 GPIO enables/disables pull-up pair for SDA/SCL independently |

Slot 4 uses direct GPIO control of the 4.7 kΩ pull-up enable (MOSFET switch on pull-up rail) to isolate it from the shared bus when not in use. This avoids requiring a second PCA9546A for a single remaining channel.

**Hub firmware accesses PCA9546A at 0x70 on the host-side LPI2C1 bus** (separate from the smart module slave buses). To address module at slot n: firmware writes channel-enable byte to PCA9546A (bits [3:0] = channel mask), then performs I2C transaction to 0x30 on the same bus behind the switch.

### 4.3 Pull-Up Resistors

Each smart-module SDA and SCL line (pins 10 and 11 of the 20-pin Hirose connector) requires 4.7 kΩ pull-ups to 3.3 V on the hub PCB.

| Placement | Count | Value | Total |
|-----------|-------|-------|-------|
| PCA9546A side (slots 0–3) | 2 per channel × 4 = 8 | 4.7 kΩ, 0402 | 8 resistors |
| GPIO mux side (slot 4) | 2 | 4.7 kΩ, 0402 | 2 resistors |
| Host side (hub LPI2C1 to PCA9546A) | 2 | 4.7 kΩ, 0402 | 2 resistors |
| **Total** | | | **12 × 4.7 kΩ, 0402** |

Pull-ups on the slave side (slots 0–4) must be located between the PCA9546A channel output (or GPIO mux output for slot 4) and the Hirose ZIF connector pin. This ensures only one channel's pull-ups are active at a time, controlled by the PCA9546A channel enable.

---

## 5. ZONE_ID to Gain Switch Sequencing

The sequence between ZONE_ID ADC detection and TIA gain switch assertion is **safety-critical**: if the TIA is still in high-gain mode (Rf = 47 kΩ) when the smart module InGaAs PD begins receiving light during I2C initialisation, the ADC output will be invalid (saturated). The firmware must follow the exact ordering below.

### 5.1 Required Ordering (Smart Module Detection)

```
1. ADC reads ZONE_ID (pin 18) for the slot.
2. ZONE_ID debounce: 3× reads at 100 ms intervals, ≥ 2/3 must read < 1100 counts.
3. After debounce confirms smart module (ADC majority < 1100):
   a. Assert GAIN_SEL[n] = HIGH  →  DG2788A selects Rf = 22 kΩ.
      [minimum 10 µs setup; DG2788A switch propagation delay < 1 µs]
   b. Enable I2C mux for the slot (PCA9546A channel enable, or GPIO mux for slot 4).
   c. Probe I2C address 0x30 within 5 ms timeout.
4. On I2C probe success: proceed to session enable sequence (np_pbm1064_session_start).
```

**Rationale for ordering gain switch before I2C mux:** The ATtiny402 on the module powers up when VCC_3V3 (pin 12) is available. VCC_3V3 on the hub PCB is always live when the hub is powered; the InGaAs PD is therefore powered as soon as the module is physically connected. Although PBM LEDs are not enabled until after I2C session start, ambient light can produce non-trivial PD current. The TIA gain must be at Rf = 22 kΩ before any valid PD ADC readings are taken.

### 5.2 Required Ordering (Smart Module Removal)

```
1. ZONE_ID removal debounce: counts ≥ 4000 for 3 consecutive reads.
2. Disable I2C mux for the slot (PCA9546A channel disable, or GPIO mux deassert).
3. Deassert GAIN_SEL[n] = LOW  →  DG2788A returns to Rf = 47 kΩ (default).
4. Reset slot state to NP_SM_IDLE.
```

On removal, disable I2C first to prevent stale transactions if the module is being swapped for a base module. Then reset gain to HIGH to restore default state for a subsequent base module insertion.

### 5.3 Power-On Boot Sequence Requirement

The i.MX RT1062 GPIO2 bank defaults to input (tri-state) on reset. The boot sequence must configure all GAIN_SEL[0..4] pins as GPIO outputs driven LOW **before** starting the zone detection task and before the first LPADC1 readings are taken. This ensures the hub PCB always starts in high-gain (47 kΩ) mode, which is safe for base modules and empty slots.

**Required boot sequence addition (main processor firmware):**
```c
/* Configure GAIN_SEL[0..4] as output LOW before zone detect task starts */
np_pbm1064_hal_tia_gain_boot_init();   /* drives GPIO_B0_04..08 LOW */
```

This function must execute before `np_pbm1064_detect_init()` is called. See `np_pbm1064_hal.h` (OI-PBM-HW-01).

---

## 6. 3.3 V Supply Current Budget (OI-PBM-HW-03)

### 6.1 Smart Module VCC_3V3 Load

Each smart module draws ≤ 50 mA on VCC_3V3 (pin 12): ATtiny402 quiescent + IRLML6344 gate drive. This is specified in NP-HW-FPC-001 Rev E §3.1.

**Maximum simultaneous smart module draw:**

| Scenario | 3.3 V load | Note |
|----------|-----------|------|
| 0 smart modules | 0 mA | |
| 1 smart module | 50 mA | |
| 5 smart modules (all slots) | **250 mA** | worst case |

### 6.2 Existing 3.3 V Rail Capacity

Hub PCB 3.3 V rail is generated by an LDO or switching regulator supplying hub processor peripherals (i.MX RT1062 I/O, sensors, BT/Wi-Fi module 3.3 V). **Hub PCB designer must verify** that the 3.3 V regulator has ≥ 500 mA headroom above existing hub peripherals to accommodate 5× smart modules.

**Recommendation:** If existing regulator is ≤ 800 mA total budget, add a dedicated 500 mA LDO (e.g., TI TPS7A20 or equivalent, SOT-23-5, ≤ 200 mV dropout) on the smart module VCC_3V3 rail, powered from the hub 5 V supply. This isolates smart module transients from the hub processor 3.3 V rail. BOM delta: +$0.30–0.50 per hub PCB.

### 6.3 DG2788A and PCA9546A Supply Current

| Component | Quiescent current | Active current (per unit) |
|-----------|-----------------|--------------------------|
| DG2788A × 5 | < 1 µA each | Negligible |
| PCA9546A × 1 | 10 µA typical | Negligible |

No significant contribution to 3.3 V budget from gain switch ICs.

---

## 7. BOM Delta — Hub PCB Rev B

| Component | Qty | Unit cost | Total |
|-----------|-----|-----------|-------|
| Vishay DG2788A (SOT-23-8), dual SPDT gain switch | 5 | $0.15–0.25 | **$0.75–1.25** |
| NXP PCA9546A I2C switch (TSSOP-16) | 1 | $0.50 | **$0.50** |
| 4.7 kΩ, 0402, 1% pull-up resistors | 12 | $0.005 | **$0.06** |
| 47 kΩ, 0402, 1% (Rf_A, per TIA × 10) | 10 | $0.005 | **$0.05** |
| 22.1 kΩ, 0402, 1% (Rf_B, per TIA × 10) | 10 | $0.005 | **$0.05** |
| Optional: 500 mA LDO for smart module VCC_3V3 (if needed) | 1 | $0.30–0.50 | $0.30–0.50 |
| **Hub PCB Rev B total BOM delta** | | | **$1.41–1.91 (+ optional LDO)** |

This is within the $0.75–1.25 BOM estimate stated in CLAUDE.md for the DG2788A alone; total hub delta including I2C mux and pull-ups is $1.41–1.91.

---

## 8. PCB Layout Notes

1. **DG2788A placement:** Mount adjacent to the TIA op-amp for each slot. Keep switched feedback traces (Rf_A and Rf_B paths) as short as possible (< 5 mm) to minimise parasitic capacitance on the high-impedance feedback node.
2. **Rf_A / Rf_B resistors:** Place both resistors on the same PCB face as the DG2788A. Use 0402 with consistent copper pour treatment to minimise stray capacitance difference between the two paths.
3. **GAIN_SEL GPIO traces:** Route away from Rf traces and TIA input traces. 50 Ω controlled impedance is not required (low-frequency digital control, < 1 MHz transitions); however, keep traces < 20 mm to prevent coupling.
4. **PCA9546A placement:** Central hub PCB location; I2C bus segment from PCA9546A to each Hirose ZIF connector. Minimise bus segment length per channel to control capacitance (< 100 pF per segment recommended for 400 kHz fast-mode).
5. **Ground plane:** Ensure continuous ground plane beneath TIA and DG2788A circuits. No splits near feedback resistors.

---

## 9. Firmware Implications (OI-PBM-HW-01 Resolution)

The firmware changes required by this hardware revision are specified in NP-FW-PBM1064-001 Rev A (amended by Issue #62):

1. **New HAL function:** `np_pbm1064_hal_tia_gain_set(slot, gain)` — asserts or deasserts `GAIN_SEL[n]` GPIO. Stub provided; platform team implements with actual GPIO_B0 register writes.
2. **New HAL function:** `np_pbm1064_hal_tia_gain_boot_init()` — configures all 5 GAIN_SEL pins as output LOW at boot. Called before zone detection task.
3. **Detection sequence change:** `np_pbm1064_detect.c` calls `np_pbm1064_hal_tia_gain_set(slot, NP_TIA_GAIN_LOW)` after smart module debounce confirms ZONE_ID < 1100, and **before** `np_pbm1064_hal_i2c_mux_enable(slot, true)`.
4. **Removal sequence change:** `np_pbm1064_detect.c` calls `np_pbm1064_hal_tia_gain_set(slot, NP_TIA_GAIN_HIGH)` after I2C mux disable on smart module removal.
5. **Per-slot gain state:** `np_sm_slot_ctx_t` tracks current TIA gain setting for SHDR logging and diagnostic purposes.

See `firmware/pbm_1064nm/` for implementation. FAI-SM-04 (three-channel bench verification) and FAI-SM-06 (InGaAs dose metering accuracy) require hardware Rev B PCB with DG2788A populated to pass.

---

## 10. Open Items

| ID | Description | Blocking |
|----|-------------|---------|
| OI-HUB-01 | Hub PCB designer to verify 3.3 V regulator budget ≥ 500 mA headroom above existing peripherals (§6.2). Add dedicated LDO if insufficient. | Pre-prototype |
| OI-HUB-02 | Hub PCB layout DRC: Rf feedback trace length < 5 mm from DG2788A to TIA op-amp (§8). Sign-off required before Gerber release. | Pre-prototype |
| OI-HUB-03 | Hub PCB Gerber release for Rev B (includes DG2788A footprint × 5, PCA9546A footprint, pull-up resistors, Rf pairs). | FAI-SM-04/06 bench build |
| OI-HUB-04 | GPIO_B0_04..08 IOMUX configuration verified in i.MX RT1062 board support package (BSP) before bring-up. | Pre-prototype |
| OI-HUB-05 | Optional 500 mA LDO: decision pending 3.3 V regulator current audit (OI-HUB-01). | Pre-prototype |

**OI-PBM-HW-01 is now CLOSED (SPECIFIED)** — hardware design specified in this document. OI-PBM-HW-02 is now CLOSED (SPECIFIED) by §4 of this document. Remaining open items (OI-HUB-01 through OI-HUB-05) require PCB layout and bring-up team action.

---

## 11. Design Review Checklist

| Item | Description | Status |
|------|-------------|--------|
| HUB-DRC-01 | DG2788A GAIN_SEL default LOW confirmed (GPIO power-on state analysis) | Open — requires i.MX RT1062 BSP review (OI-HUB-04) |
| HUB-DRC-02 | TIA saturation analysis complete for Rf = 22 kΩ + InGaAs max current | ✓ (§2) — 1.58 V < 3.0 V op-amp swing limit |
| HUB-DRC-03 | Rf_A = 47 kΩ and Rf_B = 22.1 kΩ, 1%, ≤ 50 ppm/°C, matched ≤ 0.5% per slot | Open — component selection to BOM |
| HUB-DRC-04 | DG2788A RON ≤ 2.5 Ω impact on TIA offset quantified | ✓ (§3.5) — 2.5 Ω << Rf; negligible |
| HUB-DRC-05 | PCA9546A I2C address non-conflicting with other hub I2C peripherals | Open — hub I2C address map audit required |
| HUB-DRC-06 | 3.3 V supply budget (5× smart modules = 250 mA) verified | Open — OI-HUB-01 |
| HUB-DRC-07 | Gain switch assert before I2C mux enable sequencing confirmed in firmware | ✓ (§5.1; firmware/pbm_1064nm/src/np_pbm1064_detect.c Rev B) |
| HUB-DRC-08 | Boot init function configures GAIN_SEL[0..4] LOW before zone detect task | ✓ (§5.3; np_pbm1064_hal_tia_gain_boot_init) |
| HUB-DRC-09 | Feedback trace length ≤ 5 mm from DG2788A to TIA op-amp | Open — layout DRC (OI-HUB-02) |
| HUB-DRC-10 | PCA9546A channel enable/disable firmware tested with 5-module simultaneous scenario | Open — FAI-SM-04 bench |
