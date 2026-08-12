/*
 * NeurOne Hub Control — Cervical VNS (T2) Module Driver
 * Document: NP-FW-HUB-001 Rev 1 §8.8, NP-FW-CVNS-001 Rev 1
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
 *   OI-CVNS-HUB-09: np_cvns_hal_impedance_measure_start()/_poll() — async hub-side
 *                   per-electrode kΩ measurement (1 kHz AC probe, same path the
 *                   re-enable manager's gate-3 check uses) feeding the advisory
 *                   impedance step with GENUINE values instead of a placeholder.
 *
 * Scheduler wiring (OI-CVNS-HUB-07) — the callers of the three entry points below
 * live in the hub scheduler (np_session_runner.c, np_hub_control_main.c):
 *   - np_mod_cvns_tick(now_ms, now_s) is driven every NP_CVNS_STIM_TICK_MS (100 ms)
 *     by the session-runner control task (np_runner_run) for as long as
 *     np_mod_cvns_active() reports a command is running.
 *   - np_mod_cvns_push_ppg(sample, ts_ms) is fed from the platform PPG ADC ISR at
 *     NP_CVNS_PPG_SAMPLE_RATE_HZ via the hub seam np_hub_ppg_isr_sample()
 *     (np_hub_control_main.c), which stamps each sample with the same
 *     free-running ms clock the runner passes to the tick.
 *   - np_mod_cvns_set_heartbeat_status(granted_mask, mcu_status) once per beat
 *     from the safety-heartbeat task (OI-CVNS-HUB-08 — see cvns_apply_heartbeat).
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

/*
 * OI-CVNS-HUB-09: hub-side per-electrode CVNS impedance measurement.
 * Asynchronous (start once, poll each tick), mirroring the re-enable manager's
 * gate-3 HAL (np_cvns_hal_impedance_start/_poll) — but returns the actual
 * per-electrode kΩ rather than a pass/fail bit, so the advisory impedance step
 * can record genuine values into the library's UHDR/SHDR impedance fields.
 *
 * np_cvns_hal_impedance_measure_start — begin a fresh measurement.  Returns
 *   NP_HUB_OK if the measurement was started, else the flow falls back to the
 *   nominal placeholder (liveness preserved even without a measurement path).
 * np_cvns_hal_impedance_measure_poll — returns true when complete, filling
 *   kohm_out[NP_CVNS_ELECTRODE_COUNT] ([0]=left, [1]=right).  While it returns
 *   false the measurement is still in flight and kohm_out is left untouched.
 */
extern np_hub_status_t np_cvns_hal_impedance_measure_start(void);      /* OI-CVNS-HUB-09 */
extern bool            np_cvns_hal_impedance_measure_poll(float kohm_out[]); /* OI-CVNS-HUB-09 */

/* ── Library contexts (single active CVNS session per device) ────────────────── */

static np_cvns_interlock_ctx_t s_interlock;
static np_cvns_stim_ctx_t      s_stim;
static np_cvns_session_ctx_t   s_session;

/* ── Driver state ────────────────────────────────────────────────────────────── */

/* OI-CVNS-HUB-09: advisory per-electrode impedance measurement sub-state. */
typedef enum {
    NP_CVNS_IMP_IDLE      = 0,  /* no measurement in flight; start on next entry */
    NP_CVNS_IMP_MEASURING = 1,  /* hub-side HAL running; polling each tick        */
    NP_CVNS_IMP_DONE      = 2,  /* a passing result was fed; do not re-measure    */
} np_mod_cvns_imp_state_t;

typedef struct {
    bool     ready;             /* init() completed; contexts valid            */
    bool     active;            /* a session command is running                */
    bool     enable_requested;  /* NP_SAFETY_EN_CVNS currently requested        */
    /* OI-CVNS-HUB-09 advisory impedance measurement (reset each session). */
    np_mod_cvns_imp_state_t imp_state;
    uint32_t imp_start_ms;      /* now_ms when the current measurement began     */
    /* OI-CVNS-HUB-11: set while the driver is deliberately faulting the library
     * on an impedance cross-validation failure, so cvns_fault_cb suppresses its
     * generic SHDR log (the divergence is logged once with a dedicated code). */
    bool     crossval_faulting;
    /* Cached, non-biology display fields for telemetry() (UHDR-class current /
     * impedance, same as np_mod_stim; NO HR is ever cached here). */
    uint16_t last_current_ua;
    float    last_impedance_kohm;
    np_cvns_fault_reason_t last_fault;
} np_mod_cvns_state_t;

