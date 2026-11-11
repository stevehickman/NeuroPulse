/*
 * NeurOne Firmware-in-the-Loop Simulator — stimulation scenario tests (NP-SIM-FW-001 §8 step 3, REQ-SIMFW-13)
 *
 * HOST TEST CODE ONLY.  Runs the device's own np_mod_stim, np_stim_xcheck, np_ramp_plan and np_safety_spi
 * sources against sim electrode pairs, and asserts on the events the sim layer reports and on the state the
 * firmware leaves.  The clock is virtual (np_sim_advance_ms), so the ramps are deterministic.
 *
 * What these tests are, and are not.  The tests that name REQ-SIMFW-04, -11 and -13 verify the simulator.  The
 * rest drive the firmware through a whole-path scenario and assert what it did; the firmware's own behaviour is
 * verified by np_mod_stim_tests and np_stim_xcheck_tests, and a run here is not verification evidence
 * (REQ-SIMFW-06).
 */
#include "np_sim_bus.h"
#include "np_sim_pbm_harness.h"
#include "np_sim_test_util.h"

#include "np_hub_types.h"
#include "np_ramp_plan.h"
#include "np_safety_spi.h"

#include <stdio.h>
#include <string.h>

static int g_failures;

static void check(int cond, const char *name)
{
    if (cond) { printf("PASS: %s\n", name); }
    else      { printf("FAIL: %s\n", name); g_failures++; }
}


/* Two electrode pairs.  The impedance is a test input, not a model default. */
static void setup(void)
{
    np_sim_set_sink(capture);
    np_sim_use_real_clock();
    np_simh_init();
    run("stim_add 0 5.0");
    run("stim_add 1 5.0");
    np_sim_advance_ms(1000U);          /* virtual from here on */
    g_cap_n = 0;
}

static np_sim_module_t *pair(unsigned p) { return np_sim_find(NP_SIM_KIND_STIM_PAIR, (uint16_t)p); }
static uint16_t requested(void)           { return np_safety_spi_get_requested_mask(); }

static void cur_ua(uint16_t out[]) { np_safety_spi_get_channel_currents(out); }

/* ── BES / tACS ────────────────────────────────────────────────────────────── */

/* @verifies-none Baseline of the firmware path; np_mod_stim_tests verifies the driver. Simulator evidence only (REQ-SIMFW-06). */
static void test_bes_drive_and_stop(void)
{
    setup();
    run("bes 0 0 10000 500");
    check(result_code_is(0), "bes: a command is accepted");
    check(pair(0)->driven && !pair(0)->stim_dc && pair(0)->stim_amp_ua == 500, "bes: the pair holds the commanded AC drive");
    check((requested() & NP_SAFETY_EN_BES_TACS) != 0U, "bes: the hub requested the BES/tACS enable");
    uint16_t c[NP_SAFETY_MAX_CHANNELS]; cur_ua(c);
    check(c[NP_SAFETY_CH_BES_TACS] == 500U, "bes: the hub published the commanded current to the safety channel");
    run("sample bes");
    check(field("\"ev\":\"stim_sample\"", "\"current_ua\":") == 500.0 &&
          count("\"divergence_flagged\":true") == 0, "bes: the delivered peak equals the commanded one and nothing is flagged");
    check(strstr(last_with("\"ev\":\"stim_sample\""), "\"charge_uc\":0") != NULL,
          "bes: the model reports 0 charge for a charge-balanced waveform (a gap, not a measurement)");
    run("stim_stop bes");
    check(!pair(0)->driven && (requested() & NP_SAFETY_EN_BES_TACS) == 0U, "bes: stop stops the DAC and drops the enable");
    cur_ua(c);
    check(c[NP_SAFETY_CH_BES_TACS] == 0U, "bes: stop clears the published current");
}

/* @verifies-none The amplitude limit is np_mod_stim.c's; np_mod_stim_tests verifies it (REQ-FWHUB-78). Simulator evidence only (REQ-SIMFW-06). */
static void test_bes_amplitude_above_the_limit_is_refused_before_the_dac(void)
{
    setup();
    run("bes 0 0 10000 1500");
    check(!result_code_is(0), "bes limit: 1.5 mA is refused, not clamped to 1 mA");
    check(!pair(0)->driven && pair(0)->stim_amp_ua == 0U, "bes limit: the DAC was never commanded");
    uint16_t c[NP_SAFETY_MAX_CHANNELS]; cur_ua(c);
    check(c[NP_SAFETY_CH_BES_TACS] == 0U && (requested() & NP_SAFETY_EN_BES_TACS) == 0U,
          "bes limit: no current was published to the safety channel and no enable requested");
    g_cap_n = 0;
    run("bes 0 0 10000 1000");
    check(result_code_is(0) && pair(0)->stim_amp_ua == 1000U, "bes limit: exactly 1 mA is accepted and driven as authored");
}

