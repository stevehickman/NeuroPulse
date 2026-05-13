#ifndef NP_PBM1064_CONFIG_H
#define NP_PBM1064_CONFIG_H

/* Smart module ZONE_ID detection
 * Pin 18 resistor: 3.3 kΩ to GND with 10 kΩ pull-up to 3.3 V
 * VOUT = 3.3 × 3.3 / (10 + 3.3) = 0.819 V → ADC ≈ 1017 counts (12-bit, Vref=3.3V)
 * Threshold set at midpoint between smart module (1017) and ZM-01 base (2048).
 * Source: NP-FW-PBM1064-001 Rev A §3.1 */
#define NP_PBM1064_SMART_MODULE_ADC_THRESHOLD   1100u   /* counts */
#define NP_PBM1064_SMART_MODULE_RESISTOR_OHMS   3300u   /* Ω on pin 18 */

/* Debounce: 3 reads at 100 ms, ≥2/3 majority (RISK-18) */
#define NP_PBM1064_DEBOUNCE_READS               3u
#define NP_PBM1064_DEBOUNCE_MAJORITY            2u
#define NP_PBM1064_DEBOUNCE_INTERVAL_MS         100u
#define NP_PBM1064_INSERTION_SETTLE_MS          20u

/* I2C */
#define NP_PBM1064_I2C_ADDR                     0x30u
#define NP_PBM1064_I2C_SPEED_HZ                 100000u
#define NP_PBM1064_I2C_PROBE_TIMEOUT_MS         5u

/* On-module driver IC register map (NP-FW-PBM1064-001 Rev A §5.1) */
#define NP_PBM1064_REG_STATUS                   0x00u
#define NP_PBM1064_REG_CH_ENABLE                0x01u
#define NP_PBM1064_REG_CUR_A                    0x02u   /* 660 nm */
#define NP_PBM1064_REG_CUR_B                    0x03u   /* 808 nm */
#define NP_PBM1064_REG_CUR_C                    0x04u   /* 1064 nm */
#define NP_PBM1064_REG_PWM_FREQ_A               0x05u
#define NP_PBM1064_REG_PWM_FREQ_B               0x06u
#define NP_PBM1064_REG_PWM_FREQ_C               0x07u
#define NP_PBM1064_REG_DUTY_A                   0x08u
#define NP_PBM1064_REG_DUTY_B                   0x09u
#define NP_PBM1064_REG_DUTY_C                   0x0Au
#define NP_PBM1064_REG_THERMAL                  0x0Bu
#define NP_PBM1064_REG_FAULT_LATCH              0x0Cu
#define NP_PBM1064_REG_CONFIG                   0x0Du

/* STATUS register bit masks */
#define NP_PBM1064_STATUS_READY                 (1u << 0)
#define NP_PBM1064_STATUS_OCP_C                 (1u << 4)
#define NP_PBM1064_STATUS_OCP_B                 (1u << 5)
#define NP_PBM1064_STATUS_OCP_A                 (1u << 6)
#define NP_PBM1064_STATUS_THERMAL_FAULT         (1u << 7)

/* CH_ENABLE bit masks */
#define NP_PBM1064_CH_A                         (1u << 0)   /* 660 nm */
#define NP_PBM1064_CH_B                         (1u << 1)   /* 808 nm */
#define NP_PBM1064_CH_C                         (1u << 2)   /* 1064 nm */

/* CONFIG register */
#define NP_PBM1064_CONFIG_SOFT_START            (1u << 0)
#define NP_PBM1064_CONFIG_PWM_SYNC              (1u << 1)

/* Safety: duty cycle ceiling — hub firmware never writes above this (NP-FW-PBM1064-001 §5.3)
 * 0x32 = 50 in decimal = 25% (register encodes 0.5%/LSB) */
#define NP_PBM1064_DUTY_MAX_REG                 0x32u       /* 25% duty cycle ceiling */
#define NP_PBM1064_DUTY_PCT_MAX                 25u

/* Current range */
#define NP_PBM1064_CUR_MA_MIN                   80u
#define NP_PBM1064_CUR_MA_MAX                   200u

/* Dose metering */
#define NP_PBM1064_DOSE_TICK_HZ                 10u         /* PD ADC sample rate */
#define NP_PBM1064_DOSE_LIMIT_660_J_CM2         60.0f
#define NP_PBM1064_DOSE_LIMIT_808_J_CM2         60.0f
#define NP_PBM1064_DOSE_LIMIT_1064_J_CM2        36.0f       /* conservative — see OI-SES-01 */
#define NP_PBM1064_DOSE_WARN_PCT                90u         /* warn at 90% of limit */

/* Aggregate irradiance ceiling: 3-channel combined (NP-FW-PBM1064-001 §5.4)
 * See OI-PBM-05 — pending regulatory opinion for final value */
#define NP_PBM1064_AGGREGATE_IRRADIANCE_MW_CM2  600.0f

/* Thermal thresholds (°C) */
#define NP_PBM1064_THERMAL_WARN_C              38
#define NP_PBM1064_THERMAL_THROTTLE_C          40
#define NP_PBM1064_THERMAL_HW_LIMIT_C          42          /* hardware NTC path, not firmware */

/* PD1/PD2 ratio thresholds for fouling/aging detection */
#define NP_PBM1064_FOULING_RATIO_DELTA         (-0.25f)    /* PD1/PD2 drops >25% below nominal */
#define NP_PBM1064_AGING_RATIO_THRESHOLD       (0.70f)     /* both PDs drop to <70% of baseline */

/* NTC poll interval during session */
#define NP_PBM1064_NTC_POLL_INTERVAL_MS        1000u

/* I2C status poll during session */
#define NP_PBM1064_STATUS_POLL_INTERVAL_MS     5000u

/* Default InGaAs calibration coefficients (mW/cm² per ADC count)
 * Applied when no factory calibration is present in Config partition.
 * Source: estimated from InGaAs responsivity ~0.85 A/W at 1064 nm and TIA gain.
 * Factory calibration from integrating sphere will override these per OI-PBM-04. */
#define NP_PBM1064_K_DEFAULT_PD1_1064          (0.12f)
#define NP_PBM1064_K_DEFAULT_PD2_1064          (0.09f)
#define NP_PBM1064_K_DEFAULT_RATIO_NOM_1064    (1.33f)     /* PD1/PD2 nominal at factory */

/* Number of zones */
#define NP_PBM1064_ZONE_COUNT                  5u
#define NP_PBM1064_WAVELENGTH_COUNT            3u          /* 0=660nm, 1=808nm, 2=1064nm */

/* Ramp duration */
#define NP_PBM1064_RAMP_UP_MS                  30000u
#define NP_PBM1064_RAMP_DOWN_MS                30000u
#define NP_PBM1064_RAMP_TICK_MS                100u

#endif /* NP_PBM1064_CONFIG_H */
