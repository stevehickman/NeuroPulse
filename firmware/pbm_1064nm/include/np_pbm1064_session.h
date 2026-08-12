/*
 * NeurOne 1064nm Smart Zone Module — Session Orchestration API
 * Document: NP-FW-PBM1064-001 Rev 1 §7 / NP-SES-1064-001 §1 (v5)
 */

#ifndef NP_PBM1064_SESSION_H
#define NP_PBM1064_SESSION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "np_pbm1064_types.h"
#include "np_pbm1064_drive.h"
#include "np_pbm1064_dose.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Session context (single instance; static allocation).
 *
 * active_socket_id[i] / active_preset[i] / drv[i] / dose[i] / cal[i] all
 * describe the same i-th socket of the session's expanded active-socket
 * list (see np_pbm1064_session_desc_expand()) — NOT array index == socket
 * ID. Only the first active_socket_count entries are meaningful.
 */
typedef struct {
    np_pbm1064_stage_t        stage;
    np_pbm1064_session_desc_t desc;

    uint8_t              active_socket_count;
    uint8_t              active_socket_id[NP_PBM1064_SESSION_MAX_ACTIVE_SOCKETS];
    np_pbm1064_preset_t  active_preset[NP_PBM1064_SESSION_MAX_ACTIVE_SOCKETS];

    np_pbm1064_drv_slot_t     drv[NP_PBM1064_SESSION_MAX_ACTIVE_SOCKETS];
    np_pbm1064_dose_state_t   dose[NP_PBM1064_SESSION_MAX_ACTIVE_SOCKETS];
    np_pbm1064_cal_t          cal[NP_PBM1064_SESSION_MAX_ACTIVE_SOCKETS][NP_PBM1064_WL_COUNT];
    np_pbm1064_session_record_t record;

    /* Ramp state */
    uint32_t ramp_start_ms;
    uint32_t ramp_tick_count;

    /* Dose tick tracking */
    uint32_t last_dose_tick_ms;
    uint32_t last_i2c_poll_ms;
    uint32_t last_ntc_poll_ms;

    /* EEG-adaptive state */
    float    eeg_dominant_freq_hz;   /* updated by EEG layer at 1 Hz              */
    uint8_t  eeg_tick_in_band[NP_PBM1064_SESSION_MAX_ACTIVE_SOCKETS]; /* hysteresis counter */
    uint8_t  current_freq_code;      /* uniform-mode shared freq code             */

    /* NTC throttle state */
    float    ntc_temp_c[NP_PBM1064_SESSION_MAX_ACTIVE_SOCKETS];
    bool     ch_c_throttled;
    bool     ch_b_throttled;

    /* Callbacks */
    np_pbm1064_session_end_cb_t session_end_cb;
    np_pbm1064_fault_cb_t       fault_cb;
    np_pbm1064_display_cb_t     display_cb;

    uint32_t device_session_count;
} np_pbm1064_session_ctx_t;

/*
 * Initialize session context.  Must be called once before start.
 */
void np_pbm1064_session_init(np_pbm1064_session_ctx_t    *ctx,
                               np_pbm1064_session_end_cb_t  end_cb,
                               np_pbm1064_fault_cb_t        fault_cb,
                               np_pbm1064_display_cb_t      display_cb,
                               uint32_t                     device_session_count);

/*
 * Begin a session.  Verifies Ed25519 signature, runs preflight, starts ramp.
 * desc must remain valid for the duration of the session.
 * Returns NP_PBM1064_ERR_SESSION_ACTIVE if already running,
 *         NP_PBM1064_ERR_INVALID_ARG if the descriptor's groups address more
 *         than NP_PBM1064_SESSION_MAX_ACTIVE_SOCKETS distinct sockets.
 */
np_pbm1064_status_t np_pbm1064_session_start(
    np_pbm1064_session_ctx_t          *ctx,
    const np_pbm1064_session_desc_t   *desc);

/*
 * Session tick — call at NP_PBM1064_DOSE_TICK_MS (100 ms) intervals.
 * Drives ramp, dose metering, I2C status poll, NTC poll, EEG-adaptive.
 * Returns NP_PBM1064_OK during normal operation;
 *         NP_PBM1064_ERR_NO_SESSION if no session is active.
 */
np_pbm1064_status_t np_pbm1064_session_tick(np_pbm1064_session_ctx_t *ctx,
                                               uint32_t now_ms);

