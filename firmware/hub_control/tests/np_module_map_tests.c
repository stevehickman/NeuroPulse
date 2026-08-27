/*
 * NeurOne Hub Control — Hexagonal Zone-Module Map Host Tests
 * Design study: hexagonal zone-module redesign (2026-07-15 session).
 *
 * Exercises the 2-level (socket:element) addressing, the power-on poll /
 * UID-change inventory cache, address→physical resolution, group resolution
 * (socket sets, address sets, type include/exclude, invalid-kind fail-closed),
 * and NVRAM serialize/load + persist/restore via injected HAL stubs.
 *
 * There are no firmware-defined groups: the predefined-lobe-group path was retired
 * (OI-HUB-C14) and zone membership arrives as a caller-supplied socket list.
 *
 * No FreeRTOS, no hardware. IEC 62304 Class B — SW-02 hub control.
 * Build with -DNP_BUILD_TESTS=ON (see firmware/hub_control/CMakeLists.txt).
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../include/np_module_map.h"

/* The mirror-constant pin in test_cal_length_matches_pbm_library(): this is the
 * one translation unit that can see BOTH np_module_map.h and the pbm library's
 * headers, so it is where NP_PBM_MODULE_UID_LEN == NP_HEXMAP_UID_LEN is
 * asserted. The libraries themselves cannot include each other — hub_control
 * links pbm, not the reverse. */
#include "np_pbm_config.h"
#include "np_pbm_types.h"

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

/* ── Test helmet geometry: 8 wired sockets + 1 unwired socket (index 8) ────────
 * Geometry is metric only (OI-HUB-C14 retired lobe/side). The socket NAMES below
 * still carry anatomical intent so the tests read like real montages, but that
 * intent lives in the name, not in a field the firmware stores. */

enum { SOCK_FL = 0, SOCK_FR, SOCK_TL, SOCK_TR, SOCK_PL, SOCK_PR, SOCK_OL, SOCK_OR,
       SOCK_UNWIRED, N_SOCK };

static const np_socket_geom_t GEOM[N_SOCK] = {
    { true,  -30,  -80 },
    { true,   30,  -80 },
    { true,  -70,    0 },
    { true,   70,    0 },
    { true,  -25,   40 },
    { true,   25,   40 },
    { true,  -25,  110 },
    { true,   25,  110 },
    { false,   0,    0 },  /* not wired */
};

/* Resolve one zone's socket list — the shape every zone query now takes: the app
 * compiles a zone from 00-zones.npps into a socket bitmap, firmware expands it to
 * a KIND_SOCKET_SET query. Replaces the retired np_module_map_predefined(). */
static np_hub_status_t resolve_sockets(const uint16_t *sockets, uint16_t count,
                                      uint64_t type_mask, bool type_exclude,
                                      np_hex_addr_t *out, uint16_t max,
                                      uint16_t *count_out)
{
    np_group_query_t q;
    memset(&q, 0, sizeof(q));
    q.kind         = NP_GROUP_KIND_SOCKET_SET;
    q.sockets      = sockets;
    q.socket_count = count;
    q.type_mask    = type_mask;
    q.type_exclude = type_exclude;
    return np_module_map_resolve_group(&q, out, max, count_out);
}

/* ── Inventory callback: hands out a fixed type list, counts its own calls ───── */

static int      g_inv_calls;
static uint8_t  g_inv_types[NP_HEXMAP_MAX_ELEMENTS];
static uint8_t  g_inv_count;
static np_hub_status_t g_inv_rc;

static np_hub_status_t inv_cb(uint16_t socket_id, void *ctx,
                              uint8_t *types_out, uint8_t max, uint8_t *count_out)
{
    (void)socket_id; (void)ctx;
    g_inv_calls++;
    if (g_inv_rc != NP_HUB_OK) {
        return g_inv_rc;
    }
    uint8_t n = g_inv_count;
    if (n > max) {
        n = max;               /* mirror a well-behaved module respecting max */
    }
    memcpy(types_out, g_inv_types, n);
    *count_out = g_inv_count;   /* report the *real* count (may exceed max: fail path) */
    return NP_HUB_OK;
}

static void inv_set(const np_elem_type_t *types, uint8_t n)
{
    g_inv_calls = 0;
    g_inv_rc    = NP_HUB_OK;
    g_inv_count = n;
    for (uint8_t i = 0; i < n && i < NP_HEXMAP_MAX_ELEMENTS; i++) {
        g_inv_types[i] = (uint8_t)types[i];
    }
}

static np_module_uid_t uid_of(uint8_t seed)
{
    np_module_uid_t u;
    for (unsigned i = 0; i < NP_HEXMAP_UID_LEN; i++) {
        u.b[i] = (uint8_t)(seed + i);
    }
    return u;
}

/* A representative PBM tile: 660 + 808 + 1064 + NTC + PD1 + PD2. */
static const np_elem_type_t PBM_TILE[] = {
    NP_ELEM_LED_660, NP_ELEM_LED_808, NP_ELEM_LED_1064,
    NP_ELEM_NTC, NP_ELEM_PD_FORWARD, NP_ELEM_PD_BACK,
};
#define PBM_TILE_N (sizeof(PBM_TILE) / sizeof(PBM_TILE[0]))

/* Plug a fresh PBM tile (uid seed) into a socket; returns changed flag. */
static bool plug_pbm(uint16_t socket, uint8_t seed)
{
    inv_set(PBM_TILE, (uint8_t)PBM_TILE_N);
    np_module_uid_t u = uid_of(seed);
    bool changed = false;
    (void)np_module_map_apply_poll(socket, &u, 0x00, inv_cb, NULL, &changed);
    return changed;
}

/* A T1-B EEG/electrode tile: dual-rated electrode + reduced PBM + NTC. */
static const np_elem_type_t EEG_TILE[] = {
    NP_ELEM_DUAL_ELECTRODE, NP_ELEM_LED_660, NP_ELEM_LED_808, NP_ELEM_NTC,
};
#define EEG_TILE_N (sizeof(EEG_TILE) / sizeof(EEG_TILE[0]))

static void plug_eeg(uint16_t socket, uint8_t seed)
{
    inv_set(EEG_TILE, (uint8_t)EEG_TILE_N);
    np_module_uid_t u = uid_of(seed);
    bool changed = false;
    (void)np_module_map_apply_poll(socket, &u, 0x00, inv_cb, NULL, &changed);
}

/* ── NVRAM HAL stubs (test-controlled) ────────────────────────────────────────── */

static uint8_t g_nvram[NP_HEXMAP_HDR_BYTES +
                       NP_HEXMAP_MAX_SOCKETS * NP_HEXMAP_REC_BYTES +
                       NP_HEXMAP_CRC_BYTES];
static size_t  g_nvram_len;
static bool    g_nvram_written;

np_hub_status_t np_hexmap_nvram_write(const uint8_t *buf, size_t len)
{
    if (len > sizeof(g_nvram)) {
        return NP_HUB_ERR_GENERIC;
    }
    memcpy(g_nvram, buf, len);
    g_nvram_len     = len;
    g_nvram_written = true;
    return NP_HUB_OK;
}

np_hub_status_t np_hexmap_nvram_read(uint8_t *buf, size_t len, size_t *read_len)
{
    if (!g_nvram_written || g_nvram_len > len) {
        return NP_HUB_ERR_NOT_PRESENT;
    }
    memcpy(buf, g_nvram, g_nvram_len);
    *read_len = g_nvram_len;
    return NP_HUB_OK;
}

/* ── Tests ────────────────────────────────────────────────────────────────────── */

static void test_addr_pack(void)
{
    /* Max is 127:127 — socket is the FULL 7-bit domain since MAX_SOCKETS went
     * 64 -> 128. (This asserted 63 while MAX_SOCKETS was 64.) */
    np_hex_addr_t a = { NP_HEXMAP_MAX_SOCKETS - 1, NP_HEXMAP_MAX_ELEMENTS - 1 };
    uint16_t v = np_hex_addr_pack(a);
    np_hex_addr_t b = np_hex_addr_unpack(v);
    check(b.socket_id == 127 && b.element_id == 127, "addr pack/unpack round-trip max");
    check(NP_HEXMAP_MAX_SOCKETS == (1 << NP_HEXMAP_SOCKET_BITS),
          "MAX_SOCKETS is the full 7-bit major domain");

    np_hex_addr_t c = { 14, 7 };
    np_hex_addr_t d = np_hex_addr_unpack(np_hex_addr_pack(c));
    check(d.socket_id == 14 && d.element_id == 7, "addr pack/unpack 14:07");
    check((np_hex_addr_pack(c) >> 7) == 14, "socket in high 7 bits");
}

