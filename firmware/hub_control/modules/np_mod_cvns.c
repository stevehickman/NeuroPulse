/*
 * NeurOne Hub Control — Cervical VNS (T2) Module Driver
 * Document: NP-FW-HUB-001 Rev A §8.8, NP-FW-CVNS-001 Rev A
 *
 * Hub ↔ library integration layer for the cervical VNS (tcVNS) T2 accessory.
 * This driver is the ONLY glue between the hub's module framework
 * (np_module_registry / np_session_runner / np_safety_spi) and the standalone
 * cervical VNS safety library (firmware/cervical_vns/: np_cvns_session,
 * np_cvns_stim, np_cvns_interlock).  It owns the three library contexts,
 * translates a signed np_mod_cvns_params_t command into a validated
 * np_cvns_session_config_t, and bridges the two safety channels:
 *
 *   • The unified hub↔safety-MCU heartbeat (np_safety_spi, NP_SAFETY_EN_CVNS)
 *     physically gates the cervical VNS enable GPIO.  The hub requests that
 *     enable bit only once the library has cleared its own pre-stimulation gate
 *     (impedance → cardiac baseline → safety-MCU grant) and is actually
 *     delivering stimulation.  It is dropped the instant the library faults,
 *     the session ends, or the runner stops the command.
 *
 *   • The Class C safety MCU's cardiac interlock (NP-FW-CVNS-001 §6) owns the
 *     final cutoff.  A cutoff surfaces to the hub two independent ways, and this
 *     driver handles BOTH:
 *       1. Main-processor side — the library's PPG/Pan-Tompkins pipeline
 *          (fed via np_mod_cvns_push_ppg) fires the fault callback below when it
 *          detects an HR excursion, data loss, or safety-MCU loss.
 *       2. Unified heartbeat — NP_SAFETY_STATUS_CARDIAC in the heartbeat reply,
 *          consumed by the hub-global re-enable manager (np_cvns_reenable, driven
 *          by the heartbeat task).  That path is NOT this driver's concern; it
 *          only re-arms the enable bit after all three re-enable gates pass.
 *
 * SAFETY-CRITICAL CARDIAC-INTERLOCK CONFIG (see cvns_interlock_config() below):
 *   - PPG sample rate is pinned to NP_CVNS_PPG_SAMPLE_RATE_HZ (compile-time
 *     asserted) — the Pan-Tompkins timing constants depend on it.
 *   - Cardiac baseline is MANDATORY and cannot be waived by the descriptor.
 *     A malicious/buggy descriptor asking to skip it (baseline_req == 0) is
 *     ignored: the interlock always requires a valid baseline before any enable,
 *     and this driver never requests the unified enable bit before the library
 *     reports the session in an active stimulation stage.
 *   - The HR-change cutoff limit (NP_CVNS_HR_CHANGE_LIMIT_BPM), observation
 *     window, and main-vs-MCU baseline cross-validation tolerance are resident
 *     constants in np_cvns_config.h and enforced by the library + safety MCU.
 *
 * Privacy: this driver stores and emits NO user biology beyond what every
 * electrical-stim module already exposes (commanded current, electrode
 * impedance — UHDR, same class as np_mod_stim).  Baseline HR and R-R data live
 * only inside the library contexts and the UHDR session record it writes.  The
 * SHDR fault log written here carries flags only, with a SUPPRESSED (0)
 * timestamp so no session-relative time co-locates with a cardiac event
 * (CLAUDE.md fault-latch privacy gate; NP-FW-CVNS-001 SHDR routing).
 *
 * IEC 62304 Class B (SW-02 hub control); the cutoff itself is Class C (safety
 * MCU) with this driver as a non-authoritative main-processor participant.
 *
 * HAL stubs (provided by the hub platform layer; mocked in host tests):
 *   OI-CVNS-HUB-04: np_cvns_hal_accessory_present() → bool (accessory-port detect)
 *   OI-CVNS-HUB-05: np_mod_cvns_hal_now_ms()        → uint32_t (free-running ms)
 *   OI-CVNS-HUB-06: np_mod_cvns_hal_now_unix()      → uint32_t (UTC epoch seconds)
 *
 * Scheduler wiring (OI-CVNS-HUB-07, hub main task):
 *   - np_mod_cvns_tick(now_ms, now_s) every NP_CVNS_STIM_TICK_MS (100 ms) while a
 *     CVNS command is active (session-runner control task).
 *   - np_mod_cvns_push_ppg(sample, ts_ms) from the PPG ISR at
 *     NP_CVNS_PPG_SAMPLE_RATE_HZ.
 */

