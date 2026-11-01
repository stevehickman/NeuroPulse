/*
 * NeurOne Hub Control — np_runner_run() Host Tests
 *
 * The session runner's run loop: command dispatch and the implicit stops, the records it writes, the way a session ends
 * (completion, abort, fault), the graceful shutdown and its order, pause and resume, and the lease it releases last.
 * np_runner_load_tests covers the load step; until this suite nothing ran the loop.
 *
 * The loop is one thread whose only blocking call is vTaskDelay and which reads its events by polling, so it runs
 * deterministically on a VIRTUAL CLOCK: vTaskDelay advances time and fires the events a test scripted for that moment
 * (np_runner_abort(), np_runner_pause(), an event bit).  The wall clock starts well after zero so that a confusion of
 * wall time with session time shows.  Every call the loop makes to a seam is logged with its time, so order is asserted
 * against what happened and not against what the code says.
 *
 * Linked REAL: np_session_runner.c (the unit under test), np_session_lease.c, np_protocol.c, np_session_pause.c,
 * np_ramp_plan.c, np_chan_decl.c.  Doubles: the Ed25519 verifier and serial HAL (as np_protocol_tests.c does), the safety
 * MCU, the boot flag, the event group, the kernel, the module registry, the socket dispatch, the loggers, the dose model,
 * the cross-check, the consumables and the cervical VNS and tDCS ramp models.
 *
 * No hardware.  IEC 62304 Class B — SW-02 hub control.
 * Build with -DNP_BUILD_TESTS=ON (see firmware/hub_control/CMakeLists.txt).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "np_chan_decl.h"
#include "np_consumables.h"
#include "np_cvns_reenable.h"
#include "np_hub_config.h"
#include "np_hub_types.h"
#include "np_log_backend.h"
#include "np_module_registry.h"
#include "np_pause_limit.h"
#include "np_pbm_socket_telem.h"
#include "np_protocol.h"
#include "np_ramp_plan.h"
#include "np_safety_spi.h"
#include "np_session_fitting.h"
#include "np_session_lease.h"
#include "np_session_log.h"
#include "np_session_progress.h"
#include "np_session_runner.h"
#include "np_socket_dispatch.h"
#include "np_stim_xcheck.h"
#include "np_sw02_platform_hal.h"

static int g_failures;

static void check(int cond, const char *name)
{
    if (cond) {
        printf("PASS: %s\n", name);
    } else {
        printf("FAIL: %s\n", name);
        g_failures++;
    }
}

/* ── The virtual clock and the script ─────────────────────────────────────────── */

#define RUN_T0_MS          50000U      /* the wall clock when a session's run begins: never zero */
#define RUN_RUNAWAY_MS     900000U     /* a run that has not ended by here is stuck, and is stopped */
#define RUN_SCRIPT_MAX     8U
#define RUN_EVENT_MAX      1024U

typedef void (*script_fn)(void);

typedef struct {
    uint32_t  at_ms;                    /* session time (wall time minus RUN_T0_MS) */
    script_fn fn;
    bool      fired;
} script_t;

static uint32_t g_now_ms;
static script_t g_script[RUN_SCRIPT_MAX];
static unsigned g_script_n;
static bool     g_runaway;

static EventBits_t g_event_bits;

static void script_at(uint32_t session_ms, script_fn fn)
{
    g_script[g_script_n].at_ms = session_ms;
    g_script[g_script_n].fn    = fn;
    g_script[g_script_n].fired = false;
    g_script_n++;
}

/* The sleep: time passes, and whatever was scripted to happen inside it happens at its own moment, so the loop sees it
 * when it wakes and not before — which is what an event posted from another task is. */
void vTaskDelay(const TickType_t ticks)
{
    const uint32_t target = g_now_ms + (uint32_t)ticks * portTICK_PERIOD_MS;
    for (unsigned i = 0U; i < g_script_n; i++) {
        if (!g_script[i].fired && g_script[i].at_ms + RUN_T0_MS <= target) {
            g_script[i].fired = true;
            if (g_script[i].at_ms + RUN_T0_MS > g_now_ms) {
                g_now_ms = g_script[i].at_ms + RUN_T0_MS;
            }
            g_script[i].fn();
        }
    }
    g_now_ms = target;
    if (g_now_ms > RUN_T0_MS + RUN_RUNAWAY_MS && !g_runaway) {
        g_runaway = true;
        g_event_bits |= NP_EV_SESSION_ABORT;      /* end it, so the test fails instead of hanging */
    }
}

TickType_t xTaskGetTickCount(void) { return (TickType_t)g_now_ms; }

void       vPortEnterCritical(void) {}
void       vPortExitCritical(void)  {}

void np_freertos_assert_failed(const char *file, unsigned long line)
{
    printf("FAIL: configASSERT at %s:%lu\n", file, line);
    g_failures++;
}

static int g_mutex_token;

QueueHandle_t xQueueCreateMutex(const uint8_t type)           { (void)type; return (QueueHandle_t)&g_mutex_token; }
BaseType_t xQueueSemaphoreTake(QueueHandle_t q, TickType_t t) { (void)q; (void)t; return pdTRUE; }
BaseType_t xQueueGenericSend(QueueHandle_t q, const void *item, TickType_t t, const BaseType_t pos)
{
    (void)q; (void)item; (void)t; (void)pos;
    return pdTRUE;
}

EventBits_t xEventGroupSetBits(EventGroupHandle_t g, const EventBits_t bits)
{
    (void)g;
    g_event_bits |= bits;
    return g_event_bits;
}

/* xEventGroupGetBits is a macro for xEventGroupClearBits(g, 0), so this is the runner's read as well. */
EventBits_t xEventGroupClearBits(EventGroupHandle_t g, const EventBits_t bits)
{
    (void)g;
    g_event_bits &= ~bits;
    return g_event_bits;
}

/* ── The log of what the loop did ─────────────────────────────────────────────── */

typedef enum {
    RUN_EV_LOG_START,       /* np_log_session_start */
    RUN_EV_LOG_FITTING,     /* np_log_session_fitting */
    RUN_EV_LOG_CMD,         /* a = slot, b = accepted */
    RUN_EV_CONTROL,         /* a = slot, b = params_len (0 is a stop) */
    RUN_EV_ENABLE,          /* a = channel mask requested */
    RUN_EV_DISABLE,         /* a = channel mask withdrawn */
    RUN_EV_DISABLE_ALL,
    RUN_EV_SOCK_CMD,
    RUN_EV_SOCK_STOP_ALL,
    RUN_EV_SOCK_PAUSE,
    RUN_EV_SOCK_RESUME,
    RUN_EV_LOG_END,
    RUN_EV_FLUSH,
    RUN_EV_CONS_END,        /* a = mods_active_mask */
    RUN_EV_PROGRESS,
    RUN_EV_LOG_PAUSE,       /* a = paused */
    RUN_EV_SHDR_FAULT,      /* a = slot, b = fault code */
    RUN_EV_TELEM,           /* a = slot */
    RUN_EV_PST_TELEM,
    RUN_EV_GEOM_BES,
    RUN_EV_GEOM_TDCS,
    RUN_EV_GEOM_CLIN,
    RUN_EV_SEND_LIMITS,
    RUN_EV_SEND_WAVE
} run_ev_kind_t;

