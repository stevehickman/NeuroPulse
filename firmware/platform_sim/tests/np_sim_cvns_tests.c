/*
 * NeurOne Firmware-in-the-Loop Simulator — cervical VNS scenario tests (NP-SIM-FW-001 §8 step 3, REQ-SIMFW-13)
 *
 * HOST TEST CODE ONLY.  Runs the device's own np_mod_cvns.c, the cervical VNS library it integrates (session, cardiac interlock
 * with its Pan-Tompkins detector, stimulation ramp) and the re-enable manager against the sim accessory.  The scenario stands in
 * for the safety MCU (through the driver's own heartbeat entry points) and for the PPG ISR (a deterministic pulse train at 200 Hz,
 * ticking the driver every 100 ms).  The clock is virtual, so a 10 s baseline runs at once.
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


/* The accessory, hub impedance 2.5 / 2.5 kohm (inside the 5 kohm window).  The values are test inputs, not model defaults. */
static void setup(void)
{
    np_sim_set_sink(capture);
    np_sim_use_real_clock();
    np_simh_init();
    run("cvns_add 2.5 2.5 1700000000");
    run("cvns_init");
    g_cap_n = 0;
}

static bool enable_bit(void) { return (np_safety_spi_get_requested_mask() & NP_SAFETY_EN_CVNS) != 0U; }

/* The numbers of the last cvns_state event. */
static int stage(void)      { return (int)field("\"ev\":\"cvns_state\"", "\"stage\":"); }
static int interlock(void)  { return (int)field("\"ev\":\"cvns_state\"", "\"interlock\":"); }
static int abort_code(void) { return (int)field("\"ev\":\"cvns_state\"", "\"abort_reason\":"); }

#define STAGE_IMPEDANCE 1
#define STAGE_BASELINE  2
#define STAGE_FAULT     7

/* @verifies-none Probe and bring-up are np_mod_cvns.c's; np_mod_cvns_tests verifies them. Simulator evidence only (REQ-SIMFW-06). */
static void test_detect_and_init(void)
{
    setup();
    run("cvns_detect");
    check(field("\"ev\":\"cvns_detect\"", "\"code\":") == 0.0, "detect: an accessory that reports present is detected");
    run("cvns_init");
    check(count2("\"fn\":\"np_log_shdr_zone_auth\"", "\"pass\":true") == 1, "init: the authentication pass is logged to SHDR once");
    run("cvns_absent");
    run("cvns_detect");
    check(field("\"ev\":\"cvns_detect\"", "\"code\":") != 0.0, "detect: a removed accessory is not detected");
}

/* @verifies-none A command before init; np_mod_cvns_tests verifies the refusal.  Runs first: the driver's state is static, so only the first scenario of a process sees it uninitialised. Simulator evidence only (REQ-SIMFW-06). */
static void test_a_command_before_init_is_refused(void)
{
    np_sim_set_sink(capture);
    np_sim_use_real_clock();
    np_simh_init();
    run("cvns_add 2.5 2.5 1700000000");
    g_cap_n = 0;
    run("cvns 2 10000 1000 250 10 1");
    check(!result_code_is(0) && !enable_bit(), "init: a command before the driver is initialised is refused and enables nothing");
}

/* @verifies-none Descriptor validation is np_mod_cvns.c's; np_mod_cvns_tests verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_an_out_of_range_descriptor_is_refused_before_anything_starts(void)
{
    setup();
    const char *bad[] = { "cvns 2 500 1000 250 10 1",      /* 0.5 Hz: below the 1 Hz floor      */
                          "cvns 2 30000 1000 250 10 1",    /* 30 Hz: above the 25 Hz ceiling    */
                          "cvns 2 10000 3000 250 10 1",    /* 3 mA: above the 2 mA limit        */
                          "cvns 2 10000 1000 100 10 1",    /* 100 us: below the pulse-width floor */
                          "cvns 2 10000 1000 2000 10 1",   /* 2000 us: above its ceiling        */
                          "cvns 5 10000 1000 250 10 1" };  /* not a side                        */
    bool all = true;
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        run(bad[i]);
        const bool refused = !result_code_is(0) && field("\"ev\":\"result\"", "\"active\":") == 0.0;
        run("cvns_state");
        if (!refused || stage() != 0) { all = false; }
    }
    check(all, "validate: each of six out-of-range descriptors is refused with the driver idle");
    run("cvns 2 10000 1000 0 10 1");
    check(result_code_is(0), "validate: a pulse width of 0 means the 250 us default and is accepted");
}

/* @verifies-none The pre-delivery stages are np_mod_cvns.c's and the library's; np_mod_cvns_tests verifies the driver. Simulator evidence only (REQ-SIMFW-06). */
static void test_a_start_does_not_enable_anything(void)
{
    setup();
    run("cvns 2 10000 1000 250 10 1");
    run("cvns_state");
    check(stage() == STAGE_IMPEDANCE && !enable_bit(), "start: the session is in the impedance check and no enable is requested");
    uint16_t c[NP_SAFETY_MAX_CHANNELS];
    np_safety_spi_get_channel_currents(c);
    check(c[NP_SAFETY_CH_CVNS] == 0U, "start: and no current is published to the safety channel");
}

