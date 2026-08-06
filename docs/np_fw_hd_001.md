# sLORETA-Guided HD-tDCS Firmware Specification

**Project:** NeurOne
**Document:** NP-FW-HD-001
**Revision:** B
**Date:** 2026-08-05
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

This document specifies the firmware implementation of sLORETA-guided HD-tDCS for the NeurOne Pro (T2) platform. The feature encompasses:

- Real-time sLORETA cortical source localization from 21-ch qEEG resting-state data
- Automatic MNI target → T2 cap electrode mapping
- 4×1 ring montage configuration with independent per-channel current control
- HD-tDCS stimulation delivery via the 21-ch tACS driver
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

**Depth limit of this claim.** The figure above is quoted at 10 mm depth and is a claim about *cortical surface* targets, which sit 11–29 mm from their nearest cap electrode (§4.3). It does not extend to structures on the medial wall. ACC at (0, 28, 28) is **47.1 mm** from Fz, its nearest electrode on the 21-ch cap, and ~37.9 mm from Fpz — the closest any 10-10 scalp position reaches. No electrode placement makes ACC focally reachable, so this is a property of head geometry, not of the cap or the selection algorithm.

Targets are therefore classified `NP_HD_TARGET_DEPTH_SURFACE` or `NP_HD_TARGET_DEPTH_DEEP` (`np_hd_clinical_target_depth()`). A 4×1 over a DEEP target delivers **indirect network modulation, not focal stimulation of the structure**, and must not be presented to a clinician as the latter. Deep targets remain fully valid *sLORETA source-localization* targets — sLORETA resolves deep sources — and the two roles must not be conflated: locating an abnormality at ACC does not imply the anode can be placed to reach it.

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
| `ACC` | (0, 28, 28) | Anxiety, anterior cingulate — **DEEP** (47.1 mm from Fz; indirect modulation only, see §2.3) |
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
        Exact ties resolve to the LOWEST voxel index (see §5.2)

Step 4: Band power at the peak voxel from the per-band cross-spectral
        covariances accumulated in parallel with C (§5.3).  Independent of
        steps 1-3: a band-power failure leaves the peak result intact.
