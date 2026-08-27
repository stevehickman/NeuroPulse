/*
 * NeurOne 1064nm Smart Zone Module — InGaAs PD Dose Metering
 * Document: NP-FW-PBM1064-001 Rev 1 §6
 *
 * Dual-PD architecture (RISK-14 Option B):
 *   PD1 — behind PDMS optical window (forward emission)
 *   PD2 — scalp-facing (backscattered tissue power)
 *
 * PD1/PD2 ratio disambiguates fouling (PD1↓, PD2 stable) from LED aging
 * (both↓ proportional).  Raw PD counts → UHDR; ratio slope → SHDR only.
 */

#include "np_pbm_dose.h"
#include "np_pbm_drive.h"
#include "np_pbm_hal.h"
#include <string.h>

/* ── Factory-default calibration coefficients ────────────────────────────────── */

/*
 * Used when Config partition has no factory calibration (integrating sphere
 * data not yet available for a given slot/wavelength combination).
 * SHDR flag cal_source=NP_CAL_DEFAULT is set in this case.
 */
static const np_pbm_cal_t k_default_cal[NP_PBM_WL_COUNT] = {
    /* NP_WL_660NM  */ { .K_PD1 = 0.120f, .K_PD2 = 0.090f, .K_ratio_nom = 1.333f, .valid = false },
    /* NP_WL_808NM  */ { .K_PD1 = 0.105f, .K_PD2 = 0.085f, .K_ratio_nom = 1.235f, .valid = false },
    /* NP_WL_1064NM */ { .K_PD1 = 0.088f, .K_PD2 = 0.072f, .K_ratio_nom = 1.222f, .valid = false },
};

/* ── Calibration loading (UID-keyed, OI-HUB-C06) ─────────────────────────────── */

static np_pbm_cal_provider_fn s_cal_provider     = NULL;
static void                      *s_cal_provider_ctx = NULL;

void np_pbm_dose_set_cal_provider(np_pbm_cal_provider_fn fn, void *ctx)
{
    s_cal_provider     = fn;
    s_cal_provider_ctx = ctx;
}

bool np_pbm_module_uid_is_zero(const np_pbm_module_uid_t *uid)
{
    if (uid == NULL) { return true; }
    for (uint8_t i = 0; i < NP_PBM_MODULE_UID_LEN; i++) {
        if (uid->b[i] != 0U) { return false; }
    }
    return true;
}

static void load_defaults(np_pbm_cal_t cal_out[NP_PBM_WL_COUNT])
{
    for (uint8_t w = 0; w < NP_PBM_WL_COUNT; w++) {
        cal_out[w] = k_default_cal[w]; /* valid = false → SHDR: DEFAULT */
    }
}

np_cal_source_t np_pbm_dose_load_cal(
    const np_pbm_module_uid_t *uid,
    np_pbm_cal_t               cal_out[NP_PBM_WL_COUNT])
{
    if (cal_out == NULL) { return NP_CAL_DEFAULT; }

    /*
     * Fail closed in every direction that is not an affirmative hit on THIS
     * module's UID. A zero UID means the hub reported an empty socket or could
     * not identify the occupant; there is no coherent record to fetch, and
     * falling through to any other module's coefficients is the defect
     * OI-HUB-C06 exists to prevent.
     */
    if (np_pbm_module_uid_is_zero(uid) || s_cal_provider == NULL) {
        load_defaults(cal_out);
        return NP_CAL_DEFAULT;
    }

    /*
     * Scratch, not cal_out: a provider that wrote some wavelength rows and
     * then failed would otherwise leave this socket holding a mix of one
     * module's factory coefficients and another's defaults, reported as
     * DEFAULT. Publishing only on success makes a partial write unobservable.
     */
    np_pbm_cal_t scratch[NP_PBM_WL_COUNT];
    if (!s_cal_provider(uid, scratch, s_cal_provider_ctx)) {
        load_defaults(cal_out);
        return NP_CAL_DEFAULT;
    }

    for (uint8_t w = 0; w < NP_PBM_WL_COUNT; w++) {
        cal_out[w] = scratch[w];
    }
    return NP_CAL_FACTORY;
}

