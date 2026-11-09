/*
 * NeurOne Firmware-in-the-Loop Simulator — audio scenario tests (NP-SIM-FW-001 §8 step 3, REQ-SIMFW-13)
 *
 * HOST TEST CODE ONLY.  Runs the device's own np_mod_audio.c against the sim audio path, and asserts on what the
 * driver told the output stage and on the events the sim layer reports.
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


/* The audio path, with a clean mesh at 8 ohm.  The impedance is a test input, not a model default. */
static void setup(void)
{
    np_sim_set_sink(capture);
    np_sim_use_real_clock();
    np_simh_init();
    run("audio_add 8");
    run("audio_init");
    g_cap_n = 0;
}

static np_sim_module_t *au(void) { return np_sim_find(NP_SIM_KIND_AUDIO, 0U); }

/* @verifies-none Probe and codec bring-up are np_mod_audio.c's; the hub suite verifies them. Simulator evidence only (REQ-SIMFW-06). */
static void test_detect_and_codec_init(void)
{
    setup();
    check(au()->audio_inited, "init: the codec was brought up");
    run("audio_detect");
    check(field("\"ev\":\"audio_detect\"", "\"code\":") == 0.0, "detect: reports the audio path present (it reads nothing, the codec being fixed silicon)");
    run("fail 0 audio_codec_init -17 1");
    run("audio_init");
    check(!result_code_is(0) && field("\"ev\":\"result\"", "\"code\":") == -17.0, "init: a codec that fails to start is reported to the caller");
    run("audio_absent");
    run("audio_init");
    check(field("\"ev\":\"result\"", "\"code\":") != 0.0, "init: an absent codec is reported");
}

/* @verifies-none The mode switch is np_mod_audio.c's; the hub suite verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_each_mode_reaches_the_output_stage(void)
{
    setup();
    run("audio 0 200 10000 40 0 0");
    check(au()->audio_mode == 1U && au()->audio_carrier_hz == 200U && au()->audio_beat_mhz == 10000U && au()->audio_level_half_db == 40U,
          "mode: binaural carries carrier, beat and volume");
    run("audio 1 440 0 30 0 0");
    check(au()->audio_mode == 2U && au()->audio_carrier_hz == 440U, "mode: isochronic carries its frequency");
    run("audio 2 0 0 25 0 0");
    check(au()->audio_mode == 3U, "mode: 2 is pink noise");
    run("audio 3 0 0 25 0 0");
    check(au()->audio_mode == 4U, "mode: 3 is brown noise");
    run("audio 4 0 0 25 0 0");
    check(au()->audio_mode == 0U && !au()->driven, "mode: 4 stops the planar drivers");
    run("audio 9 0 0 25 0 0");
    check(au()->audio_mode == 0U, "mode: an unknown mode stops them too");
}

/* @verifies-none Bone conduction handling is np_mod_audio.c's; the hub suite verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_bone_conduction_follows_the_command(void)
{
    setup();
    run("audio 0 200 10000 40 1 0");
    check(au()->audio_bone_on && au()->audio_bone_hz == 200U && au()->audio_bone_level_half_db == 40U, "bone: on at the carrier and the volume");
    run("audio 0 200 10000 40 0 0");
    check(!au()->audio_bone_on && au()->audio_mode == 1U, "bone: a later command without it turns it off and leaves the planar drivers");
}

/* @verifies-none Stop is np_mod_audio.c's; the hub suite verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_stop_and_lost_stops(void)
{
    setup();
    run("audio 0 200 10000 40 1 0");
    run("audio_stop");
    check(au()->audio_mode == 0U && !au()->audio_bone_on && !au()->driven, "stop: planar drivers and bone transducer both stop");
    run("audio 0 200 10000 40 1 0");
    run("fail 0 audio_stop_planar -17 1");
    run("audio_stop");
    check(result_code_is(0) && au()->audio_mode == 1U, "lost stop: the driver reports success while the planar drivers keep playing");
    run("audio_stop");
    check(au()->audio_mode == 0U, "lost stop: a second stop lands (the bone path was cleared, the planar stop is called every time)");
    run("audio 0 200 10000 40 1 0");
    run("fail 0 audio_bone_stop -17 1");
    run("audio_stop");
    check(au()->audio_bone_on, "lost stop: a lost bone stop leaves the transducer on");
    run("audio_stop");
    check(au()->audio_bone_on, "lost stop: and the driver cleared its flag at the first stop, so it does not try the bone path again");
}

/* @verifies-none EEG-adaptive beat selection is np_mod_audio.c's; the hub suite verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_eeg_adaptive_beat_follows_the_band(void)
{
    setup();
    run("audio_eeg_band 2");
    check(au()->audio_mode == 0U, "adaptive: nothing is played for a band update while no session is active");
    run("audio 0 200 6000 40 0 1");
    run("audio_eeg_band 2");
    check(au()->audio_beat_mhz == 10000U, "adaptive: alpha selects a 10 Hz beat");
    run("audio_eeg_band 4");
    check(au()->audio_beat_mhz == 40000U, "adaptive: gamma selects a 40 Hz beat");
    run("audio_eeg_band 9");
    check(au()->audio_beat_mhz == 40000U, "adaptive: a band that is not one of the five changes nothing");
    run("audio 0 200 6000 40 0 0");
    run("audio_eeg_band 1");
    check(au()->audio_beat_mhz == 6000U, "adaptive: a command without it ignores band updates");
}

/* @verifies-none Characterisation of np_mod_audio.c's mode handling under EEG adaptation; the finding is OI-SIMFW-19. Simulator evidence only (REQ-SIMFW-06). */
static void test_adaptation_switches_a_non_binaural_mode_to_binaural(void)
{
    setup();
    run("audio 1 440 0 30 0 1");
    check(au()->audio_mode == 2U, "mode switch: isochronic is playing");
    run("audio_eeg_band 2");
    check(au()->audio_mode == 1U, "mode switch: an EEG band update turns it into binaural (OI-SIMFW-19)");
}

