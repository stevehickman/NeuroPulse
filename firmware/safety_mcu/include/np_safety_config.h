/*
 * NeuroPulse Safety MCU — Hardware Configuration
 * Target: STM32G071 (Cortex-M0+, 64 MHz)
 * Document: NP-SW-001 Rev A, NP-FW-EMMC-001 Rev A §4.2
 *
 * GPIO assignments, peripheral config, and timing constants for the
 * safety MCU.  All stimulation enable GPIOs are active-LOW: asserting LOW
 * drives the enable HIGH via an inverting buffer.  This fail-safe design
 * ensures power loss or reset cuts stimulation immediately.
 *
 * GPIO bank assignments are provisional pending PCB layout (G1 gate).
 */

#ifndef NP_SAFETY_CONFIG_H
#define NP_SAFETY_CONFIG_H

#include <stdint.h>

/* ── Clock ────────────────────────────────────────────────────────────────── */

#define NP_SAFETY_SYSCLK_HZ     64000000UL  /* 64 MHz from HSI16 × PLL */

/* ── SPI (SPI1, slave) ────────────────────────────────────────────────────── */
/* SPI1: PA5=SCK, PA6=MISO, PA7=MOSI, PA4=NSS (hardware NSS management)      */
#define NP_SAFETY_SPI_INSTANCE  SPI1
#define NP_SAFETY_FRAME_LEN     8U   /* bytes per exchange (matches hub_config.h) */

/* ── Watchdog timing ─────────────────────────────────────────────────────── */
#define NP_SAFETY_WDG_TIMEOUT_MS    1500U  /* heartbeat missed → cutoff */
#define NP_SAFETY_HEARTBEAT_EXP_MS  200U   /* expected period from main processor */
#define NP_SAFETY_SYSTICK_HZ        1000U  /* 1ms SysTick resolution */

/* ── Stimulation enable GPIOs (active-LOW open-drain) ────────────────────── */
/* Each bit position matches NP_SAFETY_EN_* in hub_control/np_hub_config.h   */
/* PBM zone modules — PA0..PA4 */
#define NP_EN_PBM_ZONE0_PORT    GPIOA
#define NP_EN_PBM_ZONE0_PIN     (1U << 0)
#define NP_EN_PBM_ZONE1_PORT    GPIOA
#define NP_EN_PBM_ZONE1_PIN     (1U << 1)
#define NP_EN_PBM_ZONE2_PORT    GPIOA
#define NP_EN_PBM_ZONE2_PIN     (1U << 2)
#define NP_EN_PBM_ZONE3_PORT    GPIOA
#define NP_EN_PBM_ZONE3_PIN     (1U << 3)
#define NP_EN_PBM_ZONE4_PORT    GPIOA
#define NP_EN_PBM_ZONE4_PIN     (1U << 4)
/* Stimulation channels — PB0..PB7 */
#define NP_EN_BES_PORT          GPIOB
#define NP_EN_BES_PIN           (1U << 0)
#define NP_EN_TDCS_PORT         GPIOB
#define NP_EN_TDCS_PIN          (1U << 1)
#define NP_EN_VNS_PORT          GPIOB
#define NP_EN_VNS_PIN           (1U << 2)
#define NP_EN_VISUAL_PORT       GPIOB
#define NP_EN_VISUAL_PIN        (1U << 3)
#define NP_EN_INTRANASAL_PORT   GPIOB
#define NP_EN_INTRANASAL_PIN    (1U << 4)
#define NP_EN_CVNS_PORT         GPIOB
#define NP_EN_CVNS_PIN          (1U << 5)
#define NP_EN_TMS_PORT          GPIOB
#define NP_EN_TMS_PIN           (1U << 6)
#define NP_EN_PBM_1170_PORT     GPIOB
#define NP_EN_PBM_1170_PIN      (1U << 7)
#define NP_EN_CLIN_STIM_PORT    GPIOB
#define NP_EN_CLIN_STIM_PIN     (1U << 8)

