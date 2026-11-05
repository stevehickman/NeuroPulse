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

#define STAGE_RAMP_UP   3
#define STAGE_ACTIVE    4
#define STAGE_COMPLETE  6
#define MCU_GRANT       0x400          /* NP_SAFETY_EN_CVNS */

static unsigned published_ua(void)
{
    uint16_t c[NP_SAFETY_MAX_CHANNELS];
    np_safety_spi_get_channel_currents(c);
    return (unsigned)c[NP_SAFETY_CH_CVNS];
}

/* A session in steady delivery: the safety MCU granting from the start, 70 BPM, 20 s of simulated time. */
static void reach_active(void)
{
    setup();
    run("cvns 2 10000 1000 250 10 1");
    run("cvns_mcu 0x400 0");
    run("cvns_run 20000 70");
}

/* A session that gets from the baseline to delivery (OI-SIMFW-22, fixed).  It used to fault one tick after the baseline completed. */
/* @verifies-none Baseline to delivery is np_cvns_session.c's and np_mod_cvns.c's; np_mod_cvns_tests verifies it (REQ-FWHUB-65). Simulator evidence only (REQ-SIMFW-06). */
static void test_a_session_gets_from_baseline_to_delivery(void)
{
    setup();
    run("cvns 2 10000 1000 250 10 1");
    run("cvns_mcu 0x400 0");                         /* the safety MCU grants the enable from the start */
    run("cvns_run 7600 70");
    bool saw_request = false;
    for (int i = 0; i < 12 && stage() != STAGE_RAMP_UP; i++) {
        run("cvns_run 100 70");
        if (interlock() == 1 && enable_bit()) { saw_request = true; }
    }
    check(saw_request, "baseline to delivery: the baseline completed, the interlock went to PRE_SESSION and the driver requested the enable");
    check(stage() == STAGE_RAMP_UP && interlock() == 2 && abort_code() == 0 && enable_bit(),
          "baseline to delivery: the grant moved it to ENABLED and the session is in the ramp with no abort (OI-SIMFW-22, fixed)");
    check(published_ua() == 1000U, "baseline to delivery: and the commanded current is published to the safety channel");
    run("cvns_run 12000 70");
    check(stage() == STAGE_ACTIVE && enable_bit(), "baseline to delivery: after the 10 s ramp the session is ACTIVE with the enable held");
    run("cvns_sample");
    check(strstr(last_with("\"ev\":\"cvns_sample\""), "\"ramp_active\":true") != NULL,
          "baseline to delivery: telemetry reports the delivering state");
}

/* Waiting for the grant is not a fault. */
/* @verifies-none The grant wait is np_cvns_session.c's; np_mod_cvns_tests verifies it (REQ-FWHUB-65). Simulator evidence only (REQ-SIMFW-06). */
static void test_a_session_waits_for_the_grant_without_faulting(void)
{
    setup();
    run("cvns 2 10000 1000 250 10 1");
    run("cvns_mcu 0 0");                             /* alive, not granting */
    run("cvns_run 22000 70");
    check(stage() == STAGE_BASELINE && interlock() == 1 && enable_bit() && abort_code() == 0,
          "grant wait: 22 s in, with no grant, the session holds in BASELINE with the interlock PRE_SESSION, the enable requested and no fault");
    run("cvns_mcu 0x400 0");
    run("cvns_run 500 70");
    check(stage() == STAGE_RAMP_UP, "grant wait: the grant, when it comes, starts the ramp");
}

/* The safety MCU refusing the enable ends the session. */
/* @verifies-none Enable refusal is np_mod_cvns.c's; np_mod_cvns_tests verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_the_safety_mcu_refusing_the_enable_faults_the_session(void)
{
    setup();
    run("cvns 2 10000 1000 250 10 1");
    run("cvns_mcu 0 0");
    run("cvns_run 10000 70");
    check(stage() == STAGE_BASELINE && interlock() == 1 && enable_bit(), "refusal: precondition, the enable is requested and awaiting the grant");
    run("cvns_mcu 0 8");                             /* NP_SAFETY_STATUS_IMPEDANCE, no grant */
    run("cvns_run 300 70");
    check(stage() == STAGE_FAULT && !enable_bit() && published_ua() == 0U,
          "refusal: an adverse status with no grant faults the session, drops the enable and clears the published current");
}

