/*
 * NeurOne Safety MCU — SW01-M05: Cervical VNS Cardiac Rhythm Interlock
 * Document: NP-SW-001 Rev 1, NP-FW-CVNS-001 Rev 1, NP-FMEA-001 Rev 1 §SW01-M05
 *
 * Monitors R-peak GPIO pulses from the main processor (RPEAK_IN, PA8).
 * Uses TIM2 at 1MHz to capture RR intervals with 1µs resolution.
 *
 * Cutoff condition: the current 8-interval mean differs by more than
 *   NP_CARDIAC_HR_DELTA_BPM from any 1 s snapshot of it in the last 18 s
 *   (NP-FW-CVNS-001 Rev 10 §5.4, OI-CVNS-12, principal 2026-10-01).  Until
 *   Rev 10 the comparison was against a baseline refreshed unconditionally
 *   every 5 s, which adopted a step part-way through the 8-interval window:
 *   a sustained 70 -> 50 BPM fall was never cut.
 * Cutoff response: CVNS_ENABLE_L/R GPIO cleared within <5.1ms worst-case
 *   (SysTick ISR at 200Hz = 5ms period, TIM6 compare ISR latency <0.1ms).
 *   Specified cutoff time: <100ms (FAI-CV02).
 *
 * FMEA-M05-02 mitigation: HR delta comparison uses int16_t signed arithmetic
 * to prevent underflow when current HR < baseline HR (MISRA C:2012 Rule 10.1).
 *
 * Class C self-sufficiency (NP-RISK-002 OI-RISK2-05, principal 2026-09-25).
 * Two rules keep this interlock from being blind while cervical VNS runs:
 *   - PRE-ARM HOLD: until NP_CARDIAC_BASELINE_BEATS intervals have armed the
 *     baseline, NP_SAFETY_EN_CVNS is withheld.  Before this, CVNS was granted
 *     as soon as requested and the interlock could not fire until armed — so a
 *     session whose R-peaks never arrived ran with no Class C cardiac
 *     monitoring at all.  The hold is silent (no status, no lockout): the hub
 *     reads an absent grant as request latency, not a fault.
 *   - STALENESS: while CVNS is granted, no R-peak edge for
 *     NP_CARDIAC_RPEAK_STALE_MS cuts it exactly as a cardiac event does
 *     (lockout, CARDIAC status, persisted).  Before this, a lost R-peak stream
 *     froze the HR at its last 8-beat mean and only the hub's Class B timer
 *     could stop stimulation.
 * np_cardiac_interlock_arm_reset() is called on each new CVNS request so a
 * baseline and last-edge time left over from an earlier session can neither
 * grant early nor trip at once.
 *
 * Power-cycle persistence (NP-SW-FAULTMSG-001 P1, OI-FAULTMSG-01): a cutoff the
 * app has not yet acknowledged survives a power-on reset.  This module does not
 * touch flash itself — writing stalls the core, so np_safety_main.c does it
 * after the GPIO cutoff has been applied.  Here: a cutoff and a completed
 * re-enable each post a write request (np_cardiac_interlock_nv_request), and at
 * boot np_cardiac_interlock_restore() re-asserts a persisted cutoff.
 */

#include "np_safety_config.h"
#include "np_safety_hal.h"
#include "np_safety_protocol.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* HAL: np_hal_get_tick_ms (ms), np_hal_tim2_get_capture (µs, 1 MHz free-run),
 * np_hal_rpeak_edge_pending / _clear — all declared in np_safety_hal.h.     */

/* ── Module state ─────────────────────────────────────────────────────────── */
#define NP_RR_BUF_SIZE   8U

static uint32_t s_rr_buf[NP_RR_BUF_SIZE];   /* RR intervals in µs */
static uint8_t  s_rr_head;
static uint8_t  s_rr_count;
static uint32_t s_last_capture;              /* TIM2 count at last R-peak */
static uint32_t s_last_edge_ms;              /* SysTick ms at last R-peak (staleness) */
static bool     s_first_beat_seen;           /* guard: skip phantom first RR */

/* Heart-rate history (OI-CVNS-12): one snapshot of the 8-interval mean every
 * NP_CARDIAC_HR_SNAP_MS, the oldest overwritten first.  s_hist_count == 0 means
 * "seed from the current mean on the next armed tick". */
static int16_t  s_hist_bpm[NP_CARDIAC_HR_HIST_LEN];   /* beats per minute, signed */
static uint8_t  s_hist_head;
static uint8_t  s_hist_count;
static uint32_t s_hist_last_ms;
static bool     s_baseline_valid;
static bool     s_lockout_active;
static uint32_t s_lockout_start_ms;         /* stored start (not end) for wrap safety */
static bool     s_cutoff_active;