/* ── Dose tick ───────────────────────────────────────────────────────────────── */

np_pbm_status_t np_pbm_dose_tick(
    uint8_t                    socket_id,
    const np_pbm_cal_t     cal[NP_PBM_WL_COUNT],
    np_pbm_dose_state_t   *dose)
{
    const float tick_duration_s = (float)NP_PBM_DOSE_TICK_MS / 1000.0f;

    for (uint8_t w = 0; w < NP_PBM_WL_COUNT; w++) {
        if (dose->dose_limit_hit[w]) {
            continue; /* channel already disabled for this wavelength */
        }

        uint16_t pd1_raw = 0, pd2_raw = 0;
        if (!np_pbm_hal_adc_read_pd(socket_id, 0, &pd1_raw)) { continue; }
        if (!np_pbm_hal_adc_read_pd(socket_id, 1, &pd2_raw)) { continue; }

        dose->pd1_counts[w] = (float)pd1_raw;
        dose->pd2_counts[w] = (float)pd2_raw;

        /* Compute ratio-corrected irradiance. */
        float ratio_adj = 0.0f;
        if (pd2_raw > 0U && cal[w].K_ratio_nom > 0.0f) {
            float ratio = dose->pd1_counts[w] / (float)pd2_raw;
            dose->ratio_current = ratio;
            ratio_adj = (ratio / cal[w].K_ratio_nom) - 1.0f;
        }

        float irr = cal[w].K_PD1 * dose->pd1_counts[w] * (1.0f + ratio_adj);
        if (irr < 0.0f) { irr = 0.0f; }
        dose->irradiance_mW_cm2[w] = irr;

        /* Accumulate dose. */
        dose->dose_J_cm2[w] += irr * 0.001f * tick_duration_s;

        /* Check per-wavelength dose limit. */
        const float limits[NP_PBM_WL_COUNT] = {
            NP_PBM_DOSE_LIMIT_660_J_CM2,
            NP_PBM_DOSE_LIMIT_808_J_CM2,
            NP_PBM_DOSE_LIMIT_1064_J_CM2,
        };
        if (dose->dose_J_cm2[w] >= limits[w]) {
            dose->dose_limit_hit[w] = true;
            return NP_PBM_ERR_DOSE_LIMIT;
        }
    }
    return NP_PBM_OK;
}

/* ── Aggregate irradiance ────────────────────────────────────────────────────── */

float np_pbm_dose_aggregate_irradiance(const np_pbm_dose_state_t *dose)
{
    float total = 0.0f;
    for (uint8_t w = 0; w < NP_PBM_WL_COUNT; w++) {
        if (!dose->dose_limit_hit[w]) {
            total += dose->irradiance_mW_cm2[w];
        }
    }
    return total;
}

/* ── PD1/PD2 ratio evaluation (fouling vs aging) ─────────────────────────────── */

