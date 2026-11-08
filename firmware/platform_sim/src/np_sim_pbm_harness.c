/*
 * NeurOne Firmware-in-the-Loop Simulator — PBM harness (NP-SIM-FW-001 §8 step 1)
 * HOST ONLY.  The command interpreter the spike process and the tests share:
 * one text line in, events out through the sim sink.  The command list is in
 * spike/np_sim_pbm_spike.c.
 */
#include "np_sim_bus.h"
#include "np_sim_pbm_harness.h"

#include "np_hub_types.h"
#include "np_module_map.h"
#include "np_protocol.h"
#include "np_safety_spi.h"
#include "np_socket_dispatch.h"
#include "np_sw02_platform_hal.h"
#include "np_socket_layouts.h"
#include "np_layout_bind.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The helmet's layout, bound by np_layout_bind() from the generated table (REQ-FWHUB-64): the simulator holds no
 * socket count of its own.  Hardware version 1, the default, until a scenario says otherwise. */
static const np_socket_layout_t *g_layout;

/* The module map learns what is in a socket by asking it.  Here the sim module
 * is asked, so the placement check the dispatcher runs sees the sim's inventory
 * and nothing else. */
static np_hub_status_t inv_cb(uint16_t socket_id, void *ctx,
                              uint8_t *types_out, uint8_t max, uint8_t *count_out)
{
    (void)ctx; (void)max;
    uint8_t n = 0;
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_PBM_BASE, socket_id);
    if (m != NULL && m->present && m->kind == NP_SIM_KIND_PBM_BASE) {
        types_out[n++] = NP_ELEM_LED_660;
        types_out[n++] = NP_ELEM_LED_808;
        types_out[n++] = NP_ELEM_PD_FORWARD;
        types_out[n++] = NP_ELEM_NTC;
    }
    *count_out = n;
    return NP_HUB_OK;
}

static void poll_socket(uint16_t s)
{
    np_module_uid_t u;
    memset(&u, 0, sizeof u);
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_PBM_BASE, s);
    if (m != NULL && m->present) { u.b[0] = (uint8_t)(s + 1U); u.b[7] = 0x41U; }
    bool changed = false;
    (void)np_module_map_apply_poll(s, &u, 0x00, inv_cb, NULL, &changed);
}

static np_session_cmd_t base_cmd(const uint16_t *socks, unsigned n, bool stop)
{
    np_session_cmd_t c;
    memset(&c, 0, sizeof c);
    c.mod_type    = NP_MOD_PBM_BASE;
    c.slot_id     = NP_HUB_SLOT_NONE;
    c.target_kind = NP_PROTO_TARGET_SOCKET_MASK;
    for (unsigned i = 0; i < n; i++) {
        c.socket_mask[socks[i] / 8U] |= (uint8_t)(1U << (socks[i] % 8U));
    }
    if (!stop) {
        const np_mod_pbm_base_params_t p = { .freq_code = 0x28, .duty = 0x20,
                                             .cur_a = 0x40, .cur_b = 0x40 };
        memcpy(c.params, &p, sizeof p);
        c.params_len = sizeof p;
    }
    return c;
}

static unsigned parse_list(const char *s, uint16_t *out, unsigned max)
{
    unsigned n = 0;
    while (*s != '\0' && n < max) {
        char *end;
        long v = strtol(s, &end, 10);
        if (end == s || v < 0 || v >= (long)NP_HEXMAP_MAX_SOCKETS) { break; }
        out[n++] = (uint16_t)v;
        s = (*end == ',') ? end + 1 : end;
    }
    return n;
}

static void result(const char *cmd, int code)
{
    np_sim_emit("{\"ev\":\"result\",\"cmd\":\"%s\",\"code\":%d,\"active\":%u,\"requested_enables\":%u}",
                cmd, code, (unsigned)np_sock_disp_active_count(),
                (unsigned)np_safety_spi_get_requested_mask());
}

static void status(void)
{
    for (unsigned s = 0; s < NP_HEXMAP_MAX_SOCKETS; s++) {
        np_sim_module_t *m = np_sim_find(NP_SIM_KIND_PBM_BASE, (uint16_t)s);
        if (m == NULL) { continue; }
        np_sim_emit("{\"ev\":\"module_state\",\"id\":\"%s:%u\",\"present\":%s,\"driven\":%s,"
                    "\"cur_a\":%u,\"cur_b\":%u,\"ntc_c\":%.1f}",
                    np_sim_kind_name(m->kind), s, m->present ? "true" : "false",
                    m->driven ? "true" : "false", m->cur_a, m->cur_b, (double)m->ntc_c);
    }
    np_simh_stim_status();
    np_simh_eeg_status();
    np_simh_vns_status();
    np_simh_audio_status();
    np_simh_visual_status();
    np_simh_ins_status();
    np_simh_cvns_status();
    np_simh_safety_status();
    result("status", 0);
}

void np_simh_init(void)
{
    np_sim_reset();
    np_sim_governor_open_for_test(false);   /* the production governor, which refuses every load */
    /* The firmware's own binding, as bring-up does it (REQ-FWHUB-64): the layout of hardware version 1. */
    (void)np_layout_bind(1U);
    g_layout = np_layout_bound();
    np_sock_disp_reset();
    np_simh_stim_init();
    np_simh_eeg_init();
    np_simh_vns_init();
    np_simh_audio_init();
    np_simh_visual_init();
    np_simh_ins_init();
    np_simh_cvns_init();
    np_simh_safety_init();
}

