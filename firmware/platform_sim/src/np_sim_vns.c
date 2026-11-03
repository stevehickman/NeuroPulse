/*
 * NeurOne Firmware-in-the-Loop Simulator — auricular VNS seams (NP-SIM-FW-001 §8 step 3, REQ-SIMFW-13)
 * HOST ONLY; see np_sim_bus.h.
 *
 * Defines the nine np_mod_vns_hal_* seams of np_sw02_platform_hal.h for the ear clip (np_mod_vns_hal_now_ms is the
 * simulator's clock, not a module).  np_mod_vns.c and the HRV session library it drives are the device's own sources.
 *
 *   contact impedance   the value the scenario gave the clip.  No default.  A clip the scenario removed, or a read
 *                       that fails, answers 0 ohm: below the driver's lower contact bound, so the driver refuses
 *                       to enable.  The seam returns a float and carries no status, and a model that answered
 *                       with something inside the bounds would be saying contact exists.
 *   stimulator          what the driver last told it, per side (0 left, 1 right, 2 both).  The clip drives only
 *                       when the amplitude is non-zero.
 *   HRV readouts        RMSSD, coherence and heart rate: the values the scenario gave the clip.  No default and
 *                       no derivation from a PPG waveform (gap, NP-SIM-FW-001 §15).
 */
#include "np_sim_bus.h"

#include "np_sw02_platform_hal.h"

#include <stdio.h>

static np_sim_module_t *resolve(const char *op_name)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_VNS, 0U);
    if (m == NULL || !m->present) {
        np_sim_report_absent(NP_SIM_KIND_VNS, 0U, op_name);
        return NULL;
    }
    return m;
}

/* A float readout: pinned if pinned, else the module's value; an injected failure or an absent clip answers `fail`. */
static float read_float(np_sim_op_t op, const char *op_name, float model, float fail)
{
    np_sim_module_t *m = resolve(op_name);
    if (m == NULL) { return fail; }
    int code;
    if (np_sim_take_response(m, op, &code)) {
        np_sim_report_hal(m, op, "[]", code, "injected");
        return fail;
    }
    const np_sim_override_t *o = &m->ov[op];
    const float v = o->has_reading ? o->reading : model;
    char args[32];
    (void)snprintf(args, sizeof args, "[%.2f]", (double)v);
    np_sim_report_hal(m, op, args, (int)NP_HUB_OK, o->has_reading ? "injected" : "default");
    return v;
}

float np_mod_vns_hal_impedance_check(void)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_VNS, 0U);
    return read_float(NP_SIM_OP_VNS_IMPEDANCE, "vns_impedance", m != NULL ? m->vns_impedance_ohm : 0.0f, 0.0f);
}
float np_mod_vns_hal_get_rmssd(void)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_VNS, 0U);
    return read_float(NP_SIM_OP_VNS_RMSSD, "vns_rmssd", m != NULL ? m->vns_rmssd_ms : 0.0f, 0.0f);
}
float np_mod_vns_hal_get_coherence(void)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_VNS, 0U);
    return read_float(NP_SIM_OP_VNS_COHERENCE, "vns_coherence", m != NULL ? m->vns_coherence : 0.0f, 0.0f);
}
float np_mod_vns_hal_get_hr_bpm(void)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_VNS, 0U);
    return read_float(NP_SIM_OP_VNS_HR, "vns_hr", m != NULL ? m->vns_hr_bpm : 0.0f, 0.0f);
}

uint32_t np_mod_vns_hal_now_ms(void) { return np_sim_now_ms(); }

np_hub_status_t np_mod_vns_hal_stim_set(uint8_t side, uint32_t freq_mhz, uint16_t amp_ua, uint16_t pw_us)
{
    np_sim_module_t *m = resolve("vns_stim_set");
    if (m == NULL) { return NP_HUB_ERR_NOT_PRESENT; }
    char args[64];
    (void)snprintf(args, sizeof args, "[%u,%u,%u,%u]", side, (unsigned)freq_mhz, amp_ua, pw_us);
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_VNS_STIM_SET, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_VNS_STIM_SET, args, code, "injected");
        return (np_hub_status_t)code;
    }
    m->vns_side_on[0] = (side == 0U || side == 2U);
    m->vns_side_on[1] = (side == 1U || side == 2U);
    m->vns_freq_mhz = freq_mhz; m->vns_amp_ua = amp_ua; m->vns_pw_us = pw_us;
    m->driven = (amp_ua != 0U) && (m->vns_side_on[0] || m->vns_side_on[1]);
    np_sim_report_hal(m, NP_SIM_OP_VNS_STIM_SET, args, (int)NP_HUB_OK, "default");
    return NP_HUB_OK;
}

np_hub_status_t np_mod_vns_hal_stim_stop(uint8_t side)
{
    np_sim_module_t *m = resolve("vns_stim_stop");
    if (m == NULL) { return NP_HUB_ERR_NOT_PRESENT; }
    char args[16];
    (void)snprintf(args, sizeof args, "[%u]", side);
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_VNS_STIM_STOP, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_VNS_STIM_STOP, args, code, "injected");
        return (np_hub_status_t)code;    /* the stimulator keeps driving */
    }
    if (side == 0U || side == 2U) { m->vns_side_on[0] = false; }
    if (side == 1U || side == 2U) { m->vns_side_on[1] = false; }
    m->driven = m->vns_amp_ua != 0U && (m->vns_side_on[0] || m->vns_side_on[1]);
    np_sim_report_hal(m, NP_SIM_OP_VNS_STIM_STOP, args, (int)NP_HUB_OK, "default");
    return NP_HUB_OK;
}

static void flag_op(np_sim_op_t op, const char *name, bool *field, bool value, bool is_ref)
{
    np_sim_module_t *m = resolve(name);
    if (m == NULL) { return; }
    char args[16];
    (void)snprintf(args, sizeof args, is_ref ? "[%s]" : "[]", value ? "true" : "false");
    int code;
    if (np_sim_take_response(m, op, &code)) {
        np_sim_report_hal(m, op, args, code, "injected");
        return;                          /* the call did not happen */
    }
    *field = value;
    np_sim_report_hal(m, op, args, (int)NP_HUB_OK, "default");
}

void np_mod_vns_hal_ppg_start(void)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_VNS, 0U);
    bool dummy = false;
    flag_op(NP_SIM_OP_VNS_PPG_START, "vns_ppg_start", m != NULL ? &m->vns_ppg_on : &dummy, true, false);
}
void np_mod_vns_hal_ppg_stop(void)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_VNS, 0U);
    bool dummy = false;
    flag_op(NP_SIM_OP_VNS_PPG_STOP, "vns_ppg_stop", m != NULL ? &m->vns_ppg_on : &dummy, false, false);
}
void np_mod_vns_hal_eeg_ref_route(bool enable)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_VNS, 0U);
    bool dummy = false;
    flag_op(NP_SIM_OP_VNS_EEG_REF, "vns_eeg_ref", m != NULL ? &m->vns_eeg_ref : &dummy, enable, true);
}
