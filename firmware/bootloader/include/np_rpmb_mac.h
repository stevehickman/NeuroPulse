/*
 * NeurOne Bootloader — RPMB frame MAC, HAL seam (OI-PRIV-32; OI-SW02-01, NP-SW-001)
 *
 * The RPMB MAC is HMAC-SHA256 (RFC 2104 over FIPS 180-4 SHA-256) under the
 * 32-byte RPMB key, over the 284 bytes of a frame from its data field to its
 * end (np_emmc_rpmb.h). Message authentication is cryptographic under
 * CLAUDE.md §22, which puts cryptographic code in Rust in common/npps-core;
 * the bootloader is C. Whether this primitive is written in the bootloader's C
 * (beside its existing C SHA-256), linked as no_std Rust from npps-core, or
 * taken from the i.MX RT1062's DCP hash engine is a decision this change does
 * not make: OI-SW02-01, briefed in
 * docs/human-input/oi-sw02-01-rpmb-mac-in-the-bootloader.md.
 *
 * Contract for every implementation:
 *   - NP_OK only with the 32-byte HMAC-SHA256 of `msg` under `key` in `mac`.
 *   - Any other return is an error; the RPMB module then refuses the frame,
 *     and the floor store reports an unreadable floor (every image refused).
 *
 * The device build links np_rpmb_mac_unbuilt.c, which fails, until OI-SW02-01
 * is decided. Host tests link an HMAC built on np_signature.c's SHA-256 and
 * checked against RFC 4231's vectors.
 */
#ifndef NP_RPMB_MAC_H
#define NP_RPMB_MAC_H

#include <stddef.h>
#include <stdint.h>
#include "np_types.h"

np_status_t np_rpmb_hmac_sha256(const uint8_t key[32], const uint8_t *msg,
                                size_t len, uint8_t mac[32]);

#endif /* NP_RPMB_MAC_H */
