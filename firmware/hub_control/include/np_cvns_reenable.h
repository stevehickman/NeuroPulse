/*
 * NeurOne Hub Control Program — Cervical VNS Re-Enable Manager (OI-CVNS-HUB-01)
 * Document: NP-FW-HUB-001 Rev 1 §7a, NP-FW-CVNS-001 Rev 1
 *
 * Decides when the hub may assert NP_SESSION_STATUS_CVNS_REENABLE (heartbeat
 * session_status bit 1) after the safety MCU's cardiac interlock has cut
 * cervical VNS stimulation (NP_SAFETY_STATUS_CARDIAC latched in the heartbeat
 * reply).
 *
 * The bit is asserted ONLY when ALL THREE conditions hold, in order:
 *
 *   (1) NP_CARDIAC_LOCKOUT_MS (30s) elapsed since the hub OBSERVED the
 *       cardiac cutoff (first heartbeat reply with CARDIAC set).  Because the
 *       hub observation lags the MCU cutoff by at most one heartbeat period
 *       (200ms), the hub's 30s window always fully contains the safety MCU's
 *       own NP_CARDIAC_LOCKOUT_MS window — the MCU can never see the bit while
 *       its lockout is still active.
 *
 *   (2) Explicit operator/user confirmation received via the app
 *       (np_cvns_reenable_confirm()).  Confirmations are single-use, are only
 *       accepted AFTER the lockout has elapsed (a stale pre-lockout tap is
 *       rejected, never queued), and are consumed by the impedance step.
 *
 *   (3) A NEW hub-side impedance check was requested (started by the confirm
 *       call) and PASSED.  Fail, garbage, or timeout all return the machine
 *       to AWAIT_CONFIRM — fail-closed.
 *
 * State machine (single bit-emitting state; every anomaly exits it):
 *
 *   IDLE ──CARDIAC rising edge──▶ LOCKOUT ──30s elapsed──▶ AWAIT_CONFIRM
 *     ▲                                                        │ confirm()
 *     │                                                        ▼
 *     │◀──CARDIAC cleared / session not running── IMPEDANCE (HAL running)
 *     │                          pass │      fail/timeout └──▶ AWAIT_CONFIRM
 *     │                               ▼
 *     └──CARDIAC cleared (success)── ASSERTING ──2s, CARDIAC still set──▶
 *                                                          AWAIT_CONFIRM
 *
 * np_cvns_reenable_bit_active() returns true only in ASSERTING.  The heartbeat
 * task mirrors that into np_safety_spi_set_cvns_reenable() each beat.
 *
 * Pure C, no FreeRTOS dependency — host-testable (np_cvns_reenable_tests).
 * All elapsed-time computations use unsigned subtraction (wrap-safe at ~49.7
 * days of tick time).
 *
 * Privacy: this module handles no user biology.  It sees only the safety MCU
 * status byte (device condition — SHDR class).  Lifecycle events surfaced to
 * the caller carry no HR/RR data.
 *
 * IEC 62304 Class B (SW-02).  Safety backstop: the Class C safety MCU
 * independently ignores the re-enable bit while its own lockout is active
 * (np_cardiac_interlock_reenable() denies during s_lockout_active).
 */

#ifndef NP_CVNS_REENABLE_H
#define NP_CVNS_REENABLE_H

#include <stdint.h>
#include <stdbool.h>
#include "np_hub_types.h"

/* ── States ─────────────────────────────────────────────────────────────────── */

typedef enum {
    NP_CVNS_RN_IDLE          = 0,  /* no cardiac cutoff pending                  */
    NP_CVNS_RN_LOCKOUT       = 1,  /* cutoff observed; 30s lockout running       */
    NP_CVNS_RN_AWAIT_CONFIRM = 2,  /* lockout elapsed; waiting for app confirm   */
    NP_CVNS_RN_IMPEDANCE     = 3,  /* confirmed; hub-side impedance check running */
    NP_CVNS_RN_ASSERTING     = 4,  /* all three gates passed; bit asserted       */
} np_cvns_reenable_state_t;

/* ── Lifecycle events (returned to the caller; np_cvns_reenable_shdr_code()
 *    decides which, if any, is written to SHDR — the cutoff only) ──────────── */

typedef enum {
    NP_CVNS_RN_EV_NONE             = 0,
    NP_CVNS_RN_EV_CUTOFF_DETECTED  = 1,  /* CARDIAC rising edge observed         */
    NP_CVNS_RN_EV_CONFIRM_OPEN     = 2,  /* 30s lockout elapsed — app may confirm */
    NP_CVNS_RN_EV_IMPEDANCE_PASSED = 3,  /* gate 3 passed — bit now asserting    */
    NP_CVNS_RN_EV_IMPEDANCE_FAILED = 4,  /* check failed — back to AWAIT_CONFIRM */
    NP_CVNS_RN_EV_IMPEDANCE_TIMEOUT= 5,  /* check never completed — fail-closed  */
    NP_CVNS_RN_EV_REENABLE_COMPLETE= 6,  /* MCU cleared CARDIAC — re-enable done */
    NP_CVNS_RN_EV_ASSERT_TIMEOUT   = 7,  /* MCU did not clear CARDIAC in time    */
    NP_CVNS_RN_EV_CARDIAC_CLEARED  = 8,  /* CARDIAC cleared without hub assert
                                          * (MCU reset / external) — back to IDLE */
} np_cvns_reenable_event_t;

