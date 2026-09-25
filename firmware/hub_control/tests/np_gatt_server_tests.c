/*
 * Host tests — np_gatt_server + np_warranty_token (OI-WA-03, GitHub #381)
 *
 * Links the real server, the real warranty-token module and the real cervical
 * fault summary.  Models the platform seams (BLE stack registration and
 * notify, TRNG, the UHDR blob store), the Config store's replicated-record API
 * (whose own behaviour np_cfg_store_tests covers against real littlefs), the
 * session count, and the two hub entry points the cervical writes call.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../include/np_gatt_server.h"
#include "../include/np_warranty_token.h"
#include "../include/np_cvns_fault_summary.h"
#include "../include/np_cfg_store.h"
#include "../include/np_session_log.h"

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

/* BLE stack */
static const np_gatt_char_t *g_reg_table;
static size_t   g_reg_count;
static int      g_registers;
static uint16_t g_ntf_id;
static uint8_t  g_ntf[64];
static size_t   g_ntf_len;
static int      g_notifies;

np_hub_status_t np_gatt_hal_register(const np_gatt_char_t *table, size_t count)
{
    g_reg_table = table;
    g_reg_count = count;
    g_registers++;
    return NP_HUB_OK;
}

void np_gatt_hal_notify(uint16_t id, const uint8_t *data, size_t len)
{
    g_ntf_id = id;
    memcpy(g_ntf, data, len);
    g_ntf_len = len;
    g_notifies++;
}

/* TRNG */
static uint8_t         g_trng_fill;     /* first byte of the next draw */
static bool            g_trng_stuck;    /* every byte the same          */
static np_hub_status_t g_trng_rc;
static int             g_trng_draws;

np_hub_status_t np_warranty_hal_trng_generate(uint8_t *buf, size_t len)
{
    g_trng_draws++;
    for (size_t i = 0U; i < len; i++) {
        buf[i] = g_trng_stuck ? 0x00U : (uint8_t)(g_trng_fill + (i * 37U));
    }
    return g_trng_rc;
}

/* Config store: one replicated record, the warranty token.  `g_cfg_read_rc`
 * forces a read outcome; otherwise the record is OK if written, else absent. */
static uint8_t         g_cfg_rec[NP_WARRANTY_RECORD_LEN];
static bool            g_cfg_written;
static np_hub_status_t g_cfg_read_rc;
static np_hub_status_t g_cfg_write_rc;
static int             g_cfg_writes;

np_hub_status_t np_cfg_store_replicated_read(np_cfg_file_t file,
                                             uint8_t *payload, size_t len)
{
    if (file != NP_CFG_FILE_WARRANTY_TOKEN || len != NP_WARRANTY_RECORD_LEN) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    if (g_cfg_read_rc != NP_HUB_OK) {
        return g_cfg_read_rc;
    }
    if (!g_cfg_written) {
        return NP_HUB_ERR_NOT_PRESENT;
    }
    memcpy(payload, g_cfg_rec, len);
    return NP_HUB_OK;
}

np_hub_status_t np_cfg_store_replicated_write(np_cfg_file_t file,
                                              const uint8_t *payload, size_t len)
{
    g_cfg_writes++;
    if (file != NP_CFG_FILE_WARRANTY_TOKEN || len != NP_WARRANTY_RECORD_LEN) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    if (g_cfg_write_rc != NP_HUB_OK) {
        return g_cfg_write_rc;
    }
    memcpy(g_cfg_rec, payload, len);
    g_cfg_written = true;
    return NP_HUB_OK;
}

static uint32_t g_session_count;
uint32_t np_log_session_count(void) { return g_session_count; }

/* Cervical fault summary: UHDR blob store (empty) and the hub entry points. */
np_hub_status_t np_cvfs_hal_load(uint8_t *buf, size_t cap, size_t *len_out)
{
    (void)buf;
    (void)cap;
    *len_out = 0U;
    return NP_HUB_OK;
}

np_hub_status_t np_cvfs_hal_save(const uint8_t *buf, size_t len)
{
    (void)buf;
    (void)len;
    return NP_HUB_OK;
}

