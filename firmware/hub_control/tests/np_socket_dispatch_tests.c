/*
 * NeurOne Hub Control — Socket-Indexed Dispatch Host Tests (OI-FWHUB-01)
 * Document: NP-FW-HUB-001 Rev 2 §3.3, §5.6, §10; NP-FW-MMSOCK-001 Rev 1 §3.6, §11
 *
 * np_socket_dispatch.c is the path by which a socket-addressed transcranial PBM
 * command reaches an emitter. Before it existed there was no such path at all;
 * now that it does, the properties that matter are the ones that decide WHERE
 * light goes and WHETHER UHDR says it went there:
 *
 *   - every named socket is driven, or none is (placement, power, driver fault);
 *   - nothing but a lattice PBM emitter is socket-addressable;
 *   - a stop is always admitted, and one command's stop never cuts the cranial
 *     gate under another command's running sockets;
 *   - a lost stop drops the whole lattice;
 *   - and the governor that SHIPS refuses every drive (OI-FWHUB-09).
 *
 * Real np_module_map (inventory + placement) and np_protocol (socket expansion)
 * are linked. The PBM socket driver, the safety-SPI requests and an admitting
 * power governor are doubles. The production governor is linked renamed as
 * np_pbm_power_admit_production (see CMakeLists.txt).
 *
 * No FreeRTOS, no hardware. IEC 62304 Class B — SW-02 hub control.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../include/np_hub_types.h"
#include "../include/np_module_map.h"
#include "../include/np_protocol.h"
#include "../include/np_safety_spi.h"
#include "../include/np_socket_dispatch.h"

/* np_protocol.c's platform seams — same signatures the production contract
 * declares, so drift is a compile error here too. */
#include "np_sw02_platform_hal.h"

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

/* ── np_protocol.c link seams (never exercised: no blob is parsed here) ─────── */

np_hub_status_t np_proto_hal_get_device_serial(uint8_t *buf, size_t len)
{
    (void)buf; (void)len;
    return NP_HUB_ERR_NOT_PRESENT;
}

np_hub_status_t np_proto_hal_get_proto_pubkey(uint8_t *pub_key_out)
{
    memset(pub_key_out, 0, 32);
    return NP_HUB_OK;
}

int np_ed25519_verify(const uint8_t *msg, size_t msg_len,
                      const uint8_t *sig, const uint8_t *pub_key)
{
    (void)msg; (void)msg_len; (void)sig; (void)pub_key;
    return -1;
}

/* ── np_module_map fixtures ──────────────────────────────────────────────────── */

#define N_SOCKETS 80u

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

typedef enum { TILE_EMPTY, TILE_BASE, TILE_SMART, TILE_EEG, TILE_T1B } tile_kind_t;

static np_socket_geom_t g_geom[N_SOCKETS];
static tile_kind_t      g_tile[N_SOCKETS];

static np_hub_status_t inv_cb(uint16_t socket_id, void *ctx,
                              uint8_t *types_out, uint8_t max, uint8_t *count_out)
{
    (void)ctx; (void)max;
    uint8_t n = 0;
    switch (g_tile[socket_id]) {
    case TILE_BASE:     /* T1-A: 660 + 808, PD, NTC */
        types_out[n++] = NP_ELEM_LED_660;
        types_out[n++] = NP_ELEM_LED_808;
        types_out[n++] = NP_ELEM_PD_FORWARD;
        types_out[n++] = NP_ELEM_NTC;
        break;
    case TILE_SMART:    /* T1-C: + 1064 */
        types_out[n++] = NP_ELEM_LED_660;
        types_out[n++] = NP_ELEM_LED_808;
        types_out[n++] = NP_ELEM_LED_1064;
        types_out[n++] = NP_ELEM_PD_FORWARD;
        types_out[n++] = NP_ELEM_NTC;
        break;
    case TILE_EEG:      /* electrode only, no emitter */
        types_out[n++] = NP_ELEM_DUAL_ELECTRODE;
        break;
    case TILE_T1B:      /* T1-B as NP-HEX-ZM-001 §4a defines it: 660 + 808 around a
                         * dual-rated electrode, PD, NTC */
        types_out[n++] = NP_ELEM_LED_660;
        types_out[n++] = NP_ELEM_LED_808;
        types_out[n++] = NP_ELEM_DUAL_ELECTRODE;
        types_out[n++] = NP_ELEM_PD_FORWARD;
        types_out[n++] = NP_ELEM_NTC;
        break;
    default:
        break;
    }
    *count_out = n;
    return NP_HUB_OK;
}

