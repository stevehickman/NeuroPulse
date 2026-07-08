/*
 * NeurOne Hub Control — PBM Zone Module Driver (Base + Smart)
 * Document: NP-FW-HUB-001 Rev A §8.1
 *
 * Handles both base modules (660nm + 808nm, direct PWM) and smart modules
 * (+1064nm, ATtiny402 I2C slave).  Module type is determined by the registry
 * entry; the same four function pointers serve both variants.
 *
 * Delegates detection to np_pbm1064_detect.c (existing firmware/pbm_1064nm/).
 * Delegates I2C driving for smart modules to np_pbm1064_drive.c.
 * Delegates dose metering to np_pbm1064_dose.c.
 *
 * HAL stubs:
 *   OI-PBM-HAL-01: np_mod_pbm_hal_pwm_set(slot, cur_a, cur_b, freq, duty)
 *     Sets base module PWM registers on the hub FPC driver.
 *   OI-PBM-HAL-02: np_mod_pbm_hal_ntc_read(slot) → float °C
 *   OI-PBM-HAL-03: np_mod_pbm_hal_pd_read(slot, wl_idx) → uint16_t ADC counts
 */

#include "np_hub_types.h"
#include "np_module_registry.h"
#include "np_session_log.h"
#include "np_safety_spi.h"
#include <string.h>

/* Pull in existing PBM detection and drive infrastructure. */
#include "np_pbm1064_detect.h"
#include "np_pbm1064_drive.h"
#include "np_pbm1064_dose.h"
#include "np_pbm1064_types.h"
#include "np_pbm1064_config.h"

/* ── HAL stubs ────────────────────────────────────────────────────────────────── */

extern np_hub_status_t np_mod_pbm_hal_pwm_set(uint8_t slot,
                                                uint8_t cur_a, uint8_t cur_b,
                                                uint8_t freq_code, uint8_t duty);
extern float    np_mod_pbm_hal_ntc_read(uint8_t slot);
extern uint16_t np_mod_pbm_hal_pd_read(uint8_t slot, uint8_t wl_idx);

/* ── Per-slot state ──────────────────────────────────────────────────────────── */

typedef struct {
    np_hub_mod_type_t       type;         /* BASE or SMART */
    bool                    active;
    float                   ntc_peak_c;
    uint16_t                throttle_count;
    np_pbm1064_dose_state_t dose;
} np_mod_pbm_state_t;

static np_mod_pbm_state_t s_state[NP_HUB_ZONE_SLOT_COUNT];

/* ── Duty ceiling enforcement ────────────────────────────────────────────────── */

static uint8_t clamp_duty(uint8_t duty)
{
    return (duty > NP_PBM1064_DUTY_MAX_REG) ? NP_PBM1064_DUTY_MAX_REG : duty;
}

/* ── Detect ──────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_pbm_detect(uint8_t slot, np_hub_mod_type_t *type_out)
{
    if (slot >= NP_HUB_ZONE_SLOT_COUNT || type_out == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    np_slot_type_t slot_type;
    /* np_pbm1064_detect_slot() performs ZONE_ID ADC classification with 3×100ms
     * debounce (RISK-18) and TIA gain switch for smart modules (OI-PBM-HW-01). */
    int rc = np_pbm1064_detect_slot((int)slot, &slot_type);
    if (rc != 0) {
        return NP_HUB_ERR_NOT_PRESENT;
    }

    switch (slot_type) {
    case NP_SLOT_SMART:
        *type_out = NP_MOD_PBM_SMART;
        return NP_HUB_OK;
    case NP_SLOT_BASE_MODULE:
        *type_out = NP_MOD_PBM_BASE;
        return NP_HUB_OK;
    default:
        return NP_HUB_ERR_NOT_PRESENT;
    }
}

/* ── Init ────────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_pbm_init(uint8_t slot)
{
    if (slot >= NP_HUB_ZONE_SLOT_COUNT) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    memset(&s_state[slot], 0, sizeof(np_mod_pbm_state_t));
    s_state[slot].ntc_peak_c = 0.0f;

    /* ADS1299 self-calibration is handled by the EEG module; PBM does not
     * perform a session-start calibration beyond what detect already confirmed. */
    return NP_HUB_OK;
}

