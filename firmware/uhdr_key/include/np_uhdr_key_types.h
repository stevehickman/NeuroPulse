/*
 * NeurOne Two-Layer UHDR Key Scheme — Status Types + Config Record
 * Document: NP-FW-EMMC-002 Rev A §C
 *
 * Convention (project-wide): NP_UHDR_OK = 0, all error codes negative.
 */

#ifndef NP_UHDR_KEY_TYPES_H
#define NP_UHDR_KEY_TYPES_H

#include <stdint.h>

typedef enum {
    NP_UHDR_OK           =  0,  /* Success                                     */
    NP_UHDR_ERR_TRNG     = -1,  /* TRNG failed to produce key/salt/nonce       */
    NP_UHDR_ERR_KDF      = -2,  /* Argon2id WKMD derivation failed             */
    NP_UHDR_ERR_CRYPTO   = -3,  /* AES-256-GCM wrap/unwrap operation failed    */
    NP_UHDR_ERR_AUTH     = -4,  /* GCM tag mismatch — wrong credential         */
    NP_UHDR_ERR_CONFIG   = -5,  /* Config partition read/write failed          */
    NP_UHDR_ERR_MOUNT    = -6,  /* UHDR partition XTS mount failed             */
    NP_UHDR_ERR_STATE    = -7,  /* Precondition violated (e.g. not unlocked)   */
    NP_UHDR_ERR_INVALID  = -8,  /* Invalid argument (null/length/range)        */
    NP_UHDR_ERR_PARAM    = -9   /* Stored KDF params below floor (tamper/rollback) */
} np_uhdr_status_t;

/*
 * Config partition UKMD record — NP-FW-EMMC-002 Rev A §C.3.
 *
 * Stored at Config offset NP_UHDR_UKMD_RECORD_OFFSET.  This is the ONLY
 * on-storage representation of the UHDR master key, and it is always the
 * AES-256-GCM ciphertext wrapped under the WKMD — the plaintext UKMD is never
 * written to any partition.
 *
 * FLEET-EXPORT PROHIBITION (privacy review Finding 4): this record is strictly
 * device-local.  None of its fields — ukmd_ciphertext, ukmd_nonce, ukmd_tag,
 * and especially argon2id_salt (a stable per-device pseudo-identifier) — may
 * ever be copied into SHDR, the fleet database, telemetry, or any diagnostic
 * export.  The SHDR schema CI gate (ci/test_shdr_schema.py, KEYMAT check)
 * enforces this at the database layer; keep this record out of every export
 * path at the firmware layer.
 *
 * The layout is byte-exact and packed so the on-flash image is stable across
 * toolchains: 32 + 12 + 16 + 32 + 4 + 4 + 4 + 4 + 84 = 192 bytes.
 *
 * SPEC DISCREPANCY (flagged — OI-UHDRK-08): NP-FW-EMMC-002 Rev A §C.3 prints
 * `reserved[44]` while its own comment states "Pad to 192 bytes".  Those two
 * are inconsistent: the named fields before `reserved` sum to 108 bytes, so
 * reaching the stated 192-byte record requires reserved = 84, not 44 (44 would
 * yield a 152-byte record).  We honour the explicit stated record size (192) —
 * the allocation intent for the Config slot at offset 0x1000 — and size
 * `reserved` to 84.  If the spec author confirms 152 was intended, shrink
 * `reserved` to 44 and set NP_UHDR_UKMD_RECORD_SIZE to 152; nothing reads the
 * reserved region, so either resolution is functionally safe.
 */
typedef struct __attribute__((packed)) {
    uint8_t  ukmd_ciphertext[32];   /* AES-256-GCM(UKMD, WKMD, nonce=ukmd_nonce) */
    uint8_t  ukmd_nonce[12];        /* AES-GCM nonce — random, unique per wrap    */
    uint8_t  ukmd_tag[16];          /* AES-GCM authentication tag                 */
    uint8_t  argon2id_salt[32];     /* Salt for WKMD derivation — device-unique   */
    uint32_t argon2id_version;      /* 0x13 = Argon2id version 1.3                */
    uint32_t argon2id_m_cost;       /* Memory: 65536 KiB (64 MiB)                 */
    uint32_t argon2id_t_cost;       /* Iterations: 4                              */
    uint32_t argon2id_parallelism;  /* p = 1                                      */
    uint8_t  reserved[84];          /* Pad to 192 bytes — future use (see above)  */
} np_ukmd_record_t;

#endif /* NP_UHDR_KEY_TYPES_H */
