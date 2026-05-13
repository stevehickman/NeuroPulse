/*
 * NeuroPulse 1064nm Smart Zone Module — I2C LED Driver Protocol
 * Document: NP-FW-PBM1064-001 Rev A §5
 *
 * All duty register writes clamp to NP_PBM1064_DUTY_MAX_REG (0x32 = 25%).
 * Startup sequence: CONFIG → CUR → FREQ → DUTY → CH_ENABLE → STATUS check.
 * Periodic STATUS + FAULT_LATCH poll at 5 s intervals handles OCP and thermal.
 */

#include "np_pbm1064_drive.h"
#include "np_pbm1064_hal.h"
#include <string.h>

/* ── Internal helpers ───────────────────────────────────────────────────────── */

static uint8_t clamp_duty(uint8_t requested)
{
    return (requested <= NP_PBM1064_DUTY_MAX_REG) ?
           requested : NP_PBM1064_DUTY_MAX_REG;
}

static np_pbm1064_status_t write_reg(uint8_t slot, uint8_t reg, uint8_t val)
{
    return np_pbm1064_hal_i2c_write(slot, reg, &val, 1);
}

static np_pbm1064_status_t read_reg(uint8_t slot, uint8_t reg, uint8_t *val)
{
    return np_pbm1064_hal_i2c_read(slot, reg, val, 1);
}

/*
 * Map freq_hz preset field to nearest driver PWM frequency code.
 * 0 → CW, 2→0x01, 6→0x06, 10→0x0A, 20→0x14, 40→0x28.
 */
static uint8_t map_freq_hz_to_code(uint8_t freq_hz)
{
    if (freq_hz == 0U)  { return NP_PBM1064_FREQ_CODE_CW;   }
    if (freq_hz <= 3U)  { return NP_PBM1064_FREQ_CODE_2HZ;  }
    if (freq_hz <= 8U)  { return NP_PBM1064_FREQ_CODE_6HZ;  }
    if (freq_hz <= 15U) { return NP_PBM1064_FREQ_CODE_10HZ; }
    if (freq_hz <= 30U) { return NP_PBM1064_FREQ_CODE_20HZ; }
    return NP_PBM1064_FREQ_CODE_40HZ;
}

/* ── Startup sequence ───────────────────────────────────────────────────────── */

np_pbm1064_status_t np_pbm1064_drive_startup(uint8_t slot,
                                               np_pbm1064_drv_slot_t *drv,
                                               const np_pbm1064_preset_t *preset)
{
    np_pbm1064_status_t rc;

    /* Step 1: CONFIG — soft reset to ensure clean register state. */
    rc = write_reg(slot, NP_PBM1064_REG_CONFIG, NP_PBM1064_CONFIG_SOFT_RESET |
                                                  NP_PBM1064_CONFIG_PWM_MODE);
    if (rc != NP_PBM1064_OK) { return rc; }

    /* Step 2: CUR — set current setpoints per preset. */
    rc = write_reg(slot, NP_PBM1064_REG_CUR_A, preset->cur_a); if (rc != NP_PBM1064_OK) { return rc; }
    rc = write_reg(slot, NP_PBM1064_REG_CUR_B, preset->cur_b); if (rc != NP_PBM1064_OK) { return rc; }
    rc = write_reg(slot, NP_PBM1064_REG_CUR_C, preset->cur_c); if (rc != NP_PBM1064_OK) { return rc; }
    drv->cur[0] = preset->cur_a;
    drv->cur[1] = preset->cur_b;
    drv->cur[2] = preset->cur_c;

    /* Step 3: FREQ — map freq_hz preset to driver codes. */
    uint8_t fcode = map_freq_hz_to_code(preset->freq_hz);
    rc = write_reg(slot, NP_PBM1064_REG_PWM_FREQ_A, fcode); if (rc != NP_PBM1064_OK) { return rc; }
    rc = write_reg(slot, NP_PBM1064_REG_PWM_FREQ_B, fcode); if (rc != NP_PBM1064_OK) { return rc; }
    rc = write_reg(slot, NP_PBM1064_REG_PWM_FREQ_C, fcode); if (rc != NP_PBM1064_OK) { return rc; }
    drv->freq_code[0] = fcode;
    drv->freq_code[1] = fcode;
    drv->freq_code[2] = fcode;

    /* Step 4: DUTY — clamped to 25%. Write 0 initially (ramp starts at 0). */
    rc = write_reg(slot, NP_PBM1064_REG_DUTY_A, 0U); if (rc != NP_PBM1064_OK) { return rc; }
    rc = write_reg(slot, NP_PBM1064_REG_DUTY_B, 0U); if (rc != NP_PBM1064_OK) { return rc; }
    rc = write_reg(slot, NP_PBM1064_REG_DUTY_C, 0U); if (rc != NP_PBM1064_OK) { return rc; }
    drv->duty[0] = 0;
    drv->duty[1] = 0;
    drv->duty[2] = 0;

    /* Set thermal fault threshold register. */
    rc = write_reg(slot, NP_PBM1064_REG_THERMAL,
                   (uint8_t)NP_PBM1064_THERMAL_FAULT_C);
    if (rc != NP_PBM1064_OK) { return rc; }

    /* Step 5: CH_ENABLE — enable channels per preset mask. */
    uint8_t ch_en = preset->channel_mask & NP_PBM1064_CH_ALL_EN;
    rc = write_reg(slot, NP_PBM1064_REG_CH_ENABLE, ch_en);
    if (rc != NP_PBM1064_OK) { return rc; }
    drv->ch_enable = ch_en;

    /* Step 6: STATUS check — abort if FAULT bit set. */
    uint8_t status = 0;
    rc = read_reg(slot, NP_PBM1064_REG_STATUS, &status);
    if (rc != NP_PBM1064_OK) { return rc; }
    drv->status = status;

    if (status & NP_PBM1064_STATUS_FAULT) {
        return NP_PBM1064_ERR_DRIVER_FAULT;
    }

    drv->initialised = true;
    return NP_PBM1064_OK;
}