/* Pending non-volatile write: a new "cutoff awaiting acknowledgement" value
 * that np_safety_main.c has not yet persisted. */
static bool     s_nv_request;
static bool     s_nv_value;

/* A cutoff persisted before the last power-on reset, not yet re-asserted. */
static bool     s_restored_pending;

np_safe_status_t np_cardiac_interlock_init(void)
{
    for (uint8_t i = 0U; i < NP_RR_BUF_SIZE; i++) {
        s_rr_buf[i] = 0U;
    }
    s_rr_head              = 0U;
    s_rr_count             = 0U;
    s_last_capture         = 0U;
    s_last_edge_ms         = 0U;
    s_first_beat_seen      = false;
    for (uint8_t i = 0U; i < NP_CARDIAC_HR_HIST_LEN; i++) {
        s_hist_bpm[i] = 0;
    }
    s_hist_head            = 0U;
    s_hist_count           = 0U;
    s_hist_last_ms         = 0U;
    s_baseline_valid       = false;
    s_lockout_active       = false;
    s_lockout_start_ms     = 0U;
    s_cutoff_active        = false;
    s_nv_request           = false;
    s_nv_value             = false;
    s_restored_pending     = false;
    return NP_SAFE_OK;
}

/*
 * np_cardiac_interlock_arm_reset — called by main on the rising edge of a CVNS
 * request.  Discards the baseline and the R-R buffer, so the new request must
 * re-arm on fresh beats (pre-arm hold).  That is also why an edge time left from
 * an earlier session cannot trip staleness: staleness is evaluated only once
 * armed, and arming takes NP_CARDIAC_BASELINE_BEATS fresh edges, each of which
 * rewrites s_last_edge_ms.  Lockout, cutoff and a restored cutoff are NOT
 * touched: this re-arms monitoring, it never clears a cutoff.
 */
void np_cardiac_interlock_arm_reset(void)
{
    s_baseline_valid  = false;
    s_rr_count        = 0U;
    s_rr_head         = 0U;
    s_first_beat_seen = false;
}

/* Trip: the one cutoff path, shared by the HR-delta and staleness conditions. */
static void cardiac_cutoff(np_safety_state_t *state, uint32_t now_ms)
{
    s_cutoff_active      = true;
    s_lockout_active     = true;
    s_lockout_start_ms   = now_ms;
    state->granted_mask &= (uint16_t)~NP_SAFETY_EN_CVNS;
    state->status       |= NP_SAFETY_STATUS_CARDIAC | NP_SAFETY_STATUS_CUTOFF;
    state->fault_slot    = 10U; /* slot index for CVNS */
    s_nv_request         = true;  /* persist AFTER the */
    s_nv_value           = true;  /* GPIO cutoff (main) */
}

/*
 * np_cardiac_interlock_restore — called once at boot with the persisted value
 * from np_nv_cardiac_pending().
 *
 * A persisted cutoff is held LATENT: nothing changes until a session actually
 * requests the CVNS channel.  At that point np_cardiac_interlock_tick() — which
 * main.c runs only while CVNS is requested — turns it into a live cutoff before
 * the grant reaches the GPIO (see the top of the tick).
 *
 * Why latent rather than re-asserting CARDIAC at boot: the lockout only counts
 * down while CVNS is requested, and a session that never requests CVNS has
 * nothing for a cardiac cutoff to protect.  Latent, a non-cervical session is
 * unaffected, and a cervical one meets exactly the state a live cutoff leaves.
 * (CARDIAC itself withholds only NP_CARDIAC_BLOCK_MASK — see np_spi_watchdog.c
 * — so even a live cutoff no longer stops the other modalities.)
 */
void np_cardiac_interlock_restore(bool cutoff_pending)
{
    s_restored_pending = cutoff_pending;
}

/*
 * np_cardiac_interlock_user_changed — the active user has changed between
 * sessions (principal, 2026-09-22: a cutoff is held for the user who triggered
 * it and nobody else).  The previous user's live cutoff state leaves RAM — it
 * stays persisted for them in np_nv_state and returns if they come back — and
 * the new user's own persisted state is re-armed as latent.  CUTOFF is left for
 * np_spi_watchdog_tick() to clear, since another interlock may hold it.
 */
void np_cardiac_interlock_user_changed(np_safety_state_t *state, bool new_user_blocked)
{
    s_cutoff_active    = false;
    s_lockout_active   = false;
    s_baseline_valid   = false;
    s_rr_count         = 0U;
    s_rr_head          = 0U;
    s_first_beat_seen  = false;
    s_restored_pending = new_user_blocked;
    state->status     &= (uint8_t)~NP_SAFETY_STATUS_CARDIAC;
}