/* ── Control ─────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_pbm_control(uint8_t slot, const void *params, uint16_t len)
{
    if (slot >= NP_HUB_ZONE_SLOT_COUNT) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    /* Stop command: params==NULL or len==0 */
    if (params == NULL || len == 0U) {
        if (s_state[slot].type == NP_MOD_PBM_SMART) {
            (void)np_pbm1064_drive_disable_all((int)slot);
        } else {
            (void)np_mod_pbm_hal_pwm_set(slot, 0U, 0U, 0U, 0U);
        }
        s_state[slot].active = false;
        np_safety_spi_request_disable(NP_SAFETY_EN_PBM_ZONE_0 << slot);
        return NP_HUB_OK;
    }

    /* NTC over-temperature check before enabling */
    float ntc = np_mod_pbm_hal_ntc_read(slot);
    if (ntc >= (float)NP_PBM1064_THERMAL_CUTOFF_C) {
        np_log_shdr_fault(slot, NP_MOD_PBM_BASE, 0x01U /* thermal */, 0U);
        return NP_HUB_ERR_GENERIC;
    }

    if (s_state[slot].type == NP_MOD_PBM_SMART &&
        len >= sizeof(np_mod_pbm_smart_params_t)) {

        const np_mod_pbm_smart_params_t *p =
            (const np_mod_pbm_smart_params_t *)params;

        uint8_t duty = clamp_duty(p->duty);
        int rc = np_pbm1064_drive_set_all((int)slot,
                                           p->cur_a, p->cur_b, p->cur_c,
                                           p->ch_mask,
                                           p->freq_code, duty);
        if (rc != 0) {
            return NP_HUB_ERR_MOD_FAULT;
        }

    } else if (len >= sizeof(np_mod_pbm_base_params_t)) {

        const np_mod_pbm_base_params_t *p =
            (const np_mod_pbm_base_params_t *)params;

        uint8_t duty = clamp_duty(p->duty);
        np_hub_status_t rc = np_mod_pbm_hal_pwm_set(slot,
                                                      p->cur_a, p->cur_b,
                                                      p->freq_code, duty);
        if (rc != NP_HUB_OK) {
            return NP_HUB_ERR_MOD_FAULT;
        }
    } else {
        return NP_HUB_ERR_INVALID_ARG;
    }

    s_state[slot].active = true;
    return NP_HUB_OK;
}

/* ── Telemetry ───────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_pbm_telemetry(uint8_t slot, np_telem_record_t *out)
{
    if (slot >= NP_HUB_ZONE_SLOT_COUNT || out == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    out->mod_type = (s_state[slot].type == NP_MOD_PBM_SMART)
                    ? NP_MOD_PBM_SMART : NP_MOD_PBM_BASE;
    out->slot = slot;

    np_telem_pbm_t *p = &out->data.pbm;
    memset(p, 0, sizeof(*p));

    float ntc = np_mod_pbm_hal_ntc_read(slot);
    if (ntc > s_state[slot].ntc_peak_c) {
        s_state[slot].ntc_peak_c = ntc;
    }
    p->ntc_peak_c[slot] = s_state[slot].ntc_peak_c;

    /* NTC thermal throttle check */
    if (ntc >= (float)NP_PBM1064_THERMAL_FAULT_C && s_state[slot].active) {
        s_state[slot].throttle_count++;
        (void)np_mod_pbm_control(slot, NULL, 0U); /* graceful stop */
        p->fault_mask |= (1U << slot);
    }

    p->throttle_events = s_state[slot].throttle_count;

    /* Read PD1/PD2 for dose metering (smart modules only) */
    if (s_state[slot].type == NP_MOD_PBM_SMART) {
        for (uint8_t wl = 0U; wl < NP_PBM1064_WL_COUNT; wl++) {
            uint16_t pd1 = np_mod_pbm_hal_pd_read(slot, wl);
            /* PD2 on pin 19 — read via separate channel */
            uint16_t pd2 = np_mod_pbm_hal_pd_read(slot, wl + NP_PBM1064_WL_COUNT);
            if (pd1 > 0U) {
                p->pd_ratio[slot] = (pd2 > 0U) ? ((float)pd1 / (float)pd2) : 0.0f;
            }
        }

        /* Dose update via existing dose module */
        np_pbm1064_dose_tick((int)slot, &s_state[slot].dose,
                              (float)NP_PBM1064_DOSE_TICK_MS / 1000.0f);
        for (uint8_t z = 0U; z < NP_HUB_ZONE_SLOT_COUNT; z++) {
            p->dose_J_cm2[z] = s_state[slot].dose.dose_J_cm2[0]; /* summary */
        }
    }

    return NP_HUB_OK;
}

/* ── Shutdown ────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_pbm_shutdown(uint8_t slot)
{
    return np_mod_pbm_control(slot, NULL, 0U);
}
