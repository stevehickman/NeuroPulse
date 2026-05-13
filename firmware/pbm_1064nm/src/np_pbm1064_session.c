#include "np_pbm1064.h"
#include "np_pbm1064_config.h"
#include <string.h>

/* HAL stubs (OI-PBM-03, OI-PBM-04) */
extern void np_pbm_safety_mcu_enable(uint8_t zone, bool en);
extern void np_delay_ms(uint32_t ms);
extern uint32_t np_tick_ms(void);

static void record_shdr(const np_pbm1064_session_t *session)
{
    /* SHDR: device health only — no user biology.
     * Platform team writes np_pbm1064_shdr_t to SHDR partition via
     * NP-FW-EMMC-001 Rev A §7 SHDR write API. */
    np_pbm1064_shdr_t shdr;
    memset(&shdr, 0, sizeof(shdr));

    for (uint8_t zone = 0u; zone < NP_PBM1064_ZONE_COUNT; zone++) {
        const np_pbm1064_zone_state_t *zs = &session->zone[zone];
        for (uint8_t wl = 0u; wl < NP_PBM1064_WAVELENGTH_COUNT; wl++) {
            shdr.pd1_pd2_ratio_final[zone][wl] = zs->pd1_pd2_ratio[wl];
        }
        shdr.fouling_flag[zone]      = zs->fouling_flag;
        shdr.aging_flag[zone]        = zs->aging_flag;
        shdr.driver_fault_count[zone] = zs->driver_fault_count;
        shdr.ntc_peak[zone]          = zs->ntc_c;   /* peak captured during session tick */
        shdr.cal_from_factory[zone]  = session->cal.cal_from_factory[zone];
    }

    /* Platform: np_shdr_write(SHDR_TYPE_PBM1064, &shdr, sizeof(shdr)) */
    (void)shdr;
}

bool np_pbm1064_session_preflight(np_pbm1064_session_t *session)
{
    bool pass = true;

    for (uint8_t slot = 0u; slot < NP_PBM1064_ZONE_COUNT; slot++) {
        if (!(session->smart_module_mask & (1u << slot))) continue;

        if (!session->zone[slot].driver_ready) {
            pass = false;
            continue;
        }

        uint8_t status = np_pbm1064_read_status(slot);
        if (!(status & NP_PBM1064_STATUS_READY) ||
             (status & (NP_PBM1064_STATUS_THERMAL_FAULT |
                        NP_PBM1064_STATUS_OCP_A |
                        NP_PBM1064_STATUS_OCP_B |
                        NP_PBM1064_STATUS_OCP_C))) {
            pass = false;
        }

        int8_t ntc = np_pbm1064_read_ntc(slot);
        session->zone[slot].ntc_c = ntc;
        if (ntc > NP_PBM1064_THERMAL_WARN_C) {
            /* Warn but do not block — session layer reports to app */
        }
    }

    np_pbm1064_load_cal(&session->cal);
    return pass;
}

void np_pbm1064_session_start(np_pbm1064_session_t *session)
{
    for (uint8_t slot = 0u; slot < NP_PBM1064_ZONE_COUNT; slot++) {
        if (!(session->smart_module_mask & (1u << slot))) continue;

        /* Safety MCU GPIO enable before any I2C channel enable */
        np_pbm_safety_mcu_enable(slot, true);

        np_pbm1064_drive_init(slot, &session->preset[slot]);

        /* Record PD baseline for aging detection */
        for (uint8_t wl = 0u; wl < NP_PBM1064_WAVELENGTH_COUNT; wl++) {
            session->zone[slot].baseline_pd1[wl] = session->zone[slot].irr_pd1[wl];
        }

        /* Enable all configured channels */
        uint8_t ch_mask = NP_PBM1064_CH_A | NP_PBM1064_CH_B | NP_PBM1064_CH_C;
        np_pbm1064_set_ch_enable(slot, ch_mask);
        session->zone[slot].active_ch_mask = ch_mask;
    }

    session->session_active = true;
    session->session_tick_ms = np_tick_ms();
}

void np_pbm1064_session_eeg_update(np_pbm1064_session_t *session, float eeg_freq_hz)
{
    session->eeg_target_freq_hz = eeg_freq_hz;

    for (uint8_t slot = 0u; slot < NP_PBM1064_ZONE_COUNT; slot++) {
        if (!(session->smart_module_mask & (1u << slot))) continue;

        const np_pbm1064_preset_t *preset = &session->preset[slot];

        if (preset->cortical_gradient_adapt) {
            /* Gradient: CH_A=eeg_freq, CH_B=eeg_freq/2, CH_C=eeg_freq/4
             * Experimental — labelled Research mode in app (OI-SES-02) */
            uint8_t fa = (uint8_t)(eeg_freq_hz + 0.5f);
            uint8_t fb = (uint8_t)(eeg_freq_hz / 2.0f + 0.5f);
            uint8_t fc = (uint8_t)(eeg_freq_hz / 4.0f + 0.5f);
            np_pbm1064_set_freq(slot, NP_PBM_CH_A, fa);
            np_pbm1064_set_freq(slot, NP_PBM_CH_B, (fb > 0u) ? fb : 1u);
            np_pbm1064_set_freq(slot, NP_PBM_CH_C, (fc > 0u) ? fc : 1u);
        } else {
            /* Uniform: all channels track EEG frequency */
            uint8_t f = (uint8_t)(eeg_freq_hz + 0.5f);
            np_pbm1064_set_freq(slot, NP_PBM_CH_A, f);
            np_pbm1064_set_freq(slot, NP_PBM_CH_B, f);
            np_pbm1064_set_freq(slot, NP_PBM_CH_C, f);
        }
    }
}

