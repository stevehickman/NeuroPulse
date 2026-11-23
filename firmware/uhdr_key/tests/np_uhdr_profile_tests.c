/*
 * NeurOne Per-Person UHDR Keys — Host Tests
 * Document: NP-FW-EMMC-002 §C.8 (OI-PRIV-34, NP-PRIV-ANALYSIS-004)
 *
 * Exercises the profile key store on the host with the NPTEST_HOST stubs
 * (np_uhdr_key.c for TRNG / Argon2id / hw_bind / GCM; np_uhdr_profile.c for the
 * slot table and the recovery KDF).  The stubs authenticate, so a wrong
 * credential, a wrong recovery code, another helmet's fused key or a tampered
 * record really fail the tag.
 *
 * Return convention: 0 = PASS, non-zero = failure count.
 */

#include "np_uhdr_key.h"
#include "np_uhdr_profile.h"

#include <stdio.h>
#include <string.h>

/* Test-only controls (NPTEST_HOST). */
extern void np_uhdr_host_reset(void);
extern void np_uhdr_host_set_device_key(const uint8_t *key);
extern uint32_t np_uhdr_host_attempts(void);
extern void np_uhdr_host_set_uptime_ms(uint64_t ms);
extern void np_uhdr_host_attempts_write_fail(bool fail);
extern np_uhdr_status_t np_uhdr_hal_attempts_write(uint32_t failed);
extern np_uhdr_profile_record_t *np_uhdr_host_profile_slot_mut(uint8_t slot);
extern bool np_uhdr_host_profile_slot_used(uint8_t slot);
extern void np_uhdr_host_profile_write_fail(bool fail);
extern void np_uhdr_host_profile_reset(void);

static int g_fail_count = 0;

#define ASSERT(cond, msg)                                            \
    do {                                                             \
        if (!(cond)) {                                              \
            printf("FAIL [%s:%d] %s\n", __func__, __LINE__, (msg)); \
            g_fail_count++;                                         \
        }                                                          \
    } while (0)

static const uint8_t CRED_ANA[] = { 'a', 'n', 'a', '-', 'p', 'h', 'o', 'n', 'e' };
static const uint8_t CRED_BEN[] = { 'b', 'e', 'n', '-', 'p', 'a', 's', 's' };
static const uint8_t CRED_NEW[] = { 'n', 'e', 'w', '-', 'h', 'e', 'l', 'm' };

static const uint8_t DEVKEY_2[NP_UHDR_HWKEY_LEN] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00,
    0x0F, 0x1E, 0x2D, 0x3C, 0x4B, 0x5A, 0x69, 0x78,
    0x87, 0x96, 0xA5, 0xB4, 0xC3, 0xD2, 0xE1, 0xF0,
};

static int is_zero(const uint8_t *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (b[i] != 0U) return 0;
    }
    return 1;
}

/* Fresh helmet: no records, a readable zero attempt count, the default key. */
static void fresh(void)
{
    np_uhdr_host_reset();
    np_uhdr_host_profile_reset();
    (void)np_uhdr_hal_attempts_write(0U);
}

/* @verifies-none harness self-check: the on-flash record is byte-exact */
static void test_record_size(void)
{
    ASSERT(sizeof(np_uhdr_profile_record_t) == NP_UHDR_PROFILE_RECORD_SIZE,
           "profile record is 212 bytes");
}