static uint32_t        g_posted_user;
static int             g_posted_users;
static np_hub_status_t g_confirm_rc;
static int             g_confirms;

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
    g_reg_table = NULL;
    g_reg_count = 0U;
    g_registers = 0;
    g_ntf_id = 0U;
    g_ntf_len = 0U;
    g_notifies = 0;
    g_trng_fill = 0x11U;
    g_trng_stuck = false;
    g_trng_rc = NP_HUB_OK;
    g_trng_draws = 0;
    memset(g_cfg_rec, 0, sizeof(g_cfg_rec));
    g_cfg_written = false;
    g_cfg_read_rc = NP_HUB_OK;
    g_cfg_write_rc = NP_HUB_OK;
    g_cfg_writes = 0;
    g_session_count = 0U;
    g_posted_user = 0U;
    g_posted_users = 0;
    g_confirm_rc = NP_HUB_OK;
    g_confirms = 0;
    np_warranty_token_reset_cache();
    np_cvfs_init();
}

/* ── Helpers ──────────────────────────────────────────────────────────────── */

/* The canonical string form of a 16-byte LSB-first UUID. */
static void uuid_string(const uint8_t u[16], char out[37])
{
    static const char hex[] = "0123456789ABCDEF";
    size_t o = 0U;
    for (int i = 15; i >= 0; i--) {
        out[o++] = hex[u[i] >> 4];
        out[o++] = hex[u[i] & 0x0FU];
        if (i == 12 || i == 10 || i == 8 || i == 6) {
            out[o++] = '-';
        }
    }
    out[o] = '\0';
}

static bool all_zero(const uint8_t *p, size_t n)
{
    for (size_t i = 0U; i < n; i++) {
        if (p[i] != 0U) {
            return false;
        }
    }
    return true;
}

/* ── UUIDs and the table ──────────────────────────────────────────────────── */

static void test_uuids_match_the_apps(void)
{
    /* Copied from app/ios/NeurOne/BLE/GATTCharacteristics.swift (NPUUID) and
     * app/android/core/.../ble/GattUuids.kt, which must stay byte-identical
     * to each other and to this. */
    static const struct { uint16_t id; const char *s; } k[] = {
        { NP_GATT_ID_SERVICE,               "4E455550-0001-1000-8000-00805F9B34FB" },
        { NP_GATT_ID_WARRANTY_TOKEN,        "4E455550-0010-1000-8000-00805F9B34FB" },
        { NP_GATT_ID_CVNS_FAULT_STATUS,     "4E455550-0014-1000-8000-00805F9B34FB" },
        { NP_GATT_ID_CVNS_REENABLE_CONFIRM, "4E455550-0015-1000-8000-00805F9B34FB" },
        { NP_GATT_ID_ACTIVE_USER,           "4E455550-0016-1000-8000-00805F9B34FB" },
    };
    bool ok = true;
    for (size_t i = 0U; i < sizeof(k) / sizeof(k[0]); i++) {
        uint8_t u[16];
        char    s[37];
        np_gatt_uuid128(k[i].id, u);
        uuid_string(u, s);
        if (strcmp(s, k[i].s) != 0) {
            printf("  0x%04X built %s, the apps use %s\n", k[i].id, s, k[i].s);
            ok = false;
        }
    }
    check(ok, "uuid: every published UUID is the string both apps carry");
}

static void test_table_is_what_the_hub_produces(void)
{
    size_t n = 0U;
    const np_gatt_char_t *t = np_gatt_table(&n);
    check(n == 4U, "table: four characteristics, each with a producer");

    bool coherent = true;
    bool pending_absent = true;
    for (size_t i = 0U; i < n; i++) {
        bool r = (t[i].props & NP_GATT_PROP_READ) != 0U;
        bool w = (t[i].props & NP_GATT_PROP_WRITE) != 0U;
        if (r != (t[i].read != NULL) || w != (t[i].write != NULL) ||
            t[i].max_len == 0U || t[i].max_len > NP_GATT_VALUE_MAX) {
            coherent = false;
        }
        /* OI-ACC-08 / OI-ACC-07: no producer yet, so not published. */
        if (t[i].id == 0x0007U || t[i].id == 0x0013U) {
            pending_absent = false;
        }
    }
    check(coherent, "table: a handler exists iff its property does; lengths in range");
    check(pending_absent, "table: 0x0007 and 0x0013 are not published without a producer");

    reset_world();
    check(np_gatt_init() == NP_HUB_OK && g_registers == 1 &&
          g_reg_table == t && g_reg_count == n,
          "init: registers exactly the table with the stack");
}

/* ── Access control and lengths ───────────────────────────────────────────── */

