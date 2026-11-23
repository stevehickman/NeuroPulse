/*
 * NeurOne Two-Layer UHDR Key Scheme — Status Types + Config Record
 * Document: NP-FW-EMMC-002 Rev 1 §C
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
    NP_UHDR_ERR_PARAM    = -9,  /* Stored KDF params below floor (tamper/rollback) */
    NP_UHDR_ERR_LOCKOUT  = -10  /* Unlock back-off not yet elapsed (§C.7)      */
} np_uhdr_status_t;

/*
 * Config partition UKMD record — NP-FW-EMMC-002 Rev 1 §C.3.
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
 * toolchains: 64 + 12 + 16 + 32 + 4 + 4 + 4 + 4 + 52 = 192 bytes.
 *
 * 2026-11-23 (OI-PRIV-35, NP-PRIV-ANALYSIS-004 M9): `ukmd_ciphertext` grew from
 * 32 to 64 bytes because the UKMD is now the full XTS-AES-256 key K1 || K2
 * (NP-FW-EMMC-001 EMMC-UHDR-04).  The 32 bytes came out of `reserved`
 * (84 -> 52), so the record keeps its stated 192 bytes and its place in
 * Config.  No record has been minted on a device (the Config seams trap on
 * target), so no stored record has the old layout.
 *
 * SPEC DISCREPANCY (OI-UHDRK-08), as it stands after the change: NP-FW-EMMC-002
 * §C.3 printed `reserved[44]` beside "Pad to 192 bytes", which disagreed before
 * (108 + 44 = 152).  The code keeps the stated record size (192) and §C.3 now
 * prints the same layout as this struct.  If 152 had been meant, the record
 * would now need `reserved[12]`; nothing reads the reserved region, so either
 * resolution is functionally safe.
 */
typedef struct __attribute__((packed)) {
    uint8_t  ukmd_ciphertext[64];   /* AES-256-GCM(UKMD K1||K2, WKMD, ukmd_nonce)  */
    uint8_t  ukmd_nonce[12];        /* AES-GCM nonce — random, unique per wrap    */
    uint8_t  ukmd_tag[16];          /* AES-GCM authentication tag                 */
    uint8_t  argon2id_salt[32];     /* Salt for WKMD derivation — device-unique   */
    uint32_t argon2id_version;      /* 0x13 = Argon2id version 1.3                */
    uint32_t argon2id_m_cost;       /* Memory: 65536 KiB (64 MiB)                 */
    uint32_t argon2id_t_cost;       /* Iterations: 4                              */
    uint32_t argon2id_parallelism;  /* p = 1                                      */
    uint8_t  reserved[52];          /* Pad to 192 bytes — future use (see above)  */
} np_ukmd_record_t;

#endif /* NP_UHDR_KEY_TYPES_H */