/* @verifies REQ-EMMC2-02#a */
static void test_create_generates_per_person_secrets(void)
{
    fresh();
    np_uhdr_profile_ctx_t a, b;
    uint8_t code_a[NP_UHDR_RECOVERY_CODE_LEN], code_b[NP_UHDR_RECOVERY_CODE_LEN];

    ASSERT(np_uhdr_profile_create(0, CRED_ANA, sizeof(CRED_ANA), &a, code_a) == NP_UHDR_OK,
           "create slot 0");
    ASSERT(np_uhdr_profile_create(1, CRED_BEN, sizeof(CRED_BEN), &b, code_b) == NP_UHDR_OK,
           "create slot 1");
    ASSERT(a.open && b.open, "both left open");
    ASSERT(memcmp(a.pk, b.pk, NP_UHDR_PROFILE_KEY_LEN) != 0, "each person has a different key");
    ASSERT(memcmp(a.profile_id, b.profile_id, NP_UHDR_PROFILE_ID_LEN) != 0, "different ids");
    ASSERT(memcmp(code_a, code_b, NP_UHDR_RECOVERY_CODE_LEN) != 0, "different recovery codes");

    const np_uhdr_profile_record_t *r = np_uhdr_host_profile_slot_mut(0);
    ASSERT(r->state == NP_UHDR_PROFILE_LIVE, "a created profile is LIVE");
    ASSERT(memcmp(r->w1_ct, a.pk, NP_UHDR_PROFILE_KEY_LEN) != 0, "wrap 1 is not the key");
    ASSERT(memcmp(r->w2_ct, a.pk, NP_UHDR_PROFILE_KEY_LEN) != 0, "wrap 2 is not the key");

    np_uhdr_profile_ctx_t c;
    uint8_t code_c[NP_UHDR_RECOVERY_CODE_LEN];
    ASSERT(np_uhdr_profile_create(0, CRED_BEN, sizeof(CRED_BEN), &c, code_c) == NP_UHDR_ERR_EXISTS,
           "an occupied slot is never overwritten");
    ASSERT(!c.open, "the refused create leaves no key open");
    ASSERT(np_uhdr_profile_create(NP_UHDR_PROFILE_MAX, CRED_BEN, sizeof(CRED_BEN), &c, code_c)
               == NP_UHDR_ERR_INVALID, "a slot beyond the cap is refused");
}

/* @verifies REQ-EMMC2-02#b */
static void test_one_persons_credential_opens_only_their_profile(void)
{
    fresh();
    np_uhdr_profile_ctx_t a, b, x;
    uint8_t code[NP_UHDR_RECOVERY_CODE_LEN];
    uint8_t pk_a[NP_UHDR_PROFILE_KEY_LEN];

    (void)np_uhdr_profile_create(0, CRED_ANA, sizeof(CRED_ANA), &a, code);
    memcpy(pk_a, a.pk, sizeof(pk_a));
    np_uhdr_profile_close(&a);
    ASSERT(!a.open && is_zero(a.pk, sizeof(a.pk)), "close zeroes the key");
    (void)np_uhdr_profile_create(1, CRED_BEN, sizeof(CRED_BEN), &b, code);
    np_uhdr_profile_close(&b);

    ASSERT(np_uhdr_profile_unlock(0, CRED_ANA, sizeof(CRED_ANA), &x) == NP_UHDR_OK,
           "Ana's credential opens Ana's profile");
    ASSERT(memcmp(x.pk, pk_a, sizeof(pk_a)) == 0, "and recovers the same key");
    np_uhdr_profile_close(&x);

    ASSERT(np_uhdr_profile_unlock(0, CRED_BEN, sizeof(CRED_BEN), &x) == NP_UHDR_ERR_AUTH,
           "Ben's credential does not open Ana's profile");
    ASSERT(!x.open && is_zero(x.pk, sizeof(x.pk)), "a refused unlock leaves nothing open");
    ASSERT(np_uhdr_profile_unlock(2, CRED_ANA, sizeof(CRED_ANA), &x) == NP_UHDR_ERR_EMPTY,
           "an empty slot opens nothing");
}

