/*
 * NeuroPulse 1064nm Smart Zone Module — Configuration Constants
 * Document: NP-FW-PBM1064-001 Rev A §3
 * Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
 *
 * Hardware context:
 *   Pin 10 = SDA (repurposed LED_B line), Pin 11 = SCL
 *   Pin 18 = ZONE_ID: 3.3 kΩ for smart module, 10k–220k for base modules
 *   10 kΩ pull-up to 3.3 V on hub PCB.
 *
 *   Smart module ZONE_ID (3.3 kΩ):
 *     Vout = 3.3 V × 3300 / (10000 + 3300) = 0.819 V → ADC ≈ 1016 counts
 *   Threshold: ADC < 1100 → smart module; 1100–4000 → base module.
 */

#ifndef NP_PBM1064_CONFIG_H
#define NP_PBM1064_CONFIG_H

/* ── Smart module ZONE_ID ADC threshold ─────────────────────────────────────── */

#define NP_PBM1064_ADC_SMART_MAX        1099U   /* < 1100 → smart module detected  */
#define NP_PBM1064_ADC_BASE_MIN         1100U   /* ≥ 1100 → base module ladder     */
#define NP_PBM1064_ADC_NO_MODULE_MIN    4000U   /* ≥ 4000 → no module present      */
#define NP_PBM1064_ADC_BITS             12U
#define NP_PBM1064_ADC_VREF_MV          3300U
#define NP_PBM1064_ADC_PULLUP_OHMS      10000U

/* ── Debounce (RISK-18; same policy as base modules) ─────────────────────────── */

#define NP_PBM1064_DEBOUNCE_READS       3U      /* ADC reads per cycle             */
#define NP_PBM1064_DEBOUNCE_MAJORITY    2U      /* ≥ 2/3 must agree                */
#define NP_PBM1064_DEBOUNCE_INTERVAL_MS 100U
#define NP_PBM1064_INSERTION_SETTLE_MS  20U
#define NP_PBM1064_REMOVAL_DEBOUNCE_MS  50U
#define NP_PBM1064_POLL_IDLE_MS         50U
#define NP_PBM1064_POLL_ACTIVE_MS       100U

/* ── I2C protocol ───────────────────────────────────────────────────────────── */

/* On-module 3-channel LED driver I2C address (7-bit). */
#define NP_PBM1064_I2C_ADDR             0x30U
#define NP_PBM1064_I2C_PROBE_TIMEOUT_MS 5U
#define NP_PBM1064_I2C_CLK_HZ          400000UL   /* 400 kHz fast mode            */
#define NP_PBM1064_I2C_STATUS_POLL_MS   5000U      /* 5 s periodic STATUS read     */

/* ── Driver IC register addresses (0x00–0x0D) ───────────────────────────────── */

#define NP_PBM1064_REG_STATUS           0x00U
#define NP_PBM1064_REG_CH_ENABLE        0x01U
#define NP_PBM1064_REG_CUR_A            0x02U   /* 660nm                          */
#define NP_PBM1064_REG_CUR_B            0x03U   /* 808nm                          */
#define NP_PBM1064_REG_CUR_C            0x04U   /* 1064nm                         */
#define NP_PBM1064_REG_PWM_FREQ_A       0x05U
#define NP_PBM1064_REG_PWM_FREQ_B       0x06U
#define NP_PBM1064_REG_PWM_FREQ_C       0x07U
#define NP_PBM1064_REG_DUTY_A           0x08U
#define NP_PBM1064_REG_DUTY_B           0x09U
#define NP_PBM1064_REG_DUTY_C           0x0AU
#define NP_PBM1064_REG_THERMAL          0x0BU
#define NP_PBM1064_REG_FAULT_LATCH      0x0CU
#define NP_PBM1064_REG_CONFIG           0x0DU

/* ── STATUS register bit masks ───────────────────────────────────────────────── */

#define NP_PBM1064_STATUS_FAULT         (1U << 0)
#define NP_PBM1064_STATUS_THERMAL       (1U << 1)
#define NP_PBM1064_STATUS_OCP_A         (1U << 2)
#define NP_PBM1064_STATUS_OCP_B         (1U << 3)
#define NP_PBM1064_STATUS_OCP_C         (1U << 4)

/* ── CH_ENABLE register bit masks ────────────────────────────────────────────── */

#define NP_PBM1064_CH_A_EN              (1U << 0)   /* 660nm                      */
#define NP_PBM1064_CH_B_EN              (1U << 1)   /* 808nm                      */
#define NP_PBM1064_CH_C_EN              (1U << 2)   /* 1064nm                     */
#define NP_PBM1064_CH_ALL_EN            (NP_PBM1064_CH_A_EN | NP_PBM1064_CH_B_EN | NP_PBM1064_CH_C_EN)

