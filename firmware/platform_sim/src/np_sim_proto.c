/*
 * The app's protocol upload, over the hub's own transport (NP-SIM-FW-001 §26, OI-SIMFW-38).
 *
 * What the real app does: it compiles a protocol (header, commands, a zeroed 64-byte signature slot: hubCompiler.ts), a signing service
 * fills the slot, and the app writes the framed bytes (a 4-byte little-endian length, then the blob) to the hub over BLE GATT or USB-C CDC.
 * The hub's transport (np_transport.c) reassembles them; task_protocol_rx takes the blob out, hashes it (np_upload_check.c, SWR-UP-01),
 * verifies and loads it (np_runner_load), and reports the verdict to the app as the 34-byte upload-check frame.
 *
 * What runs here is the device's own code: np_transport_feed, np_hal_proto_queue_receive, np_upload_check_received and _result, and
 * np_runner_load (the boot admission, the session lease, np_protocol_verify_and_parse with the device-serial replay guard, then the tier,
 * hardware-version, layout, address and photic admissions).  np_runner_run is not modelled (OI-SIMFW-45), so a load that succeeds does not
 * start a session and holds the lease: the verdict is the hub's own, the run is not.
 *
 * Commands (stdin, one line each):
 *   proto_sign <hex>   play the SIGNING SERVICE: sign an unsigned blob (zeroed signature slot) with the TEST protocol key; emits proto_signed
 *   proto_feed <hex>   write these bytes of the framed stream to the transport, as one BLE write or one CDC read would
 *   proto_pump         one pass of task_protocol_rx: take a complete blob, if any, hash it, np_runner_load it, report the verdict
 *   proto_serial <hex> set the TEST device serial the replay guard compares with (32 bytes; all zero by default, UC-137)
 *   proto_boot admit|refuse  the boot decision: whether the device's config partition mounted and sessions are admitted
 *
 * The signing key is a TEST key whose seed is below (REQ-SIMFW-08); signing for the real service stays in Rust (CLAUDE.md §22) and is not
 * on this route (OI-SIMFW-41).  Nothing here is verification evidence (REQ-SIMFW-06).
 */
#include "np_sim_bus.h"
#include "np_sim_pbm_harness.h"
#include "np_sim_safety.h"

#include "np_gatt_server.h"
#include "np_layout_bind.h"
#include "np_log_backend.h"
#include "np_protocol.h"
#include "np_safety_spi.h"
#include "np_session_runner.h"
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

/* The hub's own bring-up gives the runner an event group; here it is a token, and what the runner posts to it is reported (see
 * xEventGroupSetBits in np_sim_spike_link.c). */
static int  g_ev_token;
static bool g_runner_inited;
static bool g_boot_decided;
static void np_runner_init_once(void)
{
    if (!g_runner_inited) { np_runner_init((EventGroupHandle_t)&g_ev_token); g_runner_inited = true; }
    if (!g_boot_decided) {      /* a boot nothing has decided: the simulator's default is a bound block device (UC-137) */
        (void)np_log_backend_init();
        g_boot_decided = true;
    }
}

/* The device serial the TEST device reports: all zero, which is what the app's compiler writes into the header when it has no device
 * serial (the golden descriptors, common/testdata/hub-descriptor-golden.json), so a protocol compiled without one passes the
 * replay guard, and a protocol for another serial does not (proto_serial changes it).  UC-137. */
static uint8_t g_serial[NP_GATT_DEVICE_SERIAL_LEN];

np_hub_status_t np_sim_proto_serial(uint8_t *buf, size_t len)
{
    if (buf == NULL || len < NP_GATT_DEVICE_SERIAL_LEN) { return NP_HUB_ERR_INVALID_ARG; }
    memcpy(buf, g_serial, NP_GATT_DEVICE_SERIAL_LEN);
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
    if (strcmp(cmd, "proto_serial") == 0) {
        uint8_t tmp[NP_GATT_DEVICE_SERIAL_LEN];
        int n = (a != NULL) ? unhex(a, tmp, sizeof tmp) : -1;
        if (n != (int)NP_GATT_DEVICE_SERIAL_LEN) { bad(cmd, "needs the device serial as hex, whole"); return true; }
        memcpy(g_serial, tmp, sizeof g_serial);
        np_sim_emit("{\"ev\":\"result\",\"cmd\":\"proto_serial\",\"code\":0}");
        return true;
    }
    if (strcmp(cmd, "proto_boot") == 0) {
        /* The boot decision of np_hub_control_main.c (OI-NVRAM-19): the config partition mounted, so sessions are admitted, or it did not and
         * np_runner_load refuses every protocol as NP_HUB_ERR_BOOT_REFUSED.  The firmware's own functions set the flag. */
        if (a != NULL && strcmp(a, "admit") == 0)       { (void)np_log_backend_init(); }
        else if (a != NULL && strcmp(a, "refuse") == 0) { np_log_backend_refuse(); }
        else { bad(cmd, "needs admit or refuse"); return true; }
        g_boot_decided = true;
        np_sim_emit("{\"ev\":\"result\",\"cmd\":\"proto_boot\",\"code\":0,\"admits\":%s}", np_log_backend_admits_sessions() ? "true" : "false");
        return true;
    }
    if (strcmp(cmd, "proto_pump") == 0) {
        size_t len = 0U;
        np_hub_status_t rc = np_hal_proto_queue_receive(g_rx, sizeof g_rx, &len, 0U);
        if (rc != NP_HUB_OK || len == 0U) {
            np_sim_emit("{\"ev\":\"result\",\"cmd\":\"proto_pump\",\"code\":%d,\"blob\":false}", (int)rc);
            return true;
        }
        /* task_protocol_rx: hash the bytes as they arrived, then np_runner_load: the device's own admissions, in its own order (the boot
         * flag, the session lease, verify and parse with the device-serial replay guard, the tier, the hardware version, the layout and
         * its addresses, the photic refusal).  A load that succeeds holds the session lease until a run ends it. */
        np_upload_check_received(g_rx, len);
        np_runner_init_once();
        np_hub_status_t v = np_runner_load(g_rx, len);
        np_session_desc_t desc;
        memset(&desc, 0, sizeof desc);
        (void)np_protocol_verify_and_parse(g_rx, len, &desc);   /* for the command count the event reports; the load above is the verdict */
        np_upload_check_result(v);
        memset(g_rx, 0, len);
        np_sim_emit("{\"ev\":\"result\",\"cmd\":\"proto_pump\",\"code\":%d,\"blob\":true,\"len\":%u,\"cmd_count\":%u,"
                    "\"loaded\":%s,\"session_started\":false,\"safety_mcu\":\"%s\"}",
                    (int)v, (unsigned)len, v == NP_HUB_OK ? (unsigned)desc.cmd_count : 0U, v == NP_HUB_OK ? "true" : "false", np_sim_safety_label());
        return true;
    }
    return false;
}