/* @verifies REQ-EMMC2-02#c */
static void test_recovery_code_is_not_stored(void)
{
    fresh();
    np_uhdr_profile_ctx_t a;
    uint8_t code[NP_UHDR_RECOVERY_CODE_LEN];
    (void)np_uhdr_profile_create(0, CRED_ANA, sizeof(CRED_ANA), &a, code);

    const uint8_t *rec = (const uint8_t *)np_uhdr_host_profile_slot_mut(0);
    int found = 0;
    for (size_t off = 0; off + NP_UHDR_RECOVERY_CODE_LEN <= NP_UHDR_PROFILE_RECORD_SIZE; off++) {
        if (memcmp(rec + off, code, NP_UHDR_RECOVERY_CODE_LEN) == 0) { found = 1; }
    }
    ASSERT(!found, "the recovery code appears nowhere in the stored record");

    /* A create that fails returns no code. */
    fresh();
    np_uhdr_host_profile_write_fail(true);
    memset(code, 0xEE, sizeof(code));
    ASSERT(np_uhdr_profile_create(0, CRED_ANA, sizeof(CRED_ANA), &a, code) == NP_UHDR_ERR_CONFIG,
           "a create whose record cannot be written fails");
    ASSERT(is_zero(code, sizeof(code)) && !a.open, "and hands out no code and no key");
}

/* @verifies REQ-EMMC2-02#d */
static void test_delete_destroys_the_profile(void)
{
    fresh();
    np_uhdr_profile_ctx_t a;
    uint8_t code[NP_UHDR_RECOVERY_CODE_LEN];
    (void)np_uhdr_profile_create(0, CRED_ANA, sizeof(CRED_ANA), &a, code);

    ASSERT(np_uhdr_profile_delete(0, &a) == NP_UHDR_OK, "delete");
    ASSERT(!a.open && is_zero(a.pk, sizeof(a.pk)), "the open context on that slot is zeroed");
    ASSERT(!np_uhdr_host_profile_slot_used(0), "the record is gone");
    ASSERT(np_uhdr_profile_unlock(0, CRED_ANA, sizeof(CRED_ANA), &a) == NP_UHDR_ERR_EMPTY,
           "the credential opens nothing");
    ASSERT(np_uhdr_profile_rebind(0, code, CRED_ANA, sizeof(CRED_ANA), &a) == NP_UHDR_ERR_EMPTY,
           "nor does the recovery code");
}

/* @verifies REQ-EMMC2-03#a */
static void test_record_is_bound_to_the_helmet(void)
{
    fresh();
    np_uhdr_profile_ctx_t a;
    uint8_t code[NP_UHDR_RECOVERY_CODE_LEN];
    (void)np_uhdr_profile_create(0, CRED_ANA, sizeof(CRED_ANA), &a, code);
    np_uhdr_profile_close(&a);

    np_uhdr_host_set_device_key(DEVKEY_2);   /* the same record on another helmet */
    ASSERT(np_uhdr_profile_unlock(0, CRED_ANA, sizeof(CRED_ANA), &a) == NP_UHDR_ERR_AUTH,
           "the right credential does not open a copied record on another helmet");
}

/* @verifies REQ-EMMC2-03#b */
static void test_export_carries_no_unlock_wrap(void)
{
    fresh();
    np_uhdr_profile_ctx_t a;
    uint8_t code[NP_UHDR_RECOVERY_CODE_LEN];
    np_uhdr_profile_export_t ex;
    (void)np_uhdr_profile_create(0, CRED_ANA, sizeof(CRED_ANA), &a, code);

    ASSERT(np_uhdr_profile_export(0, &ex) == NP_UHDR_OK, "export");
    ASSERT(ex.state == NP_UHDR_PROFILE_DORMANT, "an export is dormant");
    ASSERT(is_zero(ex.kdf_salt, sizeof(ex.kdf_salt)) && is_zero(ex.w1_nonce, sizeof(ex.w1_nonce)) &&
           is_zero(ex.w1_ct, sizeof(ex.w1_ct)) && is_zero(ex.w1_tag, sizeof(ex.w1_tag)) &&
           ex.argon2id_m_cost == 0U, "an export holds no wrap-1 byte");
    ASSERT(np_uhdr_host_profile_slot_mut(0)->state == NP_UHDR_PROFILE_LIVE,
           "exporting leaves the helmet's own record live");
}

