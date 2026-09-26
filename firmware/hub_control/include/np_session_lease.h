/*
 * NeurOne Hub Control Program — Session Lease
 * Document: NP-FW-HUB-001 Rev 12 §2.2 (REQ-FWHUB-03, OI-FWHUB-17)
 *
 * Keeps module detection off a session by exclusion, not by a state check.
 *
 * The runner CLAIMS the lease in np_runner_load(), before it touches the
 * descriptor, and RELEASES it when np_runner_run() has set the final state —
 * or on any load failure.  task_module_detect wraps each single-slot rescan in
 * np_lease_probe_begin() / np_lease_probe_end().  Both sides go through one
 * lock, so:
 *   - a probe never STARTS while the lease is held, and
 *   - a claim never COMPLETES while a probe is in progress: it waits for
 *     that one probe to end.
 * The window Rev 8 left — a session starting between the detect task's state
 * read and its detect() — is therefore closed rather than narrowed.
 *
 * The lock is injected so this file is pure C and host-testable; on target it
 * is a FreeRTOS mutex (np_session_runner.c), whose priority inheritance lifts
 * the lowest-priority detect task over a waiting task_protocol_rx for the
 * length of one probe.  Every take and its give are in the same call on the
 * same task, as a FreeRTOS mutex requires.  Before np_lease_init() (and on
 * host, unless a test injects one) there is no lock and the lease is a flag.
 */

#ifndef NP_SESSION_LEASE_H
#define NP_SESSION_LEASE_H

#include "np_hub_types.h"
#include <stdbool.h>

typedef void (*np_lease_lock_fn)(void);

/* Install the lock pair and clear the lease.  Call once, before any task runs. */
void np_lease_init(np_lease_lock_fn lock, np_lease_lock_fn unlock);

/* Claim the lease for a session.  NP_HUB_ERR_SESSION_ACTIVE if it is already
 * held (a session is loaded, running or stopping); NP_HUB_OK otherwise.  Waits
 * for a probe in progress to end. */
np_hub_status_t np_lease_claim(void);

/* Release the lease.  Idempotent. */
void np_lease_release(void);

/* true: no session holds the lease, and the lock is now HELD by the caller,
 * who must call np_lease_probe_end() after its one probe.
 * false: a session holds the lease; nothing is held and nothing may be
 * probed. */
bool np_lease_probe_begin(void);
void np_lease_probe_end(void);

/* Whether a session holds the lease (a read for diagnostics and tests). */
bool np_lease_held(void);

#endif /* NP_SESSION_LEASE_H */
