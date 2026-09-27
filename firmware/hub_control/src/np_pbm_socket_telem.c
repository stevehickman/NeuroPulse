/*
 * NeurOne Hub Control — Socket-path PBM telemetry, dose metering and the idle
 * tile health pass (OI-FWHUB-10)
 * Document: NP-FW-HUB-001 Rev 15 §6.8
 *
 * See np_pbm_socket_telem.h for what reaches UHDR and SHDR, and why.
 */

#include "np_pbm_socket_telem.h"
#include "np_module_map.h"
#include "np_session_log.h"
#include "np_socket_dispatch.h"   /* np_mod_pbm_socket_disable_channel() */
#include "np_pbm_config.h"
#include "np_pbm_dose.h"
#include "np_pbm_hal.h"
#include "np_pbm_types.h"
#include "np_sw02_platform_hal.h"
#include "np_session_lease.h"      /* maintenance runs outside sessions only */
#include <string.h>

typedef char _np_pst_pool_matches_pbm_bound[
    (NP_PST_MAX_TRACKED == NP_PBM_SESSION_MAX_ACTIVE_SOCKETS) ? 1 : -1];
typedef char _np_pst_three_wavelengths[(NP_PBM_WL_COUNT == 3U) ? 1 : -1];

/* Wavelength index → the channel np_mod_pbm_socket_disable_channel() latches. */
static const uint8_t k_wl_ch[NP_PBM_WL_COUNT] = {
    NP_PBM_CH_A_EN,   /* 660 nm  */
    NP_PBM_CH_B_EN,   /* 808 nm  */
    NP_PBM_CH_C_EN,   /* 1064 nm */
};

/* ── Session state ───────────────────────────────────────────────────────── */

typedef struct {
    bool                in_use;
    bool                driving;
    bool                throttled;
    bool                ntc_valid;
    uint16_t            socket_id;
    uint8_t             mod_type;
    float               ntc_c;
    float               ntc_peak_c;
    np_pbm_cal_t        cal[NP_PBM_WL_COUNT];
    np_pbm_dose_state_t dose;
} pst_entry_t;

static pst_entry_t     s_pool[NP_PST_MAX_TRACKED];
static np_pst_counts_t s_counts;
static uint32_t        s_last_tick_ms;
static bool            s_tick_started;

static pst_entry_t *find(uint16_t socket_id)
{
    for (uint32_t i = 0U; i < NP_PST_MAX_TRACKED; i++) {
        if (s_pool[i].in_use && s_pool[i].socket_id == socket_id) {
            return &s_pool[i];
        }
    }
    return NULL;
}

static void to_pbm_uid(const np_module_uid_t *in, np_pbm_module_uid_t *out)
{
    memcpy(out->b, in->b, sizeof out->b);
}

/* The module's own coefficients, by UID; defaults for an unknown or empty
 * socket (OI-HUB-C06 — never another module's, never socket-keyed). */
static np_cal_source_t load_cal(uint16_t socket_id, np_pbm_cal_t cal[NP_PBM_WL_COUNT])
{
    np_module_uid_t     uid;
    np_pbm_module_uid_t puid;
    if (np_module_map_socket_uid(socket_id, &uid) != NP_HUB_OK) {
        memset(&uid, 0, sizeof uid);
    }
    to_pbm_uid(&uid, &puid);
    return np_pbm_dose_load_cal(&puid, cal);
}

void np_pst_session_reset(void)
{
    memset(s_pool, 0, sizeof s_pool);
    memset(&s_counts, 0, sizeof s_counts);
    s_tick_started = false;
}

np_hub_status_t np_pst_track(uint16_t socket_id, np_hub_mod_type_t mod_type)
{
    if (socket_id >= NP_HEXMAP_MAX_SOCKETS) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    pst_entry_t *e = find(socket_id);
    if (e == NULL) {
        for (uint32_t i = 0U; i < NP_PST_MAX_TRACKED && e == NULL; i++) {
            if (!s_pool[i].in_use) {
                e = &s_pool[i];
            }
        }
        if (e == NULL) {
            return NP_HUB_ERR_GENERIC;   /* no socket is driven unmetered */
        }
        memset(e, 0, sizeof *e);
        e->in_use    = true;
        e->socket_id = socket_id;
        np_pbm_dose_reset(&e->dose);
        (void)load_cal(socket_id, e->cal);
    }
    e->mod_type = (uint8_t)mod_type;
    e->driving  = true;
    return NP_HUB_OK;
}

void np_pst_untrack_driving(uint16_t socket_id)
{
    pst_entry_t *e = find(socket_id);
    if (e != NULL) {
        e->driving = false;
    }
}

