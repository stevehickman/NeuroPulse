/*
 * NeuroPulse Hub Control Program — Session Runner Implementation
 * Document: NP-FW-HUB-001 Rev A §5
 *
 * Execution model:
 *  - Commands in desc.cmds[] are pre-sorted by start_ms (np_protocol_sort_cmds).
 *  - np_runner_run() loops on a NP_RUNNER_TICK_MS schedule, dispatching commands
 *    whose start_ms ≤ elapsed_ms.
 *  - Each command registers a stop event at (start_ms + duration_ms) in
 *    s_ctx.stop_at_ms[slot].  If a new command arrives for the same slot before
 *    the stop fires, the stop time is overwritten.
 *  - Telemetry is requested every NP_RUNNER_TELEM_INTERVAL_MS via a callback
 *    posted to the telemetry task queue.
 *  - NP_EV_SESSION_ABORT or NP_EV_SAFETY_FAULT unblocks the loop immediately.
 */

#include "np_session_runner.h"
#include "np_protocol.h"
#include "np_module_registry.h"
#include "np_session_log.h"
#include "np_safety_spi.h"
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include <string.h>
#include <limits.h>

/* ── Internal state ───────────────────────────────────────────────────────────── */

static np_runner_ctx_t     s_ctx;
static EventGroupHandle_t  s_events;

/* ── Internal helpers ─────────────────────────────────────────────────────────── */

static uint32_t ticks_to_ms(TickType_t ticks)
{
    return (uint32_t)(ticks * portTICK_PERIOD_MS);
}

static uint32_t elapsed_ms_now(void)
{
    return ticks_to_ms(xTaskGetTickCount() - s_ctx.start_tick);
}

/*
 * slot_mask_to_safety_bit — returns the NP_SAFETY_EN_* bit for the given slot.
 * Returns 0 for audio (not safety-MCU gated).
 */
static uint16_t slot_to_safety_bit(uint8_t slot)
{
    static const uint16_t k_map[NP_HUB_SLOT_MAX] = {
        [NP_HUB_SLOT_ZONE_0]     = NP_SAFETY_EN_PBM_ZONE_0,
        [NP_HUB_SLOT_ZONE_1]     = NP_SAFETY_EN_PBM_ZONE_1,
        [NP_HUB_SLOT_ZONE_2]     = NP_SAFETY_EN_PBM_ZONE_2,
        [NP_HUB_SLOT_ZONE_3]     = NP_SAFETY_EN_PBM_ZONE_3,
        [NP_HUB_SLOT_ZONE_4]     = NP_SAFETY_EN_PBM_ZONE_4,
        [NP_HUB_SLOT_EEG]        = 0U,  /* EEG is passive — no safety MCU gate */
        [NP_HUB_SLOT_AUDIO]      = NP_SAFETY_EN_AUDIO,
        [NP_HUB_SLOT_VISUAL]     = NP_SAFETY_EN_VISUAL,
        [NP_HUB_SLOT_VNS_HRV]   = NP_SAFETY_EN_VNS_HRV,
        [NP_HUB_SLOT_INTRANASAL] = NP_SAFETY_EN_INTRANASAL,
        [NP_HUB_SLOT_CVNS]       = NP_SAFETY_EN_CVNS,
    };
    return (slot < NP_HUB_SLOT_MAX) ? k_map[slot] : 0U;
}

/*
 * dispatch_command — call the registered control function for each slot set
 * in cmd->slot_mask, request enable from safety MCU, and record stop time.
 */
static void dispatch_command(const np_session_cmd_t *cmd, uint32_t now_ms)
{
    for (uint8_t slot = 0U; slot < NP_HUB_SLOT_MAX; slot++) {
        if (!((cmd->slot_mask >> slot) & 1U)) {
            continue;
        }
        np_mod_entry_t *mod = np_mod_reg_get(slot);
        if (mod == NULL || mod->control == NULL) {
            continue;
        }

        const void *params     = (cmd->params_len > 0U) ? cmd->params : NULL;
        uint16_t    params_len = cmd->params_len;

        np_hub_status_t rc = mod->control(slot, params, params_len);
        if (rc != NP_HUB_OK) {
            /* Module fault — log to SHDR, record in session, but continue. */
            np_log_shdr_fault(slot, mod->type, (uint8_t)(-rc), now_ms);
            s_ctx.shdr.abort_reason = (uint8_t)NP_ABORT_MOD_FAULT;
            /* Non-fatal: session continues; safety MCU owns hard cutoff. */
        }

        /* Request enable from safety MCU for stimulation channels. */
        if (params != NULL) {
            uint16_t bit = slot_to_safety_bit(slot);
            if (bit != 0U) {
                np_safety_spi_request_enable(bit);
            }
        }

        /* Record when this slot should auto-stop. */
        if (cmd->duration_ms > 0U) {
            s_ctx.stop_at_ms[slot] = cmd->start_ms + cmd->duration_ms;
        }
    }
}