/* @verifies-none The hub-side impedance step is np_mod_cvns.c's; np_mod_cvns_tests verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_the_hub_measures_impedance_once_a_heartbeat_has_arrived(void)
{
    setup();
    run("cvns 2 10000 1000 250 10 1");
    run("cvns_run 1000 70");
    check(stage() == STAGE_IMPEDANCE && count("\"op\":\"cvns_imp_measure_start\"") == 0,
          "impedance: with no heartbeat from the safety MCU the driver does not even start the measurement");
    run("cvns_mcu 0 0");
    run("cvns_run 500 70");
    check(stage() == STAGE_BASELINE, "impedance: once a heartbeat has arrived the measurement runs and the session moves to the baseline");
    check(count2("\"op\":\"cvns_imp_measure_poll\"", "\"source\":\"default\"") >= 1, "impedance: from the accessory's own readings, marked default");
}

/* @verifies-none An out-of-window electrode; the driver's and library's handling is the hub suite's. Simulator evidence only (REQ-SIMFW-06). */
static void test_a_high_impedance_electrode_does_not_pass_the_check(void)
{
    setup();
    run("reading 0 cvns_imp_measure_poll 6.5");
    run("cvns 2 10000 1000 250 10 1");
    run("cvns_mcu 0 0");
    run("cvns_run 3000 70");
    check(stage() == STAGE_IMPEDANCE && !enable_bit(), "high impedance: 6.5 kohm on the electrodes does not pass the check, so the session does not proceed to a baseline");
}

/* @verifies-none Cross-validation of the two impedance reports; np_mod_cvns_tests verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_the_two_impedance_reports_must_agree(void)
{
    setup();
    run("cvns 2 10000 1000 250 10 1");
    run("cvns_mcu 0 0");
    run("cvns_mcu_imp 4.2 4.2");
    run("cvns_run 1000 70");
    check(stage() == STAGE_FAULT && !enable_bit(), "cross-check: a safety MCU impedance 1.7 kohm from the hub's faults the session before any baseline");
    setup();
    run("cvns 2 10000 1000 250 10 1");
    run("cvns_mcu 0 0");
    run("cvns_mcu_imp 2.6 2.4");
    run("cvns_run 1000 70");
    check(stage() == STAGE_BASELINE, "cross-check: reports within the tolerance agree and the session moves on");
}

/* @verifies-none The nominal fallback is np_mod_cvns.c's; np_mod_cvns_tests verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_no_measurement_path_falls_back_to_a_nominal_value(void)
{
    setup();
    run("fail 0 cvns_imp_measure_start -17 1");
    run("cvns 2 10000 1000 250 10 1");
    run("cvns_mcu 0 0");
    run("cvns_run 500 70");
    check(stage() == STAGE_BASELINE, "fallback: a measurement that cannot start does not stop the session; a nominal value is fed and it moves on");
    check(count2("\"op\":\"cvns_imp_measure_start\"", "\"source\":\"injected\"") == 1, "fallback: the failed start is reported as injected");
}

/* @verifies-none The Pan-Tompkins detector is the library's; its tests verify it. Simulator evidence only (REQ-SIMFW-06). */
static void test_the_real_detector_finds_a_baseline_in_a_pulse_train(void)
{
    setup();
    run("cvns 2 10000 1000 250 10 1");
    run("cvns_mcu 0 0");
    run("cvns_run 5000 70");
    run("cvns_state");
    check(field("\"ev\":\"cvns_state\"", "\"rpeak_pulses\":") >= 3.0,
          "baseline: the detector is finding beats (R-peak pulses counted)");
    run("cvns_run 4000 70");
    const char *line = last_with("\"ev\":\"cvns_state\"");
    check(strstr(line, "\"baseline_valid\":true") != NULL, "baseline: five beats later it has a baseline");
    check(field("\"ev\":\"cvns_state\"", "\"baseline_hr\":") > 68.0 && field("\"ev\":\"cvns_state\"", "\"baseline_hr\":") < 72.0,
          "baseline: and the baseline heart rate is the 70 beats per minute of the pulse train");
}

/* @verifies-none Characterisation of a session that cannot leave the baseline stage; the finding is OI-SIMFW-22. Simulator evidence only (REQ-SIMFW-06). */
static void test_a_session_cannot_get_from_baseline_to_delivery(void)
{
    setup();
    run("cvns 2 10000 1000 250 10 1");
    run("cvns_mcu 0x400 0");                         /* the safety MCU grants the enable from the start */
    run("cvns_run 7600 70");
    bool saw_request = false;
    for (int i = 0; i < 12 && stage() != STAGE_FAULT; i++) {
        run("cvns_run 100 70");
        if (interlock() == 1 && enable_bit()) { saw_request = true; }
    }
    check(saw_request, "baseline to delivery: the baseline completed, the interlock went to PRE_SESSION and the driver requested the enable");
    check(stage() == STAGE_FAULT && abort_code() == 6 && !enable_bit(),
          "baseline to delivery: one tick later the session is in FAULT with abort reason 6 (already active) and the enable dropped, with the grant present (OI-SIMFW-22)");
    uint16_t c[NP_SAFETY_MAX_CHANNELS];
    np_safety_spi_get_channel_currents(c);
    check(c[NP_SAFETY_CH_CVNS] == 0U, "baseline to delivery: and nothing was delivered: the published current is back to 0");
}

