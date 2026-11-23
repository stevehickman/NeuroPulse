/*
 * NeurOne Bootloader — RPMB authentication key, HAL seam (OI-PRIV-32, NP-PRIV-ANALYSIS-004)
 *
 * The rollback floor lives in the eMMC's RPMB (the principal, 2026-11-23,
 * docs/human-input/oi-priv-32-firmware-rollback-floor-and-dfu.md §8). Every
 * RPMB write is authenticated with a 32-byte key programmed into the eMMC once
 * at manufacture; the bootloader must obtain the same key at every boot.
 *
 * Contract (REQ-RPMBK-01, NP-REG-UPG-001 §7.10):
 *   - np_rpmb_key_get() writes this unit's own RPMB key into key[] and returns
 *     NP_OK, or returns an error. On an error key[] holds no key material.
 *   - It never returns a constant, default, all-zero or build-time key, in any
 *     build, test mode or bench mode. A caller that gets an error refuses every
 *     image, exactly as an unreadable floor does (REQ-SW02-15 (b)).
 *   - The caller wipes key[] once the RPMB exchange is done.
 *
 * Where the key comes from is not decided: docs/human-input/oi-priv-32-rpmb-key-source.md.
 * Until it is, the device build links np_rpmb_key_unbuilt.c, which always fails.
 */
#ifndef NP_RPMB_KEY_H
#define NP_RPMB_KEY_H

#include "np_types.h"

np_status_t np_rpmb_key_get(uint8_t key[32]);

#endif /* NP_RPMB_KEY_H */
