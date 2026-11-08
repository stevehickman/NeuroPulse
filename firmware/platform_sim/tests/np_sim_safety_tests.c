/*
 * NeurOne Firmware-in-the-Loop Simulator — safety MCU in the loop, scenario tests (NP-SIM-FW-001 §22, OI-SIMFW-09, REQ-SIMFW-14)
 *
 * HOST TEST CODE ONLY.  Runs the device's own safety MCU (firmware/safety_mcu/src, SW-01 Class C, unmodified) behind the hub's own
 * np_safety_spi.c, over the simulator's SPI link and HAL, and drives each interlock to its cutoff.  The scenario plays the heartbeat
 * task and the app (the signing of a session hash).  The clock is virtual: one polling iteration of the MCU per millisecond.
 *
 * Each scenario runs in a forked child.  The MCU's modules keep their state in file-scope statics, which a power cycle clears on the
 * device and a thread restart would not, so a clean MCU is a new process.  The flash and the OTP are shared memory the children
 * inherit, so a scenario can power-cycle: one child leaves its flash, the next boots from it.
 *
 * What these tests are, and are not.  They verify the simulator's link and HAL, and they show what the device's code does through
 * a whole path on the host.  The firmware's own behaviour is verified by the safety MCU's host suites and on hardware, and a run
 * here is not verification evidence (REQ-SIMFW-06).
 */
#include "np_sim_bus.h"
#include "np_sim_pbm_harness.h"
#include "np_sim_safety.h"
#include "np_sim_test_util.h"

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

/* Run a scenario in a child process; the child's failures are the parent's. */
static void in_child(void (*scenario)(void))
{
    fflush(stdout);
    const pid_t pid = fork();
    if (pid == 0) {
        scenario();
        fflush(stdout);
        _exit(g_failures == 0 ? 0 : 1);
    }
    int st = 0;
    (void)waitpid(pid, &st, 0);
    if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) { g_failures++; printf("FAIL: scenario child did not finish cleanly\n"); }
}

#define RUNNING 3
#define SIG_PENDING 0x80U
#define STATUS_FAULT 0x01U
#define STATUS_WATCHDOG 0x02U
#define STATUS_CUTOFF 0x04U
#define STATUS_THERMAL 0x10U
#define STATUS_CHARGE 0x20U
#define STATUS_CARDIAC 0x40U

#define LINE_PBM 0
#define LINE_BES 1
#define LINE_TDCS 2
#define LINE_CVNS 6

#define EN_PBM  (1U << 0)
#define EN_BES  (1U << 5)
#define EN_TDCS (1U << 6)
#define EN_CVNS (1U << 10)

static void say(const char *fmt, unsigned a, unsigned b)
{
    printf("      ");
    printf(fmt, a, b);
    printf("\n");
}

/* A fresh MCU: the OTP root key programmed (a test seed), a tier record if asked, then attached. */
static void boot(bool root, unsigned tier)
{
    np_sim_set_sink(capture);
    np_sim_use_real_clock();
    np_simh_init();
    if (root) { run("safety_root 7"); }
    if (tier != 0U) {
        run("safety_uid 9");
        char l[32];
        (void)snprintf(l, sizeof l, "safety_tier %u 1", tier);
        run(l);
    }
    run("safety_attach");
}

/* A session as the hub starts it: a running heartbeat, the signature once the MCU has seen the session begin, then a beat. */
static void start_session(void)
{
    run("safety_run 1000 3");
    run("safety_sig good");
    run("safety_run 400 3");
}

static void declare_dc(unsigned ch, unsigned area_mcm2)
{
    char l[48];
    (void)snprintf(l, sizeof l, "safety_wave %u 1 0", ch);
    run(l);
    (void)snprintf(l, sizeof l, "safety_limits %u %u", ch, area_mcm2);
    run(l);
}

static unsigned granted(void) { return np_safety_spi_get_granted_mask(); }
static unsigned status(void)  { return np_safety_spi_get_status(); }

