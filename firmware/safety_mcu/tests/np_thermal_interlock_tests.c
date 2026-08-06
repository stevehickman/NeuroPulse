/*
 * NeurOne Safety MCU — SW01-M04 Thermal Interlock Host Tests
 * Document: NP-SW-001 Rev A §SW01-M04, NP-HW-HUB-001 Rev C §7.2
 *
 * Why this file exists.  np_thermal_interlock.c is compiled only into the ARM
 * cross target, so nothing executable covered it.  It also contained the one
 * place where an NTC channel index was used as an enable-BIT POSITION:
 *
 *     state->granted_mask &= ~(1U << ch);        // ch = 0..4
 *
 * That was correct only while bits 0–4 were the five per-zone PBM enables.
 * After the §7.2 collapse to a single NP_SAFETY_EN_PBM_CRANIAL at bit 0, the
 * same expression would clear RESERVED bits 1–4 for sense domains 1–4 — cutting
 * nothing at all while reporting a thermal cutoff.  An over-temp on four of the
 * five cranial sense domains would have left the emitters energised.
 *
 * The decisive test below is test_every_cranial_domain_cuts_pbm: it fails
 * against the pre-§7.2 expression for domains 1–4 and passes against the fix.
 *
 * IEC 62304 Class C — SW-01 safety MCU.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../include/np_safety_config.h"
#include "../include/np_safety_protocol.h"

/* ── Unit under test ─────────────────────────────────────────────────────────── */
extern np_safe_status_t np_thermal_interlock_init(void);
extern void             np_thermal_interlock_tick(np_safety_state_t *state);

/* ── HAL stubs ───────────────────────────────────────────────────────────────── */

static uint32_t s_now_ms;
static uint16_t s_adc[NP_NTC_CHANNEL_COUNT];

uint32_t np_hal_get_tick_ms(void)
{
    return s_now_ms;
}

uint16_t np_hal_adc_read_channel(uint8_t channel)
{
    return (channel < NP_NTC_CHANNEL_COUNT) ? s_adc[channel] : 0U;
}

/* ADC counts for the 10kΩ B=3950K divider in np_thermal_interlock.c.
 * 660 counts → 62 °C (at the NP_NTC_CUTOFF_DEG_C threshold);
 * 900 counts → 53 °C (at or below NP_NTC_REARM_DEG_C = 55).            */
#define ADC_HOT   660U
#define ADC_COOL  900U

static int g_failures = 0;

static void check(int cond, const char *name)
{
    if (cond) {
        printf("PASS: %s\n", name);
    } else {
        printf("FAIL: %s\n", name);
        g_failures++;
    }
}

/* Advance past NP_THERMAL_POLL_MS (10ms) so the tick actually samples. */
static void tick(np_safety_state_t *state)
{
    s_now_ms += 20U;
    np_thermal_interlock_tick(state);
}

static np_safety_state_t fresh(void)
{
    np_safety_state_t s;
    memset(&s, 0, sizeof(s));
    s.requested_mask = NP_SAFETY_EN_ALL_MASK;
    s.granted_mask   = NP_SAFETY_EN_ALL_MASK;
    s.fault_slot     = NP_FAULT_SLOT_NONE;
    s.session_active = true;
    return s;
}

static void all_cool(void)
{
    uint8_t i;
    for (i = 0U; i < NP_NTC_CHANNEL_COUNT; i++) {
        s_adc[i] = ADC_COOL;
    }
}

static void reset_module(void)
{
    s_now_ms = 0U;
    all_cool();
    (void)np_thermal_interlock_init();
}

/* ── Test: a cool device grants everything and latches nothing ──────────────── */

static void test_cool_is_quiet(void)
{
    reset_module();
    np_safety_state_t s = fresh();
    tick(&s);

    check(s.granted_mask == NP_SAFETY_EN_ALL_MASK,
          "all channels cool: granted_mask untouched");
    check((s.status & NP_SAFETY_STATUS_THERMAL) == 0U,
          "all channels cool: THERMAL not set");
    check(s.fault_slot == NP_FAULT_SLOT_NONE,
          "all channels cool: fault_slot stays NONE");
}

/* ── Test (decisive): EVERY cranial sense domain cuts cranial PBM ───────────────
 *
 * Domains 1–4 are the regression.  Under the retired `~(1U << ch)` expression
 * these cleared reserved bits 1–4 — a no-op — leaving NP_SAFETY_EN_PBM_CRANIAL
 * set while the status byte claimed a thermal cutoff.
 */