```

### 5.2 Covariance accumulation

Welford-style running mean across epochs. Each epoch is a 1024-sample block at 500 Hz (2.048 s), with the per-channel epoch mean subtracted. Covariance updated as:

```
C_n = (1 - 1/n) × C_{n-1} + (1/n) × x_epoch x_epoch^T / n_samples
```

The blend is applied **once per epoch**, not once per sample. Applying it inside the sample loop turns it into a geometric decay of rate (1 − 1/n) per sample, which annihilates all but the last few samples of a 1024-sample epoch and reports the whole map low by ~1/N. FAI-HD01's absolute-magnitude assertion and FAI-HD01-D's estimator-variance floor exist to pin this.

**Peak tie-breaking (step 3) is specified, not incidental.** The argmax uses a strict `>` against a `peak_val` seeded from `P[0]`, so on an exact tie the **lowest voxel index** wins. Two tied voxels are two different scalp targets and the peak drives montage selection, so the outcome cannot be left to whichever candidate the comparison happens to visit last. Lowest index is chosen because it makes the result depend only on voxel ordering in the weight matrix, which is fixed at build time and reproducible across runs and revisions. FAI-HD01-F4 (§12.1) pins this, so a refactor to `>=` — which would silently select the highest tied index — fails in CI.

**This broadband covariance is deliberately unwindowed.** A window would scale the variance estimate by the window power factor U (0.375 for Hann) and bias every source-power value low by that factor for no benefit — windowing is a spectral-leakage control, and this accumulator produces no spectrum. Hann windowing applies only to the band-power path in §5.3, which does produce one. The two accumulators run over the same epochs and the same mean-subtracted samples, but are otherwise independent; neither can perturb the other.

### 5.3 Band power decomposition

`np_sloreta_band_power()` returns source power in each of delta / theta / alpha / beta at a specified voxel, computed from a **per-band cross-spectral covariance** accumulated alongside the broadband one.

**Per epoch** (`sloreta_accumulate_spectra()`), for each of the 21 channels:

1. Subtract the epoch mean, apply a **periodic Hann window** (denominator N, not N−1 — the periodic form places a bin-centred tone in exactly three bins with no sidelobe leakage).
2. Run a 1024-point radix-2 DIT FFT (precomputed twiddle table; the per-butterfly complex recurrence used elsewhere in the codebase compounds float32 rotation error across 512 steps in the final stage).
3. Retain only bins `NP_HD_BAND_BIN_LO … NP_HD_BAND_BIN_HI` (1…61, i.e. 0.49–29.8 Hz) — 10 KB of scratch rather than the 168 KB a full 21 × 1024 complex spectrum would need.

Then, for each band *b* and each channel pair (i, j):

```
C_b[i][j] = (2 / (N² · U)) · Σ_{k ∈ band b} Re( X_i[k] · conj(X_j[k]) )
```

where `U = (1/N)·Σ w[n]²` is the window power (computed numerically at init, not hardcoded) and the factor 2 accounts for summing positive frequencies only. `C_b` is folded into a running across-epoch mean with the same once-per-epoch blend as §5.2.

Band power at a voxel is then the same scalar quadratic form evaluated against the band's matrix:

```
P_b[v] = W_v^T · C_b · W_v
```

Each band is an independent measurement over a disjoint bin set. Nothing in this path reads the broadband source-power map from §5.1.

**Properties.** Each `P_b[v]` equals `|W_v · X[k]|²` summed over the band's bins, so it is non-negative by construction. For a coherent tone of amplitude A lying inside one band, that band evaluates to exactly A²/2 — the tone's variance — so the four bands sum to the in-band portion of the time-domain variance from §5.2. FAI-HD01-E asserts both identities.

**Bin extents are declared once**, in `np_hd_config.h`. `np_sloreta.c` iterates each band over its own `[lo, hi]` range, so bands may be re-cut with gaps or overlaps with no code change; `_Static_assert`s enforce that every band lies inside the retained span, above DC and below Nyquist.

#### 5.3.1 Limitations — what this output is and is not

Stated explicitly because §2.1 step 3 uses band output to characterise hypo-/hyperactivation (e.g. high frontal theta → DLPFC_L), and that is a clinical reading.

| | |
|---|---|
| **Units** | Source-space, **not** absolute cortical µV²/Hz. The value carries the scalp signal's µV² scale through the weight matrix W, so its absolute magnitude depends on how W was normalised offline (§5.1). **Ratios** — between bands at one voxel, and between voxels within one band — are the defensible readings. A raw magnitude must not be compared across devices, cap montages, or W revisions. |
| **Band edges** | A Hann-windowed tone occupies three bins, so a component within one bin (0.49 Hz) of a band edge contributes to the neighbouring band. Band boundaries are soft at that scale; a "theta vs alpha" distinction at 7.8–8.3 Hz is not resolvable by this method. |
| **Frequency span** | 0.49–29.8 Hz only. No gamma band is defined for this module. Activity above 29.8 Hz is not represented in any returned value and is **not** folded into beta. |
| **Spectral resolution** | 0.4883 Hz/bin, fixed by 1024 points at 500 Hz. Delta spans only 8 bins, so delta estimates carry the fewest degrees of freedom of the four. |
| **Availability** | Requires at least one full 1024-sample epoch. A short epoch still feeds the broadband covariance but is **skipped** here rather than zero-padded, because padding would rescale the spectrum and bias every band low. Until then the call returns `NP_HD_ERR_NOT_READY` with `valid == false` and all values zeroed. |
| **Validity** | Consumers **must** check `np_hd_band_power_t.valid`. A zeroed struct means "not measured", never "no activity in this band". |
| **Not localisation** | Peak voxel selection (§5.1) uses the broadband covariance only. Band power is a descriptor of the peak once found; it does not participate in choosing it. |

> **Superseded implementation (Rev A).** Rev A computed the broadband quadratic form once and multiplied it by four compile-time constants derived from the bin counts of each band. Because those multipliers were constants, the four returned values were always in fixed proportion, and beta was always largest purely because it spans 35 of the 61 bins. Every possible EEG input produced the same decomposition. No FFT existed in the module. Any Rev A band figure appearing in an analysis, report, or submission is **not a spectral measurement** and must not be relied upon. FAI-HD01-E's cross-stimulus assertion (§12.1) is the regression guard: it cannot be satisfied by any fixed-ratio implementation, under any choice of weights.

### 5.4 API

| Function | Description |
|----------|-------------|
| `np_sloreta_init(ctx, W, mni_lut, n_voxels)` | Bind weight matrix + MNI LUT |
| `np_sloreta_reset(ctx)` | Clear covariance; prepare for new session |
| `np_sloreta_push_epoch(ctx, samples, n_samples)` | Accumulate one EEG epoch into both the broadband and per-band accumulators |
| `np_sloreta_compute_map(ctx, source_power, n)` | Compute full voxel power map |
| `np_sloreta_find_peak(ctx, power, n, result)` | Peak detection + MNI lookup (also fills `result.peak_bands`) |
| `np_sloreta_band_power(ctx, voxel, bands)` | Per-band source power at voxel (§5.3) |

`np_sloreta_band_power()` takes no source-power argument. It reads the per-band cross-spectral covariances directly; it does not partition the broadband map, and a signature implying otherwise would misrepresent the computation.

`np_sloreta_push_epoch()` is **not reentrant and not ISR-safe** — the spectral path drives 21 transforms through ~26 KB of module-static scratch. Call it from one task. At one epoch per 2.048 s the ~1.3 Mflop cost is ≈0.1 % of one core.
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

Because both rings are energised **simultaneously**, "separate" is an invariant over all ten electrodes together, not over each ring in isolation:

1. No electrode may appear in both rings. One Ag/AgCl pellet carries one net current; it cannot serve two independently driven rings.
2. All ten electrodes must map to distinct tACS driver channels (§6.4), across hemispheres as well as within one.

Both rings are selected against a single shared claim set (`ring_select_cathodes()` in `np_hd_montage.c`), so the invariant holds by construction; `np_hd_montage_validate()` re-checks it independently across all ten electrodes as a post-condition.

**Contention.** Where the two rings want the same electrode — in practice a midline one such as Cz, which both M1_L and M1_R rings select as a cathode — the **primary** ring keeps it and the **mirror** ring falls through to its next-nearest candidate. Primary is the ring built at the caller's target; mirror is the one built from the reflected coordinate. Priority follows provenance, not laterality. The rings are consequently not required to be geometrically symmetric; only distinct. With 21 driver channels for 10 simultaneously active electrodes, the only remaining contention is for the electrodes themselves, so the mirror ring yields only where the two rings genuinely overlap — on this cap, the midline. A target whose mirror ring still cannot be completed returns `NP_HD_ERR_MONTAGE_INVALID` rather than a ring that `np_hd_montage_validate()` would reject.

**Midline targets.** A target on x = 0 mirrors onto itself, so it has no contralateral homologue and bilateral is undefined for it — not merely contended. `np_hd_montage_select_bilateral()` returns `NP_HD_ERR_MONTAGE_INVALID`. This covers the predefined targets ACC (0, 28, 28) and MPFC (0, 52, 6), and also near-midline targets whose mirrored coordinate resolves to the same nearest electrode. Use `NP_HD_MONTAGE_RING_4X1` for these.

### 6.4 tACS driver channel assignment

**21 driver channels, one per cap electrode, no sharing.** `k_driver_channel[]` is the identity map: electrode *i* is driven by channel *i*. Maximum 5 of 21 channels active per ring, 10 for bilateral. Distinctness is validated by `np_hd_montage_validate()`.

This supersedes a 16-channel placeholder that wrapped electrodes 16–20 back onto channels 11–15. **16 was never the binding number** — a bilateral 4×1 energises 10 electrodes, so 16 channels were always sufficient. Every observed collision came from the *mapping*, which aliased geometric neighbours (Cz↔Pz, C4↔P4, T8↔P8, P7↔O1, P3↔O2). Because a 4×1 ring draws its cathodes from the electrodes nearest its anode, aliasing neighbours guarantees collisions: C4↔P4 made the M1_R ring undeliverable, since P4 is one of C4's four nearest electrodes.

The T2 cap already carries 21 conductors — its Ag/AgCl electrodes are dual-rated for EEG recording and stimulation current (CLAUDE.md §3 T2) — so only the driver was ever 16-channel, not the harness.

**Constraint of record if sharing is ever reintroduced:** never alias two electrodes within one ring radius (~60 mm) of each other.

**NP-HW-TCAP-001 (T2 cap wiring specification) still requires authoring** to become the controlled hardware source of truth; the firmware table is the current authority until it exists.

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

#### UHDR (user biology — NeurOne never holds key)

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

#### SHDR (device metrics — NeurOne fleet telemetry)

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
| OI-HD-03 | `platform_driver_set_current(ch, ua)` | 21-ch tACS driver SPI | Stimulation |
| OI-HD-04 | `platform_driver_all_off()` | 21-ch tACS driver | Safety |
| OI-HD-05 | `platform_safety_mcu_request_impedance()` | STM32G071 SPI | Impedance check |
| OI-HD-06 | `platform_safety_mcu_request_enable()` | STM32G071 SPI | Stimulation start |
| OI-HD-07 | `platform_safety_mcu_disable()` | STM32G071 SPI | Stim abort |

---

## 11. Memory Budget

| Item | Size | Location |
|------|------|----------|
| Weight matrix W | 205 KB | LPSDR4 (loaded from Config partition) |
| Source power array | 9.6 KB | LPSDR4 (`.lpsdr4` section) |
| `np_sloreta_ctx_t` — broadband covariance 21×21 | 1.8 KB | SRAM (inside session pool) |
| `np_sloreta_ctx_t` — per-band covariances 4×21×21 | 7.1 KB | SRAM (inside session pool) |
| `np_sloreta_ctx_t` — pointers, counters, channel means | ≈110 B | SRAM (inside session pool) |
| `np_hd_stim_ctx_t` | ≤256 B | SRAM |
| `np_hd_montage_t` | ≈32 B | SRAM |
| `np_hd_session_t` (static pool, includes the sLORETA ctx above) | ≈9.5 KB | SRAM |
| Spectral module statics — Hann 4 KB + twiddles 4 KB + FFT workspace 8 KB + retained spectra 10 KB | ≈26 KB | SRAM (`np_sloreta.c` file-scope) |
| **Total SRAM** | **≈36 KB** | of 1 MB on-chip |
| **Total LPSDR4** | **≈215 KB** | of 32 MB |

The Rev A figure of "≤48 B" for `np_sloreta_ctx_t` counted only the pointers and counters and omitted the 21×21 covariance matrix the struct has always contained; the ≈1.5 KB session-pool line inherited the same omission. Both are corrected above alongside the Rev B additions.

The spectral statics are shared across contexts and hold no per-session state — they are live only between entry and return of `np_sloreta_push_epoch()`, which is what makes that function non-reentrant (§5.4). Both totals sit far inside budget; SRAM is the binding resource and is at ≈3.5 % of the 1 MB on-chip pool.

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

**Software plumbing check (CI):** Synthetic weight matrix with known dominant channel; verify peak voxel correct, absolute source-power magnitude correct, epoch accumulation clears on reset. See `fai_hd01_sloreta_plumbing()`.

#### HD01-D — covariance estimator variance (CI, stochastic input)

Zero-mean uniform white noise of known variance A²/12, repeated over 24 independent runs. A deterministic stimulus cannot separate "used all the samples" from "used a few of them" — a discard shows up only as a constant factor, which ordering and ratio assertions are blind to and which cancels in the argmax. See `fai_hd01d_estimator_variance()`.

| ID | Criterion | Limit |
|----|-----------|-------|
| HD01-D1 | Mean estimate vs true variance | within 2 % |
| HD01-D2 | Coefficient of variation across runs | ≤ 3× the derived floor √(0.8/n_eff) |

#### HD01-E — band power is spectral, not a fixed ratio (CI)

**Category:** Software-only (full CI coverage). See `fai_hd01e_band_power_spectral()`.

A bin-centred tone is placed inside each band in turn: delta bin 5 (2.44 Hz), theta bin 12 (5.86 Hz), alpha bin 20 (9.77 Hz), beta bin 40 (19.53 Hz). Each sits ≥3 bins clear of its band edges, so the periodic-Hann 3-bin mainlobe falls entirely inside the intended band and the expected magnitude is derivable in closed form rather than tuned.

| ID | Criterion | Limit |
|----|-----------|-------|
| HD01-E1 | Stimulus band vs each other band | > 20× |
| HD01-E2 | Stimulus band absolute magnitude vs A²/2 | within 2 % |
| HD01-E3 | Stimulus band share of four-band total | ≥ 95 % |
| HD01-E4 | Any band value negative | never |
| HD01-E5 | alpha:delta ratio under alpha stimulus vs under delta stimulus | > 100× (ratio must **invert**) |
| HD01-E6 | Two-tone: band sum vs independently accumulated time-domain variance | within 2 % |
| HD01-E7 | Two voxels reading different channels | different decompositions |
| HD01-E8 | `NOT_READY` when empty / short-epoch / post-reset; `INVALID_ARG` and `NO_WEIGHT_MATRIX` on bad arguments | exact status codes |

**HD01-E5 is the load-bearing criterion.** E1's dominance could in principle be satisfied by a per-band fudge factor; E5 cannot. For any implementation that scales one broadband scalar by fixed per-band weights, both sides of the comparison reduce to `w_alpha · w_delta · P_a · P_d` and the assertion becomes 1 > 100 — false for every possible choice of weights. Measured against the Rev A implementation the HD01-E suite produces 25 failures, E5 among them; the upgraded HD01 band assertion adds a 26th.

#### HD01-F — peak selection is a real argmax (CI)

**Category:** Software-only (full CI coverage). See `fai_hd01f_peak_argmax()`.

Every other HD01 case builds its synthetic weight matrix so that **voxel 0** is the driven one. `np_sloreta_find_peak()` seeds `peak_val` from `source_power[0]` and updates only inside the comparison, so under those stimuli the update body never executed — gcov reported both of its lines as `#####` with the whole suite passing. This matters because `np_hd_session_compute_sloreta()` feeds `peak_mni` directly into `np_hd_montage_select_*()`: the argmax picks the electrode ring current is delivered through, so a silent defect is a wrong-target stimulation path, not a cosmetic one.

