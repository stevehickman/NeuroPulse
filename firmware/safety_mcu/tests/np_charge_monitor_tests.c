/*
 * NeurOne Safety MCU — SW01-M03 Charge Monitor Host Tests
 * Document: NP-SW-001 Rev 2 §SW01-M03, NP-FMEA-001 Rev 7 §3.3
 *
 * Tests the WAVEFORM-AWARE charge model (OI-CHARGE-05):
 *   - DC channels accumulate and are checked against a per-session mC/cm²
 *     budget by np_charge_monitor_tick().
 *   - Pulsed/AC channels never accumulate and are checked against a per-phase
 *     µC/cm² ceiling by np_charge_monitor_phase_tick().
 *   - np_charge_monitor_decl_gate() holds any UNDECLARED electrical channel
 *     out of granted_mask (fail-closed).
 *   - The OI-CHARGE-02/-03/-04 geometry path and its two independent gates.
 *
 * These tests exercise the monitor logic using a stubbed np_safety_state_t.
 * They do NOT require ARM cross-compilation or SPI hardware.
 *
 * IEC 62304 Class C — SW-01 safety MCU.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "../include/np_safety_config.h"
#include "../include/np_safety_protocol.h"

/* ── Test infrastructure ────────────────────────────────────────────────────── */

static int g_failures = 0;

static void check(int cond, const char *name)
{
    if (cond) {
        printf("PASS: %s\n", name);
    } else {
        printf("FAIL: %s\n", name);
        g_failures++;
    }
}

/* Forward-declare all public functions from np_charge_monitor.c */
extern np_safe_status_t np_charge_monitor_init(void);
extern void np_charge_monitor_accumulate(uint8_t channel, uint32_t current_ua, uint32_t dt_us);
extern void np_charge_monitor_tick(np_safety_state_t *state);
extern void np_charge_monitor_reset_session(np_safety_state_t *state);
extern void np_charge_monitor_set_channel_area_mcm2(uint8_t channel, uint16_t area_mcm2);
extern void np_charge_monitor_set_channel_waveform(uint8_t channel, uint8_t wave_class,
                                                   uint32_t phase_us);
extern void np_charge_monitor_geom_gate(np_safety_state_t *state);
extern void np_charge_monitor_decl_gate(np_safety_state_t *state);
extern bool np_charge_monitor_is_dc(uint8_t channel);
extern void np_charge_monitor_phase_tick(np_safety_state_t *state,
                                         const uint16_t    *current_ua,
                                         uint8_t            channel_count);

/* Charge-monitor channel indices = bit positions of the NP_SAFETY_EN_* bits. */
#define NP_SAFETY_CH_CLIN_STIM_IDX  13U
#define NP_SAFETY_CH_TDCS_IDX       6U
#define NP_SAFETY_CH_BES_IDX        5U
#define NP_SAFETY_CH_VNS_IDX        7U

#define HEARTBEAT_US  ((uint32_t)NP_SAFETY_HEARTBEAT_EXP_MS * 1000UL)  /* 200000 */

/* ── Helper: build a clean state ────────────────────────────────────────────── */
static np_safety_state_t fresh_state(void)
{
    np_safety_state_t s;
    memset(&s, 0, sizeof(s));
    s.fault_slot    = 0xFFU;
    s.granted_mask  = 0x3FFFU;  /* all 14 channels granted */
    s.session_active = true;
    return s;
}

/* Declare a channel DC with a given pad area (the common tDCS setup). */
static void declare_dc(uint8_t ch, uint16_t area_mcm2)
{
    np_charge_monitor_set_channel_area_mcm2(ch, area_mcm2);
    np_charge_monitor_set_channel_waveform(ch, NP_CHARGE_WAVE_DC, 0U);
}

/* One heartbeat's worth of commanded current, as the main loop would. */
static void beat(uint8_t ch, uint32_t ua)
{
    np_charge_monitor_accumulate(ch, ua, HEARTBEAT_US);
}

/* Build a current_ua[] array with one channel commanding. */
static void one_current(uint16_t *buf, uint8_t ch, uint16_t ua)
{
    memset(buf, 0, sizeof(uint16_t) * NP_SAFETY_MAX_CHANNELS);
    buf[ch] = ua;
}

/* ── Test: init returns OK and no tick fires on zero accumulation ────────────── */

static void test_init_clean(void)
{
    np_safe_status_t rc = np_charge_monitor_init();
    check(rc == NP_SAFE_OK, "init returns NP_SAFE_OK");

    np_safety_state_t s = fresh_state();
    np_charge_monitor_tick(&s);

    check(s.granted_mask == 0x3FFFU,
          "tick with zero charge: granted_mask unchanged");
    check((s.status & NP_SAFETY_STATUS_CHARGE) == 0U,
          "tick with zero charge: CHARGE bit not set");
    check(s.fault_slot == 0xFFU,
          "tick with zero charge: fault_slot == 0xFF");
}

/* ── Test: the DC per-session budget is enforced ─────────────────────────────── */

