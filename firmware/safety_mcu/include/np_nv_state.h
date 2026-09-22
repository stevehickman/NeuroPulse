/*
 * NeurOne Safety MCU — SW01-M09: Non-Volatile Safety State
 * Document: NP-SW-FAULTMSG-001 P1 (OI-FAULTMSG-01); NP-FW-CVNS-001 §5.4
 *
 * The one fact the safety MCU remembers across a POWER-ON reset: a cervical
 * VNS cardiac cutoff that the app has not yet acknowledged.  See
 * src/np_nv_state.c for the record format and why it fails closed.
 *
 * IEC 62304 Class C — MISRA C:2012.  C11, no GNU extensions.
 */

#ifndef NP_NV_STATE_H
#define NP_NV_STATE_H

#include "np_safety_protocol.h"
#include <stdbool.h>

/* Scan the NV pages and cache the current state.  Call once at boot, before
 * np_nv_cardiac_pending().  Non-blocking (reads only). */
np_safe_status_t np_nv_state_init(void);

/* true if a cardiac cutoff is awaiting app acknowledgement.  FAIL-CLOSED: a
 * newest record that cannot be decoded reads as true.  Never-written storage
 * reads as false. */
bool np_nv_cardiac_pending(void);

/* Record a new state.  Returns true only when the record reads back as written.
 * BLOCKING and CORE-STALLING (see np_safety_hal.h): call only while every
 * stimulation channel is off. */
bool np_nv_cardiac_pending_set(bool pending);

#endif /* NP_NV_STATE_H */
