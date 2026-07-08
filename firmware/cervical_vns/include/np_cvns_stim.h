/*
 * NeurOne Cervical VNS — Stimulation Delivery API
 * Document: NP-FW-CVNS-001 Rev A §7
 *
 * Manages biphasic charge-balanced waveform parameters and current ramp.
 * Safety MCU owns the physical enable GPIO; this module computes target current
 * levels and exchanges them with the safety MCU via SPI.
 */

#ifndef NP_CVNS_STIM_H
#define NP_CVNS_STIM_H

#include "np_cvns_types.h"

/* ── Opaque context ──────────────────────────────────────────────────────────── */
struct np_cvns_stim_ctx {
    np_cvns_session_config_t  config;
    np_cvns_safety_response_cb_t safety_cb;

    np_cvns_stim_phase_t  phase;
    uint32_t              phase_start_ms;
    uint32_t              active_end_ms;

    uint16_t  target_ua;
    uint16_t  actual_ua;       /* current during ramp                           */
    float     impedance_kohm[NP_CVNS_ELECTRODE_COUNT];
    bool      impedance_ok;
    bool      safety_mcu_granted;
};

_Static_assert(sizeof(struct np_cvns_stim_ctx) <= NP_CVNS_STIM_CTX_SIZE_BYTES,
               "np_cvns_stim_ctx exceeds NP_CVNS_STIM_CTX_SIZE_BYTES");

/* ── Lifecycle ───────────────────────────────────────────────────────────────── */
np_cvns_status_t np_cvns_stim_init(np_cvns_stim_ctx_t           *ctx,
                                    const np_cvns_session_config_t *config,
                                    np_cvns_safety_response_cb_t   safety_cb);

void np_cvns_stim_deinit(np_cvns_stim_ctx_t *ctx);

/* ── Impedance check ─────────────────────────────────────────────────────────── */
np_cvns_status_t np_cvns_stim_request_impedance_check(np_cvns_stim_ctx_t *ctx);

/* ── Stimulation control ─────────────────────────────────────────────────────── */
np_cvns_status_t np_cvns_stim_start(np_cvns_stim_ctx_t *ctx,
                                     uint32_t            now_ms);

void np_cvns_stim_stop(np_cvns_stim_ctx_t *ctx);

void np_cvns_stim_tick(np_cvns_stim_ctx_t *ctx, uint32_t now_ms);

/* ── Safety MCU response (called from SPI ISR) ───────────────────────────────── */
void np_cvns_stim_safety_mcu_response(np_cvns_stim_ctx_t *ctx,
                                       bool                granted,
                                       const float         impedance_kohm[]);

/* ── Diagnostics ─────────────────────────────────────────────────────────────── */
np_cvns_stim_phase_t np_cvns_stim_phase(const np_cvns_stim_ctx_t *ctx);
uint16_t             np_cvns_stim_current_ua(const np_cvns_stim_ctx_t *ctx);
float                np_cvns_stim_mean_impedance_kohm(const np_cvns_stim_ctx_t *ctx);

#endif /* NP_CVNS_STIM_H */
