/*
 * NeurOne Two-Layer UHDR Key Scheme — Public API and Platform HAL
 * Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
 * Document: NP-FW-EMMC-002 Rev 1 §C
 *
 * Implements the two-layer key lifecycle:
 *
 *   provision          — first activation: mint UKMD + salt, wrap UKMD under
 *                        the initial credential's WKMD, persist the Config
 *                        record, leave the partition unlocked (§C.2).
 *   unlock             — app foreground: credential → WKMD → decrypt UKMD →
 *                        mount UHDR XTS → zero WKMD (§C.4).
 *   background         — app to background: zero UKMD from SRAM (§C.4 step 7).
 *   change_credential  — biometric/PIN change: re-wrap the SAME UKMD under a
 *                        new WKMD; UHDR ciphertext untouched (§C.5).
 *
 * The credential bytes passed in are whatever the app derives from the user's
 * biometric/PIN and delivers over the secure USB-C transport (§C.4 step 2);
 * this module's boundary starts at those bytes.  PIN fallback (§C.6) is the
 * same path with the PIN as the credential — there is no separate PIN key.
 *
 * Hardening applied 2026-07-08 (privacy review):
 *   - Finding 1 (hardware binding): WKMD is derived from the credential
 *     (Argon2id) AND a device-fused, non-extractable hardware key (OI-UHDRK-09)
 *     so an eMMC image cannot be brute-forced offline.
 *   - Finding 2 (record integrity): the salt + KDF parameters are bound into
 *     the GCM tag as AAD, and unlock/change_credential validate the stored
 *     parameters against NP_UHDR_ARGON2ID_*_MIN before use.
 *   - Finding 3 (secret residency): every unlock failure path scrubs the UKMD.
 *
 * SRAM-secret hygiene:
 *   - UKMD lives in the caller-owned context ONLY while unlocked.
 *   - WKMD and the intermediate credential-key are transient function-locals;
 *     they are NEVER stored in the context and are zeroed with memset_explicit
 *     before every return.
 *   - No heap.  The caller owns the context (stack or static instance).
 */

#ifndef NP_UHDR_KEY_H
#define NP_UHDR_KEY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "np_uhdr_key_types.h"
#include "np_uhdr_key_config.h"

/* ── Unlock context ───────────────────────────────────────────────────────────
 *
 * `ukmd` is an SRAM-only secret: valid only while `unlocked` is true, zeroed by
 * np_uhdr_key_background().  It MUST NOT be written to any eMMC partition,
 * logged, or copied to non-volatile storage.  WKMD deliberately has no home
 * here — it never outlives the function that derives it.
 */
typedef struct {
    uint8_t ukmd[NP_UHDR_UKMD_LEN];   /* Layer-1 master key — SRAM only        */
    bool    unlocked;                 /* true when ukmd is live and UHDR mounted */
} np_uhdr_key_ctx_t;

/* ── Lifecycle ─────────────────────────────────────────────────────────────── */

/*
 * np_uhdr_key_provision
 *   First-activation provisioning.  Generates a fresh 64-byte UKMD (K1 || K2,
 *   refused with NP_UHDR_ERR_TRNG if the halves are equal), Argon2id salt, and
 *   GCM nonce from the TRNG; derives WKMD from `credential`; GCM-wraps the UKMD;
 *   writes the Config UKMD record; mounts the UHDR partition; and leaves the
 *   context unlocked with the UKMD live.  WKMD is zeroed before return.
 *
 *   On any failure all key material (UKMD, WKMD) is zeroized, the context is
 *   left locked, and the specific error is returned.
 *
 *   Returns NP_UHDR_ERR_INVALID if ctx/credential is NULL or cred_len is 0.
 */
np_uhdr_status_t np_uhdr_key_provision(np_uhdr_key_ctx_t *ctx,
                                       const uint8_t *credential,
                                       size_t cred_len);

/*
 * np_uhdr_key_unlock
 *   Foreground unlock (§C.4).  Reads the Config UKMD record, derives WKMD from
 *   `credential`, GCM-decrypts and authenticates the UKMD, mounts the UHDR
 *   partition with the 64-byte UKMD as its XTS-AES-256 key (K1 || K2), zeros
 *   WKMD, and marks the context unlocked.
 *
 *   A wrong credential yields a GCM tag mismatch: the UKMD buffer is zeroized,
 *   the context stays locked, and NP_UHDR_ERR_AUTH is returned.
 *
 *   Attempt limit (§C.7, REQ-EMMC2-01): the attempt is charged to the persisted
 *   failed-attempt count before the KDF runs, and a successful authentication
 *   resets it.  While the back-off after NP_UHDR_UNLOCK_FREE_ATTEMPTS failures
 *   has not elapsed the call returns NP_UHDR_ERR_LOCKOUT without running the
 *   KDF and without charging.  If the count cannot be read or the charge cannot
 *   be persisted the attempt is refused with NP_UHDR_ERR_CONFIG (fail closed).
 *
 *   A decrypted key whose halves are equal is never mounted: the UKMD is
 *   zeroized and NP_UHDR_ERR_CRYPTO returned (REQ-EMMC2-06).
 *
 *   Returns NP_UHDR_ERR_INVALID if ctx/credential is NULL or cred_len is 0.
 */
