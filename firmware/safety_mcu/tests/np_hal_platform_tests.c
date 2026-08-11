/*
 * NeurOne Safety MCU — SW-01 Platform Layer Host Tests
 * Document: NP-SW-001 §4.1, NP-SW-CI-001 §4.4 (Defect E / OI-SWCI-17, phase 7)
 *
 * The seventh Class C host-test target.  It exists because linking is not
 * enough: phase 7 makes np_safety_mcu.elf a real artifact, which proves the 25
 * np_hal_* symbols EXIST, and proves nothing whatsoever about what they DO.
 *
 * np_safety_hal.h names that gap precisely — the header guarantees the ABI and
 * leaves the behavioural contract as prose, and warns that "a driver that
 * implements np_hal_gpio_write_pin() with 1 = ENABLED has an identical
 * signature, compiles clean, links clean, and inverts every stimulation enable
 * line."  These tests close the specific instances of that gap where getting it
 * backwards enables stimulation, corrupts a cardiac timestamp, or hides an
 * unprovisioned device.
 *
 * THEY COMPILE THE REAL DRIVER SOURCES.  platform/np_hal_gpio.c,
 * np_hal_pin.c, np_hal_spi.c and np_hal_otp.c are built into this binary
 * against the plain-C register file in platform/np_hal_fake_regs.h, so an
 * assertion here is an assertion about the shipped code, not about a mock of
 * it.  Drivers with a hardware handshake (clock, ADC, impedance) are absent on
 * purpose — see the "WHAT IT DOES AND DOES NOT MODEL" note in that header.
 *
 * Build with -DNP_BUILD_TESTS=ON.  No ARM toolchain required.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../platform/np_hal_internal.h"
#include "../include/np_safety_protocol.h"

/* ── Fake register file storage ───────────────────────────────────────────── */
GPIO_TypeDef np_hal_fake_gpioa;
GPIO_TypeDef np_hal_fake_gpiob;
RCC_TypeDef  np_hal_fake_rcc;
SPI_TypeDef  np_hal_fake_spi1;
TIM_TypeDef  np_hal_fake_tim2;
TIM_TypeDef  np_hal_fake_tim3;
ADC_TypeDef  np_hal_fake_adc1;
EXTI_TypeDef np_hal_fake_exti;
uint8_t      np_hal_fake_otp[1024];

/* ── HAL doubles for symbols whose drivers are not in this target ───────────
 * Defined against np_safety_hal.h, so drift from the production contract is a
 * compile error (OI-SWCI-18).                                                */
static uint16_t g_adc_value  = 0U;
static uint32_t g_tick_ms    = 0U;
static uint32_t g_adc_reads  = 0U;   /* proves the tick actually polled */

uint32_t np_hal_get_tick_ms(void) { return g_tick_ms; }

uint16_t np_hal_adc_read_channel(uint8_t channel)
{
    (void)channel;
    g_adc_reads++;
    return g_adc_value;
}

/* ── Unit under test: internal, not part of the 25 ─────────────────────────── */
typedef enum {
    K_NONE = 0, K_HEARTBEAT, K_SIG_CMD, K_CHAN_LIMIT
} kind_t;
extern int np_hal_spi_classify(uint16_t len);

/* Real Class C module linked in to prove the ADC fail-safe DIRECTION. */
extern np_safe_status_t np_thermal_interlock_init(void);
extern void             np_thermal_interlock_tick(np_safety_state_t *state);

/* Mirrors NP_THERMAL_POLL_MS, which np_thermal_interlock.c defines privately in
 * the .c file rather than in np_safety_config.h.  Deliberately NOT hoisted into
 * the header — that would edit the Class C unit under test — following the
 * TEST_RR_BUF_SIZE precedent in np_cardiac_interlock_tests.c.  If the real
 * value ever exceeds this, the rate limiter stops clearing and the g_adc_reads
 * assertions below fail loudly rather than going quietly vacuous. */
#define NP_THERMAL_POLL_MS_MIRROR  10U

