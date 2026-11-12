/*
 * NeurOne Firmware-in-the-Loop Simulator — visual goggle seams (NP-SIM-FW-001 §8 step 3, REQ-SIMFW-13)
 * HOST ONLY; see np_sim_bus.h.
 *
 * Defines the seven np_mod_visual_hal_* seams of np_sw02_platform_hal.h.  np_mod_visual.c is the device's own source,
 * including hall_unseated_isr: the model calls the handler the driver registered, so the not-seated edge runs the real
 * code (np_mod_visual_hal_led_stop, np_safety_spi_request_disable_from_isr, the latch) and not a flag.
 *
 *   sensors           seated, eye open and MPE-ok are what the scenario sets; no default.  The three seams that
 *                     return a boolean answer FALSE for an injected failure or an absent goggle: false is the
 *                     refusing answer for all three (np_sw02_platform_hal.h: "false on any fault"), so a fault
 *                     cannot read as permission.
 *   LEDs              what the driver last commanded: zone mask, wavelength select, frequency, duty, and whether
 *                     they are lit.  No optical power, no irradiance, no MPE: the part that enforces the ceiling is
 *                     hardware (CLAUDE.md §4.2) and is not modelled, so `mpe_ok` is a switch the scenario flips.
 *   the Hall edge     `np_sim_visual_lift()` clears the seated reading and delivers the edge to the registered
 *                     handler, if there is one, the way the platform interrupt would.  An armed
 *                     vis_lift_during_command delivers it once between the seated read and the next LED command,
 *                     the window the driver's s_hall_cut latch exists for.
 *
 * Injection.  led_set, emdr and the register call return a status, so an armed response is returned.  led_stop returns
 * nothing: an armed response makes the call not happen, and the LEDs stay lit.
 */
#include "np_sim_bus.h"
#include "np_sim_port_shim.h"

#include "np_sw02_platform_hal.h"

#include <stdio.h>

static np_sim_module_t *resolve(const char *op_name)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_VISUAL, 0U);
    if (m == NULL || !m->present) {
        np_sim_report_absent(NP_SIM_KIND_VISUAL, 0U, op_name);
        return NULL;
    }
    return m;
}

void np_sim_visual_lift(void)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_VISUAL, 0U);
    if (m == NULL) { return; }
    m->vis_seated = false;
    np_sim_emit("{\"ev\":\"hall_edge\",\"t\":%u,\"id\":\"visual:0\",\"handler_registered\":%s}",
                (unsigned)np_sim_now_ms(), m->vis_isr != NULL ? "true" : "false");
    if (m->vis_isr != NULL) {
        m->vis_edges_delivered++;
        np_sim_isr_enter();           /* xPortIsInsideInterrupt() is true for the handler (np_sim_port_shim.h) */
        m->vis_isr();                 /* the platform interrupt: the driver's own handler runs */
        np_sim_isr_exit();
    }
}

/* The lift that arrives between the seated read and the enable: fires once, before the next LED or EMDR command. */
static void maybe_lift_now(np_sim_module_t *m)
{
    if (m->vis_lift_during_command) {
        m->vis_lift_during_command = false;
        np_sim_visual_lift();
    }
}

np_hub_status_t np_mod_visual_hal_led_set(uint16_t zone_mask, uint8_t wl_sel, uint8_t freq_hz, uint8_t duty_pct)
{
    np_sim_module_t *m = resolve("vis_led_set");
    if (m == NULL) { return NP_HUB_ERR_NOT_PRESENT; }
    maybe_lift_now(m);
    char args[64];
    (void)snprintf(args, sizeof args, "[%u,%u,%u,%u]", zone_mask, wl_sel, freq_hz, duty_pct);
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_VIS_LED_SET, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_VIS_LED_SET, args, code, "injected");
        return (np_hub_status_t)code;
    }
    m->vis_zone_mask = zone_mask; m->vis_wl = wl_sel; m->vis_freq_hz = freq_hz; m->vis_duty_pct = duty_pct;
    m->vis_emitting = (zone_mask != 0U && duty_pct != 0U);
    m->driven = m->vis_emitting || m->vis_emdr_on;
    np_sim_report_hal(m, NP_SIM_OP_VIS_LED_SET, args, (int)NP_HUB_OK, "default");
    return NP_HUB_OK;
}