#include "np_hub_types.h"
#include "np_module_registry.h"
#include "np_safety_spi.h"
#include "np_session_log.h"
#include <string.h>

/* Cervical VNS safety library (firmware/cervical_vns/). */
#include "np_cvns_session.h"
#include "np_cvns_interlock.h"
#include "np_cvns_stim.h"

/* The Pan-Tompkins detector timing constants (refractory, integration window,
 * running-max history) are all derived for a fixed sample rate.  If the PPG ADC
 * rate ever changes, the interlock config below must be revisited — trap it. */
_Static_assert(NP_CVNS_PPG_SAMPLE_RATE_HZ == 200U,
               "cardiac interlock config assumes 200 Hz PPG; revalidate Pan-Tompkins constants");

/* ── HAL stubs ───────────────────────────────────────────────────────────────── */

extern bool     np_cvns_hal_accessory_present(void);   /* OI-CVNS-HUB-04 */
extern uint32_t np_mod_cvns_hal_now_ms(void);           /* OI-CVNS-HUB-05 */
extern uint32_t np_mod_cvns_hal_now_unix(void);         /* OI-CVNS-HUB-06 */

/* ── Library contexts (single active CVNS session per device) ────────────────── */

static np_cvns_interlock_ctx_t s_interlock;
static np_cvns_stim_ctx_t      s_stim;
static np_cvns_session_ctx_t   s_session;

/* ── Driver state ────────────────────────────────────────────────────────────── */

typedef struct {
    bool     ready;             /* init() completed; contexts valid            */
    bool     active;            /* a session command is running                */
    bool     enable_requested;  /* NP_SAFETY_EN_CVNS currently requested        */
    /* Cached, non-biology display fields for telemetry() (UHDR-class current /
     * impedance, same as np_mod_stim; NO HR is ever cached here). */
    uint16_t last_current_ua;
    float    last_impedance_kohm;
    np_cvns_fault_reason_t last_fault;
} np_mod_cvns_state_t;

static np_mod_cvns_state_t s_state;

/* ── Safety-critical cardiac-interlock config ────────────────────────────────── */

/*
 * The single authoritative construction of the main-processor cardiac interlock
 * configuration.  Kept in one place so the safety-relevant parameters are easy
 * to review.  The HR-change limit, observation window, and cross-validation
 * tolerance are NOT set here — they are resident constants in np_cvns_config.h,
 * compiled into the library and the safety MCU, and cannot be overridden by a
 * session descriptor.  This config only supplies the PPG sample rate (which the
 * detector timing depends on) and the interlock's initial wall-clock reference.
 */
static np_cvns_interlock_config_t cvns_interlock_config(void)
{
    np_cvns_interlock_config_t cfg;
    cfg.ppg_sample_rate_hz = NP_CVNS_PPG_SAMPLE_RATE_HZ;
    cfg.now_s              = np_mod_cvns_hal_now_unix();
    return cfg;
}

/* ── Descriptor → validated session config ───────────────────────────────────── */

/*
 * Convert a signed np_mod_cvns_params_t command into a fully clamped
 * np_cvns_session_config_t.  Every field is forced into the library's accepted
 * range so np_cvns_stim_init() (which re-validates) can never be handed an
 * out-of-range value from a malformed descriptor.  Pure and side-effect free —
 * exercised directly by host tests.
 *
 * Mappings:
 *   freq_mhz (mHz) → freq_hz, rounded, clamped 1..25 Hz
 *   amplitude_ua   → current_ua, clamped ≤ 2000 µA (2 mA absolute)
 *   pulse_width_us → clamped 200..1000 µs (0 → 250 µs default, in range)
 *   side {0=right,1=left,2=bilateral} → electrode_config
 *        {0=bilateral,1=unilateral_L,2=unilateral_R}
 *   duration_s     → NP_CVNS_SESSION_MAX_S (120 s) upper safety bound; the
 *                    session-runner stop command normally ends it earlier, but
 *                    the library auto-ramps-down at this bound if stop is missed.
 *
 * NB: the descriptor's baseline_req is deliberately NOT consulted — cardiac
 * baseline is mandatory (see file header).  ramp_s has no config field; the
 * library ramps at its fixed NP_CVNS_RAMP_UP_S.
 */