typedef struct {
    run_ev_kind_t kind;
    uint32_t      at_ms;     /* session time */
    uint32_t      a, b;
} run_ev_t;

static run_ev_t g_log[RUN_EVENT_MAX];
static unsigned g_log_n;

static void note(run_ev_kind_t kind, uint32_t a, uint32_t b)
{
    if (g_log_n < RUN_EVENT_MAX) {
        g_log[g_log_n].kind  = kind;
        g_log[g_log_n].at_ms = g_now_ms - RUN_T0_MS;
        g_log[g_log_n].a     = a;
        g_log[g_log_n].b     = b;
        g_log_n++;
    }
}

static unsigned count_of(run_ev_kind_t kind)
{
    unsigned n = 0U;
    for (unsigned i = 0U; i < g_log_n; i++) { if (g_log[i].kind == kind) { n++; } }
    return n;
}

/* The index of the n-th (0-based) event of `kind` whose `a` is `a`, or -1. */
static int find_a(run_ev_kind_t kind, uint32_t a, unsigned n)
{
    for (unsigned i = 0U; i < g_log_n; i++) {
        if (g_log[i].kind == kind && g_log[i].a == a) {
            if (n == 0U) { return (int)i; }
            n--;
        }
    }
    return -1;
}

static int first_of(run_ev_kind_t kind)
{
    for (unsigned i = 0U; i < g_log_n; i++) { if (g_log[i].kind == kind) { return (int)i; } }
    return -1;
}

static int last_of(run_ev_kind_t kind)
{
    for (int i = (int)g_log_n - 1; i >= 0; i--) { if (g_log[i].kind == kind) { return i; } }
    return -1;
}

/* ── What the doubles report, and how they behave ─────────────────────────────── */

static bool    g_admits_sessions;
static uint8_t g_tier;
static uint8_t g_hw_version;
static int     g_verify_result;

static np_hub_status_t g_control_rc[NP_HUB_SLOT_MAX];
static bool            g_present[NP_HUB_SLOT_MAX];
static np_mod_entry_t  g_mods[NP_HUB_SLOT_MAX];
static np_hub_status_t g_sock_rc;
static uint16_t        g_sock_unrestored;
static unsigned        g_diverge_on_call;     /* the np_stim_xcheck_observe() call that reports a divergence; 0 never */
static unsigned        g_observe_calls;
static uint32_t        g_pause_limit_s;

/* What the loop wrote at its end, captured when it was written. */
static np_session_uhdr_record_t g_end_uhdr;
static np_session_shdr_record_t g_end_shdr;
static bool                     g_lease_held_at_end;
static bool                     g_lease_held_at_flush;
static bool                     g_lease_held_at_cons;
static uint32_t                 g_cons_mask;
static np_chan_decl_t           g_unused_decl;

np_hub_status_t np_proto_hal_get_device_serial(uint8_t *buf, size_t len)
{
    (void)buf; (void)len;
    return NP_HUB_ERR_NOT_PRESENT;
}

np_hub_status_t np_proto_hal_get_proto_pubkey(uint8_t *pub_key_out)
{
    memset(pub_key_out, 0, 32);
    return NP_HUB_OK;
}

int np_ed25519_verify(const uint8_t *msg, size_t msg_len, const uint8_t *sig, const uint8_t *pub_key)
{
    (void)msg; (void)msg_len; (void)sig; (void)pub_key;
    return g_verify_result;
}

uint8_t np_safety_spi_get_tier(void)             { return g_tier; }
uint8_t np_safety_spi_get_hardware_version(void) { return g_hw_version; }

bool np_safety_spi_get_tier_report(uint8_t *reason_out, bool *refused_out)
{
    if (reason_out != NULL)  { *reason_out = 0U; }
    if (refused_out != NULL) { *refused_out = false; }
    return true;
}

void np_safety_spi_request_enable(uint16_t channel_mask)  { note(RUN_EV_ENABLE, channel_mask, 0U); }
void np_safety_spi_request_disable(uint16_t channel_mask) { note(RUN_EV_DISABLE, channel_mask, 0U); }
void np_safety_spi_disable_all(void)                      { note(RUN_EV_DISABLE_ALL, 0U, 0U); }
void np_safety_spi_set_geom_required(bool v)              { if (v) { note(RUN_EV_GEOM_CLIN, 0U, 0U); } }
void np_safety_spi_set_geom_required_tdcs(bool v)         { if (v) { note(RUN_EV_GEOM_TDCS, 0U, 0U); } }
void np_safety_spi_set_geom_required_bes(bool v)          { if (v) { note(RUN_EV_GEOM_BES, 0U, 0U); } }

np_hub_status_t np_safety_spi_send_channel_limits(const uint16_t *area_mcm2, uint8_t count)
{
    (void)area_mcm2; (void)count;
    note(RUN_EV_SEND_LIMITS, 0U, 0U);
    return NP_HUB_OK;
}

np_hub_status_t np_safety_spi_send_channel_waveforms(const uint8_t *wave_class, const uint32_t *phase_us, uint8_t count)
{
    (void)wave_class; (void)phase_us; (void)count;
    note(RUN_EV_SEND_WAVE, 0U, 0U);
    return NP_HUB_OK;
}

bool np_log_backend_admits_sessions(void) { return g_admits_sessions; }

/* ── The modules ─────────────────────────────────────────────────────────────── */

static np_hub_status_t mod_control(uint8_t slot, const void *params, uint16_t params_len)
{
    (void)params;
    note(RUN_EV_CONTROL, slot, params_len);
    return g_control_rc[slot];
}

static np_hub_status_t mod_telemetry(uint8_t slot, np_telem_record_t *out)
{
    note(RUN_EV_TELEM, slot, 0U);
    memset(out, 0, sizeof *out);
    out->slot = slot;
    return NP_HUB_OK;
}

np_mod_entry_t *np_mod_reg_get(uint8_t slot)
{
    return (slot < NP_HUB_SLOT_MAX && g_present[slot]) ? &g_mods[slot] : NULL;
}

static void module_present(uint8_t slot)
{
    g_present[slot]         = true;
    g_mods[slot].slot       = slot;
    g_mods[slot].present    = true;
    g_mods[slot].control    = mod_control;
    g_mods[slot].telemetry  = mod_telemetry;
}

/* ── The loggers ─────────────────────────────────────────────────────────────── */

void np_log_session_start(const np_session_uhdr_record_t *rec) { (void)rec; note(RUN_EV_LOG_START, 0U, 0U); }
void np_log_session_fitting(const uint8_t *frame, size_t len)  { (void)frame; (void)len; note(RUN_EV_LOG_FITTING, 0U, 0U); }
size_t np_fitting_build(uint8_t hw, uint8_t *out, size_t cap)  { (void)hw; (void)out; (void)cap; return 0U; }