static void plug(uint16_t socket, tile_kind_t kind)
{
    np_module_uid_t u;
    memset(&u, 0, sizeof u);
    u.b[0] = (uint8_t)(socket + 1U);
    u.b[7] = (uint8_t)(0x40U + (uint8_t)kind);
    g_tile[socket] = kind;
    bool changed = false;
    (void)np_module_map_apply_poll(socket, &u, 0x00, inv_cb, NULL, &changed);
}

/* ── Doubles: PBM socket driver ──────────────────────────────────────────────── */

#define LOG_MAX 512
typedef struct { char op; uint16_t socket; np_hub_mod_type_t type; } drv_event_t;

static drv_event_t g_log[LOG_MAX];
static int         g_log_n;
static int         g_fail_drive_socket = -1;   /* drive() fails for this socket */
static int         g_fail_stop_socket  = -1;   /* stop()  fails for this socket */

np_hub_status_t np_mod_pbm_socket_drive(uint16_t socket_id, np_hub_mod_type_t mod_type,
                                        const void *params, uint16_t len)
{
    (void)params; (void)len;
    if (g_log_n < LOG_MAX) { g_log[g_log_n++] = (drv_event_t){ 'D', socket_id, mod_type }; }
    return ((int)socket_id == g_fail_drive_socket) ? NP_HUB_ERR_MOD_FAULT : NP_HUB_OK;
}

np_hub_status_t np_mod_pbm_socket_stop(uint16_t socket_id)
{
    if (g_log_n < LOG_MAX) { g_log[g_log_n++] = (drv_event_t){ 'S', socket_id, NP_MOD_NONE }; }
    return ((int)socket_id == g_fail_stop_socket) ? NP_HUB_ERR_MOD_FAULT : NP_HUB_OK;
}

static int count_ops(char op)
{
    int n = 0;
    for (int i = 0; i < g_log_n; i++) { if (g_log[i].op == op) { n++; } }
    return n;
}

static bool was_stopped(uint16_t s)
{
    for (int i = 0; i < g_log_n; i++) {
        if (g_log[i].op == 'S' && g_log[i].socket == s) { return true; }
    }
    return false;
}

/* ── Doubles: safety-SPI requests ────────────────────────────────────────────── */

static uint16_t g_requested;          /* modelled like np_safety_spi.c */
static int      g_enable_calls, g_disable_calls;

void np_safety_spi_request_enable(uint16_t mask)  { g_requested |= mask;  g_enable_calls++;  }
void np_safety_spi_request_disable(uint16_t mask) { g_requested &= (uint16_t)~mask; g_disable_calls++; }

static bool cranial_requested(void) { return (g_requested & NP_SAFETY_EN_PBM_CRANIAL) != 0U; }

/* ── Doubles: power governor ─────────────────────────────────────────────────── */

bool np_pbm_power_admit_production(const np_session_cmd_t      *cmd,
                                   const np_sock_disp_socket_t *current);

static bool     g_admit = true;
static int      g_admit_calls;
static uint16_t g_admit_saw_active;   /* active sockets in `current` at last call */

bool np_pbm_power_admit(const np_session_cmd_t *cmd, const np_sock_disp_socket_t *current)
{
    (void)cmd;
    g_admit_calls++;
    g_admit_saw_active = 0;
    for (unsigned s = 0; s < NP_HEXMAP_MAX_SOCKETS; s++) {
        if (current[s].active) { g_admit_saw_active++; }
    }
    return g_admit;
}

/* ── Command builders ────────────────────────────────────────────────────────── */

static np_session_cmd_t sock_cmd(np_hub_mod_type_t type, const uint16_t *socks, unsigned n)
{
    np_session_cmd_t c;
    memset(&c, 0, sizeof c);
    c.mod_type    = type;
    c.slot_id     = NP_HUB_SLOT_NONE;
    c.target_kind = NP_PROTO_TARGET_SOCKET_MASK;
    for (unsigned i = 0; i < n; i++) {
        c.socket_mask[socks[i] / 8U] |= (uint8_t)(1U << (socks[i] % 8U));
    }
    return c;
}

