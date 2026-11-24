/*
 * NeurOne Bootloader — hardware entropy, HAL seam (OI-PRIV-32; OI-SW02-03, NP-SW-001)
 *
 * The RPMB request nonce (16 bytes) must be unpredictable: a nonce an attacker
 * can guess lets a response recorded earlier, carrying an older and lower
 * floor, be replayed against the request that reuses it. Supplying entropy is
 * the platform's part under CLAUDE.md §22, so this seam reads the hardware
 * generator and does nothing else to the bytes.
 *
 * Contract for every implementation:
 *   - NP_OK only with `len` bytes from the hardware true random number
 *     generator in `buf`.
 *   - Any other return is an error; the RPMB module sends no request, and the
 *     floor store reports an unreadable floor (every image refused).
 *
 * The bootloader has no TRNG driver (OI-SW02-03). The device build links
 * np_entropy_unbuilt.c, which fails, until it has one. Host tests link a fake.
 */
#ifndef NP_ENTROPY_H
#define NP_ENTROPY_H

#include <stddef.h>
#include <stdint.h>
#include "np_types.h"

np_status_t np_entropy_get(uint8_t *buf, size_t len);

#endif /* NP_ENTROPY_H */