void np_log_command(const np_session_cmd_t *cmd, uint32_t session_ms, bool accepted)
{
    (void)session_ms;
    note(RUN_EV_LOG_CMD, cmd->slot_id, accepted ? 1U : 0U);
}

void np_log_session_pause(uint32_t session_ms, bool paused, uint16_t unrestored)
{
    (void)session_ms; (void)unrestored;
    note(RUN_EV_LOG_PAUSE, paused ? 1U : 0U, 0U);
}

void np_log_session_end(const np_session_uhdr_record_t *u, const np_session_shdr_record_t *s)
{
    g_end_uhdr = *u;
    g_end_shdr = *s;
    g_lease_held_at_end = np_lease_held();
    note(RUN_EV_LOG_END, 0U, 0U);
}

void np_log_flush(void)
{
    g_lease_held_at_flush = np_lease_held();
    note(RUN_EV_FLUSH, 0U, 0U);
}

void np_log_shdr_fault(uint8_t slot, np_hub_mod_type_t type, uint8_t fault_code, uint32_t session_ms)
{
    (void)type; (void)session_ms;
    note(RUN_EV_SHDR_FAULT, slot, fault_code);
}

void np_log_shdr_pbm_session_counts(const np_pst_counts_t *counts) { (void)counts; }
void np_log_telemetry(const np_telem_record_t *rec)               { (void)rec; }

void np_cons_on_session_end(uint32_t mods_active_mask)
{
    g_cons_mask = mods_active_mask;
    g_lease_held_at_cons = np_lease_held();
    note(RUN_EV_CONS_END, mods_active_mask, 0U);
}

void np_progress_notify(void) { note(RUN_EV_PROGRESS, 0U, 0U); }

static np_progress_in_t g_progress;

void np_progress_build(const np_progress_in_t *in, uint8_t out[NP_PROGRESS_LEN])
{
    g_progress = *in;
    memset(out, 0, NP_PROGRESS_LEN);
}

/* ── The socket dispatch, the dose model, the cross-check, the pause limit ───── */

void np_sock_disp_reset(void)   {}
void np_pst_session_reset(void) {}

np_hub_status_t np_sock_disp_command(const np_session_cmd_t *cmd)
{
    (void)cmd;
    note(RUN_EV_SOCK_CMD, 0U, 0U);
    return g_sock_rc;
}

void     np_sock_disp_process_stops(uint32_t now_ms) { (void)now_ms; }
uint32_t np_sock_disp_next_stop_ms(uint32_t now_ms)  { (void)now_ms; return 0U; }
void     np_sock_disp_stop_all(void)                 { note(RUN_EV_SOCK_STOP_ALL, 0U, 0U); }
void     np_sock_disp_pause(void)                    { note(RUN_EV_SOCK_PAUSE, 0U, 0U); }
uint16_t np_sock_disp_resume(void)                   { note(RUN_EV_SOCK_RESUME, 0U, 0U); return g_sock_unrestored; }

void np_pst_dose_poll(uint32_t now_ms)    { (void)now_ms; }
void np_pst_log_telemetry(uint32_t ms)    { (void)ms; note(RUN_EV_PST_TELEM, 0U, 0U); }
void np_pst_get_counts(np_pst_counts_t *out) { memset(out, 0, sizeof *out); }

void np_stim_xcheck_reset(void) { g_observe_calls = 0U; }

bool np_stim_xcheck_observe(const np_telem_record_t *rec, uint32_t now_ms)
{
    (void)rec; (void)now_ms;
    g_observe_calls++;
    return g_diverge_on_call != 0U && g_observe_calls == g_diverge_on_call;
}

uint32_t np_pause_limit_seconds(void) { return g_pause_limit_s; }

void np_cvns_reenable_session_reset(void) {}

/* ── The cervical VNS and tDCS ramp models: idle ─────────────────────────────── */

bool     np_mod_cvns_active(void)                                 { return false; }
void     np_mod_cvns_tick(uint32_t now_ms, uint32_t now_s)        { (void)now_ms; (void)now_s; }
void     np_mod_cvns_suspend(void)                                {}
void     np_mod_cvns_ramp(uint32_t now_ms, np_ramp_report_t *out) { (void)now_ms; memset(out, 0, sizeof *out); }
uint32_t np_mod_cvns_ramp_up_ms(void)                             { return 0U; }
uint32_t np_mod_cvns_hal_now_ms(void)                             { return 0U; }
uint32_t np_mod_cvns_hal_now_unix(void)                           { return 0U; }
void     np_mod_stim_tick(uint32_t wall_ms)                       { (void)wall_ms; }
bool     np_mod_stim_tdcs_pause(void)                             { return false; }
void     np_mod_stim_tdcs_ramp(np_ramp_report_t *out)             { memset(out, 0, sizeof *out); }
uint32_t np_mod_stim_tdcs_ramp_up_ms(const void *p, uint16_t len) { (void)p; (void)len; return 0U; }

/* ── Blob builder ─────────────────────────────────────────────────────────────
 * Explicit byte offsets, as np_protocol_tests.c does, so a layout change is caught here and not mirrored. */

#define RUN_BLOB_CAP 1024

typedef struct {
    uint8_t buf[RUN_BLOB_CAP];
    size_t  len;
    uint8_t cmd_count;
} blob_t;

static void blob_begin(blob_t *b, uint32_t duration_ms)
{
    memset(b, 0, sizeof *b);
    const uint32_t magic = NP_HUB_PROTO_MAGIC;
    const uint16_t version = NP_HUB_PROTO_VERSION;
    const uint32_t compiled_at = 1700000000UL;
    memcpy(b->buf + 0, &magic, 4);
    memcpy(b->buf + 4, &version, 2);
    memcpy(b->buf + 24, &compiled_at, 4);
    memcpy(b->buf + 60, &duration_ms, 4);
    b->buf[64] = 1U;                            /* hardware_version */
    b->len = 68U;
}

static void blob_cmd(blob_t *b, uint8_t mod_type, uint8_t slot_id, uint32_t start_ms, uint32_t duration_ms,
                     uint8_t target_kind, const uint8_t *target, uint8_t target_len,
                     const uint8_t *params, uint16_t params_len)
{
    uint8_t *p = b->buf + b->len;
    p[0] = mod_type;
    p[1] = slot_id;
    memcpy(p + 2, &start_ms, 4);
    memcpy(p + 6, &duration_ms, 4);
    memcpy(p + 10, &params_len, 2);
    p[12] = target_kind;
    p[13] = target_len;
    b->len += 14U;
    if (target_len > 0U) { memcpy(b->buf + b->len, target, target_len); b->len += target_len; }
    if (params_len > 0U) { memcpy(b->buf + b->len, params, params_len); b->len += params_len; }
    b->cmd_count++;
}

