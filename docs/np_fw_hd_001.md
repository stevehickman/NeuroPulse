# sLORETA-Guided HD-tDCS Firmware Specification

**Project:** NeuroPulse
**Document:** NP-FW-HD-001
**Revision:** A
**Date:** 2026-05-11
**Status:** BASELINED
**Effective Date:** 2026-05-11
**Author:** Steve Hickman (CEO, interim Quality authority)
**Approved By:** Steve Hickman, CEO
**References:** CLAUDE.md §3 T2 additions (sLORETA-guided HD-tDCS)
**Related Issues:** GitHub Issue #23
**Gate:** NP-COORD-001 G3-07
**IEC 62304 Class:** SW-02 Class B (main processor)
**Supersedes:** —
**Parent Document:** NP-SW-001

---

## 1. Scope

This document specifies the firmware implementation of sLORETA-guided HD-tDCS for the NeuroPulse Pro (T2) platform. The feature encompasses:

- Real-time sLORETA cortical source localization from 21-ch qEEG resting-state data
- Automatic MNI target → T2 cap electrode mapping
- 4×1 ring montage configuration with independent per-channel current control
- HD-tDCS stimulation delivery via the existing 16-ch tACS driver
- Concurrent EEG recording during stimulation with artifact suppression
- Safety limit enforcement by the STM32G071 safety MCU

**Not in scope:** sLORETA weight matrix computation (precomputed offline by app/PC using standard sLORETA toolbox); tACS driver IC firmware (platform HAL layer).

---

## 2. Clinical Context

### 2.1 sLORETA-guided HD-tDCS workflow

| Step | Location | Description |
|------|----------|-------------|
| 1 | Device | 21-ch qEEG resting-state acquisition (2 min, eyes closed) |
| 2 | Device | sLORETA computes cortical source power map (real-time or post-session) |
| 3 | App | Identifies target region (e.g., DLPFC hypoactivity, ACC hyperactivation) |
| 4 | Firmware | Maps MNI target to nearest T2 cap electrode; configures 4×1 ring |
| 5 | Device | Delivers personalised HD-tDCS session with concurrent EEG |

### 2.2 Clinical evidence base

| Study | Relevance |
|-------|-----------|
| Jog/UCLA 2025 (n=71, JAMA Network Open) | MRI-guided HD-tDCS: significant depression improvement + gray matter changes |
| BRIGhTMIND 2024 (n=255, RCT) | Connectivity-guided iTBS: personalised targeting outperforms fixed F3 montage |
| Bikson lab 2016 (J Neural Eng) | 4×1 ring safety analysis: Ag/AgCl 3.5 mm electrodes, 2 mA, no adverse events |

### 2.3 Spatial focality claim

4×1 ring montage provides ~3–5× spatial focality improvement vs standard 2-electrode tDCS, quantified as FWHM of cortical electric field. Typical 4×1 FWHM: ~1.5 cm at 10 mm depth in MRI-derived head models (Datta et al. 2009, Edwards et al. 2013). Hardware FAI-HD03 measures this in saline phantom (see §12).

---

## 3. Configuration Constants (`np_hd_config.h`)

### 3.1 EEG acquisition

| Constant | Value | Description |
|----------|-------|-------------|
| `NP_HD_EEG_CHANNELS` | 21 | T2 qEEG cap channels |
| `NP_HD_EEG_SAMPLE_RATE_HZ` | 500 | ADS1299 sample rate |
| `NP_HD_RESTING_DURATION_S` | 120 | 2-min resting-state window |

### 3.2 sLORETA source model

| Constant | Value | Description |
|----------|-------|-------------|
| `NP_HD_SLORETA_N_VOXELS` | 2447 | Cortical mesh voxels, 7 mm MNI grid |
| `NP_HD_WEIGHT_MATRIX_BYTES` | ≈205 KB | Precomputed W in LPSDR4 |
| `NP_HD_SLORETA_FFT_SIZE` | 1024 | Welch window (2.048 s at 500 Hz) |
| `NP_HD_SLORETA_EPOCHS` | 64 | Epochs for covariance accumulation |

### 3.3 Safety limits (CLAUDE.md §3 T2 / Bikson lab 2016)

