/*
 * NeurOne Zone Module Announcement — Configuration Constants
 * Document: NP-FW-ZA-001 Rev 1 §3
 * Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
 *
 * This header defines all compile-time constants for the zone module
 * insertion detection and bone conduction announcement subsystem.
 *
 * Hardware context (from NP-HW-FPC-001 Rev 4):
 *   Pin 18 = ZONE_ID: 1% resistor in module (R_Z) to GND; 10 kΩ pull-up
 *   (R_PU) to 3.3 V on the hub PCB forms a voltage divider read by LPADC.
 *
 *   Vout = 3.3 V × R_Z / (R_PU + R_Z)
 *
 *   No module (open circuit): Vout ≈ 3.3 V (pulled high) → ADC ≈ 4095
 *
 * ZONE_ID debounce (RISK-18, docs/reference/durability-maintenance.md §7.1):
 *   3× ADC reads at 100 ms intervals; ≥ 2/3 reads must agree.
 */

#ifndef NP_ZONE_ANNOUNCE_CONFIG_H
#define NP_ZONE_ANNOUNCE_CONFIG_H

/* ── LPADC hardware ─────────────────────────────────────────────────────────── */

/* i.MX RT1062 LPADC1 base address and channel for ZONE_ID (pin 18 of FPC ZIF). */
#define NP_ZA_LPADC_BASE            0x40050000UL   /* LPADC1                     */
#define NP_ZA_LPADC_CHANNEL         12U            /* single-ended, side A       */
#define NP_ZA_ADC_RESOLUTION_BITS   12U
#define NP_ZA_ADC_MAX               4095U          /* 2^12 - 1                   */
#define NP_ZA_ADC_VREF_MV           3300U          /* millivolts                 */
#define NP_ZA_ADC_PULLUP_OHMS       10000U         /* 10 kΩ on hub PCB           */

/* ── ADC zone classification thresholds (midpoints between nominal values) ──── */
/*
 * Nominal ADC counts (R_PU = 10 kΩ, Vref = 3.3 V, 12-bit ADC):
 *   ZM-01  10 kΩ → Vout = 1.650 V → 2048 counts
 *   ZM-02  22 kΩ → Vout = 2.269 V → 2818 counts
 *   ZM-03  47 kΩ → Vout = 2.719 V → 3378 counts
 *   ZM-04 100 kΩ → Vout = 3.000 V → 3723 counts
 *   ZM-05 220 kΩ → Vout = 3.157 V → 3918 counts
 *   None  (open) → Vout ≈ 3.300 V → 4095 counts
 *
 * Thresholds are set at the midpoint between adjacent nominals.
 */
#define NP_ZA_ADC_ZM01_LO          1024U   /* below → UNKNOWN (wiring fault)    */
#define NP_ZA_ADC_ZM01_HI          2433U
#define NP_ZA_ADC_ZM02_LO          2434U
#define NP_ZA_ADC_ZM02_HI          3098U
#define NP_ZA_ADC_ZM03_LO          3099U
#define NP_ZA_ADC_ZM03_HI          3550U
#define NP_ZA_ADC_ZM04_LO          3551U
#define NP_ZA_ADC_ZM04_HI          3820U
#define NP_ZA_ADC_ZM05_LO          3821U
#define NP_ZA_ADC_ZM05_HI          3999U
#define NP_ZA_ADC_NO_MODULE_LO     4000U   /* ≥ this → no module present        */

/* ── Debounce timing (durability-maintenance.md §7.1 / RISK-18) ────────────── */
#define NP_ZA_DEBOUNCE_READS        3U     /* total ADC reads per debounce cycle */
#define NP_ZA_DEBOUNCE_MAJORITY     2U     /* minimum agreeing reads to pass     */
#define NP_ZA_DEBOUNCE_INTERVAL_MS  100U   /* ms between successive reads        */

/* Minimum insertion hold time before debounce begins (contact settle). */
#define NP_ZA_INSERTION_SETTLE_MS   20U