/*
 * np_cardiac_interlock_nv_request — true if a value is waiting to be persisted;
 * writes it to *pending_out.  np_cardiac_interlock_nv_done(written) clears the
 * request once that value has been persisted — but only if it is still the
 * latest one, so a newer request posted in between is never dropped.  A newer
 * request simply replaces an unwritten older one: only the latest value
 * matters.
 */
bool np_cardiac_interlock_nv_request(bool *pending_out)
{
    if (s_nv_request && (pending_out != NULL)) {
        *pending_out = s_nv_value;
    }
    return s_nv_request;
}

void np_cardiac_interlock_nv_done(bool written)
{
    if (s_nv_value == written) {
        s_nv_request = false;
    }
}

/*
 * np_cardiac_interlock_reenable — called by main when hub asserts
 * NP_SESSION_STATUS_CVNS_REENABLE after lockout expires.
 * Clears the CARDIAC status bit, re-arms the interlock, and resets the
 * baseline so fresh beats are required before the next cutoff can fire.
 * The caller must also request a new impedance check for CVNS.
 */
void np_cardiac_interlock_reenable(np_safety_state_t *state)
{
    if (s_lockout_active) {
        return;  /* lockout has not yet expired — deny re-enable */
    }
    state->status    &= (uint8_t)~NP_SAFETY_STATUS_CARDIAC;
    state->status    &= (uint8_t)~NP_SAFETY_STATUS_CUTOFF;
    s_cutoff_active   = false;
    s_baseline_valid  = false;  /* force fresh baseline accumulation */
    s_rr_count        = 0U;
    s_rr_head         = 0U;
    s_first_beat_seen = false;
    s_nv_request      = true;   /* persist: acknowledged */
    s_nv_value        = false;
}

static int16_t rr_to_bpm(uint32_t rr_us)
{
    if (rr_us == 0U) {
        return 0;
    }
    /* BPM = 60,000,000 / RR_µs.
     * Clamp to INT16_MAX before cast: values < 1831µs (>32767 BPM) would
     * overflow int16_t.  Such RR intervals are physiologically impossible in
     * normal sinus rhythm but can arise from noise/motion artifacts on the
     * RPEAK_IN line.  Clamping prevents undefined-behavior truncation and
     * a resulting phantom cardiac cutoff. (MISRA C:2012 Rule 10.4) */
    uint32_t bpm_u32 = 60000000UL / rr_us;
    if (bpm_u32 > (uint32_t)INT16_MAX) {
        return INT16_MAX;   /* saturate; interlock compares delta, not abs value */
    }
    return (int16_t)bpm_u32;
}

static int16_t current_hr_bpm(void)
{
    if (s_rr_count == 0U) {
        return 0;
    }
    /* Mean of last NP_RR_BUF_SIZE intervals (ring buffer, order-independent) */
    uint32_t sum = 0U;
    uint8_t  n   = (s_rr_count < NP_RR_BUF_SIZE) ? s_rr_count : NP_RR_BUF_SIZE;
    for (uint8_t i = 0U; i < n; i++) {
        sum += s_rr_buf[i];
    }
    return rr_to_bpm(sum / (uint32_t)n);
}

/* History: restart from one snapshot (arming, re-arming after a lockout). */
static void hist_seed(int16_t bpm, uint32_t now_ms)
{
    s_hist_bpm[0]  = bpm;
    s_hist_head    = (uint8_t)(1U % NP_CARDIAC_HR_HIST_LEN);
    s_hist_count   = 1U;
    s_hist_last_ms = now_ms;
}

static void hist_push(int16_t bpm, uint32_t now_ms)
{
    s_hist_bpm[s_hist_head] = bpm;
    s_hist_head = (uint8_t)((s_hist_head + 1U) % NP_CARDIAC_HR_HIST_LEN);
    if (s_hist_count < NP_CARDIAC_HR_HIST_LEN) {
        s_hist_count++;
    }
    s_hist_last_ms = now_ms;
}

/* Largest |cur − snapshot| over the history (order-independent).  Signed
 * arithmetic, so a fall compares by magnitude (FMEA-M05-02).  Every mean is in
 * [0, INT16_MAX] (rr_to_bpm saturates), so each difference fits in int32_t and
 * its magnitude in uint16_t. */
static uint16_t hist_max_delta(int16_t cur_bpm)
{
    uint16_t max_delta = 0U;
    for (uint8_t i = 0U; i < s_hist_count; i++) {
        int32_t d = (int32_t)cur_bpm - (int32_t)s_hist_bpm[i];
        if (d < 0) {
            d = -d;
        }
        if ((uint16_t)d > max_delta) {
            max_delta = (uint16_t)d;
        }
    }
    return max_delta;
}

