/*
 * NeurOne Per-Person UHDR Keys on a Shared Helmet — profile key store
 * Document: NP-FW-EMMC-002 §C.8 (OI-PRIV-34, NP-PRIV-ANALYSIS-004)
 * IEC 62304 Class: SW-02 Class B (main processor)
 *
 * See np_uhdr_profile.h for the design.  This translation unit holds the pure
 * key-store logic and, in NPTEST_HOST builds, RAM-backed stubs for the four
 * profile seams.  The TRNG, Argon2id, hw_bind and AES-GCM seams are shared with
 * np_uhdr_key.c (and stubbed there for the host).
 *
 * Secret hygiene, as np_uhdr_key.c: PK, WKMD and RK live in SRAM only for the
 * span of one call (PK also in an open context), and every path out of a
 * function zeroes what it no longer needs.
 */

#include "np_uhdr_profile.h"
#include "np_uhdr_key.h"
#include "np_uhdr_key_internal.h"

#include <string.h>

typedef char np_uhdr_profile_record_size_check[
    (sizeof(np_uhdr_profile_record_t) == NP_UHDR_PROFILE_RECORD_SIZE) ? 1 : -1];

/* AAD lengths (§C.8.2). */
#define W1_AAD_LEN  (NP_UHDR_PROFILE_ID_LEN + NP_UHDR_SALT_LEN + 16U)          /* 64 */
#define W2_AAD_LEN  (NP_UHDR_PROFILE_ID_LEN + NP_UHDR_RECOVERY_SALT_LEN + 4U)  /* 36 */

typedef char np_uhdr_profile_aad_fits[(W1_AAD_LEN <= NP_UHDR_AAD_MAX_LEN) ? 1 : -1];

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static void put_u32(uint8_t *out, uint32_t v)
{
    out[0] = (uint8_t)(v & 0xFFU);
    out[1] = (uint8_t)((v >> 8) & 0xFFU);
    out[2] = (uint8_t)((v >> 16) & 0xFFU);
    out[3] = (uint8_t)((v >> 24) & 0xFFU);
}

/* Wrap 1 binds the profile id, salt and KDF parameters (compare §C.3 F2). */
static void build_w1_aad(const np_uhdr_profile_record_t *r, uint8_t aad[W1_AAD_LEN])
{
    memcpy(aad, r->profile_id, NP_UHDR_PROFILE_ID_LEN);
    memcpy(aad + NP_UHDR_PROFILE_ID_LEN, r->kdf_salt, NP_UHDR_SALT_LEN);
    uint8_t *p = aad + NP_UHDR_PROFILE_ID_LEN + NP_UHDR_SALT_LEN;
    put_u32(p + 0,  r->argon2id_version);
    put_u32(p + 4,  r->argon2id_m_cost);
    put_u32(p + 8,  r->argon2id_t_cost);
    put_u32(p + 12, r->argon2id_parallelism);
}

/* Wrap 2 binds the profile id, the recovery salt and the record format, so a
 * wrap cannot be moved onto another profile's record. */
static void build_w2_aad(const np_uhdr_profile_record_t *r, uint8_t aad[W2_AAD_LEN])
{
    memcpy(aad, r->profile_id, NP_UHDR_PROFILE_ID_LEN);
    memcpy(aad + NP_UHDR_PROFILE_ID_LEN, r->rec_salt, NP_UHDR_RECOVERY_SALT_LEN);
    put_u32(aad + NP_UHDR_PROFILE_ID_LEN + NP_UHDR_RECOVERY_SALT_LEN, r->format);
}

static void zero_wrap1(np_uhdr_profile_record_t *r)
{
    memset(r->kdf_salt, 0, sizeof(r->kdf_salt));
    r->argon2id_version = 0U;
    r->argon2id_m_cost = 0U;
    r->argon2id_t_cost = 0U;
    r->argon2id_parallelism = 0U;
    memset(r->w1_nonce, 0, sizeof(r->w1_nonce));
    memset(r->w1_ct, 0, sizeof(r->w1_ct));
    memset(r->w1_tag, 0, sizeof(r->w1_tag));
}

