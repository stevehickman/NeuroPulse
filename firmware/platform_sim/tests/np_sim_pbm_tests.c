/*
 * NeurOne Firmware-in-the-Loop Simulator — PBM spike tests (NP-SIM-FW-001 Rev 2 §8 step 1)
 *
 * HOST TEST CODE ONLY.  Runs the device's own np_socket_dispatch, np_module_map,
 * np_protocol, np_mod_pbm, np_pbm_socket_telem and np_safety_spi sources against
 * sim PBM modules, and asserts on the events the sim layer reports.
 *
 * What these tests are, and are not.  The tests that name REQ-SIMFW-04 and
 * REQ-SIMFW-11 verify the simulator.  The rest drive the firmware through a
 * whole-path scenario and assert what it did; the firmware's own behaviour is
 * verified by np_socket_dispatch_tests, np_mod_pbm_chlatch_tests and the rest of
 * the hub suite, and a run here is not verification evidence (REQ-SIMFW-06).
 * They exist to show that the scenarios the simulator is for are expressible.
 */
#include "np_sim_bus.h"
#include "np_sim_pbm_harness.h"

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

/* ── event capture ─────────────────────────────────────────────────────────── */

#define CAP_MAX   256
#define CAP_LINE  512
static char g_cap[CAP_MAX][CAP_LINE];
static int  g_cap_n;

static void capture(const char *line)
{
    if (g_cap_n < CAP_MAX) {
        strncpy(g_cap[g_cap_n], line, CAP_LINE - 1);
        g_cap[g_cap_n][CAP_LINE - 1] = '\0';
        g_cap_n++;
    }
}

static int count(const char *needle)
{
    int n = 0;
    for (int i = 0; i < g_cap_n; i++) { if (strstr(g_cap[i], needle) != NULL) { n++; } }
    return n;
}

static int count2(const char *a, const char *b)
{
    int n = 0;
    for (int i = 0; i < g_cap_n; i++) {
        if (strstr(g_cap[i], a) != NULL && strstr(g_cap[i], b) != NULL) { n++; }
    }
    return n;
}

static const char *last_result(void)
{
    for (int i = g_cap_n - 1; i >= 0; i--) {
        if (strstr(g_cap[i], "\"ev\":\"result\"") != NULL) { return g_cap[i]; }
    }
    return "";
}

static void run(const char *line)
{
    char buf[256];
    strncpy(buf, line, sizeof buf - 1);
    buf[sizeof buf - 1] = '\0';
    np_simh_line(buf);
}

/* Three base tiles at sockets 0..2, all reading the same temperature the test
 * supplies.  The temperature is a test input, not a model default. */
static void setup(void)
{
    np_sim_set_sink(capture);
    np_simh_init();
    g_cap_n = 0;
    run("add 0 34.0");
    run("add 1 34.0");
    run("add 2 34.0");
    g_cap_n = 0;
}

static bool driven(unsigned s)
{
    np_sim_module_t *m = np_sim_find((uint16_t)s);
    return m != NULL && m->driven;
}

static bool cranial_requested(void)
{
    return (np_safety_spi_get_requested_mask() & NP_SAFETY_EN_PBM_CRANIAL) != 0U;
}

/* ── tests ─────────────────────────────────────────────────────────────────── */

/* @verifies-none A baseline of the firmware path; np_socket_dispatch_tests verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_baseline_drive(void)
{
    setup();
    run("drive 0,1,2");
    check(strstr(last_result(), "\"code\":0") != NULL, "baseline: drive of three tiles returns OK");
    check(driven(0) && driven(1) && driven(2), "baseline: all three sim modules hold a commanded drive");
    check(cranial_requested(), "baseline: the hub requested the cranial enable");
}

/* @verifies REQ-SIMFW-04 */
static void test_default_answers_are_marked(void)
{
    setup();
    run("drive 0,1,2");
    check(count2("\"op\":\"socket_pwm_set\"", "\"source\":\"default\"") == 3,
          "unscripted: each pwm_set answered from the default model is marked default");
    check(count("\"source\":\"injected\"") == 0, "unscripted: nothing is marked injected when nothing was injected");
}

/* @verifies-none Scenario: one tile's write fails mid-command; np_socket_dispatch_tests verifies the rollback. Simulator evidence only (REQ-SIMFW-06). */
static void test_injected_write_fault_rolls_back(void)
{
    setup();
    run("fail 1 socket_pwm_set -17 1");
    run("drive 0,1,2");
    check(strstr(last_result(), "\"code\":-17") != NULL, "fault: the command reports the driver fault");
    check(!driven(0) && !driven(1) && !driven(2), "fault: no socket is left driven by the failed command");
    check(!cranial_requested(), "fault: the cranial enable is not requested after rollback");
    check(count2("\"id\":\"pbm_base:1\"", "\"source\":\"injected\"") >= 1,
          "fault: the injected answer is reported on socket 1");
    check(count("\"fn\":\"np_log_shdr_fault\"") >= 1, "fault: the firmware logged an SHDR fault event");
}