void np_cardiac_interlock_tick(np_safety_state_t *state)
{
    uint32_t now_ms = np_hal_get_tick_ms();

    /* A cutoff persisted across a power cycle meets its first CVNS request:
     * re-assert it exactly as a live cutoff leaves the state, before this
     * iteration's grant reaches the GPIO.  Clearing it is the ordinary
     * re-enable path (hub lockout + app confirmation + impedance, REQ-CVNS-09).
     * The lockout restarts now: the time since the cutoff is unknown (no RTC,
     * no battery), so it is treated as having only just happened.  No write is
     * posted — the persisted value is already "pending". */
    if (s_restored_pending && state->cvns_active) {
        s_restored_pending  = false;
        s_cutoff_active     = true;
        s_lockout_active    = true;
        s_lockout_start_ms  = now_ms;
        state->granted_mask &= (uint16_t)~NP_SAFETY_EN_CVNS;
        state->status       |= NP_SAFETY_STATUS_CARDIAC | NP_SAFETY_STATUS_CUTOFF;
        state->fault_slot    = 10U; /* slot index for CVNS */
        return;
    }

    /* Clear lockout if elapsed — use elapsed subtraction for wrap safety at ~49 days.
     * The history is discarded with it: no snapshot was taken during the
     * lockout, so its entries are the pre-event rates, and comparing the
     * post-event rate with them would re-trip at once and restart the lockout. */
    if (s_lockout_active && ((now_ms - s_lockout_start_ms) >= NP_CARDIAC_LOCKOUT_MS)) {
        s_lockout_active = false;
        s_cutoff_active  = false;  /* re-arm interlock for subsequent events */
        s_hist_count     = 0U;
    }

    /* Capture R-peak if signalled by main processor */
    if (np_hal_rpeak_edge_pending()) {
        uint32_t capture = np_hal_tim2_get_capture();
        np_hal_rpeak_edge_clear();

        if (s_first_beat_seen) {
            /* Skip first beat: s_last_capture=0 → phantom interval = boot time */
            uint32_t rr_us = capture - s_last_capture;

            s_rr_buf[s_rr_head] = rr_us;
            s_rr_head = (s_rr_head + 1U) % NP_RR_BUF_SIZE;
            if (s_rr_count < NP_RR_BUF_SIZE) {
                s_rr_count++;
            }
        }
        s_first_beat_seen = true;
        s_last_capture    = capture;
        s_last_edge_ms    = now_ms;

        /* Establish baseline after NP_CARDIAC_BASELINE_BEATS valid intervals:
         * the arming mean is the first history snapshot. */
        if (!s_baseline_valid && s_rr_count >= NP_CARDIAC_BASELINE_BEATS) {
            hist_seed(current_hr_bpm(), now_ms);
            s_baseline_valid = true;
        }
    }

    if (!state->cvns_active || s_lockout_active) {
        return;
    }

    /* PRE-ARM HOLD (OI-RISK2-05): no Class C baseline, no cervical grant. */
    if (!s_baseline_valid) {
        state->granted_mask &= (uint16_t)~NP_SAFETY_EN_CVNS;
        return;
    }

    /* STALENESS (OI-RISK2-05): only while CVNS is actually granted — under a
     * CARDIAC cutoff it is already withheld, so lockout expiry cannot re-trip. */
    if (!s_cutoff_active &&
        (state->granted_mask & NP_SAFETY_EN_CVNS) != 0U &&
        (now_ms - s_last_edge_ms) >= NP_CARDIAC_RPEAK_STALE_MS) {
        cardiac_cutoff(state, now_ms);
        return;
    }

    /* Compute current HR once; reuse below for both the snapshot and the
     * comparison.  Computing twice risks a different result if an R-peak edge
     * arrives between the two calls (ring buffer updates mid-tick). */
    int16_t cur_bpm = current_hr_bpm();

    /* Snapshot the current mean every NP_CARDIAC_HR_SNAP_MS.  No snapshot is
     * taken under a cutoff: the lockout returns above, and its expiry empties
     * the history, so the first armed tick after it reseeds here. */
    if (s_hist_count == 0U) {
        hist_seed(cur_bpm, now_ms);
    } else if ((now_ms - s_hist_last_ms) >= NP_CARDIAC_HR_SNAP_MS) {
        hist_push(cur_bpm, now_ms);
    }

    /* Compare the current HR with every snapshot in the horizon (OI-CVNS-12).
     * There is no baseline refresh to race: a step stays visible against the
     * last pre-change snapshot until that snapshot is 18 s old, which outlasts
     * a change spread over 5 s plus the 8-interval transit at any rate down to
     * 40 BPM (derivation at NP_CARDIAC_HR_HIST_LEN).  Signed, so a fall
     * compares by magnitude (FMEA-M05-02). */
    uint16_t delta_bpm = hist_max_delta(cur_bpm);

    if (delta_bpm > NP_CARDIAC_HR_DELTA_BPM && !s_cutoff_active) {
        cardiac_cutoff(state, now_ms);
    }
}
