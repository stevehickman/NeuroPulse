/*
 * NeurOne Two-Layer UHDR Key Scheme — Host Tests
 * Document: NP-FW-EMMC-002 Rev 1 §C  (+ 2026-07-08 privacy-review hardening)
 *
 * Exercises provision → unlock → background → change_credential on the host
 * using the NPTEST_HOST HAL stubs (Argon2id/hw_bind/GCM modeled with np_crypto
 * SHA-256, so the wrap authenticates).  Verifies the security invariants:
 *
 *   T1  record is byte-exact 192 bytes (§C.3).
 *   T2  provision leaves ctx unlocked, mounts with the UKMD, wraps (ciphertext
 *       != plaintext UKMD), and writes the fixed Argon2id parameters.
 *   T3  background zeroes the UKMD and clears unlocked (idempotent / NULL-safe).
 *   T4  unlock with the correct credential recovers the exact UKMD (§C.4).
 *   T5  unlock with a WRONG credential returns AUTH and leaves ctx locked +
 *       zeroed (§C.4 — real probe via the authenticating stub).
 *   T6  change_credential re-wraps the SAME UKMD without touching UHDR (no
 *       remount); then NEW cred unlocks and OLD cred fails AUTH (§C.5).
 *   T7  mount HAL is called with the UKMD, never the WKMD (§C.4 step 5).
 *   T8  invalid args / locked-state precondition are rejected.
 *   T9  hardware binding (Finding 1): the SAME credential + record on a
 *       DIFFERENT device key fails AUTH; restoring the device key unlocks.
 *   T10 KDF-parameter rollback (Finding 2): a record with below-floor params is
 *       rejected with NP_UHDR_ERR_PARAM before any KDF work.
 *   T11 AAD binding (Finding 2): tampering an above-floor param or the salt
 *       fails AUTH (params + salt are authenticated).
 *   T12 unlock scrubs a prior UKMD on an early-exit failure (Finding 3).
 *
 * Return convention: 0 = PASS, non-zero = failure count.
 */

#include "np_uhdr_key.h"
#include "np_uhdr_key_config.h"

#include <stdio.h>
#include <string.h>

/* Test-only accessors from np_uhdr_key.c (NPTEST_HOST). */
extern const uint8_t          *np_uhdr_host_last_mounted_key(void);
extern uint32_t                np_uhdr_host_mount_count(void);
extern const np_ukmd_record_t *np_uhdr_host_config(void);
extern np_ukmd_record_t       *np_uhdr_host_config_mut(void);
extern void                    np_uhdr_host_set_device_key(const uint8_t *key);
extern void                    np_uhdr_host_reset(void);

static int g_fail_count = 0;

#define ASSERT(cond, msg)                                            \
    do {                                                             \
        if (!(cond)) {                                              \
            printf("FAIL [%s:%d] %s\n", __func__, __LINE__, (msg)); \
            g_fail_count++;                                         \
        }                                                          \
    } while (0)

static const uint8_t CRED_A[]  = { 'p', 'i', 'n', '-', 'a', 'l', 'p', 'h', 'a' };
static const uint8_t CRED_B[]  = { 'b', 'i', 'o', '-', 'b', 'e', 't', 'a' };

/* A second simulated device's fused key. */
static const uint8_t DEVKEY_2[NP_UHDR_HWKEY_LEN] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00,
    0x0F, 0x1E, 0x2D, 0x3C, 0x4B, 0x5A, 0x69, 0x78,
    0x87, 0x96, 0xA5, 0xB4, 0xC3, 0xD2, 0xE1, 0xF0,
};

static int buf_is_zero(const uint8_t *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (b[i] != 0U) return 0;
    }
    return 1;
}

static void test_record_size(void)
{
    ASSERT(sizeof(np_ukmd_record_t) == NP_UHDR_UKMD_RECORD_SIZE,
           "np_ukmd_record_t must be exactly 192 bytes");
    ASSERT(NP_UHDR_UKMD_RECORD_SIZE == 192U, "record size constant is 192");
}

