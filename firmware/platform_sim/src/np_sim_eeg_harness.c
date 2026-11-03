/*
 * NeurOne Firmware-in-the-Loop Simulator — EEG harness (NP-SIM-FW-001 §8 step 3, REQ-SIMFW-13)
 * HOST ONLY.  Text commands that drive the REAL np_mod_eeg.c against the sim ADS1299.
 *
 *   eeg_add [id]                  the front end, with the ID register value the part reads (default 0x3E, the datasheet's)
 *   eeg_absent | eeg_present | eeg_clear
 *   eeg_detect                    np_mod_eeg_detect: reads the ID register
 *   eeg_init                      np_mod_eeg_init: the power-on sequence and a self-calibration
 *   eeg <chmask> <gain> <notch> <ref_mode> <adaptive>   np_mod_eeg_control
 *   eeg_stop | eeg_shutdown       control with no parameters / np_mod_eeg_shutdown
 *   eeg_impedance <ch> <kohm>     what the part will measure on a channel (scenario state)
 *   eeg_band <band> <db>          the power the part will report in a band (scenario state)
 *   eeg_sample                    np_mod_eeg_telemetry
 *   eeg_regs                      the register file, as the driver left it
 *   eeg_raw <len>                 a command of the wrong size
 * (fail and reading are the shared commands of np_sim_pbm_harness.c; an eeg_* op name addresses the front end.)
 */
#include "np_sim_bus.h"
#include "np_sim_pbm_harness.h"

#include "np_hub_types.h"
#include "np_hub_config.h"
#include "np_sw02_platform_hal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern np_hub_status_t np_mod_eeg_detect(uint8_t slot, np_hub_mod_type_t *type_out);
extern np_hub_status_t np_mod_eeg_init(uint8_t slot);
extern np_hub_status_t np_mod_eeg_control(uint8_t slot, const void *params, uint16_t len);
extern np_hub_status_t np_mod_eeg_telemetry(uint8_t slot, np_telem_record_t *out);
extern np_hub_status_t np_mod_eeg_shutdown(uint8_t slot);

void np_simh_eeg_init(void) { }

static void eeg_result(const char *cmd, int code)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_EEG, 0U);
    np_sim_emit("{\"ev\":\"result\",\"cmd\":\"%s\",\"code\":%d,\"eeg_running\":%s,\"calibrations\":%u}",
                cmd, code, (m != NULL && m->eeg_running) ? "true" : "false", m != NULL ? (unsigned)m->eeg_calibrations : 0U);
}

void np_simh_eeg_status(void)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_EEG, 0U);
    if (m == NULL) { return; }
    np_sim_emit("{\"ev\":\"module_state\",\"id\":\"eeg:0\",\"present\":%s,\"driven\":%s,\"calibrations\":%u}",
                m->present ? "true" : "false", m->eeg_running ? "true" : "false", (unsigned)m->eeg_calibrations);
}

