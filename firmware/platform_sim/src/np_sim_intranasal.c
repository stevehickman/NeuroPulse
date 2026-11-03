/*
 * NeurOne Firmware-in-the-Loop Simulator — intranasal Y-probe seams (NP-SIM-FW-001 §8 step 3, REQ-SIMFW-13)
 * HOST ONLY; see np_sim_bus.h.
 *
 * Defines the five np_mod_ins_hal_* seams of np_sw02_platform_hal.h.  np_mod_intranasal.c is the device's own source.
 *
 *   sensors    sleeve authentication and photodiode contact are what the scenario sets.  Both boolean seams answer
 *              FALSE for an injected failure or an absent probe: false refuses.
 *   emitters   what the driver last commanded per side (0 left, 1 right): currents, frequency code, duty.
 *   dose ADC   the counts the scenario gave each side and wavelength.  No default, and no derivation from emission:
 *              the model does not turn an emitter setting into a photodiode reading.
 */
#include "np_sim_bus.h"

#include "np_sw02_platform_hal.h"

#include <stdio.h>

static np_sim_module_t *resolve(const char *op_name)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_INTRANASAL, 0U);
    if (m == NULL || !m->present) {
        np_sim_report_absent(NP_SIM_KIND_INTRANASAL, 0U, op_name);
        return NULL;
    }
    return m;
}

static bool read_bool(np_sim_op_t op, const char *op_name, bool (*model)(const np_sim_module_t *))
{
    np_sim_module_t *m = resolve(op_name);
    if (m == NULL) { return false; }
    int code;
    if (np_sim_take_response(m, op, &code)) {
        np_sim_report_hal(m, op, "[]", code, "injected");
        return false;
    }
    const np_sim_override_t *o = &m->ov[op];
    const bool v = o->has_reading ? (o->reading != 0.0f) : model(m);
    char args[16];
    (void)snprintf(args, sizeof args, "[%s]", v ? "true" : "false");
    np_sim_report_hal(m, op, args, (int)NP_HUB_OK, o->has_reading ? "injected" : "default");
    return v;
}

static bool model_auth(const np_sim_module_t *m)    { return m->ins_auth; }
static bool model_contact(const np_sim_module_t *m) { return m->ins_contact; }

bool np_mod_ins_hal_auth_check(void) { return read_bool(NP_SIM_OP_INS_AUTH, "ins_auth", model_auth); }
bool np_mod_ins_hal_pd_contact(void) { return read_bool(NP_SIM_OP_INS_CONTACT, "ins_contact", model_contact); }

np_hub_status_t np_mod_ins_hal_pwm_set(uint8_t side, uint8_t cur_660, uint8_t cur_808, uint8_t freq_code, uint8_t duty)
{
    np_sim_module_t *m = resolve("ins_pwm_set");
    if (m == NULL) { return NP_HUB_ERR_NOT_PRESENT; }
    char args[48];
    (void)snprintf(args, sizeof args, "[%u,%u,%u,%u,%u]", side, cur_660, cur_808, freq_code, duty);
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_INS_PWM_SET, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_INS_PWM_SET, args, code, "injected");
        return (np_hub_status_t)code;   /* a failed write leaves that side as it was */
    }
    if (side > 1U) { return NP_HUB_ERR_INVALID_ARG; }
    m->ins_cur660[side] = cur_660; m->ins_cur808[side] = cur_808; m->ins_freq[side] = freq_code; m->ins_duty[side] = duty;
    m->ins_side_on[side] = (duty != 0U && (cur_660 != 0U || cur_808 != 0U));
    m->driven = m->ins_side_on[0] || m->ins_side_on[1];
    np_sim_report_hal(m, NP_SIM_OP_INS_PWM_SET, args, (int)NP_HUB_OK, "default");
    return NP_HUB_OK;
}

void np_mod_ins_hal_pwm_stop(uint8_t side)
{
    np_sim_module_t *m = resolve("ins_pwm_stop");
    if (m == NULL || side > 1U) { return; }
    char args[16];
    (void)snprintf(args, sizeof args, "[%u]", side);
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_INS_PWM_STOP, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_INS_PWM_STOP, args, code, "injected");
        return;                         /* that side keeps emitting */
    }
    m->ins_side_on[side] = false;
    m->driven = m->ins_side_on[0] || m->ins_side_on[1];
    np_sim_report_hal(m, NP_SIM_OP_INS_PWM_STOP, args, (int)NP_HUB_OK, "default");
}

uint16_t np_mod_ins_hal_pd_read(uint8_t side, uint8_t wl)
{
    np_sim_module_t *m = resolve("ins_pd_read");
    if (m == NULL || side > 1U || wl > 1U) { return 0U; }
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_INS_PD_READ, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_INS_PD_READ, "[]", code, "injected");
        return 0U;
    }
    const np_sim_override_t *o = &m->ov[NP_SIM_OP_INS_PD_READ];
    const uint16_t v = o->has_reading ? (uint16_t)o->reading : m->ins_pd[side][wl];
    char args[24];
    (void)snprintf(args, sizeof args, "[%u,%u,%u]", side, wl, v);
    np_sim_report_hal(m, NP_SIM_OP_INS_PD_READ, args, (int)NP_HUB_OK, o->has_reading ? "injected" : "default");
    return v;
}