/* @verifies REQ-EMMC2-03#c */
static void test_wrap1_binds_id_salt_and_parameters(void)
{
    np_uhdr_profile_ctx_t a;
    uint8_t code[NP_UHDR_RECOVERY_CODE_LEN];

    fresh();
    (void)np_uhdr_profile_create(0, CRED_ANA, sizeof(CRED_ANA), &a, code);
    np_uhdr_host_profile_slot_mut(0)->profile_id[0] ^= 0x01U;
    ASSERT(np_uhdr_profile_unlock(0, CRED_ANA, sizeof(CRED_ANA), &a) == NP_UHDR_ERR_AUTH,
           "a changed profile id fails the tag");

    fresh();
    (void)np_uhdr_profile_create(0, CRED_ANA, sizeof(CRED_ANA), &a, code);
    np_uhdr_host_profile_slot_mut(0)->kdf_salt[3] ^= 0x80U;
    ASSERT(np_uhdr_profile_unlock(0, CRED_ANA, sizeof(CRED_ANA), &a) == NP_UHDR_ERR_AUTH,
           "a changed salt fails the tag");

    fresh();
    (void)np_uhdr_profile_create(0, CRED_ANA, sizeof(CRED_ANA), &a, code);
    np_uhdr_host_profile_slot_mut(0)->argon2id_t_cost += 1U;
    ASSERT(np_uhdr_profile_unlock(0, CRED_ANA, sizeof(CRED_ANA), &a) == NP_UHDR_ERR_AUTH,
           "a raised parameter fails the tag");

    fresh();
    (void)np_uhdr_profile_create(0, CRED_ANA, sizeof(CRED_ANA), &a, code);
    np_uhdr_host_profile_slot_mut(0)->argon2id_m_cost = NP_UHDR_ARGON2ID_M_COST_MIN - 1U;
    ASSERT(np_uhdr_profile_unlock(0, CRED_ANA, sizeof(CRED_ANA), &a) == NP_UHDR_ERR_PARAM,
           "a parameter below the floor is refused before any KDF");
    ASSERT(np_uhdr_host_attempts() == 0U, "and is not charged");
}

/* @verifies REQ-EMMC2-04#a */
static void test_export_opens_with_nothing_the_clinic_holds(void)
{
    fresh();
    np_uhdr_profile_ctx_t a, x;
    uint8_t code[NP_UHDR_RECOVERY_CODE_LEN];
    np_uhdr_profile_export_t ex;
    (void)np_uhdr_profile_create(0, CRED_ANA, sizeof(CRED_ANA), &a, code);
    np_uhdr_profile_close(&a);

    ASSERT(np_uhdr_profile_export(0, &ex) == NP_UHDR_OK, "the clinic exports with no credential");

    /* The clinic imports it into a replacement helmet. */
    np_uhdr_host_profile_reset();
    np_uhdr_host_set_device_key(DEVKEY_2);
    ASSERT(np_uhdr_profile_import(0, &ex) == NP_UHDR_OK, "import into the replacement");
    ASSERT(np_uhdr_profile_unlock(0, CRED_ANA, sizeof(CRED_ANA), &x) == NP_UHDR_ERR_STATE,
           "even the person's own credential does not open a dormant import");
    ASSERT(!x.open, "nothing opened");
}

