/*
 * NeurOne HRV Biofeedback — taVNS Inspiration-Phase Synchronization
 * Document: NP-FW-HRV-001 Rev 1 §8
 *
 * Inspiration detection algorithm (RSA slope method):
 *
 *   Respiratory Sinus Arrhythmia causes HR to INCREASE (RR to SHORTEN) during
 *   inspiration and DECREASE (RR to LENGTHEN) during expiration.
 *
 *   A sliding window of NP_TAVNS_INSP_SLOPE_WIN successive RR differences
 *   (dRR[i] = RR[i] - RR[i-1]) is maintained.  The sign of the window mean:
 *     < -NP_TAVNS_SLOPE_HYSTERESIS ms/beat → inspiration (RR shortening)
 *     > +NP_TAVNS_SLOPE_HYSTERESIS ms/beat → expiration  (RR lengthening)
 *
 *   Inspiration onset (positive-to-negative crossing) opens the taVNS gate.
 *   Expiration onset (negative-to-positive crossing) closes it.
 *
 *   Safety failsafe: the gate is forcibly closed after
 *   NP_TAVNS_MAX_INSP_DURATION_MS regardless of slope state, preventing
 *   continuous stimulation if RSA signal degrades.
 *
 * Safety MCU interface:
 *   np_hrv_tavns_request_enable() → calls safety MCU SPI API.  The safety MCU
 *   independently verifies clip impedance before asserting the VNS enable GPIO.
 *   This module only manages the firmware-side gate logic; hardware authority
 *   remains with the safety MCU at all times.
 */

#ifndef NP_HRV_TAVNS_SYNC_H
#define NP_HRV_TAVNS_SYNC_H

#include "np_hrv_types.h"

/*
 * Callback invoked when the inspiration gate OPENS (stim should start).
 * The callback is responsible for programming taVNS pulse parameters and
 * requesting safety MCU enable.
 *
 * `freq_hz`    — carrier frequency (1–25 Hz)
 * `current_ua` — peak current in microamperes (100–2000)
 */
typedef void (*np_tavns_enable_cb_t)(uint16_t freq_hz, uint16_t current_ua);

/*
 * Callback invoked when the gate CLOSES (stim should stop).
 */
typedef void (*np_tavns_disable_cb_t)(void);

/*
 * Initialize the taVNS synchronization state machine.
 * `stim_freq_hz` and `stim_current_ua` are validated against hardware limits
 * and stored.  `enable_cb` / `disable_cb` are called on gate transitions.
 */
np_hrv_status_t np_hrv_tavns_init(np_tavns_state_t  *tavns,
                                   uint16_t           stim_freq_hz,
                                   uint16_t           stim_current_ua,
                                   np_tavns_enable_cb_t  enable_cb,
                                   np_tavns_disable_cb_t disable_cb);

/*
 * Feed a new R-R interval (ms) and the current system time into the state machine.
 * Must be called each time np_hrv_ppg_process_sample() returns true.
 *
 * Internally updates the slope buffer, detects phase transitions, and fires
 * the appropriate enable/disable callback.
 */
void np_hrv_tavns_process_rr(np_tavns_state_t *tavns,
                               uint16_t          rr_ms,
                               uint32_t          now_ms);

/*
 * Tick function: call at ≥ 10 Hz to enforce the failsafe gate timeout.
 * Also used to update tavns->safety_mcu_granted from async safety MCU response.
 */
void np_hrv_tavns_tick(np_tavns_state_t *tavns, uint32_t now_ms);

/*
 * Immediately close the taVNS gate (e.g., session abort, safety MCU veto).
 * Safe to call from any context.
 */
void np_hrv_tavns_force_disable(np_tavns_state_t *tavns);

/*
 * Called by the safety MCU SPI ISR to notify that the stim enable GPIO has
 * been asserted (grant=true) or vetoed (grant=false).
 */
void np_hrv_tavns_safety_mcu_response(np_tavns_state_t *tavns, bool grant);

#endif /* NP_HRV_TAVNS_SYNC_H */
