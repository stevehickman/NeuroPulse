/*
 * NeurOne Firmware-in-the-Loop Simulator — T2 and accessory stub scenario tests (NP-SIM-FW-001 §8 step 3, REQ-SIMFW-13)
 *
 * HOST TEST CODE ONLY.  Runs the device's own np_mod_t2_stubs.c: six placeholder modules (21-channel qEEG, TMS, 1170 nm PBM,
 * clinical tACS, HD-tDCS, the vibrotactile pad) that have no hardware driver yet.  The module has no seam, so the scenario
 * is the call and the assertion is what it answered and what it asked of the safety MCU.
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


static const char *const NAMES[] = { "qeeg", "tms", "pbm_1170nm", "clin_tacs", "hd_tdcs", "vibrotactile" };
#define N_NAMES (sizeof NAMES / sizeof NAMES[0])

static void setup(void)
{
    np_sim_set_sink(capture);
    np_sim_use_real_clock();
    np_simh_init();
    g_cap_n = 0;
}

static void call(const char *name, const char *op)
{
    char t[64];
    (void)snprintf(t, sizeof t, "stub %s %s", name, op);
    run(t);
}

/* @verifies-none The stubs refuse; np_module_registry_tests verifies the table that holds them. Simulator evidence only (REQ-SIMFW-06). */
static void test_every_stub_refuses_and_requests_nothing(void)
{
    setup();
    bool refuse = true, quiet = true;
    for (unsigned i = 0; i < N_NAMES; i++) {
        const char *ops[] = { "detect", "init", "control", "telemetry" };
        for (unsigned k = 0; k < 4; k++) {
            call(NAMES[i], ops[k]);
            if (field("\"ev\":\"stub\"", "\"code\":") != (double)NP_HUB_ERR_NOT_PRESENT) { refuse = false; }
            if (field("\"ev\":\"stub\"", "\"requested_enables\":") != 0.0) { quiet = false; }
        }
    }
    check(refuse, "stubs: detect, init, control and telemetry of all six answer NOT_PRESENT");
    check(quiet, "stubs: and none of them requested a safety enable, so no T2 line is asked for");
    check(count("\"ev\":\"stub\"") == (int)(N_NAMES * 4U), "stubs: 24 calls were made and reported");
}

/* @verifies-none The stubs' shutdown; np_module_registry_tests verifies the table that holds them. Simulator evidence only (REQ-SIMFW-06). */
static void test_shutdown_succeeds_so_a_session_can_end(void)
{
    setup();
    bool ok = true;
    for (unsigned i = 0; i < N_NAMES; i++) {
        call(NAMES[i], "shutdown");
        if (field("\"ev\":\"stub\"", "\"code\":") != 0.0) { ok = false; }
    }
    check(ok, "shutdown: all six report success, which is what lets the runner stop a module it never started");
}

/* @verifies-none An unknown stub name is the harness's, not the firmware's. Simulator evidence only (REQ-SIMFW-06). */
static void test_an_unknown_name_is_not_a_stub(void)
{
    setup();
    run("stub nonsense detect");
    check(count("\"ev\":\"stub\"") == 0 && count("\"ev\":\"error\"") == 1, "unknown: a name that is not one of the six is an error, not a call");
}

int main(void)
{
    test_every_stub_refuses_and_requests_nothing();
    test_shutdown_succeeds_so_a_session_can_end();
    test_an_unknown_name_is_not_a_stub();
    printf("%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
