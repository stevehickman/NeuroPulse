/*
 * np_session_count.h — the persisted device session count (OI-LFS-12)
 * Document: NP-SOUP-LFS-001 Rev 9 §13.13; NP-FW-EMMC-001 EMMC-SHDR-09
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * EMMC-SHDR-09: "Session count is incremented in Config partition at session
 * start; it is the only time-like reference in SHDR."  Until OI-LFS-12 the
 * count was incremented in RAM by np_session_log and never written back, so a
 * reboot restarted it from a platform seam that nothing implemented.
 *
 * The count is a Config file, NP_CFG_FILE_SESSION_COUNT, kept REPLICATED by
 * np_cfg_store: two enveloped copies in two metadata pairs, the higher
 * generation winning.  A torn write therefore leaves the old count or the new
 * one — never an older one — and one lost entry is repaired on first read.
 *
 * It is device-condition data (an unsigned count, no timestamp, no biology):
 * SHDR-classified under CLAUDE.md §5.1, and it bounds no emission, so
 * REQ-LFS-01 does not reach it.
 */

#ifndef NP_SESSION_COUNT_H
#define NP_SESSION_COUNT_H

#include <stdint.h>

#include "np_hub_types.h"

/*
 * Read the persisted count into *out.
 *   NP_HUB_OK               *out is the last committed count
 *   NP_HUB_ERR_NOT_PRESENT  never committed (a new device): *out = 0
 *   anything else           the store is unavailable: *out = 0
 * In every non-OK case the logger still never reuses a UHDR session file —
 * it steps past existing files (np_session_log.h) — so 0 is a safe seed.
 */
np_hub_status_t np_session_count_load(uint32_t *out);

/* Persist `count`.  Called by np_session_log BEFORE the session's UHDR file is
 * created, so no file can exist whose count was not committed first. */
np_hub_status_t np_session_count_commit(uint32_t count);

#endif /* NP_SESSION_COUNT_H */
