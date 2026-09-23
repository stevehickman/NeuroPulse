/*
 * Host tests — np_cvns_fault_summary (NP-SW-FAULTMSG-001 §9.6)
 *
 * Models the three platform seams (UHDR blob store, GATT notify) and the two
 * hub entry points the ATT write handlers call.  The critical section is the
 * host model in np_transport.c.
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../include/np_cvns_fault_summary.h"

static int g_failures = 0;

static void check(int cond, const char *name)
{
    if (cond) {
        printf("PASS: %s\n", name);
    } else {
        printf("FAIL: %s\n", name);
        g_failures++;
    }
}

/* ── Seam models ──────────────────────────────────────────────────────────── */

static uint8_t  g_store[NP_CVFS_BLOB_MAX];
static size_t   g_store_len;
static bool     g_save_fails;
static int      g_saves;

static uint8_t  g_notified[NP_CVFS_FRAME_MAX];
static size_t   g_notified_len;
static int      g_notifies;

static uint32_t        g_posted_user;
static int             g_posted_users;
static np_hub_status_t g_confirm_rc = NP_HUB_OK;
static int             g_confirms;

np_hub_status_t np_cvfs_hal_load(uint8_t *buf, size_t cap, size_t *len_out)
{
    if (g_store_len > cap) { return NP_HUB_ERR_GENERIC; }
    memcpy(buf, g_store, g_store_len);
    *len_out = g_store_len;
    return NP_HUB_OK;
}

np_hub_status_t np_cvfs_hal_save(const uint8_t *buf, size_t len)
{
    g_saves++;
    if (g_save_fails || (len > sizeof g_store)) { return NP_HUB_ERR_GENERIC; }
    memcpy(g_store, buf, len);
    g_store_len = len;
    return NP_HUB_OK;
}

void np_cvfs_hal_notify(const uint8_t *frame, size_t len)
{
    memcpy(g_notified, frame, len);
    g_notified_len = len;
    g_notifies++;
}

np_hub_status_t np_hub_set_active_user(uint32_t user_tag)
{
    g_posted_user = user_tag;
    g_posted_users++;
    return NP_HUB_OK;
}

np_hub_status_t np_hub_cvns_reenable_confirm(void)
{
    g_confirms++;
    return g_confirm_rc;
}

static void reset_world(void)
{
    g_store_len = 0u;
    g_save_fails = false;
    g_saves = 0;
    g_notified_len = 0u;
    g_notifies = 0;
    g_posted_user = 0u;
    g_posted_users = 0;
    g_confirm_rc = NP_HUB_OK;
    g_confirms = 0;
    np_cvfs_init();
}

static size_t frame_now(uint8_t state, bool valid, uint8_t flags, uint8_t *out)
{
    size_t len = 0u;
    (void)np_cvfs_build_frame(state, valid, flags, out, NP_CVFS_FRAME_MAX, &len);
    return len;
}

