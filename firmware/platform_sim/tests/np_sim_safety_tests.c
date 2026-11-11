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

/* A scenario on a new part.  Once a scenario has shared its silicon with its children (the power cycles), every later child would boot on what
 * the earlier ones left in flash and OTP, so each later scenario that is not itself a power cycle starts from an erased part. */
static void alone(void (*scenario)(void));

/* Run a scenario in a child process; the child's failures are the parent's. */
static void in_child(void (*scenario)(void))
{
    fflush(stdout);
    const pid_t pid = fork();
    if (pid == 0) {
        scenario();
        fflush(stdout);
        exit(g_failures == 0 ? 0 : 1);       /* exit, not _exit: the coverage counters are written by the exit handlers */
    }
    int st = 0;
    (void)waitpid(pid, &st, 0);
    if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) { g_failures++; printf("FAIL: scenario child did not finish cleanly\n"); }
}

static void alone(void (*scenario)(void))
{
    np_sim_safety_silicon_erase();
    in_child(scenario);
}

#define RUNNING 3
#define SIG_PENDING 0x80U
#define STATUS_FAULT 0x01U
#define STATUS_WATCHDOG 0x02U
#define STATUS_CUTOFF 0x04U
#define STATUS_CARDIAC 0x40U

#define LINE_BES 1
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
static void test_the_link_reports_absence_until_the_mcu_is_attached(void)
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
static void test_the_mcu_boots_with_every_enable_disabled(void)
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
static void test_a_signed_session_is_granted_and_an_unsigned_one_is_not(void)
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
static void test_a_bad_signature_is_a_cutoff(void)
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
static void test_an_unprovisioned_otp_refuses_every_session(void)
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

/* @verifies REQ-SIMFW-14#b */
static void test_a_root_key_that_fails_its_crc_refuses_every_session(void)
{
    np_sim_set_sink(capture);
    np_sim_use_real_clock();
    np_simh_init();
    run("safety_root 7");
    run("safety_root_corrupt");
    run("safety_attach");
    run("safety_run 1000 3");
    run("safety_sig good");
    np_safety_spi_request_enable(EN_PBM);
    run("safety_run 600 3");
    np_sim_safety_info_t i;
    np_sim_safety_info(&i);
    check(i.last_reply_fault_slot == 0xF8U, "a programmed key that no longer matches its CRC: the fault slot says so (0xF8), not unprovisioned");
    check(granted() == 0U, "and nothing is granted");
}

/* @verifies REQ-SIMFW-14#c */
static void test_losing_the_heartbeat_cuts_every_line_at_the_watchdog_timeout(void)
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
static void test_a_hung_loop_leaves_the_enables_until_the_independent_watchdog_expires(void)
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
static void test_the_dc_charge_ceiling_cuts_the_channel(void)
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
static void test_the_per_phase_ceiling_cuts_a_pulsed_channel_and_passes_one_inside_it(void)
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
static void test_an_undeclared_electrical_channel_is_held_off(void)
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
static void test_the_impedance_gate_withholds_a_channel_until_the_contact_passes(void)
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
static void test_the_thermal_interlock_cuts_at_its_cutoff_and_holds(void)
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
static void test_a_t1_unit_withholds_the_cervical_vns_line(void)
{
    boot(true, 1U);
    np_sim_safety_set_impedance_ohm(3U, 2000U);
    start_session();
    np_safety_spi_request_enable(EN_CVNS);
    run("safety_run 1000 3");
    check(!np_sim_safety_line_enabled(LINE_CVNS) && (granted() & EN_CVNS) == 0U, "a signed T1 record: the T2 line is withheld");
    check(np_safety_spi_get_tier() == 1U, "and the tier report says T1");
}

/* A baseline of 70 bpm R-peaks with the hub's own estimate agreeing, then a step to 100 bpm.  `user` names the active user first, between sessions. */
static void cvns_boot(unsigned user)
{
    boot(true, 2U);
    np_sim_safety_set_impedance_ohm(3U, 2000U);
    np_sim_safety_set_cvns_electrode_ohm(0U, 2000U);
    np_sim_safety_set_cvns_electrode_ohm(1U, 2000U);
    if (user != 0U) {
        char l[32];
        run("safety_run 400 0");
        (void)snprintf(l, sizeof l, "safety_user %u", user);
        run(l);
        run("safety_run 400 0");
    }
    run("safety_hr 1 700 0");
    np_safety_spi_request_enable(EN_CVNS);                      /* requested at session start, so the contact check runs */
    np_safety_spi_set_channel_current(10U, 1000U);
    run("safety_run 1000 3 70");
    run("safety_sig good");
    run("safety_wave 10 2 200");                                /* charge-balanced pulses, 200 us a phase: declared, or held off */
}

static void cvns_up(void) { cvns_boot(0U); }

/* @verifies REQ-SIMFW-14#c */
static void test_a_heart_rate_step_cuts_the_cervical_vns_line_and_latches(void)
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
static void phase_one_the_cutoff(void)
{
    cvns_up();
    for (int i = 0; i < 30; i++) { run("safety_hr 1 700 0"); run("safety_run 1000 3 70"); }
    for (int s = 0; s < 40 && np_sim_safety_line_enabled(LINE_CVNS); s++) { run("safety_hr 1 1000 0"); run("safety_run 1000 3 100"); }
    run("safety_run 2000 3 100");
    check((status() & STATUS_CARDIAC) != 0U, "power-on 1: the cutoff happened");
}

static void phase_two_the_power_on(void)
{
    cvns_up();                                                  /* same silicon, a new process: a power-on reset */
    for (int i = 0; i < 12; i++) { run("safety_hr 1 700 0"); run("safety_run 1000 3 70"); }
    uint8_t flags = 0U;
    const bool valid = np_safety_spi_get_cardiac_report(&flags);
    check(valid && (flags & 0x01U) != 0U, "power-on 2: the MCU reports the active user's cervical VNS withheld, from its flash");
    check(!np_sim_safety_line_enabled(LINE_CVNS), "and the line stays disabled although the heart is steady");
}

/* @verifies REQ-SIMFW-14#c */
static void test_a_power_cycled_mcu_remembers_a_cardiac_cutoff(void)
{
    np_sim_safety_silicon();                                    /* the parent owns the flash and OTP the two children share */
    in_child(phase_one_the_cutoff);
    in_child(phase_two_the_power_on);
}

