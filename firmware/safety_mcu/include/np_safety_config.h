/*
 * NeurOne Safety MCU — Hardware Configuration
 * Target: STM32G071 (Cortex-M0+, 64 MHz)
 * Document: NP-SW-001 Rev 1, NP-FW-EMMC-001 Rev 1 §4.2
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
 * NP_SAFETY_SPI_INSTANCE — is used in np_gpio_mgr.c alone.  A host test that
 * starts using one of them fails to compile on the undeclared symbol, which is
 * the loud outcome, not a silent one.  The one deliberate exception is
 * np_gpio_mgr_tests (NP-FMEA-001 OI-FMEA-11), which force-includes
 * tests/np_gpio_mgr_test_ports.h to give GPIOA/GPIOB opaque host addresses;
 * that header #errors under STM32G071xx, so the cross build cannot see it.
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

/* Hardware independent watchdog (IWDG) — backstop for a hung SAFETY-MCU main
 * loop, which the heartbeat watchdog above cannot see because it runs in that
 * same loop (NP-FMEA-001 FMEA-M02-02/-05, NP-RISK-002 OI-RISK2-05).  Nominal
 * timeout at LSI = 32 kHz; the LSI's 29.5–34 kHz spread puts the real value at
 * ~941–1085 ms.  PROVISIONAL: it must exceed the main loop's worst-case
 * iteration, which includes an Ed25519 verify (np_session_sig_verify) and a
 * 40 ms flash page erase and has never been measured on silicon — measure and
 * tighten under OI-SWCI-49.  It is kept at or under the heartbeat timeout so
 * the backstop is never slower than the primary.                             */
#define NP_SAFETY_IWDG_TIMEOUT_MS   1000U
#define NP_SAFETY_SYSTICK_HZ        1000U  /* 1ms SysTick resolution */

/* Heartbeat sequence gate (NP-FMEA-001 FMEA-M02-03, OI-FMEA-12 (a)).  A
 * well-formed heartbeat resets the watchdog only once the 3-bit counter in
 * session_status (NP_SESSION_STATUS_SEQ_*) has made NP_SAFETY_SEQ_RUN_MIN
 * consecutive forward steps of 1..NP_SAFETY_SEQ_MAX_STEP.  A repeat (step 0)
 * or a backward step (step 4..7) restarts the run.  Why each number:
 *   - MAX_STEP 3 tolerates two lost frames between accepted beats without
 *     restarting the run, and stays under half the modulus (8) so a backward
 *     step can never alias as a forward one.
 *   - RUN_MIN 2 is the smallest run that a two-buffer replay (X, X+1, X, ...)
 *     cannot build: its steps alternate +1 and +7.  RUN_MIN 1 would accept
 *     every second frame of that replay.
 * Cost: after a reset or a restarted run, the first two well-formed beats are
 * not accepted (400 ms at the 200 ms period); the watchdog still fires at
 * NP_SAFETY_WDG_TIMEOUT_MS after the last ACCEPTED beat.                    */
#define NP_SAFETY_SEQ_MAX_STEP      3U
#define NP_SAFETY_SEQ_RUN_MIN       2U

/* Tick liveness (NP-FMEA-001 FMEA-M02-02, OI-FMEA-12 (b)).  Every
 * NP_SAFETY_TICK_CHECK_MS (on either counter) the SysTick millisecond count is
 * compared with TIM2's independent 1 MHz count.  A disagreement larger than
 * NP_SAFETY_TICK_TOL_MS latches an all-channel FAULT (NP_FAULT_SLOT_TICK).
 * Both counters run from SYSCLK, so their ratio is exact by construction and
 * the tolerance only has to absorb 1 ms quantisation and read skew; 10 % of
 * the window catches a stopped SysTick within ~100 ms and a SysTick running
 * more than 10 % slow or fast.  NOT caught: SYSCLK itself slowing, which
 * slows both counters alike (see NP-FMEA-001 FMEA-M02-02).                  */
#define NP_SAFETY_TICK_CHECK_MS     100U
#define NP_SAFETY_TICK_TOL_MS       10U

/* ── Stimulation enable GPIOs (active-LOW open-drain) ────────────────────── */
/* One line per allocated NP_SAFETY_EN_* bit (10 since NP-HW-HUB-001 Rev 3
 * §7.2).  GPIO pin numbering is per-port and does NOT track enable-bit
 * position — the mapping is the explicit one in np_gpio_mgr_apply().        */

