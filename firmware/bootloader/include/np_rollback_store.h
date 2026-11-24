/*
 * NeurOne Bootloader — rollback-floor store, HAL seam (OI-PRIV-32, NP-PRIV-ANALYSIS-004)
 *
 * The monotonic minimum firmware version lives in the eMMC's RPMB (decided by
 * the principal, 2026-11-23: docs/human-input/oi-priv-32-firmware-rollback-
 * floor-and-dfu.md §8). The RPMB driver and the RPMB key's provisioning are
 * not built.
 *
 * Contract for every implementation:
 *   - np_rollback_store_read() reports the floor, or an error. It never
 *     invents a value: an unprovisioned or unreadable store is an error, and
 *     np_rollback.c refuses every image on an error.
 *   - np_rollback_store_raise() stores `floor` only if it is greater than the
 *     stored floor; a lower or equal value leaves the store unchanged and is
 *     NP_OK. The store must not be lowerable by any software path, which is
 *     why it is an authenticated store (RPMB), not an ordinary eMMC sector.
 *   - A blank RPMB, or one whose key does not authenticate (a replaced eMMC,
 *     say), is an error like any other: it refuses every image, never reads
 *     as floor 0. A restore from NeurOne's registry, if one is built, only
 *     ever raises the floor (brief §4.1, §8).
 *
 * The device build links np_rollback_store_unbuilt.c until the RPMB store is built:
 * its read fails, so the device installs no firmware update and no DFU image.
 * Host tests link a fake.
 */
#ifndef NP_ROLLBACK_STORE_H
#define NP_ROLLBACK_STORE_H

#include "np_types.h"

np_status_t np_rollback_store_read(uint32_t *floor);
np_status_t np_rollback_store_raise(uint32_t floor);

#endif /* NP_ROLLBACK_STORE_H */
