/*
 * np_module_usage_tests.c — the per-module running totals (REQ-FWHUB-93)
 * Document: NP-FW-HUB-001 §6.11; NP-FW-NVRAM-001 §3.3.1.6, §8.5
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * The Config store is replaced by two in-memory replicated records (the tiles'
 * and the ports') that can each be made absent or unreadable, so the load and
 * commit rules are tested without littlefs (np_cfg_store_tests covers the
 * store itself).
 *
 * These tables are Map 3 (NP-FW-NVRAM-001 §8.5, §3.3.1.6).  The version rule
 * for the record (§7.2) is checked here against the production reader and
 * writer, for each table.
 * Falsified by hand (2026-11-25, for the one-table record): with
 * np_mu_decode() refusing a version other than NP_MU_VERSION, or a row length
 * other than the version-1 row, the version test fails; with image_update()
 * zeroing every row before writing it, the tail-carrying assertion fails.
 * Falsified again by hand (2026-11-26, two tables, OI-NVRAM-23): with
 * np_mu_slot_on() ignoring a change of mask on a slot already on, the port
 * channel assertions fail; with fold() adding a channel's session whatever
 * ch_used says, the dark-channel assertion fails; with np_mu_session_end()
 * returning before committing the other table when one cannot be read, the
 * independence assertion fails.
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

/* ── The store stand-in: one record per table ────────────────────────────────── */

typedef struct {
    uint8_t         rec[NP_CFG_REPLICA_TABLE_MAX_PAYLOAD];
    size_t          len;
    bool            exists;
    np_hub_status_t read_rc;   /* forced read result; NP_HUB_OK = use rec */
    unsigned        writes;
    size_t          last_write_len;
} rec_t;

static rec_t g_st[NP_MU_TABLE_COUNT];
#define TILES (&g_st[NP_MU_TABLE_TILES])
#define PORTS (&g_st[NP_MU_TABLE_PORTS])

static rec_t *rec_of(np_cfg_file_t file)
{
    if (file == NP_CFG_FILE_MODULE_USAGE) { return TILES; }
    if (file == NP_CFG_FILE_MODULE_PORTS) { return PORTS; }
    return NULL;
}

np_hub_status_t np_cfg_store_replicated_read(np_cfg_file_t file, uint8_t *payload, size_t len)
{
    rec_t *r = rec_of(file);
    if (r == NULL) { return NP_HUB_ERR_INVALID_ARG; }
    if (r->read_rc != NP_HUB_OK) { return r->read_rc; }
    if (!r->exists) { return NP_HUB_ERR_NOT_PRESENT; }
    if (len != r->len) { return NP_HUB_ERR_STORE_INTEGRITY; }
    memcpy(payload, r->rec, len);
    return NP_HUB_OK;
}

np_hub_status_t np_cfg_store_replicated_write(np_cfg_file_t file, const uint8_t *payload, size_t len)
{
    rec_t *r = rec_of(file);
    if (r == NULL || len > sizeof r->rec) { return NP_HUB_ERR_INVALID_ARG; }
    memcpy(r->rec, payload, len);
    r->len = len;
    r->exists = true;
    r->writes++;
    r->last_write_len = len;
    return NP_HUB_OK;
}

static void store_empty(void)
{
    memset(g_st, 0, sizeof g_st);
    g_st[0].read_rc = NP_HUB_OK;
    g_st[1].read_rc = NP_HUB_OK;
}

static unsigned writes(void) { return TILES->writes + PORTS->writes; }

/* Reboot: RAM forgotten, the records kept. */
static void reboot(void)
{
    np_mu_init();
    (void)np_mu_load();
}

/* The download's snapshot of the loaded tables: its row count, or 0 when they are not loaded. */
static size_t snap(np_mu_row_t *rows)
{
    size_t n = 0U;
    return (np_mu_snapshot(rows, NP_MU_MAX_ROWS, &n) == NP_HUB_OK) ? n : 0U;
}

