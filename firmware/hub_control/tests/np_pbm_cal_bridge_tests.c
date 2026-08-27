/*
 * NeurOne Hub Control — PBM calibration bridge host tests (OI-HUB-C06)
 *
 * End-to-end: a module is inventoried into np_module_map, its factory
 * coefficients are stored against its UID, and a PBM session started through
 * firmware/pbm picks them up — for that module, at whatever socket it
 * happens to be in, and for no other module.
 *
 * The two properties that make this worth testing rather than asserting are
 * both swap properties: move the module to another socket and the calibration
 * must move with it; put a different module in the same socket and it must NOT
 * inherit what was there. Everything else would pass against a socket-keyed
 * implementation.
 *
 * No FreeRTOS, no hardware. Build with -DNP_BUILD_TESTS=ON.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "np_module_map.h"
#include "np_pbm_cal_bridge.h"
#include "np_pbm_session.h"
#include "np_pbm_dose.h"

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

/* ── np_module_map fixtures ──────────────────────────────────────────────────── */

#define N_SOCKETS 80u

static np_socket_geom_t g_geom[N_SOCKETS];

static const np_elem_type_t PBM_TILE[] = {
    NP_ELEM_LED_660, NP_ELEM_LED_808, NP_ELEM_LED_1064,
    NP_ELEM_PD_FORWARD, NP_ELEM_PD_BACK, NP_ELEM_NTC,
};
#define PBM_TILE_N (sizeof(PBM_TILE) / sizeof(PBM_TILE[0]))

static np_hub_status_t inv_cb(uint16_t socket_id, void *ctx,
                              uint8_t *types_out, uint8_t max, uint8_t *count_out)
{
    (void)socket_id; (void)ctx;
    if (max < PBM_TILE_N) { return NP_HUB_ERR_CMD_TOO_MANY; }
    for (unsigned i = 0; i < PBM_TILE_N; i++) {
        types_out[i] = (uint8_t)PBM_TILE[i];
    }
    *count_out = (uint8_t)PBM_TILE_N;
    return NP_HUB_OK;
}

/* NVRAM HAL. These tests never persist or restore, but np_module_map.c
 * references both symbols, so the link needs them. */
np_hub_status_t np_hexmap_nvram_write(const uint8_t *buf, size_t len)
{
    (void)buf; (void)len;
    return NP_HUB_OK;
}

np_hub_status_t np_hexmap_nvram_read(uint8_t *buf, size_t len, size_t *read_len)
{
    (void)buf; (void)len; (void)read_len;
    return NP_HUB_ERR_NOT_PRESENT;
}

static np_module_uid_t map_uid(uint8_t seed)
{
    np_module_uid_t u;
    memset(&u, 0, sizeof(u));
    u.b[0] = seed;
    u.b[7] = 0x5Au;
    return u;
}

static void geom_init(void)
{
    for (unsigned i = 0; i < N_SOCKETS; i++) {
        g_geom[i].present_in_helmet = true;
        g_geom[i].x_mm = (int16_t)i;
        g_geom[i].y_mm = (int16_t)i;
    }
    (void)np_module_map_init(g_geom, (uint16_t)N_SOCKETS);
}

static void plug(uint16_t socket, uint8_t seed)
{
    np_module_uid_t u = map_uid(seed);
    bool changed = false;
    (void)np_module_map_apply_poll(socket, &u, 0x00, inv_cb, NULL, &changed);
}

static void unplug(uint16_t socket)
{
    np_module_uid_t zero;
    memset(&zero, 0, sizeof(zero));
    bool changed = false;
    (void)np_module_map_apply_poll(socket, &zero, 0x00, inv_cb, NULL, &changed);
}

static void store_cal(uint8_t seed, float base)
{
    float cal[NP_HEXMAP_CAL_FLOATS];
    for (unsigned i = 0; i < NP_HEXMAP_CAL_FLOATS; i++) {
        cal[i] = base + (float)i * 0.01f;
    }
    np_module_uid_t u = map_uid(seed);
    (void)np_module_map_set_cal(&u, cal);
}

/* ── PBM session fixtures ────────────────────────────────────────────────────── */

static void mask_set(uint8_t mask[NP_PBM_SOCKET_MASK_BYTES], uint8_t socket_id)
{
    mask[socket_id / 8U] |= (uint8_t)(1U << (socket_id % 8U));
}

/* Start a session over the two given sockets and leave the ctx populated. */
static void start_session(np_pbm_session_ctx_t *ctx, uint8_t s0, uint8_t s1)
{
    np_pbm_session_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.hdr.version     = NP_SES1064_VERSION;
    desc.hdr.group_count = 1U;
    desc.hdr.duration_s  = 62U;
    mask_set(desc.groups[0].mask, s0);
    mask_set(desc.groups[0].mask, s1);
    desc.groups[0].preset.cur_a        = 180U;
    desc.groups[0].preset.cur_b        = 180U;
    desc.groups[0].preset.cur_c        = 150U;
    desc.groups[0].preset.freq_hz      = 40U;
    desc.groups[0].preset.duty         = 0x10U;
    desc.groups[0].preset.channel_mask = NP_PBM_CH_ALL_EN;

    np_pbm_session_init(ctx, NULL, NULL, NULL, 0U);
    (void)np_pbm_session_start(ctx, &desc);
}

