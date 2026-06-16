/*
 * NeuroPulse Hub Control — Visual Stimulation Goggles Driver
 * Document: NP-FW-HUB-001 Rev A §8.6
 *
 * Controls 108 micro-LEDs per lens (660nm + 808-830nm), 6 zones per eye.
 * Safety interlocks (all three layers independently enforced):
 *   1. IR proximity sensor: eye-open detection before enable
 *   2. Hall sensor: goggle lift → instant LED cutoff (GPIO interrupt, not polled)
 *   3. Hardware current limit: IEC 62471 MPE ceiling; hardware-enforced by safety MCU
 *   4. ADS1299 Oz channel: photoparoxysmal EEG detection → halt <200ms
 *
 * Mode F (NIR retinal walk): 808-830nm at ≤ MPE during normal-looking wear.
 * EMDR: alternating L/R LED activation at emdr_rate_mhz.
 *
 * HAL stubs:
 *   OI-VIS-01: np_mod_visual_hal_led_set(zone_mask, wl, freq_hz, duty_pct)
 *   OI-VIS-02: np_mod_visual_hal_led_stop()
 *   OI-VIS-03: np_mod_visual_hal_ir_eye_open() → bool
 *   OI-VIS-04: np_mod_visual_hal_hall_lifted() → bool  (goggles off head)
 *   OI-VIS-05: np_mod_visual_hal_emdr_set(rate_mhz) — L/R alternation
 *   OI-VIS-06: np_mod_visual_hal_mpe_check() → bool (false = MPE exceeded)
 */

#include "np_hub_config.h"
#include "np_hub_types.h"
#include "np_module_registry.h"
#include "np_safety_spi.h"
#include "np_session_runner.h"
#include <string.h>

extern np_hub_status_t np_mod_visual_hal_led_set(uint16_t zone_mask, uint8_t wl_sel,
                                                   uint8_t freq_hz, uint8_t duty_pct);
extern void np_mod_visual_hal_led_stop(void);
extern bool np_mod_visual_hal_ir_eye_open(void);
extern bool np_mod_visual_hal_hall_lifted(void);
extern void np_mod_visual_hal_emdr_set(uint8_t rate_mhz);
extern bool np_mod_visual_hal_mpe_check(void);

/* ── State ───────────────────────────────────────────────────────────────────── */

typedef struct {
    bool    active;
    bool    ppx_halt;    /* photoparoxysmal halt — set by EEG interrupt */
    uint8_t last_mode;
} np_mod_visual_state_t;

static np_mod_visual_state_t s_state;

/* Called from EEG ISR / task when photoparoxysmal pattern detected at Oz. */
void np_mod_visual_ppx_halt(void)
{
    s_state.ppx_halt = true;
    np_mod_visual_hal_led_stop();
    np_safety_spi_request_disable(NP_SAFETY_EN_VISUAL);
    np_runner_abort(NP_ABORT_EEG_SEIZURE);
}

/* ── Detect ──────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_visual_detect(uint8_t slot, np_hub_mod_type_t *type_out)
{
    (void)slot;
    /* Goggles detected by Hall sensor: magnet present → goggles seated. */
    if (!np_mod_visual_hal_hall_lifted()) {
        *type_out = NP_MOD_VISUAL;
        return NP_HUB_OK;
    }
    return NP_HUB_ERR_NOT_PRESENT;
}

/* ── Init ────────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_visual_init(uint8_t slot)
{
    (void)slot;
    memset(&s_state, 0, sizeof(s_state));
    np_mod_visual_hal_led_stop();
    return NP_HUB_OK;
}

/* ── Control ─────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_visual_control(uint8_t slot, const void *params, uint16_t len)
{
    (void)slot;

    /* Stop */
    if (params == NULL || len == 0U) {
        np_mod_visual_hal_led_stop();
        s_state.active = false;
        np_safety_spi_request_disable(NP_SAFETY_EN_VISUAL);
        return NP_HUB_OK;
    }

    /* Refuse if PPX halt is active */
    if (s_state.ppx_halt) {
        return NP_HUB_ERR_SAFETY_REJECTED;
    }

    if (len < sizeof(np_mod_visual_params_t)) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    const np_mod_visual_params_t *p = (const np_mod_visual_params_t *)params;

    /* Safety check 1: Hall sensor — goggles must be on head */
    if (np_mod_visual_hal_hall_lifted()) {
        return NP_HUB_ERR_SAFETY_REJECTED;
    }

    /* Safety check 2: Eye open (IR proximity) required for retinal modes.
     * Mode F path is guarded by NP_MODE_F_REGULATORY_CLEARED — if Mode F is
     * not cleared, mode_f_enable is ignored here and in the activation block below. */