static void test_init(void)
{
    check(np_module_map_init(GEOM, N_SOCK) == NP_HUB_OK, "init ok");
    check(np_module_map_init(NULL, N_SOCK) == NP_HUB_ERR_INVALID_ARG, "init null geom");
    check(np_module_map_init(GEOM, 0) == NP_HUB_ERR_INVALID_ARG, "init zero sockets");
    check(np_module_map_init(GEOM, NP_HEXMAP_MAX_SOCKETS + 1) == NP_HUB_ERR_INVALID_ARG,
          "init too many sockets");
    /* leave a valid init in place */
    (void)np_module_map_init(GEOM, N_SOCK);
}

static void test_resolve_empty(void)
{
    (void)np_module_map_init(GEOM, N_SOCK);
    np_physical_loc_t loc;
    np_hex_addr_t a = { SOCK_FL, 0 };
    check(np_module_map_resolve(a, &loc) == NP_HUB_ERR_NOT_PRESENT,
          "resolve empty socket → NOT_PRESENT");
}

static void test_poll_new_and_resolve(void)
{
    (void)np_module_map_init(GEOM, N_SOCK);
    bool changed = plug_pbm(SOCK_FL, 0x10);
    check(changed, "new module → changed=true");
    check(g_inv_calls == 1, "new module → inventory fetched once");

    np_physical_loc_t loc;
    np_hex_addr_t a0 = { SOCK_FL, 0 };
    check(np_module_map_resolve(a0, &loc) == NP_HUB_OK, "resolve element 0 ok");
    check(loc.x_mm == -30 && loc.y_mm == -80,
          "resolve → the socket's own metric geometry row");
    check(loc.elem_type == NP_ELEM_LED_660, "resolve elem 0 → LED_660");

    np_hex_addr_t a2 = { SOCK_FL, 2 };
    check(np_module_map_resolve(a2, &loc) == NP_HUB_OK && loc.elem_type == NP_ELEM_LED_1064,
          "resolve elem 2 → LED_1064");

    np_hex_addr_t oob = { SOCK_FL, (uint8_t)PBM_TILE_N };
    check(np_module_map_resolve(oob, &loc) == NP_HUB_ERR_NOT_PRESENT,
          "resolve element out of range → NOT_PRESENT");
}

static void test_poll_same_uid_no_reinventory(void)
{
    (void)np_module_map_init(GEOM, N_SOCK);
    (void)plug_pbm(SOCK_FL, 0x20);
    check(g_inv_calls == 1, "first plug inventoried");

    /* Same UID re-poll: no re-inventory, changed=false, health refreshed. */
    inv_set(PBM_TILE, (uint8_t)PBM_TILE_N);  /* resets g_inv_calls to 0 */
    np_module_uid_t u = uid_of(0x20);
    bool changed = true;
    np_hub_status_t rc = np_module_map_apply_poll(SOCK_FL, &u, 0x55, inv_cb, NULL, &changed);
    check(rc == NP_HUB_OK, "same-uid poll ok");
    check(!changed, "same UID → changed=false");
    check(g_inv_calls == 0, "same UID → not re-inventoried");
}

static void test_poll_uid_change_reinventory(void)
{
    (void)np_module_map_init(GEOM, N_SOCK);
    (void)plug_pbm(SOCK_FL, 0x30);

    /* Different UID: a new module swapped in — must re-inventory. */
    static const np_elem_type_t EEG_TILE[] = {
        NP_ELEM_EEG_ELECTRODE, NP_ELEM_NTC,
    };
    inv_set(EEG_TILE, 2);
    np_module_uid_t u = uid_of(0x99);
    bool changed = false;
    np_hub_status_t rc = np_module_map_apply_poll(SOCK_FL, &u, 0x00, inv_cb, NULL, &changed);
    check(rc == NP_HUB_OK, "uid-change poll ok");
    check(changed, "uid change → changed=true");
    check(g_inv_calls == 1, "uid change → re-inventoried");

    np_physical_loc_t loc;
    np_hex_addr_t a0 = { SOCK_FL, 0 };
    check(np_module_map_resolve(a0, &loc) == NP_HUB_OK && loc.elem_type == NP_ELEM_EEG_ELECTRODE,
          "after swap resolve → EEG_ELECTRODE");
}

static void test_poll_removal(void)
{
    (void)np_module_map_init(GEOM, N_SOCK);
    (void)plug_pbm(SOCK_FL, 0x40);

    np_module_uid_t zero;
    memset(&zero, 0, sizeof(zero));
    bool changed = false;
    np_hub_status_t rc = np_module_map_apply_poll(SOCK_FL, &zero, 0x00, inv_cb, NULL, &changed);
    check(rc == NP_HUB_OK, "removal poll ok");
    check(changed, "removal → changed=true");

    np_physical_loc_t loc;
    np_hex_addr_t a0 = { SOCK_FL, 0 };
    check(np_module_map_resolve(a0, &loc) == NP_HUB_ERR_NOT_PRESENT,
          "after removal resolve → NOT_PRESENT");

    /* Re-poll empty socket: no state change, changed=false. */
    changed = true;
    (void)np_module_map_apply_poll(SOCK_FL, &zero, 0x00, inv_cb, NULL, &changed);
    check(!changed, "re-poll already-empty → changed=false");
}

static void test_poll_invalid(void)
{
    (void)np_module_map_init(GEOM, N_SOCK);
    np_module_uid_t u = uid_of(1);
    bool changed = false;
    check(np_module_map_apply_poll(N_SOCK, &u, 0, inv_cb, NULL, &changed)
              == NP_HUB_ERR_INVALID_ARG, "poll out-of-range socket → INVALID_ARG");
    check(np_module_map_apply_poll(SOCK_UNWIRED, &u, 0, inv_cb, NULL, &changed)
              == NP_HUB_ERR_INVALID_ARG, "poll unwired socket → INVALID_ARG");
    check(np_module_map_apply_poll(SOCK_FL, NULL, 0, inv_cb, NULL, &changed)
              == NP_HUB_ERR_INVALID_ARG, "poll null uid → INVALID_ARG");
}

static void test_poll_fail_closed_overflow(void)
{
    (void)np_module_map_init(GEOM, N_SOCK);
    /* Module claims more elements than the map can hold (200 > 128) → fail closed. */
    inv_set(PBM_TILE, (uint8_t)PBM_TILE_N);
    g_inv_count = 200;                       /* reported count exceeds NP_HEXMAP_MAX_ELEMENTS */
    np_module_uid_t u = uid_of(0x77);
    bool changed = false;
    np_hub_status_t rc = np_module_map_apply_poll(SOCK_FL, &u, 0, inv_cb, NULL, &changed);
    check(rc == NP_HUB_ERR_CMD_TOO_MANY, "over-max inventory → CMD_TOO_MANY");

    np_physical_loc_t loc;
    np_hex_addr_t a0 = { SOCK_FL, 0 };
    check(np_module_map_resolve(a0, &loc) == NP_HUB_ERR_NOT_PRESENT,
          "over-max module left not present (fail closed)");
}

static void test_poll_new_null_cb_fail_closed(void)
{
    (void)np_module_map_init(GEOM, N_SOCK);
    np_module_uid_t u = uid_of(0x88);
    bool changed = false;
    np_hub_status_t rc = np_module_map_apply_poll(SOCK_FL, &u, 0, NULL, NULL, &changed);
    check(rc == NP_HUB_ERR_NOT_PRESENT, "new module with NULL inv_cb → NOT_PRESENT");
    np_physical_loc_t loc;
    np_hex_addr_t a0 = { SOCK_FL, 0 };
    check(np_module_map_resolve(a0, &loc) == NP_HUB_ERR_NOT_PRESENT,
          "NULL-cb module left not present");
}