static void test_dc_session_limit_enforced(void)
{
    np_charge_monitor_init();
    /* 35 cm² sponge pad → 150 mC/cm² × 35 cm² = 5250 mC = 5.25e9 nC. */
    declare_dc(NP_SAFETY_CH_TDCS_IDX, 35000U);

    /* 5.24 C in one synthetic push: 2000 µA × 2,620,000,000 µs / 1000.
     * Split into two calls to stay inside uint32 dt.                        */
    np_charge_monitor_accumulate(NP_SAFETY_CH_TDCS_IDX, 2000U, 1310000000UL);
    np_charge_monitor_accumulate(NP_SAFETY_CH_TDCS_IDX, 2000U, 1310000000UL);

    np_safety_state_t s = fresh_state();
    np_charge_monitor_tick(&s);
    check((s.status & NP_SAFETY_STATUS_CHARGE) == 0U,
          "DC: 5.24 C < 5.25 C budget on a 35 cm² pad → no trip");

    /* One more heartbeat at 2 mA (400 µC) crosses it. */
    for (int i = 0; i < 30; i++) { beat(NP_SAFETY_CH_TDCS_IDX, 2000U); }
    np_charge_monitor_tick(&s);
    check((s.status & NP_SAFETY_STATUS_CHARGE) != 0U,
          "DC: crossing the 5.25 C budget sets CHARGE");
    check((s.status & NP_SAFETY_STATUS_CUTOFF) != 0U,
          "DC: crossing the budget sets CUTOFF");
    check((s.granted_mask & (1U << NP_SAFETY_CH_TDCS_IDX)) == 0U,
          "DC: tDCS channel cut from granted_mask");
    check(s.fault_slot == NP_SAFETY_CH_TDCS_IDX,
          "DC: fault_slot names the channel that tripped");
}

/*
 * THE REGRESSION THIS WHOLE ITEM EXISTS FOR.  Under the retired model the
 * 40-µC/cm²-against-a-session-integral ceiling gave a 35 cm² pad a 1.4 mC
 * budget, which 2 mA exhausts in 0.7 s — less than the 30 s ramp the firmware
 * enforces as a MINIMUM, and far less than the routine clinical protocol.
 * 2 mA × 20 min on a 35 cm² pad is 68.6 mC/cm², and it must now run to
 * completion.
 */
static void test_two_ma_twenty_minutes_completes(void)
{
    np_charge_monitor_init();
    declare_dc(NP_SAFETY_CH_TDCS_IDX, 35000U);

    np_safety_state_t s = fresh_state();

    /* 20 minutes of 200 ms heartbeats at a commanded 2 mA = 6000 beats. */
    for (int i = 0; i < 6000; i++) {
        beat(NP_SAFETY_CH_TDCS_IDX, 2000U);
        np_charge_monitor_tick(&s);
    }

    check((s.granted_mask & (1U << NP_SAFETY_CH_TDCS_IDX)) != 0U,
          "2 mA × 20 min on a 35 cm² pad runs to completion (68.6 of 150 mC/cm²)");
    check((s.status & NP_SAFETY_STATUS_CHARGE) == 0U,
          "2 mA × 20 min never raises CHARGE");

    /* And the first 0.7 s — where the retired ceiling tripped — is clean. */
    np_charge_monitor_init();
    declare_dc(NP_SAFETY_CH_TDCS_IDX, 35000U);
    np_safety_state_t s2 = fresh_state();
    for (int i = 0; i < 4; i++) {          /* 4 beats = 0.8 s */
        beat(NP_SAFETY_CH_TDCS_IDX, 2000U);
        np_charge_monitor_tick(&s2);
    }
    check((s2.granted_mask & (1U << NP_SAFETY_CH_TDCS_IDX)) != 0U,
          "2 mA survives the first 0.8 s (the retired ceiling cut it at 0.7 s)");
}

/* ── Test: pulsed channels never accumulate a session dose ───────────────────── */

/*
 * BES/tACS, VNS, cervical VNS and clinical tACS are charge-balanced biphasic:
 * net delivered charge is ~zero, so a session integral of |I| is not a dose.
 * Under the retired model they would have tripped in 0.4–1.0 s at their rated
 * currents.  np_charge_monitor_tick() must ignore them entirely.
 */
static void test_pulsed_channel_never_accumulates(void)
{
    np_charge_monitor_init();
    np_charge_monitor_set_channel_area_mcm2(NP_SAFETY_CH_VNS_IDX, 500U);
    np_charge_monitor_set_channel_waveform(NP_SAFETY_CH_VNS_IDX,
                                           NP_CHARGE_WAVE_PULSE, 250U);

    check(!np_charge_monitor_is_dc(NP_SAFETY_CH_VNS_IDX),
          "pulsed: is_dc() false for a PULSE-declared channel");

    np_safety_state_t s = fresh_state();
    /* An hour of 2 mA heartbeats — vastly beyond any DC budget. */
    for (int i = 0; i < 18000; i++) {
        beat(NP_SAFETY_CH_VNS_IDX, 2000U);
        np_charge_monitor_tick(&s);
    }
    check((s.granted_mask & (1U << NP_SAFETY_CH_VNS_IDX)) != 0U,
          "pulsed: an hour at 2 mA never trips the session budget");
    check((s.status & NP_SAFETY_STATUS_CHARGE) == 0U,
          "pulsed: session budget raises no CHARGE on a pulsed channel");
}

/* ── Test: the per-phase ceiling ─────────────────────────────────────────────── */

