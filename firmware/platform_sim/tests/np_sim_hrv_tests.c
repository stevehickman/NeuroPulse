/*
 * NeurOne Firmware-in-the-Loop Simulator — the HRV biofeedback library, reached (NP-SIM-FW-001 §32, REQ-SIMFW-24, OI-SIMFW-60)
 *
 * HOST TEST CODE ONLY.  np_mod_vns.c starts the HRV session when a VNS command names a protocol; these scenarios then play the three feeders
 * the library was written for and nothing in the device's own code supplies (OI-SIMFW-78): the 200 Hz PPG interrupt, the 500 Hz EEG stream
 * and the 10 Hz HRV task, with SYNTHETIC signals (np_sim_hrv_harness.c).  What the session reports comes out as the library's own coherence
 * result and the session record it hands the logger at its end.  Two library functions whose results the session keeps to itself (the pacer's
 * sweep and the EEG bands with the adaptive step) are called directly.
 *
 * What these tests are, and are not.  Virtual clock, synthetic signals; a pass is NOT verification evidence (REQ-SIMFW-06) and is no claim about
 * a real heart.  Each scenario runs in a forked child: the library and the driver keep module-static state.
 */
#define NP_SIM_CAP_LONG
#include "np_sim_bus.h"
#include "np_sim_pbm_harness.h"
#include "np_sim_test_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int g_failures;

static void check(int cond, const char *name)
{
    if (cond) { printf("PASS: %s\n", name); }
    else      { printf("FAIL: %s\n", name); g_failures++; }
}

static void in_child(void (*scenario)(void))
{
    fflush(stdout);
    const pid_t pid = fork();
    if (pid == 0) {
        g_failures = 0;
        alarm(120);   /* a scenario that hangs fails; it does not hold the suite */
        scenario();
        fflush(stdout);
        exit(g_failures == 0 ? 0 : 1);
    }
    int st = 0;
    (void)waitpid(pid, &st, 0);
    if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) { g_failures++; printf("FAIL: scenario child did not finish cleanly\n"); }
}

/* A clip in contact and a VNS command that names an HRV protocol (1 taVNS sync, 2 EEG dual, 3 PBM): the driver creates and starts the library's session. */
static void start(int proto)
{
    char c[64];
    np_sim_set_sink(capture);
    np_sim_use_real_clock();
    np_simh_init();
    run("vns_add 1500");
    run("vns_init");
    run("hrv_reset");
    (void)snprintf(c, sizeof c, "vns 0 25000 2000 250 1 0 %d", proto);
    run(c);
    g_cap_n = 0;
}

/* A second session in the same scenario: the first was released by its stop. */
static void restart(int proto)
{
    char c[64];
    run("hrv_reset");
    (void)snprintf(c, sizeof c, "vns 0 25000 2000 250 1 0 %d", proto);
    run(c);
}

static void hrv(const char *line) { run(line); }
static double rec(const char *key) { return field("\"fn\":\"np_log_hrv_session\"", key); }
static int    records(void)        { return count2("\"fn\":\"np_log_hrv_session\"", "\"has_record\":true"); }
static double st(const char *key)  { return field("\"ev\":\"hrv_state\"", key); }
static void   stop_session(void)   { run("vns_stop"); }

/* @verifies REQ-SIMFW-24#a */
static void test_the_drivers_session_never_advances_without_a_feeder(void)
{
    start(1);
    run("hrv_state");
    check(count2("\"ev\":\"hrv_state\"", "\"has_session\":true") == 1, "a VNS command that names a protocol started the library's session");
    np_sim_advance_ms(1300000U);   /* 1300 virtual seconds with the device's own code and nothing else: the session's own length is 1200 s */
    run("hrv_state");
    check(st("\"coherence_code\":") == -2.0 && count2("\"ev\":\"hrv_state\"", "\"valid\":true") == 0,
          "after 1300 s no coherence exists: nothing fed or ticked the session");
    check(records() == 0, "and the session's own timer never ended it: no record was handed to the logger");
    stop_session();
    check(records() == 1 && rec("\"duration_s\":") == 1300.0 && rec("\"rr_count\":") == 0.0,
          "only the driver's stop ended it, 1300 s long, with no R-R interval recorded");
}

