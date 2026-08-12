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

/* ── Module state ────────────────────────────────────────────────────────── */
static uint32_t s_last_beat_ms  = 0U;
static bool     s_watchdog_fired = false;

np_safe_status_t np_spi_watchdog_init(void)
{
    s_last_beat_ms  = np_hal_get_tick_ms();
    s_watchdog_fired = false;
    return NP_SAFE_OK;
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
                                              NP_SAFETY_STATUS_CARDIAC      |
                                              NP_SAFETY_STATUS_SIG_PENDING);
    /* NP_SAFETY_STATUS_WATCHDOG intentionally excluded: the WATCHDOG bit is
     * cleared above in this function on each valid heartbeat; including it in
     * the active_faults mask would block the grant mask during the same tick
     * cycle that clears it. */

    /* Only clear CUTOFF if no other interlock is actively asserting it */
    if (active_faults == 0U) {
        state->status &= (uint8_t)~NP_SAFETY_STATUS_CUTOFF;
    }

    /* Grant requested channels if no fault conditions are active */
    if (active_faults == 0U) {
        state->granted_mask = state->requested_mask & NP_SAFETY_EN_ALL_MASK;
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
