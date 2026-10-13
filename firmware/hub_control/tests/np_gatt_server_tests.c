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
#include "../include/np_hub_config.h"
#include "../include/np_cvns_fault_summary.h"
#include "../include/np_cfg_store.h"
#include "../include/np_session_log.h"
#include "../include/np_session_progress.h"
#include "../include/np_safety_spi.h"
#include "../include/np_session_stop.h"
#include "../include/np_upload_check.h"
#include "np_crypto.h"

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

/* Device serial (OI-AND-WIRE-02): the same seam np_protocol.c compares against. */
static uint8_t         g_serial[NP_GATT_DEVICE_SERIAL_LEN];
static np_hub_status_t g_serial_rc;

np_hub_status_t np_proto_hal_get_device_serial(uint8_t *buf, size_t len)
{
    if (g_serial_rc != NP_HUB_OK) {
        return g_serial_rc;
    }
    memcpy(buf, g_serial, len < sizeof g_serial ? len : sizeof g_serial);
    return NP_HUB_OK;
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

static uint8_t g_hw_version = 1U;
uint8_t np_safety_spi_get_hardware_version(void) { return g_hw_version; }

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
    g_serial_rc = NP_HUB_OK;
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
    np_upload_check_init();
    np_session_stop_init(NULL, NULL);
    np_session_pause_init(NULL, NULL);
    np_progress_set_source(NULL);
}

/* ── Helpers ──────────────────────────────────────────────────────────────── */