void np_pst_note_predrive_refusal(void)
{
    if (s_counts.predrive_refusals < UINT16_MAX) { s_counts.predrive_refusals++; }
}

void np_pst_note_drive_fault(void)
{
    if (s_counts.drive_faults < UINT16_MAX) { s_counts.drive_faults++; }
}

/* One dose tick for one driven socket. */
static void tick_one(pst_entry_t *e)
{
    float t = 0.0f;
    if (np_mod_pbm_hal_socket_ntc_read(e->socket_id, &t) == NP_HUB_OK) {
        e->ntc_valid = true;
        e->ntc_c     = t;
        if (t > e->ntc_peak_c) {
            e->ntc_peak_c = t;
        }
    } else {
        e->ntc_valid = false;
    }

    /* An unreadable NTC is treated as a hot one: the socket goes dark. */
    if (!e->ntc_valid || e->ntc_c >= (float)NP_PBM_THERMAL_FAULT_C) {
        if (!e->throttled) {
            e->throttled = true;
            if (s_counts.throttle_events < UINT16_MAX) { s_counts.throttle_events++; }
        }
        (void)np_mod_pbm_socket_disable_channel(e->socket_id, NP_PBM_CH_ALL_EN);
        e->driving = false;
        return;
    }

    bool before[NP_PBM_WL_COUNT];
    for (uint8_t w = 0U; w < NP_PBM_WL_COUNT; w++) {
        before[w] = e->dose.dose_limit_hit[w];
    }
    /* np_pbm_dose_tick() stops at the first newly reached limit; the next
     * tick carries on with the rest, so no limit is missed for longer than a
     * tick. */
    (void)np_pbm_dose_tick((uint8_t)e->socket_id, e->cal, &e->dose);
    if (e->mod_type == (uint8_t)NP_MOD_PBM_BASE) {
        /* A base tile has no 1064 nm emitter.  np_pbm_dose_tick() integrates
         * every wavelength from the same PD pair, so it would book a 1064 nm
         * dose the tile cannot deliver: that row is "not measured". */
        e->dose.dose_J_cm2[NP_WL_1064NM]        = 0.0f;
        e->dose.irradiance_mW_cm2[NP_WL_1064NM] = 0.0f;
        e->dose.pd_valid[NP_WL_1064NM]          = false;
        e->dose.dose_limit_hit[NP_WL_1064NM]    = false;
        before[NP_WL_1064NM]                    = false;
    }
    for (uint8_t w = 0U; w < NP_PBM_WL_COUNT; w++) {
        if (e->dose.dose_limit_hit[w] && !before[w]) {
            (void)np_mod_pbm_socket_disable_channel(e->socket_id, k_wl_ch[w]);
        }
    }
}

void np_pst_dose_poll(uint32_t now_ms)
{
    if (!s_tick_started) {
        s_tick_started = true;
        s_last_tick_ms = now_ms;
        return;
    }
    for (uint32_t n = 0U;
         n < NP_PST_TICK_CATCHUP_MAX && (now_ms - s_last_tick_ms) >= NP_PBM_DOSE_TICK_MS;
         n++) {
        s_last_tick_ms += NP_PBM_DOSE_TICK_MS;
        for (uint32_t i = 0U; i < NP_PST_MAX_TRACKED; i++) {
            if (s_pool[i].in_use && s_pool[i].driving) {
                tick_one(&s_pool[i]);
            }
        }
    }
    /* Further behind than the catch-up bound: the lost interval is not
     * integrated.  Dose is under-counted, never over-counted, and the gap
     * is bounded by the runner's own stall. */
    if ((now_ms - s_last_tick_ms) >= NP_PBM_DOSE_TICK_MS) {
        s_last_tick_ms = now_ms;
    }
}

static void fill_record(const pst_entry_t *e, np_pst_socket_record_t *r)
{
    memset(r, 0, sizeof *r);
    r->socket_id  = e->socket_id;
    r->mod_type   = e->mod_type;
    r->ntc_c      = e->ntc_c;
    r->ntc_peak_c = e->ntc_peak_c;
    uint8_t f = 0U;
    for (uint8_t w = 0U; w < NP_PBM_WL_COUNT; w++) {
        if (e->dose.pd_valid[w]) {
            f |= (uint8_t)(1U << (NP_PST_FLAG_PD_VALID_SHIFT + w));
        }
        if (e->dose.dose_limit_hit[w]) {
            f |= (uint8_t)(1U << (NP_PST_FLAG_LIMIT_SHIFT + w));
        }
        r->dose_J_cm2[w]        = e->dose.dose_J_cm2[w];
        r->irradiance_mW_cm2[w] = e->dose.pd_valid[w] ? e->dose.irradiance_mW_cm2[w] : 0.0f;
    }
    if (e->throttled) { f |= NP_PST_FLAG_THROTTLED; }
    if (e->ntc_valid) { f |= NP_PST_FLAG_NTC_VALID; }
    r->flags = f;
}