static void test_phase_ceiling_pulse(void)
{
    uint16_t cur[NP_SAFETY_MAX_CHANNELS];

    np_charge_monitor_init();
    /* VNS auricular clip: 0.5 cm² → per-phase budget 40 × 500 = 20,000 nC = 20 µC. */
    np_charge_monitor_set_channel_area_mcm2(NP_SAFETY_CH_VNS_IDX, 500U);
    np_charge_monitor_set_channel_waveform(NP_SAFETY_CH_VNS_IDX,
                                           NP_CHARGE_WAVE_PULSE, 250U);

    /* Rated worst case: 2 mA × 250 µs = 500 nC — 2.5% of budget. */
    np_safety_state_t s = fresh_state();
    one_current(cur, NP_SAFETY_CH_VNS_IDX, 2000U);
    np_charge_monitor_phase_tick(&s, cur, NP_SAFETY_MAX_CHANNELS);
    check((s.granted_mask & (1U << NP_SAFETY_CH_VNS_IDX)) != 0U,
          "per-phase: VNS at its rated 2 mA / 250 µs passes (0.5 of 20 µC)");

    /* A 10,000 µs phase at 2 mA is 20 µC — exactly the ceiling, so it trips. */
    np_charge_monitor_init();
    np_charge_monitor_set_channel_area_mcm2(NP_SAFETY_CH_VNS_IDX, 500U);
    np_charge_monitor_set_channel_waveform(NP_SAFETY_CH_VNS_IDX,
                                           NP_CHARGE_WAVE_PULSE, 10000U);
    np_safety_state_t s2 = fresh_state();
    np_charge_monitor_phase_tick(&s2, cur, NP_SAFETY_MAX_CHANNELS);
    check((s2.granted_mask & (1U << NP_SAFETY_CH_VNS_IDX)) == 0U,
          "per-phase: 2 mA × 10 ms = 20 µC hits the 20 µC ceiling → cut");
    check((s2.status & NP_SAFETY_STATUS_CHARGE) != 0U,
          "per-phase: exceeding the ceiling sets CHARGE");
    check(s2.fault_slot == NP_SAFETY_CH_VNS_IDX,
          "per-phase: fault_slot names the channel");
}

/*
 * The 2/pi factor for a sine is load-bearing, not a refinement.  At identical
 * amplitude and phase duration a sinusoid delivers 2/pi of what a rectangular
 * pulse does, and at the 0.5 Hz bottom of the BES/tACS band — where the
 * half-period is a full second and phase charge is largest — that is exactly
 * the difference between passing and failing.
 */
static void test_phase_ceiling_sine_vs_square(void)
{
    uint16_t cur[NP_SAFETY_MAX_CHANNELS];
    one_current(cur, NP_SAFETY_CH_BES_IDX, 1000U);   /* 1 mA, the T1 tACS cap */

    /* 25 cm² pad → per-phase budget 40 × 25000 = 1,000,000 nC = 1000 µC.
     * 0.5 Hz → half-period 1,000,000 µs.
     *   square: 1000 µA × 1e6 µs = 1,000,000 nC  → AT the ceiling, trips
     *   sine:   × 2/pi           =   636,740 nC  → 64% of it, passes        */
    np_charge_monitor_init();
    np_charge_monitor_set_channel_area_mcm2(NP_SAFETY_CH_BES_IDX, 25000U);
    np_charge_monitor_set_channel_waveform(NP_SAFETY_CH_BES_IDX,
                                           NP_CHARGE_WAVE_SINE, 1000000UL);
    np_safety_state_t s = fresh_state();
    np_charge_monitor_phase_tick(&s, cur, NP_SAFETY_MAX_CHANNELS);
    check((s.granted_mask & (1U << NP_SAFETY_CH_BES_IDX)) != 0U,
          "per-phase: 1 mA 0.5 Hz SINE passes (636 of 1000 µC)");

    np_charge_monitor_init();
    np_charge_monitor_set_channel_area_mcm2(NP_SAFETY_CH_BES_IDX, 25000U);
    np_charge_monitor_set_channel_waveform(NP_SAFETY_CH_BES_IDX,
                                           NP_CHARGE_WAVE_PULSE, 1000000UL);
    np_safety_state_t s2 = fresh_state();
    np_charge_monitor_phase_tick(&s2, cur, NP_SAFETY_MAX_CHANNELS);
    check((s2.granted_mask & (1U << NP_SAFETY_CH_BES_IDX)) == 0U,
          "per-phase: the same 1 mA / 1 s as a SQUARE wave trips (1000 µC)");
}

/* A channel commanding nothing, or not granted, is not judged. */
static void test_phase_tick_ignores_idle_channels(void)
{
    uint16_t cur[NP_SAFETY_MAX_CHANNELS];

    np_charge_monitor_init();
    np_charge_monitor_set_channel_area_mcm2(NP_SAFETY_CH_VNS_IDX, 500U);
    np_charge_monitor_set_channel_waveform(NP_SAFETY_CH_VNS_IDX,
                                           NP_CHARGE_WAVE_PULSE, 10000U);

    /* Commanding 0 µA: over-ceiling phase width, but nothing is flowing. */
    np_safety_state_t s = fresh_state();
    one_current(cur, NP_SAFETY_CH_VNS_IDX, 0U);
    np_charge_monitor_phase_tick(&s, cur, NP_SAFETY_MAX_CHANNELS);
    check((s.status & NP_SAFETY_STATUS_CHARGE) == 0U,
          "per-phase: a channel commanding 0 µA is not judged");

    /* Granted bit already clear: nothing to cut, no fault to raise. */
    np_safety_state_t s2 = fresh_state();
    s2.granted_mask &= (uint16_t)~(1U << NP_SAFETY_CH_VNS_IDX);
    one_current(cur, NP_SAFETY_CH_VNS_IDX, 2000U);
    np_charge_monitor_phase_tick(&s2, cur, NP_SAFETY_MAX_CHANNELS);
    check((s2.status & NP_SAFETY_STATUS_CHARGE) == 0U,
          "per-phase: an ungranted channel is not judged");

    /* NULL array must be a no-op, not a crash. */
    np_safety_state_t s3 = fresh_state();
    np_charge_monitor_phase_tick(&s3, NULL, NP_SAFETY_MAX_CHANNELS);
    check(s3.granted_mask == 0x3FFFU, "per-phase: NULL current array is a no-op");
}

