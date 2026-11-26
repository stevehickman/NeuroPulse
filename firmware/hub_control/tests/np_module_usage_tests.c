/*
 * np_module_usage_tests.c — the per-module running totals (REQ-FWHUB-93)
 * Document: NP-FW-HUB-001 §6.11; NP-FW-NVRAM-001 §8.5
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * The Config store is replaced by one in-memory replicated record that can be
 * made absent or unreadable, so the load and commit rules are tested without
 * littlefs (np_cfg_store_tests covers the store itself).
 *
 * This table is Map 3 (NP-FW-NVRAM-001 §8.5, §3.3.1.6).  The version rule
 * for the record (§7.2) is checked here against the production reader and
 * writer, as np_map3_record_tests once checked the retired journal's.
 * Falsified by hand (2026-11-25): with np_mu_decode() refusing a version
 * other than NP_MU_VERSION, or a row length other than NP_MU_ROW_BYTES, the
 * version test fails; with image_update() zeroing every row before writing
 * it, the tail-carrying assertion fails.
 */

#include <math.h>
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

/* The download's snapshot of the loaded table: its row count, or 0 when it is not loaded. */
static size_t snap(np_mu_row_t *rows)
{
    size_t n = 0U;
    return (np_mu_snapshot(rows, NP_MU_MAX_ROWS, &n) == NP_HUB_OK) ? n : 0U;
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
    const size_t n = snap(rows);
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
    check(snap(rows) == 1U, "upsert: five sessions of one module are one row, not five");
    np_mu_row_t r;
    check(stored_row(&tile, &r) && r.sessions_used == 5U && r.used_s == 5U && r.present_s == 10U,
          "upsert: the row holds the running totals");

    reboot();
    check(snap(rows) == 1U && rows[0].sessions_used == 5U,
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
    check(snap(rows) == 1U, "faults: a fault with no slot makes no row");

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
    check(snap(rows) == NP_MU_MAX_ROWS, "full: the table stays at its capacity");
    np_mu_row_t r;
    check(stored_row(&newcomer, &r) && r.sessions_present == 1U, "full: a new module still gets a row");
    check(stored_row(&kept, &r), "full: a module in use this session is never the one given up");
}

/* @verifies REQ-FWHUB-93#f */
static void test_mates_dose_throttles_and_the_unseated_flag_are_running_totals(void)
{
    store_empty();
    reboot();
    const np_mu_key_t tile = np_mu_key_uid(UID_A);
    const np_mu_key_t port = np_mu_key_port(NP_HUB_SLOT_VNS_HRV, NP_MOD_VNS_HRV);

    /* Between sessions: a hot-plugged accessory's mate is held and lands at the next commit. */
    np_mu_note_mate(&port);
    np_mu_session_begin();
    np_mu_add_used(&tile, 60000U);
    np_mu_add_dose(&tile, 2500U);
    np_mu_note_throttle(&tile);
    (void)np_mu_session_end(60000U);
    np_mu_row_t r;
    check(stored_row(&port, &r) && r.mate_cycles == 1U && r.sessions_present == 0U,
          "mates: a mate noted between sessions is one mate, and no session present");
    check(stored_row(&tile, &r) && r.dose_accum == 2500U && r.throttle_count == 1U && r.fault_flags == 0U,
          "dose and throttle: a session's dose and its throttle land in the tile's row");

    np_mu_session_begin();
    np_mu_add_used(&tile, 1000U);
    np_mu_add_dose(&tile, 500U);
    np_mu_note_unseated_at_close(&tile);
    np_mu_note_mate(&tile);
    (void)np_mu_session_end(1000U);
    check(stored_row(&tile, &r) && r.dose_accum == 3000U && r.throttle_count == 1U && r.mate_cycles == 1U,
          "running totals: the next session adds to them; a session that did not throttle adds no throttle");
    check(r.fault_flags == NP_MU_FAULT_UNSEATED_AT_CLOSE, "flags: a module closed unseated sets bit 0");

    np_mu_session_begin();
    np_mu_add_used(&tile, 1000U);
    (void)np_mu_session_end(1000U);
    check(stored_row(&tile, &r) && r.fault_flags == NP_MU_FAULT_UNSEATED_AT_CLOSE,
          "flags: bit 0 stays set for good, a running OR, not a per-session value");

    const float d3[3] = { 1.2345f, 0.0f, 2.0f };
    const float bad[3] = { -1.0f, NAN, 0.75f };   /* a negative and a NaN add nothing */
    const float huge[2] = { 3.0e6f, 3.0e6f };
    check(np_mu_dose_mJ_cm2(d3, 3U) == 3235U, "dose: J/cm² over the wavelengths, summed, in whole mJ/cm² (3.2345 → 3235)");
    check(np_mu_dose_mJ_cm2(bad, 3U) == 750U, "dose: a negative or not-a-number entry adds nothing");
    check(np_mu_dose_mJ_cm2(huge, 2U) == UINT32_MAX, "dose: a sum past the field's range is its maximum, not a wrap");
    check(np_mu_dose_mJ_cm2(NULL, 3U) == 0U, "dose: no record is no dose");
}

/* @verifies REQ-FWHUB-93#g */
static void test_a_row_known_to_fall_short_says_so(void)
{
    /* (1) A commit dropped because the record could not be read. */
    store_empty();
    reboot();
    const np_mu_key_t tile = np_mu_key_uid(UID_A);
    const np_mu_key_t other = np_mu_key_uid(UID_B);
    np_mu_session_begin();
    np_mu_mark_present(&tile);
    np_mu_mark_present(&other);
    (void)np_mu_session_end(1000U);
    np_mu_row_t r;
    check(stored_row(&tile, &r) && r.detail_lost == 0U, "detail_lost: a row with nothing missing is clear");

    g_read_rc = NP_HUB_ERR_STORE_INTEGRITY;
    np_mu_init();
    (void)np_mu_load();
    np_mu_session_begin();
    np_mu_add_used(&tile, 5000U);
    check(np_mu_session_end(5000U) != NP_HUB_OK, "detail_lost: a session whose record cannot be read is dropped");
    g_read_rc = NP_HUB_OK;
    np_mu_session_begin();
    np_mu_mark_present(&other);
    (void)np_mu_session_end(1000U);
    check(stored_row(&tile, &r) && r.detail_lost == 1U && r.sessions_used == 0U,
          "detail_lost: the module whose session was dropped is marked short at the next commit");
    check(stored_row(&other, &r) && r.detail_lost == 0U && r.sessions_present == 2U,
          "detail_lost: a module that lost nothing is not marked");
    np_mu_session_begin();
    (void)np_mu_session_end(1000U);
    check(stored_row(&tile, &r) && r.detail_lost == 1U, "detail_lost: once set it stays set");

    /* (2) A row created by giving up another module's row. */
    store_empty();
    reboot();
    uint8_t uid[NP_MU_ID_LEN] = { 0 };
    for (unsigned i = 0U; i < NP_MU_MAX_ROWS; i++) {
        uid[0] = (uint8_t)i; uid[1] = (uint8_t)(i >> 8); uid[7] = 0x66U;
        const np_mu_key_t k = np_mu_key_uid(uid);
        np_mu_session_begin();
        np_mu_mark_present(&k);
        (void)np_mu_session_end(1000U);
    }
    uid[0] = 0U; uid[1] = 0U;
    const np_mu_key_t first = np_mu_key_uid(uid);
    check(stored_row(&first, &r) && r.detail_lost == 0U, "detail_lost: rows created while the table had room are clear");
    const uint8_t late_uid[NP_MU_ID_LEN] = { 0xEE, 0xEF, 0, 0, 0, 0, 0, 0x66U };
    const np_mu_key_t late = np_mu_key_uid(late_uid);
    np_mu_session_begin();
    np_mu_mark_present(&late);
    (void)np_mu_session_end(1000U);
    check(stored_row(&late, &r) && r.detail_lost == 1U && r.sessions_present == 1U,
          "detail_lost: a row that took another module's place is marked, because the helmet has forgotten a module");
    check(!stored_row(&first, &r), "detail_lost: the given-up module's row is gone from the snapshot, which is how the app sees it");
}

/* Build a record as a newer firmware would: version 2, rows `row_len` bytes, the
 * version-1 fields where version 1 puts them and a tail of 0xC0.. after them. */
static void newer_record(uint8_t *rec, const np_mu_row_t *rows, size_t n, uint8_t row_len)
{
    static uint8_t v1[NP_MU_TABLE_BYTES];
    np_mu_encode(rows, n, v1);
    memset(rec, 0, NP_MU_TABLE_BYTES);
    rec[0] = 2U;
    rec[1] = row_len;
    rec[2] = (uint8_t)n;
    rec[3] = (uint8_t)(n >> 8);
    for (size_t i = 0U; i < n; i++) {
        uint8_t *r = &rec[NP_MU_HDR_BYTES + i * row_len];
        memcpy(r, &v1[NP_MU_HDR_BYTES + i * NP_MU_ROW_BYTES], NP_MU_ROW_BYTES);
        for (size_t k = NP_MU_ROW_BYTES; k < row_len; k++) { r[k] = (uint8_t)(0xC0U + i + k); }
    }
}

/* @verifies REQ-FWHUB-93#h */
static void test_a_record_of_another_version_is_read_and_its_newer_fields_kept(void)
{
    enum { ROW2 = NP_MU_ROW_BYTES + 12U };
    np_mu_row_t in[2];
    memset(in, 0, sizeof in);
    in[0].key = np_mu_key_uid(UID_A);
    in[0].sessions_present = 7U; in[0].mate_cycles = 3U; in[0].dose_accum = 4000U;
    in[1].key = np_mu_key_port(NP_HUB_SLOT_CVNS, NP_MOD_CVNS);
    in[1].sessions_used = 2U;

    store_empty();
    newer_record(g_rec, in, 2U, (uint8_t)ROW2);
    g_rec_len = NP_MU_TABLE_BYTES;
    g_rec_exists = true;
    np_mu_init();
    check(np_mu_load() == NP_HUB_OK, "version: a version-2 record with a longer row is loaded, not refused");
    np_mu_row_t r;
    check(stored_row(&in[0].key, &r) && r.sessions_present == 7U && r.mate_cycles == 3U && r.dose_accum == 4000U,
          "version: every version-1 field of a newer row is read where version 1 puts it");
    check(stored_row(&in[1].key, &r) && r.sessions_used == 2U, "version: the row length steps to the next row");

    const np_mu_key_t newcomer = np_mu_key_uid(UID_B);
    np_mu_session_begin();
    np_mu_mark_present(&in[0].key);
    np_mu_mark_present(&newcomer);
    (void)np_mu_session_end(1000U);
    check(g_rec[0] == 2U && g_rec[1] == ROW2 && g_rec[2] == 3U,
          "version: the rewrite keeps the newer version and its row length");
    const uint8_t *row0 = &g_rec[NP_MU_HDR_BYTES];
    bool tail_kept = true;
    for (size_t k = NP_MU_ROW_BYTES; k < ROW2; k++) { if (row0[k] != (uint8_t)(0xC0U + k)) { tail_kept = false; } }
    check(tail_kept, "version: a field this firmware does not know is carried through its rewrite unchanged");
    const uint8_t *row2 = &g_rec[NP_MU_HDR_BYTES + 2U * ROW2];
    bool tail_zero = true;
    for (size_t k = NP_MU_ROW_BYTES; k < ROW2; k++) { if (row2[k] != 0U) { tail_zero = false; } }
    check(tail_zero && stored_row(&newcomer, &r) && r.sessions_present == 1U,
          "version: a row this firmware creates starts its newer fields at zero");
    check(stored_row(&in[0].key, &r) && r.sessions_present == 8U, "version: and the totals it knows still run on");

    /* What is refused, whole, rather than misread. */
    static uint8_t bad[NP_MU_TABLE_BYTES];
    np_mu_row_t out[NP_MU_MAX_ROWS];
    size_t n = 0U;
    np_mu_encode(in, 2U, bad);
    bad[0] = 0U;
    check(!np_mu_decode(bad, out, NP_MU_MAX_ROWS, &n), "version: version 0, which no firmware writes, is refused");
    np_mu_encode(in, 2U, bad);
    bad[1] = (uint8_t)(NP_MU_ROW_BYTES - 1U);
    check(!np_mu_decode(bad, out, NP_MU_MAX_ROWS, &n), "version: a row shorter than version 1's is refused");
    np_mu_encode(in, 2U, bad);
    bad[1] = (uint8_t)(NP_MU_ROW_MAX_BYTES + 1U);
    check(!np_mu_decode(bad, out, NP_MU_MAX_ROWS, &n), "version: a row too long for the record is refused");
    np_mu_encode(in, 2U, bad);
    bad[2] = (uint8_t)(NP_MU_MAX_ROWS + 1U);
    check(!np_mu_decode(bad, out, NP_MU_MAX_ROWS, &n), "version: more rows than the table holds is refused");
    np_mu_encode(in, 2U, bad);
    bad[NP_MU_HDR_BYTES + NP_MU_ROW_BYTES] = 9U;          /* the second row's key kind */
    check(!np_mu_decode(bad, out, NP_MU_MAX_ROWS, &n), "version: a row with an unknown key kind is refused");

    /* A record of the wrong length is the store's refusal, and the table is then not loaded. */
    g_rec_len = NP_MU_TABLE_BYTES - 1U;
    np_mu_init();
    check(np_mu_load() == NP_HUB_ERR_STORE_INTEGRITY && !np_mu_loaded(),
          "version: a record of another length is unreadable, never taken as empty");
}

/* @verifies REQ-FWHUB-93#i */
static void test_the_snapshot_reports_unknown_never_an_empty_table(void)
{
    store_empty();
    reboot();
    const np_mu_key_t tile = np_mu_key_uid(UID_A);
    np_mu_session_begin();
    np_mu_mark_present(&tile);
    (void)np_mu_session_end(1000U);
    np_mu_row_t rows[NP_MU_MAX_ROWS];
    size_t n = 99U;
    check(np_mu_snapshot(rows, NP_MU_MAX_ROWS, &n) == NP_HUB_OK && n == 1U, "snapshot: a loaded table is read whole");
    const unsigned writes = g_writes;
    check(np_mu_snapshot(rows, NP_MU_MAX_ROWS, &n) == NP_HUB_OK && n == 1U && g_writes == writes,
          "snapshot: reading it changes nothing on the helmet (no watermark, no row retired)");

    g_read_rc = NP_HUB_ERR_STORE_IO;
    np_mu_init();
    (void)np_mu_load();
    n = 99U;
    check(np_mu_snapshot(rows, NP_MU_MAX_ROWS, &n) == NP_HUB_ERR_STORE_IO && n == 0U,
          "snapshot: a table that could not be read is reported unknown, not as a table with no rows");
    check(np_mu_snapshot(NULL, NP_MU_MAX_ROWS, &n) == NP_HUB_ERR_INVALID_ARG, "snapshot: no buffer is refused");
    g_read_rc = NP_HUB_OK;
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
    in[0].mate_cycles = 0xA1B2C3D4U; in[0].dose_accum = 0x55667788U; in[0].throttle_count = 0x0BADF00DU;
    in[0].fault_flags = 0x81U; in[0].detail_lost = 1U;
    in[1].key = np_mu_key_port(3U, NP_MOD_AUDIO);
    static uint8_t buf[NP_MU_TABLE_BYTES];
    np_mu_encode(in, 2U, buf);
    np_mu_row_t out[2];
    size_t n = 0U;
    check(buf[0] == NP_MU_VERSION && buf[1] == NP_MU_ROW_BYTES && buf[NP_MU_HDR_BYTES + NP_MU_ROW_BYTES - 1U] == 0U,
          "codec: a version-1 record of 64-byte rows, the reserved byte zero");
    check(np_mu_decode(buf, out, 2U, &n) && n == 2U && memcmp(&in[0], &out[0], sizeof in[0]) == 0
          && memcmp(&in[1], &out[1], sizeof in[1]) == 0, "codec: every field survives encode and decode");
}

int main(void)
{
    test_each_module_has_one_row_keyed_by_its_uid_or_its_port();
    test_present_and_used_are_counted_with_their_time();
    test_the_table_is_upserted_once_per_session_never_appended();
    test_an_unreadable_table_is_never_overwritten();
    test_faults_are_counted_against_the_accessory_port();
    test_a_full_table_gives_up_the_least_present_module_not_one_in_use();
    test_mates_dose_throttles_and_the_unseated_flag_are_running_totals();
    test_a_row_known_to_fall_short_says_so();
    test_a_record_of_another_version_is_read_and_its_newer_fields_kept();
    test_the_snapshot_reports_unknown_never_an_empty_table();
    test_codec_round_trip();
    printf("\n%s (%d failures)\n", g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", g_failures);
    return g_failures == 0 ? 0 : 1;
}