static np_mod_cvns_state_t s_state;

/* ── Unified-heartbeat snapshot (OI-CVNS-HUB-08) ─────────────────────────────── */

/*
 * Latest safety-MCU heartbeat reply, published by the safety-heartbeat task via
 * np_mod_cvns_set_heartbeat_status() and consumed by cvns_apply_heartbeat() from
 * the session-runner control task (np_mod_cvns_tick).  Two naturally-atomic word
 * stores; `volatile` because the writer and reader run on different tasks.  A
 * snapshot at most one beat (200 ms) stale is harmless — it is re-evaluated on
 * every 100 ms tick.
 */
static volatile bool     s_hb_valid;
static volatile uint16_t s_hb_granted_mask;
static volatile uint8_t  s_hb_mcu_status;

/*
 * OI-CVNS-HUB-11: latest per-electrode impedance the safety MCU reported over
 * SPI (published by the heartbeat task via np_mod_cvns_set_mcu_impedance()).
 * Consumed by cvns_advisory_impedance_step() to cross-validate against the hub's
 * own measurement.  Same single-writer discipline as the heartbeat snapshot: the
 * heartbeat task only STORES here; ALL library mutation is on the control tick.
 */
static volatile bool  s_hb_mcu_imp_valid;
static volatile float s_hb_mcu_imp_kohm[NP_CVNS_ELECTRODE_COUNT];

/*
 * Advisory pre-session impedance FALLBACK value (OI-CVNS-HUB-09): when no
 * hub-side measurement is available (the measurement HAL cannot start, or a
 * measurement does not complete within NP_CVNS_HB_IMPEDANCE_MEASURE_TIMEOUT_MS),
 * the library's advisory NP_CVNS_STAGE_IMPEDANCE step is satisfied with this
 * nominal in-window figure so the session still proceeds.  When the measurement
 * DOES complete it supersedes this placeholder with genuine per-electrode kΩ.
 * Either way the AUTHORITATIVE impedance gate is the safety MCU's enable
 * refusal, not this value.
 */
#define NP_CVNS_HB_IMPEDANCE_NOMINAL_KOHM  (NP_CVNS_IMPEDANCE_MAX_KOHM * 0.5f)

/*
 * OI-CVNS-HUB-09: max wall-clock a single advisory measurement may run before
 * the driver falls back to the nominal placeholder (fail-open on liveness — the
 * safety MCU remains the authoritative gate).  Matches the re-enable manager's
 * gate-3 timeout (NP_CVNS_REENABLE_IMP_TIMEOUT_MS) — same measurement hardware.
 */
#define NP_CVNS_HB_IMPEDANCE_MEASURE_TIMEOUT_MS  NP_CVNS_REENABLE_IMP_TIMEOUT_MS

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

/* ── OI-CVNS-HUB-09: advisory per-electrode impedance measurement ─────────────── */

/* Feed the nominal in-window fallback to the library so the advisory impedance
 * step can proceed when no genuine measurement is available. */
static void cvns_feed_nominal_impedance(void)
{
    float imp[NP_CVNS_ELECTRODE_COUNT];
    for (uint8_t i = 0U; i < NP_CVNS_ELECTRODE_COUNT; i++) {
        imp[i] = NP_CVNS_HB_IMPEDANCE_NOMINAL_KOHM;
    }
    np_cvns_stim_safety_mcu_response(&s_stim, false, imp);
}

/*
 * OI-CVNS-HUB-11: cross-validate the hub's genuine per-electrode impedance
 * measurement against the safety MCU's report (if one is available this
 * session).  Returns true iff the two DIVERGE beyond NP_CVNS_IMPEDANCE_CROSSVAL_
 * KOHM on any electrode.  When the MCU report is not yet valid, returns false
 * (no cross-check possible — the MCU's own enable refusal stays authoritative).
 * Mirrors the cardiac main-vs-MCU baseline cross-check (NP_CVNS_BASELINE_
 * CROSSVAL_BPM).
 */
