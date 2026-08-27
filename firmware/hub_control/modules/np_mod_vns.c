/*
 * NeurOne Hub Control — VNS + HRV Auricular Clip Driver
 * Document: NP-FW-HUB-001 Rev 1 §8.4
 *
 * Manages the auricular VNS + PPG HRV clip on NP_HUB_SLOT_VNS_HRV.
 * Detected by accessory port impedance check.  Delegates HRV biofeedback
 * protocol to np_hrv_biofeedback (firmware/hrv_biofeedback/) which runs as a
 * separate FreeRTOS thread; this driver provides the start/stop interface.
 *
 * Safety: VNS stimulation enable is safety-MCU gated (NP_SAFETY_EN_VNS_HRV).
 * Contact confirmation (impedance check) required before safety MCU will grant
 * enable — checked in np_mod_vns_control before requesting enable.
 *
 * HAL stubs:
 *   OI-VNS-01: np_mod_vns_hal_impedance_check() → float ohm (contact check)
 *   OI-VNS-02: np_mod_vns_hal_stim_set(side, freq_mhz, amp_ua, pw_us)
 *   OI-VNS-03: np_mod_vns_hal_stim_stop(side)
 *   OI-VNS-04: np_mod_vns_hal_ppg_start()  — starts PPG 808-830nm LED + PD
 *   OI-VNS-05: np_mod_vns_hal_ppg_stop()
 *   OI-VNS-06: np_mod_vns_hal_eeg_ref_route(enable) — A1/A2 to EEG reference
 *   OI-VNS-07: np_mod_vns_hal_get_rmssd()   → float ms (from hrv_biofeedback)
 *   OI-VNS-08: np_mod_vns_hal_get_coherence() → float 0–10
 *   OI-VNS-09: np_mod_vns_hal_get_hr_bpm()  → float
 *   OI-VNS-10: np_mod_vns_hal_now_ms()      → uint32_t (free-running ms tick;
 *              same clock the HRV session uses for start/stop/tick timing)
 */

#include "np_hub_types.h"
#include "np_module_registry.h"
#include "np_safety_spi.h"
#include <string.h>

/* From firmware/hrv_biofeedback/ */
#include "np_hrv_session.h"
#include "np_hrv_types.h"

extern float np_mod_vns_hal_impedance_check(void);
extern np_hub_status_t np_mod_vns_hal_stim_set(uint8_t side, uint16_t freq_mhz,
                                                  uint16_t amp_ua, uint8_t pw_us);
extern np_hub_status_t np_mod_vns_hal_stim_stop(uint8_t side);
extern void np_mod_vns_hal_ppg_start(void);
extern void np_mod_vns_hal_ppg_stop(void);
extern void np_mod_vns_hal_eeg_ref_route(bool enable);
extern float np_mod_vns_hal_get_rmssd(void);
extern float np_mod_vns_hal_get_coherence(void);
extern float np_mod_vns_hal_get_hr_bpm(void);
extern uint32_t np_mod_vns_hal_now_ms(void);            /* OI-VNS-10 */

/* Safety: contact impedance threshold for VNS enable (ohm). */
#define VNS_CONTACT_MIN_OHM   500.0f   /* below this → poor contact */
#define VNS_CONTACT_MAX_OHM   5000.0f  /* above this → no contact */
#define VNS_MAX_AMP_UA        2000U

/*
 * HRV session duration upper bound (s).  The hub session runner ends the VNS
 * session explicitly via a stop command; this only bounds the HRV library's
 * internal auto-complete timer (np_hrv_session_tick) so a missed stop cannot
 * run the coherence session forever.  Value is the top of the documented
 * 300–1200 s range (np_hrv_session_config_t.duration_s).
 */
#define VNS_HRV_DEFAULT_DURATION_S   1200U

/* ── State ───────────────────────────────────────────────────────────────────── */

typedef struct {
    bool              active;
    bool              ppg_on;
    uint8_t           active_side;
    float             last_impedance_ohm;
    np_hrv_session_t *hrv_sess;   /* NULL when no HRV protocol is running */
} np_mod_vns_state_t;

static np_mod_vns_state_t s_state;

/* ── HRV biofeedback session lifecycle ────────────────────────────────────────── */

/*
 * Session-end callback (np_hrv_session_init requires it non-NULL).  The HRV
 * library builds the finalized UHDR record and passes it here; the storage
 * commit path lives outside this driver, so there is nothing to do but
 * acknowledge.  The context is released by vns_hrv_session_stop() immediately
 * after np_hrv_session_stop() returns.
 */
static void vns_hrv_session_end_cb(const np_hrv_session_record_t *record,
                                   np_hrv_status_t                reason)
{
    (void)record;
    (void)reason;
}

/* Start the HRV biofeedback session for protocol `proto` (caller ensures > 0). */
static void vns_hrv_session_start(uint8_t proto)
{
    if (s_state.hrv_sess != NULL) {
        return;  /* already running — one HRV session per device */
    }

    uint32_t now = np_mod_vns_hal_now_ms();

    np_hrv_session_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.protocol   = proto;                       /* np_hrv_protocol_t value    */
    cfg.duration_s = VNS_HRV_DEFAULT_DURATION_S;
    /* pacer_rate_bpm left 0.0 → HRV lib uses personalized/default resonance.   */

    /* create() copies cfg by value; the HRV module owns its own FreeRTOS task. */
    s_state.hrv_sess = np_hrv_session_init(&cfg, NULL,
                                             vns_hrv_session_end_cb, now);
    if (s_state.hrv_sess != NULL) {
        (void)np_hrv_session_start(s_state.hrv_sess, now);
    }
}

