/*
 * The seams np_runner_load needs, and the ones np_runner_run would (NP-SIM-FW-001 §26, OI-SIMFW-43).
 *
 * np_session_runner.c is the device's own code and is linked unchanged.  What it calls that the host cannot provide is here, in the
 * simulator's layer only (REQ-SIMFW-01):
 *
 *   - the kernel calls the lease and the run loop use (event group, mutex, delay, tick, assert), as a one-thread model with a virtual
 *     clock: np_runner_load posts NP_EV_SESSION_START and the post is reported as an event, as the hub's runner task would receive it;
 *   - the boot admission, which is the real np_log_backend.c's flag: np_log_backend_init() admits sessions, np_log_backend_refuse() does
 *     not.  What the device does at boot depends on its config partition mounting (np_hub_control_main.c, OI-NVRAM-19); the simulator
 *     chooses and says so (proto_boot, UC-137);
 *   - the seams only np_runner_run reaches (the session log, the consumables count, the module registry, the pause limit).  Nothing in
 *     the simulator calls np_runner_run yet (OI-SIMFW-45), so these are traps: the first call says so as an event, and answers the way
 *     a part that is absent would.  A trap that answered plausibly would hide that the run is not modelled (the rule np_platform_stub.c
 *     states for the device build).
 *
 * Nothing here is verification evidence (REQ-SIMFW-06).
 */
#include "np_sim_bus.h"

#include "np_hub_types.h"
#include "np_module_registry.h"
#include "np_session_log.h"
#include "np_session_runner.h"
#include "np_stim_xcheck.h"

#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"
#include "task.h"

#include <stdbool.h>
#include <stdint.h>

/* ── the kernel, as one thread with a virtual clock ──────────────────────────────────────────────────────────────────────────────── */

static EventBits_t g_bits;
static int         g_mutex_token;

TickType_t xTaskGetTickCount(void) { return (TickType_t)(np_sim_now_ms() / portTICK_PERIOD_MS); }

void vTaskDelay(const TickType_t ticks) { np_sim_advance_ms((uint32_t)ticks * portTICK_PERIOD_MS); }

EventBits_t xEventGroupSetBits(EventGroupHandle_t g, const EventBits_t bits)
{
    (void)g;
    g_bits |= bits;
    np_sim_emit("{\"ev\":\"runner_event\",\"t\":%u,\"set\":%u,\"bits\":%u}", (unsigned)np_sim_now_ms(), (unsigned)bits, (unsigned)g_bits);
    return g_bits;
}

/* xEventGroupGetBits is a macro for xEventGroupClearBits(g, 0). */
EventBits_t xEventGroupClearBits(EventGroupHandle_t g, const EventBits_t bits)
{
    (void)g;
    g_bits &= ~bits;
    return g_bits;
}

EventBits_t xEventGroupWaitBits(EventGroupHandle_t g, const EventBits_t bits, const BaseType_t clear,
                                const BaseType_t all, const TickType_t ticks)
{
    (void)g; (void)clear; (void)all;
    if ((g_bits & bits) == 0U) { np_sim_advance_ms((uint32_t)ticks * portTICK_PERIOD_MS); }
    return g_bits;
}

QueueHandle_t xQueueCreateMutex(const uint8_t type)           { (void)type; return (QueueHandle_t)&g_mutex_token; }
BaseType_t xQueueSemaphoreTake(QueueHandle_t q, TickType_t t) { (void)q; (void)t; return pdTRUE; }
BaseType_t xQueueGenericSend(QueueHandle_t q, const void *item, TickType_t t, const BaseType_t pos)
{
    (void)q; (void)item; (void)t; (void)pos;
    return pdTRUE;
}

void np_freertos_assert_failed(const char *file, unsigned long line)
{
    np_sim_emit("{\"ev\":\"error\",\"what\":\"configASSERT at %s:%lu\"}", file, line);
}

/* ── the seams only np_runner_run reaches: traps (OI-SIMFW-45) ───────────────────────────────────────────────────────────────────── */

static void unmodelled(const char *what)
{
    static unsigned said;
    if (said < 8U) {
        said++;
        np_sim_emit("{\"ev\":\"sim_unmodelled\",\"what\":\"%s\",\"note\":\"np_runner_run is not modelled (OI-SIMFW-45)\"}", what);
    }
}

void np_log_session_start(const np_session_uhdr_record_t *rec)  { (void)rec; unmodelled("np_log_session_start"); }
void np_log_session_fitting(const uint8_t *frame, size_t len)   { (void)frame; (void)len; unmodelled("np_log_session_fitting"); }
void np_log_command(const np_session_cmd_t *cmd, uint32_t session_ms, bool accepted)
{ (void)cmd; (void)session_ms; (void)accepted; unmodelled("np_log_command"); }
void np_log_session_pause(uint32_t session_ms, bool paused, uint16_t unrestored)
{ (void)session_ms; (void)paused; (void)unrestored; unmodelled("np_log_session_pause"); }
void np_log_session_end(const np_session_uhdr_record_t *u, const np_session_shdr_record_t *s)
{ (void)u; (void)s; unmodelled("np_log_session_end"); }
void np_log_flush(void)                                         { unmodelled("np_log_flush"); }
void np_log_shdr_pbm_session_counts(const np_pst_counts_t *c)   { (void)c; unmodelled("np_log_shdr_pbm_session_counts"); }
void np_log_telemetry(const np_telem_record_t *rec)             { (void)rec; unmodelled("np_log_telemetry"); }
np_mod_entry_t *np_mod_reg_get(uint8_t slot)                    { (void)slot; unmodelled("np_mod_reg_get"); return NULL; }