void np_pst_log_telemetry(uint32_t session_ms)
{
    for (uint32_t i = 0U; i < NP_PST_MAX_TRACKED; i++) {
        if (s_pool[i].in_use) {
            np_pst_socket_record_t r;
            fill_record(&s_pool[i], &r);
            np_log_pbm_socket(&r, session_ms);
        }
    }
}

void np_pst_get_counts(np_pst_counts_t *out)
{
    if (out != NULL) { *out = s_counts; }
}

bool np_pst_get_record(uint16_t socket_id, np_pst_socket_record_t *out)
{
    const pst_entry_t *e = find(socket_id);
    if (e == NULL || out == NULL) {
        return false;
    }
    fill_record(e, out);
    return true;
}

/* ── Idle health pass and maintenance tests (§6.9) ───────────────────────── */

/* The latest state of each tile, RAM only.  A power cycle forgets it: the
 * boot pass re-runs the non-emitting tests, and an LED verdict has to be
 * earned again by a maintenance run. */
typedef struct {
    bool            seen;
    np_module_uid_t uid;
    uint8_t         led_verdict;           /* np_maint_verdict_t */
    uint8_t         pd1_pct[NP_PBM_WL_COUNT];
} tile_state_t;

static bool         s_health_pending = true;   /* from boot */
static uint16_t     s_health_next;
static tile_state_t s_tile[NP_HEXMAP_MAX_SOCKETS];

void np_pst_health_request(void)
{
    s_health_pending = true;
}

bool np_pst_health_pending(void)
{
    return s_health_pending;
}

/* The tile kind from its element inventory: a 1064 nm emitter makes it a
 * smart tile (T1-C); 660/808 alone a base tile.  NP_MOD_NONE for a module
 * with no PBM emitter (an electrode tile). */
static np_hub_mod_type_t tile_kind(uint16_t socket_id)
{
    bool led = false;
    for (uint16_t el = 0U; el < NP_HEXMAP_MAX_ELEMENTS; el++) {
        np_hex_addr_t     a = { .socket_id = (uint8_t)socket_id, .element_id = (uint8_t)el };
        np_physical_loc_t loc;
        if (np_module_map_resolve(a, &loc) != NP_HUB_OK) {
            break;
        }
        if (loc.elem_type == NP_ELEM_LED_1064) {
            return NP_MOD_PBM_SMART;
        }
        if (loc.elem_type == NP_ELEM_LED_660 || loc.elem_type == NP_ELEM_LED_808) {
            led = true;
        }
    }
    return led ? NP_MOD_PBM_BASE : NP_MOD_NONE;
}

/* A PBM tile at this socket: its kind, and the UID bookkeeping for F14. */
static bool tile_open(uint16_t socket_id, np_pst_health_record_t *h)
{
    np_module_uid_t uid;
    if (np_module_map_socket_uid(socket_id, &uid) != NP_HUB_OK ||
        np_module_uid_is_zero(&uid)) {
        memset(&s_tile[socket_id], 0, sizeof s_tile[socket_id]);   /* empty */
        return false;
    }
    np_hub_mod_type_t kind = tile_kind(socket_id);
    if (kind == NP_MOD_NONE) {
        return false;                                               /* not PBM */
    }
    tile_state_t *t = &s_tile[socket_id];
    if (t->seen && !np_module_uid_equal(&uid, &t->uid)) {
        /* A different module: the old one's results are not this one's. */
        memset(t, 0, sizeof *t);
        t->seen = true;
        h->health_flags |= NP_PST_HEALTH_UID_CHANGED;
    }
    if (!t->seen) {
        memset(t, 0, sizeof *t);
        t->seen = true;
        if ((h->health_flags & NP_PST_HEALTH_UID_CHANGED) == 0U) {
            h->health_flags |= NP_PST_HEALTH_FIRST_PASS;
        }
    }
    t->uid = uid;
    h->socket_id = socket_id;
    h->mod_type  = (uint8_t)kind;
    for (uint8_t w = 0U; w < NP_PBM_WL_COUNT; w++) {
        h->pd1_pct[w] = (t->led_verdict == NP_MAINT_V_NOT_RUN ||
                         t->led_verdict == NP_MAINT_V_REFUSED)
                        ? NP_PST_PD1_NOT_MEASURED : t->pd1_pct[w];
    }
    h->verdict[NP_MAINT_T_LED] = t->led_verdict;
    return true;
}

