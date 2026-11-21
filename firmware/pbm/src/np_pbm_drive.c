/*
 * NeurOne 1064nm Smart Zone Module — I2C LED Driver Protocol
 * Document: NP-FW-PBM1064-001 Rev 1 §5
 *
 * A duty write above NP_PBM_DUTY_MAX_REG (0x32 = 25%) is REFUSED and writes nothing; it is never clamped
 * (NP-FW-PBM1064-001 §5.5; NP-FW-HUB-001 REQ-FWHUB-78; CLAUDE.md §3).  Interim with the 25 % cap: OI-HEXTILE-31.
 * Startup sequence: CONFIG → CUR → FREQ → DUTY → CH_ENABLE → STATUS check.
 * Periodic STATUS + FAULT_LATCH poll at 5 s intervals handles OCP and thermal.
 */

#include "np_pbm_drive.h"
#include "np_pbm_hal.h"
#include <string.h>

/* ── Internal helpers ───────────────────────────────────────────────────────── */

static np_pbm_status_t write_reg(uint8_t slot, uint8_t reg, uint8_t val)
{
    return np_pbm_hal_i2c_write(slot, reg, &val, 1);
}

static np_pbm_status_t read_reg(uint8_t slot, uint8_t reg, uint8_t *val)
{
    return np_pbm_hal_i2c_read(slot, reg, val, 1);
}

/*
 * Write a pulse frequency (millihertz, little-endian, 0 = CW) to one FREQ register and read it back.  The tile refuses a
 * frequency it cannot realise and keeps the previous value (NP-FW-HEXTILE-001 REQ-FWTILE-15), acknowledging the write
 * either way, so the read-back is the only way to know.  A different value is NP_PBM_ERR_FREQ_REFUSED.
 */
static np_pbm_status_t write_freq(uint8_t slot, uint8_t reg, uint32_t mhz)
{
    uint8_t out[4] = { (uint8_t)mhz, (uint8_t)(mhz >> 8), (uint8_t)(mhz >> 16), (uint8_t)(mhz >> 24) };
    uint8_t back[4] = { 0U, 0U, 0U, 0U };
    np_pbm_status_t rc = np_pbm_hal_i2c_write(slot, reg, out, 4U);
    if (rc != NP_PBM_OK) { return rc; }
    rc = np_pbm_hal_i2c_read(slot, reg, back, 4U);
    if (rc != NP_PBM_OK) { return rc; }
    return (memcmp(out, back, sizeof out) == 0) ? NP_PBM_OK : NP_PBM_ERR_FREQ_REFUSED;
}

/* ── Startup sequence ───────────────────────────────────────────────────────── */

np_pbm_status_t np_pbm_drive_startup(uint8_t slot,
                                               np_pbm_drv_slot_t *drv,
                                               const np_pbm_preset_t *preset)
{
    np_pbm_status_t rc;

    /* Step 1: CONFIG — soft reset to ensure clean register state. */
    rc = write_reg(slot, NP_PBM_REG_CONFIG, NP_PBM_CONFIG_SOFT_RESET |
                                                  NP_PBM_CONFIG_PWM_MODE);
    if (rc != NP_PBM_OK) { return rc; }

    /* Step 2: CUR — set current setpoints per preset. */
    rc = write_reg(slot, NP_PBM_REG_CUR_A, preset->cur_a); if (rc != NP_PBM_OK) { return rc; }
    rc = write_reg(slot, NP_PBM_REG_CUR_B, preset->cur_b); if (rc != NP_PBM_OK) { return rc; }
    rc = write_reg(slot, NP_PBM_REG_CUR_C, preset->cur_c); if (rc != NP_PBM_OK) { return rc; }
    drv->cur[0] = preset->cur_a;
    drv->cur[1] = preset->cur_b;
    drv->cur[2] = preset->cur_c;

    /* Step 3: FREQ — the preset's pulse frequency, millihertz, on every channel. */
    rc = write_freq(slot, NP_PBM_REG_FREQ_A, preset->freq_mhz); if (rc != NP_PBM_OK) { return rc; }
    rc = write_freq(slot, NP_PBM_REG_FREQ_B, preset->freq_mhz); if (rc != NP_PBM_OK) { return rc; }
    rc = write_freq(slot, NP_PBM_REG_FREQ_C, preset->freq_mhz); if (rc != NP_PBM_OK) { return rc; }
    drv->freq_mhz[0] = preset->freq_mhz;
    drv->freq_mhz[1] = preset->freq_mhz;
    drv->freq_mhz[2] = preset->freq_mhz;

    /* Step 4: DUTY — write 0 initially (ramp starts at 0).  The preset's duty is written by
     * np_pbm_drive_set_duty(), which refuses a value above 25%. */
    rc = write_reg(slot, NP_PBM_REG_DUTY_A, 0U); if (rc != NP_PBM_OK) { return rc; }
    rc = write_reg(slot, NP_PBM_REG_DUTY_B, 0U); if (rc != NP_PBM_OK) { return rc; }
    rc = write_reg(slot, NP_PBM_REG_DUTY_C, 0U); if (rc != NP_PBM_OK) { return rc; }
    drv->duty[0] = 0;
    drv->duty[1] = 0;
    drv->duty[2] = 0;

    /* Set thermal fault threshold register. */
    rc = write_reg(slot, NP_PBM_REG_THERMAL,
                   (uint8_t)NP_PBM_THERMAL_FAULT_C);
    if (rc != NP_PBM_OK) { return rc; }

    /* Step 5: CH_ENABLE — enable channels per preset mask. */
    uint8_t ch_en = preset->channel_mask & NP_PBM_CH_ALL_EN;
    rc = write_reg(slot, NP_PBM_REG_CH_ENABLE, ch_en);
    if (rc != NP_PBM_OK) { return rc; }
    drv->ch_enable = ch_en;

    /* Step 6: STATUS check — abort if FAULT bit set. */
    uint8_t status = 0;
    rc = read_reg(slot, NP_PBM_REG_STATUS, &status);
    if (rc != NP_PBM_OK) { return rc; }
    drv->status = status;

    if (status & NP_PBM_STATUS_FAULT) {
        return NP_PBM_ERR_DRIVER_FAULT;
    }

    drv->initialized = true;
    return NP_PBM_OK;
}