static void test_access_rules(void)
{
    uint8_t buf[64];
    size_t  len = 0U;
    const uint8_t one = 0x01U;
    reset_world();

    check(np_gatt_on_read(0x0007U, 0U, buf, sizeof buf, &len) == NP_ATT_ATTRIBUTE_NOT_FOUND,
          "access: an unpublished id is not found on read");
    check(np_gatt_on_write(0x000FU, &one, 1U) == NP_ATT_ATTRIBUTE_NOT_FOUND,
          "access: an unpublished id is not found on write");
    check(np_gatt_on_write(NP_GATT_ID_WARRANTY_TOKEN, buf, 32U) == NP_ATT_WRITE_NOT_PERMITTED,
          "access: the warranty token cannot be written");
    check(np_gatt_on_write(NP_GATT_ID_CVNS_FAULT_STATUS, buf, 4U) == NP_ATT_WRITE_NOT_PERMITTED,
          "access: the fault summary cannot be written");
    check(np_gatt_on_read(NP_GATT_ID_ACTIVE_USER, 0U, buf, sizeof buf, &len) == NP_ATT_READ_NOT_PERMITTED,
          "access: the active user cannot be read back");
    check(np_gatt_on_read(NP_GATT_ID_CVNS_REENABLE_CONFIRM, 0U, buf, sizeof buf, &len) == NP_ATT_READ_NOT_PERMITTED,
          "access: the re-enable confirm cannot be read");
    check(np_gatt_notify(NP_GATT_ID_WARRANTY_TOKEN, buf, 32U) == NP_HUB_ERR_INVALID_ARG &&
          g_notifies == 0,
          "notify: refused on a characteristic without NOTIFY");
    check(np_gatt_notify(NP_GATT_ID_CVNS_FAULT_STATUS, buf, NP_CVFS_FRAME_MAX + 1U) == NP_HUB_ERR_INVALID_ARG &&
          g_notifies == 0,
          "notify: an over-length value is refused, never sent truncated");
}

/* ── Warranty token ───────────────────────────────────────────────────────── */

static void test_warranty_first_read_provisions_once(void)
{
    uint8_t a[64];
    uint8_t b[64];
    size_t  la = 0U;
    size_t  lb = 0U;
    reset_world();
    g_session_count = 0x01020304U;

    check(np_gatt_on_read(NP_GATT_ID_WARRANTY_TOKEN, 0U, a, sizeof a, &la) == NP_ATT_OK &&
          la == NP_WARRANTY_TOKEN_LEN,
          "warranty: the first read returns 32 bytes");
    check(g_trng_draws == 1 && g_cfg_writes == 1 && g_cfg_written,
          "warranty: the first read draws the TRNG once and persists the record");
    check(memcmp(a, g_cfg_rec, NP_WARRANTY_TOKEN_LEN) == 0,
          "warranty: what is served is what was persisted");
    check(g_cfg_rec[32] == 0x04U && g_cfg_rec[33] == 0x03U &&
          g_cfg_rec[34] == 0x02U && g_cfg_rec[35] == 0x01U &&
          all_zero(&g_cfg_rec[NP_WARRANTY_OFF_RESERVED],
                   NP_WARRANTY_RECORD_LEN - NP_WARRANTY_OFF_RESERVED),
          "warranty: record = token, session count LE, zeroed reserved (§A.2)");

    check(np_gatt_on_read(NP_GATT_ID_WARRANTY_TOKEN, 0U, b, sizeof b, &lb) == NP_ATT_OK &&
          lb == la && memcmp(a, b, la) == 0 && g_trng_draws == 1 && g_cfg_writes == 1,
          "warranty: a second read serves the same token, no new draw or write");

    /* A reboot: RAM forgotten, the store kept.  A new TRNG value on offer
     * proves the token comes from the store, not a fresh draw. */
    np_warranty_token_reset_cache();
    g_trng_fill = 0x77U;
    check(np_gatt_on_read(NP_GATT_ID_WARRANTY_TOKEN, 0U, b, sizeof b, &lb) == NP_ATT_OK &&
          memcmp(a, b, NP_WARRANTY_TOKEN_LEN) == 0 && g_trng_draws == 1,
          "warranty: after a reboot the stored token is served, not regenerated");
}