/* @verifies REQ-SIMFW-14#b */
static void test_the_mcu_rejects_a_corrupt_heartbeat_and_then_cuts_at_the_watchdog(void)
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
static void test_a_frozen_systick_is_caught_against_tim2(void)
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
static void test_a_flash_that_cannot_record_a_cutoff_raises_its_own_fault(void)
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

/* ── Paths no scenario reached before (OI-SIMFW-25): warm reset, the signature lock, re-enable, the gates, per-user scope ───── */

#define STATUS_IMPEDANCE 0x08U
#define LINE_CLIN_STIM 9
#define EN_CLIN_STIM (1U << 13)
#define LINE_VNS 3
#define NV_USER_BLOCKED 0x01U
#define NV_OUTSTANDING  0x02U

static void settle(void) { np_sim_advance_ms(5U); }

static unsigned fault_slot(void)
{
    np_sim_safety_info_t i;
    np_sim_safety_info(&i);
    return i.last_reply_fault_slot;
}

/* ── The warm reset ── */

/* A bad signature latches FAULT and CUTOFF with slot 0xFD; the loop then hangs and the independent watchdog resets the part. */
static void phase_one_fault_then_iwdg_reset(void)
{
    boot(true, 0U);
    run("safety_run 1000 3");
    run("safety_sig bad");
    np_safety_spi_request_enable(EN_PBM);
    run("safety_run 600 3");
    check((status() & STATUS_FAULT) != 0U, "reset 1: a bad signature latched FAULT before the reset");
    np_sim_safety_info_t i;
    np_sim_safety_info(&i);
    check((i.latch_status & STATUS_FAULT) != 0U && i.latch_slot == 0xFDU && i.latch_count >= 1U,
          "reset 1: the MCU's own fault latch holds the fault and its slot (0xFD)");
    say("latch before the reset: status %u, slot %u", i.latch_status, i.latch_slot);
    run("safety_fault stall 1");
    np_sim_advance_ms(1100U);
    np_sim_safety_info(&i);
    check(i.held_in_reset && i.iwdg_resets == 1U, "reset 1: the independent watchdog reset the MCU");
    /* The reset MCU is not restarted: the loop does not run again, however long the clock runs, and the hub cannot reach it. */
    const uint32_t iterations = i.iterations;
    np_sim_advance_ms(5000U);
    np_sim_safety_info(&i);
    check(i.iterations == iterations, "held in reset: no iteration of the polling loop runs while the clock runs on");
    check(!np_sim_safety_attach(), "held in reset: attaching again does not start another MCU in this process");
    check(np_safety_spi_heartbeat(NP_SESSION_RUNNING, EN_PBM, NULL, 0U) == NP_HUB_ERR_TIMEOUT, "held in reset: the hub's heartbeat times out");
    check(!np_sim_safety_line_enabled(LINE_PBM), "held in reset: every enable line is disabled");
}

static void phase_two_the_warm_boot(void)
{
    boot(true, 0U);                                         /* same silicon: the latch the last MCU left is what it starts from */
    np_sim_safety_info_t i;
    np_sim_safety_info(&i);
    check((i.latch_status & STATUS_FAULT) != 0U && i.latch_slot == 0xFDU,
          "warm boot: after the first polling iteration the latch still holds the prior fault and its slot (0xFD)");
    run("safety_run 1000 0");
    np_sim_safety_info(&i);
    say("warm boot: replies carry status %u, fault slot %u", status(), i.last_reply_fault_slot);
    say("warm boot: the latch now holds status %u, slot %u", i.latch_status, i.latch_slot);
    check((status() & STATUS_FAULT) != 0U, "warm boot: the MCU reports the prior fault (FAULT) with no session running");
    check(i.last_reply_fault_slot == 0xFFU && status() == STATUS_FAULT, "warm boot: but the reply carries the generic FAULT with no slot, not the slot the latch held (OI-SIMFW-32)");
    check(i.latch_slot == 0xFFU && i.latch_status == STATUS_FAULT, "warm boot: and the next commit overwrote the latch with that generic FAULT, so the specific slot and status bits are gone (OI-SIMFW-32)");
    np_safety_spi_request_enable(EN_PBM);
    start_session();
    run("safety_run 1000 3");
    check(granted() == 0U && !np_sim_safety_line_enabled(LINE_PBM), "warm boot: a good signature does not grant a line while the prior fault stands");
    run("safety_run 600 0");                                /* the session ends, and a second one starts */
    start_session();
    run("safety_run 1000 3");
    check((status() & STATUS_FAULT) != 0U && granted() == 0U, "warm boot: and a second session does not clear it: no command the app can send clears a restored fault (OI-SIMFW-32)");
}

static void phase_three_the_power_on(void)
{
    boot(true, 0U);
    np_safety_spi_request_enable(EN_PBM);
    start_session();
    run("safety_run 1000 3");
    check((status() & STATUS_FAULT) == 0U && np_sim_safety_line_enabled(LINE_PBM), "power-on after power loss: the latch is gone and a signed session is granted");
}

/* @verifies REQ-SIMFW-14#c */
static void test_a_warm_reset_keeps_the_fault_latch_and_a_power_on_reset_clears_it(void)
{
    np_sim_safety_silicon();
    in_child(phase_one_fault_then_iwdg_reset);
    in_child(phase_two_the_warm_boot);
    np_sim_safety_power_on_reset();                         /* power loss between the two */
    in_child(phase_three_the_power_on);
}

/* ── The signature lock ── */

static void a_session(void)
{
    run("safety_run 400 0");                                /* the last session ends: session_active falls */
    run("safety_run 1000 3");                               /* a new one starts and waits for its signature */
}

