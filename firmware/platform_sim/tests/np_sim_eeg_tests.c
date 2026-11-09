/*
 * NeurOne Firmware-in-the-Loop Simulator — EEG scenario tests (NP-SIM-FW-001 §8 step 3, REQ-SIMFW-13)
 *
 * HOST TEST CODE ONLY.  Runs the device's own np_mod_eeg.c against the sim ADS1299, and asserts on the register file
 * the driver leaves, the events the sim layer reports and the telemetry the driver builds.
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


/* The sim ADS1299 with the datasheet's ID.  Impedances and band powers are test inputs, not model defaults. */
static void setup(void)
{
    np_sim_set_sink(capture);
    np_sim_use_real_clock();
    np_simh_init();
    run("eeg_add");
    g_cap_n = 0;
}

static np_sim_module_t *eeg(void) { return np_sim_find(NP_SIM_KIND_EEG, 0U); }
static uint8_t reg(unsigned r)    { return eeg()->eeg_regs[r]; }

/* @verifies-none The probe is np_mod_eeg.c's; the hub suite verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_detect_reads_the_id_register(void)
{
    setup();
    run("eeg_detect");
    check(field("\"ev\":\"eeg_detect\"", "\"code\":") == 0.0, "detect: the datasheet's ID is an ADS1299");
    run("eeg_absent");
    run("eeg_detect");
    check(field("\"ev\":\"eeg_detect\"", "\"code\":") != 0.0, "detect: an absent front end reads no ID and is not detected");
    setup();
    run("eeg_add 0x3F");                            /* ID[4:0] = 11111b: another part */
    run("eeg_detect");
    check(field("\"ev\":\"eeg_detect\"", "\"code\":") != 0.0, "detect: a different part's ID is refused");
}

/* @verifies-none The power-on sequence is np_mod_eeg.c's; the hub suite verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_init_writes_the_power_on_sequence(void)
{
    setup();
    run("eeg_init");
    check(result_code_is(0), "init: succeeds");
    check(reg(1) == 0x96U && reg(2) == 0xD0U && reg(3) == 0xE0U, "init: CONFIG1..3 hold 500 SPS, test signal off, reference buffer on");
    bool all = true;
    for (unsigned ch = 0; ch < 8U; ch++) { if (reg(5U + ch) != 0x60U) { all = false; } }
    check(all, "init: all eight channels are normal input at PGA 24");
    check(eeg()->eeg_calibrations == 1U, "init: one self-calibration ran");
    check(reg(0) == 0x3EU, "init: and the ID register is not writable");
}

/* @verifies-none Channel configuration is np_mod_eeg.c's; the hub suite verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_control_configures_enabled_and_powers_down_the_rest(void)
{
    setup();
    run("eeg_init");
    run("eeg 0x0F 3 1 0 0");
    check(result_code_is(0) && eeg()->eeg_running, "control: accepted and the converter runs");
    check(reg(5) == 0x30U && reg(8) == 0x30U, "control: enabled channels carry the authored gain (PGA 6x)");
    check(reg(9) == 0x81U && reg(12) == 0x81U, "control: disabled channels are shorted and powered down");
    run("eeg 0xFF 6 0 0 0");
    check(reg(9) == 0x60U, "control: a second command re-enables them");
}

/* @verifies-none Stop is np_mod_eeg.c's; the hub suite verifies it (REQ-FWEEG-01). Simulator evidence only (REQ-SIMFW-06). */
static void test_stop_and_a_lost_stop(void)
{
    setup();
    run("eeg 0xFF 6 0 0 0");
    run("eeg_stop");
    check(!eeg()->eeg_running, "stop: the converter stops");
    run("eeg 0xFF 6 0 0 0");
    run("fail 0 eeg_stop -17 1");
    run("eeg_stop");
    check(!result_code_is(0) && eeg()->eeg_running,
          "lost stop: the driver reports the failure while the converter still runs (OI-SIMFW-16, closed)");
    run("eeg_stop");
    check(result_code_is(0) && !eeg()->eeg_running,
          "lost stop: a second stop retries it, the converter stops, and the driver reports success");
    run("eeg_stop");
    check(result_code_is(0), "lost stop: and a stop with nothing recording is still accepted");
}

/* @verifies-none A lost start; np_mod_eeg.c's reply is verified by the hub suite. Simulator evidence only (REQ-SIMFW-06). */
static void test_a_lost_start_is_visible(void)
{
    setup();
    run("fail 0 eeg_start -17 1");
    run("eeg 0xFF 6 0 0 0");
    check(result_code_is(0) && !eeg()->eeg_running, "lost start: the driver reports success and the converter never started");
}