/* Cranial PBM — PA0, one policy line for the whole lattice (NP-HW-HUB-001
 * Rev 3 §7.2).  Fanned out on the hub side to one gate transistor per cluster
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
/* R-peak staleness (NP-RISK-002 OI-RISK2-05, principal 2026-09-25).  While
 * cervical VNS is granted, no R-peak edge for this long cuts it exactly as a
 * cardiac event does.  3 s is an R-R interval of 20 BPM — non-physiological for
 * a patient eligible for cervical VNS — so a live rhythm never trips it, and a
 * lost R-peak stream (cable, PPG, or main-processor fault) is caught by Class C
 * code rather than only by the hub's Class B 10 s timer. */
#define NP_CARDIAC_RPEAK_STALE_MS 3000U

/* What a cardiac cutoff blocks (principal, 2026-09-22): ONLY what the interlock
 * exists for.  The cardiac rhythm interlock is specified for cervical VNS
 * (CLAUDE.md §4.2 table; RISK-25, the carotid-sheath baroreceptor reflex), so
 * NP_SAFETY_STATUS_CARDIAC withholds the CVNS enable and nothing else — every
 * other modality that is otherwise safe keeps its grant.  Until 2026-09-22 it
 * sat in np_spi_watchdog_tick()'s all-channel fault mask.  Adding a channel
 * here is a hazard-analysis decision (NP-RISK-002), not a tuning change. */
#define NP_CARDIAC_BLOCK_MASK   (NP_SAFETY_EN_CVNS)

/* ── Non-volatile safety state (NP-SW-FAULTMSG-001 P1, OI-FAULTMSG-01) ──────
 * Two 2 KB pages at the top of flash, outside the image
 * (startup/stm32g071_flash.ld reserves them).  One 64-bit record per slot.   */
#define NP_NV_PAGE_COUNT        2U
#define NP_NV_SLOTS_PER_PAGE    256U        /* 2048 B / 8 B per double-word */
#define NP_NV_FIRST_PAGE        62U         /* flash pages 62 and 63 of 64  */
#define NP_NV_WRITE_ATTEMPTS    3U          /* then NP_FAULT_SLOT_NVSTATE   */
/* Distinct users that can hold an outstanding cutoff at once.  A cutoff for a
 * further user is recorded as NP_SAFETY_USER_ANY — withheld from everyone until
 * acknowledged — so a full table fails closed rather than dropping a cutoff. */
#define NP_NV_MAX_PENDING       8U

/* ── Thermal interlock (SW01-M04) ────────────────────────────────────────── */
/* ADC1 channels: 5 cranial thermal sense domains + 1 hub NTC.
 * A THERMAL SENSE DOMAIN IS NOT A ZONE.  It is the physical region of the shell
 * an NTC actually senses — a hardware property fixed by where the thermistors
 * are placed, so firmware may hold it (NP-HW-HUB-001 Rev 3 §4.5.1
 * discriminator).  Zones are authored socket sets in 00-zones.npps and can be
 * re-cut without touching hardware; they are never enable or sense domains.
 * Channel index is NOT a module slot id and NOT an enable-bit position — every
 * cranial domain clears the one NP_SAFETY_EN_PBM_CRANIAL bit (§7.2).          */
#define NP_NTC_CUTOFF_DEG_C     62U   /* junction temperature (not case) */
#define NP_NTC_REARM_DEG_C      55U   /* re-arm below this (7°C hysteresis) after cooldown */
#define NP_NTC_ADC_INSTANCE     ADC1
#define NP_NTC_CHANNEL_COUNT    6U    /* 5 cranial sense domains + 1 hub */
#define NP_NTC_CRANIAL_CHANNELS 5U    /* channels [0, 5) are cranial; 5 is the hub */

