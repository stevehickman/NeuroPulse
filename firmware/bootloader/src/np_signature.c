/*
 * NeurOne Bootloader — Ed25519 Firmware Signature Verification
 * Document: NP-FW-EMMC-001 Rev 1 §8.2
 *
 * Ed25519 verify is Monocypher 4.0.2's crypto_ed25519_check (RFC 8032
 * §5.1.7), behind a small-order public key guard — see the section below.
 *
 * Design principles for this security-critical module:
 *  - Constant-time comparison for the final verify step
 *  - All intermediate values are stack-allocated (no heap, no global state)
 *  - SHA-512 (inside Monocypher) is used per RFC 8032
 *  - SHA-256 is used for image integrity per NP-FW-EMMC-001 §8.2
 *
 * Signature payload (what is signed by the manufacturing root key):
 *   message = image_sha256 (32 bytes) || version_le32 (4 bytes) || size_le32 (4 bytes)
 *   sig = Ed25519-sign(private_key, message)
 *
 * Verification:
 *   Ed25519-verify(public_key, message, sig) → accept/reject
 *   + SHA-256(firmware_image) == header.image_sha256 → accept/reject
 *   + CRC32(header[0..15]) == header.header_crc32 → accept/reject
 */

#include "np_signature.h"
#include "np_config.h"
#include "monocypher-ed25519.h"   /* SOUP: Monocypher 4.0.2 (firmware/crypto/vendor) */
#include <string.h>

/* ── Manufacturing root public key (placeholder — replaced at secure build) ─ */
/* SECURITY NOTE: This placeholder key must be replaced with the production   */
/* manufacturing root key before any firmware release. Key injection occurs   */
/* at the secure build step, never in source control.                         */
/* NP_FW_PUBLIC_KEY_INIT exists ONLY so the host test can compile in a test   */
/* key it holds the private half of (tests/np_bootloader_test_key.h).  The     */
/* cross build defines nothing, so the placeholder below is what it links.     */
/*                                                                            */
/* The placeholder is NOT "y = 1", which is what it used to say: the encoding */
/* is little-endian, so a 0x01 in byte 31 is y = 2^248.  That value is not    */
/* the y of any curve point (checked 2026-09-26), so every signature fails to */
/* verify under it and an image built without key injection fails CLOSED.    */
/* Had it been y = 1 it would have been the identity, and forgeable — see    */
/* np_signature_key_is_small_order().                                         */
#ifndef NP_FW_PUBLIC_KEY_INIT
#define NP_FW_PUBLIC_KEY_INIT { \
    /* PLACEHOLDER — overwritten by build system from secure key store */ \
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, \
}
#endif
const uint8_t g_np_fw_public_key[NP_ED25519_PUBKEY_SIZE] = NP_FW_PUBLIC_KEY_INIT;

/* ─────────────────────────────────────────────────────────────────────────── */
/*                       CRC32 (IEEE 802.3 reflected)                         */
/* ─────────────────────────────────────────────────────────────────────────── */