/* ── Harness ──────────────────────────────────────────────────────────────── */
static int g_failures = 0;
static void check(int cond, const char *name)
{
    if (cond) { printf("PASS: %s\n", name); }
    else      { printf("FAIL: %s\n", name); g_failures++; }
}

static void regs_reset(void)
{
    memset(&np_hal_fake_gpioa, 0, sizeof(np_hal_fake_gpioa));
    memset(&np_hal_fake_gpiob, 0, sizeof(np_hal_fake_gpiob));
    memset(&np_hal_fake_rcc,   0, sizeof(np_hal_fake_rcc));
    memset(&np_hal_fake_spi1,  0, sizeof(np_hal_fake_spi1));
    memset(&np_hal_fake_adc1,  0, sizeof(np_hal_fake_adc1));
    memset(&np_hal_fake_exti,  0, sizeof(np_hal_fake_exti));
}

/* ── The ten enable lines, mirrored from np_safety_config.h ────────────────── */
static const struct { void *port; uint16_t pin; const char *name; }
k_lines[] = {
    { NP_EN_PBM_CRANIAL_PORT, NP_EN_PBM_CRANIAL_PIN, "PBM_CRANIAL" },
    { NP_EN_BES_PORT,         NP_EN_BES_PIN,         "BES"         },
    { NP_EN_TDCS_PORT,        NP_EN_TDCS_PIN,        "TDCS"        },
    { NP_EN_VNS_PORT,         NP_EN_VNS_PIN,         "VNS"         },
    { NP_EN_VISUAL_PORT,      NP_EN_VISUAL_PIN,      "VISUAL"      },
    { NP_EN_INTRANASAL_PORT,  NP_EN_INTRANASAL_PIN,  "INTRANASAL"  },
    { NP_EN_CVNS_PORT,        NP_EN_CVNS_PIN,        "CVNS"        },
    { NP_EN_TMS_PORT,         NP_EN_TMS_PIN,         "TMS"         },
    { NP_EN_PBM_1170_PORT,    NP_EN_PBM_1170_PIN,    "PBM_1170"    },
    { NP_EN_CLIN_STIM_PORT,   NP_EN_CLIN_STIM_PIN,   "CLIN_STIM"   },
};
#define N_LINES  (sizeof(k_lines) / sizeof(k_lines[0]))

/* ══ 1. POLARITY ═══════════════════════════════════════════════════════════════
 * The single most consequential bit in the platform layer.  Active-LOW:
 * state 1 must SET the pin (HIGH = disabled), state 0 must RESET it (LOW =
 * enabled).  BSRR bits [15:0] set, [31:16] reset.                            */
static void test_write_pin_polarity(void)
{
    size_t i;
    int    all_high_ok = 1;
    int    all_low_ok  = 1;

    for (i = 0U; i < N_LINES; i++) {
        GPIO_TypeDef *p = (GPIO_TypeDef *)k_lines[i].port;

        p->BSRR = 0U;
        np_hal_gpio_write_pin(k_lines[i].port, k_lines[i].pin, 1);
        if (p->BSRR != (uint32_t)k_lines[i].pin) {
            printf("  polarity: %s state=1 wrote BSRR=0x%08lx expected 0x%08lx\n",
                   k_lines[i].name, (unsigned long)p->BSRR,
                   (unsigned long)k_lines[i].pin);
            all_high_ok = 0;
        }

        p->BSRR = 0U;
        np_hal_gpio_write_pin(k_lines[i].port, k_lines[i].pin, 0);
        if (p->BSRR != (((uint32_t)k_lines[i].pin) << 16)) {
            printf("  polarity: %s state=0 wrote BSRR=0x%08lx expected 0x%08lx\n",
                   k_lines[i].name, (unsigned long)p->BSRR,
                   (unsigned long)(((uint32_t)k_lines[i].pin) << 16));
            all_low_ok = 0;
        }
    }
    check(all_high_ok, "write_pin state=1 drives HIGH (stimulation DISABLED) on all 10 lines");
    check(all_low_ok,  "write_pin state=0 drives LOW  (stimulation ENABLED)  on all 10 lines");
}

/* Non-zero must mean HIGH — np_safety_hal.h open question 6.  On an active-LOW
 * line the safe half of the ambiguity is "anything not 0 disables". */
