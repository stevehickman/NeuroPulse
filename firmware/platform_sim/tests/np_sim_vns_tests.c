/*
 * NeurOne Firmware-in-the-Loop Simulator — auricular VNS scenario tests (NP-SIM-FW-001 §8 step 3, REQ-SIMFW-13)
 *
 * HOST TEST CODE ONLY.  Runs the device's own np_mod_vns.c, and the HRV session library it drives, against the sim ear
 * clip, and asserts on what the stimulator was told, the enables the hub requested and the telemetry the driver builds.
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


/* The clip, in contact at 1500 ohm.  Impedance and readouts are test inputs, not model defaults. */
static void setup(void)
{
    np_sim_set_sink(capture);
    np_sim_use_real_clock();
    np_simh_init();
    run("vns_add 1500");
    run("vns_init");
    g_cap_n = 0;
}

static np_sim_module_t *vns(void) { return np_sim_find(NP_SIM_KIND_VNS, 0U); }
static bool enabled(void)         { return (np_safety_spi_get_requested_mask() & NP_SAFETY_EN_VNS_HRV) != 0U; }

/* @verifies-none The contact-range probe is np_mod_vns.c's; the hub suite verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_detect_is_a_contact_range_check(void)
{
    setup();
    run("vns_detect");
    check(field("\"ev\":\"vns_detect\"", "\"code\":") == 0.0, "detect: a clip in contact is detected");
    run("reading 0 vns_impedance 6000");
    run("vns_detect");
    check(field("\"ev\":\"vns_detect\"", "\"code\":") != 0.0, "detect: above the contact range is not a clip");
    run("clear 0");
    run("vns_absent");
    run("vns_detect");
    check(field("\"ev\":\"vns_detect\"", "\"code\":") != 0.0, "detect: a removed clip is not detected");
}

/* @verifies-none Stimulation is np_mod_vns.c's; the hub suite verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_drive_and_stop(void)
{
    setup();
    run("vns 0 25000 2000 250 1 1 0");
    check(result_code_is(0), "drive: a command with contact is accepted");
    check(vns()->driven && vns()->vns_side_on[0] && !vns()->vns_side_on[1] && vns()->vns_amp_ua == 2000U,
          "drive: the left side is told the authored amplitude");
    check(enabled() && vns()->vns_ppg_on && vns()->vns_eeg_ref, "drive: the hub requested the VNS enable, started the PPG and routed the EEG reference");
    uint16_t c[NP_SAFETY_MAX_CHANNELS];
    np_safety_spi_get_channel_currents(c);
    check(c[NP_SAFETY_CH_VNS_HRV] == 2000U, "drive: the commanded current was published to the safety channel");
    run("vns_stop");
    check(!vns()->driven && !enabled() && !vns()->vns_ppg_on && !vns()->vns_eeg_ref, "stop: stimulation, enable, PPG and the reference routing all stop");
    np_safety_spi_get_channel_currents(c);
    check(c[NP_SAFETY_CH_VNS_HRV] == 0U, "stop: and the published current is cleared");
    run("vns 2 25000 2000 250 0 0 0");
    check(vns()->vns_side_on[0] && vns()->vns_side_on[1], "drive: side 2 drives both ears");
}

/* @verifies-none The current ceiling is np_mod_vns.c's; the hub suite verifies it (REQ-FWHUB-78). Simulator evidence only (REQ-SIMFW-06). */
static void test_an_amplitude_above_the_ceiling_is_refused_before_the_stimulator(void)
{
    setup();
    run("vns 0 25000 60000 250 1 1 0");
    check(!result_code_is(0), "ceiling: 60 mA is refused, not clamped to 40 mA");
    check(!vns()->driven && !enabled() && !vns()->vns_ppg_on && !vns()->vns_eeg_ref,
          "ceiling: no stimulation, no enable, no PPG and no reference routing were started");
    uint16_t c[NP_SAFETY_MAX_CHANNELS];
    np_safety_spi_get_channel_currents(c);
    check(c[NP_SAFETY_CH_VNS_HRV] == 0U, "ceiling: and no current was published to the safety channel");
    g_cap_n = 0;
    run("vns 0 25000 40000 250 0 0 0");
    check(result_code_is(0) && vns()->vns_amp_ua == 40000U, "ceiling: exactly 40 mA is accepted and driven as authored");
}

