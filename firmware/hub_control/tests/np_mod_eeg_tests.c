/*
 * NeurOne Hub Control — EEG Driver Stop Host Tests
 * Document: NP-FW-HUB-001 §8.2, REQ-FWEEG-01 (OI-SIMFW-16)
 *
 * np_mod_eeg_hal_stop_continuous() returned nothing and np_mod_eeg_control()
 * cleared its recording flag whether or not the stop took, so a stop that did not
 * land was neither reported nor repeated (found by the firmware-in-the-loop
 * simulator, NP-SIM-FW-001 §14).  The seam now returns np_hub_status_t and the
 * driver clears the flag only on NP_HUB_OK.
 *
 * What these tests pin:
 *   1. a stop that lands is reported as success and clears the flag: a stop with
 *      nothing recording makes no HAL call;
 *   2. a stop that fails is returned to the caller and the flag stays set;
 *   3. the next stop calls the HAL again (one attempt per call, no timer) and,
 *      when it lands, reports success; after that no further call is made;
 *   4. np_mod_eeg_shutdown() reports a lost stop and a second shutdown retries it;
 *   5. a command while a stop is still lost reconfigures the channels but does not
 *      start the converter a second time (it is still running);
 *   6. a clean start, configure and stop is unchanged.
 *
 * No FreeRTOS, no hardware.  IEC 62304 Class B — SW-02 hub control.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "np_hub_types.h"
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

/* Driver under test (no per-module header; registry idiom). */
np_hub_status_t np_mod_eeg_init(uint8_t slot);
np_hub_status_t np_mod_eeg_control(uint8_t slot, const void *params, uint16_t len);
np_hub_status_t np_mod_eeg_shutdown(uint8_t slot);

/* ── Doubles ─────────────────────────────────────────────────────────────────── */

static int  g_starts, g_stops, g_writes;
static int  g_stop_failures;       /* the next N stops fail */
static bool g_running;

void np_mod_eeg_hal_spi_write_reg(uint8_t reg, uint8_t val) { (void)reg; (void)val; g_writes++; }
uint8_t np_mod_eeg_hal_spi_read_reg(uint8_t reg)            { (void)reg; return 0x3EU; }
void np_mod_eeg_hal_start_continuous(void)                  { g_starts++; g_running = true; }
np_hub_status_t np_mod_eeg_hal_stop_continuous(void)
{
    g_stops++;
    if (g_stop_failures > 0) {
        g_stop_failures--;
        return NP_HUB_ERR_MOD_FAULT;
    }
    g_running = false;
    return NP_HUB_OK;
}
float   np_mod_eeg_hal_read_impedance(uint8_t ch)           { (void)ch; return 0.0f; }
float   np_mod_eeg_hal_get_band_power(uint8_t band)         { (void)band; return 0.0f; }
uint8_t np_mod_eeg_hal_get_dominant_band(void)              { return 0U; }
void    np_mod_eeg_hal_self_calibrate(void)                 { }

static const np_mod_eeg_params_t PARAMS = { .channel_mask = 0xFFU, .gain = 6U, .notch = 0U,
                                            .ref_mode = 0U, .adaptive_out = 0U };

static np_hub_status_t start(void) { return np_mod_eeg_control(NP_HUB_SLOT_EEG, &PARAMS, (uint16_t)sizeof PARAMS); }
static np_hub_status_t stop(void)  { return np_mod_eeg_control(NP_HUB_SLOT_EEG, NULL, 0U); }

static void setup(void)
{
    g_starts = g_stops = g_writes = 0;
    g_stop_failures = 0;
    g_running = false;
    (void)np_mod_eeg_init(NP_HUB_SLOT_EEG);
}

/* @verifies REQ-FWEEG-01#d */
static void test_a_clean_start_and_stop_are_unchanged(void)
{
    setup();
    check(start() == NP_HUB_OK && g_running && g_starts == 1, "clean: a command starts the converter once");
    check(start() == NP_HUB_OK && g_starts == 1, "clean: a second command does not start it again");
    check(stop() == NP_HUB_OK && !g_running && g_stops == 1, "clean: a stop stops it and reports success");
    check(stop() == NP_HUB_OK && g_stops == 1, "clean: a stop with nothing recording makes no HAL call");
}