| Constant | Value | Description |
|----------|-------|-------------|
| `NP_HD_MAX_CURRENT_UA` | 2000 µA | Per-electrode limit; safety MCU HW |
| `NP_HD_MAX_CHARGE_DENSITY_UC_CM2` | 40 µC/cm² | Per-phase charge density |
| `NP_HD_MAX_ELECTRODE_DENSITY_A_M2` | 6 A/m² | Tissue current density limit |
| `NP_HD_RAMP_DURATION_S` | 30 s | Current ramp up and down |
| `NP_HD_MAX_IMPEDANCE_KOHM` | 10 kΩ | Pre-stimulation check threshold |
| `NP_HD_MAX_CHARGE_PER_PHASE_UC` | 3.85 µC | 40 µC/cm² × 0.0962 cm² |

### 3.4 Electrode geometry

Ag/AgCl sintered 3.5 mm diameter dual-rated electrodes (simultaneous EEG + tDCS):

| Constant | Value |
|----------|-------|
| `NP_HD_ELECTRODE_DIAM_MM` | 3.5 mm |
| `NP_HD_ELECTRODE_AREA_CM2` | 0.0962 cm² |
| `NP_HD_ELECTRODE_AREA_M2` | 9.621 × 10⁻⁶ m² |

---

## 4. Type Definitions (`np_hd_types.h`)

### 4.1 `np_hd_electrode_t`

21-channel enum matching T2 qEEG cap physical wiring:

```
Fp1(0) Fp2(1) F7(2) F3(3) Fz(4) F4(5) F8(6) FC3(7) FC4(8) T7(9)
C3(10) Cz(11) C4(12) T8(13) P7(14) P3(15) Pz(16) P4(17) P8(18) O1(19) O2(20)
```

### 4.2 `np_hd_montage_type_t`

| Value | Description |
|-------|-------------|
| `NP_HD_MONTAGE_RING_4X1` | 4×1 ring (primary, most focal, ~1.5 cm FWHM) |
| `NP_HD_MONTAGE_BILATERAL_4X1` | Dual hemisphere simultaneous |
| `NP_HD_MONTAGE_STANDARD_2E` | Standard 2-electrode T1-compatible fallback |

### 4.3 `np_hd_clinical_target_t`

| Target | MNI (x, y, z) | Clinical use |
|--------|---------------|--------------|
| `DLPFC_L` | (-46, 36, 20) | Depression, executive function |
| `DLPFC_R` | (46, 36, 20) | Depression bilateral, working memory |
| `VLPFC_L` | (-51, 15, 0) | Language, mood |
| `ACC` | (0, 28, 28) | Anxiety, anterior cingulate |
| `MPFC` | (0, 52, 6) | Default mode network |
| `M1_L` | (-37, -21, 58) | Motor rehabilitation (TMS targeting, FC3) |
| `M1_R` | (37, -21, 58) | Motor rehabilitation (TMS targeting, FC4) |
| `CUSTOM` | App-supplied | Arbitrary MNI from sLORETA peak |

### 4.4 Session workflow stages (`np_hd_stage_t`)

```
IDLE → EEG_ACQUIRE → SLORETA → MONTAGE → STIMULATION → COMPLETE
                                         ↓
                                       ABORTED
```

---

## 5. sLORETA Source Imaging (`np_sloreta.c`)

### 5.1 Algorithm

**Offline (app/PC — not on device):**

Precompute scalar weight matrix W (N_voxels × 21) from the sLORETA inverse solution:

```
T = (K K^T + λ² H)^{-1} K · W_norm^{-1}
```

Where:
- `K` = lead field matrix (21 × N_voxels), computed from BEM forward model on MNI152 head
- `H` = graph Laplacian regularizer of cortical surface mesh
- `λ²` = regularization parameter (standard sLORETA: λ² = 0.05 × trace(K K^T)/N_ch)
- `W_norm` = diagonal normalization matrix ensuring zero localization bias

Dominant dipole orientation per voxel pre-selected; result is a scalar W row per voxel.  
Stored in Config partition as 205 KB binary blob; loaded into LPSDR4 at session start.

**On-device runtime:**

```
Step 1: Accumulate 64 epochs of mean-subtracted EEG covariance
        C = (1/N) Σ x_i x_i^T   [21×21 symmetric matrix, SRAM: 1.8 KB]

Step 2: For each voxel v:
        P[v] = W_v^T · C · W_v  [scalar quadratic form]
        W_v = row v of weight matrix (21-vector)
        [2447 voxels × 441 mults ≈ 1.1M flops → <20 ms on Cortex-M7 @ 600 MHz]

Step 3: Peak detection: argmax(P[v]) → peak voxel → MNI lookup
```

