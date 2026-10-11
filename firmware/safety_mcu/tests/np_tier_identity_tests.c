/*
 * NeurOne Safety MCU — SW01-M10 tier identity: host unit tests
 * Document: NP-REG-UPG-001 Rev 3 §7.5 (REQ-UPG-01, REQ-UPG-02), OI-UPG-01
 *
 * Built TWICE from this one file (firmware/safety_mcu/CMakeLists.txt):
 *
 *   np_tier_identity_tests        with the test authority key force-included
 *                                 (tests/np_tier_test_key.h).  Every record is
 *                                 signed here with Monocypher's real RFC 8032
 *                                 signer and verified by the real np_crypto
 *                                 verifier the image links.
 *
 *   np_tier_identity_nokey_tests  with NP_TIER_TEST_NOKEY and NO key override,
 *                                 i.e. the all-zero placeholder that ships
 *                                 today.  Proves the placeholder fails closed:
 *                                 a perfectly signed T2 record still yields T1.
 *
 * REQ-UPG-01's verification row asks for: a T1-identity unit with each T2 part
 * fitted never has the enable asserted; the same with the identity erased and
 * with it corrupted.  At this layer "fitted" is the hub requesting the line, so
 * each case below requests every enable bit and checks what the gate lets
 * through.  The hub half (a T2 protocol refused at load, as F4) is
 * hub_control/tests/np_protocol_tests.c.
 */

#include "np_safety_config.h"
#include "np_safety_hal.h"
#include "np_safety_protocol.h"
#include <stdio.h>
#include <string.h>

/* The signer: both builds sign records, the no-key build to show that even a
 * perfectly signed one gets nowhere without an authority key. */
#include "monocypher-ed25519.h"

/* ── Unit under test ──────────────────────────────────────────────────────── */
extern np_safe_status_t np_tier_identity_init(void);
extern bool             np_tier_identity_is_t2(void);
extern void             np_tier_identity_gate(np_safety_state_t *state);
extern void             np_tier_identity_build_report(np_safety_tier_report_t *out,
                                                      uint16_t requested_mask);
extern uint8_t          np_tier_identity_reason(void);
extern uint8_t          np_tier_identity_record_tier(void);
extern uint8_t          np_tier_identity_hw_version(void);

/* ── HAL doubles (declared in np_safety_hal.h, so drift is a compile error) ── */
static uint8_t g_otp_record[NP_TIER_RECORD_LEN];
static uint8_t g_uid[NP_DEVICE_UID_LEN];
static int     g_record_reads = 0;

void np_hal_otp_read_tier_record(uint8_t *buf, uint8_t len)
{
    g_record_reads++;
    memcpy(buf, g_otp_record, (len < NP_TIER_RECORD_LEN) ? len : NP_TIER_RECORD_LEN);
}

void np_hal_read_device_uid(uint8_t *buf, uint8_t len)
{
    memcpy(buf, g_uid, (len < NP_DEVICE_UID_LEN) ? len : NP_DEVICE_UID_LEN);
}

/* ── Harness ──────────────────────────────────────────────────────────────── */
static int g_failures = 0;
static void check(int cond, const char *name)
{
    if (cond) { printf("PASS: %s\n", name); }
    else      { printf("FAIL: %s\n", name); g_failures++; }
}

/* The unit's own UID, and a second unit's. */
static const uint8_t UID_THIS[NP_DEVICE_UID_LEN] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC };
#ifndef NP_TIER_TEST_NOKEY
static const uint8_t UID_OTHER[NP_DEVICE_UID_LEN] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCD };
#endif

static uint8_t g_sk[64];      /* the test authority's secret key          */
static uint8_t g_sk_rogue[64];/* someone else's key — correct format, wrong signer */
static uint8_t g_pk[32];

static void derive_keys(void)
{
    uint8_t seed[32];
    uint8_t pk_rogue[32];
    uint8_t i;
    for (i = 0U; i < 32U; i++) { seed[i] = (uint8_t)(0xA0U + i); }
    crypto_ed25519_key_pair(g_sk, g_pk, seed);          /* wipes seed */
    for (i = 0U; i < 32U; i++) { seed[i] = (uint8_t)(0x10U + i); }
    crypto_ed25519_key_pair(g_sk_rogue, pk_rogue, seed);
}