/* @verifies REQ-SIMFW-24#b */
static void test_coherence_needs_64_intervals_and_a_flat_rhythm_scores_the_maximum(void)
{
    start(1);
    hrv("hrv_run 30000 60 80 0");
    check(st("\"coherence_code\":") == -2.0, "30 s of beats is too few R-R intervals for a coherence estimate (NOT_READY)");
    hrv("hrv_run 60000 60 80 0");
    check(count2("\"ev\":\"hrv_state\"", "\"valid\":true") >= 1 && st("\"coherence\":") > 5.0 && st("\"rmssd_ms\":") > 20.0 && st("\"rmssd_ms\":") < 60.0,
          "after 90 s of a 0.1 Hz rhythm the library reports a valid, high coherence and an RMSSD of a few tens of ms");
    const double resonant = st("\"coherence\":");
    stop_session();
    check(records() == 1 && rec("\"rr_count\":") >= 88.0 && rec("\"rr_count\":") <= 90.0, "the record counts the beats the peak detector found (88 to 90 of 90)");

    g_cap_n = 0;
    restart(1);   /* a new session: the first one was released */
    hrv("hrv_run 90000 60 0 0");
    /* Pinned as found (OI-SIMFW-78): with no variability at all the library's power sums are 0, and the ratio of two near-zero numbers is the
     * maximum score.  Change this check when the owner decides what a flat rhythm should score. */
    check(count2("\"ev\":\"hrv_state\"", "\"valid\":true") >= 1 && st("\"rmssd_ms\":") < 1.0 && st("\"lf_total\":") < 0.01 && st("\"hf_total\":") < 0.01 &&
          st("\"coherence\":") == resonant,
          "a perfectly steady rhythm (RMSSD 0, no LF or HF power) scores the maximum coherence, the same 10 as the resonant one (OI-SIMFW-78)");
}

/* @verifies REQ-SIMFW-24#b */
static void test_the_peak_detector_rejects_intervals_outside_the_valid_range_and_the_buffer_saturates(void)
{
    start(1);
    hrv("hrv_run 20000 25 0 0");     /* a 2400 ms period: above NP_RR_MAX_MS */
    stop_session();
    check(records() == 1 && rec("\"rr_count\":") == 0.0, "a 2400 ms interval is outside the valid range and is not recorded");
    g_cap_n = 0;
    restart(1);
    hrv("hrv_run 20000 250 0 0");    /* a 240 ms period: below NP_RR_MIN_MS, inside the refractory period */
    stop_session();
    printf("  (240 ms beats: %.0f intervals recorded in 20 s)\n", rec("\"rr_count\":"));
    check(records() == 1 && rec("\"rr_count\":") > 0.0 && rec("\"rr_count\":") < 45.0,
          "beats 240 ms apart (250 per minute) fall inside the detector's refractory period: it reads every second beat, so about half the beats, as 480 ms intervals");
    g_cap_n = 0;
    restart(1);
    hrv("hrv_run 400000 60 80 0");
    stop_session();
    printf("  (400 s of beats: %.0f intervals recorded)\n", rec("\"rr_count\":"));
    check(rec("\"rr_count\":") >= 395.0 && rec("\"rr_count\":") <= 402.0, "400 s of beats are all counted: the record carries every accepted interval, not the 256-entry ring's occupancy (OI-UINT16-01)");
}

/* @verifies REQ-SIMFW-24#c */
static void test_the_taVNS_gate_opens_on_shortening_intervals_for_its_protocol_only_and_its_failsafe_reopens_it(void)
{
    start(1);
    hrv("hrv_run 5000 60 0 -20");
    stop_session();
    check(records() == 1 && rec("\"tavns_stim_count\":") == 1.0, "a run of shortening R-R intervals (an inspiration) opened the taVNS gate once");
    g_cap_n = 0;
    restart(1);
    hrv("hrv_run 15000 60 0 -20");
    stop_session();
    check(rec("\"tavns_stim_count\":") >= 2.0, "intervals that keep shortening past 6 s trip the failsafe, and the next onset opens it again");
    g_cap_n = 0;
    restart(2);
    hrv("hrv_run 15000 60 0 -20");
    stop_session();
    check(records() == 1 && rec("\"protocol\":") == 2.0 && rec("\"tavns_stim_count\":") == 0.0, "the same rhythm under the EEG-dual protocol never opens the gate");
}