static void test_write_pin_nonzero_is_disabled(void)
{
    GPIO_TypeDef *p = (GPIO_TypeDef *)NP_EN_CVNS_PORT;
    int ok = 1;
    int v;
    const int probes[] = { 2, -1, 255, 0x7FFFFFFF };

    for (v = 0; v < (int)(sizeof(probes) / sizeof(probes[0])); v++) {
        p->BSRR = 0U;
        np_hal_gpio_write_pin(NP_EN_CVNS_PORT, NP_EN_CVNS_PIN, probes[v]);
        if (p->BSRR != (uint32_t)NP_EN_CVNS_PIN) { ok = 0; }
    }
    check(ok, "write_pin: any non-zero state disables (fails toward stimulation OFF)");
}

/* ══ 2. THE RESET WINDOW ═══════════════════════════════════════════════════════
 * The hazard np_hal_gpio.c's header describes: if MODER becomes output while
 * ODR is still 0, every active-LOW enable is asserted until np_gpio_mgr_init()
 * runs.  The driver must preset HIGH via BSRR BEFORE configuring the pin.
 *
 * ── WHY THIS ASSERTION IS POSITIVE AND PORT-SCOPED ──────────────────────────
 * The first draft asserted the NEGATIVE — "BSRR's reset half was never set" —
 * and a mutation that DELETED the preset entirely survived it, because a BSRR
 * that is never written is also a BSRR whose reset half is clear.  The probe
 * could not tell "presets HIGH" from "does nothing".  That is the whole hazard,
 * passing.
 *
 * It is now positive: the preset must be OBSERVED.  The fake is a dumb struct
 * and does not model BSRR→ODR, so only the LAST BSRR write per port survives —
 * which is why the check is expressed per port on the last line of each, rather
 * than per line.  GPIOA carries exactly one enable (PA0, cranial PBM); GPIOB's
 * table order ends at CLIN_STIM.  Deleting the preset leaves both at 0 and both
 * assertions fail, which is verified by mutation rather than assumed.        */
static void test_gpio_init_never_asserts_an_enable(void)
{
    size_t i;
    int    od_ok   = 1;
    int    mode_ok = 1;
    int    low_ok  = 1;

    regs_reset();
    np_hal_gpio_init();

    for (i = 0U; i < N_LINES; i++) {
        GPIO_TypeDef *p   = (GPIO_TypeDef *)k_lines[i].port;
        uint8_t       pos = np_hal_pin_pos(k_lines[i].pin);

        if ((p->OTYPER & (uint32_t)k_lines[i].pin) == 0U) {
            printf("  %s is push-pull, not open-drain\n", k_lines[i].name);
            od_ok = 0;
        }
        if (((p->MODER >> (pos * 2U)) & 3U) != 1U) {
            printf("  %s MODER is not output\n", k_lines[i].name);
            mode_ok = 0;
        }
        if ((p->BSRR & (((uint32_t)k_lines[i].pin) << 16)) != 0U) {
            printf("  %s was RESET (driven LOW = ENABLED) during init\n",
                   k_lines[i].name);
            low_ok = 0;
        }
    }
    check(od_ok,   "gpio_init configures all 10 enables open-drain");
    check(mode_ok, "gpio_init configures all 10 enables as outputs");
    check(low_ok,  "gpio_init never issues a BSRR RESET on an enable line");

    /* The positive half — the preset must actually have happened. */
    check(np_hal_fake_gpioa.BSRR == (uint32_t)NP_EN_PBM_CRANIAL_PIN,
          "gpio_init PRESETS the port-A enable HIGH before configuring it as an output");
    check(np_hal_fake_gpiob.BSRR == (uint32_t)NP_EN_CLIN_STIM_PIN,
          "gpio_init PRESETS the port-B enables HIGH before configuring them as outputs");
}

static void test_gpio_init_enables_port_clocks(void)
{
    regs_reset();
    np_hal_gpio_init();
    check((np_hal_fake_rcc.IOPENR & (RCC_IOPENR_GPIOAEN | RCC_IOPENR_GPIOBEN))
              == (RCC_IOPENR_GPIOAEN | RCC_IOPENR_GPIOBEN),
          "gpio_init enables both GPIO port clocks before configuring pins");
}