/* @verifies-none Stop, suspend and shutdown; np_mod_cvns_tests verifies them. Simulator evidence only (REQ-SIMFW-06). */
static void test_stop_suspend_and_shutdown_end_the_session_cleanly(void)
{
    setup();
    run("cvns 2 10000 1000 250 10 1");
    run("cvns_mcu 0 0");
    run("cvns_run 1000 70");
    run("cvns_stop");
    check(result_code_is(0) && !enable_bit() && field("\"ev\":\"result\"", "\"active\":") == 0.0, "stop: ends the session and leaves no enable");
    setup();
    run("cvns 2 10000 1000 250 10 1");
    run("cvns_mcu 0 0");
    run("cvns_run 1000 70");
    run("cvns_suspend");
    check(!enable_bit() && field("\"ev\":\"result\"", "\"active\":") == 0.0, "suspend: a pause withdraws the enable and leaves the session resumable");
    run("cvns_shutdown");
    check(result_code_is(0), "shutdown: closes a paused session");
}

/* @verifies-none The re-enable lockout; np_cvns_reenable_tests and np_mod_cvns_tests verify it. Simulator evidence only (REQ-SIMFW-06). */
static void test_reenable_is_refused_without_a_fault_or_inside_the_lockout(void)
{
    setup();
    run("cvns_reenable");
    check(!result_code_is(0), "re-enable: with no session there is nothing to re-enable");
    run("cvns 2 10000 1000 250 10 1");
    run("cvns_mcu 0 0");
    run("cvns_mcu_imp 4.2 4.2");
    run("cvns_run 1000 70");
    run("cvns_reenable");
    check(!result_code_is(0), "re-enable: a faulted session cannot be re-enabled within the lockout and without a fresh baseline");
}

/* @verifies-none Malformed command; np_mod_cvns.c's reply is the hub suite's. Simulator evidence only (REQ-SIMFW-06). */
static void test_malformed_command(void)
{
    setup();
    run("cvns_raw 3");
    const bool refused = !result_code_is(0);
    run("cvns_state");
    check(refused && stage() == 0, "malformed: a command of the wrong size is refused and the session stays idle");
}

/* @verifies REQ-SIMFW-04 */
static void test_default_and_injected_answers_are_marked(void)
{
    setup();
    run("cvns_detect");
    run("cvns 2 10000 1000 250 10 1");
    run("cvns_mcu 0 0");
    run("cvns_run 300 70");
    check(count2("\"op\":\"cvns_accessory\"", "\"source\":\"default\"") == 1 && count2("\"op\":\"cvns_imp_measure_start\"", "\"source\":\"default\"") == 1,
          "marked: every answer from the default model says default");
    check(count("\"source\":\"injected\"") == 0, "marked: nothing is marked injected when nothing was injected");
    run("fail 0 cvns_accessory -17 1");
    run("cvns_detect");
    check(count2("\"op\":\"cvns_accessory\"", "\"source\":\"injected\"") == 1, "marked: a failed presence read is marked injected");
}

/* @verifies REQ-SIMFW-11 */
static void test_a_module_answers_only_for_itself(void)
{
    setup();
    run("audio_add 8");
    run("fail 0 cvns_imp_measure_start -17 1");
    run("audio 0 200 10000 40 0 0");
    check(np_sim_find(NP_SIM_KIND_AUDIO, 0)->driven, "isolation: a fault armed on the accessory does not touch the audio path");
    run("cvns 2 10000 1000 250 10 1");
    run("cvns_mcu 0 0");
    run("cvns_run 300 70");
    check(count2("\"op\":\"cvns_imp_measure_start\"", "\"source\":\"injected\"") == 1, "isolation: and the accessory sees its own armed fault");
}

int main(void)
{
    test_a_command_before_init_is_refused();
    test_detect_and_init();
    test_an_out_of_range_descriptor_is_refused_before_anything_starts();
    test_a_start_does_not_enable_anything();
    test_the_hub_measures_impedance_once_a_heartbeat_has_arrived();
    test_a_high_impedance_electrode_does_not_pass_the_check();
    test_the_two_impedance_reports_must_agree();
    test_no_measurement_path_falls_back_to_a_nominal_value();
    test_the_real_detector_finds_a_baseline_in_a_pulse_train();
    test_a_session_cannot_get_from_baseline_to_delivery();
    test_stop_suspend_and_shutdown_end_the_session_cleanly();
    test_reenable_is_refused_without_a_fault_or_inside_the_lockout();
    test_malformed_command();
    test_default_and_injected_answers_are_marked();
    test_a_module_answers_only_for_itself();
    printf("%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