static bool wrap1_is_zero(const np_uhdr_profile_record_t *r)
{
    uint8_t acc = 0U;
    for (size_t i = 0; i < sizeof(r->kdf_salt); i++) { acc |= r->kdf_salt[i]; }
    for (size_t i = 0; i < sizeof(r->w1_nonce); i++) { acc |= r->w1_nonce[i]; }
    for (size_t i = 0; i < sizeof(r->w1_ct); i++)    { acc |= r->w1_ct[i]; }
    for (size_t i = 0; i < sizeof(r->w1_tag); i++)   { acc |= r->w1_tag[i]; }
    return acc == 0U && r->argon2id_version == 0U && r->argon2id_m_cost == 0U &&
           r->argon2id_t_cost == 0U && r->argon2id_parallelism == 0U;
}

/* Header and state well-formed; for LIVE, the KDF parameters meet the floor. */
static np_uhdr_status_t validate_record(const np_uhdr_profile_record_t *r)
{
    if (r->magic != NP_UHDR_PROFILE_MAGIC || r->format != NP_UHDR_PROFILE_FORMAT ||
        r->reserved0[0] != 0U || r->reserved0[1] != 0U || r->reserved0[2] != 0U) {
        return NP_UHDR_ERR_CONFIG;
    }
    if (r->state == NP_UHDR_PROFILE_DORMANT) {
        return wrap1_is_zero(r) ? NP_UHDR_OK : NP_UHDR_ERR_CONFIG;
    }
    if (r->state != NP_UHDR_PROFILE_LIVE) {
        return NP_UHDR_ERR_CONFIG;
    }
    if (r->argon2id_version != NP_UHDR_ARGON2ID_VERSION ||
        r->argon2id_m_cost < NP_UHDR_ARGON2ID_M_COST_MIN ||
        r->argon2id_t_cost < NP_UHDR_ARGON2ID_T_COST_MIN ||
        r->argon2id_parallelism != NP_UHDR_ARGON2ID_PARALLELISM) {
        return NP_UHDR_ERR_PARAM;   /* rolled back or tampered (§C.3 F2) */
    }
    return NP_UHDR_OK;
}

/* Write a fresh wrap 1 of `pk` under `credential` into `r` (new salt, nonce,
 * current parameters).  Leaves the state to the caller. */
static np_uhdr_status_t seal_wrap1(np_uhdr_profile_record_t *r, const uint8_t *pk,
                                   const uint8_t *credential, size_t cred_len)
{
    uint8_t wkmd[NP_UHDR_WKMD_LEN];
    uint8_t aad[W1_AAD_LEN];
    np_uhdr_status_t st;

    if (np_uhdr_hal_trng_generate(r->kdf_salt, NP_UHDR_SALT_LEN) != NP_UHDR_OK ||
        np_uhdr_hal_trng_generate(r->w1_nonce, NP_UHDR_GCM_NONCE_LEN) != NP_UHDR_OK) {
        return NP_UHDR_ERR_TRNG;
    }
    r->argon2id_version     = NP_UHDR_ARGON2ID_VERSION;
    r->argon2id_m_cost      = NP_UHDR_ARGON2ID_M_COST;
    r->argon2id_t_cost      = NP_UHDR_ARGON2ID_T_COST;
    r->argon2id_parallelism = NP_UHDR_ARGON2ID_PARALLELISM;
    build_w1_aad(r, aad);

    st = np_uhdr_derive_wkmd(credential, cred_len, r->kdf_salt,
                             r->argon2id_version, r->argon2id_m_cost,
                             r->argon2id_t_cost, r->argon2id_parallelism, wkmd);
    if (st != NP_UHDR_OK) {
        return NP_UHDR_ERR_KDF;
    }
    st = np_uhdr_hal_aes_gcm_encrypt(wkmd, r->w1_nonce, aad, W1_AAD_LEN,
                                     pk, NP_UHDR_PROFILE_KEY_LEN, r->w1_ct, r->w1_tag);
    np_uhdr_secure_zero(wkmd, sizeof(wkmd));
    return (st == NP_UHDR_OK) ? NP_UHDR_OK : NP_UHDR_ERR_CRYPTO;
}

