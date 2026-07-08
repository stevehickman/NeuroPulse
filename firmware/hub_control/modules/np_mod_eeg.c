/*
 * NeurOne Hub Control — EEG Module Driver (ADS1299, 8-ch, 500Hz, 24-bit)
 * Document: NP-FW-HUB-001 Rev A §8.2
 *
 * The ADS1299 is fixed hardware — always present (detect always returns OK).
 * EEG waveform data is DMA-driven and written directly to UHDR via
 * np_log_eeg_sample_block() from the DMA ISR; this driver handles configuration,
 * impedance reads, and band-power computation for adaptive feedback.
 *
 * EEG self-calibration at session start:  ADS1299 internal reference is routed
 * to all channels for one calibration epoch; gain/offset coefficients are stored
 * in the Config partition (SHDR) per NP-FW-EMMC-001 Rev A §9.
 *
 * HAL stubs:
 *   OI-EEG-01: np_mod_eeg_hal_spi_write_reg(reg, val)     — ADS1299 SPI reg write
 *   OI-EEG-02: np_mod_eeg_hal_spi_read_reg(reg) → uint8_t
 *   OI-EEG-03: np_mod_eeg_hal_start_continuous()           — assert DRDY, start DMA
 *   OI-EEG-04: np_mod_eeg_hal_stop_continuous()
 *   OI-EEG-05: np_mod_eeg_hal_read_impedance(ch) → float kohm
 *   OI-EEG-06: np_mod_eeg_hal_get_band_power(band) → float dB
 *   OI-EEG-07: np_mod_eeg_hal_get_dominant_band() → uint8_t (np_eeg_band_t)
 *   OI-EEG-08: np_mod_eeg_hal_self_calibrate()    — runs internal ref epoch
 */

#include "np_hub_types.h"
#include "np_module_registry.h"
#include "np_session_log.h"
#include <string.h>

/* ADS1299 register addresses */
#define ADS_ID       0x00U
#define ADS_CONFIG1  0x01U
#define ADS_CONFIG2  0x02U
#define ADS_CONFIG3  0x03U
#define ADS_LOFF     0x04U
#define ADS_CH1SET   0x05U   /* through CH8SET at 0x0C */
#define ADS_LOFF_STATP  0x12U
#define ADS_LOFF_STATN  0x13U
#define ADS_GPIO     0x14U

/* CONFIG1: DR = 500SPS (0x96), daisy-chain off */
#define ADS_CONFIG1_500SPS  0x96U
/* CONFIG2: internal test signal off */
#define ADS_CONFIG2_INT_REF 0xD0U
/* CONFIG3: internal reference enabled, bias enabled */
#define ADS_CONFIG3_REFBUF  0xE0U
/* CHnSET: normal input, PGA gain 24 */
#define ADS_CHNSET_GAIN24   0x60U
/* CHnSET: shorted (used for calibration epoch) */
#define ADS_CHNSET_SHORT    0x01U

/* HAL stubs */
extern void     np_mod_eeg_hal_spi_write_reg(uint8_t reg, uint8_t val);
extern uint8_t  np_mod_eeg_hal_spi_read_reg(uint8_t reg);
extern void     np_mod_eeg_hal_start_continuous(void);
extern void     np_mod_eeg_hal_stop_continuous(void);
extern float    np_mod_eeg_hal_read_impedance(uint8_t ch);
extern float    np_mod_eeg_hal_get_band_power(uint8_t band);
extern uint8_t  np_mod_eeg_hal_get_dominant_band(void);
extern void     np_mod_eeg_hal_self_calibrate(void);

/* ── State ───────────────────────────────────────────────────────────────────── */

static bool    s_recording  = false;
static uint8_t s_ch_mask    = 0xFFU; /* all 8 channels by default */
static uint8_t s_notch      = 0U;
static uint8_t s_gain       = 6U;    /* 24× */
static uint8_t s_adaptive_out = 0U;

