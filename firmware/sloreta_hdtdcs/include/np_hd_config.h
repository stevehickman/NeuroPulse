/*
 * NeurOne sLORETA-guided HD-tDCS — Configuration Constants
 * Document: NP-FW-HD-001 Rev A §3
 * Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
 *
 * Safety limits per CLAUDE.md §3 T2 additions and Bikson lab (2016) safety
 * analysis for Ag/AgCl sintered 3.5 mm electrodes.
 */

#ifndef NP_HD_CONFIG_H
#define NP_HD_CONFIG_H

/* ── EEG acquisition ────────────────────────────────────────────────────────── */
#define NP_HD_EEG_CHANNELS          21U     /* T2 21-ch qEEG wet gel cap         */
#define NP_HD_EEG_SAMPLE_RATE_HZ    500U    /* ADS1299 sample rate               */
#define NP_HD_RESTING_DURATION_S    120U    /* 2-min eyes-closed resting state   */

/* ── sLORETA source model ───────────────────────────────────────────────────── */
/* Scalar sLORETA (dominant dipole orientation per voxel, pre-selected offline). */
/* Weight matrix precomputed from BEM forward model on MNI152 standard head.    */
#define NP_HD_SLORETA_N_VOXELS      2447U   /* cortical mesh, 7 mm MNI grid      */
#define NP_HD_SLORETA_N_CH          NP_HD_EEG_CHANNELS
/* Weight matrix footprint in LPSDR4: 2447 × 21 × float32 ≈ 205 KB             */
#define NP_HD_WEIGHT_MATRIX_BYTES   ((uint32_t)(NP_HD_SLORETA_N_VOXELS) *  \
                                     (uint32_t)(NP_HD_SLORETA_N_CH) *      \
                                     (uint32_t)sizeof(float))

/* ── sLORETA spectral analysis (Welch, 500 Hz / 1024 pt = 0.4883 Hz/bin) ───── */
#define NP_HD_SLORETA_FFT_SIZE      1024U
#define NP_HD_SLORETA_OVERLAP       512U    /* 50 % overlap                      */
#define NP_HD_SLORETA_EPOCHS        64U     /* epochs averaged for covariance    */

/* Band bins (0.4883 Hz/bin) */
#define NP_HD_DELTA_BIN_LO          1U      /* 0.49 Hz                           */
#define NP_HD_DELTA_BIN_HI          8U      /* 3.91 Hz                           */
#define NP_HD_THETA_BIN_LO          9U      /* 4.39 Hz                           */
#define NP_HD_THETA_BIN_HI          16U     /* 7.81 Hz                           */
#define NP_HD_ALPHA_BIN_LO          17U     /* 8.30 Hz                           */
#define NP_HD_ALPHA_BIN_HI          26U     /* 12.70 Hz                          */
#define NP_HD_BETA_BIN_LO           27U     /* 13.18 Hz                          */
#define NP_HD_BETA_BIN_HI           61U     /* 29.79 Hz                          */

/* ── Electrode geometry ─────────────────────────────────────────────────────── */
/* Ag/AgCl sintered 3.5 mm diameter, dual-rated EEG recording + tDCS delivery. */
#define NP_HD_ELECTRODE_DIAM_MM     3.5f
#define NP_HD_ELECTRODE_AREA_CM2    0.0962f /* π × (0.175 cm)²                   */
#define NP_HD_ELECTRODE_AREA_M2     9.621e-6f /* for A/m² computation            */

/* ── Safety limits (enforced by safety MCU — app cannot override) ───────────── */
#define NP_HD_MAX_CURRENT_UA        2000U   /* 2 mA per electrode                */
#define NP_HD_MAX_CHARGE_DENSITY_UC_CM2   40.0f /* µC/cm² per phase (charge-balanced) */
#define NP_HD_MAX_ELECTRODE_DENSITY_A_M2   6.0f /* tissue current density limit  */
#define NP_HD_RAMP_DURATION_S       30U     /* 30 s ramp up and ramp down        */

/* Charge per phase limit (hardware enforcement):                                */
/* Q_ph = 40 µC/cm² × 0.0962 cm² = 3.85 µC                                     */
#define NP_HD_MAX_CHARGE_PER_PHASE_UC   3.85f

/* ── Montage configuration ──────────────────────────────────────────────────── */
#define NP_HD_RING_CATHODE_COUNT    4U      /* 4×1: 4 return cathodes            */
#define NP_HD_RING_ELECTRODE_COUNT  5U      /* 1 anode + 4 cathodes              */
#define NP_HD_BILATERAL_ELEC_COUNT  10U     /* bilateral 4×1: 10 total           */
#define NP_HD_STD_2ELEC_COUNT       2U      /* standard 2-electrode fallback     */

/* Cathode current per electrode in 4×1 ring (current splits equally):          */
/* anode current / 4. For 1 mA anode: 0.25 mA per cathode.                     */
#define NP_HD_CATHODE_SPLIT_DENOM   4U

/* ── tACS driver channel mapping ────────────────────────────────────────────── */
/* 21-ch tACS driver: one channel per cap electrode, no sharing.  See the note  */
/* above k_driver_channel[] in np_hd_montage.c for why this is not 16.          */
#define NP_HD_DRIVER_CHANNELS       21U
/* Returned by np_hd_electrode_driver_channel() for an out-of-range electrode. */
#define NP_HD_DRIVER_CH_NONE        0xFFU

/* ── Impedance and quality checks ───────────────────────────────────────────── */
#define NP_HD_MAX_IMPEDANCE_KOHM    10U     /* abort stimulation if exceeded     */
#define NP_HD_IMPEDANCE_CHECK_FREQ_HZ 1000U /* AC impedance check frequency     */

/* ── Concurrent EEG during tDCS ─────────────────────────────────────────────── */
/* ADS1299 input MUX switches to short during current steps to suppress DC jump.*/
#define NP_HD_EEG_BLANK_WINDOW_US   200U    /* blanking per current step edge    */
/* SNR threshold for FAI-HD04 acceptance: EEG SNR ≥ 20 dB in alpha band        */
#define NP_HD_EEG_MIN_SNR_DB        20.0f

/* ── Stimulation session parameters ─────────────────────────────────────────── */
#define NP_HD_DEFAULT_CURRENT_UA    1000U   /* 1 mA anode (within 2 mA limit)   */
#define NP_HD_MIN_SESSION_S         120U    /* 2 minutes minimum                 */
#define NP_HD_MAX_SESSION_S         1800U   /* 30 minutes maximum                */

/* ── Safety MCU heartbeat ───────────────────────────────────────────────────── */
/* HD-tDCS shares the 200 ms SPI heartbeat from the main safety architecture.   */
/* All HD-tDCS enable lines are owned by the safety MCU GPIO (CLAUDE.md §4.2). */
#define NP_HD_SAFETY_MCU_HEARTBEAT_MS   200U
#define NP_HD_SAFETY_MCU_WATCHDOG_MS    1500U /* cutoff threshold                */

/* ── MNI coordinate range ───────────────────────────────────────────────────── */
#define NP_HD_MNI_COORD_MAX_MM      150     /* maximum MNI coordinate magnitude  */

/* ── SHDR telemetry keys ─────────────────────────────────────────────────────── */
#define NP_HD_SHDR_SESSION_KEY      "hd_tdcs_session_v1"
#define NP_HD_SHDR_IMPEDANCE_KEY    "hd_electrode_impedance_v1"
#define NP_HD_SHDR_MONTAGE_KEY      "hd_montage_log_v1"

#endif /* NP_HD_CONFIG_H */