static void blob_slot(blob_t *b, uint8_t mod_type, uint8_t slot_id, uint32_t start_ms, uint32_t duration_ms,
                      const uint8_t *params, uint16_t params_len)
{
    blob_cmd(b, mod_type, slot_id, start_ms, duration_ms, (uint8_t)NP_PROTO_TARGET_SLOT, NULL, 0U, params, params_len);
}

static size_t blob_finish(blob_t *b)
{
    b->buf[7] = b->cmd_count;
    memset(b->buf + b->len, 0, NP_HUB_PROTO_SIG_LEN);
    b->len += NP_HUB_PROTO_SIG_LEN;
    return b->len;
}

#define RUN_EEG_PARAMS_LEN   5U
#define RUN_GATED_PARAMS_LEN 8U

static const uint8_t RUN_EEG_PARAMS[RUN_EEG_PARAMS_LEN]     = { 0xFF, 6, 2, 0, 3 };
static const uint8_t RUN_GATED_PARAMS[RUN_GATED_PARAMS_LEN] = { 0, 0x90, 0x01, 0xE8, 0x03, 50, 0, 0 };

/* ── Harness ──────────────────────────────────────────────────────────────────── */

static int g_events_token;

static uint32_t       g_post_bits;
static np_hub_status_t g_pause_rc, g_resume_rc;
static bool           g_mid_lease;
static np_session_state_t g_mid_state;
static uint32_t       g_mid_elapsed[4];
static unsigned       g_mid_n;

static void fresh(void)
{
    g_mid_n = 0U;
    g_mid_lease = false;
    g_mid_state = NP_SESSION_IDLE;
    g_pause_rc = g_resume_rc = NP_HUB_OK;
    g_now_ms = RUN_T0_MS;
    g_script_n = 0U;
    g_runaway = false;
    g_event_bits = 0U;
    g_log_n = 0U;
    g_admits_sessions = true;
    g_tier = NP_TIER_T1;
    g_hw_version = 1U;
    g_verify_result = 0;
    memset(g_present, 0, sizeof g_present);
    memset(g_mods, 0, sizeof g_mods);
    for (unsigned i = 0U; i < NP_HUB_SLOT_MAX; i++) { g_control_rc[i] = NP_HUB_OK; }
    g_sock_rc = NP_HUB_OK;
    g_sock_unrestored = 0U;
    g_diverge_on_call = 0U;
    g_observe_calls = 0U;
    g_pause_limit_s = 0U;
    memset(&g_end_uhdr, 0, sizeof g_end_uhdr);
    memset(&g_end_shdr, 0, sizeof g_end_shdr);
    g_lease_held_at_end = g_lease_held_at_flush = g_lease_held_at_cons = false;
    g_cons_mask = 0U;
    (void)g_unused_decl;
    np_runner_init((EventGroupHandle_t)&g_events_token);
}

/* Load a protocol and run it, as task_hub_control does: a load posts the start event, the task consumes it, then runs. */
static np_hub_status_t load_and_run(blob_t *b)
{
    const size_t len = blob_finish(b);
    const np_hub_status_t rc = np_runner_load(b->buf, len);
    check(rc == NP_HUB_OK, "setup: the protocol loads");
    g_event_bits &= ~NP_EV_SESSION_START;
    return np_runner_run();
}

/* The session time of the first logged event of this kind and `a`, or UINT32_MAX. */
static uint32_t when_a(run_ev_kind_t kind, uint32_t a, unsigned n)
{
    const int i = find_a(kind, a, n);
    return i < 0 ? UINT32_MAX : g_log[i].at_ms;
}

static bool within(uint32_t t, uint32_t lo, uint32_t slack) { return t != UINT32_MAX && t >= lo && t <= lo + slack; }

#define RUN_SLACK_MS NP_RUNNER_TICK_MS

/* A session of two commands, the common case: EEG from 0 for 4 s, the goggles from 1 s for 2 s, ending at 6 s. (EEG is passive and the
 * goggles are safety-MCU gated; audio is not gated either, NP_SAFETY_EN_AUDIO is 0.) */
static void two_command_session(blob_t *b)
{
    blob_begin(b, 6000U);
    blob_slot(b, NP_MOD_EEG, NP_HUB_SLOT_EEG, 0U, 4000U, RUN_EEG_PARAMS, RUN_EEG_PARAMS_LEN);
    blob_slot(b, NP_MOD_VISUAL, NP_HUB_SLOT_VISUAL, 1000U, 2000U, RUN_GATED_PARAMS, RUN_GATED_PARAMS_LEN);
    module_present(NP_HUB_SLOT_EEG);
    module_present(NP_HUB_SLOT_VISUAL);
}

/* ── Tests ────────────────────────────────────────────────────────────────────── */

/* @verifies REQ-FWHUB-39 */
static void test_a_session_runs_to_its_duration_and_completes(void)
{
    fresh();
    blob_t b;
    two_command_session(&b);
    const np_hub_status_t rc = load_and_run(&b);

    check(rc == NP_HUB_OK, "complete: the run returns OK");
    check(np_runner_get_state() == NP_SESSION_COMPLETE, "complete: the final state is COMPLETE");
    check(!g_runaway, "complete: the loop ended by itself");
    check(g_end_uhdr.abort_reason == NP_ABORT_NONE && g_end_shdr.abort_reason == NP_ABORT_NONE,
          "complete: no abort reason in either record");

    check(within(when_a(RUN_EV_CONTROL, NP_HUB_SLOT_EEG, 0), 0U, RUN_SLACK_MS), "complete: EEG starts at its start time");
    check(within(when_a(RUN_EV_CONTROL, NP_HUB_SLOT_VISUAL, 0), 1000U, RUN_SLACK_MS), "complete: the goggles start at 1 s");
    check(within(when_a(RUN_EV_CONTROL, NP_HUB_SLOT_VISUAL, 1), 3000U, RUN_SLACK_MS),
          "complete: the goggles' implicit stop comes at start + duration");
    check(within(when_a(RUN_EV_CONTROL, NP_HUB_SLOT_EEG, 1), 4000U, RUN_SLACK_MS), "complete: EEG's implicit stop comes at 4 s");

    check(count_of(RUN_EV_ENABLE) == 1U && find_a(RUN_EV_ENABLE, NP_SAFETY_EN_VISUAL, 0) >= 0,
          "complete: only the goggles ask the safety MCU for an enable (EEG is passive)");
    check(within(when_a(RUN_EV_ENABLE, NP_SAFETY_EN_VISUAL, 0), 1000U, RUN_SLACK_MS)
          && within(when_a(RUN_EV_DISABLE, NP_SAFETY_EN_VISUAL, 0), 3000U, RUN_SLACK_MS),
          "complete: the enable is requested at the start and withdrawn at the stop");

    check(count_of(RUN_EV_LOG_CMD) == 2U && g_log[first_of(RUN_EV_LOG_CMD)].b == 1U, "complete: both commands logged, accepted");
    check(g_end_uhdr.mods_active_mask == ((1U << NP_MOD_EEG) | (1U << NP_MOD_VISUAL)),
          "complete: the delivered mask names EEG and the goggles");
    check(g_cons_mask == g_end_uhdr.mods_active_mask, "complete: the consumables hear the same mask");
    check(count_of(RUN_EV_LOG_END) == 1U && count_of(RUN_EV_FLUSH) == 1U && count_of(RUN_EV_CONS_END) == 1U,
          "complete: the session is closed, flushed and counted once");
}

