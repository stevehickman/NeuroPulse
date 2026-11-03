/*
 * NeurOne Firmware-in-the-Loop Simulator — module bus (NP-SIM-FW-001 §3.2)
 * HOST ONLY; see np_sim_bus.h.
 */
#include "np_sim_bus.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static np_sim_module_t g_mod[NP_SIM_KIND_COUNT][NP_SIM_MAX_MODULES];
static np_sim_sink_fn  g_sink;

static const char *const K_KIND_NAME[NP_SIM_KIND_COUNT] = { "pbm_base", "stim_pair", "eeg", "vns", "audio", "visual", "intranasal" };
static const char *const K_OP_NAME[NP_SIM_OP_COUNT]     = {
    "socket_pwm_set", "socket_ntc_read",
    "stim_dac_set", "stim_dac_stop", "stim_dc_set", "stim_dc_ramp",
    "stim_read_impedance", "stim_read_current", "stim_read_charge",
    "eeg_spi_write", "eeg_spi_read", "eeg_start", "eeg_stop", "eeg_read_impedance",
    "eeg_band_power", "eeg_dominant_band", "eeg_self_calibrate",
    "vns_impedance", "vns_stim_set", "vns_stim_stop", "vns_ppg_start", "vns_ppg_stop", "vns_eeg_ref",
    "vns_rmssd", "vns_coherence", "vns_hr",
    "audio_codec_init", "audio_binaural", "audio_isochronic", "audio_noise", "audio_stop_planar",
    "audio_bone_set", "audio_bone_stop", "audio_mesh_impedance",
    "vis_led_set", "vis_led_stop", "vis_eye_open", "vis_seated", "vis_emdr", "vis_mpe", "vis_hall_register",
    "ins_auth", "ins_contact", "ins_pwm_set", "ins_pwm_stop", "ins_pd_read"
};

const char *np_sim_kind_name(np_sim_kind_t k)
{
    return ((unsigned)k < NP_SIM_KIND_COUNT) ? K_KIND_NAME[k] : "?";
}

const char *np_sim_op_name(np_sim_op_t op)
{
    return ((unsigned)op < NP_SIM_OP_COUNT) ? K_OP_NAME[op] : "?";
}

static bool     g_virtual;
static uint32_t g_vnow_ms;

static uint32_t real_now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)((uint64_t)t.tv_sec * 1000U + (uint64_t)t.tv_nsec / 1000000U);
}

np_sim_kind_t np_sim_op_kind(np_sim_op_t op)
{
    if (op >= NP_SIM_OP_INS_AUTH)       { return NP_SIM_KIND_INTRANASAL; }
    if (op >= NP_SIM_OP_VIS_LED_SET)    { return NP_SIM_KIND_VISUAL; }
    if (op >= NP_SIM_OP_AUDIO_CODEC_INIT) { return NP_SIM_KIND_AUDIO; }
    if (op >= NP_SIM_OP_VNS_IMPEDANCE)  { return NP_SIM_KIND_VNS; }
    if (op >= NP_SIM_OP_EEG_SPI_WRITE)  { return NP_SIM_KIND_EEG; }
    if (op >= NP_SIM_OP_STIM_DAC_SET)   { return NP_SIM_KIND_STIM_PAIR; }
    return NP_SIM_KIND_PBM_BASE;
}

int np_sim_op_from_name(const char *name)
{
    for (int i = 0; i < (int)NP_SIM_OP_COUNT; i++) {
        if (strcmp(name, K_OP_NAME[i]) == 0) { return i; }
    }
    return -1;
}

uint32_t np_sim_now_ms(void) { return g_virtual ? g_vnow_ms : real_now_ms(); }

void np_sim_advance_ms(uint32_t ms)
{
    if (!g_virtual) { g_virtual = true; g_vnow_ms = real_now_ms(); }
    g_vnow_ms += ms;
}

void np_sim_use_real_clock(void) { g_virtual = false; }

void np_sim_set_sink(np_sim_sink_fn fn) { g_sink = fn; }

static void default_sink(const char *line)
{
    fputs(line, stdout);
    fputc('\n', stdout);
    fflush(stdout);
}

void np_sim_emit(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    (void)vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    (g_sink != NULL ? g_sink : default_sink)(buf);
}

void np_sim_reset(void)
{
    memset(g_mod, 0, sizeof g_mod);
}

