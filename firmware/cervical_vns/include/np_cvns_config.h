/*
 * NeurOne Cervical VNS — Configuration Constants
 * Document: NP-FW-CVNS-001 Rev 1 §3
 */

#ifndef NP_CVNS_CONFIG_H
#define NP_CVNS_CONFIG_H

/* ── Stimulation parameters ─────────────────────────────────────────────────── */
#define NP_CVNS_FREQ_HZ_MIN           1u      /* Hz; CLAUDE.md §3 T2            */
#define NP_CVNS_FREQ_HZ_MAX           25u     /* Hz; matches gammaCore range     */
#define NP_CVNS_CURRENT_UA_MAX        2000u   /* µA; 2 mA absolute limit         */
#define NP_CVNS_PULSE_WIDTH_US_MIN    200u    /* µs                              */
#define NP_CVNS_PULSE_WIDTH_US_MAX    1000u   /* µs                              */
#define NP_CVNS_SESSION_MIN_S         60u     /* s; minimum session duration     */
#define NP_CVNS_SESSION_MAX_S         120u    /* s; 2 min gammaCore protocol     */

/* ── Electrode configuration ────────────────────────────────────────────────── */
#define NP_CVNS_ELECTRODE_COUNT       2u      /* bilateral assembly              */
#define NP_CVNS_IMPEDANCE_MAX_KOHM    5.0f    /* kΩ; cervical gel electrode      */
#define NP_CVNS_IMPEDANCE_CHECK_FREQ_HZ 1000u /* Hz; AC impedance measurement   */
/* Max allowed disagreement between the hub-side per-electrode impedance
 * measurement (OI-CVNS-HUB-09) and the safety MCU's own per-electrode
 * measurement reported over SPI (OI-CVNS-HUB-11).  Mirrors the cardiac
 * NP_CVNS_BASELINE_CROSSVAL_BPM main-vs-MCU tolerance.  1.0 kΩ ≈ 20% of the
 * 5 kΩ ceiling — comfortably above two independent 1 kHz AC probes' expected
 * spread, small enough to catch a mis-wired or drifted electrode/sensor.  A
 * divergence beyond this is treated fail-closed (block enable).                */
#define NP_CVNS_IMPEDANCE_CROSSVAL_KOHM 1.0f  /* kΩ; hub vs safety MCU tolerance  */

/* ── Biphasic waveform timing ────────────────────────────────────────────────── */
#define NP_CVNS_INTER_PHASE_GAP_US    100u    /* µs; gap between phases          */
#define NP_CVNS_CHARGE_BALANCE_TOL_UC 1u      /* µC; charge balance tolerance    */

/* ── Ramp timing ─────────────────────────────────────────────────────────────── */
#define NP_CVNS_RAMP_UP_S             10u     /* s; ramp-up for comfort          */
#define NP_CVNS_RAMP_DOWN_S           5u      /* s                               */
#define NP_CVNS_STIM_TICK_MS          100u    /* ms; stim module tick rate       */

/* ── Cardiac interlock ───────────────────────────────────────────────────────── */
#define NP_CVNS_HR_CHANGE_LIMIT_BPM   15u     /* BPM; CLAUDE.md §3 T2 threshold  */
#define NP_CVNS_HR_WINDOW_S           5u      /* s; observation window           */
#define NP_CVNS_CARDIAC_POLL_MS       5u      /* ms; safety MCU polling (200 Hz) */
#define NP_CVNS_CUTOFF_LATENCY_MAX_MS 100u    /* ms; FAI-CV02 pass criterion     */
#define NP_CVNS_BASELINE_BEATS_MIN    5u      /* beats for stable baseline       */
#define NP_CVNS_RR_WINDOW_SIZE        20u     /* R-R circular buffer depth       */
#define NP_CVNS_RR_MAX_VALID_MS       2000u   /* ms; ≈30 BPM minimum HR          */
#define NP_CVNS_RR_MIN_VALID_MS       300u    /* ms; ≈200 BPM maximum HR         */
#define NP_CVNS_RPEAK_DEBOUNCE_US     30u     /* µs; GPIO edge debounce          */
#define NP_CVNS_BASELINE_CROSSVAL_BPM 5u      /* BPM; main vs safety MCU tolerance*/
#define NP_CVNS_DATA_LOSS_TIMEOUT_S   10u     /* s; max gap before soft cutoff   */

/* ── Re-enable policy ────────────────────────────────────────────────────────── */
#define NP_CVNS_REENABLE_LOCKOUT_S    30u     /* s; minimum lockout after cutoff */

/* ── Safety MCU SPI command bytes ────────────────────────────────────────────── */
#define NP_CVNS_SPI_CMD_IMPEDANCE_CHECK 0x10u
#define NP_CVNS_SPI_CMD_ENABLE          0x11u
#define NP_CVNS_SPI_CMD_DISABLE         0x12u
#define NP_CVNS_SPI_CMD_HR_BASELINE_SET 0x13u
#define NP_CVNS_SPI_CMD_STATUS_QUERY    0x14u
#define NP_CVNS_SPI_RSP_STATUS          0x80u
#define NP_CVNS_SPI_RSP_FAULT_NOTIFY    0x81u
#define NP_CVNS_SPI_RSP_IMPEDANCE       0x82u
#define NP_CVNS_SPI_RSP_ENABLE_GRANTED  0x83u
#define NP_CVNS_SPI_RSP_ENABLE_REJECTED 0x84u

/* ── Pan-Tompkins R-peak detection ──────────────────────────────────────────── */
#define NP_CVNS_PPG_SAMPLE_RATE_HZ    200u    /* Hz; PPG ADC rate                */
#define NP_CVNS_PT_INTEGRATE_WIN_MS   150u    /* ms; moving window integrator    */
#define NP_CVNS_PT_REFRACTORY_MS      200u    /* ms; post-peak refractory period */
#define NP_CVNS_PT_THRESHOLD_FRAC     0.75f   /* adaptive threshold fraction     */
#define NP_CVNS_PT_HISTORY_S          2u      /* s; running maximum history      */

/* ── Context size bound (static_assert guard) ───────────────────────────────── */
#define NP_CVNS_INTERLOCK_CTX_SIZE_BYTES  256u
#define NP_CVNS_STIM_CTX_SIZE_BYTES       128u
#define NP_CVNS_SESSION_CTX_SIZE_BYTES    512u

#endif /* NP_CVNS_CONFIG_H */