/* Build a record naming `tier`, signed by `sk` for `uid`.  `with_domain`
 * false signs body || uid without the domain string — a signature over the
 * right fields made for some other purpose. */
static void make_record_byte5(uint8_t byte5, const uint8_t *uid, const uint8_t *sk,
                              bool with_domain)
{
    uint8_t msg[NP_TIER_SIG_MSG_LEN];
    size_t  off = 0U;

    memset(g_otp_record, 0, sizeof(g_otp_record));
    g_otp_record[0] = NP_TIER_RECORD_MAGIC_0;
    g_otp_record[1] = NP_TIER_RECORD_MAGIC_1;
    g_otp_record[2] = NP_TIER_RECORD_MAGIC_2;
    g_otp_record[3] = NP_TIER_RECORD_MAGIC_3;
    g_otp_record[4] = NP_TIER_RECORD_VERSION;
    g_otp_record[5] = byte5;

    if (with_domain) {
        memcpy(msg, NP_TIER_SIG_DOMAIN, NP_TIER_SIG_DOMAIN_LEN);
        off = NP_TIER_SIG_DOMAIN_LEN;
    }
    memcpy(&msg[off], g_otp_record, NP_TIER_RECORD_BODY_LEN);
    off += NP_TIER_RECORD_BODY_LEN;
    memcpy(&msg[off], uid, NP_DEVICE_UID_LEN);
    off += NP_DEVICE_UID_LEN;

    crypto_ed25519_sign(&g_otp_record[NP_TIER_RECORD_BODY_LEN], sk, msg, off);
}

#ifndef NP_TIER_TEST_NOKEY
/* The common case: hardware version 1 in the upper nibble, `tier` in the lower. */
static void make_record(uint8_t tier, const uint8_t *uid, const uint8_t *sk,
                        bool with_domain)
{
    make_record_byte5((uint8_t)((1U << NP_TIER_BYTE_HWVER_SHIFT) | tier), uid, sk,
                      with_domain);
}
#endif

static void make_record_hw(uint8_t hw, uint8_t tier, const uint8_t *uid, const uint8_t *sk)
{
    make_record_byte5((uint8_t)((hw << NP_TIER_BYTE_HWVER_SHIFT) | tier), uid, sk, true);
}

/* Run the gate over "every line requested" and return what survives. */
static uint16_t gate_all(void)
{
    np_safety_state_t st;
    memset(&st, 0, sizeof(st));
    st.requested_mask = NP_SAFETY_EN_ALL_MASK;
    st.granted_mask   = NP_SAFETY_EN_ALL_MASK;
    np_tier_identity_gate(&st);
    return st.granted_mask;
}

static void boot(void)
{
    memcpy(g_uid, UID_THIS, sizeof(g_uid));
    (void)np_tier_identity_init();
}

/* A unit that is not T2 keeps every T1 line and loses every T2 line. */
static void expect_t1(const char *label, uint8_t reason)
{
    char name[160];
    uint16_t out = gate_all();

    snprintf(name, sizeof(name), "%s: unit is not T2", label);
    check(!np_tier_identity_is_t2(), name);
    snprintf(name, sizeof(name), "%s: reason code", label);
    check(np_tier_identity_reason() == reason, name);
    snprintf(name, sizeof(name), "%s: no T2 enable line survives the gate", label);
    check((out & NP_SAFETY_EN_T2_MASK) == 0U, name);
    snprintf(name, sizeof(name), "%s: every T1 enable line survives the gate", label);
    check(out == (uint16_t)(NP_SAFETY_EN_ALL_MASK & ~NP_SAFETY_EN_T2_MASK), name);
}

