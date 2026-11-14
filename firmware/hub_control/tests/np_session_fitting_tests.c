/*
 * Host tests — the session's hardware fitting frame (SWR-HV-02, SWR-HV-03).
 *
 * Drives the real np_module_map with simulated tiles and checks the frame np_fitting_build writes: the
 * format, the hardware version, the wavelength-rules stamp of the descriptor being run, a byte for EVERY wired
 * socket (so the record's shape does not depend on the fitting), and the module kind in each.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "np_module_map.h"
#include "np_session_fitting.h"

/* The module map persists through these; this test does not persist. */
np_hub_status_t np_hexmap_nvram_write(const uint8_t *buf, size_t len) { (void)buf; (void)len; return NP_HUB_OK; }
np_hub_status_t np_hexmap_nvram_read(uint8_t *buf, size_t len, size_t *read_len)
{
    (void)buf; (void)len; *read_len = 0U;
    return NP_HUB_ERR_NOT_PRESENT;
}

static int g_failures = 0;
static void check(int cond, const char *name)
{
    if (cond) { printf("PASS: %s\n", name); }
    else      { printf("FAIL: %s\n", name); g_failures++; }
}

#define N_WIRED 6U
static const np_socket_geom_t GEOM[N_WIRED + 1U] = {
    { true, -30, -80 }, { true, 30, -80 }, { true, -70, 0 }, { true, 70, 0 }, { true, -25, 40 }, { true, 25, 40 },
    { false, 0, 0 },
};

/* The stamp the descriptor carries: not stated (zeros), a NeurOne release, an owner set. */
static const uint8_t STAMP_NONE[NP_HUB_PROTO_RULES_STAMP_LEN] = { 0 };
static uint8_t g_stamp_release[NP_HUB_PROTO_RULES_STAMP_LEN];
static uint8_t g_stamp_owner[NP_HUB_PROTO_RULES_STAMP_LEN];
static void make_stamps(void)
{
    g_stamp_release[0] = 1U; g_stamp_release[1] = 1U; g_stamp_release[2] = 0U;
    g_stamp_owner[0]   = 2U; g_stamp_owner[1]   = 1U; g_stamp_owner[2]   = 0U;
    for (size_t k = 3U; k < NP_HUB_PROTO_RULES_STAMP_LEN; k++) {
        g_stamp_release[k] = (uint8_t)(0x40U + k);
        g_stamp_owner[k]   = (uint8_t)(0x90U + k);
    }
}

static uint8_t g_types[NP_HEXMAP_MAX_ELEMENTS];
static uint8_t g_count;

static np_hub_status_t inv_cb(uint16_t socket_id, void *ctx, uint8_t *out, uint8_t max, uint8_t *count_out)
{
    (void)socket_id; (void)ctx;
    uint8_t n = (g_count > max) ? max : g_count;
    memcpy(out, g_types, n);
    *count_out = g_count;
    return NP_HUB_OK;
}

static void plug(uint16_t socket, uint8_t seed, const np_elem_type_t *types, uint8_t n)
{
    np_module_uid_t u;
    bool changed = false;
    for (unsigned i = 0; i < NP_HEXMAP_UID_LEN; i++) { u.b[i] = (uint8_t)(seed + i); }
    for (uint8_t i = 0; i < n; i++) { g_types[i] = (uint8_t)types[i]; }
    g_count = n;
    (void)np_module_map_apply_poll(socket, &u, 0x00, inv_cb, NULL, &changed);
}

static const np_elem_type_t BASE_TILE[]  = { NP_ELEM_LED_660, NP_ELEM_LED_808, NP_ELEM_NTC };
static const np_elem_type_t SMART_TILE[] = { NP_ELEM_LED_660, NP_ELEM_LED_808, NP_ELEM_LED_1064, NP_ELEM_NTC };
static const np_elem_type_t ELECTRODE[]  = { NP_ELEM_NTC };

/* @verifies SWR-HV-02 */
static void test_frame_has_a_byte_per_wired_socket_whatever_is_fitted(void)
{
    uint8_t f[NP_FITTING_MAX_LEN];
    size_t  n;

    check(np_fitting_build(1U, STAMP_NONE, f, sizeof f) == 0U, "fitting: an unbound map writes no frame");

    (void)np_module_map_init(GEOM, N_WIRED + 1U);   /* the unwired entry still counts toward the bound geometry */
    const uint16_t wired = np_module_map_socket_count();

    n = np_fitting_build(1U, STAMP_NONE, f, sizeof f);
    check(n == NP_FITTING_HDR_LEN + wired && f[0] == NP_FITTING_FORMAT && f[NP_FITTING_IDX_HW] == 1U &&
          f[NP_FITTING_IDX_COUNT] == wired,
          "fitting: the frame is the header plus one byte for every socket the map was bound to");
    bool all_empty = true;
    for (size_t i = 0U; i < wired; i++) { if (f[NP_FITTING_HDR_LEN + i] != NP_FITTING_EMPTY) { all_empty = false; } }
    check(all_empty, "fitting: with nothing fitted every socket reads empty, and the length is unchanged");

    plug(0U, 0x10U, BASE_TILE, 3U);
    plug(2U, 0x20U, SMART_TILE, 4U);
    plug(3U, 0x30U, ELECTRODE, 1U);
    const size_t n2 = np_fitting_build(1U, STAMP_NONE, f, sizeof f);
    check(n2 == n, "fitting: the length does not depend on what is fitted");
    check(f[NP_FITTING_HDR_LEN + 0U] == (uint8_t)NP_MOD_PBM_BASE, "fitting: a 660/808 tile is a base tile");
    check(f[NP_FITTING_HDR_LEN + 1U] == NP_FITTING_EMPTY, "fitting: an empty socket is null (0)");
    check(f[NP_FITTING_HDR_LEN + 2U] == (uint8_t)NP_MOD_PBM_SMART, "fitting: a tile with a 1064 nm emitter is a smart tile");
    check(f[NP_FITTING_HDR_LEN + 3U] == NP_FITTING_OTHER, "fitting: a module with no PBM emitter is 'other', not empty");
}

