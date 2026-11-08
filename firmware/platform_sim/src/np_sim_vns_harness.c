/*
 * NeurOne Firmware-in-the-Loop Simulator — auricular VNS harness (NP-SIM-FW-001 §8 step 3, REQ-SIMFW-13)
 * HOST ONLY.  Text commands that drive the REAL np_mod_vns.c, and through it the real HRV session library
 * (firmware/hrv_biofeedback/), against the sim ear clip.
 *
 *   vns_add <ohm>                  the clip, with the contact impedance it reads
 *   vns_absent | vns_present | vns_clear
 *   vns_readouts <rmssd_ms> <coherence> <hr_bpm>   what the clip will report (scenario state)
 *   vns_detect | vns_init
 *   vns <side> <freq_mhz> <amp_ua> <pw_us> <ppg> <eeg_ref> <hrv_proto>   np_mod_vns_control
 *   vns_stop | vns_shutdown
 *   vns_sample                     np_mod_vns_telemetry
 *   vns_raw <len>                  a command of the wrong size
 * (fail and reading are the shared commands of np_sim_pbm_harness.c; a vns_* op name addresses the clip.)
 */
#include "np_sim_bus.h"
#include "np_sim_safety.h"
#include "np_sim_pbm_harness.h"

#include "np_hub_types.h"
#include "np_hub_config.h"
#include "np_safety_spi.h"
#include "np_sw02_platform_hal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern np_hub_status_t np_mod_vns_detect(uint8_t slot, np_hub_mod_type_t *type_out);
extern np_hub_status_t np_mod_vns_init(uint8_t slot);
extern np_hub_status_t np_mod_vns_control(uint8_t slot, const void *params, uint16_t len);
extern np_hub_status_t np_mod_vns_telemetry(uint8_t slot, np_telem_record_t *out);
extern np_hub_status_t np_mod_vns_shutdown(uint8_t slot);

void np_simh_vns_init(void) { }

static void vns_result(const char *cmd, int code)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_VNS, 0U);
    np_sim_emit("{\"ev\":\"result\",\"cmd\":\"%s\",\"code\":%d,\"requested_enables\":%u,\"granted_enables\":%u,"
                "\"stimulating\":%s,\"ppg_on\":%s,\"eeg_ref\":%s,\"safety_mcu\":\"%s\"}",
                cmd, code, (unsigned)np_safety_spi_get_requested_mask(), (unsigned)np_safety_spi_get_granted_mask(),
                (m != NULL && m->driven) ? "true" : "false", (m != NULL && m->vns_ppg_on) ? "true" : "false",
                (m != NULL && m->vns_eeg_ref) ? "true" : "false", np_sim_safety_label());
}

void np_simh_vns_status(void)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_VNS, 0U);
    if (m == NULL) { return; }
    np_sim_emit("{\"ev\":\"module_state\",\"id\":\"vns:0\",\"present\":%s,\"driven\":%s,\"left\":%s,\"right\":%s,"
                "\"amp_ua\":%u,\"ppg_on\":%s}",
                m->present ? "true" : "false", m->driven ? "true" : "false", m->vns_side_on[0] ? "true" : "false",
                m->vns_side_on[1] ? "true" : "false", (unsigned)m->vns_amp_ua, m->vns_ppg_on ? "true" : "false");
}

bool np_simh_vns_line(const char *cmd, char *a, char *b, char *c, char *d, char *e)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_VNS, 0U);
    if (strcmp(cmd, "vns_add") == 0 && a) {
        (void)np_sim_add(NP_SIM_KIND_VNS, 0U, (float)atof(a));
    } else if (strcmp(cmd, "vns_absent") == 0) {
        (void)np_sim_set_present(NP_SIM_KIND_VNS, 0U, false);
    } else if (strcmp(cmd, "vns_present") == 0) {
        (void)np_sim_set_present(NP_SIM_KIND_VNS, 0U, true);
    } else if (strcmp(cmd, "vns_clear") == 0) {
        (void)np_sim_clear(NP_SIM_KIND_VNS, 0U);
    } else if (strcmp(cmd, "vns_readouts") == 0 && a && b && c && m != NULL) {
        m->vns_rmssd_ms = (float)atof(a); m->vns_coherence = (float)atof(b); m->vns_hr_bpm = (float)atof(c);
    } else if (strcmp(cmd, "vns_detect") == 0) {
        np_hub_mod_type_t t = NP_MOD_NONE;
        const np_hub_status_t rc = np_mod_vns_detect(NP_HUB_SLOT_VNS_HRV, &t);
        np_sim_emit("{\"ev\":\"vns_detect\",\"code\":%d,\"mod_type\":%d}", (int)rc, (int)t);
    } else if (strcmp(cmd, "vns_init") == 0) {
        vns_result(cmd, (int)np_mod_vns_init(NP_HUB_SLOT_VNS_HRV));
    } else if (strcmp(cmd, "vns") == 0 && a && b && c && d && e) {
        int ppg = 0, ref = 0, proto = 0;            /* e is the rest of the line: <ppg> <eeg_ref> <hrv_proto> */
        if (sscanf(e, "%d %d %d", &ppg, &ref, &proto) != 3) { return false; }
        const np_mod_vns_hrv_params_t p = { .side = (uint8_t)atoi(a), .freq_mhz = (uint32_t)strtoul(b, NULL, 10),
                                            .amplitude_ua = (uint16_t)atoi(c), .pulse_width_us = (uint16_t)atoi(d),
                                            .ppg_enable = (uint8_t)ppg, .eeg_ref_enable = (uint8_t)ref,
                                            .hrv_proto = (uint8_t)proto };
        vns_result(cmd, (int)np_mod_vns_control(NP_HUB_SLOT_VNS_HRV, &p, (uint16_t)sizeof p));
    } else if (strcmp(cmd, "vns_stop") == 0) {
        vns_result(cmd, (int)np_mod_vns_control(NP_HUB_SLOT_VNS_HRV, NULL, 0U));
    } else if (strcmp(cmd, "vns_shutdown") == 0) {
        vns_result(cmd, (int)np_mod_vns_shutdown(NP_HUB_SLOT_VNS_HRV));
    } else if (strcmp(cmd, "vns_raw") == 0 && a) {
        uint8_t zeros[32];
        memset(zeros, 0, sizeof zeros);
        const unsigned len = (unsigned)atoi(a);
        vns_result(cmd, (int)np_mod_vns_control(NP_HUB_SLOT_VNS_HRV, zeros, (uint16_t)(len > sizeof zeros ? sizeof zeros : len)));
    } else if (strcmp(cmd, "vns_sample") == 0) {
        np_telem_record_t rec;
        memset(&rec, 0, sizeof rec);
        const np_hub_status_t rc = np_mod_vns_telemetry(NP_HUB_SLOT_VNS_HRV, &rec);
        const np_telem_vns_hrv_t *h = &rec.data.vns_hrv;
        np_sim_emit("{\"ev\":\"vns_sample\",\"code\":%d,\"rmssd_ms\":%.1f,\"coherence\":%.1f,\"hr_bpm\":%.1f,"
                    "\"impedance_ohm\":%u,\"contact_confirmed\":%s}",
                    (int)rc, isfinite(h->rmssd_ms) ? (double)h->rmssd_ms : -1.0,
                    isfinite(h->coherence_score) ? (double)h->coherence_score : -1.0,
                    isfinite(h->hr_bpm) ? (double)h->hr_bpm : -1.0, (unsigned)h->impedance_ohm,
                    h->contact_confirmed ? "true" : "false");
    } else {
        return false;
    }
    return true;
}