/* ══ The T2 set itself ════════════════════════════════════════════════════ */
/* @verifies REQ-UPG-01#a REQ-UPG-03 */
static void test_t2_mask_is_exactly_the_four_t2_lines(void)
{
    check(NP_SAFETY_EN_T2_MASK == (NP_SAFETY_EN_CVNS | NP_SAFETY_EN_TMS |
                                   NP_SAFETY_EN_PBM_1170NM | NP_SAFETY_EN_CLIN_STIM),
          "T2 mask = cervical VNS | TMS | 1170 nm | clinical stim");
    check((NP_SAFETY_EN_T2_MASK & (NP_SAFETY_EN_PBM_CRANIAL | NP_SAFETY_EN_BES_TACS |
                                   NP_SAFETY_EN_TDCS | NP_SAFETY_EN_VNS_HRV |
                                   NP_SAFETY_EN_VISUAL | NP_SAFETY_EN_INTRANASAL)) == 0U,
          "T2 mask contains no T1 line (REQ-UPG-03: carried modules keep working)");
    check((NP_SAFETY_EN_T2_MASK & ~NP_SAFETY_EN_ALL_MASK) == 0U,
          "T2 mask contains only allocated enable bits");
}

#ifndef NP_TIER_TEST_NOKEY

/* @verifies-none harness self-check: the test authority key matches its seed */
static void test_key_matches_seed(void)
{
    static const uint8_t COMPILED[32] = NP_TIER_AUTHORITY_PUBKEY_INIT;
    check(memcmp(g_pk, COMPILED, 32) == 0,
          "compiled test authority key is the one derived from the test seed");
}

/* @verifies REQ-UPG-01#c */
static void test_blank_otp_is_t1(void)
{
    memset(g_otp_record, 0xFF, sizeof(g_otp_record));
    boot();
    expect_t1("blank OTP window (identity never written)", NP_TIER_REASON_BLANK);
}

/* @verifies REQ-UPG-02#a */
static void test_signed_t2_for_this_unit_is_t2(void)
{
    uint16_t out;
    make_record(NP_TIER_T2, UID_THIS, g_sk, true);
    boot();
    out = gate_all();
    check(np_tier_identity_is_t2(), "signed T2 record for this UID: unit is T2");
    check(np_tier_identity_reason() == NP_TIER_REASON_OK, "signed T2 record: reason OK");
    check(out == NP_SAFETY_EN_ALL_MASK, "signed T2 record: gate withholds nothing");
}

/* @verifies REQ-UPG-01#a */
static void test_signed_t1_is_t1(void)
{
    make_record(NP_TIER_T1, UID_THIS, g_sk, true);
    boot();
    expect_t1("signed T1 record", NP_TIER_REASON_OK);
    check(np_tier_identity_record_tier() == NP_TIER_T1, "signed T1 record: record names T1");
}

/* @verifies REQ-UPG-01#c REQ-UPG-02#a */
static void test_record_copied_from_another_unit_is_t1(void)
{
    /* A genuine T2 record, lifted from another unit's OTP into this one's. */
    make_record(NP_TIER_T2, UID_OTHER, g_sk, true);
    boot();
    expect_t1("genuine T2 record from ANOTHER unit (device binding)",
              NP_TIER_REASON_SIGNATURE);
}

/* @verifies REQ-UPG-01#c REQ-UPG-02#a */
static void test_tier_byte_edited_is_t1(void)
{
    /* Signed as T1, then the tier byte rewritten to T2. */
    make_record(NP_TIER_T1, UID_THIS, g_sk, true);
    g_otp_record[5] = (uint8_t)((1U << NP_TIER_BYTE_HWVER_SHIFT) | NP_TIER_T2);
    boot();
    expect_t1("T1 record with its tier byte edited to T2", NP_TIER_REASON_SIGNATURE);
}

/* @verifies REQ-UPG-01#c REQ-UPG-02#a */
static void test_signature_corrupted_is_t1(void)
{
    make_record(NP_TIER_T2, UID_THIS, g_sk, true);
    g_otp_record[NP_TIER_RECORD_LEN - 1U] ^= 0x01U;
    boot();
    expect_t1("T2 record with one signature bit flipped", NP_TIER_REASON_SIGNATURE);
}

/* @verifies REQ-UPG-01#c REQ-UPG-02#a */
static void test_wrong_signer_is_t1(void)
{
    make_record(NP_TIER_T2, UID_THIS, g_sk_rogue, true);
    boot();
    expect_t1("T2 record signed by a key that is not the tier authority",
              NP_TIER_REASON_SIGNATURE);
}