/* @verifies REQ-AUDIO-17 */
static void test_a_level_above_the_ceiling_is_refused_and_changes_nothing(void)
{
    setup();
    run("audio 0 200 10000 60 1 0");
    check(au()->audio_level_half_db == 60U, "ceiling: a 30 dB SPL command is applied");
    run("audio 0 200 10000 171 1 0");
    check(au()->audio_level_half_db == 60U && au()->audio_bone_level_half_db == 60U,
          "ceiling: 85.5 dB SPL is refused and neither output changes (OI-SIMFW-19)");
    run("audio 0 200 10000 200 1 0");
    check(au()->audio_level_half_db == 60U, "ceiling: 100 dB SPL is refused too, not clamped");
    run("audio 0 200 10000 170 1 0");
    check(au()->audio_level_half_db == 170U && au()->audio_bone_level_half_db == 170U,
          "ceiling: exactly 85 dB SPL is delivered to both outputs");
}

/* @verifies-none Malformed command; np_mod_audio.c's reply is the hub suite's. Simulator evidence only (REQ-SIMFW-06). */
static void test_malformed_command_and_shutdown(void)
{
    setup();
    run("audio_raw 3");
    check(field("\"ev\":\"result\"", "\"code\":") != 0.0 && au()->audio_mode == 0U, "malformed: a command of the wrong size is refused and nothing plays");
    run("audio 0 200 10000 40 1 0");
    run("audio_shutdown");
    check(au()->audio_mode == 0U && !au()->audio_bone_on, "shutdown: stops everything");
}

/* @verifies-none Telemetry is np_mod_audio.c's; the hub suite verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_telemetry_reads_the_mesh_and_returns_no_record(void)
{
    setup();
    run("audio_sample");
    check(field("\"ev\":\"audio_sample\"", "\"code\":") == 0.0, "telemetry: succeeds");
    check(count2("\"op\":\"audio_mesh_impedance\"", "\"source\":\"default\"") == 1, "telemetry: and reads the mesh once, from the model");
    run("reading 0 audio_mesh_impedance 900");
    run("audio_sample");
    check(count2("\"op\":\"audio_mesh_impedance\"", "\"source\":\"injected\"") == 1, "telemetry: a fouled mesh reading is marked injected");
}

/* @verifies REQ-SIMFW-04 */
static void test_default_and_injected_answers_are_marked(void)
{
    setup();
    run("audio 0 200 10000 40 1 0");
    check(count2("\"op\":\"audio_binaural\"", "\"source\":\"default\"") == 1 &&
          count2("\"op\":\"audio_bone_set\"", "\"source\":\"default\"") == 1,
          "marked: every answer from the default model says default");
    check(count("\"source\":\"injected\"") == 0, "marked: nothing is marked injected when nothing was injected");
    run("fail 0 audio_binaural -17 1");
    run("audio 0 200 10000 40 0 0");
    check(count2("\"op\":\"audio_binaural\"", "\"source\":\"injected\"") == 1, "marked: a swallowed call is marked injected");
}

/* @verifies REQ-SIMFW-11 */
static void test_a_module_answers_only_for_itself(void)
{
    setup();
    run("eeg_add");
    run("vns_add 1500");
    run("fail 0 audio_binaural -17 1");
    run("eeg 0xFF 6 0 0 0");
    run("vns 0 25000 2000 250 0 0 0");
    check(np_sim_find(NP_SIM_KIND_EEG, 0)->eeg_running && np_sim_find(NP_SIM_KIND_VNS, 0)->driven,
          "isolation: a fault armed on the audio path does not touch the EEG front end or the clip");
    run("audio 0 200 10000 40 0 0");
    check(au()->audio_mode == 0U, "isolation: and the audio path sees its own armed fault");
}

int main(void)
{
    test_detect_and_codec_init();
    test_each_mode_reaches_the_output_stage();
    test_bone_conduction_follows_the_command();
    test_stop_and_lost_stops();
    test_eeg_adaptive_beat_follows_the_band();
    test_adaptation_switches_a_non_binaural_mode_to_binaural();
    test_a_level_above_the_ceiling_is_refused_and_changes_nothing();
    test_malformed_command_and_shutdown();
    test_telemetry_reads_the_mesh_and_returns_no_record();
    test_default_and_injected_answers_are_marked();
    test_a_module_answers_only_for_itself();
    printf("%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
