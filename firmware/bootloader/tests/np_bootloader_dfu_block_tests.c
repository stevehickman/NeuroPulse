/*
 * NeurOne Bootloader — DFU download block length, host tests (OI-UINT16-01)
 *
 * np_dfu.c drives USB registers and cannot be built on a host; the two decisions that mattered are pure and live in
 * np_dfu_block.h: whether a DFU_DNLOAD wLength fits the receive buffer, and the dTD token built from it.
 */
#include <stdint.h>
#include <stdio.h>
#include "np_dfu_block.h"

static int g_fail;
#define CHECK(c, m) do { if (c) { printf("PASS: %s\n", m); } else { printf("FAIL: %s\n", m); g_fail++; } } while (0)

/* @verifies-none Bootloader host test of the DFU block bound; no requirement row names the bootloader USB path yet (OI-UINT16-01). */
static void test_block_length_bound(void)
{
    CHECK(np_dfu_block_len_ok(1U), "a 1-byte block fits");
    CHECK(np_dfu_block_len_ok((uint16_t)NP_DFU_TRANSFER_SIZE), "a block of exactly the transfer size fits");
    CHECK(!np_dfu_block_len_ok((uint16_t)(NP_DFU_TRANSFER_SIZE + 1U)), "one byte over the transfer size is refused");
    CHECK(!np_dfu_block_len_ok(0x8000U), "0x8000 is refused");
    CHECK(!np_dfu_block_len_ok(0xFFFFU), "65535 is refused, not clamped");
}

/* @verifies-none Bootloader host test of the DFU dTD token (OI-UINT16-01). */
static void test_token_does_not_overflow_signed_int(void)
{
    CHECK(np_dfu_dtd_token(0x1000U) == ((0x1000UL << 16U) | 0x80UL), "4096 bytes: length in bits 30:16, ACTIVE set");
    CHECK(np_dfu_dtd_token(0x8000U) == ((0x8000UL << 16U) | 0x80UL), "0x8000 is built in 32 bits, with no signed shift");
    CHECK(np_dfu_dtd_token(0xFFFFU) == ((0xFFFFUL << 16U) | 0x80UL), "65535 keeps all sixteen length bits");
}

int main(void)
{
    test_block_length_bound();
    test_token_does_not_overflow_signed_int();
    if (g_fail != 0) { printf("FAILED (%d)\n", g_fail); return 1; }
    printf("OK\n");
    return 0;
}
