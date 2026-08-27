/*
 * NeurOne 1064nm Smart Zone Module — Platform HAL Stubs
 * Document: NP-FW-PBM1064-001 Rev 1 §3
 *
 * These functions are implemented as stubs in np_pbm_hal.c and must be
 * replaced by the platform team with real peripheral driver implementations:
 *
 *   OI-PBM-01: np_pbm_hal_adc_read_pd  — LPADC1 for PD1/PD2 per zone
 *   OI-PBM-02: np_pbm_hal_i2c_write / _read — LPI2C3 slot-addressed
 *   OI-PBM-03: np_pbm_hal_safety_mcu_enable — SPI to STM32G071
 *
 * Stub implementations return NP_PBM_OK and leave output parameters at
 * deterministic test values so that software-passable FAI tests can run.
 */

#ifndef NP_PBM_HAL_H
#define NP_PBM_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "np_pbm_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ⚠ RETIRED by NP-HW-HUB-001 Rev 3 (2026-07-29) — DO NOT IMPLEMENT ON TARGET.
 * ZONE_ID resistor-ladder detection is gone: module type comes from UID-based
 * auto-inventory (np_module_map), per NP-FW-PBM1064-001's supersession note.
 * Retained only to keep the host-test build linking.
 */
bool np_pbm_hal_adc_read_zone_id(uint8_t slot, uint16_t *counts_out);

/*
 * Read PD1 or PD2 photodiode ADC for slot.
 * slot:   0–4.
 * pd_ch:  0 = PD1 (forward emission), 1 = PD2 (scalp-side backscatter).
 * counts_out: 12-bit ADC result.
 * Returns true on success.  OI-PBM-01.
 *
 * NP-HW-HUB-001 Rev 3 (2026-07-29) RESHAPES rather than retires this call.
 * It survives as a thin accessor (so np_pbm_dose.c needs no restructuring)
 * but `slot` becomes a uint16_t socket_id (0..127) and the value is served from
 * a CACHED cluster frame, not a direct LPADC conversion: at 80 sockets the
 * 10 Hz dose tick must pull 10 per-cluster frames via
 * np_hub_cluster_read_frame(), not issue 160 individual bus reads.
 * See NP-HW-HUB-001 Rev 3 §9.2–9.3.
 */
bool np_pbm_hal_adc_read_pd(uint8_t slot, uint8_t pd_ch,
                                 uint16_t *counts_out);

/*
 * ⚠ RETIRED by NP-HW-HUB-001 Rev 3 (2026-07-29) — DO NOT IMPLEMENT ON TARGET.
 * Rev 2 enabled a hub-side LPI2C3 GPIO mux (or a PCA9546A channel) per zone
 * slot. Under Rev 3 the hub has ONE differential cluster bus and no per-socket
 * I2C peripheral: each cluster controller owns its own PCA9548A and isolates
 * its ≤8 sockets locally, one hop from the module. Module transactions are
 * TUNNELLED — the hub asks a cluster controller to relay them. Retained only
 * to keep the host-test build linking. See NP-HW-HUB-001 Rev 3 §5.2, §9.1.
 */
np_pbm_status_t np_pbm_hal_i2c_mux_enable(uint8_t slot, bool enable);

/*
 * Write len bytes to driver IC register reg_addr at 7-bit address
 * NP_PBM_I2C_ADDR on slot's LPI2C3 instance.
 * Returns NP_PBM_OK or NP_PBM_ERR_I2C_WRITE.  OI-PBM-02.
 */
np_pbm_status_t np_pbm_hal_i2c_write(uint8_t  slot,
                                               uint8_t  reg_addr,
                                               const uint8_t *data,
                                               uint8_t  len);

/*
 * Read len bytes from driver IC register reg_addr.
 * Returns NP_PBM_OK or NP_PBM_ERR_I2C_READ.  OI-PBM-02.
 */
np_pbm_status_t np_pbm_hal_i2c_read(uint8_t  slot,
                                              uint8_t  reg_addr,
                                              uint8_t *data,
                                              uint8_t  len);

/*
 * Probe I2C: send address 0x30 and check for ACK within timeout_ms.
 * Returns true if ACK received.  OI-PBM-02.
 */
bool np_pbm_hal_i2c_probe(uint8_t slot, uint8_t i2c_addr,
                                uint32_t timeout_ms);

/*
 * Assert or deassert the safety MCU GPIO enable line for smart module slot.
 * safety MCU owns all stimulation GPIO — main processor requests enable via SPI.
 * Returns NP_PBM_OK or NP_PBM_ERR_SAFETY_REJECTED.  OI-PBM-03.
 */
np_pbm_status_t np_pbm_hal_safety_mcu_enable(uint8_t slot,
                                                       bool    enable);

/*
 * Read NTC temperature (°C) for the specified zone slot.
 * Returns NP_PBM_OK on success; temp_c_out unchanged on error.
 */
np_pbm_status_t np_pbm_hal_ntc_read(uint8_t slot, float *temp_c_out);

/*
 * Return system monotonic time in milliseconds (wraps at UINT32_MAX).
 * Maps to FreeRTOS xTaskGetTickCount() × portTICK_PERIOD_MS.
 */
