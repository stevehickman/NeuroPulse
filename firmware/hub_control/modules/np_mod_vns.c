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
#include "np_log_hrv.h"
#include "np_cmd_limits.h"
#include <stdint.h>
#include <string.h>

/* From firmware/hrv_biofeedback/ */
#include "np_hrv_session.h"
#include "np_hrv_types.h"

#include "np_sw02_platform_hal.h"

/* Safety: contact impedance threshold for VNS enable (ohm). */
#define VNS_CONTACT_MIN_OHM   500.0f   /* below this → poor contact */
#define VNS_CONTACT_MAX_OHM   5000.0f  /* above this → no contact */

/*
 * HRV session duration upper bound (s).  The hub session runner ends the VNS
 * session explicitly via a stop command; this only bounds the HRV library's
 * internal auto-complete timer (np_hrv_session_tick) so a missed stop cannot
 * run the coherence session forever.  Value is the top of the documented
 * 300–1200 s range (np_hrv_session_config_t.duration_s).
 */
#define VNS_HRV_DEFAULT_DURATION_S   1200U

/*
 * True for a finite float.  Tested on the bits, not with isfinite(): the hub is
 * built with -ffast-math (CMakeLists.txt), which makes the compiler assume no
 * NaN or infinity and may fold isfinite() and a NaN comparison to a constant.
 * An all-ones exponent field is NaN or infinity.
 */
static bool vns_float_is_finite(float v)
{
    uint32_t bits;
    memcpy(&bits, &v, sizeof(bits));
    return (bits & 0x7F800000UL) != 0x7F800000UL;
}

/* ── State ───────────────────────────────────────────────────────────────────── */

typedef struct {
    bool              active;
    bool              ppg_on;
    uint8_t           active_side;
    bool              stim_stop_pending; /* a stimulator stop failed or a start write may have landed: stop it again */
    float             last_impedance_ohm;
    np_hrv_session_t *hrv_sess;   /* NULL when no HRV protocol is running */
} np_mod_vns_state_t;

static np_mod_vns_state_t s_state;

/* ── HRV biofeedback session lifecycle ────────────────────────────────────────── */

/*
 * Session-end callback (np_hrv_session_create requires it non-NULL).  The HRV
 * library builds the finalized UHDR record and passes it here, and it is
 * committed to this session's UHDR file (OI-HRV-03): the logger appends it to
 * the partition np_uhdr_key_unlock() mounted AES-256-XTS under the user's
 * key.  It fires from np_hrv_session_stop(), which this driver reaches from
 * control(NULL), and the runner stops every module before
 * np_log_session_end(), so the record lands in the session it describes.
 * The context is released by vns_hrv_session_stop() immediately after
 * np_hrv_session_stop() returns.
 */
