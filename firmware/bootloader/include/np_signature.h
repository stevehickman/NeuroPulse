/*
 * NeurOne Bootloader — Ed25519 Firmware Signature Verification
 * Document: NP-FW-EMMC-001 Rev A §8.2 (Ed25519 + SHA-256 image integrity)
 *
 * All firmware images must be signed by the NeurOne manufacturing root key
 * before the bootloader will execute them.
 *
 * Signature covers:  Ed25519-sign(image_sha256 || version_le32 || size_le32)
 * Public key:        NP_FW_PUBLIC_KEY (embedded in bootloader at build time)
 */

#ifndef NP_SIGNATURE_H
#define NP_SIGNATURE_H

#include "np_types.h"

/* ── Manufacturing root public key ───────────────────────────────────────── */
/* 32-byte Ed25519 public key. Replace with production key before tooling.    */
/* This is a placeholder; the real key is injected at secure build time.      */
extern const uint8_t g_np_fw_public_key[NP_ED25519_PUBKEY_SIZE];

/* ── Public API ───────────────────────────────────────────────────────────── */

/* Verify the Ed25519 signature in `header` over the given image.             */
/*   header     - pointer to np_image_header_t at start of bank               */
/*   image_data - pointer to firmware bytes (immediately after header)        */
/* Returns NP_OK if signature and image SHA-256 both verify.                  */
/* Returns NP_ERR_BAD_HEADER_CRC, NP_ERR_BAD_SIGNATURE, or                   */
/*         NP_ERR_BAD_IMAGE_HASH on any failure.                              */
np_status_t np_signature_verify(const np_image_header_t *header,
                                const uint8_t *image_data);

/* Compute SHA-256 of `len` bytes at `data`, store 32-byte digest in `out`.  */
void np_sha256(const uint8_t *data, size_t len, uint8_t out[NP_SHA256_SIZE]);

/* Compute SHA-512 of `len` bytes at `data`, store 64-byte digest in `out`.  */
void np_sha512(const uint8_t *data, size_t len, uint8_t out[NP_SHA512_SIZE]);

/* CRC32 (IEEE 802.3) over `len` bytes starting at `data`.                   */
uint32_t np_crc32(const uint8_t *data, size_t len);

#endif /* NP_SIGNATURE_H */