void np_mod_visual_hal_led_stop(void)
{
    np_sim_module_t *m = resolve("vis_led_stop");
    if (m == NULL) { return; }
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_VIS_LED_STOP, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_VIS_LED_STOP, "[]", code, "injected");
        return;                       /* the LEDs stay lit */
    }
    m->vis_emitting = false; m->vis_emdr_on = false; m->driven = false;
    np_sim_report_hal(m, NP_SIM_OP_VIS_LED_STOP, "[]", (int)NP_HUB_OK, "default");
}

/* A boolean sensor: false (the refusing answer) when the goggle is absent or a failure is armed. */
static bool read_bool(np_sim_op_t op, const char *op_name, bool model, bool *seen_absent)
{
    np_sim_module_t *m = resolve(op_name);
    if (m == NULL) { if (seen_absent) { *seen_absent = true; } return false; }
    int code;
    if (np_sim_take_response(m, op, &code)) {
        np_sim_report_hal(m, op, "[]", code, "injected");
        return false;
    }
    const np_sim_override_t *o = &m->ov[op];
    const bool v = o->has_reading ? (o->reading != 0.0f) : model;
    char args[16];
    (void)snprintf(args, sizeof args, "[%s]", v ? "true" : "false");
    np_sim_report_hal(m, op, args, (int)NP_HUB_OK, o->has_reading ? "injected" : "default");
    return v;
}

bool np_mod_visual_hal_ir_eye_open(void)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_VISUAL, 0U);
    return read_bool(NP_SIM_OP_VIS_EYE_OPEN, "vis_eye_open", m != NULL && m->vis_eye_open, NULL);
}

bool np_mod_visual_hal_goggle_seated(void)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_VISUAL, 0U);
    return read_bool(NP_SIM_OP_VIS_SEATED, "vis_seated", m != NULL && m->vis_seated, NULL);
}

bool np_mod_visual_hal_mpe_check(void)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_VISUAL, 0U);
    return read_bool(NP_SIM_OP_VIS_MPE, "vis_mpe", m != NULL && m->vis_mpe_ok, NULL);
}

void np_mod_visual_hal_emdr_set(uint8_t rate_mhz)
{
    np_sim_module_t *m = resolve("vis_emdr");
    if (m == NULL) { return; }
    maybe_lift_now(m);
    char args[16];
    (void)snprintf(args, sizeof args, "[%u]", rate_mhz);
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_VIS_EMDR, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_VIS_EMDR, args, code, "injected");
        return;
    }
    m->vis_emdr_rate_mhz = rate_mhz; m->vis_emdr_on = (rate_mhz != 0U);
    m->driven = m->vis_emitting || m->vis_emdr_on;
    np_sim_report_hal(m, NP_SIM_OP_VIS_EMDR, args, (int)NP_HUB_OK, "default");
}

np_hub_status_t np_mod_visual_hal_hall_irq_register(np_mod_visual_unseated_isr_t on_unseated)
{
    np_sim_module_t *m = resolve("vis_hall_register");
    if (m == NULL) { return NP_HUB_ERR_NOT_PRESENT; }
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_VIS_HALL_REGISTER, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_VIS_HALL_REGISTER, "[]", code, "injected");
        m->vis_isr = NULL;                /* "a return other than NP_HUB_OK means no edge will be delivered" */
        return (np_hub_status_t)code;
    }
    m->vis_isr = on_unseated;             /* NULL disarms */
    np_sim_report_hal(m, NP_SIM_OP_VIS_HALL_REGISTER, on_unseated != NULL ? "[\"armed\"]" : "[\"disarmed\"]",
                      (int)NP_HUB_OK, "default");
    return NP_HUB_OK;
}