static np_session_cmd_t base_cmd(const uint16_t *socks, unsigned n)
{
    np_session_cmd_t c = sock_cmd(NP_MOD_PBM_BASE, socks, n);
    const np_mod_pbm_base_params_t p = { .freq_code = 0x28, .duty = 0x20, .cur_a = 0x40, .cur_b = 0x40 };
    memcpy(c.params, &p, sizeof p);
    c.params_len = sizeof p;
    return c;
}

static np_session_cmd_t smart_cmd(const uint16_t *socks, unsigned n, uint8_t ch_mask)
{
    np_session_cmd_t c = sock_cmd(NP_MOD_PBM_SMART, socks, n);
    const np_mod_pbm_smart_params_t p = { .freq_code = 0x28, .duty = 0x20, .cur_a = 0x40,
                                          .cur_b = 0x40, .cur_c = 0x40, .ch_mask = ch_mask };
    memcpy(c.params, &p, sizeof p);
    c.params_len = sizeof p;
    return c;
}

static np_session_cmd_t stop_cmd(np_hub_mod_type_t type, const uint16_t *socks, unsigned n)
{
    return sock_cmd(type, socks, n);   /* params_len 0 = stop */
}

/* ── Fixture ─────────────────────────────────────────────────────────────────── */

static void setup(void)
{
    for (unsigned i = 0; i < N_SOCKETS; i++) {
        g_geom[i].present_in_helmet = true;
        g_geom[i].x_mm = (int16_t)i;
        g_geom[i].y_mm = 0;
        g_tile[i] = TILE_EMPTY;
    }
    (void)np_module_map_init(g_geom, (uint16_t)N_SOCKETS);

    /* 0-9 base, 10-19 smart, 20 EEG-only, 21-22 T1-B, 23+ empty. */
    for (uint16_t s = 0; s < 10; s++)  { plug(s, TILE_BASE);  }
    for (uint16_t s = 10; s < 20; s++) { plug(s, TILE_SMART); }
    plug(20, TILE_EEG);
    plug(21, TILE_T1B);
    plug(22, TILE_T1B);

    np_sock_disp_reset();
    g_log_n = 0;
    g_fail_drive_socket = g_fail_stop_socket = -1;
    g_requested = 0; g_enable_calls = g_disable_calls = 0;
    g_admit = true; g_admit_calls = 0; g_admit_saw_active = 0;
}

/* ── Tests ───────────────────────────────────────────────────────────────────── */

static void test_production_governor_is_closed(void)
{
    setup();
    const uint16_t s[] = { 1 };
    np_session_cmd_t c = base_cmd(s, 1);
    check(!np_pbm_power_admit_production(&c, np_sock_disp_socket(0)),
          "production governor refuses a one-tile base load (OI-FWHUB-09)");
    np_session_cmd_t c2 = smart_cmd(s, 1, 0x04);
    check(!np_pbm_power_admit_production(&c2, np_sock_disp_socket(0)),
          "production governor refuses a one-tile smart load");
}

static void test_power_refusal_drives_nothing(void)
{
    setup();
    g_admit = false;
    const uint16_t s[] = { 1, 2, 3 };
    np_session_cmd_t c = base_cmd(s, 3);
    check(np_sock_disp_command(&c) == NP_HUB_ERR_POWER_BUDGET, "refused load -> POWER_BUDGET");
    check(count_ops('D') == 0, "refused load: no socket driven");
    check(g_enable_calls == 0 && !cranial_requested(), "refused load: cranial enable never requested");
    check(np_sock_disp_active_count() == 0, "refused load: nothing active");
}

static void test_base_drive_all_sockets(void)
{
    setup();
    const uint16_t s[] = { 7, 2, 4 };           /* unsorted on purpose */
    np_session_cmd_t c = base_cmd(s, 3);
    c.start_ms = 1000; c.duration_ms = 5000;
    check(np_sock_disp_command(&c) == NP_HUB_OK, "base drive on 3 sockets -> OK");
    check(count_ops('D') == 3, "exactly 3 drives");
    check(g_log[0].socket == 2 && g_log[1].socket == 4 && g_log[2].socket == 7,
          "drives in ascending socket order");
    check(g_log[0].type == NP_MOD_PBM_BASE, "driven as base");
    check(np_sock_disp_active_count() == 3, "3 active");
    check(cranial_requested() && g_enable_calls == 1, "cranial enable requested once, after drive");
    check(np_sock_disp_socket(4)->stop_at_ms == 6000, "auto-stop = start + duration");
    check(g_admit_calls == 1, "governor consulted once");
}

