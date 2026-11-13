/*
 * The hub's own boot path and task set, in the simulator (NP-SIM-FW-001 §31, OI-SIMFW-45, OI-SIMFW-50).
 *
 * np_hub_control_app_main() is the device's own entry: it brings the subsystems up in the order its correctness depends on (the factory-reset
 * boot check, the log backend, the session log and count, the registry scan, the logger lock, the event group, the runner), creates the five
 * tasks and starts the scheduler.  It runs here unchanged, on a boot thread, over the task layer of np_sim_kernel.c.  What this file adds is
 * the platform's part:
 *
 *   - the storage the boot path binds first (np_sim_storage_up, np_sim_gatt.c): the device's Config-instance geometry over a RAM medium,
 *     formatted as a new part would be;
 *   - np_hal_status_led_set, the left-temple LED seam: reported as a hub_led event;
 *   - three commands.
 *
 * Commands (stdin, one line each; the bridge validates the arguments, REQ-SIMFW-12):
 *   hub_boot [admit|refuse]   run np_hub_control_app_main.  `refuse` makes the factory-reset marker read UNKNOWN first (the device's own boot
 *                             check then refuses data, np_log_backend_refuse runs, and np_runner_load refuses every protocol as
 *                             NP_HUB_ERR_BOOT_REFUSED).  Once only.  Add the modules (stim_add, ins_add, ...) and attach the safety MCU
 *                             before or after: the registry scans at boot and the detect task rescans the hot-plug slots.
 *   hub_run <ms>              move virtual time forward, running the hub's tasks as they fall due.  The hub's own heartbeat task beats the
 *                             safety MCU; safety_run and safety_beat are refused while it runs, because two things would beat.
 *   hub_status                the tasks, the runner, the lease, the boot decision, the LED
 *
 * What the twin does not model here: a task is never preempted between kernel calls (np_sim_kernel.h), the FreeRTOS kernel itself is not the
 * device's, and the hub's timing is the firmware's logic against a virtual clock (UC-124).  Nothing is verification evidence (REQ-SIMFW-06).
 */
#include "np_sim_bus.h"
#include "np_sim_kernel.h"
#include "np_sim_pbm_harness.h"
#include "np_sim_safety.h"

#include "np_factory_reset_test_hal.h"
#include "np_log_backend.h"
#include "np_session_lease.h"
#include "np_session_progress.h"
#include "np_session_runner.h"
#include "np_sw02_platform_hal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void np_hub_control_app_main(void);

/* The host model of the SNVS_LPGPR1 register the factory-reset module keeps its "reset in progress" flag in: the platform owns the register, and
 * np_factory_reset.c reads and writes it (np_factory_reset_tests.c defines it the same way for its own host build). */
volatile uint32_t np_fr_host_snvs_lpgpr1 = 0U;

static np_led_state_t g_led = NP_LED_IDLE;
static bool           g_booted;

static const char *led_name(np_led_state_t s) { return s == NP_LED_SESSION ? "session" : s == NP_LED_FAULT ? "fault" : "idle"; }

void np_hal_status_led_set(np_led_state_t state)
{
    g_led = state;
    np_sim_emit("{\"ev\":\"hub_led\",\"t\":%u,\"led\":\"%s\"}", (unsigned)np_sim_now_ms(), led_name(state));
}

bool np_sim_hub_booted(void) { return g_booted; }

static const char *state_name(np_session_state_t s)
{
    switch (s) {
    case NP_SESSION_IDLE:      return "idle";
    case NP_SESSION_LOADING:   return "loading";
    case NP_SESSION_VERIFYING: return "verifying";
    case NP_SESSION_RUNNING:   return "running";
    case NP_SESSION_PAUSED:    return "paused";
    case NP_SESSION_STOPPING:  return "stopping";
    case NP_SESSION_COMPLETE:  return "complete";
    case NP_SESSION_FAULT:     return "fault";
    default:                   return "?";
    }
}

static unsigned le32(const uint8_t *p) { return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24); }