/* ── Duty control ───────────────────────────────────────────────────────────── */

np_pbm1064_status_t np_pbm1064_drive_set_duty(uint8_t slot,
                                                np_pbm1064_drv_slot_t *drv,
                                                uint8_t ch_mask,
                                                uint8_t duty)
{
    uint8_t clamped = clamp_duty(duty);
    np_pbm1064_status_t rc = NP_PBM1064_OK;

    if (ch_mask & NP_PBM1064_CH_A_EN) {
        rc = write_reg(slot, NP_PBM1064_REG_DUTY_A, clamped);
        if (rc != NP_PBM1064_OK) { return rc; }
        drv->duty[0] = clamped;
    }
    if (ch_mask & NP_PBM1064_CH_B_EN) {
        rc = write_reg(slot, NP_PBM1064_REG_DUTY_B, clamped);
        if (rc != NP_PBM1064_OK) { return rc; }
        drv->duty[1] = clamped;
    }
    if (ch_mask & NP_PBM1064_CH_C_EN) {
        rc = write_reg(slot, NP_PBM1064_REG_DUTY_C, clamped);
        if (rc != NP_PBM1064_OK) { return rc; }
        drv->duty[2] = clamped;
    }
    return NP_PBM1064_OK;
}

/* ── Frequency control ──────────────────────────────────────────────────────── */

np_pbm1064_status_t np_pbm1064_drive_set_freq(uint8_t slot,
                                                np_pbm1064_drv_slot_t *drv,
                                                uint8_t ch_mask,
                                                uint8_t freq_code)
{
    np_pbm1064_status_t rc = NP_PBM1064_OK;

    if (ch_mask & NP_PBM1064_CH_A_EN) {
        rc = write_reg(slot, NP_PBM1064_REG_PWM_FREQ_A, freq_code);
        if (rc != NP_PBM1064_OK) { return rc; }
        drv->freq_code[0] = freq_code;
    }
    if (ch_mask & NP_PBM1064_CH_B_EN) {
        rc = write_reg(slot, NP_PBM1064_REG_PWM_FREQ_B, freq_code);
        if (rc != NP_PBM1064_OK) { return rc; }
        drv->freq_code[1] = freq_code;
    }
    if (ch_mask & NP_PBM1064_CH_C_EN) {
        rc = write_reg(slot, NP_PBM1064_REG_PWM_FREQ_C, freq_code);
        if (rc != NP_PBM1064_OK) { return rc; }
        drv->freq_code[2] = freq_code;
    }
    return NP_PBM1064_OK;
}

/* ── Channel enable ─────────────────────────────────────────────────────────── */

np_pbm1064_status_t np_pbm1064_drive_set_ch_enable(uint8_t slot,
                                                      np_pbm1064_drv_slot_t *drv,
                                                      uint8_t ch_enable_mask)
{
    uint8_t masked = ch_enable_mask & NP_PBM1064_CH_ALL_EN;
    np_pbm1064_status_t rc = write_reg(slot, NP_PBM1064_REG_CH_ENABLE, masked);
    if (rc == NP_PBM1064_OK) {
        drv->ch_enable = masked;
    }
    return rc;
}

/* ── Periodic status poll ───────────────────────────────────────────────────── */

