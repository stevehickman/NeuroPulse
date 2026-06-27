/*
 * NeuroPulse — Shared Cryptographic Primitives
 * Document: NP-SW-001 §9.4
 *
 * Shared static library consumed by:
 *   • firmware/bootloader/     (SW-02 Class B — i.MX RT1062 Cortex-M7)
 *   • firmware/safety_mcu/     (SW-01 Class C — STM32G071 Cortex-M0+)
 *
 * Resolves OI-SW01-M07-02 (BLOCKING ARM link error):
 *   np_session_sig.c declared np_ed25519_verify extern but no object file
 *   in SAFETY_SOURCES provided the definition.  This library is the definition.
 *
 * IEC 62304 Class C — MISRA C:2012 mandatory for safety MCU consumers.
 * No CPU-specific flags; inherits them from the consuming project's CMakeLists.
 *
 * Algorithms:
 *   SHA-256   FIPS 180-4
 *   SHA-512   FIPS 180-4
 *   CRC-32    IEEE 802.3 (poly 0xEDB88320, reflected)
 *   Ed25519   RFC 8032 §5.1.7 verify-only (no key generation, no signing)
 *
 * SECURITY NOTE:
 *   The Ed25519 implementation contains no secret-dependent branches and
 *   uses a constant-time final comparison (ct_memcmp).  It is suitable for
 *   public-key verification but has NOT been audited against side-channel
 *   attacks.  No signing path is provided; the safety MCU never holds
 *   the manufacturing root private key.
 */

#ifndef NP_CRYPTO_H
#define NP_CRYPTO_H

#include <stdint.h>

/* ── Size constants ───────────────────────────────────────────────────────── */

#define NP_CRYPTO_SHA256_SIZE           32U
#define NP_CRYPTO_SHA512_SIZE           64U
#define NP_CRYPTO_ED25519_PUBKEY_SIZE   32U
#define NP_CRYPTO_ED25519_SIG_SIZE      64U

/*
 * Maximum message length accepted by np_ed25519_verify.
 * Session hash (safety MCU): 32 bytes.  Firmware header msg (bootloader): 40 bytes.
 * 256 provides margin without excessive stack allocation in the verifier.
 */
#define NP_CRYPTO_ED25519_MSG_MAX_LEN   256U

/* ── SHA-256 ──────────────────────────────────────────────────────────────── */

/*
 * Compute SHA-256 over `len` bytes of `data`.
 * Result written to `out[NP_CRYPTO_SHA256_SIZE]`.
 * When len == 0, data is not dereferenced.
 */
void np_sha256(const uint8_t *data, uint32_t len,
               uint8_t out[NP_CRYPTO_SHA256_SIZE]);

/* ── SHA-512 ──────────────────────────────────────────────────────────────── */

/*
 * Compute SHA-512 over `len` bytes of `data`.
 * Result written to `out[NP_CRYPTO_SHA512_SIZE]`.
 * When len == 0, data is not dereferenced.
 */
void np_sha512(const uint8_t *data, uint32_t len,
               uint8_t out[NP_CRYPTO_SHA512_SIZE]);

/* ── CRC-32 (IEEE 802.3) ──────────────────────────────────────────────────── */

/*
 * CRC-32 over `len` bytes of `data`.
 * Poly: 0xEDB88320 (reflected), init: 0xFFFFFFFF, final XOR: 0xFFFFFFFF.
 * CRC-32("") == 0x00000000.  CRC-32("123456789") == 0xCBF43926.
 */
uint32_t np_crc32(const uint8_t *data, uint32_t len);

/* ── Ed25519 verify (RFC 8032 §5.1.7) ────────────────────────────────────── */

/*
 * Verify an Ed25519 signature.
 *
 * Parameters (note order matches np_session_sig.c existing extern):
 *   pubkey   — 32-byte Ed25519 public key (curve point A encoded)
 *   msg      — message bytes; may be NULL when msg_len == 0
 *   msg_len  — message length in bytes; must be <= NP_CRYPTO_ED25519_MSG_MAX_LEN
 *   sig      — 64-byte signature (R || S, little-endian)
 *
 * Returns:
 *   0    — signature valid
 *  -1    — signature invalid or inputs out of range
 *
 * Constant-time: the final comparison uses ct_memcmp; no branch on secret.
 * Public-key operations are variable-time (standard for Ed25519 verify).
 */
int np_ed25519_verify(const uint8_t *pubkey,
                       const uint8_t *msg, uint32_t msg_len,
                       const uint8_t *sig);

#endif /* NP_CRYPTO_H */