static void test_every_cranial_domain_cuts_pbm(void)
{
    uint8_t domain;
    for (domain = 0U; domain < NP_NTC_CRANIAL_CHANNELS; domain++) {
        char name[96];

        reset_module();
        np_safety_state_t s = fresh();

        s_adc[domain] = ADC_HOT;
        tick(&s);

        (void)snprintf(name, sizeof(name),
                       "cranial domain %u over-temp clears EN_PBM_CRANIAL", domain);
        check((s.granted_mask & NP_SAFETY_EN_PBM_CRANIAL) == 0U, name);

        (void)snprintf(name, sizeof(name),
                       "cranial domain %u over-temp sets THERMAL+CUTOFF", domain);
        check((s.status & (NP_SAFETY_STATUS_THERMAL | NP_SAFETY_STATUS_CUTOFF)) ==
                  (NP_SAFETY_STATUS_THERMAL | NP_SAFETY_STATUS_CUTOFF), name);

        (void)snprintf(name, sizeof(name),
                       "cranial domain %u reported in fault_slot", domain);
        check(s.fault_slot == domain, name);

        /* Reserved bits must never appear in granted_mask as a side effect. */
        (void)snprintf(name, sizeof(name),
                       "cranial domain %u leaves reserved bits 1-4 clear", domain);
        check((s.granted_mask & 0x001EU) == 0U, name);
    }
}

/* ── Test: a cranial cut is scoped to PBM, not to every modality ────────────── */

static void test_cranial_cut_spares_other_modalities(void)
{
    reset_module();
    np_safety_state_t s = fresh();

    s_adc[2] = ADC_HOT;
    tick(&s);

    check((s.granted_mask & NP_SAFETY_EN_PBM_CRANIAL) == 0U,
          "cranial over-temp: PBM cut");
    check((s.granted_mask & NP_SAFETY_EN_CVNS) != 0U,
          "cranial over-temp: CVNS untouched");
    check((s.granted_mask & NP_SAFETY_EN_CLIN_STIM) != 0U,
          "cranial over-temp: CLIN_STIM untouched");
    check((s.granted_mask & NP_SAFETY_EN_PBM_1170NM) != 0U,
          "cranial over-temp: 1170nm laser is its own bit, untouched");
}

/* ── Test: the hub NTC still cuts everything ────────────────────────────────── */

static void test_hub_ntc_cuts_all(void)
{
    reset_module();
    np_safety_state_t s = fresh();

    s_adc[NP_NTC_CRANIAL_CHANNELS] = ADC_HOT;   /* channel 5 = hub */
    tick(&s);

    check(s.granted_mask == 0U, "hub NTC over-temp: granted_mask cleared entirely");
    check(s.fault_slot == NP_FAULT_SLOT_HUB_NTC,
          "hub NTC over-temp: fault_slot == NP_FAULT_SLOT_HUB_NTC");
}

/* ── Test: hysteresis — THERMAL clears only when every domain has cooled ────── */

static void test_rearm_requires_all_domains_cool(void)
{
    reset_module();
    np_safety_state_t s = fresh();

    s_adc[0] = ADC_HOT;
    s_adc[3] = ADC_HOT;
    tick(&s);
    check((s.status & NP_SAFETY_STATUS_THERMAL) != 0U,
          "two domains hot: THERMAL set");

    /* One cools; the other is still latched. */
    s_adc[0] = ADC_COOL;
    tick(&s);
    check((s.status & NP_SAFETY_STATUS_THERMAL) != 0U,
          "one of two domains cooled: THERMAL still asserted");

    /* Both cool: the module releases. */
    s_adc[3] = ADC_COOL;
    tick(&s);
    check((s.status & NP_SAFETY_STATUS_THERMAL) == 0U,
          "both domains cooled: THERMAL released");
}

/* ── Test: polling is rate-limited to NP_THERMAL_POLL_MS ────────────────────── */

static void test_poll_rate_limited(void)
{
    reset_module();
    np_safety_state_t s = fresh();

    s_adc[1] = ADC_HOT;
    s_now_ms += 5U;                  /* under the 10ms poll interval */
    np_thermal_interlock_tick(&s);

    check((s.granted_mask & NP_SAFETY_EN_PBM_CRANIAL) != 0U,
          "tick inside poll interval does not sample");

    tick(&s);                        /* now past the interval */
    check((s.granted_mask & NP_SAFETY_EN_PBM_CRANIAL) == 0U,
          "tick past poll interval samples and cuts");
}

int main(void)
{
    printf("=== np_thermal_interlock_tests ===\n");

    test_cool_is_quiet();
    test_every_cranial_domain_cuts_pbm();
    test_cranial_cut_spares_other_modalities();
    test_hub_ntc_cuts_all();
    test_rearm_requires_all_domains_cool();
    test_poll_rate_limited();

    printf("\n%s: %d failure(s)\n",
           (g_failures == 0) ? "PASS" : "FAIL", g_failures);
    return (g_failures == 0) ? 0 : 1;
}