/* @verifies REQ-EMMC2-04#b */
static void test_import_only_into_an_empty_slot_as_dormant(void)
{
    fresh();
    np_uhdr_profile_ctx_t a, b;
    uint8_t code[NP_UHDR_RECOVERY_CODE_LEN];
    np_uhdr_profile_export_t ex, bad;
    (void)np_uhdr_profile_create(0, CRED_ANA, sizeof(CRED_ANA), &a, code);
    (void)np_uhdr_profile_create(1, CRED_BEN, sizeof(CRED_BEN), &b, code);
    (void)np_uhdr_profile_export(0, &ex);

    ASSERT(np_uhdr_profile_import(1, &ex) == NP_UHDR_ERR_EXISTS,
           "an import never overwrites another person's profile");

    memcpy(&bad, &ex, sizeof(bad));
    bad.w1_ct[0] = 0x01U;
    ASSERT(np_uhdr_profile_import(2, &bad) == NP_UHDR_ERR_INVALID,
           "an export carrying any wrap-1 byte is refused");
    memcpy(&bad, &ex, sizeof(bad));
    bad.state = NP_UHDR_PROFILE_LIVE;
    ASSERT(np_uhdr_profile_import(2, &bad) == NP_UHDR_ERR_INVALID, "a LIVE record is refused");
    memcpy(&bad, &ex, sizeof(bad));
    bad.magic ^= 1U;
    ASSERT(np_uhdr_profile_import(2, &bad) == NP_UHDR_ERR_INVALID, "a malformed record is refused");
    ASSERT(!np_uhdr_host_profile_slot_used(2), "nothing was written by the refusals");

    ASSERT(np_uhdr_profile_import(2, &ex) == NP_UHDR_OK, "a well-formed export imports");
    ASSERT(np_uhdr_host_profile_slot_mut(2)->state == NP_UHDR_PROFILE_DORMANT, "as dormant");
}

/* @verifies REQ-EMMC2-04#c */
static void test_recovery_code_rebinds_to_this_helmet(void)
{
    fresh();
    np_uhdr_profile_ctx_t a, x;
    uint8_t code[NP_UHDR_RECOVERY_CODE_LEN];
    uint8_t pk_a[NP_UHDR_PROFILE_KEY_LEN];
    np_uhdr_profile_export_t ex;
    (void)np_uhdr_profile_create(0, CRED_ANA, sizeof(CRED_ANA), &a, code);
    memcpy(pk_a, a.pk, sizeof(pk_a));
    np_uhdr_profile_close(&a);
    (void)np_uhdr_profile_export(0, &ex);

    /* Ana's own helmet. */
    np_uhdr_host_profile_reset();
    np_uhdr_host_set_device_key(DEVKEY_2);
    (void)np_uhdr_profile_import(3, &ex);
    ASSERT(np_uhdr_profile_rebind(3, code, CRED_NEW, sizeof(CRED_NEW), &x) == NP_UHDR_OK,
           "the recovery code re-binds the profile");
    ASSERT(x.open && memcmp(x.pk, pk_a, sizeof(pk_a)) == 0, "to the same key, so her files open");
    ASSERT(np_uhdr_host_profile_slot_mut(3)->state == NP_UHDR_PROFILE_LIVE, "now live here");
    np_uhdr_profile_close(&x);

    ASSERT(np_uhdr_profile_unlock(3, CRED_NEW, sizeof(CRED_NEW), &x) == NP_UHDR_OK,
           "her new credential opens it on this helmet");
    ASSERT(memcmp(x.pk, pk_a, sizeof(pk_a)) == 0, "with the same key");
    np_uhdr_profile_close(&x);
    ASSERT(np_uhdr_profile_unlock(3, CRED_ANA, sizeof(CRED_ANA), &x) == NP_UHDR_ERR_AUTH,
           "the old helmet's credential is not what opens it here");

    /* The same code still re-binds (a forgotten credential). */
    ASSERT(np_uhdr_profile_rebind(3, code, CRED_ANA, sizeof(CRED_ANA), &x) == NP_UHDR_OK,
           "a live profile can be re-bound with its code after a forgotten credential");
}