/* NTC sense-node pins and ADC1 input channels, one per sense domain.
 * PROVISIONAL pending PCB layout (G1 gate) — same status as every other GPIO
 * assignment in this file; recorded as OI-SWCI-28.  Added at phase 7 because
 * np_hal_adc.c must name a concrete input per domain and the platform layer is
 * not the place to invent a pin map: it belongs beside the enable lines, where
 * the double-assignment that bit NP_EN_PBM_ZONE4_PIN/PA4 would be visible.
 *
 * Constrained by what is already spoken for: PA0 is the cranial PBM enable,
 * PA4..PA7 are SPI1, PA8 is the R-peak input, PB0..PB8 are the nine remaining
 * enable lines.  The domain index is NOT a module slot and NOT an enable-bit
 * position (see the note above); np_hal_adc.c maps it through a table.        */
#define NP_NTC0_PORT            GPIOA
#define NP_NTC0_PIN             (1U << 1)
#define NP_NTC0_ADC_CH          1U
#define NP_NTC1_PORT            GPIOA
#define NP_NTC1_PIN             (1U << 2)
#define NP_NTC1_ADC_CH          2U
#define NP_NTC2_PORT            GPIOA
#define NP_NTC2_PIN             (1U << 3)
#define NP_NTC2_ADC_CH          3U
#define NP_NTC3_PORT            GPIOB
#define NP_NTC3_PIN             (1U << 10)
#define NP_NTC3_ADC_CH          11U
#define NP_NTC4_PORT            GPIOB
#define NP_NTC4_PIN             (1U << 11)
#define NP_NTC4_ADC_CH          12U
#define NP_NTC5_PORT            GPIOB   /* hub NTC */
#define NP_NTC5_PIN             (1U << 12)
#define NP_NTC5_ADC_CH          16U

/* ── Charge monitor (SW01-M03) — TWO ceilings, one per waveform class ─────── */
/*
 * OI-CHARGE-05 (a)(b)(d).  Until 2026-09-15 this block declared ONE ceiling,
 * 40 µC/cm², and np_charge_monitor.c applied it to a session-cumulative
 * integral of |I|·dt on every electrical channel.  That is two different
 * mistakes at once, and the numeral 40 being plausible in both readings is
 * what hid them:
 *
 *   - 40 µC/cm² is a real and correct ceiling, but it is a PER-PHASE limit for
 *     PULSED stimulation (Shannon 1992; McCreery 1990).  Comparing it against
 *     a quantity integrated over a whole session is a category error, not a
 *     conservative choice: it gives a 35 cm² pad a 1.4 mC budget that 2 mA
 *     exhausts in 0.7 s — less than the 30 s ramp this firmware enforces as a
 *     MINIMUM.
 *   - For the charge-balanced biphasic modalities (BES/tACS, VNS, cervical
 *     VNS, clinical tACS) net delivered charge is ~zero by construction, so a
 *     session integral of |I| is not a physical dose at all.  Per phase is the
 *     quantity that has a damage threshold behind it.
 *
 * So the ceiling splits by waveform class, and each half now has a source.
 * See NP-DT-001 DI-SAFE-01 / DI-SAFE-01a and NP-SW-001 §SW01-M03 for the
 * derivations; the citation chain used to be circular (OI-CHARGE-05 (d)) and
 * is not any more.
 */

/* PULSED / AC channels — charge per PHASE, per electrode.
 * Shannon (1992) / McCreery (1990) pulsed-stimulation charge-density limits
 * for reversible (non-damaging) charge injection.  UNCHANGED IN VALUE: this
 * is the figure the tree has always carried, now applied to the waveform
 * class it is actually about.                                               */
#define NP_CHARGE_PHASE_LIMIT_UC_CM2   40U   /* µC/cm² per phase */

/* DC channels — charge per SESSION, per electrode.
 * 150 mC/cm².  Two anchors, both outside this repository's own assertions:
 *   - Envelope: bounds the conventional large-pad human tDCS literature.  The
 *     most-exposed large RCT protocol in docs/tdcs_database_full.csv is
 *     Brunoni ELECT-TDCS (2013/2017), 2 mA × 30 min on 25 cm² = 144 mC/cm².
 *   - Margin: 35× below Liebetanz et al. (2009), whose measured rat epicranial
 *     DC lesion threshold is 52,400 C/m² = 5,240 mC/cm².  That is the only
 *     MEASURED damage threshold for DC anywhere in the evidence base.
 * The routine clinical protocol 2 mA × 20 min on a 35 cm² pad is 68.6 mC/cm²
 * and is therefore available, which at the retired 40-in-the-wrong-unit
 * ceiling it was not.  PROVISIONAL pending Regulatory sign-off — see
 * OI-CHARGE-06.                                                             */