/* @verifies REQ-SIMFW-14#c */
static void test_three_failed_signatures_hard_lock_and_a_good_one_releases_the_next_session(void)
{
    boot(true, 0U);
    np_safety_spi_request_enable(EN_PBM);
    for (int n = 1; n <= 3; n++) {
        a_session();
        if (n > 1) {
            check((status() & (STATUS_FAULT | STATUS_CUTOFF)) == 0U && (status() & SIG_PENDING) != 0U,
                  "below the limit: a new session start clears the earlier signature fault");
        }
        run("safety_sig bad");
        run("safety_run 400 3");
        check((status() & STATUS_FAULT) != 0U && (status() & STATUS_CUTOFF) != 0U && fault_slot() == 0xFDU, "a bad signature: FAULT, CUTOFF and slot 0xFD");
    }
    a_session();
    check((status() & STATUS_FAULT) != 0U && fault_slot() == 0xFDU, "the third consecutive failure is a hard lock: a new session start does not clear the fault");
    run("safety_sig good");
    run("safety_run 600 3");
    check((status() & SIG_PENDING) == 0U && (status() & STATUS_FAULT) != 0U && granted() == 0U && !np_sim_safety_line_enabled(LINE_PBM),
          "and a good signature clears SIG_PENDING but not the fault: nothing is granted");
    a_session();
    check((status() & STATUS_FAULT) == 0U, "yet the good signature reset the failure count: the next session start clears the lock, with no power cycle (OI-SIMFW-33)");
    run("safety_sig good");
    run("safety_run 600 3");
    check(np_sim_safety_line_enabled(LINE_PBM), "and that session is granted");
}

/* @verifies REQ-SIMFW-14#b */
static void test_three_corrupt_signature_frames_latch_a_fault_that_no_session_clears(void)
{
    boot(true, 0U);
    np_safety_spi_request_enable(EN_PBM);
    run("safety_run 1000 3");
    run("safety_fault cmd_flip 3 0");                       /* the next three command frames arrive with their first byte inverted */
    for (int k = 0; k < 2; k++) { run("safety_sig good"); run("safety_run 200 3"); }
    check((status() & STATUS_FAULT) == 0U && (status() & SIG_PENDING) != 0U, "two corrupt frames: SIG_PENDING stays, no fault yet");
    run("safety_sig good");
    run("safety_run 200 3");
    run("safety_beat 3");                                   /* a reply is the state one iteration back: read it after one more beat */
    check((status() & STATUS_FAULT) != 0U && fault_slot() == 0xFCU, "the third consecutive corrupt frame latches FAULT with slot 0xFC");
    run("safety_sig good");                                 /* the flips are used up: a clean frame */
    run("safety_run 600 3");
    check((status() & SIG_PENDING) == 0U && granted() == 0U, "a clean signature then clears SIG_PENDING, yet nothing is granted under the fault");
    a_session();
    run("safety_sig good");
    run("safety_run 600 3");
    check((status() & STATUS_FAULT) != 0U && fault_slot() == 0xFCU && granted() == 0U,
          "and a new session does not clear it: only a signature fault (0xFD) is recoverable at a session start (OI-SIMFW-33)");
}

/* ── Re-enable of the cervical VNS line after a cardiac cutoff ── */

static void steady(int seconds, bool report)
{
    for (int i = 0; i < seconds; i++) {
        if (report) { run("safety_hr 1 700 0"); }
        run("safety_run 1000 3 70");
    }
}

/* Steady 70 bpm, then a hub estimate 15 bpm away from the MCU's own: the cross-check cuts and latches. */
static void cut_by_the_hub_disagreeing(void)
{
    for (int s = 0; s < 10 && np_sim_safety_line_enabled(LINE_CVNS); s++) {
        run("safety_hr 1 850 0");
        run("safety_run 1000 3 70");
    }
}

static uint32_t reenable_after_a_cutoff(bool contact_lost_first)
{
    cvns_up();
    steady(12, true);
    check(np_sim_safety_line_enabled(LINE_CVNS), "steady and agreeing: the cervical VNS line is driven");
    cut_by_the_hub_disagreeing();
    check(!np_sim_safety_line_enabled(LINE_CVNS) && (status() & STATUS_CARDIAC) != 0U, "a hub estimate 15 bpm from the MCU's mean: cut and CARDIAC");
    const uint32_t t_cut = np_sim_now_ms();
    run("safety_reenable 1");                               /* the hub asks for a re-enable at once */
    for (int s = 0; s < 20; s++) { run("safety_hr 1 700 0"); run("safety_run 1000 3 70"); }
    check((status() & STATUS_CARDIAC) != 0U && !np_sim_safety_line_enabled(LINE_CVNS), "20 s after the cutoff a re-enable request is denied: the 30 s lockout has not elapsed");
    if (contact_lost_first) { np_sim_safety_set_impedance_ohm(3U, 999999U); }
    uint32_t t_clear = 0U;
    for (int s = 0; s < 20 && t_clear == 0U; s++) {
        run("safety_hr 1 700 0");
        run("safety_run 1000 3 70");
        if ((status() & STATUS_CARDIAC) == 0U) { t_clear = np_sim_now_ms(); }
    }
    say("CARDIAC cleared %u ms after the cutoff (lockout 30000)", t_clear - t_cut, 0U);
    check(t_clear != 0U && (t_clear - t_cut) >= 30000U, "once the lockout has elapsed the standing re-enable request clears CARDIAC");
    return t_clear;
}

/* @verifies REQ-SIMFW-14#c */
static void test_a_cardiac_cutoff_is_reenabled_after_the_lockout_and_the_line_returns(void)
{
    (void)reenable_after_a_cutoff(false);
    check(!np_sim_safety_line_enabled(LINE_CVNS), "right after the re-enable the baseline is discarded and the line is not yet granted");
    steady(12, true);
    check(np_sim_safety_line_enabled(LINE_CVNS) && (status() & STATUS_CARDIAC) == 0U, "after a fresh baseline, the hub's agreement and a passing contact check the line is granted again");
    uint8_t flags = 0xFFU;
    check(np_safety_spi_get_cardiac_report(&flags) && (flags & (NV_USER_BLOCKED | NV_OUTSTANDING)) == 0U, "and the acknowledgement was persisted: no outstanding cutoff");
}

/* @verifies REQ-SIMFW-14#c */
static void test_a_reenable_repeats_the_impedance_check(void)
{
    (void)reenable_after_a_cutoff(true);                    /* the contact opens before the re-enable */
    steady(12, true);
    say("contact lost before the re-enable: line enabled %u, status %u", np_sim_safety_line_enabled(LINE_CVNS) ? 1U : 0U, status());
    check(!np_sim_safety_line_enabled(LINE_CVNS), "an open contact at the re-enable: the line stays withheld, though the baseline re-formed and the hub agrees");
    check((status() & STATUS_CARDIAC) == 0U, "and it is the impedance gate that holds it, not the cardiac latch");
}

