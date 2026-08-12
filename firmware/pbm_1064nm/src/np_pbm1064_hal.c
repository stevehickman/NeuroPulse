/*
 * NeurOne 1064nm Smart Zone Module — Platform HAL Stub Implementations
 * Document: NP-FW-PBM1064-001 Rev 1 §3
 *
 * OI-PBM-01: np_pbm1064_hal_adc_read_pd    — STUB (platform team: wire to LPADC1)
 * OI-PBM-02: np_pbm1064_hal_i2c_write/_read — STUB (platform team: wire to LPI2C3)
 * OI-PBM-03: np_pbm1064_hal_safety_mcu_enable — STUB (platform team: wire to SPI/GPIO)
 *
 * Stub ADC returns deterministic values that allow software-passable FAI tests
 * to exercise the full detection and dose metering code paths:
 *   - np_pbm1064_hal_adc_read_zone_id: returns 1016 (smart module, slot 0) or
 *     ADC values matching ZM-01–05 for slots 1–5 (base modules).
 *   - np_pbm1064_hal_adc_read_pd: returns nominal PD counts for 100 mW/cm².
 *   - np_pbm1064_hal_i2c_write/_read: returns NP_PBM1064_OK (no-op).
 *   - np_pbm1064_hal_i2c_probe: returns true (ACK simulated).
 *   - np_pbm1064_hal_safety_mcu_enable: returns NP_PBM1064_OK.
 *
 * Replace this file entirely with real peripheral driver code before hardware
 * bring-up.  Do NOT call these stubs in production firmware.
 */

#include "np_pbm1064_hal.h"
#include <string.h>
#include <stddef.h>

/* ── Stub ADC values (deterministic, used by FAI tests) ─────────────────────── */

/*
 * Slot 0 returns smart module ADC (~1016 counts for 3.3 kΩ).
 * Slots 1–4 return base module values matching ZM-01–05 ladder.
 */
static const uint16_t k_stub_zone_id_adc[5] = {
    1016U,  /* slot 0 → smart module (3.3 kΩ)   */
    2048U,  /* slot 1 → ZM-01 (10 kΩ)           */
    2818U,  /* slot 2 → ZM-02 (22 kΩ)           */
    3378U,  /* slot 3 → ZM-03 (47 kΩ)           */
    3723U,  /* slot 4 → ZM-04 (100 kΩ)          */
};

/*
 * PD1 and PD2 stub counts calibrated to produce ~100 mW/cm² irradiance
 * given default K_PD1 = 0.088 (1064nm).
 * PD1 = 100 / 0.088 ≈ 1136; PD2 = PD1 / K_ratio_nom = 1136 / 1.222 ≈ 929.
 */
static const uint16_t k_stub_pd_counts[2] = { 1136U, 929U };

/* Monotonic ms counter (incremented by 100 ms per call for FAI determinism). */
static uint32_t s_stub_now_ms = 0U;

/* ── I2C register shadow (per slot; 14 registers 0x00–0x0D) ─────────────────── */

static uint8_t s_i2c_regs[5][14];

/* ── Public stub implementations ─────────────────────────────────────────────── */

bool np_pbm1064_hal_adc_read_zone_id(uint8_t slot, uint16_t *counts_out)
{
    if (!counts_out || slot >= 5U) { return false; }
    *counts_out = k_stub_zone_id_adc[slot];
    return true;
}

bool np_pbm1064_hal_adc_read_pd(uint8_t slot, uint8_t pd_ch,
                                  uint16_t *counts_out)
{
    if (!counts_out || slot >= 5U || pd_ch > 1U) { return false; }
    *counts_out = k_stub_pd_counts[pd_ch];
    return true;
}

np_pbm1064_status_t np_pbm1064_hal_i2c_mux_enable(uint8_t slot, bool enable)
{
    (void)slot;
    (void)enable;
    /* Stub: mux GPIO toggled by platform driver. */
    return NP_PBM1064_OK;
}

