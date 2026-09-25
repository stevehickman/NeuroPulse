/*
 * NeurOne Safety MCU — SW01-M02: SPI Heartbeat Watchdog
 * Document: NP-SW-001 Rev 1, NP-FMEA-001 Rev 1 §SW01-M02
 *
 * Monitors heartbeat frames from the i.MX RT1062 main processor.
 * Expected period: 200ms.  Watchdog timeout: 1500ms.
 * On timeout: clears granted_mask → np_gpio_mgr_apply() cuts all stimulation
 * within one main-loop iteration (< 1ms; spec: < 50ms).
 */

#include "np_safety_config.h"
#include "np_safety_hal.h"
#include "np_safety_protocol.h"
#include <stdint.h>
#include <stdbool.h>

/* HAL: np_hal_get_tick_ms — SysTick-backed millisecond counter, declared in
 * np_safety_hal.h.  The 1500 ms NP_SAFETY_WDG_TIMEOUT_MS below is in the same
 * unit by contract, not by coincidence.                                     */

/* HAL: np_hal_tim2_now_us — TIM2's free-running 1 MHz count, the independent
 * timebase the tick-liveness check compares SysTick against (OI-FMEA-12 (b)). */

/* ── Module state ────────────────────────────────────────────────────────── */
static uint32_t s_last_beat_ms  = 0U;
static bool     s_watchdog_fired = false;

/* Sequence gate (OI-FMEA-12 (a)): counter on the last well-formed heartbeat,
 * and how many consecutive forward steps end at it. */
static bool     s_seq_seen = false;
static uint8_t  s_seq_last = 0U;
static uint8_t  s_seq_run  = 0U;

/* Tick liveness (OI-FMEA-12 (b)): both counters at the last comparison. */
static uint32_t s_live_ref_ms = 0U;
static uint32_t s_live_ref_us = 0U;
static bool     s_tick_fault  = false;

np_safe_status_t np_spi_watchdog_init(void)
{
    s_last_beat_ms  = np_hal_get_tick_ms();
    s_watchdog_fired = false;
    s_seq_seen = false;
    s_seq_last = 0U;
    s_seq_run  = 0U;
    s_live_ref_ms = s_last_beat_ms;
    s_live_ref_us = np_hal_tim2_now_us();
    s_tick_fault  = false;
    return NP_SAFE_OK;
}

/*
 * np_spi_watchdog_seq_accept — called for every heartbeat that passed magic
 * and both checksums, with the 3-bit counter from its session_status byte.
 * Returns true if the frame is live evidence of the hub: only then may the
 * caller act on the frame's fields and call np_spi_watchdog_tick().
 *
 * NP-FMEA-001 FMEA-M02-03, OI-FMEA-12 (a).  A repeated counter (a stuck
 * buffer) or a backward step restarts the run; a forward step of
 * 1..NP_SAFETY_SEQ_MAX_STEP (up to two lost frames) extends it.  A frame is
 * accepted once the run reaches NP_SAFETY_SEQ_RUN_MIN, which a two-buffer
 * replay cannot build.  A replayed ring of three or more buffers with advancing
 * counters is NOT distinguishable from a live hub by the counter alone.
 */
bool np_spi_watchdog_seq_accept(uint8_t seq)
{
    uint8_t step;

    seq &= (uint8_t)(NP_HEARTBEAT_SEQ_MODULUS - 1U);
    step = (uint8_t)((uint8_t)(seq - s_seq_last) &
                     (uint8_t)(NP_HEARTBEAT_SEQ_MODULUS - 1U));

    if (s_seq_seen && (step >= 1U) && (step <= NP_SAFETY_SEQ_MAX_STEP)) {
        if (s_seq_run < NP_SAFETY_SEQ_RUN_MIN) {
            s_seq_run++;
        }
    } else {
        s_seq_run = 0U;
    }
    s_seq_seen = true;
    s_seq_last = seq;

    return s_seq_run >= NP_SAFETY_SEQ_RUN_MIN;
}

/*
 * np_spi_watchdog_tick — called from main loop when a valid heartbeat frame
 * is received.  Resets the watchdog timer and evaluates the requested enable
 * mask against current interlock state to produce granted_mask.
 *
 * If no valid frame is received, the caller skips this tick; the tick-
 * independent watchdog check in np_spi_watchdog_check() fires on timeout.
 */