The property is probed twice. `find_peak()` takes the source-power array as an argument, so hand-built arrays isolate the comparison from the covariance and weight-matrix path entirely; the covariance path is then exercised separately with a `W` in which a **later** voxel reads the largest-amplitude channel, which proves the two halves are wired together.

| ID | Criterion | Limit |
|----|-----------|-------|
| HD01-F1 | Peak found at index 0, 1, mid-array, and last | exact index, all four positions |
| HD01-F2 | A larger value one past `n_voxels` is ignored; `n_voxels == 1` returns voxel 0 | `n_voxels` is a hard bound |
| HD01-F3 | All-negative map peaks at its least-negative entry | exact index (pins the `source_power[0]` seed) |
| HD01-F4 | Exact tie | **lowest** index wins |
| HD01-F5 | `peak_mni` / `peak_source_power` correspond to the reported `peak_voxel` | exact (verbatim copy, no tolerance) |
| HD01-F6 | NULL `ctx` / `source_power` / `out`, and `n_voxels == 0` | `INVALID_ARG` |
| HD01-F7 | Through `compute_map()`, a later driven voxel is the reported peak | mid-array and last |

**HD01-F4 pins a deliberate choice, not an accident.** Tie-breaking was previously unspecified. The strict `>` comparison retains the lowest index, which is chosen because it makes the result depend only on voxel ordering in the weight matrix — fixed at build time and reproducible across runs and revisions. Two tied voxels are two different scalp targets, so "whichever the compiler felt like" is not an acceptable answer for a stimulation target. A refactor to `>=` would silently switch to the highest tied index and must fail here.