void np_pbm_dose_evaluate_ratio(
    uint8_t                        socket_id,
    const np_pbm_cal_t        *cal_1064,
    const np_pbm_dose_state_t *dose,
    uint32_t                       device_session_count,
    bool                          *fouling_out,
    bool                          *aging_out)
{
    *fouling_out = false;
    *aging_out   = false;

    float pd1 = dose->pd1_counts[NP_WL_1064NM];
    float pd2 = dose->pd2_counts[NP_WL_1064NM];

    if (pd2 < (float)NP_PBM_PD2_STABLE_MIN) {
        return; /* PD2 too low to be meaningful */
    }

    float ratio = (pd2 > 0.0f) ? (pd1 / pd2) : 0.0f;

    /* Fouling: PD1 attenuated ≥20%, PD2 remains stable. */
    if (ratio < (cal_1064->K_ratio_nom * NP_PBM_FOULING_RATIO_THRESH)) {
        *fouling_out = true;
    }

    /* Aging: both PD1 and PD2 attenuated, ratio still within ±15% of nominal. */
    float ratio_norm = cal_1064->K_ratio_nom;
    bool ratio_stable = (ratio > ratio_norm * 0.85f) && (ratio < ratio_norm * 1.15f);

    /* Approximate factory baseline PD1 from K_PD1 and expected irradiance. */
    float pd1_factory_baseline = cal_1064->K_PD1 > 0.0f ?
                                 (100.0f / cal_1064->K_PD1) : 0.0f;
    bool  pd1_depleted = (pd1_factory_baseline > 0.0f) &&
                          (pd1 < pd1_factory_baseline * NP_PBM_AGING_PD1_PMIN);

    if (ratio_stable && pd1_depleted) {
        *aging_out = true;
    }

    /*
     * Write only the ratio (device metric — no user biology) to SHDR.
     * The fault entry records ratio and session count; no raw PD counts.
     */
    if (*fouling_out || *aging_out) {
        np_pbm_shdr_fault_entry_t fe = {
            .device_session_count = device_session_count,
            .socket_id            = socket_id,
            .channel              = 2U,  /* CH_C (1064nm) */
            .fault_reason         = *fouling_out ? NP_PBM_FAULT_NONE :
                                                    NP_PBM_FAULT_NONE,
            /* Ratio encoded as 8-bit fixed-point ×100 for SHDR compactness. */
            .status_reg_value     = (uint8_t)(ratio * 100.0f > 255.0f ?
                                              255U : (uint8_t)(ratio * 100.0f)),
        };
        np_pbm_hal_shdr_log_fault(&fe);
    }
}

/* ── Aggregate irradiance throttle cascade ───────────────────────────────────── */

np_pbm_status_t np_pbm_dose_apply_throttle(
    uint8_t socket_id,
    float   aggregate_irradiance,
    np_pbm_drv_slot_t *drv)
{
    float excess = aggregate_irradiance - NP_PBM_AGGREGATE_IRRADIANCE_MW_CM2;
    if (excess <= 0.0f) {
        return NP_PBM_OK;
    }

    /*
     * Throttle cascade: CH_C → CH_B → CH_A.
     * Reduce duty proportionally on each channel to shed the excess irradiance.
     * Using a simple linear approximation: duty reduction = excess / total.
     */
    float reduction_frac = excess / aggregate_irradiance;
    if (reduction_frac > 1.0f) { reduction_frac = 1.0f; }

    /* CH_C first (1064nm, deepest). */
    if (drv->ch_enable & NP_PBM_CH_C_EN) {
        uint8_t new_duty_c = (uint8_t)((float)drv->duty[2] * (1.0f - reduction_frac));
        np_pbm_drive_set_duty(socket_id, drv, NP_PBM_CH_C_EN, new_duty_c);
        return NP_PBM_OK;
    }

    /* CH_B (808nm). */
    if (drv->ch_enable & NP_PBM_CH_B_EN) {
        uint8_t new_duty_b = (uint8_t)((float)drv->duty[1] * (1.0f - reduction_frac));
        np_pbm_drive_set_duty(socket_id, drv, NP_PBM_CH_B_EN, new_duty_b);
        return NP_PBM_OK;
    }

    /* CH_A (660nm) last resort. */
    if (drv->ch_enable & NP_PBM_CH_A_EN) {
        uint8_t new_duty_a = (uint8_t)((float)drv->duty[0] * (1.0f - reduction_frac));
        np_pbm_drive_set_duty(socket_id, drv, NP_PBM_CH_A_EN, new_duty_a);
    }

    return NP_PBM_ERR_IRRADIANCE_LIMIT;
}

/* ── Reset ───────────────────────────────────────────────────────────────────── */

void np_pbm_dose_reset(np_pbm_dose_state_t *dose)
{
    memset(dose, 0, sizeof(*dose));
}