/* @verifies REQ-FWHUB-40 SWR-HV-02 */
static void test_the_session_start_record_and_the_fitting_come_before_any_command(void)
{
    fresh();
    blob_t b;
    two_command_session(&b);
    (void)load_and_run(&b);

    const int start = first_of(RUN_EV_LOG_START);
    const int fitting = first_of(RUN_EV_LOG_FITTING);
    const int cmd = first_of(RUN_EV_LOG_CMD);
    const int ctl = first_of(RUN_EV_CONTROL);
    check(start >= 0 && fitting > start, "start: the session-start record, then the fitting");
    check(cmd > fitting && ctl > fitting, "start: no command is logged or dispatched before the fitting is written");
    check(count_of(RUN_EV_LOG_START) == 1U && count_of(RUN_EV_LOG_FITTING) == 1U, "start: each is written once");
}

/* @verifies REQ-FWHUB-39 */
static void test_a_refused_command_is_logged_refused_and_not_counted_delivered(void)
{
    fresh();
    blob_t b;
    two_command_session(&b);
    g_control_rc[NP_HUB_SLOT_VISUAL] = NP_HUB_ERR_MOD_FAULT;
    const np_hub_status_t rc = load_and_run(&b);

    const int audio = find_a(RUN_EV_LOG_CMD, NP_HUB_SLOT_VISUAL, 0);
    check(audio >= 0 && g_log[audio].b == 0U, "refused: the goggle command is logged, marked refused");
    check(find_a(RUN_EV_ENABLE, NP_SAFETY_EN_VISUAL, 0) < 0, "refused: no enable is requested for a command the module refused");
    check(g_end_uhdr.mods_active_mask == (1U << NP_MOD_EEG), "refused: only EEG is in the delivered mask");
    check(g_end_uhdr.abort_reason == NP_ABORT_MOD_FAULT, "refused: the session records the module fault");
    check(within(when_a(RUN_EV_CONTROL, NP_HUB_SLOT_EEG, 1), 4000U, RUN_SLACK_MS),
          "refused: the session carries on, and EEG still stops on time");
    check(first_of(RUN_EV_DISABLE_ALL) >= 0 && within(g_log[first_of(RUN_EV_DISABLE_ALL)].at_ms, 6000U, RUN_SLACK_MS),
          "refused: and runs to its duration before it shuts down");
    check(rc == NP_HUB_ERR_SAFETY_FAULT && np_runner_get_state() == NP_SESSION_FAULT,
          "refused: it ends as a FAULT, since a reason was recorded");
}


/* ── More scripted moments ────────────────────────────────────────────────────── */


static void post_bits(void)           { xEventGroupSetBits((EventGroupHandle_t)&g_events_token, g_post_bits); }
static void post_abort_user(void)     { np_runner_abort(NP_ABORT_USER); }
static void post_abort_seizure(void)  { np_runner_abort(NP_ABORT_EEG_SEIZURE); }
static void do_pause_now(void)        { g_pause_rc = np_runner_pause(); }
static void do_resume_now(void)       { g_resume_rc = np_runner_resume(); }
static void try_resume_now(void)      { g_resume_rc = np_runner_resume(); }

static void look_midrun(void)
{
    g_mid_lease = np_lease_held();
    g_mid_state = np_runner_get_state();
    if (g_mid_n < 4U) { g_mid_elapsed[g_mid_n++] = np_runner_elapsed_ms(); }
}

/* One long EEG command, for the tests that end a session early. */
static void long_session(blob_t *b, uint32_t duration_ms)
{
    blob_begin(b, duration_ms);
    blob_slot(b, NP_MOD_EEG, NP_HUB_SLOT_EEG, 0U, duration_ms, RUN_EEG_PARAMS, RUN_EEG_PARAMS_LEN);
    module_present(NP_HUB_SLOT_EEG);
}

typedef struct {
    uint32_t           bits;
    np_abort_reason_t  expect;
    const char        *name;
} abort_case_t;

/* @verifies REQ-FWHUB-12 */
static void test_each_stop_event_ends_the_session_with_its_own_reason_and_the_worst_wins(void)
{
    const uint32_t all = NP_EV_SESSION_ABORT | NP_EV_SAFETY_FAULT | NP_EV_THERMAL_FAULT | NP_EV_EEG_PPX | NP_EV_POWER_FAULT;
    const abort_case_t cases[] = {
        { NP_EV_SESSION_ABORT,                                   NP_ABORT_USER,       "user stop" },
        { NP_EV_POWER_FAULT,                                     NP_ABORT_POWER,      "power fault" },
        { NP_EV_EEG_PPX,                                         NP_ABORT_EEG_SEIZURE, "photoparoxysmal" },
        { NP_EV_THERMAL_FAULT,                                   NP_ABORT_THERMAL,    "thermal" },
        { NP_EV_SAFETY_FAULT,                                    NP_ABORT_SAFETY_MCU, "safety MCU" },
        { NP_EV_SESSION_ABORT | NP_EV_POWER_FAULT,               NP_ABORT_POWER,      "power outranks a user stop" },
        { NP_EV_POWER_FAULT | NP_EV_EEG_PPX | NP_EV_SESSION_ABORT, NP_ABORT_EEG_SEIZURE, "seizure outranks power" },
        { all & ~NP_EV_SAFETY_FAULT,                             NP_ABORT_THERMAL,    "thermal outranks seizure" },
        { all,                                                   NP_ABORT_SAFETY_MCU, "safety MCU outranks all" },
    };
    for (size_t i = 0U; i < sizeof cases / sizeof cases[0]; i++) {
        fresh();
        blob_t b;
        long_session(&b, 60000U);
        g_post_bits = cases[i].bits;
        script_at(300U, post_bits);
        const np_hub_status_t rc = load_and_run(&b);
        char name[128];
        snprintf(name, sizeof name, "reasons: %s: recorded in both records", cases[i].name);
        check(g_end_uhdr.abort_reason == cases[i].expect && g_end_shdr.abort_reason == cases[i].expect, name);
        snprintf(name, sizeof name, "reasons: %s: the run reports a fault and ends FAULT", cases[i].name);
        check(rc == NP_HUB_ERR_SAFETY_FAULT && np_runner_get_state() == NP_SESSION_FAULT, name);
        snprintf(name, sizeof name, "reasons: %s: it ended early and shut down once", cases[i].name);
        check(count_of(RUN_EV_DISABLE_ALL) == 1U && !g_runaway && g_log[first_of(RUN_EV_DISABLE_ALL)].at_ms < 60000U, name);
    }
}