/* ── The cross-check's failure paths, and R-peak staleness ── */

/* @verifies REQ-SIMFW-14#c */
static void test_without_the_hubs_estimate_the_line_is_withheld_silently(void)
{
    cvns_up();
    steady(20, false);
    check(!np_sim_safety_line_enabled(LINE_CVNS) && (status() & STATUS_CARDIAC) == 0U, "no hub estimate before the first agreement: the grant is withheld, with no cutoff and no latch");
}

/* @verifies REQ-SIMFW-14#c */
static void test_a_hub_estimate_flagged_invalid_after_agreement_cuts_the_line(void)
{
    cvns_up();
    steady(12, true);
    check(np_sim_safety_line_enabled(LINE_CVNS), "agreeing: driven");
    run("safety_hr 0 700 0");                               /* the hub says its estimate is not valid */
    run("safety_run 1000 3 70");
    check(!np_sim_safety_line_enabled(LINE_CVNS) && (status() & STATUS_CARDIAC) != 0U, "an invalid estimate after an agreement: cut and CARDIAC");
}

/* @verifies REQ-SIMFW-14#c */
static void test_a_hub_that_goes_quiet_after_agreement_cuts_the_line_when_its_estimate_ages_out(void)
{
    cvns_up();
    steady(12, true);
    check(np_sim_safety_line_enabled(LINE_CVNS), "agreeing: driven");
    const uint32_t t0 = np_sim_now_ms();
    uint32_t cut = 0U;
    for (int s = 0; s < 8 && cut == 0U; s++) {
        run("safety_run 1000 3 70");                        /* no report at all */
        if (!np_sim_safety_line_enabled(LINE_CVNS)) { cut = np_sim_now_ms() - t0; }
    }
    say("the line was cut %u ms after the last hub report (stale at 3000)", cut, 0U);
    check(cut != 0U && (status() & STATUS_CARDIAC) != 0U, "no report for longer than the staleness bound: cut and CARDIAC");
}

/* @verifies REQ-SIMFW-14#c */
static void test_a_stopped_r_peak_stream_cuts_the_cervical_line(void)
{
    cvns_up();
    steady(12, true);
    check(np_sim_safety_line_enabled(LINE_CVNS), "agreeing: driven");
    const uint32_t t0 = np_sim_now_ms();
    uint32_t cut = 0U;
    for (int s = 0; s < 8 && cut == 0U; s++) {
        run("safety_hr 1 700 0");                           /* the hub still agrees; the R-peak line has gone quiet */
        run("safety_run 1000 3 0");
        if (!np_sim_safety_line_enabled(LINE_CVNS)) { cut = np_sim_now_ms() - t0; }
    }
    say("the line was cut %u ms after the R-peaks stopped (stale at 3000)", cut, 0U);
    run("safety_beat 3");
    check(cut != 0U && cut <= 5000U && (status() & STATUS_CARDIAC) != 0U, "no R-peak for 3 s while granted: cut and CARDIAC");
}

/* A T2 session with the contacts good and no R-peak yet: the scenario then plays the R-peak line itself. */
static void cvns_boot_without_peaks(void)
{
    boot(true, 2U);
    np_sim_safety_set_impedance_ohm(3U, 2000U);
    np_sim_safety_set_cvns_electrode_ohm(0U, 2000U);
    np_sim_safety_set_cvns_electrode_ohm(1U, 2000U);
    run("safety_hr 1 700 0");
    np_safety_spi_request_enable(EN_CVNS);
    np_safety_spi_set_channel_current(10U, 1000U);
    run("safety_run 1000 3");
    run("safety_sig good");
    run("safety_wave 10 2 200");
}

/* @verifies REQ-SIMFW-14#c */
static void test_a_burst_of_edges_on_the_r_peak_line_is_read_as_an_artefact_and_cuts(void)
{
    cvns_boot_without_peaks();                              /* a session with no R-peak yet */
    for (int k = 0; k < 12; k++) { np_sim_safety_rpeak(); np_sim_advance_ms(1U); }       /* the first baseline is twelve edges a millisecond apart */
    run("safety_hr 1 700 0");
    run("safety_run 1000 3");
    run("safety_beat 3");
    check(!np_sim_safety_line_enabled(LINE_CVNS) && (status() & STATUS_CARDIAC) != 0U,
          "a baseline of edges 1 ms apart (a rate beyond any heart) saturates the mean, disagrees with the hub, and is cut and latched");
}

/* The hub's estimate is the MCU's own mean to the millisecond (the scenario makes the edges, so it knows them), so the cross-check
 * agrees throughout a slow rise and never trips: what cuts is the rate history, 15 bpm against a snapshot at most 18 s old. */
/* @verifies REQ-SIMFW-14#c */
static void test_a_slow_rise_the_hub_agrees_with_is_cut_by_the_rate_history(void)
{
    cvns_boot_without_peaks();
    uint32_t rr[8];
    unsigned n = 0U, head = 0U;
    uint32_t next_edge = np_sim_now_ms() + 857U, last_edge = 0U;
    const uint32_t t0 = np_sim_now_ms();
    uint32_t cut = 0U;
    unsigned rate_at_cut = 0U;
    char l[48];
    for (uint32_t ms = 0U; ms < 70000U && cut == 0U; ms++) {
        if ((ms % 200U) == 0U) { run("safety_beat 3"); }
        if ((ms % 1000U) == 500U && n >= 8U) {
            uint32_t sum = 0U;
            for (unsigned i = 0U; i < 8U; i++) { sum += rr[i]; }
            (void)snprintf(l, sizeof l, "safety_hr 1 %u 0", (unsigned)(600000000UL / (sum * 1000U / 8U)));
            run(l);
        }
        np_sim_advance_ms(1U);
        if (np_sim_now_ms() >= next_edge) {
            np_sim_safety_rpeak();
            if (last_edge != 0U) { rr[head] = np_sim_now_ms() - last_edge; head = (head + 1U) % 8U; if (n < 8U) { n++; } }
            last_edge = np_sim_now_ms();
            const uint32_t rise_s = (np_sim_now_ms() - t0) > 15000U ? (np_sim_now_ms() - t0 - 15000U) / 1000U : 0U;   /* 1 bpm a second after 15 s */
            const unsigned bpm = 70U + rise_s;
            next_edge = np_sim_now_ms() + 60000U / bpm;
            rate_at_cut = bpm;
        }
        if (n >= 8U && !np_sim_safety_line_enabled(LINE_CVNS) && np_sim_now_ms() - t0 > 14000U) { cut = np_sim_now_ms() - t0; }
    }
    run("safety_beat 3");
    say("cut %u ms in, at about %u bpm", cut, rate_at_cut);
    check(cut > 20000U && rate_at_cut >= 84U, "the hub agreeing throughout, a rise of more than 15 bpm is cut by the history, well after the cross-check would have");
    check((status() & STATUS_CARDIAC) != 0U, "and CARDIAC is latched");
}

