/*
 * Host tests — the status indicators and stealth mode (REQ-FWHUB-85, REQ-FWHUB-86; OI-PRIV-37, NP-PRIV-ANALYSIS-004).
 *
 * The output is a recorder bound in place of np_hal_status_led_set.  What is tested: a session the person stopped and a
 * pause timeout go back to idle and never light the fault indicator; every other abort reason does; stealth darkens
 * idle and session and never a fault; a stealth change neither hides nor clears a latched fault; every boot starts
 * visible; and the STEALTH wire reads back, refuses an unknown bit and a wrong length.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "np_status_led.h"
#include "np_gatt_server.h"

static int g_failures = 0;
static void check(int cond, const char *name)
{
    if (cond) { printf("PASS: %s\n", name); }
    else      { printf("FAIL: %s\n", name); g_failures++; }
}

void np_transport_hal_enter_critical(void) {}
void np_transport_hal_exit_critical(void) {}

/* np_runner_end_status() lives in np_session_runner.c, which is ARM-and-FreeRTOS heavy.  The mapping is pinned against
 * the real function in np_runner_run_tests.c; here the LED module is driven with the statuses that function returns. */

static np_led_state_t g_out[64];
static unsigned       g_n;
static void record(np_led_state_t s) { if (g_n < sizeof g_out / sizeof g_out[0]) { g_out[g_n] = s; } g_n++; }
static np_led_state_t last(void) { return g_n ? g_out[(g_n - 1U) % (sizeof g_out / sizeof g_out[0])] : (np_led_state_t)0xFF; }
static bool ever(np_led_state_t s) { for (unsigned i = 0; i < g_n && i < sizeof g_out / sizeof g_out[0]; i++) { if (g_out[i] == s) return true; } return false; }

static void boot(void)
{
    g_n = 0U;
    np_status_led_bind(record);
    np_status_led_init();
}

/* @verifies REQ-FWHUB-85#a */
static void test_a_stop_or_a_pause_timeout_goes_back_to_idle_and_never_lights_the_fault(void)
{
    boot();
    np_status_led_session_started();
    check(last() == NP_LED_SESSION, "stop: a running session shows SESSION");
    np_status_led_session_ended(NP_HUB_ERR_STOPPED);
    check(last() == NP_LED_IDLE, "stop: a session the person stopped (or a pause timed out) goes back to IDLE");
    check(!ever(NP_LED_FAULT), "stop: and the fault indicator never lit");

    boot();
    np_status_led_session_started();
    np_status_led_session_ended(NP_HUB_OK);
    check(last() == NP_LED_IDLE && !ever(NP_LED_FAULT), "complete: a session that ran its length goes back to IDLE");
}

/* @verifies REQ-FWHUB-85#b */
static void test_every_other_end_status_lights_the_fault(void)
{
    const np_hub_status_t faults[] = { NP_HUB_ERR_SAFETY_FAULT, NP_HUB_ERR_MOD_FAULT, NP_HUB_ERR_GENERIC, NP_HUB_ERR_TIMEOUT };
    for (size_t i = 0U; i < sizeof faults / sizeof faults[0]; i++) {
        boot();
        np_status_led_session_started();
        np_status_led_session_ended(faults[i]);
        char name[96];
        snprintf(name, sizeof name, "fault: end status %d lights FAULT", (int)faults[i]);
        check(last() == NP_LED_FAULT, name);
    }
    boot();
    np_status_led_fault();
    check(last() == NP_LED_FAULT, "fault: the heartbeat task's fault lights FAULT at once");
    np_status_led_session_started();
    check(last() == NP_LED_SESSION, "fault: the next session start clears the latched fault");
}

/* @verifies REQ-FWHUB-86#a */
static void test_stealth_darkens_idle_and_session(void)
{
    boot();
    np_status_led_set_stealth(true);
    check(last() == NP_LED_OFF, "stealth: at idle the indicators go dark at once");
    np_status_led_session_started();
    check(last() == NP_LED_OFF && !ever(NP_LED_SESSION), "stealth: a session starting in stealth never shows SESSION");
    np_status_led_session_ended(NP_HUB_ERR_STOPPED);
    check(last() == NP_LED_OFF, "stealth: a stop in stealth stays dark");
    np_status_led_set_stealth(false);
    check(last() == NP_LED_IDLE, "stealth: turning it off shows IDLE again");
    np_status_led_session_started();
    np_status_led_set_stealth(true);
    check(last() == NP_LED_OFF, "stealth: turned on mid-session, SESSION goes dark");
    np_status_led_set_stealth(false);
    check(last() == NP_LED_SESSION, "stealth: turned off mid-session, SESSION shows again");
}

