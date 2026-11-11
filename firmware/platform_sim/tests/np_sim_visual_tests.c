/*
 * NeurOne Firmware-in-the-Loop Simulator — visual-goggle scenario tests (NP-SIM-FW-001 §8 step 3, REQ-SIMFW-13)
 *
 * HOST TEST CODE ONLY.  Runs the device's own np_mod_visual.c against the sim goggles, including its Hall-edge
 * handler, which the sim delivers the way the platform interrupt would, and asserts on what the LEDs were told, the
 * enable the hub requested and the events the sim layer reports.
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


/* Goggles seated, eye open, MPE ok, handler armed by the driver's init.  The sensor readings are test inputs. */
static void setup(void)
{
    np_sim_set_sink(capture);
    np_sim_use_real_clock();
    np_simh_init();
    run("vis_add 1 1 1");
    run("vis_init");
    g_cap_n = 0;
}

static np_sim_module_t *vis(void) { return np_sim_find(NP_SIM_KIND_VISUAL, 0U); }
static bool enabled(void)         { return (np_safety_spi_get_requested_mask() & NP_SAFETY_EN_VISUAL) != 0U; }

/* @verifies-none The probe and the arming are np_mod_visual.c's; the hub suite verifies them. Simulator evidence only (REQ-SIMFW-06). */
static void test_detect_and_arming(void)
{
    setup();
    check(vis()->vis_isr != NULL, "init: the driver registered its not-seated handler");
    run("vis_detect");
    check(field("\"ev\":\"vis_detect\"", "\"code\":") == 0.0, "detect: seated goggles are detected");
    run("vis_sensor seated 0");
    run("vis_detect");
    check(field("\"ev\":\"vis_detect\"", "\"code\":") != 0.0, "detect: lifted goggles are not");
    run("vis_sensor seated 1");
    run("vis 0 2 50 0 63 0 0 0 0");
    check(result_code_is(0) && enabled(), "arming: with the handler armed a command is accepted");
}

/* @verifies-none Refusing to emit without an armed lift interrupt is np_mod_visual.c's; the hub suite verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_no_armed_handler_means_no_emission(void)
{
    setup();
    run("fail 0 vis_hall_register -17 1");
    run("vis_init");
    check(vis()->vis_isr == NULL, "unarmed: the registration was refused, so no handler is registered");
    run("vis 0 2 50 0 63 0 0 0 0");
    check(!result_code_is(0) && !enabled() && !vis()->vis_emitting, "unarmed: and the goggles are never driven");
}

/* @verifies-none Modes are np_mod_visual.c's; the hub suite verifies them. Simulator evidence only (REQ-SIMFW-06). */
static void test_each_mode_reaches_the_leds(void)
{
    setup();
    run("vis 0 2 50 0 63 0 0 0 0");
    check(vis()->vis_emitting && vis()->vis_zone_mask == 63U && vis()->vis_freq_hz == 2U && vis()->vis_duty_pct == 50U && vis()->vis_wl == 0U,
          "photic: zones, frequency, duty and wavelength reach the LEDs");
    run("vis_stop");
    run("vis 0 2 50 0 255 15 0 0 0");
    check(vis()->vis_zone_mask == 0x0FFFU, "photic: the 12-bit zone mask is the low byte plus the high nibble");
    run("vis_stop");
    run("vis 1 0 0 0 0 0 20 0 0");
    check(vis()->vis_emdr_on && vis()->vis_emdr_rate_mhz == 20U && !vis()->vis_emitting, "emdr: alternation runs and the photic LEDs do not");
    run("vis_stop");
    run("vis 2 0 40 0 7 0 0 0 0");
    check(vis()->vis_emitting && vis()->vis_wl == 1U && vis()->vis_freq_hz == 0U, "retinal: 808 nm continuous");
    run("vis_stop");
    run("vis 3 2 50 0 63 0 20 0 0");
    check(vis()->vis_emitting && vis()->vis_emdr_on, "combined: both");
    run("vis_stop");
    check(!vis()->driven && !enabled(), "stop: the LEDs go dark and the enable drops");
}

/* @verifies-none The 3 to 30 Hz refusal (REQ-FWHUB-65) is np_mod_visual.c's; the hub suite verifies it. Simulator evidence only (REQ-SIMFW-06). The other photic tests drive at 2 Hz, outside it, so the goggles are the shipped ones. */
static void test_photic_driving_in_the_ppx_zone_is_refused(void)
{
    setup();
    run("vis 0 10 50 0 63 0 0 0 0");
    check(!result_code_is(0) && !enabled() && !vis()->vis_emitting, "zone: photic at 10 Hz is refused and nothing is driven, as on the device");
    run("vis 3 30 50 0 63 0 20 0 0");
    check(!result_code_is(0) && !enabled() && !vis()->vis_emitting, "zone: combined at 30 Hz is refused too");
    run("vis 0 31 50 0 63 0 0 0 0");
    check(result_code_is(0) && vis()->vis_emitting, "zone: 31 Hz is outside it and drives");
}