void np_spi_watchdog_tick(np_safety_state_t         *state,
                          const np_safety_rx_ext_frame_t *rx,
                          np_safety_tx_frame_t           *tx)
{
    (void)rx;
    (void)tx;

    s_last_beat_ms   = np_hal_get_tick_ms();
    s_watchdog_fired = false;

    /* Clear watchdog status bit on successful receipt */
    state->status &= (uint8_t)~NP_SAFETY_STATUS_WATCHDOG;

    /* Evaluate active faults once into a local so the two guards below see an
     * identical snapshot.  Evaluating the expression twice with no critical
     * section would be a race if any interlock ever runs from an ISR context:
     * a fault asserted between the two evaluations would clear CUTOFF while
     * simultaneously granting the full requested mask. (MISRA C:2012 R.13.5)
     *
     * NP_SAFETY_STATUS_SIG_PENDING: set on session start, cleared when hub
     * delivers a valid session descriptor signature via np_safety_sig_cmd_t.
     * Blocks grant_mask until the signature is verified; not a CUTOFF fault. */
    uint8_t active_faults = state->status & (NP_SAFETY_STATUS_FAULT       |
                                              NP_SAFETY_STATUS_THERMAL      |
                                              NP_SAFETY_STATUS_CHARGE       |
                                              NP_SAFETY_STATUS_SIG_PENDING);
    /* NP_SAFETY_STATUS_CARDIAC is NOT an all-channel fault: it withholds only
     * NP_CARDIAC_BLOCK_MASK (cervical VNS), below.  It still keeps CUTOFF set,
     * because the cervical channel is cut. */
    bool cardiac = (state->status & NP_SAFETY_STATUS_CARDIAC) != 0U;
    /* NP_SAFETY_STATUS_WATCHDOG intentionally excluded: the WATCHDOG bit is
     * cleared above in this function on each valid heartbeat; including it in
     * the active_faults mask would block the grant mask during the same tick
     * cycle that clears it. */

    /* Only clear CUTOFF if no interlock is actively asserting it */
    if ((active_faults == 0U) && !cardiac) {
        state->status &= (uint8_t)~NP_SAFETY_STATUS_CUTOFF;
    }

    /* Grant requested channels if no all-channel fault is active; a cardiac
     * cutoff then withholds only the channels it exists for. */
    if (active_faults == 0U) {
        state->granted_mask = state->requested_mask & NP_SAFETY_EN_ALL_MASK;
        if (cardiac) {
            state->granted_mask &= (uint16_t)~NP_CARDIAC_BLOCK_MASK;
        }
    } else {
        state->granted_mask = 0U;
    }
}

/*
 * np_spi_watchdog_check — called every main-loop iteration regardless of SPI
 * frame availability.  Fires cutoff if timeout elapsed.
 * FMEA-M02-01 mitigation: timeout comparison uses unsigned arithmetic to
 * prevent wrap-around false negative (MISRA C:2012 Rule 10.1).
 */
void np_spi_watchdog_check(np_safety_state_t *state)
{
    uint32_t now     = np_hal_get_tick_ms();
    uint32_t elapsed = now - s_last_beat_ms;  /* unsigned wrap-around safe */

    if (elapsed >= NP_SAFETY_WDG_TIMEOUT_MS && !s_watchdog_fired) {
        s_watchdog_fired     = true;
        state->granted_mask  = 0U;
        state->status       |= NP_SAFETY_STATUS_WATCHDOG | NP_SAFETY_STATUS_CUTOFF;
    }
}

/*
 * np_spi_watchdog_tick_liveness — called every main-loop iteration.  Compares
 * the SysTick millisecond count, which times the heartbeat watchdog, the
 * cardiac lockout and the R-peak staleness cutoff, against TIM2's independent
 * 1 MHz count.
 *
 * NP-FMEA-001 FMEA-M02-02, OI-FMEA-12 (b).  A stopped or slowed SysTick
 * freezes np_hal_get_tick_ms(), so the heartbeat watchdog can never elapse,
 * while the main loop keeps running and keeps refreshing the IWDG.  Once
 * either counter has advanced NP_SAFETY_TICK_CHECK_MS, the two elapsed times
 * must agree within NP_SAFETY_TICK_TOL_MS.  Otherwise an all-channel FAULT is
 * latched.  The latch is held HERE and re-asserted on every call, not left to
 * the status byte, because another module may later overwrite fault_slot and
 * clear FAULT (np_session_sig_reenable() does so for a SIG_FAIL slot).  Only a
 * reset clears it.  A stopped TIM2 with a live SysTick trips the same check.
 */
void np_spi_watchdog_tick_liveness(np_safety_state_t *state)
{
    uint32_t now_ms = np_hal_get_tick_ms();
    uint32_t now_us = np_hal_tim2_now_us();
    uint32_t d_ms   = now_ms - s_live_ref_ms;      /* unsigned wrap-around safe */
    uint32_t d_us   = now_us - s_live_ref_us;
    uint32_t us_ms  = d_us / 1000U;
    uint32_t diff;

    if (s_tick_fault) {
        state->granted_mask = 0U;
        state->status      |= NP_SAFETY_STATUS_FAULT | NP_SAFETY_STATUS_CUTOFF;
        state->fault_slot   = NP_FAULT_SLOT_TICK;
        return;
    }

    if ((d_ms < NP_SAFETY_TICK_CHECK_MS) && (us_ms < NP_SAFETY_TICK_CHECK_MS)) {
        return;
    }

    diff = (d_ms > us_ms) ? (d_ms - us_ms) : (us_ms - d_ms);
    if (diff > NP_SAFETY_TICK_TOL_MS) {
        s_tick_fault        = true;
        state->granted_mask = 0U;
        state->status      |= NP_SAFETY_STATUS_FAULT | NP_SAFETY_STATUS_CUTOFF;
        state->fault_slot   = NP_FAULT_SLOT_TICK;
    }

    s_live_ref_ms = now_ms;
    s_live_ref_us = now_us;
}
