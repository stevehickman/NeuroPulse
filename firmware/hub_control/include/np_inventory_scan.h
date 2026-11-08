/*
 * NeurOne Hub Control Program — Module Inventory Scan Service
 * Document: NP-HEX-ZM-001 §7 (OI-HEXMAP-03), NP-FW-HUB-001 REQ-FWHUB-73, OI-HWFIT-01
 *
 * The call sites of the module map's pass. np_hexmap_scan_all() is one whole pass; this decides WHEN one runs:
 *
 *   boot                      the map is restored from Config (fail-closed: a failed restore leaves it empty), the inventory is marked
 *                             stale so an early upload waits, and a pass is owed. The supercap is assumed not to carry the hub through a
 *                             loss of power, so every power-up scans before a socket protocol is admitted.
 *   a rim latch is open       the cluster levers are reachable only through the rim latches, so no module can be seated or released while
 *                             all are closed, and this covers a module entering or leaving a socket. While any latch is open the
 *                             inventory is stale and no pass runs (the modules are being changed); a pass is owed.
 *   a power source connects   a rise of np_hal_power_source_present(). A pass is owed.
 *   the layout is rebound     np_module_map_init() empties the map, so a pass is owed (np_inventory_request()).
 *   a session ends            a pass asked for under a session was deferred (np_hexmap_scan_pending()); it runs once the lease is free.
 *
 * np_inventory_service() is called by the module-detect task each poll. It does the owed pass if the latches are closed, under the
 * probe lease when one can be had, and persists the map once if anything changed. A pass is not retried after it FAILS: the next
 * trigger starts the next one. No FreeRTOS dependency; host-tested by tests/np_inventory_scan_tests.c.
 */

#ifndef NP_INVENTORY_SCAN_H
#define NP_INVENTORY_SCAN_H

#include <stdbool.h>

/* Restore the cached map, mark the inventory stale and owe a pass. After np_layout_bind(), before the scheduler. */
void np_inventory_boot(void);

/* Owe a pass and mark the inventory stale at once, so no upload is admitted on it meanwhile. Safe from any task. */
void np_inventory_request(void);

/* Sample the triggers and do the pass that is owed, if it can be done now. Returns true when a pass ran this call. */
bool np_inventory_service(void);

#endif /* NP_INVENTORY_SCAN_H */