void np_simh_line(char *line)
{
    char *save = NULL;
    char *cmd = strtok_r(line, " \t\r\n", &save);
    if (cmd == NULL) { return; }
    char *a = strtok_r(NULL, " \t\r\n", &save);
    char *b = strtok_r(NULL, " \t\r\n", &save);
    char *c = strtok_r(NULL, " \t\r\n", &save);
    char *d = strtok_r(NULL, " \t\r\n", &save);
    char *e = strtok_r(NULL, "\r\n", &save);   /* the rest of the line: one token for most commands, three for vns */

    /* The other modules' commands first.  "fail" and "reading" are shared: an op name implies its module kind. */
    if (np_simh_stim_line(cmd, a, b, c, d, e)) { return; }
    if (np_simh_eeg_line(cmd, a, b, c, d, e)) { return; }
    if (np_simh_vns_line(cmd, a, b, c, d, e)) { return; }
    if (np_simh_audio_line(cmd, a, b, c, d, e)) { return; }
    if (np_simh_visual_line(cmd, a, b, c, d, e)) { return; }
    if (np_simh_ins_line(cmd, a, b, c, d, e)) { return; }
    if (np_simh_stubs_line(cmd, a, b)) { return; }
    if (np_simh_cvns_line(cmd, a, b, c, d, e)) { return; }
    if (np_simh_safety_line(cmd, a, b, c)) { return; }

    uint16_t socks[NP_HEXMAP_MAX_SOCKETS];
    unsigned n;

    if (strcmp(cmd, "add") == 0 && a && b) {
        uint16_t s = (uint16_t)atoi(a);
        if (!np_socket_layout_valid_address(g_layout, s)) {
            np_sim_emit("{\"ev\":\"error\",\"what\":\"not a valid address: %s\",\"hardware_version\":%d,\"socket\":%u}",
                        g_layout != NULL ? "past this helmet's lattice" : "no layout is bound",
                        g_layout != NULL ? (int)g_layout->hardware_version : -1, (unsigned)s);
        } else if (np_sim_add(NP_SIM_KIND_PBM_BASE, s, (float)atof(b)) != NULL) { poll_socket(s); }
    } else if ((strcmp(cmd, "drive") == 0 || strcmp(cmd, "stop") == 0) && a) {
        n = parse_list(a, socks, NP_HEXMAP_MAX_SOCKETS);
        np_session_cmd_t cm = base_cmd(socks, n, cmd[0] == 's');
        result(cmd, (int)np_sock_disp_command(&cm));
    } else if (strcmp(cmd, "fail") == 0 && a && b && c && d) {
        int op = np_sim_op_from_name(b);
        if (op >= 0) {   /* an optional 5th word: answer that many calls normally first */
            np_sim_set_response_after(np_sim_op_kind((np_sim_op_t)op), (uint16_t)atoi(a), (np_sim_op_t)op, atoi(c), atoi(d), e != NULL ? atoi(e) : 0);
        }
    } else if (strcmp(cmd, "reading") == 0 && a && b && c) {
        int op = np_sim_op_from_name(b);
        if (op >= 0) { np_sim_set_reading(np_sim_op_kind((np_sim_op_t)op), (uint16_t)atoi(a), (np_sim_op_t)op, (float)atof(c)); }
    } else if ((strcmp(cmd, "absent") == 0 || strcmp(cmd, "present") == 0) && a) {
        uint16_t s = (uint16_t)atoi(a);
        np_sim_set_present(NP_SIM_KIND_PBM_BASE, s, cmd[0] == 'p');
        poll_socket(s);
    } else if (strcmp(cmd, "report") == 0 && a) {
        /* The safety MCU's report names the helmet's hardware version: the firmware's own follow-the-report path. */
        bool rebound = false;
        np_hub_status_t rc = np_layout_follow_report((uint8_t)atoi(a), true, &rebound);
        g_layout = np_layout_bound();
        np_sim_emit("{\"ev\":\"layout\",\"reported\":%d,\"rebound\":%s,\"code\":%d,\"bound_version\":%d}",
                    atoi(a), rebound ? "true" : "false", (int)rc, g_layout != NULL ? (int)g_layout->hardware_version : -1);
        if (rebound) {                        /* the module map was re-initialised: poll what is there again */
            for (unsigned s = 0; s < NP_HEXMAP_MAX_SOCKETS; s++) {
                if (np_sim_find(NP_SIM_KIND_PBM_BASE, (uint16_t)s) != NULL) { poll_socket((uint16_t)s); }
            }
        }
    } else if (strcmp(cmd, "clear") == 0 && a) {
        np_sim_clear(NP_SIM_KIND_PBM_BASE, (uint16_t)atoi(a));
    } else if (strcmp(cmd, "status") == 0) {
        status();
    } else if (strcmp(cmd, "quit") == 0) {
        exit(0);
    } else {
        np_sim_emit("{\"ev\":\"error\",\"what\":\"unknown or incomplete command\"}");
    }
}

