/*
 * NeurOne HD-tDCS — Stimulation Delivery API
 * Document: NP-FW-HD-001 Rev A §7
 *
 * Programs the 16-ch tACS driver with per-channel DC currents.
 * All enable/disable lines are owned by the safety MCU.
 * Charge density is enforced by safety MCU hardware; this module also
 * tracks it in software for display and UHDR recording.
 */

#ifndef NP_HD_STIM_H
#define NP_HD_STIM_H

#include "np_hd_types.h"

/* ── Context (opaque) ────────────────────────────────────────────────────────── */
typedef struct np_hd_stim_ctx np_hd_stim_ctx_t;
#define NP_HD_STIM_CTX_SIZE_BYTES   (256U)

/* ── Safety MCU response callback ───────────────────────────────────────────── */
/*
 * Called asynchronously from the SPI interrupt when the safety MCU replies to
 * an impedance check or stimulation enable request.
 *
 * granted: true if safety MCU approved the request.
 * impedance_kohm: per-electrode impedance array (NP_HD_RING_ELECTRODE_COUNT
 *   entries); valid only for impedance check replies.
 */
typedef void (*np_hd_safety_response_cb_t)(np_hd_stim_ctx_t *ctx,
                                            bool              granted,
                                            const float       impedance_kohm[]);

/* ── Lifecycle ───────────────────────────────────────────────────────────────── */

/*
 * Initialize stimulation context for the given montage.
 * Maps montage electrodes to tACS driver channels.
 * safety_cb: called when safety MCU responds via SPI.
 */
np_hd_status_t np_hd_stim_init(np_hd_stim_ctx_t         *ctx,
                                 const np_hd_montage_t    *montage,
                                 np_hd_safety_response_cb_t safety_cb);

/* Tear down: forces all driver channels to 0, releases context. */
void np_hd_stim_deinit(np_hd_stim_ctx_t *ctx);

/* ── Impedance check ─────────────────────────────────────────────────────────── */

/*
 * Request the safety MCU to measure electrode impedance for all montage
 * electrodes via 1 kHz AC injection.  Response arrives in safety_cb.
 * Returns NP_HD_ERR_SESSION_ACTIVE if stimulation is already running.
 */
np_hd_status_t np_hd_stim_request_impedance_check(np_hd_stim_ctx_t *ctx);

/* ── Stimulation control ──────────────────────────────────────────────────────── */

/*
 * Request stimulation start.  Sends enable request to safety MCU.
 * Safety MCU confirms impedance + charge density limit before granting.
 * Actual current delivery begins in np_hd_stim_tick() after grant.
 *
 * anode_current_ua: target anode current (≤ NP_HD_MAX_CURRENT_UA).
 *   Cathode currents are anode_current_ua / NP_HD_CATHODE_SPLIT_DENOM each.
 * duration_s: total stimulation time including 30 s ramps.
 */
np_hd_status_t np_hd_stim_start(np_hd_stim_ctx_t *ctx,
                                  uint16_t          anode_current_ua,
                                  uint16_t          duration_s,
                                  uint32_t          now_ms);

/*
 * Immediately stop all stimulation.  Drives all channels to 0 within one
 * SPI transaction.  Safety MCU clears its enable lines independently via
 * the 200 ms heartbeat watchdog (CLAUDE.md §4.2).
 */
void np_hd_stim_stop(np_hd_stim_ctx_t *ctx);

/*
 * Periodic tick — call from a FreeRTOS task at 100 ms interval.
 *
 * Drives the ramp state machine:
 *   RAMP_UP: linearly increases current from 0 to target over 30 s.
 *   STEADY:  holds at target for (duration_s - 60) seconds.
 *   RAMP_DOWN: linearly decreases from target to 0 over 30 s.
 *   DONE:    all channels zeroed; signals session end.
 *
 * Also monitors accumulated charge per electrode and calls np_hd_stim_stop()
 * if the NP_HD_MAX_CHARGE_PER_PHASE_UC limit is approached.
 */
void np_hd_stim_tick(np_hd_stim_ctx_t *ctx, uint32_t now_ms);

/*
 * Deliver the safety MCU's asynchronous response to a previous request.
 * Call from SPI ISR or safety MCU task when SPI reply arrives.
 */
void np_hd_stim_safety_mcu_response(np_hd_stim_ctx_t *ctx,
                                     bool              granted,
                                     const float       impedance_kohm[]);

/* ── Diagnostics ─────────────────────────────────────────────────────────────── */

np_hd_stim_phase_t np_hd_stim_phase(const np_hd_stim_ctx_t *ctx);

/* Accumulated charge delivered to anode electrode this session (µA·s). */
float np_hd_stim_anode_charge_ua_s(const np_hd_stim_ctx_t *ctx);

/* Charge density at anode (µC/cm²) = charge_ua_s / (area_cm² × 1000). */
float np_hd_stim_charge_density_uc_cm2(const np_hd_stim_ctx_t *ctx);

/*
 * Fill per-electrode state array (NP_HD_RING_ELECTRODE_COUNT entries).
 * Returns NP_HD_ERR_INVALID_ARG if out is NULL.
 */
np_hd_status_t np_hd_stim_get_electrode_states(const np_hd_stim_ctx_t *ctx,
                                                 np_hd_electrode_state_t *out,
                                                 uint8_t                  n);

#endif /* NP_HD_STIM_H */