/* ── Per-user scope ── */

static void cvns_next_session(void)
{
    np_safety_spi_request_enable(EN_CVNS);                  /* requested at session start, so the contact check runs */
    run("safety_run 1000 3 70");
    run("safety_sig good");
    run("safety_wave 10 2 200");
}

static void user_change(const char *line)
{
    np_safety_spi_request_disable(EN_CVNS);                 /* between sessions the hub holds no request: a user change waits for every grant to be gone */
    run("safety_run 400 0");
    run(line);
    run("safety_run 400 0");
}

static bool cardiac_flag(uint8_t mask)
{
    uint8_t f = 0U;
    return np_safety_spi_get_cardiac_report(&f) && (f & mask) == mask;
}

/* @verifies REQ-SIMFW-14#c */
static void test_a_cardiac_cutoff_is_held_for_the_user_who_triggered_it_and_nobody_else(void)
{
    boot(true, 2U);
    np_sim_safety_set_impedance_ohm(3U, 2000U);
    np_sim_safety_set_cvns_electrode_ohm(0U, 2000U);
    np_sim_safety_set_cvns_electrode_ohm(1U, 2000U);
    user_change("safety_user 1");
    run("safety_hr 1 700 0");
    np_safety_spi_request_enable(EN_CVNS);
    np_safety_spi_set_channel_current(10U, 1000U);
    cvns_next_session();
    steady(12, true);
    check(np_sim_safety_line_enabled(LINE_CVNS), "user 1: steady and agreeing: driven");
    cut_by_the_hub_disagreeing();
    check((status() & STATUS_CARDIAC) != 0U, "user 1: cut and CARDIAC");
    run("safety_run 2000 0");                               /* the session ends; the cutoff is written to flash */
    check(cardiac_flag(NV_USER_BLOCKED) && cardiac_flag(NV_OUTSTANDING), "user 1: the MCU reports its cervical VNS withheld and a cutoff outstanding");
    user_change("safety_user 2");
    check(!cardiac_flag(NV_USER_BLOCKED) && cardiac_flag(NV_OUTSTANDING), "user 2: not withheld, though a cutoff is outstanding on the device");
    user_change("safety_user 0");                           /* the reserved 'unspecified' tag */
    check(!cardiac_flag(NV_USER_BLOCKED), "the reserved tag 0 is refused: user 2 stays the active user");
    cvns_next_session();
    steady(12, true);
    check(np_sim_safety_line_enabled(LINE_CVNS) && (status() & STATUS_CARDIAC) == 0U, "user 2: a session on the same device is granted: the cutoff was not theirs");
    run("safety_user 1");                                   /* mid-session: ignored */
    steady(1, true);
    check(!cardiac_flag(NV_USER_BLOCKED) && np_sim_safety_line_enabled(LINE_CVNS), "a user change during a session is ignored");
    user_change("safety_user 1");
    check(cardiac_flag(NV_USER_BLOCKED), "user 1 again: withheld from flash");
    cvns_next_session();
    steady(12, true);
    check(!np_sim_safety_line_enabled(LINE_CVNS) && (status() & STATUS_CARDIAC) != 0U, "user 1's next cervical session meets the cutoff: withheld and CARDIAC");
}

/* A flash that cannot record the active user raises its own fault after the bounded retries. */
/* @verifies REQ-SIMFW-14#b */
static void test_a_flash_that_cannot_record_the_active_user_raises_its_own_fault(void)
{
    boot(true, 0U);
    run("safety_run 400 0");
    run("safety_fault nv_program 1000");
    for (int k = 0; k < 3; k++) { run("safety_user 5"); run("safety_run 200 0"); }
    run("safety_beat 0");
    check((status() & STATUS_FAULT) != 0U && fault_slot() == 0xFAU, "three failed writes of the active user: FAULT with the non-volatile-state slot (0xFA)");
}

/* The log is a ring of two pages: after enough changes the live page is full and the log is compacted into the other. */
static void phase_one_many_user_changes(void)
{
    cvns_boot(1U);
    steady(12, true);
    cut_by_the_hub_disagreeing();
    run("safety_run 2000 0");
    check(cardiac_flag(NV_OUTSTANDING), "log 1: a cutoff outstanding for user 1");
    run("safety_run 400 0");
    for (int k = 0; k < 300; k++) {
        run((k & 1) != 0 ? "safety_user 2" : "safety_user 3");
        np_sim_advance_ms(3U);
        if ((k % 100) == 99) { run("safety_beat 0"); }
    }
    run("safety_user 1");
    run("safety_run 600 0");
    check(cardiac_flag(NV_USER_BLOCKED | NV_OUTSTANDING), "log 1: after 300 user changes (a page rollover) user 1 is still withheld");
}

static void phase_two_the_log_after_the_rollover(void)
{
    boot(true, 2U);
    run("safety_run 600 0");
    check(cardiac_flag(NV_USER_BLOCKED) && cardiac_flag(NV_OUTSTANDING), "log 2: a power-on reads the compacted log back: user 1 withheld, the cutoff outstanding");
}

/* @verifies REQ-SIMFW-14#c */
static void test_the_cutoff_log_survives_a_page_rollover_and_a_power_cycle(void)
{
    np_sim_safety_silicon();
    in_child(phase_one_many_user_changes);
    in_child(phase_two_the_log_after_the_rollover);
}