/* Index of a socket in the session's expanded active list, or -1. */
static int slot_of(const np_pbm_session_ctx_t *ctx, uint8_t socket_id)
{
    for (uint8_t i = 0; i < ctx->active_socket_count; i++) {
        if (ctx->active_socket_id[i] == socket_id) { return (int)i; }
    }
    return -1;
}

/* ── Tests ───────────────────────────────────────────────────────────────────── */

static void test_bridge_not_installed_is_defaults_only(void)
{
    geom_init();
    plug(0, 0x11);
    store_cal(0x11, 0.500f);
    np_hub_pbm_cal_bridge_remove();

    np_pbm_session_ctx_t ctx;
    start_session(&ctx, 0U, 1U);

    check(ctx.active_cal_source[0] == (uint8_t)NP_CAL_DEFAULT,
          "bridge absent: DEFAULT even though the map holds a record "
          "(an un-bridged build is degraded, never wrong)");
    (void)np_pbm_session_abort(&ctx, NP_PBM_FAULT_NONE);
}

static void test_bridge_delivers_factory_cal_for_the_right_module(void)
{
    geom_init();
    plug(0, 0x11);          /* has a factory record */
    plug(1, 0x22);          /* inventoried, but no record stored */
    store_cal(0x11, 0.500f);
    np_hub_pbm_cal_bridge_install();

    np_pbm_session_ctx_t ctx;
    start_session(&ctx, 0U, 1U);

    int i0 = slot_of(&ctx, 0U), i1 = slot_of(&ctx, 1U);
    check(i0 >= 0 && i1 >= 0, "bridge: both sockets are active in the session");

    check(ctx.active_cal_source[i0] == (uint8_t)NP_CAL_FACTORY,
          "bridge: socket 0's module resolves to FACTORY");
    check(ctx.active_cal_source[i1] == (uint8_t)NP_CAL_DEFAULT,
          "bridge: socket 1's module (no record) resolves to DEFAULT");
    check(ctx.cal[i0][NP_WL_660NM].K_PD1 == 0.500f,
          "bridge: coefficients arrive in [wl][coeff] order — 660nm K_PD1 first");
    check(ctx.cal[i0][NP_WL_1064NM].K_PD2 == 0.570f,
          "bridge: 1064nm K_PD2 is flat index 3*2+1 = 7");
    check(ctx.cal[i1][NP_WL_1064NM].K_PD1 == 0.088f,
          "bridge: the record-less module gets firmware defaults, not its "
          "neighbour's coefficients");

    (void)np_pbm_session_abort(&ctx, NP_PBM_FAULT_NONE);
    np_hub_pbm_cal_bridge_remove();
}

static void test_bridge_calibration_follows_the_module(void)
{
    /* Same module, different socket. A socket-keyed store fails here. */
    geom_init();
    plug(0, 0x11);
    store_cal(0x11, 0.500f);
    np_hub_pbm_cal_bridge_install();

    /* Move the module from socket 0 to socket 40 and re-supply its record —
     * the hub cache is per-occupancy, so a factory-cal read follows the plug. */
    unplug(0);
    plug(40, 0x11);
    store_cal(0x11, 0.500f);

    np_pbm_session_ctx_t ctx;
    start_session(&ctx, 0U, 40U);

    int i0 = slot_of(&ctx, 0U), i40 = slot_of(&ctx, 40U);
    check(ctx.active_cal_source[i40] == (uint8_t)NP_CAL_FACTORY,
          "bridge: FACTORY followed the module to socket 40");
    check(ctx.active_cal_source[i0] == (uint8_t)NP_CAL_DEFAULT,
          "bridge: socket 0, now empty, is DEFAULT — provenance is not a "
          "property of the hole");
    check(ctx.cal[i40][NP_WL_660NM].K_PD1 == 0.500f,
          "bridge: the module's own coefficients at its new socket");

    (void)np_pbm_session_abort(&ctx, NP_PBM_FAULT_NONE);
    np_hub_pbm_cal_bridge_remove();
}

static void test_bridge_swap_does_not_inherit(void)
{
    /* Different module, same socket. This is the invisible-miscalibration path
     * that OI-HUB-C06 exists to close. */
    geom_init();
    plug(7, 0x11);
    store_cal(0x11, 0.500f);
    np_hub_pbm_cal_bridge_install();

    plug(7, 0x99);   /* swap in a different module at the same socket */

    np_pbm_session_ctx_t ctx;
    start_session(&ctx, 7U, 8U);

    int i7 = slot_of(&ctx, 7U);
    check(ctx.active_cal_source[i7] == (uint8_t)NP_CAL_DEFAULT,
          "bridge: a swapped-in module does NOT inherit the previous "
          "occupant's calibration (NP-HW-HUB-001 Rev 3 §9.5)");
    check(ctx.cal[i7][NP_WL_660NM].K_PD1 == 0.120f,
          "bridge: it gets firmware defaults, not 0.500f");

    (void)np_pbm_session_abort(&ctx, NP_PBM_FAULT_NONE);
    np_hub_pbm_cal_bridge_remove();
}

int main(void)
{
    test_bridge_not_installed_is_defaults_only();
    test_bridge_delivers_factory_cal_for_the_right_module();
    test_bridge_calibration_follows_the_module();
    test_bridge_swap_does_not_inherit();

    if (g_failures == 0) {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d TEST(S) FAILED\n", g_failures);
    return 1;
}