void np_mod_cvns_build_config(const np_mod_cvns_params_t *p,
                              np_cvns_session_config_t   *out)
{
    /* Frequency: mHz → Hz with round-to-nearest, then clamp to 1..25 Hz. */
    uint32_t freq_hz = ((uint32_t)p->freq_mhz + 500U) / 1000U;
    if (freq_hz < NP_CVNS_FREQ_HZ_MIN) { freq_hz = NP_CVNS_FREQ_HZ_MIN; }
    if (freq_hz > NP_CVNS_FREQ_HZ_MAX) { freq_hz = NP_CVNS_FREQ_HZ_MAX; }
    out->freq_hz = (uint16_t)freq_hz;

    /* Current: hard cap at the 2 mA absolute limit. */
    out->current_ua = (p->amplitude_ua > NP_CVNS_CURRENT_UA_MAX)
                        ? NP_CVNS_CURRENT_UA_MAX : p->amplitude_ua;

    /* Pulse width: 0 → default 250 µs; otherwise clamp into 200..1000 µs. */
    uint16_t pw = (p->pulse_width_us == 0U) ? 250U : p->pulse_width_us;
    if (pw < NP_CVNS_PULSE_WIDTH_US_MIN) { pw = NP_CVNS_PULSE_WIDTH_US_MIN; }
    if (pw > NP_CVNS_PULSE_WIDTH_US_MAX) { pw = NP_CVNS_PULSE_WIDTH_US_MAX; }
    out->pulse_width_us = pw;

    /* Electrode side mapping. */
    switch (p->side) {
    case 2U:  out->electrode_config = 0U; break; /* bilateral    */
    case 1U:  out->electrode_config = 1U; break; /* unilateral L */
    case 0U:  out->electrode_config = 2U; break; /* unilateral R */
    default:  out->electrode_config = 0U; break; /* fail safe → bilateral */
    }

    /* Runner controls session end; bound the library's auto-complete timer. */
    out->duration_s = NP_CVNS_SESSION_MAX_S;
}

/* ── Unified enable-bit management ───────────────────────────────────────────── */

static void cvns_request_enable(void)
{
    if (!s_state.enable_requested) {
        np_safety_spi_request_enable(NP_SAFETY_EN_CVNS);
        s_state.enable_requested = true;
    }
}

static void cvns_drop_enable(void)
{
    if (s_state.enable_requested) {
        np_safety_spi_request_disable(NP_SAFETY_EN_CVNS);
        s_state.enable_requested = false;
    }
}

/* True only for stages in which the library is (or is about to be) driving
 * current into the patient — the only stages in which the unified enable bit
 * should be asserted. */
static bool stage_is_delivering(np_cvns_stage_t stage)
{
    return (stage == NP_CVNS_STAGE_RAMP_UP) ||
           (stage == NP_CVNS_STAGE_ACTIVE)  ||
           (stage == NP_CVNS_STAGE_RAMP_DOWN);
}

/* ── Library callbacks ───────────────────────────────────────────────────────── */

/*
 * Interlock fault callback — invoked by np_cvns_interlock.c when the
 * main-processor side detects a cardiac excursion, R-peak data loss, or loss of
 * the safety MCU.  The safety MCU has already (independently) cut the enable
 * GPIO; here we mirror that into the unified enable bitmask and record the event
 * to SHDR (flags only, suppressed timestamp).
 */
static void cvns_fault_cb(np_cvns_interlock_ctx_t *interlock,
                          np_cvns_fault_reason_t   reason)
{
    (void)interlock;
    s_state.last_fault = reason;

    /* Fail-safe: drop the unified enable request immediately. */
    cvns_drop_enable();

    /* SHDR safety-interlock log: device-condition flags only.  Timestamp is
     * suppressed (0) so a cardiac-event time never co-locates with a session
     * clock in SHDR (privacy gate). */
    np_log_shdr_fault(NP_HUB_SLOT_CVNS, NP_MOD_CVNS, (uint8_t)reason, 0U);
}

/*
 * Session-end callback — invoked by np_cvns_session.c on normal completion or
 * fault.  The library has already written its own UHDR/SHDR records via its
 * platform hooks; here we only tear down the hub-side wiring.
 */