/* Power lost part-way through compacting the log: the new page holds a snapshot with no COMMIT beside the full old page. */
static void phase_one_a_compaction_cut_short(void)
{
    cvns_boot(1U);                                          /* one write: the active user */
    steady(12, true);
    cut_by_the_hub_disagreeing();
    run("safety_run 2000 0");                               /* one write: user 1's cutoff */
    run("safety_run 400 0");
    np_sim_safety_info_t i;
    np_sim_safety_info(&i);
    for (int k = 0; i.nv_programs < 256U && k < 400; k++) {     /* until the live page's 256 slots are all written */
        run((k & 1) != 0 ? "safety_user 2" : "safety_user 3");
        np_sim_advance_ms(3U);
        if ((k % 100) == 99) { run("safety_beat 0"); }
        np_sim_safety_info(&i);
    }
    check(i.nv_programs == 256U, "compaction 1: the live page holds 256 records");
    run("safety_beat 0");
    run("safety_fault nv_program 1 3");                     /* the snapshot's RESET, USER and SET land; its COMMIT does not */
    run("safety_user 9");                                   /* the 257th write: compaction */
    run("safety_run 400 0");
    check(cardiac_flag(NV_OUTSTANDING), "compaction 1: the log still reads a cutoff outstanding after a compaction that lost its COMMIT");
}

static void phase_two_after_the_compaction_was_cut_short(void)
{
    boot(true, 2U);
    run("safety_run 600 0");
    check(cardiac_flag(NV_OUTSTANDING), "compaction 2: a power-on over a snapshot that never committed still reads the cutoff outstanding");
    user_change("safety_user 7");                           /* the next change finishes the compaction that was cut short */
    uint8_t f = 0U;
    (void)np_safety_spi_get_cardiac_report(&f);
    say("compaction 2: user 7 reads flags %u after the compaction is completed", f, 0U);
    check(cardiac_flag(NV_OUTSTANDING | NV_USER_BLOCKED), "compaction 2: the next write completes it, and a user who never had a cutoff is withheld too: an uncommitted snapshot is read as everyone's (OI-SIMFW-35)");
    user_change("safety_user 1");
    check(cardiac_flag(NV_USER_BLOCKED), "compaction 2: and user 1 is still withheld");
}

/* @verifies REQ-SIMFW-14#c */
static void test_a_compaction_cut_short_loses_no_cutoff(void)
{
    np_sim_safety_silicon();
    in_child(phase_one_a_compaction_cut_short);
    in_child(phase_two_after_the_compaction_was_cut_short);
}

/* ── Thermal re-arm and the hub's own sensor ── */

/* @verifies REQ-SIMFW-14#c */
static void test_the_thermal_interlock_re_arms_below_its_hysteresis_and_the_hub_sensor_cuts_everything(void)
{
    boot(true, 0U);
    np_sim_safety_set_impedance_ohm(2U, 5000U);
    np_safety_spi_request_enable(EN_PBM | EN_BES);
    np_safety_spi_set_channel_current(5U, 1000U);
    start_session();
    run("safety_wave 5 2 20000");
    run("safety_limits 5 1000");
    run("safety_run 400 3");
    check(np_sim_safety_line_enabled(LINE_PBM) && np_sim_safety_line_enabled(LINE_BES), "cold: the cranial PBM and BES lines are driven");
    np_sim_safety_set_adc(0U, 600U);                        /* 65 C on a cranial domain */
    run("safety_run 400 3");
    check(!np_sim_safety_line_enabled(LINE_PBM) && !np_sim_safety_line_enabled(LINE_BES),
          "a hot cranial domain cuts the PBM line, and the BES line with it: THERMAL withholds every channel at the next grant");
    np_sim_safety_set_adc(0U, 700U);                        /* 60 C: below the cutoff, above the re-arm */
    run("safety_run 400 3");
    check(!np_sim_safety_line_enabled(LINE_PBM) && (status() & STATUS_THERMAL) != 0U, "60 C: still cut (the 7 C hysteresis)");
    np_sim_safety_set_adc(0U, 1000U);                       /* 50 C */
    run("safety_run 600 3");
    check((status() & STATUS_THERMAL) == 0U && np_sim_safety_line_enabled(LINE_PBM), "50 C: the interlock re-arms and the PBM line is granted again");
    np_sim_safety_set_adc(5U, 100U);                        /* the hub's own sensor, below the table's end: read as 110 C */
    run("safety_run 400 3");
    check(!np_sim_safety_line_enabled(LINE_PBM) && !np_sim_safety_line_enabled(LINE_BES) && fault_slot() == 0xFBU,
          "the hub's sensor too hot: every line is cut, with the hub-NTC slot (0xFB)");
    np_sim_safety_set_adc(5U, 4095U);
    run("safety_run 600 3");
    check(np_sim_safety_line_enabled(LINE_PBM) && np_sim_safety_line_enabled(LINE_BES), "and both return when the hub's sensor is cold again");
}

/* ── The withhold report, from the MCU's own gates to the hub's getter (OI-SIMFW-61) ── */

/* @verifies REQ-FWHUB-81#d */
static void test_the_hub_learns_which_gate_withheld_a_line(void)
{
    uint16_t decl = 0U, geom = 0U;

    boot(true, 0U);
    np_sim_safety_set_impedance_ohm(1U, 5000U);
    run("safety_run 1000 3");
    run("safety_sig good");
    np_safety_spi_request_enable(EN_TDCS);
    np_safety_spi_set_channel_current(6U, 1000U);
    run("safety_run 800 3");
    check(!np_sim_safety_line_enabled(LINE_TDCS), "withhold report: the undeclared channel is held off (status OK, no fault slot)");
    check(np_safety_spi_get_withhold_report(&decl, &geom), "withhold report: the hub reads a valid report off the MCU's own reply");
    check((decl & EN_TDCS) != 0U && geom == 0U,
          "withhold report: the declaration gate named the tDCS line and the geometry gate named nothing");
    declare_dc(6U, 1000U);
    run("safety_run 800 3");
    check(np_sim_safety_line_enabled(LINE_TDCS), "withhold report: declared, the line is driven");
    check(np_safety_spi_get_withhold_report(&decl, &geom) && (decl & EN_TDCS) == 0U && geom == 0U,
          "withhold report: the next beat's report no longer names the line");
}

