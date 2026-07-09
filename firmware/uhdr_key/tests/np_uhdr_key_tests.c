/*
 * NeurOne Two-Layer UHDR Key Scheme — Host Tests
 * Document: NP-FW-EMMC-002 Rev A §C
 *
 * Exercises provision → unlock → background → change_credential on the host
 * using the NPTEST_HOST HAL stubs (Argon2id/GCM modeled with np_crypto
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

    if (g_fail_count == 0) {
        printf("PASS: all np_uhdr_key tests\n");
    } else {
        printf("FAILED: %d assertion(s)\n", g_fail_count);
    }
    return g_fail_count;
}