static void test_provision(void)
{
    np_uhdr_host_reset();
    np_uhdr_key_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    ASSERT(np_uhdr_key_provision(&ctx, CRED_A, sizeof(CRED_A)) == NP_UHDR_OK,
           "provision should succeed");
    ASSERT(ctx.unlocked, "ctx should be unlocked after provision");
    ASSERT(!buf_is_zero(ctx.ukmd, NP_UHDR_UKMD_LEN),
           "UKMD should be populated after provision");

    const np_ukmd_record_t *rec = np_uhdr_host_config();
    ASSERT(rec->argon2id_version == NP_UHDR_ARGON2ID_VERSION, "argon2 version");
    ASSERT(rec->argon2id_m_cost == NP_UHDR_ARGON2ID_M_COST, "argon2 m_cost");
    ASSERT(rec->argon2id_t_cost == NP_UHDR_ARGON2ID_T_COST, "argon2 t_cost");
    ASSERT(rec->argon2id_parallelism == NP_UHDR_ARGON2ID_PARALLELISM,
           "argon2 parallelism");
    /* Wrapped: the stored ciphertext must not equal the plaintext UKMD. */
    ASSERT(memcmp(rec->ukmd_ciphertext, ctx.ukmd, NP_UHDR_UKMD_LEN) != 0,
           "Config ciphertext must not equal plaintext UKMD (ISC-16)");

    np_uhdr_key_background(&ctx);
}

static void test_background(void)
{
    np_uhdr_host_reset();
    np_uhdr_key_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ASSERT(np_uhdr_key_provision(&ctx, CRED_A, sizeof(CRED_A)) == NP_UHDR_OK,
           "provision for background");

    np_uhdr_key_background(&ctx);
    ASSERT(buf_is_zero(ctx.ukmd, NP_UHDR_UKMD_LEN),
           "UKMD zeroed after background");
    ASSERT(!ctx.unlocked, "unlocked cleared after background");

    /* Idempotent + NULL-safe. */
    np_uhdr_key_background(&ctx);
    np_uhdr_key_background(NULL);
}

static void test_unlock_correct(void)
{
    np_uhdr_host_reset();
    np_uhdr_key_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ASSERT(np_uhdr_key_provision(&ctx, CRED_A, sizeof(CRED_A)) == NP_UHDR_OK,
           "provision for unlock");

    uint8_t ukmd_snapshot[NP_UHDR_UKMD_LEN];
    memcpy(ukmd_snapshot, ctx.ukmd, NP_UHDR_UKMD_LEN);

    np_uhdr_key_background(&ctx);
    ASSERT(buf_is_zero(ctx.ukmd, NP_UHDR_UKMD_LEN), "UKMD gone after background");

    uint32_t mounts_before = np_uhdr_host_mount_count();
    ASSERT(np_uhdr_key_unlock(&ctx, CRED_A, sizeof(CRED_A)) == NP_UHDR_OK,
           "unlock with correct credential succeeds");
    ASSERT(ctx.unlocked, "ctx unlocked after unlock");
    ASSERT(memcmp(ctx.ukmd, ukmd_snapshot, NP_UHDR_UKMD_LEN) == 0,
           "unlock recovers the exact provisioned UKMD");
    ASSERT(np_uhdr_host_mount_count() == mounts_before + 1U,
           "unlock mounts the UHDR partition once");

    np_uhdr_key_background(&ctx);
}

static void test_unlock_wrong(void)
{
    np_uhdr_host_reset();
    np_uhdr_key_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ASSERT(np_uhdr_key_provision(&ctx, CRED_A, sizeof(CRED_A)) == NP_UHDR_OK,
           "provision for wrong-cred");
    np_uhdr_key_background(&ctx);

    uint32_t mounts_before = np_uhdr_host_mount_count();
    ASSERT(np_uhdr_key_unlock(&ctx, CRED_B, sizeof(CRED_B)) == NP_UHDR_ERR_AUTH,
           "unlock with wrong credential returns AUTH");
    ASSERT(!ctx.unlocked, "ctx stays locked after wrong credential");
    ASSERT(buf_is_zero(ctx.ukmd, NP_UHDR_UKMD_LEN),
           "UKMD zeroed after failed unlock");
    ASSERT(np_uhdr_host_mount_count() == mounts_before,
           "no mount on failed unlock");
}