/* ── Test: the fail-closed declaration gate ─────────────────────────────────── */

static void test_decl_gate_blocks_undeclared_electrical(void)
{
    np_charge_monitor_init();

    np_safety_state_t s = fresh_state();
    np_charge_monitor_decl_gate(&s);

    check((s.granted_mask & NP_SAFETY_EN_TDCS) == 0U,
          "decl gate: undeclared tDCS blocked");
    check((s.granted_mask & NP_SAFETY_EN_BES_TACS) == 0U,
          "decl gate: undeclared BES/tACS blocked");
    check((s.granted_mask & NP_SAFETY_EN_VNS_HRV) == 0U,
          "decl gate: undeclared VNS blocked");
    check((s.granted_mask & NP_SAFETY_EN_CVNS) == 0U,
          "decl gate: undeclared cervical VNS blocked");
    check((s.granted_mask & NP_SAFETY_EN_CLIN_STIM) == 0U,
          "decl gate: undeclared clinical stim blocked");

    /* Non-electrical channels inject no charge through electrodes and must
     * never be gated by this — PBM and visual would stop working. */
    check((s.granted_mask & NP_SAFETY_EN_PBM_CRANIAL) != 0U,
          "decl gate: PBM cranial never gated");
    check((s.granted_mask & NP_SAFETY_EN_VISUAL) != 0U,
          "decl gate: visual never gated");
    check((s.granted_mask & NP_SAFETY_EN_TMS) != 0U,
          "decl gate: TMS never gated");
    check((s.granted_mask & NP_SAFETY_EN_INTRANASAL) != 0U,
          "decl gate: intranasal PBM never gated");
    check((s.granted_mask & NP_SAFETY_EN_PBM_1170NM) != 0U,
          "decl gate: 1170nm PBM never gated");
}

static void test_decl_gate_opens_on_declaration(void)
{
    np_charge_monitor_init();
    np_charge_monitor_set_channel_waveform(NP_SAFETY_CH_TDCS_IDX,
                                           NP_CHARGE_WAVE_DC, 0U);

    np_safety_state_t s = fresh_state();
    np_charge_monitor_decl_gate(&s);
    check((s.granted_mask & NP_SAFETY_EN_TDCS) != 0U,
          "decl gate: tDCS granted once declared");
    check((s.granted_mask & NP_SAFETY_EN_BES_TACS) == 0U,
          "decl gate: declaring one channel does not open another");
}

static void test_decl_gate_rearms_each_session(void)
{
    np_charge_monitor_init();
    np_charge_monitor_set_channel_waveform(NP_SAFETY_CH_TDCS_IDX,
                                           NP_CHARGE_WAVE_DC, 0U);
    np_charge_monitor_reset_session(NULL);

    np_safety_state_t s = fresh_state();
    np_charge_monitor_decl_gate(&s);
    check((s.granted_mask & NP_SAFETY_EN_TDCS) == 0U,
          "decl gate: re-armed by reset_session (a declaration never outlives its session)");
}

/* A zero class must not clear an existing declaration — a partial frame
 * declares some channels and says nothing about the rest. */
static void test_zero_class_does_not_undeclare(void)
{
    np_charge_monitor_init();
    np_charge_monitor_set_channel_waveform(NP_SAFETY_CH_TDCS_IDX,
                                           NP_CHARGE_WAVE_DC, 0U);
    np_charge_monitor_set_channel_waveform(NP_SAFETY_CH_TDCS_IDX, 0U, 0U);

    check(np_charge_monitor_is_dc(NP_SAFETY_CH_TDCS_IDX),
          "a zero wave_class leaves an existing declaration intact");
}

/*
 * NP_SAFETY_CH_CLIN_STIM carries HD-tDCS (DC) and clinical tACS (AC) on one
 * enable bit.  A session with both must be held to BOTH ceilings — the classes
 * OR together rather than the last one written winning.
 */
static void test_mixed_class_channel_runs_both_checks(void)
{
    uint16_t cur[NP_SAFETY_MAX_CHANNELS];

    np_charge_monitor_init();
    np_charge_monitor_set_channel_area_mcm2(NP_SAFETY_CH_CLIN_STIM_IDX, 25000U);
    np_charge_monitor_set_channel_waveform(NP_SAFETY_CH_CLIN_STIM_IDX,
                                           NP_CHARGE_WAVE_DC, 0U);
    np_charge_monitor_set_channel_waveform(NP_SAFETY_CH_CLIN_STIM_IDX,
                                           NP_CHARGE_WAVE_PULSE, 1000000UL);

    check(np_charge_monitor_is_dc(NP_SAFETY_CH_CLIN_STIM_IDX),
          "mixed class: the DC declaration survives a later AC declaration");

    /* The per-phase check still applies: 1 mA × 1 s = 1000 µC = the ceiling. */
    np_safety_state_t s = fresh_state();
    one_current(cur, NP_SAFETY_CH_CLIN_STIM_IDX, 1000U);
    np_charge_monitor_phase_tick(&s, cur, NP_SAFETY_MAX_CHANNELS);
    check((s.granted_mask & (1U << NP_SAFETY_CH_CLIN_STIM_IDX)) == 0U,
          "mixed class: the per-phase ceiling still bites on a DC-declared channel");
}

/* ── Test: reset_session clears accumulators and the latched fault ──────────── */