/* Module must be absent for this long before a re-insertion is detected. */
#define NP_ZA_REMOVAL_DEBOUNCE_MS   50U

/* ── Bone conduction audio output ────────────────────────────────────────────── */

/* SAI3 used for bone conduction piezo (SAI1/SAI2 reserved for planar magnetic). */
#define NP_ZA_SAI_BASE              0x4038C000UL   /* SAI3                       */
#define NP_ZA_SAI_DMA_CHANNEL       5U

/* Audio parameters. */
#define NP_ZA_AUDIO_SAMPLE_RATE_HZ  8000U   /* 8 kHz mono — adequate for tones  */
#define NP_ZA_AUDIO_BITS_PER_SAMPLE 16U
#define NP_ZA_AUDIO_CHANNELS        1U      /* mono; I2S frame padded to stereo  */

/* DDS sine LUT size (must be power of 2). */
#define NP_ZA_SINE_LUT_SIZE         256U
#define NP_ZA_SINE_LUT_SHIFT        24U     /* top 8 bits of 32-bit accumulator  */

/* Peak amplitude for bone conduction output (Q15: 0x7FFF = 0 dBFS). */
#define NP_ZA_AUDIO_AMPLITUDE_Q15   0x4000  /* −6 dBFS — comfortable bone cond. */

/* Amplitude ramp duration on start and end (milliseconds). */
#define NP_ZA_AUDIO_RAMP_MS         20U

/* DMA ping-pong buffer depth (samples per half-buffer). */
#define NP_ZA_DMA_HALF_BUF_SAMPLES  64U
#define NP_ZA_DMA_BUF_SAMPLES       (NP_ZA_DMA_HALF_BUF_SAMPLES * 2U)

/* ── Zone tone sequence parameters ──────────────────────────────────────────── */

/* Maximum tone segments in one announcement clip. */
#define NP_ZA_MAX_TONE_SEGMENTS     12U

/* Beep tone frequencies (Hz). */
#define NP_ZA_TONE_BEEP_HZ          440U    /* A4 — zone position pulse          */
#define NP_ZA_TONE_CONFIRM_HZ       880U    /* A5 — "connected" confirmation     */
#define NP_ZA_TONE_ERROR_LO_HZ      200U    /* low tone for unknown-zone fault   */
#define NP_ZA_TONE_ERROR_HI_HZ      150U    /* descending error pair             */

/* Beep / gap durations (ms). */
#define NP_ZA_BEEP_DURATION_MS      120U    /* zone-position pulse on-time       */
#define NP_ZA_BEEP_GAP_MS           80U     /* gap between zone-position pulses  */
#define NP_ZA_PRE_CONFIRM_GAP_MS    180U    /* gap before confirmation tone      */
#define NP_ZA_CONFIRM_DURATION_MS   250U    /* duration of confirmation tone     */
#define NP_ZA_ERROR_DURATION_MS     300U    /* duration of each error tone       */
#define NP_ZA_ERROR_GAP_MS          100U

/* ── SHDR logging keys ───────────────────────────────────────────────────────── */

/* Written to SHDR partition: accessory authentication pass/fail per insertion   */
/* (listed in NP-FW-EMMC-001 Rev 1 §12 session data classification table).      */
#define NP_ZA_SHDR_AUTH_KEY         "zm_auth_v1"

/* ── Polling / task timing ───────────────────────────────────────────────────── */

/* How often the zone announce task polls the ADC while idle. */
#define NP_ZA_POLL_IDLE_MS          50U

/* How often it polls during debounce (overridden by NP_ZA_DEBOUNCE_INTERVAL_MS).*/
#define NP_ZA_POLL_DEBOUNCE_MS      NP_ZA_DEBOUNCE_INTERVAL_MS

/* FreeRTOS task stack and priority. */
#define NP_ZA_TASK_STACK_WORDS      512U
#define NP_ZA_TASK_PRIORITY         3U      /* below HRV (5) and EEG (6)         */

#endif /* NP_ZONE_ANNOUNCE_CONFIG_H */
