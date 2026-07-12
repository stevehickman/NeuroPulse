/*
 * NeurOne Hub Control Program — FreeRTOS Task Definitions and Entry Point
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
#include "np_log_backend.h"
#include "np_transport.h"
#include "np_safety_spi.h"
#include "np_cvns_reenable.h"
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include <string.h>

/* ── HAL stubs ────────────────────────────────────────────────────────────────── */

/* np_hal_proto_queue_receive (OI-HUB-MAIN-01) is implemented by np_transport.c
 * and declared in np_transport.h — the BLE GATT / USB-C CDC transport feeds it
 * via np_transport_feed(). */
typedef enum { NP_LED_IDLE = 0, NP_LED_SESSION, NP_LED_FAULT } np_led_state_t;
extern void np_hal_status_led_set(np_led_state_t state);
extern uint32_t np_hal_get_device_session_count(void);

/* Cervical VNS module bridge (OI-CVNS-HUB-08): the safety-heartbeat task hands
 * the fresh granted mask + MCU status to np_mod_cvns so its next tick advances
 * the library enable/impedance state machine.  Declared extern here (registry
 * idiom — module drivers expose no per-module header). */
extern void np_mod_cvns_set_heartbeat_status(uint16_t granted_mask,
                                             uint8_t  mcu_status);

/* Cervical VNS PPG-ISR seam (OI-CVNS-HUB-07): the platform PPG ADC ISR calls
 * np_hub_ppg_isr_sample() (below), which forwards to the module's Pan-Tompkins
 * feed.  Declared extern here (registry idiom — module drivers expose no header). */
extern void     np_mod_cvns_push_ppg(uint32_t sample, uint32_t timestamp_ms);
extern uint32_t np_mod_cvns_hal_now_ms(void);

/* ── Globals ──────────────────────────────────────────────────────────────────── */

static EventGroupHandle_t g_hub_events;

/* Protocol receive buffer — in LPSDR4 RAM (32 MB). Sized for the largest
 * expected protocol blob (NP_HUB_PROTO_BLOB_MAX, defined in np_hub_types.h). */
static uint8_t g_proto_buf[NP_HUB_PROTO_BLOB_MAX];

/* ── SHDR log callback (wired from module registry to session logger) ─────────── */

static void shdr_zone_auth_cb(uint8_t slot, np_hub_mod_type_t type, bool pass)
{
    np_log_shdr_zone_auth(slot, type, pass);
}

/* ── task_safety_heartbeat ────────────────────────────────────────────────────── */

/*
 * log_cvns_event — route a CVNS re-enable lifecycle event to SHDR.
 *
 * Privacy (NP-FW-CVNS-001: "SHDR: cutoff flag only, no HR values"): every
 * event is logged with a SUPPRESSED (0) timestamp so no session-relative time
 * can co-locate a device log entry with a UHDR-class cardiac event (see the
 * CLAUDE.md fault-latch privacy gate).  Event codes are flags only.
 */
static void log_cvns_event(np_cvns_reenable_event_t ev)
{
    uint8_t code;
    switch (ev) {
    case NP_CVNS_RN_EV_CUTOFF_DETECTED:   code = NP_CVNS_SHDR_EV_CUTOFF;         break;
    case NP_CVNS_RN_EV_CONFIRM_OPEN:      code = NP_CVNS_SHDR_EV_CONFIRM_OPEN;   break;
    case NP_CVNS_RN_EV_IMPEDANCE_FAILED:  code = NP_CVNS_SHDR_EV_IMP_FAILED;     break;
    case NP_CVNS_RN_EV_IMPEDANCE_TIMEOUT: code = NP_CVNS_SHDR_EV_IMP_TIMEOUT;    break;
    case NP_CVNS_RN_EV_REENABLE_COMPLETE: code = NP_CVNS_SHDR_EV_REENABLED;      break;
    case NP_CVNS_RN_EV_ASSERT_TIMEOUT:    code = NP_CVNS_SHDR_EV_ASSERT_TIMEOUT; break;
    default:
        return;  /* NONE / IMPEDANCE_PASSED / CARDIAC_CLEARED: not logged */
    }
    np_log_shdr_fault(NP_HUB_SLOT_CVNS, NP_MOD_CVNS, code, 0U /* ts suppressed */);
}