static void test_reset_clears_accumulators(void)
{
    np_charge_monitor_init();
    declare_dc(3U, 25000U);

    /* Push channel 3 past its 3.75 C budget. */
    np_charge_monitor_accumulate(3U, 2000U, 2000000000UL);
    np_charge_monitor_accumulate(3U, 2000U, 2000000000UL);

    np_safety_state_t s = fresh_state();
    np_charge_monitor_tick(&s);
    check((s.status & NP_SAFETY_STATUS_CHARGE) != 0U, "ch 3 tripped before reset");

    /* Reset on the SAME persistent state (as the real main loop does — s_state is
     * a static that survives session boundaries).  reset_session must clear the
     * latched CHARGE bit in place, otherwise np_spi_watchdog_tick would keep
     * forcing granted_mask=0 for the entire power cycle. */
    np_charge_monitor_reset_session(&s);
    check((s.status & NP_SAFETY_STATUS_CHARGE) == 0U,
          "CHARGE cleared in persistent state after reset_session");

    /* Accumulators and limit_reached flags must also be cleared. */
    declare_dc(3U, 25000U);
    s = fresh_state();
    np_charge_monitor_tick(&s);
    check((s.status & NP_SAFETY_STATUS_CHARGE) == 0U, "CHARGE stays clear after reset_session");
    check(s.granted_mask == 0x3FFFU,                  "granted_mask full after reset_session");
}

/* ── Test: out-of-range channel is silently ignored ─────────────────────────── */

static void test_out_of_range_channel_ignored(void)
{
    np_charge_monitor_init();
    np_charge_monitor_accumulate(14U, 99999U, 999999UL);   /* channel 14 = out of range */
    np_charge_monitor_accumulate(255U, 99999U, 999999UL);  /* definitely out of range */
    np_charge_monitor_set_channel_area_mcm2(14U, 1U);      /* also out of range */
    np_charge_monitor_set_channel_waveform(200U, NP_CHARGE_WAVE_DC, 10U);

    np_safety_state_t s = fresh_state();
    np_charge_monitor_tick(&s);

    check((s.status & NP_SAFETY_STATUS_CHARGE) == 0U,
          "out-of-range channel does not trigger charge fault");
    check(s.granted_mask == 0x3FFFU,
          "granted_mask unaffected by out-of-range accumulate");
    check(!np_charge_monitor_is_dc(200U),
          "is_dc() is false for an out-of-range channel");
}

/* ── Test: tick is idempotent after limit reached ────────────────────────────── */

static void test_tick_idempotent_after_trip(void)
{
    np_charge_monitor_init();
    declare_dc(0U, 25000U);

    np_charge_monitor_accumulate(0U, 2000U, 2000000000UL);
    np_charge_monitor_accumulate(0U, 2000U, 2000000000UL);

    np_safety_state_t s = fresh_state();
    np_charge_monitor_tick(&s);  /* first tick: trips */

    uint16_t mask_after_first   = s.granted_mask;
    uint8_t  status_after_first = s.status;

    /* Continue accumulating — limit_reached flag prevents double-trip */
    beat(0U, 2000U);
    np_charge_monitor_tick(&s);  /* second tick: already tripped */

    check(s.granted_mask == mask_after_first,
          "granted_mask stable after second tick (idempotent)");
    check(s.status == status_after_first,
          "status stable after second tick (idempotent)");
}

/* ── Test: charge accumulation arithmetic ───────────────────────────────────── */

static void test_charge_accumulation_math(void)
{
    np_charge_monitor_init();
    /* 1 cm² electrode → 150 mC/cm² × 1 cm² = 150 mC = 150,000,000 nC. */
    declare_dc(6U, 1000U);

    /* 200 µA for 200 ms = 200 × 200000 / 1000 = 40,000 nC = 40 µC. */
    beat(6U, 200U);
    np_safety_state_t s = fresh_state();
    np_charge_monitor_tick(&s);
    check((s.status & NP_SAFETY_STATUS_CHARGE) == 0U,
          "40 µC << 150 mC budget: CHARGE not set");

    /* 3750 beats total = 3750 × 40 µC = 150 mC, exactly the budget. */
    for (int i = 0; i < 3749; i++) { beat(6U, 200U); }
    np_charge_monitor_tick(&s);
    check((s.status & NP_SAFETY_STATUS_CHARGE) != 0U,
          "3750 × 40 µC = 150 mC == budget: CHARGE set");
    check((s.granted_mask & (1U << 6)) == 0U,
          "ch 6 cut at its 150 mC budget");
}

/* ── Test: per-channel geometry (OI-CHARGE-02) ──────────────────────────────── */

static void test_set_channel_area_hd_tdcs(void)
{
    uint16_t cur[NP_SAFETY_MAX_CHANNELS];

    np_charge_monitor_init();
    /* HD-tDCS 3.5mm electrode: 96 milli-cm² → per-phase 40 × 96 = 3840 nC
     * = 3.84 µC, 260× tighter than a 25 cm² pad's 1000 µC.  With a 2 ms phase
     * that budget is spent at 1.92 mA, just under the modality's 2 mA cap —
     * which is the point of declaring the geometry at all.                  */
    np_charge_monitor_set_channel_area_mcm2(NP_SAFETY_CH_CLIN_STIM_IDX, 96U);
    np_charge_monitor_set_channel_waveform(NP_SAFETY_CH_CLIN_STIM_IDX,
                                           NP_CHARGE_WAVE_PULSE, 2000U);

    /* 1.9 mA × 2000 µs = 3800 nC — just under 3840. */
    np_safety_state_t s = fresh_state();
    one_current(cur, NP_SAFETY_CH_CLIN_STIM_IDX, 1900U);
    np_charge_monitor_phase_tick(&s, cur, NP_SAFETY_MAX_CHANNELS);
    check((s.granted_mask & (1U << NP_SAFETY_CH_CLIN_STIM_IDX)) != 0U,
          "HD-tDCS geometry: 3.80 µC < 3.84 µC per-phase limit → no trip");

    /* 2.0 mA × 2000 µs = 4000 nC — over 3840. */
    np_safety_state_t s2 = fresh_state();
    one_current(cur, NP_SAFETY_CH_CLIN_STIM_IDX, 2000U);
    np_charge_monitor_phase_tick(&s2, cur, NP_SAFETY_MAX_CHANNELS);
    check((s2.granted_mask & (1U << NP_SAFETY_CH_CLIN_STIM_IDX)) == 0U,
          "HD-tDCS geometry: 4.00 µC > 3.84 µC per-phase limit → cut");
}