static const uint8_t UID_A[NP_MU_ID_LEN] = { 0xA1, 2, 3, 4, 5, 6, 7, 8 };
static const uint8_t UID_B[NP_MU_ID_LEN] = { 0xB1, 2, 3, 4, 5, 6, 7, 8 };

/* The stored row for `key`, read back through the codec from its table's record itself. */
static bool stored_row(const np_mu_key_t *key, np_mu_row_t *out)
{
    static np_mu_row_t rows[NP_MU_MAX_ROWS];
    const np_mu_table_t t = (key->kind == NP_MU_KEY_UID) ? NP_MU_TABLE_TILES : NP_MU_TABLE_PORTS;
    const rec_t *r = &g_st[t];
    size_t n = 0U;
    if (!r->exists || !np_mu_decode(t, r->rec, rows, NP_MU_MAX_ROWS, &n)) { return false; }
    for (size_t i = 0U; i < n; i++) {
        if (rows[i].key.kind == key->kind && memcmp(rows[i].key.id, key->id, NP_MU_ID_LEN) == 0) {
            *out = rows[i];
            return true;
        }
    }
    return false;
}

/* Fill the tiles' table: tile i is present i+1 times, so tile 0 is the least present. */
static void fill_tiles(uint8_t tag)
{
    uint8_t uid[NP_MU_ID_LEN] = { 0 };
    for (unsigned i = 0U; i < NP_MU_TILE_ROWS; i++) {
        uid[0] = (uint8_t)i; uid[1] = (uint8_t)(i >> 8); uid[7] = tag;
        const np_mu_key_t k = np_mu_key_uid(uid);
        np_mu_session_begin();
        np_mu_mark_present(&k);
        (void)np_mu_session_end(1000U);
    }
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
          "rows: a tile's row is keyed by its UID, in the tiles' record");
    check(stored_row(&port, &r) && r.key.kind == NP_MU_KEY_PORT && r.key.id[0] == NP_HUB_SLOT_CVNS
          && r.key.id[1] == (uint8_t)NP_MOD_CVNS,
          "rows: an accessory's row is keyed by its slot and module type, in the ports' record");
    check(TILES->rec[2] == 1U && PORTS->rec[2] == 1U, "rows: each record holds only its own kind's row");
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
    np_mu_slot_on(NP_HUB_SLOT_TDCS, NP_MOD_TDCS, 0U, 1000U);
    np_mu_slot_on(NP_HUB_SLOT_TDCS, NP_MOD_TDCS, 0U, 5000U);   /* already on: the first start stands */
    np_mu_slot_off(NP_HUB_SLOT_TDCS, 31000U);       /* 30 s */
    np_mu_slot_off(NP_HUB_SLOT_TDCS, 40000U);       /* off with nothing on: nothing */
    np_mu_slot_on(NP_HUB_SLOT_TDCS, NP_MOD_TDCS, 0U, 50000U);  /* still on at the end: closed at 120 s → 70 s */
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
    check(TILES->writes == 5U && PORTS->writes == 5U, "upsert: one write of each record per session end");
    check(TILES->last_write_len == NP_MU_TILE_TABLE_BYTES && PORTS->last_write_len == NP_MU_PORT_TABLE_BYTES,
          "upsert: every write is the same fixed-size record for its table, whatever has happened");
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
    check(np_mu_loaded(), "load: absent records are empty tables, loaded");

    const np_mu_key_t tile = np_mu_key_uid(UID_A);
    const np_mu_key_t port = np_mu_key_port(NP_HUB_SLOT_VNS_HRV, NP_MOD_VNS_HRV);
    np_mu_session_begin();
    np_mu_add_used(&tile, 4000U);
    np_mu_mark_present(&port);
    (void)np_mu_session_end(5000U);
    const unsigned tile_writes = TILES->writes;
    const unsigned port_writes = PORTS->writes;

    TILES->read_rc = NP_HUB_ERR_STORE_IO;        /* the tiles' record exists but cannot be read */
    np_mu_init();
    check(np_mu_load() == NP_HUB_ERR_STORE_IO && !np_mu_loaded() && !np_mu_table_loaded(NP_MU_TABLE_TILES)
          && np_mu_table_loaded(NP_MU_TABLE_PORTS),
          "load: an unreadable record leaves its table unloaded, and only its table");
    np_mu_session_begin();
    np_mu_add_used(&tile, 4000U);
    np_mu_mark_present(&port);
    check(np_mu_session_end(5000U) != NP_HUB_OK && TILES->writes == tile_writes && np_mu_sessions_dropped() == 1U,
          "load: the session end writes nothing over it, and counts the session dropped");
    np_mu_row_t r;
    check(PORTS->writes == port_writes + 1U && stored_row(&port, &r) && r.sessions_present == 2U,
          "load: the other table commits as usual");

    TILES->read_rc = NP_HUB_OK;                  /* readable again */
    np_mu_session_begin();
    np_mu_add_used(&tile, 4000U);
    (void)np_mu_session_end(5000U);
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
    fill_tiles(0x55U);
    const np_mu_key_t port = np_mu_key_port(NP_HUB_SLOT_AUDIO, NP_MOD_AUDIO);
    np_mu_session_begin();
    np_mu_mark_present(&port);
    (void)np_mu_session_end(1000U);

    uint8_t uid[NP_MU_ID_LEN] = { 1U, 0U, 0, 0, 0, 0, 0, 0x55U };
    const np_mu_key_t kept = np_mu_key_uid(uid);
    np_mu_session_begin();
    np_mu_mark_present(&kept);                       /* module 1 is in use this session */
    uint8_t fresh_uid[NP_MU_ID_LEN] = { 0xEE, 0xEE, 0, 0, 0, 0, 0, 0x55U };
    const np_mu_key_t newcomer = np_mu_key_uid(fresh_uid);
    np_mu_mark_present(&newcomer);
    (void)np_mu_session_end(1000U);

    np_mu_row_t rows[NP_MU_MAX_ROWS];
    check(snap(rows) == NP_MU_TILE_ROWS + 1U, "full: the tiles' table stays at its 128 rows");
    np_mu_row_t r;
    check(stored_row(&newcomer, &r) && r.sessions_present == 1U, "full: a new module still gets a row");
    check(stored_row(&kept, &r), "full: a module in use this session is never the one given up");
    check(stored_row(&port, &r) && r.sessions_present == 1U && r.detail_lost == 0U,
          "full: a tile never takes a port's row, which has a table of its own");
}