/* Stop and release the HRV biofeedback session if one is running. */
static void vns_hrv_session_stop(void)
{
    if (s_state.hrv_sess != NULL) {
        np_hrv_session_stop(s_state.hrv_sess, np_mod_vns_hal_now_ms());
        np_hrv_session_destroy(s_state.hrv_sess);
        s_state.hrv_sess = NULL;
    }
}

/* ── Detect ──────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_vns_detect(uint8_t slot, np_hub_mod_type_t *type_out)
{
    (void)slot;
    float imp = np_mod_vns_hal_impedance_check();
    /* Clip is present if impedance is within the contact range. */
    if (imp >= VNS_CONTACT_MIN_OHM && imp <= VNS_CONTACT_MAX_OHM) {
        *type_out = NP_MOD_VNS_HRV;
        return NP_HUB_OK;
    }
    return NP_HUB_ERR_NOT_PRESENT;
}

/* ── Init ────────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_vns_init(uint8_t slot)
{
    (void)slot;
    memset(&s_state, 0, sizeof(s_state));
    s_state.last_impedance_ohm = np_mod_vns_hal_impedance_check();
    return NP_HUB_OK;
}

/* ── Control ─────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_vns_control(uint8_t slot, const void *params, uint16_t len)
{
    (void)slot;

    /* Stop */
    if (params == NULL || len == 0U) {
        if (s_state.active) {
            (void)np_mod_vns_hal_stim_stop(s_state.active_side);
            s_state.active = false;
            np_safety_spi_request_disable(NP_SAFETY_EN_VNS_HRV);
        }
        if (s_state.ppg_on) {
            np_mod_vns_hal_ppg_stop();
            s_state.ppg_on = false;
        }
        vns_hrv_session_stop();
        np_mod_vns_hal_eeg_ref_route(false);
        return NP_HUB_OK;
    }

    if (len < sizeof(np_mod_vns_hrv_params_t)) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    const np_mod_vns_hrv_params_t *p = (const np_mod_vns_hrv_params_t *)params;

    /* Contact confirmation before enable */
    float imp = np_mod_vns_hal_impedance_check();
    s_state.last_impedance_ohm = imp;
    if (imp < VNS_CONTACT_MIN_OHM || imp > VNS_CONTACT_MAX_OHM) {
        return NP_HUB_ERR_SAFETY_REJECTED; /* safety MCU will also reject */
    }

    /* Route A1/A2 EEG reference if requested */
    np_mod_vns_hal_eeg_ref_route(p->eeg_ref_enable != 0U);

    /* PPG */
    if (p->ppg_enable) {
        np_mod_vns_hal_ppg_start();
        s_state.ppg_on = true;
    }

    /* Start HRV biofeedback library if needed */
    if (p->hrv_proto > 0U) {
        /* The HRV session module handles its own FreeRTOS task — we create,
         * start, and later tear it down via the local lifecycle helpers. */
        vns_hrv_session_start(p->hrv_proto);
    }

    /* VNS stimulation */
    uint16_t amp = (p->amplitude_ua > VNS_MAX_AMP_UA) ? VNS_MAX_AMP_UA : p->amplitude_ua;

    np_hub_status_t rc = np_mod_vns_hal_stim_set(p->side,
                                                    p->freq_mhz,
                                                    amp,
                                                    p->pulse_width_us);
    if (rc != NP_HUB_OK) { return NP_HUB_ERR_MOD_FAULT; }

    s_state.active      = true;
    s_state.active_side = p->side;
    np_safety_spi_request_enable(NP_SAFETY_EN_VNS_HRV);

    return NP_HUB_OK;
}

/* ── Telemetry ───────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_vns_telemetry(uint8_t slot, np_telem_record_t *out)
{
    (void)slot;
    if (out == NULL) { return NP_HUB_ERR_INVALID_ARG; }

    out->mod_type = NP_MOD_VNS_HRV;
    out->slot     = NP_HUB_SLOT_VNS_HRV;

    np_telem_vns_hrv_t *h = &out->data.vns_hrv;
    h->rmssd_ms           = np_mod_vns_hal_get_rmssd();       /* UHDR */
    h->coherence_score    = np_mod_vns_hal_get_coherence();   /* UHDR */
    h->hr_bpm             = np_mod_vns_hal_get_hr_bpm();      /* UHDR */
    h->impedance_ohm      = (uint16_t)s_state.last_impedance_ohm; /* UHDR */
    h->contact_confirmed  = s_state.active;                   /* SHDR-safe bool */

    return NP_HUB_OK;
}

/* ── Shutdown ────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_vns_shutdown(uint8_t slot)
{
    /* control(NULL, 0) stops VNS stim, PPG, EEG routing, and the HRV session. */
    return np_mod_vns_control(slot, NULL, 0U);
}