/*
 * REQ-FWHUB-33, pinned for every modality that uses an electrode rather than
 * only for tDCS. NP-FW-MMSOCK-001 §3.6 is the reason: a lattice electrode is a
 * T1-B pod of at most ~1.02 cm², BES/tACS has no declared area and no geometry
 * gate, so the safety MCU would check it against the 25 cm² pad default — a
 * ~24x fail-open (RISK-MMSOCK-01, OI-MMSOCK-02). Admitting any of these here
 * "because it is just like PBM" is that hazard. The targets are aimed at a T1-B
 * socket, so placement is not what refuses them.
 *
 * THE STOP IS THE DISCRIMINATING CASE. A stop skips every gate after gate 1, so
 * only gate 1 can refuse it. A drive is refused by gate 1 too, but a mis-typed
 * drive would ALSO die at the params gates (its block is read as the wrong
 * struct), so the drive assertion alone cannot show gate 1 holds. Falsified:
 * admitting NP_MOD_BES_TACS at gate 1 fails the stop assertion and not the
 * drive one (NP-FW-MMSOCK-001 §11).
 */
static void test_not_socket_addressable(void)
{
    setup();
    const uint16_t s[] = { 21 };   /* a T1-B: carries NP_ELEM_DUAL_ELECTRODE */
    static const np_hub_mod_type_t k_electrode_mods[] = {
        NP_MOD_EEG, NP_MOD_BES_TACS, NP_MOD_TDCS, NP_MOD_VNS_HRV,
        NP_MOD_CVNS, NP_MOD_CLIN_TACS, NP_MOD_HD_TDCS, NP_MOD_QEEG_21CH,
    };
    const unsigned n_mods = (unsigned)(sizeof k_electrode_mods / sizeof k_electrode_mods[0]);

    unsigned drive_refused = 0U, stop_refused = 0U;
    for (unsigned i = 0; i < n_mods; i++) {
        np_session_cmd_t c = base_cmd(s, 1);          /* a well-formed params block */
        c.mod_type = k_electrode_mods[i];
        if (np_sock_disp_command(&c) == NP_HUB_ERR_INVALID_ARG) { drive_refused++; }
        np_session_cmd_t st = stop_cmd(k_electrode_mods[i], s, 1);
        if (np_sock_disp_command(&st) == NP_HUB_ERR_INVALID_ARG) { stop_refused++; }
    }
    check(drive_refused == n_mods, "every electrode modality's drive on a T1-B socket -> INVALID_ARG");
    check(stop_refused == n_mods, "every electrode modality's stop on a T1-B socket -> INVALID_ARG");

    np_session_cmd_t c = base_cmd(s, 1);
    c.mod_type = NP_MOD_PBM_1170NM;
    check(np_sock_disp_command(&c) == NP_HUB_ERR_INVALID_ARG, "1170 nm on a socket target -> INVALID_ARG");
    check(count_ops('D') == 0 && g_admit_calls == 0, "nothing driven, governor not asked");
    check(g_enable_calls == 0 && g_requested == 0U, "no enable of any kind requested");

    np_session_cmd_t slot = base_cmd(s, 1);
    slot.target_kind = NP_PROTO_TARGET_SLOT;
    check(np_sock_disp_command(&slot) == NP_HUB_ERR_INVALID_ARG, "slot-kind command refused here (no cross-routing)");
}

/*
 * PBM on a T1-B's emitters is admitted exactly like PBM on a T1-A: the
 * electrode element is neither required nor a reason to refuse. This is the
 * behaviour NP-FW-MMSOCK-001 §3.2 recommends keeping (P-3). If the principal
 * decides P-3 against, this is the test to invert.
 */