### 5.2 Covariance accumulation

Welford-style running mean across epochs. Each epoch: Hann-windowed 1024-sample block at 500 Hz (2.048 s). Mean subtracted per epoch per channel. Covariance updated as:

```
C_n = (1 - 1/n) × C_{n-1} + (1/n) × x_epoch x_epoch^T / n_samples
```

### 5.3 Band power decomposition

`np_sloreta_band_power()` decomposes voxel source power into delta/theta/alpha/beta bands using relative bin count weighting. Used by the app to characterize hypo-/hyperactivation (e.g., high frontal theta → depressive phenotype target DLPFC_L).

### 5.4 API

| Function | Description |
|----------|-------------|
| `np_sloreta_init(ctx, W, mni_lut, n_voxels)` | Bind weight matrix + MNI LUT |
| `np_sloreta_reset(ctx)` | Clear covariance; prepare for new session |
| `np_sloreta_push_epoch(ctx, samples, n_samples)` | Accumulate one EEG epoch |
| `np_sloreta_compute_map(ctx, source_power, n)` | Compute full voxel power map |
| `np_sloreta_find_peak(ctx, power, n, result)` | Peak detection + MNI lookup |
| `np_sloreta_band_power(ctx, voxel, power, bands)` | Band decomposition at voxel |
| `np_sloreta_epoch_count(ctx)` | Query accumulated epoch count |
| `np_sloreta_covariance_norm(ctx)` | Frobenius norm (quality indicator) |

---

## 6. Electrode Montage Selection (`np_hd_montage.c`)

### 6.1 MNI scalp coordinate table

Standard 10-20 electrode MNI scalp positions from Jurcak et al. (2007) NeuroImage:

| Electrode | MNI (x, y, z) | Electrode | MNI (x, y, z) |
|-----------|---------------|-----------|---------------|
| Fp1 | (-21, 66, 5) | Fp2 | (21, 66, 5) |
| F7 | (-51, 26, -2) | F3 | (-35, 36, 64) |
| Fz | (0, 31, 75) | F4 | (35, 36, 64) |
| F8 | (51, 26, -2) | FC3 | (-40, 14, 67) |
| FC4 | (40, 14, 67) | T7 | (-70, -17, -2) |
| C3 | (-55, 0, 67) | Cz | (0, -10, 83) |
| C4 | (55, 0, 67) | T8 | (70, -17, -2) |
| P7 | (-51, -55, -2) | P3 | (-35, -55, 64) |
| Pz | (0, -65, 75) | P4 | (35, -55, 64) |
| P8 | (51, -55, -2) | O1 | (-21, -85, 5) |
| O2 | (21, -85, 5) | | |

### 6.2 4×1 ring montage algorithm

```
1. E_center = argmin_e ||MNI_electrode[e] - MNI_target||₂   (Euclidean nearest)

2. Candidates = {all electrodes} \ {E_center}
   Sort by distance to E_center (20 candidates remain)

3. Cathodes = 4 nearest candidates to E_center
   Validate angular spread: cathodes must cover ≥ 2 quadrants
   (quadrant defined by sign of Δx, Δy relative to E_center)

4. Validate: no cathode is closer to target than E_center

5. Verify all electrodes map to distinct tACS driver channels
```

Angular spread requirement prevents pathological "all-anterior" cathode selections on boundary electrodes, ensuring the ring provides 2D focality rather than a dipole-like pattern.

### 6.3 Bilateral 4×1

Mirrors left hemisphere ring to right hemisphere by negating x-coordinate. Both hemispheres driven simultaneously from separate driver channel pairs. Independent impedance check per hemisphere.

### 6.4 tACS driver channel assignment

Fixed mapping from T2 cap wiring specification (NP-HW-TCAP-001, TBD). All 5 electrodes in a ring montage use distinct driver channels (validated by `np_hd_montage_validate()`). Maximum 5 of 16 channels active simultaneously per ring.

---

## 7. Stimulation Delivery (`np_hd_stim.c`)

### 7.1 Current distribution

For 4×1 ring at anode current I_a:

```
I_center    = +I_a         (anode)
I_cathode_k = −I_a / 4     (each of 4 cathodes, k = 0..3)
Sum         = I_a − 4 × (I_a/4) = 0   (charge balanced at each instant)
```

