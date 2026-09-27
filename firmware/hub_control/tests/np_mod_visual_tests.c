/*
 * NeurOne Hub Control — Visual Goggle Hall Interrupt Host Tests
 * Document: NP-FW-HUB-001 Rev 18 §8.6 (REQ-FWHUB-21, RISK-FWHUB-09, OI-FWHUB-26)
 *
 * REQ-FWHUB-21 said the goggle Hall cutoff is an interrupt, and RISK-FWHUB-09
 * was Accepted on that control.  No interrupt existed.  The only in-session
 * cutoff was inside np_mod_visual_telemetry(), so a lifted goggle kept emitting
 * until the next telemetry tick.
 *
 * What these tests pin:
 *   1. init() arms the interrupt through np_mod_visual_hal_hall_irq_register();
 *   2. a goggle whose interrupt cannot be armed never emits;
 *   3. the edge alone, with no telemetry call, stops the LEDs and clears the
 *      visual enable request, using only the ISR-safe disable;
 *   4. an edge landing between control()'s seated read and its enable request
 *      does not leave the goggle lit;
 *   5. re-seating never resumes emission, and a new command is admitted;
 *   6. the telemetry backstop still cuts a missed edge.
 *
 * The ISR is reached only through the pointer the driver registers, never by
 * name, so the suite also builds against a driver with no ISR at all.
 *
 * Falsification (NP-CONV-001 §8), 2026-09-27: built against the pre-Rev-18
 * driver (f21cb11:firmware/hub_control/modules/np_mod_visual.c), the suite
 * fails 8 of 13 checks, in cases 1–5.  Three mutants of this driver were each
 * caught: the post-enable latch check removed (case 4, 2 checks); the ISR
 * calling the task-only disable (case 3, 1 check); and the armed check removed
 * (case 2, 2 checks).
 *
 * No FreeRTOS, no hardware.  IEC 62304 Class B — SW-02 hub control.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "np_hub_config.h"
#include "np_hub_types.h"
#include "np_session_runner.h"

/* The doubles below define symbols declared in np_sw02_platform_hal.h, so
 * drift from the production contract is a compile error here too
 * (OI-SWCI-40). */
#include "np_sw02_platform_hal.h"

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

/* ── Driver under test ────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_visual_init(uint8_t slot);
np_hub_status_t np_mod_visual_control(uint8_t slot, const void *params, uint16_t len);
np_hub_status_t np_mod_visual_telemetry(uint8_t slot, np_telem_record_t *out);

/* ── Doubles: a model of the hardware the driver commands ─────────────────────── */

static bool     g_seated;        /* what the Hall element reads */
static bool     g_leds_on;       /* last LED command was a set, not a stop */
static uint16_t g_req_mask;      /* the requested-enable mask, modelled */
static bool     g_in_isr;
static int      g_task_call_in_isr;   /* task-only API called from the ISR */
static np_hub_status_t g_register_rc;
static np_mod_visual_unseated_isr_t g_isr;
static int      g_register_calls;

/* When set, the next led_set() lifts the goggle and fires the edge from inside
 * it, which is the window between control()'s seated read and its enable. */
static bool     g_lift_during_led_set;

static void fire_edge(void)
{
    g_seated = false;
    if (g_isr != NULL) {
        g_in_isr = true;
        g_isr();
        g_in_isr = false;
    }
}

static void reset_doubles(void)
{
    g_seated = true;
    g_leds_on = false;
    g_req_mask = 0U;
    g_in_isr = false;
    g_task_call_in_isr = 0;
    g_register_rc = NP_HUB_OK;
    g_isr = NULL;
    g_register_calls = 0;
    g_lift_during_led_set = false;
}

np_hub_status_t np_mod_visual_hal_hall_irq_register(np_mod_visual_unseated_isr_t on_unseated)
{
    g_register_calls++;
    if (g_register_rc == NP_HUB_OK) { g_isr = on_unseated; }
    return g_register_rc;
}

np_hub_status_t np_mod_visual_hal_led_set(uint16_t zone_mask, uint8_t wl_sel,
                                          uint8_t freq_hz, uint8_t duty_pct)
{
    (void)zone_mask; (void)wl_sel; (void)freq_hz; (void)duty_pct;
    g_leds_on = true;
    if (g_lift_during_led_set) {
        g_lift_during_led_set = false;
        fire_edge();
        g_leds_on = true;   /* the set completes after the ISR returns */
    }
    return NP_HUB_OK;
}

void np_mod_visual_hal_led_stop(void)        { g_leds_on = false; }
bool np_mod_visual_hal_ir_eye_open(void)     { return true; }
bool np_mod_visual_hal_goggle_seated(void)   { return g_seated; }
void np_mod_visual_hal_emdr_set(uint8_t r)   { (void)r; g_leds_on = true; }
bool np_mod_visual_hal_mpe_check(void)       { return true; }

