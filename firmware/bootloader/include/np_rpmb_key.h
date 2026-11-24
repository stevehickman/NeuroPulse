/*
 * NeurOne Bootloader — RPMB authentication key, HAL seam (OI-PRIV-32, NP-PRIV-ANALYSIS-004)
 *
 * np_rpmb_key_get() returns the 32-byte key the eMMC's RPMB was programmed
 * with, or an error. Where the key comes from (a fuse field, a value derived
 * from a device-unique secret) and the factory step that programs it into the
 * eMMC are NOT decided here: they are the work of the "OI-PRIV-32: factory step
 * that programs the RPMB key" change, which implements this exact signature.
 *
 * Contract for every implementation:
 *   - NP_OK only with all 32 bytes of the key written to `key`.
 *   - Any other return is an error, and the floor store treats it as an
 *     unreadable floor: np_rollback_admit() refuses every image. There is no
 *     default key and no all-zero key.
 *   - The caller wipes its copy (np_rpmb_wipe) when it is done.
 *
 * The device build links np_rpmb_key_unbuilt.c, which fails, until the key
 * source exists: the floor is then unreadable and the device installs no OTA or
 * DFU image (fail closed). Host tests link a fake.
 */
#ifndef NP_RPMB_KEY_H
#define NP_RPMB_KEY_H

#include <stdint.h>
#include "np_types.h"

np_status_t np_rpmb_key_get(uint8_t key[32]);

#endif /* NP_RPMB_KEY_H */
