/*
 * NeurOne Two-Layer UHDR Key Scheme
 * Document: NP-FW-EMMC-002 Rev 1 §C.2–§C.6
 *
 * See np_uhdr_key.h for the architecture, contract, and SRAM-secret hygiene
 * rules.  This translation unit contains the pure key-lifecycle logic plus, in
 * NPTEST_HOST builds, HAL stubs that model TRNG / Argon2id / hardware-binding /
 * AES-256-GCM with np_crypto SHA-256 so the wrap authenticates and the security
 * invariants (wrong-credential rejection, hardware binding, record-tamper
 * rejection, UKMD-not-WKMD mount) are host-testable.
 *
 * Hardened 2026-07-08 per the privacy review:
 *   F1  WKMD = combine( Argon2id(credential, salt), device-fused HW key ) so an
 *       eMMC image cannot be brute-forced offline (OI-UHDRK-09).
 *   F2  Salt + KDF parameters are bound into the GCM tag as AAD, and stored KDF
 *       parameters are validated against a floor before use.
 *   F3  Every unlock failure path scrubs the UKMD from SRAM.
 *
 * Sensitive-data zeroing uses memset_explicit() (project convention).  A
 * guarded non-elidable fallback is supplied for the -std=c11 cross build.
 */

#include "np_uhdr_key.h"

#include <string.h>

/* Compile-time guarantee the on-flash record is byte-exact (§C.3). */
typedef char np_uhdr_record_size_check[
    (sizeof(np_ukmd_record_t) == NP_UHDR_UKMD_RECORD_SIZE) ? 1 : -1];

/* ── memset_explicit fallback (non-elidable) ─────────────────────────────────
 * memset_explicit() is C23; the firmware is built -std=c11.  Mirror the
 * convention used by firmware/anon: a volatile function pointer the compiler
 * cannot prove dead, so it cannot elide the clear of a secret buffer.
 */
#if !defined(NP_UHDR_HAVE_MEMSET_EXPLICIT) && \
    !(defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L)

static void *(*const volatile np_uhdr_memset_fn)(void *, int, size_t) = memset;

static void memset_explicit(void *dst, int ch, size_t len)
{
    np_uhdr_memset_fn(dst, ch, len);
}
#endif

/* ── Internal helpers ─────────────────────────────────────────────────────── */

/*
 * Fill `rec` with the fixed Argon2id parameters and zeroed reserved bytes.
 * Callers set salt / nonce / ciphertext / tag separately.
 */
static void set_record_params(np_ukmd_record_t *rec)
{
    rec->argon2id_version     = NP_UHDR_ARGON2ID_VERSION;
    rec->argon2id_m_cost      = NP_UHDR_ARGON2ID_M_COST;
    rec->argon2id_t_cost      = NP_UHDR_ARGON2ID_T_COST;
    rec->argon2id_parallelism = NP_UHDR_ARGON2ID_PARALLELISM;
    memset(rec->reserved, 0, sizeof(rec->reserved));
}

/*
 * Reject a stored record whose KDF parameters fall below the floor
 * (privacy review Finding 2).  A rolled-back or corrupted record cannot be used
 * to derive a WKMD with weakened settings.
 */
static np_uhdr_status_t validate_kdf_params(const np_ukmd_record_t *rec)
{
    if (rec->argon2id_version != NP_UHDR_ARGON2ID_VERSION ||
        rec->argon2id_m_cost < NP_UHDR_ARGON2ID_M_COST_MIN ||
        rec->argon2id_t_cost < NP_UHDR_ARGON2ID_T_COST_MIN ||
        rec->argon2id_parallelism != NP_UHDR_ARGON2ID_PARALLELISM) {
        return NP_UHDR_ERR_PARAM;
    }
    return NP_UHDR_OK;
}

/* Serialise a uint32 little-endian into out[0..3]. */
static void put_u32_le(uint8_t *out, uint32_t v)
{
    out[0] = (uint8_t)(v & 0xFFU);
    out[1] = (uint8_t)((v >> 8) & 0xFFU);
    out[2] = (uint8_t)((v >> 16) & 0xFFU);
    out[3] = (uint8_t)((v >> 24) & 0xFFU);
}

