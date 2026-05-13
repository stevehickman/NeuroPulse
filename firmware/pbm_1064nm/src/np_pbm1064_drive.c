#include "np_pbm1064.h"
#include "np_pbm1064_config.h"
#include <stddef.h>

extern np_err_t np_i2c_write(uint8_t slot, uint8_t addr, uint8_t reg, uint8_t val);
extern np_err_t np_i2c_read(uint8_t slot, uint8_t addr, uint8_t reg, uint8_t *val);

static uint8_t clamp_duty(uint8_t duty_pct)
{
    return (duty_pct > NP_PBM1064_DUTY_PCT_MAX) ? NP_PBM1064_DUTY_PCT_MAX : duty_pct;
}

/* Convert duty_pct to register value: register encodes 0.5%/LSB */
static uint8_t duty_to_reg(uint8_t duty_pct)
{
    uint16_t reg = (uint16_t)duty_pct * 2u;
    return (reg > NP_PBM1064_DUTY_MAX_REG) ? NP_PBM1064_DUTY_MAX_REG : (uint8_t)reg;
}

/* Convert freq_hz to register value: register encodes 0.5 Hz/LSB, 0=CW */
static uint8_t freq_to_reg(uint8_t freq_hz)
{
    if (freq_hz == 0u) return 0x00u;    /* CW */
    uint16_t reg = (uint16_t)freq_hz * 2u;
    return (reg > 0xFFu) ? 0xFFu : (uint8_t)reg;
}

void np_pbm1064_drive_init(uint8_t slot, const np_pbm1064_preset_t *preset)
{
    uint8_t cur_reg;

    /* CONFIG: enable PWM sync and soft start */
    np_i2c_write(slot, NP_PBM1064_I2C_ADDR, NP_PBM1064_REG_CONFIG,
                 NP_PBM1064_CONFIG_PWM_SYNC | NP_PBM1064_CONFIG_SOFT_START);

    /* Current: same for all channels (cur_pct applies uniformly) */
    cur_reg = (uint8_t)((uint32_t)NP_PBM1064_CUR_MA_MIN +
              (uint32_t)(NP_PBM1064_CUR_MA_MAX - NP_PBM1064_CUR_MA_MIN)
              * preset->cur_pct / 100u);
    np_i2c_write(slot, NP_PBM1064_I2C_ADDR, NP_PBM1064_REG_CUR_A, cur_reg);
    np_i2c_write(slot, NP_PBM1064_I2C_ADDR, NP_PBM1064_REG_CUR_B, cur_reg);
    np_i2c_write(slot, NP_PBM1064_I2C_ADDR, NP_PBM1064_REG_CUR_C, cur_reg);

    /* Frequency */
    np_i2c_write(slot, NP_PBM1064_I2C_ADDR, NP_PBM1064_REG_PWM_FREQ_A,
                 freq_to_reg(preset->freq_hz_A));
    np_i2c_write(slot, NP_PBM1064_I2C_ADDR, NP_PBM1064_REG_PWM_FREQ_B,
                 freq_to_reg(preset->freq_hz_B));
    np_i2c_write(slot, NP_PBM1064_I2C_ADDR, NP_PBM1064_REG_PWM_FREQ_C,
                 freq_to_reg(preset->freq_hz_C));

    /* Duty cycle — ceiling enforced unconditionally */
    np_i2c_write(slot, NP_PBM1064_I2C_ADDR, NP_PBM1064_REG_DUTY_A,
                 duty_to_reg(clamp_duty(preset->duty_pct_A)));
    np_i2c_write(slot, NP_PBM1064_I2C_ADDR, NP_PBM1064_REG_DUTY_B,
                 duty_to_reg(clamp_duty(preset->duty_pct_B)));
    np_i2c_write(slot, NP_PBM1064_I2C_ADDR, NP_PBM1064_REG_DUTY_C,
                 duty_to_reg(clamp_duty(preset->duty_pct_C)));
}

void np_pbm1064_set_ch_enable(uint8_t slot, uint8_t ch_mask)
{
    /* Mask to valid channel bits only */
    uint8_t safe_mask = ch_mask & (NP_PBM1064_CH_A | NP_PBM1064_CH_B | NP_PBM1064_CH_C);
    np_i2c_write(slot, NP_PBM1064_I2C_ADDR, NP_PBM1064_REG_CH_ENABLE, safe_mask);
}

void np_pbm1064_set_freq(uint8_t slot, np_pbm_channel_t ch, uint8_t freq_hz)
{
    static const uint8_t freq_regs[] = {
        NP_PBM1064_REG_PWM_FREQ_A,
        NP_PBM1064_REG_PWM_FREQ_B,
        NP_PBM1064_REG_PWM_FREQ_C,
    };
    if ((uint8_t)ch < 3u) {
        np_i2c_write(slot, NP_PBM1064_I2C_ADDR, freq_regs[ch], freq_to_reg(freq_hz));
    }
}

void np_pbm1064_set_duty(uint8_t slot, np_pbm_channel_t ch, uint8_t duty_pct)
{
    static const uint8_t duty_regs[] = {
        NP_PBM1064_REG_DUTY_A,
        NP_PBM1064_REG_DUTY_B,
        NP_PBM1064_REG_DUTY_C,
    };
    if ((uint8_t)ch < 3u) {
        np_i2c_write(slot, NP_PBM1064_I2C_ADDR, duty_regs[ch],
                     duty_to_reg(clamp_duty(duty_pct)));
    }
}

uint8_t np_pbm1064_read_status(uint8_t slot)
{
    uint8_t status = 0xFFu;
    np_i2c_read(slot, NP_PBM1064_I2C_ADDR, NP_PBM1064_REG_STATUS, &status);
    return status;
}

int8_t np_pbm1064_read_ntc(uint8_t slot)
{
    uint8_t raw = 0u;
    np_i2c_read(slot, NP_PBM1064_I2C_ADDR, NP_PBM1064_REG_THERMAL, &raw);
    return (int8_t)raw;
}
