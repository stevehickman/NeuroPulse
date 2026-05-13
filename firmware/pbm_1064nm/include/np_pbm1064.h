#ifndef NP_PBM1064_H
#define NP_PBM1064_H

#include "np_pbm1064_types.h"

/* -----------------------------------------------------------------------
 * np_pbm1064_detect — Smart module slot detection
 * ----------------------------------------------------------------------- */

/* Run smart module detection for all 5 zone slots.
 * Reads ZONE_ID ADC with 3×100 ms debounce (RISK-18).
 * Populates session->smart_module_mask and zone[i].slot_type.
 * Base modules continue via existing zone_announce path; this function
 * sets NP_SLOT_SMART_MODULE only for ADC < NP_PBM1064_SMART_MODULE_ADC_THRESHOLD. */
void np_pbm1064_detect_all(np_pbm1064_session_t *session);

/* Probe one slot's on-module driver IC via I2C.
 * Returns true if I2C ACK received within NP_PBM1064_I2C_PROBE_TIMEOUT_MS. */
bool np_pbm1064_i2c_probe(uint8_t slot);

/* -----------------------------------------------------------------------
 * np_pbm1064_drive — I2C command protocol to on-module driver
 * ----------------------------------------------------------------------- */

/* Initialise driver IC for a zone slot: write CONFIG, current, frequency, duty.
 * Enforces duty cycle ceiling (NP_PBM1064_DUTY_MAX_REG) unconditionally. */
void np_pbm1064_drive_init(uint8_t slot, const np_pbm1064_preset_t *preset);

/* Enable/disable individual channels. Safety MCU PBM_EN_L is separate
 * (always asserted by np_pbm_safety_mcu_enable HAL before calling this). */
void np_pbm1064_set_ch_enable(uint8_t slot, uint8_t ch_mask);

/* Update frequency for a channel (EEG-adaptive closed-loop updates). */
void np_pbm1064_set_freq(uint8_t slot, np_pbm_channel_t ch, uint8_t freq_hz);

/* Update duty for a channel. Silently clamps to NP_PBM1064_DUTY_MAX_REG. */
void np_pbm1064_set_duty(uint8_t slot, np_pbm_channel_t ch, uint8_t duty_pct);

/* Read STATUS register. Returns 0xFF on I2C error (all fault bits set). */
uint8_t np_pbm1064_read_status(uint8_t slot);

/* Read NTC temperature (°C, signed) from driver IC. */
int8_t np_pbm1064_read_ntc(uint8_t slot);

/* -----------------------------------------------------------------------
 * np_pbm1064_dose — InGaAs PD dose metering
 * ----------------------------------------------------------------------- */

/* Load calibration coefficients from Config partition.
 * Falls back to NP_PBM1064_K_DEFAULT_* values if no factory cal present;
 * sets cal_from_factory[zone] = false and SHDR flag. */
void np_pbm1064_load_cal(np_pbm_cal_t *cal);

/* 10 Hz dose accumulation tick. Call from session timer ISR or task.
 * Reads PD1+PD2 ADC for all active smart module zones, accumulates J/cm²,
 * checks fouling/aging ratio thresholds, writes UHDR sample. */
void np_pbm1064_dose_tick(np_pbm1064_session_t *session);

/* Check aggregate irradiance estimate against thermal budget ceiling.
 * Calls np_pbm1064_throttle_cascade if over limit. */
void np_pbm1064_thermal_check(np_pbm1064_session_t *session);

/* Throttle cascade: reduce highest-power channel first.
 * Priority: CH_C → CH_B → CH_A (per §5.4 NP-FW-PBM1064-001). */
void np_pbm1064_throttle_cascade(uint8_t zone, float p_aggregate_mw_cm2,
                                 np_pbm1064_session_t *session);

/* -----------------------------------------------------------------------
 * np_pbm1064_session — Session orchestration
 * ----------------------------------------------------------------------- */

/* Run full pre-session checklist (§3.1 NP-SES-1064-001).
 * Returns true if all checks pass and session may proceed. */
bool np_pbm1064_session_preflight(np_pbm1064_session_t *session);

/* Start session: enable safety MCU GPIO, init driver ICs, start ramp. */
void np_pbm1064_session_start(np_pbm1064_session_t *session);

/* Update EEG-adaptive frequency for all active smart module zones.
 * Called by EEG closed-loop task when target frequency changes.
 * Applies cortical gradient if preset->cortical_gradient_adapt is set. */
void np_pbm1064_session_eeg_update(np_pbm1064_session_t *session, float eeg_freq_hz);

/* Periodic session tick (call at 100 ms interval).
 * Drives dose accumulation (10 Hz), NTC poll (1 Hz), I2C status poll (5 s),
 * thermal check, ramp state machine. */
void np_pbm1064_session_tick(np_pbm1064_session_t *session);

/* Initiate session end: begin ramp-down, write UHDR, write SHDR. */
void np_pbm1064_session_stop(np_pbm1064_session_t *session);

/* -----------------------------------------------------------------------
 * np_pbm1064_t2_combined — T2 combined 1064 + 1170 nm session
 * ----------------------------------------------------------------------- */

/* Initialise T2 combined session. Both hardware systems must be ready.
 * Returns false if 1170 nm system unavailable (abort session). */
bool np_pbm1064_t2_init(np_pbm1064_t2_session_t *t2, const np_pbm1064_preset_t *zone_presets,
                         void *laser_preset);

/* T2 combined session tick (100 ms). Coordinates both systems.
 * Applies thermal throttle priority: 1170 nm throttled first (§4.3 NP-SES-1064-001). */
void np_pbm1064_t2_tick(np_pbm1064_t2_session_t *t2);

/* Stop T2 combined session. Ramp-down both systems. Write combined UHDR/SHDR. */
void np_pbm1064_t2_stop(np_pbm1064_t2_session_t *t2);

#endif /* NP_PBM1064_H */
