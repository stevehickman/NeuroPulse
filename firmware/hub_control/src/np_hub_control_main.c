/*
 * NeuroPulse Hub Control Program — FreeRTOS Task Definitions and Entry Point
 * Document: NP-FW-HUB-001 Rev A §2
 *
 * Four tasks:
 *
 *   task_safety_heartbeat  (prio 4 — REALTIME)
 *     Sends SPI heartbeat to safety MCU every NP_SAFETY_HEARTBEAT_MS.
 *     Never suspends for longer than that.  If the safety MCU returns a fault,
 *     posts NP_EV_SAFETY_FAULT to abort the running session.
 *
 *   task_hub_control  (prio 3 — HIGH)
 *     Waits for NP_EV_SESSION_START, then calls np_runner_run() which blocks
 *     until the session completes or is aborted.  Also handles incoming
 *     protocol blobs posted to g_proto_queue by the transport layer (BLE/USB-C).
 *
 *   task_telemetry  (prio 2 — NORMAL)
 *     Calls np_log_flush() on NP_LOG_UHDR_FLUSH_MS / NP_LOG_SHDR_FLUSH_MS
 *     intervals to ensure eMMC write-through without blocking the runner.
 *
 *   task_module_detect  (prio 1 — LOW)
 *     Polls for module insertion/removal during idle.  Suspended while a
 *     session is active (yields immediately on NP_SESSION_RUNNING state).
 *
 * Entry: np_hub_control_app_main() — called by the main processor's
 * post-bootloader application startup code after clocks, eMMC, and USB-C PD
 * negotiation are complete.
 *
 * HAL stubs:
 *   OI-HUB-MAIN-01: np_hal_proto_queue_receive(buf, buf_len, recv_len_out, timeout_ms)
 *     Blocks until a protocol blob arrives from BLE GATT or USB-C CDC transport.
 *   OI-HUB-MAIN-02: np_hal_status_led_set(state) — left-temple power LED.
 *   OI-HUB-MAIN-03: np_hal_get_device_session_count() → uint32_t from SHDR.
 */

#include "np_hub_types.h"
#include "np_module_registry.h"
#include "np_protocol.h"
#include "np_session_runner.h"
#include "np_session_log.h"
#include "np_safety_spi.h"
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include <string.h>

/* ── HAL stubs ────────────────────────────────────────────────────────────────── */

extern np_hub_status_t np_hal_proto_queue_receive(uint8_t  *buf,
                                                   size_t    buf_len,
                                                   size_t   *recv_len_out,
                                                   uint32_t  timeout_ms);
typedef enum { NP_LED_IDLE = 0, NP_LED_SESSION, NP_LED_FAULT } np_led_state_t;
extern void np_hal_status_led_set(np_led_state_t state);
extern uint32_t np_hal_get_device_session_count(void);

/* ── Globals ──────────────────────────────────────────────────────────────────── */

static EventGroupHandle_t g_hub_events;

/* Protocol receive buffer — in LPSDR4 RAM (32 MB). Sized for largest expected
 * protocol (NP_HUB_PROTO_CMD_MAX × max command size + header + signature). */
#define PROTO_BUF_LEN (sizeof(np_proto_header_t) + \
                       NP_HUB_PROTO_CMD_MAX * (sizeof(np_proto_cmd_hdr_t) + \
                                               NP_HUB_PROTO_PARAMS_MAX) + \
                       NP_HUB_PROTO_SIG_LEN)

static uint8_t g_proto_buf[PROTO_BUF_LEN];

/* ── SHDR log callback (wired from module registry to session logger) ─────────── */

static void shdr_zone_auth_cb(uint8_t slot, np_hub_mod_type_t type, bool pass)
{
    np_log_shdr_zone_auth(slot, type, pass);
}

/* ── task_safety_heartbeat ────────────────────────────────────────────────────── */

static void task_safety_heartbeat(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        np_session_state_t state = np_runner_get_state();
        uint16_t req_mask = np_safety_spi_get_granted_mask(); /* echo current grants */

        np_hub_status_t rc = np_safety_spi_heartbeat(state, req_mask);

        if (rc == NP_HUB_ERR_SAFETY_FAULT) {
            np_hal_status_led_set(NP_LED_FAULT);
            xEventGroupSetBits(g_hub_events, NP_EV_SAFETY_FAULT);
        } else if (rc == NP_HUB_ERR_TIMEOUT) {
            /* SPI timeout — safety MCU unresponsive. Safety MCU's own watchdog
             * will cut stimulation; post fault so session runner also stops. */
            xEventGroupSetBits(g_hub_events, NP_EV_SAFETY_FAULT);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(NP_SAFETY_HEARTBEAT_MS));
    }
}

/* ── task_hub_control ─────────────────────────────────────────────────────────── */

static void task_hub_control(void *arg)
{
    (void)arg;

    for (;;) {
        /* Wait for a session-start event (posted by np_runner_load on success). */
        xEventGroupWaitBits(g_hub_events, NP_EV_SESSION_START,
                            pdTRUE,  /* clear on exit */
                            pdTRUE,  /* all bits */
                            portMAX_DELAY);

        /* Clear any stale abort/fault bits from a previous session. */
        xEventGroupClearBits(g_hub_events,
                             NP_EV_SESSION_ABORT | NP_EV_SAFETY_FAULT |
                             NP_EV_THERMAL_FAULT | NP_EV_EEG_PPX |
                             NP_EV_POWER_FAULT);

        np_hal_status_led_set(NP_LED_SESSION);

        np_hub_status_t rc = np_runner_run();

        np_hal_status_led_set((rc == NP_HUB_OK) ? NP_LED_IDLE : NP_LED_FAULT);

        /* Check incoming protocol queue for next session while idle. */
        /* (Falls through to the xEventGroupWaitBits at the top of the loop.) */
    }
}

