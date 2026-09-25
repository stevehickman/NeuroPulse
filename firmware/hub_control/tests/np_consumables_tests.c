/*
 * Host tests — np_consumables, the CONSUMABLE_STATUS producer (OI-ACC-08, #381)
 *
 * Links the real module and the real GATT server, so every count is read the
 * way the app reads it: through characteristic 0x0007.  Models the Config
 * store's replicated record (np_cfg_store_tests covers the store itself
 * against real littlefs), the BLE stack, and the seams the rest of the server
 * links against.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../include/np_consumables.h"
#include "../include/np_gatt_server.h"
#include "../include/np_cfg_store.h"
#include "../include/np_cvns_fault_summary.h"
#include "../include/np_warranty_token.h"
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

static uint8_t  g_ntf[16];
static size_t   g_ntf_len;
static int      g_notifies;

np_hub_status_t np_gatt_hal_register(const np_gatt_char_t *table, size_t count)
{
    (void)table;
    (void)count;
    return NP_HUB_OK;
}

void np_gatt_hal_notify(uint16_t id, const uint8_t *data, size_t len)
{
    if (id == NP_GATT_ID_CONSUMABLE_STATUS) {
        memcpy(g_ntf, data, len);
        g_ntf_len = len;
        g_notifies++;
    }
}

/* The replicated record. */
static uint8_t         g_rec[NP_CONS_RECORD_LEN];
static bool            g_rec_present;
static np_hub_status_t g_read_rc;
static np_hub_status_t g_write_rc;
static int             g_writes;
static void          (*g_during_write)(void);   /* runs inside the next write */

np_hub_status_t np_cfg_store_replicated_read(np_cfg_file_t file,
                                             uint8_t *payload, size_t len)
{
    if (file != NP_CFG_FILE_CONSUMABLES || len != NP_CONS_RECORD_LEN) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    if (g_read_rc != NP_HUB_OK) {
        return g_read_rc;
    }
    if (!g_rec_present) {
        return NP_HUB_ERR_NOT_PRESENT;
    }
    memcpy(payload, g_rec, len);
    return NP_HUB_OK;
}

np_hub_status_t np_cfg_store_replicated_write(np_cfg_file_t file,
                                              const uint8_t *payload, size_t len)
{
    g_writes++;
    if (g_during_write != NULL) {
        void (*f)(void) = g_during_write;
        g_during_write = NULL;
        f();
    }
    if (file != NP_CFG_FILE_CONSUMABLES || len != NP_CONS_RECORD_LEN) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    if (g_write_rc != NP_HUB_OK) {
        return g_write_rc;
    }
    memcpy(g_rec, payload, len);
    g_rec_present = true;
    return NP_HUB_OK;
}

/* Linked by the rest of the server; unused here. */
np_hub_status_t np_warranty_hal_trng_generate(uint8_t *buf, size_t len)
{
    (void)buf;
    (void)len;
    return NP_HUB_ERR_GENERIC;
}
uint32_t np_log_session_count(void) { return 0U; }
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
np_hub_status_t np_hub_set_active_user(uint32_t user_tag)
{
    (void)user_tag;
    return NP_HUB_OK;
}
np_hub_status_t np_hub_cvns_reenable_confirm(void) { return NP_HUB_OK; }

/* ── Helpers ──────────────────────────────────────────────────────────────── */

#define BIT(m) (1UL << (uint8_t)(m))

static void boot(void)
{
    g_ntf_len = 0U;
    g_notifies = 0;
    g_writes = 0;
    g_during_write = NULL;
    np_cons_test_reset();
    np_cons_init();
}

static void new_device(void)
{
    memset(g_rec, 0, sizeof g_rec);
    g_rec_present = false;
    g_read_rc = NP_HUB_OK;
    g_write_rc = NP_HUB_OK;
    boot();
}

static void store_counts(uint16_t a, uint16_t b, uint16_t c, uint16_t d)
{
    const uint16_t v[4] = { a, b, c, d };
    g_rec[0] = NP_CONS_RECORD_VERSION;
    for (unsigned k = 0U; k < 4U; k++) {
        g_rec[1U + 2U * k] = (uint8_t)(v[k] & 0xFFU);
        g_rec[2U + 2U * k] = (uint8_t)(v[k] >> 8);
    }
    g_rec_present = true;
}

/* Read 0x0007 through the server as the app does; false if the READ failed. */
static bool read_counts(uint16_t out[4])
{
    uint8_t buf[16];
    size_t  len = 0U;
    if (np_gatt_on_read(NP_GATT_ID_CONSUMABLE_STATUS, 0U, buf, sizeof buf, &len) != NP_ATT_OK ||
        len != NP_CONS_WIRE_LEN) {
        return false;
    }
    for (unsigned k = 0U; k < 4U; k++) {
        out[k] = (uint16_t)buf[2U * k] | (uint16_t)((uint16_t)buf[2U * k + 1U] << 8);
    }
    return true;
}