/* ── Duty control ───────────────────────────────────────────────────────────── */

np_pbm_status_t np_pbm_drive_set_duty(uint8_t slot,
                                                np_pbm_drv_slot_t *drv,
                                                uint8_t ch_mask,
                                                uint8_t duty)
{
    /* Refused, never clamped: a clamped duty is a pulse train nobody authored.  Before any register is written,
     * so every channel keeps the duty it had. */
    if (duty > NP_PBM_DUTY_MAX_REG) {
        return NP_PBM_ERR_INVALID_ARG;
    }
    np_pbm_status_t rc = NP_PBM_OK;

    if (ch_mask & NP_PBM_CH_A_EN) {
        rc = write_reg(slot, NP_PBM_REG_DUTY_A, duty);
        if (rc != NP_PBM_OK) { return rc; }
        drv->duty[0] = duty;
    }
    if (ch_mask & NP_PBM_CH_B_EN) {
        rc = write_reg(slot, NP_PBM_REG_DUTY_B, duty);
        if (rc != NP_PBM_OK) { return rc; }
        drv->duty[1] = duty;
    }
    if (ch_mask & NP_PBM_CH_C_EN) {
        rc = write_reg(slot, NP_PBM_REG_DUTY_C, duty);
        if (rc != NP_PBM_OK) { return rc; }
        drv->duty[2] = duty;
    }
    return NP_PBM_OK;
}

/* ── Frequency control ──────────────────────────────────────────────────────── */

np_pbm_status_t np_pbm_drive_set_freq(uint8_t slot,
                                                np_pbm_drv_slot_t *drv,
                                                uint8_t ch_mask,
                                                uint32_t freq_mhz)
{
    np_pbm_status_t rc = NP_PBM_OK;

    if (ch_mask & NP_PBM_CH_A_EN) {
        rc = write_freq(slot, NP_PBM_REG_FREQ_A, freq_mhz);
        if (rc != NP_PBM_OK) { return rc; }
        drv->freq_mhz[0] = freq_mhz;
    }
    if (ch_mask & NP_PBM_CH_B_EN) {
        rc = write_freq(slot, NP_PBM_REG_FREQ_B, freq_mhz);
        if (rc != NP_PBM_OK) { return rc; }
        drv->freq_mhz[1] = freq_mhz;
    }
    if (ch_mask & NP_PBM_CH_C_EN) {
        rc = write_freq(slot, NP_PBM_REG_FREQ_C, freq_mhz);
        if (rc != NP_PBM_OK) { return rc; }
        drv->freq_mhz[2] = freq_mhz;
    }
    return NP_PBM_OK;
}

/* ── Channel enable ─────────────────────────────────────────────────────────── */

np_pbm_status_t np_pbm_drive_set_ch_enable(uint8_t slot,
                                                      np_pbm_drv_slot_t *drv,
                                                      uint8_t ch_enable_mask)
{
    uint8_t masked = ch_enable_mask & NP_PBM_CH_ALL_EN;
    np_pbm_status_t rc = write_reg(slot, NP_PBM_REG_CH_ENABLE, masked);
    if (rc == NP_PBM_OK) {
        drv->ch_enable = masked;
    }
    return rc;
}

/* ── Periodic status poll ───────────────────────────────────────────────────── */

