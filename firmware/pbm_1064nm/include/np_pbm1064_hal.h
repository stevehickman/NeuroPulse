/*
 * NeuroPulse 1064nm Smart Zone Module — Platform HAL Stubs
 * Document: NP-FW-PBM1064-001 Rev A §3
 *
 * These functions are implemented as stubs in np_pbm1064_hal.c and must be
 * replaced by the platform team with real peripheral driver implementations:
 *
 *   OI-PBM-01: np_pbm1064_hal_adc_read_pd  — LPADC1 for PD1/PD2 per zone
 *   OI-PBM-02: np_pbm1064_hal_i2c_write / _read — LPI2C3 slot-addressed
 *   OI-PBM-03: np_pbm1064_hal_safety_mcu_enable — SPI to STM32G071
 *
 * Stub implementations return NP_PBM1064_OK and leave output parameters at
 * deterministic test values so that software-passable FAI tests can run.
 */

#ifndef NP_PBM1064_HAL_H
#define NP_PBM1064_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "np_pbm1064_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Read ZONE_ID ADC for slot.
 * slot: 0–4 (ZM-01 through ZM-05 positions).
 * counts_out: 12-bit ADC result [0..4095].
 * Returns true on success.
 */
bool np_pbm1064_hal_adc_read_zone_id(uint8_t slot, uint16_t *counts_out);

/*
 * Read PD1 or PD2 photodiode ADC for slot.
 * slot:   0–4.
 * pd_ch:  0 = PD1 (forward emission), 1 = PD2 (scalp-side backscatter).
 * counts_out: 12-bit ADC result.
 * Returns true on success.  OI-PBM-01.
 */
bool np_pbm1064_hal_adc_read_pd(uint8_t slot, uint8_t pd_ch,
                                 uint16_t *counts_out);

/*
 * Enable or disable the LPI2C3 GPIO mux for the specified slot.
 * Must be called before any I2C transaction to that slot.
 * OI-PBM-02.
 */
np_pbm1064_status_t np_pbm1064_hal_i2c_mux_enable(uint8_t slot, bool enable);

/*
 * Write len bytes to driver IC register reg_addr at 7-bit address
 * NP_PBM1064_I2C_ADDR on slot's LPI2C3 instance.
 * Returns NP_PBM1064_OK or NP_PBM1064_ERR_I2C_WRITE.  OI-PBM-02.
 */
np_pbm1064_status_t np_pbm1064_hal_i2c_write(uint8_t  slot,
                                               uint8_t  reg_addr,
                                               const uint8_t *data,
                                               uint8_t  len);

/*
 * Read len bytes from driver IC register reg_addr.
 * Returns NP_PBM1064_OK or NP_PBM1064_ERR_I2C_READ.  OI-PBM-02.
 */
np_pbm1064_status_t np_pbm1064_hal_i2c_read(uint8_t  slot,
                                              uint8_t  reg_addr,
                                              uint8_t *data,
                                              uint8_t  len);

/*
 * Probe I2C: send address 0x30 and check for ACK within timeout_ms.
 * Returns true if ACK received.  OI-PBM-02.
 */
bool np_pbm1064_hal_i2c_probe(uint8_t slot, uint8_t i2c_addr,
                                uint32_t timeout_ms);

/*
 * Assert or deassert the safety MCU GPIO enable line for smart module slot.
 * safety MCU owns all stimulation GPIO — main processor requests enable via SPI.
 * Returns NP_PBM1064_OK or NP_PBM1064_ERR_SAFETY_REJECTED.  OI-PBM-03.
 */
np_pbm1064_status_t np_pbm1064_hal_safety_mcu_enable(uint8_t slot,
                                                       bool    enable);

/*
 * Read NTC temperature (°C) for the specified zone slot.
 * Returns NP_PBM1064_OK on success; temp_c_out unchanged on error.
 */
np_pbm1064_status_t np_pbm1064_hal_ntc_read(uint8_t slot, float *temp_c_out);

/*
 * Return system monotonic time in milliseconds (wraps at UINT32_MAX).
 * Maps to FreeRTOS xTaskGetTickCount() × portTICK_PERIOD_MS.
 */
uint32_t np_pbm1064_hal_now_ms(void);

/*
 * Write np_pbm1064_shdr_fault_entry_t to SHDR partition.
 * Caller provides pointer to populated entry; function appends to SHDR log.
 * No-op in stub implementation.
 */
void np_pbm1064_hal_shdr_log_fault(const np_pbm1064_shdr_fault_entry_t *entry);

/*
 * Invoke bone conduction zone announcement (delegate to NP-FW-ZA-001).
 * slot_index: 0–4; used to compute zone ID for the announcer.
 */
void np_pbm1064_hal_zone_announce(uint8_t slot_index);

/*
 * T2 subsystem throttle request (OI-PBM-07; stub pending Issue #54).
 * pct: throttle percentage 0–100 (100 = full output, 0 = off).
 */
void np_pbm1064_hal_t2_throttle_request(uint8_t pct);

#ifdef __cplusplus
}
#endif

#endif /* NP_PBM1064_HAL_H */