uint32_t np_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++) {
            uint32_t mask = -(crc & 1U);
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

static inline uint32_t rotr32(uint32_t x, uint32_t n)
{
    return (x >> n) | (x << (32U - n));
}

static void sha256_block(uint32_t h[8], const uint8_t block[64])
{
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4U]      << 24U)
             | ((uint32_t)block[i * 4U + 1U] << 16U)
             | ((uint32_t)block[i * 4U + 2U] <<  8U)
             | ((uint32_t)block[i * 4U + 3U]);
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr32(w[i-15], 7U) ^ rotr32(w[i-15], 18U) ^ (w[i-15] >> 3U);
        uint32_t s1 = rotr32(w[i-2],  17U) ^ rotr32(w[i-2],  19U) ^ (w[i-2]  >> 10U);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }

    uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

    for (int i = 0; i < 64; i++) {
        uint32_t S1  = rotr32(e, 6U) ^ rotr32(e, 11U) ^ rotr32(e, 25U);
        uint32_t ch  = (e & f) ^ (~e & g);
        uint32_t t1  = hh + S1 + ch + K256[i] + w[i];
        uint32_t S0  = rotr32(a, 2U) ^ rotr32(a, 13U) ^ rotr32(a, 22U);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2  = S0 + maj;
        hh = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

void np_sha256(const uint8_t *data, size_t len, uint8_t out[NP_SHA256_SIZE])
{
    uint32_t h[8] = {
        0x6A09E667UL, 0xBB67AE85UL, 0x3C6EF372UL, 0xA54FF53AUL,
        0x510E527FUL, 0x9B05688CUL, 0x1F83D9ABUL, 0x5BE0CD19UL,
    };

    uint8_t  block[64];
    size_t   remaining = len;
    size_t   offset    = 0U;

    /* Process full 64-byte blocks */
    while (remaining >= 64U) {
        memcpy(block, data + offset, 64U);
        sha256_block(h, block);
        offset    += 64U;
        remaining -= 64U;
    }

    /* Final block(s): padding */
    uint8_t tail[128];
    memset(tail, 0, sizeof(tail));
    memcpy(tail, data + offset, remaining);
    tail[remaining] = 0x80U;

    size_t pad_blocks;
    if (remaining < 56U) {
        pad_blocks = 1U;
    } else {
        pad_blocks = 2U;
    }

    /* Length in bits, big-endian, at end of last block */
    uint64_t bit_len = (uint64_t)len * 8U;
    uint8_t *len_field = tail + (pad_blocks * 64U) - 8U;
    for (int i = 7; i >= 0; i--) {
        len_field[i] = (uint8_t)(bit_len & 0xFFU);
        bit_len >>= 8U;
    }

    for (size_t blk = 0U; blk < pad_blocks; blk++) {
        sha256_block(h, tail + blk * 64U);
    }

    /* Store result big-endian */
    for (int i = 0; i < 8; i++) {
        out[i * 4U]     = (uint8_t)(h[i] >> 24U);
        out[i * 4U + 1U] = (uint8_t)(h[i] >> 16U);
        out[i * 4U + 2U] = (uint8_t)(h[i] >>  8U);
        out[i * 4U + 3U] = (uint8_t)(h[i]);
    }
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*                       Ed25519 verification (RFC 8032 §5.1.7)               */
/* ─────────────────────────────────────────────────────────────────────────── */
/*
 * Delegated to Monocypher 4.0.2 (crypto_ed25519_check), the same vendored SOUP
 * the safety MCU verifies with (firmware/crypto/vendor/monocypher/VERSION).
 * NP-SW-CI-001 OI-SWCI-15, found while writing its first positive test.
 *
 * This file used to carry its own Ed25519, and it could not verify a valid
 * signature.  ge_scalarmult_base() added into the register that held the base
 * point, so S*B was never computed, and ge_p3_tobytes() multiplied by
 * Z^((p-5)/8) where it needed 1/Z ("close enough for sign").  Nothing ran it:
 * the record said "both paths are covered by the same RFC 8032 test vectors"
 * and no test linked this file.  A device built from it would refuse every
 * signed image and fall to DFU.  The stated reason for keeping a private copy,
 * that -nostdlib makes Monocypher's libc dependencies unavailable, was also
 * wrong: Monocypher includes <stddef.h> and <stdint.h> and calls nothing, and
 * the memcpy/memset the compiler may emit for it are np_mem.c's.
 */

/* Constant-time byte array comparison. Returns 0 if equal, nonzero otherwise.*/
static int ct_memcmp(const uint8_t *a, const uint8_t *b, size_t n)
{
    uint8_t diff = 0U;
    for (size_t i = 0U; i < n; i++) {
        diff |= a[i] ^ b[i];
    }
    return (int)diff;
}

/*
 * Small-order public keys.  crypto_ed25519_check() uses the cofactorless
 * equation, so under a key A of order dividing 8 a forger picks any S, sets
 * R = S*B, and the equation holds for every message whose H*A is the
 * identity — for the identity itself, every message.  A key-injection step
 * that wrote such a key (the identity is one flipped byte away from the
 * placeholder above) would make the bootloader accept forged firmware, so
 * these keys are refused outright.
 *
 * The seven y-encodings of the eight torsion points, canonical and not; the
 * sign bit (byte 31, bit 7) is masked off before comparison, which covers the
 * other seven.  Derived 2026-09-26 by decoding every candidate y and checking
 * [8]P == identity in exact integer arithmetic (14 encodings with both signs);
 * the set matches libsodium's has_small_order() blocklist.
 */
static const uint8_t k_small_order[7][32] = {
    /* y = 0 (order 4) */
    { 0x00 },
    /* y = 1, the identity */
    { 0x01 },
    /* order 8 */
    { 0x26, 0xE8, 0x95, 0x8F, 0xC2, 0xB2, 0x27, 0xB0, 0x45, 0xC3, 0xF4, 0x89,
      0xF2, 0xEF, 0x98, 0xF0, 0xD5, 0xDF, 0xAC, 0x05, 0xD3, 0xC6, 0x33, 0x39,
      0xB1, 0x38, 0x02, 0x88, 0x6D, 0x53, 0xFC, 0x05 },
    /* order 8 */
    { 0xC7, 0x17, 0x6A, 0x70, 0x3D, 0x4D, 0xD8, 0x4F, 0xBA, 0x3C, 0x0B, 0x76,
      0x0D, 0x10, 0x67, 0x0F, 0x2A, 0x20, 0x53, 0xFA, 0x2C, 0x39, 0xCC, 0xC6,
      0x4E, 0xC7, 0xFD, 0x77, 0x92, 0xAC, 0x03, 0x7A },
    /* y = p - 1 (order 2) */
    { 0xEC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F },
    /* y = p, non-canonical 0 */
    { 0xED, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F },
    /* y = p + 1, non-canonical 1 */
    { 0xEE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F },
};

int np_signature_key_is_small_order(const uint8_t pk[NP_ED25519_PUBKEY_SIZE])
{
    int hit = 0;
    for (size_t k = 0U; k < 7U; k++) {
        uint8_t diff = 0U;
        for (size_t i = 0U; i < 31U; i++) {
            diff |= pk[i] ^ k_small_order[k][i];
        }
        diff |= (uint8_t)((pk[31] & 0x7FU) ^ k_small_order[k][31]);
        hit |= (diff == 0U);
    }
    return hit;
}

/* Returns 0 on success, -1 on failure. */
static int ed25519_verify(const uint8_t pk[32], const uint8_t sig[64],
                          const uint8_t *msg, size_t mlen)
{
    if (np_signature_key_is_small_order(pk)) {
        return -1;
    }
    return crypto_ed25519_check(sig, pk, msg, mlen);
}

/* ─────────────────────────────────────────────────────────────────────────── */
/*                       Public verification API                               */
/* ─────────────────────────────────────────────────────────────────────────── */

np_status_t np_signature_verify(const np_image_header_t *header,
                                 const uint8_t *image_data)
{
    /* Step 1: Validate header CRC32 over bytes [0..15] (magic+version+size+crc)*/
    uint32_t expected_crc = np_crc32((const uint8_t *)header, 12U);
    if (expected_crc != header->header_crc32) {
        return NP_ERR_BAD_HEADER_CRC;
    }

    /* Step 2: Validate image magic */
    if (header->magic != NP_IMAGE_MAGIC) {
        return NP_ERR_BAD_MAGIC;
    }

    /* Step 3: Validate image size */
    if (header->image_size == 0U || header->image_size > NP_FW_MAX_SIZE) {
        return NP_ERR_IMAGE_TOO_LARGE;
    }

    /* Step 4: Compute SHA-256 of firmware image and compare to header.      */
    /* If image_data == NULL the caller has already verified the image hash  */
    /* via streaming SHA-256 (e.g., np_ota_verify_scratch). Skip recompute. */
    if (image_data != NULL) {
        uint8_t computed_hash[NP_SHA256_SIZE];
        np_sha256(image_data, header->image_size, computed_hash);
        if (ct_memcmp(computed_hash, header->image_sha256, NP_SHA256_SIZE) != 0) {
            return NP_ERR_BAD_IMAGE_HASH;
        }
    }

    /* Step 5: Verify Ed25519 signature over (sha256 || version_le32 || size_le32) */
    uint8_t msg[40];
    memcpy(msg,       header->image_sha256, NP_SHA256_SIZE);
    msg[32] = (uint8_t)(header->version);
    msg[33] = (uint8_t)(header->version >> 8U);
    msg[34] = (uint8_t)(header->version >> 16U);
    msg[35] = (uint8_t)(header->version >> 24U);
    msg[36] = (uint8_t)(header->image_size);
    msg[37] = (uint8_t)(header->image_size >> 8U);
    msg[38] = (uint8_t)(header->image_size >> 16U);
    msg[39] = (uint8_t)(header->image_size >> 24U);

    if (ed25519_verify(g_np_fw_public_key, header->signature, msg, 40U) != 0) {
        return NP_ERR_BAD_SIGNATURE;
    }

    return NP_OK;
}