/* No enable line may collide with SPI1 (PA4..PA7) or the R-peak input (PA8).
 * np_safety_config.h records that NP_EN_PBM_ZONE4_PIN once double-booked PA4
 * with SPI1 NSS; this keeps that class of defect from returning silently. */
static void test_no_pin_collisions(void)
{
    const uint16_t reserved_a = (uint16_t)((1U << 4) | (1U << 5) | (1U << 6)
                                           | (1U << 7) | (1U << 8));
    size_t   i;
    int      ok = 1;
    uint16_t seen_a = 0U;
    uint16_t seen_b = 0U;

    for (i = 0U; i < N_LINES; i++) {
        if (k_lines[i].port == (void *)GPIOA) {
            if ((k_lines[i].pin & reserved_a) != 0U) {
                printf("  %s collides with SPI1/RPEAK on port A\n", k_lines[i].name);
                ok = 0;
            }
            if ((seen_a & k_lines[i].pin) != 0U) { ok = 0; }
            seen_a = (uint16_t)(seen_a | k_lines[i].pin);
        } else {
            if ((seen_b & k_lines[i].pin) != 0U) { ok = 0; }
            seen_b = (uint16_t)(seen_b | k_lines[i].pin);
        }
    }
    check(ok, "no enable line collides with SPI1/RPEAK or with another enable line");
}

static void test_pin_pos_rejects_malformed_masks(void)
{
    check(np_hal_pin_pos(1U << 0) == 0U,   "pin_pos(1<<0) == 0");
    check(np_hal_pin_pos(1U << 8) == 8U,   "pin_pos(1<<8) == 8");
    check(np_hal_pin_pos(1U << 15) == 15U, "pin_pos(1<<15) == 15");
    check(np_hal_pin_pos(0U) == NP_HAL_PIN_POS_INVALID,
          "pin_pos rejects a zero mask");
    check(np_hal_pin_pos(0x0003U) == NP_HAL_PIN_POS_INVALID,
          "pin_pos rejects a multi-bit mask");
}

/* ══ 3. SPI FRAME CLASSIFICATION ═══════════════════════════════════════════════
 * Transfer LENGTH is the only discriminator between the three frame types
 * (np_safety_hal.h).  A length that matches none must be rejected, not
 * delivered — every consumer memcpy's a fixed-size struct out of the buffer. */
static void test_spi_frame_classification(void)
{
    check(np_hal_spi_classify(NP_SAFETY_RX_EXT_FRAME_LEN)     == K_HEARTBEAT,
          "38 bytes classifies as the extended heartbeat");
    check(np_hal_spi_classify(NP_SAFETY_CMD_FRAME_LEN)        == K_SIG_CMD,
          "102 bytes classifies as the session-signature command");
    check(np_hal_spi_classify(NP_SAFETY_CHAN_LIMIT_FRAME_LEN) == K_CHAN_LIMIT,
          "34 bytes classifies as the per-channel limit command");

    check(np_hal_spi_classify(0U)   == K_NONE, "0 bytes is rejected");
    check(np_hal_spi_classify(8U)   == K_NONE, "8 bytes (the old reply size) is rejected");
    check(np_hal_spi_classify(37U)  == K_NONE, "37 bytes (short heartbeat) is rejected");
    check(np_hal_spi_classify(39U)  == K_NONE, "39 bytes (long heartbeat) is rejected");
    check(np_hal_spi_classify(101U) == K_NONE, "101 bytes (short sig cmd) is rejected");
    check(np_hal_spi_classify(103U) == K_NONE, "103 bytes (overrun-poisoned) is rejected");
}

/* The three lengths must stay mutually distinct — if a wire-format change ever
 * made two equal, length-based demux would silently route one as the other. */
