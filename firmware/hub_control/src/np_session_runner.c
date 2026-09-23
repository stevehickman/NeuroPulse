/*
 * NeurOne Hub Control Program — Session Runner Implementation
 * Document: NP-FW-HUB-001 Rev 2 §5
 *
 * Execution model:
 *  - Commands in desc.cmds[] are pre-sorted by start_ms (np_protocol_sort_cmds).
 *  - np_runner_run() loops on a NP_RUNNER_TICK_MS schedule, dispatching commands
 *    whose start_ms ≤ elapsed_ms.
 *  - Each command registers a stop event at (start_ms + duration_ms) in
 *    s_ctx.stop_at_ms[slot].  If a new command arrives for the same slot before
 *    the stop fires, the stop time is overwritten.
 *  - A socket-addressed command (NP_PROTO_TARGET_SOCKET_MASK) is routed to the
 *    socket-indexed registry (np_socket_dispatch.c), which tracks its own
 *    per-socket stop times; it never falls back to the slot path.
 *  - Telemetry is requested every NP_RUNNER_TELEM_INTERVAL_MS via a callback
 *    posted to the telemetry task queue.
 *  - NP_EV_SESSION_ABORT or NP_EV_SAFETY_FAULT unblocks the loop immediately.
 */

#include "np_session_runner.h"
#include "np_protocol.h"
#include "np_module_registry.h"
#include "np_socket_dispatch.h"
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
/*
 * half_period_us — the duration of ONE phase of a periodic waveform, in µs,
 * from its frequency in milli-Hz.
 *
 *   f Hz = freq_mhz / 1000, half period = 1/(2f) s = 500e6 / freq_mhz µs
 *
 * e.g. 0.5 Hz (freq_mhz = 500) → 1,000,000 µs; 40 Hz → 12,500 µs.
 *
 * A zero frequency would be a division by zero and is a malformed descriptor;
 * it returns the clamp ceiling, which is the LONGEST phase and therefore the
 * strictest per-phase verdict the safety MCU can reach — a malformed
 * descriptor fails closed rather than escaping the check.  (The MCU applies
 * the same clamp itself; this one keeps the wire value honest as well.)
 */
static uint32_t half_period_us(uint16_t freq_mhz)
{
    uint32_t us;
    if (freq_mhz == 0U) {
        return (uint32_t)NP_CHARGE_MAX_PHASE_US;
    }
    us = 500000000UL / (uint32_t)freq_mhz;
    return (us > (uint32_t)NP_CHARGE_MAX_PHASE_US)
             ? (uint32_t)NP_CHARGE_MAX_PHASE_US : us;
}

/*
 * declare_phase — record a channel's phase duration, keeping the LONGEST when
 * several commands land on one channel.  Longest phase means largest per-phase
 * charge at a given amplitude, so keeping the maximum is the strict reading —
 * the same rule, in the opposite direction, as taking the SMALLEST declared
 * electrode area.  (The safety MCU applies this max independently; doing it
 * here too means the transmitted frame already says what the MCU will
 * conclude, so the two sides never disagree about what was declared.)
 */
static void declare_phase(uint32_t *phase_us, uint8_t channel, uint32_t us)
{
    if (us > phase_us[channel]) {
        phase_us[channel] = us;
    }
}