static void test_warranty_long_read(void)
{
    uint8_t whole[64];
    uint8_t part[64];
    size_t  lw = 0U;
    size_t  lp = 0U;
    reset_world();
    (void)np_gatt_on_read(NP_GATT_ID_WARRANTY_TOKEN, 0U, whole, sizeof whole, &lw);

    /* Default ATT MTU 23: a READ carries 22 bytes, the READ BLOB the rest. */
    check(np_gatt_on_read(NP_GATT_ID_WARRANTY_TOKEN, 0U, part, 22U, &lp) == NP_ATT_OK &&
          lp == 22U && memcmp(part, whole, 22U) == 0,
          "long read: the first 22 bytes at MTU 23");
    check(np_gatt_on_read(NP_GATT_ID_WARRANTY_TOKEN, 22U, part, 22U, &lp) == NP_ATT_OK &&
          lp == 10U && memcmp(part, whole + 22, 10U) == 0,
          "long read: the blob read returns the remaining 10");
    check(np_gatt_on_read(NP_GATT_ID_WARRANTY_TOKEN, 32U, part, 22U, &lp) == NP_ATT_OK && lp == 0U,
          "long read: offset == length is an empty tail, not an error");
    check(np_gatt_on_read(NP_GATT_ID_WARRANTY_TOKEN, 33U, part, 22U, &lp) == NP_ATT_INVALID_OFFSET,
          "long read: offset past the value is refused");
}

static void test_warranty_never_regenerates_over_an_unreadable_token(void)
{
    uint8_t buf[64];
    size_t  len = 0U;
    const np_hub_status_t unknown[] = { NP_HUB_ERR_STORE_IO, NP_HUB_ERR_STORE_INTEGRITY };

    for (size_t i = 0U; i < sizeof(unknown) / sizeof(unknown[0]); i++) {
        reset_world();
        g_cfg_read_rc = unknown[i];
        check(np_gatt_on_read(NP_GATT_ID_WARRANTY_TOKEN, 0U, buf, sizeof buf, &len)
                  == NP_ATT_APP_UNAVAILABLE &&
              g_trng_draws == 0 && g_cfg_writes == 0,
              i == 0U ? "warranty: a store I/O error serves nothing and writes nothing"
                      : "warranty: a refused record serves nothing and writes nothing");
    }

    /* Present and CRC-valid, but not a token this module wrote. */
    reset_world();
    memset(g_cfg_rec, 0x5AU, NP_WARRANTY_TOKEN_LEN);
    g_cfg_rec[1] = 0x5BU;
    g_cfg_rec[NP_WARRANTY_RECORD_LEN - 1U] = 0x01U;   /* reserved byte set */
    g_cfg_written = true;
    check(np_gatt_on_read(NP_GATT_ID_WARRANTY_TOKEN, 0U, buf, sizeof buf, &len)
              == NP_ATT_APP_UNAVAILABLE && g_trng_draws == 0 && g_cfg_writes == 0,
          "warranty: a record with reserved bytes set is refused, not replaced");

    reset_world();
    memset(g_cfg_rec, 0x00U, sizeof g_cfg_rec);
    g_cfg_written = true;
    check(np_gatt_on_read(NP_GATT_ID_WARRANTY_TOKEN, 0U, buf, sizeof buf, &len)
              == NP_ATT_APP_UNAVAILABLE && g_trng_draws == 0 && g_cfg_writes == 0,
          "warranty: a stored all-zero token is refused, not replaced");
}

static void test_warranty_provisioning_failures_leave_nothing(void)
{
    uint8_t buf[64];
    size_t  len = 0U;

    reset_world();
    g_trng_rc = NP_HUB_ERR_GENERIC;
    check(np_gatt_on_read(NP_GATT_ID_WARRANTY_TOKEN, 0U, buf, sizeof buf, &len)
              == NP_ATT_APP_UNAVAILABLE && g_cfg_writes == 0,
          "warranty: a failed TRNG draw writes nothing");

    reset_world();
    g_trng_stuck = true;
    check(np_gatt_on_read(NP_GATT_ID_WARRANTY_TOKEN, 0U, buf, sizeof buf, &len)
              == NP_ATT_APP_UNAVAILABLE && g_cfg_writes == 0,
          "warranty: a stuck TRNG (every byte equal) writes nothing");

    reset_world();
    g_cfg_write_rc = NP_HUB_ERR_STORE_IO;
    check(np_gatt_on_read(NP_GATT_ID_WARRANTY_TOKEN, 0U, buf, sizeof buf, &len)
              == NP_ATT_APP_UNAVAILABLE,
          "warranty: a token that did not persist is not served");
    g_cfg_write_rc = NP_HUB_OK;
    g_trng_fill = 0x22U;
    check(np_gatt_on_read(NP_GATT_ID_WARRANTY_TOKEN, 0U, buf, sizeof buf, &len) == NP_ATT_OK &&
          g_trng_draws == 2 && memcmp(buf, g_cfg_rec, NP_WARRANTY_TOKEN_LEN) == 0,
          "warranty: the next read provisions afresh and serves the persisted token");
}