static void test_frame_lengths_are_distinct(void)
{
    check(NP_SAFETY_RX_EXT_FRAME_LEN != NP_SAFETY_CMD_FRAME_LEN
              && NP_SAFETY_RX_EXT_FRAME_LEN != NP_SAFETY_CHAN_LIMIT_FRAME_LEN
              && NP_SAFETY_CMD_FRAME_LEN    != NP_SAFETY_CHAN_LIMIT_FRAME_LEN,
          "the three frame lengths are pairwise distinct (demux is unambiguous)");
}

/* ══ 4. OTP ERASED-STATE TRANSLATION ═══════════════════════════════════════════
 * Blank STM32 OTP reads 0xFF.  The agreed unprovisioned sentinel is all-ZERO.
 * Without translation an unprovisioned device presents as a signature failure
 * instead of an unprovisioned device.                                        */
static void test_otp_blank_reads_as_zero_sentinel(void)
{
    uint8_t key[NP_ED25519_PUB_KEY_LEN];
    uint8_t i;
    int     all_zero = 1;

    memset(np_hal_fake_otp, 0xFF, sizeof(np_hal_fake_otp));
    memset(key, 0xAA, sizeof(key));
    np_hal_otp_read_pubkey(key, (uint8_t)NP_ED25519_PUB_KEY_LEN);

    for (i = 0U; i < NP_ED25519_PUB_KEY_LEN; i++) {
        if (key[i] != 0U) { all_zero = 0; }
    }
    check(all_zero, "erased OTP (all 0xFF) is reported as the all-zero unprovisioned sentinel");
}

static void test_otp_programmed_key_passes_through(void)
{
    uint8_t key[NP_ED25519_PUB_KEY_LEN];
    uint8_t i;
    int     ok = 1;

    for (i = 0U; i < NP_ED25519_PUB_KEY_LEN; i++) {
        np_hal_fake_otp[i] = (uint8_t)(i + 1U);
    }
    memset(key, 0, sizeof(key));
    np_hal_otp_read_pubkey(key, (uint8_t)NP_ED25519_PUB_KEY_LEN);

    for (i = 0U; i < NP_ED25519_PUB_KEY_LEN; i++) {
        if (key[i] != (uint8_t)(i + 1U)) { ok = 0; }
    }
    check(ok, "a programmed OTP key is returned byte-for-byte");
}

/* A PARTIALLY programmed window must NOT be flattened to the unprovisioned
 * sentinel — a half-written key is a corrupt device, not an unbuilt one, and
 * collapsing the two would hide a manufacturing escape. */
static void test_otp_partial_is_not_flattened(void)
{
    uint8_t key[NP_ED25519_PUB_KEY_LEN];

    memset(np_hal_fake_otp, 0xFF, sizeof(np_hal_fake_otp));
    np_hal_fake_otp[7] = 0x42U;                 /* one programmed byte */
    memset(key, 0, sizeof(key));
    np_hal_otp_read_pubkey(key, (uint8_t)NP_ED25519_PUB_KEY_LEN);

    check(key[7] == 0x42U && key[0] == 0xFFU,
          "a partially programmed OTP window is passed through, not flattened to the sentinel");
}

static void test_otp_clamps_len(void)
{
    uint8_t buf[NP_ED25519_PUB_KEY_LEN + 8U];
    uint8_t i;
    int     tail_clean = 1;

    memset(np_hal_fake_otp, 0x5A, sizeof(np_hal_fake_otp));
    memset(buf, 0xEE, sizeof(buf));
    np_hal_otp_read_pubkey(buf, (uint8_t)sizeof(buf));   /* asks for 40 */

    for (i = (uint8_t)NP_ED25519_PUB_KEY_LEN; i < (uint8_t)sizeof(buf); i++) {
        if (buf[i] != 0xEEU) { tail_clean = 0; }
    }
    check(tail_clean,
          "otp_read_pubkey clamps to NP_ED25519_PUB_KEY_LEN and cannot overrun the caller's buffer");
}