/* @verifies REQ-FWHUB-12 */
static void test_an_abort_keeps_the_reason_it_was_given(void)
{
    /* np_mod_visual_ppx_halt() calls np_runner_abort(NP_ABORT_EEG_SEIZURE): the halt that stops the goggles on a
     * photoparoxysmal pattern.  The record must say so, not that the person stopped it. */
    fresh();
    blob_t b;
    long_session(&b, 60000U);
    script_at(300U, post_abort_seizure);
    (void)load_and_run(&b);
    check(g_end_uhdr.abort_reason == NP_ABORT_EEG_SEIZURE && g_end_shdr.abort_reason == NP_ABORT_EEG_SEIZURE,
          "abort: np_runner_abort(EEG_SEIZURE) is recorded as a seizure halt, not as a user stop");

    fresh();
    long_session(&b, 60000U);
    script_at(300U, post_abort_user);
    (void)load_and_run(&b);
    check(g_end_uhdr.abort_reason == NP_ABORT_USER && g_end_shdr.abort_reason == NP_ABORT_USER,
          "abort: np_runner_abort(USER) is recorded as a user stop");
}

/* @verifies SWR-SS-06 */
static void test_a_stop_is_acted_on_at_the_loops_next_wake_and_never_later_than_one_telemetry_interval(void)
{
    /* No requirement states a latency.  This pins what the loop does today so that a change which lengthens it is seen:
     * the loop sleeps in vTaskDelay (it does not wait on the event group), so a stop is noticed when the sleep ends, and
     * a sleep is never longer than NP_RUNNER_TELEM_INTERVAL_MS. */
    fresh();
    blob_t b;
    long_session(&b, 60000U);
    script_at(300U, post_abort_user);
    (void)load_and_run(&b);
    const uint32_t acted = g_log[first_of(RUN_EV_DISABLE_ALL)].at_ms;
    check(acted >= 300U, "stop: nothing happens before the stop is posted");
    check(acted <= 300U + NP_RUNNER_TELEM_INTERVAL_MS, "stop: it is acted on within one telemetry interval of being posted");
    check(np_runner_get_state() == NP_SESSION_FAULT, "stop: the session has ended");
}

/* @verifies REQ-FWHUB-63 */
static void test_shutdown_drops_the_enables_first_then_stops_the_modules_then_waits_then_records(void)
{
    fresh();
    blob_t b;
    two_command_session(&b);
    module_present(NP_HUB_SLOT_VNS_HRV);     /* present but never commanded: it is stopped too */
    script_at(500U, post_abort_user);
    (void)load_and_run(&b);

    const int dis = first_of(RUN_EV_DISABLE_ALL);
    const int sock = first_of(RUN_EV_SOCK_STOP_ALL);
    const int end = first_of(RUN_EV_LOG_END);
    check(dis >= 0 && sock > dis && end > sock, "shutdown: enables dropped, then the sockets stopped, then the record closed");
    int stops_between = 0;
    bool all_stopped_after_cut = true;
    for (int i = 0; i < (int)g_log_n; i++) {
        if (g_log[i].kind == RUN_EV_CONTROL && g_log[i].b == 0U && i > dis) { stops_between++; }
        if (g_log[i].kind == RUN_EV_CONTROL && g_log[i].b == 0U && i < dis) { all_stopped_after_cut = false; }
    }
    check(all_stopped_after_cut, "shutdown: no module is stopped before the enables are dropped (decision D-13)");
    check(stops_between >= 3, "shutdown: every present module is told to stop, commanded or not");
    check(find_a(RUN_EV_CONTROL, NP_HUB_SLOT_VNS_HRV, 0) > dis, "shutdown: including one that was never commanded");
    check(g_log[end].at_ms >= g_log[sock].at_ms + NP_RUNNER_SHUTDOWN_MS,
          "shutdown: the modules get NP_RUNNER_SHUTDOWN_MS to ramp down before the record is closed");
    const int flush = first_of(RUN_EV_FLUSH);
    const int cons = first_of(RUN_EV_CONS_END);
    check(end < flush && flush < cons, "shutdown: the record is closed, flushed, then the consumables are counted");
    check(last_of(RUN_EV_PROGRESS) > cons, "shutdown: the app is told the end state last");
}

/* @verifies REQ-FWHUB-03 */
static void test_the_lease_is_held_for_the_whole_run_and_released_after_the_final_state(void)
{
    fresh();
    blob_t b;
    two_command_session(&b);
    script_at(2500U, look_midrun);
    (void)load_and_run(&b);

    check(g_mid_lease && g_mid_state == NP_SESSION_RUNNING, "lease: held while the session runs");
    check(g_lease_held_at_end && g_lease_held_at_flush && g_lease_held_at_cons,
          "lease: still held while the record is closed, flushed and the consumables counted");
    check(!np_lease_held() && np_runner_get_state() == NP_SESSION_COMPLETE, "lease: released once the final state is set");
    blob_t again;
    two_command_session(&again);
    check(np_runner_load(again.buf, blob_finish(&again)) == NP_HUB_OK, "lease: the next protocol loads");
}

/* @verifies SWR-SS-07 */
static void test_the_length_a_session_ran_is_the_running_time_in_the_records_and_to_the_app(void)
{
    fresh();
    blob_t b;
    two_command_session(&b);
    script_at(2500U, look_midrun);
    (void)load_and_run(&b);
    check(g_mid_n == 1U && g_mid_elapsed[0] == 2500U, "length: elapsed is the session's own time (the wall clock starts far from zero)");

    uint8_t frame[NP_PROGRESS_LEN];
    np_runner_progress(frame);
    check(g_progress.state == NP_SESSION_COMPLETE && g_progress.total_ms == 6000U, "length: the app is shown a finished 6 s session");
    check(g_progress.elapsed_ms == 6000U,
          "length: and the length it ran is 6 s, not the 6 s plus the shutdown wait");
    check(g_end_uhdr.duration_s == 6U && g_end_shdr.duration_s == 6U, "length: the records state the same 6 s");
}

