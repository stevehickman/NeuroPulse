/*
 * NeurOne Cervical VNS — Cardiac Interlock API
 * Document: NP-FW-CVNS-001 Rev 1 §6
 *
 * Main-processor side of the dual-processor cardiac safety interlock.
 * The safety MCU (STM32G071) independently holds the cervical VNS enable GPIO
 * and monitors cardiac rhythm via the R-peak GPIO line.  This module drives the
 * main-processor side: PPG processing, baseline computation, SPI exchange.
 */

#ifndef NP_CVNS_INTERLOCK_H
#define NP_CVNS_INTERLOCK_H

#include "np_cvns_types.h"

/* ── Interlock configuration ─────────────────────────────────────────────────── */
typedef struct {
    uint32_t ppg_sample_rate_hz;   /* must equal NP_CVNS_PPG_SAMPLE_RATE_HZ    */
    uint32_t now_s;                /* initial wall-clock time (for lockout timer)*/
} np_cvns_interlock_config_t;

/* ── Opaque context (allocated by caller) ────────────────────────────────────── */
struct np_cvns_interlock_ctx {
    np_cvns_rr_buf_t         rr_buf;
    np_cvns_interlock_state_t state;

    /* Pan-Tompkins detection state */
    float    pt_filtered;
    float    pt_diff;
    float    pt_squared;
    float    pt_integrated;
    float    pt_running_max;
    float    pt_threshold;
    uint32_t pt_last_peak_ms;
    uint32_t pt_max_window_start_ms;

    /* Intervals forwarded to the safety MCU (OI-CVNS-14): every peak pulsed on
     * RPEAK_IN, with no upper bound, so the HR report mirrors what the MCU's own
     * mean was built from.  rr_buf cannot serve: it drops intervals over
     * NP_CVNS_RR_MAX_VALID_MS, which the MCU still sees. */
    uint16_t fwd_rr_ms[NP_CVNS_FWD_RR_COUNT];
    uint8_t  fwd_head;
    uint8_t  fwd_count;
    uint32_t fwd_last_ms;

    /* Baseline */
    float    baseline_hr_bpm;
    bool     baseline_valid;
    uint8_t  baseline_beats_accumulated;

    /* Fault tracking */
    np_cvns_fault_reason_t fault_reason;
    uint32_t               fault_time_s;    /* wall-clock seconds at cutoff      */
    uint32_t               last_rpeak_ms;   /* ms timestamp; for data-loss detect*/
    uint32_t               rpeak_pulses;    /* RPEAK_IN pulses emitted (OI-CVNS-13)*/

    /* Callbacks */
    np_cvns_fault_cb_t fault_cb;
};

_Static_assert(sizeof(struct np_cvns_interlock_ctx) <= NP_CVNS_INTERLOCK_CTX_SIZE_BYTES,
               "np_cvns_interlock_ctx exceeds NP_CVNS_INTERLOCK_CTX_SIZE_BYTES");

/* ── Lifecycle ───────────────────────────────────────────────────────────────── */
np_cvns_status_t np_cvns_interlock_init(np_cvns_interlock_ctx_t        *ctx,
                                         np_cvns_interlock_config_t      config,
                                         np_cvns_fault_cb_t              fault_cb);

void np_cvns_interlock_deinit(np_cvns_interlock_ctx_t *ctx);

/* ── PPG feed-in (call from PPG ISR at NP_CVNS_PPG_SAMPLE_RATE_HZ) ──────────── */
/*
 * Each call processes one raw PPG sample through the Pan-Tompkins pipeline.
 * On R-peak detection, asserts the RPEAK_IN GPIO pulse to the safety MCU and
 * updates the R-R interval buffer.
 *
 * timestamp_ms: free-running millisecond counter from application scheduler.
 */
void np_cvns_interlock_push_ppg(np_cvns_interlock_ctx_t *ctx,
                                  uint32_t                  sample,
                                  uint32_t                  timestamp_ms);

/* ── Session control ─────────────────────────────────────────────────────────── */
/*
 * Requests stimulation enable from the safety MCU.  Fails if:
 *   - Baseline HR not yet valid (NP_CVNS_ERR_BASELINE_INVALID)
 *   - In fault lockout (NP_CVNS_ERR_LOCKOUT)
 *   - Already enabled (NP_CVNS_ERR_SESSION_ACTIVE)
 * On success, transitions to NP_CVNS_INTERLOCK_PRE_SESSION; safety MCU
 * response arrives asynchronously via np_cvns_interlock_spi_response().
 */
