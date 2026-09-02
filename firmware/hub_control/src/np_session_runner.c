/*
 * NeurOne Hub Control Program — Session Runner Implementation
 * Document: NP-FW-HUB-001 Rev 1 §5
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
#include "np_cvns_reenable.h"
#include "np_cvns_config.h"       /* NP_CVNS_STIM_TICK_MS */
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include <string.h>
#include <limits.h>

#include "np_sw02_platform_hal.h"

/*
 * Cervical VNS scheduler wiring (OI-CVNS-HUB-07).  The module driver exposes no
 * per-module header (registry idiom); these are its public entry points
 * (modules/np_mod_cvns.c).  While a CVNS command is active the runner drives
 * np_mod_cvns_tick() every NP_CVNS_STIM_TICK_MS so the cervical-VNS cardiac
 * interlock, enable-bit handoff, and current ramp advance in real time.
 * now_ms/now_s use the CVNS HAL clocks — the SAME free-running ms domain the
 * PPG-ISR seam stamps samples with — not the runner's session-relative clock.
 */
extern bool     np_mod_cvns_active(void);
extern void     np_mod_cvns_tick(uint32_t now_ms, uint32_t now_s);

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
        /* Slots 0-4 (the retired zone slots) deliberately have NO entry, so they
         * zero-init to 0 and request no enable.  Cranial PBM is gated by the one
         * NP_SAFETY_EN_PBM_CRANIAL bit (NP-HW-HUB-001 Rev 3 §7.2), and the
         * requester for it is the socket-dispatch path, which does not exist yet
         * (OI-HUB-SOCKET-01 — see the dispatch_command note below).  Mapping a
         * retired slot to the cranial bit instead would let a stale zone target
         * enable the WHOLE lattice; 0 fails closed.  np_protocol_verify_and_parse
         * already rejects slot_id < NP_HUB_SLOT_FIRST_VALID, so this is the
         * second of two independent barriers, not the only one.               */
        [NP_HUB_SLOT_EEG]        = 0U,  /* EEG is passive — no safety MCU gate */
        [NP_HUB_SLOT_AUDIO]      = NP_SAFETY_EN_AUDIO,
        [NP_HUB_SLOT_VISUAL]     = NP_SAFETY_EN_VISUAL,
        [NP_HUB_SLOT_VNS_HRV]   = NP_SAFETY_EN_VNS_HRV,
        [NP_HUB_SLOT_INTRANASAL] = NP_SAFETY_EN_INTRANASAL,
        [NP_HUB_SLOT_CVNS]       = NP_SAFETY_EN_CVNS,
        [NP_HUB_SLOT_TMS]        = NP_SAFETY_EN_TMS,
        [NP_HUB_SLOT_PBM_1170NM] = NP_SAFETY_EN_PBM_1170NM,
        /* CLIN_TACS and HD_TDCS share the one 21-ch driver, hence one gate. */
        [NP_HUB_SLOT_CLIN_TACS]  = NP_SAFETY_EN_CLIN_STIM,
        [NP_HUB_SLOT_HD_TDCS]    = NP_SAFETY_EN_CLIN_STIM,
        [NP_HUB_SLOT_BES_TACS]   = NP_SAFETY_EN_BES_TACS,
        [NP_HUB_SLOT_TDCS]       = NP_SAFETY_EN_TDCS,
        /* QEEG is passive recording, VIBROTACTILE is not safety-MCU gated. */
    };
    return (slot < NP_HUB_SLOT_MAX) ? k_map[slot] : 0U;
}

