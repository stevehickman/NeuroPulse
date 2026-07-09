/*
 * NeurOne Two-Layer UHDR Key Scheme
 * Document: NP-FW-EMMC-002 Rev A §C.2–§C.6
 *
 * See np_uhdr_key.h for the architecture, contract, and SRAM-secret hygiene
 * rules.  This translation unit contains the pure key-lifecycle logic plus, in
 * NPTEST_HOST builds, HAL stubs that model TRNG / Argon2id / AES-256-GCM with
 * np_crypto SHA-256 so the wrap authenticates and the security invariants
 * (wrong-credential rejection, UKMD-not-WKMD mount) are host-testable.
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

    /* Layer-2 wrapper key from the initial credential. */
    st = np_uhdr_hal_argon2id(credential, cred_len, rec.argon2id_salt,
                              NP_UHDR_ARGON2ID_M_COST, NP_UHDR_ARGON2ID_T_COST,
                              NP_UHDR_ARGON2ID_PARALLELISM, wkmd);
    if (st != NP_UHDR_OK) { st = NP_UHDR_ERR_KDF; goto fail; }

    /* Wrap the UKMD.  Only the ciphertext + tag ever touch Config. */
    st = np_uhdr_hal_aes_gcm_encrypt(wkmd, rec.ukmd_nonce,
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
    np_uhdr_status_t st;

    ctx->unlocked = false;

    st = np_uhdr_hal_config_read_ukmd(&rec);
    if (st != NP_UHDR_OK) {
        memset(&rec, 0, sizeof(rec));
        return NP_UHDR_ERR_CONFIG;
    }

    /* Re-derive the wrapper key using the record's stored salt + parameters. */
    st = np_uhdr_hal_argon2id(credential, cred_len, rec.argon2id_salt,
                              rec.argon2id_m_cost, rec.argon2id_t_cost,
                              rec.argon2id_parallelism, wkmd);
    if (st != NP_UHDR_OK) {
        memset_explicit(wkmd, 0, sizeof(wkmd));
        memset(&rec, 0, sizeof(rec));
        return NP_UHDR_ERR_KDF;
    }

    /* Decrypt + authenticate.  A wrong credential fails the tag here. */
    st = np_uhdr_hal_aes_gcm_decrypt(wkmd, rec.ukmd_nonce,
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
    np_uhdr_status_t st;

    /* Read the existing record: the salt (§C.5 step 3) is reused unchanged. */
    st = np_uhdr_hal_config_read_ukmd(&rec);
    if (st != NP_UHDR_OK) {
        memset(&rec, 0, sizeof(rec));
        return NP_UHDR_ERR_CONFIG;
    }

    /* New wrapper key from the new credential, same salt + cost parameters. */
    st = np_uhdr_hal_argon2id(new_credential, new_cred_len, rec.argon2id_salt,
                              rec.argon2id_m_cost, rec.argon2id_t_cost,
                              rec.argon2id_parallelism, wkmd);
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
    st = np_uhdr_hal_aes_gcm_encrypt(wkmd, rec.ukmd_nonce,
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
 *   - Argon2id(cred, salt)  = SHA-256( SHA-256(cred) || salt )      (32 bytes)
 *   - GCM keystream         = SHA-256( key || nonce )               (32 bytes)
 *   - GCM tag               = SHA-256( key || nonce || ct )[0..16]
 * A wrong credential ⇒ wrong WKMD ⇒ wrong tag ⇒ NP_UHDR_ERR_AUTH, exactly as a
 * real AEAD would behave.  This is NOT cryptography; it exists to make the
 * lifecycle and its security invariants exercisable on the host CI.
 *
 * Only UKMD-sized (<= 32-byte) plaintext is supported (keystream is one
 * SHA-256 block); the module only ever wraps the 32-byte UKMD.
 */
#ifdef NPTEST_HOST

#include "np_crypto.h"

np_uhdr_status_t np_uhdr_hal_trng_generate(uint8_t *buf, size_t len)
{
    if (buf == NULL) {
        return NP_UHDR_ERR_INVALID;
    }
    for (size_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)(0xA5U ^ (uint8_t)i);
    }
    return NP_UHDR_OK;
}

np_uhdr_status_t np_uhdr_hal_argon2id(const uint8_t *credential,
                                      size_t cred_len,
                                      const uint8_t *salt,
                                      uint32_t m_cost,
                                      uint32_t t_cost,
                                      uint32_t parallelism,
                                      uint8_t *out)
{
    (void)m_cost; (void)t_cost; (void)parallelism;
    if (credential == NULL || salt == NULL || out == NULL) {
        return NP_UHDR_ERR_INVALID;
    }
    /* h = SHA-256(credential); mix = SHA-256(h || salt). */
    uint8_t mix[NP_CRYPTO_SHA256_SIZE + NP_UHDR_SALT_LEN];
    np_sha256(credential, (uint32_t)cred_len, mix);
    memcpy(mix + NP_CRYPTO_SHA256_SIZE, salt, NP_UHDR_SALT_LEN);
    np_sha256(mix, (uint32_t)sizeof(mix), out);   /* 32-byte WKMD */
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

/* Derive the tag = SHA-256(key || nonce || ct)[0..NP_UHDR_GCM_TAG_LEN]. */
static void host_gcm_tag(const uint8_t *key, const uint8_t *nonce,
                         const uint8_t *ct, size_t ct_len,
                         uint8_t tag[NP_UHDR_GCM_TAG_LEN])
{
    uint8_t buf[NP_UHDR_WKMD_LEN + NP_UHDR_GCM_NONCE_LEN + NP_UHDR_UKMD_LEN];
    uint8_t digest[NP_CRYPTO_SHA256_SIZE];
    memcpy(buf, key, NP_UHDR_WKMD_LEN);
    memcpy(buf + NP_UHDR_WKMD_LEN, nonce, NP_UHDR_GCM_NONCE_LEN);
    memcpy(buf + NP_UHDR_WKMD_LEN + NP_UHDR_GCM_NONCE_LEN, ct, ct_len);
    np_sha256(buf, (uint32_t)(NP_UHDR_WKMD_LEN + NP_UHDR_GCM_NONCE_LEN + ct_len),
              digest);
    memcpy(tag, digest, NP_UHDR_GCM_TAG_LEN);
}

np_uhdr_status_t np_uhdr_hal_aes_gcm_encrypt(const uint8_t *key,
                                             const uint8_t *nonce,
                                             const uint8_t *pt,
                                             size_t pt_len,
                                             uint8_t *ct,
                                             uint8_t *tag)
{
    if (key == NULL || nonce == NULL || pt == NULL || ct == NULL || tag == NULL) {
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
    host_gcm_tag(key, nonce, ct, pt_len, tag);
    return NP_UHDR_OK;
}

np_uhdr_status_t np_uhdr_hal_aes_gcm_decrypt(const uint8_t *key,
                                             const uint8_t *nonce,
                                             const uint8_t *ct,
                                             size_t ct_len,
                                             const uint8_t *tag,
                                             uint8_t *pt)
{
    if (key == NULL || nonce == NULL || ct == NULL || tag == NULL || pt == NULL) {
        return NP_UHDR_ERR_INVALID;
    }
    if (ct_len == 0U || ct_len > NP_CRYPTO_SHA256_SIZE) {
        return NP_UHDR_ERR_CRYPTO;
    }
    /* Authenticate first — never emit authentic plaintext on a tag mismatch. */
    uint8_t expect[NP_UHDR_GCM_TAG_LEN];
    host_gcm_tag(key, nonce, ct, ct_len, expect);

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
void np_uhdr_host_reset(void)
{
    memset(&g_host_config, 0, sizeof(g_host_config));
    g_host_config_valid = false;
    memset(g_host_mounted_key, 0, sizeof(g_host_mounted_key));
    g_host_mount_count = 0U;
}

#endif /* NPTEST_HOST */