static void test_change_credential(void)
{
    np_uhdr_host_reset();
    np_uhdr_key_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ASSERT(np_uhdr_key_provision(&ctx, CRED_A, sizeof(CRED_A)) == NP_UHDR_OK,
           "provision for change-cred");

    uint8_t ukmd_snapshot[NP_UHDR_UKMD_LEN];
    memcpy(ukmd_snapshot, ctx.ukmd, NP_UHDR_UKMD_LEN);

    /* Must be unlocked to re-wrap. */
    uint32_t mounts_before = np_uhdr_host_mount_count();
    ASSERT(np_uhdr_key_change_credential(&ctx, CRED_B, sizeof(CRED_B)) == NP_UHDR_OK,
           "change_credential succeeds while unlocked");
    ASSERT(ctx.unlocked, "ctx stays unlocked after change");
    ASSERT(memcmp(ctx.ukmd, ukmd_snapshot, NP_UHDR_UKMD_LEN) == 0,
           "UKMD unchanged across credential change");
    ASSERT(np_uhdr_host_mount_count() == mounts_before,
           "change_credential does NOT remount / re-encrypt UHDR (§C.5)");

    /* New credential now unlocks; old credential must fail. */
    np_uhdr_key_background(&ctx);
    ASSERT(np_uhdr_key_unlock(&ctx, CRED_B, sizeof(CRED_B)) == NP_UHDR_OK,
           "new credential unlocks after change");
    ASSERT(memcmp(ctx.ukmd, ukmd_snapshot, NP_UHDR_UKMD_LEN) == 0,
           "new-credential unlock recovers original UKMD");

    np_uhdr_key_background(&ctx);
    ASSERT(np_uhdr_key_unlock(&ctx, CRED_A, sizeof(CRED_A)) == NP_UHDR_ERR_AUTH,
           "old credential fails after change");

    np_uhdr_key_background(&ctx);
}

static void test_mount_uses_ukmd(void)
{
    np_uhdr_host_reset();
    np_uhdr_key_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ASSERT(np_uhdr_key_provision(&ctx, CRED_A, sizeof(CRED_A)) == NP_UHDR_OK,
           "provision for mount-key check");

    ASSERT(memcmp(np_uhdr_host_last_mounted_key(), ctx.ukmd, NP_UHDR_UKMD_LEN) == 0,
           "UHDR mounted with the UKMD, not the WKMD (ISC-10)");

    np_uhdr_key_background(&ctx);
}

static void test_invalid_args(void)
{
    np_uhdr_host_reset();
    np_uhdr_key_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    ASSERT(np_uhdr_key_provision(NULL, CRED_A, sizeof(CRED_A)) == NP_UHDR_ERR_INVALID,
           "provision NULL ctx invalid");
    ASSERT(np_uhdr_key_provision(&ctx, NULL, 4) == NP_UHDR_ERR_INVALID,
           "provision NULL cred invalid");
    ASSERT(np_uhdr_key_provision(&ctx, CRED_A, 0) == NP_UHDR_ERR_INVALID,
           "provision zero cred_len invalid");

    ASSERT(np_uhdr_key_unlock(NULL, CRED_A, sizeof(CRED_A)) == NP_UHDR_ERR_INVALID,
           "unlock NULL ctx invalid");
    ASSERT(np_uhdr_key_unlock(&ctx, NULL, 4) == NP_UHDR_ERR_INVALID,
           "unlock NULL cred invalid");

    /* change_credential requires unlocked state. */
    memset(&ctx, 0, sizeof(ctx));
    ASSERT(np_uhdr_key_change_credential(&ctx, CRED_B, sizeof(CRED_B))
               == NP_UHDR_ERR_STATE,
           "change_credential while locked returns STATE");
    ASSERT(np_uhdr_key_change_credential(NULL, CRED_B, sizeof(CRED_B))
               == NP_UHDR_ERR_INVALID,
           "change_credential NULL ctx invalid");
}