static bool counts_are(uint16_t a, uint16_t b, uint16_t c, uint16_t d)
{
    uint16_t v[4];
    return read_counts(v) && v[0] == a && v[1] == b && v[2] == c && v[3] == d;
}

static bool stored_are(uint16_t a, uint16_t b, uint16_t c, uint16_t d)
{
    const uint16_t v[4] = { a, b, c, d };
    if (!g_rec_present || g_rec[0] != NP_CONS_RECORD_VERSION) {
        return false;
    }
    for (unsigned k = 0U; k < 4U; k++) {
        uint16_t s = (uint16_t)g_rec[1U + 2U * k] | (uint16_t)((uint16_t)g_rec[2U + 2U * k] << 8);
        if (s != v[k]) {
            return false;
        }
    }
    return true;
}

static np_att_status_t reset_kind(uint8_t k)
{
    return np_gatt_on_write(NP_GATT_ID_CONSUMABLE_STATUS, &k, 1U);
}

/* ── Tests ────────────────────────────────────────────────────────────────── */

static void test_new_device_reads_zero(void)
{
    new_device();
    check(counts_are(0U, 0U, 0U, 0U), "new device: an absent record reads as four zeros");
    check(g_writes == 0, "new device: loading writes nothing");
}

static void test_increment_rule(void)
{
    new_device();

    np_cons_on_session_end(BIT(NP_MOD_INTRANASAL) | BIT(NP_MOD_EEG));
    check(counts_are(1U, 1U, 0U, 0U), "increment: intranasal → sleeves, EEG → hydrogel tips");
    check(g_writes == 1 && stored_are(1U, 1U, 0U, 0U),
          "increment: persisted at session end, not left for the heartbeat");

    np_cons_on_session_end(BIT(NP_MOD_VNS_HRV) | BIT(NP_MOD_AUDIO));
    check(counts_are(1U, 1U, 1U, 1U), "increment: auricular VNS → clip pads, audio → cup foam");

    np_cons_on_session_end(BIT(NP_MOD_PBM_BASE) | BIT(NP_MOD_TDCS) | BIT(NP_MOD_BES_TACS) |
                           BIT(NP_MOD_CVNS) | BIT(NP_MOD_QEEG_21CH) | BIT(NP_MOD_VISUAL));
    check(counts_are(1U, 1U, 1U, 1U) && g_writes == 2,
          "increment: a session that drove none of the four changes nothing and writes nothing");

    np_cons_on_session_end(0UL);
    check(counts_are(1U, 1U, 1U, 1U) && g_writes == 2,
          "increment: a session that drove nothing (refused or empty) counts for nothing");
}

static void test_counts_survive_a_reboot(void)
{
    new_device();
    np_cons_on_session_end(BIT(NP_MOD_EEG));
    np_cons_on_session_end(BIT(NP_MOD_EEG) | BIT(NP_MOD_AUDIO));
    boot();
    check(counts_are(0U, 2U, 0U, 1U), "reboot: the hub's counts come back from Config");
}

static void test_replacement_reset(void)
{
    new_device();
    store_counts(3U, 40U, 7U, 150U);
    boot();
    np_cons_poll();
    check(g_notifies == 1, "reset: the loaded counts are notified once");

    check(reset_kind(1U) == NP_ATT_OK && counts_are(3U, 0U, 7U, 150U),
          "reset: writing kind 1 zeroes the hydrogel count, and only it");
    check(stored_are(3U, 40U, 7U, 150U), "reset: not yet persisted before the poll");
    np_cons_poll();
    check(stored_are(3U, 0U, 7U, 150U), "reset: the next poll persists it");
    check(g_notifies == 2 && g_ntf_len == 8U && g_ntf[2] == 0U && g_ntf[3] == 0U,
          "reset: and notifies the zero, so the app's overwrite agrees with it");

    np_cons_on_session_end(BIT(NP_MOD_EEG));
    check(counts_are(3U, 1U, 7U, 150U), "reset: the new part's count starts from the next session");

    int w = g_writes;
    check(reset_kind(0U) == NP_ATT_OK && reset_kind(0U) == NP_ATT_OK, "reset: resetting twice is accepted");
    np_cons_poll();
    np_cons_poll();
    check(g_writes == w + 1, "reset: a reset that changes nothing writes nothing more");

    uint8_t two[2] = { 0U, 0U };
    uint8_t four = 4U;
    check(np_gatt_on_write(NP_GATT_ID_CONSUMABLE_STATUS, two, 2U) == NP_ATT_INVALID_VALUE_LENGTH,
          "reset: a 2-byte write is refused before the module sees it");
    check(np_gatt_on_write(NP_GATT_ID_CONSUMABLE_STATUS, &four, 1U) == NP_ATT_APP_REFUSED,
          "reset: a kind index past the four is refused");
}