static bool cvns_impedance_diverges(const float hub_kohm[])
{
    if (!s_hb_mcu_imp_valid) {
        return false;
    }
    for (uint8_t i = 0U; i < NP_CVNS_ELECTRODE_COUNT; i++) {
        float d = hub_kohm[i] - s_hb_mcu_imp_kohm[i];
        if (d < 0.0f) { d = -d; }
        if (d > NP_CVNS_IMPEDANCE_CROSSVAL_KOHM) {
            return true;
        }
    }
    return false;
}

/*
 * OI-CVNS-HUB-11: hub-vs-MCU impedance disagreement — fail closed.  Logs one
 * SHDR diagnostic (dedicated crossval code, suppressed timestamp) and drives the
 * library to FAULT so stimulation is never enabled with mismatched impedance
 * sensing.  The safety MCU's enable refusal remains the authoritative gate; this
 * is an additional main-processor block.  The crossval_faulting flag suppresses
 * cvns_fault_cb's generic SHDR log so exactly ONE diagnostic is recorded.
 */
static void cvns_impedance_crossval_fail(void)
{
    s_state.crossval_faulting = true;
    np_log_shdr_fault(NP_HUB_SLOT_CVNS, NP_MOD_CVNS,
                      NP_CVNS_SHDR_EV_IMP_CROSSVAL, 0U /* ts suppressed */);
    /* Drive the library to FAULT: stim → FAULT, interlock → FAULT (which fires
     * cvns_fault_cb → drops the unified enable; its generic log is suppressed).
     * The next np_cvns_session_tick() advances the session stage to FAULT. */
    np_cvns_stim_safety_mcu_response(&s_stim, false, NULL);
    np_cvns_interlock_spi_response(&s_interlock,
                                   NP_CVNS_SPI_RSP_ENABLE_REJECTED, NULL, 0U);
    s_state.crossval_faulting = false;
}

/*
 * Drive the hub-side per-electrode impedance measurement for the advisory
 * NP_CVNS_STAGE_IMPEDANCE step, replacing the OI-CVNS-HUB-09 nominal placeholder
 * with genuine measured kΩ.  Called only while the library is in the impedance
 * stage and has not yet accepted a reading (stage == IMPEDANCE && !impedance_ok).
 *
 * The measured values flow into the library's impedance_kohm[] and thence into
 * the UHDR/SHDR session records and app display — so the recorded impedance is
 * honest.  The AUTHORITATIVE enable gate remains the safety MCU's refusal in
 * cvns_apply_heartbeat() block (3); this only supplies truthful values.
 *
 * Retry-until-pass while in the impedance stage:
 *   IDLE      → start a measurement.  If the HAL cannot start (absent/busy),
 *               feed the nominal fallback and latch DONE — the session proceeds
 *               exactly as it did before OI-CVNS-HUB-09 (liveness preserved).
 *   MEASURING → poll.  Complete + all-in-window → feed values, latch DONE (the
 *               library advances to baseline this tick).  Complete + any
 *               electrode out-of-window → feed the values (records the reading;
 *               impedance_ok stays false so the library holds), then IDLE to
 *               re-measure (recovers if electrode contact is corrected).  Not
 *               complete + timeout elapsed → nominal fallback, latch DONE so a
 *               stalled HAL never hard-stalls the session.
 *   DONE      → nothing (a passing/fallback reading was already fed).
 *
 * Returns true iff the step FAULTED the library on an OI-CVNS-HUB-11 impedance
 * cross-validation divergence, so the caller can stop processing this beat.
 */