static void populate_all(void)
{
    (void)np_module_map_init(GEOM, N_SOCK);
    for (uint16_t s = SOCK_FL; s <= SOCK_OR; s++) {
        (void)plug_pbm(s, (uint8_t)(0x10 + s));
    }
}

/* Zone resolution, at the only granularity firmware now has: a socket list.
 * Was test_group_predefined, which drove the same assertions through the retired
 * np_module_map_predefined() / NP_GROUP_KIND_LOBE path (OI-HUB-C14). The properties
 * proven are unchanged — socket fidelity of the output, and a multi-socket zone
 * expanding to every member's elements. What is gone is firmware's opinion about
 * WHICH sockets those are; the zone file owns that. */
static void test_group_zone_socket_set(void)
{
    populate_all();
    np_hex_addr_t out[64];
    uint16_t n = 0;

    /* A single-socket zone. */
    const uint16_t frontal_l[] = { SOCK_FL };
    check(resolve_sockets(frontal_l, 1, 0, false, out, 64, &n) == NP_HUB_OK,
          "single-socket zone resolves");
    check(n == PBM_TILE_N, "single-socket zone returns one tile's worth of elements");
    bool all_fl = true;
    for (uint16_t i = 0; i < n; i++) {
        if (out[i].socket_id != SOCK_FL) { all_fl = false; }
    }
    check(all_fl, "every returned address names the requested socket");

    /* A two-socket zone — what "both hemispheres of a lobe" now looks like on the
     * wire: the app unions the two zones' socket lists and sends one bitmap. */
    const uint16_t occipital_lr[] = { SOCK_OL, SOCK_OR };
    check(resolve_sockets(occipital_lr, 2, 0, false, out, 64, &n) == NP_HUB_OK,
          "two-socket zone resolves");
    check(n == 2 * PBM_TILE_N, "two-socket zone → both sockets' elements");
}

/* A query whose `kind` was never set must fail closed. This is newly reachable:
 * kind 0 used to mean NP_GROUP_KIND_LOBE, so a memset-zeroed query silently
 * resolved a frontal-left lobe group. With the lobe kind retired, 0 is unassigned
 * and lands on the default arm (OI-HUB-C14). */
static void test_group_invalid_kind_fails_closed(void)
{
    populate_all();
    np_hex_addr_t out[64];
    uint16_t n = 99;
    np_group_query_t q;

    memset(&q, 0, sizeof(q));   /* kind == 0 — no longer a valid group kind */
    check(np_module_map_resolve_group(&q, out, 64, &n) == NP_HUB_ERR_INVALID_ARG,
          "memset-zeroed query (kind 0) → INVALID_ARG, not a silent lobe group");

    memset(&q, 0, sizeof(q));
    q.kind = (np_group_kind_t)99;
    check(np_module_map_resolve_group(&q, out, 64, &n) == NP_HUB_ERR_INVALID_ARG,
          "unknown group kind → INVALID_ARG");
}

static void test_group_type_filter(void)
{
    populate_all();
    np_hex_addr_t out[64];
    uint16_t n = 0;
    const uint16_t one_socket[] = { SOCK_FL };

    /* Include only 808 nm across the zone. */
    np_hub_status_t rc = resolve_sockets(one_socket, 1, NP_ELEM_BIT(NP_ELEM_LED_808),
                                         false, out, 64, &n);
    check(rc == NP_HUB_OK && n == 1, "include-only LED_808 → 1 element");
    np_physical_loc_t loc;
    check(np_module_map_resolve(out[0], &loc) == NP_HUB_OK && loc.elem_type == NP_ELEM_LED_808,
          "included element really is LED_808");

    /* Exclude the two photodiodes + NTC → leaves the 3 LEDs. */
    uint64_t sensors = NP_ELEM_BIT(NP_ELEM_PD_FORWARD) |
                       NP_ELEM_BIT(NP_ELEM_PD_BACK) |
                       NP_ELEM_BIT(NP_ELEM_NTC);
    rc = resolve_sockets(one_socket, 1, sensors, true, out, 64, &n);
    check(rc == NP_HUB_OK && n == 3, "exclude sensors → 3 LEDs remain");

    /* mask 0 = no filter, in BOTH modes → all elements of the tile. */
    rc = resolve_sockets(one_socket, 1, 0, false, out, 64, &n);
    check(rc == NP_HUB_OK && n == PBM_TILE_N, "mask 0 include → all elements");
    rc = resolve_sockets(one_socket, 1, 0, true, out, 64, &n);
    check(rc == NP_HUB_OK && n == PBM_TILE_N, "mask 0 exclude → still all elements");
}

static void test_group_socket_and_addr_set(void)
{
    populate_all();
    np_hex_addr_t out[64];
    uint16_t n = 0;

    uint16_t sockets[] = { SOCK_FL, SOCK_FR };
    np_group_query_t q;
    memset(&q, 0, sizeof(q));
    q.kind = NP_GROUP_KIND_SOCKET_SET;
    q.sockets = sockets;
    q.socket_count = 2;
    check(np_module_map_resolve_group(&q, out, 64, &n) == NP_HUB_OK, "socket set ok");
    check(n == 2 * PBM_TILE_N, "socket set → both sockets' elements");

    np_hex_addr_t addrs[] = { { SOCK_PL, 0 }, { SOCK_PR, 2 }, { SOCK_UNWIRED, 0 } };
    memset(&q, 0, sizeof(q));
    q.kind = NP_GROUP_KIND_ADDR_SET;
    q.addrs = addrs;
    q.addr_count = 3;
    check(np_module_map_resolve_group(&q, out, 64, &n) == NP_HUB_OK, "addr set ok");
    check(n == 2, "addr set drops the unresolvable (unwired) address");
}

static void test_group_overflow(void)
{
    populate_all();
    np_hex_addr_t out[4];
    uint16_t n = 0;
    /* Both frontal sockets = 12 elements > the 4-entry out[]. */
    const uint16_t frontal_lr[] = { SOCK_FL, SOCK_FR };
    np_hub_status_t rc = resolve_sockets(frontal_lr, 2, 0, false, out, 4, &n);
    check(rc == NP_HUB_ERR_CMD_TOO_MANY, "overflow → CMD_TOO_MANY");
    check(n == 4, "overflow fills exactly max");
}

static void test_nvram_roundtrip(void)
{
    populate_all();
    size_t sz = np_module_map_serialized_size();
    check(sz == NP_HEXMAP_HDR_BYTES + (size_t)N_SOCK * NP_HEXMAP_REC_BYTES + NP_HEXMAP_CRC_BYTES,
          "serialized size = header + records + crc");

    static uint8_t buf[NP_HEXMAP_HDR_BYTES + NP_HEXMAP_MAX_SOCKETS * NP_HEXMAP_REC_BYTES +
                       NP_HEXMAP_CRC_BYTES];
    int wrote = np_module_map_serialize(buf, sizeof(buf));
    check(wrote == (int)sz, "serialize wrote full blob");

    /* Wipe live state (re-init clears records), then load. */
    (void)np_module_map_init(GEOM, N_SOCK);
    np_physical_loc_t loc;
    np_hex_addr_t a0 = { SOCK_TL, 0 };
    check(np_module_map_resolve(a0, &loc) == NP_HUB_ERR_NOT_PRESENT, "records cleared by re-init");

    check(np_module_map_load(buf, (size_t)wrote) == NP_HUB_OK, "load ok");
    check(np_module_map_resolve(a0, &loc) == NP_HUB_OK && loc.elem_type == NP_ELEM_LED_660,
          "loaded inventory resolves");
}