static void test_set_channel_area_default_pad(void)
{
    np_charge_monitor_init();

    /* Area 0 = keep current value: channel 4 keeps the 25 cm² fallback, so its
     * DC budget is 150 × 25 = 3750 mC.  1 mC must not trip it.              */
    np_charge_monitor_set_channel_area_mcm2(4U, 0U);
    np_charge_monitor_set_channel_waveform(4U, NP_CHARGE_WAVE_DC, 0U);
    np_charge_monitor_accumulate(4U, 1000U, 1000000UL);     /* 1 mC */
    np_safety_state_t s = fresh_state();
    np_charge_monitor_tick(&s);
    check((s.status & NP_SAFETY_STATUS_CHARGE) == 0U,
          "area 0 leaves the fallback geometry intact (1 mC << 3750 mC)");
}

static void test_area_override_isolated(void)
{
    np_charge_monitor_init();

    /* Tighten CLIN_STIM (ch 13) via area; ch 0 must keep the fallback. */
    np_charge_monitor_set_channel_area_mcm2(NP_SAFETY_CH_CLIN_STIM_IDX, 96U);
    declare_dc(0U, 0U);   /* declare DC, no area → 25 cm² fallback, 3750 mC */

    np_charge_monitor_accumulate(0U, 1000U, 500000000UL);   /* 500 mC */
    np_safety_state_t s = fresh_state();
    np_charge_monitor_tick(&s);
    check((s.granted_mask & (1U << 0)) != 0U,
          "untouched ch 0 keeps its fallback budget after a ch 13 area override");
    check((s.status & NP_SAFETY_STATUS_CHARGE) == 0U,
          "500 mC on a fallback-geometry channel does not trip");
}

/* ── Tests: OI-CHARGE-03 / -04 geometry gates ───────────────────────────────── */

static void test_geom_gate_blocks_until_applied(void)
{
    np_charge_monitor_reset_session(NULL);   /* geom NOT applied yet */

    np_safety_state_t s = fresh_state();
    s.geom_required = true;
    np_charge_monitor_geom_gate(&s);
    check((s.granted_mask & (1U << NP_SAFETY_CH_CLIN_STIM_IDX)) == 0U,
          "geom gate: CLIN_STIM blocked when required and not applied (fail-closed)");
    check((s.granted_mask & (1U << NP_SAFETY_CH_TDCS_IDX)) != 0U,
          "geom gate: TDCS unaffected by the CLIN_STIM gate");
    check((s.granted_mask & (1U << 7)) != 0U,
          "geom gate: ungated channels unaffected");

    np_charge_monitor_set_channel_area_mcm2(NP_SAFETY_CH_CLIN_STIM_IDX, 96U);
    np_safety_state_t s2 = fresh_state();
    s2.geom_required = true;
    np_charge_monitor_geom_gate(&s2);
    check((s2.granted_mask & (1U << NP_SAFETY_CH_CLIN_STIM_IDX)) != 0U,
          "geom gate: CLIN_STIM granted after area applied");
}

static void test_geom_gate_not_required_passes(void)
{
    np_charge_monitor_reset_session(NULL);

    np_safety_state_t s = fresh_state();
    s.geom_required = false;
    np_charge_monitor_geom_gate(&s);
    check((s.granted_mask & (1U << NP_SAFETY_CH_CLIN_STIM_IDX)) != 0U,
          "geom gate: CLIN_STIM NOT blocked when geom_required is false (tACS)");
}

static void test_geom_gate_rearms_each_session(void)
{
    np_charge_monitor_reset_session(NULL);
    np_charge_monitor_set_channel_area_mcm2(NP_SAFETY_CH_CLIN_STIM_IDX, 96U);

    np_charge_monitor_reset_session(NULL);   /* new session — applied flag cleared */
    np_safety_state_t s = fresh_state();
    s.geom_required = true;
    np_charge_monitor_geom_gate(&s);
    check((s.granted_mask & (1U << NP_SAFETY_CH_CLIN_STIM_IDX)) == 0U,
          "geom gate: re-armed after reset_session (blocks again until re-applied)");
}

