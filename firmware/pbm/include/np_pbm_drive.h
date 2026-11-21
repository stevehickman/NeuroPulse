/*
 * NeurOne 1064nm Smart Zone Module — I2C LED Driver API
 * Document: NP-FW-PBM1064-001 Rev 1 §5
 */

#ifndef NP_PBM_DRIVE_H
#define NP_PBM_DRIVE_H

#include <stdint.h>
#include <stdbool.h>
#include "np_pbm_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Per-slot driver state. */
typedef struct {
    uint8_t cur[3];          /* current setpoints: [0]=CH_A, [1]=CH_B, [2]=CH_C  */
    uint32_t freq_mhz[3];    /* pulse frequency per channel, millihertz (0 = CW)  */
    uint8_t duty[3];         /* duty values written (as commanded; never clamped)  */
    uint8_t ch_enable;       /* last written CH_ENABLE register                   */
    uint8_t status;          /* last STATUS read                                  */
    bool    initialized;     /* true after successful startup sequence             */
} np_pbm_drv_slot_t;

/*
 * Execute the driver IC startup sequence per NP-FW-PBM1064-001 §5.3.
 * Writes CONFIG → CUR → FREQ → DUTY → CH_ENABLE, then reads STATUS.
 * On STATUS.FAULT=1 returns NP_PBM_ERR_DRIVER_FAULT and does NOT
 * call np_pbm_hal_safety_mcu_enable().
 */
np_pbm_status_t np_pbm_drive_startup(uint8_t slot,
                                               np_pbm_drv_slot_t *drv,
                                               const np_pbm_preset_t *preset);

/*
 * Set duty on one or all channels.  A duty above NP_PBM_DUTY_MAX_REG is refused
 * (NP_PBM_ERR_INVALID_ARG) and nothing is written; it is never clamped.
 * ch_mask: bitmask (NP_PBM_CH_A_EN etc.); 0xFF = all enabled channels.
 */
np_pbm_status_t np_pbm_drive_set_duty(uint8_t slot,
                                                np_pbm_drv_slot_t *drv,
                                                uint8_t ch_mask,
                                                uint8_t duty);

/*
 * Set the pulse frequency (millihertz, 0 = CW) on all enabled channels, then read each FREQ register back.  A tile that
 * did not keep the value (it refuses a frequency it cannot realise, and keeps the previous one) is reported as
 * NP_PBM_ERR_FREQ_REFUSED, so the command fails instead of running at another frequency (OI-PBMTILE-01).
 */
np_pbm_status_t np_pbm_drive_set_freq(uint8_t slot,
                                                np_pbm_drv_slot_t *drv,
                                                uint8_t ch_mask,
                                                uint32_t freq_mhz);

/*
 * Enable or disable individual channels.
 */
np_pbm_status_t np_pbm_drive_set_ch_enable(uint8_t slot,
                                                      np_pbm_drv_slot_t *drv,
                                                      uint8_t ch_enable_mask);

/*
 * Periodic 5 s status poll.  Reads STATUS, THERMAL, and FAULT_LATCH.
 * On fault: disables affected channels and logs to SHDR.
 * Returns NP_PBM_OK if no fault; NP_PBM_ERR_DRIVER_FAULT or
 * NP_PBM_ERR_THERMAL or NP_PBM_ERR_OCP on fault conditions.
 */
np_pbm_status_t np_pbm_drive_poll_status(uint8_t slot,
                                                    np_pbm_drv_slot_t *drv,
                                                    uint32_t device_session_count);

/*
 * Immediately disable all channels (emergency stop).
 * Does not clear FAULT_LATCH — preserves diagnostic info.
 */
np_pbm_status_t np_pbm_drive_disable_all(uint8_t slot,
                                                    np_pbm_drv_slot_t *drv);

/*
 * The EEG-adaptive pulse frequency: the dominant frequency itself, in millihertz (no EEG band picks it, and no code table
 * does).  Applies NP_PBM_EEG_HYSTERESIS_TICKS hysteresis via ticks_off_code, the count of consecutive ticks the dominant
 * frequency has differed from the running one.
 * Returns the new frequency in millihertz, or NP_PBM_FREQ_NO_CHANGE if the hysteresis has not yet elapsed, the
 * frequency is the running one, or it is not a frequency (zero, negative, not a number, or past 32 bits of millihertz).
 */
uint32_t np_pbm_drive_map_eeg_freq(float     dominant_freq_hz,
                                         uint32_t  current_freq_mhz,
                                         uint8_t  *ticks_off_code);

#ifdef __cplusplus
}
#endif

#endif /* NP_PBM_DRIVE_H */
