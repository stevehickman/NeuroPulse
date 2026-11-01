/*
 * NeurOne Firmware-in-the-Loop Simulator — module bus (NP-SIM-FW-001 Rev 2 §3.2)
 * HOST ONLY; see np_sim_bus.h.
 */
#include "np_sim_bus.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static np_sim_module_t g_mod[NP_SIM_MAX_MODULES];
static np_sim_sink_fn  g_sink;

static const char *const K_KIND_NAME[NP_SIM_KIND_COUNT] = { "pbm_base" };
static const char *const K_OP_NAME[NP_SIM_OP_COUNT]     = { "socket_pwm_set", "socket_ntc_read" };

const char *np_sim_kind_name(np_sim_kind_t k)
{
    return ((unsigned)k < NP_SIM_KIND_COUNT) ? K_KIND_NAME[k] : "?";
}

const char *np_sim_op_name(np_sim_op_t op)
{
    return ((unsigned)op < NP_SIM_OP_COUNT) ? K_OP_NAME[op] : "?";
}

uint32_t np_sim_now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)((uint64_t)t.tv_sec * 1000U + (uint64_t)t.tv_nsec / 1000000U);
}

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

np_sim_module_t *np_sim_add(np_sim_kind_t kind, uint16_t index, float ntc_c)
{
    if (index >= NP_SIM_MAX_MODULES || (unsigned)kind >= NP_SIM_KIND_COUNT) { return NULL; }
    np_sim_module_t *m = &g_mod[index];
    memset(m, 0, sizeof *m);
    m->used    = true;
    m->kind    = kind;
    m->index   = index;
    m->present = true;
    m->ntc_c   = ntc_c;
    np_sim_emit("{\"ev\":\"module\",\"id\":\"%s:%u\",\"kind\":\"%s\",\"index\":%u,\"present\":true,\"ntc_c\":%.1f}",
                np_sim_kind_name(kind), (unsigned)index, np_sim_kind_name(kind),
                (unsigned)index, (double)ntc_c);
    return m;
}

np_sim_module_t *np_sim_find(uint16_t index)
{
    if (index >= NP_SIM_MAX_MODULES || !g_mod[index].used) { return NULL; }
    return &g_mod[index];
}

static void report_inject(const np_sim_module_t *m, const char *what)
{
    np_sim_emit("{\"ev\":\"inject\",\"id\":\"%s:%u\",\"what\":\"%s\"}",
                np_sim_kind_name(m->kind), (unsigned)m->index, what);
}

bool np_sim_set_response(uint16_t index, np_sim_op_t op, int code, int times)
{
    np_sim_module_t *m = np_sim_find(index);
    if (m == NULL || (unsigned)op >= NP_SIM_OP_COUNT) { return false; }
    m->ov[op].active = true;
    m->ov[op].code   = code;
    m->ov[op].times  = times;
    report_inject(m, np_sim_op_name(op));
    return true;
}

bool np_sim_set_reading(uint16_t index, np_sim_op_t op, float value)
{
    np_sim_module_t *m = np_sim_find(index);
    if (m == NULL || (unsigned)op >= NP_SIM_OP_COUNT) { return false; }
    m->ov[op].has_reading = true;
    m->ov[op].reading     = value;
    report_inject(m, np_sim_op_name(op));
    return true;
}

bool np_sim_set_present(uint16_t index, bool present)
{
    np_sim_module_t *m = np_sim_find(index);
    if (m == NULL) { return false; }
    m->present = present;
    report_inject(m, present ? "present" : "absent");
    return true;
}

bool np_sim_clear(uint16_t index)
{
    np_sim_module_t *m = np_sim_find(index);
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
    *code = o->code;
    if (o->times > 0 && --o->times == 0) { o->active = false; }
    return true;
}