np_uhdr_status_t np_uhdr_key_unlock(np_uhdr_key_ctx_t *ctx,
                                    const uint8_t *credential,
                                    size_t cred_len);

/*
 * np_uhdr_key_unlock_wait_ms
 *   How long, in milliseconds of uptime, until np_uhdr_key_unlock will next
 *   test a credential (§C.7).  0 means an attempt is allowed now.  Reads the
 *   persisted count; changes nothing.  Lets the app say "try again in ..."
 *   instead of offering an attempt the device will refuse.
 *   Returns NP_UHDR_ERR_INVALID if wait_ms is NULL, NP_UHDR_ERR_CONFIG if the
 *   count cannot be read.
 */
np_uhdr_status_t np_uhdr_key_unlock_wait_ms(uint64_t *wait_ms);

/*
 * np_uhdr_key_background
 *   Called when the app moves to background (§C.4 step 7).  Zeros the UKMD from
 *   SRAM with memset_explicit and clears `unlocked`.  Idempotent; NULL-safe.
 *   Note (§C.4 step 7): UKMD is cleared on background, NOT on session end — the
 *   session log write happens while still foregrounded/unlocked.
 */
void np_uhdr_key_background(np_uhdr_key_ctx_t *ctx);

/*
 * np_uhdr_key_change_credential
 *   Biometric/PIN change (§C.5).  Requires the context to be currently unlocked
 *   (UKMD live in SRAM).  Derives a new WKMD from `new_credential`, generates a
 *   fresh nonce, re-wraps the SAME UKMD, and atomically updates the Config UKMD
 *   record (LittleFS transaction — modeled as one atomic HAL write).  The UHDR
 *   partition ciphertext is NOT re-encrypted.  New WKMD is zeroed before return.
 *
 *   The context stays unlocked with the same UKMD on success.
 *
 *   Returns NP_UHDR_ERR_STATE   if the context is not unlocked.
 *   Returns NP_UHDR_ERR_INVALID if ctx/new_credential is NULL or len is 0.
 */
np_uhdr_status_t np_uhdr_key_change_credential(np_uhdr_key_ctx_t *ctx,
                                               const uint8_t *new_credential,
                                               size_t new_cred_len);

/* ── Platform HAL stubs ──────────────────────────────────────────────────────
 *
 * Implemented by the platform integration layer.  NPTEST_HOST builds provide
 * stubs in np_uhdr_key.c: TRNG is deterministic; Argon2id and AES-GCM are
 * modeled with np_crypto SHA-256 so the wrap authenticates (a wrong credential
 * produces a wrong WKMD → wrong tag → NP_UHDR_ERR_AUTH).  On target these are
 * backed by the RNGB, the Argon2id implementation, and the CAAM/DCP AES engine.
 */

/* OI-UHDRK-01: fill buf with len bytes from the hardware TRNG (RNGB).          */
extern np_uhdr_status_t np_uhdr_hal_trng_generate(uint8_t *buf, size_t len);

/*
 * OI-UHDRK-02: Argon2id KDF.  Derive a NP_UHDR_WKMD_LEN-byte credential key from
 * the credential and salt with the given algorithm `version` (e.g. 0x13 =
 * Argon2 v1.3) and cost parameters (m/t/p).  The caller passes the record's
 * stored version so the KDF is pinned to the version the record was wrapped
 * under — never an implementation default.  `out` is exactly NP_UHDR_WKMD_LEN
 * bytes.  This output is the credential-derived INTERMEDIATE — it is combined
 * with the device-fused key (OI-UHDRK-09) to form the actual WKMD; it is NOT
 * used as a wrapping key on its own.
 */
extern np_uhdr_status_t np_uhdr_hal_argon2id(const uint8_t *credential,
                                             size_t cred_len,
                                             const uint8_t *salt,
                                             uint32_t version,
                                             uint32_t m_cost,
                                             uint32_t t_cost,
                                             uint32_t parallelism,
                                             uint8_t *out /* [NP_UHDR_WKMD_LEN] */);

/*
 * OI-UHDRK-09: device-hardware binding (privacy review Finding 1).  Combine the
 * credential-derived intermediate key `ckey` with a device-fused,
 * non-extractable hardware key (i.MX RT1062 CAAM/OTPMK-derived HUK) into the
 * final NP_UHDR_WKMD_LEN-byte WKMD.  On target this MUST use a key that never
 * leaves the silicon and cannot be read by software, so that WKMD is
 * unreproducible off the originating device — defeating offline brute-force of
 * an eMMC image.  A pure-software combine (e.g. a bare HKDF with a key readable
 * from software) does NOT satisfy this contract.
 */