/* ── CONFIG register bit masks ───────────────────────────────────────────────── */

#define NP_PBM1064_CONFIG_SOFT_RESET    (1U << 0)
#define NP_PBM1064_CONFIG_PWM_MODE      (1U << 1)
#define NP_PBM1064_CONFIG_SYNC_EN       (1U << 2)

/* ── Duty cycle ceiling (25%, IEC 60601 peak pulsed limit) ──────────────────── */

/*
 * DUTY register encodes 0.5% per LSB: 0x00=0%, 0x32=25%, 0xC8=100%.
 * Hub firmware never writes > NP_PBM1064_DUTY_MAX_REG to any DUTY register.
 */
#define NP_PBM1064_DUTY_MAX_REG         0x32U   /* 50 decimal; 25% duty           */
#define NP_PBM1064_DUTY_FULL_REG        0xC8U   /* 200 decimal; 100% (CW ref)     */

/* ── PWM frequency codes ─────────────────────────────────────────────────────── */

#define NP_PBM1064_FREQ_CODE_CW         0x00U   /* continuous wave (DC)           */
#define NP_PBM1064_FREQ_CODE_2HZ        0x01U
#define NP_PBM1064_FREQ_CODE_6HZ        0x06U
#define NP_PBM1064_FREQ_CODE_10HZ       0x0AU
#define NP_PBM1064_FREQ_CODE_20HZ       0x14U
#define NP_PBM1064_FREQ_CODE_40HZ       0x28U   /* default: gamma                  */

/* ── Thermal limits (IEC 60601, CLAUDE.md §3) ───────────────────────────────── */

#define NP_PBM1064_THERMAL_FAULT_C      62U     /* NTC junction — THERMAL register  */
#define NP_PBM1064_THERMAL_CUTOFF_C     65U     /* immediate all-channel disable    */
#define NP_PBM1064_THERMAL_RECHECK_MS   5000U   /* ms after throttle before retry  */

/* ── Dose limits (per wavelength, per zone, per session; NP-SES-1064-001) ───── */

#define NP_PBM1064_DOSE_LIMIT_660_J_CM2        60.0f
#define NP_PBM1064_DOSE_LIMIT_808_J_CM2        60.0f
#define NP_PBM1064_DOSE_LIMIT_1064_J_CM2       36.0f   /* conservative; see OI-SES-01 */

/* Aggregate irradiance ceiling (OI-PBM-05 — pending RISK-03 regulatory opinion). */
#define NP_PBM1064_AGGREGATE_IRRADIANCE_MW_CM2 600.0f

/* ── Dose metering ───────────────────────────────────────────────────────────── */

#define NP_PBM1064_DOSE_TICK_HZ         10U     /* PD ADC sample + dose integrate  */
#define NP_PBM1064_DOSE_TICK_MS         (1000U / NP_PBM1064_DOSE_TICK_HZ)
#define NP_PBM1064_NTC_POLL_HZ          1U
#define NP_PBM1064_NTC_POLL_MS          1000U

/* PD2 minimum count to declare it "stable" (not a dead channel) for fouling test.*/
#define NP_PBM1064_PD2_STABLE_MIN       200U

/* Fouling: ratio drops >20% below nominal (PD2 remains stable). */
#define NP_PBM1064_FOULING_RATIO_THRESH 0.80f

/* Aging: absolute PD1 count drops below this with ratio stable (% of factory ADC).*/
#define NP_PBM1064_AGING_PD1_PMIN      0.70f   /* 70% of factory baseline         */

/* EEG-adaptive hysteresis: freq code only updates after N consecutive ticks. */
#define NP_PBM1064_EEG_HYSTERESIS_TICKS 3U

/* ── Session parameters ──────────────────────────────────────────────────────── */

#define NP_PBM1064_RAMP_DURATION_S      30U
#define NP_PBM1064_RAMP_TICK_MS         NP_PBM1064_DOSE_TICK_MS  /* 100 ms        */
#define NP_PBM1064_SESSION_DUR_MIN_S    60U
#define NP_PBM1064_SESSION_DUR_MAX_S    3600U

/* ── Calibration ─────────────────────────────────────────────────────────────── */

#define NP_PBM1064_WL_COUNT             3U      /* 0=660nm, 1=808nm, 2=1064nm     */
#define NP_PBM1064_ZONE_COUNT           5U      /* ZM-01 through ZM-05            */

/* ── FreeRTOS task parameters ────────────────────────────────────────────────── */

#define NP_PBM1064_TASK_STACK_WORDS     768U
#define NP_PBM1064_TASK_PRIORITY        4U

#endif /* NP_PBM1064_CONFIG_H */