/* @verifies-none Scenario: the DAC write fails; the driver's refusal is np_mod_stim_tests's. Simulator evidence only (REQ-SIMFW-06). */
static void test_bes_dac_fault_leaves_nothing_enabled(void)
{
    setup();
    run("fail 0 stim_dac_set -17 1");
    run("bes 0 0 10000 500");
    check(!result_code_is(0), "bes fault: the command reports a fault");
    check(!pair(0)->driven && (requested() & NP_SAFETY_EN_BES_TACS) == 0U,
          "bes fault: the pair is not driven and the enable was never requested");
    check(count2("\"op\":\"stim_dac_set\"", "\"source\":\"injected\"") == 1, "bes fault: the injected answer is reported as injected");
}

/* @verifies-none Scenario: no such electrode pair; the firmware's answer is np_mod_stim's. Simulator evidence only (REQ-SIMFW-06). */
static void test_an_absent_pair_is_refused(void)
{
    setup();
    run("stim_absent 0");
    run("bes 0 0 10000 500");
    check(!result_code_is(0) && (requested() & NP_SAFETY_EN_BES_TACS) == 0U, "absent pair: the command fails and no enable is requested");
    check(count2("\"op\":\"stim_dac_set\"", "\"source\":\"absent\"") == 1, "absent pair: the firmware's write is reported as absent");
    run("stim_present 0");
    run("bes 0 0 10000 500");
    check(result_code_is(0) && pair(0)->driven, "absent pair: present again, it drives");
}

/* ── the commanded-versus-delivered cross-check ───────────────────────────── */

/* @verifies-none Scenario: the DAC delivers more than commanded; np_stim_xcheck_tests verifies the check. Simulator evidence only (REQ-SIMFW-06). */
static void test_over_delivery_is_flagged_once(void)
{
    setup();
    run("bes 0 0 10000 500");
    run("reading 0 stim_read_current 900");
    for (int i = 0; i < 6; i++) { run("sample bes"); }
    check(count("\"divergence_flagged\":true") == 1, "xcheck: delivery above the commanded bound is flagged exactly once");
    run("status");
    check(strstr(last_with("\"ev\":\"stim_channels\""), "\"bes_xcheck_latched\":true") != NULL, "xcheck: the channel latch is set");
}

/* @verifies-none Scenario: a failed delivered-current read; np_stim_xcheck_tests verifies the fail-closed rule. Simulator evidence only (REQ-SIMFW-06). */
static void test_a_failed_current_read_counts_against_the_bound(void)
{
    setup();
    run("bes 0 0 10000 500");
    run("fail 0 stim_read_current 0 -1");
    for (int i = 0; i < 6; i++) { run("sample bes"); }
    check(count("\"divergence_flagged\":true") == 1, "xcheck: a read that fails (non-finite) is not read as safe");
}

/* ── tDCS ──────────────────────────────────────────────────────────────────── */

/* @verifies-none Baseline of the tDCS ramp; np_mod_stim_tests verifies the ramp model. Simulator evidence only (REQ-SIMFW-06). */
static void test_tdcs_ramp_up_and_charge(void)
{
    setup();
    run("tdcs 0 1000 0 30 25000");
    check(result_code_is(0), "tdcs: a command is accepted");
    check((requested() & NP_SAFETY_EN_TDCS) != 0U, "tdcs: the hub requested the tDCS enable");
    run("advance 15000");
    check(field("\"ev\":\"clock\"", "\"tdcs_ramp_pct\":") == 50.0, "tdcs: halfway through the ramp the driver reports 50%");
    run("sample tdcs");
    check(field("\"ev\":\"stim_sample\"", "\"current_ua\":") == 500.0 && count("\"divergence_flagged\":true") == 0,
          "tdcs: the DAC delivers half the target at half the ramp and nothing is flagged");
    run("advance 15000");
    check(field("\"ev\":\"clock\"", "\"tdcs_ramp_phase\":") == (double)NP_RAMP_NONE, "tdcs: the ramp is over after 30 s");
    run("sample tdcs");
    check(field("\"ev\":\"stim_sample\"", "\"current_ua\":") == 1000.0, "tdcs: the target is delivered at steady state");
    check(field("\"ev\":\"stim_sample\"", "\"charge_uc\":") == 15000.0,
          "tdcs: the charge after the ramp is the triangle's area, 0.5 x 1000 uA x 30 s");
    run("advance 10000");
    run("sample tdcs");
    check(field("\"ev\":\"stim_sample\"", "\"charge_uc\":") == 25000.0, "tdcs: and 1000 uA for a further 10 s adds 10000 uC");
}

