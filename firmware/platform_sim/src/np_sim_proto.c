/*
 * The app's protocol upload, over the hub's own transport (NP-SIM-FW-001 §26, OI-SIMFW-38).
 *
 * What the real app does: it compiles a protocol (header, commands, a zeroed 64-byte signature slot: hubCompiler.ts), a signing service
 * fills the slot, and the app writes the framed bytes (a 4-byte little-endian length, then the blob) to the hub over BLE GATT or USB-C CDC.
 * The hub's transport (np_transport.c) reassembles them; task_protocol_rx takes the blob out, hashes it (np_upload_check.c, SWR-UP-01),
 * verifies and loads it (np_runner_load), and reports the verdict to the app as the 34-byte upload-check frame.
 *
 * What runs here is the device's own code for every step but the last: np_transport_feed, np_hal_proto_queue_receive,
 * np_upload_check_received and _result, and np_protocol_verify_and_parse with the admissions np_runner_load makes before it asks the
 * safety MCU for anything.  np_runner_load itself is not in the loop (np_sim_spike_link.c), so a session is not started: the verdict
 * is the hub's own, the run is not.
 *
 * Commands (stdin, one line each):
 *   proto_sign <hex>   play the SIGNING SERVICE: sign an unsigned blob (zeroed signature slot) with the TEST protocol key; emits proto_signed
 *   proto_feed <hex>   write these bytes of the framed stream to the transport, as one BLE write or one CDC read would
 *   proto_pump         one pass of task_protocol_rx: take a complete blob, if any, hash it, verify it, report the verdict
 *
 * The signing key is a TEST key whose seed is below (REQ-SIMFW-08); signing for the real service stays in Rust (CLAUDE.md §22) and is not
 * on this route (OI-SIMFW-41).  Nothing here is verification evidence (REQ-SIMFW-06).
 */
#include "np_sim_bus.h"
#include "np_sim_pbm_harness.h"
#include "np_sim_safety.h"

#include "np_gatt_server.h"
#include "np_layout_bind.h"
#include "np_protocol.h"
#include "np_safety_spi.h"
#include "np_transport.h"
#include "np_upload_check.h"

#include "monocypher-ed25519.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The TEST protocol signing key's seed: 0xC0 + index, so it is not the safety harness's TEST root key (seed 7) or its tier authority
 * (0xA0 + index).  A choice, in unjustified-choices.md. */
#define PROTO_SEED_BASE 0xC0U

static uint8_t g_sk[64];
static uint8_t g_pk[32];
static bool    g_key_ready;

static void key_init(void)
{
    if (g_key_ready) { return; }
    uint8_t seed[32];
    for (unsigned i = 0U; i < 32U; i++) { seed[i] = (uint8_t)(PROTO_SEED_BASE + i); }
    crypto_ed25519_key_pair(g_sk, g_pk, seed);   /* wipes the seed */
    g_key_ready = true;
}

void np_sim_proto_pubkey(uint8_t pub[32])
{
    key_init();
    memcpy(pub, g_pk, 32);
}

/* np_gatt_server.c is not linked: the hub's notification is reported as the event the app would receive.  A double of first-party
 * code, listed as a gap (OI-SIMFW-39). */
np_hub_status_t np_gatt_notify(uint16_t id, const uint8_t *data, size_t len)
{
    char hex[2U * 64U + 1U];
    size_t n = len < 64U ? len : 64U;
    for (size_t i = 0U; i < n; i++) { (void)snprintf(&hex[2U * i], 3U, "%02x", data[i]); }
    np_sim_emit("{\"ev\":\"gatt_notify\",\"t\":%u,\"id\":%u,\"len\":%u,\"hex\":\"%s\"}", (unsigned)np_sim_now_ms(), (unsigned)id, (unsigned)len, hex);
    return NP_HUB_OK;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') { return c - '0'; }
    if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
    if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
    return -1;
}