bool np_simh_eeg_line(const char *cmd, char *a, char *b, char *c, char *d, char *e)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_EEG, 0U);
    if (strcmp(cmd, "eeg_add") == 0) {
        m = np_sim_add(NP_SIM_KIND_EEG, 0U, 0.0f);
        if (m != NULL) { m->eeg_regs[0] = (a != NULL) ? (uint8_t)strtol(a, NULL, 0) : 0x3EU; }
    } else if (strcmp(cmd, "eeg_absent") == 0) {
        (void)np_sim_set_present(NP_SIM_KIND_EEG, 0U, false);
    } else if (strcmp(cmd, "eeg_present") == 0) {
        (void)np_sim_set_present(NP_SIM_KIND_EEG, 0U, true);
    } else if (strcmp(cmd, "eeg_clear") == 0) {
        (void)np_sim_clear(NP_SIM_KIND_EEG, 0U);
    } else if (strcmp(cmd, "eeg_detect") == 0) {
        np_hub_mod_type_t t = NP_MOD_NONE;
        const np_hub_status_t rc = np_mod_eeg_detect(NP_HUB_SLOT_EEG, &t);
        np_sim_emit("{\"ev\":\"eeg_detect\",\"code\":%d,\"mod_type\":%d}", (int)rc, (int)t);
    } else if (strcmp(cmd, "eeg_init") == 0) {
        eeg_result(cmd, (int)np_mod_eeg_init(NP_HUB_SLOT_EEG));
    } else if (strcmp(cmd, "eeg") == 0 && a && b && c && d && e) {
        const np_mod_eeg_params_t p = { .channel_mask = (uint8_t)strtol(a, NULL, 0), .gain = (uint8_t)atoi(b),
                                        .notch = (uint8_t)atoi(c), .ref_mode = (uint8_t)atoi(d),
                                        .adaptive_out = (uint8_t)atoi(e) };
        eeg_result(cmd, (int)np_mod_eeg_control(NP_HUB_SLOT_EEG, &p, (uint16_t)sizeof p));
    } else if (strcmp(cmd, "eeg_stop") == 0) {
        eeg_result(cmd, (int)np_mod_eeg_control(NP_HUB_SLOT_EEG, NULL, 0U));
    } else if (strcmp(cmd, "eeg_shutdown") == 0) {
        eeg_result(cmd, (int)np_mod_eeg_shutdown(NP_HUB_SLOT_EEG));
    } else if (strcmp(cmd, "eeg_raw") == 0 && a) {
        uint8_t zeros[16];
        memset(zeros, 0, sizeof zeros);
        const unsigned len = (unsigned)atoi(a);
        eeg_result(cmd, (int)np_mod_eeg_control(NP_HUB_SLOT_EEG, zeros, (uint16_t)(len > sizeof zeros ? sizeof zeros : len)));
    } else if (strcmp(cmd, "eeg_impedance") == 0 && a && b && m != NULL) {
        const unsigned ch = (unsigned)atoi(a);
        if (ch < 8U) { m->eeg_impedance_kohm[ch] = (float)atof(b); }
    } else if (strcmp(cmd, "eeg_band") == 0 && a && b && m != NULL) {
        const unsigned band = (unsigned)atoi(a);
        if (band < 5U) { m->eeg_band_db[band] = (float)atof(b); }
    } else if (strcmp(cmd, "eeg_sample") == 0) {
        np_telem_record_t rec;
        memset(&rec, 0, sizeof rec);
        const np_hub_status_t rc = np_mod_eeg_telemetry(NP_HUB_SLOT_EEG, &rec);
        char imp[160] = "";
        size_t n = 0;
        for (unsigned ch = 0; ch < NP_EEG_CHANNELS && n + 20U < sizeof imp; ch++) {
            const float v = rec.data.eeg.impedance_kohm[ch];
            n += (size_t)snprintf(imp + n, sizeof imp - n, "%s%.1f", ch ? "," : "", isfinite(v) ? (double)v : -1.0);
        }
        np_sim_emit("{\"ev\":\"eeg_sample\",\"code\":%d,\"dominant_band\":%u,\"impedance_kohm\":[%s],"
                    "\"band_db\":[%.1f,%.1f,%.1f,%.1f,%.1f]}",
                    (int)rc, (unsigned)rec.data.eeg.dominant_band, imp,
                    (double)rec.data.eeg.band_power_db[0], (double)rec.data.eeg.band_power_db[1],
                    (double)rec.data.eeg.band_power_db[2], (double)rec.data.eeg.band_power_db[3],
                    (double)rec.data.eeg.band_power_db[4]);
    } else if (strcmp(cmd, "eeg_regs") == 0 && m != NULL) {
        np_sim_emit("{\"ev\":\"eeg_regs\",\"regs\":[%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u]}",
                    m->eeg_regs[0], m->eeg_regs[1], m->eeg_regs[2], m->eeg_regs[3], m->eeg_regs[4], m->eeg_regs[5],
                    m->eeg_regs[6], m->eeg_regs[7], m->eeg_regs[8], m->eeg_regs[9], m->eeg_regs[10],
                    m->eeg_regs[11], m->eeg_regs[12]);
    } else {
        return false;
    }
    return true;
}
