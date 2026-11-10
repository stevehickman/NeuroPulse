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
/* @verifies-file REQ-FWHUB-21 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "np_hub_config.h"
#include "np_hub_types.h"
#include "np_session_runner.h"
#include "np_mod_visual.h"
#include "np_ppx_detect.h"
#include "np_ppx_policy.h"

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
static bool     g_mpe_ok = true; /* what the exposure check reads (OI-SIMFW-47) */
static bool     g_leds_on;       /* last LED command was a set, not a stop */
static uint16_t g_req_mask;      /* the requested-enable mask, modelled */
static bool     g_in_isr;
static bool     g_eye_open = true;   /* what the IR proximity sensor reads */
static int      g_led_sets;          /* led_set() calls since reset */
static np_hub_status_t g_led_rc;     /* what led_set() answers (OI-SIMFW-20) */
static int      g_emdr_sets;         /* emdr_set() calls since reset */
static uint8_t  g_last_wl, g_last_freq, g_last_duty;
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
    g_mpe_ok = true;
    g_seated = true;
    g_leds_on = false;
    g_req_mask = 0U;
    g_in_isr = false;
    g_task_call_in_isr = 0;
    g_register_rc = NP_HUB_OK;
    g_isr = NULL;
    g_register_calls = 0;
    g_lift_during_led_set = false;
    g_eye_open = true;
    g_led_sets = 0;
    g_led_rc = NP_HUB_OK;
    g_emdr_sets = 0;
    g_last_wl = g_last_freq = g_last_duty = 0U;
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
    (void)zone_mask;
    g_led_sets++;
    g_last_wl = wl_sel; g_last_freq = freq_hz; g_last_duty = duty_pct;
    g_leds_on = true;
    if (g_lift_during_led_set) {
        g_lift_during_led_set = false;
        fire_edge();
        g_leds_on = true;   /* the set completes after the ISR returns */
    }
    /* A failed command is modelled as the worst case: it may have lit the LEDs before it reported the error, so only the
     * driver's own stop leaves them dark (OI-SIMFW-20). */
    return g_led_rc;
}

void np_mod_visual_hal_led_stop(void)        { g_leds_on = false; }
bool np_mod_visual_hal_ir_eye_open(void)     { return g_eye_open; }
bool np_mod_visual_hal_goggle_seated(void)   { return g_seated; }
void np_mod_visual_hal_emdr_set(uint8_t r)   { (void)r; g_emdr_sets++; g_leds_on = true; }
bool np_mod_visual_hal_mpe_check(void)       { return g_mpe_ok; }

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
static int g_panics;
static np_hold_cause_t g_last_cause;
np_hub_status_t np_runner_halt(np_hold_cause_t cause) { g_panics++; g_last_cause = cause; return NP_HUB_OK; }
np_hub_status_t np_runner_panic(void) { return np_runner_halt(NP_HOLD_USER_PANIC); }

/* ── Helpers ──────────────────────────────────────────────────────────────────── */