/* Data loss in delivery. */
/* @verifies-none The data-loss cutoff is the library's; np_cvns_fai_tests verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_losing_the_r_peaks_in_delivery_cuts_the_session(void)
{
    reach_active();
    check(stage() == STAGE_ACTIVE && enable_bit() && published_ua() == 1000U, "data loss: precondition, in steady delivery");
    g_cap_n = 0;
    run("cvns_run 12000 -1");                        /* no PPG samples at all */
    check(stage() == STAGE_FAULT && !enable_bit() && published_ua() == 0U,
          "data loss: 12 s with no R-peaks faults the session, drops the enable and clears the published current");
    check(count("\"fn\":\"np_log_shdr_fault\"") == 1, "data loss: and the fault is logged to SHDR once");
}

/* A heart-rate step is not this side's cutoff. */
/* @verifies-none Characterisation: the main-processor side has no heart-rate excursion cutoff (NP-FW-CVNS-001 section 6); that cutoff is the safety MCU's, which is not in the loop (OI-SIMFW-09). Simulator evidence only (REQ-SIMFW-06). */
static void test_a_heart_rate_step_is_not_a_cutoff_on_the_main_side(void)
{
    reach_active();
    run("cvns_run 10000 100");                       /* 70 -> 100 BPM, a 30 BPM step */
    check(stage() == STAGE_ACTIVE && enable_bit(),
          "rate step: a 30 BPM step leaves the session in delivery; on this side only data loss cuts it, the excursion cutoff is the safety MCU's");
}

/* Stop in delivery. */
/* @verifies-none Stop is np_mod_cvns.c's; np_mod_cvns_tests verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_stop_in_delivery_ends_the_session_and_drops_the_enable(void)
{
    reach_active();
    run("cvns_stop");
    check(result_code_is(0) && !enable_bit() && published_ua() == 0U && field("\"ev\":\"result\"", "\"active\":") == 0.0,
          "stop: ends the session at once, drops the enable and clears the published current");
    run("cvns_state");
    check(stage() == 0, "stop: the session is back at IDLE");
}

/* The session ends at its length. */
/* @verifies-none The session length bound is the library's; np_cvns_fai_tests verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_the_session_completes_at_its_length(void)
{
    reach_active();
    run("cvns_run 90000 70");
    check(stage() == STAGE_ACTIVE, "complete: 110 s in, the session is still in delivery");
    run("cvns_run 30000 70");
    check(stage() == STAGE_COMPLETE && !enable_bit() && published_ua() == 0U && field("\"ev\":\"cvns_state\"", "\"active\":") == 0.0,
          "complete: at its 120 s length it completes, the driver is released and the enable is dropped");
}

/* Characterisation of a re-enable that cannot succeed (OI-SIMFW-23). */
/* @verifies-none Characterisation of the re-enable path after a session fault; the finding is OI-SIMFW-23. Simulator evidence only (REQ-SIMFW-06). */
static void test_a_faulted_session_cannot_be_re_enabled(void)
{
    reach_active();
    run("cvns_run 12000 -1");
    check(stage() == STAGE_FAULT && interlock() == 0,
          "re-enable: after the fault the interlock is IDLE, not FAULT: the session's fault path disabled it");
    run("cvns_run 40000 70");                        /* past the 30 s lockout, the pulse train back */
    run("cvns_reenable");
    check(!result_code_is(0) && stage() == STAGE_FAULT && !enable_bit(),
          "re-enable: refused even after the lockout, because the interlock only re-enables from FAULT (OI-SIMFW-23)");
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
    test_a_session_gets_from_baseline_to_delivery();
    test_a_session_waits_for_the_grant_without_faulting();
    test_the_safety_mcu_refusing_the_enable_faults_the_session();
    test_losing_the_r_peaks_in_delivery_cuts_the_session();
    test_a_heart_rate_step_is_not_a_cutoff_on_the_main_side();
    test_stop_in_delivery_ends_the_session_and_drops_the_enable();
    test_the_session_completes_at_its_length();
    test_a_faulted_session_cannot_be_re_enabled();
    test_stop_suspend_and_shutdown_end_the_session_cleanly();
    test_reenable_is_refused_without_a_fault_or_inside_the_lockout();
    test_malformed_command();
    test_default_and_injected_answers_are_marked();
    test_a_module_answers_only_for_itself();
    printf("%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