/*
 * dispatch_command — call the registered control function for the command's
 * slot, request enable from safety MCU, and record stop time.
 *
 * SOCKET-ADDRESSED COMMANDS ARE NOT DISPATCHED (OI-HUB-SOCKET-01).
 * ---------------------------------------------------------------
 * Protocol v2 lets a cranial command name helmet SOCKETS rather than slots
 * (np_proto_target_kind_t). The parser understands them and np_module_map
 * resolves them to (socket:element) addresses — but the control-dispatch path
 * below is still the legacy fixed-slot registry (np_module_registry), which has
 * no socket-indexed entries. The safety-MCU enable bitmap is no longer the
 * blocker it once was — NP-HW-HUB-001 Rev 3 §7.2 replaced the per-zone-slot
 * NP_SAFETY_EN_PBM_ZONE_0..4 with the single NP_SAFETY_EN_PBM_CRANIAL bit, which
 * a socket-addressed command can request unchanged. What remains missing is the
 * socket-indexed dispatch registry itself.
 *
 * Until then a socket-addressed command is logged and DROPPED. The alternative
 * — falling back to the slot path — would dispatch a command targeting, say,
 * eleven frontal-left sockets to whatever module sits in slot 0, i.e. deliver
 * stimulation somewhere the protocol never named. A missed dose is recoverable;
 * a wrong-site dose is not. Fail closed.
 */