static bool cvns_advisory_impedance_step(uint32_t now_ms)
{
    switch (s_state.imp_state) {

    case NP_CVNS_IMP_IDLE:
        if (np_cvns_hal_impedance_measure_start() == NP_HUB_OK) {
            s_state.imp_start_ms = now_ms;
            s_state.imp_state    = NP_CVNS_IMP_MEASURING;
        } else {
            cvns_feed_nominal_impedance();
            s_state.imp_state = NP_CVNS_IMP_DONE;
        }
        break;

    case NP_CVNS_IMP_MEASURING: {
        float kohm[NP_CVNS_ELECTRODE_COUNT];
        if (np_cvns_hal_impedance_measure_poll(kohm)) {
            bool all_ok = true;
            for (uint8_t i = 0U; i < NP_CVNS_ELECTRODE_COUNT; i++) {
                if (kohm[i] > NP_CVNS_IMPEDANCE_MAX_KOHM) { all_ok = false; }
            }

            /* OI-CVNS-HUB-11: a genuine, individually-in-window hub reading is
             * cross-validated against the safety MCU's per-electrode report.  A
             * disagreement beyond tolerance is fail-closed: fault the library and
             * do NOT feed a passing impedance (the session never reaches
             * delivery).  When the two agree — or the MCU report is not yet
             * available — proceed exactly as before. */
            if (all_ok && cvns_impedance_diverges(kohm)) {
                cvns_impedance_crossval_fail();
                s_state.imp_state = NP_CVNS_IMP_DONE;   /* resolved (faulted) */
                return true;
            }

            np_cvns_stim_safety_mcu_response(&s_stim, false, kohm);
            /* Pass → done; out-of-window → re-measure (fail-closed, no advance). */
            s_state.imp_state = all_ok ? NP_CVNS_IMP_DONE : NP_CVNS_IMP_IDLE;
        } else if ((now_ms - s_state.imp_start_ms) >=
                   NP_CVNS_HB_IMPEDANCE_MEASURE_TIMEOUT_MS) {
            cvns_feed_nominal_impedance();
            s_state.imp_state = NP_CVNS_IMP_DONE;
        }
        break;
    }

    case NP_CVNS_IMP_DONE:
    default:
        break;
    }
    return false;
}

/* ── OI-CVNS-HUB-08: unified heartbeat → library SPI responses ───────────────── */

/*
 * The np_cervical_vns library was written against a *dedicated* main↔safety-MCU
 * SPI link and advances its enable/impedance state machine only when fed the
 * responses np_cvns_interlock_spi_response() / np_cvns_stim_safety_mcu_response().
 * In the real hub there is no such link — the unified 200 ms heartbeat
 * (np_safety_spi) is the only channel to the safety MCU.  This bridge translates
 * the two facts the heartbeat reply carries about cervical VNS —
 *   • whether the safety MCU is granting NP_SAFETY_EN_CVNS (granted_mask), and
 *   • the MCU status byte (NP_SAFETY_STATUS_IMPEDANCE / _CUTOFF / hard faults)
 * — into those response entry points, so the library's advisory state machine
 * stays in lock-step with the physically-authoritative safety MCU.
 *
 * Division of authority:
 *   • Enable + impedance gating belong to the safety MCU: the library only
 *     *believes* it is enabled after the MCU actually grants the CVNS bit
 *     (ENABLE_GRANTED fed here); if the MCU refuses or withdraws a granted enable
 *     for a non-cardiac reason (impedance / hard fault / cutoff), the library is
 *     faulted (ENABLE_REJECTED).
 *   • The cardiac cutoff path (NP_SAFETY_STATUS_CARDIAC) is deliberately NOT
 *     handled here — it is owned by the hub-global re-enable manager
 *     (np_cvns_reenable, OI-CVNS-HUB-01) and the main-processor PPG path
 *     (np_mod_cvns_push_ppg → cvns_fault_cb).  Feeding it here too would
 *     double-drive the library fault and double-log SHDR.
 *
 * Single-writer of the library contexts: the heartbeat task only STORES the
 * snapshot (np_mod_cvns_set_heartbeat_status); ALL library mutation happens here,
 * invoked from np_mod_cvns_tick() on the one control-task context.
 *
 * OI-CVNS-HUB-09 (CLOSED): the advisory NP_CVNS_STAGE_IMPEDANCE step is now
 *   satisfied by a genuine hub-side per-electrode measurement
 *   (cvns_advisory_impedance_step → np_cvns_hal_impedance_measure_*), so the
 *   UHDR/SHDR impedance fields record real kΩ.  The unified heartbeat still
 *   carries only a pass/fail impedance bit; when no measurement is available the
 *   step falls back to NP_CVNS_HB_IMPEDANCE_NOMINAL_KOHM (previous behaviour).
 *
 * Residual:
 *   A safety-MCU enable refusal surfaces through the library's generic fault
 *     path, which np_cvns_session_tick() records with cutoff_occurred=1 even
 *     though it was not a cardiac cutoff; the SHDR fault_reason is correct
 *     (NP_CVNS_FAULT_SAFETY_MCU).  Correcting the library's fault taxonomy is a
 *     separate library change, not this hub bridge.
 */