/*
 * Build the GCM AAD (privacy review Finding 2): salt || version || m || t || p,
 * little-endian, exactly NP_UHDR_AAD_LEN bytes.  Binding these into the tag
 * makes any tamper of the record's salt or KDF parameters fail authentication.
 */
static void build_aad(const np_ukmd_record_t *rec, uint8_t aad[NP_UHDR_AAD_LEN])
{
    memcpy(aad, rec->argon2id_salt, NP_UHDR_SALT_LEN);
    put_u32_le(aad + NP_UHDR_SALT_LEN + 0,  rec->argon2id_version);
    put_u32_le(aad + NP_UHDR_SALT_LEN + 4,  rec->argon2id_m_cost);
    put_u32_le(aad + NP_UHDR_SALT_LEN + 8,  rec->argon2id_t_cost);
    put_u32_le(aad + NP_UHDR_SALT_LEN + 12, rec->argon2id_parallelism);
}

/*
 * Derive the final WKMD (privacy review Finding 1).  Two stages:
 *   1. credential-key = Argon2id(credential, salt, m/t/p)   [OI-UHDRK-02]
 *   2. WKMD           = hw_bind(credential-key, device HUK)  [OI-UHDRK-09]
 * The intermediate credential-key is zeroed before return; only `wkmd_out`
 * survives.  Without the device-fused key, the Argon2id output is worthless —
 * that is what defeats offline brute-force of an eMMC image.
 */
static np_uhdr_status_t derive_wkmd(const uint8_t *credential, size_t cred_len,
                                    const uint8_t *salt, uint32_t version,
                                    uint32_t m_cost, uint32_t t_cost,
                                    uint32_t parallelism,
                                    uint8_t *wkmd_out)
{
    uint8_t ckey[NP_UHDR_WKMD_LEN];

    np_uhdr_status_t st = np_uhdr_hal_argon2id(credential, cred_len, salt,
                                               version, m_cost, t_cost,
                                               parallelism, ckey);
    if (st != NP_UHDR_OK) {
        memset_explicit(ckey, 0, sizeof(ckey));
        return NP_UHDR_ERR_KDF;
    }

    st = np_uhdr_hal_hw_bind(ckey, wkmd_out);
    memset_explicit(ckey, 0, sizeof(ckey));
    if (st != NP_UHDR_OK) {
        memset_explicit(wkmd_out, 0, NP_UHDR_WKMD_LEN);
        return NP_UHDR_ERR_KDF;
    }
    return NP_UHDR_OK;
}

/* ── Lifecycle ───────────────────────────────────────────────────────────────*/

np_uhdr_status_t np_uhdr_key_provision(np_uhdr_key_ctx_t *ctx,
                                       const uint8_t *credential,
                                       size_t cred_len)
{
    if (ctx == NULL || credential == NULL || cred_len == 0U) {
        return NP_UHDR_ERR_INVALID;
    }

    np_ukmd_record_t rec;
    uint8_t wkmd[NP_UHDR_WKMD_LEN];
    uint8_t aad[NP_UHDR_AAD_LEN];
    np_uhdr_status_t st;

    ctx->unlocked = false;
    memset(&rec, 0, sizeof(rec));

    /* Mint the Layer-1 master key directly into the context. */
    st = np_uhdr_hal_trng_generate(ctx->ukmd, NP_UHDR_UKMD_LEN);
    if (st != NP_UHDR_OK) { st = NP_UHDR_ERR_TRNG; goto fail; }

    /* Device-unique salt + per-wrap nonce. */
    st = np_uhdr_hal_trng_generate(rec.argon2id_salt, NP_UHDR_SALT_LEN);
    if (st != NP_UHDR_OK) { st = NP_UHDR_ERR_TRNG; goto fail; }
    st = np_uhdr_hal_trng_generate(rec.ukmd_nonce, NP_UHDR_GCM_NONCE_LEN);
    if (st != NP_UHDR_OK) { st = NP_UHDR_ERR_TRNG; goto fail; }

    set_record_params(&rec);
    build_aad(&rec, aad);

    /* Layer-2 wrapper key: Argon2id(credential) bound to the device HW key. */
    st = derive_wkmd(credential, cred_len, rec.argon2id_salt,
                     NP_UHDR_ARGON2ID_VERSION, NP_UHDR_ARGON2ID_M_COST,
                     NP_UHDR_ARGON2ID_T_COST, NP_UHDR_ARGON2ID_PARALLELISM, wkmd);
    if (st != NP_UHDR_OK) { goto fail; }

    /* Wrap the UKMD; salt+params are authenticated as AAD.  Only the ciphertext
     * + tag ever touch Config. */
    st = np_uhdr_hal_aes_gcm_encrypt(wkmd, rec.ukmd_nonce, aad, NP_UHDR_AAD_LEN,
                                     ctx->ukmd, NP_UHDR_UKMD_LEN,
                                     rec.ukmd_ciphertext, rec.ukmd_tag);
    if (st != NP_UHDR_OK) { st = NP_UHDR_ERR_CRYPTO; goto fail; }

    st = np_uhdr_hal_config_write_ukmd(&rec);
    if (st != NP_UHDR_OK) { st = NP_UHDR_ERR_CONFIG; goto fail; }

    st = np_uhdr_hal_mount_uhdr(ctx->ukmd);
    if (st != NP_UHDR_OK) { st = NP_UHDR_ERR_MOUNT; goto fail; }

    /* Success: WKMD is done; UKMD stays live in the context. */
    memset_explicit(wkmd, 0, sizeof(wkmd));
    memset(&rec, 0, sizeof(rec));   /* rec holds only ciphertext — not secret */
    ctx->unlocked = true;
    return NP_UHDR_OK;

fail:
    memset_explicit(wkmd, 0, sizeof(wkmd));
    memset_explicit(ctx->ukmd, 0, NP_UHDR_UKMD_LEN);
    memset(&rec, 0, sizeof(rec));
    ctx->unlocked = false;
    return st;
}