#define NP_CHARGE_DC_LIMIT_MC_CM2     150U   /* mC/cm² per session */

/* Default electrode area for a channel that declares no geometry: 25 cm².
 * Unchanged, and still never what tDCS or HD-tDCS actually runs against —
 * OI-CHARGE-03/-04's geometry gate holds those channels OFF rather than
 * letting them fall back here.                                              */
#define NP_ELECTRODE_AREA_CM2       25U   /* default electrode area */

/* Per-electrode per-phase budget at the default geometry (µC).             */
#define NP_CHARGE_PHASE_LIMIT_UC \
    (NP_CHARGE_PHASE_LIMIT_UC_CM2 * NP_ELECTRODE_AREA_CM2)

/* ── Impedance check (SW01-M06) ──────────────────────────────────────────── */
/* 1 kHz AC test current injected for 50ms; reject if Zmeasured > 10kΩ.     */
#define NP_IMPEDANCE_TEST_HZ    1000U
#define NP_IMPEDANCE_TEST_MS    50U
#define NP_IMPEDANCE_MAX_OHM    10000U  /* above this → contacts not confirmed */

/* Minimum impedance floor per checked channel (NP-FMEA-001 FMEA-M06-02,
 * OI-FMEA-13 (ii)).  A reading BELOW the floor fails the check exactly as one
 * above NP_IMPEDANCE_MAX_OHM does.  0 Ω is physically impossible for any skin
 * electrode, and it is what np_hal_impedance_read_ohm() returns when the sense
 * node sits at ADC full scale (a shorted sense leg or a stuck converter), so
 * without a floor an AFE fault reads as a perfect contact and grants.
 *
 *   VNS_HRV, CVNS  500 Ω — the auricular contact floor the hub already applies
 *                  (VNS_CONTACT_MIN_OHM, np_mod_vns.c) and the cervical gel
 *                  minimum FMEA-M06-02's mitigation names.
 *   tDCS, BES_TACS 1 Ω  — rejects 0 Ω only.  No tES floor is derivable from
 *                  anything in this repository, and a guessed one would refuse
 *                  well-wetted pads; the derived floor is OI-FMEA-14.
 *
 * Like the maximum, these are read through the uncalibrated reference leg
 * (OI-SWCI-34).  Indexed by impedance-check channel, 0..3.                  */
#define NP_IMPEDANCE_MIN_OHM_VNS_HRV   500U
#define NP_IMPEDANCE_MIN_OHM_TDCS      1U
#define NP_IMPEDANCE_MIN_OHM_BES_TACS  1U
#define NP_IMPEDANCE_MIN_OHM_CVNS      500U

/* Impedance analog front end — ALL PROVISIONAL, recorded as OI-SWCI-34.
 *
 * Unlike SPI1/TIM2/ADC1/GPIO, which this file already named before phase 7,
 * NOTHING in this repository specifies the impedance excitation source, the
 * sense amplifier, the reference leg or their pins.  np_hal_impedance.c needs
 * concrete names to be a driver rather than a stub, so they are declared here
 * — beside the other provisional assignments and clearly marked — instead of
 * being buried in the platform layer where the analog review would not find
 * them.  NP_IMP_SENSE_R_OHM in particular is an UNCALIBRATED placeholder: the
 * ohms np_hal_impedance_read_ohm() returns do not rest on any bench
 * measurement.  The FAIL-SAFE DIRECTION (errors report an impedance above
 * NP_IMPEDANCE_MAX_OHM, refusing the enable) holds regardless of calibration. */
#define NP_IMP_EXC_TIM          TIM3    /* 1 kHz excitation source */
#define NP_IMP_SENSE_PORT       GPIOA
#define NP_IMP_SENSE_R_OHM      10000U  /* reference leg — UNCALIBRATED */
#define NP_IMP0_ADC_CH          4U      /* VNS_HRV  */
#define NP_IMP1_ADC_CH          5U      /* tDCS     */
#define NP_IMP2_ADC_CH          6U      /* BES_TACS */
#define NP_IMP3_ADC_CH          7U      /* CVNS     */
#define NP_IMP_CVNS_L_ADC_CH    8U      /* CVNS electrode 0 = left  */
#define NP_IMP_CVNS_R_ADC_CH    9U      /* CVNS electrode 1 = right */

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
#define NP_FAULT_SLOT_NVSTATE     0xFAU  /* non-volatile safety state could not be written */
#define NP_FAULT_SLOT_TICK        0xF9U  /* SysTick disagrees with TIM2 (OI-FMEA-12 (b)) */