static void cvns_apply_heartbeat(uint32_t now_ms)
{
    if (!s_state.ready || !s_state.active || !s_hb_valid) {
        return;
    }

    const uint16_t granted = s_hb_granted_mask;
    const uint8_t  status  = s_hb_mcu_status;

    const bool cvns_granted    = (granted & NP_SAFETY_EN_CVNS) != 0U;
    const bool cardiac         = (status & NP_SAFETY_STATUS_CARDIAC) != 0U;
    const bool impedance_fault = (status & NP_SAFETY_STATUS_IMPEDANCE) != 0U;
    const bool adverse         = ((status & NP_CVNS_NONRECOVERABLE_FAULTS) != 0U) ||
                                 ((status & NP_SAFETY_STATUS_CUTOFF) != 0U) ||
                                 impedance_fault;

    const np_cvns_stage_t           stage    = np_cvns_session_stage(&s_session);
    const np_cvns_interlock_state_t il_state = np_cvns_interlock_state(&s_interlock);

    /* (1) Advisory impedance step (OI-CVNS-HUB-09) — drive a genuine hub-side
     * per-electrode measurement, feeding real kΩ into the library so its
     * UHDR/SHDR impedance fields are honest; fall back to a nominal in-window
     * value only when no measurement is available, so the library still proceeds
     * past NP_CVNS_STAGE_IMPEDANCE to baseline.  The AUTHORITATIVE impedance gate
     * remains the safety MCU's enable refusal in (3). */
    if (stage == NP_CVNS_STAGE_IMPEDANCE && !s_stim.impedance_ok) {
        if (cvns_advisory_impedance_step(now_ms)) {
            /* OI-CVNS-HUB-11 cross-validation divergence — the library is
             * faulted; do not evaluate the enable grant/reject blocks below
             * against this (now stale) beat. */
            return;
        }
    }

    /* (2) Enable grant — the MCU has asserted the CVNS enable GPIO while the
     * library is waiting for it: advance the interlock PRE_SESSION → ENABLED and
     * mark the stim granted so its ramp may begin.  Gated on PRE_SESSION so it
     * fires exactly once per enable. */
    if (il_state == NP_CVNS_INTERLOCK_PRE_SESSION && cvns_granted) {
        np_cvns_interlock_spi_response(&s_interlock,
                                       NP_CVNS_SPI_RSP_ENABLE_GRANTED, NULL, 0U);
        np_cvns_stim_safety_mcu_response(&s_stim, true, NULL);
        return;
    }

    /* (3) Enable refusal / withdrawal (NON-cardiac only).  Fire only on an
     * explicit adverse MCU signal (never merely on an absent grant bit, which is
     * ordinary request latency), while the hub is still requesting the enable —
     * so a normal ramp-down (hub already dropped the request) is never faulted,
     * and the cardiac path is left entirely to np_cvns_reenable + the PPG path. */
    if (!cardiac && !cvns_granted && adverse && s_state.enable_requested &&
        (il_state == NP_CVNS_INTERLOCK_PRE_SESSION ||
         il_state == NP_CVNS_INTERLOCK_ENABLED)) {
        np_cvns_interlock_spi_response(&s_interlock,
                                       NP_CVNS_SPI_RSP_ENABLE_REJECTED, NULL, 0U);
        np_cvns_stim_safety_mcu_response(&s_stim, false, NULL);
    }
}

/*
 * np_mod_cvns_set_heartbeat_status — publish the latest safety-MCU heartbeat
 * reply for the next tick to fold into the library (OI-CVNS-HUB-08).  Called
 * once per beat from the safety-heartbeat task AFTER np_safety_spi_heartbeat().
 * Store-only: it never touches the library contexts (single-writer rule).
 */
void np_mod_cvns_set_heartbeat_status(uint16_t granted_mask, uint8_t mcu_status)
{
    s_hb_granted_mask = granted_mask;
    s_hb_mcu_status   = mcu_status;
    s_hb_valid        = true;
}

