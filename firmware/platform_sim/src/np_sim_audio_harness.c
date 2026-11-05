/*
 * NeurOne Firmware-in-the-Loop Simulator — audio harness (NP-SIM-FW-001 §8 step 3, REQ-SIMFW-13)
 * HOST ONLY.  Text commands that drive the REAL np_mod_audio.c against the sim audio path.
 *
 *   audio_add <mesh_ohm>           the audio path, with the mesh impedance it reads
 *   audio_absent | audio_present | audio_clear
 *   audio_detect | audio_init
 *   audio <mode> <carrier_hz> <beat_mhz> <vol_pct> <bone> <adaptive>   np_mod_audio_control
 *   audio_stop | audio_shutdown
 *   audio_eeg_band <band>          np_mod_audio_set_eeg_band, the call the EEG telemetry path makes
 *   audio_sample                   np_mod_audio_telemetry
 *   audio_raw <len>                a command of the wrong size
 * (fail and reading are the shared commands of np_sim_pbm_harness.c; an audio_* op name addresses this path.)
 */
#include "np_sim_bus.h"
#include "np_sim_pbm_harness.h"

#include "np_hub_types.h"
#include "np_hub_config.h"
#include "np_sw02_platform_hal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern np_hub_status_t np_mod_audio_detect(uint8_t slot, np_hub_mod_type_t *type_out);
extern np_hub_status_t np_mod_audio_init(uint8_t slot);
extern np_hub_status_t np_mod_audio_control(uint8_t slot, const void *params, uint16_t len);
extern np_hub_status_t np_mod_audio_telemetry(uint8_t slot, np_telem_record_t *out);
extern np_hub_status_t np_mod_audio_shutdown(uint8_t slot);
extern void            np_mod_audio_set_eeg_band(uint8_t band);

void np_simh_audio_init(void) { }

static void audio_state_event(const char *ev, const char *cmd, int code)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_AUDIO, 0U);
    np_sim_emit("{\"ev\":\"%s\",\"cmd\":\"%s\",\"code\":%d,\"mode\":%u,\"carrier_hz\":%u,\"beat_mhz\":%u,\"vol_pct\":%u,"
                "\"bone_on\":%s,\"bone_hz\":%u,\"codec_inited\":%s}",
                ev, cmd, code, m ? (unsigned)m->audio_mode : 0U, m ? (unsigned)m->audio_carrier_hz : 0U,
                m ? (unsigned)m->audio_beat_mhz : 0U, m ? (unsigned)m->audio_vol_pct : 0U,
                (m && m->audio_bone_on) ? "true" : "false", m ? (unsigned)m->audio_bone_hz : 0U,
                (m && m->audio_inited) ? "true" : "false");
}

void np_simh_audio_status(void)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_AUDIO, 0U);
    if (m == NULL) { return; }
    np_sim_emit("{\"ev\":\"module_state\",\"id\":\"audio:0\",\"present\":%s,\"driven\":%s,\"mode\":%u,\"vol_pct\":%u,\"bone_on\":%s}",
                m->present ? "true" : "false", m->driven ? "true" : "false", (unsigned)m->audio_mode,
                (unsigned)m->audio_vol_pct, m->audio_bone_on ? "true" : "false");
}

bool np_simh_audio_line(const char *cmd, char *a, char *b, char *c, char *d, char *e)
{
    if (strcmp(cmd, "audio_add") == 0 && a) {
        (void)np_sim_add(NP_SIM_KIND_AUDIO, 0U, (float)atof(a));
    } else if (strcmp(cmd, "audio_absent") == 0) {
        (void)np_sim_set_present(NP_SIM_KIND_AUDIO, 0U, false);
    } else if (strcmp(cmd, "audio_present") == 0) {
        (void)np_sim_set_present(NP_SIM_KIND_AUDIO, 0U, true);
    } else if (strcmp(cmd, "audio_clear") == 0) {
        (void)np_sim_clear(NP_SIM_KIND_AUDIO, 0U);
    } else if (strcmp(cmd, "audio_detect") == 0) {
        np_hub_mod_type_t t = NP_MOD_NONE;
        const np_hub_status_t rc = np_mod_audio_detect(NP_HUB_SLOT_AUDIO, &t);
        np_sim_emit("{\"ev\":\"audio_detect\",\"code\":%d,\"mod_type\":%d}", (int)rc, (int)t);
    } else if (strcmp(cmd, "audio_init") == 0) {
        audio_state_event("result", cmd, (int)np_mod_audio_init(NP_HUB_SLOT_AUDIO));
    } else if (strcmp(cmd, "audio") == 0 && a && b && c && d && e) {
        int bone = 0, adaptive = 0;
        if (sscanf(e, "%d %d", &bone, &adaptive) != 2) { return false; }   /* e is the rest of the line */
        const np_mod_audio_params_t p = { .mode = (uint8_t)atoi(a), .carrier_hz = (uint16_t)atoi(b),
                                          .beat_mhz = (uint16_t)atoi(c), .volume_pct = (uint8_t)atoi(d),
                                          .bone_conduct_en = (uint8_t)bone, .eeg_adaptive = (uint8_t)adaptive };
        audio_state_event("result", cmd, (int)np_mod_audio_control(NP_HUB_SLOT_AUDIO, &p, (uint16_t)sizeof p));
    } else if (strcmp(cmd, "audio_stop") == 0) {
        audio_state_event("result", cmd, (int)np_mod_audio_control(NP_HUB_SLOT_AUDIO, NULL, 0U));
    } else if (strcmp(cmd, "audio_shutdown") == 0) {
        audio_state_event("result", cmd, (int)np_mod_audio_shutdown(NP_HUB_SLOT_AUDIO));
    } else if (strcmp(cmd, "audio_raw") == 0 && a) {
        uint8_t zeros[16];
        memset(zeros, 0, sizeof zeros);
        const unsigned len = (unsigned)atoi(a);
        audio_state_event("result", cmd, (int)np_mod_audio_control(NP_HUB_SLOT_AUDIO, zeros, (uint16_t)(len > sizeof zeros ? sizeof zeros : len)));
    } else if (strcmp(cmd, "audio_eeg_band") == 0 && a) {
        np_mod_audio_set_eeg_band((uint8_t)atoi(a));
        audio_state_event("result", cmd, 0);
    } else if (strcmp(cmd, "audio_sample") == 0) {
        np_telem_record_t rec;
        memset(&rec, 0, sizeof rec);
        const np_hub_status_t rc = np_mod_audio_telemetry(NP_HUB_SLOT_AUDIO, &rec);
        np_sim_emit("{\"ev\":\"audio_sample\",\"code\":%d,\"mod_type\":%d}", (int)rc, (int)rec.mod_type);
    } else {
        return false;
    }
    return true;
}