np_uhdr_status_t np_uhdr_key_unlock(np_uhdr_key_ctx_t *ctx,
                                    const uint8_t *credential,
                                    size_t cred_len)
{
    if (ctx == NULL || credential == NULL || cred_len == 0U) {
        return NP_UHDR_ERR_INVALID;
    }

    np_ukmd_record_t rec;
    uint8_t wkmd[NP_UHDR_WKMD_LEN];
    uint8_t aad[NP_UHDR_AAD_LEN];
    np_uhdr_status_t st;

    ctx->unlocked = false;

    st = np_uhdr_hal_config_read_ukmd(&rec);
    if (st != NP_UHDR_OK) {
        /* Finding 3: scrub any prior UKMD even on early exit. */
        memset_explicit(ctx->ukmd, 0, NP_UHDR_UKMD_LEN);
        memset(&rec, 0, sizeof(rec));
        return NP_UHDR_ERR_CONFIG;
    }

    /* Finding 2: reject a record whose KDF parameters were rolled back/tampered. */
    st = validate_kdf_params(&rec);
    if (st != NP_UHDR_OK) {
        memset_explicit(ctx->ukmd, 0, NP_UHDR_UKMD_LEN);
        memset(&rec, 0, sizeof(rec));
        return NP_UHDR_ERR_PARAM;
    }

    build_aad(&rec, aad);

    /* Re-derive the wrapper key using the record's stored version + salt +
     * parameters (all validated above) and the device-fused key. */
    st = derive_wkmd(credential, cred_len, rec.argon2id_salt,
                     rec.argon2id_version, rec.argon2id_m_cost,
                     rec.argon2id_t_cost, rec.argon2id_parallelism, wkmd);
    if (st != NP_UHDR_OK) {
        memset_explicit(wkmd, 0, sizeof(wkmd));
        memset_explicit(ctx->ukmd, 0, NP_UHDR_UKMD_LEN);
        memset(&rec, 0, sizeof(rec));
        return NP_UHDR_ERR_KDF;
    }

    /* Decrypt + authenticate.  A wrong credential, a foreign device, or a
     * tampered salt/params all fail the tag here. */
    st = np_uhdr_hal_aes_gcm_decrypt(wkmd, rec.ukmd_nonce, aad, NP_UHDR_AAD_LEN,
                                     rec.ukmd_ciphertext, NP_UHDR_UKMD_LEN,
                                     rec.ukmd_tag, ctx->ukmd);
    memset_explicit(wkmd, 0, sizeof(wkmd));
    if (st != NP_UHDR_OK) {
        /* AUTH or CRYPTO: leave nothing recoverable, stay locked. */
        memset_explicit(ctx->ukmd, 0, NP_UHDR_UKMD_LEN);
        memset(&rec, 0, sizeof(rec));
        return (st == NP_UHDR_ERR_AUTH) ? NP_UHDR_ERR_AUTH : NP_UHDR_ERR_CRYPTO;
    }

    st = np_uhdr_hal_mount_uhdr(ctx->ukmd);
    memset(&rec, 0, sizeof(rec));
    if (st != NP_UHDR_OK) {
        memset_explicit(ctx->ukmd, 0, NP_UHDR_UKMD_LEN);
        return NP_UHDR_ERR_MOUNT;
    }

    ctx->unlocked = true;
    return NP_UHDR_OK;
}