np_pbm1064_status_t np_pbm1064_hal_i2c_write(uint8_t        slot,
                                               uint8_t        reg_addr,
                                               const uint8_t *data,
                                               uint8_t        len)
{
    if (slot >= 5U || !data || reg_addr + len > 14U) {
        return NP_PBM1064_ERR_I2C_WRITE;
    }
    for (uint8_t i = 0; i < len; i++) {
        s_i2c_regs[slot][reg_addr + i] = data[i];
    }
    return NP_PBM1064_OK;
}

np_pbm1064_status_t np_pbm1064_hal_i2c_read(uint8_t  slot,
                                              uint8_t  reg_addr,
                                              uint8_t *data,
                                              uint8_t  len)
{
    if (slot >= 5U || !data || reg_addr + len > 14U) {
        return NP_PBM1064_ERR_I2C_READ;
    }
    for (uint8_t i = 0; i < len; i++) {
        data[i] = s_i2c_regs[slot][reg_addr + i];
    }
    return NP_PBM1064_OK;
}

bool np_pbm1064_hal_i2c_probe(uint8_t slot, uint8_t i2c_addr,
                                uint32_t timeout_ms)
{
    (void)timeout_ms;
    /* Stub: ACK for slot 0 (smart module), NAK for others. */
    return (slot == 0U && i2c_addr == NP_PBM1064_I2C_ADDR);
}

np_pbm1064_status_t np_pbm1064_hal_safety_mcu_enable(uint8_t slot,
                                                       bool    enable)
{
    (void)slot;
    (void)enable;
    /* Stub: SPI transaction to STM32G071 safety MCU pending OI-PBM-03. */
    return NP_PBM1064_OK;
}

np_pbm1064_status_t np_pbm1064_hal_ntc_read(uint8_t slot, float *temp_c_out)
{
    (void)slot;
    if (!temp_c_out) { return NP_PBM1064_ERR_INVALID_ARG; }
    *temp_c_out = 37.0f; /* room temperature + slight scalp warming */
    return NP_PBM1064_OK;
}

uint32_t np_pbm1064_hal_now_ms(void)
{
    s_stub_now_ms += NP_PBM1064_DOSE_TICK_MS;
    return s_stub_now_ms;
}

void np_pbm1064_hal_shdr_log_fault(const np_pbm1064_shdr_fault_entry_t *entry)
{
    /* Stub: real HAL writes to SHDR LittleFS file on eMMC. */
    (void)entry;
}

void np_pbm1064_hal_zone_announce(uint8_t slot_index)
{
    /* Stub: real HAL delegates to np_za_insert_cb (NP-FW-ZA-001). */
    (void)slot_index;
}

void np_pbm1064_hal_t2_throttle_request(uint8_t pct)
{
    /* Stub: T2 1170nm laser throttle (Issue #54 — OI-PBM-07). */
    (void)pct;
}

/* ── T2 1170nm laser HAL stubs (NP-SES-1064-001 §6) ───────────────────── */

/* Stub state for FAI test determinism. */
static bool    s_stub_t2_1170_enabled  = false;
static uint8_t s_stub_t2_1170_duty_pct = 0U;
static float   s_stub_t2_1170_dose_J   = 0.0f;  /* accumulates per set_duty call  */
static float   s_stub_t2_1170_tec_temp = 38.0f; /* nominal TEC temp (healthy)     */

np_pbm1064_status_t np_pbm1064_hal_t2_1170_enable(bool enable)
{
    /*
     * Real implementation: SPI request to safety MCU → safety MCU verifies
     * impedance + checks cardiac interlock (cervical VNS path) before asserting
     * laser enable GPIO.  Stub: tracks state for FAI test assertions.
     */
    s_stub_t2_1170_enabled = enable;
    if (!enable) {
        s_stub_t2_1170_duty_pct = 0U;
        s_stub_t2_1170_dose_J   = 0.0f;
    }
    return NP_PBM1064_OK;
}

np_pbm1064_status_t np_pbm1064_hal_t2_1170_set_duty(uint8_t duty_pct)
{
    /* Real implementation: I2C/SPI write to T2 laser module controller. */
    if (duty_pct > 100U) { duty_pct = 100U; }
    s_stub_t2_1170_duty_pct = duty_pct;
    return NP_PBM1064_OK;
}