static void test_nvram_reject_bad(void)
{
    populate_all();
    static uint8_t buf[NP_HEXMAP_HDR_BYTES + NP_HEXMAP_MAX_SOCKETS * NP_HEXMAP_REC_BYTES +
                       NP_HEXMAP_CRC_BYTES];
    int wrote = np_module_map_serialize(buf, sizeof(buf));
    check(wrote > 0, "serialize for tamper test");

    uint8_t bad_magic[8] = { 0 };
    memcpy(bad_magic, buf, 8);
    bad_magic[0] ^= 0xFFu;
    check(np_module_map_load(bad_magic, sizeof(bad_magic)) == NP_HUB_ERR_BAD_MAGIC,
          "bad magic rejected");

    /* Corrupt a body byte → CRC mismatch. */
    buf[NP_HEXMAP_HDR_BYTES + NP_HEXMAP_UID_LEN + 2] ^= 0xFFu;  /* flip an elem type */
    check(np_module_map_load(buf, (size_t)wrote) == NP_HUB_ERR_BAD_SIGNATURE,
          "corrupted body → CRC failure");
}

static void test_nvram_hal_persist_restore(void)
{
    g_nvram_written = false;
    populate_all();
    check(np_module_map_persist() == NP_HUB_OK, "persist via HAL ok");
    check(g_nvram_written, "HAL write happened");

    (void)np_module_map_init(GEOM, N_SOCK);   /* wipe live state */
    check(np_module_map_restore() == NP_HUB_OK, "restore via HAL ok");
    np_physical_loc_t loc;
    np_hex_addr_t a = { SOCK_OR, 2 };
    check(np_module_map_resolve(a, &loc) == NP_HUB_OK && loc.elem_type == NP_ELEM_LED_1064,
          "restored inventory resolves occipital-R elem 2");

    /* Restore with nothing stored → HAL not-present propagates. */
    g_nvram_written = false;
    (void)np_module_map_init(GEOM, N_SOCK);
    check(np_module_map_restore() == NP_HUB_ERR_NOT_PRESENT, "restore with empty NVRAM propagates");
}

static void test_placement_check(void)
{
    /* Mixed insertion: PBM tiles everywhere, EEG tiles only at FL and OL. */
    (void)np_module_map_init(GEOM, N_SOCK);
    for (uint16_t s = SOCK_FL; s <= SOCK_OR; s++) {
        (void)plug_pbm(s, (uint8_t)(0x10 + s));
    }
    plug_eeg(SOCK_FL, 0xA0);   /* electrode at frontal-left */
    plug_eeg(SOCK_OL, 0xA1);   /* electrode at occipital-left (stand-in for Oz) */

    const uint64_t ELECTRODE = NP_ELEM_BIT(NP_ELEM_EEG_ELECTRODE) |
                               NP_ELEM_BIT(NP_ELEM_DUAL_ELECTRODE);
    uint8_t  failed[8];
    uint16_t fc = 99;

    /* Safety gate: "Oz" (SOCK_OL) must have an electrode → satisfied. */
    np_placement_req_t oz_ok[] = { { SOCK_OL, ELECTRODE } };
    check(np_module_map_check_placement(oz_ok, 1, failed, 8, &fc) == NP_HUB_OK,
          "placement: electrode present at Oz → OK");
    check(fc == 0, "placement: no failures when satisfied");

    /* Same gate on a PBM-only socket → fails, and reports the socket. */
    np_placement_req_t oz_bad[] = { { SOCK_TR, ELECTRODE } };
    check(np_module_map_check_placement(oz_bad, 1, failed, 8, &fc) == NP_HUB_ERR_NOT_PRESENT,
          "placement: no electrode at PBM socket → NOT_PRESENT");
    check(fc == 1 && failed[0] == SOCK_TR, "placement: failing socket reported");

    /* Dual-rated electrode satisfies BOTH an EEG requirement and a tES requirement. */
    np_placement_req_t as_eeg[] = { { SOCK_FL, NP_ELEM_BIT(NP_ELEM_EEG_ELECTRODE) |
                                                NP_ELEM_BIT(NP_ELEM_DUAL_ELECTRODE) } };
    np_placement_req_t as_tes[] = { { SOCK_FL, NP_ELEM_BIT(NP_ELEM_TES_ELECTRODE) |
                                                NP_ELEM_BIT(NP_ELEM_DUAL_ELECTRODE) } };
    check(np_module_map_check_placement(as_eeg, 1, failed, 8, &fc) == NP_HUB_OK,
          "placement: dual electrode satisfies EEG requirement");
    check(np_module_map_check_placement(as_tes, 1, failed, 8, &fc) == NP_HUB_OK,
          "placement: dual electrode satisfies tES requirement");

    /* tES montage: two sockets required; one lacks an electrode → 1 failure. */
    np_placement_req_t montage[] = {
        { SOCK_FL, ELECTRODE },   /* has dual electrode */
        { SOCK_FR, ELECTRODE },   /* PBM only → fail */
    };
    check(np_module_map_check_placement(montage, 2, failed, 8, &fc) == NP_HUB_ERR_NOT_PRESENT,
          "placement: incomplete tES montage → NOT_PRESENT");
    check(fc == 1 && failed[0] == SOCK_FR, "placement: reports the one missing montage socket");

    /* Empty spec → OK; NULL list still counts failures. */
    check(np_module_map_check_placement(NULL, 0, NULL, 0, &fc) == NP_HUB_OK,
          "placement: empty spec → OK");
    check(np_module_map_check_placement(oz_bad, 1, NULL, 0, &fc) == NP_HUB_ERR_NOT_PRESENT &&
          fc == 1, "placement: NULL list still returns failure + count");
}

/* ── High-socket coverage (64..127) ────────────────────────────────────────────
 * MAX_SOCKETS went 64 -> 128 so all 80 helmet sockets are addressable. The tests
 * above all run on the 9-socket GEOM, which cannot reach the upper half of the
 * major domain. These exercise it directly: resolution, both group kinds,
 * placement (whose socket ids travel through uint8_t), and a full-occupancy
 * NVRAM round-trip at the exact NP_HEXMAP_NVRAM_MAX_BYTES bound.
 *
 * Note the numbering base: firmware socket_id is 0-based, so the 78th 1-based
 * NPPS socket is firmware socket_id 77 — covered explicitly below. */

#define SOCK_HI_FIRST  64    /* first socket unreachable under the old ceiling  */
#define SOCK_NPPS_78   77    /* 1-based NPPS socket 78 == 0-based socket_id 77   */
#define SOCK_HI_LAST   (NP_HEXMAP_MAX_SOCKETS - 1)   /* 127                      */
#define SOCK_HI_PLACE  100   /* arbitrary high socket for placement checks       */

static np_socket_geom_t FULL_GEOM[NP_HEXMAP_MAX_SOCKETS];

/* Every socket wired, with distinct metric coordinates per socket so a
 * field-shift or off-by-one in geometry indexing shows up as a wrong x/y. */
static void full_geom_build(void)
{
    for (uint16_t s = 0; s < NP_HEXMAP_MAX_SOCKETS; s++) {
        FULL_GEOM[s].present_in_helmet = true;
        FULL_GEOM[s].x_mm = (int16_t)s;
        FULL_GEOM[s].y_mm = (int16_t)(1000 + s);
    }
}

static void full_geom_init(void)
{
    full_geom_build();
    (void)np_module_map_init(FULL_GEOM, NP_HEXMAP_MAX_SOCKETS);
}

/* 128 sockets x 6-element tiles = 768 addresses. */
static np_hex_addr_t g_hi_out[NP_HEXMAP_MAX_SOCKETS * 8];

static void test_high_socket_init_and_resolve(void)
{
    full_geom_build();
    check(np_module_map_init(FULL_GEOM, NP_HEXMAP_MAX_SOCKETS) == NP_HUB_OK,
          "init accepts a full 128-socket geometry");

    (void)plug_pbm(SOCK_HI_FIRST, 0x64);
    (void)plug_pbm(SOCK_NPPS_78,  0x4D);
    (void)plug_pbm(SOCK_HI_LAST,  0x7F);

    np_physical_loc_t loc;
    np_hex_addr_t a64 = { SOCK_HI_FIRST, 0 };
    check(np_module_map_resolve(a64, &loc) == NP_HUB_OK, "resolve socket 64 ok");
    check(loc.x_mm == (int16_t)SOCK_HI_FIRST && loc.y_mm == (int16_t)(1000 + SOCK_HI_FIRST),
          "socket 64 resolves to its own geometry row");
    check(loc.elem_type == NP_ELEM_LED_660, "socket 64 elem 0 → LED_660");

    np_hex_addr_t a77 = { SOCK_NPPS_78, 2 };
    check(np_module_map_resolve(a77, &loc) == NP_HUB_OK && loc.elem_type == NP_ELEM_LED_1064,
          "resolve socket 77 (NPPS 1-based socket 78) elem 2 → LED_1064");

    np_hex_addr_t a127 = { SOCK_HI_LAST, 0 };
    check(np_module_map_resolve(a127, &loc) == NP_HUB_OK, "resolve socket 127 ok");
    check(loc.x_mm == (int16_t)SOCK_HI_LAST && loc.y_mm == (int16_t)(1000 + SOCK_HI_LAST),
          "socket 127 resolves to its own geometry row");

    /* A high socket with no module still fails closed. */
    np_hex_addr_t a_empty = { 90, 0 };
    check(np_module_map_resolve(a_empty, &loc) == NP_HUB_ERR_NOT_PRESENT,
          "empty high socket → NOT_PRESENT");
}