/* @verifies-none Telemetry is np_mod_eeg.c's; the hub suite verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_telemetry_reads_enabled_channels_and_the_dominant_band(void)
{
    setup();
    run("eeg_init");
    for (int ch = 0; ch < 8; ch++) {
        char t[40];
        (void)snprintf(t, sizeof t, "eeg_impedance %d %d", ch, 10 + ch);
        run(t);
    }
    run("eeg_band 0 -3");
    run("eeg_band 1 -9");
    run("eeg_band 2 4");
    run("eeg_band 3 -1");
    run("eeg_band 4 -12");
    run("eeg 0x03 6 0 0 0");
    run("eeg_sample");
    const char *line = last_with("\"ev\":\"eeg_sample\"");
    check(strstr(line, "\"impedance_kohm\":[10.0,11.0,0.0,0.0,0.0,0.0,0.0,0.0]") != NULL,
          "telemetry: impedance is read for enabled channels only");
    check(strstr(line, "\"dominant_band\":2") != NULL, "telemetry: the dominant band is the strongest of the five");
    check(strstr(line, "\"band_db\":[-3.0,-9.0,4.0,-1.0,-12.0]") != NULL, "telemetry: and each band's power is read");
    run("reading 0 eeg_dominant_band 4");
    run("eeg_sample");
    check(strstr(last_with("\"ev\":\"eeg_sample\""), "\"dominant_band\":4") != NULL, "telemetry: a pinned dominant band reaches the record");
    run("fail 0 eeg_read_impedance 0 -1");
    run("eeg_sample");
    check(strstr(last_with("\"ev\":\"eeg_sample\""), "\"impedance_kohm\":[-1.0,-1.0,0.0") != NULL,
          "telemetry: a failed impedance read reaches the record as a non-finite value");
}

/* @verifies-none Malformed command and calibration failure; np_mod_eeg.c's replies are the hub suite's. Simulator evidence only (REQ-SIMFW-06). */
static void test_malformed_command_and_failed_calibration(void)
{
    setup();
    run("eeg_raw 2");
    check(!result_code_is(0) && !eeg()->eeg_running, "malformed: a command of the wrong size is refused and nothing starts");
    run("fail 0 eeg_self_calibrate -17 1");
    run("eeg_init");
    check(eeg()->eeg_calibrations == 0U, "calibration: an injected failure means the calibration did not run");
    run("eeg_shutdown");
    check(result_code_is(0), "shutdown: with nothing recording it is a no-op");
}

/* @verifies-none An absent front end; the driver's behaviour is the hub suite's. Simulator evidence only (REQ-SIMFW-06). */
static void test_an_absent_front_end_takes_nothing(void)
{
    setup();
    run("eeg_absent");
    run("eeg 0xFF 6 0 0 0");
    check(!eeg()->eeg_running && reg(5) == 0U, "absent: no register changed and nothing started");
    check(count2("\"op\":\"eeg_spi_write\"", "\"source\":\"absent\"") >= 8, "absent: each write is reported as absent");
}

/* @verifies REQ-SIMFW-04 */
static void test_default_and_injected_answers_are_marked(void)
{
    setup();
    run("eeg_init");
    run("eeg_sample");
    check(count2("\"op\":\"eeg_spi_write\"", "\"source\":\"default\"") >= 11 &&
          count2("\"op\":\"eeg_read_impedance\"", "\"source\":\"default\"") == 8 &&
          count2("\"op\":\"eeg_band_power\"", "\"source\":\"default\"") == 5 &&
          count2("\"op\":\"eeg_self_calibrate\"", "\"source\":\"default\"") == 1,
          "marked: every answer from the default model says default");
    check(count("\"source\":\"injected\"") == 0, "marked: nothing is marked injected when nothing was injected");
    run("reading 0 eeg_read_impedance 99");
    run("eeg_sample");
    check(count2("\"op\":\"eeg_read_impedance\"", "\"source\":\"injected\"") == 8, "marked: a pinned reading is marked injected on every channel read");
}

/* @verifies REQ-SIMFW-11 */
static void test_a_module_answers_only_for_itself(void)
{
    setup();
    run("stim_add 0 5.0");                          /* a stimulation pair with the same index 0 */
    run("fail 0 eeg_start -17 1");
    run("bes 0 0 10000 500");
    check(np_sim_find(NP_SIM_KIND_STIM_PAIR, 0)->driven, "isolation: a fault armed on the front end does not touch the pair at index 0");
    run("fail 0 stim_dac_set -17 1");
    run("eeg 0xFF 6 0 0 0");
    check(!eeg()->eeg_running && count("\"op\":\"eeg_start\"") >= 1, "isolation: and the front end sees only its own armed fault");
}

int main(void)
{
    test_detect_reads_the_id_register();
    test_init_writes_the_power_on_sequence();
    test_control_configures_enabled_and_powers_down_the_rest();
    test_stop_and_a_lost_stop();
    test_a_lost_start_is_visible();
    test_telemetry_reads_enabled_channels_and_the_dominant_band();
    test_malformed_command_and_failed_calibration();
    test_an_absent_front_end_takes_nothing();
    test_default_and_injected_answers_are_marked();
    test_a_module_answers_only_for_itself();
    printf("%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
