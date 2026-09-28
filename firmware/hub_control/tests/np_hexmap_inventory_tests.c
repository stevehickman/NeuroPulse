/*
 * NeurOne Hub Control — hex-tile identity + inventory host tests (OI-HEXMAP-02)
 * Document: NP-HEX-ZM-001 Rev 9 §4.1
 *
 * Links the real np_module_map.c and np_hexmap_inventory.c. The socket I2C
 * seam (np_pbm_hal_i2c_read) is a simulated tile per socket: a 256-byte
 * register space holding the identity block, with fault injection. The NVRAM
 * HAL is an in-memory double, so a restored record can be confirmed by the
 * real poll.
 *
 * What the tests hold the unit to:
 *   - a well-formed tile is inventoried on first sight and never again while
 *     its UID stays (the only reason the cache exists);
 *   - every failure leaves the socket EMPTY, including one that held a module;
 *   - an inventory whose block names another UID than the header the poll
 *     read is refused (a swap between the two reads);
 *   - unknown element types reject the module whole;
 *   - a record restored from npmp.bin becomes usable through this poll.
 *
 * No FreeRTOS, no hardware. IEC 62304 Class B — SW-02 hub control.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "np_hexmap_inventory.h"
#include "np_module_map.h"
#include "np_pbm_hal.h"
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

/* ── Simulated tiles ──────────────────────────────────────────────────────────── */

enum { N_SOCK = 6, SOCK_UNWIRED = 5 };

static const np_socket_geom_t GEOM[N_SOCK] = {
    { true, -30, -80 }, { true, 30, -80 }, { true, -70, 0 },
    { true,  70,   0 }, { true, -25, 40 }, { false, 0, 0 },
};

typedef struct {
    bool    present;
    uint8_t regs[256];
    int     fail_on_len;       /* fail any read of this length (-1 = never)   */
    /* Swap on the Nth identity read: after `swap_after` reads of the ID
     * register, the tile's register space becomes swap_regs. */
    int     swap_after;
    uint8_t swap_regs[256];
} sim_tile_t;

static sim_tile_t g_tile[N_SOCK];
static int        g_id_reads[N_SOCK];     /* reads at NP_HEXMAP_ID_REG      */
static int        g_full_reads[N_SOCK];   /* of those, whole-block reads    */

np_pbm_status_t np_pbm_hal_i2c_read(uint8_t slot, uint8_t reg_addr,
                                    uint8_t *data, uint8_t len)
{
    if (slot >= N_SOCK || data == NULL) {
        return NP_PBM_ERR_I2C_READ;
    }
    sim_tile_t *t = &g_tile[slot];
    if (!t->present || (int)len == t->fail_on_len ||
        (unsigned)reg_addr + len > 256u) {
        return NP_PBM_ERR_I2C_READ;
    }
    if (reg_addr == NP_HEXMAP_ID_REG) {
        if (t->swap_after >= 0 && g_id_reads[slot] == t->swap_after) {
            memcpy(t->regs, t->swap_regs, sizeof(t->regs));
            t->swap_after = -1;
        }
        g_id_reads[slot]++;
        if (len > NP_HEXMAP_ID_HDR_BYTES) {
            g_full_reads[slot]++;
        }
    }
    memcpy(data, &t->regs[reg_addr], len);
    return NP_PBM_OK;
}

/* Unused by the unit; defined so the link does not depend on np_pbm_hal.c. */
np_pbm_status_t np_pbm_hal_i2c_write(uint8_t slot, uint8_t reg_addr,
                                     const uint8_t *data, uint8_t len)
{
    (void)slot; (void)reg_addr; (void)data; (void)len;
    return NP_PBM_ERR_I2C_WRITE;
}

/* Build an identity block into a register space. */
static void write_block(uint8_t regs[256], uint8_t uid_seed, const uint8_t *types,
                        uint8_t n, uint8_t status)
{
    memset(regs, 0, 256);
    regs[NP_HEXMAP_STATUS_REG] = status;
    uint8_t *b = &regs[NP_HEXMAP_ID_REG];
    b[0] = NP_HEXMAP_ID_FORMAT;
    for (unsigned i = 0; i < NP_HEXMAP_UID_LEN; i++) {
        b[1 + i] = (uint8_t)(uid_seed + i);
    }
    b[9] = n;
    memcpy(&b[10], types, n);
    uint32_t crc = np_hexmap_crc32(b, (size_t)10u + n);
    b[10 + n + 0] = (uint8_t)(crc);
    b[10 + n + 1] = (uint8_t)(crc >> 8);
    b[10 + n + 2] = (uint8_t)(crc >> 16);
    b[10 + n + 3] = (uint8_t)(crc >> 24);
}

