/*
 * NeuroPulse — Shared Cryptographic Primitives
 * Document: NP-SW-001 Rev A §4.1
 *
 * Resolves OI-SW01-M07-02: shared Ed25519 implementation for safety MCU and bootloader.
 *
 * Ed25519 verify backend: Monocypher 4.0.2 optional/monocypher-ed25519 (RFC 8032, SHA-512).
 * SHA-256, SHA-512, CRC-32 are implemented directly in this file (FIPS 180-4, IEEE 802.3).
 *
 * IEC 62304 Class C: consumed by SW-01 Safety MCU. MISRA C:2012 mandatory.
 * Public API uses uint32_t for all length parameters.
 */

#include "np_crypto.h"
#include "monocypher-ed25519.h"
#include <string.h>

/* ─────────────────────────────────────────────────────────────────────────── */
/*                       CRC32 (IEEE 802.3 reflected)                         */
/* ─────────────────────────────────────────────────────────────────────────── */

#define NP_CRC32_POLY 0xEDB88320UL

uint32_t np_crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t i;
    for (i = 0U; i < len; i++) {
        uint32_t j;
        crc ^= (uint32_t)data[i];
        for (j = 0U; j < 8U; j++) {
            uint32_t mask = (uint32_t)(-(int32_t)(crc & 1U));
            crc = (crc >> 1U) ^ (NP_CRC32_POLY & mask);
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*                              SHA-256                                        */
/* ─────────────────────────────────────────────────────────────────────────── */

static const uint32_t K256[64] = {
    0x428A2F98UL, 0x71374491UL, 0xB5C0FBCFUL, 0xE9B5DBA5UL,
    0x3956C25BUL, 0x59F111F1UL, 0x923F82A4UL, 0xAB1C5ED5UL,
    0xD807AA98UL, 0x12835B01UL, 0x243185BEUL, 0x550C7DC3UL,
    0x72BE5D74UL, 0x80DEB1FEUL, 0x9BDC06A7UL, 0xC19BF174UL,
    0xE49B69C1UL, 0xEFBE4786UL, 0x0FC19DC6UL, 0x240CA1CCUL,
    0x2DE92C6FUL, 0x4A7484AAUL, 0x5CB0A9DCUL, 0x76F988DAUL,
    0x983E5152UL, 0xA831C66DUL, 0xB00327C8UL, 0xBF597FC7UL,
    0xC6E00BF3UL, 0xD5A79147UL, 0x06CA6351UL, 0x14292967UL,
    0x27B70A85UL, 0x2E1B2138UL, 0x4D2C6DFCUL, 0x53380D13UL,
    0x650A7354UL, 0x766A0ABBUL, 0x81C2C92EUL, 0x92722C85UL,
    0xA2BFE8A1UL, 0xA81A664BUL, 0xC24B8B70UL, 0xC76C51A3UL,
    0xD192E819UL, 0xD6990624UL, 0xF40E3585UL, 0x106AA070UL,
    0x19A4C116UL, 0x1E376C08UL, 0x2748774CUL, 0x34B0BCB5UL,
    0x391C0CB3UL, 0x4ED8AA4AUL, 0x5B9CCA4FUL, 0x682E6FF3UL,
    0x748F82EEUL, 0x78A5636FUL, 0x84C87814UL, 0x8CC70208UL,
    0x90BEFFFAUL, 0xA4506CEBUL, 0xBEF9A3F7UL, 0xC67178F2UL,
};

static uint32_t rotr32(uint32_t x, uint32_t n)
{
    return (x >> n) | (x << (32U - n));
}

static void sha256_block(uint32_t h[8], const uint8_t block[64])
{
    uint32_t w[64];
    int i;
    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4]      << 24U)
             | ((uint32_t)block[i * 4 + 1]  << 16U)
             | ((uint32_t)block[i * 4 + 2]  <<  8U)
             | ((uint32_t)block[i * 4 + 3]);
    }
    for (i = 16; i < 64; i++) {
        uint32_t s0 = rotr32(w[i-15], 7U) ^ rotr32(w[i-15], 18U) ^ (w[i-15] >> 3U);
        uint32_t s1 = rotr32(w[i-2],  17U) ^ rotr32(w[i-2],  19U) ^ (w[i-2]  >> 10U);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }

    uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

    for (i = 0; i < 64; i++) {
        uint32_t S1  = rotr32(e, 6U) ^ rotr32(e, 11U) ^ rotr32(e, 25U);
        uint32_t ch  = (e & f) ^ (~e & g);
        uint32_t t1  = hh + S1 + ch + K256[i] + w[i];
        uint32_t S0  = rotr32(a, 2U) ^ rotr32(a, 13U) ^ rotr32(a, 22U);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2  = S0 + maj;
        hh = g; g = f; f = e; e = d + t1;
        d  = c; c = b; b = a; a = t1 + t2;
    }

    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