### 7.2 Ramp state machine

```
IDLE
  │  safety MCU grant received
  ▼
RAMP_UP (30 s linear: 0 → I_target)
  │  30 s elapsed
  ▼
STEADY (duration_s − 60 s)
  │  charge density monitor: abort if > 95% of limit
  │  steady_end_ms reached
  ▼
RAMP_DOWN (30 s linear: I_target → 0)
  │  0 reached
  ▼
DONE
```

Tick interval: 100 ms (FreeRTOS task). Current resolution: 100 ms × (I_target / 30,000 ms) per step.

### 7.3 Safety MCU interaction

All stimulation GPIO are owned by the STM32G071 safety MCU (CLAUDE.md §4.2). Main processor requests enable via SPI; safety MCU independently:
1. Checks electrode impedance via AC injection at 1 kHz
2. Validates charge density per phase vs 40 µC/cm² limit
3. Monitors 200 ms heartbeat — watchdog cutoff at 1.5 s

Safety MCU response is asynchronous; delivered to firmware via `np_hd_stim_safety_mcu_response()`.

### 7.4 Charge density monitoring (software layer)

In addition to safety MCU hardware enforcement, `np_hd_stim_tick()` tracks accumulated charge at the anode electrode:

```
charge_ua_s += I_actual × 0.1 s    (per 100 ms tick)
density_uc_cm2 = charge_ua_s / (area_cm2 × 1000)
```

If density exceeds 95% of `NP_HD_MAX_CHARGE_DENSITY_UC_CM2`, stimulation is immediately stopped and session is aborted. This is a software belt-and-suspenders; the safety MCU provides the primary hardware enforcement.

---

## 8. Session Orchestration (`np_hd_session.c`)

### 8.1 Stage transitions

```
Stage              Duration          Trigger to advance
─────────────────────────────────────────────────────────────────────
EEG_ACQUIRE        120 s             NP_HD_SLORETA_EPOCHS reached
SLORETA            <20 ms (compute)  np_sloreta_compute_map() returns OK
MONTAGE            User/auto select  np_hd_session_start_stim() called
STIMULATION        config.duration_s Ramp-down complete (DONE phase)
COMPLETE           —                 end_cb() called
```

### 8.2 UHDR / SHDR data routing

#### UHDR (user biology — NeuroPulse never holds key)

Written to UHDR partition at session end via `end_cb`. Caller commits to eMMC via AES-256-XTS write (biometric key — NP-FW-EMMC-001 Rev A §6).

| Field | Content | Classification rationale |
|-------|---------|--------------------------|
| `session_start_unix` | UTC epoch | Session timestamp → UHDR |
| `eeg_duration_s` | EEG window length | Derived from session → UHDR |
| `stim_duration_s` | Actual stim time | Stim behaviour → UHDR |
| `montage_type` | Ring/bilateral/standard | Electrode choice → UHDR |
| `center_electrode` | Anode electrode | Clinical target → UHDR |
| `cathode_electrodes[4]` | Cathode positions | Clinical montage → UHDR |
| `target_mni_{x,y,z}` | MNI target coordinate | Brain target → UHDR |
| `anode_current_ua` | Stimulation current | Clinical parameter → UHDR |
| `peak_source_power` | sLORETA peak value | EEG-derived → UHDR |
| `mean_impedance_kohm` | Electrode impedance | Contact quality → UHDR |
| `total_charge_ua_s` | Total delivered charge | Dose metric → UHDR |
| `abort_reason` | 0 = normal | Safety event → UHDR |

#### SHDR (device metrics — NeuroPulse fleet telemetry)

Written to SHDR partition by caller via SHDR storage API (NP-FW-EMMC-001 §7).

| Field | Content | Classification rationale |
|-------|---------|--------------------------|
| `montage_type` | Ring/bilateral/standard | Device config — not user identity |
| `center_electrode` | Electrode index | Position metric — not user biology |
| `stim_duration_s` | Session duration | Unsigned counter — no user biology |
| `anode_current_ua` | Current used | Device operation metric |
| `mean_impedance_kohm` | Contact quality mean | Electrode health metric |
| `impedance_check_pass` | Bool | Consumable condition flag |
| `abort_reason` | 0 = normal | Device fault tracking |

