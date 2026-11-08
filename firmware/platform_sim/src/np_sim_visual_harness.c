/*
 * NeurOne Firmware-in-the-Loop Simulator — visual harness (NP-SIM-FW-001 §8 step 3, REQ-SIMFW-13)
 * HOST ONLY.  Text commands that drive the REAL np_mod_visual.c against the sim goggles.
 *
 *   vis_add <seated> <eye_open> <mpe_ok>     the goggles, and what their three sensors read (0 or 1)
 *   vis_absent | vis_present | vis_clear
 *   vis_sensor seated|eye|mpe 0|1            change what a sensor reads
 *   vis_detect | vis_init
 *   vis <mode> <freq_hz> <duty_pct> <wl> <zone_lo> <zone_hi> <emdr_mhz> <mode_f> <shade>   np_mod_visual_control
 *   vis_stop | vis_shutdown
 *   vis_lift                                 the goggle is lifted: the Hall edge reaches the registered handler
 *   vis_lift_silent                          lifted, no edge (the poll backstop is all that sees it)
 *   vis_lift_during_command                  the next command is interrupted by a lift between its read and its enable
 *   vis_lower                                seated again
 *   vis_ppx                                  np_mod_visual_ppx_halt, the photoparoxysmal halt the EEG path calls
 *   vis_sample                               np_mod_visual_telemetry
 *   vis_raw <len>                            a command of the wrong size
 * (fail and reading are the shared commands of np_sim_pbm_harness.c; a vis_* op name addresses the goggles.)
 */
#include "np_sim_bus.h"
#include "np_sim_safety.h"
#include "np_sim_pbm_harness.h"

#include "np_hub_types.h"
#include "np_hub_config.h"
#include "np_safety_spi.h"
#include "np_sw02_platform_hal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern np_hub_status_t np_mod_visual_detect(uint8_t slot, np_hub_mod_type_t *type_out);
extern np_hub_status_t np_mod_visual_init(uint8_t slot);
extern np_hub_status_t np_mod_visual_control(uint8_t slot, const void *params, uint16_t len);
extern np_hub_status_t np_mod_visual_telemetry(uint8_t slot, np_telem_record_t *out);
extern np_hub_status_t np_mod_visual_shutdown(uint8_t slot);
extern void            np_mod_visual_ppx_halt(void);

void np_simh_visual_init(void) { }

static void vis_result(const char *cmd, int code)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_VISUAL, 0U);
    np_sim_emit("{\"ev\":\"result\",\"cmd\":\"%s\",\"code\":%d,\"requested_enables\":%u,\"granted_enables\":%u,"
                "\"emitting\":%s,\"emdr\":%s,\"seated\":%s,\"edges_delivered\":%u,\"safety_mcu\":\"%s\"}",
                cmd, code, (unsigned)np_safety_spi_get_requested_mask(), (unsigned)np_safety_spi_get_granted_mask(),
                (m && m->vis_emitting) ? "true" : "false", (m && m->vis_emdr_on) ? "true" : "false",
                (m && m->vis_seated) ? "true" : "false", m ? (unsigned)m->vis_edges_delivered : 0U, np_sim_safety_label());
}

void np_simh_visual_status(void)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_VISUAL, 0U);
    if (m == NULL) { return; }
    np_sim_emit("{\"ev\":\"module_state\",\"id\":\"visual:0\",\"present\":%s,\"driven\":%s,\"seated\":%s,\"zone_mask\":%u,\"duty_pct\":%u}",
                m->present ? "true" : "false", m->driven ? "true" : "false", m->vis_seated ? "true" : "false",
                (unsigned)m->vis_zone_mask, (unsigned)m->vis_duty_pct);
}