static void cvns_session_end_cb(const np_cvns_session_record_t *record,
                                np_cvns_status_t                result)
{
    (void)record;
    (void)result;
    cvns_drop_enable();
    s_state.active = false;
}

/*
 * Display callback — cache the non-biology live fields for telemetry().  HR is
 * deliberately NOT cached (UHDR user biology stays inside the library).
 */
static void cvns_display_cb(const np_cvns_display_state_t *state)
{
    s_state.last_current_ua = state->current_ua;
    /* Report the worse (higher) of the two electrode impedances as the driver's
     * single impedance figure. */
    float imp = state->impedance_left_kohm;
    if (state->impedance_right_kohm > imp) { imp = state->impedance_right_kohm; }
    s_state.last_impedance_kohm = imp;
}

/* ── Detect ──────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_cvns_detect(uint8_t slot, np_hub_mod_type_t *type_out)
{
    (void)slot;
    if (np_cvns_hal_accessory_present()) {
        *type_out = NP_MOD_CVNS;
        return NP_HUB_OK;
    }
    return NP_HUB_ERR_NOT_PRESENT;
}

/* ── Init ────────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_cvns_init(uint8_t slot)
{
    (void)slot;
    memset(&s_state, 0, sizeof(s_state));
    s_state.last_fault = NP_CVNS_FAULT_NONE;

    /* Cardiac interlock: safety-critical config + fault callback wiring. */
    if (np_cvns_interlock_init(&s_interlock, cvns_interlock_config(),
                               cvns_fault_cb) != NP_CVNS_OK) {
        return NP_HUB_ERR_MOD_INIT;
    }

    /* Session orchestrator: composes interlock + stim; owns UHDR/SHDR records. */
    if (np_cvns_session_init(&s_session, &s_interlock, &s_stim,
                             cvns_session_end_cb, cvns_display_cb) != NP_CVNS_OK) {
        return NP_HUB_ERR_MOD_INIT;
    }

    /* SHDR accessory-authentication log (pass — accessory detected + init OK). */
    np_log_shdr_zone_auth(NP_HUB_SLOT_CVNS, NP_MOD_CVNS, true);

    s_state.ready = true;
    return NP_HUB_OK;
}

/* ── Control ─────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_cvns_control(uint8_t slot, const void *params, uint16_t len)
{
    (void)slot;

    if (!s_state.ready) {
        return NP_HUB_ERR_MOD_INIT;
    }

    /* Stop — graceful ramp-down and unified disable. */
    if (params == NULL || len == 0U) {
        if (s_state.active) {
            np_cvns_session_stop(&s_session, np_mod_cvns_hal_now_ms());
            s_state.active = false;
        }
        cvns_drop_enable();
        return NP_HUB_OK;
    }

    if (len < sizeof(np_mod_cvns_params_t)) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    const np_mod_cvns_params_t *p = (const np_mod_cvns_params_t *)params;

    np_cvns_session_config_t cfg;
    np_mod_cvns_build_config(p, &cfg);

    /* Begin the library workflow: impedance → cardiac baseline → (grant) → ramp.
     * The unified enable bit is NOT requested here — only once the library
     * reaches a stimulation-delivery stage in np_mod_cvns_tick(). */
    np_cvns_status_t rc = np_cvns_session_start(&s_session, &cfg,
                                                np_mod_cvns_hal_now_ms(),
                                                np_mod_cvns_hal_now_unix());
    if (rc != NP_CVNS_OK) {
        return (rc == NP_CVNS_ERR_SESSION_ACTIVE) ? NP_HUB_ERR_SESSION_ACTIVE
                                                   : NP_HUB_ERR_MOD_FAULT;
    }

    s_state.active           = true;
    s_state.last_fault       = NP_CVNS_FAULT_NONE;
    return NP_HUB_OK;
}

/* ── Scheduler tick (OI-CVNS-HUB-07) ─────────────────────────────────────────── */

/*
 * Drive the cervical VNS session state machine.  Call every NP_CVNS_STIM_TICK_MS
 * (100 ms) while a CVNS command is active.
 *
 * now_ms: free-running millisecond counter (same clock as push_ppg timestamps).
 * now_s:  wall-clock seconds (for interlock lockout / re-enable timers).
 */