static void test_pbm_on_t1b_admitted(void)
{
    setup();
    const uint16_t s[] = { 21, 22, 3 };   /* two T1-B and one T1-A in one command */
    np_session_cmd_t c = base_cmd(s, 3);
    check(np_sock_disp_command(&c) == NP_HUB_OK, "base PBM on T1-B + T1-A sockets -> OK");
    check(count_ops('D') == 3 && np_sock_disp_active_count() == 3, "all three driven");
    check(np_sock_disp_socket(21)->mod_type == NP_MOD_PBM_BASE, "T1-B socket recorded as base PBM");
    check(cranial_requested(), "cranial enable requested for a T1-B drive");

    setup();
    const uint16_t t[] = { 21 };
    np_session_cmd_t sm = smart_cmd(t, 1, 0x04);   /* T1-B carries no 1064 nm */
    check(np_sock_disp_command(&sm) == NP_HUB_ERR_NOT_PRESENT,
          "1064 nm on a T1-B -> NOT_PRESENT (placement by emitter, not by tile name)");
}

static void test_malformed(void)
{
    setup();
    np_session_cmd_t empty = base_cmd(NULL, 0);
    check(np_sock_disp_command(&empty) == NP_HUB_ERR_INVALID_ARG, "empty mask -> INVALID_ARG");

    const uint16_t s[] = { 1 };
    np_session_cmd_t shortp = base_cmd(s, 1);
    shortp.params_len = 3;
    check(np_sock_disp_command(&shortp) == NP_HUB_ERR_INVALID_ARG, "short base params -> INVALID_ARG");

    np_session_cmd_t longp = base_cmd(s, 1);
    longp.params_len = sizeof(np_mod_pbm_smart_params_t);
    check(np_sock_disp_command(&longp) == NP_HUB_ERR_INVALID_ARG, "base with smart-sized params -> INVALID_ARG");

    const uint16_t sm[] = { 11 };
    np_session_cmd_t badmask = smart_cmd(sm, 1, 0x09);
    check(np_sock_disp_command(&badmask) == NP_HUB_ERR_INVALID_ARG,
          "smart ch_mask with a real channel plus bit 3 -> INVALID_ARG (not silently masked)");
    np_session_cmd_t nomask = smart_cmd(sm, 1, 0x00);
    check(np_sock_disp_command(&nomask) == NP_HUB_ERR_INVALID_ARG, "smart lighting nothing -> INVALID_ARG");

    np_session_cmd_t dark = base_cmd(s, 1);
    ((np_mod_pbm_base_params_t *)(void *)dark.params)->cur_a = 0;
    ((np_mod_pbm_base_params_t *)(void *)dark.params)->cur_b = 0;
    check(np_sock_disp_command(&dark) == NP_HUB_ERR_INVALID_ARG, "base lighting nothing -> INVALID_ARG");

    check(count_ops('D') == 0 && !cranial_requested(), "no malformed command drove anything");
}

static void test_placement_all_or_nothing(void)
{
    setup();
    /* 1064 asked of three sockets, one of which is a base tile (no 1064). */
    const uint16_t s[] = { 11, 12, 3 };
    np_session_cmd_t c = smart_cmd(s, 3, 0x04);
    check(np_sock_disp_command(&c) == NP_HUB_ERR_NOT_PRESENT, "one socket lacks 1064 -> NOT_PRESENT");
    check(count_ops('D') == 0, "placement failure drives NO socket, not the capable two");

    const uint16_t e[] = { 1, 30 };             /* 30 is empty */
    np_session_cmd_t c2 = base_cmd(e, 2);
    check(np_sock_disp_command(&c2) == NP_HUB_ERR_NOT_PRESENT, "empty socket -> NOT_PRESENT");

    const uint16_t eeg[] = { 20 };
    np_session_cmd_t c3 = base_cmd(eeg, 1);
    check(np_sock_disp_command(&c3) == NP_HUB_ERR_NOT_PRESENT, "electrode-only tile -> NOT_PRESENT");

    const uint16_t out[] = { 100 };             /* in the wire domain, not this shell */
    np_session_cmd_t c4 = base_cmd(out, 1);
    check(np_sock_disp_command(&c4) == NP_HUB_ERR_NOT_PRESENT, "socket beyond the shell -> NOT_PRESENT");

    check(count_ops('D') == 0 && g_admit_calls == 0, "governor never asked about an unplaceable load");

    /* Smart 660+808 only is satisfiable by a smart tile but a base tile too. */
    const uint16_t mix[] = { 3, 13 };
    np_session_cmd_t c5 = smart_cmd(mix, 2, 0x03);
    check(np_sock_disp_command(&c5) == NP_HUB_OK, "smart 660+808 on base+smart tiles -> OK (placement is by emitter)");
}

