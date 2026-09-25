/*
 * NeurOne Hub Control Program — Socket-Indexed Dispatch Registry
 * Document: NP-FW-HUB-001 Rev 2 §3.3, §5.6 (OI-FWHUB-01)
 *
 * See np_socket_dispatch.h for the admission order and why each step is
 * all-or-nothing. No FreeRTOS dependency — host-tested by
 * tests/np_socket_dispatch_tests.c.
 */

#include "np_socket_dispatch.h"
#include "np_protocol.h"
#include "np_safety_spi.h"
#include <string.h>

/* ── State ───────────────────────────────────────────────────────────────────── */

static np_sock_disp_socket_t s_sock[NP_HEXMAP_MAX_SOCKETS];
static uint16_t              s_active_count;

/* Scratch for one command: the expanded socket list and its placement
 * requirements (≤ 3 emitter types per socket). Static, not stack: 128 × 3
 * requirements is ~3 KiB, which does not belong on the runner task's stack. */
static uint16_t           s_list[NP_HEXMAP_MAX_SOCKETS];
static np_placement_req_t s_reqs[NP_HEXMAP_MAX_SOCKETS * 3U];
static bool               s_started[NP_HEXMAP_MAX_SOCKETS];

/* ── Helpers ─────────────────────────────────────────────────────────────────── */

static bool is_socket_addressable(np_hub_mod_type_t t)
{
    return t == NP_MOD_PBM_BASE || t == NP_MOD_PBM_SMART;
}

static uint16_t params_size_for(np_hub_mod_type_t t)
{
    return (t == NP_MOD_PBM_SMART) ? (uint16_t)sizeof(np_mod_pbm_smart_params_t)
                                   : (uint16_t)sizeof(np_mod_pbm_base_params_t);
}

/*
 * required_emitters — the element types a DRIVE command asks to light, as an
 * array of NP_ELEM_* values. Returns the count (0..3), or -1 if the params
 * are malformed.
 *
 * Base modules have no channel mask: an emitter is asked for iff its commanded
 * irradiance is non-zero. Smart modules light exactly the ch_mask channels, and
 * ch_mask bits above bit 2 name channels no module has.
 */
static int required_emitters(np_hub_mod_type_t t, const uint8_t *params,
                             np_elem_type_t out[3])
{
    int n = 0;
    if (t == NP_MOD_PBM_BASE) {
        const np_mod_pbm_base_params_t *p =
            (const np_mod_pbm_base_params_t *)(const void *)params;
        if (p->irr_a != 0U) { out[n++] = NP_ELEM_LED_660; }
        if (p->irr_b != 0U) { out[n++] = NP_ELEM_LED_808; }
    } else {
        const np_mod_pbm_smart_params_t *p =
            (const np_mod_pbm_smart_params_t *)(const void *)params;
        if ((p->ch_mask & (uint8_t)~0x07U) != 0U) {
            return -1;
        }
        if ((p->ch_mask & 0x01U) != 0U) { out[n++] = NP_ELEM_LED_660;  }
        if ((p->ch_mask & 0x02U) != 0U) { out[n++] = NP_ELEM_LED_808;  }
        if ((p->ch_mask & 0x04U) != 0U) { out[n++] = NP_ELEM_LED_1064; }
    }
    return n;
}

static void mark_inactive(uint16_t s)
{
    if (s_sock[s].active) {
        s_active_count--;
    }
    memset(&s_sock[s], 0, sizeof s_sock[s]);
}

/* Stop every active socket, ignoring individual stop failures, and release
 * the cranial enable. Used both at session end and when control of an
 * emitter has been lost — in the latter case the safety MCU's whole-lattice
 * cut is what actually guarantees the emitter goes dark. */
static void stop_everything(void)
{
    for (uint16_t s = 0U; s < NP_HEXMAP_MAX_SOCKETS; s++) {
        if (s_sock[s].active) {
            (void)np_mod_pbm_socket_stop(s);
            mark_inactive(s);
        }
    }
    np_safety_spi_request_disable(NP_SAFETY_EN_PBM_CRANIAL);
}

/*
 * stop_socket — stop one socket. Returns false if the driver refused, in
 * which case firmware no longer knows the emitter is dark and the caller
 * must fall back to stop_everything().
 */
static bool stop_socket(uint16_t s)
{
    if (!s_sock[s].active) {
        return true;
    }
    np_hub_status_t rc = np_mod_pbm_socket_stop(s);
    mark_inactive(s);
    return rc == NP_HUB_OK;
}

static void release_cranial_if_idle(void)
{
    if (s_active_count == 0U) {
        np_safety_spi_request_disable(NP_SAFETY_EN_PBM_CRANIAL);
    }
}

/* ── Public API ──────────────────────────────────────────────────────────────── */

void np_sock_disp_reset(void)
{
    memset(s_sock, 0, sizeof s_sock);
    s_active_count = 0U;
}