static void test_tdcs_geom_gate_blocks_until_applied(void)
{
    np_charge_monitor_reset_session(NULL);

    np_safety_state_t s = fresh_state();
    s.geom_required_tdcs = true;
    np_charge_monitor_geom_gate(&s);
    check((s.granted_mask & (1U << NP_SAFETY_CH_TDCS_IDX)) == 0U,
          "tdcs geom gate: TDCS blocked when required and not applied (fail-closed)");
    check((s.granted_mask & (1U << NP_SAFETY_CH_CLIN_STIM_IDX)) != 0U,
          "tdcs geom gate: CLIN_STIM unaffected by the tDCS gate");

    np_charge_monitor_set_channel_area_mcm2(NP_SAFETY_CH_TDCS_IDX, 35000U);
    np_safety_state_t s2 = fresh_state();
    s2.geom_required_tdcs = true;
    np_charge_monitor_geom_gate(&s2);
    check((s2.granted_mask & (1U << NP_SAFETY_CH_TDCS_IDX)) != 0U,
          "tdcs geom gate: TDCS granted after declared area applied");

    /* The DECLARED pad, not the fallback, sets the budget: 150 mC/cm² × 35 cm²
     * = 5250 mC, where the 25 cm² fallback would give 3750 mC.  4000 mC would
     * have tripped the fallback and must not trip the declared pad.  That
     * difference IS OI-CHARGE-04 — the app authored against one geometry and
     * the enforcer used another.                                             */
    np_charge_monitor_set_channel_waveform(NP_SAFETY_CH_TDCS_IDX,
                                           NP_CHARGE_WAVE_DC, 0U);
    np_charge_monitor_accumulate(NP_SAFETY_CH_TDCS_IDX, 2000U, 2000000000UL);  /* 4000 mC */
    np_charge_monitor_tick(&s2);
    check((s2.granted_mask & (1U << NP_SAFETY_CH_TDCS_IDX)) != 0U,
          "tdcs geom gate: 4000 mC does not trip a declared 35 cm² pad (5250 mC)");
    np_charge_monitor_accumulate(NP_SAFETY_CH_TDCS_IDX, 2000U, 700000000UL);   /* +1400 mC */
    np_charge_monitor_tick(&s2);
    check((s2.granted_mask & (1U << NP_SAFETY_CH_TDCS_IDX)) == 0U,
          "tdcs geom gate: 5400 mC trips the declared 35 cm² budget");
}

static void test_geom_gates_are_independent(void)
{
    np_charge_monitor_reset_session(NULL);
    np_charge_monitor_set_channel_area_mcm2(NP_SAFETY_CH_TDCS_IDX, 25000U);

    np_safety_state_t s = fresh_state();
    s.geom_required      = false;   /* clinical tACS declares no geometry */
    s.geom_required_tdcs = true;    /* tDCS does, and it was applied      */
    np_charge_monitor_geom_gate(&s);
    check((s.granted_mask & (1U << NP_SAFETY_CH_TDCS_IDX)) != 0U,
          "gate independence: TDCS granted (its own geometry applied)");
    check((s.granted_mask & (1U << NP_SAFETY_CH_CLIN_STIM_IDX)) != 0U,
          "gate independence: clinical tACS not gated by the tDCS declaration");

    np_charge_monitor_reset_session(NULL);
    np_charge_monitor_set_channel_area_mcm2(NP_SAFETY_CH_CLIN_STIM_IDX, 96U);
    np_safety_state_t s2 = fresh_state();
    s2.geom_required      = true;
    s2.geom_required_tdcs = true;
    np_charge_monitor_geom_gate(&s2);
    check((s2.granted_mask & (1U << NP_SAFETY_CH_CLIN_STIM_IDX)) != 0U,
          "gate independence: CLIN_STIM granted (its own geometry applied)");
    check((s2.granted_mask & (1U << NP_SAFETY_CH_TDCS_IDX)) == 0U,
          "gate independence: TDCS still blocked (its geometry never landed)");
}

static void test_tdcs_geom_gate_rearms_each_session(void)
{
    np_charge_monitor_reset_session(NULL);
    np_charge_monitor_set_channel_area_mcm2(NP_SAFETY_CH_TDCS_IDX, 35000U);

    np_charge_monitor_reset_session(NULL);
    np_safety_state_t s = fresh_state();
    s.geom_required_tdcs = true;
    np_charge_monitor_geom_gate(&s);
    check((s.granted_mask & (1U << NP_SAFETY_CH_TDCS_IDX)) == 0U,
          "tdcs geom gate: re-armed after reset_session (blocks until re-declared)");
}

/* ── OI-MMSOCK-02 (NP-FW-MMSOCK-001 §3.6.1, §5.3 C-1): the BES/tACS gate ──── */

/* Fail-closed until declared, and the DECLARED area — not the 25 cm² fallback
 * — sets the per-phase ceiling.  A 1 cm² lattice electrode is 1000 mcm² →
 * 40 µC ceiling = 40,000 nC.  1 mA sine at 5 Hz: phase 100 ms, the monitor
 * charges 1000 µA × 100,000 µs / 1000 = 100,000 nC (it treats the commanded
 * amplitude as a rectangular phase — the conservative reading).  That passes
 * the 25 cm² fallback (1,000,000 nC) and must trip the declared 1 cm²; that
 * gap is exactly the ~24x fail-open this gate closes.                       */