void np_mod_cvns_tick(uint32_t now_ms, uint32_t now_s)
{
    if (!s_state.ready || !s_state.active) {
        return;
    }

    np_cvns_session_tick(&s_session, now_ms, now_s);

    /* Track the unified enable bit to the library's delivery stage: assert once
     * stimulation is (about to be) delivered, drop the moment it is not. */
    np_cvns_stage_t stage = np_cvns_session_stage(&s_session);
    if (stage_is_delivering(stage)) {
        cvns_request_enable();
    } else {
        cvns_drop_enable();
    }

    /* Terminal stages release the driver so a new command can start. */
    if (stage == NP_CVNS_STAGE_COMPLETE ||
        stage == NP_CVNS_STAGE_FAULT   ||
        stage == NP_CVNS_STAGE_IDLE) {
        s_state.active = false;
    }
}

/* ── PPG feed (OI-CVNS-HUB-07; PPG ISR) ──────────────────────────────────────── */

/*
 * Push one raw PPG ADC sample into the cardiac interlock's Pan-Tompkins
 * pipeline.  Call from the PPG ISR at NP_CVNS_PPG_SAMPLE_RATE_HZ.  On R-peak
 * detection the library asserts the RPEAK_IN GPIO to the safety MCU and updates
 * the R-R buffer used for baseline and excursion detection.
 */
void np_mod_cvns_push_ppg(uint32_t sample, uint32_t timestamp_ms)
{
    if (!s_state.ready || !s_state.active) {
        return;
    }
    np_cvns_interlock_push_ppg(&s_interlock, sample, timestamp_ms);
}

/* ── App-confirmed re-enable after cardiac cutoff ────────────────────────────── */

/*
 * Library-side re-enable request following an explicit in-app user confirmation.
 * Complements the hub-global re-enable manager (np_cvns_reenable): that manager
 * decides when the UNIFIED enable bit may re-assert (3-gate: lockout + confirm +
 * fresh impedance); this call re-arms the LIBRARY's interlock (baseline + its
 * own lockout).  Both must agree before stimulation resumes.
 */
np_hub_status_t np_mod_cvns_request_reenable(uint32_t now_s)
{
    if (!s_state.ready) {
        return NP_HUB_ERR_MOD_INIT;
    }
    np_cvns_status_t rc = np_cvns_session_request_reenable(&s_session, now_s);
    if (rc == NP_CVNS_OK)        { return NP_HUB_OK; }
    if (rc == NP_CVNS_ERR_LOCKOUT) { return NP_HUB_ERR_SAFETY_REJECTED; }
    return NP_HUB_ERR_MOD_FAULT;
}

/* ── Telemetry ───────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_cvns_telemetry(uint8_t slot, np_telem_record_t *out)
{
    (void)slot;
    if (out == NULL) { return NP_HUB_ERR_INVALID_ARG; }

    out->mod_type = NP_MOD_CVNS;
    out->slot     = NP_HUB_SLOT_CVNS;

    /* Report through the shared electrical-stim telemetry record (same class as
     * BES/tACS/tDCS): commanded current + electrode impedance are UHDR; the
     * ramp flag is SHDR-safe.  Charge accounting is owned by the safety MCU. */
    np_telem_stim_t *s = &out->data.stim;
    s->current_ua     = (float)s_state.last_current_ua;
    s->impedance_kohm = s_state.last_impedance_kohm;
    s->charge_uC      = 0U;
    s->ramp_active    = stage_is_delivering(np_cvns_session_stage(&s_session));

    return NP_HUB_OK;
}

/* ── Shutdown ────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_cvns_shutdown(uint8_t slot)
{
    return np_mod_cvns_control(slot, NULL, 0U);
}

/* ── Test seams (host tests only) ────────────────────────────────────────────── */

#ifdef NPTEST_HOST
np_cvns_interlock_ctx_t *np_mod_cvns_test_interlock(void) { return &s_interlock; }
np_cvns_stim_ctx_t      *np_mod_cvns_test_stim(void)      { return &s_stim; }
np_cvns_session_ctx_t   *np_mod_cvns_test_session(void)   { return &s_session; }
bool                     np_mod_cvns_test_enable_requested(void) { return s_state.enable_requested; }
bool                     np_mod_cvns_test_active(void)    { return s_state.active; }
/* Directly exercise the interlock fault path (cutoff → unified disable + SHDR). */
void np_mod_cvns_test_fire_fault(np_cvns_fault_reason_t reason)
{
    cvns_fault_cb(&s_interlock, reason);
}
#endif /* NPTEST_HOST */