void np_uhdr_key_background(np_uhdr_key_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    memset_explicit(ctx->ukmd, 0, NP_UHDR_UKMD_LEN);
    ctx->unlocked = false;
}

np_uhdr_status_t np_uhdr_key_change_credential(np_uhdr_key_ctx_t *ctx,
                                               const uint8_t *new_credential,
                                               size_t new_cred_len)
{
    if (ctx == NULL || new_credential == NULL || new_cred_len == 0U) {
        return NP_UHDR_ERR_INVALID;
    }
    if (!ctx->unlocked) {
        /* UKMD must be live in SRAM to re-wrap it (§C.5 step 1). */
        return NP_UHDR_ERR_STATE;
    }

    np_ukmd_record_t rec;
    uint8_t wkmd[NP_UHDR_WKMD_LEN];
    uint8_t aad[NP_UHDR_AAD_LEN];
    np_uhdr_status_t st;

    /* Read the existing record: the salt (§C.5 step 3) is reused unchanged. */
    st = np_uhdr_hal_config_read_ukmd(&rec);
    if (st != NP_UHDR_OK) {
        memset(&rec, 0, sizeof(rec));
        return NP_UHDR_ERR_CONFIG;
    }

    /* Reject a tampered record before re-wrapping (keeps the live session). */
    st = validate_kdf_params(&rec);
    if (st != NP_UHDR_OK) {
        memset(&rec, 0, sizeof(rec));
        return NP_UHDR_ERR_PARAM;
    }

    /* Normalise the parameters to the current constants (salt preserved), then
     * bind salt+params as AAD for the new wrap. */
    set_record_params(&rec);
    build_aad(&rec, aad);

    /* New wrapper key from the new credential, same salt, device-bound. */
    st = derive_wkmd(new_credential, new_cred_len, rec.argon2id_salt,
                     NP_UHDR_ARGON2ID_VERSION, NP_UHDR_ARGON2ID_M_COST,
                     NP_UHDR_ARGON2ID_T_COST, NP_UHDR_ARGON2ID_PARALLELISM, wkmd);
    if (st != NP_UHDR_OK) {
        memset_explicit(wkmd, 0, sizeof(wkmd));
        memset(&rec, 0, sizeof(rec));
        return NP_UHDR_ERR_KDF;
    }

    /* Fresh nonce for the new wrap (§C.5 step 4). */
    st = np_uhdr_hal_trng_generate(rec.ukmd_nonce, NP_UHDR_GCM_NONCE_LEN);
    if (st != NP_UHDR_OK) {
        memset_explicit(wkmd, 0, sizeof(wkmd));
        memset(&rec, 0, sizeof(rec));
        return NP_UHDR_ERR_TRNG;
    }

    /* Re-wrap the SAME UKMD — the UHDR partition ciphertext is untouched. */
    st = np_uhdr_hal_aes_gcm_encrypt(wkmd, rec.ukmd_nonce, aad, NP_UHDR_AAD_LEN,
                                     ctx->ukmd, NP_UHDR_UKMD_LEN,
                                     rec.ukmd_ciphertext, rec.ukmd_tag);
    memset_explicit(wkmd, 0, sizeof(wkmd));
    if (st != NP_UHDR_OK) {
        memset(&rec, 0, sizeof(rec));
        return NP_UHDR_ERR_CRYPTO;
    }

    /* Atomic Config update (LittleFS transaction). */
    st = np_uhdr_hal_config_write_ukmd(&rec);
    memset(&rec, 0, sizeof(rec));
    if (st != NP_UHDR_OK) {
        return NP_UHDR_ERR_CONFIG;
    }

    /* Context stays unlocked with the same UKMD. */
    return NP_UHDR_OK;
}