/*
 * process_stops — issue stop (control(slot, NULL, 0)) for any slot whose
 * stop_at_ms has been reached and is not overridden by a pending command.
 */
static void process_stops(uint32_t now_ms)
{
    for (uint8_t slot = 0U; slot < NP_HUB_SLOT_MAX; slot++) {
        if (s_ctx.stop_at_ms[slot] == 0U) {
            continue;
        }
        if (now_ms < s_ctx.stop_at_ms[slot]) {
            continue;
        }

        np_mod_entry_t *mod = np_mod_reg_get(slot);
        if (mod != NULL && mod->control != NULL) {
            (void)mod->control(slot, NULL, 0U);
        }
        /* De-request safety MCU enable for this slot. */
        uint16_t bit = slot_to_safety_bit(slot);
        if (bit != 0U) {
            np_safety_spi_request_disable(bit);
        }
        s_ctx.stop_at_ms[slot] = 0U;
    }
}

/*
 * ms_until_next_event — returns the number of ms until the next command dispatch
 * or stop event, capped at NP_RUNNER_TELEM_INTERVAL_MS.
 */
static uint32_t ms_until_next_event(uint32_t now_ms)
{
    uint32_t nearest = NP_RUNNER_TELEM_INTERVAL_MS;

    /* Next command */
    if (s_ctx.next_cmd_idx < s_ctx.desc.cmd_count) {
        uint32_t cmd_start = s_ctx.desc.cmds[s_ctx.next_cmd_idx].start_ms;
        if (cmd_start > now_ms) {
            uint32_t delta = cmd_start - now_ms;
            if (delta < nearest) {
                nearest = delta;
            }
        } else {
            return 0U;
        }
    }

    /* Next stop */
    for (uint8_t slot = 0U; slot < NP_HUB_SLOT_MAX; slot++) {
        if (s_ctx.stop_at_ms[slot] > now_ms) {
            uint32_t delta = s_ctx.stop_at_ms[slot] - now_ms;
            if (delta < nearest) {
                nearest = delta;
            }
        }
    }

    if (nearest < NP_RUNNER_TICK_MS) {
        nearest = NP_RUNNER_TICK_MS;
    }
    return nearest;
}

/* ── Public API ───────────────────────────────────────────────────────────────── */

void np_runner_init(EventGroupHandle_t hub_events)
{
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_events = hub_events;
    s_ctx.state = NP_SESSION_IDLE;
}

np_hub_status_t np_runner_load(const uint8_t *proto_buf, size_t proto_len)
{
    if (s_ctx.state == NP_SESSION_RUNNING || s_ctx.state == NP_SESSION_STOPPING) {
        return NP_HUB_ERR_SESSION_ACTIVE;
    }
    if (proto_buf == NULL || proto_len == 0U) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.state = NP_SESSION_LOADING;

    s_ctx.state = NP_SESSION_VERIFYING;
    np_hub_status_t rc = np_protocol_verify_and_parse(proto_buf, proto_len,
                                                       &s_ctx.desc);
    if (rc != NP_HUB_OK) {
        s_ctx.state = NP_SESSION_IDLE;
        return rc;
    }

    /* Initialise stop-tracking to "no stop pending". */
    for (uint8_t i = 0U; i < NP_HUB_SLOT_MAX; i++) {
        s_ctx.stop_at_ms[i] = 0U;
    }

    s_ctx.state        = NP_SESSION_IDLE;
    s_ctx.next_cmd_idx = 0U;

    xEventGroupSetBits(s_events, NP_EV_SESSION_START);
    return NP_HUB_OK;
}

