/*
 * NeurOne Hub Control Program — why a session is held (SWR-SS-09, REQ-FWHUB-72)
 * Document: NP-SW-001 §6.3 (SWR-SS-09); NP-FW-HUB-001 §8.6
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * SESSION_HOLD (READ/NOTIFY, 1 byte): the cause of the hold the session is in, so the app can tell a pause the person asked for
 * from a halt the helmet made on its own, and put the Stop / Resume question to the person for the second. It is a
 * characteristic of its own and not a byte of SESSION_PROGRESS so that a hub and an app of different ages still agree on that
 * frame (an app that knows nothing of this one simply never reads it).
 *
 *   0  not held
 *   1  the person paused it (SESSION_PAUSE 0x01)
 *   2  the person pressed the panic control (SESSION_PAUSE 0x03)
 *   3  the photoparoxysmal detector found a discharge
 *   4  the detector could no longer see the EEG signal
 *
 * A detector cause outranks a person's: if the helmet halts on its own while a pause or a panic is held, the cause becomes the
 * detector's, because that is the one the person has not decided on. It returns to 0 when the session is running again or over.
 */
#ifndef NP_SESSION_HOLD_H
#define NP_SESSION_HOLD_H

#include <stddef.h>
#include <stdint.h>

#include "np_hub_types.h"

#define NP_HOLD_LEN   1U

typedef enum {
    NP_HOLD_NONE               = 0,
    NP_HOLD_USER_PAUSE         = 1,
    NP_HOLD_USER_PANIC         = 2,
    NP_HOLD_DETECTOR_DISCHARGE = 3,
    NP_HOLD_DETECTOR_BLIND     = 4,
} np_hold_cause_t;

/* The cause a detector halt carries outranks a person's. */
static inline int np_hold_outranks(np_hold_cause_t a, np_hold_cause_t b)
{
    return (int)a > (int)b;
}

typedef np_hold_cause_t (*np_hold_source_fn)(void);

void np_hold_set_source(np_hold_source_fn source);

/* The GATT read of SESSION_HOLD. */
np_hub_status_t np_hold_read(uint8_t *buf, size_t cap, size_t *len_out);

/* Send the current cause as a notification (the runner calls this whenever the cause changes). */
void np_hold_notify(void);

#endif /* NP_SESSION_HOLD_H */