/* ── Host test stubs (NPTEST_HOST) ───────────────────────────────────────────
 *
 * NOT compiled into target firmware.  These model the HAL with np_crypto
 * SHA-256 so the GCM wrap actually authenticates:
 *   - Argon2id(cred, salt)     = SHA-256( SHA-256(cred) || salt )     (32 bytes)
 *   - hw_bind(ckey, devkey)    = SHA-256( ckey || device_key )        (32 bytes)
 *   - GCM keystream            = SHA-256( key || nonce )              (32 bytes)
 *   - GCM tag                  = SHA-256( key || nonce || aad || ct )[0..16]
 * A wrong credential, a different device key, or a tampered salt/param (in the
 * AAD) all flip the tag ⇒ NP_UHDR_ERR_AUTH, exactly as a real AEAD would.  This
 * is NOT cryptography; it exists to make the lifecycle and its security
 * invariants exercisable on the host CI.
 *
 * Only UKMD-sized (<= 32-byte) plaintext is supported (keystream is one
 * SHA-256 block); the module only ever wraps the 32-byte UKMD.
 */
#ifdef NPTEST_HOST

#include "np_crypto.h"

/*
 * Simulated device-fused hardware key.  On real silicon this is a CAAM/OTPMK
 * HUK that software cannot read; here it is a RAM value the tests can swap to
 * simulate "a different device", proving hardware binding.
 */
static const uint8_t NP_UHDR_HOST_DEVKEY_DEFAULT[NP_UHDR_HWKEY_LEN] = {
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
    0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
    0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
};
static uint8_t g_host_device_key[NP_UHDR_HWKEY_LEN];
static bool    g_host_device_key_init = false;

static const uint8_t *host_device_key(void)
{
    if (!g_host_device_key_init) {
        memcpy(g_host_device_key, NP_UHDR_HOST_DEVKEY_DEFAULT, NP_UHDR_HWKEY_LEN);
        g_host_device_key_init = true;
    }
    return g_host_device_key;
}

/*
 * Counter-seeded so successive calls yield DISTINCT output (models a real RNG
 * for nonce-uniqueness testing — CR-3).  Still deterministic per test run:
 * np_uhdr_host_reset() rewinds the counter, so a given test is reproducible.
 */
static uint32_t g_host_trng_ctr = 0U;

np_uhdr_status_t np_uhdr_hal_trng_generate(uint8_t *buf, size_t len)
{
    if (buf == NULL) {
        return NP_UHDR_ERR_INVALID;
    }
    const uint32_t c = g_host_trng_ctr++;    /* unique per call */
    for (size_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)(0xA5U ^ (uint8_t)i ^ (uint8_t)(c * 31U + (c >> 3)));
    }
    return NP_UHDR_OK;
}

np_uhdr_status_t np_uhdr_hal_argon2id(const uint8_t *credential,
                                      size_t cred_len,
                                      const uint8_t *salt,
                                      uint32_t version,
                                      uint32_t m_cost,
                                      uint32_t t_cost,
                                      uint32_t parallelism,
                                      uint8_t *out)
{
    (void)m_cost; (void)t_cost; (void)parallelism;
    if (credential == NULL || salt == NULL || out == NULL) {
        return NP_UHDR_ERR_INVALID;
    }
    /* ckey = SHA-256( SHA-256(cred) || salt || version_le ).  `version` is
     * folded in so a record wrapped under a different Argon2 version derives a
     * different key — mirroring the real KDF's version dependence and exercising
     * the newly-plumbed version parameter (CR-2). */
    uint8_t mix[NP_CRYPTO_SHA256_SIZE + NP_UHDR_SALT_LEN + 4U];
    np_sha256(credential, (uint32_t)cred_len, mix);
    memcpy(mix + NP_CRYPTO_SHA256_SIZE, salt, NP_UHDR_SALT_LEN);
    put_u32_le(mix + NP_CRYPTO_SHA256_SIZE + NP_UHDR_SALT_LEN, version);
    np_sha256(mix, (uint32_t)sizeof(mix), out);   /* 32-byte credential key */
    return NP_UHDR_OK;
}