/* Recompute the CRC after a test edits a block, so the edit is the only
 * thing wrong with it and the CRC check cannot be what refuses it. */
static void reseal(uint8_t regs[256])
{
    uint8_t *b = &regs[NP_HEXMAP_ID_REG];
    uint8_t  n = b[9];
    uint32_t crc = np_hexmap_crc32(b, (size_t)10u + n);
    for (unsigned i = 0; i < 4u; i++) {
        b[10 + n + i] = (uint8_t)(crc >> (8u * i));
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

static const uint8_t T1A[] = { NP_ELEM_LED_660, NP_ELEM_LED_808, NP_ELEM_PD_FORWARD,
                               NP_ELEM_PD_BACK, NP_ELEM_NTC };
static const uint8_t T1B[] = { NP_ELEM_LED_660, NP_ELEM_LED_808, NP_ELEM_NTC,
                               NP_ELEM_DUAL_ELECTRODE };

static void plug(uint16_t s, uint8_t seed, const uint8_t *types, uint8_t n)
{
    g_tile[s].present = true;
    write_block(g_tile[s].regs, seed, types, n, 0x00u);
}

static void reset_all(void)
{
    memset(g_tile, 0, sizeof(g_tile));
    for (unsigned s = 0; s < N_SOCK; s++) {
        g_tile[s].fail_on_len = -1;
        g_tile[s].swap_after  = -1;
    }
    memset(g_id_reads, 0, sizeof(g_id_reads));
    memset(g_full_reads, 0, sizeof(g_full_reads));
    (void)np_module_map_init(GEOM, N_SOCK);
}

static bool socket_holds(uint16_t s, uint8_t seed)
{
    np_module_uid_t got, want = uid_of(seed);
    return np_module_map_socket_uid(s, &got) == NP_HUB_OK &&
           np_module_uid_equal(&got, &want);
}

static bool socket_empty(uint16_t s)
{
    np_module_uid_t got;
    return np_module_map_socket_uid(s, &got) == NP_HUB_ERR_NOT_PRESENT;
}

static np_elem_type_t elem_at(uint16_t s, uint8_t e)
{
    np_hex_addr_t     a = { (uint8_t)s, e };
    np_physical_loc_t loc;
    if (np_module_map_resolve(a, &loc) != NP_HUB_OK) {
        return (np_elem_type_t)0xFF;
    }
    return loc.elem_type;
}

/* ── NVRAM double (restore path) ──────────────────────────────────────────────── */

static uint8_t g_nv[NP_HEXMAP_NVRAM_MAX_BYTES];
static size_t  g_nv_len;
static bool    g_nv_ok;

np_hub_status_t np_hexmap_nvram_write(const uint8_t *buf, size_t len)
{
    if (len > sizeof(g_nv)) {
        return NP_HUB_ERR_GENERIC;
    }
    memcpy(g_nv, buf, len);
    g_nv_len = len;
    g_nv_ok  = true;
    return NP_HUB_OK;
}

np_hub_status_t np_hexmap_nvram_read(uint8_t *buf, size_t len, size_t *read_len)
{
    if (!g_nv_ok || g_nv_len > len) {
        return NP_HUB_ERR_NOT_PRESENT;
    }
    memcpy(buf, g_nv, g_nv_len);
    *read_len = g_nv_len;
    return NP_HUB_OK;
}

/* ── Tests ────────────────────────────────────────────────────────────────────── */

static void test_empty_socket(void)
{
    reset_all();
    bool changed = true;
    check(np_hexmap_poll_socket(0, &changed) == NP_HUB_ERR_NOT_PRESENT,
          "empty socket: NOT_PRESENT");
    check(!changed, "empty socket that was empty: no change");
    check(socket_empty(0), "empty socket stays empty");
}

static void test_new_module_inventoried(void)
{
    reset_all();
    plug(1, 0x10, T1A, sizeof(T1A));
    bool changed = false;
    check(np_hexmap_poll_socket(1, &changed) == NP_HUB_OK, "new module: OK");
    check(changed, "new module: changed");
    check(socket_holds(1, 0x10), "new module: UID stored");
    check(elem_at(1, 0) == NP_ELEM_LED_660 && elem_at(1, 4) == NP_ELEM_NTC,
          "new module: element types stored in element_id order");
    check(elem_at(1, 5) == (np_elem_type_t)0xFF, "new module: no element past count");
    check(g_full_reads[1] == 1, "new module: exactly one whole-block read");
}

static void test_unchanged_module_not_reinventoried(void)
{
    reset_all();
    plug(2, 0x20, T1B, sizeof(T1B));
    bool changed = false;
    (void)np_hexmap_poll_socket(2, &changed);
    for (int i = 0; i < 5; i++) {
        (void)np_hexmap_poll_socket(2, &changed);
    }
    check(!changed, "unchanged module: last poll reports no change");
    check(g_full_reads[2] == 1, "unchanged module: never re-inventoried");
    check(g_id_reads[2] == 7, "unchanged module: header read each poll");
    check(socket_holds(2, 0x20), "unchanged module: still present");
}

static void test_swap_between_polls(void)
{
    reset_all();
    plug(0, 0x10, T1A, sizeof(T1A));
    bool changed;
    (void)np_hexmap_poll_socket(0, &changed);
    plug(0, 0x30, T1B, sizeof(T1B));
    check(np_hexmap_poll_socket(0, &changed) == NP_HUB_OK && changed,
          "swap between polls: re-inventoried");
    check(socket_holds(0, 0x30) && elem_at(0, 3) == NP_ELEM_DUAL_ELECTRODE,
          "swap between polls: new module's elements");
}

static void test_removal_clears(void)
{
    reset_all();
    plug(3, 0x40, T1A, sizeof(T1A));
    bool changed;
    (void)np_hexmap_poll_socket(3, &changed);
    g_tile[3].present = false;
    check(np_hexmap_poll_socket(3, &changed) == NP_HUB_ERR_NOT_PRESENT && changed,
          "removal: reported and changed");
    check(socket_empty(3), "removal: socket empty");
}

/* A module whose identity turns unreadable must not keep its old record. */
static void test_read_fault_clears_present_module(void)
{
    reset_all();
    plug(3, 0x40, T1A, sizeof(T1A));
    bool changed;
    (void)np_hexmap_poll_socket(3, &changed);

    g_tile[3].fail_on_len = NP_HEXMAP_ID_HDR_BYTES;
    check(np_hexmap_poll_socket(3, &changed) != NP_HUB_OK && changed,
          "header read fault on a present module: error + changed");
    check(socket_empty(3), "header read fault: socket cleared, not stale");

    g_tile[3].fail_on_len = -1;
    (void)np_hexmap_poll_socket(3, &changed);
    g_tile[3].fail_on_len = 1;   /* the STATUS (health) read */
    check(np_hexmap_poll_socket(3, &changed) != NP_HUB_OK && socket_empty(3),
          "health read fault: socket cleared");
}

static void test_bad_crc_fails_closed(void)
{
    reset_all();
    plug(1, 0x10, T1A, sizeof(T1A));
    g_tile[1].regs[NP_HEXMAP_ID_REG + 10 + 2] ^= 0x01;   /* flip a type bit */
    bool changed;
    check(np_hexmap_poll_socket(1, &changed) == NP_HUB_ERR_BAD_MAGIC,
          "corrupt block: BAD_MAGIC");
    check(socket_empty(1), "corrupt block: not present");
}

static void test_whole_block_read_fault(void)
{
    reset_all();
    plug(1, 0x10, T1A, sizeof(T1A));
    g_tile[1].fail_on_len = (int)(NP_HEXMAP_ID_HDR_BYTES + sizeof(T1A) +
                                  NP_HEXMAP_ID_CRC_BYTES);
    bool changed;
    check(np_hexmap_poll_socket(1, &changed) != NP_HUB_OK && socket_empty(1),
          "whole-block read fault: not present");
}

/* Header names A; by the time the whole block is read the tile is B, with an
 * intact CRC. Storing B's types under A's UID is the defect being refused. */
static void test_swap_between_header_and_block(void)
{
    reset_all();
    plug(4, 0x50, T1A, sizeof(T1A));
    g_tile[4].swap_after = 1;   /* the second ID read sees the other tile */
    write_block(g_tile[4].swap_regs, 0x60, T1A, sizeof(T1A), 0);
    bool changed;
    check(np_hexmap_poll_socket(4, &changed) != NP_HUB_OK,
          "swap mid-inventory: refused");
    check(socket_empty(4), "swap mid-inventory: nothing stored under either UID");
}

/* Same UID, different count between header and block: refused. */
static void test_count_mismatch(void)
{
    reset_all();
    plug(4, 0x50, T1A, sizeof(T1A));
    g_tile[4].swap_after = 1;
    write_block(g_tile[4].swap_regs, 0x50, T1B, sizeof(T1B), 0);
    bool changed;
    check(np_hexmap_poll_socket(4, &changed) != NP_HUB_OK && socket_empty(4),
          "count differs between header and block: refused");
}

/* The format byte is checked in the CRC-covered block too, not only in the
 * unchecked header: a header read as format 1 does not vouch for the block. */
static void test_block_format_checked(void)
{
    reset_all();
    plug(4, 0x50, T1A, sizeof(T1A));
    g_tile[4].swap_after = 1;
    write_block(g_tile[4].swap_regs, 0x50, T1A, sizeof(T1A), 0);
    g_tile[4].swap_regs[NP_HEXMAP_ID_REG] = 0x02;
    reseal(g_tile[4].swap_regs);
    bool changed;
    check(np_hexmap_poll_socket(4, &changed) == NP_HUB_ERR_BAD_VERSION &&
          socket_empty(4), "block format differs from header: refused");
}

static void test_unknown_type_rejects_module(void)
{
    reset_all();
    const uint8_t t[] = { NP_ELEM_LED_660, (uint8_t)NP_ELEM_TYPE_COUNT, NP_ELEM_NTC };
    plug(2, 0x70, t, sizeof(t));
    bool changed;
    check(np_hexmap_poll_socket(2, &changed) == NP_HUB_ERR_BAD_VERSION,
          "unknown element type: BAD_VERSION");
    check(socket_empty(2), "unknown element type: whole module rejected");

    const uint8_t last[] = { (uint8_t)(NP_ELEM_TYPE_COUNT - 1) };
    plug(2, 0x71, last, sizeof(last));
    check(np_hexmap_poll_socket(2, &changed) == NP_HUB_OK,
          "highest known element type: accepted");
}

static void test_bad_headers(void)
{
    reset_all();
    bool changed;

    plug(0, 0x10, T1A, sizeof(T1A));
    g_tile[0].regs[NP_HEXMAP_ID_REG] = 0x02;
    reseal(g_tile[0].regs);
    check(np_hexmap_poll_socket(0, &changed) == NP_HUB_ERR_BAD_VERSION &&
          socket_empty(0), "wrong format byte: refused");

    plug(0, 0x00, T1A, sizeof(T1A));
    memset(&g_tile[0].regs[NP_HEXMAP_ID_REG + 1], 0x00, NP_HEXMAP_UID_LEN);
    reseal(g_tile[0].regs);
    check(np_hexmap_poll_socket(0, &changed) == NP_HUB_ERR_BAD_MAGIC &&
          socket_empty(0), "all-zero UID: refused");

    plug(0, 0x10, T1A, sizeof(T1A));
    memset(&g_tile[0].regs[NP_HEXMAP_ID_REG + 1], 0xFF, NP_HEXMAP_UID_LEN);
    reseal(g_tile[0].regs);
    check(np_hexmap_poll_socket(0, &changed) == NP_HUB_ERR_BAD_MAGIC &&
          socket_empty(0), "all-0xFF UID: refused");

    plug(0, 0x10, T1A, sizeof(T1A));
    g_tile[0].regs[NP_HEXMAP_ID_REG + 9] = (uint8_t)(NP_HEXMAP_MAX_ELEMENTS + 1);
    check(np_hexmap_poll_socket(0, &changed) == NP_HUB_ERR_BAD_MAGIC &&
          socket_empty(0), "count above NP_HEXMAP_MAX_ELEMENTS: refused");
}

static void test_full_element_domain(void)
{
    reset_all();
    uint8_t t[NP_HEXMAP_MAX_ELEMENTS];
    for (unsigned i = 0; i < NP_HEXMAP_MAX_ELEMENTS; i++) {
        t[i] = (uint8_t)(1u + (i % (NP_ELEM_TYPE_COUNT - 1u)));
    }
    plug(1, 0x80, t, (uint8_t)NP_HEXMAP_MAX_ELEMENTS);
    bool changed;
    check(np_hexmap_poll_socket(1, &changed) == NP_HUB_OK,
          "128-element module: inventoried in one 142-byte read");
    check(elem_at(1, 127) == (np_elem_type_t)t[127], "128-element module: last element");
}

static void test_zero_element_module(void)
{
    reset_all();
    plug(1, 0x90, T1A, 0);
    bool changed;
    check(np_hexmap_poll_socket(1, &changed) == NP_HUB_OK && socket_holds(1, 0x90),
          "zero-element module: present, resolves nothing");
    check(elem_at(1, 0) == (np_elem_type_t)0xFF, "zero-element module: no element 0");
}

static void test_unwired_and_out_of_domain(void)
{
    reset_all();
    plug(SOCK_UNWIRED, 0xA0, T1A, sizeof(T1A));
    bool changed = true;
    check(np_hexmap_poll_socket(SOCK_UNWIRED, &changed) == NP_HUB_ERR_INVALID_ARG &&
          !changed, "unwired socket: INVALID_ARG, no change");
    check(np_hexmap_poll_socket(NP_HEXMAP_MAX_SOCKETS, &changed) ==
          NP_HUB_ERR_INVALID_ARG, "socket past the domain: INVALID_ARG");
}

static void test_inventory_fn_contract(void)
{
    reset_all();
    plug(1, 0x10, T1A, sizeof(T1A));
    uint8_t types[NP_HEXMAP_MAX_ELEMENTS];
    uint8_t count = 0xEE;
    check(np_hexmap_inventory_read(1, NULL, types, sizeof(types), &count) ==
          NP_HUB_ERR_INVALID_ARG && count == 0xEE,
          "inventory_fn without the poll's header: refused, nothing written");

    np_hexmap_ident_t id;
    check(np_hexmap_read_ident(1, &id) == NP_HUB_OK, "read_ident: OK");
    check(np_hexmap_inventory_read(1, &id, types, 2, &count) ==
          NP_HUB_ERR_CMD_TOO_MANY && count == 0xEE,
          "inventory_fn: count above max refused");
    check(np_hexmap_inventory_read(1, &id, types, sizeof(types), &count) ==
          NP_HUB_OK && count == sizeof(T1A) && types[1] == NP_ELEM_LED_808,
          "inventory_fn: fills types and count");
}

/* REQ-LFS-02 end to end: a record restored from npmp.bin answers nothing until
 * this poll sees its UID, then answers without a re-inventory. A different
 * module in the socket replaces it. */
static void test_restore_confirmed_by_poll(void)
{
    reset_all();
    plug(0, 0x10, T1A, sizeof(T1A));
    plug(1, 0x20, T1B, sizeof(T1B));
    bool changed;
    (void)np_hexmap_poll_socket(0, &changed);
    (void)np_hexmap_poll_socket(1, &changed);
    check(np_module_map_persist() == NP_HUB_OK, "restore: persisted");

    /* Reboot: fresh map, restore, nothing usable yet. */
    (void)np_module_map_init(GEOM, N_SOCK);
    check(np_module_map_restore() == NP_HUB_OK, "restore: restored");
    check(socket_empty(0) && socket_empty(1), "restore: unconfirmed records answer nothing");

    memset(g_full_reads, 0, sizeof(g_full_reads));
    plug(1, 0x21, T1A, sizeof(T1A));    /* socket 1 changed while powered off */
    (void)np_hexmap_poll_socket(0, &changed);
    check(!changed && socket_holds(0, 0x10) && g_full_reads[0] == 0,
          "restore: same module confirmed without re-inventory");
    (void)np_hexmap_poll_socket(1, &changed);
    check(changed && socket_holds(1, 0x21) && g_full_reads[1] == 1 &&
          elem_at(1, 2) == NP_ELEM_PD_FORWARD,
          "restore: different module replaces the cached record");
}

int main(void)
{
    printf("=== np_hexmap_inventory tests (OI-HEXMAP-02) ===\n");
    test_empty_socket();
    test_new_module_inventoried();
    test_unchanged_module_not_reinventoried();
    test_swap_between_polls();
    test_removal_clears();
    test_read_fault_clears_present_module();
    test_bad_crc_fails_closed();
    test_whole_block_read_fault();
    test_swap_between_header_and_block();
    test_count_mismatch();
    test_block_format_checked();
    test_unknown_type_rejects_module();
    test_bad_headers();
    test_full_element_domain();
    test_zero_element_module();
    test_unwired_and_out_of_domain();
    test_inventory_fn_contract();
    test_restore_confirmed_by_poll();

    if (g_failures == 0) {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d TEST(S) FAILED\n", g_failures);
    return 1;
}