/* @verifies SWR-HV-02 */
static void test_hardware_version_is_carried_and_bounds_are_checked(void)
{
    uint8_t f[NP_FITTING_MAX_LEN];
    (void)np_module_map_init(GEOM, N_WIRED + 1U);
    const uint16_t wired = np_module_map_socket_count();

    check(np_fitting_build(7U, STAMP_NONE, f, sizeof f) != 0U && f[NP_FITTING_IDX_HW] == 7U,
          "fitting: the helmet hardware version is in the frame");
    check(np_fitting_build(1U, STAMP_NONE, f, (size_t)NP_FITTING_HDR_LEN + wired - 1U) == 0U,
          "fitting: a buffer one byte short writes nothing, never a truncated frame");
    check(np_fitting_build(1U, STAMP_NONE, NULL, 100U) == 0U, "fitting: no buffer writes nothing");
}

/* @verifies SWR-HV-02 */
static void test_a_swapped_module_changes_the_record_not_the_protocol(void)
{
    uint8_t a[NP_FITTING_MAX_LEN];
    uint8_t b[NP_FITTING_MAX_LEN];
    (void)np_module_map_init(GEOM, N_WIRED + 1U);
    plug(1U, 0x40U, BASE_TILE, 3U);
    (void)np_fitting_build(1U, STAMP_NONE, a, sizeof a);
    plug(1U, 0x50U, SMART_TILE, 4U);   /* a different tile in the same socket */
    (void)np_fitting_build(1U, STAMP_NONE, b, sizeof b);
    check(a[NP_FITTING_HDR_LEN + 1U] == (uint8_t)NP_MOD_PBM_BASE && b[NP_FITTING_HDR_LEN + 1U] == (uint8_t)NP_MOD_PBM_SMART,
          "fitting: the record states what is fitted now, not what was fitted before");
}

/* @verifies SWR-HV-03#c */
static void test_the_rules_stamp_is_copied_unchanged_and_never_changes_the_shape(void)
{
    uint8_t a[NP_FITTING_MAX_LEN];
    uint8_t b[NP_FITTING_MAX_LEN];
    uint8_t c[NP_FITTING_MAX_LEN];
    make_stamps();
    (void)np_module_map_init(GEOM, N_WIRED + 1U);
    plug(0U, 0x10U, BASE_TILE, 3U);
    const size_t la = np_fitting_build(1U, STAMP_NONE, a, sizeof a);
    const size_t lb = np_fitting_build(1U, g_stamp_release, b, sizeof b);
    const size_t lc = np_fitting_build(1U, g_stamp_owner, c, sizeof c);
    check(la != 0U && la == lb && lb == lc, "stamp: the frame's length does not depend on the rules (5.1 rule 2)");
    check(memcmp(&a[NP_FITTING_IDX_STAMP], STAMP_NONE, sizeof STAMP_NONE) == 0,
          "stamp: a descriptor that states nothing is recorded as zeros, origin 0, never an invented version");
    check(memcmp(&b[NP_FITTING_IDX_STAMP], g_stamp_release, NP_HUB_PROTO_RULES_STAMP_LEN) == 0 &&
          memcmp(&c[NP_FITTING_IDX_STAMP], g_stamp_owner, NP_HUB_PROTO_RULES_STAMP_LEN) == 0,
          "stamp: the release and the owner set's windows are copied byte for byte");
    check(memcmp(&a[NP_FITTING_HDR_LEN], &b[NP_FITTING_HDR_LEN], la - NP_FITTING_HDR_LEN) == 0 &&
          a[NP_FITTING_IDX_HW] == b[NP_FITTING_IDX_HW] && a[0] == b[0],
          "stamp: nothing else in the frame moves with it");
    check(np_fitting_build(1U, NULL, a, sizeof a) == 0U, "stamp: no stamp writes no frame");
    check(NP_FITTING_HDR_LEN == 42U && NP_FITTING_FORMAT == 2U, "stamp: format 2, a 42-byte header through n");
}

int main(void)
{
    test_frame_has_a_byte_per_wired_socket_whatever_is_fitted();
    test_hardware_version_is_carried_and_bounds_are_checked();
    test_a_swapped_module_changes_the_record_not_the_protocol();
    test_the_rules_stamp_is_copied_unchanged_and_never_changes_the_shape();
    if (g_failures != 0) { printf("\n%d FAILURE(S)\n", g_failures); return 1; }
    printf("\nAll np_session_fitting tests passed.\n");
    return 0;
}
