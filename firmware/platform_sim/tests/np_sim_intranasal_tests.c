/*
 * NeurOne Firmware-in-the-Loop Simulator — intranasal Y-probe scenario tests (NP-SIM-FW-001 §8 step 3, REQ-SIMFW-13)
 *
 * HOST TEST CODE ONLY.  Runs the device's own np_mod_intranasal.c against the sim Y-probe, and asserts on what each side
 * was told, the enable the hub requested and the events the sim layer reports.
 *
 * What these tests are, and are not.  The tests that name REQ-SIMFW-04, -11 and -13 verify the simulator.  The
 * rest drive the firmware through a whole-path scenario and assert what it did; the firmware's own behaviour is
 * verified by the hub suite, and a run here is not verification evidence
 * (REQ-SIMFW-06).
 */
#include "np_sim_bus.h"
#include "np_sim_pbm_harness.h"
#include "np_sim_test_util.h"

#include "np_hub_types.h"
#include "np_safety_spi.h"

#include <stdio.h>
#include <string.h>

static int g_failures;

static void check(int cond, const char *name)
{
    if (cond) { printf("PASS: %s\n", name); }
    else      { printf("FAIL: %s\n", name); g_failures++; }
}


/* A probe with a matching sleeve and contact; both sides idle.  The sensor readings are test inputs. */
static void setup(void)
{
    np_sim_set_sink(capture);
    np_sim_use_real_clock();
    np_simh_init();
    run("ins_add 1 1");
    run("ins_init");
    g_cap_n = 0;
}

static np_sim_module_t *ins(void) { return np_sim_find(NP_SIM_KIND_INTRANASAL, 0U); }
static bool enabled(void)         { return (np_safety_spi_get_requested_mask() & NP_SAFETY_EN_INTRANASAL) != 0U; }

/* @verifies-none Probe and authentication logging are np_mod_intranasal.c's; the hub suite verifies them. Simulator evidence only (REQ-SIMFW-06). */
static void test_detect_and_authentication(void)
{
    setup();
    run("ins_detect");
    check(field("\"ev\":\"ins_detect\"", "\"code\":") == 0.0, "detect: a probe with a matching sleeve and contact is detected");
    run("ins_sensor contact 0");
    run("ins_detect");
    check(field("\"ev\":\"ins_detect\"", "\"code\":") != 0.0, "detect: no contact is not a probe");
    run("ins_sensor contact 1");
    run("ins_sensor auth 0");
    run("ins_detect");
    check(field("\"ev\":\"ins_detect\"", "\"code\":") != 0.0, "detect: a sleeve that does not authenticate is not a probe");
    g_cap_n = 0;
    run("ins_init");
    check(!result_code_is(0) && count2("\"fn\":\"np_log_shdr_zone_auth\"", "\"pass\":false") == 1,
          "init: an unauthenticated sleeve fails init and the failure is logged to SHDR once");
}

/* @verifies-none Sides are np_mod_intranasal.c's; the hub suite verifies them. Simulator evidence only (REQ-SIMFW-06). */
static void test_each_side_selection_reaches_the_emitters(void)
{
    setup();
    run("ins 0 40 20 60 70");
    check(ins()->ins_side_on[0] && ins()->ins_side_on[1] && enabled(), "sides: 0 drives both");
    check(ins()->ins_cur660[1] == 60U && ins()->ins_cur808[1] == 70U && ins()->ins_freq[0] == 40U && ins()->ins_duty[0] == 20U,
          "sides: currents, frequency and duty reach the emitters");
    run("ins_stop");
    check(!ins()->driven && !enabled(), "stop: both sides stop and the enable drops");
    run("ins 1 40 20 60 70");
    check(ins()->ins_side_on[0] && !ins()->ins_side_on[1], "sides: 1 drives the left only");
    run("ins_stop");
    run("ins 2 40 20 60 70");
    check(!ins()->ins_side_on[0] && ins()->ins_side_on[1], "sides: 2 drives the right only");
}