/* Finding 1 — hardware binding: same credential + record, different device. */
static void test_hardware_binding(void)
{
    np_uhdr_host_reset();
    np_uhdr_key_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ASSERT(np_uhdr_key_provision(&ctx, CRED_A, sizeof(CRED_A)) == NP_UHDR_OK,
           "provision on device 1");
    uint8_t ukmd_snapshot[NP_UHDR_UKMD_LEN];
    memcpy(ukmd_snapshot, ctx.ukmd, NP_UHDR_UKMD_LEN);
    np_uhdr_key_background(&ctx);

    /* Move the record to a different device (different fused key). */
    np_uhdr_host_set_device_key(DEVKEY_2);
    ASSERT(np_uhdr_key_unlock(&ctx, CRED_A, sizeof(CRED_A)) == NP_UHDR_ERR_AUTH,
           "correct credential on a DIFFERENT device fails (offline-attack defeated)");
    ASSERT(buf_is_zero(ctx.ukmd, NP_UHDR_UKMD_LEN),
           "UKMD scrubbed after foreign-device unlock");

    /* Back on the original device, the same credential unlocks. */
    np_uhdr_host_reset();   /* restores the default device key */
    ASSERT(np_uhdr_key_provision(&ctx, CRED_A, sizeof(CRED_A)) == NP_UHDR_OK,
           "re-provision on device 1");
    memcpy(ukmd_snapshot, ctx.ukmd, NP_UHDR_UKMD_LEN);
    np_uhdr_key_background(&ctx);
    ASSERT(np_uhdr_key_unlock(&ctx, CRED_A, sizeof(CRED_A)) == NP_UHDR_OK,
           "same credential on the original device unlocks");
    ASSERT(memcmp(ctx.ukmd, ukmd_snapshot, NP_UHDR_UKMD_LEN) == 0,
           "device-1 unlock recovers the UKMD");
    np_uhdr_key_background(&ctx);
}

/* Finding 2 — a below-floor KDF parameter is rejected before any KDF work. */
static void test_param_rollback(void)
{
    np_uhdr_host_reset();
    np_uhdr_key_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ASSERT(np_uhdr_key_provision(&ctx, CRED_A, sizeof(CRED_A)) == NP_UHDR_OK,
           "provision for param-rollback");
    np_uhdr_key_background(&ctx);

    /* Roll the memory cost back to 1 KiB. */
    np_uhdr_host_config_mut()->argon2id_m_cost = 1U;
    ASSERT(np_uhdr_key_unlock(&ctx, CRED_A, sizeof(CRED_A)) == NP_UHDR_ERR_PARAM,
           "below-floor m_cost rejected with PARAM");
    ASSERT(buf_is_zero(ctx.ukmd, NP_UHDR_UKMD_LEN), "UKMD scrubbed on PARAM");

    /* Restore m_cost, corrupt the version instead. */
    np_uhdr_host_config_mut()->argon2id_m_cost = NP_UHDR_ARGON2ID_M_COST;
    np_uhdr_host_config_mut()->argon2id_version = 0x10U;
    ASSERT(np_uhdr_key_unlock(&ctx, CRED_A, sizeof(CRED_A)) == NP_UHDR_ERR_PARAM,
           "wrong argon2 version rejected with PARAM");
}

/* Finding 2 — salt and above-floor params are authenticated via AAD. */
static void test_aad_binding(void)
{
    np_uhdr_host_reset();
    np_uhdr_key_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ASSERT(np_uhdr_key_provision(&ctx, CRED_A, sizeof(CRED_A)) == NP_UHDR_OK,
           "provision for aad-binding");
    np_uhdr_key_background(&ctx);

    /* Above-floor param change passes validation but breaks the AAD. */
    np_uhdr_host_config_mut()->argon2id_m_cost = NP_UHDR_ARGON2ID_M_COST * 2U;
    ASSERT(np_uhdr_key_unlock(&ctx, CRED_A, sizeof(CRED_A)) == NP_UHDR_ERR_AUTH,
           "above-floor param tamper fails AUTH via AAD");
    np_uhdr_host_config_mut()->argon2id_m_cost = NP_UHDR_ARGON2ID_M_COST;

    /* Flipping a salt byte changes both derivation and AAD → AUTH. */
    np_uhdr_host_config_mut()->argon2id_salt[0] ^= 0xFFU;
    ASSERT(np_uhdr_key_unlock(&ctx, CRED_A, sizeof(CRED_A)) == NP_UHDR_ERR_AUTH,
           "tampered salt fails AUTH");
    ASSERT(buf_is_zero(ctx.ukmd, NP_UHDR_UKMD_LEN), "UKMD scrubbed after AAD fail");
}