static void test_driver_fault_rolls_back(void)
{
    setup();
    const uint16_t s[] = { 1, 2, 3, 4 };
    g_fail_drive_socket = 3;
    np_session_cmd_t c = base_cmd(s, 4);
    check(np_sock_disp_command(&c) == NP_HUB_ERR_MOD_FAULT, "3rd socket fails -> MOD_FAULT");
    check(was_stopped(1) && was_stopped(2) && was_stopped(3), "started sockets and the failing one are stopped");
    check(!was_stopped(4) && count_ops('D') == 3, "socket 4 never touched");
    check(np_sock_disp_active_count() == 0, "nothing left active");
    check(g_enable_calls == 0 && !cranial_requested(), "cranial enable never requested");
}

static void test_rollback_stop_failure_drops_lattice(void)
{
    setup();
    const uint16_t other[] = { 8 };
    np_session_cmd_t co = base_cmd(other, 1);
    (void)np_sock_disp_command(&co);              /* an unrelated running socket */

    const uint16_t s[] = { 1, 2, 3 };
    g_fail_drive_socket = 3;
    g_fail_stop_socket  = 2;                       /* rollback cannot quiesce 2 */
    np_session_cmd_t c = base_cmd(s, 3);
    check(np_sock_disp_command(&c) == NP_HUB_ERR_MOD_FAULT, "drive fault + rollback stop fault -> MOD_FAULT");
    check(was_stopped(8), "unrelated socket stopped: control of an emitter was lost");
    check(np_sock_disp_active_count() == 0 && !cranial_requested(), "whole lattice released");
}

static void test_auto_stop_failure_drops_lattice(void)
{
    setup();
    const uint16_t a[] = { 1 };
    const uint16_t b[] = { 2 };
    np_session_cmd_t ca = base_cmd(a, 1); ca.duration_ms = 1000;
    np_session_cmd_t cb = base_cmd(b, 1);          /* open-ended */
    (void)np_sock_disp_command(&ca);
    (void)np_sock_disp_command(&cb);
    g_fail_stop_socket = 1;
    np_sock_disp_process_stops(1000);
    check(was_stopped(2), "failed auto-stop stops every other socket");
    check(np_sock_disp_active_count() == 0 && !cranial_requested(), "failed auto-stop releases the gate");
}

static void test_stop_keeps_gate_for_others(void)
{
    setup();
    const uint16_t a[] = { 1, 2 };
    const uint16_t b[] = { 5, 6 };
    np_session_cmd_t ca = base_cmd(a, 2);
    np_session_cmd_t cb = base_cmd(b, 2);
    (void)np_sock_disp_command(&ca);
    (void)np_sock_disp_command(&cb);
    check(g_admit_saw_active == 2, "governor saw A's 2 sockets when admitting B");

    np_session_cmd_t sa = stop_cmd(NP_MOD_PBM_BASE, a, 2);
    g_admit = false;   /* a stop must not depend on the governor */
    int admits_before = g_admit_calls;
    check(np_sock_disp_command(&sa) == NP_HUB_OK, "stop A -> OK even with governor refusing");
    check(g_admit_calls == admits_before, "governor not consulted for a stop");
    check(np_sock_disp_active_count() == 2, "B still active");
    check(cranial_requested(), "cranial gate still requested while B runs");

    np_session_cmd_t sb = stop_cmd(NP_MOD_PBM_BASE, b, 2);
    (void)np_sock_disp_command(&sb);
    check(np_sock_disp_active_count() == 0 && !cranial_requested(), "last stop releases the cranial gate");

    np_session_cmd_t again = stop_cmd(NP_MOD_PBM_BASE, b, 2);
    check(np_sock_disp_command(&again) == NP_HUB_OK, "stopping idle sockets is a no-op OK");
}

