/*
 * NeurOne Safety MCU — SW01-M05: Cervical VNS Cardiac Rhythm Interlock
 * Document: NP-SW-001 Rev A, NP-FW-CVNS-001 Rev A, NP-FMEA-001 Rev A §SW01-M05
 *
 * Monitors R-peak GPIO pulses from the main processor (RPEAK_IN, PA8).
 * Uses TIM2 at 1MHz to capture RR intervals with 1µs resolution.
 *
 * Cutoff condition: HR change > NP_CARDIAC_HR_DELTA_BPM within a 5s window.
 * Cutoff response: CVNS_ENABLE_L/R GPIO cleared within <5.1ms worst-case
 *   (SysTick ISR at 200Hz = 5ms period, TIM6 compare ISR latency <0.1ms).
 *   Specified cutoff time: <100ms (FAI-CV02).
 *
 * FMEA-M05-02 mitigation: HR delta comparison uses int16_t signed arithmetic
 * to prevent underflow when current HR < baseline HR (MISRA C:2012 Rule 10.1).
 */

#include "np_safety_config.h"
#include "np_safety_protocol.h"
#include <stdint.h>
#include <stdbool.h>

/* ── HAL stubs ───────────────────────────────────────────────────────────── */
extern uint32_t np_hal_get_tick_ms(void);
extern uint32_t np_hal_tim2_get_capture(void);    /* last captured TIM2 count */
extern bool     np_hal_rpeak_edge_pending(void);  /* R-peak edge detected */
extern void     np_hal_rpeak_edge_clear(void);

/* ── Module state ─────────────────────────────────────────────────────────── */
#define NP_RR_BUF_SIZE   8U

static uint32_t s_rr_buf[NP_RR_BUF_SIZE];   /* RR intervals in µs */
static uint8_t  s_rr_head;
static uint8_t  s_rr_count;
static uint32_t s_last_capture;              /* TIM2 count at last R-peak */
static bool     s_first_beat_seen;           /* guard: skip phantom first RR */

static int16_t  s_baseline_bpm;             /* beats per minute, signed */
static uint32_t s_baseline_established_ms;
static bool     s_baseline_valid;
static bool     s_lockout_active;
static uint32_t s_lockout_start_ms;         /* stored start (not end) for wrap safety */
static bool     s_cutoff_active;

np_safe_status_t np_cardiac_interlock_init(void)
{
    for (uint8_t i = 0U; i < NP_RR_BUF_SIZE; i++) {
        s_rr_buf[i] = 0U;
    }
    s_rr_head              = 0U;
    s_rr_count             = 0U;
    s_last_capture         = 0U;
    s_first_beat_seen      = false;
    s_baseline_bpm         = 0;
    s_baseline_valid       = false;
    s_lockout_active       = false;
    s_lockout_start_ms     = 0U;
    s_cutoff_active        = false;
    return NP_SAFE_OK;
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

void np_cardiac_interlock_tick(np_safety_state_t *state)
{
    uint32_t now_ms = np_hal_get_tick_ms();

    /* Clear lockout if elapsed — use elapsed subtraction for wrap safety at ~49 days */
    if (s_lockout_active && ((now_ms - s_lockout_start_ms) >= NP_CARDIAC_LOCKOUT_MS)) {
        s_lockout_active = false;
        s_cutoff_active  = false;  /* re-arm interlock for subsequent events */
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

        /* Establish baseline after NP_CARDIAC_BASELINE_BEATS valid intervals */
        if (!s_baseline_valid && s_rr_count >= NP_CARDIAC_BASELINE_BEATS) {
            s_baseline_bpm            = current_hr_bpm();
            s_baseline_valid          = true;
            s_baseline_established_ms = now_ms;
        }
    }

    if (!s_baseline_valid || !state->cvns_active || s_lockout_active) {
        return;
    }

    /* Compute current HR once; reuse below for both baseline refresh and delta
     * comparison.  Computing twice risks a different result if an R-peak edge
     * arrives between the two calls (ring buffer updates mid-tick). */
    int16_t cur_bpm = current_hr_bpm();

    /* Refresh rolling baseline every observation window, but ONLY when no cutoff
     * is active.  Refreshing after a cutoff event would adopt the elevated
     * post-event HR as the new resting baseline, desensitising the interlock
     * for subsequent events.  Gate: s_cutoff_active = true during and after
     * the event until explicit reenable clears it. */
    if (!s_cutoff_active &&
        (now_ms - s_baseline_established_ms) >= NP_CARDIAC_OBS_MS &&
        s_rr_count >= NP_CARDIAC_BASELINE_BEATS) {
        s_baseline_bpm            = cur_bpm;
        s_baseline_established_ms = now_ms;
    }

    /* Compare current HR to baseline — int16_t prevents underflow (FMEA-M05-02) */
    int16_t delta_bpm = (int16_t)(cur_bpm - s_baseline_bpm);
    if (delta_bpm < 0) {
        delta_bpm = (int16_t)-delta_bpm;
    }

    if ((uint16_t)delta_bpm > NP_CARDIAC_HR_DELTA_BPM && !s_cutoff_active) {
        s_cutoff_active                           = true;
        s_lockout_active                          = true;
        s_lockout_start_ms                        = now_ms;
        state->granted_mask                      &= (uint16_t)~NP_SAFETY_EN_CVNS;
        state->status                            |= NP_SAFETY_STATUS_CARDIAC |
                                                    NP_SAFETY_STATUS_CUTOFF;
        state->fault_slot                         = 10U; /* slot index for CVNS */
    }
}