/* @verifies REQ-UPG-01#c REQ-UPG-02#a */
static void test_signature_without_domain_is_t1(void)
{
    make_record(NP_TIER_T2, UID_THIS, g_sk, false);
    boot();
    expect_t1("T2 record signed without the tier domain string",
              NP_TIER_REASON_SIGNATURE);
}

/* @verifies REQ-UPG-01#c REQ-UPG-02#a */
static void test_format_rejections_are_t1(void)
{
    make_record(NP_TIER_T2, UID_THIS, g_sk, true);
    g_otp_record[0] = 0x00U;
    boot();
    expect_t1("bad magic", NP_TIER_REASON_FORMAT);

    make_record(NP_TIER_T2, UID_THIS, g_sk, true);
    g_otp_record[4] = (uint8_t)(NP_TIER_RECORD_VERSION + 1U);
    boot();
    expect_t1("unknown record version", NP_TIER_REASON_FORMAT);

    make_record(0x03U, UID_THIS, g_sk, true);   /* validly signed, unknown tier */
    boot();
    expect_t1("validly signed record naming an unknown tier code", NP_TIER_REASON_FORMAT);

    make_record(NP_TIER_T2, UID_THIS, g_sk, true);
    g_otp_record[6] = 0x01U;
    boot();
    expect_t1("non-zero reserved byte", NP_TIER_REASON_FORMAT);

    memset(g_otp_record, 0x00, sizeof(g_otp_record));
    boot();
    expect_t1("all-zero record window", NP_TIER_REASON_FORMAT);
}

/* ══ Hardware version (SWR-HV-06, SWR-HV-07, REQ-UPG-04) ═════════════════ */
/* @verifies REQ-UPG-04 */
static void test_hw_version_every_nibble(void)
{
    uint8_t hw;
    bool    ok = true, tier_ok = true;
    for (hw = 1U; hw <= 15U; hw++) {
        make_record_hw(hw, NP_TIER_T2, UID_THIS, g_sk);
        boot();
        if (np_tier_identity_hw_version() != hw) { ok = false; }
        if (!np_tier_identity_is_t2())           { tier_ok = false; }
    }
    check(ok, "hw version: each upper-nibble value 1..15 of a verified record reads back");
    check(tier_ok, "hw version: the T2 verdict is the same at every version (independent)");

    make_record_hw(7U, NP_TIER_T1, UID_THIS, g_sk);
    boot();
    check(np_tier_identity_hw_version() == 7U && !np_tier_identity_is_t2(),
          "hw version: a verified T1 record at version 7 reads 7 and stays T1");
}

/* @verifies REQ-UPG-04 */
static void test_hw_version_zero_nibble_is_malformed(void)
{
    make_record_hw(0U, NP_TIER_T2, UID_THIS, g_sk);   /* validly signed */
    boot();
    expect_t1("upper nibble 0 (validly signed)", NP_TIER_REASON_FORMAT);
    check(np_tier_identity_hw_version() == NP_HW_VERSION_DEFAULT,
          "hw version: a malformed record reads as the default");
}

/* @verifies REQ-UPG-04 */
static void test_hw_version_unverified_reads_default(void)
{
    make_record_hw(5U, NP_TIER_T2, UID_OTHER, g_sk);
    boot();
    check(np_tier_identity_hw_version() == NP_HW_VERSION_DEFAULT,
          "hw version: another unit's record (version 5) reads 1 here");

    make_record_hw(5U, NP_TIER_T1, UID_THIS, g_sk);
    g_otp_record[5] = (uint8_t)((9U << NP_TIER_BYTE_HWVER_SHIFT) | NP_TIER_T1);
    boot();
    check(np_tier_identity_hw_version() == NP_HW_VERSION_DEFAULT,
          "hw version: an edited version nibble breaks the signature and reads 1");

    make_record_hw(5U, NP_TIER_T2, UID_THIS, g_sk_rogue);
    boot();
    check(np_tier_identity_hw_version() == NP_HW_VERSION_DEFAULT,
          "hw version: a record from the wrong signer reads 1");

    memset(g_otp_record, 0xFF, sizeof(g_otp_record));
    boot();
    check(np_tier_identity_hw_version() == NP_HW_VERSION_DEFAULT,
          "hw version: a blank window reads 1, not a fault");

    make_record_hw(5U, NP_TIER_T2, UID_THIS, g_sk);
    boot();
    memset(g_otp_record, 0xFF, sizeof(g_otp_record));
    boot();
    check(np_tier_identity_hw_version() == NP_HW_VERSION_DEFAULT,
          "hw version: a re-init against a blank window does not keep the old version");
}

