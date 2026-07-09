/*
 * NeurOne Two-Layer UHDR Key Scheme — Configuration Constants
 * Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
 * Document: NP-FW-EMMC-002 Rev A §C
 *
 * Two-layer architecture (§C.2), hardened per the 2026-07-08 privacy review:
 *   Layer 1 UKMD — 32-byte TRNG master key, used directly as the UHDR
 *                  AES-256-XTS key.  Stored in Config only as GCM ciphertext.
 *   Layer 2 WKMD — 32-byte wrapper key derived from BOTH the credential
 *                  (Argon2id) AND a device-fused, non-extractable hardware key
 *                  (CAAM/OTPMK HUK, HAL OI-UHDRK-09).  Encrypts the UKMD.
 *                  Lives in SRAM only for the duration of a wrap/unwrap and is
 *                  zeroed immediately after.
 *
 * A credential (biometric/PIN) change re-derives WKMD and re-wraps the 32-byte
 * UKMD; the ≤6.9 GiB UHDR partition ciphertext is never re-encrypted.
 *
 * Hardware binding (privacy review Finding 1): mixing the device-fused key into
 * WKMD means an attacker who images the eMMC CANNOT brute-force the credential
 * offline — the Argon2id work is worthless without the specific device's fused
 * key.  This is what defeats the PIN-fallback (§C.6) offline-attack vector.
 */

#ifndef NP_UHDR_KEY_CONFIG_H
#define NP_UHDR_KEY_CONFIG_H

#include <stdint.h>

/* ── Key / crypto material lengths (§C.2, §C.3) ──────────────────────────── */

/* Layer 1 master key — also the AES-256-XTS key length for the UHDR partition. */
#define NP_UHDR_UKMD_LEN            32U

/* Layer 2 wrapper key derived from the credential via Argon2id.                */
#define NP_UHDR_WKMD_LEN            32U

/* AES-256-GCM nonce and tag lengths for the UKMD wrap.                         */
#define NP_UHDR_GCM_NONCE_LEN       12U
#define NP_UHDR_GCM_TAG_LEN         16U

/* Argon2id salt length — device-unique, TRNG-generated at provisioning.        */
#define NP_UHDR_SALT_LEN            32U

/* Device-fused hardware key length (CAAM/OTPMK HUK), mixed into WKMD.           */
#define NP_UHDR_HWKEY_LEN           32U

/*
 * Additional Authenticated Data length for the UKMD wrap (privacy review
 * Finding 2): the record's salt + the four Argon2id parameter words are bound
 * into the GCM tag as AAD so a tampered record fails authentication rather than
 * silently pinning weaker KDF settings.  32 (salt) + 4×4 (version/m/t/p) = 48.
 */
#define NP_UHDR_AAD_LEN            (NP_UHDR_SALT_LEN + 16U)

/* ── Argon2id parameters (§C.3 — identical to NP-FW-EMMC-001 Rev A §6) ────── */

#define NP_UHDR_ARGON2ID_VERSION       0x13U    /* Argon2 v1.3                   */
#define NP_UHDR_ARGON2ID_M_COST        65536U   /* 64 MiB, in KiB                */
#define NP_UHDR_ARGON2ID_T_COST        4U       /* iterations                    */
#define NP_UHDR_ARGON2ID_PARALLELISM   1U       /* p = 1                         */

/*
 * Minimum acceptable KDF parameters (privacy review Finding 2).  unlock and
 * change_credential REJECT a stored record whose parameters fall below these
 * floors, so a rolled-back or corrupted record cannot weaken the KDF.  The
 * floors equal the provisioning constants above (the only version we mint).
 */
#define NP_UHDR_ARGON2ID_M_COST_MIN    NP_UHDR_ARGON2ID_M_COST
#define NP_UHDR_ARGON2ID_T_COST_MIN    NP_UHDR_ARGON2ID_T_COST

/* ── Config partition placement (§C.3) ───────────────────────────────────── */

/* UKMD record lives at Config partition byte offset 0x1000.                    */
#define NP_UHDR_UKMD_RECORD_OFFSET  0x1000U

/* Byte-exact record size (see np_ukmd_record_t): asserted at compile time.     */
#define NP_UHDR_UKMD_RECORD_SIZE    192U

#endif /* NP_UHDR_KEY_CONFIG_H */