static void test_high_socket_groups(void)
{
    full_geom_init();
    (void)plug_pbm(SOCK_HI_FIRST, 0x64);
    (void)plug_pbm(SOCK_HI_LAST,  0x7F);
    uint16_t n = 0;

    /* KIND_SOCKET_SET across the old ceiling. */
    uint16_t sockets[] = { SOCK_HI_FIRST, SOCK_HI_LAST };
    np_group_query_t q;
    memset(&q, 0, sizeof(q));
    q.kind = NP_GROUP_KIND_SOCKET_SET;
    q.sockets = sockets;
    q.socket_count = 2;
    check(np_module_map_resolve_group(&q, g_hi_out, 1024, &n) == NP_HUB_OK,
          "socket-set group over sockets 64 and 127 ok");
    check(n == 2 * PBM_TILE_N, "socket-set group → both high tiles' elements");
    bool any_above_63 = false;
    for (uint16_t i = 0; i < n; i++) {
        if (g_hi_out[i].socket_id > 63) { any_above_63 = true; }
    }
    check(any_above_63, "group results carry socket ids above the old 64 ceiling");

    /* KIND_ADDR_SET on an explicit high address. */
    np_hex_addr_t addrs[] = { { SOCK_HI_LAST, 2 }, { 90, 0 } };  /* 90 = empty */
    memset(&q, 0, sizeof(q));
    q.kind = NP_GROUP_KIND_ADDR_SET;
    q.addrs = addrs;
    q.addr_count = 2;
    check(np_module_map_resolve_group(&q, g_hi_out, 1024, &n) == NP_HUB_OK,
          "addr-set group with high address ok");
    check(n == 1 && g_hi_out[0].socket_id == SOCK_HI_LAST,
          "addr-set keeps socket 127, drops the empty high socket");

    /* A zone whose socket list lies wholly in the upper half. Was driven through
     * the retired predefined occipital-R lobe group (OI-HUB-C14); the properties
     * proven — a group resolving socket id 127, and the type filter applying above
     * the old 64 ceiling — are unchanged.
     *
     * The list deliberately includes sockets 90 and 91, which are WIRED BUT EMPTY in
     * FULL_GEOM. A PARTIALLY-POPULATED ZONE IS THE NORMAL CASE — a Core or Home Lite
     * build populates only a subset of the lattice — and emit_socket() must skip an
     * empty member by returning "continue", not "overflow". The retired lobe query
     * covered this incidentally: it swept the whole geometry for a lobe, so every
     * call pushed unpopulated sockets through emit_socket. Nothing on the socket-set
     * path inherited that, so the members are named explicitly here. Without them,
     * a regression turning that skip into a `false` return would make any protocol
     * targeting a partly-populated zone fail with a spurious CMD_TOO_MANY, and no
     * test would notice. */
    const uint16_t high_zone[] = { 90, 91, SOCK_HI_LAST };
    check(resolve_sockets(high_zone, 3, 0, false, g_hi_out, 1024, &n) == NP_HUB_OK,
          "zone over high sockets ok, empty members skipped not treated as overflow");
    check(n == PBM_TILE_N, "high zone → the one populated high tile");
    check(g_hi_out[0].socket_id == SOCK_HI_LAST,
          "group resolved a socket id of 127");

    /* Type filter still applies in the upper half. */
    check(resolve_sockets(high_zone, 3, NP_ELEM_BIT(NP_ELEM_LED_808), false,
                          g_hi_out, 1024, &n) == NP_HUB_OK && n == 1,
          "type filter on high socket → 1 element");
}

static void test_high_socket_placement(void)
{
    full_geom_init();
    plug_eeg(SOCK_HI_PLACE, 0xB0);      /* dual-rated electrode tile at socket 100 */
    (void)plug_pbm(SOCK_HI_LAST, 0x7F); /* PBM only at socket 127                  */

    const uint64_t ELECTRODE = NP_ELEM_BIT(NP_ELEM_EEG_ELECTRODE) |
                               NP_ELEM_BIT(NP_ELEM_DUAL_ELECTRODE);
    uint8_t  failed[8];
    uint16_t fc = 99;

    np_placement_req_t ok_req[] = { { SOCK_HI_PLACE, ELECTRODE } };
    check(np_module_map_check_placement(ok_req, 1, failed, 8, &fc) == NP_HUB_OK,
          "placement satisfied at socket 100");
    check(fc == 0, "placement: no failures at high socket when satisfied");

    /* Failure path: the reported socket id must survive the uint8_t out-param. */
    np_placement_req_t bad_req[] = { { SOCK_HI_LAST, ELECTRODE } };
    check(np_module_map_check_placement(bad_req, 1, failed, 8, &fc) == NP_HUB_ERR_NOT_PRESENT,
          "placement unsatisfied at socket 127 → NOT_PRESENT");
    check(fc == 1 && failed[0] == SOCK_HI_LAST,
          "placement reports failing socket id 127 without truncation");
}

static void test_nvram_full_occupancy_roundtrip(void)
{
    full_geom_init();
    for (uint16_t s = 0; s < NP_HEXMAP_MAX_SOCKETS; s++) {
        (void)plug_pbm(s, (uint8_t)(s + 1u));   /* seed 0 would be a zero-ish UID */
    }

    size_t sz = np_module_map_serialized_size();
    check(sz == NP_HEXMAP_NVRAM_MAX_BYTES,
          "full-occupancy serialized size == NP_HEXMAP_NVRAM_MAX_BYTES");
    check(sz == 22412u, "NP_HEXMAP_NVRAM_MAX_BYTES is 22412 bytes at 128 sockets (v3: +36B cal/record)");

    static uint8_t buf[NP_HEXMAP_NVRAM_MAX_BYTES];
    int wrote = np_module_map_serialize(buf, sizeof(buf));
    check(wrote == (int)sz, "serialize wrote the full 128-socket blob");

    full_geom_init();                            /* wipe live state */
    np_physical_loc_t loc;
    np_hex_addr_t a = { SOCK_HI_LAST, 0 };
    check(np_module_map_resolve(a, &loc) == NP_HUB_ERR_NOT_PRESENT,
          "high-socket records cleared by re-init");

    check(np_module_map_load(buf, (size_t)wrote) == NP_HUB_OK,
          "load full 128-socket blob ok");
    check(np_module_map_resolve(a, &loc) == NP_HUB_OK && loc.elem_type == NP_ELEM_LED_660,
          "socket 127 survives the NVRAM round-trip");
    np_hex_addr_t a64 = { SOCK_HI_FIRST, 2 };
    check(np_module_map_resolve(a64, &loc) == NP_HUB_OK && loc.elem_type == NP_ELEM_LED_1064,
          "socket 64 survives the NVRAM round-trip");

    /* And through the HAL, at full occupancy. */
    g_nvram_written = false;
    check(np_module_map_persist() == NP_HUB_OK, "persist full-occupancy blob via HAL");
    check(g_nvram_len == NP_HEXMAP_NVRAM_MAX_BYTES, "HAL received the full 22412-byte blob");
    full_geom_init();
    check(np_module_map_restore() == NP_HUB_OK, "restore full-occupancy blob via HAL");
    check(np_module_map_resolve(a, &loc) == NP_HUB_OK,
          "socket 127 resolves after HAL restore");
}

