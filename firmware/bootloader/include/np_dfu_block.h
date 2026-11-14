/*
 * NeurOne Bootloader — DFU download block length (OI-UINT16-01)
 *
 * A DFU_DNLOAD request carries its block length in wLength, a 16-bit field.  The block is received into a static
 * buffer of NP_DFU_TRANSFER_SIZE bytes, so a length above that would let the USB controller write past the buffer.
 * It is refused (the request is stalled), never clamped: a host that sent more than the transfer size is broken or
 * hostile, and receiving the first NP_DFU_TRANSFER_SIZE bytes of it would write a truncated block into the image.
 *
 * Both functions are pure, so a host test can check them; np_dfu.c itself drives USB registers and cannot be built
 * on a host.
 */
#ifndef NP_DFU_BLOCK_H
#define NP_DFU_BLOCK_H

#include <stdbool.h>
#include <stdint.h>
#include "np_config.h"

/* True when a block of wLength bytes fits the receive buffer. */
static inline bool np_dfu_block_len_ok(uint16_t wLength)
{
    return (uint32_t)wLength <= (uint32_t)NP_DFU_TRANSFER_SIZE;
}

/* The dTD token for a receive of len bytes: total-bytes field in bits 30:16 and the ACTIVE bit (7).  The length is
 * widened to 32 bits before the shift: a uint16_t promotes to signed int, and 0x8000 << 16 overflows it. */
static inline uint32_t np_dfu_dtd_token(uint16_t len)
{
    return ((uint32_t)len << 16U) | (1U << 7U);
}

#endif /* NP_DFU_BLOCK_H */