/* @verifies-none The 30 s floor is np_mod_stim.c's; np_mod_stim_tests verifies it (REQ-FWHUB-78). Simulator evidence only (REQ-SIMFW-06). */
static void test_tdcs_ramp_floor_and_area_are_enforced(void)
{
    setup();
    run("tdcs 0 1000 0 5 25000");
    check(!result_code_is(0) && !pair(0)->driven && (requested() & NP_SAFETY_EN_TDCS) == 0U,
          "tdcs floor: a 5 s ramp is refused, not lengthened to 30 s, and nothing is driven or enabled");
    g_cap_n = 0;
    run("tdcs 0 2500 0 30 25000");
    check(!result_code_is(0) && !pair(0)->driven && (requested() & NP_SAFETY_EN_TDCS) == 0U,
          "tdcs limit: 2.5 mA is refused, not clamped to 2 mA, and nothing is driven or enabled");
    g_cap_n = 0;
    run("tdcs 0 1000 0 30 0");
    check(!result_code_is(0) && (requested() & NP_SAFETY_EN_TDCS) == 0U,
          "tdcs area: an undeclared pad area is refused and the enable never requested");
}

/* @verifies-none Scenario: stop mid-ramp; the cross-check hold is np_stim_xcheck_tests's. Simulator evidence only (REQ-SIMFW-06). */
static void test_tdcs_stop_ramps_down_under_the_hold(void)
{
    setup();
    run("tdcs 0 1000 0 30 25000");
    run("advance 30000");
    run("stim_stop tdcs");
    check((requested() & NP_SAFETY_EN_TDCS) == 0U, "tdcs stop: the enable is dropped at once");
    uint16_t c[NP_SAFETY_MAX_CHANNELS]; cur_ua(c);
    check(c[NP_SAFETY_CH_TDCS] == 0U, "tdcs stop: the published current is cleared at once");
    run("advance 10000");
    run("sample tdcs");
    check(field("\"ev\":\"stim_sample\"", "\"current_ua\":") > 600.0 && field("\"ev\":\"stim_sample\"", "\"current_ua\":") < 700.0,
          "tdcs stop: 10 s into the 30 s ramp-down the DAC still delivers two thirds of the level");
    for (int i = 0; i < 3; i++) { run("sample tdcs"); }
    check(count("\"divergence_flagged\":true") == 0, "tdcs stop: that delivery is inside the cross-check's hold, so it is not a divergence");
}

/* @verifies-none Scenario: pause and resume; SWR-SS-05 is verified by np_mod_stim_tests. Simulator evidence only (REQ-SIMFW-06). */
static void test_tdcs_pause_and_resume_reverse_the_ramp(void)
{
    setup();
    run("tdcs 0 1000 0 30 25000");
    run("advance 15000");
    run("tdcs_pause");
    check(result_code_is(1), "tdcs pause: the driver is ramping down with the enable held");
    run("advance 5000");
    run("sample tdcs");
    const double mid = field("\"ev\":\"stim_sample\"", "\"current_ua\":");
    check(mid > 300.0 && mid < 400.0, "tdcs pause: 5 s into the ramp-down the level fell from 500 to about 333 uA");
    run("tdcs 0 1000 0 30 25000");
    check(result_code_is(0), "tdcs resume: the same command during the ramp-down is accepted");
    run("advance 3000");
    run("sample tdcs");
    check(field("\"ev\":\"stim_sample\"", "\"current_ua\":") > mid, "tdcs resume: the level rises again from where the ramp-down had got to");
}

/* @verifies-none Scenario: a failed ramp command at the DAC; np_mod_stim_tests verifies the driver's reply. Simulator evidence only (REQ-SIMFW-06). */
static void test_tdcs_ramp_fault_leaves_nothing_enabled(void)
{
    setup();
    run("fail 0 stim_dc_ramp -17 1");
    run("tdcs 0 1000 0 30 25000");
    check(!result_code_is(0) && (requested() & NP_SAFETY_EN_TDCS) == 0U, "tdcs fault: a failed ramp command leaves the enable unrequested");
}