/* @verifies-none The duty ceiling is np_mod_intranasal.c's; the hub suite verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_duty_is_capped_before_the_emitters(void)
{
    setup();
    run("ins 0 40 200 60 70");
    check(ins()->ins_duty[0] == 0x32U && ins()->ins_duty[1] == 0x32U, "duty: 200 reaches the emitters as the 0x32 ceiling");
}

/* @verifies-none Authentication gating is np_mod_intranasal.c's; the hub suite verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_an_unauthenticated_probe_is_never_driven(void)
{
    setup();
    run("ins_sensor auth 0");
    run("ins_init");
    run("ins 0 40 20 60 70");
    check(!result_code_is(0) && !ins()->driven && !enabled(), "auth: after a failed authentication a command is refused and nothing is driven");
}

/* @verifies-none Characterisation of np_mod_intranasal.c authenticating once, at init; the finding is OI-SIMFW-21 point 1, still open (no specified trigger). Simulator evidence only (REQ-SIMFW-06). */
static void test_authentication_is_checked_once_at_init(void)
{
    setup();
    run("ins_sensor auth 0");
    run("ins_sensor contact 0");
    run("ins 0 40 20 60 70");
    check(result_code_is(0) && ins()->driven && enabled(),
          "auth once: a probe whose sleeve and contact have since gone is still driven, because the answer is cached from init (OI-SIMFW-21 point 1, open)");
}

/* @verifies-none A partly applied bilateral command is undone by np_mod_intranasal.c (REQ-NASAL-15, OI-SIMFW-21, fixed); np_mod_intranasal_tests verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_a_bilateral_command_that_fails_on_the_second_side_leaves_nothing_driven(void)
{
    setup();
    run("fail 0 ins_pwm_set -17 1 1");                       /* the second pwm_set fails */
    run("ins 0 40 20 60 70");
    check(!result_code_is(0) && !enabled(), "half-applied: a failure on the right side fails the command and requests no enable");
    check(!ins()->ins_side_on[0] && !ins()->ins_side_on[1] && !ins()->driven,
          "half-applied: the left side, already written, is stopped, so nothing is driven (OI-SIMFW-21, fixed)");
    run("ins_stop");
    check(!ins()->driven && !enabled(), "half-applied: and a stop afterwards leaves it so");
}

/* @verifies-none Dose telemetry is np_mod_intranasal.c's; the hub suite verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_telemetry_reads_both_probes(void)
{
    setup();
    run("ins_pd 0 0 1000");
    run("ins_pd 0 1 500");
    run("ins_pd 1 0 200");
    run("ins_pd 1 1 100");
    run("ins_sample");
    const char *line = last_with("\"ev\":\"ins_sample\"");
    check(strstr(line, "\"dose_left\":1.500") != NULL && strstr(line, "\"dose_right\":0.300") != NULL,
          "telemetry: each side's dose proxy is its two photodiode readings summed (the driver's placeholder constant)");
    check(count2("\"op\":\"ins_pd_read\"", "\"source\":\"default\"") == 4, "telemetry: four photodiode reads, all from the model");
}

/* @verifies-none Malformed command and shutdown; np_mod_intranasal.c's replies are the hub suite's. Simulator evidence only (REQ-SIMFW-06). */
static void test_malformed_command_and_shutdown(void)
{
    setup();
    run("ins_raw 3");
    check(!result_code_is(0) && !ins()->driven, "malformed: a command of the wrong size is refused");
    run("ins 0 40 20 60 70");
    run("ins_shutdown");
    check(result_code_is(0) && !ins()->driven && !enabled(), "shutdown: stops both sides");
}

/* @verifies REQ-SIMFW-04 */
static void test_default_and_injected_answers_are_marked(void)
{
    setup();
    run("ins 0 40 20 60 70");
    check(count2("\"op\":\"ins_pwm_set\"", "\"source\":\"default\"") == 2, "marked: every answer from the default model says default");
    check(count("\"source\":\"injected\"") == 0, "marked: nothing is marked injected when nothing was injected");
    run("reading 0 ins_pd_read 777");
    run("ins_sample");
    check(count2("\"op\":\"ins_pd_read\"", "\"source\":\"injected\"") == 4, "marked: a pinned reading is marked injected");
}

/* @verifies REQ-SIMFW-11 */
static void test_a_module_answers_only_for_itself(void)
{
    setup();
    run("audio_add 8");
    run("fail 0 ins_pwm_set -17 1");
    run("audio 0 200 10000 40 0 0");
    check(np_sim_find(NP_SIM_KIND_AUDIO, 0)->driven, "isolation: a fault armed on the probe does not touch the audio path");
    run("ins 0 40 20 60 70");
    check(!result_code_is(0), "isolation: and the probe sees its own armed fault");
}

int main(void)
{
    test_detect_and_authentication();
    test_each_side_selection_reaches_the_emitters();
    test_duty_is_capped_before_the_emitters();
    test_an_unauthenticated_probe_is_never_driven();
    test_authentication_is_checked_once_at_init();
    test_a_bilateral_command_that_fails_on_the_second_side_leaves_nothing_driven();
    test_telemetry_reads_both_probes();
    test_malformed_command_and_shutdown();
    test_default_and_injected_answers_are_marked();
    test_a_module_answers_only_for_itself();
    printf("%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
