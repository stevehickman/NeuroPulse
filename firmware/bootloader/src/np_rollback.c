/*
 * NeurOne Bootloader — firmware anti-rollback (OI-PRIV-32, NP-PRIV-ANALYSIS-004 Finding M5)
 * Requirement: REQ-SW02-15. The rule and where it is enforced: np_rollback.h.
 */
#include "np_rollback.h"
#include "np_rollback_store.h"

np_status_t np_rollback_admit(uint32_t signed_version)
{
    uint32_t floor = 0U;
    if (np_rollback_store_read(&floor) != NP_OK) {
        return NP_ERR_ROLLBACK_STORE;   /* fail closed: no floor, no install */
    }
    return np_rollback_version_ok(signed_version, floor) ? NP_OK : NP_ERR_ROLLBACK;
}

np_status_t np_rollback_commit(uint32_t confirmed_version)
{
    uint32_t floor = 0U;
    if (np_rollback_store_read(&floor) != NP_OK) {
        return NP_ERR_ROLLBACK_STORE;
    }
    if (confirmed_version <= floor) {
        return NP_OK;                   /* never lowered, nothing to raise */
    }
    return (np_rollback_store_raise(confirmed_version) == NP_OK) ? NP_OK : NP_ERR_ROLLBACK_STORE;
}