/* The non-emitting tests.  None has an output that could carry the wearer's
 * biology: the probe and calibration source are device facts, and the NTC
 * test keeps a verdict and discards the reading. */
static void run_non_emitting(uint16_t socket_id, uint8_t test_mask,
                             np_pst_health_record_t *h)
{
    if (test_mask & (1U << NP_MAINT_T_PROBE)) {
        h->verdict[NP_MAINT_T_PROBE] =
            (h->mod_type != (uint8_t)NP_MOD_PBM_SMART) ? NP_MAINT_V_NA
            : np_pbm_hal_i2c_probe((uint8_t)socket_id, NP_PBM_I2C_ADDR,
                                   NP_PBM_I2C_PROBE_TIMEOUT_MS)
              ? NP_MAINT_V_PASS : NP_MAINT_V_FAIL;
    }
    if (test_mask & (1U << NP_MAINT_T_CAL)) {
        np_pbm_cal_t cal[NP_PBM_WL_COUNT];
        h->cal_source = (uint8_t)load_cal(socket_id, cal);
        h->verdict[NP_MAINT_T_CAL] = (h->cal_source == (uint8_t)NP_CAL_FACTORY)
                                     ? NP_MAINT_V_PASS : NP_MAINT_V_FAIL;
    }
    if (test_mask & (1U << NP_MAINT_T_NTC)) {
        float t = 0.0f;
        bool ok = (np_mod_pbm_hal_socket_ntc_read(socket_id, &t) == NP_HUB_OK) &&
                  (t == t) && (t < (float)NP_PBM_THERMAL_FAULT_C);
        h->verdict[NP_MAINT_T_NTC] = ok ? NP_MAINT_V_PASS : NP_MAINT_V_FAIL;
    }
}

bool np_pst_health_step(void)
{
    if (!s_health_pending) {
        return true;
    }
    np_pst_health_record_t h;
    memset(&h, 0, sizeof h);
    if (tile_open(s_health_next, &h)) {
        run_non_emitting(s_health_next,
                         (uint8_t)(NP_MAINT_TEST_ALL & (uint8_t)~NP_MAINT_TEST_EMITTING), &h);
        np_log_shdr_pbm_socket_health(&h);
    }
    s_health_next++;
    if (s_health_next >= NP_HEXMAP_MAX_SOCKETS) {
        s_health_next    = 0U;
        s_health_pending = false;
        return true;
    }
    return false;
}

static bool mask_has(const uint8_t *mask, uint16_t socket_id)
{
    return (mask[socket_id / 8U] & (uint8_t)(1U << (socket_id % 8U))) != 0U;
}

np_hub_status_t np_pst_maint_run(const np_maint_request_t *req,
                                 np_maint_result_fn        on_result,
                                 void                     *ctx)
{
    if (req == NULL || (req->test_mask & NP_MAINT_TEST_ALL) == 0U) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    bool any = false;
    for (uint16_t i = 0U; i < NP_HUB_SOCKET_MASK_BYTES; i++) {
        any = any || (req->socket_mask[i] != 0U);
    }
    if (!any) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    np_hub_status_t rc = NP_HUB_OK;
    for (uint16_t sock = 0U; sock < NP_HEXMAP_MAX_SOCKETS; sock++) {
        if (!mask_has(req->socket_mask, sock)) {
            continue;
        }
        if (!np_lease_probe_begin()) {
            rc = NP_HUB_ERR_SESSION_ACTIVE;   /* never test under a session */
            break;
        }
        np_pst_health_record_t h;
        memset(&h, 0, sizeof h);
        if (tile_open(sock, &h)) {
            run_non_emitting(sock, req->test_mask, &h);
            if (req->test_mask & NP_MAINT_TEST_EMITTING) {
                /* OI-FWHUB-20: an emitting test needs a signed maintenance
                 * session with the helmet in a hub-detected dock, never on a
                 * head.  Neither exists, so it is refused, and a verdict
                 * already earned is kept. */
                h.verdict[NP_MAINT_T_LED] = NP_MAINT_V_REFUSED;
                if (s_tile[sock].led_verdict == NP_MAINT_V_NOT_RUN) {
                    s_tile[sock].led_verdict = NP_MAINT_V_REFUSED;
                }
            }
            if (on_result != NULL) {
                on_result(&h, ctx);
            }
        }
        np_lease_probe_end();
    }
    /* SHDR gets the whole lattice's latest state from the next pass, never a
     * record of the subset this run tested. */
    np_pst_health_request();
    return rc;
}
