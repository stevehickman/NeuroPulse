/*
 * NeurOne Bootloader — RPMB key, device build, NOT BUILT (OI-PRIV-32, NP-PRIV-ANALYSIS-004)
 *
 * The key source and the factory step that programs the key into the eMMC
 * belong to the "OI-PRIV-32: factory step that programs the RPMB key" change,
 * which replaces this file with an implementation of np_rpmb_key.h.
 *
 * Until then the key is unavailable, so the RPMB floor is unreadable and
 * np_rollback_admit() refuses every image: the device installs no OTA update
 * and accepts no DFU image. That is the fail-closed reading of the contract.
 * It is not replaced by a fixed or all-zero key, which would let anyone who
 * reads this source forge the floor.
 */
#include "np_rpmb_key.h"

np_status_t np_rpmb_key_get(uint8_t key[32])
{
    (void)key;
    return NP_ERR_ROLLBACK_STORE;
}
