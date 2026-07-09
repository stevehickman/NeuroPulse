/*
 * NeurOne Two-Layer UHDR Key Scheme — Configuration Constants
 * Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
 * Document: NP-FW-EMMC-002 Rev A §C
 *
 * Two-layer architecture (§C.2):
 *   Layer 1 UKMD — 32-byte TRNG master key, used directly as the UHDR
 *                  AES-256-XTS key.  Stored in Config only as GCM ciphertext.
 *   Layer 2 WKMD — 32-byte Argon2id-derived wrapper key.  Encrypts the UKMD.
 *                  Lives in SRAM only for the duration of a wrap/unwrap and is
 *                  zeroed immediately after.
 *
 * A credential (biometric/PIN) change re-derives WKMD and re-wraps the 32-byte
 * UKMD; the ≤6.9 GiB UHDR partition ciphertext is never re-encrypted.
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

/* ── Argon2id parameters (§C.3 — identical to NP-FW-EMMC-001 Rev A §6) ────── */

#define NP_UHDR_ARGON2ID_VERSION       0x13U    /* Argon2 v1.3                   */
#define NP_UHDR_ARGON2ID_M_COST        65536U   /* 64 MiB, in KiB                */
#define NP_UHDR_ARGON2ID_T_COST        4U       /* iterations                    */
#define NP_UHDR_ARGON2ID_PARALLELISM   1U       /* p = 1                         */

/* ── Config partition placement (§C.3) ───────────────────────────────────── */

/* UKMD record lives at Config partition byte offset 0x1000.                    */
#define NP_UHDR_UKMD_RECORD_OFFSET  0x1000U

/* Byte-exact record size (see np_ukmd_record_t): asserted at compile time.     */
#define NP_UHDR_UKMD_RECORD_SIZE    192U

#endif /* NP_UHDR_KEY_CONFIG_H */