/* Derive RK from the recovery code (zeroed by the caller on every path). */
static np_uhdr_status_t derive_rk(const uint8_t *code, const uint8_t *salt, uint8_t *rk)
{
    if (np_uhdr_hal_recovery_kdf(code, salt, rk) != NP_UHDR_OK) {
        np_uhdr_secure_zero(rk, NP_UHDR_RECOVERY_KEY_LEN);
        return NP_UHDR_ERR_KDF;
    }
    return NP_UHDR_OK;
}

static void fail_ctx(np_uhdr_profile_ctx_t *ctx)
{
    if (ctx != NULL) {
        np_uhdr_secure_zero(ctx->pk, sizeof(ctx->pk));
        ctx->open = false;
    }
}

static void open_ctx(np_uhdr_profile_ctx_t *ctx, uint8_t slot,
                     const np_uhdr_profile_record_t *r)
{
    memcpy(ctx->profile_id, r->profile_id, NP_UHDR_PROFILE_ID_LEN);
    ctx->slot = slot;
    ctx->open = true;
}

/* ── API ──────────────────────────────────────────────────────────────────── */

np_uhdr_status_t np_uhdr_profile_create(uint8_t slot,
                                        const uint8_t *credential, size_t cred_len,
                                        np_uhdr_profile_ctx_t *ctx,
                                        uint8_t recovery_code_out[NP_UHDR_RECOVERY_CODE_LEN])
{
    if (ctx == NULL || credential == NULL || cred_len == 0U ||
        recovery_code_out == NULL || slot >= NP_UHDR_PROFILE_MAX) {
        return NP_UHDR_ERR_INVALID;
    }
    fail_ctx(ctx);

    np_uhdr_profile_record_t r;
    uint8_t rk[NP_UHDR_RECOVERY_KEY_LEN];
    uint8_t aad[W2_AAD_LEN];
    np_uhdr_status_t st;

    st = np_uhdr_hal_profile_read(slot, &r);
    if (st == NP_UHDR_OK) {
        return NP_UHDR_ERR_EXISTS;          /* never overwrite a person's key */
    }
    if (st != NP_UHDR_ERR_EMPTY) {
        return NP_UHDR_ERR_CONFIG;
    }

    memset(&r, 0, sizeof(r));
    r.magic  = NP_UHDR_PROFILE_MAGIC;
    r.format = NP_UHDR_PROFILE_FORMAT;
    r.state  = NP_UHDR_PROFILE_LIVE;

    if (np_uhdr_hal_trng_generate(ctx->pk, NP_UHDR_PROFILE_KEY_LEN) != NP_UHDR_OK ||
        np_uhdr_hal_trng_generate(r.profile_id, NP_UHDR_PROFILE_ID_LEN) != NP_UHDR_OK ||
        np_uhdr_hal_trng_generate(recovery_code_out, NP_UHDR_RECOVERY_CODE_LEN) != NP_UHDR_OK ||
        np_uhdr_hal_trng_generate(r.rec_salt, NP_UHDR_RECOVERY_SALT_LEN) != NP_UHDR_OK ||
        np_uhdr_hal_trng_generate(r.w2_nonce, NP_UHDR_GCM_NONCE_LEN) != NP_UHDR_OK) {
        st = NP_UHDR_ERR_TRNG;
        goto fail;
    }

    st = seal_wrap1(&r, ctx->pk, credential, cred_len);
    if (st != NP_UHDR_OK) { goto fail; }

    st = derive_rk(recovery_code_out, r.rec_salt, rk);
    if (st != NP_UHDR_OK) { goto fail; }
    build_w2_aad(&r, aad);
    st = np_uhdr_hal_aes_gcm_encrypt(rk, r.w2_nonce, aad, W2_AAD_LEN,
                                     ctx->pk, NP_UHDR_PROFILE_KEY_LEN, r.w2_ct, r.w2_tag);
    np_uhdr_secure_zero(rk, sizeof(rk));
    if (st != NP_UHDR_OK) { st = NP_UHDR_ERR_CRYPTO; goto fail; }

    if (np_uhdr_hal_profile_write(slot, &r) != NP_UHDR_OK) {
        st = NP_UHDR_ERR_CONFIG;
        goto fail;
    }

    open_ctx(ctx, slot, &r);
    memset(&r, 0, sizeof(r));               /* ciphertext only, not secret */
    return NP_UHDR_OK;

fail:
    np_uhdr_secure_zero(rk, sizeof(rk));
    np_uhdr_secure_zero(recovery_code_out, NP_UHDR_RECOVERY_CODE_LEN);
    memset(&r, 0, sizeof(r));
    fail_ctx(ctx);
    return st;
}