Verified by mutation rather than by coverage alone. Seven defective `find_peak()` variants were built and run: argmin (`<`), update body deleted, `>=` tie-break, short loop bound, over-reading loop bound, `peak_val` seeded from `0.0f`, and a loop starting at `v == 2`. **Five of the seven survived the pre-existing suite with zero failures**; all seven are killed by HD01-F, each by the criterion aimed at it.

### 12.2 FAI-HD02 — MNI→10-20 electrode mapping accuracy

**Category:** Software-only (full CI coverage)

| ID | Criterion | Limit |
|----|-----------|-------|
| HD02-A | Nearest electrode distance | ≤ 35 mm for SURFACE targets; > 35 mm for DEEP targets (checked both ways so neither class can be silently misclassified) |
| HD02-B | Centre electrode closest to target | Always: no cathode closer |
| HD02-C | Cathode angular spread | ≥ 2 quadrants around centre |
| HD02-D | Driver channel conflicts | Zero: `np_hd_montage_validate()` returns OK |
| HD02-E | Standard 2-electrode | Anode ≠ cathode always |
| HD02-F | Bilateral hemisphere laterality | L anode x < 0, R anode x > 0 |

All sub-criteria (HD02-A through HD02-K) pass in `fai_hd02_electrode_mapping()` (see test file).