static uint16_t slot_to_safety_bit(uint8_t slot)
{
    static const uint16_t k_map[NP_HUB_SLOT_MAX] = {
        /* Slots 0-4 (the retired zone slots) deliberately have NO entry, so they
         * zero-init to 0 and request no enable.  Cranial PBM is gated by the one
         * NP_SAFETY_EN_PBM_CRANIAL bit (NP-HW-HUB-001 Rev 3 §7.2), and the
         * requester for it is the socket-dispatch path (np_socket_dispatch.c),
         * which owns that bit outright.  Mapping a
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
 * dispatch_command — route a command to the registry its target kind names,
 * and for a slot command request enable from the safety MCU and record the
 * stop time.
 *
 * TWO REGISTRIES, NO FALLBACK (OI-FWHUB-01, RISK-FWHUB-01).
 * ---------------------------------------------------------
 * A SLOT command goes to np_module_registry. A SOCKET_MASK command goes to
 * np_socket_dispatch, which checks placement and power, drives every named
 * socket or none, and owns the one NP_SAFETY_EN_PBM_CRANIAL bit. Neither ever
 * falls through to the other: a socket command dispatched through the slot path
 * would deliver an eleven-socket frontal-left dose to whatever sits in slot 0,
 * and a wrong-site dose is not recoverable where a missed one is.
 *
 * A socket command the registry refuses — placement, power, a driver fault — is
 * logged to SHDR and returns false, so it never enters UHDR's delivered mask.
 * Today every transcranial DRIVE command is refused at np_pbm_power_admit(),
 * whose production definition is closed until the OI-HEXTILE-09 governor
 * exists (OI-FWHUB-09); stops are always admitted.
 */
static bool dispatch_command(const np_session_cmd_t *cmd, uint32_t now_ms)
{
    if (cmd->target_kind == NP_PROTO_TARGET_SOCKET_MASK) {
        np_hub_status_t src = np_sock_disp_command(cmd);
        if (src != NP_HUB_OK) {
            /* Logged against NP_HUB_SLOT_NONE, not slot 0: the command named no
             * slot, and attributing the refusal to the retired zone-0 slot would
             * put a fault in the SHDR device-health log against a module that was
             * never involved. The fault code says which gate refused it. */
            np_log_shdr_fault(NP_HUB_SLOT_NONE, cmd->mod_type,
                              (uint8_t)(-src), now_ms);
            if (s_ctx.abort_reason == NP_ABORT_NONE) {
                s_ctx.abort_reason = NP_ABORT_MOD_FAULT;
            }
            return false;
        }
        /* A stop is dispatched but delivers nothing; only a drive marks the
         * modality as delivered in UHDR. */
        return cmd->params_len > 0U;
    }

    if (cmd->target_kind != NP_PROTO_TARGET_SLOT) {
        /* Unreachable: the parser rejects every other kind. Fail closed. */
        np_log_shdr_fault(NP_HUB_SLOT_NONE, cmd->mod_type,
                          (uint8_t)(-NP_HUB_ERR_INVALID_ARG), now_ms);
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
    {
        uint32_t sock_stop = np_sock_disp_next_stop_ms(now_ms);
        if (sock_stop != 0U && sock_stop - now_ms < nearest) {
            nearest = sock_stop - now_ms;
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
    np_sock_disp_reset();
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

    /* ── OI-CHARGE-02 / -04: deliver electrode geometry to the safety MCU ────── */
    /* Scan the (already-verified) descriptor for the two modalities whose
     * electrode geometry the safety MCU cannot know on its own, and hand it
     * their per-electrode area so its charge monitor enforces the
     * geometry-correct 40µC/cm² limit instead of the 1000µC / 25cm² pad
     * default.  Only the area is sent — the density constant stays on the
     * safety MCU.  Sent during setup, before any enable is requested.
     *
     *   CLIN_STIM (OI-CHARGE-02) — HD-tDCS ring/bilateral 4×1 montages use the
     *     3.5mm electrodes, a fixed area the montage code implies (3.84µC).
     *   TDCS (OI-CHARGE-04) — T1 tDCS pad area is NOT implied by anything the
     *     descriptor otherwise carries: electrode_pair names 10-20 sites, not
     *     pad sizes.  It is authored per protocol and travels in the signed
     *     descriptor as np_mod_tdcs_params_t.electrode_area_mcm2, so the app
     *     pre-flight and this enforcer divide by the same declared number.
     *
     * A tDCS command that declares NO area (0) deliberately leaves
     * area_mcm2[TDCS] at 0 — "keep default" to the MCU — while still arming
     * the tDCS geometry gate, so the MCU never grants TDCS at all.  Silently
     * running such a protocol against the 25cm² fallback is the fail-OPEN
     * behaviour OI-CHARGE-03 rejected for CLIN_STIM.
     *
     * With several tDCS commands the SMALLEST declared area wins: the limit is
     * per-channel and the accumulator is shared across the session, so the
     * tightest declared geometry is the only conservative choice.            */
    {
        uint16_t area_mcm2[NP_SAFETY_MAX_CHANNELS];
        uint8_t  wave_class[NP_SAFETY_MAX_CHANNELS];
        uint32_t phase_us[NP_SAFETY_MAX_CHANNELS];
        bool     have_override = false;
        bool     have_tdcs     = false;
        bool     have_electrical = false;
        memset(area_mcm2,   0, sizeof(area_mcm2));
        memset(wave_class,  0, sizeof(wave_class));
        memset(phase_us,    0, sizeof(phase_us));

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
                /* HD-tDCS is DC — the per-session budget applies to it. */
                wave_class[NP_SAFETY_CH_CLIN_STIM] |= NP_CHARGE_WAVE_DC;
                have_electrical = true;
            } else if (c->mod_type == NP_MOD_TDCS &&
                       c->params_len >= sizeof(np_mod_tdcs_params_t)) {
                const np_mod_tdcs_params_t *p =
                    (const np_mod_tdcs_params_t *)(const void *)c->params;
                have_tdcs = true;
                if (p->electrode_area_mcm2 > 0U &&
                    (area_mcm2[NP_SAFETY_CH_TDCS] == 0U ||
                     p->electrode_area_mcm2 < area_mcm2[NP_SAFETY_CH_TDCS])) {
                    area_mcm2[NP_SAFETY_CH_TDCS] = p->electrode_area_mcm2;
                }
                wave_class[NP_SAFETY_CH_TDCS] |= NP_CHARGE_WAVE_DC;
                have_electrical = true;
            } else if (c->mod_type == NP_MOD_BES_TACS &&
                       c->params_len >= sizeof(np_mod_bes_tacs_params_t)) {
                const np_mod_bes_tacs_params_t *p =
                    (const np_mod_bes_tacs_params_t *)(const void *)c->params;
                area_mcm2[NP_SAFETY_CH_BES_TACS] = NP_BES_ELECTRODE_AREA_MCM2;
                wave_class[NP_SAFETY_CH_BES_TACS] |=
                    (p->waveform == 0U) ? NP_CHARGE_WAVE_SINE
                                        : NP_CHARGE_WAVE_PULSE;
                declare_phase(phase_us, NP_SAFETY_CH_BES_TACS,
                              half_period_us(p->freq_mhz));
                have_electrical = true;
            } else if (c->mod_type == NP_MOD_CLIN_TACS &&
                       c->params_len >= sizeof(np_mod_clin_tacs_params_t)) {
                const np_mod_clin_tacs_params_t *p =
                    (const np_mod_clin_tacs_params_t *)(const void *)c->params;
                /* Shares CLIN_STIM with HD-tDCS; classes OR together and the
                 * MCU runs both checks.  Declares no geometry of its own, so
                 * it never arms the OI-CHARGE-03 gate (unchanged behaviour). */
                wave_class[NP_SAFETY_CH_CLIN_STIM] |=
                    (p->waveform == 0U) ? NP_CHARGE_WAVE_SINE
                                        : NP_CHARGE_WAVE_PULSE;
                declare_phase(phase_us, NP_SAFETY_CH_CLIN_STIM,
                              half_period_us(p->freq_mhz));
                have_electrical = true;
            } else if (c->mod_type == NP_MOD_VNS_HRV &&
                       c->params_len >= sizeof(np_mod_vns_hrv_params_t)) {
                const np_mod_vns_hrv_params_t *p =
                    (const np_mod_vns_hrv_params_t *)(const void *)c->params;
                uint32_t pw = (p->pulse_width_us == 0U)
                                ? 250UL : (uint32_t)p->pulse_width_us;
                area_mcm2[NP_SAFETY_CH_VNS_HRV] = NP_VNS_ELECTRODE_AREA_MCM2;
                wave_class[NP_SAFETY_CH_VNS_HRV] |= NP_CHARGE_WAVE_PULSE;
                declare_phase(phase_us, NP_SAFETY_CH_VNS_HRV, pw);
                have_electrical = true;
            } else if (c->mod_type == NP_MOD_CVNS &&
                       c->params_len >= sizeof(np_mod_cvns_params_t)) {
                const np_mod_cvns_params_t *p =
                    (const np_mod_cvns_params_t *)(const void *)c->params;
                uint32_t pw = (p->pulse_width_us == 0U)
                                ? 250UL : (uint32_t)p->pulse_width_us;
                area_mcm2[NP_SAFETY_CH_CVNS] = NP_CVNS_ELECTRODE_AREA_MCM2;
                wave_class[NP_SAFETY_CH_CVNS] |= NP_CHARGE_WAVE_PULSE;
                declare_phase(phase_us, NP_SAFETY_CH_CVNS, pw);
                have_electrical = true;
            } else {
                /* modality drives no electrode */
            }
        }

        if (have_override) {
            /* OI-CHARGE-03: arm the safety MCU's fail-safe gate FIRST (every
             * heartbeat now carries GEOM_REQUIRED), then deliver the area.  If
             * the area command is lost, the MCU keeps CLIN_STIM disabled until
             * a retry lands — HD-tDCS never runs at the permissive default.   */
            np_safety_spi_set_geom_required(true);
        }
        if (have_tdcs) {
            /* OI-CHARGE-04: same ordering, own bit, own channel. */
            np_safety_spi_set_geom_required_tdcs(true);
        }
        if (have_override || have_tdcs) {
            (void)np_safety_spi_send_channel_limits(area_mcm2,
                                                    NP_SAFETY_MAX_CHANNELS);
        }
        if (have_electrical) {
            /* OI-CHARGE-05 (b): tell the MCU which ceiling each electrical
             * channel is held to.  No arming bit is needed — the MCU's
             * declaration gate blocks any electrical channel it has heard
             * nothing about, so losing this frame costs the session its
             * electrical modalities rather than their monitoring.            */
            (void)np_safety_spi_send_channel_waveforms(wave_class, phase_us,
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
             * record; a command that was refused (e.g. a socket target the power
             * governor did not admit, OI-FWHUB-09) must not appear in it as
             * delivered. */
            if (dispatch_command(&s_ctx.desc.cmds[s_ctx.next_cmd_idx], now_ms)) {
                s_ctx.uhdr.mods_active_mask |=
                    (1U << (uint8_t)s_ctx.desc.cmds[s_ctx.next_cmd_idx].mod_type);
            }
            s_ctx.next_cmd_idx++;
        }

        /* Issue implicit stops for expired commands */
        process_stops(now_ms);
        np_sock_disp_process_stops(now_ms);

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
    np_sock_disp_stop_all();

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