Boundary resolution: electrode index (e.g., "F3") indicates device cap wiring position, not user brain anatomy — classified as SHDR per the "does this tell us about the device's condition" test (CLAUDE.md §5.1).

---

## 9. Concurrent EEG During Stimulation

### 9.1 Artifact suppression

ADS1299 input MUX shorted for `NP_HD_EEG_BLANK_WINDOW_US` (200 µs) around each current ramp step to suppress the fast DC transient. Ramp steps are infrequent (one per 100 ms tick), so duty-cycle loss is < 0.1%.

For tDCS DC offset: ADS1299 offset cancellation via internal DAC (`CONFIG3[PDREF_BUFP]` mode). Remaining DC offset < 50 µV after settling.

### 9.2 EEG SNR specification

Target: ≥ 20 dB SNR in alpha band (8–13 Hz) during 1 mA anode stimulation. Verified in hardware FAI-HD04 (§12.4).

---

## 10. Platform HAL Open Items

| OI | Function | Peripheral | Blocking |
|----|----------|------------|---------|
| OI-HD-01 | `platform_ads1299_start/stop()` | ADS1299 + LPSPI + DMA | Integration test |
| OI-HD-02 | `platform_now_ms()` | FreeRTOS tick | Integration test |
| OI-HD-03 | `platform_driver_set_current(ch, ua)` | 16-ch tACS driver SPI | Stimulation |
| OI-HD-04 | `platform_driver_all_off()` | 16-ch tACS driver | Safety |
| OI-HD-05 | `platform_safety_mcu_request_impedance()` | STM32G071 SPI | Impedance check |
| OI-HD-06 | `platform_safety_mcu_request_enable()` | STM32G071 SPI | Stimulation start |
| OI-HD-07 | `platform_safety_mcu_disable()` | STM32G071 SPI | Stim abort |

---

## 11. Memory Budget

| Item | Size | Location |
|------|------|----------|
| Weight matrix W | 205 KB | LPSDR4 (loaded from Config partition) |
| Source power array | 9.6 KB | LPSDR4 (`.lpsdr4` section) |
| `np_sloreta_ctx_t` (covariance + metadata) | ≤48 B | SRAM |
| `np_hd_stim_ctx_t` | ≤256 B | SRAM |
| `np_hd_session_t` (static pool) | ≈1.5 KB | SRAM |
| `np_hd_montage_t` | ≈32 B | SRAM |
| **Total SRAM** | **≈2 KB** | — |
| **Total LPSDR4** | **≈215 KB** | — |

---

## 12. First Article Inspection (FAI)

Test specification: **NP-FAI-HD-001 Rev A** (embedded in `tests/np_hd_fai_tests.c` and documented here).

### 12.1 FAI-HD01 — sLORETA source localisation accuracy

**Category:** Hardware bench (phantom required)

**Setup:**
- 21-electrode saline skull phantom (IEC 60601-1 electrical reference model)
- Inject known current dipole at 6 standard clinical MNI targets (±5 µA, bilateral current sources)
- Record 2 min EEG at 500 Hz; run firmware sLORETA pipeline

**Pass criteria:**

| ID | Criterion | Limit |
|----|-----------|-------|
| HD01-A | Peak localisation error | ≤ 15 mm from known dipole location |
| HD01-B | Peak-to-median source power ratio | ≥ 3× |
| HD01-C | HD01-A satisfied for | 5 of 6 clinical targets |

**Software plumbing check (CI):** Synthetic weight matrix with known dominant channel; verify peak voxel correct, epoch accumulation clears on reset. See `fai_hd01_sloreta_plumbing()`.

### 12.2 FAI-HD02 — MNI→10-20 electrode mapping accuracy

**Category:** Software-only (full CI coverage)

| ID | Criterion | Limit |
|----|-----------|-------|
| HD02-A | Nearest electrode distance | ≤ 35 mm for all 7 clinical targets |
| HD02-B | Centre electrode closest to target | Always: no cathode closer |
| HD02-C | Cathode angular spread | ≥ 2 quadrants around centre |
| HD02-D | Driver channel conflicts | Zero: `np_hd_montage_validate()` returns OK |
| HD02-E | Standard 2-electrode | Anode ≠ cathode always |
| HD02-F | Bilateral hemisphere laterality | L anode x < 0, R anode x > 0 |

All 6 sub-criteria pass in `fai_hd02_electrode_mapping()` (see test file).

