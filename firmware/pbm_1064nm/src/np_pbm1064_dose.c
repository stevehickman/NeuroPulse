#include "np_pbm1064.h"
#include "np_pbm1064_config.h"
#include <string.h>

/* HAL stubs (OI-PBM-01, OI-PBM-05, OI-PBM-06) */
extern uint16_t np_adc_read_pd(uint8_t zone, uint8_t pd_channel);
extern void     np_uhdr_pbm_sample(uint8_t zone, uint16_t wl_nm,
                                   float irr_pd1, float irr_pd2);
extern np_err_t np_config_read_pbm_cal(uint8_t zone, np_pbm_cal_t *out);

/* PD channel indices for ADC mux — platform-specific (OI-PBM-01) */
#define PD1_ADC_CHANNEL  0u
#define PD2_ADC_CHANNEL  1u

static const uint16_t wl_nm[NP_PBM1064_WAVELENGTH_COUNT] = {660u, 808u, 1064u};

void np_pbm1064_load_cal(np_pbm_cal_t *cal)
{
    uint8_t zone, wl;
    bool any_factory = false;

    for (zone = 0u; zone < NP_PBM1064_ZONE_COUNT; zone++) {
        np_pbm_cal_t zone_cal;
        if (np_config_read_pbm_cal(zone, &zone_cal) == NP_OK) {
            for (wl = 0u; wl < NP_PBM1064_WAVELENGTH_COUNT; wl++) {
                cal->K_PD1[zone][wl]      = zone_cal.K_PD1[zone][wl];
                cal->K_PD2[zone][wl]      = zone_cal.K_PD2[zone][wl];
                cal->K_ratio_nom[zone][wl] = zone_cal.K_ratio_nom[zone][wl];
            }
            cal->cal_from_factory[zone] = true;
            any_factory = true;
        } else {
            /* Default coefficients for 1064 nm; 660/808 channels use base module calibration */
            cal->K_PD1[zone][NP_PBM_CH_C]       = NP_PBM1064_K_DEFAULT_PD1_1064;
            cal->K_PD2[zone][NP_PBM_CH_C]       = NP_PBM1064_K_DEFAULT_PD2_1064;
            cal->K_ratio_nom[zone][NP_PBM_CH_C] = NP_PBM1064_K_DEFAULT_RATIO_NOM_1064;
            cal->cal_from_factory[zone] = false;
        }
    }
    (void)any_factory;
}

static void dose_tick_zone(uint8_t zone, np_pbm_channel_t ch,
                           np_pbm1064_zone_state_t *zs,
                           const np_pbm_cal_t *cal)
{
    uint16_t adc_pd1 = np_adc_read_pd(zone, PD1_ADC_CHANNEL);
    uint16_t adc_pd2 = np_adc_read_pd(zone, PD2_ADC_CHANNEL);

    float irr_pd1 = (float)adc_pd1 * cal->K_PD1[zone][ch];
    float irr_pd2 = (float)adc_pd2 * cal->K_PD2[zone][ch];

    zs->irr_pd1[ch] = irr_pd1;
    zs->irr_pd2[ch] = irr_pd2;

    /* PD1/PD2 ratio for fouling/aging disambiguation (RISK-14 Option B) */
    float ratio = (irr_pd2 > 0.5f) ? (irr_pd1 / irr_pd2) : 0.0f;
    zs->pd1_pd2_ratio[ch] = ratio;

    float ratio_delta = ratio - cal->K_ratio_nom[zone][ch];
    if (ratio_delta < NP_PBM1064_FOULING_RATIO_DELTA) {
        zs->fouling_flag = true;
    } else if (zs->baseline_pd1[ch] > 0.5f &&
               irr_pd1 < zs->baseline_pd1[ch] * NP_PBM1064_AGING_RATIO_THRESHOLD) {
        zs->aging_flag = true;
    }

    /* Accumulate dose: J/cm² = mW/cm² × 0.1 s × 1e-3 */
    zs->dose_j_cm2[ch] += irr_pd1 * (1.0f / (float)NP_PBM1064_DOSE_TICK_HZ) * 1e-3f;

    /* Dose limit check */
    static const float limits[NP_PBM1064_WAVELENGTH_COUNT] = {
        NP_PBM1064_DOSE_LIMIT_660_J_CM2,
        NP_PBM1064_DOSE_LIMIT_808_J_CM2,
        NP_PBM1064_DOSE_LIMIT_1064_J_CM2,
    };
    float warn_threshold = limits[ch] * NP_PBM1064_DOSE_WARN_PCT / 100.0f;
    if (!zs->dose_warn[ch] && zs->dose_j_cm2[ch] >= warn_threshold) {
        zs->dose_warn[ch] = true;
        /* App notification happens at session layer via status field */
    }
    if (!zs->dose_limit_hit[ch] && zs->dose_j_cm2[ch] >= limits[ch]) {
        zs->dose_limit_hit[ch] = true;
    }

    /* UHDR write (user health data — irradiance time series, per wavelength) */
    np_uhdr_pbm_sample(zone, wl_nm[ch], irr_pd1, irr_pd2);
}