np_pbm_status_t np_pbm_drive_poll_status(uint8_t slot,
                                                    np_pbm_drv_slot_t *drv,
                                                    uint32_t device_session_count)
{
    uint8_t status = 0;
    uint8_t fault_latch = 0;

    np_pbm_status_t rc = read_reg(slot, NP_PBM_REG_STATUS, &status);
    if (rc != NP_PBM_OK) { return rc; }

    rc = read_reg(slot, NP_PBM_REG_FAULT_LATCH, &fault_latch);
    if (rc != NP_PBM_OK) { return rc; }

    drv->status = status;

    if (!(status & NP_PBM_STATUS_FAULT)) {
        return NP_PBM_OK;
    }

    /* Fault detected — log to SHDR and disable affected channels. */
    np_pbm_fault_t reason = NP_PBM_FAULT_NONE;
    uint8_t disable_mask = 0;

    if (status & NP_PBM_STATUS_THERMAL) {
        reason       = NP_PBM_FAULT_THERMAL;
        disable_mask = NP_PBM_CH_ALL_EN;
    }
    if (status & NP_PBM_STATUS_OCP_C) {
        reason       = NP_PBM_FAULT_OCP_C;
        disable_mask |= NP_PBM_CH_C_EN;
    }
    if (status & NP_PBM_STATUS_OCP_B) {
        reason       = (reason == NP_PBM_FAULT_NONE) ? NP_PBM_FAULT_OCP_B : reason;
        disable_mask |= NP_PBM_CH_B_EN;
    }
    if (status & NP_PBM_STATUS_OCP_A) {
        reason       = (reason == NP_PBM_FAULT_NONE) ? NP_PBM_FAULT_OCP_A : reason;
        disable_mask |= NP_PBM_CH_A_EN;
    }

    /* Disable affected channels. */
    uint8_t new_en = drv->ch_enable & ~disable_mask;
    write_reg(slot, NP_PBM_REG_CH_ENABLE, new_en);
    drv->ch_enable = new_en;

    /* Clear fault latch. */
    write_reg(slot, NP_PBM_REG_FAULT_LATCH, 0xFF);

    /* Log to SHDR. */
    np_pbm_shdr_fault_entry_t fe = {
        .device_session_count = device_session_count,
        .socket_id            = slot,
        .channel              = (disable_mask == NP_PBM_CH_ALL_EN) ? 0xFF :
                                (disable_mask & NP_PBM_CH_C_EN) ? 2U :
                                (disable_mask & NP_PBM_CH_B_EN) ? 1U : 0U,
        .fault_reason         = (uint8_t)reason,
        .status_reg_value     = status,
    };
    np_pbm_hal_shdr_log_fault(&fe);

    if (reason == NP_PBM_FAULT_THERMAL) {
        return NP_PBM_ERR_THERMAL;
    }
    return NP_PBM_ERR_OCP;
}

/* ── Emergency stop ─────────────────────────────────────────────────────────── */

np_pbm_status_t np_pbm_drive_disable_all(uint8_t slot,
                                                    np_pbm_drv_slot_t *drv)
{
    write_reg(slot, NP_PBM_REG_DUTY_A,    0U);
    write_reg(slot, NP_PBM_REG_DUTY_B,    0U);
    write_reg(slot, NP_PBM_REG_DUTY_C,    0U);
    np_pbm_status_t rc = write_reg(slot, NP_PBM_REG_CH_ENABLE, 0U);
    drv->ch_enable  = 0;
    drv->duty[0]    = 0;
    drv->duty[1]    = 0;
    drv->duty[2]    = 0;
    return rc;
}

/* ── EEG-adaptive frequency mapping ─────────────────────────────────────────── */

uint32_t np_pbm_drive_map_eeg_freq(float     dominant_freq_hz,
                                         uint32_t  current_freq_mhz,
                                         uint8_t  *ticks_off_code)
{
    /* The target is the dominant frequency itself.  No EEG band and no code table plays a part. */
    if (!(dominant_freq_hz > 0.0f) || dominant_freq_hz > 4294967.0f) {
        *ticks_off_code = 0;
        return NP_PBM_FREQ_NO_CHANGE;       /* not a frequency */
    }
    const uint32_t target_mhz = (uint32_t)(dominant_freq_hz * 1000.0f + 0.5f);
    if (target_mhz == 0U || target_mhz == current_freq_mhz) {
        *ticks_off_code = 0;
        return NP_PBM_FREQ_NO_CHANGE;       /* no change */
    }

    (*ticks_off_code)++;
    if (*ticks_off_code < NP_PBM_EEG_HYSTERESIS_TICKS) {
        return NP_PBM_FREQ_NO_CHANGE;       /* hysteresis — not yet */
    }

    *ticks_off_code = 0;
    return target_mhz;
}
