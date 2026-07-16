/*
 * NeurOne Hub Control — Hexagonal Zone-Module Map Host Tests
 * Design study: hexagonal zone-module redesign (2026-07-15 session).
 *
 * Exercises the 2-level (socket:element) addressing, the power-on poll /
 * UID-change inventory cache, address→physical resolution, group resolution
 * (predefined lobe groups, socket sets, address sets, type include/exclude),
 * and NVRAM serialize/load + persist/restore via injected HAL stubs.
 *
 * No FreeRTOS, no hardware. IEC 62304 Class B — SW-02 hub control.
 * Build with -DNP_BUILD_TESTS=ON (see firmware/hub_control/CMakeLists.txt).
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../include/np_module_map.h"

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

/* ── Test helmet geometry: 8 lobe/side sockets + 1 unwired socket (index 8) ──── */

enum { SOCK_FL = 0, SOCK_FR, SOCK_TL, SOCK_TR, SOCK_PL, SOCK_PR, SOCK_OL, SOCK_OR,
       SOCK_UNWIRED, N_SOCK };

static const np_socket_geom_t GEOM[N_SOCK] = {
    { true,  NP_LOBE_FRONTAL,   NP_SIDE_LEFT,  -30,  -80 },
    { true,  NP_LOBE_FRONTAL,   NP_SIDE_RIGHT,  30,  -80 },
    { true,  NP_LOBE_TEMPORAL,  NP_SIDE_LEFT,  -70,    0 },
    { true,  NP_LOBE_TEMPORAL,  NP_SIDE_RIGHT,  70,    0 },
    { true,  NP_LOBE_PARIETAL,  NP_SIDE_LEFT,  -25,   40 },
    { true,  NP_LOBE_PARIETAL,  NP_SIDE_RIGHT,  25,   40 },
    { true,  NP_LOBE_OCCIPITAL, NP_SIDE_LEFT,  -25,  110 },
    { true,  NP_LOBE_OCCIPITAL, NP_SIDE_RIGHT,  25,  110 },
    { false, NP_LOBE_NONE,      NP_SIDE_MIDLINE, 0,    0 },  /* not wired */
};

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
    np_hex_addr_t a = { 63, 127 };
    uint16_t v = np_hex_addr_pack(a);
    np_hex_addr_t b = np_hex_addr_unpack(v);
    check(b.socket_id == 63 && b.element_id == 127, "addr pack/unpack round-trip max");

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
    check(loc.lobe == NP_LOBE_FRONTAL && loc.side == NP_SIDE_LEFT,
          "resolve → frontal-left geometry");
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

static void test_group_predefined(void)
{
    populate_all();
    np_hex_addr_t out[64];
    uint16_t n = 0;
    check(np_module_map_predefined(NP_PGROUP_FRONTAL_L, 0, false, out, 64, &n) == NP_HUB_OK,
          "predefined frontal-L ok");
    check(n == PBM_TILE_N, "frontal-L returns one tile's worth of elements");
    bool all_fl = true;
    for (uint16_t i = 0; i < n; i++) {
        if (out[i].socket_id != SOCK_FL) { all_fl = false; }
    }
    check(all_fl, "frontal-L addresses all from SOCK_FL");

    /* Occipital wildcard side = both hemispheres. */
    np_group_query_t q;
    memset(&q, 0, sizeof(q));
    q.kind = NP_GROUP_KIND_LOBE;
    q.lobe = NP_LOBE_OCCIPITAL;
    q.side = NP_SIDE_MIDLINE;
    check(np_module_map_resolve_group(&q, out, 64, &n) == NP_HUB_OK, "occipital wildcard ok");
    check(n == 2 * PBM_TILE_N, "occipital MIDLINE query → both L and R");
}

static void test_group_type_filter(void)
{
    populate_all();
    np_hex_addr_t out[64];
    uint16_t n = 0;

    /* Include only 808 nm across frontal-left. */
    np_hub_status_t rc = np_module_map_predefined(
        NP_PGROUP_FRONTAL_L, NP_ELEM_BIT(NP_ELEM_LED_808), false, out, 64, &n);
    check(rc == NP_HUB_OK && n == 1, "include-only LED_808 → 1 element");
    np_physical_loc_t loc;
    check(np_module_map_resolve(out[0], &loc) == NP_HUB_OK && loc.elem_type == NP_ELEM_LED_808,
          "included element really is LED_808");

    /* Exclude the two photodiodes + NTC → leaves the 3 LEDs. */
    uint64_t sensors = NP_ELEM_BIT(NP_ELEM_PD_FORWARD) |
                       NP_ELEM_BIT(NP_ELEM_PD_BACK) |
                       NP_ELEM_BIT(NP_ELEM_NTC);
    rc = np_module_map_predefined(NP_PGROUP_FRONTAL_L, sensors, true, out, 64, &n);
    check(rc == NP_HUB_OK && n == 3, "exclude sensors → 3 LEDs remain");

    /* mask 0 = no filter, in BOTH modes → all elements of the tile. */
    rc = np_module_map_predefined(NP_PGROUP_FRONTAL_L, 0, false, out, 64, &n);
    check(rc == NP_HUB_OK && n == PBM_TILE_N, "mask 0 include → all elements");
    rc = np_module_map_predefined(NP_PGROUP_FRONTAL_L, 0, true, out, 64, &n);
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
    np_group_query_t q;
    memset(&q, 0, sizeof(q));
    q.kind = NP_GROUP_KIND_LOBE;
    q.lobe = NP_LOBE_FRONTAL;
    q.side = NP_SIDE_MIDLINE;      /* both frontal sockets = 12 elements > 4 */
    np_hub_status_t rc = np_module_map_resolve_group(&q, out, 4, &n);
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
    test_group_predefined();
    test_group_type_filter();
    test_group_socket_and_addr_set();
    test_group_overflow();
    test_nvram_roundtrip();
    test_nvram_reject_bad();
    test_nvram_hal_persist_restore();
    test_placement_check();

    if (g_failures == 0) {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d TEST(S) FAILED\n", g_failures);
    return 1;
}