void np_pbm1064_dose_tick(np_pbm1064_session_t *session)
{
    uint8_t slot;
    for (slot = 0u; slot < NP_PBM1064_ZONE_COUNT; slot++) {
        if (session->zone[slot].slot_type != NP_SLOT_SMART_MODULE) continue;
        if (!session->zone[slot].driver_ready) continue;

        uint8_t ch_mask = session->zone[slot].active_ch_mask;
        if (ch_mask & NP_PBM1064_CH_A) dose_tick_zone(slot, NP_PBM_CH_A,
                                                       &session->zone[slot], &session->cal);
        if (ch_mask & NP_PBM1064_CH_B) dose_tick_zone(slot, NP_PBM_CH_B,
                                                       &session->zone[slot], &session->cal);
        if (ch_mask & NP_PBM1064_CH_C) dose_tick_zone(slot, NP_PBM_CH_C,
                                                       &session->zone[slot], &session->cal);
    }
}

void np_pbm1064_throttle_cascade(uint8_t zone, float p_aggregate_mw_cm2,
                                 np_pbm1064_session_t *session)
{
    np_pbm1064_zone_state_t *zs = &session->zone[zone];
    float excess = p_aggregate_mw_cm2 - NP_PBM1064_AGGREGATE_IRRADIANCE_MW_CM2;

    /* Throttle CH_C first, then CH_B, preserve CH_A */
    if ((zs->active_ch_mask & NP_PBM1064_CH_C) && excess > 0.0f) {
        uint8_t new_duty = (uint8_t)(session->preset[zone].duty_pct_C * 0.75f);
        np_pbm1064_set_duty(zone, NP_PBM_CH_C, new_duty);
        excess -= session->zone[zone].irr_pd1[NP_PBM_CH_C] * 0.25f;
    }
    if ((zs->active_ch_mask & NP_PBM1064_CH_B) && excess > 0.0f) {
        uint8_t new_duty = (uint8_t)(session->preset[zone].duty_pct_B * 0.75f);
        np_pbm1064_set_duty(zone, NP_PBM_CH_B, new_duty);
    }
}

void np_pbm1064_thermal_check(np_pbm1064_session_t *session)
{
    uint8_t slot;
    for (slot = 0u; slot < NP_PBM1064_ZONE_COUNT; slot++) {
        if (session->zone[slot].slot_type != NP_SLOT_SMART_MODULE) continue;

        float p_agg = session->zone[slot].irr_pd1[NP_PBM_CH_A]
                    + session->zone[slot].irr_pd1[NP_PBM_CH_B]
                    + session->zone[slot].irr_pd1[NP_PBM_CH_C];

        if (p_agg > NP_PBM1064_AGGREGATE_IRRADIANCE_MW_CM2) {
            np_pbm1064_throttle_cascade(slot, p_agg, session);
        }
    }
}