/* ── scenarios ────────────────────────────────────────────────────────────────────────────────────────────────────── */

/* @verifies REQ-SIMFW-14#a */
static void scn_the_link_reports_absence_until_the_mcu_is_attached(void)
{
    np_sim_set_sink(capture);
    np_simh_init();
    check(strcmp(np_sim_safety_label(), "not_in_loop") == 0, "before attach: the label says the safety MCU is not in the loop");
    const np_hub_status_t rc = np_safety_spi_heartbeat(NP_SESSION_IDLE, 0U, NULL, 0U);
    check(rc == NP_HUB_ERR_TIMEOUT, "before attach: a heartbeat times out, as a missing MCU does");
    check(count("\"symbol\":\"np_safety_hal_spi_transfer\"") >= 1, "and the event says the link is unrouted");
    run("safety_root 7");
    run("safety_attach");
    check(strcmp(np_sim_safety_label(), "in_loop") == 0, "after attach: the label says in_loop");
    check(np_sim_safety_attached(), "after attach: the MCU is attached");
}

/* @verifies REQ-SIMFW-14#b */
static void scn_the_mcu_boots_with_every_enable_disabled(void)
{
    boot(true, 0U);
    bool any = false;
    for (unsigned i = 0U; i < NP_SIM_SAFETY_LINES; i++) { any = any || np_sim_safety_line_enabled((uint8_t)i); }
    check(!any, "boot: all ten enable lines are disabled (HIGH)");
    run("safety_run 1000 0");
    check(status() == 0U && granted() == 0U, "idle heartbeats: no fault, nothing granted");
    np_sim_safety_info_t i;
    np_sim_safety_info(&i);
    check(i.iterations >= 1000U, "the MCU's polling loop ran once per virtual millisecond");
    check(i.iwdg_resets == 0U, "and the independent watchdog was refreshed throughout");
}

/* @verifies REQ-SIMFW-14#b */
static void scn_a_signed_session_is_granted_and_an_unsigned_one_is_not(void)
{
    boot(true, 0U);
    run("safety_run 1000 3");
    check((status() & SIG_PENDING) != 0U, "a running session is held SIG_PENDING until its signature arrives");
    np_safety_spi_request_enable(EN_PBM);
    run("safety_run 400 3");
    check(granted() == 0U && !np_sim_safety_line_enabled(LINE_PBM), "unsigned: the requested line is not granted and stays disabled");
    run("safety_sig good");
    run("safety_run 1000 3");
    check((status() & SIG_PENDING) == 0U, "a good signature clears SIG_PENDING");
    check((granted() & EN_PBM) != 0U && np_sim_safety_line_enabled(LINE_PBM), "and the line is granted and driven LOW");
}

/* @verifies REQ-SIMFW-14#b */
static void scn_a_bad_signature_is_a_cutoff(void)
{
    boot(true, 0U);
    run("safety_run 1000 3");
    run("safety_sig bad");
    np_safety_spi_request_enable(EN_PBM);
    run("safety_run 600 3");
    check((status() & STATUS_FAULT) != 0U && (status() & STATUS_CUTOFF) != 0U, "a bad signature sets FAULT and CUTOFF");
    check(granted() == 0U && !np_sim_safety_line_enabled(LINE_PBM), "and nothing is granted");
    np_sim_safety_info_t i;
    np_sim_safety_info(&i);
    check(i.last_reply_fault_slot == 0xFDU, "the fault slot names the signature failure (0xFD)");
}

/* @verifies REQ-SIMFW-14#b */
static void scn_an_unprovisioned_otp_refuses_every_session(void)
{
    boot(false, 0U);
    run("safety_run 1000 3");
    run("safety_sig good");
    np_safety_spi_request_enable(EN_PBM);
    run("safety_run 600 3");
    np_sim_safety_info_t i;
    np_sim_safety_info(&i);
    check(i.last_reply_fault_slot == 0xFEU, "erased OTP: the fault slot says unprovisioned (0xFE)");
    check(granted() == 0U, "and nothing is granted");
}