/* @verifies REQ-SIMFW-24#d */
static void test_a_session_ends_by_its_own_timer_once_and_a_new_one_starts_after_a_stop(void)
{
    start(1);
    hrv("hrv_run 1300000 60 80 0");
    check(records() == 1 && rec("\"duration_s\":") == 1200.0 && rec("\"reason\":") == 0.0, "fed and ticked, the session ended itself at 1200 s with one record");
    stop_session();
    check(records() == 1, "the driver's later stop wrote nothing: the session was already over");
    g_cap_n = 0;
    restart(1);
    run("hrv_state");
    check(count2("\"ev\":\"hrv_state\"", "\"has_session\":true") == 1 && st("\"coherence_code\":") == -2.0, "a new session starts after the stop, with no coherence yet");
}

/* @verifies REQ-SIMFW-24#d */
static void test_a_session_that_finished_its_sweep_still_records_no_personal_rate(void)
{
    start(1);
    hrv("hrv_run 900000 60 80 0");
    stop_session();
    check(records() == 1 && rec("\"duration_s\":") == 900.0 && rec("\"target_rate_bpm\":") == 0.0,
          "900 s covers the 7 sweep steps, and the record still carries no personalised breathing rate");
    g_cap_n = 0;
    run("hrv_pacer 1000000 5.5");
    check(field("\"ev\":\"hrv_pacer\"", "\"finalise_code\":") == 0.0 && field("\"ev\":\"hrv_pacer\"", "\"best_rate_bpm\":") == 5.5,
          "called directly, the library's own finalise does find the best rate from a finished sweep (OI-SIMFW-78)");
}

/* @verifies REQ-SIMFW-24#e */
static void test_the_pacer_sweeps_and_settles_on_the_rate_with_the_best_coherence(void)
{
    start(1);
    run("hrv_pacer 100000 5.5");
    check(field("\"ev\":\"hrv_pacer\"", "\"start_rate_bpm\":") == 4.0 && count2("\"ev\":\"hrv_pacer\"", "\"sweep_active\":true") == 1 &&
          field("\"ev\":\"hrv_pacer\"", "\"finalise_code\":") == -2.0 && field("\"ev\":\"hrv_pacer\"", "\"inspires\":") >= 3.0,
          "a pacer with no personalised rate starts a sweep at 4 breaths a minute, announces breath phases, and refuses to finalise a sweep still running");
    run("hrv_pacer 1000000 5.5");
    check(count2("\"ev\":\"hrv_pacer\"", "\"personalized\":true") >= 1 && field("\"ev\":\"hrv_pacer\"", "\"target_rate_bpm\":") == 5.5, "a finished sweep settles on the rate where coherence peaked (5.5)");
    run("hrv_pacer 1000000 6.5");
    check(field("\"ev\":\"hrv_pacer\"", "\"target_rate_bpm\":") == 6.5, "and on 6.5 when that is where it peaked");
}

/* @verifies REQ-SIMFW-24#e */
static void test_a_given_rate_does_not_stop_the_sweep_and_a_personalised_rate_does(void)
{
    start(1);
    run("hrv_pacer 10000 5.5 6.0 0");
    check(field("\"ev\":\"hrv_pacer\"", "\"start_rate_bpm\":") == 4.0 && count2("\"ev\":\"hrv_pacer\"", "\"sweep_active\":true") == 1,
          "an in-range starting rate of 6 is overwritten: the pacer sweeps from 4 whenever no personalised rate is given");
    run("hrv_pacer 10000 5.5 6.0 5.0");
    check(field("\"ev\":\"hrv_pacer\"", "\"start_rate_bpm\":") == 5.0 && count2("\"ev\":\"hrv_pacer\"", "\"personalized\":true") >= 1 &&
          count2("\"ev\":\"hrv_pacer\"", "\"sweep_active\":false") == 1, "a personalised rate of 5 is taken as it is and no sweep runs");
}