/* The canonical string form of a 16-byte LSB-first UUID. */
static void uuid_string(const uint8_t u[16], char out[37])
{
    static const char HEX_DIGITS[] = "0123456789ABCDEF";
    size_t o = 0U;
    for (int i = 15; i >= 0; i--) {
        out[o++] = HEX_DIGITS[u[i] >> 4];
        out[o++] = HEX_DIGITS[u[i] & 0x0FU];
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

/* @verifies REQ-FWHUB-56 */
static void test_uuids_match_the_contract(void)
{
    /* The expected strings, written out independently of the generator (common/npps/constants.json), which the apps'
     * UUID strings are built from too: a drift in the base or an id fails here. */
    static const struct { uint16_t id; const char *s; } k[] = {
        { NP_GATT_SERVICE_ID,               "4E455550-0001-1000-8000-00805F9B34FB" },
        { NP_GATT_CONSUMABLE_STATUS_ID,     "4E455550-0007-1000-8000-00805F9B34FB" },
        { NP_GATT_WARRANTY_TOKEN_ID,        "4E455550-0010-1000-8000-00805F9B34FB" },
        { NP_GATT_CVNS_FAULT_STATUS_ID,     "4E455550-0014-1000-8000-00805F9B34FB" },
        { NP_GATT_CVNS_REENABLE_CONFIRM_ID, "4E455550-0015-1000-8000-00805F9B34FB" },
        { NP_GATT_ACTIVE_USER_ID,           "4E455550-0016-1000-8000-00805F9B34FB" },
        { NP_GATT_DEVICE_SERIAL_ID,         "4E455550-0017-1000-8000-00805F9B34FB" },
        { NP_GATT_HARDWARE_VERSION_ID,      "4E455550-001C-1000-8000-00805F9B34FB" },
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

/* @verifies REQ-FWHUB-56 */
static void test_table_is_what_the_hub_produces(void)
{
    size_t n = 0U;
    const np_gatt_char_t *t = np_gatt_table(&n);
    check(n == 12U, "table: twelve characteristics, each with a producer");

    bool coherent = true;
    bool pending_absent = true;
    for (size_t i = 0U; i < n; i++) {
        bool r = (t[i].props & NP_GATT_PROP_READ) != 0U;
        bool w = (t[i].props & NP_GATT_PROP_WRITE) != 0U;
        bool nt = (t[i].props & NP_GATT_PROP_NOTIFY) != 0U;
        if (r != (t[i].read != NULL) || w != (t[i].write != NULL) ||
            (r || nt) != (t[i].max_len != 0U) || w != (t[i].write_len != 0U) ||
            t[i].max_len > NP_GATT_VALUE_MAX || t[i].write_len > NP_GATT_VALUE_MAX) {
            coherent = false;
        }
        /* OI-ACC-07: no producer yet, so not published. */
        if (t[i].id == 0x0013U) {
            pending_absent = false;
        }
    }
    check(coherent, "table: a handler exists iff its property does; lengths in range");
    check(pending_absent, "table: 0x0013 is not published without a producer");

    reset_world();
    check(np_gatt_init() == NP_HUB_OK && g_registers == 1 &&
          g_reg_table == t && g_reg_count == n,
          "init: registers exactly the table with the stack");
}

/* ── Access control and lengths ───────────────────────────────────────────── */

/* @verifies REQ-FWHUB-56 */
static void test_access_rules(void)
{
    uint8_t buf[64];
    size_t  len = 0U;
    const uint8_t one = 0x01U;
    reset_world();

    check(np_gatt_on_read(0x0013U, 0U, buf, sizeof buf, &len) == NP_ATT_ATTRIBUTE_NOT_FOUND,
          "access: an unpublished id is not found on read");
    check(np_gatt_on_write(0x0008U, &one, 1U) == NP_ATT_ATTRIBUTE_NOT_FOUND,
          "access: an unpublished id is not found on write");
    check(np_gatt_on_write(NP_GATT_WARRANTY_TOKEN_ID, buf, 32U) == NP_ATT_WRITE_NOT_PERMITTED,
          "access: the warranty token cannot be written");
    check(np_gatt_on_write(NP_GATT_CVNS_FAULT_STATUS_ID, buf, 4U) == NP_ATT_WRITE_NOT_PERMITTED,
          "access: the fault summary cannot be written");
    check(np_gatt_on_read(NP_GATT_ACTIVE_USER_ID, 0U, buf, sizeof buf, &len) == NP_ATT_READ_NOT_PERMITTED,
          "access: the active user cannot be read back");
    check(np_gatt_on_read(NP_GATT_CVNS_REENABLE_CONFIRM_ID, 0U, buf, sizeof buf, &len) == NP_ATT_READ_NOT_PERMITTED,
          "access: the re-enable confirm cannot be read");
    check(np_gatt_notify(NP_GATT_WARRANTY_TOKEN_ID, buf, 32U) == NP_HUB_ERR_INVALID_ARG &&
          g_notifies == 0,
          "notify: refused on a characteristic without NOTIFY");
    check(np_gatt_notify(NP_GATT_CVNS_FAULT_STATUS_ID, buf, NP_CVFS_FRAME_MAX + 1U) == NP_HUB_ERR_INVALID_ARG &&
          g_notifies == 0,
          "notify: an over-length value is refused, never sent truncated");
}

/* ── Warranty token ───────────────────────────────────────────────────────── */

/* @verifies REQ-FWHUB-56 */
static void test_warranty_first_read_provisions_once(void)
{
    uint8_t a[64];
    uint8_t b[64];
    size_t  la = 0U;
    size_t  lb = 0U;
    reset_world();
    g_session_count = 0x01020304U;

    check(np_gatt_on_read(NP_GATT_WARRANTY_TOKEN_ID, 0U, a, sizeof a, &la) == NP_ATT_OK &&
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

    check(np_gatt_on_read(NP_GATT_WARRANTY_TOKEN_ID, 0U, b, sizeof b, &lb) == NP_ATT_OK &&
          lb == la && memcmp(a, b, la) == 0 && g_trng_draws == 1 && g_cfg_writes == 1,
          "warranty: a second read serves the same token, no new draw or write");

    /* A reboot: RAM forgotten, the store kept.  A new TRNG value on offer
     * proves the token comes from the store, not a fresh draw. */
    np_warranty_token_reset_cache();
    g_trng_fill = 0x77U;
    check(np_gatt_on_read(NP_GATT_WARRANTY_TOKEN_ID, 0U, b, sizeof b, &lb) == NP_ATT_OK &&
          memcmp(a, b, NP_WARRANTY_TOKEN_LEN) == 0 && g_trng_draws == 1,
          "warranty: after a reboot the stored token is served, not regenerated");
}

/* @verifies REQ-FWHUB-56 */
static void test_warranty_long_read(void)
{
    uint8_t whole[64];
    uint8_t part[64];
    size_t  lw = 0U;
    size_t  lp = 0U;
    reset_world();
    (void)np_gatt_on_read(NP_GATT_WARRANTY_TOKEN_ID, 0U, whole, sizeof whole, &lw);

    /* Default ATT MTU 23: a READ carries 22 bytes, the READ BLOB the rest. */
    check(np_gatt_on_read(NP_GATT_WARRANTY_TOKEN_ID, 0U, part, 22U, &lp) == NP_ATT_OK &&
          lp == 22U && memcmp(part, whole, 22U) == 0,
          "long read: the first 22 bytes at MTU 23");
    check(np_gatt_on_read(NP_GATT_WARRANTY_TOKEN_ID, 22U, part, 22U, &lp) == NP_ATT_OK &&
          lp == 10U && memcmp(part, whole + 22, 10U) == 0,
          "long read: the blob read returns the remaining 10");
    check(np_gatt_on_read(NP_GATT_WARRANTY_TOKEN_ID, 32U, part, 22U, &lp) == NP_ATT_OK && lp == 0U,
          "long read: offset == length is an empty tail, not an error");
    check(np_gatt_on_read(NP_GATT_WARRANTY_TOKEN_ID, 33U, part, 22U, &lp) == NP_ATT_INVALID_OFFSET,
          "long read: offset past the value is refused");
}

/* @verifies REQ-FWHUB-56 */
static void test_warranty_never_regenerates_over_an_unreadable_token(void)
{
    uint8_t buf[64];
    size_t  len = 0U;
    const np_hub_status_t unknown[] = { NP_HUB_ERR_STORE_IO, NP_HUB_ERR_STORE_INTEGRITY };

    for (size_t i = 0U; i < sizeof(unknown) / sizeof(unknown[0]); i++) {
        reset_world();
        g_cfg_read_rc = unknown[i];
        check(np_gatt_on_read(NP_GATT_WARRANTY_TOKEN_ID, 0U, buf, sizeof buf, &len)
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
    check(np_gatt_on_read(NP_GATT_WARRANTY_TOKEN_ID, 0U, buf, sizeof buf, &len)
              == NP_ATT_APP_UNAVAILABLE && g_trng_draws == 0 && g_cfg_writes == 0,
          "warranty: a record with reserved bytes set is refused, not replaced");

    reset_world();
    memset(g_cfg_rec, 0x00U, sizeof g_cfg_rec);
    g_cfg_written = true;
    check(np_gatt_on_read(NP_GATT_WARRANTY_TOKEN_ID, 0U, buf, sizeof buf, &len)
              == NP_ATT_APP_UNAVAILABLE && g_trng_draws == 0 && g_cfg_writes == 0,
          "warranty: a stored all-zero token is refused, not replaced");
}

/* @verifies REQ-FWHUB-56 */
static void test_warranty_provisioning_failures_leave_nothing(void)
{
    uint8_t buf[64];
    size_t  len = 0U;

    reset_world();
    g_trng_rc = NP_HUB_ERR_GENERIC;
    check(np_gatt_on_read(NP_GATT_WARRANTY_TOKEN_ID, 0U, buf, sizeof buf, &len)
              == NP_ATT_APP_UNAVAILABLE && g_cfg_writes == 0,
          "warranty: a failed TRNG draw writes nothing");

    reset_world();
    g_trng_stuck = true;
    check(np_gatt_on_read(NP_GATT_WARRANTY_TOKEN_ID, 0U, buf, sizeof buf, &len)
              == NP_ATT_APP_UNAVAILABLE && g_cfg_writes == 0,
          "warranty: a stuck TRNG (every byte equal) writes nothing");

    reset_world();
    g_cfg_write_rc = NP_HUB_ERR_STORE_IO;
    check(np_gatt_on_read(NP_GATT_WARRANTY_TOKEN_ID, 0U, buf, sizeof buf, &len)
              == NP_ATT_APP_UNAVAILABLE,
          "warranty: a token that did not persist is not served");
    g_cfg_write_rc = NP_HUB_OK;
    g_trng_fill = 0x22U;
    check(np_gatt_on_read(NP_GATT_WARRANTY_TOKEN_ID, 0U, buf, sizeof buf, &len) == NP_ATT_OK &&
          g_trng_draws == 2 && memcmp(buf, g_cfg_rec, NP_WARRANTY_TOKEN_LEN) == 0,
          "warranty: the next read provisions afresh and serves the persisted token");
}

/* ── Cervical fault summary characteristics ───────────────────────────────── */

/* ── Device serial (OI-AND-WIRE-02) ───────────────────────────────────────── */

/* @verifies REQ-FWHUB-56 */
static void test_device_serial(void)
{
    uint8_t buf[64];
    size_t  len = 0U;
    reset_world();
    for (size_t i = 0U; i < sizeof g_serial; i++) {
        g_serial[i] = (uint8_t)(0x40U + i);
    }

    check(np_gatt_on_read(NP_GATT_DEVICE_SERIAL_ID, 0U, buf, sizeof buf, &len) == NP_ATT_OK &&
          len == NP_GATT_DEVICE_SERIAL_LEN && memcmp(buf, g_serial, len) == 0,
          "serial: the read serves the bytes the protocol check compares with");

    size_t n = 0U;
    const np_gatt_char_t *t = np_gatt_table(&n);
    bool enc = false;
    for (size_t i = 0U; i < n; i++) {
        if (t[i].id == NP_GATT_DEVICE_SERIAL_ID) {
            enc = (t[i].props & NP_GATT_PROP_ENC) != 0U &&
                  (t[i].props & (NP_GATT_PROP_WRITE | NP_GATT_PROP_NOTIFY)) == 0U;
        }
    }
    check(enc, "serial: read-only, and the stack must require an encrypted link");

    check(np_gatt_on_write(NP_GATT_DEVICE_SERIAL_ID, buf, 32U) == NP_ATT_WRITE_NOT_PERMITTED,
          "serial: cannot be written");

    g_serial_rc = NP_HUB_ERR_INVALID_ARG;
    memset(buf, 0xAA, sizeof buf);
    check(np_gatt_on_read(NP_GATT_DEVICE_SERIAL_ID, 0U, buf, sizeof buf, &len) == NP_ATT_APP_UNAVAILABLE,
          "serial: an unreadable serial is unavailable, never a zero serial");
    g_serial_rc = NP_HUB_OK;
}

/* @verifies SWR-HV-06 */
static void test_hardware_version_is_read_from_the_safety_mcu_and_cannot_be_written(void)
{
    uint8_t buf[8];
    size_t  len = 0U;
    reset_world();

    for (uint8_t v = 1U; v <= 15U; v++) {
        g_hw_version = v;
        check(np_gatt_on_read(NP_GATT_HARDWARE_VERSION_ID, 0U, buf, sizeof buf, &len) == NP_ATT_OK &&
              len == NP_GATT_HARDWARE_VERSION_LEN && buf[0] == v,
              "hardware version: the read serves the safety MCU's value, one byte");
    }
    g_hw_version = 1U;

    check(np_gatt_on_write(NP_GATT_HARDWARE_VERSION_ID, buf, 1U) == NP_ATT_WRITE_NOT_PERMITTED,
          "hardware version: nothing in the field writes it");

    size_t n = 0U;
    const np_gatt_char_t *t = np_gatt_table(&n);
    bool read_only = false;
    for (size_t i = 0U; i < n; i++) {
        if (t[i].id == NP_GATT_HARDWARE_VERSION_ID) {
            read_only = (t[i].props & NP_GATT_PROP_READ) != 0U &&
                        (t[i].props & (NP_GATT_PROP_WRITE | NP_GATT_PROP_NOTIFY)) == 0U;
        }
    }
    check(read_only, "hardware version: read-only in the table");
}

/* @verifies REQ-FWHUB-56 */
static void test_active_user_write(void)
{
    const uint8_t tag[4] = { 0x04U, 0x03U, 0x02U, 0x01U };
    const uint8_t zero[4] = { 0U, 0U, 0U, 0U };
    reset_world();

    check(np_gatt_on_write(NP_GATT_ACTIVE_USER_ID, tag, 4U) == NP_ATT_OK &&
          g_posted_users == 1 && g_posted_user == 0x01020304U,
          "active user: a 4-byte LE tag is posted to the hub");
    check(np_gatt_on_write(NP_GATT_ACTIVE_USER_ID, tag, 3U) == NP_ATT_INVALID_VALUE_LENGTH &&
          np_gatt_on_write(NP_GATT_ACTIVE_USER_ID, tag, 5U) == NP_ATT_INVALID_VALUE_LENGTH &&
          g_posted_users == 1,
          "active user: any other length is refused before the handler");
    check(np_gatt_on_write(NP_GATT_ACTIVE_USER_ID, zero, 4U) == NP_ATT_APP_REFUSED &&
          g_posted_users == 1,
          "active user: the reserved tag 0 is refused as an application error");
}

/* @verifies REQ-CVNS-09#f */
static void test_reenable_confirm_write(void)
{
    const uint8_t yes = 0x01U;
    const uint8_t two = 0x02U;
    reset_world();

    check(np_gatt_on_write(NP_GATT_CVNS_REENABLE_CONFIRM_ID, &yes, 1U) == NP_ATT_OK &&
          g_confirms == 1,
          "reenable: 0x01 is forwarded to the re-enable manager");
    g_confirm_rc = NP_HUB_ERR_NO_SESSION;
    check(np_gatt_on_write(NP_GATT_CVNS_REENABLE_CONFIRM_ID, &yes, 1U) == NP_ATT_APP_REFUSED &&
          g_confirms == 2,
          "reenable: a confirm the hub is not awaiting fails the app's write");
    check(np_gatt_on_write(NP_GATT_CVNS_REENABLE_CONFIRM_ID, &two, 1U) == NP_ATT_APP_REFUSED &&
          g_confirms == 2,
          "reenable: any value but 0x01 never reaches the manager");
}

/* @verifies REQ-FWHUB-56 */
static void test_fault_status_read_and_notify_agree(void)
{
    uint8_t rd[64];
    size_t  len = 0U;
    reset_world();

    np_cvfs_set_user(0x01020304U);
    (void)np_cvfs_record_fault(43U, NP_CVNS_FAULT_IMPEDANCE, NP_CVFS_SIDE_RIGHT);
    np_cvfs_poll(2U, true, 0x01U);

    check(g_notifies == 1 && g_ntf_id == NP_GATT_CVNS_FAULT_STATUS_ID &&
          g_ntf_len == NP_CVFS_HEADER_LEN + NP_CVFS_RECORD_LEN,
          "fault status: a change is notified on 0x0014, whole");
    check(np_gatt_on_read(NP_GATT_CVNS_FAULT_STATUS_ID, 0U, rd, sizeof rd, &len) == NP_ATT_OK &&
          len == g_ntf_len && memcmp(rd, g_ntf, len) == 0,
          "fault status: a READ returns the frame last notified");
    check(rd[0] == NP_CVFS_WIRE_VERSION && rd[1] == 2U && rd[2] == 1U && rd[3] == 0x01U &&
          rd[4] == 43U && rd[8] == (uint8_t)NP_CVNS_FAULT_IMPEDANCE && rd[9] == NP_CVFS_SIDE_RIGHT,
          "fault status: the frame is the one the apps parse");

    np_cvfs_poll(2U, true, 0x01U);
    check(g_notifies == 1, "fault status: an unchanged frame is not re-notified");
}


/* ── Upload check (SWR-UP-01) ─────────────────────────────────────────────── */

static void upload_frame(uint8_t out[NP_UPLOAD_CHECK_LEN])
{
    size_t len = 0U;
    check(np_gatt_on_read(NP_GATT_UPLOAD_CHECK_ID, 0U, out, NP_UPLOAD_CHECK_LEN, &len) == NP_ATT_OK &&
          len == NP_UPLOAD_CHECK_LEN, "upload check: the read returns the 34-byte frame");
}

/* @verifies SWR-UP-01 */
static void test_upload_check_hash_and_verdict(void)
{
    static const uint8_t ABC[3] = { 'a', 'b', 'c' };
    /* SHA-256("abc"), FIPS 180-4. */
    static const uint8_t ABC_SHA[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad };
    uint8_t f[NP_UPLOAD_CHECK_LEN];
    reset_world();

    upload_frame(f);
    check(f[0] == NP_UPLOAD_NONE && f[1] == 0U && all_zero(&f[2], 32U),
          "upload check: before any upload the status is NONE and the hash is zero");

    np_upload_check_received(ABC, sizeof ABC);
    upload_frame(f);
    check(f[0] == NP_UPLOAD_PENDING && f[1] == 0U && memcmp(&f[2], ABC_SHA, 32U) == 0,
          "upload check: received bytes are hashed at once (SHA-256) and the verdict is PENDING");
    check(g_notifies == 0, "upload check: nothing is notified before there is a verdict");

    np_upload_check_result(NP_HUB_OK);
    upload_frame(f);
    check(f[0] == NP_UPLOAD_ACCEPTED && f[1] == 0U && memcmp(&f[2], ABC_SHA, 32U) == 0,
          "upload check: an accepted load reports ACCEPTED with the hash of what was received");
    check(g_notifies == 1 && g_ntf_id == NP_GATT_UPLOAD_CHECK_ID && g_ntf_len == NP_UPLOAD_CHECK_LEN &&
          memcmp(g_ntf, f, NP_UPLOAD_CHECK_LEN) == 0,
          "upload check: the verdict is notified, and is the frame a read returns");
}

/* @verifies SWR-UP-01 */
static void test_upload_check_refusal_and_corruption(void)
{
    static const uint8_t GOOD[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    static const uint8_t ALTERED[8] = { 1, 2, 3, 4, 5, 6, 7, 9 };
    uint8_t good_hash[NP_SHA256_SIZE];
    uint8_t f[NP_UPLOAD_CHECK_LEN];
    reset_world();

    np_sha256(GOOD, sizeof GOOD, good_hash);

    np_upload_check_received(ALTERED, sizeof ALTERED);
    np_upload_check_result(NP_HUB_ERR_BAD_SIGNATURE);
    upload_frame(f);
    check(f[0] == NP_UPLOAD_REFUSED && f[1] == 6U,
          "upload check: a refusal reports REFUSED and the code of the refusal (bad signature = 6)");
    check(memcmp(&f[2], good_hash, 32U) != 0,
          "upload check: an altered upload hashes differently from the bytes the app sent");

    np_upload_check_received(GOOD, 5U);   /* truncated */
    np_upload_check_result(NP_HUB_ERR_BAD_MAGIC);
    upload_frame(f);
    check(f[0] == NP_UPLOAD_REFUSED && memcmp(&f[2], good_hash, 32U) != 0,
          "upload check: a truncated upload hashes differently from the bytes the app sent");

    /* The hash is of the bytes as they arrived, whatever happens to them next. */
    np_upload_check_received(GOOD, sizeof GOOD);
    np_upload_check_result(NP_HUB_ERR_SESSION_ACTIVE);
    upload_frame(f);
    check(f[0] == NP_UPLOAD_REFUSED && f[1] == 13U && memcmp(&f[2], good_hash, 32U) == 0,
          "upload check: a refusal after a clean upload still carries the hash of what arrived");
}

/* @verifies SWR-UP-01 */
static void test_upload_check_without_a_hash_changes_nothing(void)
{
    static const uint8_t BYTES[4] = { 9, 9, 9, 9 };
    uint8_t before[NP_UPLOAD_CHECK_LEN];
    uint8_t after[NP_UPLOAD_CHECK_LEN];
    reset_world();

    np_upload_check_result(NP_HUB_OK);   /* a verdict with no hash to attach it to */
    upload_frame(after);
    check(after[0] == NP_UPLOAD_NONE && g_notifies == 0,
          "upload check: a verdict with no received bytes is ignored, not reported as an accept");

    np_upload_check_received(BYTES, sizeof BYTES);
    np_upload_check_result(NP_HUB_OK);
    upload_frame(before);
    np_upload_check_received(NULL, 4U);
    np_upload_check_received(BYTES, 0U);
    upload_frame(after);
    check(memcmp(before, after, NP_UPLOAD_CHECK_LEN) == 0,
          "upload check: an empty or missing buffer leaves the last report as it was");

    check(np_gatt_on_write(NP_GATT_UPLOAD_CHECK_ID, BYTES, 1U) == NP_ATT_WRITE_NOT_PERMITTED,
          "upload check: the report cannot be written by the app");
}

/* ── Session stop (SWR-SS-06) ─────────────────────────────────────────────── */

static np_session_state_t g_stop_state;
static int                g_aborts;
static np_abort_reason_t  g_abort_reason;

static np_session_state_t stop_state_fn(void) { return g_stop_state; }
static void stop_abort_fn(np_abort_reason_t reason) { g_aborts++; g_abort_reason = reason; }

/* @verifies SWR-SS-06 */
static void test_stop_write_ends_a_running_or_paused_session(void)
{
    const uint8_t stop = NP_SESSION_STOP_VALUE;
    reset_world();
    np_session_stop_init(stop_state_fn, stop_abort_fn);
    g_aborts = 0;

    g_stop_state = NP_SESSION_RUNNING;
    check(np_gatt_on_write(NP_GATT_SESSION_STOP_ID, &stop, 1U) == NP_ATT_OK &&
          g_aborts == 1 && g_abort_reason == NP_ABORT_USER,
          "stop: a stop request over the link aborts a running session with reason USER");

    g_stop_state = NP_SESSION_PAUSED;
    check(np_gatt_on_write(NP_GATT_SESSION_STOP_ID, &stop, 1U) == NP_ATT_OK && g_aborts == 2,
          "stop: a paused session can be stopped too");
}

/* @verifies SWR-SS-06 */
static void test_stop_write_is_refused_when_it_would_not_take_effect(void)
{
    const uint8_t stop = NP_SESSION_STOP_VALUE;
    const uint8_t not_stop = 0x00U;
    const uint8_t two[2] = { 0x01U, 0x01U };
    static const np_session_state_t NOT_RUNNING[] = {
        NP_SESSION_IDLE, NP_SESSION_LOADING, NP_SESSION_VERIFYING, NP_SESSION_STOPPING,
        NP_SESSION_COMPLETE, NP_SESSION_FAULT };
    reset_world();
    np_session_stop_init(stop_state_fn, stop_abort_fn);
    g_aborts = 0;

    bool all_refused = true;
    for (size_t i = 0U; i < sizeof(NOT_RUNNING) / sizeof(NOT_RUNNING[0]); i++) {
        g_stop_state = NOT_RUNNING[i];
        if (np_gatt_on_write(NP_GATT_SESSION_STOP_ID, &stop, 1U) != NP_ATT_APP_REFUSED) {
            all_refused = false;
        }
    }
    check(all_refused && g_aborts == 0,
          "stop: outside a running or paused session the write is refused, so the app is not told it worked");

    g_stop_state = NP_SESSION_RUNNING;
    check(np_gatt_on_write(NP_GATT_SESSION_STOP_ID, &not_stop, 1U) == NP_ATT_APP_REFUSED && g_aborts == 0,
          "stop: only 0x01 is a stop");
    check(np_gatt_on_write(NP_GATT_SESSION_STOP_ID, two, 2U) == NP_ATT_INVALID_VALUE_LENGTH && g_aborts == 0,
          "stop: the write is exactly one byte");
    check(np_gatt_on_read(NP_GATT_SESSION_STOP_ID, 0U, (uint8_t[4]){0}, 4U, &(size_t){0}) == NP_ATT_READ_NOT_PERMITTED,
          "stop: the characteristic cannot be read");

    np_session_stop_init(NULL, NULL);
    check(np_gatt_on_write(NP_GATT_SESSION_STOP_ID, &stop, 1U) == NP_ATT_APP_REFUSED,
          "stop: with no session runner bound the write is refused, never ignored");
}

/* ── Session progress (SWR-SS-07) ─────────────────────────────────────────── */

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static np_session_cmd_t make_cmd(np_hub_mod_type_t t, uint32_t start, uint32_t dur)
{
    np_session_cmd_t c;
    memset(&c, 0, sizeof c);
    c.mod_type = t;
    c.start_ms = start;
    c.duration_ms = dur;
    return c;
}

/* @verifies SWR-SS-07 */
static void test_progress_frame_elapsed_remaining_and_active_part(void)
{
    np_session_cmd_t cmds[3];
    uint8_t f[NP_PROGRESS_LEN];
    np_progress_in_t in;
    memset(&in, 0, sizeof in);

    cmds[0] = make_cmd(NP_MOD_PBM_BASE, 0U, 300000U);
    cmds[1] = make_cmd(NP_MOD_AUDIO, 60000U, 60000U);
    cmds[2] = make_cmd(NP_MOD_EEG, 400000U, 100000U);
    in.state = NP_SESSION_RUNNING;
    in.elapsed_ms = 90000U;
    in.total_ms = 600000U;
    in.cmds = cmds;
    in.cmd_count = 3U;

    np_progress_build(&in, f);
    check(f[0] == NP_SESSION_RUNNING && rd32(&f[4]) == 90000U && rd32(&f[8]) == 510000U &&
          rd32(&f[12]) == 600000U,
          "progress: elapsed, remaining and total are the helmet's figures");
    check(f[1] == 1U && (f[2] | (f[3] << 8)) == ((1 << NP_MOD_PBM_BASE) | (1 << NP_MOD_AUDIO)),
          "progress: the active part is the highest running command, and the mask has each running modality");

    in.elapsed_ms = 130000U;   /* the audio command has ended; the PBM command is still running */
    np_progress_build(&in, f);
    check(f[1] == 0U && (f[2] | (f[3] << 8)) == (1 << NP_MOD_PBM_BASE),
          "progress: when a command ends the active part and mask follow it");

    in.elapsed_ms = 350000U;   /* between commands */
    np_progress_build(&in, f);
    check(f[1] == NP_PROGRESS_NO_PART && f[2] == 0U && f[3] == 0U,
          "progress: with nothing running there is no active part");

    in.elapsed_ms = 700000U;   /* past the end */
    np_progress_build(&in, f);
    check(rd32(&f[4]) == 600000U && rd32(&f[8]) == 0U,
          "progress: elapsed never exceeds the total and remaining stops at zero");
}

/* @verifies SWR-SS-07 */
static void test_progress_states_and_unbounded_commands(void)
{
    np_session_cmd_t cmds[1];
    uint8_t f[NP_PROGRESS_LEN];
    np_progress_in_t in;
    memset(&in, 0, sizeof in);

    cmds[0] = make_cmd(NP_MOD_EEG, 0U, 0U);   /* duration 0: until the session ends */
    in.state = NP_SESSION_PAUSED;
    in.elapsed_ms = 1000U;
    in.total_ms = 5000U;
    in.cmds = cmds;
    in.cmd_count = 1U;
    np_progress_build(&in, f);
    check(f[0] == NP_SESSION_PAUSED && rd32(&f[4]) == 1000U && f[1] == 0U,
          "progress: a paused session reports the elapsed time it was given, unchanged, and its part");

    in.state = NP_SESSION_COMPLETE;
    np_progress_build(&in, f);
    check(f[0] == NP_SESSION_COMPLETE && f[1] == NP_PROGRESS_NO_PART && f[2] == 0U,
          "progress: a finished session has no active part");

    in.state = NP_SESSION_IDLE;
    in.total_ms = 0U;
    in.elapsed_ms = 0U;
    in.cmds = NULL;
    in.cmd_count = 0U;
    np_progress_build(&in, f);
    check(f[0] == NP_SESSION_IDLE && rd32(&f[4]) == 0U && rd32(&f[8]) == 0U && rd32(&f[12]) == 0U &&
          f[1] == NP_PROGRESS_NO_PART, "progress: an idle hub reports zeros and no part");
}


/* @verifies SWR-SS-07 */
static void test_progress_frame_reports_the_ramp(void)
{
    uint8_t f[NP_PROGRESS_LEN];
    np_progress_in_t in;
    memset(&in, 0, sizeof in);
    in.state = NP_SESSION_PAUSED;
    in.total_ms = 5000U;
    np_progress_build(&in, f);
    check(f[16] == NP_RAMP_HELD, "progress: paused with nothing ramping is HELD");

    in.ramp.phase = NP_RAMP_DOWN; in.ramp.pct = 33U; in.ramp.remaining_ms = 20000U;
    np_progress_build(&in, f);
    check(f[16] == NP_RAMP_DOWN && f[17] == 33U && rd32(&f[18]) == 20000U, "progress: a ramp-down reports its phase, percent and time left");

    in.state = NP_SESSION_RUNNING;
    in.ramp.phase = NP_RAMP_PRECHECK; in.ramp.pct = 0U; in.ramp.remaining_ms = NP_RAMP_UNKNOWN_MS;
    np_progress_build(&in, f);
    check(f[16] == NP_RAMP_PRECHECK && rd32(&f[18]) == NP_RAMP_UNKNOWN_MS, "progress: the cervical VNS checks report an unknown time");

    in.ramp.phase = NP_RAMP_NONE;
    np_progress_build(&in, f);
    check(f[16] == NP_RAMP_NONE && f[17] == 0U, "progress: a running session with no ramp reports none");
}

static void fake_progress_source(uint8_t out[NP_PROGRESS_LEN])
{
    memset(out, 0, NP_PROGRESS_LEN);
    out[0] = NP_SESSION_RUNNING;
    out[1] = 2U;
    out[4] = 0x10U;
}

/* @verifies SWR-SS-07 */
static void test_progress_is_read_and_notified_through_the_table(void)
{
    uint8_t buf[NP_PROGRESS_LEN];
    size_t  len = 0U;
    reset_world();

    check(np_gatt_on_read(NP_GATT_SESSION_PROGRESS_ID, 0U, buf, sizeof buf, &len) == NP_ATT_APP_UNAVAILABLE,
          "progress: with no runner bound the read is unavailable, not a made-up zero");

    np_progress_set_source(fake_progress_source);
    check(np_gatt_on_read(NP_GATT_SESSION_PROGRESS_ID, 0U, buf, sizeof buf, &len) == NP_ATT_OK &&
          len == NP_PROGRESS_LEN && buf[0] == NP_SESSION_RUNNING && buf[1] == 2U && buf[4] == 0x10U,
          "progress: the read returns the runner's frame");
    np_progress_notify();
    check(g_notifies == 1 && g_ntf_id == NP_GATT_SESSION_PROGRESS_ID && g_ntf_len == NP_PROGRESS_LEN &&
          memcmp(g_ntf, buf, NP_PROGRESS_LEN) == 0,
          "progress: the notification is the same frame");
    uint8_t w = 1U;
    check(np_gatt_on_write(NP_GATT_SESSION_PROGRESS_ID, &w, 1U) == NP_ATT_WRITE_NOT_PERMITTED,
          "progress: the app cannot write it");
}

/* ── Session pause and resume (SWR-SS-05) ─────────────────────────────────── */

static int              g_pause_calls;
static int              g_resume_calls;
static np_hub_status_t  g_pause_rc;
static np_hub_status_t  g_resume_rc;

static np_hub_status_t pause_fn(void)  { g_pause_calls++;  return g_pause_rc; }
static np_hub_status_t resume_fn(void) { g_resume_calls++; return g_resume_rc; }

/* @verifies SWR-SS-05 */
static void test_pause_write_forwards_pause_and_resume_to_the_runner(void)
{
    const uint8_t pause = NP_SESSION_PAUSE_VALUE;
    const uint8_t resume = NP_SESSION_RESUME_VALUE;
    reset_world();
    g_pause_calls = g_resume_calls = 0;
    g_pause_rc = g_resume_rc = NP_HUB_OK;
    np_session_pause_init(pause_fn, resume_fn);

    check(np_gatt_on_write(NP_GATT_SESSION_PAUSE_ID, &pause, 1U) == NP_ATT_OK &&
          g_pause_calls == 1 && g_resume_calls == 0,
          "pause: 0x01 asks the runner to pause");
    check(np_gatt_on_write(NP_GATT_SESSION_PAUSE_ID, &resume, 1U) == NP_ATT_OK &&
          g_pause_calls == 1 && g_resume_calls == 1,
          "pause: 0x02 asks the runner to resume");
}

/* @verifies SWR-SS-05 */
static void test_pause_write_is_refused_when_the_runner_refuses_or_the_value_is_wrong(void)
{
    const uint8_t pause = NP_SESSION_PAUSE_VALUE;
    const uint8_t resume = NP_SESSION_RESUME_VALUE;
    const uint8_t other = 0x03U;
    reset_world();
    g_pause_calls = g_resume_calls = 0;
    g_pause_rc = g_resume_rc = NP_HUB_ERR_NO_SESSION;   /* the runner is not in the state the request needs */
    np_session_pause_init(pause_fn, resume_fn);

    check(np_gatt_on_write(NP_GATT_SESSION_PAUSE_ID, &pause, 1U) == NP_ATT_APP_REFUSED,
          "pause: a pause the runner refuses is refused to the app, so it is not told it worked");
    check(np_gatt_on_write(NP_GATT_SESSION_PAUSE_ID, &resume, 1U) == NP_ATT_APP_REFUSED,
          "pause: so is a resume");
    g_pause_calls = g_resume_calls = 0;
    check(np_gatt_on_write(NP_GATT_SESSION_PAUSE_ID, &other, 1U) == NP_ATT_APP_REFUSED &&
          g_pause_calls == 0 && g_resume_calls == 0,
          "pause: any value but 0x01 and 0x02 is refused without reaching the runner");

    np_session_pause_init(NULL, NULL);
    check(np_gatt_on_write(NP_GATT_SESSION_PAUSE_ID, &pause, 1U) == NP_ATT_APP_REFUSED,
          "pause: with no runner bound the write is refused, never ignored");
    check(np_gatt_on_read(NP_GATT_SESSION_PAUSE_ID, 0U, (uint8_t[4]){0}, 4U, &(size_t){0}) == NP_ATT_READ_NOT_PERMITTED,
          "pause: the characteristic cannot be read");
}

/* @verifies SWR-SS-05 */
static void test_pause_refused_while_ramping_says_so(void)
{
    const uint8_t pause = NP_SESSION_PAUSE_VALUE;
    const uint8_t resume = NP_SESSION_RESUME_VALUE;
    reset_world();
    g_pause_calls = g_resume_calls = 0;
    g_pause_rc = g_resume_rc = NP_HUB_ERR_RAMPING;
    np_session_pause_init(pause_fn, resume_fn);
    check(np_gatt_on_write(NP_GATT_SESSION_PAUSE_ID, &pause, 1U) == NP_ATT_APP_RAMPING,
          "pause: refused because the protocol is ramping up is a distinct error (0x82), so the app can say why");
    check(np_gatt_on_write(NP_GATT_SESSION_PAUSE_ID, &resume, 1U) == NP_ATT_APP_RAMPING,
          "pause: so is a resume while one is already ramping up");
}

int main(void)
{
    test_uuids_match_the_contract();
    test_table_is_what_the_hub_produces();
    test_access_rules();
    test_warranty_first_read_provisions_once();
    test_warranty_long_read();
    test_warranty_never_regenerates_over_an_unreadable_token();
    test_warranty_provisioning_failures_leave_nothing();
    test_device_serial();
    test_hardware_version_is_read_from_the_safety_mcu_and_cannot_be_written();
    test_active_user_write();
    test_reenable_confirm_write();
    test_fault_status_read_and_notify_agree();
    test_upload_check_hash_and_verdict();
    test_upload_check_refusal_and_corruption();
    test_upload_check_without_a_hash_changes_nothing();
    test_stop_write_ends_a_running_or_paused_session();
    test_stop_write_is_refused_when_it_would_not_take_effect();
    test_progress_frame_elapsed_remaining_and_active_part();
    test_progress_states_and_unbounded_commands();
    test_progress_frame_reports_the_ramp();
    test_progress_is_read_and_notified_through_the_table();
    test_pause_write_forwards_pause_and_resume_to_the_runner();
    test_pause_write_is_refused_when_the_runner_refuses_or_the_value_is_wrong();
    test_pause_refused_while_ramping_says_so();

    if (g_failures != 0) {
        printf("\n%d FAILURE(S)\n", g_failures);
        return 1;
    }
    printf("\nAll np_gatt_server tests passed.\n");
    return 0;
}
