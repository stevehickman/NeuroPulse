/*
 * NeurOne Hub Control Program — Cervical VNS Re-Enable Manager (OI-CVNS-HUB-01)
 * Document: NP-FW-HUB-001 Rev 1 §7a, NP-FW-CVNS-001 Rev 1
 *
 * See np_cvns_reenable.h for the full state-machine contract.
 *
 * Design invariants:
 *   - Exactly ONE state (ASSERTING) makes np_cvns_reenable_bit_active() true,
 *     and exactly ONE edge (impedance-pass) enters it.
 *   - Every anomaly — impedance fail/timeout, MCU not clearing CARDIAC,
 *     session leaving RUNNING, CARDIAC clearing externally — exits toward a
 *     deasserted state.  Fail-closed throughout.
 *   - All elapsed-time math is unsigned subtraction (wrap-safe).
 *   - No FreeRTOS, no dynamic allocation, no floating point.
 *
 * Concurrency (SINGLE-WRITER design):
 *   np_cvns_reenable_confirm() is called from the app-transport task; every
 *   OTHER function that mutates state runs in the higher-priority heartbeat
 *   task, which can preempt the transport task at any instruction.  To make
 *   the confirm path race-free WITHOUT a lock (and without a FreeRTOS
 *   dependency in this pure module), confirm() does NOT transition state or
 *   call the impedance HAL.  It only posts a one-shot intent flag
 *   (s_confirm_pending, a single aligned word — atomic on Cortex-M/M0+).
 *   The heartbeat task is the ONLY mutator of s_state: it consumes the pending
 *   flag from AWAIT_CONFIRM, re-validating the gate at consume time, so a
 *   confirmation can never attach to a cutoff whose 30s lockout (gate 1) has
 *   not elapsed or whose sequence was restarted by a fresh cutoff.  The flag
 *   is cleared on every reset and on every new cutoff rising edge, so a stale
 *   confirmation is discarded rather than carried into a later cycle.
 */

#include "np_cvns_reenable.h"

/* ── Module state (mutated only by the heartbeat task, except s_confirm_pending) */

static np_cvns_reenable_state_t s_state;
static bool     s_prev_cardiac;        /* CARDIAC bit on the previous beat      */
static uint32_t s_cutoff_observed_ms;  /* tick when hub first saw CARDIAC set   */
static uint32_t s_impedance_start_ms;  /* tick when hub-side check was started  */
static uint32_t s_assert_start_ms;     /* tick when ASSERTING was entered       */

/* Written true by confirm() (transport task), consumed/cleared by the heartbeat
 * task.  Single producer, single consumer, single word → lock-free safe.       */
static volatile bool s_confirm_pending;

/* ── Internal helpers ───────────────────────────────────────────────────────── */

static void reset_machine(void)
{
    s_state               = NP_CVNS_RN_IDLE;
    s_prev_cardiac        = false;
    s_cutoff_observed_ms  = 0U;
    s_impedance_start_ms  = 0U;
    s_assert_start_ms     = 0U;
    s_confirm_pending     = false;  /* discard any un-consumed confirmation */
}

/* ── Public API ─────────────────────────────────────────────────────────────── */

void np_cvns_reenable_init(void)
{
    reset_machine();
}

void np_cvns_reenable_session_reset(void)
{
    reset_machine();
}