/* The runner's own progress frame (SWR-SS-07), decoded: the figures the helmet holds, not the twin's. */
static void emit_state(const char *why)
{
    uint8_t f[NP_PROGRESS_LEN];
    np_runner_progress(f);
    np_sim_emit("{\"ev\":\"hub_state\",\"t\":%u,\"why\":\"%s\",\"runner\":\"%s\",\"part\":%u,\"active_mask\":%u,\"elapsed_ms\":%u,\"remaining_ms\":%u,"
                "\"total_ms\":%u,\"ramp_phase\":%u,\"ramp_pct\":%u,\"hold\":%u,\"led\":\"%s\",\"safety_mcu\":\"%s\"}",
                (unsigned)np_sim_now_ms(), why, state_name((np_session_state_t)f[0]), (unsigned)f[1], (unsigned)(f[2] | (f[3] << 8)),
                le32(&f[4]), le32(&f[8]), le32(&f[12]), (unsigned)f[16], (unsigned)f[17], (unsigned)np_runner_hold_cause(), led_name(g_led),
                np_sim_safety_label());
}

static void emit_status(void)
{
    np_sim_emit("{\"ev\":\"hub_status\",\"booted\":%s,\"admits\":%s,\"switches\":%u,\"tasks\":%u}", g_booted ? "true" : "false",
                np_log_backend_admits_sessions() ? "true" : "false", (unsigned)np_sim_kernel_context_switches(), np_sim_kernel_task_count());
    for (unsigned i = 0U; i < np_sim_kernel_task_count(); i++) {
        np_sim_emit("{\"ev\":\"hub_task\",\"name\":\"%s\",\"prio\":%u,\"state\":\"%s\"}", np_sim_kernel_task_name(i), np_sim_kernel_task_prio(i),
                    np_sim_kernel_task_state(i));
    }
    if (g_booted) { emit_state("status"); }
}

static void result(const char *cmd, int code)
{
    np_sim_emit("{\"ev\":\"result\",\"cmd\":\"%s\",\"code\":%d}", cmd, code);
}

static void bad(const char *cmd, const char *why)
{
    np_sim_emit("{\"ev\":\"error\",\"what\":\"%s: %s\"}", cmd, why);
}

/* The longest run one command asks for, in virtual ms: the cap safety_run has (UC-127). */
#define HUB_RUN_MAX_MS  60000U
/* The state is reported this often while time moves, so the page sees a session advance and not only its end. */
#define HUB_REPORT_MS   250U

bool np_simh_hub_line(const char *cmd, char *a)
{
    if (strcmp(cmd, "hub_boot") == 0) {
        if (g_booted) { bad(cmd, "the hub is already running"); return true; }
        const bool refuse = (a != NULL && strcmp(a, "refuse") == 0);
        if (a != NULL && !refuse && strcmp(a, "admit") != 0) { bad(cmd, "needs admit or refuse"); return true; }
        if (np_sim_safety_attached() == false) {
            np_sim_emit("{\"ev\":\"sim_note\",\"what\":\"hub_boot without a safety MCU attached: the heartbeat task will time out until one is\"}");
        }
        np_hub_status_t rc = np_sim_storage_up();
        if (rc != NP_HUB_OK) { result(cmd, (int)rc); return true; }
        np_fr_host_hal.marker_state = refuse ? NP_FR_MARKER_UNKNOWN : NP_FR_MARKER_ABSENT;   /* the host model of the Config marker the boot check reads */
        g_booted = np_sim_kernel_boot(np_hub_control_app_main);
        if (g_booted) { np_sim_gatt_mark_up(); }
        np_sim_emit("{\"ev\":\"result\",\"cmd\":\"hub_boot\",\"code\":%d,\"booted\":%s,\"admits\":%s,\"tasks\":%u}", g_booted ? 0 : -1,
                    g_booted ? "true" : "false", np_log_backend_admits_sessions() ? "true" : "false", np_sim_kernel_task_count());
        return true;
    }
    if (strcmp(cmd, "hub_status") == 0) { emit_status(); return true; }
    if (strcmp(cmd, "hub_run") == 0) {
        if (!g_booted) { bad(cmd, "the hub is not running (hub_boot first)"); return true; }
        const long ms = (a != NULL) ? atol(a) : -1L;
        if (ms < 0L || (unsigned long)ms > HUB_RUN_MAX_MS) { bad(cmd, "needs a number of virtual ms, 0 to 60000"); return true; }
        uint32_t left = (uint32_t)ms;
        while (left > 0U) {
            const uint32_t step = left < HUB_REPORT_MS ? left : HUB_REPORT_MS;
            (void)np_sim_kernel_run_ms(step);
            left -= step;
            emit_state("run");
        }
        np_sim_emit("{\"ev\":\"result\",\"cmd\":\"hub_run\",\"code\":0,\"ms\":%ld}", ms);
        return true;
    }
    return false;
}