/* @verifies REQ-FWHUB-81#d */
static void test_the_hub_learns_which_geometry_was_missing(void)
{
    uint16_t decl = 0U, geom = 0U;

    boot(true, 2U);
    np_sim_safety_set_impedance_ohm(1U, 5000U);
    np_sim_safety_set_impedance_ohm(2U, 5000U);
    np_safety_spi_request_enable(EN_TDCS | EN_BES);
    np_safety_spi_set_channel_current(6U, 1000U);
    np_safety_spi_set_channel_current(5U, 1000U);
    run("safety_geom tdcs 1");
    run("safety_geom bes 1");
    start_session();
    run("safety_wave 6 1 0");
    settle();
    run("safety_wave 5 2 20000");
    run("safety_run 800 3");
    check(np_safety_spi_get_withhold_report(&decl, &geom), "geometry report: a valid report arrives");
    check(geom == (uint16_t)(EN_TDCS | EN_BES) && decl == 0U,
          "geometry report: declared and in contact, the tDCS and BES/tACS lines are named by the geometry gate, not the declaration gate");
    run("safety_limits 6 100");
    run("safety_run 400 3");
    check(np_safety_spi_get_withhold_report(&decl, &geom) && geom == EN_BES,
          "geometry report: an area for tDCS takes tDCS out of the report and leaves BES/tACS");
    run("safety_limits 5 1000");
    run("safety_run 400 3");
    check(np_safety_spi_get_withhold_report(&decl, &geom) && geom == 0U && decl == 0U,
          "geometry report: an area for each line empties the report");
}

/* ── The geometry gates, and the commands the MCU takes as sent ── */

/* @verifies REQ-SIMFW-14#c */
static void test_each_geometry_gate_holds_its_channel_until_an_area_arrives(void)
{
    boot(true, 2U);
    np_sim_safety_set_impedance_ohm(1U, 5000U);
    np_sim_safety_set_impedance_ohm(2U, 5000U);
    np_safety_spi_request_enable(EN_TDCS | EN_BES | EN_CLIN_STIM);
    np_safety_spi_set_channel_current(6U, 1000U);
    np_safety_spi_set_channel_current(5U, 1000U);
    np_safety_spi_set_channel_current(13U, 1000U);
    run("safety_geom tdcs 1");
    run("safety_geom bes 1");
    run("safety_geom clin_stim 1");
    start_session();
    run("safety_wave 6 1 0");
    settle();                                               /* a command frame is held until an iteration takes it, and the newest wins */
    run("safety_wave 5 2 20000");
    settle();
    run("safety_wave 13 2 20000");
    run("safety_run 800 3");
    check(!np_sim_safety_line_enabled(LINE_TDCS) && !np_sim_safety_line_enabled(LINE_BES) && !np_sim_safety_line_enabled(LINE_CLIN_STIM),
          "geometry required and none declared: tDCS, BES/tACS and clinical stimulation are all held off, though declared and in contact");
    run("safety_limits 6 0");                               /* an area of 0 means 'keep the current limit' */
    run("safety_run 400 3");
    check(!np_sim_safety_line_enabled(LINE_TDCS), "an area of 0 does not open the gate");
    run("safety_limits 6 100");
    run("safety_run 400 3");
    check(np_sim_safety_line_enabled(LINE_TDCS) && !np_sim_safety_line_enabled(LINE_BES) && !np_sim_safety_line_enabled(LINE_CLIN_STIM),
          "an area for tDCS opens the tDCS gate only");
    run("safety_limits 5 1000");
    run("safety_run 400 3");
    check(np_sim_safety_line_enabled(LINE_BES) && !np_sim_safety_line_enabled(LINE_CLIN_STIM), "an area for BES/tACS opens its gate, and not clinical stimulation's");
    run("safety_limits 13 1000");
    run("safety_run 400 3");
    check(np_sim_safety_line_enabled(LINE_CLIN_STIM), "an area for clinical stimulation opens the last gate");
    run("safety_geom tdcs 0");
    run("safety_limits 6 0");
    run("safety_run 400 3");
    check(np_sim_safety_line_enabled(LINE_TDCS), "a session that does not require geometry on a channel is not gated on it");
}

/* @verifies REQ-SIMFW-14#c */
static void test_the_charge_monitor_takes_the_hubs_area_and_phase_as_sent(void)
{
    boot(true, 0U);
    np_sim_safety_set_impedance_ohm(1U, 5000U);
    np_sim_safety_set_impedance_ohm(2U, 5000U);
    np_safety_spi_request_enable(EN_TDCS | EN_BES);
    np_safety_spi_set_channel_current(6U, 2000U);
    np_safety_spi_set_channel_current(5U, 1000U);
    start_session();
    declare_dc(6U, 100U);                                   /* 0.1 cm2: 15 mC, 7.5 s at 2 mA */
    settle();
    run("safety_wave 5 4 40000");                           /* sinusoidal, 40 ms half-period, 1 mA: 2/pi x 40 uC = 25 uC */
    run("safety_limits 5 1000");                            /* 1 cm2: 40 uC per phase */
    run("safety_run 2000 3");
    check(np_sim_safety_line_enabled(LINE_TDCS) && np_sim_safety_line_enabled(LINE_BES), "tDCS inside its 15 mC budget, and a sine inside 40 uC per phase: both driven");
    run("safety_limits 6 25000");                           /* the area frame is a checksum, not a signature: a larger one is taken, mid-session */
    run("safety_run 10000 3");
    check(np_sim_safety_line_enabled(LINE_TDCS), "an area raised mid-session to 25 cm2 raises the budget: the channel is not cut at 7.5 s (OI-SIMFW-34)");
}

