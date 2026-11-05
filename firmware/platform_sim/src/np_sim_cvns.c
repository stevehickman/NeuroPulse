/*
 * NeurOne Firmware-in-the-Loop Simulator — cervical VNS seams (NP-SIM-FW-001 §8 step 3, REQ-SIMFW-13)
 * HOST ONLY; see np_sim_bus.h.
 *
 * Defines the platform seams np_mod_cvns.c and the re-enable manager call: accessory presence, the two clocks, the hub-side
 * electrode impedance measurement, and the re-enable gate's fresh impedance check.
 *
 * What is NOT here, and why that matters.  The cervical VNS library (firmware/cervical_vns/) reaches the safety MCU, the
 * stimulator and the R-peak line through `static` functions inside its own sources (np_cvns_stim.c `platform_set_current`,
 * `platform_safety_mcu_request_enable`, np_cvns_interlock.c `platform_rpeak_gpio_pulse`, ...).  Those are not link-time seams: they
 * are no-ops compiled into the library, and REQ-SIMFW-03 forbids changing a firmware file for the simulator.  So the library runs
 * entire and real, and the simulator cannot see the current it asks for or the R-peak pulses it makes.  What it can see is what the
 * driver does with the library's state: the stage, the interlock state, the unified enable bit and the commanded current published
 * to the safety channel.  And the scenario stands in for the safety MCU the way the heartbeat task does, through the driver's own
 * entry points (np_mod_cvns_set_heartbeat_status, np_mod_cvns_set_mcu_impedance).
 *
 *   clocks        np_mod_cvns_hal_now_ms is the simulator's clock; np_mod_cvns_hal_now_unix is that clock in seconds plus the
 *                 base the scenario gave the accessory.
 *   impedance     the per-electrode kohm the scenario gave the accessory.  The measurement completes on the first poll; an
 *                 injected failure makes the start return its code (the driver then falls back to its nominal placeholder).
 *   re-enable     gate 3's fresh impedance verdict is the scenario's pass flag.
 */
#include "np_sim_bus.h"

#include "np_sw02_platform_hal.h"
#include "np_cvns_reenable.h"

#include <stdio.h>

static np_sim_module_t *resolve(const char *op_name)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_CVNS, 0U);
    if (m == NULL || !m->present) {
        np_sim_report_absent(NP_SIM_KIND_CVNS, 0U, op_name);
        return NULL;
    }
    return m;
}

bool np_cvns_hal_accessory_present(void)
{
    np_sim_module_t *m = resolve("cvns_accessory");
    if (m == NULL) { return false; }
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_CVNS_ACCESSORY, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_CVNS_ACCESSORY, "[]", code, "injected");
        return false;
    }
    np_sim_report_hal(m, NP_SIM_OP_CVNS_ACCESSORY, "[true]", (int)NP_HUB_OK, "default");
    return true;
}

uint32_t np_mod_cvns_hal_now_ms(void) { return np_sim_now_ms(); }

uint32_t np_mod_cvns_hal_now_unix(void)
{
    const np_sim_module_t *m = np_sim_find(NP_SIM_KIND_CVNS, 0U);
    return (m != NULL ? m->cvns_unix_base_s : 0U) + np_sim_now_ms() / 1000U;
}

np_hub_status_t np_cvns_hal_impedance_measure_start(void)
{
    np_sim_module_t *m = resolve("cvns_imp_measure_start");
    if (m == NULL) { return NP_HUB_ERR_NOT_PRESENT; }
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_CVNS_IMP_MEASURE_START, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_CVNS_IMP_MEASURE_START, "[]", code, "injected");
        return (np_hub_status_t)code;
    }
    np_sim_report_hal(m, NP_SIM_OP_CVNS_IMP_MEASURE_START, "[]", (int)NP_HUB_OK, "default");
    return NP_HUB_OK;
}

bool np_cvns_hal_impedance_measure_poll(float kohm_out[])
{
    np_sim_module_t *m = resolve("cvns_imp_measure_poll");
    if (m == NULL) { return false; }
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_CVNS_IMP_MEASURE_POLL, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_CVNS_IMP_MEASURE_POLL, "[]", code, "injected");
        return false;                   /* still in flight: kohm_out untouched */
    }
    const np_sim_override_t *o = &m->ov[NP_SIM_OP_CVNS_IMP_MEASURE_POLL];
    kohm_out[0] = o->has_reading ? o->reading : m->cvns_kohm[0];
    kohm_out[1] = o->has_reading ? o->reading : m->cvns_kohm[1];
    char args[48];
    (void)snprintf(args, sizeof args, "[%.2f,%.2f]", (double)kohm_out[0], (double)kohm_out[1]);
    np_sim_report_hal(m, NP_SIM_OP_CVNS_IMP_MEASURE_POLL, args, (int)NP_HUB_OK, o->has_reading ? "injected" : "default");
    return true;
}

np_hub_status_t np_cvns_hal_impedance_start(void)
{
    np_sim_module_t *m = resolve("cvns_reenable_imp_start");
    if (m == NULL) { return NP_HUB_ERR_NOT_PRESENT; }
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_CVNS_REENABLE_IMP_START, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_CVNS_REENABLE_IMP_START, "[]", code, "injected");
        return (np_hub_status_t)code;
    }
    np_sim_report_hal(m, NP_SIM_OP_CVNS_REENABLE_IMP_START, "[]", (int)NP_HUB_OK, "default");
    return NP_HUB_OK;
}

bool np_cvns_hal_impedance_poll(bool *passed_out)
{
    np_sim_module_t *m = resolve("cvns_reenable_imp_poll");
    if (m == NULL) { return false; }
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_CVNS_REENABLE_IMP_POLL, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_CVNS_REENABLE_IMP_POLL, "[]", code, "injected");
        return false;
    }
    *passed_out = m->cvns_reenable_pass;
    np_sim_report_hal(m, NP_SIM_OP_CVNS_REENABLE_IMP_POLL, m->cvns_reenable_pass ? "[true]" : "[false]", (int)NP_HUB_OK, "default");
    return true;
}
