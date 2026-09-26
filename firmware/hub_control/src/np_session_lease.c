/*
 * NeurOne Hub Control Program — Session Lease
 * Document: NP-FW-HUB-001 Rev 12 §2.2 (REQ-FWHUB-03, OI-FWHUB-17)
 *
 * See np_session_lease.h.  s_held is read and written only with the lock
 * taken, so the claim and the probe decision are each atomic against the
 * other.
 */

#include "np_session_lease.h"
#include <stddef.h>

static np_lease_lock_fn s_lock;
static np_lease_lock_fn s_unlock;
static bool             s_held;

static void take(void) { if (s_lock != NULL)   { s_lock(); } }
static void give(void) { if (s_unlock != NULL) { s_unlock(); } }

void np_lease_init(np_lease_lock_fn lock, np_lease_lock_fn unlock)
{
    s_lock   = lock;
    s_unlock = unlock;
    s_held   = false;
}

np_hub_status_t np_lease_claim(void)
{
    take();
    if (s_held) {
        give();
        return NP_HUB_ERR_SESSION_ACTIVE;
    }
    s_held = true;
    give();
    return NP_HUB_OK;
}

void np_lease_release(void)
{
    take();
    s_held = false;
    give();
}

bool np_lease_probe_begin(void)
{
    take();
    if (s_held) {
        give();
        return false;
    }
    return true;    /* lock stays held across the caller's one probe */
}

void np_lease_probe_end(void)
{
    give();
}

bool np_lease_held(void)
{
    take();
    bool held = s_held;
    give();
    return held;
}