np_uhdr_status_t np_uhdr_profile_unlock(uint8_t slot,
                                        const uint8_t *credential, size_t cred_len,
                                        np_uhdr_profile_ctx_t *ctx)
{
    if (ctx == NULL || credential == NULL || cred_len == 0U ||
        slot >= NP_UHDR_PROFILE_MAX) {
        return NP_UHDR_ERR_INVALID;
    }
    fail_ctx(ctx);

    np_uhdr_profile_record_t r;
    uint8_t wkmd[NP_UHDR_WKMD_LEN];
    uint8_t aad[W1_AAD_LEN];
    uint32_t failed = 0U;
    uint64_t now = 0U;
    np_uhdr_status_t st;

    st = np_uhdr_attempt_gate(&failed, &now);
    if (st != NP_UHDR_OK) {
        return st;
    }

    st = np_uhdr_hal_profile_read(slot, &r);
    if (st != NP_UHDR_OK) {
        memset(&r, 0, sizeof(r));
        return (st == NP_UHDR_ERR_EMPTY) ? NP_UHDR_ERR_EMPTY : NP_UHDR_ERR_CONFIG;
    }
    st = validate_record(&r);
    if (st != NP_UHDR_OK) {
        memset(&r, 0, sizeof(r));
        return st;
    }
    if (r.state != NP_UHDR_PROFILE_LIVE) {
        memset(&r, 0, sizeof(r));
        return NP_UHDR_ERR_STATE;           /* dormant: rebind with the code */
    }
    build_w1_aad(&r, aad);

    if (np_uhdr_attempt_charge(failed, now) != NP_UHDR_OK) {
        memset(&r, 0, sizeof(r));
        return NP_UHDR_ERR_CONFIG;
    }

    st = np_uhdr_derive_wkmd(credential, cred_len, r.kdf_salt, r.argon2id_version,
                             r.argon2id_m_cost, r.argon2id_t_cost,
                             r.argon2id_parallelism, wkmd);
    if (st != NP_UHDR_OK) {
        memset(&r, 0, sizeof(r));
        return NP_UHDR_ERR_KDF;
    }
    st = np_uhdr_hal_aes_gcm_decrypt(wkmd, r.w1_nonce, aad, W1_AAD_LEN,
                                     r.w1_ct, NP_UHDR_PROFILE_KEY_LEN, r.w1_tag, ctx->pk);
    np_uhdr_secure_zero(wkmd, sizeof(wkmd));
    if (st != NP_UHDR_OK) {
        memset(&r, 0, sizeof(r));
        fail_ctx(ctx);
        return (st == NP_UHDR_ERR_AUTH) ? NP_UHDR_ERR_AUTH : NP_UHDR_ERR_CRYPTO;
    }

    np_uhdr_attempt_clear();
    open_ctx(ctx, slot, &r);
    memset(&r, 0, sizeof(r));
    return NP_UHDR_OK;
}