/* @verifies REQ-SIMFW-24#e */
static void test_the_eeg_bands_and_the_adaptive_step_follow_the_signal(void)
{
    start(2);
    run("hrv_eeg_bands 20 5 1 5");
    check(field("\"ev\":\"hrv_eeg_bands\"", "\"alpha\":") > 5.0 * field("\"ev\":\"hrv_eeg_bands\"", "\"theta\":") && field("\"ev\":\"hrv_eeg_bands\"", "\"alpha_theta_ratio\":") > 1.5 &&
          field("\"ev\":\"hrv_eeg_bands\"", "\"nudge_bpm\":") == 0.0, "an alpha-dominant signal: alpha power above theta, a ratio above the 1.5 target, and no nudge to the pacer");
    run("hrv_eeg_bands 5 20 1 5");
    check(field("\"ev\":\"hrv_eeg_bands\"", "\"theta\":") > 5.0 * field("\"ev\":\"hrv_eeg_bands\"", "\"alpha\":") && field("\"ev\":\"hrv_eeg_bands\"", "\"nudge_bpm\":") == -0.25,
          "a theta-dominant signal with a valid HRV coherence: the pacer is nudged down by 0.25 breaths a minute");
    run("hrv_eeg_bands 5 20 1 3");
    check(field("\"ev\":\"hrv_eeg_bands\"", "\"nudge_bpm\":") == 0.25, "with the HRV coherence below 5 the rule that recovers it wins: the pacer is nudged up by 0.25, whatever the EEG says");
    run("hrv_eeg_bands 5 20 0 0");
    check(field("\"ev\":\"hrv_eeg_bands\"", "\"nudge_bpm\":") == 0.0, "the same signal with no valid HRV coherence gives no nudge");
}

/* @verifies REQ-SIMFW-24#e */
static void test_the_dual_protocol_runs_the_eeg_path_through_the_drivers_session(void)
{
    start(2);
    hrv("hrv_run 100000 60 80 0 20 10");
    stop_session();
    check(records() == 1 && rec("\"protocol\":") == 2.0 && rec("\"rr_count\":") >= 98.0 && rec("\"mean_coherence\":") > 5.0,
          "the EEG-dual protocol ran 100 s of PPG and EEG through the driver's session and ended with a record of protocol 2");
}

/* @verifies REQ-SIMFW-24#e */
static void test_a_theta_dominant_eeg_nudges_the_sessions_pacer_and_the_other_protocols_ignore_an_eeg_stream(void)
{
    start(2);
    hrv("hrv_run 100000 60 80 0 5 20");   /* three adaptive steps (every 30 s); each asks the pacer for 0.25 breaths a minute less, below its 4 minimum */
    stop_session();
    check(records() == 1 && rec("\"protocol\":") == 2.0 && rec("\"rr_count\":") >= 98.0,
          "a theta-dominant stream through the driver's dual session runs the adaptive step against the pacer (what it did is not visible: the driver gives the session no display callback)");
    g_cap_n = 0;
    restart(1);
    hrv("hrv_run 100000 60 80 0 5 20");
    stop_session();
    check(records() == 1 && rec("\"protocol\":") == 1.0 && rec("\"mean_coherence\":") > 5.0, "the same stream under the taVNS protocol is ignored, and the session is as it was without one");
}

int main(void)
{
    in_child(test_the_drivers_session_never_advances_without_a_feeder);
    in_child(test_coherence_needs_64_intervals_and_a_flat_rhythm_scores_the_maximum);
    in_child(test_the_peak_detector_rejects_intervals_outside_the_valid_range_and_the_buffer_saturates);
    in_child(test_the_taVNS_gate_opens_on_shortening_intervals_for_its_protocol_only_and_its_failsafe_reopens_it);
    in_child(test_a_session_ends_by_its_own_timer_once_and_a_new_one_starts_after_a_stop);
    in_child(test_a_session_that_finished_its_sweep_still_records_no_personal_rate);
    in_child(test_the_pacer_sweeps_and_settles_on_the_rate_with_the_best_coherence);
    in_child(test_a_given_rate_does_not_stop_the_sweep_and_a_personalised_rate_does);
    in_child(test_the_eeg_bands_and_the_adaptive_step_follow_the_signal);
    in_child(test_a_theta_dominant_eeg_nudges_the_sessions_pacer_and_the_other_protocols_ignore_an_eeg_stream);
    in_child(test_the_dual_protocol_runs_the_eeg_path_through_the_drivers_session);
    if (g_failures == 0) { printf("ALL PASS (0 failures)\n"); return 0; }
    printf("%d FAILURES\n", g_failures);
    return 1;
}