/*
 * Deliver an updated EEG dominant frequency to the running session.
 * Called by the EEG processing task at 1 Hz (uniform adaptive mode).
 * No-op if no session active or eeg_adaptive_mode == 0.
 */
void np_pbm1064_session_update_eeg_freq(np_pbm1064_session_ctx_t *ctx,
                                          float dominant_freq_hz);

/*
 * Abort a running session immediately.  Disables all channels.
 */
np_pbm1064_status_t np_pbm1064_session_abort(np_pbm1064_session_ctx_t *ctx,
                                               np_pbm1064_fault_t reason);

/*
 * Query current session stage.
 */
np_pbm1064_stage_t np_pbm1064_session_stage(
    const np_pbm1064_session_ctx_t *ctx);

/* ═══════════════════════════════════════════════════════════════════════════
 * Session descriptor v5 wire encode/decode (NP-SES-1064-001 §1)
 *
 * The Ed25519 signature covers exactly the transmitted bytes — hdr (8B) +
 * hdr.group_count * sizeof(np_pbm1064_preset_group_t) — never the full
 * NP_PBM1064_SESSION_MAX_PRESET_GROUPS capacity. See the struct-level
 * comment on np_pbm1064_session_desc_t in np_pbm1064_types.h for why that
 * distinction is security-relevant, not cosmetic.
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * Byte length of the full wire encoding (header + group_count groups +
 * 64-byte signature) for a descriptor with the given group_count.
 * Returns 0 if group_count > NP_PBM1064_SESSION_MAX_PRESET_GROUPS.
 */
size_t np_pbm1064_session_desc_wire_len(uint8_t group_count);

/*
 * Byte length of the SIGNED span only (wire length minus the trailing
 * signature) — what Ed25519 signs and what a verifier must be called over.
 * Returns 0 if group_count > NP_PBM1064_SESSION_MAX_PRESET_GROUPS.
 */
size_t np_pbm1064_session_desc_signed_len(uint8_t group_count);

/*
 * Serialize desc->hdr.group_count groups (NOT the full array capacity) into
 * `out`. out_cap must be >= np_pbm1064_session_desc_wire_len(desc->hdr.group_count).
 * Returns bytes written, or 0 on invalid group_count or insufficient out_cap.
 */
size_t np_pbm1064_session_desc_serialize(const np_pbm1064_session_desc_t *desc,
                                          uint8_t *out, size_t out_cap);

/*
 * Parse `in_len` wire bytes into `desc_out`, zeroing every group beyond the
 * parsed group_count. Validates group_count <= NP_PBM1064_SESSION_MAX_PRESET_GROUPS
 * and that in_len exactly equals np_pbm1064_session_desc_wire_len(group_count)
 * (a short or padded blob is rejected, not truncated/zero-extended).
 *
 * Does NOT verify the Ed25519 signature — the caller must do that separately,
 * over the first np_pbm1064_session_desc_signed_len(group_count) bytes of
 * `in`, before trusting the parsed result. Signature verification remains a
 * stub pending OI-PBM-SIG (unchanged from v4) — this function only makes the
 * signed span computable correctly for whenever that HAL lands.
 */
np_pbm1064_status_t np_pbm1064_session_desc_parse(const uint8_t *in, size_t in_len,
                                                    np_pbm1064_session_desc_t *desc_out);

/*
 * Expand a parsed descriptor's preset groups into a flat, ascending,
 * deduplicated active-socket list: active_socket_id_out[i] / active_preset_out[i]
 * name the i-th distinct socket addressed by any group and the preset that
 * applies to it. If a socket's bit is set in more than one group's mask, the
 * FIRST group (lowest index) wins — matching the array's signed order.
 *
 * `capacity` bounds both output arrays. Returns NP_PBM1064_ERR_INVALID_ARG
 * (fail closed, writing nothing) if the combined groups address more than
 * `capacity` distinct sockets, rather than silently driving a subset of what
 * was requested — a missed dose is recoverable, a partially-honoured session
 * is not.
 */
np_pbm1064_status_t np_pbm1064_session_desc_expand(
    const np_pbm1064_session_desc_t *desc,
    uint8_t              *active_socket_id_out,
    np_pbm1064_preset_t  *active_preset_out,
    uint8_t              *active_socket_count_out,
    uint8_t               capacity);

#ifdef __cplusplus
}
#endif

#endif /* NP_PBM1064_SESSION_H */
