/*
 * The hub's GATT front and its storage, in the simulator (NP-SIM-FW-001 §27, OI-SIMFW-38).
 *
 * The app reaches the hub over GATT: it discovers the service, reads and writes characteristics and takes notifications.  The hub's own
 * np_gatt_server.c is what answers; it is linked here with the producers its table names, and this file is the two things it asks the
 * platform for.  No firmware file is edited (REQ-SIMFW-03).
 *
 *   np_gatt_hal_register   the stack's registration: the table is kept so gatt_table can list it
 *   np_gatt_hal_notify     a notification: reported as a gatt_notify event, to every subscribed central (the central filters)
 *
 * The producers keep their settings through np_cfg_store.c over LittleFS, so the storage is here too: the device's own Config-instance
 * geometry (np_lfs_instance.c) over a RAM medium, which is the block device the hub's own storage tests use (tests/np_lfs_powerbd.c).
 * It is formatted at the first gatt_init, as a new part would be.  A RAM medium is not the eMMC: no wear, no timing, no power loss
 * unless a test injects one (UC-138).
 *
 * Commands (decimal ids, hex bytes; the bridge validates both, REQ-SIMFW-12):
 *   gatt_init                 bring up the storage and the service (np_gatt_init), once
 *   gatt_table                list the characteristics the hub publishes
 *   gatt_read <id> <offset>   np_gatt_on_read: the ATT status and the bytes
 *   gatt_write <id> <hex>     np_gatt_on_write: the ATT status
 *
 * A characteristic the hub does not publish answers NP_ATT_ATTRIBUTE_NOT_FOUND, as it does on the device.  That includes the protocol
 * upload (0x0008): np_gatt_server.h says binding np_transport_feed to it is a separate change, so the simulator does not bind it either.
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/random.h>

#include "lfs.h"
#include "np_cfg_store.h"
#include "np_gatt_server.h"
#include "np_lfs_instance.h"
#include "np_lfs_powerbd.h"
#include "np_sim_bus.h"
#include "np_sim_pbm_harness.h"
#include "np_warranty_token.h"

#define SIM_MEDIA_BYTES ((size_t)NP_LFS_CFG_BLOCK_SIZE * (size_t)NP_LFS_CFG_BLOCK_COUNT)

static uint8_t           g_media[SIM_MEDIA_BYTES];
static uint8_t           g_dirty[(NP_LFS_CFG_BLOCK_COUNT + 7U) / 8U];
static lfs_t             g_lfs;
static struct lfs_config g_cfg;
static np_powerbd_t      g_bd;
static bool              g_up;
static const np_gatt_char_t *g_registered;
static size_t            g_registered_count;

static int  sim_lock(const struct lfs_config *c)   { (void)c; return 0; }
static int  sim_unlock(const struct lfs_config *c) { (void)c; return 0; }
static void store_lock(void)   { }
static void store_unlock(void) { }

np_hub_status_t np_gatt_hal_register(const np_gatt_char_t *table, size_t count)
{
    g_registered = table;
    g_registered_count = count;
    return NP_HUB_OK;
}

/* The warranty token is drawn from the TRNG at the first read and kept (np_warranty_token.c).  Here the draw is the host's entropy
 * (getrandom), which has none of the RT1062 TRNG's NIST SP 800-90B health tests: the token's value is random and kept, and nothing more is
 * claimed of it (UC-138). */
np_hub_status_t np_warranty_hal_trng_generate(uint8_t *buf, size_t len)
{
    size_t got = 0U;
    while (got < len) {
        ssize_t r = getrandom(buf + got, len - got, 0);
        if (r <= 0) { return NP_HUB_ERR_MOD_FAULT; }
        got += (size_t)r;
    }
    return NP_HUB_OK;
}

static void emit_hex(const char *head, const uint8_t *d, size_t n, const char *tail)
{
    char *hex = malloc(n * 2U + 1U);
    if (hex == NULL) { np_sim_emit("{\"ev\":\"error\",\"what\":\"gatt: out of memory\"}"); return; }
    for (size_t i = 0U; i < n; i++) { (void)snprintf(&hex[2U * i], 3U, "%02x", d[i]); }
    hex[n * 2U] = '\0';
    np_sim_emit("%s\"hex\":\"%s\"%s", head, hex, tail);
    free(hex);
}

void np_gatt_hal_notify(uint16_t id, const uint8_t *data, size_t len)
{
    char head[96];
    (void)snprintf(head, sizeof head, "{\"ev\":\"gatt_notify\",\"t\":%u,\"id\":%u,\"len\":%u,", (unsigned)np_sim_now_ms(), (unsigned)id, (unsigned)len);
    emit_hex(head, data, len, "}");
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') { return c - '0'; }
    if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
    if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
    return -1;
}

