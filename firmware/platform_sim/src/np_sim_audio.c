/*
 * NeurOne Firmware-in-the-Loop Simulator — audio seams (NP-SIM-FW-001 §8 step 3, REQ-SIMFW-13)
 * HOST ONLY; see np_sim_bus.h.
 *
 * Defines the eight np_mod_audio_hal_* seams of np_sw02_platform_hal.h: the codec, the planar drivers and the bone
 * transducer.  np_mod_audio.c is the device's own source.
 *
 * The model records what the driver told the output stage and nothing it would sound like: planar mode (off, binaural,
 * isochronic, pink or brown noise), carrier, beat and volume, and the bone transducer's frequency and volume.  There is
 * no sample path and no sound pressure level, so volume is a percentage the driver passed on, not a level.  The mesh
 * impedance is what the scenario gave the part, no default.
 *
 * Injection.  Five seams return nothing, so an armed response makes the CALL not happen (as for the EEG front end);
 * codec_init returns a status, so an armed response is returned.
 */
#include "np_sim_bus.h"

#include "np_sw02_platform_hal.h"

#include <math.h>
#include <stdio.h>

static np_sim_module_t *resolve(const char *op_name)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_AUDIO, 0U);
    if (m == NULL || !m->present) {
        np_sim_report_absent(NP_SIM_KIND_AUDIO, 0U, op_name);
        return NULL;
    }
    return m;
}

/* True if an armed response swallowed the call (reported); the caller then does nothing. */
static bool swallowed(np_sim_module_t *m, np_sim_op_t op, const char *args)
{
    int code;
    if (np_sim_take_response(m, op, &code)) {
        np_sim_report_hal(m, op, args, code, "injected");
        return true;
    }
    return false;
}

np_hub_status_t np_mod_audio_hal_codec_init(void)
{
    np_sim_module_t *m = resolve("audio_codec_init");
    if (m == NULL) { return NP_HUB_ERR_NOT_PRESENT; }
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_AUDIO_CODEC_INIT, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_AUDIO_CODEC_INIT, "[]", code, "injected");
        return (np_hub_status_t)code;
    }
    m->audio_inited = true;
    np_sim_report_hal(m, NP_SIM_OP_AUDIO_CODEC_INIT, "[]", (int)NP_HUB_OK, "default");
    return NP_HUB_OK;
}

void np_mod_audio_hal_set_binaural(uint16_t carrier_hz, uint16_t beat_mhz, uint8_t vol_pct)
{
    np_sim_module_t *m = resolve("audio_binaural");
    if (m == NULL) { return; }
    char args[48];
    (void)snprintf(args, sizeof args, "[%u,%u,%u]", carrier_hz, beat_mhz, vol_pct);
    if (swallowed(m, NP_SIM_OP_AUDIO_BINAURAL, args)) { return; }
    m->audio_mode = 1U; m->audio_carrier_hz = carrier_hz; m->audio_beat_mhz = beat_mhz; m->audio_vol_pct = vol_pct;
    m->driven = (vol_pct != 0U);
    np_sim_report_hal(m, NP_SIM_OP_AUDIO_BINAURAL, args, (int)NP_HUB_OK, "default");
}

void np_mod_audio_hal_set_isochronic(uint16_t freq_hz, uint8_t vol_pct)
{
    np_sim_module_t *m = resolve("audio_isochronic");
    if (m == NULL) { return; }
    char args[32];
    (void)snprintf(args, sizeof args, "[%u,%u]", freq_hz, vol_pct);
    if (swallowed(m, NP_SIM_OP_AUDIO_ISOCHRONIC, args)) { return; }
    m->audio_mode = 2U; m->audio_carrier_hz = freq_hz; m->audio_vol_pct = vol_pct;
    m->driven = (vol_pct != 0U);
    np_sim_report_hal(m, NP_SIM_OP_AUDIO_ISOCHRONIC, args, (int)NP_HUB_OK, "default");
}

void np_mod_audio_hal_set_noise(uint8_t type, uint8_t vol_pct)
{
    np_sim_module_t *m = resolve("audio_noise");
    if (m == NULL) { return; }
    char args[32];
    (void)snprintf(args, sizeof args, "[%u,%u]", type, vol_pct);
    if (swallowed(m, NP_SIM_OP_AUDIO_NOISE, args)) { return; }
    m->audio_mode = (type == 0U) ? 3U : 4U; m->audio_vol_pct = vol_pct;
    m->driven = (vol_pct != 0U);
    np_sim_report_hal(m, NP_SIM_OP_AUDIO_NOISE, args, (int)NP_HUB_OK, "default");
}

void np_mod_audio_hal_stop_planar(void)
{
    np_sim_module_t *m = resolve("audio_stop_planar");
    if (m == NULL) { return; }
    if (swallowed(m, NP_SIM_OP_AUDIO_STOP_PLANAR, "[]")) { return; }   /* a lost stop: the planar drivers keep playing */
    m->audio_mode = 0U; m->audio_vol_pct = 0U;
    m->driven = m->audio_bone_on;
    np_sim_report_hal(m, NP_SIM_OP_AUDIO_STOP_PLANAR, "[]", (int)NP_HUB_OK, "default");
}

void np_mod_audio_hal_bone_set(uint16_t freq_hz, uint8_t vol_pct)
{
    np_sim_module_t *m = resolve("audio_bone_set");
    if (m == NULL) { return; }
    char args[32];
    (void)snprintf(args, sizeof args, "[%u,%u]", freq_hz, vol_pct);
    if (swallowed(m, NP_SIM_OP_AUDIO_BONE_SET, args)) { return; }
    m->audio_bone_on = (vol_pct != 0U); m->audio_bone_hz = freq_hz; m->audio_bone_vol_pct = vol_pct;
    m->driven = m->audio_mode != 0U || m->audio_bone_on;
    np_sim_report_hal(m, NP_SIM_OP_AUDIO_BONE_SET, args, (int)NP_HUB_OK, "default");
}

void np_mod_audio_hal_bone_stop(void)
{
    np_sim_module_t *m = resolve("audio_bone_stop");
    if (m == NULL) { return; }
    if (swallowed(m, NP_SIM_OP_AUDIO_BONE_STOP, "[]")) { return; }
    m->audio_bone_on = false; m->audio_bone_vol_pct = 0U;
    m->driven = m->audio_mode != 0U;
    np_sim_report_hal(m, NP_SIM_OP_AUDIO_BONE_STOP, "[]", (int)NP_HUB_OK, "default");
}

float np_mod_audio_hal_mesh_impedance(void)
{
    np_sim_module_t *m = resolve("audio_mesh_impedance");
    if (m == NULL) { return NAN; }
    int code;
    if (np_sim_take_response(m, NP_SIM_OP_AUDIO_MESH_IMPEDANCE, &code)) {
        np_sim_report_hal(m, NP_SIM_OP_AUDIO_MESH_IMPEDANCE, "[]", code, "injected");
        return NAN;
    }
    const np_sim_override_t *o = &m->ov[NP_SIM_OP_AUDIO_MESH_IMPEDANCE];
    const float v = o->has_reading ? o->reading : m->audio_mesh_ohm;
    char args[32];
    (void)snprintf(args, sizeof args, "[%.1f]", (double)v);
    np_sim_report_hal(m, NP_SIM_OP_AUDIO_MESH_IMPEDANCE, args, (int)NP_HUB_OK, o->has_reading ? "injected" : "default");
    return v;
}
