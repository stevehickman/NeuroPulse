/*
 * NeurOne OTA — State structure, CRC validation, and boot-attempt logic.
 * Document: NP-FW-EMMC-001 Rev A §8.3 (dual-bank OTA state record)
 *
 * This module is host-testable (no hardware dependencies).
 * The state record is persisted to the Config partition by np_ota_stage_swap()
 * and consumed by np_ota_handle_boot_verification() in the bootloader.
 */

#ifndef NP_OTA_STATE_H
#define NP_OTA_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Constants ───────────────────────────────────────────────────────────── */

#define NP_OTA_STATE_MAGIC       0x4E504F54UL  /* "NPOT" */
#define NP_OTA_SHA256_SIZE       32U
#define NP_OTA_BOOT_MAX_ATTEMPTS 3U
#define NP_OTA_CRC32_POLY        0xEDB88320UL  /* IEEE 802.3 reflected */

/* Byte offset at which the CRC field begins in np_ota_state_t. */
/* CRC32 is computed over bytes [0 .. NP_OTA_CRC_OFFSET - 1]. */
#define NP_OTA_CRC_OFFSET        40U  /* magic(4)+bank(1)+attempts(1)+rsvd(2)+sha256(32) */

/* ── State record ────────────────────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint32_t magic;                          /* NP_OTA_STATE_MAGIC               */
    uint8_t  target_bank;                    /* 0 = Bank A, 1 = Bank B           */
    uint8_t  boot_attempts;                  /* Incremented per boot of candidate */
    uint16_t reserved;
    uint8_t  staged_sha256[NP_OTA_SHA256_SIZE]; /* SHA-256 of the staged image   */
    uint32_t crc32;                          /* CRC32 over bytes [0..39]         */
} np_ota_state_t;

/* Compile-time layout check */
typedef char _np_ota_state_size_check[
    (sizeof(np_ota_state_t) == NP_OTA_CRC_OFFSET + 4U) ? 1 : -1];

/* ── Validation result ───────────────────────────────────────────────────── */

typedef enum {
    NP_OTA_STATE_OK               =  0,
    NP_OTA_STATE_BAD_MAGIC        = -1,
    NP_OTA_STATE_BAD_CRC          = -2,
    NP_OTA_STATE_MAX_ATTEMPTS     = -3,  /* boot_attempts >= NP_OTA_BOOT_MAX_ATTEMPTS */
    NP_OTA_STATE_INVALID_BANK     = -4,
} np_ota_state_result_t;

/* ── Public API ──────────────────────────────────────────────────────────── */

/*
 * Initialize a state record with valid magic and CRC.
 * target_bank: 0 = Bank A, 1 = Bank B.
 * sha256: 32-byte hash of the staged firmware image.
 */
void np_ota_state_init(np_ota_state_t       *state,
                       uint8_t               target_bank,
                       const uint8_t         sha256[NP_OTA_SHA256_SIZE]);

/*
 * Validate a state record read from persistent storage.
 * Returns NP_OTA_STATE_OK on success, or a negative error code.
 * NP_OTA_STATE_MAX_ATTEMPTS is returned when boot_attempts >= NP_OTA_BOOT_MAX_ATTEMPTS
 * even if the CRC and magic are valid — the caller should trigger rollback.
 */
np_ota_state_result_t np_ota_state_validate(const np_ota_state_t *state);

/*
 * Increment boot_attempts and recompute CRC.
 * Returns true if the updated attempt count is still below the limit,
 * or false if rollback is needed (attempts would reach the limit).
 */
bool np_ota_state_increment_attempts(np_ota_state_t *state);

/*
 * Compute CRC32 over the first NP_OTA_CRC_OFFSET bytes.
 * Exposed for tests and for callers that need to compute the expected CRC.
 */
uint32_t np_ota_state_crc32(const np_ota_state_t *state);

#endif /* NP_OTA_STATE_H */