static void test_unreadable_store_is_not_zero(void)
{
    const np_hub_status_t bad[] = { NP_HUB_ERR_STORE_IO, NP_HUB_ERR_STORE_INTEGRITY };
    for (size_t i = 0U; i < 2U; i++) {
        new_device();
        store_counts(1U, 10U, 0U, 0U);   /* a sleeve prompt is pending */
        g_read_rc = bad[i];
        boot();
        uint16_t v[4];
        uint8_t  buf[16];
        size_t   len = 0U;
        check(!read_counts(v) &&
              np_gatt_on_read(NP_GATT_ID_CONSUMABLE_STATUS, 0U, buf, sizeof buf, &len)
                  == NP_ATT_APP_UNAVAILABLE,
              i == 0U ? "unreadable (I/O): READ fails rather than serving zeros"
                      : "unreadable (integrity): READ fails rather than serving zeros");
        check(reset_kind(0U) == NP_ATT_APP_REFUSED, "unreadable: a reset is refused, so the app retries");
        np_cons_on_session_end(BIT(NP_MOD_INTRANASAL));
        np_cons_poll();
        check(g_writes == 0 && g_notifies == 0 && stored_are(1U, 10U, 0U, 0U),
              "unreadable: nothing is written over the record, nothing notified");

        g_read_rc = NP_HUB_OK;   /* the store recovers */
        np_cons_poll();
        check(counts_are(2U, 10U, 0U, 0U) && stored_are(2U, 10U, 0U, 0U) && g_notifies == 1,
              "unreadable: once it loads, the held session is applied, persisted and notified");
    }

    new_device();
    store_counts(1U, 1U, 1U, 1U);
    g_rec[0] = 0x7EU;   /* a record this firmware did not write */
    boot();
    uint16_t v[4];
    check(!read_counts(v), "unreadable: a wrong record version is not believed");
}

static void test_saturates(void)
{
    new_device();
    store_counts(0xFFFEU, 0U, 0U, 0U);
    boot();
    np_cons_on_session_end(BIT(NP_MOD_INTRANASAL));
    np_cons_on_session_end(BIT(NP_MOD_INTRANASAL));
    check(counts_are(0xFFFFU, 0U, 0U, 0U), "saturation: a count stops at 65535, never wraps to 0");
}

static void test_failed_write_is_retried(void)
{
    new_device();
    g_write_rc = NP_HUB_ERR_STORE_IO;
    np_cons_on_session_end(BIT(NP_MOD_VNS_HRV));
    check(!g_rec_present && counts_are(0U, 0U, 1U, 0U),
          "retry: a failed write keeps the count in RAM");
    g_write_rc = NP_HUB_OK;
    np_cons_poll();
    check(stored_are(0U, 0U, 1U, 0U), "retry: the next poll writes it");
}

static void reset_sleeves_now(void)
{
    (void)reset_kind(0U);
}

static void test_no_stale_overwrite(void)
{
    new_device();
    store_counts(5U, 0U, 0U, 0U);
    boot();
    /* A replacement lands while the session-end write is in flight: that
     * write carries the old snapshot, so it must not mark the store clean. */
    g_during_write = reset_sleeves_now;
    np_cons_on_session_end(BIT(NP_MOD_INTRANASAL));
    check(stored_are(6U, 0U, 0U, 0U) && counts_are(0U, 0U, 0U, 0U),
          "race: the in-flight write stored the snapshot it took; RAM has the reset");
    np_cons_poll();
    check(stored_are(0U, 0U, 0U, 0U), "race: the next poll writes the newer value, not the older");
}

static void test_notify_on_change_only(void)
{
    new_device();
    np_cons_poll();
    np_cons_poll();
    check(g_notifies == 1, "notify: the first frame once, then nothing while unchanged");
    np_cons_on_session_end(BIT(NP_MOD_AUDIO));
    np_cons_poll();
    check(g_notifies == 2 && g_ntf[6] == 1U && g_ntf[7] == 0U,
          "notify: a change is notified, little-endian, in kind order");
}

int main(void)
{
    test_new_device_reads_zero();
    test_increment_rule();
    test_counts_survive_a_reboot();
    test_replacement_reset();
    test_unreadable_store_is_not_zero();
    test_saturates();
    test_failed_write_is_retried();
    test_no_stale_overwrite();
    test_notify_on_change_only();

    if (g_failures != 0) {
        printf("\n%d FAILURE(S)\n", g_failures);
        return 1;
    }
    printf("\nAll np_consumables tests passed.\n");
    return 0;
}