static uint32_t counter_at(const uint8_t *f, unsigned rec)
{
    const uint8_t *p = &f[NP_CVFS_HEADER_LEN + (rec * NP_CVFS_RECORD_LEN)];
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* ── Tests ────────────────────────────────────────────────────────────────── */

static void test_empty(void)
{
    uint8_t f[NP_CVFS_FRAME_MAX];
    size_t  len = 0u;
    reset_world();
    check(np_cvfs_read(f, sizeof f, &len) == NP_HUB_OK && len == 4u &&
          f[0] == 0x01u && f[1] == 0u && f[2] == 0u && f[3] == 0u,
          "empty: read before any poll is a valid empty frame");
    check(np_cvfs_current_user() == 0u, "empty: no user named");
    check(np_cvfs_build_frame(0u, false, 0u, f, NP_CVFS_FRAME_MAX - 1u, &len) == NP_HUB_ERR_INVALID_ARG,
          "empty: short buffer refused");
}

/* The same frame GATTParserTests.testParseCervicalFaultStatus parses, plus
 * the blanket-warning flag. */
static void test_golden_frame_matches_app_parser(void)
{
    static const uint8_t expected[] = {
        0x01, 0x02, 0x03, 0x02,
        41, 0, 0, 0, 2, 0x00, 0, 0,
        42, 0, 0, 0, 1, 0x00, 0, 0,
        43, 0, 0, 0, 4, 0x02, 0, 0,
    };
    uint8_t f[NP_CVFS_FRAME_MAX];
    size_t  len;
    reset_world();
    np_cvfs_set_user(0x01020304u);
    (void)np_cvfs_record_fault(41u, NP_CVNS_FAULT_DATA_LOSS, 0u);
    (void)np_cvfs_record_fault(42u, NP_CVNS_FAULT_HR_CHANGE, 0u);
    (void)np_cvfs_record_fault(43u, NP_CVNS_FAULT_IMPEDANCE, NP_CVFS_SIDE_RIGHT);
    len = frame_now(2u, true, 0x02u, f);
    check(len == sizeof expected && memcmp(f, expected, len) == 0,
          "golden: bytes equal the frame the iOS/Android parsers are tested against");
}

static void test_flags_and_state(void)
{
    uint8_t f[NP_CVFS_FRAME_MAX];
    reset_world();
    (void)frame_now(4u, true, 0xFFu, f);
    check(f[1] == 4u && f[3] == 0x03u, "flags: only bits 0-1 of the NV report reach the wire");
    (void)frame_now(4u, false, 0x03u, f);
    check(f[3] == 0u, "flags: no valid NV report -> flags 0");
    (void)frame_now(9u, true, 0u, f);
    check(f[1] == 0u, "state: an out-of-range state is sent as idle, never as a value the app rejects");
}

static void test_per_user_scope(void)
{
    uint8_t f[NP_CVFS_FRAME_MAX];
    size_t  len;
    reset_world();
    (void)np_cvfs_record_fault(1u, NP_CVNS_FAULT_HR_CHANGE, 0u);   /* unattributed */
    np_cvfs_set_user(0xA);
    (void)np_cvfs_record_fault(2u, NP_CVNS_FAULT_HR_CHANGE, 0u);
    np_cvfs_set_user(0xB);
    (void)np_cvfs_record_fault(3u, NP_CVNS_FAULT_DATA_LOSS, 0u);

    len = frame_now(0u, true, 0u, f);
    check(len == 4u + 2u * 8u && f[2] == 2u && counter_at(f, 0) == 1u && counter_at(f, 1) == 3u,
          "scope: B sees the unattributed record and B's own, never A's");
    np_cvfs_set_user(0xA);
    len = frame_now(0u, true, 0u, f);
    check(len == 4u + 2u * 8u && counter_at(f, 0) == 1u && counter_at(f, 1) == 2u,
          "scope: A sees the unattributed record and A's own, never B's");
    np_cvfs_set_user(0u);
    np_cvfs_set_user(0xFFFFFFFFu);
    check(np_cvfs_current_user() == 0xAu, "scope: reserved tags never become the attribution");
}

static void test_newest_four_oldest_first_and_eviction(void)
{
    uint8_t  f[NP_CVFS_FRAME_MAX];
    uint32_t c;
    reset_world();
    for (c = 1u; c <= 10u; c++) {
        (void)np_cvfs_record_fault(c, NP_CVNS_FAULT_WATCHDOG, 0u);
    }
    (void)frame_now(0u, true, 0u, f);
    check(f[2] == 4u && counter_at(f, 0) == 7u && counter_at(f, 3) == 10u,
          "window: the four newest, oldest first");

    /* Only 8 are kept: after persisting and reloading, 3..10 remain. */
    np_cvfs_poll(0u, true, 0u);
    np_cvfs_init();
    np_cvfs_set_user(0x77u);   /* a user with nothing of their own */
    (void)np_cvfs_record_fault(11u, NP_CVNS_FAULT_WATCHDOG, 0u);  /* evicts 3 */
    np_cvfs_set_user(0x78u);
    (void)frame_now(0u, true, 0u, f);
    check(f[2] == 4u && counter_at(f, 0) == 7u && counter_at(f, 3) == 10u,
          "eviction: store holds 8 across users; oldest drops first");
}

static void test_record_validation(void)
{
    uint8_t f[NP_CVFS_FRAME_MAX];
    reset_world();
    check(np_cvfs_record_fault(1u, NP_CVNS_FAULT_NONE, 0u) == NP_HUB_ERR_INVALID_ARG,
          "record: NONE refused");
    check(np_cvfs_record_fault(1u, (np_cvns_fault_reason_t)6, 0u) == NP_HUB_ERR_INVALID_ARG,
          "record: unknown kind refused");
    (void)np_cvfs_record_fault(2u, NP_CVNS_FAULT_HR_CHANGE, NP_CVFS_SIDE_LEFT);
    (void)np_cvfs_record_fault(3u, NP_CVNS_FAULT_IMPEDANCE, 0xFFu);
    (void)frame_now(0u, true, 0u, f);
    check(f[2] == 2u && f[4 + 5] == 0u, "record: a side mask on a non-pad fault is dropped");
    check(f[12 + 5] == 0x03u, "record: reserved side bits are dropped");
}

static void test_side_mask(void)
{
    check(np_cvfs_pad_side_mask(9.0f, 2.0f) == NP_CVFS_SIDE_LEFT, "sides: left out of window");
    check(np_cvfs_pad_side_mask(2.0f, 9.0f) == NP_CVFS_SIDE_RIGHT, "sides: right out of window");
    check(np_cvfs_pad_side_mask(9.0f, 9.0f) == 0x03u, "sides: both out of window");
    check(np_cvfs_pad_side_mask(NAN, 2.0f) == NP_CVFS_SIDE_LEFT, "sides: NaN counts as failing");
    check(np_cvfs_pad_side_mask(0.0f, 2.0f) == NP_CVFS_SIDE_LEFT, "sides: zero counts as failing");
    check(np_cvfs_pad_side_mask(2.0f, 3.0f) == 0x03u,
          "sides: a pad fault with both in window names both, never neither");
}

static void test_persistence_round_trip(void)
{
    uint8_t a[NP_CVFS_FRAME_MAX];
    uint8_t b[NP_CVFS_FRAME_MAX];
    size_t  la;
    size_t  lb;
    reset_world();
    np_cvfs_set_user(0xC0FFEEu);
    (void)np_cvfs_record_fault(5u, NP_CVNS_FAULT_HR_CHANGE, 0u);
    (void)np_cvfs_record_fault(6u, NP_CVNS_FAULT_IMPEDANCE, NP_CVFS_SIDE_LEFT);
    la = frame_now(1u, true, 0x01u, a);
    np_cvfs_poll(1u, true, 0x01u);
    check(g_saves == 1 && g_store_len == 6u + 2u * 10u + 4u, "persist: one save of the exact blob size");

    np_cvfs_init();   /* power cycle */
    lb = frame_now(1u, true, 0x01u, b);
    check(np_cvfs_current_user() == 0xC0FFEEu, "persist: the named user survives a power cycle");
    check(la == lb && memcmp(a, b, la) == 0, "persist: the frame is identical after reload");

    np_cvfs_poll(1u, true, 0x01u);
    check(g_saves == 1, "persist: nothing changed -> no second save");
}

static void test_corrupt_blob_starts_empty(void)
{
    uint8_t good[NP_CVFS_BLOB_MAX];
    size_t  good_len;
    uint8_t f[NP_CVFS_FRAME_MAX];

    reset_world();
    np_cvfs_set_user(0x55u);
    (void)np_cvfs_record_fault(9u, NP_CVNS_FAULT_HR_CHANGE, 0u);
    np_cvfs_poll(0u, true, 0u);
    memcpy(good, g_store, g_store_len);
    good_len = g_store_len;

    g_store[good_len - 1u] ^= 0x01u;
    np_cvfs_init();
    check(frame_now(0u, true, 0u, f) == 4u && np_cvfs_current_user() == 0u, "corrupt: CRC mismatch -> empty");

    memcpy(g_store, good, good_len);
    g_store[0] = 0x02u;
    np_cvfs_init();
    check(frame_now(0u, true, 0u, f) == 4u, "corrupt: unknown version -> empty");

    memcpy(g_store, good, good_len);
    g_store_len = good_len - 1u;
    np_cvfs_init();
    check(frame_now(0u, true, 0u, f) == 4u, "corrupt: truncated -> empty");

    g_store_len = 3u;
    np_cvfs_init();
    check(frame_now(0u, true, 0u, f) == 4u, "corrupt: runt -> empty");

    memcpy(g_store, good, good_len);
    g_store_len = good_len;
    np_cvfs_init();
    check(frame_now(0u, true, 0u, f) == 12u && np_cvfs_current_user() == 0x55u,
          "corrupt: the untouched blob still loads (the checks above were the corruption)");
}

static void test_failed_save_retries(void)
{
    reset_world();
    (void)np_cvfs_record_fault(1u, NP_CVNS_FAULT_HR_CHANGE, 0u);
    g_save_fails = true;
    np_cvfs_poll(0u, true, 0u);
    check(g_saves == 1 && g_store_len == 0u, "retry: first save failed");
    g_save_fails = false;
    np_cvfs_poll(0u, true, 0u);
    check(g_saves == 2 && g_store_len > 0u, "retry: the next poll saves it");
}

static void test_notify_only_on_change(void)
{
    uint8_t f[NP_CVFS_FRAME_MAX];
    size_t  len = 0u;
    reset_world();
    np_cvfs_poll(0u, true, 0u);
    check(g_notifies == 1 && g_notified_len == 4u, "notify: first poll publishes");
    np_cvfs_poll(0u, true, 0u);
    check(g_notifies == 1, "notify: unchanged -> silent");
    np_cvfs_poll(2u, true, 0u);
    check(g_notifies == 2 && g_notified[1] == 2u, "notify: re-enable state change publishes");
    np_cvfs_poll(2u, true, 0x02u);
    check(g_notifies == 3 && g_notified[3] == 0x02u, "notify: flag change publishes");
    (void)np_cvfs_record_fault(8u, NP_CVNS_FAULT_HR_CHANGE, 0u);
    np_cvfs_poll(2u, true, 0x02u);
    check(g_notifies == 4 && g_notified[2] == 1u, "notify: a new fault publishes");
    check(np_cvfs_read(f, sizeof f, &len) == NP_HUB_OK && len == g_notified_len &&
          memcmp(f, g_notified, len) == 0, "read: returns the last published frame");
}

static void test_active_user_write(void)
{
    const uint8_t ok[4]   = { 0x04, 0x03, 0x02, 0x01 };
    const uint8_t zero[4] = { 0, 0, 0, 0 };
    const uint8_t any[4]  = { 0xFF, 0xFF, 0xFF, 0xFF };
    reset_world();
    check(np_cvfs_on_active_user_write(ok, 3u) == NP_HUB_ERR_INVALID_ARG, "user write: 3 bytes refused");
    check(np_cvfs_on_active_user_write(ok, 5u) == NP_HUB_ERR_INVALID_ARG, "user write: 5 bytes refused");
    check(np_cvfs_on_active_user_write(zero, 4u) == NP_HUB_ERR_INVALID_ARG, "user write: 0 refused");
    check(np_cvfs_on_active_user_write(any, 4u) == NP_HUB_ERR_INVALID_ARG, "user write: 0xFFFFFFFF refused");
    check(np_cvfs_on_active_user_write(NULL, 4u) == NP_HUB_ERR_INVALID_ARG, "user write: NULL refused");
    check(g_posted_users == 0, "user write: nothing refused reached the hub");
    check(np_cvfs_on_active_user_write(ok, 4u) == NP_HUB_OK && g_posted_user == 0x01020304u,
          "user write: little-endian tag posted to the hub (same tag the apps derive)");
    check(np_cvfs_current_user() == 0u,
          "user write: attribution waits until the heartbeat forwards the tag to the safety MCU");
}

static void test_reenable_confirm_write(void)
{
    const uint8_t one = 0x01u;
    const uint8_t two = 0x02u;
    reset_world();
    check(np_cvfs_on_reenable_confirm_write(&two, 1u) == NP_HUB_ERR_INVALID_ARG, "confirm: 0x02 refused");
    check(np_cvfs_on_reenable_confirm_write(&one, 2u) == NP_HUB_ERR_INVALID_ARG, "confirm: 2 bytes refused");
    check(np_cvfs_on_reenable_confirm_write(NULL, 1u) == NP_HUB_ERR_INVALID_ARG, "confirm: NULL refused");
    check(g_confirms == 0, "confirm: nothing refused reached the re-enable manager");
    check(np_cvfs_on_reenable_confirm_write(&one, 1u) == NP_HUB_OK && g_confirms == 1, "confirm: 0x01 forwarded");
    g_confirm_rc = NP_HUB_ERR_SAFETY_REJECTED;
    check(np_cvfs_on_reenable_confirm_write(&one, 1u) == NP_HUB_ERR_SAFETY_REJECTED,
          "confirm: a rejection outside the confirm window reaches the app as a failed write");
}

int main(void)
{
    printf("── np_cvns_fault_summary_tests (NP-SW-FAULTMSG-001 §9.6) ──\n");
    test_empty();
    test_golden_frame_matches_app_parser();
    test_flags_and_state();
    test_per_user_scope();
    test_newest_four_oldest_first_and_eviction();
    test_record_validation();
    test_side_mask();
    test_persistence_round_trip();
    test_corrupt_blob_starts_empty();
    test_failed_save_retries();
    test_notify_only_on_change();
    test_active_user_write();
    test_reenable_confirm_write();
    if (g_failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}