/* @verifies REQ-SIMFW-14#c */
static void scn_losing_the_heartbeat_cuts_every_line_at_the_watchdog_timeout(void)
{
    boot(true, 0U);
    start_session();
    np_safety_spi_request_enable(EN_PBM);
    run("safety_run 400 3");
    check(np_sim_safety_line_enabled(LINE_PBM), "granted and driven before the heartbeat stops");
    const uint32_t last = np_sim_now_ms();
    /* The last beat of safety_run went out at the start of its final 200 ms; the MCU saw it within a millisecond. */
    uint32_t cut_ms = 0U;
    for (uint32_t ms = 1U; ms <= 3000U; ms++) {
        np_sim_advance_ms(1U);
        if (!np_sim_safety_line_enabled(LINE_PBM)) { cut_ms = ms; break; }
    }
    say("the line was cut %u ms after the last heartbeat period ended (last at t=%u)", cut_ms, last);
    check(cut_ms != 0U, "the line is cut with no heartbeat");
    /* 1500 ms from the beat; that beat was 200 ms before `last`. */
    check(cut_ms >= 1200U && cut_ms <= 1400U, "and the cut falls at the 1.5 s watchdog, counted from the last heartbeat");
}

/* @verifies REQ-SIMFW-14#c */
static void scn_a_hung_loop_leaves_the_enables_until_the_independent_watchdog_expires(void)
{
    boot(true, 0U);
    start_session();
    np_safety_spi_request_enable(EN_PBM);
    run("safety_run 400 3");
    check(np_sim_safety_line_enabled(LINE_PBM), "granted and driven before the loop hangs");
    run("safety_fault stall 1");
    np_sim_advance_ms(900U);
    check(np_sim_safety_line_enabled(LINE_PBM), "900 ms into the hang the line is still driven: nothing in the loop can cut it");
    np_sim_advance_ms(200U);
    np_sim_safety_info_t i;
    np_sim_safety_info(&i);
    check(i.held_in_reset && i.iwdg_resets == 1U, "the independent watchdog resets the MCU");
    check(!np_sim_safety_line_enabled(LINE_PBM), "and the reset path holds every enable disabled");
    check(np_safety_spi_heartbeat(NP_SESSION_RUNNING, EN_PBM, NULL, 0U) == NP_HUB_ERR_TIMEOUT, "and the hub's heartbeat times out");
}

/* @verifies REQ-SIMFW-14#c */
static void scn_the_dc_charge_ceiling_cuts_the_channel(void)
{
    boot(true, 0U);
    np_sim_safety_set_impedance_ohm(1U, 5000U);                 /* tDCS contact: inside the window */
    run("safety_run 1000 3");
    run("safety_sig good");
    declare_dc(6U, 100U);                                       /* DC, a 0.1 cm2 electrode: 15 mC for the session */
    np_safety_spi_request_enable(EN_TDCS);
    np_safety_spi_set_channel_current(6U, 2000U);               /* 2 mA */
    run("safety_run 1000 3");
    check((granted() & EN_TDCS) != 0U && np_sim_safety_line_enabled(LINE_TDCS), "tDCS is granted and driven while under its budget");
    const uint32_t t0 = np_sim_now_ms();
    uint32_t cut = 0U;
    for (int beat = 0; beat < 80 && cut == 0U; beat++) {
        run("safety_beat 3");
        np_sim_advance_ms(200U);
        if (!np_sim_safety_line_enabled(LINE_TDCS)) { cut = np_sim_now_ms() - t0; }
    }
    say("tDCS cut %u ms after it was driven (budget 15 mC at 2 mA = 7500 ms; status %u)", cut, status());
    check(cut != 0U, "the DC ceiling cuts the channel");
    check(cut >= 6000U && cut <= 8500U, "at the commanded-dose limit");
    run("safety_beat 3");
    check((status() & STATUS_CHARGE) != 0U, "and the status says CHARGE");
}