/* ── Cervical fault summary characteristics ───────────────────────────────── */

static void test_active_user_write(void)
{
    const uint8_t tag[4] = { 0x04U, 0x03U, 0x02U, 0x01U };
    const uint8_t zero[4] = { 0U, 0U, 0U, 0U };
    reset_world();

    check(np_gatt_on_write(NP_GATT_ID_ACTIVE_USER, tag, 4U) == NP_ATT_OK &&
          g_posted_users == 1 && g_posted_user == 0x01020304U,
          "active user: a 4-byte LE tag is posted to the hub");
    check(np_gatt_on_write(NP_GATT_ID_ACTIVE_USER, tag, 3U) == NP_ATT_INVALID_VALUE_LENGTH &&
          np_gatt_on_write(NP_GATT_ID_ACTIVE_USER, tag, 5U) == NP_ATT_INVALID_VALUE_LENGTH &&
          g_posted_users == 1,
          "active user: any other length is refused before the handler");
    check(np_gatt_on_write(NP_GATT_ID_ACTIVE_USER, zero, 4U) == NP_ATT_APP_REFUSED &&
          g_posted_users == 1,
          "active user: the reserved tag 0 is refused as an application error");
}

static void test_reenable_confirm_write(void)
{
    const uint8_t yes = 0x01U;
    const uint8_t two = 0x02U;
    reset_world();

    check(np_gatt_on_write(NP_GATT_ID_CVNS_REENABLE_CONFIRM, &yes, 1U) == NP_ATT_OK &&
          g_confirms == 1,
          "reenable: 0x01 is forwarded to the re-enable manager");
    g_confirm_rc = NP_HUB_ERR_NO_SESSION;
    check(np_gatt_on_write(NP_GATT_ID_CVNS_REENABLE_CONFIRM, &yes, 1U) == NP_ATT_APP_REFUSED &&
          g_confirms == 2,
          "reenable: a confirm the hub is not awaiting fails the app's write");
    check(np_gatt_on_write(NP_GATT_ID_CVNS_REENABLE_CONFIRM, &two, 1U) == NP_ATT_APP_REFUSED &&
          g_confirms == 2,
          "reenable: any value but 0x01 never reaches the manager");
}

static void test_fault_status_read_and_notify_agree(void)
{
    uint8_t rd[64];
    size_t  len = 0U;
    reset_world();

    np_cvfs_set_user(0x01020304U);
    (void)np_cvfs_record_fault(43U, NP_CVNS_FAULT_IMPEDANCE, NP_CVFS_SIDE_RIGHT);
    np_cvfs_poll(2U, true, 0x01U);

    check(g_notifies == 1 && g_ntf_id == NP_GATT_ID_CVNS_FAULT_STATUS &&
          g_ntf_len == NP_CVFS_HEADER_LEN + NP_CVFS_RECORD_LEN,
          "fault status: a change is notified on 0x0014, whole");
    check(np_gatt_on_read(NP_GATT_ID_CVNS_FAULT_STATUS, 0U, rd, sizeof rd, &len) == NP_ATT_OK &&
          len == g_ntf_len && memcmp(rd, g_ntf, len) == 0,
          "fault status: a READ returns the frame last notified");
    check(rd[0] == NP_CVFS_WIRE_VERSION && rd[1] == 2U && rd[2] == 1U && rd[3] == 0x01U &&
          rd[4] == 43U && rd[8] == (uint8_t)NP_CVNS_FAULT_IMPEDANCE && rd[9] == NP_CVFS_SIDE_RIGHT,
          "fault status: the frame is the one the apps parse");

    np_cvfs_poll(2U, true, 0x01U);
    check(g_notifies == 1, "fault status: an unchanged frame is not re-notified");
}

int main(void)
{
    test_uuids_match_the_apps();
    test_table_is_what_the_hub_produces();
    test_access_rules();
    test_warranty_first_read_provisions_once();
    test_warranty_long_read();
    test_warranty_never_regenerates_over_an_unreadable_token();
    test_warranty_provisioning_failures_leave_nothing();
    test_active_user_write();
    test_reenable_confirm_write();
    test_fault_status_read_and_notify_agree();

    if (g_failures != 0) {
        printf("\n%d FAILURE(S)\n", g_failures);
        return 1;
    }
    printf("\nAll np_gatt_server tests passed.\n");
    return 0;
}
