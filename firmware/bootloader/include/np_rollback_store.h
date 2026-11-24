/*
 * NeurOne Bootloader — rollback-floor store, HAL seam (OI-PRIV-32, NP-PRIV-ANALYSIS-004)
 *
 * The monotonic minimum firmware version lives in the eMMC's RPMB (decided by
 * the principal, 2026-11-23: docs/human-input/oi-priv-32-firmware-rollback-
 * floor-and-dfu.md §8). The device implementation is np_rollback_store_rpmb.c
 * (REQ-SW02-16): the floor's place and record are np_rollback_rpmb.h, the
 * frame protocol np_emmc_rpmb.h.
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
 * The store's own seams are still unbuilt in the device build and fail: the
 * RPMB key (np_rpmb_key.h), the HMAC-SHA256 primitive (np_rpmb_mac.h,
 * OI-SW02-01) and the nonce entropy (np_entropy.h, OI-SW02-03). Until all three
 * are built the read fails, so the device installs no firmware update and no
 * DFU image. np_bootloader_rollback_tests links a fake store;
 * np_bootloader_rpmb_tests runs the RPMB store against a fake RPMB device.
 */
#ifndef NP_ROLLBACK_STORE_H
#define NP_ROLLBACK_STORE_H

#include "np_types.h"

np_status_t np_rollback_store_read(uint32_t *floor);
np_status_t np_rollback_store_raise(uint32_t floor);

#endif /* NP_ROLLBACK_STORE_H */