void np_safety_spi_request_enable(uint16_t bits)
{
    if (g_in_isr) { g_task_call_in_isr++; }
    g_req_mask |= bits;
}

void np_safety_spi_request_disable(uint16_t bits)
{
    if (g_in_isr) { g_task_call_in_isr++; }
    g_req_mask &= (uint16_t)~bits;
}

void np_safety_spi_request_disable_from_isr(uint16_t bits)
{
    g_req_mask &= (uint16_t)~bits;
}

void np_runner_abort(np_abort_reason_t reason) { (void)reason; }

/* ── Helpers ──────────────────────────────────────────────────────────────────── */

static np_mod_visual_params_t photic(void)
{
    np_mod_visual_params_t p;
    memset(&p, 0, sizeof(p));
    p.mode = 0U;
    p.freq_hz = 10U;
    p.duty_pct = 50U;
    p.zone_mask_lo = 0xFFU;
    p.zone_mask_hi = 0x0FU;
    return p;
}

static np_hub_status_t start(void)
{
    np_mod_visual_params_t p = photic();
    return np_mod_visual_control(NP_HUB_SLOT_VISUAL, &p, (uint16_t)sizeof(p));
}

static bool emitting(void)
{
    return g_leds_on || (g_req_mask & NP_SAFETY_EN_VISUAL) != 0U;
}

/* ── Tests ────────────────────────────────────────────────────────────────────── */

static void test_init_arms_the_interrupt(void)
{
    reset_doubles();
    (void)np_mod_visual_init(NP_HUB_SLOT_VISUAL);
    check(g_register_calls == 1 && g_isr != NULL,
          "init: registers a non-NULL Hall edge handler");

    check(start() == NP_HUB_OK && emitting(),
          "init: an armed, seated goggle is admitted");
}

static void test_unarmed_goggle_never_emits(void)
{
    reset_doubles();
    g_register_rc = NP_HUB_ERR_GENERIC;
    (void)np_mod_visual_init(NP_HUB_SLOT_VISUAL);

    check(start() == NP_HUB_ERR_SAFETY_REJECTED,
          "unarmed: control() is refused when the interrupt cannot be armed");
    check(!emitting(),
          "unarmed: no LED set and no enable request");
}

static void test_edge_cuts_without_telemetry(void)
{
    reset_doubles();
    (void)np_mod_visual_init(NP_HUB_SLOT_VISUAL);
    (void)start();
    check(emitting(), "edge: precondition, the goggle is emitting");

    fire_edge();   /* no telemetry call follows */

    check(!g_leds_on, "edge: the ISR alone stops the LEDs");
    check((g_req_mask & NP_SAFETY_EN_VISUAL) == 0U,
          "edge: the ISR alone clears the visual enable request");
    check(g_task_call_in_isr == 0,
          "edge: the ISR uses only the ISR-safe disable");
}

static void test_edge_inside_control_window(void)
{
    reset_doubles();
    (void)np_mod_visual_init(NP_HUB_SLOT_VISUAL);

    g_lift_during_led_set = true;
    np_hub_status_t rc = start();

    check(rc == NP_HUB_ERR_SAFETY_REJECTED,
          "window: an edge between the seated read and the enable is reported");
    check(!emitting(),
          "window: the goggle is dark and the enable request is clear afterwards");
}

static void test_reseat_does_not_resume(void)
{
    reset_doubles();
    (void)np_mod_visual_init(NP_HUB_SLOT_VISUAL);
    (void)start();
    fire_edge();
    g_seated = true;   /* lowered again before any telemetry tick */

    np_telem_record_t t;
    memset(&t, 0, sizeof(t));
    (void)np_mod_visual_telemetry(NP_HUB_SLOT_VISUAL, &t);
    check(!emitting(), "reseat: lowering the goggle does not resume emission");

    check(start() == NP_HUB_OK && emitting(),
          "reseat: a new command after re-seating is admitted");
}

static void test_telemetry_backstop(void)
{
    reset_doubles();
    (void)np_mod_visual_init(NP_HUB_SLOT_VISUAL);
    (void)start();
    g_seated = false;   /* lifted, and the edge was missed */

    np_telem_record_t t;
    memset(&t, 0, sizeof(t));
    (void)np_mod_visual_telemetry(NP_HUB_SLOT_VISUAL, &t);
    check(!emitting(), "backstop: telemetry still cuts a missed edge");
}

int main(void)
{
    test_init_arms_the_interrupt();
    test_unarmed_goggle_never_emits();
    test_edge_cuts_without_telemetry();
    test_edge_inside_control_window();
    test_reseat_does_not_resume();
    test_telemetry_backstop();

    if (g_failures != 0) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nAll checks passed\n");
    return 0;
}