/* ── Detect ──────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_eeg_detect(uint8_t slot, np_hub_mod_type_t *type_out)
{
    (void)slot;
    /* ADS1299 is fixed silicon on the hub PCB — always present.
     * Read the ID register to confirm SPI communication. */
    uint8_t id = np_mod_eeg_hal_spi_read_reg(ADS_ID);
    if ((id & 0x1FU) != 0x1EU) { /* ADS1299: ID[4:0] = 1 1110b */
        return NP_HUB_ERR_NOT_PRESENT;
    }
    *type_out = NP_MOD_EEG;
    return NP_HUB_OK;
}

/* ── Init ────────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_eeg_init(uint8_t slot)
{
    (void)slot;
    s_recording   = false;
    s_ch_mask     = 0xFFU;
    s_notch       = 0U;
    s_gain        = 6U;
    s_adaptive_out= 0U;

    /* Power-on reset sequence per ADS1299 datasheet. */
    np_mod_eeg_hal_spi_write_reg(ADS_CONFIG1, ADS_CONFIG1_500SPS);
    np_mod_eeg_hal_spi_write_reg(ADS_CONFIG2, ADS_CONFIG2_INT_REF);
    np_mod_eeg_hal_spi_write_reg(ADS_CONFIG3, ADS_CONFIG3_REFBUF);

    /* Configure all channels: normal input, PGA 24× */
    for (uint8_t ch = 0U; ch < NP_EEG_CHANNELS; ch++) {
        np_mod_eeg_hal_spi_write_reg(ADS_CH1SET + ch, ADS_CHNSET_GAIN24);
    }

    /* Self-calibration at session start (eliminates gain/offset drift). */
    np_mod_eeg_hal_self_calibrate();

    return NP_HUB_OK;
}

/* ── Control ─────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_eeg_control(uint8_t slot, const void *params, uint16_t len)
{
    (void)slot;

    /* Stop */
    if (params == NULL || len == 0U) {
        if (s_recording) {
            np_mod_eeg_hal_stop_continuous();
            s_recording = false;
        }
        return NP_HUB_OK;
    }

    if (len < sizeof(np_mod_eeg_params_t)) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    const np_mod_eeg_params_t *p = (const np_mod_eeg_params_t *)params;
    s_ch_mask     = p->channel_mask;
    s_notch       = p->notch;
    s_gain        = p->gain;
    s_adaptive_out= p->adaptive_out;

    /* Apply gain to enabled channels; power down disabled ones. */
    uint8_t gain_reg = (uint8_t)(p->gain << 4) & 0x70U; /* PGA[2:0] in CHnSET[6:4] */
    for (uint8_t ch = 0U; ch < NP_EEG_CHANNELS; ch++) {
        if (s_ch_mask & (1U << ch)) {
            np_mod_eeg_hal_spi_write_reg(ADS_CH1SET + ch, gain_reg);
        } else {
            /* Power down channel: MUX = 001 (shorted) */
            np_mod_eeg_hal_spi_write_reg(ADS_CH1SET + ch,
                                          (uint8_t)(0x80U | ADS_CHNSET_SHORT));
        }
    }

    if (!s_recording) {
        np_mod_eeg_hal_start_continuous();
        s_recording = true;
    }

    return NP_HUB_OK;
}

/* ── Telemetry ───────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_eeg_telemetry(uint8_t slot, np_telem_record_t *out)
{
    (void)slot;
    if (out == NULL) { return NP_HUB_ERR_INVALID_ARG; }

    out->mod_type = NP_MOD_EEG;
    out->slot     = NP_HUB_SLOT_EEG;

    np_telem_eeg_t *e = &out->data.eeg;

    /* Impedance per channel (kohm) — UHDR */
    for (uint8_t ch = 0U; ch < NP_EEG_CHANNELS; ch++) {
        e->impedance_kohm[ch] = (s_ch_mask & (1U << ch))
                                 ? np_mod_eeg_hal_read_impedance(ch)
                                 : 0.0f;
    }

    /* Band power and dominant band — UHDR */
    e->dominant_band = np_mod_eeg_hal_get_dominant_band();
    for (uint8_t b = 0U; b < 5U; b++) {
        e->band_power_db[b] = np_mod_eeg_hal_get_band_power(b);
    }

    return NP_HUB_OK;
}

/* ── Shutdown ────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_eeg_shutdown(uint8_t slot)
{
    return np_mod_eeg_control(slot, NULL, 0U);
}