void np_sha256(const uint8_t *data, uint32_t len,
               uint8_t out[NP_CRYPTO_SHA256_SIZE])
{
    uint32_t h[8] = {
        0x6A09E667UL, 0xBB67AE85UL, 0x3C6EF372UL, 0xA54FF53AUL,
        0x510E527FUL, 0x9B05688CUL, 0x1F83D9ABUL, 0x5BE0CD19UL,
    };

    uint8_t  block[64];
    uint32_t remaining = len;
    uint32_t offset    = 0U;
    int      i;

    while (remaining >= 64U) {
        memcpy(block, data + offset, 64U);
        sha256_block(h, block);
        offset    += 64U;
        remaining -= 64U;
    }

    uint8_t tail[128];
    uint32_t pad_blocks;
    uint64_t bit_len;
    uint8_t *len_field;

    memset(tail, 0, sizeof(tail));
    memcpy(tail, data + offset, remaining);
    tail[remaining] = 0x80U;

    pad_blocks = (remaining < 56U) ? 1U : 2U;

    bit_len = (uint64_t)len * 8U;
    len_field = tail + (pad_blocks * 64U) - 8U;
    for (i = 7; i >= 0; i--) {
        len_field[i] = (uint8_t)(bit_len & 0xFFU);
        bit_len >>= 8U;
    }

    for (i = 0; i < (int)pad_blocks; i++) {
        sha256_block(h, tail + (uint32_t)i * 64U);
    }

    for (i = 0; i < 8; i++) {
        out[i * 4U]      = (uint8_t)(h[i] >> 24U);
        out[i * 4U + 1U] = (uint8_t)(h[i] >> 16U);
        out[i * 4U + 2U] = (uint8_t)(h[i] >>  8U);
        out[i * 4U + 3U] = (uint8_t)(h[i]);
    }
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*                              SHA-512                                        */
/* ─────────────────────────────────────────────────────────────────────────── */

static const uint64_t K512[80] = {
    0x428A2F98D728AE22ULL, 0x7137449123EF65CDULL, 0xB5C0FBCFEC4D3B2FULL,
    0xE9B5DBA58189DBBCULL, 0x3956C25BF348B538ULL, 0x59F111F1B605D019ULL,
    0x923F82A4AF194F9BULL, 0xAB1C5ED5DA6D8118ULL, 0xD807AA98A3030242ULL,
    0x12835B0145706FBEULL, 0x243185BE4EE4B28CULL, 0x550C7DC3D5FFB4E2ULL,
    0x72BE5D74F27B896FULL, 0x80DEB1FE3B1696B1ULL, 0x9BDC06A725C71235ULL,
    0xC19BF174CF692694ULL, 0xE49B69C19EF14AD2ULL, 0xEFBE4786384F25E3ULL,
    0x0FC19DC68B8CD5B5ULL, 0x240CA1CC77AC9C65ULL, 0x2DE92C6F592B0275ULL,
    0x4A7484AA6EA6E483ULL, 0x5CB0A9DCBD41FBD4ULL, 0x76F988DA831153B5ULL,
    0x983E5152EE66DFABULL, 0xA831C66D2DB43210ULL, 0xB00327C898FB213FULL,
    0xBF597FC7BEEF0EE4ULL, 0xC6E00BF33DA88FC2ULL, 0xD5A79147930AA725ULL,
    0x06CA6351E003826FULL, 0x142929670A0E6E70ULL, 0x27B70A8546D22FFCULL,
    0x2E1B21385C26C926ULL, 0x4D2C6DFC5AC42AEDULL, 0x53380D139D95B3DFULL,
    0x650A73548BAF63DEULL, 0x766A0ABB3C77B2A8ULL, 0x81C2C92E47EDAEE6ULL,
    0x92722C851482353BULL, 0xA2BFE8A14CF10364ULL, 0xA81A664BBC423001ULL,
    0xC24B8B70D0F89791ULL, 0xC76C51A30654BE30ULL, 0xD192E819D6EF5218ULL,
    0xD69906245565A910ULL, 0xF40E35855771202AULL, 0x106AA07032BBD1B8ULL,
    0x19A4C116B8D2D0C8ULL, 0x1E376C085141AB53ULL, 0x2748774CDF8EEB99ULL,
    0x34B0BCB5E19B48A8ULL, 0x391C0CB3C5C95A63ULL, 0x4ED8AA4AE3418ACBULL,
    0x5B9CCA4F7763E373ULL, 0x682E6FF3D6B2B8A3ULL, 0x748F82EE5DEFB2FCULL,
    0x78A5636F43172F60ULL, 0x84C87814A1F0AB72ULL, 0x8CC702081A6439ECULL,
    0x90BEFFFA23631E28ULL, 0xA4506CEBDE82BDE9ULL, 0xBEF9A3F7B2C67915ULL,
    0xC67178F2E372532BULL, 0xCA273ECEEA26619CULL, 0xD186B8C721C0C207ULL,
    0xEADA7DD6CDE0EB1EULL, 0xF57D4F7FEE6ED178ULL, 0x06F067AA72176FBAULL,
    0x0A637DC5A2C898A6ULL, 0x113F9804BEF90DAEULL, 0x1B710B35131C471BULL,
    0x28DB77F523047D84ULL, 0x32CAAB7B40C72493ULL, 0x3C9EBE0A15C9BEBCULL,
    0x431D67C49C100D4CULL, 0x4CC5D4BECB3E42B6ULL, 0x597F299CFC657E2AULL,
    0x5FCB6FAB3AD6FAECULL, 0x6C44198C4A475817ULL,
};

static uint64_t rotr64(uint64_t x, uint64_t n)
{
    return (x >> n) | (x << (64U - n));
}

static void sha512_block(uint64_t h[8], const uint8_t block[128])
{
    uint64_t w[80];
    int i;
    for (i = 0; i < 16; i++) {
        w[i] = ((uint64_t)block[i * 8]      << 56U)
             | ((uint64_t)block[i * 8 + 1]  << 48U)
             | ((uint64_t)block[i * 8 + 2]  << 40U)
             | ((uint64_t)block[i * 8 + 3]  << 32U)
             | ((uint64_t)block[i * 8 + 4]  << 24U)
             | ((uint64_t)block[i * 8 + 5]  << 16U)
             | ((uint64_t)block[i * 8 + 6]  <<  8U)
             | ((uint64_t)block[i * 8 + 7]);
    }
    for (i = 16; i < 80; i++) {
        uint64_t s0 = rotr64(w[i-15], 1U)  ^ rotr64(w[i-15], 8U)  ^ (w[i-15] >> 7U);
        uint64_t s1 = rotr64(w[i-2],  19U) ^ rotr64(w[i-2],  61U) ^ (w[i-2]  >> 6U);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }

    uint64_t a = h[0], b = h[1], c = h[2], d = h[3];
    uint64_t e = h[4], f = h[5], g = h[6], hh = h[7];

    for (i = 0; i < 80; i++) {
        uint64_t S1  = rotr64(e, 14U) ^ rotr64(e, 18U) ^ rotr64(e, 41U);
        uint64_t ch  = (e & f) ^ (~e & g);
        uint64_t t1  = hh + S1 + ch + K512[i] + w[i];
        uint64_t S0  = rotr64(a, 28U) ^ rotr64(a, 34U) ^ rotr64(a, 39U);
        uint64_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint64_t t2  = S0 + maj;
        hh = g; g = f; f = e; e = d + t1;
        d  = c; c = b; b = a; a = t1 + t2;
    }

    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

void np_sha512(const uint8_t *data, uint32_t len,
               uint8_t out[NP_CRYPTO_SHA512_SIZE])
{
    uint64_t h[8] = {
        0x6A09E667F3BCC908ULL, 0xBB67AE8584CAA73BULL,
        0x3C6EF372FE94F82BULL, 0xA54FF53A5F1D36F1ULL,
        0x510E527FADE682D1ULL, 0x9B05688C2B3E6C1FULL,
        0x1F83D9ABFB41BD6BULL, 0x5BE0CD19137E2179ULL,
    };

    uint8_t  block[128];
    uint32_t remaining = len;
    uint32_t offset    = 0U;
    int      i;

    while (remaining >= 128U) {
        memcpy(block, data + offset, 128U);
        sha512_block(h, block);
        offset    += 128U;
        remaining -= 128U;
    }

    uint8_t  tail[256];
    uint32_t pad_blocks;
    uint64_t bit_len;
    uint8_t *len_field;

    memset(tail, 0, sizeof(tail));
    memcpy(tail, data + offset, remaining);
    tail[remaining] = 0x80U;

    /* SHA-512 padding: 128-bit length at end; high 64 bits always 0 here */
    pad_blocks = (remaining < 112U) ? 1U : 2U;
    bit_len    = (uint64_t)len * 8U;
    len_field  = tail + (pad_blocks * 128U) - 8U;
    for (i = 7; i >= 0; i--) {
        len_field[i] = (uint8_t)(bit_len & 0xFFU);
        bit_len >>= 8U;
    }

    for (i = 0; i < (int)pad_blocks; i++) {
        sha512_block(h, tail + (uint32_t)i * 128U);
    }

    for (i = 0; i < 8; i++) {
        out[i * 8U + 0U] = (uint8_t)(h[i] >> 56U);
        out[i * 8U + 1U] = (uint8_t)(h[i] >> 48U);
        out[i * 8U + 2U] = (uint8_t)(h[i] >> 40U);
        out[i * 8U + 3U] = (uint8_t)(h[i] >> 32U);
        out[i * 8U + 4U] = (uint8_t)(h[i] >> 24U);
        out[i * 8U + 5U] = (uint8_t)(h[i] >> 16U);
        out[i * 8U + 6U] = (uint8_t)(h[i] >>  8U);
        out[i * 8U + 7U] = (uint8_t)(h[i]);
    }
}


/* ─────────────────────────────────────────────────────────────────────────── */
/*              Ed25519 verify (RFC 8032 §5.1.7)                              */
/*              Backend: Monocypher 4.0.2 optional/monocypher-ed25519         */
/* ─────────────────────────────────────────────────────────────────────────── */

/*
 * np_ed25519_verify — verify an Ed25519 signature.
 *
 * Delegates to crypto_ed25519_check (Monocypher 4.0.2, RFC 8032, SHA-512).
 * The 256-byte msg_len limit is enforced before the call to avoid unbounded
 * stack use inside monocypher-ed25519 on safety-MCU (36 KB SRAM).
 *
 * Returns 0 for a valid signature, -1 for invalid.
 */
int np_ed25519_verify(const uint8_t *pubkey,
                       const uint8_t *msg, uint32_t msg_len,
                       const uint8_t *sig)
{
    uint32_t i;
    uint32_t or_bytes = 0U;

    if (msg_len > NP_CRYPTO_ED25519_MSG_MAX_LEN) {
        return -1;
    }

    /*
     * Reject all-zero public key.  [0x00…00] decodes to a small-subgroup
     * point of order 4; with S=0 the verification equation degenerates and
     * accepts any message.  No valid manufacturing root key is low-order.
     */
    for (i = 0U; i < NP_CRYPTO_ED25519_PUBKEY_SIZE; i++) {
        or_bytes |= (uint32_t)pubkey[i];
    }
    if (or_bytes == 0U) {
        return -1;
    }

    /*
     * crypto_ed25519_check(sig, pk, msg, msg_size) — Monocypher parameter order.
     * Returns 0 for valid, -1 for invalid (same convention as np_ed25519_verify).
     */
    return crypto_ed25519_check(sig, pubkey, msg, (size_t)msg_len);
}