/* A v1 (64-socket-era) blob must be REJECTED, not migrated and not misread.
 * load() checks version before CRC, so flipping the version field alone is a
 * faithful stand-in for an old blob. */
static void test_nvram_rejects_old_version(void)
{
    full_geom_init();
    (void)plug_pbm(SOCK_HI_LAST, 0x7F);
    static uint8_t buf[NP_HEXMAP_NVRAM_MAX_BYTES];
    int wrote = np_module_map_serialize(buf, sizeof(buf));
    check(wrote > 0, "serialize for version test");

    check(buf[4] == 0x03u && buf[5] == 0x00u, "serialized blob carries version 0x0003");

    buf[4] = 0x02u;  buf[5] = 0x00u;            /* pretend it is the old v2 blob */
    check(np_module_map_load(buf, (size_t)wrote) == NP_HUB_ERR_BAD_VERSION,
          "v2 blob rejected with BAD_VERSION (v3 records are 36B longer; "
          "read as v3 a v2 blob hands each socket its neighbour's UID)");

    /* Rejection happens before any record is touched, so live state is intact —
     * load() is all-or-nothing, never a partial merge of a foreign layout. */
    np_physical_loc_t loc;
    np_hex_addr_t a = { SOCK_HI_LAST, 0 };
    check(np_module_map_resolve(a, &loc) == NP_HUB_OK,
          "rejected v1 blob did not clobber live inventory");

    /* The discard-and-rebuild path in situ: a v1 blob in NVRAM makes restore()
     * propagate BAD_VERSION, and a freshly-initialised map stays empty until the
     * hub re-polls every socket. No migration code runs. */
    check(np_hexmap_nvram_write(buf, (size_t)wrote) == NP_HUB_OK, "stage v1 blob in NVRAM");

    /* The map MUST be populated before restore(), otherwise the final assertion
     * is tautological: full_geom_init() memsets s_map, so an empty map after a
     * failed restore() would prove nothing about restore() at all. Populate
     * first, so "empty afterwards" is a real claim about the failure path. */
    full_geom_init();
    (void)plug_pbm(SOCK_HI_LAST, 0x7F);
    check(np_module_map_resolve(a, &loc) == NP_HUB_OK, "inventory live before restore");

    check(np_module_map_restore() == NP_HUB_ERR_BAD_VERSION,
          "restore over a v1 blob propagates BAD_VERSION");
    check(np_module_map_resolve(a, &loc) == NP_HUB_ERR_NOT_PRESENT,
          "failed restore clears stale inventory → rebuilt by polling");
}

/* ── Packed-address validation (out-of-domain must fail, not alias) ──────────── */

static void test_addr_validation(void)
{
    np_hex_addr_t max_ok = { NP_HEXMAP_MAX_SOCKETS - 1, NP_HEXMAP_MAX_ELEMENTS - 1 };
    check(np_hex_addr_valid(max_ok), "valid: 127:127 in domain");

    np_hex_addr_t bad_sock = { NP_HEXMAP_MAX_SOCKETS, 0 };        /* 128 */
    np_hex_addr_t bad_elem = { 0, NP_HEXMAP_MAX_ELEMENTS };       /* 128 */
    check(!np_hex_addr_valid(bad_sock), "valid: socket 128 out of domain");
    check(!np_hex_addr_valid(bad_elem), "valid: element 128 out of domain");

    /* The whole point: 128 must NOT silently become socket 0. */
    check(np_hex_addr_pack(bad_sock) == NP_HEX_ADDR_INVALID,
          "pack socket 128 → INVALID (not aliased to socket 0)");
    check(np_hex_addr_pack(bad_elem) == NP_HEX_ADDR_INVALID, "pack element 128 → INVALID");
    np_hex_addr_t worst = { 255, 255 };
    check(np_hex_addr_pack(worst) == NP_HEX_ADDR_INVALID, "pack 255:255 → INVALID");

    /* Valid packs stay well clear of the sentinel. */
    check(np_hex_addr_pack(max_ok) == NP_HEX_ADDR_MAX, "pack 127:127 → NP_HEX_ADDR_MAX");
    check(NP_HEX_ADDR_MAX < NP_HEX_ADDR_INVALID, "sentinel is out of band");

    /* Unpacking the sentinel yields an address no geometry can resolve. */
    np_hex_addr_t u = np_hex_addr_unpack(NP_HEX_ADDR_INVALID);
    check(u.socket_id == NP_HEX_SOCKET_INVALID, "unpack INVALID → sentinel socket");
    check(u.socket_id >= NP_HEXMAP_MAX_SOCKETS, "sentinel socket is outside every domain");

    full_geom_init();                       /* widest possible geometry, 128 sockets */
    (void)plug_pbm(0, 0x11);
    np_physical_loc_t loc;
    check(np_module_map_resolve(u, &loc) == NP_HUB_ERR_NOT_PRESENT,
          "sentinel address fails closed at resolve, even at n_sockets=128");
}

/* ── Zone membership: inclusive by default, explicit dis-include overrides ─────
 * The authoring rule lives in protocols/predefined/00-zones.npps: a socket falling
 * even partially within a zone is IN it unless a protocol dis-includes it by
 * authoring a narrower zone. Midline sockets straddle, so the zone file lists each
 * in BOTH hemisphere zones of its lobe.
 *
 * Firmware does not evaluate that rule — it receives its RESULT as a socket list
 * (OI-HUB-C14 retired the lobe filter). What these tests hold firmware to is the
 * consequence: the socket lists the zone file produces must resolve correctly, and
 * a union of two overlapping zones must not drive the shared socket twice. */

enum { MG_L = 0, MG_MID, MG_R, MG_PAR_L, MG_N };

static const np_socket_geom_t MID_GEOM[MG_N] = {
    { true, -30, -80 },
    { true,   0, -80 },   /* the midline socket — straddles, so it is in both zones */
    { true,  30, -80 },
    { true, -25,  40 },
};

/* The socket lists 00-zones.npps yields for a lobe's two hemisphere zones. The
 * midline socket appears in BOTH — that overlap is the point. */
static const uint16_t ZONE_FRONTAL_L[] = { MG_L, MG_MID };
static const uint16_t ZONE_FRONTAL_R[] = { MG_R, MG_MID };
static const uint16_t ZONE_PARIETAL_L[] = { MG_PAR_L };

static void mid_geom_populate(void)
{
    (void)np_module_map_init(MID_GEOM, MG_N);
    for (uint16_t s = 0; s < MG_N; s++) {
        (void)plug_pbm(s, (uint8_t)(0xC0 + s));
    }
}

static bool out_contains_socket(const np_hex_addr_t *o, uint16_t n, uint8_t socket)
{
    for (uint16_t i = 0; i < n; i++) {
        if (o[i].socket_id == socket) { return true; }
    }
    return false;
}

static void test_midline_included_in_both_hemispheres(void)
{
    mid_geom_populate();
    uint16_t n = 0;

    check(resolve_sockets(ZONE_FRONTAL_L, 2, 0, false, g_hi_out, 1024, &n) == NP_HUB_OK,
          "frontal-L zone list with a midline socket ok");
    check(n == 2 * PBM_TILE_N, "frontal-L → left socket + midline socket");
    check(out_contains_socket(g_hi_out, n, MG_MID), "midline socket IS in frontal-LEFT");
    check(!out_contains_socket(g_hi_out, n, MG_R), "frontal-L excludes the right socket");

    check(resolve_sockets(ZONE_FRONTAL_R, 2, 0, false, g_hi_out, 1024, &n) == NP_HUB_OK,
          "frontal-R zone list with a midline socket ok");
    check(n == 2 * PBM_TILE_N, "frontal-R → right socket + midline socket");
    check(out_contains_socket(g_hi_out, n, MG_MID), "midline socket IS in frontal-RIGHT");
    check(!out_contains_socket(g_hi_out, n, MG_L), "frontal-R excludes the left socket");

    /* Both hemispheres together — the whole lobe, each socket exactly once. */
    const uint16_t both[] = { MG_L, MG_MID, MG_R, MG_MID };
    check(resolve_sockets(both, 4, 0, false, g_hi_out, 1024, &n) == NP_HUB_OK &&
          n == 3 * PBM_TILE_N, "both hemispheres → all three frontal sockets, once each");

    /* A zone that does not list the midline socket does not get it: membership is
     * exactly what the zone file said, with no firmware inference on top. */
    check(resolve_sockets(ZONE_PARIETAL_L, 1, 0, false, g_hi_out, 1024, &n) == NP_HUB_OK &&
          n == PBM_TILE_N,
          "parietal-L unaffected by the frontal midline socket");
    check(!out_contains_socket(g_hi_out, n, MG_MID),
          "a zone that omits the midline socket never receives it");
}