/* @verifies-none Contact confirmation is np_mod_vns.c's; the hub suite verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_no_contact_means_no_stimulation(void)
{
    setup();
    run("reading 0 vns_impedance 100");
    run("vns 0 25000 2000 250 1 1 0");
    check(!result_code_is(0) && !vns()->driven && !enabled(), "contact: an impedance below the range refuses the command and enables nothing");
    run("clear 0");
    run("vns_absent");
    run("vns 0 25000 2000 250 0 0 0");
    check(!result_code_is(0) && !enabled(), "contact: a removed clip reads 0 ohm and the command is refused");
    check(count2("\"op\":\"vns_impedance\"", "\"source\":\"absent\"") >= 1, "contact: and the read is reported as absent");
}

/* @verifies-none A non-finite impedance read; the driver's comparison is np_mod_vns.c's. Simulator evidence only (REQ-SIMFW-06). */
static void test_a_non_finite_impedance_is_not_read_as_no_contact(void)
{
    setup();
    run("reading 0 vns_impedance nan");
    run("vns 0 25000 2000 250 0 0 0");
    check(!result_code_is(0) && !vns()->driven && !enabled(),
          "non-finite contact: a NaN impedance is refused and enables nothing (OI-SIMFW-17, closed)");
}

/* @verifies-none A failed stimulator write; the driver's reply is the hub suite's (REQ-VNSC-14#a). Simulator evidence only (REQ-SIMFW-06). */
static void test_a_failed_stimulator_write_enables_nothing(void)
{
    setup();
    run("fail 0 vns_stim_set -17 1");
    run("vns 0 25000 2000 250 1 1 0");
    check(!result_code_is(0) && !enabled() && !vns()->driven, "fault: a failed write refuses the command and requests no enable");
    check(!vns()->vns_ppg_on && !vns()->vns_eeg_ref,
          "fault: and the PPG and the EEG reference routing, started before the write, are stopped (OI-SIMFW-18, closed)");
}

/* @verifies-none A lost stop; np_mod_vns.c's reply is the hub suite's (REQ-VNSC-14#b, #c). Simulator evidence only (REQ-SIMFW-06). */
static void test_a_lost_stimulator_stop_is_reported_and_retried(void)
{
    setup();
    run("vns 0 25000 2000 250 1 1 0");
    run("fail 0 vns_stim_stop -17 1");
    run("vns_stop");
    check(!result_code_is(0) && !enabled() && vns()->driven,
          "lost stop: the hub drops the enable, reports the failure, and the stimulator still drives (OI-SIMFW-18, closed)");
    check(!vns()->vns_ppg_on && !vns()->vns_eeg_ref, "lost stop: the PPG and the reference routing stopped regardless");
    run("vns_stop");
    check(result_code_is(0) && !vns()->driven, "lost stop: the next stop retries the stimulator, which stops, and reports success");
}

/* @verifies-none The HRV session lifecycle is the library's and np_mod_vns.c's; their suites verify it. Simulator evidence only (REQ-SIMFW-06). */
static void test_the_real_hrv_session_starts_and_ends_with_the_command(void)
{
    setup();
    run("vns 0 25000 2000 250 1 0 1");
    check(result_code_is(0), "hrv: a command that asks for an HRV protocol is accepted");
    check(count("\"fn\":\"np_log_hrv_session\"") == 0, "hrv: and no record is written while the session runs");
    run("vns_stop");
    check(count("\"fn\":\"np_log_hrv_session\"") == 1, "hrv: stopping the VNS module ends the real session, which hands its record to the logger once");
    run("vns_stop");
    check(count("\"fn\":\"np_log_hrv_session\"") == 1, "hrv: a second stop writes nothing");
}