np_uhdr_status_t np_uhdr_hal_hw_bind(const uint8_t *ckey, uint8_t *wkmd_out)
{
    if (ckey == NULL || wkmd_out == NULL) {
        return NP_UHDR_ERR_INVALID;
    }
    /* WKMD = SHA-256( ckey || device_key ). */
    uint8_t buf[NP_UHDR_WKMD_LEN + NP_UHDR_HWKEY_LEN];
    memcpy(buf, ckey, NP_UHDR_WKMD_LEN);
    memcpy(buf + NP_UHDR_WKMD_LEN, host_device_key(), NP_UHDR_HWKEY_LEN);
    np_sha256(buf, (uint32_t)sizeof(buf), wkmd_out);
    return NP_UHDR_OK;
}

/* Derive the 32-byte keystream = SHA-256(key || nonce). */
static void host_gcm_keystream(const uint8_t *key, const uint8_t *nonce,
                               uint8_t ks[NP_CRYPTO_SHA256_SIZE])
{
    uint8_t buf[NP_UHDR_WKMD_LEN + NP_UHDR_GCM_NONCE_LEN];
    memcpy(buf, key, NP_UHDR_WKMD_LEN);
    memcpy(buf + NP_UHDR_WKMD_LEN, nonce, NP_UHDR_GCM_NONCE_LEN);
    np_sha256(buf, (uint32_t)sizeof(buf), ks);
}

/* Derive tag = SHA-256(key || nonce || aad || ct)[0..NP_UHDR_GCM_TAG_LEN]. */
static void host_gcm_tag(const uint8_t *key, const uint8_t *nonce,
                         const uint8_t *aad, size_t aad_len,
                         const uint8_t *ct, size_t ct_len,
                         uint8_t tag[NP_UHDR_GCM_TAG_LEN])
{
    uint8_t buf[NP_UHDR_WKMD_LEN + NP_UHDR_GCM_NONCE_LEN +
                NP_UHDR_AAD_LEN + NP_UHDR_UKMD_LEN];
    uint8_t digest[NP_CRYPTO_SHA256_SIZE];
    size_t off = 0;
    memcpy(buf + off, key, NP_UHDR_WKMD_LEN);        off += NP_UHDR_WKMD_LEN;
    memcpy(buf + off, nonce, NP_UHDR_GCM_NONCE_LEN); off += NP_UHDR_GCM_NONCE_LEN;
    if (aad != NULL && aad_len > 0U) {
        memcpy(buf + off, aad, aad_len);             off += aad_len;
    }
    memcpy(buf + off, ct, ct_len);                   off += ct_len;
    np_sha256(buf, (uint32_t)off, digest);
    memcpy(tag, digest, NP_UHDR_GCM_TAG_LEN);
}

np_uhdr_status_t np_uhdr_hal_aes_gcm_encrypt(const uint8_t *key,
                                             const uint8_t *nonce,
                                             const uint8_t *aad,
                                             size_t aad_len,
                                             const uint8_t *pt,
                                             size_t pt_len,
                                             uint8_t *ct,
                                             uint8_t *tag)
{
    if (key == NULL || nonce == NULL || pt == NULL || ct == NULL || tag == NULL) {
        return NP_UHDR_ERR_INVALID;
    }
    if ((aad == NULL && aad_len > 0U) || aad_len > NP_UHDR_AAD_LEN) {
        return NP_UHDR_ERR_INVALID;
    }
    if (pt_len == 0U || pt_len > NP_CRYPTO_SHA256_SIZE) {
        return NP_UHDR_ERR_CRYPTO;   /* host stub covers only UKMD-sized wraps */
    }
    uint8_t ks[NP_CRYPTO_SHA256_SIZE];
    host_gcm_keystream(key, nonce, ks);
    for (size_t i = 0; i < pt_len; i++) {
        ct[i] = (uint8_t)(pt[i] ^ ks[i]);
    }
    host_gcm_tag(key, nonce, aad, aad_len, ct, pt_len, tag);
    return NP_UHDR_OK;
}

