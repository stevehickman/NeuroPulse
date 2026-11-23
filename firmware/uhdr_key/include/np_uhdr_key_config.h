/*
 * NeurOne Two-Layer UHDR Key Scheme — Configuration Constants
 * Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
 * Document: NP-FW-EMMC-002 Rev 1 §C
 *
 * Two-layer architecture (§C.2), hardened per the 2026-07-08 privacy review:
 *   Layer 1 UKMD — 32-byte TRNG master key, used directly as the UHDR
 *                  XTS key material (32 bytes is not XTS-AES-256: OI-PRIV-35,
 *                  below).  Stored in Config only as GCM ciphertext.
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

/*
 * Layer 1 master key length.  The UKMD is handed to the UHDR mount
 * (np_uhdr_hal_mount_uhdr) as its XTS key material.
 *
 * OPEN (OI-PRIV-35, NP-PRIV-ANALYSIS-004 Finding M9): 32 bytes is NOT an
 * AES-256-XTS key.  XTS-AES-256 takes 64 bytes (Key1 || Key2, IEEE Std 1619;
 * NIST SP 800-38E), so 32 bytes used directly is XTS-AES-128 or the
 * degenerate Key1 = Key2 case.  Whether to widen the UKMD to 64 bytes or to
 * derive two independent XTS keys from it is the cryptography reviewer's call
 * (OI-EMMC2-02; brief docs/human-input/oi-priv-35-uhdr-xts-key-and-argon2id.md).
 * Nothing on target consumes the key yet: np_uhdr_hal_mount_uhdr traps.
 */
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

/* Largest AAD any wrap in this module authenticates: the profile record's
 * wrap 1 binds profile id (16) + salt (32) + the four KDF words (16) = 64
 * (NP-FW-EMMC-002 §C.8).  The §C.3 UKMD wrap uses NP_UHDR_AAD_LEN (48). */
#define NP_UHDR_AAD_MAX_LEN        64U

/* ── Argon2id parameters (§C.3 — identical to NP-FW-EMMC-001 Rev 1 §6) ────── */

/*
 * OPEN (OI-PRIV-35, Finding M10): 64 MiB of Argon2id memory exceeds the whole
 * 32 MB external SDRAM of the i.MX RT1062 board (docs/reference/hardware-detail.md
 * §4.1), so this value cannot run on target as written.  It has never shipped
 * and no record has been minted under it on a device.  It must be settled
 * against the memory map BEFORE any device is provisioned, because once a
 * record exists the floor below forbids lowering it (CLAUDE.md §5.1, protection
 * only goes up).  Brief: docs/human-input/oi-priv-35-uhdr-xts-key-and-argon2id.md.
 */
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

/* ── Unlock attempt limit (§C.7, REQ-EMMC2-01; OI-PRIV-35 Finding M10) ────────
 *
 * Every unlock attempt that reaches the KDF is charged to a failed-attempt
 * count persisted in Config ("auth lockout state", NP-FW-NVRAM-001 §3) BEFORE
 * the credential is tested, so cutting power mid-attempt cannot skip the
 * charge.  A successful authentication resets the count to zero.  Once the
 * count reaches NP_UHDR_UNLOCK_FREE_ATTEMPTS, the next attempt is refused
 * (NP_UHDR_ERR_LOCKOUT, no KDF run, nothing charged) until
 *     wait(n) = min(BASE << (n - FREE), MAX)   milliseconds
 * of monotonic uptime have passed since the later of boot and the last
 * attempt.  The device has no RTC backup (CLAUDE.md §4.5), so the wait is
 * counted in uptime: a power cycle restarts the wait rather than ending it.
 *
 * PROVISIONAL (UC-203): the three figures below are choices, not derivations.
 * Nothing derives 5, 30 s or 24 h; NIST SP 800-63B-3 §5.2.2 bounds consecutive
 * failures (at most 100) but does not state these values.  They make an online
 * guess of a short PIN slow; they do not set its security level.  The
 * cryptography reviewer (OI-EMMC2-02) is asked to confirm or replace them.
 * Fail-safe direction: lowering FREE or raising BASE / MAX only strengthens.
 */
#define NP_UHDR_UNLOCK_FREE_ATTEMPTS     5U          /* attempts with no wait     */
#define NP_UHDR_UNLOCK_BACKOFF_BASE_MS   30000U      /* first wait: 30 s          */
#define NP_UHDR_UNLOCK_BACKOFF_MAX_MS    86400000U   /* longest wait: 24 h        */

/* ── Config partition placement (§C.3) ───────────────────────────────────── */

/* UKMD record lives at Config partition byte offset 0x1000.                    */
#define NP_UHDR_UKMD_RECORD_OFFSET  0x1000U

/* Byte-exact record size (see np_ukmd_record_t): asserted at compile time.     */
#define NP_UHDR_UKMD_RECORD_SIZE    192U

#endif /* NP_UHDR_KEY_CONFIG_H */