/* @verifies REQ-UPG-04 */
static void test_hw_version_in_report(void)
{
    np_safety_tier_report_t r;
    make_record_hw(3U, NP_TIER_T1, UID_THIS, g_sk);
    boot();
    np_tier_identity_build_report(&r, 0U);
    check(r.hw_version == 3U, "report: carries the verified hardware version");
    check(r.checksum == (uint16_t)(r.magic + r.tier + r.flags + r.reason + r.hw_version),
          "report: checksum covers the hardware version");
    check(sizeof(r) == NP_SAFETY_TIER_REPORT_LEN, "report: wire size is the declared length");
}

/* @verifies REQ-UPG-02#a */
static void test_signing_domain_is_the_version_2_string(void)
{
    check(memcmp(NP_TIER_SIG_DOMAIN, "NeurOne.TierId.2", NP_TIER_SIG_DOMAIN_LEN) == 0 &&
          strlen(NP_TIER_SIG_DOMAIN) == NP_TIER_SIG_DOMAIN_LEN,
          "domain string is the new 16-character string");
    /* A record signed under the old domain must not verify. */
    {
        uint8_t msg[NP_TIER_SIG_MSG_LEN];
        make_record(NP_TIER_T2, UID_THIS, g_sk, true);
        memcpy(msg, "NeurOne.TierId.1", NP_TIER_SIG_DOMAIN_LEN);
        memcpy(&msg[NP_TIER_SIG_DOMAIN_LEN], g_otp_record, NP_TIER_RECORD_BODY_LEN);
        memcpy(&msg[NP_TIER_SIG_DOMAIN_LEN + NP_TIER_RECORD_BODY_LEN], UID_THIS,
               NP_DEVICE_UID_LEN);
        crypto_ed25519_sign(&g_otp_record[NP_TIER_RECORD_BODY_LEN], g_sk, msg, sizeof(msg));
        boot();
        expect_t1("record signed under the old domain string", NP_TIER_REASON_SIGNATURE);
    }
}

/* @verifies REQ-UPG-01#c */
static void test_verdict_is_retaken_at_each_boot(void)
{
    /* T2 then a blank window: the second boot must not inherit the first. */
    make_record(NP_TIER_T2, UID_THIS, g_sk, true);
    boot();
    memset(g_otp_record, 0xFF, sizeof(g_otp_record));
    boot();
    expect_t1("T2 verdict does not survive a re-init against a blank window",
              NP_TIER_REASON_BLANK);
}

/* @verifies REQ-UPG-01#a */
static void test_each_t2_line_individually(void)
{
    static const uint16_t TIER_ENABLE_LINES[] = {
        NP_SAFETY_EN_CVNS, NP_SAFETY_EN_TMS, NP_SAFETY_EN_PBM_1170NM,
        NP_SAFETY_EN_CLIN_STIM };
    size_t i;
    bool   all_withheld = true;

    make_record(NP_TIER_T1, UID_THIS, g_sk, true);
    boot();
    for (i = 0U; i < sizeof(TIER_ENABLE_LINES) / sizeof(TIER_ENABLE_LINES[0]); i++) {
        np_safety_state_t st;
        memset(&st, 0, sizeof(st));
        st.requested_mask = TIER_ENABLE_LINES[i] | NP_SAFETY_EN_BES_TACS;
        st.granted_mask   = st.requested_mask;
        np_tier_identity_gate(&st);
        if (st.granted_mask != NP_SAFETY_EN_BES_TACS) {
            all_withheld = false;
        }
    }
    check(all_withheld,
          "T1 unit: each T2 line requested alone beside a T1 line is withheld, the T1 line kept");
}