np_uhdr_status_t np_uhdr_profile_rebind(uint8_t slot,
                                        const uint8_t recovery_code[NP_UHDR_RECOVERY_CODE_LEN],
                                        const uint8_t *new_credential, size_t new_cred_len,
                                        np_uhdr_profile_ctx_t *ctx)
{
    if (ctx == NULL || recovery_code == NULL || new_credential == NULL ||
        new_cred_len == 0U || slot >= NP_UHDR_PROFILE_MAX) {
        return NP_UHDR_ERR_INVALID;
    }
    fail_ctx(ctx);

    np_uhdr_profile_record_t r;
    uint8_t rk[NP_UHDR_RECOVERY_KEY_LEN];
    uint8_t aad[W2_AAD_LEN];
    uint32_t failed = 0U;
    uint64_t now = 0U;
    np_uhdr_status_t st;

    st = np_uhdr_attempt_gate(&failed, &now);
    if (st != NP_UHDR_OK) {
        return st;
    }

    st = np_uhdr_hal_profile_read(slot, &r);
    if (st != NP_UHDR_OK) {
        memset(&r, 0, sizeof(r));
        return (st == NP_UHDR_ERR_EMPTY) ? NP_UHDR_ERR_EMPTY : NP_UHDR_ERR_CONFIG;
    }
    st = validate_record(&r);
    if (st != NP_UHDR_OK) {
        memset(&r, 0, sizeof(r));
        return st;
    }
    build_w2_aad(&r, aad);

    if (np_uhdr_attempt_charge(failed, now) != NP_UHDR_OK) {
        memset(&r, 0, sizeof(r));
        return NP_UHDR_ERR_CONFIG;
    }

    st = derive_rk(recovery_code, r.rec_salt, rk);
    if (st != NP_UHDR_OK) {
        memset(&r, 0, sizeof(r));
        return st;
    }
    st = np_uhdr_hal_aes_gcm_decrypt(rk, r.w2_nonce, aad, W2_AAD_LEN,
                                     r.w2_ct, NP_UHDR_PROFILE_KEY_LEN, r.w2_tag, ctx->pk);
    np_uhdr_secure_zero(rk, sizeof(rk));
    if (st != NP_UHDR_OK) {
        memset(&r, 0, sizeof(r));
        fail_ctx(ctx);
        return (st == NP_UHDR_ERR_AUTH) ? NP_UHDR_ERR_AUTH : NP_UHDR_ERR_CRYPTO;
    }
    np_uhdr_attempt_clear();

    /* Bind to this helmet: a fresh wrap 1 under the new credential. */
    st = seal_wrap1(&r, ctx->pk, new_credential, new_cred_len);
    if (st != NP_UHDR_OK) {
        memset(&r, 0, sizeof(r));
        fail_ctx(ctx);
        return st;
    }
    r.state = NP_UHDR_PROFILE_LIVE;
    if (np_uhdr_hal_profile_write(slot, &r) != NP_UHDR_OK) {
        memset(&r, 0, sizeof(r));
        fail_ctx(ctx);
        return NP_UHDR_ERR_CONFIG;
    }

    open_ctx(ctx, slot, &r);
    memset(&r, 0, sizeof(r));
    return NP_UHDR_OK;
}