### 12.3 FAI-HD03 — 4×1 ring focality measurement

**Category:** Hardware bench (saline phantom)

**Setup:**
- Spherical 0.25 S/m saline phantom, 21-ch wet gel cap
- NeurOne T2 HD-tDCS: 4×1 ring on C3 at 1 mA anode
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
| B | 2026-08-05 | §5.3 band power reimplemented as a real spectral measurement. Rev A computed one broadband quadratic form and scaled it by four compile-time bin-count constants, so the four band values were always in fixed proportion and every EEG input produced the same decomposition; no FFT existed in the module. Rev B accumulates a per-band cross-spectral covariance from a Hann-windowed 1024-point FFT per channel per epoch and evaluates `W_v^T C_b W_v` per band. Adds §5.3.1 stating units, band-edge softness, frequency span, and validity semantics, and a superseded-implementation notice for any Rev A band figure. `np_sloreta_band_power()` loses its unused `source_power` argument (§5.4); `np_hd_band_power_t` gains `valid`. §5.2 clarifies that the broadband covariance is deliberately unwindowed and that Hann applies to the spectral path only. §11 memory budget corrected — the Rev A `np_sloreta_ctx_t` figure omitted the covariance matrix — and extended with the per-band matrices and spectral statics. §12.1 adds FAI-HD01-E (single-band dominance, cross-stimulus ratio inversion, Parseval reconciliation) and documents the existing HD01-D criteria. No safety limit, montage, stimulation, or data-routing behaviour changed. |
| C | 2026-08-05 | §12.1 adds FAI-HD01-F — `np_sloreta_find_peak()`'s argmax update body had zero test executions, because every existing case drove voxel 0 and the peak is seeded from `P[0]`. Mutation testing confirms five of seven defective `find_peak()` variants survived the pre-existing suite with zero failures. §5.1/§5.2 now specify peak tie-breaking (lowest voxel index on an exact tie), which was previously unspecified; HD01-F4 pins it. **Test and documentation only — no firmware source changed.** |
