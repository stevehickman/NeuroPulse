/*
 * NeurOne Safety MCU — Hardware Configuration
 * Target: STM32G071 (Cortex-M0+, 64 MHz)
 * Document: NP-SW-001 Rev A, NP-FW-EMMC-001 Rev A §4.2
 *
 * GPIO assignments, peripheral config, and timing constants for the
 * safety MCU.  All stimulation enable GPIOs are active-LOW open-drain:
 * LOW = stimulation enabled, HIGH = disabled.  Power loss or reset
 * drives HIGH = safe (stimulation off).
 *
 * GPIO bank assignments are provisional pending PCB layout (G1 gate).
 */

#ifndef NP_SAFETY_CONFIG_H
#define NP_SAFETY_CONFIG_H

#include <stdint.h>

/* CMSIS device header — supplies GPIOA/GPIOB and the peripheral register maps
 * this file names below (SPI1, TIM2, ADC1).  Vendored SOUP: see
 * firmware/vendor/cmsis_device_g0/VERSION and NP-SW-001 §9.5.
 *
 * Gated on STM32G071xx because that define is set by the CROSS build only
 * (firmware/safety_mcu/CMakeLists.txt, below the NP_BUILD_TESTS return), and the
 * host-test build must not pull an ARM CMSIS core header into an x86 compile.
 * That gate is exact rather than convenient: every macro guarded by it —
 * NP_EN_*_PORT, NP_RPEAK_IN_PORT, NP_CARDIAC_TIM, NP_NTC_ADC_INSTANCE,
 * NP_SAFETY_SPI_INSTANCE — is used in np_gpio_mgr.c alone, and np_gpio_mgr.c is
 * in no host-test target.  A host test that starts using one of them fails to
 * compile on the undeclared symbol, which is the loud outcome, not a silent one.
 */
#if defined(STM32G071xx)
#  include "stm32g0xx.h"
#endif

/* ── Clock ────────────────────────────────────────────────────────────────── */

#define NP_SAFETY_SYSCLK_HZ     64000000UL  /* 64 MHz from HSI16 × PLL */

/* ── SPI (SPI1, slave) ────────────────────────────────────────────────────── */
/* SPI1: PA5=SCK, PA6=MISO, PA7=MOSI, PA4=NSS (hardware NSS management)      */
#define NP_SAFETY_SPI_INSTANCE  SPI1
#define NP_SAFETY_FRAME_LEN     8U   /* MCU reply frame size (matches hub_config.h) */

/* ── Watchdog timing ─────────────────────────────────────────────────────── */
#define NP_SAFETY_WDG_TIMEOUT_MS    1500U  /* heartbeat missed → cutoff */
#define NP_SAFETY_HEARTBEAT_EXP_MS  200U   /* expected period from main processor */
#define NP_SAFETY_SYSTICK_HZ        1000U  /* 1ms SysTick resolution */

/* ── Stimulation enable GPIOs (active-LOW open-drain) ────────────────────── */
/* One line per allocated NP_SAFETY_EN_* bit (10 since NP-HW-HUB-001 Rev C
 * §7.2).  GPIO pin numbering is per-port and does NOT track enable-bit
 * position — the mapping is the explicit one in np_gpio_mgr_apply().        */

/* Cranial PBM — PA0, one policy line for the whole lattice (NP-HW-HUB-001
 * Rev C §7.2).  Fanned out on the hub side to one gate transistor per cluster
 * on each cluster's LED drive rail (NP-HW-HEXTILE-001 D-8); §7.4 states the
 * contract as "12–16 physical enable lines fanned out from one policy bit", so
 * the safety MCU spends exactly one GPIO here, not one per cluster.
 *
 * PA1..PA4 are released by this change.  PA4 in particular was DOUBLE-ASSIGNED:
 * the SPI1 block above claims it for hardware NSS, and the retired
 * NP_EN_PBM_ZONE4_PIN also claimed it (NP-HW-HEXTILE-001 OI-HEXTILE-13 note (c),
 * cross-referenced from NP-FMEA-001 FMEA-M08-04).  Retiring the zone-enable
 * macros removes that conflict; PA4 is now SPI1 NSS only.                     */
#define NP_EN_PBM_CRANIAL_PORT  GPIOA
#define NP_EN_PBM_CRANIAL_PIN   (1U << 0)
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
/* ADC1 channels: 5 cranial thermal sense domains + 1 hub NTC.
 * A THERMAL SENSE DOMAIN IS NOT A ZONE.  It is the physical region of the shell
 * an NTC actually senses — a hardware property fixed by where the thermistors
 * are placed, so firmware may hold it (NP-HW-HUB-001 Rev C §4.5.1
 * discriminator).  Zones are authored socket sets in 00-zones.npps and can be
 * re-cut without touching hardware; they are never enable or sense domains.
 * Channel index is NOT a module slot id and NOT an enable-bit position — every
 * cranial domain clears the one NP_SAFETY_EN_PBM_CRANIAL bit (§7.2).          */
#define NP_NTC_CUTOFF_DEG_C     62U   /* junction temperature (not case) */
#define NP_NTC_REARM_DEG_C      55U   /* re-arm below this (7°C hysteresis) after cooldown */
#define NP_NTC_ADC_INSTANCE     ADC1
#define NP_NTC_CHANNEL_COUNT    6U    /* 5 cranial sense domains + 1 hub */
#define NP_NTC_CRANIAL_CHANNELS 5U    /* channels [0, 5) are cranial; 5 is the hub */

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
#define NP_FAULT_SLOT_HUB_NTC      0xFBU  /* hub NTC thermal cutoff (all channels) */

/* Session signature escalation limits */
#define NP_SAFETY_SIG_BAD_CMD_MAX   3U  /* consecutive bad-magic/checksum frames → FAULT */
#define NP_SIG_FAIL_MAX             3U  /* consecutive Ed25519 verify failures → hard lock */

#endif /* NP_SAFETY_CONFIG_H */