np_uhdr_status_t np_uhdr_profile_change_credential(np_uhdr_profile_ctx_t *ctx,
                                                   const uint8_t *new_credential,
                                                   size_t new_cred_len)
{
    if (ctx == NULL || new_credential == NULL || new_cred_len == 0U) {
        return NP_UHDR_ERR_INVALID;
    }
    if (!ctx->open || ctx->slot >= NP_UHDR_PROFILE_MAX) {
        return NP_UHDR_ERR_STATE;
    }

    np_uhdr_profile_record_t r;
    np_uhdr_status_t st = np_uhdr_hal_profile_read(ctx->slot, &r);
    if (st != NP_UHDR_OK) {
        memset(&r, 0, sizeof(r));
        return NP_UHDR_ERR_CONFIG;
    }
    st = validate_record(&r);
    if (st != NP_UHDR_OK || r.state != NP_UHDR_PROFILE_LIVE ||
        memcmp(r.profile_id, ctx->profile_id, NP_UHDR_PROFILE_ID_LEN) != 0) {
        memset(&r, 0, sizeof(r));
        return (st != NP_UHDR_OK) ? st : NP_UHDR_ERR_STATE;
    }

    st = seal_wrap1(&r, ctx->pk, new_credential, new_cred_len);
    if (st != NP_UHDR_OK) {
        memset(&r, 0, sizeof(r));
        return st;
    }
    st = np_uhdr_hal_profile_write(ctx->slot, &r);
    memset(&r, 0, sizeof(r));
    return (st == NP_UHDR_OK) ? NP_UHDR_OK : NP_UHDR_ERR_CONFIG;
}

void np_uhdr_profile_close(np_uhdr_profile_ctx_t *ctx)
{
    fail_ctx(ctx);
}

np_uhdr_status_t np_uhdr_profile_export(uint8_t slot, np_uhdr_profile_export_t *out)
{
    if (out == NULL || slot >= NP_UHDR_PROFILE_MAX) {
        return NP_UHDR_ERR_INVALID;
    }
    np_uhdr_profile_record_t r;
    np_uhdr_status_t st = np_uhdr_hal_profile_read(slot, &r);
    if (st != NP_UHDR_OK) {
        memset(&r, 0, sizeof(r));
        return (st == NP_UHDR_ERR_EMPTY) ? NP_UHDR_ERR_EMPTY : NP_UHDR_ERR_CONFIG;
    }
    st = validate_record(&r);
    if (st != NP_UHDR_OK) {
        memset(&r, 0, sizeof(r));
        return st;
    }
    zero_wrap1(&r);                         /* the device-bound wrap stays */
    r.state = NP_UHDR_PROFILE_DORMANT;
    memcpy(out, &r, sizeof(*out));
    memset(&r, 0, sizeof(r));
    return NP_UHDR_OK;
}

np_uhdr_status_t np_uhdr_profile_import(uint8_t slot, const np_uhdr_profile_export_t *in)
{
    if (in == NULL || slot >= NP_UHDR_PROFILE_MAX) {
        return NP_UHDR_ERR_INVALID;
    }
    if (in->state != NP_UHDR_PROFILE_DORMANT) {
        return NP_UHDR_ERR_INVALID;
    }
    np_uhdr_status_t st = validate_record(in);   /* includes wrap-1-is-zero */
    if (st != NP_UHDR_OK) {
        return NP_UHDR_ERR_INVALID;
    }

    np_uhdr_profile_record_t existing;
    st = np_uhdr_hal_profile_read(slot, &existing);
    memset(&existing, 0, sizeof(existing));
    if (st == NP_UHDR_OK) {
        return NP_UHDR_ERR_EXISTS;
    }
    if (st != NP_UHDR_ERR_EMPTY) {
        return NP_UHDR_ERR_CONFIG;
    }
    return (np_uhdr_hal_profile_write(slot, in) == NP_UHDR_OK) ? NP_UHDR_OK
                                                               : NP_UHDR_ERR_CONFIG;
}

np_uhdr_status_t np_uhdr_profile_delete(uint8_t slot, np_uhdr_profile_ctx_t *ctx)
{
    if (slot >= NP_UHDR_PROFILE_MAX) {
        return NP_UHDR_ERR_INVALID;
    }
    if (ctx != NULL && ctx->open && ctx->slot == slot) {
        fail_ctx(ctx);
    }
    return (np_uhdr_hal_profile_erase(slot) == NP_UHDR_OK) ? NP_UHDR_OK
                                                           : NP_UHDR_ERR_CONFIG;
}