/* ── API ────────────────────────────────────────────────────────────────────── */

/* Reset to IDLE with the re-enable bit inactive.  Called at hub init. */
void np_cvns_reenable_init(void);

/* Identical to init; called by the session runner at every session start so a
 * stale machine can never leak a prior session's state into a new one. */
void np_cvns_reenable_session_reset(void);

/*
 * np_cvns_reenable_on_heartbeat — drive the state machine.  Called by the
 * heartbeat task once per heartbeat exchange, AFTER np_safety_spi_heartbeat().
 *
 * mcu_status:      safety MCU status byte from the most recent heartbeat reply
 *                  (np_safety_spi_get_status()).
 * session_running: true iff the session runner state is NP_SESSION_RUNNING.
 *                  Any other state hard-resets the machine to IDLE (fail-safe:
 *                  no re-enable flow survives a session boundary).
 * now_ms:          monotonic millisecond tick (FreeRTOS tick on target; test
 *                  clock on host).  Wrap-safe.
 *
 * Returns the lifecycle event produced by this beat (NP_CVNS_RN_EV_NONE most
 * beats).  The caller writes to SHDR only what np_cvns_reenable_shdr_code()
 * names for it: the cutoff flag, and nothing for any other event.
 */
np_cvns_reenable_event_t np_cvns_reenable_on_heartbeat(uint8_t  mcu_status,
                                                        bool     session_running,
                                                        uint32_t now_ms);

/*
 * np_cvns_reenable_confirm — deliver the explicit operator/user confirmation
 * from the app (BLE GATT / USB-C CDC transport; wiring is OI-CVNS-HUB-03).
 * Called from the transport task, NOT the heartbeat task.
 *
 * Single-writer design (race-free without a lock): this call does NOT
 * transition state or touch hardware.  It posts a one-shot intent that the
 * heartbeat task consumes from AWAIT_CONFIRM, where it re-validates the gate
 * and starts the hub-side impedance check (gate 3).  Acceptance therefore
 * takes effect on the next heartbeat (≤200ms), and a confirmation can never
 * attach to a cutoff whose 30s lockout has not elapsed or whose sequence was
 * restarted by a fresh cutoff.
 *
 * Accepted (intent posted) ONLY when the machine is in NP_CVNS_RN_AWAIT_CONFIRM
 * (a cardiac cutoff is pending AND the 30s lockout has elapsed).
 *
 * Returns:
 *   NP_HUB_OK                  — confirmation accepted; impedance check will
 *                                start on the next heartbeat
 *   NP_HUB_ERR_SAFETY_REJECTED — not in the confirm window (no cutoff pending,
 *                                lockout not elapsed, or already processing)
 *
 * (If the impedance HAL later fails to start, the heartbeat task returns the
 * machine to AWAIT_CONFIRM and emits NP_CVNS_RN_EV_IMPEDANCE_FAILED; the app
 * may re-confirm.)
 */
np_hub_status_t np_cvns_reenable_confirm(void);

/* True ONLY while in NP_CVNS_RN_ASSERTING — the single bit-emitting state. */
bool np_cvns_reenable_bit_active(void);

/* Current state, for app status queries and tests. */
np_cvns_reenable_state_t np_cvns_reenable_get_state(void);

/*
 * np_cvns_reenable_shdr_code — the SHDR fault code an event is written under
 * (np_log_shdr_fault()), or 0 when the event is not written to SHDR.
 *
 * Only NP_CVNS_RN_EV_CUTOFF_DETECTED maps, to NP_CVNS_SHDR_EV_CUTOFF: the bare
 * fact that the interlock cut stimulation is device-class (NP-FW-CVNS-001
 * §8.2, data-architecture-detail.md §5.1).  Every other event returns 0.  They
 * all follow a cutoff, and IMPEDANCE_* / REENABLE_COMPLETE / ASSERT_TIMEOUT
 * happen only after the wearer confirmed resumption, so in sequence they say
 * whether the person resumed neck stimulation after a cardiac event and how
 * often skin contact failed: UHDR by CLAUDE.md §5.1's defining test, and never
 * SHDR (OI-PRIV-28 (NP-PRIV-ANALYSIS-004), REQ-FWHUB-85).  Pure; no state.
 */
uint8_t np_cvns_reenable_shdr_code(np_cvns_reenable_event_t ev);

/* ── HAL stubs (OI-CVNS-HUB-02) ─────────────────────────────────────────────── */

/*
 * Hub-side CVNS electrode impedance measurement, via the cervical VNS
 * accessory's measurement path (1kHz AC probe, same hardware used for the
 * pre-session check).  Asynchronous: start, then poll each heartbeat.
 *
 * np_cvns_hal_impedance_start — begin a fresh measurement.  Returns NP_HUB_OK
 *   if the measurement was started.
 * np_cvns_hal_impedance_poll — returns true when the measurement is complete;
 *   *passed_out is set true iff the measured impedance is within the CVNS
 *   electrode acceptance window.  While returning false, *passed_out is
 *   untouched.
 */
extern np_hub_status_t np_cvns_hal_impedance_start(void);
extern bool            np_cvns_hal_impedance_poll(bool *passed_out);

#endif /* NP_CVNS_REENABLE_H */