static void test_auto_stop(void)
{
    setup();
    const uint16_t a[] = { 1 };
    const uint16_t b[] = { 2 };
    np_session_cmd_t ca = base_cmd(a, 1); ca.start_ms = 0; ca.duration_ms = 1000;
    np_session_cmd_t cb = base_cmd(b, 1); cb.start_ms = 0; cb.duration_ms = 3000;
    (void)np_sock_disp_command(&ca);
    (void)np_sock_disp_command(&cb);
    check(np_sock_disp_next_stop_ms(0) == 1000, "next stop is the earliest");
    np_sock_disp_process_stops(999);
    check(np_sock_disp_active_count() == 2, "nothing stops early");
    np_sock_disp_process_stops(1000);
    check(was_stopped(1) && !was_stopped(2), "socket 1 auto-stops at its deadline");
    check(cranial_requested(), "gate held for socket 2");
    check(np_sock_disp_next_stop_ms(1000) == 3000, "next stop moves on");
    np_sock_disp_process_stops(3000);
    check(np_sock_disp_active_count() == 0 && !cranial_requested(), "last auto-stop releases the gate");
    check(np_sock_disp_next_stop_ms(3000) == 0, "no stop pending");

    setup();
    np_session_cmd_t open_ended = base_cmd(a, 1);   /* duration 0 = until stopped */
    (void)np_sock_disp_command(&open_ended);
    np_sock_disp_process_stops(0xFFFFFFFFu);
    check(np_sock_disp_active_count() == 1, "duration 0 never auto-stops");
}

static void test_lost_stop_drops_lattice(void)
{
    setup();
    const uint16_t a[] = { 1 };
    const uint16_t b[] = { 5, 6 };
    np_session_cmd_t ca = base_cmd(a, 1);
    np_session_cmd_t cb = base_cmd(b, 2);
    (void)np_sock_disp_command(&ca);
    (void)np_sock_disp_command(&cb);

    g_fail_stop_socket = 1;
    np_session_cmd_t sa = stop_cmd(NP_MOD_PBM_BASE, a, 1);
    check(np_sock_disp_command(&sa) == NP_HUB_ERR_MOD_FAULT, "failed stop -> MOD_FAULT");
    check(was_stopped(5) && was_stopped(6), "every other socket stopped too");
    check(np_sock_disp_active_count() == 0, "nothing recorded active");
    check(!cranial_requested(), "cranial gate released although B had been running");
}

static void test_type_switch_stops_first(void)
{
    setup();
    const uint16_t s[] = { 12 };                 /* smart tile */
    np_session_cmd_t base = base_cmd(s, 1);
    (void)np_sock_disp_command(&base);
    g_log_n = 0;
    np_session_cmd_t smart = smart_cmd(s, 1, 0x07);
    check(np_sock_disp_command(&smart) == NP_HUB_OK, "re-drive socket as smart -> OK");
    check(g_log_n == 2 && g_log[0].op == 'S' && g_log[1].op == 'D' &&
          g_log[1].type == NP_MOD_PBM_SMART, "old type stopped before new type driven");
    check(np_sock_disp_active_count() == 1, "still one socket, counted once");

    g_log_n = 0;
    (void)np_sock_disp_command(&smart);
    check(g_log_n == 1 && g_log[0].op == 'D', "same-type re-drive does not stop first");
}

static void test_stop_all(void)
{
    setup();
    const uint16_t s[] = { 1, 2, 11 };
    np_session_cmd_t c = base_cmd(s, 3);
    (void)np_sock_disp_command(&c);
    np_sock_disp_stop_all();
    check(was_stopped(1) && was_stopped(2) && was_stopped(11), "stop_all stops every active socket");
    check(np_sock_disp_active_count() == 0 && !cranial_requested(), "stop_all releases the gate");
}

int main(void)
{
    test_production_governor_is_closed();
    test_power_refusal_drives_nothing();
    test_base_drive_all_sockets();
    test_not_socket_addressable();
    test_pbm_on_t1b_admitted();
    test_malformed();
    test_placement_all_or_nothing();
    test_driver_fault_rolls_back();
    test_stop_keeps_gate_for_others();
    test_auto_stop();
    test_lost_stop_drops_lattice();
    test_rollback_stop_failure_drops_lattice();
    test_auto_stop_failure_drops_lattice();
    test_type_switch_stops_first();
    test_stop_all();

    if (g_failures != 0) {
        printf("\n%d FAILURE(S)\n", g_failures);
        return 1;
    }
    printf("\nAll socket-dispatch tests passed.\n");
    return 0;
}