/* ── Host test stubs (NPTEST_HOST) ───────────────────────────────────────────
 * NOT compiled into target firmware.  A RAM-backed slot table for the three
 * Config seams, and a SHA-256 stand-in for the recovery KDF:
 *   RK = SHA-256( code || salt || "NPRK" )
 * This is NOT cryptography; it makes a wrong code fail the wrap-2 tag so the
 * lifecycle is exercisable on host CI.
 */
#ifdef NPTEST_HOST

#include "np_crypto.h"

static np_uhdr_profile_record_t g_host_slots[NP_UHDR_PROFILE_MAX];
static bool g_host_slot_used[NP_UHDR_PROFILE_MAX];
static bool g_host_profile_write_fail = false;

np_uhdr_status_t np_uhdr_hal_profile_read(uint8_t profile, np_uhdr_profile_record_t *out)
{
    if (out == NULL || profile >= NP_UHDR_PROFILE_MAX) {
        return NP_UHDR_ERR_INVALID;
    }
    if (!g_host_slot_used[profile]) {
        return NP_UHDR_ERR_EMPTY;
    }
    memcpy(out, &g_host_slots[profile], sizeof(*out));
    return NP_UHDR_OK;
}

np_uhdr_status_t np_uhdr_hal_profile_write(uint8_t profile, const np_uhdr_profile_record_t *rec)
{
    if (rec == NULL || profile >= NP_UHDR_PROFILE_MAX) {
        return NP_UHDR_ERR_INVALID;
    }
    if (g_host_profile_write_fail) {
        return NP_UHDR_ERR_CONFIG;
    }
    memcpy(&g_host_slots[profile], rec, sizeof(*rec));
    g_host_slot_used[profile] = true;
    return NP_UHDR_OK;
}

np_uhdr_status_t np_uhdr_hal_profile_erase(uint8_t profile)
{
    if (profile >= NP_UHDR_PROFILE_MAX) {
        return NP_UHDR_ERR_INVALID;
    }
    memset(&g_host_slots[profile], 0, sizeof(g_host_slots[profile]));
    g_host_slot_used[profile] = false;
    return NP_UHDR_OK;
}

np_uhdr_status_t np_uhdr_hal_recovery_kdf(const uint8_t code[NP_UHDR_RECOVERY_CODE_LEN],
                                          const uint8_t salt[NP_UHDR_RECOVERY_SALT_LEN],
                                          uint8_t rk_out[NP_UHDR_RECOVERY_KEY_LEN])
{
    if (code == NULL || salt == NULL || rk_out == NULL) {
        return NP_UHDR_ERR_INVALID;
    }
    uint8_t buf[NP_UHDR_RECOVERY_CODE_LEN + NP_UHDR_RECOVERY_SALT_LEN + 4U];
    memcpy(buf, code, NP_UHDR_RECOVERY_CODE_LEN);
    memcpy(buf + NP_UHDR_RECOVERY_CODE_LEN, salt, NP_UHDR_RECOVERY_SALT_LEN);
    memcpy(buf + NP_UHDR_RECOVERY_CODE_LEN + NP_UHDR_RECOVERY_SALT_LEN, "NPRK", 4U);
    np_sha256(buf, (uint32_t)sizeof(buf), rk_out);
    return NP_UHDR_OK;
}

/* Test-only controls. */
np_uhdr_profile_record_t *np_uhdr_host_profile_slot_mut(uint8_t slot) { return &g_host_slots[slot]; }
bool np_uhdr_host_profile_slot_used(uint8_t slot) { return g_host_slot_used[slot]; }
void np_uhdr_host_profile_write_fail(bool f) { g_host_profile_write_fail = f; }

void np_uhdr_host_profile_reset(void)
{
    memset(g_host_slots, 0, sizeof(g_host_slots));
    memset(g_host_slot_used, 0, sizeof(g_host_slot_used));
    g_host_profile_write_fail = false;
}

#endif /* NPTEST_HOST */