/* @verifies REQ-FWEEG-01#a */
static void test_a_lost_stop_is_reported(void)
{
    setup();
    (void)start();
    g_stop_failures = 1;
    const np_hub_status_t rc = stop();
    check(rc == NP_HUB_ERR_MOD_FAULT, "lost stop: the failure is returned to the caller");
    check(g_running, "lost stop: the converter is still running (the double kept it)");
}

/* @verifies REQ-FWEEG-01#b */
static void test_a_lost_stop_is_retried_by_the_next_stop(void)
{
    setup();
    (void)start();
    g_stop_failures = 2;
    check(stop() != NP_HUB_OK && g_stops == 1, "retry: first stop lost, one attempt made");
    check(stop() != NP_HUB_OK && g_stops == 2, "retry: the second stop is attempted again, one attempt per call, and is lost too");
    check(stop() == NP_HUB_OK && g_stops == 3 && !g_running, "retry: the third stop lands, the converter stops, success is reported");
    check(stop() == NP_HUB_OK && g_stops == 3, "retry: once it landed no further call is made");
}

/* @verifies REQ-FWEEG-01#c */
static void test_shutdown_reports_and_retries(void)
{
    setup();
    (void)start();
    g_stop_failures = 1;
    check(np_mod_eeg_shutdown(NP_HUB_SLOT_EEG) == NP_HUB_ERR_MOD_FAULT, "shutdown: a lost stop is reported");
    check(np_mod_eeg_shutdown(NP_HUB_SLOT_EEG) == NP_HUB_OK && !g_running, "shutdown: a second shutdown retries it and the converter stops");
}

/* @verifies REQ-FWEEG-01#b */
static void test_a_command_while_a_stop_is_lost_does_not_restart(void)
{
    setup();
    (void)start();
    g_stop_failures = 1;
    (void)stop();
    const int writes = g_writes;
    check(start() == NP_HUB_OK, "reconfigure: a command is accepted while the converter is still running");
    check(g_writes > writes && g_starts == 1, "reconfigure: the channels are rewritten and the converter is not started a second time");
    check(stop() == NP_HUB_OK && !g_running, "reconfigure: and the next stop still lands");
}

/* A gain code above 6 used to be masked into the 3-bit register field: 8 was written, and recorded, as 1x. */
/* @verifies REQ-FWHUB-78#h,f */
static void test_a_gain_code_outside_the_wire_definition_is_refused_and_writes_nothing(void)
{
    setup();
    np_mod_eeg_params_t p = PARAMS;
    const int writes = g_writes;
    p.gain = 7U;
    check(np_mod_eeg_control(NP_HUB_SLOT_EEG, &p, (uint16_t)sizeof p) == NP_HUB_ERR_INVALID_ARG, "gain: code 7 is refused");
    p.gain = 8U;
    check(np_mod_eeg_control(NP_HUB_SLOT_EEG, &p, (uint16_t)sizeof p) == NP_HUB_ERR_INVALID_ARG, "gain: code 8 is refused, not written as 1x");
    check(g_writes == writes && g_starts == 0 && !g_running, "gain: no register was written and acquisition was not started");

    p.gain = 6U;
    check(np_mod_eeg_control(NP_HUB_SLOT_EEG, &p, (uint16_t)sizeof p) == NP_HUB_OK && g_starts == 1 && g_running,
          "gain: code 6, the highest the wire defines, is accepted and acquisition starts");
    p.gain = 9U;
    const int after = g_writes;
    check(np_mod_eeg_control(NP_HUB_SLOT_EEG, &p, (uint16_t)sizeof p) == NP_HUB_ERR_INVALID_ARG && g_writes == after && g_running,
          "gain: a refused command leaves a running acquisition and its channel registers as they were");
    (void)stop();
}

int main(void)
{
    test_a_clean_start_and_stop_are_unchanged();
    test_a_lost_stop_is_reported();
    test_a_lost_stop_is_retried_by_the_next_stop();
    test_shutdown_reports_and_retries();
    test_a_command_while_a_stop_is_lost_does_not_restart();
    test_a_gain_code_outside_the_wire_definition_is_refused_and_writes_nothing();
    printf("\n%s (%d failure(s))\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures);
    return g_failures == 0 ? 0 : 1;
}
