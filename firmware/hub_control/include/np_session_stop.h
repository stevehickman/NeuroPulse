/*
 * NeurOne Hub Control Program — session stop (SWR-SS-06, OI-UI-KMP-09 R5)
 * Document: NP-SW-001 §6.3 (SWR-SS-06); NP-SOUP-BTLE-001 §5.3 (the stop is a link write)
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * SESSION_STOP (WRITE, 1 byte, 0x01) ends the session at the hub.  The app wrote this characteristic
 * before the hub published it; until now nothing answered, so the only way to end a session was the
 * cable.  The handler asks the session runner to abort with reason NP_ABORT_USER, which takes the same
 * path as every other abort: the runner's graceful shutdown drops every enable line through the safety
 * MCU, closes the records and reports the end.  It adds no path to a stimulation line.
 *
 * What it accepts.  A stop is accepted while the session is RUNNING or PAUSED.  In any other state the
 * write is refused (ATT 0x80), so the app learns the stop did not take effect rather than assuming it
 * did.  That includes the interval between a protocol being loaded and its run starting: the runner
 * clears stale abort bits when a run starts, so a stop accepted then would be lost silently.
 *
 * The runner is reached through two functions set at bring-up, so the handler is host-testable
 * without the scheduler.
 */

#ifndef NP_SESSION_STOP_H
#define NP_SESSION_STOP_H

#include <stddef.h>
#include <stdint.h>

#include "np_hub_types.h"

#define NP_SESSION_STOP_VALUE   0x01U

typedef np_session_state_t (*np_session_stop_state_fn)(void);
typedef void               (*np_session_stop_abort_fn)(np_abort_reason_t reason);

/* Bring-up: where the session state is read and where an abort is requested. */
void np_session_stop_init(np_session_stop_state_fn state, np_session_stop_abort_fn abort_fn);

/* The GATT write handler (exactly one byte, 0x01). */
np_hub_status_t np_session_stop_on_write(const uint8_t *data, size_t len);

#endif /* NP_SESSION_STOP_H */
