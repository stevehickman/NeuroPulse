/*
 * NeurOne Firmware-in-the-Loop Simulator — stimulation seams (NP-SIM-FW-001 §8 step 3, REQ-SIMFW-13)
 * HOST ONLY; see np_sim_bus.h.
 *
 * Defines the seven np_mod_stim_hal_* seams of np_sw02_platform_hal.h for an electrode pair of the
 * stimulation DAC.  np_mod_stim.c, np_stim_xcheck.c, np_ramp_plan.c and np_safety_spi.c are the device's
 * own sources; this file is only the silicon they talk to.
 *
 * The default model is the least that answers honestly, and it states what it does not know:
 *
 *   delivered current  AC: the commanded peak, the same quantity np_stim_xcheck compares (np_mod_stim.c,
 *                          OI-STIM-06).  DC: a straight line from the level when the ramp was commanded to
 *                          its target over the commanded seconds.  Every call site of np_mod_stim_hal_dc_ramp
 *                          passes the time to cover the distance from the present level, so that is how the
 *                          model reads it.
 *   charge             DC: the integral of the delivered magnitude, in microcoulombs, returned in the
 *                          32 bits the seam carries (OI-UINT16-01).  AC: 0.  The seam's comment says "session total" and does
 *                          not say what a charge-balanced waveform's total is; the model does not invent one.
 *   polarity           reported as a magnitude.  The seam does not define what polarity 0 and 1 mean.
 *   impedance          the value the scenario gave the pair.  No default.
 *
 * A pair the scenario never added answers NOT_PRESENT for a command, and a non-finite reading (NaN) for a
 * read: np_stim_xcheck counts a non-finite reading against the bound, so an absent DAC is not read as safe.
 */
#include "np_sim_bus.h"

#include "np_sw02_platform_hal.h"

#include <math.h>
#include <stdio.h>

static np_sim_module_t *resolve(uint8_t pair, const char *op_name)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_STIM_PAIR, pair);
    if (m == NULL || !m->present) {
        np_sim_report_absent(NP_SIM_KIND_STIM_PAIR, pair, op_name);
        return NULL;
    }
    return m;
}

/* The DC level (microamps) at `at_ms` for the segment the module holds. */
static double dc_level_at(const np_sim_module_t *m, uint32_t at_ms)
{
    const double from = m->stim_dc_from_ua, to = m->stim_dc_to_ua;
    const double span_ms = (double)m->stim_ramp_s * 1000.0;
    const double dt = (double)(uint32_t)(at_ms - m->stim_seg_start_ms);
    if (span_ms <= 0.0 || dt >= span_ms) { return to; }
    return from + (to - from) * (dt / span_ms);
}

static double delivered_ua(const np_sim_module_t *m)
{
    return m->stim_dc ? dc_level_at(m, np_sim_now_ms()) : (double)m->stim_amp_ua;
}

/* Bring the DC charge integral up to now.  The level is linear inside the segment and constant after it, so the
 * integral is exact: two trapezoids. */
static void charge_update(np_sim_module_t *m)
{
    const uint32_t now = np_sim_now_ms();
    if (!m->stim_dc) { m->stim_charge_mark_ms = now; return; }
    const uint32_t seg_end = m->stim_seg_start_ms + (uint32_t)m->stim_ramp_s * 1000U;
    uint32_t t0 = m->stim_charge_mark_ms;
    if (t0 < m->stim_seg_start_ms) { t0 = m->stim_seg_start_ms; }
    double uc = 0.0;
    uint32_t t1 = now;
    if (t1 > seg_end) { t1 = seg_end; }
    if (t1 > t0) {
        uc += 0.5 * (dc_level_at(m, t0) + dc_level_at(m, t1)) * ((double)(t1 - t0) / 1000.0);
    }
    const uint32_t flat0 = (t0 > seg_end) ? t0 : seg_end;
    if (now > flat0) {
        uc += (double)m->stim_dc_to_ua * ((double)(now - flat0) / 1000.0);
    }
    m->stim_charge_uc += uc;
    m->stim_charge_mark_ms = now;
}

np_hub_status_t np_mod_stim_hal_dac_set(uint8_t pair, uint8_t waveform, uint16_t freq_mhz, uint16_t amp_ua)
{
    np_sim_module_t *m = resolve(pair, "stim_dac_set");
    if (m == NULL) { return NP_HUB_ERR_NOT_PRESENT; }

    char args[96];
    (void)snprintf(args, sizeof args, "[%u,%u,%u]", waveform, freq_mhz, amp_ua);
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_STIM_DAC_SET, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_STIM_DAC_SET, args, code, "injected");
        return (np_hub_status_t)code;   /* a failed write leaves the DAC as it was */
    }
    charge_update(m);
    m->stim_dc = false;
    m->stim_waveform = waveform; m->stim_freq_mhz = freq_mhz; m->stim_amp_ua = amp_ua;
    m->driven = (amp_ua != 0U);
    np_sim_report_hal(m, NP_SIM_OP_STIM_DAC_SET, args, (int)NP_HUB_OK, "default");
    return NP_HUB_OK;
}

np_hub_status_t np_mod_stim_hal_dac_stop(uint8_t pair)
{
    np_sim_module_t *m = resolve(pair, "stim_dac_stop");
    if (m == NULL) { return NP_HUB_ERR_NOT_PRESENT; }
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_STIM_DAC_STOP, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_STIM_DAC_STOP, "[]", code, "injected");
        return (np_hub_status_t)code;   /* the DAC keeps driving: the model's commanded state is unchanged */
    }
    charge_update(m);
    m->stim_amp_ua = 0U;
    m->driven = false;
    np_sim_report_hal(m, NP_SIM_OP_STIM_DAC_STOP, "[]", (int)NP_HUB_OK, "default");
    return NP_HUB_OK;
}