/* How many entries in out[] name this socket? */
static uint16_t count_socket(const np_hex_addr_t *o, uint16_t n, uint8_t socket)
{
    uint16_t c = 0;
    for (uint16_t i = 0; i < n; i++) {
        if (o[i].socket_id == socket) { c++; }
    }
    return c;
}

/* Dedup. Overlapping zones are the normal case under inclusive membership, so a
 * union naming a midline socket twice must still drive it ONCE — otherwise that
 * module takes double J/cm² in a single session. Models the real shape of
 * clinical-04-pbm-depression: zones ["Frontal Left", "Frontal Right"], whose
 * socket lists both contain the frontal midline sockets. */
static void test_overlapping_zone_union_dedups(void)
{
    mid_geom_populate();
    uint16_t n = 0;
    np_group_query_t q;

    /* The union a caller builds from two overlapping zone lists: the midline
     * socket appears once per zone. */
    const uint16_t union_lr[] = { MG_L, MG_MID,      /* "Frontal Left"  */
                                  MG_R, MG_MID };    /* "Frontal Right" */
    memset(&q, 0, sizeof(q));
    q.kind = NP_GROUP_KIND_SOCKET_SET;
    q.sockets = union_lr;
    q.socket_count = 4;
    check(np_module_map_resolve_group(&q, g_hi_out, 1024, &n) == NP_HUB_OK,
          "union of two overlapping zones resolves");
    check(n == 3 * PBM_TILE_N, "union → three distinct sockets, not four entries");
    check(count_socket(g_hi_out, n, MG_MID) == PBM_TILE_N,
          "midline socket emitted exactly once (no double dose)");
    check(count_socket(g_hi_out, n, MG_L) == PBM_TILE_N, "left socket emitted once");
    check(count_socket(g_hi_out, n, MG_R) == PBM_TILE_N, "right socket emitted once");

    /* Degenerate repeat of a single socket collapses too. */
    const uint16_t same_four[] = { MG_MID, MG_MID, MG_MID, MG_MID };
    q.sockets = same_four;
    q.socket_count = 4;
    check(np_module_map_resolve_group(&q, g_hi_out, 1024, &n) == NP_HUB_OK &&
          n == PBM_TILE_N, "socket repeated 4x → emitted once");

    /* ADDR_SET dedups per (socket:element), not per socket: two different
     * elements of one socket are distinct and both survive. */
    const np_hex_addr_t addrs[] = {
        { MG_MID, 0 }, { MG_MID, 0 },      /* exact duplicate → one entry  */
        { MG_MID, 1 },                     /* same socket, other element   */
        { MG_L,   0 }, { MG_L,   0 },      /* exact duplicate → one entry  */
    };
    memset(&q, 0, sizeof(q));
    q.kind = NP_GROUP_KIND_ADDR_SET;
    q.addrs = addrs;
    q.addr_count = 5;
    check(np_module_map_resolve_group(&q, g_hi_out, 1024, &n) == NP_HUB_OK && n == 3,
          "addr-set dedups exact duplicates, keeps distinct elements");
    check(count_socket(g_hi_out, n, MG_MID) == 2,
          "two distinct elements of the midline socket both kept");

    /* Dedup is per-resolve, not per-list-position: the same union resolved again
     * gives the same single-emission result, so no `seen` state leaks between calls. */
    memset(&q, 0, sizeof(q));
    q.kind = NP_GROUP_KIND_SOCKET_SET;
    q.sockets = union_lr;
    q.socket_count = 4;
    check(np_module_map_resolve_group(&q, g_hi_out, 1024, &n) == NP_HUB_OK &&
          n == 3 * PBM_TILE_N &&
          count_socket(g_hi_out, n, MG_MID) == PBM_TILE_N,
          "re-resolving the same union still emits the midline socket once");
}


/* ── UID-keyed PBM calibration (OI-HUB-C06) ───────────────────────────────────
 *
 * The property under test is that calibration follows the MODULE, not the
 * socket. Every one of these would still pass against a socket-keyed store
 * EXCEPT test_cal_follows_module_across_sockets and
 * test_cal_cleared_when_occupant_changes — those two are the ones that bite,
 * and each was confirmed to fail against a deliberately socket-keyed
 * find_by_uid before being trusted.
 * ────────────────────────────────────────────────────────────────────────── */

static void fill_cal(float cal[NP_HEXMAP_CAL_FLOATS], float base)
{
    for (unsigned i = 0; i < NP_HEXMAP_CAL_FLOATS; i++) {
        cal[i] = base + (float)i * 0.001f;
    }
}

static void test_cal_set_get_by_uid(void)
{
    full_geom_init();
    (void)plug_pbm(7, 0x11);

    float in[NP_HEXMAP_CAL_FLOATS];
    fill_cal(in, 0.120f);
    np_module_uid_t u = uid_of(0x11);

    check(np_module_map_set_cal(&u, in) == NP_HUB_OK, "cal: set by UID succeeds");

    float out[NP_HEXMAP_CAL_FLOATS] = { 0 };
    check(np_module_map_get_cal(&u, out) == NP_HUB_OK, "cal: get by UID succeeds");
    int same = 1;
    for (unsigned i = 0; i < NP_HEXMAP_CAL_FLOATS; i++) {
        if (out[i] != in[i]) { same = 0; }
    }
    check(same, "cal: all 9 coefficients round-trip exactly");
}

static void test_cal_unknown_uid_fails_closed(void)
{
    full_geom_init();
    (void)plug_pbm(7, 0x11);

    float in[NP_HEXMAP_CAL_FLOATS];
    fill_cal(in, 0.120f);
    np_module_uid_t known = uid_of(0x11);
    (void)np_module_map_set_cal(&known, in);

    /* A module the helmet has never seen must NOT inherit the calibration of
     * the module that is present. This is the defect OI-HUB-C06 exists for. */
    np_module_uid_t stranger = uid_of(0x22);
    float out[NP_HEXMAP_CAL_FLOATS];
    for (unsigned i = 0; i < NP_HEXMAP_CAL_FLOATS; i++) { out[i] = -1.0f; }

    check(np_module_map_get_cal(&stranger, out) == NP_HUB_ERR_NOT_PRESENT,
          "cal: unknown UID returns NOT_PRESENT, never another module's data");
    int untouched = 1;
    for (unsigned i = 0; i < NP_HEXMAP_CAL_FLOATS; i++) {
        if (out[i] != -1.0f) { untouched = 0; }
    }
    check(untouched, "cal: failed get writes nothing into the caller's buffer");

    np_module_uid_t zero;
    memset(&zero, 0, sizeof(zero));
    check(np_module_map_get_cal(&zero, out) == NP_HUB_ERR_NOT_PRESENT,
          "cal: zero UID (empty socket) never resolves to a record");
    check(np_module_map_set_cal(&zero, in) == NP_HUB_ERR_INVALID_ARG,
          "cal: cannot store against the zero UID");
}

static void test_cal_present_module_without_record(void)
{
    full_geom_init();
    (void)plug_pbm(9, 0x33);
    np_module_uid_t u = uid_of(0x33);
    float out[NP_HEXMAP_CAL_FLOATS];
    check(np_module_map_get_cal(&u, out) == NP_HUB_ERR_NOT_PRESENT,
          "cal: a present module with no factory record reports NOT_PRESENT "
          "(caller falls back to defaults + NP_CAL_DEFAULT)");
}

