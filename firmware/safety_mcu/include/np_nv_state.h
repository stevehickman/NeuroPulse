/*
 * NeurOne Safety MCU — SW01-M09: Non-Volatile Safety State
 * Document: NP-SW-FAULTMSG-001 P1 (OI-FAULTMSG-01) and the per-user scope
 *           decision of 2026-09-22; NP-FW-CVNS-001 §5.4
 *
 * What the safety MCU remembers across a POWER-ON reset:
 *   - the ACTIVE USER — an opaque tag the app sends (np_safety_user_cmd_t);
 *     the same user is assumed until the app names another;
 *   - which users have a cervical VNS cardiac cutoff not yet acknowledged.
 * A cutoff is held for the user who triggered it and nobody else.  See
 * src/np_nv_state.c for the record log and why it fails closed.
 *
 * IEC 62304 Class C — MISRA C:2012.  C11, no GNU extensions.
 */

#ifndef NP_NV_STATE_H
#define NP_NV_STATE_H

#include "np_safety_protocol.h"
#include <stdbool.h>
#include <stdint.h>

/* Scan and replay the NV pages.  Call once at boot, before any query.
 * Non-blocking (reads only). */
np_safe_status_t np_nv_state_init(void);

/* The active user tag; NP_SAFETY_USER_UNSPECIFIED until an app names one. */
uint32_t np_nv_current_user(void);

/* true if cervical VNS must be withheld for `user`: they have an outstanding
 * cutoff, or an outstanding cutoff cannot be attributed to one person (recorded
 * before any user was named, or a torn record), or `user` is unspecified while
 * any cutoff is outstanding.  FAIL-CLOSED throughout. */
bool np_nv_cardiac_blocked(uint32_t user);

/* true if ANY user on this device has an outstanding cutoff — drives the
 * blanket warning every user sees (np_safety_nv_report_t). */
bool np_nv_cardiac_outstanding(void);

/* Record that `user` now has (pending) or no longer has (!pending) an
 * outstanding cutoff.  Acknowledging also clears the unattributable entries —
 * the person who ran the confirmation owns them.  Returns true only when the
 * replayed state reads back as intended.  BLOCKING and CORE-STALLING
 * (np_safety_hal.h): call only while every stimulation channel is off. */
bool np_nv_cardiac_set(uint32_t user, bool pending);

/* Record a change of active user.  Same return and stall contract. */
bool np_nv_set_current_user(uint32_t user);

#endif /* NP_NV_STATE_H */