np_hub_status_t np_sock_disp_command(const np_session_cmd_t *cmd)
{
    if (cmd == NULL || cmd->target_kind != NP_PROTO_TARGET_SOCKET_MASK) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    /* 1. Only a driven lattice emitter is socket-addressable. Electrode
     *    modalities (EEG, tES, VNS) are refused here deliberately, not merely
     *    unimplemented: a lattice electrode is a T1-B pod, and BES/tACS has no
     *    declared area or geometry gate, so the safety MCU would check it
     *    against the 25 cm² pad default (NP-FW-MMSOCK-001 §3.6, OI-MMSOCK-02). */
    if (!is_socket_addressable(cmd->mod_type)) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    /* 2. Expand. The bitmap is duplicate-free by construction, which is what
     *    keeps a midline socket named by two overlapping zones from being
     *    driven (and dosed) twice. */
    uint16_t n = 0U;
    if (np_protocol_socket_expand(cmd, s_list, NP_HEXMAP_MAX_SOCKETS, &n) != NP_HUB_OK
        || n == 0U) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    /* 3. STOP: always admitted. */
    if (cmd->params_len == 0U) {
        bool lost = false;
        for (uint16_t i = 0U; i < n; i++) {
            if (!stop_socket(s_list[i])) {
                lost = true;
            }
        }
        if (lost) {
            stop_everything();
            return NP_HUB_ERR_MOD_FAULT;
        }
        release_cranial_if_idle();
        return NP_HUB_OK;
    }

    /* 4. Params must be exactly the struct the mod_type defines. A short block
     *    would be read past its end; a long one means the producer and this
     *    firmware disagree about the struct. */
    if (cmd->params_len != params_size_for(cmd->mod_type)) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    np_elem_type_t need[3];
    int n_need = required_emitters(cmd->mod_type, cmd->params, need);
    if (n_need <= 0) {
        /* Malformed, or a drive command that lights nothing — which UHDR would
         * otherwise record as a delivered PBM modality. */
        return NP_HUB_ERR_INVALID_ARG;
    }

    /* 5. Placement, all sockets, before any hardware is touched. One
     *    requirement per (socket, emitter): check_placement is satisfied by ANY
     *    type in a mask, and the command needs ALL of them. */
    uint16_t n_reqs = 0U;
    for (uint16_t i = 0U; i < n; i++) {
        for (int k = 0; k < n_need; k++) {
            s_reqs[n_reqs].socket_id = (uint8_t)s_list[i];
            s_reqs[n_reqs].type_mask = NP_ELEM_BIT(need[k]);
            n_reqs++;
        }
    }
    uint16_t n_fail = 0U;
    if (np_module_map_check_placement(s_reqs, n_reqs, NULL, 0U, &n_fail) != NP_HUB_OK) {
        return NP_HUB_ERR_NOT_PRESENT;
    }

    /* 6. Power (OI-HEXTILE-09). */
    if (!np_pbm_power_admit(cmd, s_sock)) {
        return NP_HUB_ERR_POWER_BUDGET;
    }

    /* 7. Drive. A socket already driven by a DIFFERENT module type is stopped
     *    first: base and smart are driven through different hardware paths, and
     *    reprogramming one does not quiesce the other. */
    uint32_t stop_at = (cmd->duration_ms > 0U) ? cmd->start_ms + cmd->duration_ms : 0U;
    memset(s_started, 0, sizeof s_started);

    for (uint16_t i = 0U; i < n; i++) {
        const uint16_t s = s_list[i];

        if (s_sock[s].active && s_sock[s].mod_type != cmd->mod_type) {
            if (!stop_socket(s)) {
                stop_everything();
                return NP_HUB_ERR_MOD_FAULT;
            }
        }

        np_hub_status_t rc = np_mod_pbm_socket_drive(s, cmd->mod_type,
                                                     cmd->params, cmd->params_len);
        if (rc != NP_HUB_OK) {
            /* Roll back every socket this command touched, including this one
             * (the driver may have got part-way). A socket this command merely
             * re-programmed is stopped too — its previous preset cannot be
             * restored reliably, and dark is the safe direction. */
            bool lost = (np_mod_pbm_socket_stop(s) != NP_HUB_OK);
            mark_inactive(s);
            for (uint16_t j = 0U; j < i; j++) {
                if (s_started[s_list[j]] && !stop_socket(s_list[j])) {
                    lost = true;
                }
            }
            if (lost) {
                stop_everything();
            } else {
                release_cranial_if_idle();
            }
            return NP_HUB_ERR_MOD_FAULT;
        }

        if (!s_sock[s].active) {
            s_active_count++;
        }
        s_sock[s].active     = true;
        s_sock[s].mod_type   = cmd->mod_type;
        s_sock[s].params_len = (uint8_t)cmd->params_len;
        memcpy(s_sock[s].params, cmd->params, cmd->params_len);
        s_sock[s].stop_at_ms = stop_at;
        s_started[s] = true;
    }

    /* Every addressed emitter is configured; only now open the lattice gate. */
    np_safety_spi_request_enable(NP_SAFETY_EN_PBM_CRANIAL);
    return NP_HUB_OK;
}

void np_sock_disp_process_stops(uint32_t now_ms)
{
    bool lost = false;
    bool any  = false;
    for (uint16_t s = 0U; s < NP_HEXMAP_MAX_SOCKETS; s++) {
        if (!s_sock[s].active || s_sock[s].stop_at_ms == 0U ||
            now_ms < s_sock[s].stop_at_ms) {
            continue;
        }
        any = true;
        if (!stop_socket(s)) {
            lost = true;
        }
    }
    if (lost) {
        stop_everything();
    } else if (any) {
        release_cranial_if_idle();
    }
}

uint32_t np_sock_disp_next_stop_ms(uint32_t now_ms)
{
    uint32_t best = 0U;
    for (uint16_t s = 0U; s < NP_HEXMAP_MAX_SOCKETS; s++) {
        uint32_t t = s_sock[s].stop_at_ms;
        if (s_sock[s].active && t > now_ms && (best == 0U || t < best)) {
            best = t;
        }
    }
    return best;
}

void np_sock_disp_stop_all(void)
{
    stop_everything();
}

uint16_t np_sock_disp_active_count(void)
{
    return s_active_count;
}

const np_sock_disp_socket_t *np_sock_disp_socket(uint16_t socket_id)
{
    return (socket_id < NP_HEXMAP_MAX_SOCKETS) ? &s_sock[socket_id] : NULL;
}