/* Finding 3 — an early-exit failure scrubs a previously-live UKMD. */
static void test_unlock_scrubs_on_early_exit(void)
{
    np_uhdr_host_reset();
    np_uhdr_key_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ASSERT(np_uhdr_key_provision(&ctx, CRED_A, sizeof(CRED_A)) == NP_UHDR_OK,
           "provision so a UKMD is live");
    ASSERT(!buf_is_zero(ctx.ukmd, NP_UHDR_UKMD_LEN), "UKMD live pre-check");

    /* Corrupt params so the very next unlock takes the PARAM early-exit while a
     * prior UKMD is still resident in ctx. */
    np_uhdr_host_config_mut()->argon2id_t_cost = 1U;
    ASSERT(np_uhdr_key_unlock(&ctx, CRED_A, sizeof(CRED_A)) == NP_UHDR_ERR_PARAM,
           "early-exit path taken (PARAM)");
    ASSERT(buf_is_zero(ctx.ukmd, NP_UHDR_UKMD_LEN),
           "prior UKMD scrubbed on early-exit failure (Finding 3)");
}

/* CR-3 — a re-wrap draws a fresh, distinct GCM nonce (no nonce reuse), and the
 * provisioning draws are distinct from one another. */
static void test_nonce_uniqueness(void)
{
    np_uhdr_host_reset();
    np_uhdr_key_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ASSERT(np_uhdr_key_provision(&ctx, CRED_A, sizeof(CRED_A)) == NP_UHDR_OK,
           "provision for nonce-uniqueness");

    uint8_t nonce1[NP_UHDR_GCM_NONCE_LEN];
    uint8_t salt1[NP_UHDR_SALT_LEN];
    memcpy(nonce1, np_uhdr_host_config()->ukmd_nonce, NP_UHDR_GCM_NONCE_LEN);
    memcpy(salt1, np_uhdr_host_config()->argon2id_salt, NP_UHDR_SALT_LEN);

    /* Provisioning draws (salt vs nonce) must be distinct — the old
     * deterministic stub made every TRNG call identical. */
    ASSERT(memcmp(nonce1, salt1, NP_UHDR_GCM_NONCE_LEN) != 0,
           "provision nonce distinct from salt (TRNG no longer constant)");

    ASSERT(np_uhdr_key_change_credential(&ctx, CRED_B, sizeof(CRED_B)) == NP_UHDR_OK,
           "change_credential for nonce-uniqueness");
    uint8_t nonce2[NP_UHDR_GCM_NONCE_LEN];
    memcpy(nonce2, np_uhdr_host_config()->ukmd_nonce, NP_UHDR_GCM_NONCE_LEN);

    ASSERT(memcmp(nonce1, nonce2, NP_UHDR_GCM_NONCE_LEN) != 0,
           "re-wrap uses a fresh, distinct GCM nonce (no nonce reuse)");

    np_uhdr_key_background(&ctx);
}

int main(void)
{
    test_record_size();
    test_provision();
    test_background();
    test_unlock_correct();
    test_unlock_wrong();
    test_change_credential();
    test_mount_uses_ukmd();
    test_invalid_args();
    test_hardware_binding();
    test_param_rollback();
    test_aad_binding();
    test_unlock_scrubs_on_early_exit();
    test_nonce_uniqueness();

    if (g_fail_count == 0) {
        printf("PASS: all np_uhdr_key tests\n");
    } else {
        printf("FAILED: %d assertion(s)\n", g_fail_count);
    }
    return g_fail_count;
}