bool np_simh_visual_line(const char *cmd, char *a, char *b, char *c, char *d, char *e)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_VISUAL, 0U);
    if (strcmp(cmd, "vis_add") == 0 && a && b && c) {
        m = np_sim_add(NP_SIM_KIND_VISUAL, 0U, 0.0f);
        if (m != NULL) { m->vis_seated = atoi(a) != 0; m->vis_eye_open = atoi(b) != 0; m->vis_mpe_ok = atoi(c) != 0; }
    } else if (strcmp(cmd, "vis_absent") == 0) {
        (void)np_sim_set_present(NP_SIM_KIND_VISUAL, 0U, false);
    } else if (strcmp(cmd, "vis_present") == 0) {
        (void)np_sim_set_present(NP_SIM_KIND_VISUAL, 0U, true);
    } else if (strcmp(cmd, "vis_clear") == 0) {
        (void)np_sim_clear(NP_SIM_KIND_VISUAL, 0U);
    } else if (strcmp(cmd, "vis_sensor") == 0 && a && b && m != NULL) {
        const bool v = atoi(b) != 0;
        if (strcmp(a, "seated") == 0)    { m->vis_seated = v; }
        else if (strcmp(a, "eye") == 0)  { m->vis_eye_open = v; }
        else if (strcmp(a, "mpe") == 0)  { m->vis_mpe_ok = v; }
        else { return false; }
    } else if (strcmp(cmd, "vis_detect") == 0) {
        np_hub_mod_type_t t = NP_MOD_NONE;
        const np_hub_status_t rc = np_mod_visual_detect(NP_HUB_SLOT_VISUAL, &t);
        np_sim_emit("{\"ev\":\"vis_detect\",\"code\":%d,\"mod_type\":%d}", (int)rc, (int)t);
    } else if (strcmp(cmd, "vis_init") == 0) {
        vis_result(cmd, (int)np_mod_visual_init(NP_HUB_SLOT_VISUAL));
    } else if (strcmp(cmd, "vis") == 0 && a && b && c && d && e) {
        int zlo = 0, zhi = 0, emdr = 0, modef = 0, shade = 0;     /* e is the rest of the line */
        if (sscanf(e, "%d %d %d %d %d", &zlo, &zhi, &emdr, &modef, &shade) != 5) { return false; }
        /* a=mode b=freq c=duty d=wl then zone_lo zone_hi emdr mode_f shade; the params struct's order is its own */
        np_mod_visual_params_t p;
        memset(&p, 0, sizeof p);
        p.mode = (uint8_t)atoi(a); p.freq_hz = (uint8_t)atoi(b); p.duty_pct = (uint8_t)atoi(c); p.wl_select = (uint8_t)atoi(d);
        p.zone_mask_lo = (uint8_t)zlo; p.zone_mask_hi = (uint8_t)zhi; p.emdr_rate_mhz = (uint8_t)emdr;
        p.mode_f_enable = (uint8_t)modef; p.shade_req = (uint8_t)shade;
        vis_result(cmd, (int)np_mod_visual_control(NP_HUB_SLOT_VISUAL, &p, (uint16_t)sizeof p));
    } else if (strcmp(cmd, "vis_stop") == 0) {
        vis_result(cmd, (int)np_mod_visual_control(NP_HUB_SLOT_VISUAL, NULL, 0U));
    } else if (strcmp(cmd, "vis_shutdown") == 0) {
        vis_result(cmd, (int)np_mod_visual_shutdown(NP_HUB_SLOT_VISUAL));
    } else if (strcmp(cmd, "vis_raw") == 0 && a) {
        uint8_t zeros[16];
        memset(zeros, 0, sizeof zeros);
        const unsigned len = (unsigned)atoi(a);
        vis_result(cmd, (int)np_mod_visual_control(NP_HUB_SLOT_VISUAL, zeros, (uint16_t)(len > sizeof zeros ? sizeof zeros : len)));
    } else if (strcmp(cmd, "vis_lift") == 0) {
        np_sim_visual_lift();
        vis_result(cmd, 0);
    } else if (strcmp(cmd, "vis_lift_silent") == 0 && m != NULL) {
        m->vis_seated = false;
        vis_result(cmd, 0);
    } else if (strcmp(cmd, "vis_lift_during_command") == 0 && m != NULL) {
        m->vis_lift_during_command = true;
    } else if (strcmp(cmd, "vis_lower") == 0 && m != NULL) {
        m->vis_seated = true;
    } else if (strcmp(cmd, "vis_ppx") == 0) {
        np_mod_visual_ppx_halt();
        vis_result(cmd, 0);
    } else if (strcmp(cmd, "vis_sample") == 0) {
        np_telem_record_t rec;
        memset(&rec, 0, sizeof rec);
        const np_hub_status_t rc = np_mod_visual_telemetry(NP_HUB_SLOT_VISUAL, &rec);
        np_sim_emit("{\"ev\":\"vis_sample\",\"code\":%d,\"active_mode\":%u,\"goggle_seated\":%s,\"eye_open\":%s,\"ppx_halt\":%s}",
                    (int)rc, (unsigned)rec.data.visual.active_mode, rec.data.visual.goggle_seated ? "true" : "false",
                    rec.data.visual.eye_open ? "true" : "false", rec.data.visual.ppx_halt ? "true" : "false");
        vis_result(cmd, (int)rc);
    } else {
        return false;
    }
    return true;
}