np_cvns_status_t np_cvns_interlock_request_enable(np_cvns_interlock_ctx_t *ctx,
                                                    uint32_t                  now_s);

/*
 * Immediate disable — no fault, no lockout.  Used for normal session end.
 */
void np_cvns_interlock_disable(np_cvns_interlock_ctx_t *ctx);

/*
 * Re-enable after fault.  Fails if lockout has not elapsed, or if baseline
 * is not re-established, or if no explicit app confirmation has been received.
 * app_confirmed: must be true (set by session manager after app sends
 * NP_CVNS_SPI_CMD_ENABLE following explicit user action in app UI).
 */
np_cvns_status_t np_cvns_interlock_request_reenable(np_cvns_interlock_ctx_t *ctx,
                                                      bool                     app_confirmed,
                                                      uint32_t                 now_s);

/* ── Safety MCU SPI response handler ────────────────────────────────────────── */
/*
 * Called from the SPI ISR when the safety MCU transmits a response byte.
 * cmd:     the response command byte (NP_CVNS_SPI_RSP_*)
 * payload: response payload bytes
 * len:     payload length
 */
void np_cvns_interlock_spi_response(np_cvns_interlock_ctx_t *ctx,
                                     uint8_t                   cmd,
                                     const uint8_t            *payload,
                                     uint8_t                   len);

/* ── Periodic tick (call at 1 Hz from application scheduler) ─────────────────── */
/*
 * Checks data-loss timeout and lockout expiry.
 * now_ms: free-running millisecond counter.
 * now_s:  wall-clock seconds.
 */
void np_cvns_interlock_tick(np_cvns_interlock_ctx_t *ctx,
                              uint32_t                  now_ms,
                              uint32_t                  now_s);

/* ── State accessors ─────────────────────────────────────────────────────────── */
np_cvns_interlock_state_t np_cvns_interlock_state(const np_cvns_interlock_ctx_t *ctx);
float                     np_cvns_interlock_baseline_hr(const np_cvns_interlock_ctx_t *ctx);
/* Main-processor HR estimate now: the mean of the last
 * NP_CVNS_BASELINE_BEATS_MIN R-R intervals, the same window the baseline uses.
 * 0 if no interval has been seen.  Not the safety MCU's value — the two sides
 * are independent implementations (NP-FW-CVNS-001 §5). */
float                     np_cvns_interlock_current_hr(const np_cvns_interlock_ctx_t *ctx);
/*
 * Heart-rate estimate for the safety MCU's ±5 BPM cross-check (OI-CVNS-14).
 * The mean of the last `n` intervals that were FORWARDED on RPEAK_IN, in 0.1 BPM,
 * with the age of the newest one.  Returns false, and writes nothing, until n
 * intervals exist or if the newest is older than UINT16_MAX ms: the frame then
 * carries "no estimate".  `n` is NP_SAFETY_HR_REPORT_INTERVALS, the MCU's own
 * span.  The estimate comes from this side's detector, not from the pulse train,
 * so it follows a fault in the GPIO or timer path but NOT one in the detector:
 * a detector that re-fires is seen identically on both sides (§14.7).
 */
bool                      np_cvns_interlock_hr_report(const np_cvns_interlock_ctx_t *ctx,
                                                      uint32_t now_ms, uint8_t n,
                                                      uint16_t *hr_x10_out,
                                                      uint16_t *age_ms_out);
bool                      np_cvns_interlock_baseline_valid(const np_cvns_interlock_ctx_t *ctx);
np_cvns_fault_reason_t    np_cvns_interlock_fault_reason(const np_cvns_interlock_ctx_t *ctx);
/* RPEAK_IN pulses emitted to the safety MCU since init.  Every detected peak
 * >= NP_CVNS_RR_MIN_VALID_MS after the last is pulsed, with no upper bound
 * (NP-FW-CVNS-001 Rev 11 §14.6, OI-CVNS-13). */
uint32_t                  np_cvns_interlock_rpeak_pulses(const np_cvns_interlock_ctx_t *ctx);

uint32_t np_cvns_interlock_reenable_lockout_remaining_s(
                                     const np_cvns_interlock_ctx_t *ctx,
                                     uint32_t                       now_s);

#endif /* NP_CVNS_INTERLOCK_H */
