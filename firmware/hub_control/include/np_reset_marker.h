/*
 * np_reset_marker.h — the durable factory-reset marker in the Config partition
 * Document: NP-FW-NVRAM-001 Rev 3 §3.4.1 (option A, principal decision
 *           2026-09-26; OI-NVRAM-05), NP-FW-EMMC-002 Rev 4 §B.3–§B.4
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * The headset has no VBAT rail, so SNVS_LPGPR1's reset_in_progress flag is
 * cleared by exactly the event it was meant to detect: a power loss mid-reset.
 * This module is the flag's durable replacement for that case.  It keeps the
 * evidence that a reset started in the Config partition itself:
 *
 *   R-3   np_factory_reset_hal_marker_write() writes NP_CFG_FILE_RESET_MARKER
 *         (REPLICATED: two copies, two metadata pairs) BEFORE the first erase;
 *   R-7   erases the partition it lives in, so the marker goes with it;
 *   R-10  formats Config and writes the new defaults — no marker.
 *
 * So from R-3 until R-7 the marker is present, and from R-7 until R-10 Config
 * has no filesystem at all.  np_factory_reset_hal_marker_state() reports which
 * of those states Config is in, and np_factory_reset_boot_check() completes
 * the reset on either.
 *
 * The two HAL functions of np_factory_reset.h are defined here, over
 * np_cfg_store.  np_reset_marker_classify() is the decision itself, separated
 * so it can be tested without a medium.
 *
 * Nothing here is wired into bring-up yet: Config is not mounted at boot until
 * a block device exists to mount (#340).  OI-NVRAM-17 records the call site
 * np_hub_control_app_main() needs — before np_log_backend_init(), which is the
 * first thing to touch UHDR or SHDR.
 */

#ifndef NP_RESET_MARKER_H
#define NP_RESET_MARKER_H

#include "np_hub_types.h"
#include "np_factory_reset_types.h"

/* The marker's payload.  Its content carries no information beyond "a reset
 * was commanded and has started": no time, no count, no identity. */
#define NP_RESET_MARKER_PAYLOAD      "NPRSTMK1"
#define NP_RESET_MARKER_PAYLOAD_LEN  8U

/*
 * The decision, from the two facts it depends on:
 *   mount_st   np_cfg_store_mount()'s result
 *   read_st    np_cfg_store_replicated_read()'s result for the marker, when
 *              the mount succeeded (ignored otherwise)
 *
 *   mount INTEGRITY                     → NO_STORE (no filesystem)
 *   mount any other failure             → UNKNOWN
 *   read  OK                            → PRESENT
 *   read  NOT_PRESENT                   → ABSENT
 *   read  STORE_INTEGRITY               → PRESENT: a copy exists and was
 *                                         refused, so a write was begun, and a
 *                                         marker write is only ever begun by a
 *                                         reset the user commanded
 *   read  anything else (STORE_IO …)    → UNKNOWN
 */
np_fr_marker_state_t np_reset_marker_classify(np_hub_status_t mount_st,
                                              np_hub_status_t read_st);

#endif /* NP_RESET_MARKER_H */