static int unhex(const char *h, uint8_t *out, size_t cap)
{
    size_t n = strlen(h);
    if (n == 0U || (n % 2U) != 0U || n / 2U > cap) { return -1; }
    for (size_t i = 0U; i < n / 2U; i++) {
        int hi = hexval(h[2U * i]), lo = hexval(h[2U * i + 1U]);
        if (hi < 0 || lo < 0) { return -1; }
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return (int)(n / 2U);
}

static void bad(const char *cmd, const char *why)
{
    np_sim_emit("{\"ev\":\"error\",\"what\":\"%s: %s\"}", cmd, why);
}

/* A new part: the RAM medium wiped, the Config instance formatted and mounted through the store, then the service registered. */
static np_hub_status_t bring_up(void)
{
    memset(&g_cfg, 0, sizeof g_cfg);
    np_hub_status_t rc = np_lfs_config_apply(&g_cfg);
    if (rc != NP_HUB_OK) { return rc; }
    np_powerbd_bind(&g_bd, &g_cfg, g_media, NP_LFS_CFG_BLOCK_SIZE, NP_LFS_CFG_BLOCK_COUNT, NP_LFS_CFG_PROG_SIZE, g_dirty, sizeof g_dirty);
    g_cfg.lock = sim_lock;
    g_cfg.unlock = sim_unlock;
    rc = np_lfs_config_validate(&g_cfg);
    if (rc != NP_HUB_OK) { return rc; }
    np_powerbd_wipe(&g_bd);
    np_powerbd_power_cycle(&g_bd);
    memset(&g_lfs, 0, sizeof g_lfs);
    rc = np_cfg_store_bind(&g_lfs, &g_cfg, store_lock, store_unlock);
    if (rc != NP_HUB_OK) { return rc; }
    rc = np_cfg_store_format();
    if (rc != NP_HUB_OK) { return rc; }
    rc = np_cfg_store_mount();
    if (rc != NP_HUB_OK) { return rc; }
    return np_gatt_init();
}

bool np_simh_gatt_line(const char *cmd, char *a, char *b)
{
    if (strcmp(cmd, "gatt_init") == 0) {
        if (g_up) { bad(cmd, "already up"); return true; }
        np_hub_status_t rc = bring_up();
        g_up = (rc == NP_HUB_OK);
        np_sim_emit("{\"ev\":\"result\",\"cmd\":\"gatt_init\",\"code\":%d,\"up\":%s}", (int)rc, g_up ? "true" : "false");
        return true;
    }
    if (strncmp(cmd, "gatt_", 5) != 0) { return false; }
    if (!g_up) { bad(cmd, "the GATT service is not up (gatt_init first)"); return true; }

    if (strcmp(cmd, "gatt_table") == 0) {
        size_t n = 0U;
        const np_gatt_char_t *t = np_gatt_table(&n);
        char line[64 + 80 * 40];
        size_t off = (size_t)snprintf(line, sizeof line, "{\"ev\":\"gatt_table\",\"chars\":[");
        for (size_t i = 0U; i < n && off + 80U < sizeof line; i++) {
            off += (size_t)snprintf(&line[off], sizeof line - off, "%s{\"id\":%u,\"props\":%u,\"max_len\":%u,\"write_len\":%u}",
                                    i == 0U ? "" : ",", (unsigned)t[i].id, (unsigned)t[i].props, (unsigned)t[i].max_len, (unsigned)t[i].write_len);
        }
        (void)snprintf(&line[off], sizeof line - off, "]}");
        np_sim_emit("%s", line);
        return true;
    }
    if (strcmp(cmd, "gatt_read") == 0) {
        if (a == NULL || b == NULL) { bad(cmd, "needs an id and an offset"); return true; }
        uint8_t buf[NP_GATT_VALUE_MAX];
        size_t len = 0U;
        const unsigned id = (unsigned)atoi(a);
        np_att_status_t st = np_gatt_on_read((uint16_t)id, (uint16_t)atoi(b), buf, sizeof buf, &len);
        char head[64];
        (void)snprintf(head, sizeof head, "{\"ev\":\"gatt_read\",\"id\":%u,\"att\":%d,", id, (int)st);
        emit_hex(head, st == NP_ATT_OK && len > 0U ? buf : (const uint8_t *)"", st == NP_ATT_OK ? len : 0U, "}");
        memset(buf, 0, sizeof buf);
        return true;
    }
    if (strcmp(cmd, "gatt_write") == 0) {
        /* Room for a whole ATT write (the app's upload chunks are up to 512 bytes), so the hub's own answer is the one reported:
         * a characteristic it does not publish is not found, whatever the length. */
        static uint8_t buf[512];
        int n = (b != NULL) ? unhex(b, buf, sizeof buf) : -1;
        if (a == NULL || n < 0) { bad(cmd, "needs an id and the bytes as hex"); return true; }
        const unsigned id = (unsigned)atoi(a);
        np_att_status_t st = np_gatt_on_write((uint16_t)id, buf, (size_t)n);
        np_sim_emit("{\"ev\":\"gatt_write\",\"id\":%u,\"att\":%d,\"len\":%d}", id, (int)st, n);
        memset(buf, 0, sizeof buf);
        return true;
    }
    return false;
}