static bool dispatch_command(const np_session_cmd_t *cmd, uint32_t now_ms)
{
    if (cmd->target_kind != NP_PROTO_TARGET_SLOT) {
        /* Logged against NP_HUB_SLOT_NONE, not slot 0: the command named no
         * slot, and attributing the drop to the retired zone-0 slot would put a
         * fault in the SHDR device-health log against a module that was never
         * involved. */
        np_log_shdr_fault(NP_HUB_SLOT_NONE, cmd->mod_type,
                          (uint8_t)(-NP_HUB_ERR_NOT_PRESENT), now_ms);
        if (s_ctx.abort_reason == NP_ABORT_NONE) {
            s_ctx.abort_reason = NP_ABORT_MOD_FAULT;
        }
        return false;
    }

    /* The parser has already pinned slot_id to a valid, non-retired slot for
     * this target kind, so this is a belt-and-braces bound check on the array
     * index rather than a validation. */
    const uint8_t slot = cmd->slot_id;
    if (slot >= NP_HUB_SLOT_MAX) {
        return false;
    }

    np_mod_entry_t *mod = np_mod_reg_get(slot);
    if (mod == NULL || mod->control == NULL) {
        return false;
    }

    const void *params     = (cmd->params_len > 0U) ? cmd->params : NULL;
    uint16_t    params_len = cmd->params_len;

    np_hub_status_t rc = mod->control(slot, params, params_len);
    if (rc != NP_HUB_OK) {
        /* Module fault — log to SHDR, record in session, but continue.
         * Recorded on s_ctx.abort_reason, not directly on s_ctx.shdr: the
         * session-end block overwrites shdr.abort_reason from s_ctx.abort_reason
         * unconditionally, so writing the record field here was erased before it
         * was ever logged. Non-fatal: session continues; safety MCU owns hard
         * cutoff. */
        if (s_ctx.abort_reason == NP_ABORT_NONE) {
            s_ctx.abort_reason = NP_ABORT_MOD_FAULT;
        }
        return false;
    }

    /* Request enable from safety MCU for stimulation channels. */
    if (params != NULL) {
        uint16_t bit = slot_to_safety_bit(slot);
        if (bit != 0U) {
            np_safety_spi_request_enable(bit);
        }
    }

    /* Record when this slot should auto-stop. The parser has already rejected a
     * start_ms + duration_ms that would wrap, so this sum cannot land on the
     * "no stop pending" sentinel 0. */
    if (cmd->duration_ms > 0U) {
        s_ctx.stop_at_ms[slot] = cmd->start_ms + cmd->duration_ms;
    }
    return true;
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

    /* Initialize stop-tracking to "no stop pending". */
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
    /* OI-CVNS-HUB-01: hard-reset the CVNS re-enable manager at session start —
     * no cutoff/confirmation/assertion state may leak across session
     * boundaries.  (Belt-and-braces: the manager also self-resets on any
     * heartbeat where the session is not RUNNING, and
     * np_safety_spi_disable_all() clears the SPI-level bit on session end.) */
    np_cvns_reenable_session_reset();

    s_ctx.state      = NP_SESSION_RUNNING;
    s_ctx.start_tick = xTaskGetTickCount();

    /* Populate UHDR session-start record. */
    memcpy(s_ctx.uhdr.session_uuid, s_ctx.desc.session_uuid,
           NP_HUB_PROTO_UUID_LEN);
    s_ctx.uhdr.start_unix      = s_ctx.desc.compiled_at_unix;
    s_ctx.uhdr.mods_active_mask= 0U;
    s_ctx.uhdr.abort_reason    = (uint8_t)NP_ABORT_NONE;

    np_log_session_start(&s_ctx.uhdr);

    /* ── OI-CHARGE-02: deliver electrode geometry to the safety MCU ──────────── */
    /* Scan the (already-verified) descriptor for HD-tDCS commands using the
     * small 3.5mm ring/bilateral electrodes and hand the safety MCU their
     * electrode area so its charge monitor enforces the geometry-correct
     * 40µC/cm² limit (3.84µC) on CLIN_STIM instead of the 1000µC pad default.
     * Only the area is sent — the density constant stays on the safety MCU.
     * Sent during setup, before any CLIN_STIM enable is requested.            */
    {
        uint16_t area_mcm2[NP_SAFETY_MAX_CHANNELS];
        bool     have_override = false;
        memset(area_mcm2, 0, sizeof(area_mcm2));

        for (uint8_t i = 0U; i < s_ctx.desc.cmd_count; i++) {
            const np_session_cmd_t *c = &s_ctx.desc.cmds[i];
            if (c->mod_type == NP_MOD_HD_TDCS &&
                c->params_len >= sizeof(np_mod_hd_tdcs_params_t)) {
                const np_mod_hd_tdcs_params_t *p =
                    (const np_mod_hd_tdcs_params_t *)(const void *)c->params;
                if (p->montage == NP_HD_MONTAGE_RING_4X1 ||
                    p->montage == NP_HD_MONTAGE_BILATERAL_4X1) {
                    area_mcm2[NP_SAFETY_CH_CLIN_STIM] =
                        NP_HD_SMALL_ELECTRODE_AREA_MCM2;
                    have_override = true;
                }
            }
        }

        if (have_override) {
            /* OI-CHARGE-03: arm the safety MCU's fail-safe gate FIRST (every
             * heartbeat now carries GEOM_REQUIRED), then deliver the area.  If
             * the area command is lost, the MCU keeps CLIN_STIM disabled until
             * a retry lands — HD-tDCS never runs at the permissive default.   */
            np_safety_spi_set_geom_required(true);
            (void)np_safety_spi_send_channel_limits(area_mcm2,
                                                    NP_SAFETY_MAX_CHANNELS);
        }
    }

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
            /* Track which module types are active for UHDR record — only when
             * the command was actually dispatched. UHDR is the patient's dose
             * record; a command that was dropped (a socket target, until
             * OI-HUB-SOCKET-01) must not appear in it as delivered. */
            if (dispatch_command(&s_ctx.desc.cmds[s_ctx.next_cmd_idx], now_ms)) {
                s_ctx.uhdr.mods_active_mask |=
                    (1U << (uint8_t)s_ctx.desc.cmds[s_ctx.next_cmd_idx].mod_type);
            }
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

        /* ── OI-CVNS-HUB-07: drive the cervical VNS state machine ──────────────
         * While a CVNS command is active, tick the module every
         * NP_CVNS_STIM_TICK_MS so its cardiac interlock, unified-enable handoff,
         * and ramp advance in real time.  now_ms/now_s come from the CVNS HAL
         * clocks (the same free-running ms domain the PPG-ISR seam stamps samples
         * with, which the interlock's R-R timing depends on) — not the runner's
         * session-relative clock.  The tick no-ops once the module reaches a
         * terminal stage and clears active. */
        if (np_mod_cvns_active()) {
            np_mod_cvns_tick(np_mod_cvns_hal_now_ms(), np_mod_cvns_hal_now_unix());
        }

        /* Sleep until the next interesting event, but never past one CVNS tick
         * period while a CVNS session is active (re-read: a terminal tick above
         * may have just cleared active). */
        uint32_t sleep_ms = ms_until_next_event(now_ms);
        if (np_mod_cvns_active() && sleep_ms > NP_CVNS_STIM_TICK_MS) {
            sleep_ms = NP_CVNS_STIM_TICK_MS;
        }
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