/* ══ 5. THE ADC FAIL-SAFE DIRECTION ════════════════════════════════════════════
 * np_hal_adc.c returns 0 on every error path, on the claim that the consumer's
 * descending NTC table maps 0 to ~110 °C and therefore cuts.  That claim is
 * about a static table in ANOTHER translation unit, so it is verified by
 * linking the REAL np_thermal_interlock.c and watching the enable bit.
 *
 * This is the test that fails if k_ntc_adc[] is ever regenerated ascending.
 *
 * ── WHY THE TICK IS ADVANCED, AND WHY THE READ COUNT IS ASSERTED ─────────────
 * np_thermal_interlock_tick() opens with a rate limiter:
 *
 *     now = np_hal_get_tick_ms();
 *     if ((now - s_last_poll_ms) < NP_THERMAL_POLL_MS) { return; }
 *
 * The first draft of this test returned a constant 0 from the tick double, so
 * `0 - 0 = 0 < 10` and the function returned immediately having read no ADC
 * channel at all.  The cut assertion failed — correctly — and, far worse, the
 * full-scale counter-case below PASSED, because "did not cut" is exactly what
 * a function that never ran produces.  A vacuous pass on the control is how a
 * broken table would have slipped through.
 *
 * So the timebase is advanced past NP_THERMAL_POLL_MS, and g_adc_reads is
 * asserted non-zero FIRST: before believing anything about the resulting
 * granted_mask, prove the operation that produces it actually executed. */
static void test_adc_failsafe_value_actually_cuts(void)
{
    np_safety_state_t st;

    memset(&st, 0, sizeof(st));
    st.granted_mask = 0xFFFFU;
    np_thermal_interlock_init();

    g_adc_value = 0U;                     /* NP_HAL_ADC_FAILSAFE_COUNT */
    g_adc_reads = 0U;
    g_tick_ms  += (uint32_t)NP_THERMAL_POLL_MS_MIRROR;   /* clear the rate limiter */
    np_thermal_interlock_tick(&st);

    check(g_adc_reads == NP_NTC_CHANNEL_COUNT,
          "the thermal tick actually polled all 6 NTC channels (not a vacuous pass)");
    check((st.granted_mask & NP_SAFETY_EN_PBM_CRANIAL) == 0U,
          "ADC fail-safe count 0 drives the real thermal interlock to CUT cranial PBM");
    check((st.status & NP_SAFETY_STATUS_THERMAL) != 0U,
          "ADC fail-safe count 0 raises the real THERMAL status bit");
}

/* The counter-case, which is what makes the one above meaningful rather than
 * tautological: a full-scale reading must NOT cut.  If both extremes cut — or
 * if neither branch runs — the test above proves nothing about orientation. */
static void test_adc_full_scale_does_not_cut(void)
{
    np_safety_state_t st;

    memset(&st, 0, sizeof(st));
    st.granted_mask = 0xFFFFU;
    np_thermal_interlock_init();

    g_adc_value = 4095U;                  /* rail high = coldest */
    g_adc_reads = 0U;
    g_tick_ms  += (uint32_t)NP_THERMAL_POLL_MS_MIRROR;
    np_thermal_interlock_tick(&st);

    check(g_adc_reads == NP_NTC_CHANNEL_COUNT,
          "the control case also polled all 6 channels (its pass is not vacuous)");
    check((st.granted_mask & NP_SAFETY_EN_PBM_CRANIAL) != 0U,
          "full-scale ADC does NOT cut — 0 and 4095 are not both 'hot' (table orientation confirmed)");
}

int main(void)
{
    printf("=== SW-01 platform layer host tests (NP-SW-CI-001 phase 7) ===\n");

    test_write_pin_polarity();
    test_write_pin_nonzero_is_disabled();
    test_gpio_init_never_asserts_an_enable();
    test_gpio_init_enables_port_clocks();
    test_no_pin_collisions();
    test_pin_pos_rejects_malformed_masks();

    test_spi_frame_classification();
    test_frame_lengths_are_distinct();

    test_otp_blank_reads_as_zero_sentinel();
    test_otp_programmed_key_passes_through();
    test_otp_partial_is_not_flattened();
    test_otp_clamps_len();

    test_adc_failsafe_value_actually_cuts();
    test_adc_full_scale_does_not_cut();

    printf("=== %s ===\n", (g_failures == 0) ? "ALL PASS" : "FAILURES PRESENT");
    return (g_failures == 0) ? 0 : 1;
}