/* Decodes a hex string into out; the length in bytes, or -1 if it is not whole hex or does not fit. */
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

static uint8_t g_buf[NP_HUB_PROTO_BLOB_MAX + NP_TRANSPORT_LEN_PREFIX];
static uint8_t g_rx[NP_HUB_PROTO_BLOB_MAX];

static void bad(const char *cmd, const char *why)
{
    np_sim_emit("{\"ev\":\"error\",\"what\":\"%s: %s\"}", cmd, why);
}

bool np_simh_proto_line(const char *cmd, char *a)
{
    if (strcmp(cmd, "proto_sign") == 0) {
        int n = (a != NULL) ? unhex(a, g_buf, NP_HUB_PROTO_BLOB_MAX) : -1;
        if (n < (int)(sizeof(np_proto_header_t) + NP_HUB_PROTO_SIG_LEN)) { bad(cmd, "needs the blob as hex, header and signature slot included"); return true; }
        key_init();
        size_t msg_len = (size_t)n - NP_HUB_PROTO_SIG_LEN;
        crypto_ed25519_sign(&g_buf[msg_len], g_sk, g_buf, msg_len);
        char *hex = malloc((size_t)n * 2U + 1U);
        if (hex == NULL) { bad(cmd, "out of memory"); return true; }
        for (int i = 0; i < n; i++) { (void)snprintf(&hex[2 * i], 3U, "%02x", g_buf[i]); }
        np_sim_emit("{\"ev\":\"proto_signed\",\"len\":%d,\"hex\":\"%s\"}", n, hex);
        free(hex);
        return true;
    }
    if (strcmp(cmd, "proto_feed") == 0) {
        int n = (a != NULL) ? unhex(a, g_buf, sizeof g_buf) : -1;
        if (n < 0) { bad(cmd, "needs bytes as hex"); return true; }
        np_hub_status_t rc = np_transport_feed(g_buf, (size_t)n);
        np_sim_emit("{\"ev\":\"result\",\"cmd\":\"proto_feed\",\"code\":%d,\"bytes\":%d}", (int)rc, n);
        return true;
    }
    if (strcmp(cmd, "proto_pump") == 0) {
        size_t len = 0U;
        np_hub_status_t rc = np_hal_proto_queue_receive(g_rx, sizeof g_rx, &len, 0U);
        if (rc != NP_HUB_OK || len == 0U) {
            np_sim_emit("{\"ev\":\"result\",\"cmd\":\"proto_pump\",\"code\":%d,\"blob\":false}", (int)rc);
            return true;
        }
        /* task_protocol_rx: hash the bytes as they arrived, then load.  np_runner_load is not in the loop, so the load is its first
         * steps, in its order (np_session_runner.c): verify and parse, the tier, the hardware version, the layout. */
        np_upload_check_received(g_rx, len);
        np_session_desc_t desc;
        memset(&desc, 0, sizeof desc);
        np_hub_status_t v = np_protocol_verify_and_parse(g_rx, len, &desc);
        if (v == NP_HUB_OK) { v = np_protocol_tier_admit(&desc, np_safety_spi_get_tier()); }
        if (v == NP_HUB_OK) {
            v = np_protocol_hw_version_admit(&desc, np_safety_spi_get_hardware_version(), np_safety_spi_get_tier_report(NULL, NULL));
        }
        if (v == NP_HUB_OK) { v = np_layout_admit(np_safety_spi_get_hardware_version()); }
        np_upload_check_result(v);
        memset(g_rx, 0, len);
        np_sim_emit("{\"ev\":\"result\",\"cmd\":\"proto_pump\",\"code\":%d,\"blob\":true,\"len\":%u,\"cmd_count\":%u,"
                    "\"session_started\":false,\"safety_mcu\":\"%s\"}",
                    (int)v, (unsigned)len, v == NP_HUB_OK ? (unsigned)desc.cmd_count : 0U, np_sim_safety_label());
        return true;
    }
    return false;
}