static void task_safety_heartbeat(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        np_session_state_t state = np_runner_get_state();

        /* ── OI-CVNS-HUB-01: cervical VNS re-enable after cardiac cutoff ──
         * Drive the three-gate state machine and set the CVNS_REENABLE flag
         * BEFORE building this beat's TX frame, using the MCU status from the
         * most recent reply.  Setting the bit first (rather than mirroring it
         * one beat late) guarantees the CVNS_REENABLE and ACTIVE bits in the
         * SAME frame are mutually consistent: on any beat where the session is
         * not RUNNING the machine resets and the bit clears, so the frame can
         * never carry CVNS_REENABLE=1 with ACTIVE=0 during teardown.  The
         * machine is the ONLY source of the bit.                            */
        uint8_t  mcu_status = np_safety_spi_get_status();
        uint32_t now_ms     = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        np_cvns_reenable_event_t ev = np_cvns_reenable_on_heartbeat(
            mcu_status,
            (state == NP_SESSION_RUNNING),
            now_ms);
        np_safety_spi_set_cvns_reenable(np_cvns_reenable_bit_active());
        log_cvns_event(ev);

        /* Transmit the REQUESTED mask (accumulated by the session runner via
         * request_enable/request_disable) — echoing the granted mask back
         * would mean new enable requests never reach the safety MCU.        */
        uint16_t req_mask = np_safety_spi_get_requested_mask();

        /* current_ua = NULL/0: per-channel commanded-current wiring into the
         * heartbeat is a separate charge-monitor integration (OI-CHARGE-01
         * hub side); channel_count 0 keeps the MCU accumulate loop idle.    */
        np_hub_status_t rc = np_safety_spi_heartbeat(state, req_mask, NULL, 0U);

        /* OI-CVNS-HUB-08: publish THIS beat's fresh granted mask + status to the
         * cervical VNS module so its next tick folds them into the library's
         * enable/impedance SPI-response state machine. */
        np_mod_cvns_set_heartbeat_status(np_safety_spi_get_granted_mask(),
                                         np_safety_spi_get_status());

        if (rc == NP_HUB_ERR_SAFETY_FAULT) {
            /* Recoverable cardiac cutoff: CARDIAC set with no non-recoverable
             * fault bits (FAULT/WATCHDOG/THERMAL/CHARGE).  The safety MCU has
             * already cut stimulation and latched CARDIAC; the hub HOLDS the
             * session so the operator can run the re-enable flow.  Aborting
             * here would make in-session CVNS re-enable impossible.
             *
             * Use the FRESH status from THIS beat's reply — the `mcu_status`
             * read above (for the state machine) is the previous beat's value
             * and would not yet show CARDIAC on the very first cutoff beat,
             * which would spuriously abort exactly the session we must hold. */
            uint8_t fault_status = np_safety_spi_get_status();
            bool cardiac_recoverable =
                ((fault_status & NP_SAFETY_STATUS_CARDIAC) != 0U) &&
                ((fault_status & NP_CVNS_NONRECOVERABLE_FAULTS) == 0U);

            if (!cardiac_recoverable) {
                np_hal_status_led_set(NP_LED_FAULT);
                xEventGroupSetBits(g_hub_events, NP_EV_SAFETY_FAULT);
            }
        } else if (rc == NP_HUB_ERR_TIMEOUT) {
            /* SPI timeout — safety MCU unresponsive. Safety MCU's own watchdog
             * will cut stimulation; post fault so session runner also stops. */
            xEventGroupSetBits(g_hub_events, NP_EV_SAFETY_FAULT);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(NP_SAFETY_HEARTBEAT_MS));
    }
}

/*
 * np_hub_cvns_reenable_confirm — public entry point for the app-transport
 * layer (BLE GATT / USB-C CDC) to deliver the explicit operator/user
 * re-enable confirmation (OI-CVNS-HUB-01 gate 2; transport wiring is
 * OI-CVNS-HUB-03).  Race-free by construction: np_cvns_reenable_confirm()
 * only posts a one-shot intent flag; the heartbeat task (sole state mutator)
 * validates and acts on it.  See np_cvns_reenable.h single-writer design.
 */
np_hub_status_t np_hub_cvns_reenable_confirm(void)
{
    return np_cvns_reenable_confirm();
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

/* ── Cervical VNS PPG-ISR seam (OI-CVNS-HUB-07) ───────────────────────────────── */

/*
 * np_hub_ppg_isr_sample — hub entry point for the platform PPG ADC ISR.
 *
 * The cervical VNS cardiac interlock needs the raw auricular PPG sample stream at
 * NP_CVNS_PPG_SAMPLE_RATE_HZ (200 Hz) to run its Pan-Tompkins R-peak detector and
 * arm the <100 ms cardiac cutoff.  The platform PPG ISR calls this on every
 * sample; it forwards to the CVNS module, stamping with np_mod_cvns_hal_now_ms()
 * so the sample timestamp shares the exact free-running ms clock the session
 * runner passes to np_mod_cvns_tick() (the interlock's R-R timing assumes one
 * clock domain across both feeds).
 *
 * ISR-context safe: np_mod_cvns_push_ppg() self-guards (no-op unless a CVNS
 * session is active) and only appends to the interlock's R-R pipeline — no
 * blocking, no allocation.  When no CVNS session is present it is a cheap early
 * return, so the platform may route every PPG sample here unconditionally.
 *
 * (HRV biofeedback consumes the same PPG stream via its own HAL path; a unified
 * hub PPG fan-out is a future consolidation, not required by OI-CVNS-HUB-07.)
 */
void np_hub_ppg_isr_sample(uint32_t sample)
{
    np_mod_cvns_push_ppg(sample, np_mod_cvns_hal_now_ms());
}

/* ── Entry point ──────────────────────────────────────────────────────────────── */

void np_hub_control_app_main(void)
{
    /* Initialize subsystems in dependency order. */
    np_safety_spi_init();
    np_cvns_reenable_init();   /* OI-CVNS-HUB-01: re-enable manager starts IDLE */
    np_transport_init();       /* OI-HUB-MAIN-01: protocol mailbox + wait primitive */
    np_mod_reg_init();
    np_mod_reg_scan(shdr_zone_auth_cb);

    /* OI-LOG-01..04: open the UHDR/SHDR log files on the mounted partitions
     * before the session logger writes any record. */
    (void)np_log_backend_init();

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