/* @verifies-none The interlocks are np_mod_visual.c's; the hub suite verifies them. Simulator evidence only (REQ-SIMFW-06). */
static void test_the_interlocks_refuse(void)
{
    setup();
    run("vis_sensor seated 0");
    run("vis 0 2 50 0 63 0 0 0 0");
    check(!result_code_is(0) && !enabled() && !vis()->vis_emitting, "interlock: goggles not seated refuses emission");
    run("vis_sensor seated 1");
    run("vis_sensor mpe 0");
    run("vis 0 2 50 0 63 0 0 0 0");
    check(!result_code_is(0) && !enabled() && !vis()->vis_emitting, "interlock: an MPE check that fails refuses emission");
    run("vis_sensor mpe 1");
    run("vis_sensor eye 0");
    run("vis 2 0 40 0 7 0 0 0 0");
    check(!result_code_is(0) && !enabled() && !vis()->vis_emitting, "interlock: a retinal mode needs the eye open");
    run("vis 0 2 50 0 63 0 0 0 0");
    check(result_code_is(0) && vis()->vis_emitting, "interlock: a photic mode does not");
    run("vis_stop");
    run("vis 0 2 50 0 63 0 0 1 0");
    check(result_code_is(0), "interlock: the Mode F flag is ignored while NP_MODE_F_REGULATORY_CLEARED is 0, so it adds no eye check and no emission");
}

/* @verifies-none A faulted sensor reads as the refusing answer; np_mod_visual.c's use of it is the hub suite's. Simulator evidence only (REQ-SIMFW-06). */
static void test_a_faulted_sensor_reads_as_refusal(void)
{
    setup();
    run("fail 0 vis_seated -17 1");
    run("vis 0 2 50 0 63 0 0 0 0");
    check(!result_code_is(0) && !enabled(), "fault: a failed Hall read refuses emission");
    run("fail 0 vis_mpe -17 1");
    run("vis 0 2 50 0 63 0 0 0 0");
    check(!result_code_is(0) && !enabled(), "fault: a failed MPE read refuses emission");
    run("vis_absent");
    run("vis 0 2 50 0 63 0 0 0 0");
    check(!result_code_is(0) && !enabled(), "fault: absent goggles refuse emission");
}

/* @verifies-none The Hall edge cut is np_mod_visual.c's (REQ-FWHUB-21); np_mod_visual_tests verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_the_hall_edge_cuts_emission_at_once(void)
{
    setup();
    run("vis 0 2 50 0 63 0 0 0 0");
    check(vis()->vis_emitting && enabled(), "edge: emitting");
    run("vis_lift");
    check(!vis()->vis_emitting && !enabled() && vis()->vis_edges_delivered == 1U,
          "edge: the handler the driver registered cut the LEDs and dropped the enable, with no telemetry poll in between");
    run("vis_lower");
    check(!vis()->vis_emitting && !enabled(), "edge: lowering the goggle again does not resume emission");
    run("vis_sample");
    check(strstr(last_with("\"ev\":\"vis_sample\""), "\"goggle_seated\":true") != NULL, "edge: and telemetry reports it seated again");
    run("vis 0 2 50 0 63 0 0 0 0");
    check(result_code_is(0) && vis()->vis_emitting, "edge: only a new command resumes it");
}

/* @verifies-none The poll backstop is np_mod_visual.c's; np_mod_visual_tests verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_a_missed_edge_is_caught_only_by_the_poll(void)
{
    setup();
    run("vis 0 2 50 0 63 0 0 0 0");
    run("vis_lift_silent");
    check(vis()->vis_emitting && enabled(), "missed edge: with no edge delivered the goggles keep emitting while lifted");
    run("vis_sample");
    check(!vis()->vis_emitting && !enabled(), "missed edge: the telemetry poll is what cuts it");
}

/* @verifies-none The latch is np_mod_visual.c's; np_mod_visual_tests verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_a_lift_between_the_read_and_the_enable_is_caught_by_the_latch(void)
{
    setup();
    run("vis_lift_during_command");
    run("vis 0 2 50 0 63 0 0 0 0");
    check(!result_code_is(0), "race: a lift between the seated read and the enable makes the command fail");
    check(!vis()->vis_emitting && !enabled(), "race: the LEDs are dark and the enable is down although led_set and the enable ran after the edge");
    check(count("\"ev\":\"hall_edge\"") == 1 && vis()->vis_edges_delivered == 1U, "race: through exactly one delivered edge");
}

/* @verifies REQ-FWHUB-75 */
static void test_a_failed_led_command_fails_closed(void)
{
    setup();
    run("fail 0 vis_led_set -17 1");
    run("vis 0 2 50 0 63 0 0 0 0");
    check(!result_code_is(0) && !enabled() && !vis()->vis_emitting,
          "led fault: the LED command fails, the driver returns the failure, requests no enable and leaves nothing lit (OI-SIMFW-20, fixed)");
    run("vis 0 2 50 0 63 0 0 0 0");
    check(result_code_is(0) && enabled() && vis()->vis_emitting, "led fault: the injection was one call; the next command lands");
}

