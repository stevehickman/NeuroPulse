/*
 * NeurOne Hub Control — Intranasal Y-Probe Driver Host Tests
 * Document: NP-FW-HUB-001 §8.7, NP-HW-NASAL-001 REQ-NASAL-15 (OI-SIMFW-21)
 *
 * REQ-NASAL-15: a drive command that does not fully apply leaves nothing emitting.  A bilateral command writes the
 * left side and then the right.  When the right write failed the driver returned the fault but left the left side
 * emitting, and because it only set its active flag on success, its stop path (which acts only on that flag) could
 * not reach the left side (found by the firmware-in-the-loop simulator, NP-SIM-FW-001 §18).
 *
 * What these tests pin:
 *   1. a bilateral command whose second write fails stops the first side, requests no enable, reports
 *      NP_HUB_ERR_MOD_FAULT, and leaves the driver idle (a later stop is a no-op, a later good command works);
 *   2. the same for a failure on the first write, and for a single-side command that fails;
 *   3. a failure while a previous command is running stops that running command as well;
 *   4. a fully applied command still drives the sides it names and requests the enable (the fix refuses
 *      nothing it used to admit), and the duty ceiling still applies.
 *
 * NOT pinned, and deliberately unchanged: authentication is read once, at init (OI-SIMFW-21 point 1, still open).
 *
 * No FreeRTOS, no hardware.  IEC 62304 Class B — SW-02 hub control.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "np_hub_types.h"
#include "np_safety_spi.h"
#include "np_session_log.h"
#include "np_sw02_platform_hal.h"

static int g_failures = 0;

static void check(int cond, const char *name)
{
    if (cond) { printf("PASS: %s\n", name); }
    else      { printf("FAIL: %s\n", name); g_failures++; }
}

np_hub_status_t np_mod_intranasal_init(uint8_t slot);
np_hub_status_t np_mod_intranasal_control(uint8_t slot, const void *params, uint16_t len);

/* ── Doubles: what the driver asked the hardware to do ───────────────────────── */

static bool     g_auth;
static bool     g_side_on[2];
static uint8_t  g_duty[2];
static int      g_set_calls;
static int      g_fail_on_call;        /* 1-based pwm_set call that fails; 0 = none */
static uint16_t g_req_mask;

void np_safety_spi_request_enable(uint16_t bits)  { g_req_mask |= bits; }
void np_safety_spi_request_disable(uint16_t bits) { g_req_mask &= (uint16_t)~bits; }

void np_log_shdr_zone_auth(uint8_t slot, np_hub_mod_type_t type, bool pass) { (void)slot; (void)type; (void)pass; }

bool np_mod_ins_hal_auth_check(void) { return g_auth; }
bool np_mod_ins_hal_pd_contact(void) { return true; }
np_hub_status_t np_mod_ins_hal_pwm_set(uint8_t side, uint8_t cur_660, uint8_t cur_808, uint8_t freq_code, uint8_t duty)
{
    (void)freq_code;
    g_set_calls++;
    if (g_set_calls == g_fail_on_call) { return NP_HUB_ERR_GENERIC; }   /* a failed write leaves that side as it was */
    g_side_on[side] = (duty != 0U && (cur_660 != 0U || cur_808 != 0U));
    g_duty[side] = duty;
    return NP_HUB_OK;
}
void np_mod_ins_hal_pwm_stop(uint8_t side) { g_side_on[side] = false; }
uint16_t np_mod_ins_hal_pd_read(uint8_t side, uint8_t wl) { (void)side; (void)wl; return 0U; }

/* ── Helpers ──────────────────────────────────────────────────────────────────── */

#define SLOT NP_HUB_SLOT_INTRANASAL

static void reset(void)
{
    g_auth = true;
    g_side_on[0] = g_side_on[1] = false;
    g_duty[0] = g_duty[1] = 0U;
    g_set_calls = 0;
    g_fail_on_call = 0;
    g_req_mask = 0U;
    (void)np_mod_intranasal_init(SLOT);
}

static np_hub_status_t drive(uint8_t side, uint8_t duty)
{
    np_mod_intranasal_params_t p;
    memset(&p, 0, sizeof p);
    p.side = side; p.freq_code = 40U; p.duty = duty; p.cur_660 = 60U; p.cur_808 = 70U;
    return np_mod_intranasal_control(SLOT, &p, (uint16_t)sizeof p);
}

static bool enable_requested(void) { return (g_req_mask & NP_SAFETY_EN_INTRANASAL) != 0U; }