/* ── Cardiac interlock (SW01-M05) ────────────────────────────────────────── */
/* R-peak pulse from main processor: PA8, active-high, 5ms pulse width       */
#define NP_RPEAK_IN_PORT        GPIOA
#define NP_RPEAK_IN_PIN         (1U << 8)
#define NP_RPEAK_PULSE_MS       5U          /* expected pulse width */

/* TIM2 captures R-peak edges at 1MHz for RR-interval measurement */
#define NP_CARDIAC_TIM          TIM2
#define NP_CARDIAC_TIM_HZ       1000000UL   /* 1µs resolution */
#define NP_CARDIAC_HR_DELTA_BPM 15U         /* cutoff threshold */
#define NP_CARDIAC_OBS_MS       5000U       /* observation window */
#define NP_CARDIAC_LOCKOUT_MS   30000U      /* re-enable lockout */
#define NP_CARDIAC_BASELINE_BEATS 8U        /* beats to establish baseline */

/* ── Thermal interlock (SW01-M04) ────────────────────────────────────────── */
/* ADC1 channels: NTC per zone (5 channels) and hub NTC (1 channel)          */
#define NP_NTC_CUTOFF_DEG_C     62U   /* junction temperature (not case) */
#define NP_NTC_ADC_INSTANCE     ADC1
#define NP_NTC_CHANNEL_COUNT    6U    /* 5 zones + 1 hub */

/* ── Charge density monitor (SW01-M03) ───────────────────────────────────── */
/* 40 µC/cm² charge density limit; enforced per electrode, per session.      */
/* Electrode area for standard tDCS/BES electrode: 25 cm².                   */
#define NP_CHARGE_LIMIT_UC_CM2      40U   /* µC/cm² hard limit */
#define NP_ELECTRODE_AREA_CM2       25U   /* default electrode area */
/* Per-electrode absolute limit = NP_CHARGE_LIMIT_UC_CM2 * NP_ELECTRODE_AREA_CM2 */
#define NP_CHARGE_LIMIT_UC      (NP_CHARGE_LIMIT_UC_CM2 * NP_ELECTRODE_AREA_CM2)

/* ── Impedance check (SW01-M06) ──────────────────────────────────────────── */
/* 1 kHz AC test current injected for 50ms; reject if Zmeasured > 10kΩ.     */
#define NP_IMPEDANCE_TEST_HZ    1000U
#define NP_IMPEDANCE_TEST_MS    50U
#define NP_IMPEDANCE_MAX_OHM    10000U  /* above this → contacts not confirmed */

/* ── Fault latch (SW01-M08) ─────────────────────────────────────────────── */
#define NP_FAULT_LATCH_MAGIC    0xDEADBEEFUL  /* sentinel for latch validity */

/* ── Session signature (SW01-M07) ───────────────────────────────────────── */
#define NP_ED25519_PUB_KEY_LEN  32U  /* manufacturing root public key (OTP) */
/* NP_ED25519_SIG_LEN and NP_SESSION_HASH_LEN are in firmware/common/include/np_spi_wire_types.h,
 * included transitively via np_safety_protocol.h. */

/* Fault slot codes (stored in np_safety_state_t.fault_slot).
 * 0xFF = no fault (generic init value).
 * Codes below 0xF0 are reserved for per-modality interlock slots.    */
#define NP_FAULT_SLOT_NONE          0xFFU  /* no fault */
#define NP_FAULT_SLOT_SIG_FAIL      0xFDU  /* Ed25519 signature verification failed */
#define NP_FAULT_SLOT_UNPROV        0xFEU  /* OTP unprovisioned — all-zero public key */
#define NP_FAULT_SLOT_SIG_CORRUPT   0xFCU  /* repeated corrupt session sig command frames */

/* Session signature escalation limits */
#define NP_SAFETY_SIG_BAD_CMD_MAX   3U  /* consecutive bad-magic/checksum frames → FAULT */
#define NP_SIG_FAIL_MAX             3U  /* consecutive Ed25519 verify failures → hard lock */

#endif /* NP_SAFETY_CONFIG_H */