/* @verifies-none Stop; np_mod_visual.c's reply is the hub suite's. Simulator evidence only (REQ-SIMFW-06). */
static void test_a_lost_led_stop_leaves_the_leds_lit(void)
{
    setup();
    run("vis 0 2 50 0 63 0 0 0 0");
    run("fail 0 vis_led_stop -17 1");
    run("vis_stop");
    check(result_code_is(0) && !enabled() && vis()->vis_emitting,
          "lost stop: the enable drops and the driver reports success while the LEDs stay lit; the safety MCU's enable is what stops them (not in the loop)");
}

/* @verifies-none The halt is np_mod_visual.c's; the hub suite verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_the_photoparoxysmal_halt(void)
{
    setup();
    run("vis 0 2 50 0 63 0 0 0 0");
    run("vis_ppx");
    check(!vis()->vis_emitting && !enabled(), "ppx: the LEDs go dark and the enable drops");
    check(count("\"ev\":\"runner_event\"") == 0, "ppx: the driver asks the device's own runner to hold the session, which has no session running, so it posts nothing (the runner is linked, OI-SIMFW-43)");
    run("vis 0 2 50 0 63 0 0 0 0");
    check(!result_code_is(0) && !vis()->vis_emitting, "ppx: a later command is refused");
    run("vis_sample");
    check(strstr(last_with("\"ev\":\"vis_sample\""), "\"ppx_halt\":true") != NULL, "ppx: telemetry reports the halt");
    run("vis_init");
    run("vis 0 2 50 0 63 0 0 0 0");
    check(result_code_is(0), "ppx: only a new init clears it");
}

/* @verifies-none Malformed command and shutdown; np_mod_visual.c's replies are the hub suite's. Simulator evidence only (REQ-SIMFW-06). */
static void test_malformed_command_and_shutdown(void)
{
    setup();
    run("vis_raw 3");
    check(!result_code_is(0) && !vis()->vis_emitting, "malformed: a command of the wrong size is refused");
    run("vis 0 2 50 0 63 0 0 0 0");
    run("vis_shutdown");
    check(result_code_is(0) && !vis()->vis_emitting && !enabled(), "shutdown: stops everything");
}

/* @verifies REQ-SIMFW-04 */
static void test_default_and_injected_answers_are_marked(void)
{
    setup();
    run("vis 0 2 50 0 63 0 0 0 0");
    check(count2("\"op\":\"vis_seated\"", "\"source\":\"default\"") >= 1 && count2("\"op\":\"vis_mpe\"", "\"source\":\"default\"") == 1 &&
          count2("\"op\":\"vis_led_set\"", "\"source\":\"default\"") == 1,
          "marked: every answer from the default model says default");
    check(count("\"source\":\"injected\"") == 0, "marked: nothing is marked injected when nothing was injected");
    run("reading 0 vis_eye_open 0");
    run("vis_sample");
    check(count2("\"op\":\"vis_eye_open\"", "\"source\":\"injected\"") == 1, "marked: a pinned sensor reading is marked injected");
}

/* @verifies REQ-SIMFW-11 */
static void test_a_module_answers_only_for_itself(void)
{
    setup();
    run("audio_add 8");
    run("eeg_add");
    run("fail 0 vis_mpe -17 1");
    run("audio 0 200 10000 40 0 0");
    run("eeg 0xFF 6 0 0 0");
    check(np_sim_find(NP_SIM_KIND_AUDIO, 0)->driven && np_sim_find(NP_SIM_KIND_EEG, 0)->eeg_running,
          "isolation: a fault armed on the goggles does not touch the audio path or the EEG front end");
    run("vis 0 2 50 0 63 0 0 0 0");
    check(!result_code_is(0), "isolation: and the goggles see their own armed fault");
}

int main(void)
{
    test_detect_and_arming();
    test_no_armed_handler_means_no_emission();
    test_each_mode_reaches_the_leds();
    test_photic_driving_in_the_ppx_zone_is_refused();
    test_the_interlocks_refuse();
    test_a_faulted_sensor_reads_as_refusal();
    test_the_hall_edge_cuts_emission_at_once();
    test_a_missed_edge_is_caught_only_by_the_poll();
    test_a_lift_between_the_read_and_the_enable_is_caught_by_the_latch();
    test_a_failed_led_command_fails_closed();
    test_a_lost_led_stop_leaves_the_leds_lit();
    test_the_photoparoxysmal_halt();
    test_malformed_command_and_shutdown();
    test_default_and_injected_answers_are_marked();
    test_a_module_answers_only_for_itself();
    printf("%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