/* @verifies REQ-NASAL-15 */
static void test_a_failure_on_the_second_side_stops_the_first(void)
{
    reset();
    g_fail_on_call = 2;                           /* bilateral: left is written, right fails */
    np_hub_status_t rc = drive(0U, 20U);
    check(rc == NP_HUB_ERR_MOD_FAULT, "second side fails: the command reports the module fault");
    check(!g_side_on[0] && !g_side_on[1], "second side fails: the left side that was written is stopped, nothing emits");
    check(!enable_requested(), "second side fails: no enable is requested");

    check(np_mod_intranasal_control(SLOT, NULL, 0U) == NP_HUB_OK && !g_side_on[0] && !g_side_on[1],
          "second side fails: a stop afterwards is a harmless no-op");
    g_fail_on_call = 0;
    check(drive(0U, 20U) == NP_HUB_OK && g_side_on[0] && g_side_on[1] && enable_requested(),
          "second side fails: the driver is idle and a good command afterwards drives both sides");
}

/* @verifies REQ-NASAL-15 */
static void test_a_failure_on_the_first_side_leaves_nothing_emitting(void)
{
    reset();
    g_fail_on_call = 1;
    check(drive(0U, 20U) == NP_HUB_ERR_MOD_FAULT, "first side fails: the command reports the module fault");
    check(!g_side_on[0] && !g_side_on[1] && !enable_requested(), "first side fails: nothing emits and no enable is requested");
}

/* @verifies REQ-NASAL-15 */
static void test_a_failing_single_side_command_leaves_nothing_emitting(void)
{
    reset();
    g_fail_on_call = 1;
    check(drive(2U, 20U) == NP_HUB_ERR_MOD_FAULT, "right only fails: the command reports the module fault");
    check(!g_side_on[0] && !g_side_on[1] && !enable_requested(), "right only fails: nothing emits and no enable is requested");
}

/* @verifies REQ-NASAL-15 */
static void test_a_failed_command_also_stops_a_running_one(void)
{
    reset();
    check(drive(0U, 20U) == NP_HUB_OK && g_side_on[0] && g_side_on[1] && enable_requested(), "running: a bilateral command is driving");
    g_fail_on_call = g_set_calls + 2;             /* the next command's second write fails */
    check(drive(0U, 10U) == NP_HUB_ERR_MOD_FAULT, "running: the next command fails on its second side");
    check(!g_side_on[0] && !g_side_on[1] && !enable_requested(),
          "running: both sides are stopped and the enable is withdrawn (a half-replaced command is not left running)");
}

/* @verifies REQ-NASAL-15 */
static void test_a_fully_applied_command_still_drives(void)
{
    reset();
    check(drive(1U, 20U) == NP_HUB_OK && g_side_on[0] && !g_side_on[1] && enable_requested(), "applied: 1 drives the left only");
    (void)np_mod_intranasal_control(SLOT, NULL, 0U);
    check(!g_side_on[0] && !enable_requested(), "applied: stop drops the side and the enable");
    check(drive(2U, 20U) == NP_HUB_OK && !g_side_on[0] && g_side_on[1], "applied: 2 drives the right only");
}

/* The duty ceiling refuses; it does not clamp.  Duty 200 used to reach the emitters as 0x32, a pulse train nobody authored. */
/* @verifies REQ-FWHUB-78#d,f */
static void test_a_duty_above_the_ceiling_is_refused_and_starts_nothing(void)
{
    reset();
    check(drive(0U, 0x33U) == NP_HUB_ERR_INVALID_ARG, "duty: one code above the ceiling is refused");
    check(g_set_calls == 0 && !g_side_on[0] && !g_side_on[1] && !enable_requested(),
          "duty: neither side was written and no enable was requested");
    check(drive(0U, 200U) == NP_HUB_ERR_INVALID_ARG && g_set_calls == 0, "duty: 200 is refused, not written as 0x32");

    check(drive(0U, 0x32U) == NP_HUB_OK && g_duty[0] == 0x32U && g_duty[1] == 0x32U && enable_requested(),
          "duty: exactly the ceiling is accepted and reaches the emitters as authored");
}

/* @verifies REQ-FWHUB-78#f */
static void test_a_refused_duty_leaves_a_running_command_as_it_was(void)
{
    reset();
    check(drive(0U, 20U) == NP_HUB_OK && g_side_on[0] && g_side_on[1], "running: a bilateral command at duty 20 is driving");
    const int calls = g_set_calls;
    check(drive(0U, 0x40U) == NP_HUB_ERR_INVALID_ARG, "running: a later command above the ceiling is refused");
    check(g_set_calls == calls && g_side_on[0] && g_side_on[1] && g_duty[0] == 20U && g_duty[1] == 20U && enable_requested(),
          "running: nothing was written, so the command already emitting is exactly as it was, enable included");
}

int main(void)
{
    test_a_failure_on_the_second_side_stops_the_first();
    test_a_failure_on_the_first_side_leaves_nothing_emitting();
    test_a_failing_single_side_command_leaves_nothing_emitting();
    test_a_failed_command_also_stops_a_running_one();
    test_a_fully_applied_command_still_drives();
    test_a_duty_above_the_ceiling_is_refused_and_starts_nothing();
    test_a_refused_duty_leaves_a_running_command_as_it_was();
    printf("%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