/* @verifies REQ-SIMFW-14#c */
static void scn_the_per_phase_ceiling_cuts_a_pulsed_channel_and_passes_one_inside_it(void)
{
    boot(true, 0U);
    np_sim_safety_set_impedance_ohm(2U, 5000U);                 /* BES/tACS contact */
    run("safety_run 1000 3");
    run("safety_sig good");
    run("safety_wave 5 2 20000");                               /* pulsed, 20 ms phase */
    run("safety_limits 5 1000");                                /* 1 cm2: 40 uC per phase */
    np_safety_spi_request_enable(EN_BES);
    np_safety_spi_set_channel_current(5U, 1000U);               /* 1 mA x 20 ms = 20 uC */
    run("safety_run 800 3");
    check(np_sim_safety_line_enabled(LINE_BES), "1 mA over a 20 ms phase (20 uC) is inside 40 uC/cm2 and is driven");
    run("safety_wave 5 2 60000");                               /* the declared phase grows to 60 ms: 60 uC */
    run("safety_run 800 3");
    check(!np_sim_safety_line_enabled(LINE_BES), "the same current over a 60 ms phase (60 uC) is over the ceiling and is cut");
    check((status() & STATUS_CHARGE) != 0U, "and the status says CHARGE");
}

/* @verifies REQ-SIMFW-14#c */
static void scn_an_undeclared_electrical_channel_is_held_off(void)
{
    boot(true, 0U);
    np_sim_safety_set_impedance_ohm(1U, 5000U);
    run("safety_run 1000 3");
    run("safety_sig good");
    np_safety_spi_request_enable(EN_TDCS);
    np_safety_spi_set_channel_current(6U, 1000U);
    run("safety_run 800 3");
    check(!np_sim_safety_line_enabled(LINE_TDCS), "no waveform class declared: the channel is held off");
    declare_dc(6U, 1000U);
    run("safety_run 800 3");
    check(np_sim_safety_line_enabled(LINE_TDCS), "declared DC with its area: the channel is driven");
}

/* @verifies REQ-SIMFW-14#c */
static void scn_the_impedance_gate_withholds_a_channel_until_the_contact_passes(void)
{
    boot(true, 0U);
    run("safety_run 1000 3");
    run("safety_sig good");
    declare_dc(6U, 1000U);
    np_safety_spi_request_enable(EN_TDCS);
    np_safety_spi_set_channel_current(6U, 1000U);
    run("safety_run 1000 3");
    check(!np_sim_safety_line_enabled(LINE_TDCS), "an open contact (the HAL's 999999 ohm): not granted");
    np_sim_safety_set_impedance_ohm(1U, 5000U);
    run("safety_run 400 3");
    check(!np_sim_safety_line_enabled(LINE_TDCS), "a contact that passes after the check ran: the failed check stands for the session");
}

/* @verifies REQ-SIMFW-14#c */
static void scn_the_thermal_interlock_cuts_at_its_cutoff_and_holds(void)
{
    boot(true, 0U);
    start_session();
    np_safety_spi_request_enable(EN_PBM);
    run("safety_run 400 3");
    check(np_sim_safety_line_enabled(LINE_PBM), "granted and driven at a cold sensor");
    np_sim_safety_set_adc(0U, 600U);                            /* NTC_ADC table: 600 is hotter than the 62 C row (650) */
    run("safety_run 400 3");
    check(!np_sim_safety_line_enabled(LINE_PBM), "a hot cranial domain cuts the PBM line");
    check((status() & STATUS_THERMAL) != 0U, "and the status says THERMAL");
    run("safety_run 2000 3");
    check(!np_sim_safety_line_enabled(LINE_PBM), "and it stays cut while the domain stays hot");
}