/*
 * np_mod_cvns_set_mcu_impedance — publish the safety MCU's per-electrode CVNS
 * impedance (OI-CVNS-HUB-11) for the next tick's cross-validation.  Called once
 * per beat from the safety-heartbeat task AFTER np_safety_spi_heartbeat().
 * Store-only (single-writer rule); the control tick reads and compares it.
 *
 * kohm:  NP_CVNS_ELECTRODE_COUNT floats ([0]=left,[1]=right).  Ignored if NULL.
 * valid: whether the MCU reported a completed, checksum-valid measurement.
 */
void np_mod_cvns_set_mcu_impedance(const float kohm[], bool valid)
{
    if (valid && kohm != NULL) {
        for (uint8_t i = 0U; i < NP_CVNS_ELECTRODE_COUNT; i++) {
            s_hb_mcu_imp_kohm[i] = kohm[i];
        }
        s_hb_mcu_imp_valid = true;
    } else {
        s_hb_mcu_imp_valid = false;
    }
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
     * clock in SHDR (privacy gate).
     *
     * OI-CVNS-HUB-11: suppressed while the driver is faulting the library on an
     * impedance cross-validation failure — that path logs its own dedicated
     * diagnostic (NP_CVNS_SHDR_EV_IMP_CROSSVAL), so logging the generic reason
     * here too would double-record the one event. */
    if (!s_state.crossval_faulting) {
        np_log_shdr_fault(NP_HUB_SLOT_CVNS, NP_MOD_CVNS, (uint8_t)reason, 0U);
    }
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

    /* Drop any stale heartbeat snapshot (OI-CVNS-HUB-08) so a grant observed for
     * a previous session can never advance a fresh one. */
    s_hb_valid        = false;
    s_hb_granted_mask = 0U;
    s_hb_mcu_status   = NP_SAFETY_STATUS_OK;
    s_hb_mcu_imp_valid = false;   /* OI-CVNS-HUB-11 */

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
        s_hb_valid         = false;   /* OI-CVNS-HUB-08: discard stale snapshot */
        s_hb_mcu_imp_valid = false;   /* OI-CVNS-HUB-11 */
        return NP_HUB_OK;
    }

    if (len < sizeof(np_mod_cvns_params_t)) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    const np_mod_cvns_params_t *p = (const np_mod_cvns_params_t *)params;

    /* Fresh session: discard any snapshot published before this start so the
     * first tick cannot act on a previous session's grant (OI-CVNS-HUB-08), and
     * re-arm the advisory impedance measurement (OI-CVNS-HUB-09). */
    s_hb_valid           = false;
    s_hb_mcu_imp_valid   = false;   /* OI-CVNS-HUB-11: no stale MCU report */
    s_state.imp_state    = NP_CVNS_IMP_IDLE;
    s_state.imp_start_ms = 0U;

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

    /* OI-CVNS-HUB-08: fold the latest safety-MCU heartbeat status into the
     * library's SPI-response state BEFORE it advances this tick, so an enable
     * grant / impedance verdict lands the same tick the library consumes it. */
    cvns_apply_heartbeat(now_ms);

    np_cvns_session_tick(&s_session, now_ms, now_s);

    /* Track the unified enable bit to the library's intent: assert once the
     * library has requested enable (interlock PRE_SESSION, awaiting the MCU
     * grant) or is delivering, and drop the moment it is neither.  The
     * PRE_SESSION case is what lets the MCU actually see the request — its grant
     * then feeds ENABLE_GRANTED back through cvns_apply_heartbeat(). */
    np_cvns_stage_t stage = np_cvns_session_stage(&s_session);
    if (stage_is_delivering(stage) ||
        np_cvns_interlock_state(&s_interlock) == NP_CVNS_INTERLOCK_PRE_SESSION) {
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

/*
 * np_mod_cvns_active — true while a CVNS command is running, from command start
 * (impedance/baseline) through a terminal stage.  The session-runner control
 * task (OI-CVNS-HUB-07) polls this to decide when to drive np_mod_cvns_tick()
 * and when to shorten its loop sleep to NP_CVNS_STIM_TICK_MS.  Single-bool read.
 */
bool np_mod_cvns_active(void)
{
    return s_state.active;
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
/* Apply the current heartbeat snapshot to the library in isolation (no
 * session_tick), so OI-CVNS-HUB-08 grant / reject / impedance translation can be
 * asserted directly against library state. */
void np_mod_cvns_test_apply_heartbeat(uint32_t now_ms) { cvns_apply_heartbeat(now_ms); }
#endif /* NPTEST_HOST */