void np_pbm1064_session_tick(np_pbm1064_session_t *session)
{
    static uint32_t dose_accumulator_ms  = 0u;
    static uint32_t ntc_poll_ms          = 0u;
    static uint32_t status_poll_ms       = 0u;

    if (!session->session_active) return;

    uint32_t now = np_tick_ms();
    uint32_t elapsed = now - session->session_tick_ms;
    session->session_tick_ms = now;

    dose_accumulator_ms  += elapsed;
    ntc_poll_ms          += elapsed;
    status_poll_ms       += elapsed;

    /* 10 Hz dose tick */
    if (dose_accumulator_ms >= (1000u / NP_PBM1064_DOSE_TICK_HZ)) {
        np_pbm1064_dose_tick(session);
        np_pbm1064_thermal_check(session);
        dose_accumulator_ms = 0u;
    }

    /* 1 Hz NTC poll */
    if (ntc_poll_ms >= NP_PBM1064_NTC_POLL_INTERVAL_MS) {
        for (uint8_t slot = 0u; slot < NP_PBM1064_ZONE_COUNT; slot++) {
            if (!(session->smart_module_mask & (1u << slot))) continue;
            int8_t ntc = np_pbm1064_read_ntc(slot);
            if (ntc > session->zone[slot].ntc_c) {
                session->zone[slot].ntc_c = ntc;   /* track peak for SHDR */
            }
            if (ntc >= NP_PBM1064_THERMAL_THROTTLE_C) {
                /* Suspend CH_C for this zone until NTC drops */
                np_pbm1064_set_duty(slot, NP_PBM_CH_C, 0u);
            }
        }
        ntc_poll_ms = 0u;
    }

    /* 5 s I2C status poll */
    if (status_poll_ms >= NP_PBM1064_STATUS_POLL_INTERVAL_MS) {
        for (uint8_t slot = 0u; slot < NP_PBM1064_ZONE_COUNT; slot++) {
            if (!(session->smart_module_mask & (1u << slot))) continue;
            uint8_t status = np_pbm1064_read_status(slot);
            session->zone[slot].driver_status = status;
            if (status & (NP_PBM1064_STATUS_THERMAL_FAULT |
                          NP_PBM1064_STATUS_OCP_A |
                          NP_PBM1064_STATUS_OCP_B |
                          NP_PBM1064_STATUS_OCP_C)) {
                np_pbm1064_set_ch_enable(slot, 0x00u);
                session->zone[slot].active_ch_mask = 0u;
                session->zone[slot].driver_fault_count++;
                np_pbm_safety_mcu_enable(slot, false);
            }
        }
        status_poll_ms = 0u;
    }

    /* Disable channels that hit dose limit */
    for (uint8_t slot = 0u; slot < NP_PBM1064_ZONE_COUNT; slot++) {
        if (!(session->smart_module_mask & (1u << slot))) continue;
        np_pbm1064_zone_state_t *zs = &session->zone[slot];
        uint8_t new_mask = zs->active_ch_mask;
        if (zs->dose_limit_hit[NP_PBM_CH_A]) new_mask &= ~NP_PBM1064_CH_A;
        if (zs->dose_limit_hit[NP_PBM_CH_B]) new_mask &= ~NP_PBM1064_CH_B;
        if (zs->dose_limit_hit[NP_PBM_CH_C]) new_mask &= ~NP_PBM1064_CH_C;
        if (new_mask != zs->active_ch_mask) {
            np_pbm1064_set_ch_enable(slot, new_mask);
            zs->active_ch_mask = new_mask;
        }
    }
}

void np_pbm1064_session_stop(np_pbm1064_session_t *session)
{
    for (uint8_t slot = 0u; slot < NP_PBM1064_ZONE_COUNT; slot++) {
        if (!(session->smart_module_mask & (1u << slot))) continue;

        /* Ramp down: disable CH_C, then CH_B, then CH_A at intervals */
        np_pbm1064_set_duty(slot, NP_PBM_CH_C, 0u);
        np_delay_ms(NP_PBM1064_RAMP_DOWN_MS / 3u);
        np_pbm1064_set_duty(slot, NP_PBM_CH_B, 0u);
        np_delay_ms(NP_PBM1064_RAMP_DOWN_MS / 3u);
        np_pbm1064_set_duty(slot, NP_PBM_CH_A, 0u);
        np_delay_ms(NP_PBM1064_RAMP_DOWN_MS / 3u);

        np_pbm1064_set_ch_enable(slot, 0x00u);
        session->zone[slot].active_ch_mask = 0u;
        np_pbm_safety_mcu_enable(slot, false);
    }

    session->session_active = false;

    /* UHDR: dose_j_cm2 per zone per wavelength written by np_pbm1064_dose_tick;
     * platform finalises EDF+ record at session end */

    record_shdr(session);
}