np_sim_module_t *np_sim_add(np_sim_kind_t kind, uint16_t index, float value)
{
    if (index >= NP_SIM_MAX_MODULES || (unsigned)kind >= NP_SIM_KIND_COUNT) { return NULL; }
    np_sim_module_t *m = &g_mod[kind][index];
    memset(m, 0, sizeof *m);
    m->used    = true;
    m->kind    = kind;
    m->index   = index;
    m->present = true;
    if (kind == NP_SIM_KIND_STIM_PAIR) {
        m->impedance_kohm = value;
        np_sim_emit("{\"ev\":\"module\",\"id\":\"%s:%u\",\"kind\":\"%s\",\"index\":%u,\"present\":true,"
                    "\"impedance_kohm\":%.1f}",
                    np_sim_kind_name(kind), (unsigned)index, np_sim_kind_name(kind), (unsigned)index, (double)value);
    } else if (kind == NP_SIM_KIND_INTRANASAL || kind == NP_SIM_KIND_VISUAL) {
        np_sim_emit("{\"ev\":\"module\",\"id\":\"%s:%u\",\"kind\":\"%s\",\"index\":%u,\"present\":true}",
                    np_sim_kind_name(kind), (unsigned)index, np_sim_kind_name(kind), (unsigned)index);
    } else if (kind == NP_SIM_KIND_AUDIO) {
        m->audio_mesh_ohm = value;
        np_sim_emit("{\"ev\":\"module\",\"id\":\"%s:%u\",\"kind\":\"%s\",\"index\":%u,\"present\":true,"
                    "\"mesh_ohm\":%.0f}",
                    np_sim_kind_name(kind), (unsigned)index, np_sim_kind_name(kind), (unsigned)index, (double)value);
    } else if (kind == NP_SIM_KIND_VNS) {
        m->vns_impedance_ohm = value;
        np_sim_emit("{\"ev\":\"module\",\"id\":\"%s:%u\",\"kind\":\"%s\",\"index\":%u,\"present\":true,"
                    "\"impedance_ohm\":%.0f}",
                    np_sim_kind_name(kind), (unsigned)index, np_sim_kind_name(kind), (unsigned)index, (double)value);
    } else if (kind == NP_SIM_KIND_EEG) {
        np_sim_emit("{\"ev\":\"module\",\"id\":\"%s:%u\",\"kind\":\"%s\",\"index\":%u,\"present\":true}",
                    np_sim_kind_name(kind), (unsigned)index, np_sim_kind_name(kind), (unsigned)index);
    } else {
        m->ntc_c = value;
        np_sim_emit("{\"ev\":\"module\",\"id\":\"%s:%u\",\"kind\":\"%s\",\"index\":%u,\"present\":true,\"ntc_c\":%.1f}",
                    np_sim_kind_name(kind), (unsigned)index, np_sim_kind_name(kind), (unsigned)index, (double)value);
    }
    return m;
}

np_sim_module_t *np_sim_find(np_sim_kind_t kind, uint16_t index)
{
    if ((unsigned)kind >= NP_SIM_KIND_COUNT || index >= NP_SIM_MAX_MODULES || !g_mod[kind][index].used) { return NULL; }
    return &g_mod[kind][index];
}

static void report_inject(const np_sim_module_t *m, const char *what)
{
    np_sim_emit("{\"ev\":\"inject\",\"id\":\"%s:%u\",\"what\":\"%s\"}",
                np_sim_kind_name(m->kind), (unsigned)m->index, what);
}

bool np_sim_set_response(np_sim_kind_t kind, uint16_t index, np_sim_op_t op, int code, int times)
{
    return np_sim_set_response_after(kind, index, op, code, times, 0);
}

bool np_sim_set_response_after(np_sim_kind_t kind, uint16_t index, np_sim_op_t op, int code, int times, int skip)
{
    np_sim_module_t *m = np_sim_find(kind, index);
    if (m == NULL || (unsigned)op >= NP_SIM_OP_COUNT) { return false; }
    m->ov[op].active = true;
    m->ov[op].code   = code;
    m->ov[op].times  = times;
    m->ov[op].skip   = skip > 0 ? skip : 0;
    report_inject(m, np_sim_op_name(op));
    return true;
}

bool np_sim_set_reading(np_sim_kind_t kind, uint16_t index, np_sim_op_t op, float value)
{
    np_sim_module_t *m = np_sim_find(kind, index);
    if (m == NULL || (unsigned)op >= NP_SIM_OP_COUNT) { return false; }
    m->ov[op].has_reading = true;
    m->ov[op].reading     = value;
    report_inject(m, np_sim_op_name(op));
    return true;
}

bool np_sim_set_present(np_sim_kind_t kind, uint16_t index, bool present)
{
    np_sim_module_t *m = np_sim_find(kind, index);
    if (m == NULL) { return false; }
    m->present = present;
    report_inject(m, present ? "present" : "absent");
    return true;
}

bool np_sim_clear(np_sim_kind_t kind, uint16_t index)
{
    np_sim_module_t *m = np_sim_find(kind, index);
    if (m == NULL) { return false; }
    memset(m->ov, 0, sizeof m->ov);
    memset(m->answered_default, 0, sizeof m->answered_default);
    report_inject(m, "clear");
    return true;
}

bool np_sim_take_response(np_sim_module_t *m, np_sim_op_t op, int *code)
{
    np_sim_override_t *o = &m->ov[op];
    if (!o->active) { return false; }
    if (o->skip > 0) { o->skip--; return false; }       /* this call is answered normally */
    *code = o->code;
    if (o->times > 0 && --o->times == 0) { o->active = false; }
    return true;
}

void np_sim_report_hal(const np_sim_module_t *m, np_sim_op_t op, const char *args, int code, const char *source)
{
    np_sim_emit("{\"ev\":\"hal\",\"t\":%u,\"id\":\"%s:%u\",\"op\":\"%s\",\"args\":%s,"
                "\"code\":%d,\"source\":\"%s\"}",
                (unsigned)np_sim_now_ms(), np_sim_kind_name(m->kind), (unsigned)m->index,
                np_sim_op_name(op), args, code, source);
}

void np_sim_report_absent(np_sim_kind_t kind, uint16_t index, const char *op_name)
{
    np_sim_emit("{\"ev\":\"hal\",\"t\":%u,\"id\":\"%s:%u\",\"op\":\"%s\","
                "\"code\":%d,\"source\":\"absent\"}",
                (unsigned)np_sim_now_ms(), np_sim_kind_name(kind), (unsigned)index, op_name,
                (int)NP_HUB_ERR_NOT_PRESENT);
}