/* @verifies-none Scenario: malformed commands, probe and shutdown; np_mod_stim_tests verifies the driver's replies. Simulator evidence only (REQ-SIMFW-06). */
static void test_malformed_commands_probe_and_shutdown(void)
{
    setup();
    run("stim_raw 17 3");
    check(!result_code_is(0) && (requested() & NP_SAFETY_EN_BES_TACS) == 0U, "malformed: a BES/tACS command of the wrong size is refused");
    run("stim_raw 18 3");
    check(!result_code_is(0) && (requested() & NP_SAFETY_EN_TDCS) == 0U, "malformed: and a tDCS command of the wrong size");
    run("stim_raw 99 8");
    check(!result_code_is(0), "malformed: a command for a slot that is not this driver's is refused");
    run("stim_detect 17");
    check(field("\"ev\":\"stim_detect\"", "\"code\":") == 0.0, "probe: the BES/tACS slot is detected");
    run("stim_detect 18");
    check(field("\"ev\":\"stim_detect\"", "\"code\":") == 0.0, "probe: the tDCS slot is detected");
    run("stim_detect 3");
    check(field("\"ev\":\"stim_detect\"", "\"code\":") != 0.0, "probe: another slot is not this driver's");
    run("bes 0 0 10000 500");
    run("stim_shutdown bes");
    check(!pair(0)->driven && (requested() & NP_SAFETY_EN_BES_TACS) == 0U, "shutdown: the runner's end-of-session call stops a driven channel");
    run("stim_shutdown tdcs");
    check(result_code_is(0), "shutdown: and a channel that is not driven is a no-op");
}

/* @verifies-none Scenario: the runner's wait before a tDCS command settles; np_mod_stim_tests verifies the figure. Simulator evidence only (REQ-SIMFW-06). */
static void test_ramp_up_wait_is_the_whole_ramp_or_the_rest_of_it(void)
{
    setup();
    run("stim_ramp_up_ms 0 1000 0 30");
    check(field("\"ev\":\"stim_ramp_up\"", "\"ms\":") == 30000.0 && field("\"ev\":\"stim_ramp_up\"", "\"bad_length_ms\":") == 0.0,
          "ramp wait: a fresh command waits the whole ramp, and a command of the wrong size waits for nothing");
    run("tdcs 0 1000 0 30 25000");
    run("advance 30000");
    run("tdcs_pause");
    run("advance 10000");
    run("stim_ramp_up_ms 0 1000 0 30");
    check(field("\"ev\":\"stim_ramp_up\"", "\"ms\":") == 10000.0, "ramp wait: a resume 10 s into the ramp-down waits only for the 10 s it fell");
    run("stim_ramp_up_ms 0 1000 1 30");
    check(field("\"ev\":\"stim_ramp_up\"", "\"ms\":") == 30000.0, "ramp wait: a resume with the other polarity is a new command and waits the whole ramp");
}

/* @verifies-none Scenario: pause at steady state, at level 0, and when the ramp-down cannot be commanded; SWR-SS-05 is np_mod_stim_tests's. Simulator evidence only (REQ-SIMFW-06). */
static void test_pause_edges(void)
{
    setup();
    run("tdcs 0 1000 0 30 25000");
    run("tdcs_pause");
    check(result_code_is(0) && (requested() & NP_SAFETY_EN_TDCS) == 0U, "pause edge: paused at level 0 there is nothing to ramp down, so the enable drops");
    setup();
    run("tdcs 0 1000 0 30 25000");
    run("advance 30000");
    run("tdcs_pause");
    check(result_code_is(1) && (requested() & NP_SAFETY_EN_TDCS) != 0U, "pause edge: paused at steady state the enable is held through the ramp-down");
    run("advance 30000");
    check((requested() & NP_SAFETY_EN_TDCS) == 0U, "pause edge: and the tick drops it when the ramp-down is over");
    setup();
    run("tdcs 0 1000 0 30 25000");
    run("advance 30000");
    run("fail 0 stim_dc_ramp -17 1");
    run("tdcs_pause");
    check(result_code_is(0) && (requested() & NP_SAFETY_EN_TDCS) == 0U,
          "pause edge: when the ramp-down cannot be commanded the driver falls back to a stop");
    setup();
    run("tdcs 0 1000 0 30 25000");
    run("advance 30000");
    run("tdcs_pause");
    run("tdcs 0 1000 0 30 25000");
    check(result_code_is(0) && pair(0)->stim_dc_to_ua == 1000U, "pause edge: a resume at the same instant has nothing to climb and is accepted");
}