/* @verifies SWR-SS-05 */
static void test_pause_withdraws_the_enables_stops_the_clock_and_resume_redispatches(void)
{
    fresh();
    blob_t b;
    blob_begin(&b, 10000U);
    blob_slot(&b, NP_MOD_EEG, NP_HUB_SLOT_EEG, 0U, 10000U, RUN_EEG_PARAMS, RUN_EEG_PARAMS_LEN);
    blob_slot(&b, NP_MOD_VISUAL, NP_HUB_SLOT_VISUAL, 0U, 8000U, RUN_GATED_PARAMS, RUN_GATED_PARAMS_LEN);
    module_present(NP_HUB_SLOT_EEG);
    module_present(NP_HUB_SLOT_VISUAL);
    script_at(2000U, do_pause_now);
    script_at(3500U, look_midrun);
    script_at(4500U, look_midrun);
    script_at(6000U, do_resume_now);
    (void)load_and_run(&b);

    check(g_pause_rc == NP_HUB_OK && g_resume_rc == NP_HUB_OK, "pause: both requests are accepted");
    const int pause_at = find_a(RUN_EV_LOG_PAUSE, 1U, 0);
    const int resume_at = find_a(RUN_EV_LOG_PAUSE, 0U, 0);
    check(pause_at >= 0 && resume_at > pause_at, "pause: the pause and the resume are both logged, in order");
    check(g_mid_n == 2U && g_mid_state == NP_SESSION_PAUSED, "pause: the state is PAUSED in between");
    check(g_mid_elapsed[0] == g_mid_elapsed[1], "pause: the session clock stands still while paused");

    const uint32_t p = g_log[pause_at].at_ms;
    const uint32_t r = g_log[resume_at].at_ms;
    check(within(when_a(RUN_EV_CONTROL, NP_HUB_SLOT_VISUAL, 1), p, 0U) || when_a(RUN_EV_CONTROL, NP_HUB_SLOT_VISUAL, 1) <= p,
          "pause: the goggles are stopped when the pause begins");
    check(when_a(RUN_EV_DISABLE, NP_SAFETY_EN_VISUAL, 0) <= p, "pause: and their enable is withdrawn");
    check(find_a(RUN_EV_SOCK_PAUSE, 0, 0) < 0 ? count_of(RUN_EV_SOCK_PAUSE) == 1U : true, "pause: the lattice is paused");
    check(count_of(RUN_EV_SOCK_PAUSE) == 1U && count_of(RUN_EV_SOCK_RESUME) == 1U, "pause: the lattice is paused and resumed once each");
    check(when_a(RUN_EV_CONTROL, NP_HUB_SLOT_VISUAL, 2) >= r - RUN_SLACK_MS * 2U
          && find_a(RUN_EV_CONTROL, NP_HUB_SLOT_VISUAL, 2) >= 0,
          "pause: the resume dispatches the goggles' command again");
    check(find_a(RUN_EV_ENABLE, NP_SAFETY_EN_VISUAL, 1) >= 0, "pause: and asks for the enable again");
    /* The goggles' command ends 8 s into the PROTOCOL: paused time does not count. */
    const uint32_t paused_wall = r - p;
    const uint32_t stop2 = when_a(RUN_EV_CONTROL, NP_HUB_SLOT_VISUAL, 3);
    check(stop2 != UINT32_MAX && stop2 >= 8000U + paused_wall - RUN_SLACK_MS * 2U && stop2 <= 8000U + paused_wall + RUN_SLACK_MS * 2U,
          "pause: its stop still comes 8 s into the protocol, the paused time not counted");
    check(g_end_uhdr.mods_active_mask == ((1U << NP_MOD_EEG) | (1U << NP_MOD_VISUAL)), "pause: both modalities are in the delivered mask once");
}

/* @verifies SWR-SS-05 */
static void test_a_pause_that_outlasts_the_owners_limit_ends_the_session(void)
{
    fresh();
    blob_t b;
    long_session(&b, 60000U);
    g_pause_limit_s = 2U;
    script_at(1000U, do_pause_now);
    const np_hub_status_t rc = load_and_run(&b);
    const int pause_at = find_a(RUN_EV_LOG_PAUSE, 1U, 0);
    check(pause_at >= 0, "limit: the session paused");
    check(g_end_uhdr.abort_reason == NP_ABORT_PAUSE_TIMEOUT, "limit: it ended because the pause outlasted the limit");
    check(rc == NP_HUB_ERR_SAFETY_FAULT && np_runner_get_state() == NP_SESSION_FAULT, "limit: and ends FAULT");
    const uint32_t ended = g_log[first_of(RUN_EV_DISABLE_ALL)].at_ms;
    const uint32_t p = g_log[pause_at].at_ms;
    check(ended + RUN_SLACK_MS >= p + 2000U && ended <= p + 2000U + RUN_SLACK_MS * 2U, "limit: after the 2 s, measured from the start of the pause");

    fresh();
    long_session(&b, 20000U);
    g_pause_limit_s = 0U;
    script_at(1000U, do_pause_now);
    script_at(61000U, do_resume_now);
    const np_hub_status_t rc2 = load_and_run(&b);
    check(rc2 == NP_HUB_OK && g_end_uhdr.abort_reason == NP_ABORT_NONE, "limit: 0 is no limit: a minute paused, then resumed, completes");
}

/* @verifies SWR-SS-05 */
static void test_pause_and_resume_are_refused_in_the_wrong_state(void)
{
    fresh();
    check(np_runner_pause() == NP_HUB_ERR_NO_SESSION, "refuse: no pause with no session");
    check(np_runner_resume() == NP_HUB_ERR_NO_SESSION, "refuse: no resume with no session");
    blob_t b;
    long_session(&b, 5000U);
    script_at(500U, try_resume_now);        /* running, not paused */
    (void)load_and_run(&b);
    check(g_resume_rc == NP_HUB_ERR_NO_SESSION, "refuse: no resume while running");
    check(np_runner_pause() == NP_HUB_ERR_NO_SESSION, "refuse: no pause once the session has ended");
}

/* @verifies SWR-SS-05 */
static void test_a_session_stopped_while_paused_reports_the_time_it_was_running(void)
{
    fresh();
    blob_t b;
    long_session(&b, 60000U);
    script_at(2000U, do_pause_now);
    script_at(9000U, post_abort_user);
    (void)load_and_run(&b);
    const int pause_at = find_a(RUN_EV_LOG_PAUSE, 1U, 0);
    check(pause_at >= 0, "stopped paused: the session paused");
    const uint32_t ran_s = g_log[pause_at].at_ms / 1000U;
    check(g_end_uhdr.duration_s == ran_s && g_end_shdr.duration_s == ran_s,
          "stopped paused: the records report the time it was running, not the seconds it sat paused or shut down");
    check(g_end_uhdr.abort_reason == NP_ABORT_USER, "stopped paused: and that the person stopped it");
}

/* @verifies REQ-FWHUB-41 */
static void test_a_delivery_divergence_is_flagged_once_to_shdr_with_its_own_code(void)
{
    fresh();
    blob_t b;
    two_command_session(&b);
    g_diverge_on_call = 3U;        /* the third reading: the first module's, at the second telemetry pass */
    (void)load_and_run(&b);
    unsigned flags = 0U;
    uint32_t at = 0U;
    for (unsigned i = 0U; i < g_log_n; i++) {
        if (g_log[i].kind == RUN_EV_SHDR_FAULT && g_log[i].b == NP_STIM_SHDR_EV_DELIVERY_DIVERGENCE) { flags++; at = g_log[i].at_ms; }
    }
    check(flags == 1U, "divergence: exactly one SHDR flag, with the divergence code");
    check(at == 2000U, "divergence: raised at the telemetry pass that saw it");
    check(count_of(RUN_EV_TELEM) >= 2U * 5U, "divergence: telemetry is taken from every present module once a second");
    check(g_observe_calls >= 10U, "divergence: and every reading is checked");
}