/* @verifies REQ-SIMFW-14#c */
static void scn_a_t1_unit_withholds_the_cervical_vns_line(void)
{
    boot(true, 1U);
    np_sim_safety_set_impedance_ohm(3U, 2000U);
    start_session();
    np_safety_spi_request_enable(EN_CVNS);
    run("safety_run 1000 3");
    check(!np_sim_safety_line_enabled(LINE_CVNS) && (granted() & EN_CVNS) == 0U, "a signed T1 record: the T2 line is withheld");
    check(np_safety_spi_get_tier() == 1U, "and the tier report says T1");
}

/* A baseline of 70 bpm R-peaks with the hub's own estimate agreeing, then a step to 100 bpm. */
static void cvns_up(void)
{
    boot(true, 2U);
    np_sim_safety_set_impedance_ohm(3U, 2000U);
    np_sim_safety_set_cvns_electrode_ohm(0U, 2000U);
    np_sim_safety_set_cvns_electrode_ohm(1U, 2000U);
    run("safety_hr 1 700 0");
    np_safety_spi_request_enable(EN_CVNS);                      /* requested at session start, so the contact check runs */
    np_safety_spi_set_channel_current(10U, 1000U);
    run("safety_run 1000 3 70");
    run("safety_sig good");
    run("safety_wave 10 2 200");                                /* charge-balanced pulses, 200 us a phase: declared, or held off */
}

/* @verifies REQ-SIMFW-14#c */
static void scn_a_heart_rate_step_cuts_the_cervical_vns_line_and_latches(void)
{
    cvns_up();
    for (int i = 0; i < 30; i++) { run("safety_hr 1 700 0"); run("safety_run 1000 3 70"); }
    check(np_sim_safety_line_enabled(LINE_CVNS), "a T2 unit with a steady 70 bpm: the cervical VNS line is granted and driven");
    const uint32_t t0 = np_sim_now_ms();
    uint32_t cut = 0U;
    for (int s = 0; s < 40 && cut == 0U; s++) {
        run("safety_hr 1 1000 0");
        run("safety_run 1000 3 100");
        if (!np_sim_safety_line_enabled(LINE_CVNS)) { cut = np_sim_now_ms() - t0; }
    }
    say("cervical VNS cut %u ms after the R-R rate stepped from 70 to 100 bpm (status %u)", cut, status());
    check(cut != 0U, "a step of 30 bpm cuts the line");
    check((status() & STATUS_CARDIAC) != 0U, "and the status says CARDIAC");
    run("safety_run 3000 3 100");
    check(!np_sim_safety_line_enabled(LINE_CVNS), "and the cutoff stays latched with the rate steady at the new value");
}

/* A cutoff, written to flash, and a second power-on that finds it. */
static void scn_cutoff_phase_one(void)
{
    cvns_up();
    for (int i = 0; i < 30; i++) { run("safety_hr 1 700 0"); run("safety_run 1000 3 70"); }
    for (int s = 0; s < 40 && np_sim_safety_line_enabled(LINE_CVNS); s++) { run("safety_hr 1 1000 0"); run("safety_run 1000 3 100"); }
    run("safety_run 2000 3 100");
    check((status() & STATUS_CARDIAC) != 0U, "power-on 1: the cutoff happened");
}

static void scn_cutoff_phase_two(void)
{
    cvns_up();                                                  /* same silicon, a new process: a power-on reset */
    for (int i = 0; i < 12; i++) { run("safety_hr 1 700 0"); run("safety_run 1000 3 70"); }
    uint8_t flags = 0U;
    const bool valid = np_safety_spi_get_cardiac_report(&flags);
    check(valid && (flags & 0x01U) != 0U, "power-on 2: the MCU reports the active user's cervical VNS withheld, from its flash");
    check(!np_sim_safety_line_enabled(LINE_CVNS), "and the line stays disabled although the heart is steady");
}

