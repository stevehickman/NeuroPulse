/*
 * NeurOne Hub Control Program — Module Inventory Scan Service
 * See np_inventory_scan.h.
 */

#include "np_inventory_scan.h"

#include "np_hexmap_inventory.h"
#include "np_module_map.h"
#include "np_session_lease.h"
#include "np_sw02_platform_hal.h"

static volatile bool s_requested;   /* a pass is owed because of a trigger */
static bool          s_power_was;

void np_inventory_request(void)
{
    np_module_map_mark_stale();
    s_requested = true;
}

void np_inventory_boot(void)
{
    // Fail-closed: a restore that cannot be read leaves the map empty, and a restored record is unused until the pass confirms it.
    (void)np_module_map_restore();
    s_power_was = np_hal_power_source_present();
    np_inventory_request();
}

bool np_inventory_service(void)
{
    const bool power = np_hal_power_source_present();
    if (power && !s_power_was) {
        np_inventory_request();
    }
    s_power_was = power;

    if (np_hal_rim_latch_open()) {
        np_inventory_request();   // stale while open, and a pass is owed for when it closes
        return false;
    }
    if (!s_requested && !np_hexmap_scan_pending()) {
        return false;
    }
    s_requested = false;

    bool changed = false;
    np_hub_status_t rc;
    if (np_lease_probe_begin()) {
        rc = np_hexmap_scan_all(&changed);   // the lock is held for the pass: a load waits for it, which is the delay we want
        np_lease_probe_end();
    } else {
        np_hexmap_scan_defer();              // a session holds the lease: stale for new uploads, the pass remembered
        rc = NP_HUB_ERR_SESSION_ACTIVE;
    }
    if (rc == NP_HUB_OK && changed) {
        (void)np_module_map_persist();       // once per pass, and only if a socket changed
    }
    return rc != NP_HUB_ERR_SESSION_ACTIVE;
}