extern np_uhdr_status_t np_uhdr_hal_hw_bind(const uint8_t *ckey /* [NP_UHDR_WKMD_LEN] */,
                                            uint8_t *wkmd_out /* [NP_UHDR_WKMD_LEN] */);

/*
 * OI-UHDRK-03: AES-256-GCM encrypt.  Wrap `pt_len` bytes of plaintext (the
 * UKMD) under `key` (WKMD) with `nonce`, authenticating `aad_len` bytes of
 * additional data `aad` (the record salt + KDF parameters), producing `ct`
 * (pt_len bytes) and the authentication `tag` (NP_UHDR_GCM_TAG_LEN bytes).
 * `aad` may be NULL only when aad_len is 0.
 */
extern np_uhdr_status_t np_uhdr_hal_aes_gcm_encrypt(const uint8_t *key,
                                                    const uint8_t *nonce,
                                                    const uint8_t *aad,
                                                    size_t aad_len,
                                                    const uint8_t *pt,
                                                    size_t pt_len,
                                                    uint8_t *ct,
                                                    uint8_t *tag);

/*
 * OI-UHDRK-04: AES-256-GCM decrypt + authenticate.  Verify `tag` over
 * (`key`, `nonce`, `aad`, `ct`); on mismatch return NP_UHDR_ERR_AUTH WITHOUT
 * writing authentic plaintext.  On success write `ct_len` bytes of plaintext to
 * `pt`.  Any change to the AAD (tampered salt/params) fails authentication.
 */
extern np_uhdr_status_t np_uhdr_hal_aes_gcm_decrypt(const uint8_t *key,
                                                    const uint8_t *nonce,
                                                    const uint8_t *aad,
                                                    size_t aad_len,
                                                    const uint8_t *ct,
                                                    size_t ct_len,
                                                    const uint8_t *tag,
                                                    uint8_t *pt);

/* OI-UHDRK-05: read the 192-byte UKMD record from Config offset 0x1000.        */
extern np_uhdr_status_t np_uhdr_hal_config_read_ukmd(np_ukmd_record_t *out);

/*
 * OI-UHDRK-06: atomically write the 192-byte UKMD record to Config offset
 * 0x1000 (LittleFS transaction — old record survives a power loss until the
 * new one is fully committed).
 */
extern np_uhdr_status_t np_uhdr_hal_config_write_ukmd(const np_ukmd_record_t *rec);

/*
 * OI-UHDRK-07: mount the UHDR partition with `ukmd` as its XTS-AES-256 key:
 * NP_UHDR_UKMD_LEN (64) bytes, K1 = ukmd[0..31] for the data and
 * K2 = ukmd[32..63] for the tweak (IEEE Std 1619; NP-FW-EMMC-001 EMMC-UHDR-04,
 * -05).  Called with the UKMD — NEVER the WKMD — and never with K1 == K2
 * (REQ-EMMC2-06).  The cipher must be real AES-256: NP-FW-EMMC-001 EMMC-UHDR-06's
 * "AES-256 with two-key 128-bit halves" from the AES-128 DCP is not that
 * (OI-PRIV-35, NP-PRIV-ANALYSIS-004), so the implementation uses the CAAM's
 * AES-256 or the vetted software library EMMC-UHDR-06 allows.
 */
extern np_uhdr_status_t np_uhdr_hal_mount_uhdr(const uint8_t *ukmd /* [NP_UHDR_UKMD_LEN] */);

/*
 * OI-PRIV-35 (NP-PRIV-ANALYSIS-004): read the persisted failed-unlock count
 * ("auth lockout state", NP-FW-NVRAM-001 §3, Config partition).  Device-local:
 * never in SHDR, never uploaded.  A missing or unreadable value is an error,
 * never a zero, so a deleted count cannot reset the back-off.
 */
extern np_uhdr_status_t np_uhdr_hal_attempts_read(uint32_t *failed);

/*
 * OI-PRIV-35 (NP-PRIV-ANALYSIS-004): atomically persist the failed-unlock
 * count (LittleFS transaction; Config is on the EMMC-CFG-02 runtime-write
 * whitelist as "lockout state").  Returns NP_UHDR_OK only once committed.
 */
extern np_uhdr_status_t np_uhdr_hal_attempts_write(uint32_t failed);

/*
 * OI-PRIV-35 (NP-PRIV-ANALYSIS-004): monotonic milliseconds since boot.  Must
 * never go backwards within a boot.  Not wall-clock time: the device has no
 * RTC backup (CLAUDE.md §4.5), and the back-off is defined in uptime.
 */
extern uint64_t np_uhdr_hal_uptime_ms(void);

#endif /* NP_UHDR_KEY_H */
