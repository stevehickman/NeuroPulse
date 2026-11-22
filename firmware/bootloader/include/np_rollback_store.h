/*
 * NeurOne Bootloader — rollback-floor store, HAL seam (OI-PRIV-32, NP-PRIV-ANALYSIS-004)
 *
 * Where the monotonic minimum firmware version lives is a hardware choice that
 * has not been made: i.MX RT1062 OCOTP fuses, an eMMC RPMB counter, or
 * something else. It is briefed for the principal in
 * docs/human-input/oi-priv-32-firmware-rollback-floor-and-dfu.md.
 *
 * Contract for every implementation:
 *   - np_rollback_store_read() reports the floor, or an error. It never
 *     invents a value: an unprovisioned or unreadable store is an error, and
 *     np_rollback.c refuses every image on an error.
 *   - np_rollback_store_raise() stores `floor` only if it is greater than the
 *     stored floor; a lower or equal value leaves the store unchanged and is
 *     NP_OK. The store must not be lowerable by any software path, which is
 *     why the candidates are one-time-programmable fuses or an authenticated
 *     monotonic counter, not an ordinary eMMC sector.
 *
 * The device build links np_rollback_store_unbuilt.c until the choice is made:
 * its read fails, so the device installs no firmware update and no DFU image.
 * Host tests link a fake.
 */
#ifndef NP_ROLLBACK_STORE_H
#define NP_ROLLBACK_STORE_H

#include "np_types.h"

np_status_t np_rollback_store_read(uint32_t *floor);
np_status_t np_rollback_store_raise(uint32_t floor);

#endif /* NP_ROLLBACK_STORE_H */