static void vns_hrv_session_end_cb(const np_hrv_session_record_t *record,
                                   np_hrv_status_t                reason)
{
    np_log_hrv_session(record, reason);
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
    s_state.hrv_sess = np_hrv_session_create(&cfg, NULL,
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

/*
 * Stop everything this driver starts: stimulator, safety enable, published
 * current, PPG, HRV session, EEG reference routing (OI-SIMFW-18).  Used by the
 * stop command and by a start command that is refused after it began.
 *
 * The stimulator stop is the one step that can fail.  Its failure does not stop
 * the rest: the safety enable is withdrawn, the published current cleared and the
 * PPG, session and routing stopped regardless, because the enable line is what
 * the safety MCU acts on.  But a stop that did not land is not forgotten:
 * `stim_stop_pending` keeps it, the status is returned to the caller
 * (NP_HUB_ERR_MOD_FAULT), and the next stop, or the next start, attempts it
 * again.  The retry policy is that one attempt per call; no timer, no loop.
 */
static np_hub_status_t vns_stop_all(void)
{
    np_hub_status_t rc = NP_HUB_OK;

    if (s_state.active || s_state.stim_stop_pending) {
        if (np_mod_vns_hal_stim_stop(s_state.active_side) == NP_HUB_OK) {
            s_state.stim_stop_pending = false;
        } else {
            s_state.stim_stop_pending = true;
            rc = NP_HUB_ERR_MOD_FAULT;
        }
    }
    if (s_state.active) {
        s_state.active = false;
        np_safety_spi_request_disable(NP_SAFETY_EN_VNS_HRV);
        /* OI-CHARGE-05 (c): clear the published commanded current beside
         * the enable it belongs to. */
        np_safety_spi_set_channel_current(NP_SAFETY_CH_VNS_HRV, 0U);
    }
    if (s_state.ppg_on) {
        np_mod_vns_hal_ppg_stop();
        s_state.ppg_on = false;
    }
    vns_hrv_session_stop();
    np_mod_vns_hal_eeg_ref_route(false);
    return rc;
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
        return vns_stop_all();
    }

    if (len < sizeof(np_mod_vns_hrv_params_t)) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    const np_mod_vns_hrv_params_t *p = (const np_mod_vns_hrv_params_t *)params;

    /* REQ-FWHUB-76: an amplitude above the ceiling is refused, not clamped (the 40 mA stop-gap, NP_VNS_MAX_CURRENT_UA;
     * #554, #559).  First: before the impedance read, the EEG reference routing, the PPG, the HRV session and the
     * stimulator, so the refusal starts nothing (REQ-VNSC-14) and a command already running is left as it was. */
    if (np_cmd_limits_check(NP_MOD_VNS_HRV, params, len) != NP_HUB_OK) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    /* Contact confirmation before enable */
    float imp = np_mod_vns_hal_impedance_check();
    /* OI-SIMFW-17 / REQ-VNSC-07: a non-finite reading (NaN, +/-inf) is not a
     * confirmed contact.  Both range comparisons are false for NaN, so a
     * "below min or above max" refusal lets it through; the test is written
     * as "finite and inside the range" so anything else refuses.  The telemetry
     * copy is 0 for a non-finite reading: casting NaN or inf to uint16_t is
     * undefined, and 0 ohm is what an absent clip reads. */
    const bool imp_finite = vns_float_is_finite(imp);
    s_state.last_impedance_ohm = imp_finite ? imp : 0.0f;
    if (!(imp_finite && imp >= VNS_CONTACT_MIN_OHM && imp <= VNS_CONTACT_MAX_OHM)) {
        return NP_HUB_ERR_SAFETY_REJECTED; /* safety MCU will also reject */
    }

    /* OI-SIMFW-18: a stimulator stop that was lost is retried before anything new
     * starts.  If it is lost again the command is refused and nothing is started. */
    if (s_state.stim_stop_pending) {
        if (np_mod_vns_hal_stim_stop(s_state.active_side) != NP_HUB_OK) {
            return NP_HUB_ERR_MOD_FAULT;
        }
        s_state.stim_stop_pending = false;
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
    /* As commanded: a value above the ceiling was refused at the top (REQ-FWHUB-76). */
    const uint16_t amp = p->amplitude_ua;

    np_hub_status_t rc = np_mod_vns_hal_stim_set(p->side,
                                                    p->freq_mhz,
                                                    amp,
                                                    p->pulse_width_us);
    if (rc != NP_HUB_OK) {
        /* OI-SIMFW-18 / REQ-VNSC-14: a refused command leaves nothing running.
         * The PPG, the EEG reference routing and the HRV session were started
         * above; a failed write may still have partly landed, so the stimulator
         * is stopped too, and a session this command interrupted stops with it.
         * If that stop is lost it stays pending (vns_stop_all). */
        s_state.active_side       = p->side;
        s_state.stim_stop_pending = true;
        (void)vns_stop_all();
        return NP_HUB_ERR_MOD_FAULT;
    }

    s_state.active      = true;
    s_state.active_side = p->side;
    np_safety_spi_request_enable(NP_SAFETY_EN_VNS_HRV);
    /* OI-CHARGE-05 (c): publish the commanded amplitude — the same number handed
     * to the stimulator and the number that was authored, so the safety MCU's
     * per-phase check runs against what is driven. */
    np_safety_spi_set_channel_current(NP_SAFETY_CH_VNS_HRV, amp);

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