/* @verifies REQ-EMMC2-04#d */
static void test_wrong_recovery_code_opens_nothing(void)
{
    fresh();
    np_uhdr_profile_ctx_t a, x;
    uint8_t code[NP_UHDR_RECOVERY_CODE_LEN];
    np_uhdr_profile_export_t ex;
    (void)np_uhdr_profile_create(0, CRED_ANA, sizeof(CRED_ANA), &a, code);
    np_uhdr_profile_close(&a);
    (void)np_uhdr_profile_export(0, &ex);
    np_uhdr_host_profile_reset();
    (void)np_uhdr_profile_import(0, &ex);

    uint8_t wrong[NP_UHDR_RECOVERY_CODE_LEN];
    memcpy(wrong, code, sizeof(wrong));
    wrong[15] ^= 0x01U;
    ASSERT(np_uhdr_profile_rebind(0, wrong, CRED_NEW, sizeof(CRED_NEW), &x) == NP_UHDR_ERR_AUTH,
           "a wrong recovery code fails");
    ASSERT(!x.open && is_zero(x.pk, sizeof(x.pk)), "and opens nothing");
    ASSERT(np_uhdr_host_profile_slot_mut(0)->state == NP_UHDR_PROFILE_DORMANT,
           "the record stays dormant");

    /* A wrap 2 moved onto another profile's record fails (bound to the id). */
    np_uhdr_profile_record_t *r = np_uhdr_host_profile_slot_mut(0);
    r->profile_id[5] ^= 0x10U;
    ASSERT(np_uhdr_profile_rebind(0, code, CRED_NEW, sizeof(CRED_NEW), &x) == NP_UHDR_ERR_AUTH,
           "wrap 2 is bound to its profile id");
}

/* @verifies REQ-EMMC2-05#a */
static void test_profile_attempts_share_the_limit(void)
{
    fresh();
    np_uhdr_profile_ctx_t a, x;
    uint8_t code[NP_UHDR_RECOVERY_CODE_LEN];
    (void)np_uhdr_profile_create(0, CRED_ANA, sizeof(CRED_ANA), &a, code);
    np_uhdr_profile_close(&a);

    (void)np_uhdr_profile_unlock(0, CRED_BEN, sizeof(CRED_BEN), &x);
    ASSERT(np_uhdr_host_attempts() == 1U, "a failed profile unlock is charged");
    uint8_t wrong[NP_UHDR_RECOVERY_CODE_LEN] = { 0 };
    (void)np_uhdr_profile_rebind(0, wrong, CRED_NEW, sizeof(CRED_NEW), &x);
    ASSERT(np_uhdr_host_attempts() == 2U, "a failed rebind is charged to the same count");
    ASSERT(np_uhdr_profile_unlock(0, CRED_ANA, sizeof(CRED_ANA), &x) == NP_UHDR_OK, "success");
    ASSERT(np_uhdr_host_attempts() == 0U, "a success clears the count");

    /* Reach the back-off: a further attempt is refused without the KDF. */
    np_uhdr_profile_close(&x);
    for (uint32_t i = 0; i < NP_UHDR_UNLOCK_FREE_ATTEMPTS; i++) {
        (void)np_uhdr_profile_unlock(0, CRED_BEN, sizeof(CRED_BEN), &x);
    }
    const uint32_t charged = np_uhdr_host_attempts();
    ASSERT(np_uhdr_profile_unlock(0, CRED_ANA, sizeof(CRED_ANA), &x) == NP_UHDR_ERR_LOCKOUT,
           "during the back-off even the right credential is refused");
    ASSERT(np_uhdr_profile_rebind(0, code, CRED_NEW, sizeof(CRED_NEW), &x) == NP_UHDR_ERR_LOCKOUT,
           "and so is the recovery code");
    ASSERT(np_uhdr_host_attempts() == charged && !x.open, "uncharged, nothing opened");

    np_uhdr_host_set_uptime_ms(NP_UHDR_UNLOCK_BACKOFF_MAX_MS + 1U);
    ASSERT(np_uhdr_profile_unlock(0, CRED_ANA, sizeof(CRED_ANA), &x) == NP_UHDR_OK,
           "after the wait it opens");

    /* A charge that cannot be persisted refuses the attempt untested. */
    np_uhdr_profile_close(&x);
    np_uhdr_host_attempts_write_fail(true);
    ASSERT(np_uhdr_profile_unlock(0, CRED_ANA, sizeof(CRED_ANA), &x) == NP_UHDR_ERR_CONFIG,
           "an unpersistable charge refuses even the right credential");
    ASSERT(!x.open, "nothing opened");
    np_uhdr_host_attempts_write_fail(false);
}