/* @verifies REQ-SIMFW-11 */
static void test_injection_answers_only_for_its_module(void)
{
    setup();
    run("add 5 34.0");
    run("fail 5 socket_pwm_set -17 -1");
    run("reading 5 socket_ntc_read 90.0");
    g_cap_n = 0;
    run("drive 0,1,2");
    check(strstr(last_result(), "\"code\":0") != NULL, "isolation: a fault armed on socket 5 does not affect a command on 0..2");
    check(count("\"id\":\"pbm_base:5\"") == 0, "isolation: socket 5 reported no answer to a command that did not address it");
    check(count("\"source\":\"injected\"") == 0, "isolation: no answer on 0..2 came from an injection");
    check(driven(0) && driven(1) && driven(2), "isolation: the addressed modules were driven");
}

/* @verifies-none Scenario: an over-temperature tile refuses its pre-drive check; np_mod_pbm_chlatch_tests and the telemetry tests verify it. Simulator evidence only (REQ-SIMFW-06). */
static void test_hot_tile_refused(void)
{
    setup();
    run("reading 2 socket_ntc_read 70.0");
    run("drive 0,1,2");
    check(strstr(last_result(), "\"code\":0") == NULL, "hot: the command with a 70 C tile does not succeed");
    check(!driven(2), "hot: the hot tile is not driven");
    check(!driven(0) && !driven(1), "hot: the cool tiles are not left driven by a failed command");
}

/* @verifies-none Scenario: a tile absent from the inventory fails placement before any write; np_socket_dispatch_tests verifies it. Simulator evidence only (REQ-SIMFW-06). */
static void test_absent_module_fails_placement_before_any_write(void)
{
    setup();
    run("absent 1");
    g_cap_n = 0;
    run("drive 0,1");
    check(strstr(last_result(), "\"code\":-3") != NULL, "absent: the command reports NOT_PRESENT");
    check(count("\"op\":\"socket_pwm_set\"") == 0, "absent: no socket was written to, including the present one");
    check(!driven(0), "absent: the present tile is not driven");
}

/* @verifies-none Scenario: the lost-stop backstop; np_socket_dispatch_tests verifies the firmware side. Simulator evidence only (REQ-SIMFW-06). */
static void test_lost_stop_releases_the_lattice_but_not_the_emitter(void)
{
    setup();
    run("drive 0,1");
    check(driven(0) && driven(1) && cranial_requested(), "lost stop: set-up, two tiles driven");
    run("fail 0 socket_pwm_set -17 -1");
    run("stop 0,1");
    check(!cranial_requested(), "lost stop: the hub released the cranial enable (the whole-lattice backstop)");
    check(!driven(1), "lost stop: the tile whose stop worked is dark");
    check(driven(0), "lost stop: the tile whose stop failed is still driven, which is what the backstop is for");
}

/* @verifies-none Scenario: the simulator takes its socket table from the generated layout, so a socket past the helmet's lattice cannot be added. Simulator evidence only (REQ-SIMFW-06). */
static void test_an_address_past_the_lattice_is_refused(void)
{
    setup();
    run("add 79 34.0");
    check(count("\"ev\":\"error\"") == 0, "layout: the last socket of the version 1 lattice can be added");
    run("add 80 34.0");
    check(count("\"ev\":\"error\"") == 1, "layout: the first index past the lattice is refused as not a valid address");
    check(np_sim_find(80) == NULL, "layout: no module exists at an address the helmet version does not have");
}

/* @verifies-none Scenario: every valid socket of the version 1 lattice driven by one command, which the 32-socket bound refused until NP-FW-HUB-001 Rev 35 (REQ-FWHUB-64). The firmware's own tests are np_pbm_socket_telem_tests and np_pbm_session_desc_tests. Simulator evidence only (REQ-SIMFW-06). */
static void test_the_whole_lattice_can_be_driven_at_once(void)
{
    setup();
    char add[32], drive[512];
    int n = 0;
    for (int s = 3; s < 80; s++) {                    /* sockets 0..2 were added by setup() */
        (void)snprintf(add, sizeof add, "add %d 34.0", s);
        run(add);
    }
    n += snprintf(drive, sizeof drive, "drive 0");
    for (int s = 1; s < 80; s++) { n += snprintf(drive + n, sizeof drive - (size_t)n, ",%d", s); }
    run(drive);
    check(strstr(last_result(), "\"code\":0") != NULL, "lattice: a command naming all 80 sockets is accepted");
    check(strstr(last_result(), "\"active\":80") != NULL, "lattice: the hub reports all 80 sockets active");
    bool all = true;
    for (unsigned s = 0; s < 80; s++) { if (!driven(s)) { all = false; } }
    check(all, "lattice: every one of the 80 sim modules holds a commanded drive");
    check(cranial_requested(), "lattice: the cranial enable is requested");

    /* The address domain is 128; the lattice has 80, and an address past it is refused as not present. */
    run("stop 0");
    (void)snprintf(drive, sizeof drive, "drive 80");
    run(drive);
    check(strstr(last_result(), "\"code\":-3") != NULL, "lattice: an address of the domain that the lattice lacks is refused as not present");
}

int main(void)
{
    test_baseline_drive();
    test_default_answers_are_marked();
    test_injected_write_fault_rolls_back();
    test_injection_answers_only_for_its_module();
    test_hot_tile_refused();
    test_absent_module_fails_placement_before_any_write();
    test_lost_stop_releases_the_lattice_but_not_the_emitter();
    test_an_address_past_the_lattice_is_refused();
    test_the_whole_lattice_can_be_driven_at_once();
    printf("%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures,
           g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