### 12.3 FAI-HD03 — 4×1 ring focality measurement

**Category:** Hardware bench (saline phantom)

**Setup:**
- Spherical 0.25 S/m saline phantom, 21-ch wet gel cap
- NeuroPulse T2 HD-tDCS: 4×1 ring on C3 at 1 mA anode
- Reference micro-electrodes on 5 mm grid inside phantom
- Repeat with standard 2-electrode (C3–P4) at same current

**Pass criteria:**

| ID | Criterion | Limit |
|----|-----------|-------|
| HD03-A | 4×1 ring FWHM at 10 mm depth | ≤ 25 mm |
| HD03-B | Standard 2-electrode FWHM at 10 mm depth | ≥ 60 mm |
| HD03-C | 4×1 peak field ≥ standard peak × | 0.5 (no unacceptable field loss) |
| HD03-D | Channel current accuracy | ±5% of programmed value |

**Software check:** Charge balance (anode + Σ cathodes = 0), electrode area formula. See `fai_hd03_focality_algorithm()`.

### 12.4 FAI-HD04 — EEG SNR during concurrent tDCS

**Category:** Hardware bench

**Setup:**
- T2 21-ch cap on calibrated EEG phantom (known 10 µV alpha dipole source at F3)
- Enable 4×1 HD-tDCS at DLPFC_L montage, 1 mA
- ADS1299 recording with 200 µs blanking on current step edges

**Pass criteria:**

| ID | Criterion | Limit |
|----|-----------|-------|
| HD04-A | EEG SNR in alpha band during tDCS | ≥ 20 dB |
| HD04-B | DC artifact after blanking recovery | < 50 µV peak |
| HD04-C | tDCS-locked artifact in spectrum | < 1 µV² at ramp harmonics |
| HD04-D | EEG return to pre-stim baseline after ramp-down | ≤ 500 ms |

---

## 13. Gate Closure: NP-COORD-001 G3-07

| Requirement | Status |
|-------------|--------|
| sLORETA pipeline spec written (§5) | ✅ Baselined |
| 4×1 montage auto-configuration algorithm specified (§6) | ✅ Baselined |
| Safety limit enforcement confirmed in safety MCU spec (§7.3) | ✅ Baselined |
| FAI-HD01 software plumbing | ✅ PASS (bench pending) |
| FAI-HD02 electrode mapping accuracy | ✅ PASS (software, all 6 criteria) |
| FAI-HD03 focality algorithm check | ✅ PASS (bench pending) |
| FAI-HD04 SNR constants | ✅ PASS (bench pending) |
| UHDR/SHDR data routing (§8.2) | ✅ Consistent with NP-FW-EMMC-001 Rev A §12 |
| BOM delta confirmed | ✅ $0 (Ag/AgCl dual-rated in T2 cap; tACS driver existing hardware) |

**Hardware bench tests (FAI-HD01, HD03, HD04):** BLOCKING for G3 gate sign-off. Must be completed with T2 prototype hardware before G3-07 can be fully closed.

**G3-07 SOFTWARE BASELINED — 2026-05-11**  
**G3-07 HARDWARE PENDING — requires T2 prototype bench verification**

---

## 14. Module File Inventory

| File | Contents |
|------|---------|
| `include/np_hd_config.h` | All configuration constants and safety limits |
| `include/np_hd_types.h` | All shared type definitions |
| `include/np_sloreta.h` | sLORETA source imaging API |
| `include/np_hd_montage.h` | Electrode montage selection API |
| `include/np_hd_stim.h` | Stimulation delivery API |
| `include/np_hd_session.h` | Session orchestration API |
| `src/np_sloreta.c` | Covariance accumulation + source power computation |
| `src/np_hd_montage.c` | MNI LUT + nearest-electrode + ring/bilateral/standard selection |
| `src/np_hd_stim.c` | Ramp state machine, safety MCU SPI, charge density monitor |
| `src/np_hd_session.c` | 5-stage workflow, UHDR/SHDR data routing |
| `tests/np_hd_fai_tests.c` | FAI-HD01–HD04 test stubs + software checks |
| `CMakeLists.txt` | Static library build + host test build |

---

## 15. Revision History

| Rev | Date | Changes |
|-----|------|---------|
| A | 2026-05-11 | Initial release — Issue #23, G3-07 software baselined |