np_hub_status_t np_runner_run(void)
{
    s_ctx.state      = NP_SESSION_RUNNING;
    s_ctx.start_tick = xTaskGetTickCount();

    /* Populate UHDR session-start record. */
    memcpy(s_ctx.uhdr.session_uuid, s_ctx.desc.session_uuid,
           NP_HUB_PROTO_UUID_LEN);
    s_ctx.uhdr.start_unix      = s_ctx.desc.compiled_at_unix;
    s_ctx.uhdr.mods_active_mask= 0U;
    s_ctx.uhdr.abort_reason    = (uint8_t)NP_ABORT_NONE;

    np_log_session_start(&s_ctx.uhdr);

    uint32_t last_telem_ms = 0U;

    for (;;) {
        uint32_t now_ms = elapsed_ms_now();
        s_ctx.elapsed_ms = now_ms;

        /* Check for abort/fault signals. */
        EventBits_t ev = xEventGroupGetBits(s_events);
        if (ev & (NP_EV_SESSION_ABORT | NP_EV_SAFETY_FAULT |
                  NP_EV_THERMAL_FAULT | NP_EV_EEG_PPX | NP_EV_POWER_FAULT)) {
            if (ev & NP_EV_SAFETY_FAULT)   s_ctx.abort_reason = NP_ABORT_SAFETY_MCU;
            else if (ev & NP_EV_THERMAL_FAULT) s_ctx.abort_reason = NP_ABORT_THERMAL;
            else if (ev & NP_EV_EEG_PPX)   s_ctx.abort_reason = NP_ABORT_EEG_SEIZURE;
            else if (ev & NP_EV_POWER_FAULT) s_ctx.abort_reason = NP_ABORT_POWER;
            else                             s_ctx.abort_reason = NP_ABORT_USER;
            break;
        }

        /* Session duration exceeded */
        if (s_ctx.desc.duration_ms > 0U && now_ms >= s_ctx.desc.duration_ms) {
            break;
        }

        /* Dispatch all commands whose start_ms ≤ now_ms */
        while (s_ctx.next_cmd_idx < s_ctx.desc.cmd_count &&
               s_ctx.desc.cmds[s_ctx.next_cmd_idx].start_ms <= now_ms) {
            dispatch_command(&s_ctx.desc.cmds[s_ctx.next_cmd_idx], now_ms);
            /* Track which module types are active for UHDR record. */
            s_ctx.uhdr.mods_active_mask |=
                (1U << (uint8_t)s_ctx.desc.cmds[s_ctx.next_cmd_idx].mod_type);
            s_ctx.next_cmd_idx++;
        }

        /* Issue implicit stops for expired commands */
        process_stops(now_ms);

        /* Trigger telemetry snapshot */
        if (now_ms - last_telem_ms >= NP_RUNNER_TELEM_INTERVAL_MS) {
            last_telem_ms = now_ms;
            np_telem_record_t rec;
            for (uint8_t slot = 0U; slot < NP_HUB_SLOT_MAX; slot++) {
                np_mod_entry_t *mod = np_mod_reg_get(slot);
                if (mod == NULL || mod->telemetry == NULL) {
                    continue;
                }
                rec.session_ms = now_ms;
                if (mod->telemetry(slot, &rec) == NP_HUB_OK) {
                    np_log_telemetry(&rec);
                }
            }
        }

        /* Sleep until the next interesting event. */
        uint32_t sleep_ms = ms_until_next_event(now_ms);
        vTaskDelay(pdMS_TO_TICKS(sleep_ms));
    }

    /* ── Graceful shutdown ─────────────────────────────────────────────────────── */

    s_ctx.state = NP_SESSION_STOPPING;
    np_safety_spi_disable_all();

    /* Send stop to all active modules (they handle ramp-down internally). */
    for (uint8_t slot = 0U; slot < NP_HUB_SLOT_MAX; slot++) {
        np_mod_entry_t *mod = np_mod_reg_get(slot);
        if (mod != NULL && mod->control != NULL) {
            (void)mod->control(slot, NULL, 0U);
        }
    }

    /* Wait up to NP_RUNNER_SHUTDOWN_MS for modules to complete ramp-down. */
    vTaskDelay(pdMS_TO_TICKS(NP_RUNNER_SHUTDOWN_MS));

    /* Flush all active stops */
    process_stops(UINT32_MAX);

    /* Finalise records */
    uint32_t total_ms = elapsed_ms_now();
    s_ctx.uhdr.duration_s   = total_ms / 1000U;
    s_ctx.uhdr.abort_reason = (uint8_t)s_ctx.abort_reason;
    s_ctx.shdr.duration_s   = total_ms / 1000U;
    s_ctx.shdr.abort_reason = (uint8_t)s_ctx.abort_reason;
    s_ctx.shdr.mods_active_mask = s_ctx.uhdr.mods_active_mask;

    np_log_session_end(&s_ctx.uhdr, &s_ctx.shdr);
    np_log_flush();

    s_ctx.state = (s_ctx.abort_reason == NP_ABORT_NONE)
                  ? NP_SESSION_COMPLETE
                  : NP_SESSION_FAULT;

    return (s_ctx.abort_reason == NP_ABORT_NONE) ? NP_HUB_OK : NP_HUB_ERR_SAFETY_FAULT;
}

void np_runner_abort(np_abort_reason_t reason)
{
    s_ctx.abort_reason = reason;
    xEventGroupSetBits(s_events, NP_EV_SESSION_ABORT);
}

np_session_state_t np_runner_get_state(void)
{
    np_session_state_t st;
    taskENTER_CRITICAL();
    st = s_ctx.state;
    taskEXIT_CRITICAL();
    return st;
}

uint32_t np_runner_elapsed_ms(void)
{
    if (s_ctx.state != NP_SESSION_RUNNING) {
        return 0U;
    }
    return elapsed_ms_now();
}