uint32_t np_pbm_hal_now_ms(void);

/*
 * Write np_pbm_shdr_fault_entry_t to SHDR partition.
 * Caller provides pointer to populated entry; function appends to SHDR log.
 * No-op in stub implementation.
 */
void np_pbm_hal_shdr_log_fault(const np_pbm_shdr_fault_entry_t *entry);

/*
 * Invoke bone conduction zone announcement (delegate to NP-FW-ZA-001).
 * slot_index: 0–4; used to compute zone ID for the announcer.
 */
void np_pbm_hal_zone_announce(uint8_t slot_index);

/*
 * ⚠ RETIRED by NP-HW-HUB-001 Rev 3 (2026-07-29) — DO NOT IMPLEMENT ON TARGET.
 *
 * Both functions below exist only to keep the current host-test build linking.
 * They describe Hub PCB Rev B's 5-slot scheme: one DG2788A per zone slot driven
 * by a dedicated GAIN_SEL[0..4] GPIO on the i.MX RT1062 GPIO2 bank.
 *
 * Under Rev 3 there is NO GAIN_SEL signal between the RT1062 and any socket.
 * TIA gain is a pin on the cluster-controller MCU, set per-SAMPLE inside that
 * controller's PD scan loop from the module type it learned via UID inventory
 * (np_module_map) — not latched per slot from a ZONE_ID resistor ladder. One
 * shared switchable-gain TIA serves all 8 sockets of a cluster, so PD1 and PD2
 * of a socket now pass through the same Rf and the same amplifier (their gain
 * error cancels in the PD1/PD2 ratio — see Rev 3 §6.4).
 *
 * Replacement surface: firmware/cluster_ctrl/ (new IEC 62304 Class B unit) and
 * np_hub_cluster_read_frame() on the hub side. See NP-HW-HUB-001 Rev 3 §6, §9.1.
 */
np_pbm_status_t np_pbm_hal_tia_gain_set(uint8_t slot, np_tia_gain_t gain);
void np_pbm_hal_tia_gain_boot_init(void);

/*
 * T2 subsystem throttle request (OI-PBM-07; stub pending Issue #54).
 * pct: throttle percentage 0–100 (100 = full output, 0 = off).
 * Kept for backwards compatibility; prefer np_pbm_hal_t2_1170_set_duty().
 */
void np_pbm_hal_t2_throttle_request(uint8_t pct);

/*
 * ── T2 1170nm laser HAL (NP-SES-1064-001 §6) ──────────────────────────
 *
 * OI-PBM-07: Enable/disable and power control for the T2 1170nm deep PBM laser
 *             module (Issue #54 — stubs used here until that module ships).
 * OI-PBM-08: Dose and temperature readback from the T2 laser module's monitor PD
 *             and TEC sensor.
 * OI-SES-T2-01: sLORETA MNI target readback from np_fw_sloreta engine
 *                (pending T2 prototype; stubs return DLPFC_L default).
 */

/*
 * Enable or disable the T2 1170nm laser module.
 * Must be called through the safety MCU SPI interface in real implementation;
 * the safety MCU owns the laser enable GPIO and verifies impedance before enable.
 * OI-PBM-07.
 */
np_pbm_status_t np_pbm_hal_t2_1170_enable(bool enable);

/*
 * Set 1170nm laser drive power (0–100%).
 * duty_pct = 0: laser off; 100: full power (≤1000 mW/cm², TEC-limited).
 * Applied over the 30 s ramp in firmware; this function writes the raw setpoint.
 * OI-PBM-07.
 */
np_pbm_status_t np_pbm_hal_t2_1170_set_duty(uint8_t duty_pct);

/*
 * Read accumulated dose from the T2 laser module's integrated monitor PD.
 * dose_J_cm2_out: cumulative dose since last reset (session-scoped).
 * Returns NP_PBM_OK; dose_J_cm2_out unchanged on error.
 * OI-PBM-08.
 */
np_pbm_status_t np_pbm_hal_t2_1170_get_dose(float *dose_J_cm2_out);

/*
 * Read TEC coolant temperature from the T2 laser module.
 * tec_temp_c_out: TEC temperature in °C.
 * Returns NP_PBM_OK; tec_temp_c_out unchanged on error.
 * OI-PBM-08.
 */
np_pbm_status_t np_pbm_hal_t2_1170_get_temp(float *tec_temp_c_out);

/*
 * Read the sLORETA-computed MNI target coordinate for the current session.
 * Supplies the cortical target for the T2 depression protocol coordination.
 * Stubs return NP_T2_SLORETA_STUB_MNI_* (DLPFC_L) with *valid_out = true
 * so that software-passable FAI tests can exercise the logging path.
 * OI-SES-T2-01.
 */
np_pbm_status_t np_pbm_hal_t2_sloreta_get_target(int16_t *mni_x_out,
                                                           int16_t *mni_y_out,
                                                           int16_t *mni_z_out,
                                                           bool    *valid_out);

#ifdef __cplusplus
}
#endif

#endif /* NP_PBM_HAL_H */