/* @verifies REQ-SIMFW-14#c */
static void test_a_pulsed_channel_declared_with_no_phase_is_never_cut(void)
{
    boot(true, 0U);
    np_sim_safety_set_impedance_ohm(2U, 5000U);
    np_safety_spi_request_enable(EN_BES);
    np_safety_spi_set_channel_current(5U, 65535U);          /* 65 mA */
    start_session();
    run("safety_wave 5 0 20000");                           /* class 0 declares nothing */
    run("safety_limits 5 1000");                            /* 1 cm2: 40 uC per phase */
    run("safety_run 800 3");
    check(!np_sim_safety_line_enabled(LINE_BES), "a frame with waveform class 0 declares nothing: the channel stays held off");
    run("safety_wave 5 2 0");                               /* pulsed, declared phase 0 us */
    run("safety_run 2000 3");
    check(np_sim_safety_line_enabled(LINE_BES) && (status() & STATUS_CHARGE) == 0U, "a pulsed channel declared with a phase of 0 us is driven at 65 mA and never cut: its charge per phase computes as 0 (OI-SIMFW-34)");
    run("safety_wave 5 2 20000");                           /* the longest declared phase wins, and a declaration is never lowered */
    run("safety_run 800 3");
    check(!np_sim_safety_line_enabled(LINE_BES) && (status() & STATUS_CHARGE) != 0U, "a later frame that declares 20 ms makes the same current 1310 uC per phase: cut");
}

/* ── A tier record that does not verify ── */

static void a_tier_record_that_fails(unsigned reason)
{
    np_sim_safety_set_impedance_ohm(3U, 2000U);
    np_safety_spi_request_enable(EN_CVNS);
    run("safety_attach");
    run("safety_run 2000 3");
    uint8_t got = 0xFFU;
    bool refused = false;
    check(np_safety_spi_get_tier_report(&got, &refused) && got == reason, "the tier report names the reason");
    check(np_safety_spi_get_tier() == 1U && refused, "the unit is T1 and the request for a T2 line is reported refused");
    check(!np_sim_safety_line_enabled(LINE_CVNS), "and the cervical VNS line is withheld");
}

/* @verifies REQ-SIMFW-14#c */
static void test_a_tier_record_of_an_unknown_format_leaves_the_unit_t1(void)
{
    np_sim_set_sink(capture);
    np_sim_use_real_clock();
    np_simh_init();
    run("safety_root 7");
    run("safety_uid 9");
    run("safety_tier 3 1");                                 /* tier code 3: not a record this image knows */
    a_tier_record_that_fails(3U);
}

/* @verifies REQ-SIMFW-14#c */
static void test_a_tier_record_signed_for_another_unit_leaves_the_unit_t1(void)
{
    np_sim_set_sink(capture);
    np_sim_use_real_clock();
    np_simh_init();
    run("safety_root 7");
    run("safety_uid 9");
    run("safety_tier 2 1");                                 /* a genuine T2 record for unit 9 ... */
    run("safety_uid 10");                                   /* ... in the OTP of unit 10 */
    a_tier_record_that_fails(4U);
}

int main(void)
{
    in_child(test_the_link_reports_absence_until_the_mcu_is_attached);
    in_child(test_the_mcu_boots_with_every_enable_disabled);
    in_child(test_a_signed_session_is_granted_and_an_unsigned_one_is_not);
    in_child(test_a_bad_signature_is_a_cutoff);
    in_child(test_an_unprovisioned_otp_refuses_every_session);
    in_child(test_a_root_key_that_fails_its_crc_refuses_every_session);
    in_child(test_losing_the_heartbeat_cuts_every_line_at_the_watchdog_timeout);
    in_child(test_a_hung_loop_leaves_the_enables_until_the_independent_watchdog_expires);
    in_child(test_the_dc_charge_ceiling_cuts_the_channel);
    in_child(test_the_per_phase_ceiling_cuts_a_pulsed_channel_and_passes_one_inside_it);
    in_child(test_an_undeclared_electrical_channel_is_held_off);
    in_child(test_the_hub_learns_which_gate_withheld_a_line);
    in_child(test_the_impedance_gate_withholds_a_channel_until_the_contact_passes);
    in_child(test_the_thermal_interlock_cuts_at_its_cutoff_and_holds);
    in_child(test_a_t1_unit_withholds_the_cervical_vns_line);
    in_child(test_a_heart_rate_step_cuts_the_cervical_vns_line_and_latches);
    in_child(test_the_mcu_rejects_a_corrupt_heartbeat_and_then_cuts_at_the_watchdog);
    in_child(test_a_frozen_systick_is_caught_against_tim2);
    test_a_power_cycled_mcu_remembers_a_cardiac_cutoff();
    alone(test_a_flash_that_cannot_record_a_cutoff_raises_its_own_fault);
    np_sim_safety_silicon_erase();
    test_a_warm_reset_keeps_the_fault_latch_and_a_power_on_reset_clears_it();
    alone(test_three_failed_signatures_hard_lock_and_a_good_one_releases_the_next_session);
    alone(test_three_corrupt_signature_frames_latch_a_fault_that_no_session_clears);
    alone(test_a_cardiac_cutoff_is_reenabled_after_the_lockout_and_the_line_returns);
    alone(test_a_reenable_repeats_the_impedance_check);
    alone(test_without_the_hubs_estimate_the_line_is_withheld_silently);
    alone(test_a_hub_estimate_flagged_invalid_after_agreement_cuts_the_line);
    alone(test_a_hub_that_goes_quiet_after_agreement_cuts_the_line_when_its_estimate_ages_out);
    alone(test_a_stopped_r_peak_stream_cuts_the_cervical_line);
    alone(test_a_burst_of_edges_on_the_r_peak_line_is_read_as_an_artefact_and_cuts);
    alone(test_a_slow_rise_the_hub_agrees_with_is_cut_by_the_rate_history);
    alone(test_a_cardiac_cutoff_is_held_for_the_user_who_triggered_it_and_nobody_else);
    alone(test_a_flash_that_cannot_record_the_active_user_raises_its_own_fault);
    np_sim_safety_silicon_erase();
    test_the_cutoff_log_survives_a_page_rollover_and_a_power_cycle();
    np_sim_safety_silicon_erase();
    test_a_compaction_cut_short_loses_no_cutoff();
    alone(test_the_thermal_interlock_re_arms_below_its_hysteresis_and_the_hub_sensor_cuts_everything);
    alone(test_each_geometry_gate_holds_its_channel_until_an_area_arrives);
    alone(test_the_hub_learns_which_geometry_was_missing);
    alone(test_the_charge_monitor_takes_the_hubs_area_and_phase_as_sent);
    alone(test_a_pulsed_channel_declared_with_no_phase_is_never_cut);
    alone(test_a_tier_record_of_an_unknown_format_leaves_the_unit_t1);
    alone(test_a_tier_record_signed_for_another_unit_leaves_the_unit_t1);
    printf("%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