/* ── task_protocol_rx ─────────────────────────────────────────────────────────── */

/*
 * Separate task for blocking on the transport receive queue.  Calls np_runner_load
 * when a blob arrives.  If np_runner_load succeeds it posts NP_EV_SESSION_START
 * which wakes task_hub_control.
 */
static void task_protocol_rx(void *arg)
{
    (void)arg;

    for (;;) {
        size_t recv_len = 0U;
        np_hub_status_t rc = np_hal_proto_queue_receive(g_proto_buf,
                                                         sizeof(g_proto_buf),
                                                         &recv_len,
                                                         portMAX_DELAY);
        if (rc != NP_HUB_OK || recv_len == 0U) {
            continue;
        }

        /* np_runner_load verifies signature; returns error and does nothing on fail. */
        (void)np_runner_load(g_proto_buf, recv_len);

        /* Zero the receive buffer to avoid leaving a plaintext protocol in RAM. */
        memset(g_proto_buf, 0, recv_len);
    }
}

/* ── task_telemetry ───────────────────────────────────────────────────────────── */

static void task_telemetry(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        /* Flush UHDR on its own interval. */
        np_log_flush();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(NP_LOG_UHDR_FLUSH_MS));
    }
}

/* ── task_module_detect ───────────────────────────────────────────────────────── */

static void task_module_detect(void *arg)
{
    (void)arg;

    for (;;) {
        /* During an active session, yield frequently but do not rescan:
         * zone module insertion/removal is signalled via zone_announce callbacks. */
        if (np_runner_get_state() == NP_SESSION_RUNNING) {
            vTaskDelay(pdMS_TO_TICKS(NP_DETECT_ACCESSORY_POLL_MS));
            continue;
        }

        /* Idle: rescan non-zone accessory slots (visual, VNS, intranasal, CVNS).
         * Zone slots are handled by np_zone_announce; this covers the rest. */
        for (uint8_t slot = NP_HUB_SLOT_VISUAL; slot < NP_HUB_SLOT_MAX; slot++) {
            (void)np_mod_reg_rescan_zone(slot, shdr_zone_auth_cb);
        }

        vTaskDelay(pdMS_TO_TICKS(NP_DETECT_ACCESSORY_POLL_MS));
    }
}

/* ── Zone announce insert callback (called from np_zone_announce) ─────────────── */

/*
 * The np_zone_announce module (firmware/zone_announce/) detects zone module
 * insertion via ZONE_ID ADC, plays the bone-conduction announcement, and then
 * calls this callback.  We re-probe the slot to refresh the registry.
 */
void np_hub_zone_insert_cb(uint8_t zone_id, bool announcement_done)
{
    if (!announcement_done) {
        return; /* wait for the second (post-audio) callback */
    }
    /* zone_id is 1-based (np_zone_id_t); slot is 0-based */
    if (zone_id == 0U || zone_id > NP_HUB_ZONE_SLOT_COUNT) {
        return;
    }
    uint8_t slot = (uint8_t)(zone_id - 1U);
    (void)np_mod_reg_rescan_zone(slot, shdr_zone_auth_cb);
}

void np_hub_zone_remove_cb(uint8_t zone_id)
{
    if (zone_id == 0U || zone_id > NP_HUB_ZONE_SLOT_COUNT) {
        return;
    }
    uint8_t slot = (uint8_t)(zone_id - 1U);
    np_mod_entry_t *mod = np_mod_reg_get(slot);
    if (mod != NULL && mod->shutdown != NULL) {
        (void)mod->shutdown(slot);
    }
}

/* ── Entry point ──────────────────────────────────────────────────────────────── */

void np_hub_control_app_main(void)
{
    /* Initialise subsystems in dependency order. */
    np_safety_spi_init();
    np_mod_reg_init();
    np_mod_reg_scan(shdr_zone_auth_cb);

    uint32_t session_count = np_hal_get_device_session_count();
    np_log_init(session_count);

    g_hub_events = xEventGroupCreate();
    configASSERT(g_hub_events != NULL);

    np_runner_init(g_hub_events);

    np_hal_status_led_set(NP_LED_IDLE);

    /* Create FreeRTOS tasks. */
    BaseType_t ok;

    ok = xTaskCreate(task_safety_heartbeat, "SafetyBeat",
                     NP_HUB_TASK_STACK_HEARTBEAT, NULL,
                     NP_HUB_TASK_PRIO_HEARTBEAT, NULL);
    configASSERT(ok == pdPASS);

    ok = xTaskCreate(task_hub_control, "HubCtrl",
                     NP_HUB_TASK_STACK_CONTROL, NULL,
                     NP_HUB_TASK_PRIO_CONTROL, NULL);
    configASSERT(ok == pdPASS);

    ok = xTaskCreate(task_protocol_rx, "ProtoRx",
                     NP_HUB_TASK_STACK_CONTROL, NULL,
                     NP_HUB_TASK_PRIO_CONTROL - 1U, NULL);
    configASSERT(ok == pdPASS);

    ok = xTaskCreate(task_telemetry, "Telemetry",
                     NP_HUB_TASK_STACK_TELEMETRY, NULL,
                     NP_HUB_TASK_PRIO_TELEMETRY, NULL);
    configASSERT(ok == pdPASS);

    ok = xTaskCreate(task_module_detect, "ModDetect",
                     NP_HUB_TASK_STACK_DETECT, NULL,
                     NP_HUB_TASK_PRIO_DETECT, NULL);
    configASSERT(ok == pdPASS);

    vTaskStartScheduler();

    /* Should never reach here — vTaskStartScheduler() does not return. */
    for (;;) { }
}
