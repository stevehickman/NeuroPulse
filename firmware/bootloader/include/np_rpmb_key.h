/*
 * NeurOne Bootloader — RPMB authentication key, HAL seam (OI-PRIV-32, NP-PRIV-ANALYSIS-004)
 *
 * np_rpmb_key_get() returns the 32-byte key the eMMC's RPMB was programmed
 * with, or an error. Where the key comes from is undecided: the options and the
 * recommendation are in docs/human-input/oi-priv-32-rpmb-key-source.md, and the
 * factory step that programs it is NP-REG-UPG-001 §7.10 (REQ-RPMBK-01 to -06).
 *
 * Contract for every implementation:
 *   - NP_OK only with all 32 bytes of the key written to `key`.
 *   - Any other return is an error, and leaves no key material in `key`. The
 *     floor store treats it as an unreadable floor: np_rollback_admit() refuses
 *     every image. There is no default, constant, all-zero or build-time key in
 *     any build, test mode or bench mode (REQ-RPMBK-01).
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