/* @verifies REQ-FWHUB-86#b */
static void test_a_fault_shows_in_stealth_and_a_stealth_change_neither_hides_nor_clears_it(void)
{
    boot();
    np_status_led_set_stealth(true);
    np_status_led_session_started();
    np_status_led_session_ended(NP_HUB_ERR_SAFETY_FAULT);
    check(last() == NP_LED_FAULT, "fault in stealth: a session fault lights FAULT although stealth is on");

    boot();
    np_status_led_set_stealth(true);
    np_status_led_fault();
    check(last() == NP_LED_FAULT, "fault in stealth: the heartbeat task's fault lights FAULT although stealth is on");
    np_status_led_set_stealth(true);
    check(last() == NP_LED_FAULT, "fault in stealth: writing stealth on again does not hide it");
    np_status_led_set_stealth(false);
    check(last() == NP_LED_FAULT, "fault in stealth: turning stealth off does not clear it");
    const uint8_t on = NP_STEALTH_FLAG_ON;
    check(np_status_led_write(&on, 1U) == NP_HUB_OK && np_status_led_shown() == NP_LED_FAULT,
          "fault in stealth: a STEALTH write over GATT does not hide it either");
}

/* @verifies REQ-FWHUB-86#c */
static void test_the_stealth_wire_reads_back_and_refuses_what_it_does_not_know(void)
{
    boot();
    uint8_t b[4] = { 0xAA, 0, 0, 0 };
    size_t n = 0U;
    check(np_status_led_read(b, sizeof b, &n) == NP_HUB_OK && n == NP_GATT_STEALTH_LEN && b[0] == 0U,
          "wire: at boot STEALTH reads 0");
    const uint8_t on = 0x01U, off = 0x00U, unknown = 0x02U, both = 0x03U;
    check(np_status_led_write(&on, 1U) == NP_HUB_OK && np_status_led_stealth(), "wire: writing 0x01 turns stealth on");
    check(np_status_led_read(b, sizeof b, &n) == NP_HUB_OK && b[0] == 0x01U, "wire: and reads back 0x01");
    check(np_status_led_write(&unknown, 1U) == NP_HUB_ERR_INVALID_ARG && np_status_led_stealth(),
          "wire: an unknown bit is refused and changes nothing");
    check(np_status_led_write(&both, 1U) == NP_HUB_ERR_INVALID_ARG && np_status_led_stealth(),
          "wire: bit 0 with an unknown bit is refused too, not half-taken");
    const uint8_t two[2] = { 0x00, 0x00 };
    check(np_status_led_write(two, 2U) == NP_HUB_ERR_INVALID_ARG && np_status_led_write(NULL, 1U) == NP_HUB_ERR_INVALID_ARG &&
          np_status_led_stealth(), "wire: a wrong length or no data is refused");
    check(np_status_led_read(b, 0U, &n) == NP_HUB_ERR_INVALID_ARG, "wire: a read into no room is refused");
    check(np_status_led_write(&off, 1U) == NP_HUB_OK && !np_status_led_stealth() && last() == NP_LED_IDLE,
          "wire: writing 0x00 turns it off and the indicators show again");
}

/* @verifies REQ-FWHUB-86#d */
static void test_every_boot_starts_visible(void)
{
    boot();
    np_status_led_set_stealth(true);
    np_status_led_init();   /* a restart: nothing was persisted */
    check(!np_status_led_stealth() && last() == NP_LED_IDLE, "boot: after a restart stealth is off and IDLE shows");
    check(g_out[0] == NP_LED_IDLE, "boot: the first thing the indicators show is IDLE");
}

/* @verifies-none harness self-check: with no output bound the module keeps state and calls nothing */
static void test_unbound_output_keeps_state(void)
{
    g_n = 0U;
    np_status_led_bind(NULL);
    np_status_led_init();
    np_status_led_set_stealth(true);
    check(g_n == 0U && np_status_led_shown() == NP_LED_OFF, "unbound: state is kept and no output is called");
}

int main(void)
{
    test_a_stop_or_a_pause_timeout_goes_back_to_idle_and_never_lights_the_fault();
    test_every_other_end_status_lights_the_fault();
    test_stealth_darkens_idle_and_session();
    test_a_fault_shows_in_stealth_and_a_stealth_change_neither_hides_nor_clears_it();
    test_the_stealth_wire_reads_back_and_refuses_what_it_does_not_know();
    test_every_boot_starts_visible();
    test_unbound_output_keeps_state();
    printf("\n%s: %d failure(s)\n", g_failures ? "FAILED" : "OK", g_failures);
    return g_failures ? 1 : 0;
}
