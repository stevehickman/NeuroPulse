/*
 * NeurOne Hub Control Program — the pause time limit (SWR-SS-05)
 * Document: NP-SW-001 §6.3 (SWR-SS-05); NP-FW-NVRAM-001 (Config)
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * How long a session may stay paused before the hub ends it.  It is the hardware owner's option, not the wearer's
 * and not the protocol's, and it defaults to NO LIMIT: until the owner sets one, a paused session stays paused until
 * it is resumed or stopped, and a link lost during a pause leaves it paused.
 *
 * Wire (PAUSE_LIMIT, READ/WRITE, encrypted link only): a little-endian uint32, seconds.  0 is no limit.  Any value is
 * accepted; the owner decides what is sensible for the clinic, so no figure is built in.  A write takes effect at the
 * NEXT pause: the limit in force when a pause begins is the limit for that pause, so changing it never ends or extends a
 * pause already running.
 *
 * Persistence: one replicated Config record, written by np_pause_limit_poll() from the heartbeat task (never from the
 * BLE write callback, which must not block on the store).  A record that is absent is "no limit"; one that is present
 * but unreadable is also treated as no limit until a read succeeds, and is not overwritten by the default.
 *
 * WHO MAY WRITE.  The hub cannot tell the hardware owner from anyone else holding the bonded link: it accepts the write
 * only on an encrypted link (the platform seam enforces NP_GATT_PROP_ENC) and nothing more.  Restricting the setting to
 * the warranty owner is the app's (the warranty-owner profile, SWR-WO-01); whether the hub should also demand a signed
 * owner credential is not decided.
 *
 * Reaching the limit ends the session through the runner's ordinary abort path with reason NP_ABORT_PAUSE_TIMEOUT.
 */

#ifndef NP_PAUSE_LIMIT_H
#define NP_PAUSE_LIMIT_H

#include <stddef.h>
#include <stdint.h>

#include "np_hub_types.h"

#define NP_PAUSE_LIMIT_NONE   0U

/* Load the stored limit; call once at bring-up, after the Config store is bound. */
void     np_pause_limit_init(void);

/* The limit in force, in seconds; NP_PAUSE_LIMIT_NONE (0) for no limit. */
uint32_t np_pause_limit_seconds(void);

/* Persist a changed limit.  Call from the heartbeat task. */
void     np_pause_limit_poll(void);

/* The GATT read and write of PAUSE_LIMIT. */
np_hub_status_t np_pause_limit_read(uint8_t *buf, size_t cap, size_t *len_out);
np_hub_status_t np_pause_limit_write(const uint8_t *data, size_t len);

#endif /* NP_PAUSE_LIMIT_H */