/* ── Tier identity (SW01-M10, NP-REG-UPG-001 §7.5, OI-UPG-01) ────────────── */
/*
 * The signed tier-identity record, written ONCE into OTP at manufacture
 * (REQ-UPG-02).  OTP on this part can be programmed but never erased, and this
 * firmware contains no OTP write path at all, so no field route — app, OTA,
 * service partner, depot — can write or change it.
 *
 * Placement: OTP offset 0x40, immediately after the 32-byte root session key at
 * offset 0 and 8-byte aligned, because STM32G0 OTP programs in 64-bit double
 * words.  Like the root key's offset (OI-SWCI-30) this is a production-
 * programming contract, not a silicon fact.
 *
 * Record (72 bytes = 9 double words):
 *   [0..3]   magic    'N' 'P' 'T' 'I'
 *   [4]      version  NP_TIER_RECORD_VERSION
 *   [5]      tier     NP_TIER_T1 / NP_TIER_T2  (np_spi_wire_types.h)
 *   [6..7]   reserved 0x00 0x00
 *   [8..71]  Ed25519 signature by the tier authority over
 *            NP_TIER_SIG_DOMAIN (16) || record[0..7] (8) || device UID (12)
 *
 * The device UID in the signed message is what makes the record DEVICE-BOUND
 * (NP-PWRSRC-001 D-9): a T2 record copied into another unit's OTP names the
 * wrong UID and fails verification there.  The domain string keeps a tier
 * signature from ever verifying as any other signed object in the programme.
 */
#define NP_TIER_OTP_OFFSET          0x40U
#define NP_TIER_RECORD_LEN          72U
#define NP_TIER_RECORD_BODY_LEN     8U     /* bytes [0..7] — the signed fields */
#define NP_TIER_RECORD_VERSION      0x01U
#define NP_TIER_RECORD_MAGIC_0      0x4EU  /* 'N' */
#define NP_TIER_RECORD_MAGIC_1      0x50U  /* 'P' */
#define NP_TIER_RECORD_MAGIC_2      0x54U  /* 'T' */
#define NP_TIER_RECORD_MAGIC_3      0x49U  /* 'I' */
#define NP_TIER_SIG_DOMAIN          "NeurOne.TierId.1"
#define NP_TIER_SIG_DOMAIN_LEN      16U
#define NP_DEVICE_UID_LEN           12U    /* STM32G0 96-bit unique device ID */
#define NP_TIER_SIG_MSG_LEN \
    (NP_TIER_SIG_DOMAIN_LEN + NP_TIER_RECORD_BODY_LEN + NP_DEVICE_UID_LEN)

/*
 * The tier authority's Ed25519 public key, compiled into the signed safety
 * image.  NOT the OTP root session key: that key's holder signs every session
 * descriptor, and a party able to sign sessions must not thereby be able to
 * sign a tier.  Compiled in rather than read from OTP, so that nothing on an
 * unprovisioned unit's OTP can nominate its own authority.
 *
 * ALL-ZERO IS A PLACEHOLDER, AND IT FAILS CLOSED: with no authority key, no
 * record verifies and every unit is T1 (reason NP_TIER_REASON_NO_AUTHORITY).
 * The key is generated at a manufacturing key ceremony that has not happened
 * (OI-UPG-08); a T2 image cannot be released until it has.  Overridable at
 * compile time so the host test can use its own key pair.
 */
#ifndef NP_TIER_AUTHORITY_PUBKEY_INIT
#define NP_TIER_AUTHORITY_PUBKEY_INIT { \
    0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, \
    0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U }
#endif

/* Session signature escalation limits */
#define NP_SAFETY_SIG_BAD_CMD_MAX   3U  /* consecutive bad-magic/checksum frames → FAULT */
#define NP_SIG_FAIL_MAX             3U  /* consecutive Ed25519 verify failures → hard lock */

#endif /* NP_SAFETY_CONFIG_H */