static void test_cal_rejects_implausible_values(void)
{
    full_geom_init();
    (void)plug_pbm(3, 0x44);
    np_module_uid_t u = uid_of(0x44);

    float bad[NP_HEXMAP_CAL_FLOATS];
    fill_cal(bad, 0.120f);

    bad[4] = 0.0f;
    check(np_module_map_set_cal(&u, bad) == NP_HUB_ERR_INVALID_ARG,
          "cal: zero coefficient rejected (would collide with 'absent')");
    bad[4] = -0.5f;
    check(np_module_map_set_cal(&u, bad) == NP_HUB_ERR_INVALID_ARG,
          "cal: negative coefficient rejected");

    float out[NP_HEXMAP_CAL_FLOATS];
    check(np_module_map_get_cal(&u, out) == NP_HUB_ERR_NOT_PRESENT,
          "cal: a rejected set stores nothing at all");
}

static void test_cal_follows_module_across_sockets(void)
{
    /* THE test. Same physical module, different hole. A socket-keyed store
     * fails here — it would answer for socket 12 and not for socket 60. */
    full_geom_init();
    (void)plug_pbm(12, 0x55);

    float in[NP_HEXMAP_CAL_FLOATS];
    fill_cal(in, 0.088f);
    np_module_uid_t u = uid_of(0x55);
    check(np_module_map_set_cal(&u, in) == NP_HUB_OK, "cal: stored at socket 12");

    /* Unplug from 12, plug the same UID into 60. */
    np_module_uid_t zero;
    memset(&zero, 0, sizeof(zero));
    bool changed = false;
    (void)np_module_map_apply_poll(12, &zero, 0x00, inv_cb, NULL, &changed);
    (void)plug_pbm(60, 0x55);

    float out[NP_HEXMAP_CAL_FLOATS] = { 0 };
    check(np_module_map_get_cal(&u, out) == NP_HUB_ERR_NOT_PRESENT,
          "cal: record does not survive an unplug (hub cache is per-occupancy; "
          "re-supplied at factory-cal read, never inferred from the socket)");

    /* Re-store against the module at its new socket and confirm it is keyed to
     * the UID, not to 60. */
    check(np_module_map_set_cal(&u, in) == NP_HUB_OK, "cal: re-stored at socket 60");
    check(np_module_map_get_cal(&u, out) == NP_HUB_OK,
          "cal: same UID resolves at its new socket");
    check(out[0] == in[0], "cal: coefficients are the module's, at either socket");
}

static void test_cal_cleared_when_occupant_changes(void)
{
    /* A different module in the same socket must NOT inherit the previous
     * occupant's coefficients — the exact silent-wrong-calibration path. */
    full_geom_init();
    (void)plug_pbm(20, 0x66);

    float in[NP_HEXMAP_CAL_FLOATS];
    fill_cal(in, 0.105f);
    np_module_uid_t old_mod = uid_of(0x66);
    check(np_module_map_set_cal(&old_mod, in) == NP_HUB_OK, "cal: stored for old module");

    (void)plug_pbm(20, 0x77);              /* swap: new UID, same socket */

    np_module_uid_t new_mod = uid_of(0x77);
    float out[NP_HEXMAP_CAL_FLOATS];
    for (unsigned i = 0; i < NP_HEXMAP_CAL_FLOATS; i++) { out[i] = -1.0f; }
    check(np_module_map_get_cal(&new_mod, out) == NP_HUB_ERR_NOT_PRESENT,
          "cal: a swapped-in module does NOT inherit the previous occupant's "
          "calibration (OI-HUB-C06 / NP-HW-HUB-001 Rev 3 §9.5)");
    check(np_module_map_get_cal(&old_mod, out) == NP_HUB_ERR_NOT_PRESENT,
          "cal: the departed module's record is gone with it");
}

static void test_cal_survives_nvram_roundtrip(void)
{
    full_geom_init();
    (void)plug_pbm(5, 0x88);
    float in[NP_HEXMAP_CAL_FLOATS];
    fill_cal(in, 0.120f);
    np_module_uid_t u = uid_of(0x88);
    (void)np_module_map_set_cal(&u, in);

    static uint8_t buf[NP_HEXMAP_NVRAM_MAX_BYTES];
    int wrote = np_module_map_serialize(buf, sizeof(buf));
    check(wrote > 0, "cal: serialize with calibration payload");

    full_geom_init();                       /* wipe live state */
    check(np_module_map_load(buf, (size_t)wrote) == NP_HUB_OK, "cal: load blob back");

    float out[NP_HEXMAP_CAL_FLOATS] = { 0 };
    check(np_module_map_get_cal(&u, out) == NP_HUB_OK,
          "cal: record survives the NVRAM round-trip");
    int same = 1;
    for (unsigned i = 0; i < NP_HEXMAP_CAL_FLOATS; i++) {
        if (out[i] != in[i]) { same = 0; }
    }
    check(same, "cal: every coefficient survives the float round-trip bit-exactly");
}

static void test_socket_uid_lookup(void)
{
    full_geom_init();
    (void)plug_pbm(31, 0x99);

    np_module_uid_t got;
    check(np_module_map_socket_uid(31, &got) == NP_HUB_OK,
          "socket_uid: occupied socket resolves");
    np_module_uid_t want = uid_of(0x99);
    check(np_module_uid_equal(&got, &want), "socket_uid: returns the occupant's UID");

    check(np_module_map_socket_uid(32, &got) == NP_HUB_ERR_NOT_PRESENT,
          "socket_uid: empty socket fails closed (session then uses defaults)");
    check(np_module_map_socket_uid(NP_HEXMAP_MAX_SOCKETS, &got) == NP_HUB_ERR_INVALID_ARG,
          "socket_uid: out-of-domain socket rejected");
}

static void test_cal_length_matches_pbm_library(void)
{
    /* NP_PBM_MODULE_UID_LEN mirrors NP_HEXMAP_UID_LEN by comment on both
     * sides, because the link direction forbids a shared include. This is the
     * one translation unit that sees both, so it is where the mirror is pinned. */
    check(NP_PBM_MODULE_UID_LEN == NP_HEXMAP_UID_LEN,
          "pbm NP_PBM_MODULE_UID_LEN == hub NP_HEXMAP_UID_LEN");
    check(sizeof(np_pbm_module_uid_t) == sizeof(np_module_uid_t),
          "pbm np_pbm_module_uid_t and hub np_module_uid_t are the same size");
    check(NP_HEXMAP_CAL_FLOATS == (NP_PBM_WL_COUNT * 3u),
          "cal payload is exactly WL_COUNT x {K_PD1, K_PD2, K_ratio_nom}");
}
int main(void)
{
    test_addr_pack();
    test_init();
    test_resolve_empty();
    test_poll_new_and_resolve();
    test_poll_same_uid_no_reinventory();
    test_poll_uid_change_reinventory();
    test_poll_removal();
    test_poll_invalid();
    test_poll_fail_closed_overflow();
    test_poll_new_null_cb_fail_closed();
    test_group_zone_socket_set();
    test_group_invalid_kind_fails_closed();
    test_group_type_filter();
    test_group_socket_and_addr_set();
    test_group_overflow();
    test_nvram_roundtrip();
    test_nvram_reject_bad();
    test_nvram_hal_persist_restore();
    test_placement_check();

    /* Upper half of the major domain (64..127), unreachable before 64 -> 128. */
    test_high_socket_init_and_resolve();
    test_high_socket_groups();
    test_high_socket_placement();
    test_nvram_full_occupancy_roundtrip();
    test_nvram_rejects_old_version();

    /* Out-of-domain addresses must fail, not alias onto a valid socket. */
    test_addr_validation();

    /* Inclusive zone membership + the protocol dis-include override. */
    test_midline_included_in_both_hemispheres();
    test_overlapping_zone_union_dedups();

    /* UID-keyed PBM dose-metering calibration (OI-HUB-C06). */
    test_cal_set_get_by_uid();
    test_cal_unknown_uid_fails_closed();
    test_cal_present_module_without_record();
    test_cal_rejects_implausible_values();
    test_cal_follows_module_across_sockets();
    test_cal_cleared_when_occupant_changes();
    test_cal_survives_nvram_roundtrip();
    test_socket_uid_lookup();
    test_cal_length_matches_pbm_library();

    if (g_failures == 0) {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d TEST(S) FAILED\n", g_failures);
    return 1;
}
