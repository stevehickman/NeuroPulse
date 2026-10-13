/*
 * NeurOne Hub Control Program — session progress (SWR-SS-07, OI-UI-KMP-09 R6)
 * Document: NP-SW-001 §6.3 (SWR-SS-07)
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * SESSION_PROGRESS (READ/NOTIFY, 16 bytes, little-endian) tells the app where a session is, so a person
 * can decide whether to pause or stop it:
 *
 *   [0]      state            np_session_state_t
 *   [1]      active part      index, in the signed descriptor's command order, of the highest-numbered
 *                             command that is running now; 0xFF when none is
 *   [2..3]   active mask      bit n set = a running command drives modality type n (np_hub_mod_type_t)
 *   [4..7]   elapsed ms       session time; stops advancing while paused; capped at total
 *   [8..11]  remaining ms     total - elapsed, 0 once reached
 *   [12..15] total ms         the descriptor's session length
 *
 * Elapsed and remaining are what the helmet holds, so the app shows the helmet's figures and not its
 * own clock.  A "part" is a command of the descriptor: the descriptor has no other segmentation, and a
 * second numbering would be a second thing to keep in step.
 *
 * The frame is built here from a snapshot (np_progress_in_t), a pure function, so it is host-tested
 * without the runner; the runner supplies the snapshot through the source set at bring-up.
 */

#ifndef NP_SESSION_PROGRESS_H
#define NP_SESSION_PROGRESS_H

#include <stddef.h>
#include <stdint.h>

#include "np_hub_types.h"

#define NP_PROGRESS_LEN         16U
#define NP_PROGRESS_NO_PART     0xFFU

typedef struct {
    np_session_state_t       state;
    uint32_t                 elapsed_ms;   /* session time, already excluding any paused time */
    uint32_t                 total_ms;
    const np_session_cmd_t  *cmds;
    uint8_t                  cmd_count;
} np_progress_in_t;

/* Build the 16-byte frame from a snapshot. */
void np_progress_build(const np_progress_in_t *in, uint8_t out[NP_PROGRESS_LEN]);

/* The runner's snapshot function: fills `out` with the frame for the session as it is now. */
typedef void (*np_progress_source_fn)(uint8_t out[NP_PROGRESS_LEN]);

void np_progress_set_source(np_progress_source_fn source);

/* The GATT read of SESSION_PROGRESS. */
np_hub_status_t np_progress_read(uint8_t *buf, size_t cap, size_t *len_out);

/* Send the current frame as a notification (the runner calls this on its telemetry tick and at the end). */
void np_progress_notify(void);

#endif /* NP_SESSION_PROGRESS_H */
