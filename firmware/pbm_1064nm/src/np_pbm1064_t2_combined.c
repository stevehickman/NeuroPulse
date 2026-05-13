#include "np_pbm1064.h"
#include "np_pbm1064_config.h"
#include <string.h>

/* T2 1170 nm laser API stubs (OI-PBM-07, OI-PBM-08) */
extern np_err_t np_t2_pbm1170_init(void *session, void *preset);
extern np_err_t np_t2_pbm1170_start(void *session);
extern np_err_t np_t2_pbm1170_set_duty(void *session, uint8_t duty_pct);
extern np_err_t np_t2_pbm1170_stop(void *session);
extern float    np_t2_pbm1170_get_dose_j_cm2(const void *session);
extern bool     np_t2_pbm1170_is_ready(void);

bool np_pbm1064_t2_init(np_pbm1064_t2_session_t *t2, const np_pbm1064_preset_t *zone_presets,
                         void *laser_preset)
{
    if (!np_t2_pbm1170_is_ready()) {
        return false;
    }

    memset(t2, 0, sizeof(*t2));

    for (uint8_t zone = 0u; zone < NP_PBM1064_ZONE_COUNT; zone++) {
        t2->zone_session.preset[zone] = zone_presets[zone];
    }

    np_err_t err = np_t2_pbm1170_init(t2->t2_laser_session, laser_preset);
    return (err == NP_OK);
}

void np_pbm1064_t2_tick(np_pbm1064_t2_session_t *t2)
{
    /* Tick zone session (1064 nm modules) */
    np_pbm1064_session_tick(&t2->zone_session);

    /* Thermal throttle coordination: check NTC across all zones.
     * If any zone NTC exceeds threshold, throttle 1170 nm laser first (§4.3 NP-SES-1064-001). */
    for (uint8_t slot = 0u; slot < NP_PBM1064_ZONE_COUNT; slot++) {
        if (!(t2->zone_session.smart_module_mask & (1u << slot))) continue;

        int8_t ntc = t2->zone_session.zone[slot].ntc_c;

        if (ntc >= NP_PBM1064_THERMAL_THROTTLE_C && !t2->laser_throttled) {
            /* Throttle 1170 nm laser to 50% duty */
            np_t2_pbm1170_set_duty(t2->t2_laser_session, 10u);
            t2->laser_throttled = true;
        } else if (ntc < NP_PBM1064_THERMAL_WARN_C && t2->laser_throttled) {
            /* Restore laser when thermal headroom recovered */
            np_t2_pbm1170_set_duty(t2->t2_laser_session, 20u);
            t2->laser_throttled = false;
        }
    }

    /* Update combined dose from laser */
    t2->dose_1170_j_cm2 = np_t2_pbm1170_get_dose_j_cm2(t2->t2_laser_session);
}

void np_pbm1064_t2_stop(np_pbm1064_t2_session_t *t2)
{
    /* Stop 1170 nm laser first (60 s ramp-down, longer than 1064 nm 30 s ramp) */
    np_t2_pbm1170_stop(t2->t2_laser_session);

    /* Stop 1064 nm zone session */
    np_pbm1064_session_stop(&t2->zone_session);

    /* UHDR: combined dose record — platform writes both 1064 and 1170 J/cm² totals.
     * 1170 nm dose is user health data (UHDR) — tissue-delivered dose. */

    /* SHDR: combined session count increment (no timestamps, no biology) */
}