static np_mod_visual_params_t photic(void)
{
    np_mod_visual_params_t p;
    memset(&p, 0, sizeof(p));
    p.mode = 0U;
    p.freq_hz = 40U;      /* outside the photoparoxysmal zone (OI-FWHUB-29): 3-30 Hz is refused */
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

    check(start() == NP_HUB_ERR_GOGGLE_LIFT_UNARMED,
          "unarmed: control() is refused when the interrupt cannot be armed, as its own status");
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

    check(rc == NP_HUB_ERR_GOGGLES_NOT_SEATED,
          "window: an edge between the seated read and the enable is reported as a goggle not seated");
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

/* REQ-FWHUB-22 / DI-SAFE-11: Mode F is compiled out while
 * NP_MODE_F_REGULATORY_CLEARED is 0, so a command asking for it is run as the
 * plain photic command it also is, and nothing extra is emitted. */
/* @verifies REQ-FWHUB-22 */
/* @verifies DI-SAFE-11#a */
static void test_mode_f_is_compiled_out(void)
{
    check(NP_MODE_F_REGULATORY_CLEARED == 0, "mode F: the build flag is 0");

    reset_doubles();
    (void)np_mod_visual_init(NP_HUB_SLOT_VISUAL);
    np_mod_visual_params_t p = photic();
    p.mode_f_enable = 1U;
    check(np_mod_visual_control(NP_HUB_SLOT_VISUAL, &p, (uint16_t)sizeof(p)) == NP_HUB_OK,
          "mode F: a photic command that also asks for mode F is admitted as photic");
    check(g_led_sets == 1,
          "mode F: exactly one LED command, the photic one, so no NIR walk is added");
    check(g_last_wl == p.wl_select && g_last_freq == p.freq_hz && g_last_duty == p.duty_pct,
          "mode F: that command carries the authored wavelength, frequency and duty");

    reset_doubles();
    (void)np_mod_visual_init(NP_HUB_SLOT_VISUAL);
    g_eye_open = false;
    p = photic();
    p.mode_f_enable = 1U;
    check(np_mod_visual_control(NP_HUB_SLOT_VISUAL, &p, (uint16_t)sizeof(p)) == NP_HUB_OK,
          "mode F: the flag alone does not invoke the retinal eye-open check");

    p = photic();
    p.mode = 2U;               /* the dedicated retinal PBM mode keeps its check */
    check(np_mod_visual_control(NP_HUB_SLOT_VISUAL, &p, (uint16_t)sizeof(p))
              == NP_HUB_ERR_EYE_NOT_OPEN,
          "mode F: retinal mode 2 is still refused with the eye closed, as its own status");
}

static np_hub_status_t start_with(uint8_t mode, uint8_t freq_hz)
{
    np_mod_visual_params_t p = photic();
    p.mode = mode;
    p.freq_hz = freq_hz;
    return np_mod_visual_control(NP_HUB_SLOT_VISUAL, &p, (uint16_t)sizeof(p));
}

/* A fresh, armed, seated goggle. */
static void armed(void)
{
    reset_doubles();
    (void)np_mod_visual_init(NP_HUB_SLOT_VISUAL);
}

/* @verifies REQ-FWHUB-65 */
static void test_the_photoparoxysmal_zone_is_never_driven(void)
{
    /* Both ends of the zone are inside it, and nothing is driven or enabled when the answer is a refusal. */
    for (uint8_t f = 3U; f <= 30U; f++) {
        armed();
        check(start_with(0U, f) == NP_HUB_ERR_PHOTIC_ZONE && !emitting() && g_led_sets == 0,
              "zone: photic driving at every whole hertz from 3 to 30 is refused, with no LED and no enable");
    }
    armed();
    check(start_with(3U, 10U) == NP_HUB_ERR_PHOTIC_ZONE && !emitting(),
          "zone: the combined mode drives at freq_hz too, so it is refused as well");
    armed();
    check(start_with(0U, 2U) == NP_HUB_OK && emitting(), "zone: 2 Hz is outside it, and drives");
    armed();
    check(start_with(0U, 31U) == NP_HUB_OK && emitting(), "zone: 31 Hz is outside it, and drives");
    armed();
    check(start_with(0U, 40U) == NP_HUB_OK && emitting(), "zone: 40 Hz, the gamma protocols' frequency, drives");
    armed();
    check(start_with(1U, 10U) == NP_HUB_OK, "zone: EMDR does not drive at freq_hz, so a stray value there is not a photic drive");
    armed();
    check(start_with(2U, 10U) == NP_HUB_OK && emitting(), "zone: retinal PBM is not a photic drive either");
}


/* ── The detector as the zone's guard (REQ-FWHUB-69, -70, -72) ─────────────── */

static uint32_t g_rng_v = 99U;
static float vnoise(void) { float a = 0.0f; for (int i = 0; i < 12; i++) { g_rng_v = g_rng_v * 1664525U + 1013904223U; a += (float)(g_rng_v >> 8) / 16777216.0f; } return (a - 6.0f) * 10.0f; }

/* `seconds` of quiet 8-channel signal into the detector; `spike_at` >= 0 adds a 200 uV step on channels 0, 2, 3, 4 at that sample. */
static void feed_quiet(float seconds, long spike_at)
{
    const long total = (long)(seconds * 500.0f);
    float blk[20 * 8];
    for (long n0 = 0; n0 < total; n0 += 20) {
        for (long s = 0; s < 20; s++) {
            for (long c = 0; c < 8; c++) {
                float v = vnoise();
                if (n0 + s == spike_at && ((0x1DU >> c) & 1U)) { v += 200.0f; }
                blk[s * 8 + c] = v;
            }
        }
        np_ppx_feed(blk, 20U, 8U);
    }
}

/* A detector that sees, with its halt wired to the goggles, and a policy with the given evidence. */
static void guarded(np_ppx_evidence_t ev, bool override_on)
{
    np_ppx_config_t cfg = { .stop_on_occipital_only = true, .manual_override = override_on };
    np_ppx_policy_init(ev, cfg);
    np_ppx_params_t pr;
    np_ppx_params_provisional(&pr);
    np_ppx_roles_t roles = { 0x03U, 0xFCU };
    np_ppx_init(&pr, roles, true);
    np_ppx_set_halt_fn(np_mod_visual_ppx_notify);
    g_rng_v = 99U;
    feed_quiet(3.0f, -1);
    g_panics = 0;
}

static void unguard(void)
{
    np_ppx_config_t cfg = { .stop_on_occipital_only = true, .manual_override = false };
    np_ppx_policy_init(NP_PPX_EVIDENCE_NONE, cfg);
    np_ppx_set_halt_fn(NULL);
}

/* @verifies REQ-FWHUB-70 */
static void test_the_zone_is_driven_only_where_the_evidence_and_the_override_allow_it(void)
{
    armed(); guarded(NP_PPX_EVIDENCE_NONE, true);
    check(start_with(0U, 20U) == NP_HUB_ERR_PHOTIC_ZONE && !emitting(), "guard: no evidence: refused, even with the override");
    armed(); guarded(NP_PPX_EVIDENCE_BENCH, false);
    check(start_with(0U, 20U) == NP_HUB_ERR_PHOTIC_ZONE && !emitting(), "guard: bench evidence without the override: refused");
    armed(); guarded(NP_PPX_EVIDENCE_BENCH, true);
    check(start_with(0U, 20U) == NP_HUB_OK && emitting(), "guard: bench evidence with the clinician's override, detector seeing: driven");
    armed(); guarded(NP_PPX_EVIDENCE_CLINICAL, false);
    check(start_with(0U, 20U) == NP_HUB_OK && emitting(), "guard: clinical evidence: driven");
    unguard();
}

/* @verifies REQ-FWHUB-69 */
static void test_the_zone_is_not_started_when_the_detector_cannot_see(void)
{
    armed(); guarded(NP_PPX_EVIDENCE_CLINICAL, false);
    np_ppx_params_t pr; np_ppx_params_provisional(&pr);
    np_ppx_roles_t roles = { 0x03U, 0xFCU };
    np_ppx_init(&pr, roles, true);                 /* a fresh session: still warming up, so blind */
    check(start_with(0U, 20U) == NP_HUB_ERR_PPX_BLIND && !emitting() && g_led_sets == 0,
          "blind: a detector that cannot see: the zone is refused, with no LED and no enable");
    check(start_with(0U, 40U) == NP_HUB_OK && emitting(), "blind: a drive outside the zone does not wait for it");
    unguard();
}

/* @verifies REQ-FWHUB-67 */
/* @verifies REQ-FWHUB-72 */
static void test_a_discharge_during_a_zone_drive_cuts_the_leds_and_pauses_the_session(void)
{
    armed(); guarded(NP_PPX_EVIDENCE_CLINICAL, false);
    check(start_with(0U, 20U) == NP_HUB_OK && emitting() && np_mod_visual_photic_zone_active(), "halt: the zone is being driven");
    feed_quiet(2.0f, 300);
    check(!emitting() && !np_mod_visual_photic_zone_active(), "halt: the LEDs are off and the enable is withdrawn");
    check(g_panics == 1 && g_last_cause == NP_HOLD_DETECTOR_DISCHARGE, "halt: and the session is held paused, not aborted, with the detector's discharge as the cause");
    check(start_with(0U, 20U) == NP_HUB_ERR_PPX_HALT && start_with(0U, 40U) == NP_HUB_ERR_PPX_HALT,
          "halt: after a detection the goggles refuse every later drive in the session, as a halt");
    unguard();
}

/* @verifies REQ-FWHUB-67 */
static void test_a_discharge_while_nothing_in_the_zone_is_driven_stops_nothing(void)
{
    armed(); guarded(NP_PPX_EVIDENCE_CLINICAL, false);
    check(start_with(0U, 40U) == NP_HUB_OK && emitting(), "halt: 40 Hz drives");
    feed_quiet(2.0f, 300);
    check(emitting() && g_panics == 0, "halt: a discharge while only 40 Hz runs does not stop it (the zone is the guarded range)");
    unguard();
}

/* @verifies REQ-FWHUB-69 */
static void test_losing_sight_during_a_zone_drive_halts_it(void)
{
    armed(); guarded(NP_PPX_EVIDENCE_CLINICAL, false);
    check(start_with(0U, 20U) == NP_HUB_OK && emitting(), "blind: the zone is being driven");
    float blk[20 * 8];
    for (int b = 0; b < 400; b++) {                 /* every lead goes flat: 16 s is longer than the level's time constant needs */
        for (int s = 0; s < 20; s++) { for (int c = 0; c < 8; c++) { blk[s * 8 + c] = 5.0f; } }
        np_ppx_feed(blk, 20U, 8U);
    }
    check(!emitting() && g_panics == 1 && g_last_cause == NP_HOLD_DETECTOR_BLIND, "blind: the detector lost sight: the LEDs are off and the session is held paused, with blindness as the cause");
    unguard();
}

/* @verifies REQ-FWHUB-75 */
/* OI-SIMFW-20: the status of the LED command is checked.  A command that fails is not a running session. */
static void test_a_failed_led_command_is_not_a_running_session(void)
{
    np_mod_visual_params_t p;
    np_hub_status_t rc;

    /* Photic (mode 0). */
    reset_doubles();
    (void)np_mod_visual_init(NP_HUB_SLOT_VISUAL);
    g_led_rc = NP_HUB_ERR_MOD_FAULT;
    rc = start();
    check(rc == NP_HUB_ERR_MOD_FAULT, "led fault: the failure status goes back to the caller (photic)");
    check((g_req_mask & NP_SAFETY_EN_VISUAL) == 0U, "led fault: no enable is requested (photic)");
    check(!g_leds_on, "led fault: nothing is left driven (photic)");
    check(!np_mod_visual_photic_zone_active(), "led fault: the session is not marked active");

    /* The refusal is not sticky: the same command lands once the LED driver answers. */
    g_led_rc = NP_HUB_OK;
    check(start() == NP_HUB_OK && emitting(), "led fault: a later command that lands is admitted");

    /* Retinal PBM (mode 2). */
    reset_doubles();
    (void)np_mod_visual_init(NP_HUB_SLOT_VISUAL);
    p = photic();
    p.mode = 2U;
    g_led_rc = NP_HUB_ERR_MOD_FAULT;
    rc = np_mod_visual_control(NP_HUB_SLOT_VISUAL, &p, (uint16_t)sizeof(p));
    check(rc == NP_HUB_ERR_MOD_FAULT && !emitting(), "led fault: retinal PBM fails closed with the failure status");

    /* Combined (mode 3): the EMDR half is not started after a failed LED half. */
    reset_doubles();
    (void)np_mod_visual_init(NP_HUB_SLOT_VISUAL);
    p = photic();
    p.mode = 3U;
    g_led_rc = NP_HUB_ERR_MOD_FAULT;
    rc = np_mod_visual_control(NP_HUB_SLOT_VISUAL, &p, (uint16_t)sizeof(p));
    check(rc == NP_HUB_ERR_MOD_FAULT && !emitting() && g_emdr_sets == 0,
          "led fault: combined mode fails closed and does not start the EMDR half");

    /* A running session whose next command fails is stopped, not left on the old command. */
    reset_doubles();
    (void)np_mod_visual_init(NP_HUB_SLOT_VISUAL);
    check(start() == NP_HUB_OK && emitting(), "led fault: a session is running");
    g_led_rc = NP_HUB_ERR_MOD_FAULT;
    rc = start();
    check(rc == NP_HUB_ERR_MOD_FAULT && !emitting(), "led fault: a failed replacement command stops the running session");
}

/* @verifies REQ-FWHUB-77#a */
static void test_each_goggle_check_refuses_with_its_own_status(void)
{
    /* Seated but the eye closed, in the retinal mode only. */
    armed();
    g_eye_open = false;
    check(start_with(2U, 0U) == NP_HUB_ERR_EYE_NOT_OPEN && !emitting(), "own status: eye not open in a retinal mode");

    /* The exposure check. */
    armed();
    g_mpe_ok = false;
    check(start_with(1U, 0U) == NP_HUB_ERR_MPE_CHECK && !emitting(), "own status: the exposure check");

    /* Not seated. */
    armed();
    g_seated = false;
    check(start_with(1U, 0U) == NP_HUB_ERR_GOGGLES_NOT_SEATED && !emitting(), "own status: goggles not seated");

    /* No two of the goggle checks, and none of them and the old shared status, are one code (REQ-FWHUB-77 a). */
    const np_hub_status_t codes[] = {
        NP_HUB_ERR_PHOTIC_ZONE, NP_HUB_ERR_PPX_HALT, NP_HUB_ERR_PPX_BLIND, NP_HUB_ERR_GOGGLE_LIFT_UNARMED,
        NP_HUB_ERR_GOGGLES_NOT_SEATED, NP_HUB_ERR_EYE_NOT_OPEN, NP_HUB_ERR_MPE_CHECK,
    };
    int distinct = 1;
    for (size_t i = 0U; i < sizeof codes / sizeof codes[0]; i++) {
        if (codes[i] == NP_HUB_ERR_SAFETY_REJECTED || codes[i] >= 0) { distinct = 0; }
        for (size_t j = i + 1U; j < sizeof codes / sizeof codes[0]; j++) {
            if (codes[i] == codes[j]) { distinct = 0; }
        }
    }
    check(distinct, "own status: no two goggle checks share a code, and none is the shared -10");
}

int main(void)
{
    test_init_arms_the_interrupt();
    test_unarmed_goggle_never_emits();
    test_edge_cuts_without_telemetry();
    test_edge_inside_control_window();
    test_reseat_does_not_resume();
    test_telemetry_backstop();
    test_mode_f_is_compiled_out();
    test_the_photoparoxysmal_zone_is_never_driven();
    test_the_zone_is_driven_only_where_the_evidence_and_the_override_allow_it();
    test_the_zone_is_not_started_when_the_detector_cannot_see();
    test_a_discharge_during_a_zone_drive_cuts_the_leds_and_pauses_the_session();
    test_a_discharge_while_nothing_in_the_zone_is_driven_stops_nothing();
    test_losing_sight_during_a_zone_drive_halts_it();
    test_a_failed_led_command_is_not_a_running_session();
    test_each_goggle_check_refuses_with_its_own_status();

    if (g_failures != 0) {
        printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nAll checks passed\n");
    return 0;
}