/* @verifies REQ-FWHUB-36 REQ-FWHUB-37 */
static void test_electrode_geometry_reaches_the_safety_mcu_before_any_enable(void)
{
    fresh();
    blob_t b;
    blob_begin(&b, 5000U);
    const uint8_t bes[7] = { 0, 0x40, 0x9C, 0xE8, 0x03, 0, 0 };     /* 40 Hz, 1 mA, sinusoidal */
    blob_slot(&b, NP_MOD_BES_TACS, NP_HUB_SLOT_BES_TACS, 0U, 3000U, bes, sizeof bes);
    module_present(NP_HUB_SLOT_BES_TACS);
    (void)load_and_run(&b);

    const int geom = first_of(RUN_EV_GEOM_BES);
    const int wave = first_of(RUN_EV_SEND_WAVE);
    const int ctl = first_of(RUN_EV_CONTROL);
    const int en = first_of(RUN_EV_ENABLE);
    check(geom >= 0, "geometry: a BES/tACS command arms the BES geometry gate");
    check(wave >= 0, "geometry: and declares its waveform class");
    check(geom < ctl && wave < ctl && geom < en && wave < en,
          "geometry: both are sent before the command is dispatched or any enable is requested");
    check(find_a(RUN_EV_ENABLE, NP_SAFETY_EN_BES_TACS, 0) >= 0, "geometry: and the enable is then requested");
    check(first_of(RUN_EV_GEOM_TDCS) < 0 && first_of(RUN_EV_GEOM_CLIN) < 0, "geometry: no other gate is armed");

    fresh();
    blob_begin(&b, 5000U);
    blob_slot(&b, NP_MOD_EEG, NP_HUB_SLOT_EEG, 0U, 3000U, RUN_EEG_PARAMS, RUN_EEG_PARAMS_LEN);
    module_present(NP_HUB_SLOT_EEG);
    (void)load_and_run(&b);
    check(first_of(RUN_EV_GEOM_BES) < 0 && first_of(RUN_EV_SEND_LIMITS) < 0 && first_of(RUN_EV_SEND_WAVE) < 0,
          "geometry: a session with no electrical channel sends the safety MCU no geometry");
}

/* @verifies REQ-FWHUB-31 */
static void test_a_socket_command_never_reaches_a_slot_module_and_a_slot_command_never_reaches_the_sockets(void)
{
    fresh();
    blob_t b;
    blob_begin(&b, 4000U);
    uint8_t mask[NP_HUB_SOCKET_MASK_BYTES];
    memset(mask, 0, sizeof mask);
    mask[0] = 0x01U;
    const uint8_t pbm[4] = { 1, 2, 3, 4 };
    blob_cmd(&b, NP_MOD_PBM_BASE, NP_HUB_SLOT_NONE, 0U, 2000U, (uint8_t)NP_PROTO_TARGET_SOCKET_MASK,
             mask, sizeof mask, pbm, sizeof pbm);
    blob_slot(&b, NP_MOD_EEG, NP_HUB_SLOT_EEG, 0U, 2000U, RUN_EEG_PARAMS, RUN_EEG_PARAMS_LEN);
    module_present(NP_HUB_SLOT_EEG);
    (void)load_and_run(&b);
    check(count_of(RUN_EV_SOCK_CMD) >= 1U, "routing: the socket command goes to the socket dispatch");
    unsigned slot_controls = 0U;
    for (unsigned i = 0U; i < g_log_n; i++) {
        if (g_log[i].kind == RUN_EV_CONTROL && g_log[i].a != NP_HUB_SLOT_EEG) { slot_controls++; }
    }
    check(slot_controls == 0U, "routing: and to no slot module");
    check(find_a(RUN_EV_CONTROL, NP_HUB_SLOT_EEG, 0) >= 0, "routing: the slot command goes to its slot module");
    check(count_of(RUN_EV_SOCK_CMD) == 1U, "routing: the slot command never reached the socket dispatch (only the socket command did)");
}

/* @verifies REQ-FWHUB-31 */
static void test_a_refused_socket_command_is_logged_against_no_slot_and_is_not_delivered(void)
{
    fresh();
    blob_t b;
    blob_begin(&b, 4000U);
    uint8_t mask[NP_HUB_SOCKET_MASK_BYTES];
    memset(mask, 0, sizeof mask);
    mask[0] = 0x01U;
    const uint8_t pbm[4] = { 1, 2, 3, 4 };
    blob_cmd(&b, NP_MOD_PBM_BASE, NP_HUB_SLOT_NONE, 0U, 2000U, (uint8_t)NP_PROTO_TARGET_SOCKET_MASK,
             mask, sizeof mask, pbm, sizeof pbm);
    g_sock_rc = NP_HUB_ERR_MOD_FAULT;
    (void)load_and_run(&b);
    const int fault = first_of(RUN_EV_SHDR_FAULT);
    check(fault >= 0 && g_log[fault].a == NP_HUB_SLOT_NONE && g_log[fault].b == (uint32_t)(-NP_HUB_ERR_MOD_FAULT),
          "refused socket: an SHDR fault against NP_HUB_SLOT_NONE (not slot 0), carrying the gate's status");
    check((g_end_uhdr.mods_active_mask & (1U << NP_MOD_PBM_BASE)) == 0U, "refused socket: PBM is not in the delivered mask");
    check(g_end_uhdr.abort_reason == NP_ABORT_MOD_FAULT, "refused socket: the session records the module fault");
    const int cmd = find_a(RUN_EV_LOG_CMD, NP_HUB_SLOT_NONE, 0);
    check(cmd >= 0 && g_log[cmd].b == 0U, "refused socket: and the command is logged as refused");
}

/* ── main ─────────────────────────────────────────────────────────────────────── */

int main(void)
{
    test_a_session_runs_to_its_duration_and_completes();
    test_the_session_start_record_and_the_fitting_come_before_any_command();
    test_a_refused_command_is_logged_refused_and_not_counted_delivered();
    test_each_stop_event_ends_the_session_with_its_own_reason_and_the_worst_wins();
    test_an_abort_keeps_the_reason_it_was_given();
    test_a_stop_is_acted_on_at_the_loops_next_wake_and_never_later_than_one_telemetry_interval();
    test_shutdown_drops_the_enables_first_then_stops_the_modules_then_waits_then_records();
    test_the_lease_is_held_for_the_whole_run_and_released_after_the_final_state();
    test_the_length_a_session_ran_is_the_running_time_in_the_records_and_to_the_app();
    test_pause_withdraws_the_enables_stops_the_clock_and_resume_redispatches();
    test_a_pause_that_outlasts_the_owners_limit_ends_the_session();
    test_pause_and_resume_are_refused_in_the_wrong_state();
    test_a_session_stopped_while_paused_reports_the_time_it_was_running();
    test_a_delivery_divergence_is_flagged_once_to_shdr_with_its_own_code();
    test_electrode_geometry_reaches_the_safety_mcu_before_any_enable();
    test_a_socket_command_never_reaches_a_slot_module_and_a_slot_command_never_reaches_the_sockets();
    test_a_refused_socket_command_is_logged_against_no_slot_and_is_not_delivered();

    printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