#if NP_MODE_F_REGULATORY_CLEARED
    if ((p->mode == 2U || p->mode_f_enable) && !np_mod_visual_hal_ir_eye_open()) {
#else
    if (p->mode == 2U && !np_mod_visual_hal_ir_eye_open()) {
#endif
        return NP_HUB_ERR_SAFETY_REJECTED;
    }

    /* Safety check 3: IEC 62471 MPE check */
    if (!np_mod_visual_hal_mpe_check()) {
        return NP_HUB_ERR_SAFETY_REJECTED;
    }

    uint16_t zone_mask = (uint16_t)((uint16_t)p->zone_mask_lo |
                                    ((uint16_t)(p->zone_mask_hi & 0x0FU) << 8));

#if NP_MODE_F_REGULATORY_CLEARED
    /* Mode F: NIR retinal PBM — gated on RISK-03 Q-13 regulatory opinion.
     * 808-830nm at ≤ MPE, runs concurrently with photic; wl=1 (808nm) only. */
    if (p->mode_f_enable) {
        (void)np_mod_visual_hal_led_set(zone_mask, 1U, 0U, 10U); /* CW at 10% */
    }
#endif

    switch (p->mode) {
    case 0: /* photic driving */
        np_mod_visual_hal_led_set(zone_mask, p->wl_select, p->freq_hz, p->duty_pct);
        break;
    case 1: /* EMDR */
        np_mod_visual_hal_emdr_set(p->emdr_rate_mhz);
        break;
    case 2: /* retinal PBM — 808-830nm dedicated */
        np_mod_visual_hal_led_set(zone_mask, 1U, 0U, p->duty_pct);
        break;
    case 3: /* combined photic + EMDR */
        np_mod_visual_hal_led_set(zone_mask, p->wl_select, p->freq_hz, p->duty_pct);
        np_mod_visual_hal_emdr_set(p->emdr_rate_mhz);
        break;
    default:
        np_mod_visual_hal_led_stop();
        break;
    }

    s_state.active    = true;
    s_state.last_mode = p->mode;
    np_safety_spi_request_enable(NP_SAFETY_EN_VISUAL);

    return NP_HUB_OK;
}

/* ── Telemetry ───────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_visual_telemetry(uint8_t slot, np_telem_record_t *out)
{
    (void)slot;
    if (out == NULL) { return NP_HUB_ERR_INVALID_ARG; }

    out->mod_type = NP_MOD_VISUAL;
    out->slot     = NP_HUB_SLOT_VISUAL;

    np_telem_visual_t *v = &out->data.visual;
    v->active_mode = s_state.last_mode;                    /* SHDR */
    v->hall_state  = np_mod_visual_hal_hall_lifted() ? 1U : 0U; /* SHDR */
    v->eye_open    = np_mod_visual_hal_ir_eye_open();      /* UHDR */
    v->ppx_halt    = s_state.ppx_halt;                     /* SHDR */

    /* Hall lifted during active session: cut immediately */
    if (s_state.active && v->hall_state) {
        np_mod_visual_hal_led_stop();
        np_safety_spi_request_disable(NP_SAFETY_EN_VISUAL);
        s_state.active = false;
    }

    return NP_HUB_OK;
}

/* ── Shutdown ────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_visual_shutdown(uint8_t slot)
{
    return np_mod_visual_control(slot, NULL, 0U);
}
