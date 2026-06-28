/*
 * NeuroPulse 1064nm Smart Zone Module — I2C LED Driver API
 * Document: NP-FW-PBM1064-001 Rev A §5
 */

#ifndef NP_PBM1064_DRIVE_H
#define NP_PBM1064_DRIVE_H

#include <stdint.h>
#include <stdbool.h>
#include "np_pbm1064_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Per-slot driver state. */
typedef struct {
    uint8_t cur[3];          /* current setpoints: [0]=CH_A, [1]=CH_B, [2]=CH_C  */
    uint8_t freq_code[3];    /* PWM frequency codes per channel                    */
    uint8_t duty[3];         /* actual duty values written (post-clamp)            */
    uint8_t ch_enable;       /* last written CH_ENABLE register                   */
    uint8_t status;          /* last STATUS read                                  */
    bool    initialized;     /* true after successful startup sequence             */
} np_pbm1064_drv_slot_t;

/*
 * Execute the driver IC startup sequence per NP-FW-PBM1064-001 §5.3.
 * Writes CONFIG → CUR → FREQ → DUTY → CH_ENABLE, then reads STATUS.
 * On STATUS.FAULT=1 returns NP_PBM1064_ERR_DRIVER_FAULT and does NOT
 * call np_pbm1064_hal_safety_mcu_enable().
 */
np_pbm1064_status_t np_pbm1064_drive_startup(uint8_t slot,
                                               np_pbm1064_drv_slot_t *drv,
                                               const np_pbm1064_preset_t *preset);

/*
 * Set duty on one or all channels.  Clamps to NP_PBM1064_DUTY_MAX_REG.
 * ch_mask: bitmask (NP_PBM1064_CH_A_EN etc.); 0xFF = all enabled channels.
 */
np_pbm1064_status_t np_pbm1064_drive_set_duty(uint8_t slot,
                                                np_pbm1064_drv_slot_t *drv,
                                                uint8_t ch_mask,
                                                uint8_t duty);

/*
 * Set PWM frequency code on all enabled channels.
 */
np_pbm1064_status_t np_pbm1064_drive_set_freq(uint8_t slot,
                                                np_pbm1064_drv_slot_t *drv,
                                                uint8_t ch_mask,
                                                uint8_t freq_code);

/*
 * Enable or disable individual channels.
 */
np_pbm1064_status_t np_pbm1064_drive_set_ch_enable(uint8_t slot,
                                                      np_pbm1064_drv_slot_t *drv,
                                                      uint8_t ch_enable_mask);

/*
 * Periodic 5 s status poll.  Reads STATUS, THERMAL, and FAULT_LATCH.
 * On fault: disables affected channels and logs to SHDR.
 * Returns NP_PBM1064_OK if no fault; NP_PBM1064_ERR_DRIVER_FAULT or
 * NP_PBM1064_ERR_THERMAL or NP_PBM1064_ERR_OCP on fault conditions.
 */
np_pbm1064_status_t np_pbm1064_drive_poll_status(uint8_t slot,
                                                    np_pbm1064_drv_slot_t *drv,
                                                    uint32_t device_session_count);

/*
 * Immediately disable all channels (emergency stop).
 * Does not clear FAULT_LATCH — preserves diagnostic info.
 */
np_pbm1064_status_t np_pbm1064_drive_disable_all(uint8_t slot,
                                                    np_pbm1064_drv_slot_t *drv);

/*
 * Map EEG dominant frequency (Hz) to nearest driver PWM frequency code.
 * Applies NP_PBM1064_EEG_HYSTERESIS_TICKS hysteresis via tick_count_in_band.
 * Returns 0xFF if hysteresis period not yet elapsed (no change needed).
 */
uint8_t np_pbm1064_drive_map_eeg_freq(float    dominant_freq_hz,
                                        uint8_t  current_freq_code,
                                        uint8_t *tick_count_in_band);

#ifdef __cplusplus
}
#endif

#endif /* NP_PBM1064_DRIVE_H */
