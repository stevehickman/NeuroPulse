/*
 * NeurOne Firmware-in-the-Loop Simulator — intranasal harness (NP-SIM-FW-001 §8 step 3, REQ-SIMFW-13)
 * HOST ONLY.  Text commands that drive the REAL np_mod_intranasal.c against the sim Y-probe.
 *
 *   ins_add <auth> <contact>        the probe, and what its two sensors read (0 or 1)
 *   ins_absent | ins_present | ins_clear
 *   ins_sensor auth|contact 0|1     change what a sensor reads
 *   ins_pd <side> <wl> <counts>     what the photodiode will read
 *   ins_detect | ins_init
 *   ins <side> <freq_code> <duty> <cur_660> <cur_808>   np_mod_intranasal_control (side 0 both, 1 left, 2 right)
 *   ins_stop | ins_shutdown | ins_sample | ins_raw <len>
 * (fail and reading are the shared commands of np_sim_pbm_harness.c; an ins_* op name addresses the probe.)
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

extern np_hub_status_t np_mod_intranasal_detect(uint8_t slot, np_hub_mod_type_t *type_out);
extern np_hub_status_t np_mod_intranasal_init(uint8_t slot);
extern np_hub_status_t np_mod_intranasal_control(uint8_t slot, const void *params, uint16_t len);
extern np_hub_status_t np_mod_intranasal_telemetry(uint8_t slot, np_telem_record_t *out);
extern np_hub_status_t np_mod_intranasal_shutdown(uint8_t slot);

void np_simh_ins_init(void) { }

static void ins_result(const char *cmd, int code)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_INTRANASAL, 0U);
    np_sim_emit("{\"ev\":\"result\",\"cmd\":\"%s\",\"code\":%d,\"requested_enables\":%u,\"granted_enables\":%u,"
                "\"left_on\":%s,\"right_on\":%s,\"safety_mcu\":\"%s\"}",
                cmd, code, (unsigned)np_safety_spi_get_requested_mask(), (unsigned)np_safety_spi_get_granted_mask(),
                (m && m->ins_side_on[0]) ? "true" : "false", (m && m->ins_side_on[1]) ? "true" : "false", np_sim_safety_label());
}

void np_simh_ins_status(void)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_INTRANASAL, 0U);
    if (m == NULL) { return; }
    np_sim_emit("{\"ev\":\"module_state\",\"id\":\"intranasal:0\",\"present\":%s,\"driven\":%s,\"left\":%s,\"right\":%s}",
                m->present ? "true" : "false", m->driven ? "true" : "false",
                m->ins_side_on[0] ? "true" : "false", m->ins_side_on[1] ? "true" : "false");
}

bool np_simh_ins_line(const char *cmd, char *a, char *b, char *c, char *d, char *e)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_INTRANASAL, 0U);
    if (strcmp(cmd, "ins_add") == 0 && a && b) {
        m = np_sim_add(NP_SIM_KIND_INTRANASAL, 0U, 0.0f);
        if (m != NULL) { m->ins_auth = atoi(a) != 0; m->ins_contact = atoi(b) != 0; }
    } else if (strcmp(cmd, "ins_absent") == 0) {
        (void)np_sim_set_present(NP_SIM_KIND_INTRANASAL, 0U, false);
    } else if (strcmp(cmd, "ins_present") == 0) {
        (void)np_sim_set_present(NP_SIM_KIND_INTRANASAL, 0U, true);
    } else if (strcmp(cmd, "ins_clear") == 0) {
        (void)np_sim_clear(NP_SIM_KIND_INTRANASAL, 0U);
    } else if (strcmp(cmd, "ins_sensor") == 0 && a && b && m != NULL) {
        if (strcmp(a, "auth") == 0)         { m->ins_auth = atoi(b) != 0; }
        else if (strcmp(a, "contact") == 0) { m->ins_contact = atoi(b) != 0; }
        else { return false; }
    } else if (strcmp(cmd, "ins_pd") == 0 && a && b && c && m != NULL) {
        const unsigned side = (unsigned)atoi(a), wl = (unsigned)atoi(b);
        if (side < 2U && wl < 2U) { m->ins_pd[side][wl] = (uint16_t)atoi(c); }
    } else if (strcmp(cmd, "ins_detect") == 0) {
        np_hub_mod_type_t t = NP_MOD_NONE;
        const np_hub_status_t rc = np_mod_intranasal_detect(NP_HUB_SLOT_INTRANASAL, &t);
        np_sim_emit("{\"ev\":\"ins_detect\",\"code\":%d,\"mod_type\":%d}", (int)rc, (int)t);
    } else if (strcmp(cmd, "ins_init") == 0) {
        ins_result(cmd, (int)np_mod_intranasal_init(NP_HUB_SLOT_INTRANASAL));
    } else if (strcmp(cmd, "ins") == 0 && a && b && c && d && e) {
        const np_mod_intranasal_params_t p = { .side = (uint8_t)atoi(a), .freq_code = (uint8_t)atoi(b), .duty = (uint8_t)atoi(c),
                                               .cur_660 = (uint8_t)atoi(d), .cur_808 = (uint8_t)atoi(e) };
        ins_result(cmd, (int)np_mod_intranasal_control(NP_HUB_SLOT_INTRANASAL, &p, (uint16_t)sizeof p));
    } else if (strcmp(cmd, "ins_stop") == 0) {
        ins_result(cmd, (int)np_mod_intranasal_control(NP_HUB_SLOT_INTRANASAL, NULL, 0U));
    } else if (strcmp(cmd, "ins_shutdown") == 0) {
        ins_result(cmd, (int)np_mod_intranasal_shutdown(NP_HUB_SLOT_INTRANASAL));
    } else if (strcmp(cmd, "ins_raw") == 0 && a) {
        uint8_t zeros[16];
        memset(zeros, 0, sizeof zeros);
        const unsigned len = (unsigned)atoi(a);
        ins_result(cmd, (int)np_mod_intranasal_control(NP_HUB_SLOT_INTRANASAL, zeros, (uint16_t)(len > sizeof zeros ? sizeof zeros : len)));
    } else if (strcmp(cmd, "ins_sample") == 0) {
        np_telem_record_t rec;
        memset(&rec, 0, sizeof rec);
        const np_hub_status_t rc = np_mod_intranasal_telemetry(NP_HUB_SLOT_INTRANASAL, &rec);
        np_sim_emit("{\"ev\":\"ins_sample\",\"code\":%d,\"mod_type\":%d,\"dose_left\":%.3f,\"dose_right\":%.3f}",
                    (int)rc, (int)rec.mod_type, (double)rec.data.pbm.dose_J_cm2[0], (double)rec.data.pbm.dose_J_cm2[1]);
    } else {
        return false;
    }
    return true;
}