/* @verifies-none Telemetry is np_mod_vns.c's; the hub suite verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_telemetry_reports_the_readouts_and_the_contact(void)
{
    setup();
    run("vns_readouts 42.5 6.5 64");
    run("vns 0 25000 2000 250 1 0 0");
    run("vns_sample");
    const char *line = last_with("\"ev\":\"vns_sample\"");
    check(strstr(line, "\"rmssd_ms\":42.5") != NULL && strstr(line, "\"coherence\":6.5") != NULL && strstr(line, "\"hr_bpm\":64.0") != NULL,
          "telemetry: the HRV readouts reach the record");
    check(strstr(line, "\"impedance_ohm\":1500") != NULL && strstr(line, "\"contact_confirmed\":true") != NULL, "telemetry: and the contact");
    run("vns_stop");
    run("vns_sample");
    check(strstr(last_with("\"ev\":\"vns_sample\""), "\"contact_confirmed\":false") != NULL, "telemetry: contact is not confirmed once stopped");
}

/* @verifies-none Malformed command and shutdown; np_mod_vns.c's replies are the hub suite's. Simulator evidence only (REQ-SIMFW-06). */
static void test_malformed_command_and_shutdown(void)
{
    setup();
    run("vns_raw 3");
    check(!result_code_is(0) && !enabled(), "malformed: a command of the wrong size is refused");
    run("vns 0 25000 2000 250 1 1 1");
    run("vns_shutdown");
    check(result_code_is(0) && !enabled() && count("\"fn\":\"np_log_hrv_session\"") == 1, "shutdown: stops everything and ends the HRV session");
}

/* @verifies REQ-SIMFW-04 */
static void test_default_and_injected_answers_are_marked(void)
{
    setup();
    run("vns 0 25000 2000 250 1 1 0");
    run("vns_sample");
    check(count2("\"op\":\"vns_stim_set\"", "\"source\":\"default\"") == 1 &&
          count2("\"op\":\"vns_rmssd\"", "\"source\":\"default\"") == 1 &&
          count2("\"op\":\"vns_ppg_start\"", "\"source\":\"default\"") == 1,
          "marked: every answer from the default model says default");
    check(count("\"source\":\"injected\"") == 0, "marked: nothing is marked injected when nothing was injected");
    run("reading 0 vns_hr 180");
    run("vns_sample");
    check(count2("\"op\":\"vns_hr\"", "\"source\":\"injected\"") == 1, "marked: a pinned reading is marked injected");
}

/* @verifies REQ-SIMFW-11 */
static void test_a_module_answers_only_for_itself(void)
{
    setup();
    run("eeg_add");
    run("stim_add 0 5.0");
    run("fail 0 vns_stim_set -17 1");
    run("eeg 0xFF 6 0 0 0");
    run("bes 0 0 10000 500");
    check(np_sim_find(NP_SIM_KIND_EEG, 0)->eeg_running && np_sim_find(NP_SIM_KIND_STIM_PAIR, 0)->driven,
          "isolation: a fault armed on the clip does not touch the EEG front end or the stimulation pair at index 0");
    run("vns 0 25000 2000 250 0 0 0");
    check(!result_code_is(0), "isolation: and the clip sees its own armed fault");
}

int main(void)
{
    test_detect_is_a_contact_range_check();
    test_drive_and_stop();
    test_an_amplitude_above_the_ceiling_is_refused_before_the_stimulator();
    test_no_contact_means_no_stimulation();
    test_a_non_finite_impedance_is_not_read_as_no_contact();
    test_a_failed_stimulator_write_enables_nothing();
    test_a_lost_stimulator_stop_is_reported_and_retried();
    test_the_real_hrv_session_starts_and_ends_with_the_command();
    test_telemetry_reports_the_readouts_and_the_contact();
    test_malformed_command_and_shutdown();
    test_default_and_injected_answers_are_marked();
    test_a_module_answers_only_for_itself();
    printf("%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