/* @verifies REQ-UPG-01 */
static void test_report(void)
{
    np_safety_tier_report_t r;
    uint16_t sum;

    make_record(NP_TIER_T1, UID_THIS, g_sk, true);
    boot();

    np_tier_identity_build_report(&r, NP_SAFETY_EN_CVNS | NP_SAFETY_EN_VNS_HRV);
    sum = (uint16_t)(r.magic + r.tier + r.flags + r.reason + r.hw_version);
    check(r.magic == NP_SAFETY_TIER_REPORT_MAGIC && r.checksum == sum,
          "report: magic and checksum");
    check(r.tier == NP_TIER_T1, "report: T1 unit reports T1");
    check((r.flags & NP_SAFETY_TIER_FLAG_REFUSED) != 0U,
          "report: T1 unit asked for cervical VNS reports REFUSED (hub presents F4)");

    np_tier_identity_build_report(&r, NP_SAFETY_EN_VNS_HRV | NP_SAFETY_EN_TDCS);
    check(r.flags == 0U, "report: T1 unit asked only for T1 lines reports no refusal");

    make_record(NP_TIER_T2, UID_THIS, g_sk, true);
    boot();
    np_tier_identity_build_report(&r, NP_SAFETY_EN_ALL_MASK);
    check(r.tier == NP_TIER_T2 && r.flags == 0U && r.reason == NP_TIER_REASON_OK,
          "report: T2 unit asked for every line reports T2, OK, no refusal");

    memset(g_otp_record, 0xFF, sizeof(g_otp_record));
    boot();
    np_tier_identity_build_report(&r, 0U);
    check(r.tier == NP_TIER_T1 && r.reason == NP_TIER_REASON_BLANK,
          "report: blank unit reports T1 with reason BLANK (caught at final acceptance)");
}

#else /* NP_TIER_TEST_NOKEY — the image as it ships today */

/* @verifies REQ-UPG-01#c */
static void test_placeholder_key_fails_closed(void)
{
    static const uint8_t COMPILED[32] = NP_TIER_AUTHORITY_PUBKEY_INIT;
    size_t i;
    bool   zero = true;
    for (i = 0U; i < 32U; i++) { if (COMPILED[i] != 0U) { zero = false; } }
    check(zero, "no-key build: the shipped authority key is the all-zero placeholder");

    /* A record that WOULD verify under the test key. */
    make_record_hw(5U, NP_TIER_T2, UID_THIS, g_sk);
    g_record_reads = 0;
    boot();
    expect_t1("no-key build: perfectly signed T2 record", NP_TIER_REASON_NO_AUTHORITY);
    check(g_record_reads == 0,
          "no-key build: the OTP record is not even read — nothing can verify");
    check(np_tier_identity_hw_version() == NP_HW_VERSION_DEFAULT,
          "no-key build: a signed version-5 record still reads hardware version 1");
}

#endif

int main(void)
{
    derive_keys();
    test_t2_mask_is_exactly_the_four_t2_lines();
#ifndef NP_TIER_TEST_NOKEY
    test_key_matches_seed();
    test_blank_otp_is_t1();
    test_signed_t2_for_this_unit_is_t2();
    test_signed_t1_is_t1();
    test_record_copied_from_another_unit_is_t1();
    test_tier_byte_edited_is_t1();
    test_signature_corrupted_is_t1();
    test_wrong_signer_is_t1();
    test_signature_without_domain_is_t1();
    test_format_rejections_are_t1();
    test_hw_version_every_nibble();
    test_hw_version_zero_nibble_is_malformed();
    test_hw_version_unverified_reads_default();
    test_hw_version_in_report();
    test_signing_domain_is_the_version_2_string();
    test_verdict_is_retaken_at_each_boot();
    test_each_t2_line_individually();
    test_report();
#else
    test_placeholder_key_fails_closed();
#endif
    printf("\n%s: %d failure(s)\n", (g_failures == 0) ? "OK" : "FAILED", g_failures);
    return (g_failures == 0) ? 0 : 1;
}