static void test_bes_geom_gate_blocks_until_applied(void)
{
    uint16_t cur[NP_SAFETY_MAX_CHANNELS];
    np_charge_monitor_reset_session(NULL);

    np_safety_state_t s = fresh_state();
    s.geom_required_bes = true;
    np_charge_monitor_geom_gate(&s);
    check((s.granted_mask & (1U << NP_SAFETY_CH_BES_IDX)) == 0U,
          "bes geom gate: BES_TACS blocked when required and not applied (fail-closed)");
    check((s.granted_mask & (1U << NP_SAFETY_CH_TDCS_IDX)) != 0U &&
          (s.granted_mask & (1U << NP_SAFETY_CH_CLIN_STIM_IDX)) != 0U,
          "bes geom gate: TDCS and CLIN_STIM unaffected by the BES gate");

    np_charge_monitor_set_channel_area_mcm2(NP_SAFETY_CH_BES_IDX, 1000U);
    np_safety_state_t s2 = fresh_state();
    s2.geom_required_bes = true;
    np_charge_monitor_geom_gate(&s2);
    check((s2.granted_mask & (1U << NP_SAFETY_CH_BES_IDX)) != 0U,
          "bes geom gate: BES_TACS granted after declared area applied");

    np_charge_monitor_set_channel_waveform(NP_SAFETY_CH_BES_IDX,
                                           NP_CHARGE_WAVE_PULSE, 100000UL);
    one_current(cur, NP_SAFETY_CH_BES_IDX, 1000U);
    np_charge_monitor_phase_tick(&s2, cur, NP_SAFETY_MAX_CHANNELS);
    check((s2.granted_mask & (1U << NP_SAFETY_CH_BES_IDX)) == 0U,
          "bes geom gate: 1 mA / 100 ms phase trips a declared 1 cm² electrode "
          "(it would pass the 25 cm² fallback)");
}

static void test_bes_geom_gate_independent_and_rearms(void)
{
    /* Not required → never blocks, even with no area: a session with no
     * BES/tACS command must not be affected by the gate's existence.      */
    np_charge_monitor_reset_session(NULL);
    np_safety_state_t s = fresh_state();
    s.geom_required_bes = false;
    np_charge_monitor_geom_gate(&s);
    check((s.granted_mask & (1U << NP_SAFETY_CH_BES_IDX)) != 0U,
          "bes geom gate: BES_TACS not blocked when not required");

    /* A tDCS declaration does not open the BES gate, and vice versa.     */
    np_charge_monitor_reset_session(NULL);
    np_charge_monitor_set_channel_area_mcm2(NP_SAFETY_CH_TDCS_IDX, 35000U);
    np_safety_state_t s2 = fresh_state();
    s2.geom_required_tdcs = true;
    s2.geom_required_bes  = true;
    np_charge_monitor_geom_gate(&s2);
    check((s2.granted_mask & (1U << NP_SAFETY_CH_TDCS_IDX)) != 0U &&
          (s2.granted_mask & (1U << NP_SAFETY_CH_BES_IDX)) == 0U,
          "bes geom gate: a tDCS area does not open the BES gate");

    /* Re-armed per session. */
    np_charge_monitor_reset_session(NULL);
    np_charge_monitor_set_channel_area_mcm2(NP_SAFETY_CH_BES_IDX, 25000U);
    np_charge_monitor_reset_session(NULL);
    np_safety_state_t s3 = fresh_state();
    s3.geom_required_bes = true;
    np_charge_monitor_geom_gate(&s3);
    check((s3.granted_mask & (1U << NP_SAFETY_CH_BES_IDX)) == 0U,
          "bes geom gate: re-armed after reset_session (blocks until re-declared)");
}

/* The declared phase duration is clamped, so a malformed or hostile value
 * cannot overflow the per-phase arithmetic. */
static void test_phase_clamped(void)
{
    uint16_t cur[NP_SAFETY_MAX_CHANNELS];

    np_charge_monitor_init();
    np_charge_monitor_set_channel_area_mcm2(NP_SAFETY_CH_BES_IDX, 25000U);
    np_charge_monitor_set_channel_waveform(NP_SAFETY_CH_BES_IDX,
                                           NP_CHARGE_WAVE_PULSE, 0xFFFFFFFFUL);

    np_safety_state_t s = fresh_state();
    one_current(cur, NP_SAFETY_CH_BES_IDX, 65535U);
    np_charge_monitor_phase_tick(&s, cur, NP_SAFETY_MAX_CHANNELS);
    /* Clamped to 2e6 µs: 65535 × 2e6 / 1000 = 131,070,000 nC, far over the
     * 1,000,000 nC ceiling — so it trips, and did not overflow to a pass.   */
    check((s.granted_mask & (1U << NP_SAFETY_CH_BES_IDX)) == 0U,
          "per-phase: an absurd phase_us clamps and still trips (no overflow wrap)");
}

/* ── Main ───────────────────────────────────────────────────────────────────── */

int main(void)
{
    printf("=== np_charge_monitor_tests ===\n");

    test_init_clean();
    test_dc_session_limit_enforced();
    test_two_ma_twenty_minutes_completes();
    test_pulsed_channel_never_accumulates();
    test_phase_ceiling_pulse();
    test_phase_ceiling_sine_vs_square();
    test_phase_tick_ignores_idle_channels();
    test_decl_gate_blocks_undeclared_electrical();
    test_decl_gate_opens_on_declaration();
    test_decl_gate_rearms_each_session();
    test_zero_class_does_not_undeclare();
    test_mixed_class_channel_runs_both_checks();
    test_reset_clears_accumulators();
    test_out_of_range_channel_ignored();
    test_tick_idempotent_after_trip();
    test_charge_accumulation_math();
    test_set_channel_area_hd_tdcs();
    test_set_channel_area_default_pad();
    test_area_override_isolated();
    test_geom_gate_blocks_until_applied();
    test_geom_gate_not_required_passes();
    test_geom_gate_rearms_each_session();
    test_tdcs_geom_gate_blocks_until_applied();
    test_geom_gates_are_independent();
    test_tdcs_geom_gate_rearms_each_session();
    test_bes_geom_gate_blocks_until_applied();
    test_bes_geom_gate_independent_and_rearms();
    test_phase_clamped();

    printf("=== %d failure(s) ===\n", g_failures);
    return (g_failures == 0) ? 0 : 1;
}