static void scn_a_power_cycled_mcu_remembers_a_cardiac_cutoff(void)
{
    np_sim_safety_silicon();                                    /* the parent owns the flash and OTP the two children share */
    in_child(scn_cutoff_phase_one);
    in_child(scn_cutoff_phase_two);
}

/* @verifies REQ-SIMFW-14#b */
static void scn_the_mcu_rejects_a_corrupt_heartbeat_and_then_cuts_at_the_watchdog(void)
{
    boot(true, 0U);
    start_session();
    np_safety_spi_request_enable(EN_PBM);
    run("safety_run 400 3");
    check(np_sim_safety_line_enabled(LINE_PBM), "granted and driven");
    run("safety_fault spi_flip 100 0");                         /* every heartbeat now arrives with its magic byte inverted */
    run("safety_run 2000 3");
    check(!np_sim_safety_line_enabled(LINE_PBM), "heartbeats that fail the magic never reset the watchdog: the line is cut");
}

/* @verifies REQ-SIMFW-14#b */
static void scn_a_frozen_systick_is_caught_against_tim2(void)
{
    boot(true, 0U);
    start_session();
    np_safety_spi_request_enable(EN_PBM);
    run("safety_run 400 3");
    run("safety_fault systick 1");
    run("safety_run 2000 3");
    np_sim_safety_info_t i;
    np_sim_safety_info(&i);
    check(i.last_reply_fault_slot == 0xF9U && !np_sim_safety_line_enabled(LINE_PBM), "SysTick frozen, TIM2 running: fault slot 0xF9 and the line cut");
}

/* @verifies REQ-SIMFW-14#b */
static void scn_a_flash_that_cannot_record_a_cutoff_raises_its_own_fault(void)
{
    cvns_up();
    run("safety_fault nv_erase 100");
    run("safety_fault nv_program 100");
    for (int i = 0; i < 30; i++) { run("safety_hr 1 700 0"); run("safety_run 1000 3 70"); }
    for (int s = 0; s < 40 && np_sim_safety_line_enabled(LINE_CVNS); s++) { run("safety_hr 1 1000 0"); run("safety_run 1000 3 100"); }
    run("safety_run 3000 3 100");
    np_sim_safety_info_t i;
    np_sim_safety_info(&i);
    check(i.last_reply_fault_slot == 0xFAU, "a flash that fails every write: the MCU raises the non-volatile-state fault (0xFA)");
}

int main(void)
{
    in_child(scn_the_link_reports_absence_until_the_mcu_is_attached);
    in_child(scn_the_mcu_boots_with_every_enable_disabled);
    in_child(scn_a_signed_session_is_granted_and_an_unsigned_one_is_not);
    in_child(scn_a_bad_signature_is_a_cutoff);
    in_child(scn_an_unprovisioned_otp_refuses_every_session);
    in_child(scn_losing_the_heartbeat_cuts_every_line_at_the_watchdog_timeout);
    in_child(scn_a_hung_loop_leaves_the_enables_until_the_independent_watchdog_expires);
    in_child(scn_the_dc_charge_ceiling_cuts_the_channel);
    in_child(scn_the_per_phase_ceiling_cuts_a_pulsed_channel_and_passes_one_inside_it);
    in_child(scn_an_undeclared_electrical_channel_is_held_off);
    in_child(scn_the_impedance_gate_withholds_a_channel_until_the_contact_passes);
    in_child(scn_the_thermal_interlock_cuts_at_its_cutoff_and_holds);
    in_child(scn_a_t1_unit_withholds_the_cervical_vns_line);
    in_child(scn_a_heart_rate_step_cuts_the_cervical_vns_line_and_latches);
    in_child(scn_the_mcu_rejects_a_corrupt_heartbeat_and_then_cuts_at_the_watchdog);
    in_child(scn_a_frozen_systick_is_caught_against_tim2);
    scn_a_power_cycled_mcu_remembers_a_cardiac_cutoff();
    in_child(scn_a_flash_that_cannot_record_a_cutoff_raises_its_own_fault);
    printf("%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