np_cvns_reenable_event_t np_cvns_reenable_on_heartbeat(uint8_t  mcu_status,
                                                        bool     session_running,
                                                        uint32_t now_ms)
{
    bool cardiac = (mcu_status & NP_SAFETY_STATUS_CARDIAC) != 0U;

    /* Fail-safe: no re-enable flow survives a session boundary.  Any state
     * other than RUNNING (idle, paused, stopping, complete, fault) resets the
     * machine and deasserts the bit. */
    if (!session_running) {
        reset_machine();
        return NP_CVNS_RN_EV_NONE;
    }

    /* Rising-edge detection: a NEW cardiac cutoff from ANY state restarts the
     * full three-gate sequence.  Any stale confirmation or in-flight impedance
     * result from a previous cutoff is discarded. */
    bool rising    = cardiac && !s_prev_cardiac;
    s_prev_cardiac = cardiac;

    if (rising) {
        s_state              = NP_CVNS_RN_LOCKOUT;
        s_cutoff_observed_ms = now_ms;
        s_confirm_pending    = false;  /* gate 2 must be re-given for THIS cutoff */
        return NP_CVNS_RN_EV_CUTOFF_DETECTED;
    }

    switch (s_state) {

    case NP_CVNS_RN_IDLE:
        return NP_CVNS_RN_EV_NONE;

    case NP_CVNS_RN_LOCKOUT:
        if (!cardiac) {
            /* CARDIAC cleared without hub action (MCU reset / external clear):
             * nothing left to re-enable. */
            reset_machine();
            return NP_CVNS_RN_EV_CARDIAC_CLEARED;
        }
        /* Gate 1: 30s since the hub observed the cutoff notification.
         * Unsigned subtraction — wrap-safe at ~49.7 days. */
        if ((now_ms - s_cutoff_observed_ms) >= NP_CVNS_REENABLE_LOCKOUT_MS) {
            s_state = NP_CVNS_RN_AWAIT_CONFIRM;
            return NP_CVNS_RN_EV_CONFIRM_OPEN;
        }
        return NP_CVNS_RN_EV_NONE;

    case NP_CVNS_RN_AWAIT_CONFIRM:
        if (!cardiac) {
            reset_machine();
            return NP_CVNS_RN_EV_CARDIAC_CLEARED;
        }
        /* Gate 2: consume an explicit app confirmation posted via
         * np_cvns_reenable_confirm().  Consuming (and re-validating) here — in
         * the single-writer heartbeat context — is what makes the confirm path
         * race-free: the flag can only be acted on while genuinely in
         * AWAIT_CONFIRM for the current cutoff.  Starting gate 3 (impedance)
         * here also keeps the potentially-blocking HAL call out of the
         * transport task. */
        if (s_confirm_pending) {
            s_confirm_pending = false;   /* one-shot: consume the intent */
            if (np_cvns_hal_impedance_start() != NP_HUB_OK) {
                /* HAL could not start — fail-closed, stay in AWAIT_CONFIRM so
                 * the operator may re-confirm.  Bit stays inactive. */
                return NP_CVNS_RN_EV_IMPEDANCE_FAILED;
            }
            s_impedance_start_ms = now_ms;
            s_state              = NP_CVNS_RN_IMPEDANCE;
        }
        return NP_CVNS_RN_EV_NONE;

    case NP_CVNS_RN_IMPEDANCE: {
        if (!cardiac) {
            reset_machine();
            return NP_CVNS_RN_EV_CARDIAC_CLEARED;
        }
        /* Gate 3: fresh hub-side impedance check must PASS. */
        bool passed = false;
        if (np_cvns_hal_impedance_poll(&passed)) {
            if (passed) {
                s_state           = NP_CVNS_RN_ASSERTING;
                s_assert_start_ms = now_ms;
                return NP_CVNS_RN_EV_IMPEDANCE_PASSED;
            }
            /* Fail-closed: back to AWAIT_CONFIRM; a NEW confirmation (and a
             * NEW impedance check) is required for another attempt. */
            s_state = NP_CVNS_RN_AWAIT_CONFIRM;
            return NP_CVNS_RN_EV_IMPEDANCE_FAILED;
        }
        if ((now_ms - s_impedance_start_ms) >= NP_CVNS_REENABLE_IMP_TIMEOUT_MS) {
            s_state = NP_CVNS_RN_AWAIT_CONFIRM;
            return NP_CVNS_RN_EV_IMPEDANCE_TIMEOUT;
        }
        return NP_CVNS_RN_EV_NONE;
    }

    case NP_CVNS_RN_ASSERTING:
        if (!cardiac) {
            /* Safety MCU processed the re-enable: CARDIAC cleared.  Done. */
            reset_machine();
            return NP_CVNS_RN_EV_REENABLE_COMPLETE;
        }
        /* Bounded assertion: if the MCU does not clear CARDIAC (e.g. its own
         * lockout still active due to clock skew, or MCU unresponsive), stop
         * asserting and require a fresh confirmation + impedance pass. */
        if ((now_ms - s_assert_start_ms) >= NP_CVNS_REENABLE_ASSERT_TIMEOUT_MS) {
            s_state = NP_CVNS_RN_AWAIT_CONFIRM;
            return NP_CVNS_RN_EV_ASSERT_TIMEOUT;
        }
        return NP_CVNS_RN_EV_NONE;

    default:
        /* Defensive: unreachable state value — fail-closed. */
        reset_machine();
        return NP_CVNS_RN_EV_NONE;
    }
}

np_hub_status_t np_cvns_reenable_confirm(void)
{
    /* Single-writer contract: confirm() NEVER transitions state or calls the
     * HAL — it only posts a one-shot intent that the heartbeat task consumes
     * from AWAIT_CONFIRM (see np_cvns_reenable_on_heartbeat).  This is what
     * makes the confirm path race-free against heartbeat preemption without a
     * lock.
     *
     * Gate 2: accepted only when a cutoff is pending AND the 30s lockout has
     * elapsed (AWAIT_CONFIRM).  A confirmation during LOCKOUT is a stale UI
     * intent and is REJECTED, never queued.  A confirmation in IMPEDANCE or
     * ASSERTING is a duplicate and is rejected too.
     *
     * The s_state read is a single-word atomic load.  It is a best-effort
     * gate for immediate app feedback; the AUTHORITATIVE re-validation happens
     * when the heartbeat task consumes s_confirm_pending (it re-checks that it
     * is genuinely in AWAIT_CONFIRM for the current cutoff), so a state change
     * racing this read can never cause a spurious re-enable. */
    if (s_state != NP_CVNS_RN_AWAIT_CONFIRM) {
        return NP_HUB_ERR_SAFETY_REJECTED;
    }

    s_confirm_pending = true;
    return NP_HUB_OK;
}

bool np_cvns_reenable_bit_active(void)
{
    return s_state == NP_CVNS_RN_ASSERTING;
}

np_cvns_reenable_state_t np_cvns_reenable_get_state(void)
{
    return s_state;
}