/* @verifies REQ-FWHUB-93#f */
static void test_mates_throttles_and_the_unseated_flag_are_running_totals(void)
{
    store_empty();
    reboot();
    const np_mu_key_t tile = np_mu_key_uid(UID_A);
    const np_mu_key_t port = np_mu_key_port(NP_HUB_SLOT_VNS_HRV, NP_MOD_VNS_HRV);

    /* Between sessions: a hot-plugged accessory's mate is held and lands at the next commit. */
    np_mu_note_mate(&port);
    np_mu_session_begin();
    np_mu_add_used(&tile, 60000U);
    np_mu_note_throttle(&tile);
    (void)np_mu_session_end(60000U);
    np_mu_row_t r;
    check(stored_row(&port, &r) && r.mate_cycles == 1U && r.sessions_present == 0U,
          "mates: a mate noted between sessions is one mate, and no session present");
    check(stored_row(&tile, &r) && r.throttle_count == 1U && r.fault_flags == 0U,
          "throttle: a session's throttle lands in the tile's row");

    np_mu_session_begin();
    np_mu_add_used(&tile, 1000U);
    np_mu_note_unseated_at_close(&tile);
    np_mu_note_mate(&tile);
    (void)np_mu_session_end(1000U);
    check(stored_row(&tile, &r) && r.throttle_count == 1U && r.mate_cycles == 1U,
          "running totals: the next session adds to them; a session that did not throttle adds no throttle");
    check(r.fault_flags == NP_MU_FAULT_UNSEATED_AT_CLOSE, "flags: a module closed unseated sets bit 0");

    np_mu_session_begin();
    np_mu_add_used(&tile, 1000U);
    (void)np_mu_session_end(1000U);
    check(stored_row(&tile, &r) && r.fault_flags == NP_MU_FAULT_UNSEATED_AT_CLOSE,
          "flags: bit 0 stays set for good, a running OR, not a per-session value");
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

    TILES->read_rc = NP_HUB_ERR_STORE_INTEGRITY;
    np_mu_init();
    (void)np_mu_load();
    np_mu_session_begin();
    np_mu_add_used(&tile, 5000U);
    check(np_mu_session_end(5000U) != NP_HUB_OK, "detail_lost: a session whose record cannot be read is dropped");
    TILES->read_rc = NP_HUB_OK;
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
    fill_tiles(0x66U);
    uint8_t uid[NP_MU_ID_LEN] = { 0U, 0U, 0, 0, 0, 0, 0, 0x66U };
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

/* Build table `t`'s record as a newer firmware would: version 2, rows `row_len`
 * bytes, the version-1 fields where version 1 puts them and a tail of 0xC0..
 * after them. */
static void newer_record(np_mu_table_t t, uint8_t *rec, const np_mu_row_t *rows, size_t n, uint8_t row_len)
{
    static uint8_t v1[NP_MU_TABLE_MAX_BYTES];
    const size_t v1_row = (t == NP_MU_TABLE_TILES) ? NP_MU_TILE_ROW_BYTES : NP_MU_PORT_ROW_BYTES;
    np_mu_encode(t, rows, n, v1);
    memset(rec, 0, np_mu_table_bytes(t));
    rec[0] = 2U;
    rec[1] = row_len;
    rec[2] = (uint8_t)n;
    rec[3] = (uint8_t)(n >> 8);
    for (size_t i = 0U; i < n; i++) {
        uint8_t *r = &rec[NP_MU_HDR_BYTES + i * row_len];
        memcpy(r, &v1[NP_MU_HDR_BYTES + i * v1_row], v1_row);
        for (size_t k = v1_row; k < row_len; k++) { r[k] = (uint8_t)(0xC0U + i + k); }
    }
}

/* One table's version check: a newer record is read, rewritten with its tail
 * carried through, and every malformed record is refused whole. */
static void version_check(np_mu_table_t t, const np_mu_row_t *in, const np_mu_key_t *newcomer,
                          size_t v1_row, size_t row_max, const char *what)
{
    char name[160];
    const size_t row2 = v1_row + 12U;
    rec_t *st = &g_st[t];
    const size_t bytes = np_mu_table_bytes(t);

    store_empty();
    newer_record(t, st->rec, in, 2U, (uint8_t)row2);
    st->len = bytes;
    st->exists = true;
    np_mu_init();
    snprintf(name, sizeof name, "version (%s): a version-2 record with a longer row is loaded, not refused", what);
    check(np_mu_load() == NP_HUB_OK, name);
    np_mu_row_t r;
    snprintf(name, sizeof name, "version (%s): every version-1 field of a newer row is read where version 1 puts it", what);
    check(stored_row(&in[0].key, &r) && r.sessions_present == 7U && r.mate_cycles == 3U && r.ch_on_s[1] == 40U
          && r.ch_sessions[1] == 4U, name);
    snprintf(name, sizeof name, "version (%s): the row length steps to the next row", what);
    check(stored_row(&in[1].key, &r) && r.sessions_used == 2U, name);

    np_mu_session_begin();
    np_mu_mark_present(&in[0].key);
    np_mu_mark_present(newcomer);
    (void)np_mu_session_end(1000U);
    snprintf(name, sizeof name, "version (%s): the rewrite keeps the newer version and its row length", what);
    check(st->rec[0] == 2U && st->rec[1] == row2 && st->rec[2] == 3U, name);
    const uint8_t *row0 = &st->rec[NP_MU_HDR_BYTES];
    bool tail_kept = true;
    for (size_t k = v1_row; k < row2; k++) { if (row0[k] != (uint8_t)(0xC0U + k)) { tail_kept = false; } }
    snprintf(name, sizeof name, "version (%s): a field this firmware does not know is carried through its rewrite", what);
    check(tail_kept, name);
    const uint8_t *rowc = &st->rec[NP_MU_HDR_BYTES + 2U * row2];
    bool tail_zero = true;
    for (size_t k = v1_row; k < row2; k++) { if (rowc[k] != 0U) { tail_zero = false; } }
    snprintf(name, sizeof name, "version (%s): a row this firmware creates starts its newer fields at zero", what);
    check(tail_zero && stored_row(newcomer, &r) && r.sessions_present == 1U, name);
    snprintf(name, sizeof name, "version (%s): and the totals it knows still run on", what);
    check(stored_row(&in[0].key, &r) && r.sessions_present == 8U, name);

    /* What is refused, whole, rather than misread. */
    static uint8_t bad[NP_MU_TABLE_MAX_BYTES];
    np_mu_row_t out[NP_MU_MAX_ROWS];
    size_t n = 0U;
    np_mu_encode(t, in, 2U, bad);
    snprintf(name, sizeof name, "version (%s): the record as encoded decodes", what);
    check(np_mu_decode(t, bad, out, NP_MU_MAX_ROWS, &n) && n == 2U, name);
    bad[0] = 0U;
    snprintf(name, sizeof name, "version (%s): version 0, which no firmware writes, is refused", what);
    check(!np_mu_decode(t, bad, out, NP_MU_MAX_ROWS, &n), name);
    np_mu_encode(t, in, 2U, bad);
    bad[1] = (uint8_t)(v1_row - 1U);
    snprintf(name, sizeof name, "version (%s): a row shorter than version 1's is refused", what);
    check(!np_mu_decode(t, bad, out, NP_MU_MAX_ROWS, &n), name);
    if (row_max < NP_MU_ROW_LEN_FIELD_MAX) {
        np_mu_encode(t, in, 2U, bad);
        bad[1] = (uint8_t)(row_max + 1U);
        snprintf(name, sizeof name, "version (%s): a row too long for the record is refused", what);
        check(!np_mu_decode(t, bad, out, NP_MU_MAX_ROWS, &n), name);
    }
    np_mu_encode(t, in, 2U, bad);
    const size_t cap = (t == NP_MU_TABLE_TILES) ? NP_MU_TILE_ROWS : NP_MU_PORT_ROWS;
    bad[2] = (uint8_t)(cap + 1U);
    snprintf(name, sizeof name, "version (%s): more rows than the table holds is refused", what);
    check(!np_mu_decode(t, bad, out, NP_MU_MAX_ROWS, &n), name);
    np_mu_encode(t, in, 2U, bad);
    bad[NP_MU_HDR_BYTES + v1_row] = 9U;          /* the second row's key kind */
    snprintf(name, sizeof name, "version (%s): a row with an unknown key kind is refused", what);
    check(!np_mu_decode(t, bad, out, NP_MU_MAX_ROWS, &n), name);
    np_mu_encode(t, in, 2U, bad);
    bad[NP_MU_HDR_BYTES + v1_row] = (t == NP_MU_TABLE_TILES) ? (uint8_t)NP_MU_KEY_PORT : (uint8_t)NP_MU_KEY_UID;
    snprintf(name, sizeof name, "version (%s): a row of the other table's kind is refused", what);
    check(!np_mu_decode(t, bad, out, NP_MU_MAX_ROWS, &n), name);

    /* A record of the wrong length is the store's refusal, and the table is then not loaded. */
    st->len = bytes - 1U;
    np_mu_init();
    (void)np_mu_load();
    snprintf(name, sizeof name, "version (%s): a record of another length is unreadable, never taken as empty", what);
    check(!np_mu_table_loaded(t), name);
}

/* @verifies REQ-FWHUB-93#h */
static void test_a_record_of_another_version_is_read_and_its_newer_fields_kept(void)
{
    np_mu_row_t tiles[2];
    memset(tiles, 0, sizeof tiles);
    tiles[0].key = np_mu_key_uid(UID_A);
    tiles[0].sessions_present = 7U; tiles[0].mate_cycles = 3U; tiles[0].ch_on_s[1] = 40U; tiles[0].ch_sessions[1] = 4U;
    tiles[1].key = np_mu_key_uid(UID_B);
    tiles[1].sessions_used = 2U;
    const np_mu_key_t new_tile = np_mu_key_uid((const uint8_t[NP_MU_ID_LEN]){ 0xC1, 2, 3, 4, 5, 6, 7, 8 });
    version_check(NP_MU_TABLE_TILES, tiles, &new_tile, NP_MU_TILE_ROW_BYTES, NP_MU_TILE_ROW_MAX_BYTES, "tiles");

    np_mu_row_t ports[2];
    memset(ports, 0, sizeof ports);
    ports[0].key = np_mu_key_port(NP_HUB_SLOT_INTRANASAL, NP_MOD_INTRANASAL);
    ports[0].sessions_present = 7U; ports[0].mate_cycles = 3U; ports[0].ch_on_s[1] = 40U; ports[0].ch_sessions[1] = 4U;
    ports[1].key = np_mu_key_port(NP_HUB_SLOT_CVNS, NP_MOD_CVNS);
    ports[1].sessions_used = 2U;
    const np_mu_key_t new_port = np_mu_key_port(NP_HUB_SLOT_AUDIO, NP_MOD_AUDIO);
    version_check(NP_MU_TABLE_PORTS, ports, &new_port, NP_MU_PORT_ROW_BYTES, NP_MU_PORT_ROW_MAX_BYTES, "ports");

    check(NP_MU_TILE_ROW_MAX_BYTES == 95U && NP_MU_PORT_ROW_MAX_BYTES == 255U && NP_MU_PORT_TABLE_BYTES == 4849U
          && np_mu_table_bytes(NP_MU_TABLE_TILES) == 12288U,
          "version: the tiles' rows may grow to 95 B in 12,288 B; the ports' to the header's 255 B in 4,849 B");
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
    const unsigned w = writes();
    check(np_mu_snapshot(rows, NP_MU_MAX_ROWS, &n) == NP_HUB_OK && n == 1U && writes() == w,
          "snapshot: reading it changes nothing on the helmet (no watermark, no row retired)");

    PORTS->read_rc = NP_HUB_ERR_STORE_IO;
    np_mu_init();
    (void)np_mu_load();
    n = 99U;
    check(np_mu_snapshot(rows, NP_MU_MAX_ROWS, &n) == NP_HUB_ERR_STORE_IO && n == 0U,
          "snapshot: a table that could not be read is reported unknown, not as a table with no rows");
    check(np_mu_snapshot(NULL, NP_MU_MAX_ROWS, &n) == NP_HUB_ERR_INVALID_ARG, "snapshot: no buffer is refused");
    PORTS->read_rc = NP_HUB_OK;
}

/* @verifies REQ-FWHUB-93#j */
static void test_each_emitter_channel_keeps_its_on_time_and_its_sessions(void)
{
    store_empty();
    reboot();
    const np_mu_key_t tile = np_mu_key_uid(UID_A);
    const np_mu_key_t nasal = np_mu_key_port(NP_HUB_SLOT_INTRANASAL, NP_MOD_INTRANASAL);

    /* A tile: the dose meter's time per wavelength. */
    np_mu_session_begin();
    np_mu_add_used(&tile, 60000U);
    np_mu_add_channel_used(&tile, 0U, 60000U);
    np_mu_add_channel_used(&tile, 1U, 20400U);
    np_mu_add_channel_used(&tile, 3U, 9000U);       /* a tile has no fourth channel: ignored */
    /* The probe: bilateral 660 + 808 from 0 s; at 10 s the right side drops 808; off at 30 s. */
    np_mu_slot_on(NP_HUB_SLOT_INTRANASAL, NP_MOD_INTRANASAL, 0x0FU, 0U);
    np_mu_slot_on(NP_HUB_SLOT_INTRANASAL, NP_MOD_INTRANASAL, 0x07U, 10000U);
    np_mu_slot_off(NP_HUB_SLOT_INTRANASAL, 30000U);
    (void)np_mu_session_end(60000U);

    np_mu_row_t r;
    check(stored_row(&tile, &r) && r.ch_on_s[0] == 60U && r.ch_sessions[0] == 1U
          && r.ch_on_s[1] == 20U && r.ch_sessions[1] == 1U,
          "channels: each lit wavelength of a tile adds its on-time and one session");
    check(r.ch_on_s[2] == 0U && r.ch_sessions[2] == 0U && r.ch_on_s[3] == 0U && r.ch_sessions[3] == 0U,
          "channels: a dark channel counts no session, and a channel past the tile's three is not kept");
    check(stored_row(&nasal, &r) && r.ch_on_s[0] == 30U && r.ch_on_s[1] == 30U && r.ch_on_s[2] == 30U
          && r.ch_on_s[3] == 10U && r.used_s == 30U,
          "channels: the probe's four channels each keep the time they were lit; a change of mask closes only the one that went dark");
    check(r.ch_sessions[0] == 1U && r.ch_sessions[1] == 1U && r.ch_sessions[2] == 1U && r.ch_sessions[3] == 1U,
          "channels: one session per channel driven in the session, not one per command or per pulse");

    /* The next session: the right 808 nm comes back for 5 s, left 660 nm stays dark. */
    np_mu_session_begin();
    np_mu_slot_on(NP_HUB_SLOT_INTRANASAL, NP_MOD_INTRANASAL, 0x08U, 0U);
    np_mu_slot_on(NP_HUB_SLOT_INTRANASAL, NP_MOD_INTRANASAL, 0x0AU, 2000U);
    (void)np_mu_session_end(5000U);              /* still on: closed at session end */
    check(stored_row(&nasal, &r) && r.ch_on_s[3] == 15U && r.ch_sessions[3] == 2U && r.ch_on_s[1] == 33U
          && r.ch_sessions[1] == 2U && r.ch_sessions[0] == 1U && r.ch_on_s[0] == 30U,
          "channels: totals run on across sessions; a channel not lit in a session gains neither time nor a session");
    check(stored_row(&tile, &r) && r.ch_sessions[0] == 1U, "channels: a tile absent from a session is not counted in it");

    /* A slot whose channel map is not known (the goggles, OI-FWHUB-35) counts its drive and no channel. */
    const np_mu_key_t gog = np_mu_key_port(NP_HUB_SLOT_VISUAL, NP_MOD_VISUAL);
    np_mu_session_begin();
    np_mu_slot_on(NP_HUB_SLOT_VISUAL, NP_MOD_VISUAL, 0U, 0U);
    np_mu_slot_off(NP_HUB_SLOT_VISUAL, 4000U);
    (void)np_mu_session_end(5000U);
    check(stored_row(&gog, &r) && r.used_s == 4U && r.ch_sessions[0] == 0U && r.ch_on_s[0] == 0U,
          "channels: a port with no channel map counts its drive and no channel");

    reboot();
    check(stored_row(&nasal, &r) && r.ch_on_s[3] == 15U, "channels: the totals survive a reboot");
}

/* @verifies-none harness check: the stored records' codec round-trips every field */
static void test_codec_round_trip(void)
{
    np_mu_row_t in[3];
    memset(in, 0, sizeof in);
    in[0].key = np_mu_key_uid(UID_A);
    in[0].sessions_present = 0x01020304U; in[0].present_s = 0xFFFFFFFFU;
    in[0].sessions_used = 7U; in[0].used_s = 9U;
    in[0].fault_code[2] = 0x7EU; in[0].fault_count[2] = 0x11223344U; in[0].faults_other = 5U;
    in[0].mate_cycles = 0xA1B2C3D4U; in[0].throttle_count = 0x0BADF00DU;
    in[0].fault_flags = 0x81U; in[0].detail_lost = 1U;
    for (size_t c = 0U; c < NP_MU_TILE_CH; c++) { in[0].ch_on_s[c] = 0x10000000U + (uint32_t)c; in[0].ch_sessions[c] = 0x200U + (uint32_t)c; }
    in[1].key = np_mu_key_port(3U, NP_MOD_AUDIO);
    in[2].key = np_mu_key_port(NP_HUB_SLOT_INTRANASAL, NP_MOD_INTRANASAL);
    for (size_t c = 0U; c < NP_MU_PORT_CH; c++) { in[2].ch_on_s[c] = 0x30000000U + (uint32_t)c; in[2].ch_sessions[c] = 0x400U + (uint32_t)c; }

    static uint8_t buf[NP_MU_TABLE_MAX_BYTES];
    np_mu_row_t out[3];
    size_t n = 0U;
    np_mu_encode(NP_MU_TABLE_TILES, in, 3U, buf);
    check(buf[0] == NP_MU_VERSION && buf[1] == NP_MU_TILE_ROW_BYTES && buf[2] == 1U,
          "codec: a version-1 tiles' record of 83-byte rows, holding only the tile");
    check(np_mu_decode(NP_MU_TABLE_TILES, buf, out, 3U, &n) && n == 1U && memcmp(&in[0], &out[0], sizeof in[0]) == 0,
          "codec: every field of a tile row survives encode and decode");
    np_mu_encode(NP_MU_TABLE_PORTS, in, 3U, buf);
    check(buf[0] == NP_MU_VERSION && buf[1] == NP_MU_PORT_ROW_BYTES && buf[2] == 2U,
          "codec: a version-1 ports' record of 91-byte rows, holding only the ports");
    check(np_mu_decode(NP_MU_TABLE_PORTS, buf, out, 3U, &n) && n == 2U && memcmp(&in[1], &out[0], sizeof in[1]) == 0
          && memcmp(&in[2], &out[1], sizeof in[2]) == 0,
          "codec: every field of a port row, all four channels, survives encode and decode");
    check(np_mu_table_bytes(NP_MU_TABLE_COUNT) == 0U && !np_mu_decode(NP_MU_TABLE_COUNT, buf, out, 3U, &n),
          "codec: a table that does not exist has no length and decodes nothing");
}

int main(void)
{
    test_each_module_has_one_row_keyed_by_its_uid_or_its_port();
    test_present_and_used_are_counted_with_their_time();
    test_the_table_is_upserted_once_per_session_never_appended();
    test_an_unreadable_table_is_never_overwritten();
    test_faults_are_counted_against_the_accessory_port();
    test_a_full_table_gives_up_the_least_present_module_not_one_in_use();
    test_mates_throttles_and_the_unseated_flag_are_running_totals();
    test_a_row_known_to_fall_short_says_so();
    test_a_record_of_another_version_is_read_and_its_newer_fields_kept();
    test_the_snapshot_reports_unknown_never_an_empty_table();
    test_each_emitter_channel_keeps_its_on_time_and_its_sessions();
    test_codec_round_trip();
    printf("\n%s (%d failures)\n", g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", g_failures);
    return g_failures == 0 ? 0 : 1;
}
