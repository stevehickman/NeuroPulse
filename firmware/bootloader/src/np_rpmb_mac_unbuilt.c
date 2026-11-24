/*
 * NeurOne Bootloader — RPMB MAC, device build, NOT BUILT (OI-PRIV-32; OI-SW02-01, NP-SW-001)
 *
 * HMAC-SHA256 is cryptographic under CLAUDE.md §22, and whether the bootloader
 * computes it in C, in no_std Rust from common/npps-core, or on the DCP hash
 * engine is the decision OI-SW02-01 asks for
 * (docs/human-input/oi-sw02-01-rpmb-mac-in-the-bootloader.md).
 *
 * Until it is made, no RPMB frame can be authenticated, so the floor is
 * unreadable and np_rollback_admit() refuses every image (fail closed). It is
 * not replaced by a MAC that accepts every frame.
 */
#include "np_rpmb_mac.h"

np_status_t np_rpmb_hmac_sha256(const uint8_t key[32], const uint8_t *msg,
                                size_t len, uint8_t mac[32])
{
    (void)key; (void)msg; (void)len; (void)mac;
    return NP_ERR_ROLLBACK_STORE;
}
