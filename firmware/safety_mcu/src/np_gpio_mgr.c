/*
 * NeurOne Safety MCU — SW01-M01: Stimulation GPIO Manager
 * Document: NP-SW-001 Rev A, NP-FMEA-001 Rev A §SW01-M01
 *
 * Owns all 14 stimulation enable GPIO lines.  All lines are active-LOW
 * open-drain with external pull-ups to Vcc.  Asserting a GPIO LOW enables
 * the downstream stimulation driver via an inverting buffer.
 *
 * This is the ONLY module that writes stimulation GPIO.  All other modules
 * communicate intent via the safety state's granted_mask.
 */

#include "np_safety_config.h"
#include "np_safety_protocol.h"
#include <stdint.h>

/* ── HAL stub (FMEA OI-FMEA-01: bench test required) ─────────────────────── */
extern void np_hal_gpio_write_pin(void *port, uint16_t pin, int state);

np_safe_status_t np_gpio_mgr_init(void)
{
    /* All stimulation enables driven HIGH (disabled) at init.
     * np_hal_gpio_init() configures direction; this call sets initial state. */
    np_hal_gpio_write_pin(NP_EN_PBM_ZONE0_PORT, NP_EN_PBM_ZONE0_PIN, 1);
    np_hal_gpio_write_pin(NP_EN_PBM_ZONE1_PORT, NP_EN_PBM_ZONE1_PIN, 1);
    np_hal_gpio_write_pin(NP_EN_PBM_ZONE2_PORT, NP_EN_PBM_ZONE2_PIN, 1);
    np_hal_gpio_write_pin(NP_EN_PBM_ZONE3_PORT, NP_EN_PBM_ZONE3_PIN, 1);
    np_hal_gpio_write_pin(NP_EN_PBM_ZONE4_PORT, NP_EN_PBM_ZONE4_PIN, 1);
    np_hal_gpio_write_pin(NP_EN_BES_PORT,        NP_EN_BES_PIN,        1);
    np_hal_gpio_write_pin(NP_EN_TDCS_PORT,       NP_EN_TDCS_PIN,       1);
    np_hal_gpio_write_pin(NP_EN_VNS_PORT,        NP_EN_VNS_PIN,        1);
    np_hal_gpio_write_pin(NP_EN_VISUAL_PORT,     NP_EN_VISUAL_PIN,     1);
    np_hal_gpio_write_pin(NP_EN_INTRANASAL_PORT, NP_EN_INTRANASAL_PIN, 1);
    np_hal_gpio_write_pin(NP_EN_CVNS_PORT,       NP_EN_CVNS_PIN,       1);
    np_hal_gpio_write_pin(NP_EN_TMS_PORT,        NP_EN_TMS_PIN,        1);
    np_hal_gpio_write_pin(NP_EN_PBM_1170_PORT,   NP_EN_PBM_1170_PIN,   1);
    np_hal_gpio_write_pin(NP_EN_CLIN_STIM_PORT,  NP_EN_CLIN_STIM_PIN,  1);
    return NP_SAFE_OK;
}

/*
 * np_gpio_mgr_apply — write the granted_mask from safety state to GPIO.
 * Called once per main-loop iteration after all interlock ticks.
 * Cutoff (<50ms spec): clearing granted_mask and calling this function
 * completes in a single SysTick period (1ms).
 */
void np_gpio_mgr_apply(const np_safety_state_t *state)
{
    uint16_t m = state->granted_mask;

    /* Any fault clears the full granted mask before this call */
    np_hal_gpio_write_pin(NP_EN_PBM_ZONE0_PORT, NP_EN_PBM_ZONE0_PIN,
                          (m & NP_SAFETY_EN_PBM_ZONE_0) ? 0 : 1);
    np_hal_gpio_write_pin(NP_EN_PBM_ZONE1_PORT, NP_EN_PBM_ZONE1_PIN,
                          (m & NP_SAFETY_EN_PBM_ZONE_1) ? 0 : 1);
    np_hal_gpio_write_pin(NP_EN_PBM_ZONE2_PORT, NP_EN_PBM_ZONE2_PIN,
                          (m & NP_SAFETY_EN_PBM_ZONE_2) ? 0 : 1);
    np_hal_gpio_write_pin(NP_EN_PBM_ZONE3_PORT, NP_EN_PBM_ZONE3_PIN,
                          (m & NP_SAFETY_EN_PBM_ZONE_3) ? 0 : 1);
    np_hal_gpio_write_pin(NP_EN_PBM_ZONE4_PORT, NP_EN_PBM_ZONE4_PIN,
                          (m & NP_SAFETY_EN_PBM_ZONE_4) ? 0 : 1);
    np_hal_gpio_write_pin(NP_EN_BES_PORT,        NP_EN_BES_PIN,
                          (m & NP_SAFETY_EN_BES_TACS)    ? 0 : 1);
    np_hal_gpio_write_pin(NP_EN_TDCS_PORT,       NP_EN_TDCS_PIN,
                          (m & NP_SAFETY_EN_TDCS)        ? 0 : 1);
    np_hal_gpio_write_pin(NP_EN_VNS_PORT,        NP_EN_VNS_PIN,
                          (m & NP_SAFETY_EN_VNS_HRV)     ? 0 : 1);
    np_hal_gpio_write_pin(NP_EN_VISUAL_PORT,     NP_EN_VISUAL_PIN,
                          (m & NP_SAFETY_EN_VISUAL)       ? 0 : 1);
    np_hal_gpio_write_pin(NP_EN_INTRANASAL_PORT, NP_EN_INTRANASAL_PIN,
                          (m & NP_SAFETY_EN_INTRANASAL)   ? 0 : 1);
    np_hal_gpio_write_pin(NP_EN_CVNS_PORT,       NP_EN_CVNS_PIN,
                          (m & NP_SAFETY_EN_CVNS)         ? 0 : 1);
    np_hal_gpio_write_pin(NP_EN_TMS_PORT,        NP_EN_TMS_PIN,
                          (m & NP_SAFETY_EN_TMS)          ? 0 : 1);
    np_hal_gpio_write_pin(NP_EN_PBM_1170_PORT,   NP_EN_PBM_1170_PIN,
                          (m & NP_SAFETY_EN_PBM_1170NM)   ? 0 : 1);
    np_hal_gpio_write_pin(NP_EN_CLIN_STIM_PORT,  NP_EN_CLIN_STIM_PIN,
                          (m & NP_SAFETY_EN_CLIN_STIM)    ? 0 : 1);
}