/* @verifies-none Scenario: the cross-check's envelope across commands and sessions; np_stim_xcheck_tests verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_xcheck_envelope_across_commands_and_sessions(void)
{
    setup();
    run("tdcs 0 1000 0 30 25000");
    run("advance 30000");
    run("tdcs 0 600 0 30 25000");                  /* a lower command ramps down: 1000 is the bound for a while */
    run("tdcs 0 300 0 30 25000");                  /* lower again: the higher of the two holds */
    run("advance 5000");
    run("sample tdcs");
    run("sample tdcs");
    run("sample tdcs");
    check(count("\"divergence_flagged\":true") == 0, "envelope: delivery under the highest level still held is not a divergence");
    run("stim_session_start");
    check(result_code_is(0), "envelope: the runner's session-start reset is accepted with a hold still running");
    run("sample tdcs");
    check(count("\"divergence_flagged\":true") == 0, "envelope: and the running ramp-down is not flagged across the reset");
    run("stim_stop tdcs");
    run("advance 40000");
    run("stim_session_start");
    run("sample tdcs");
    check(count("\"divergence_flagged\":true") == 0, "envelope: with every output off a reset clears the hold");
}

/* ── the simulator's own properties ───────────────────────────────────────── */

/* @verifies REQ-SIMFW-04 */
static void test_default_and_injected_answers_are_marked(void)
{
    setup();
    run("bes 0 0 10000 500");
    run("sample bes");
    check(count2("\"op\":\"stim_dac_set\"", "\"source\":\"default\"") == 1 &&
          count2("\"op\":\"stim_read_current\"", "\"source\":\"default\"") == 1 &&
          count2("\"op\":\"stim_read_impedance\"", "\"source\":\"default\"") == 1 &&
          count2("\"op\":\"stim_read_charge\"", "\"source\":\"default\"") == 1,
          "marked: every answer from the default model says default");
    check(count("\"source\":\"injected\"") == 0, "marked: nothing is marked injected when nothing was injected");
    run("reading 0 stim_read_impedance 42.5");
    run("sample bes");
    check(count2("\"op\":\"stim_read_impedance\"", "\"source\":\"injected\"") == 1, "marked: a pinned reading is marked injected");
    check(field("\"ev\":\"stim_sample\"", "\"impedance_kohm\":") == 42.5, "marked: and the firmware reads the pinned value");
}

/* @verifies REQ-SIMFW-11 */
static void test_a_module_answers_only_for_itself(void)
{
    setup();
    run("add 0 34.0");                              /* a PBM tile in socket 0: same index, other kind */
    run("fail 0 stim_dac_set -17 1");               /* the stimulation pair 0 */
    run("fail 1 socket_pwm_set -17 1");             /* a tile that does not exist; must not touch a pair */
    run("bes 1 0 10000 500");
    check(result_code_is(0) && pair(1)->driven, "isolation: a fault on pair 0 and on socket 1 does not touch pair 1");
    run("bes 0 0 10000 500");
    check(!result_code_is(0), "isolation: the fault armed on pair 0 fires on pair 0");
    check(np_sim_find(NP_SIM_KIND_PBM_BASE, 0)->driven == false, "isolation: and the tile at socket 0 never saw a stimulation command");
}

int main(void)
{
    test_bes_drive_and_stop();
    test_bes_amplitude_above_the_limit_is_refused_before_the_dac();
    test_bes_dac_fault_leaves_nothing_enabled();
    test_an_absent_pair_is_refused();
    test_over_delivery_is_flagged_once();
    test_a_failed_current_read_counts_against_the_bound();
    test_tdcs_ramp_up_and_charge();
    test_tdcs_ramp_floor_and_area_are_enforced();
    test_tdcs_stop_ramps_down_under_the_hold();
    test_tdcs_pause_and_resume_reverse_the_ramp();
    test_tdcs_ramp_fault_leaves_nothing_enabled();
    test_malformed_commands_probe_and_shutdown();
    test_ramp_up_wait_is_the_whole_ramp_or_the_rest_of_it();
    test_pause_edges();
    test_xcheck_envelope_across_commands_and_sessions();
    test_default_and_injected_answers_are_marked();
    test_a_module_answers_only_for_itself();
    printf("%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
