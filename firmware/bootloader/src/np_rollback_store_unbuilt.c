/*
 * NeurOne Bootloader — rollback-floor store, device build, NOT BUILT (OI-PRIV-32, NP-PRIV-ANALYSIS-004)
 *
 * The store is the eMMC's RPMB (the principal, 2026-11-23,
 * docs/human-input/oi-priv-32-firmware-rollback-floor-and-dfu.md §8), and
 * neither the RPMB driver nor the key provisioning is built.
 *
 * Until they are, the read fails, so np_rollback_admit() refuses every image:
 * the device installs no OTA update and accepts no DFU image. That is the
 * fail-closed reading of np_rollback_store.h's contract, the same as the
 * production power governor refusing every load until OI-FWHUB-09. It is not
 * replaced by a store that reports floor 0, which would admit every older
 * signed image and look like enforcement.
 */
#include "np_rollback_store.h"

np_status_t np_rollback_store_read(uint32_t *floor)
{
    (void)floor;
    return NP_ERR_ROLLBACK_STORE;
}

np_status_t np_rollback_store_raise(uint32_t floor)
{
    (void)floor;
    return NP_ERR_ROLLBACK_STORE;
}
