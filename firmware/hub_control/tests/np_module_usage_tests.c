/*
 * np_module_usage_tests.c — the per-module running totals (REQ-FWHUB-93)
 * Document: NP-FW-HUB-001 §6.11; NP-FW-NVRAM-001 §8.5
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * The Config store is replaced by one in-memory replicated record that can be
 * made absent or unreadable, so the load and commit rules are tested without
 * littlefs (np_cfg_store_tests covers the store itself).
 */

#include <stdio.h>
#include <string.h>

#include "np_module_usage.h"
#include "np_cfg_store.h"

/* ── Harness ─────────────────────────────────────────────────────────────────── */

static int g_failures;

static void check(int cond, const char *name)
{
    printf("%s: %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond) { g_failures++; }
}

void np_transport_hal_enter_critical(void) {}
void np_transport_hal_exit_critical(void)  {}

/* ── The store stand-in ──────────────────────────────────────────────────────── */

static uint8_t         g_rec[NP_CFG_REPLICA_TABLE_MAX_PAYLOAD];
static size_t          g_rec_len;
static bool            g_rec_exists;
static np_hub_status_t g_read_rc;      /* forced read result; NP_HUB_OK = use g_rec */
static unsigned        g_writes;
static size_t          g_last_write_len;

np_hub_status_t np_cfg_store_replicated_read(np_cfg_file_t file, uint8_t *payload, size_t len)
{
    if (file != NP_CFG_FILE_MODULE_USAGE) { return NP_HUB_ERR_INVALID_ARG; }
    if (g_read_rc != NP_HUB_OK) { return g_read_rc; }
    if (!g_rec_exists) { return NP_HUB_ERR_NOT_PRESENT; }
    if (len != g_rec_len) { return NP_HUB_ERR_STORE_INTEGRITY; }
    memcpy(payload, g_rec, len);
    return NP_HUB_OK;
}

np_hub_status_t np_cfg_store_replicated_write(np_cfg_file_t file, const uint8_t *payload, size_t len)
{
    if (file != NP_CFG_FILE_MODULE_USAGE || len > sizeof g_rec) { return NP_HUB_ERR_INVALID_ARG; }
    memcpy(g_rec, payload, len);
    g_rec_len = len;
    g_rec_exists = true;
    g_writes++;
    g_last_write_len = len;
    return NP_HUB_OK;
}

static void store_empty(void)
{
    memset(g_rec, 0, sizeof g_rec);
    g_rec_len = 0U;
    g_rec_exists = false;
    g_read_rc = NP_HUB_OK;
    g_writes = 0U;
    g_last_write_len = 0U;
}

/* Reboot: RAM forgotten, the record kept. */
static void reboot(void)
{
    np_mu_init();
    (void)np_mu_load();
}

static const uint8_t UID_A[NP_MU_ID_LEN] = { 0xA1, 2, 3, 4, 5, 6, 7, 8 };
static const uint8_t UID_B[NP_MU_ID_LEN] = { 0xB1, 2, 3, 4, 5, 6, 7, 8 };

/* The stored row for `key`, read back through the codec from the record itself. */
static bool stored_row(const np_mu_key_t *key, np_mu_row_t *out)
{
    static np_mu_row_t rows[NP_MU_MAX_ROWS];
    size_t n = 0U;
    if (!g_rec_exists || !np_mu_decode(g_rec, rows, NP_MU_MAX_ROWS, &n)) { return false; }
    for (size_t i = 0U; i < n; i++) {
        if (rows[i].key.kind == key->kind && memcmp(rows[i].key.id, key->id, NP_MU_ID_LEN) == 0) {
            *out = rows[i];
            return true;
        }
    }
    return false;
}

/* ── Tests ───────────────────────────────────────────────────────────────────── */

/* @verifies REQ-FWHUB-93#a */
static void test_each_module_has_one_row_keyed_by_its_uid_or_its_port(void)
{
    store_empty();
    reboot();
    const np_mu_key_t tile = np_mu_key_uid(UID_A);
    const np_mu_key_t port = np_mu_key_port(NP_HUB_SLOT_CVNS, NP_MOD_CVNS);
    np_mu_session_begin();
    np_mu_mark_present(&tile);
    np_mu_mark_present(&tile);          /* twice in one session is once */
    np_mu_mark_present(&port);
    (void)np_mu_session_end(60000U);

    np_mu_row_t rows[NP_MU_MAX_ROWS];
    const size_t n = np_mu_snapshot(rows, NP_MU_MAX_ROWS);
    check(n == 2U, "rows: one row per module, however often it is marked");
    np_mu_row_t r;
    check(stored_row(&tile, &r) && r.key.kind == NP_MU_KEY_UID && memcmp(r.key.id, UID_A, NP_MU_ID_LEN) == 0,
          "rows: a tile's row is keyed by its UID");
    check(stored_row(&port, &r) && r.key.kind == NP_MU_KEY_PORT && r.key.id[0] == NP_HUB_SLOT_CVNS
          && r.key.id[1] == (uint8_t)NP_MOD_CVNS,
          "rows: an accessory's row is keyed by its slot and module type");
}

/* @verifies REQ-FWHUB-93#b */
static void test_present_and_used_are_counted_with_their_time(void)
{
    store_empty();
    reboot();
    const np_mu_key_t fitted_only = np_mu_key_uid(UID_A);
    const np_mu_key_t driven      = np_mu_key_uid(UID_B);
    const np_mu_key_t port        = np_mu_key_port(NP_HUB_SLOT_TDCS, NP_MOD_TDCS);

    np_mu_session_begin();
    np_mu_mark_present(&fitted_only);
    np_mu_add_used(&driven, 90400U);                 /* 90.4 s of dose ticks */
    np_mu_slot_on(NP_HUB_SLOT_TDCS, NP_MOD_TDCS, 1000U);
    np_mu_slot_on(NP_HUB_SLOT_TDCS, NP_MOD_TDCS, 5000U);   /* already on: the first start stands */
    np_mu_slot_off(NP_HUB_SLOT_TDCS, 31000U);       /* 30 s */
    np_mu_slot_off(NP_HUB_SLOT_TDCS, 40000U);       /* off with nothing on: nothing */
    np_mu_slot_on(NP_HUB_SLOT_TDCS, NP_MOD_TDCS, 50000U);  /* still on at the end: closed at 120 s → 70 s */
    (void)np_mu_session_end(120000U);

    np_mu_row_t r;
    check(stored_row(&fitted_only, &r) && r.sessions_present == 1U && r.present_s == 120U
          && r.sessions_used == 0U && r.used_s == 0U,
          "time: a fitted, undriven module counts one session present and the session's 120 s, and no use");
    check(stored_row(&driven, &r) && r.sessions_present == 1U && r.present_s == 120U
          && r.sessions_used == 1U && r.used_s == 90U,
          "time: a driven tile is present for the session and used for its 90 s of drive");
    check(stored_row(&port, &r) && r.sessions_used == 1U && r.used_s == 100U,
          "time: an accessory's drive is every on-to-off interval, the last closed at session end (30 s + 70 s)");

    /* A second, shorter session: counts and time grow separately. */
    np_mu_session_begin();
    np_mu_mark_present(&fitted_only);
    np_mu_add_used(&driven, 1500U);                  /* 1.5 s rounds to 2 s */
    (void)np_mu_session_end(10000U);
    check(stored_row(&fitted_only, &r) && r.sessions_present == 2U && r.present_s == 130U,
          "time: the next session adds one session and its own 10 s, not another 120 s");
    check(stored_row(&driven, &r) && r.sessions_used == 2U && r.used_s == 92U,
          "time: drive time adds per session, rounded to the second");
    check(stored_row(&port, &r) && r.sessions_present == 1U && r.sessions_used == 1U,
          "time: a module absent from a session is not counted in it");
}

/* @verifies REQ-FWHUB-93#c */
static void test_the_table_is_upserted_once_per_session_never_appended(void)
{
    store_empty();
    reboot();
    const np_mu_key_t tile = np_mu_key_uid(UID_A);
    for (unsigned s = 0U; s < 5U; s++) {
        np_mu_session_begin();
        np_mu_add_used(&tile, 1000U);
        (void)np_mu_session_end(2000U);
    }
    check(g_writes == 5U, "upsert: one write per session end");
    check(g_last_write_len == NP_MU_TABLE_BYTES, "upsert: every write is the same fixed-size record, whatever has happened");
    np_mu_row_t rows[NP_MU_MAX_ROWS];
    check(np_mu_snapshot(rows, NP_MU_MAX_ROWS) == 1U, "upsert: five sessions of one module are one row, not five");
    np_mu_row_t r;
    check(stored_row(&tile, &r) && r.sessions_used == 5U && r.used_s == 5U && r.present_s == 10U,
          "upsert: the row holds the running totals");

    reboot();
    check(np_mu_snapshot(rows, NP_MU_MAX_ROWS) == 1U && rows[0].sessions_used == 5U,
          "upsert: the totals survive a reboot");
}

/* @verifies REQ-FWHUB-93#d */
static void test_an_unreadable_table_is_never_overwritten(void)
{
    store_empty();
    reboot();
    check(np_mu_loaded(), "load: an absent record is an empty table, loaded");

    const np_mu_key_t tile = np_mu_key_uid(UID_A);
    np_mu_session_begin();
    np_mu_add_used(&tile, 4000U);
    (void)np_mu_session_end(5000U);
    const unsigned writes = g_writes;

    g_read_rc = NP_HUB_ERR_STORE_IO;             /* the record exists but cannot be read */
    np_mu_init();
    check(np_mu_load() == NP_HUB_ERR_STORE_IO && !np_mu_loaded(), "load: an unreadable record leaves the table unloaded");
    np_mu_session_begin();
    np_mu_add_used(&tile, 4000U);
    check(np_mu_session_end(5000U) != NP_HUB_OK && g_writes == writes && np_mu_sessions_dropped() == 1U,
          "load: the session end writes nothing over it, and counts the session dropped");

    g_read_rc = NP_HUB_OK;                       /* readable again */
    np_mu_session_begin();
    np_mu_add_used(&tile, 4000U);
    (void)np_mu_session_end(5000U);
    np_mu_row_t r;
    check(stored_row(&tile, &r) && r.sessions_used == 2U,
          "load: the next session end loads the old totals and adds to them, not to zero");
}

/* @verifies REQ-FWHUB-93#e */
static void test_faults_are_counted_against_the_accessory_port(void)
{
    store_empty();
    reboot();
    const np_mu_key_t port = np_mu_key_port(NP_HUB_SLOT_CVNS, NP_MOD_CVNS);

    /* Faults between sessions are held and land at the next commit. */
    np_mu_note_fault(NP_HUB_SLOT_CVNS, (uint8_t)NP_MOD_CVNS, 0xC1U);
    np_mu_note_fault(NP_HUB_SLOT_CVNS, (uint8_t)NP_MOD_CVNS, 0xC1U);
    np_mu_note_fault(NP_HUB_SLOT_NONE, (uint8_t)NP_MOD_PBM_BASE, 0x10U);   /* no slot: no module */
    np_mu_session_begin();
    np_mu_note_fault(NP_HUB_SLOT_CVNS, (uint8_t)NP_MOD_CVNS, 5U);
    np_mu_note_fault(NP_HUB_SLOT_CVNS, (uint8_t)NP_MOD_CVNS, 2U);
    np_mu_note_fault(NP_HUB_SLOT_CVNS, (uint8_t)NP_MOD_CVNS, 3U);
    np_mu_note_fault(NP_HUB_SLOT_CVNS, (uint8_t)NP_MOD_CVNS, 4U);          /* a fifth code: no entry left */
    (void)np_mu_session_end(1000U);

    np_mu_row_t r;
    check(stored_row(&port, &r), "faults: the port has a row");
    check(r.fault_code[0] == 0xC1U && r.fault_count[0] == 2U, "faults: a code's faults are counted together");
    check(r.fault_code[1] == 5U && r.fault_count[1] == 1U && r.fault_code[3] == 3U && r.fault_count[3] == 1U,
          "faults: each further code takes the next entry");
    check(r.faults_other == 1U, "faults: a code beyond the four entries is counted as other");
    check(r.sessions_present == 0U && r.sessions_used == 0U, "faults: a fault alone does not count a session");
    np_mu_row_t rows[NP_MU_MAX_ROWS];
    check(np_mu_snapshot(rows, NP_MU_MAX_ROWS) == 1U, "faults: a fault with no slot makes no row");

    np_mu_note_fault(NP_HUB_SLOT_CVNS, (uint8_t)NP_MOD_CVNS, 0xC1U);
    np_mu_session_begin();
    (void)np_mu_session_end(1000U);
    check(stored_row(&port, &r) && r.fault_count[0] == 3U, "faults: the totals run on across commits");
}

/* @verifies REQ-FWHUB-93#a */
static void test_a_full_table_gives_up_the_least_present_module_not_one_in_use(void)
{
    store_empty();
    reboot();
    uint8_t uid[NP_MU_ID_LEN] = { 0 };
    /* Fill the table; module i is present i+1 times, so module 0 is the least present. */
    for (unsigned i = 0U; i < NP_MU_MAX_ROWS; i++) {
        uid[0] = (uint8_t)i; uid[1] = (uint8_t)(i >> 8); uid[7] = 0x55U;
        const np_mu_key_t k = np_mu_key_uid(uid);
        np_mu_session_begin();
        np_mu_mark_present(&k);
        (void)np_mu_session_end(1000U);
    }
    uid[0] = 1U; uid[1] = 0U;
    const np_mu_key_t kept = np_mu_key_uid(uid);
    np_mu_session_begin();
    np_mu_mark_present(&kept);                       /* module 1 is in use this session */
    uint8_t fresh_uid[NP_MU_ID_LEN] = { 0xEE, 0xEE, 0, 0, 0, 0, 0, 0x55U };
    const np_mu_key_t newcomer = np_mu_key_uid(fresh_uid);
    np_mu_mark_present(&newcomer);
    (void)np_mu_session_end(1000U);

    np_mu_row_t rows[NP_MU_MAX_ROWS];
    check(np_mu_snapshot(rows, NP_MU_MAX_ROWS) == NP_MU_MAX_ROWS, "full: the table stays at its capacity");
    np_mu_row_t r;
    check(stored_row(&newcomer, &r) && r.sessions_present == 1U, "full: a new module still gets a row");
    check(stored_row(&kept, &r), "full: a module in use this session is never the one given up");
}

/* @verifies-none harness check: the stored record's codec round-trips every field */
static void test_codec_round_trip(void)
{
    np_mu_row_t in[2];
    memset(in, 0, sizeof in);
    in[0].key = np_mu_key_uid(UID_A);
    in[0].sessions_present = 0x01020304U; in[0].present_s = 0xFFFFFFFFU;
    in[0].sessions_used = 7U; in[0].used_s = 9U;
    in[0].fault_code[2] = 0x7EU; in[0].fault_count[2] = 0x11223344U; in[0].faults_other = 5U;
    in[1].key = np_mu_key_port(3U, NP_MOD_AUDIO);
    static uint8_t buf[NP_MU_TABLE_BYTES];
    np_mu_encode(in, 2U, buf);
    np_mu_row_t out[2];
    size_t n = 0U;
    check(np_mu_decode(buf, out, 2U, &n) && n == 2U && memcmp(&in[0], &out[0], sizeof in[0]) == 0
          && memcmp(&in[1], &out[1], sizeof in[1]) == 0, "codec: every field survives encode and decode");
    buf[0] = 99U;
    check(!np_mu_decode(buf, out, 2U, &n), "codec: a record of an unknown version is refused, not misread");
}

int main(void)
{
    test_each_module_has_one_row_keyed_by_its_uid_or_its_port();
    test_present_and_used_are_counted_with_their_time();
    test_the_table_is_upserted_once_per_session_never_appended();
    test_an_unreadable_table_is_never_overwritten();
    test_faults_are_counted_against_the_accessory_port();
    test_a_full_table_gives_up_the_least_present_module_not_one_in_use();
    test_codec_round_trip();
    printf("\n%s (%d failures)\n", g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", g_failures);
    return g_failures == 0 ? 0 : 1;
}