np_pbm1064_status_t np_pbm1064_hal_t2_1170_get_dose(float *dose_J_cm2_out)
{
    /*
     * Real implementation: read integrated monitor PD dose accumulator from
     * the T2 laser module (OI-PBM-08).  Stub: returns a synthetic dose
     * proportional to current duty_pct so that FAI-T2-03 can verify readback.
     */
    if (!dose_J_cm2_out) { return NP_PBM1064_ERR_INVALID_ARG; }
    if (s_stub_t2_1170_enabled) {
        /* Simulate ~2 J/cm² per second at 100% duty for FAI determinism. */
        s_stub_t2_1170_dose_J += ((float)s_stub_t2_1170_duty_pct / 100.0f) * 2.0f;
    }
    *dose_J_cm2_out = s_stub_t2_1170_dose_J;
    return NP_PBM1064_OK;
}

np_pbm1064_status_t np_pbm1064_hal_t2_1170_get_temp(float *tec_temp_c_out)
{
    /* Real implementation: read TEC thermistor from T2 laser module (OI-PBM-08). */
    if (!tec_temp_c_out) { return NP_PBM1064_ERR_INVALID_ARG; }
    *tec_temp_c_out = s_stub_t2_1170_tec_temp;
    return NP_PBM1064_OK;
}

np_pbm1064_status_t np_pbm1064_hal_t2_sloreta_get_target(int16_t *mni_x_out,
                                                           int16_t *mni_y_out,
                                                           int16_t *mni_z_out,
                                                           bool    *valid_out)
{
    /*
     * Real implementation: read computed MNI target from np_fw_sloreta engine
     * (NP-FW-HD-001 Rev 1).  Stub returns DLPFC_L default so that software-
     * passable FAI-T2-03 can verify the sLORETA logging path.  OI-SES-T2-01.
     */
    if (!mni_x_out || !mni_y_out || !mni_z_out || !valid_out) {
        return NP_PBM1064_ERR_INVALID_ARG;
    }
    *mni_x_out = NP_T2_SLORETA_STUB_MNI_X;
    *mni_y_out = NP_T2_SLORETA_STUB_MNI_Y;
    *mni_z_out = NP_T2_SLORETA_STUB_MNI_Z;
    *valid_out = true;
    return NP_PBM1064_OK;
}

/* ── TIA gain switch stubs (OI-PBM-HW-01; Hub PCB Rev B DG2788A) ────────────── */

/*
 * Stub gain state per slot — tracks the last requested gain setting so that
 * software tests can verify correct sequencing without real GPIO hardware.
 */
static np_tia_gain_t s_stub_tia_gain[5] = {
    NP_TIA_GAIN_HIGH, NP_TIA_GAIN_HIGH, NP_TIA_GAIN_HIGH,
    NP_TIA_GAIN_HIGH, NP_TIA_GAIN_HIGH
};

np_pbm1064_status_t np_pbm1064_hal_tia_gain_set(uint8_t slot, np_tia_gain_t gain)
{
    if (slot >= 5U) { return NP_PBM1064_ERR_INVALID_ARG; }
    s_stub_tia_gain[slot] = gain;
    /*
     * Real implementation: drive GPIO_B0_(04 + slot) HIGH for NP_TIA_GAIN_LOW,
     * LOW for NP_TIA_GAIN_HIGH (see NP-HW-HUB-001 Rev 2 §3.4).
     * After asserting HIGH, wait NP_PBM1064_TIA_GAIN_SETTLE_US before returning
     * to allow DG2788A switch to settle before I2C mux enable is called.
     */
    return NP_PBM1064_OK;
}

void np_pbm1064_hal_tia_gain_boot_init(void)
{
    /*
     * Real implementation: configure GPIO_B0_04..08 as GPIO2 push-pull outputs
     * driven LOW (NP_TIA_GAIN_HIGH = Rf 47 kΩ default) before zone detect starts.
     * See NP-HW-HUB-001 Rev 2 §5.3.
     */
    for (uint8_t i = 0; i < 5U; i++) {
        s_stub_tia_gain[i] = NP_TIA_GAIN_HIGH;
    }
}