/* @verifies REQ-EMMC2-05#b */
static void test_dormant_refusal_is_not_charged(void)
{
    fresh();
    np_uhdr_profile_ctx_t a, x;
    uint8_t code[NP_UHDR_RECOVERY_CODE_LEN];
    np_uhdr_profile_export_t ex;
    (void)np_uhdr_profile_create(0, CRED_ANA, sizeof(CRED_ANA), &a, code);
    (void)np_uhdr_profile_export(0, &ex);
    np_uhdr_host_profile_reset();
    (void)np_uhdr_profile_import(0, &ex);

    ASSERT(np_uhdr_profile_unlock(0, CRED_ANA, sizeof(CRED_ANA), &x) == NP_UHDR_ERR_STATE,
           "a dormant profile refuses an unlock");
    ASSERT(np_uhdr_host_attempts() == 0U, "without running the KDF or charging");
}

/* @verifies REQ-EMMC2-02#b */
static void test_change_credential_keeps_the_key(void)
{
    fresh();
    np_uhdr_profile_ctx_t a, x;
    uint8_t code[NP_UHDR_RECOVERY_CODE_LEN];
    uint8_t pk[NP_UHDR_PROFILE_KEY_LEN];
    (void)np_uhdr_profile_create(0, CRED_ANA, sizeof(CRED_ANA), &a, code);
    memcpy(pk, a.pk, sizeof(pk));
    uint8_t w2_before[NP_UHDR_PROFILE_KEY_LEN];
    memcpy(w2_before, np_uhdr_host_profile_slot_mut(0)->w2_ct, sizeof(w2_before));

    ASSERT(np_uhdr_profile_change_credential(&a, CRED_NEW, sizeof(CRED_NEW)) == NP_UHDR_OK,
           "change credential");
    ASSERT(memcmp(np_uhdr_host_profile_slot_mut(0)->w2_ct, w2_before, sizeof(w2_before)) == 0,
           "wrap 2 (and so the recovery code) is unchanged");
    np_uhdr_profile_close(&a);
    ASSERT(np_uhdr_profile_unlock(0, CRED_NEW, sizeof(CRED_NEW), &x) == NP_UHDR_OK &&
           memcmp(x.pk, pk, sizeof(pk)) == 0, "the new credential opens the same key");
    np_uhdr_profile_close(&x);
    ASSERT(np_uhdr_profile_unlock(0, CRED_ANA, sizeof(CRED_ANA), &x) == NP_UHDR_ERR_AUTH,
           "the old credential no longer does");
    ASSERT(np_uhdr_profile_change_credential(&x, CRED_ANA, sizeof(CRED_ANA)) == NP_UHDR_ERR_STATE,
           "a closed profile cannot change its credential");
}

int main(void)
{
    test_record_size();
    test_create_generates_per_person_secrets();
    test_one_persons_credential_opens_only_their_profile();
    test_recovery_code_is_not_stored();
    test_delete_destroys_the_profile();
    test_record_is_bound_to_the_helmet();
    test_export_carries_no_unlock_wrap();
    test_wrap1_binds_id_salt_and_parameters();
    test_export_opens_with_nothing_the_clinic_holds();
    test_import_only_into_an_empty_slot_as_dormant();
    test_recovery_code_rebinds_to_this_helmet();
    test_wrong_recovery_code_opens_nothing();
    test_profile_attempts_share_the_limit();
    test_dormant_refusal_is_not_charged();
    test_change_credential_keeps_the_key();

    if (g_fail_count == 0) {
        printf("PASS: all np_uhdr_profile tests\n");
    } else {
        printf("FAILED: %d assertion(s)\n", g_fail_count);
    }
    return g_fail_count;
}