np_uhdr_status_t np_uhdr_hal_aes_gcm_decrypt(const uint8_t *key,
                                             const uint8_t *nonce,
                                             const uint8_t *aad,
                                             size_t aad_len,
                                             const uint8_t *ct,
                                             size_t ct_len,
                                             const uint8_t *tag,
                                             uint8_t *pt)
{
    if (key == NULL || nonce == NULL || ct == NULL || tag == NULL || pt == NULL) {
        return NP_UHDR_ERR_INVALID;
    }
    if ((aad == NULL && aad_len > 0U) || aad_len > NP_UHDR_AAD_LEN) {
        return NP_UHDR_ERR_INVALID;
    }
    if (ct_len == 0U || ct_len > NP_CRYPTO_SHA256_SIZE) {
        return NP_UHDR_ERR_CRYPTO;
    }
    /* Authenticate first — never emit authentic plaintext on a tag mismatch. */
    uint8_t expect[NP_UHDR_GCM_TAG_LEN];
    host_gcm_tag(key, nonce, aad, aad_len, ct, ct_len, expect);

    uint8_t diff = 0U;
    for (size_t i = 0; i < NP_UHDR_GCM_TAG_LEN; i++) {
        diff |= (uint8_t)(expect[i] ^ tag[i]);   /* constant-time compare */
    }
    if (diff != 0U) {
        return NP_UHDR_ERR_AUTH;
    }

    uint8_t ks[NP_CRYPTO_SHA256_SIZE];
    host_gcm_keystream(key, nonce, ks);
    for (size_t i = 0; i < ct_len; i++) {
        pt[i] = (uint8_t)(ct[i] ^ ks[i]);
    }
    return NP_UHDR_OK;
}

/* RAM-backed Config record + mount capture for the host tests. */
static np_ukmd_record_t g_host_config;
static bool             g_host_config_valid = false;
static uint8_t          g_host_mounted_key[NP_UHDR_UKMD_LEN];
static uint32_t         g_host_mount_count  = 0U;

np_uhdr_status_t np_uhdr_hal_config_read_ukmd(np_ukmd_record_t *out)
{
    if (out == NULL) {
        return NP_UHDR_ERR_INVALID;
    }
    if (!g_host_config_valid) {
        return NP_UHDR_ERR_CONFIG;
    }
    memcpy(out, &g_host_config, sizeof(*out));
    return NP_UHDR_OK;
}

np_uhdr_status_t np_uhdr_hal_config_write_ukmd(const np_ukmd_record_t *rec)
{
    if (rec == NULL) {
        return NP_UHDR_ERR_INVALID;
    }
    memcpy(&g_host_config, rec, sizeof(g_host_config));
    g_host_config_valid = true;
    return NP_UHDR_OK;
}

np_uhdr_status_t np_uhdr_hal_mount_uhdr(const uint8_t *ukmd)
{
    if (ukmd == NULL) {
        return NP_UHDR_ERR_INVALID;
    }
    memcpy(g_host_mounted_key, ukmd, NP_UHDR_UKMD_LEN);
    g_host_mount_count++;
    return NP_UHDR_OK;
}

/* Test-only accessors (declared extern in the test file). */
const uint8_t *np_uhdr_host_last_mounted_key(void) { return g_host_mounted_key; }
uint32_t       np_uhdr_host_mount_count(void)      { return g_host_mount_count; }
const np_ukmd_record_t *np_uhdr_host_config(void)  { return &g_host_config; }

/* Mutable Config accessor so tests can simulate on-flash tampering. */
np_ukmd_record_t *np_uhdr_host_config_mut(void)    { return &g_host_config; }

/* Swap the simulated device-fused key (simulate "a different device"). */
void np_uhdr_host_set_device_key(const uint8_t key[NP_UHDR_HWKEY_LEN])
{
    memcpy(g_host_device_key, key, NP_UHDR_HWKEY_LEN);
    g_host_device_key_init = true;
}

void np_uhdr_host_reset(void)
{
    memset(&g_host_config, 0, sizeof(g_host_config));
    g_host_config_valid = false;
    memset(g_host_mounted_key, 0, sizeof(g_host_mounted_key));
    g_host_mount_count = 0U;
    /* Restore the default device key so each test starts on the same device. */
    memcpy(g_host_device_key, NP_UHDR_HOST_DEVKEY_DEFAULT, NP_UHDR_HWKEY_LEN);
    g_host_device_key_init = true;
    /* Rewind the TRNG counter so each test is reproducible. */
    g_host_trng_ctr = 0U;
}

#endif /* NPTEST_HOST */
