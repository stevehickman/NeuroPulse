/*
 * NeurOne Firmware-in-the-Loop Simulator — hub drivers commanding the safety MCU model (NP-SIM-FW-001 §24, REQ-SIMFW-07)
 *
 * HOST TEST CODE ONLY.  np_sim_safety_tests drives the safety MCU with the hub's request mask and currents set directly
 * (np_safety_spi_request_enable).  These scenarios command the hub's own module drivers instead (a PBM tile, a tDCS pair, the
 * goggles), so the request the safety MCU sees is the one the driver made, and assert the line at the pin: a module the hub
 * commands is not driven unless the safety MCU's line for it is asserted, and a line the MCU releases stays released while the hub
 * goes on commanding.  The scenario plays the heartbeat task and the app (np_sim_safety_harness.c).
 *
 * What these tests are, and are not.  Virtual clock, the MCU's register drivers replaced, a pass is NOT verification evidence
 * (REQ-SIMFW-06).  Each scenario runs in a forked child: the MCU keeps file-scope state a cutoff latches.
 */
#include "np_sim_bus.h"
#include "np_sim_pbm_harness.h"
#include "np_sim_safety.h"
#include "np_sim_test_util.h"
#include "np_sim_safety_test_consts.h"

#include "np_hub_types.h"
#include "np_safety_spi.h"

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
        scenario();
        fflush(stdout);
        exit(g_failures == 0 ? 0 : 1);
    }
    int st = 0;
    (void)waitpid(pid, &st, 0);
    if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) { g_failures++; printf("FAIL: scenario child did not finish cleanly\n"); }
}

#define LINE_VIS  4
#define NTC_HOT   600U           /* hotter than the 62 C row (650) of the firmware's table (np_thermal_interlock.c): a test input */

static void boot(void)
{
    np_sim_set_sink(capture);
    np_sim_use_real_clock();
    np_simh_init();
    np_sim_governor_open_for_test(true);          /* the tile driver behind the shipped governor, which refuses every load (OI-SIMFW-11) */
    run("safety_root 7");
    run("safety_attach");
}

/* The session as the hub starts it, after the module's own command has made its request. */
static void sign_in(void)
{
    run("safety_run 1000 3");
    run("safety_sig good");
}

static bool has(const char *line, const char *s) { return strstr(line, s) != NULL; }
static unsigned granted(void) { return np_safety_spi_get_granted_mask(); }
static unsigned status(void)  { return np_safety_spi_get_status(); }
static bool commanded(const char *id)
{
    char needle[64];
    (void)snprintf(needle, sizeof needle, "\"id\":\"%s\"", id);
    return has(last_with(needle), "\"driven\":true");
}

/* @verifies REQ-SIMFW-07 */
static void test_a_tile_is_driven_only_while_its_line_is_asserted(void)
{
    boot();
    run("add 0 34.0");
    run("drive 0");
    check((np_safety_spi_get_requested_mask() & 1U) != 0U, "the tile driver's command made the cranial PBM request");
    check(!np_sim_safety_line_enabled(LINE_PBM), "and before the session is signed no line is asserted");
    sign_in();
    run("safety_run 400 3");
    check(np_sim_safety_line_enabled(LINE_PBM) && (granted() & 1U) != 0U, "signed: the cranial line is asserted and granted");
    run("status");
    check(commanded("pbm_base:0"), "and the hub reports the tile commanded");
    run("stop 0");
    run("safety_run 400 3");
    check(!np_sim_safety_line_enabled(LINE_PBM) && (granted() & 1U) == 0U, "a stop withdraws the request, so the grant and the line go");
}

/* @verifies REQ-SIMFW-07 */
static void test_a_hot_zone_releases_the_line_while_the_hub_still_commands_the_tile(void)
{
    boot();
    run("add 0 34.0");
    run("drive 0");
    sign_in();
    run("safety_run 400 3");
    check(np_sim_safety_line_enabled(LINE_PBM), "the tile runs with its line asserted");
    np_sim_safety_set_adc(0U, NTC_HOT);
    run("safety_run 400 3");
    run("status");
    check(commanded("pbm_base:0"), "the hub still commands the tile");
    check(!np_sim_safety_line_enabled(LINE_PBM), "the safety MCU has released its line");
    check((status() & STATUS_THERMAL) != 0U, "and the status says THERMAL");
}

/* @verifies REQ-SIMFW-07 */
static void test_a_tdcs_command_is_granted_after_its_declaration_and_meets_the_charge_ceiling(void)
{
    boot();
    np_sim_safety_set_impedance_ohm(1U, 5000U);
    run("stim_add 0 5.0");
    run("tdcs 0 2000 0 30 1000");                 /* 2 mA, a declared 1 cm2 electrode: a 150 mC session ceiling */
    sign_in();
    run("safety_wave 6 1 0");                     /* the runner's declaration, sent after the session starts */
    run("safety_limits 6 1000");
    run("safety_run 1000 3");
    check(np_sim_safety_line_enabled(LINE_TDCS), "the declared tDCS channel is granted and its line asserted");
    run("safety_run 140000 3");
    check(!np_sim_safety_line_enabled(LINE_TDCS), "past the session ceiling for the declared area the line is released");
    check((status() & STATUS_CHARGE) != 0U, "and the status says CHARGE");
    run("status");
    check(commanded("stim_pair:0"), "while the hub still commands the pair");
}

/* @verifies REQ-SIMFW-07 */
static void test_the_visual_line_follows_the_goggles(void)
{
    boot();
    run("vis_add 1 1 1");
    run("vis_init");
    run("vis 0 40 50 0 63 0 0 0 0");              /* 40 Hz: outside the 3-30 Hz refusal (REQ-FWHUB-65) */
    sign_in();
    run("safety_run 400 3");
    check(np_sim_safety_line_enabled(LINE_VIS), "a photic drive outside the refused band is granted and its line asserted");
    run("vis_lift");
    run("safety_run 400 3");
    check(!np_sim_safety_line_enabled(LINE_VIS), "after the lift edge the request is withdrawn and the line is released");
}

int main(void)
{
    in_child(test_a_tile_is_driven_only_while_its_line_is_asserted);
    in_child(test_a_hot_zone_releases_the_line_while_the_hub_still_commands_the_tile);
    in_child(test_a_tdcs_command_is_granted_after_its_declaration_and_meets_the_charge_ceiling);
    in_child(test_the_visual_line_follows_the_goggles);
    printf("%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
