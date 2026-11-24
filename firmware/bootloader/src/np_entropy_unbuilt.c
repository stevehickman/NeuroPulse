/*
 * NeurOne Bootloader — hardware entropy, device build, NOT BUILT (OI-PRIV-32; OI-SW02-03, NP-SW-001)
 *
 * The bootloader has no driver for the i.MX RT1062's true random number
 * generator. Until it has one, no RPMB request nonce can be drawn, so the floor
 * is unreadable and np_rollback_admit() refuses every image (fail closed). It
 * is not replaced by a counter or a constant: a predictable nonce lets an old
 * response carrying a lower floor be replayed (np_entropy.h).
 */
#include "np_entropy.h"

np_status_t np_entropy_get(uint8_t *buf, size_t len)
{
    (void)buf; (void)len;
    return NP_ERR_ROLLBACK_STORE;
}