np_hub_status_t np_mod_stim_hal_dc_set(uint8_t pair, uint16_t current_ua, uint8_t polarity)
{
    np_sim_module_t *m = resolve(pair, "stim_dc_set");
    if (m == NULL) { return NP_HUB_ERR_NOT_PRESENT; }
    char args[48];
    (void)snprintf(args, sizeof args, "[%u,%u]", current_ua, polarity);
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_STIM_DC_SET, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_STIM_DC_SET, args, code, "injected");
        return (np_hub_status_t)code;
    }
    charge_update(m);
    m->stim_dc = true; m->stim_polarity = polarity;
    m->stim_dc_from_ua = current_ua; m->stim_dc_to_ua = current_ua;
    m->stim_ramp_s = 0U; m->stim_seg_start_ms = np_sim_now_ms();
    m->driven = (current_ua != 0U);
    np_sim_report_hal(m, NP_SIM_OP_STIM_DC_SET, args, (int)NP_HUB_OK, "default");
    return NP_HUB_OK;
}

np_hub_status_t np_mod_stim_hal_dc_ramp(uint8_t pair, uint16_t target_ua, uint8_t polarity, uint16_t ramp_s)
{
    np_sim_module_t *m = resolve(pair, "stim_dc_ramp");
    if (m == NULL) { return NP_HUB_ERR_NOT_PRESENT; }
    char args[64];
    (void)snprintf(args, sizeof args, "[%u,%u,%u]", target_ua, polarity, ramp_s);
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_STIM_DC_RAMP, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_STIM_DC_RAMP, args, code, "injected");
        return (np_hub_status_t)code;   /* a failed ramp command leaves the present segment running */
    }
    charge_update(m);
    const uint16_t here = m->stim_dc ? (uint16_t)(dc_level_at(m, np_sim_now_ms()) + 0.5) : 0U;
    m->stim_dc = true; m->stim_polarity = polarity;
    m->stim_dc_from_ua = here; m->stim_dc_to_ua = target_ua;
    m->stim_ramp_s = ramp_s; m->stim_seg_start_ms = np_sim_now_ms();
    m->driven = (here != 0U || target_ua != 0U);
    np_sim_report_hal(m, NP_SIM_OP_STIM_DC_RAMP, args, (int)NP_HUB_OK, "default");
    return NP_HUB_OK;
}

float np_mod_stim_hal_read_impedance(uint8_t pair)
{
    np_sim_module_t *m = resolve(pair, "stim_read_impedance");
    if (m == NULL) { return NAN; }
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_STIM_READ_IMPEDANCE, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_STIM_READ_IMPEDANCE, "[]", code, "injected");
        return NAN;                      /* the seam returns a float: a failed read is a non-finite one */
    }
    const np_sim_override_t *o = &m->ov[NP_SIM_OP_STIM_READ_IMPEDANCE];
    const float v = o->has_reading ? o->reading : m->impedance_kohm;
    char args[32];
    (void)snprintf(args, sizeof args, "[%.2f]", (double)v);
    np_sim_report_hal(m, NP_SIM_OP_STIM_READ_IMPEDANCE, args, (int)NP_HUB_OK, o->has_reading ? "injected" : "default");
    return v;
}

float np_mod_stim_hal_read_current(uint8_t pair)
{
    np_sim_module_t *m = resolve(pair, "stim_read_current");
    if (m == NULL) { return NAN; }
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_STIM_READ_CURRENT, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_STIM_READ_CURRENT, "[]", code, "injected");
        return NAN;
    }
    const np_sim_override_t *o = &m->ov[NP_SIM_OP_STIM_READ_CURRENT];
    const float v = o->has_reading ? o->reading : (float)delivered_ua(m);
    char args[32];
    (void)snprintf(args, sizeof args, "[%.1f]", (double)v);
    np_sim_report_hal(m, NP_SIM_OP_STIM_READ_CURRENT, args, (int)NP_HUB_OK, o->has_reading ? "injected" : "default");
    return v;
}

uint32_t np_mod_stim_hal_read_charge(uint8_t pair)
{
    np_sim_module_t *m = resolve(pair, "stim_read_charge");
    if (m == NULL) { return 0U; }
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_STIM_READ_CHARGE, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_STIM_READ_CHARGE, "[]", code, "injected");
        return 0U;
    }
    charge_update(m);
    const np_sim_override_t *o = &m->ov[NP_SIM_OP_STIM_READ_CHARGE];
    double uc = o->has_reading ? (double)o->reading : m->stim_charge_uc;
    if (uc < 0.0)     { uc = 0.0; }
    /* A double above UINT32_MAX would make the cast undefined.  4.29e9 uC is 4,295 C, which no session reaches
     * (the 150 mC/cm2 ceiling on a 25 cm2 pad is 3.75e6 uC), so the bound is never taken by a modelled value. */
    const uint32_t uc32 = (uc >= 4294967295.0) ? UINT32_MAX : (uint32_t)uc;
    char args[32];
    (void)snprintf(args, sizeof args, "[%u]", (unsigned)uc32);
    np_sim_report_hal(m, NP_SIM_OP_STIM_READ_CHARGE, args, (int)NP_HUB_OK, o->has_reading ? "injected" : "default");
    return uc32;
}