np_pbm1064_status_t np_pbm1064_drive_poll_status(uint8_t slot,
                                                    np_pbm1064_drv_slot_t *drv,
                                                    uint32_t device_session_count)
{
    uint8_t status = 0;
    uint8_t fault_latch = 0;

    np_pbm1064_status_t rc = read_reg(slot, NP_PBM1064_REG_STATUS, &status);
    if (rc != NP_PBM1064_OK) { return rc; }

    rc = read_reg(slot, NP_PBM1064_REG_FAULT_LATCH, &fault_latch);
    if (rc != NP_PBM1064_OK) { return rc; }

    drv->status = status;

    if (!(status & NP_PBM1064_STATUS_FAULT)) {
        return NP_PBM1064_OK;
    }

    /* Fault detected — log to SHDR and disable affected channels. */
    np_pbm1064_fault_t reason = NP_PBM1064_FAULT_NONE;
    uint8_t disable_mask = 0;

    if (status & NP_PBM1064_STATUS_THERMAL) {
        reason       = NP_PBM1064_FAULT_THERMAL;
        disable_mask = NP_PBM1064_CH_ALL_EN;
    }
    if (status & NP_PBM1064_STATUS_OCP_C) {
        reason       = NP_PBM1064_FAULT_OCP_C;
        disable_mask |= NP_PBM1064_CH_C_EN;
    }
    if (status & NP_PBM1064_STATUS_OCP_B) {
        reason       = (reason == NP_PBM1064_FAULT_NONE) ? NP_PBM1064_FAULT_OCP_B : reason;
        disable_mask |= NP_PBM1064_CH_B_EN;
    }
    if (status & NP_PBM1064_STATUS_OCP_A) {
        reason       = (reason == NP_PBM1064_FAULT_NONE) ? NP_PBM1064_FAULT_OCP_A : reason;
        disable_mask |= NP_PBM1064_CH_A_EN;
    }

    /* Disable affected channels. */
    uint8_t new_en = drv->ch_enable & ~disable_mask;
    write_reg(slot, NP_PBM1064_REG_CH_ENABLE, new_en);
    drv->ch_enable = new_en;

    /* Clear fault latch. */
    write_reg(slot, NP_PBM1064_REG_FAULT_LATCH, 0xFF);

    /* Log to SHDR. */
    np_pbm1064_shdr_fault_entry_t fe = {
        .device_session_count = device_session_count,
        .slot                 = slot,
        .channel              = (disable_mask == NP_PBM1064_CH_ALL_EN) ? 0xFF :
                                (disable_mask & NP_PBM1064_CH_C_EN) ? 2U :
                                (disable_mask & NP_PBM1064_CH_B_EN) ? 1U : 0U,
        .fault_reason         = (uint8_t)reason,
        .status_reg_value     = status,
    };
    np_pbm1064_hal_shdr_log_fault(&fe);

    if (reason == NP_PBM1064_FAULT_THERMAL) {
        return NP_PBM1064_ERR_THERMAL;
    }
    return NP_PBM1064_ERR_OCP;
}

/* ── Emergency stop ─────────────────────────────────────────────────────────── */

np_pbm1064_status_t np_pbm1064_drive_disable_all(uint8_t slot,
                                                    np_pbm1064_drv_slot_t *drv)
{
    write_reg(slot, NP_PBM1064_REG_DUTY_A,    0U);
    write_reg(slot, NP_PBM1064_REG_DUTY_B,    0U);
    write_reg(slot, NP_PBM1064_REG_DUTY_C,    0U);
    np_pbm1064_status_t rc = write_reg(slot, NP_PBM1064_REG_CH_ENABLE, 0U);
    drv->ch_enable  = 0;
    drv->duty[0]    = 0;
    drv->duty[1]    = 0;
    drv->duty[2]    = 0;
    return rc;
}

/* ── EEG-adaptive frequency mapping ─────────────────────────────────────────── */

uint8_t np_pbm1064_drive_map_eeg_freq(float    dominant_freq_hz,
                                        uint8_t  current_freq_code,
                                        uint8_t *tick_count_in_band)
{
    /* Classify dominant frequency into EEG band. */
    uint8_t target_code;
    if (dominant_freq_hz < 4.0f)       { target_code = NP_PBM1064_FREQ_CODE_2HZ;  }
    else if (dominant_freq_hz < 8.0f)  { target_code = NP_PBM1064_FREQ_CODE_6HZ;  }
    else if (dominant_freq_hz < 12.0f) { target_code = NP_PBM1064_FREQ_CODE_10HZ; }
    else if (dominant_freq_hz < 30.0f) { target_code = NP_PBM1064_FREQ_CODE_20HZ; }
    else                               { target_code = NP_PBM1064_FREQ_CODE_40HZ; }

    if (target_code == current_freq_code) {
        *tick_count_in_band = 0;
        return 0xFF; /* no change */
    }

    (*tick_count_in_band)++;
    if (*tick_count_in_band < NP_PBM1064_EEG_HYSTERESIS_TICKS) {
        return 0xFF; /* hysteresis — not yet */
    }

    *tick_count_in_band = 0;
    return target_code;
}
