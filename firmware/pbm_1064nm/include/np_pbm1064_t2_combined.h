/*
 * NeuroPulse 1064nm Smart Zone Module — T2 Combined Session API
 * Document: NP-FW-PBM1064-001 Rev A §8 / NP-SES-1064-001 §6
 *
 * Coordinates 1064nm smart zone modules (cortical, ~35mm depth) with the T2
 * 1170nm laser diode system (subcortical, ~40mm depth) into a single unified
 * session. Three-tier penetration stack:
 *   660nm surface → 1064nm cortical → 1170nm deep subcortical
 *
 * Thermal throttle priority (highest heat deposition first):
 *   1. 1170nm laser (subcortical — TEC-stabilized but highest per-watt absorption)
 *   2. 1064nm CH_C (on smart zone module)
 *   3. 808nm CH_B
 *   4. 660nm CH_A (surface — lowest thermal contribution)
 *
 * sLORETA coordination (T2 depression protocol, OI-SES-T2-01):
 *   np_fw_sloreta provides the target MNI coordinate; this module reads it at
 *   session start and logs to UHDR.  Pending T2 prototype — stub active now.
 *
 * Both 1064nm session and 1170nm laser ramp in parallel under a single safety
 * MCU combined enable pulse (NP-FW-PBM1064-001 §8.3).
 */

#ifndef NP_PBM1064_T2_COMBINED_H
#define NP_PBM1064_T2_COMBINED_H

#include <stdint.h>
#include <stdbool.h>
#include "np_pbm1064_types.h"
#include "np_pbm1064_session.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── T2 combined session context ─────────────────────────────────────────────── */

typedef struct {
    np_t2_stage_t             stage;

    /* Inner 1064nm session */
    np_pbm1064_session_ctx_t  pbm1064;

    /* T2 1170nm laser state */
    bool     t2_1170_active;
    uint8_t  t2_1170_duty_pct;         /* current laser duty (0–100)               */
    uint8_t  t2_1170_target_duty_pct;  /* session-descriptor target duty            */
    float    t2_1170_tec_temp_c;       /* last TEC temp reading                     */
    uint32_t t2_1170_tec_poll_ms;      /* last TEC poll timestamp                   */

    /* Thermal throttle cascade state (reset at session start) */
    bool     throttle_1170_applied;
    bool     throttle_ch_c_applied;
    bool     throttle_ch_b_applied;
    bool     throttle_ch_a_applied;

    /* Combined UHDR record (built during session, written at completion) */
    np_t2_combined_uhdr_record_t combined_record;

    /* sLORETA target read at session start */
    int16_t  sloreta_mni_x;
    int16_t  sloreta_mni_y;
    int16_t  sloreta_mni_z;
    bool     sloreta_valid;

    /* Session timing */
    uint32_t session_start_ms;
    uint32_t last_tick_ms;

    /* Callbacks */
    np_pbm1064_session_end_cb_t session_end_cb;
    np_pbm1064_fault_cb_t       fault_cb;
} np_pbm1064_t2_ctx_t;

/* ── API ─────────────────────────────────────────────────────────────────────── */

/*
 * Initialize T2 combined session context.  Must be called before any start.
 */
void np_pbm1064_t2_init(np_pbm1064_t2_ctx_t         *ctx,
                          np_pbm1064_session_end_cb_t  end_cb,
                          np_pbm1064_fault_cb_t        fault_cb,
                          uint32_t                     device_session_count);

/*
 * Start a T2 combined session from a combined session descriptor.
 * Both 1064nm and 1170nm subsystems ramp in parallel.
 * Returns NP_PBM1064_ERR_SESSION_ACTIVE if already running.
 */
np_pbm1064_status_t np_pbm1064_t2_start_combined(
    np_pbm1064_t2_ctx_t           *ctx,
    const np_t2_combined_desc_t   *desc);

/*
 * Legacy start (1064nm descriptor only; 1170nm duty defaults to 100%).
 * Kept for backwards-compatibility with FAI-SM-10.
 */
np_pbm1064_status_t np_pbm1064_t2_start(
    np_pbm1064_t2_ctx_t               *ctx,
    const np_pbm1064_session_desc_t   *desc_1064);

/*
 * T2 combined session tick — call at NP_PBM1064_DOSE_TICK_MS (100 ms) intervals.
 * Evaluates TEC temperature, throttle cascade, and delegates dose ticks.
 */
np_pbm1064_status_t np_pbm1064_t2_tick(np_pbm1064_t2_ctx_t *ctx,
                                          uint32_t now_ms);

/*
 * Apply the thermal throttle cascade.
 * throttle_fraction: 0.0–1.0 fraction of current output to reduce.
 *
 * Each call advances the cascade one step further:
 *   Call 1: 1170nm laser throttled by throttle_fraction.
 *   Call 2: 1064nm CH_C throttled by throttle_fraction.
 *   Call 3: 1064nm CH_B throttled by throttle_fraction.
 *   Call 4: 1064nm CH_A throttled (last resort).
 *
 * Once all four steps applied, function is a no-op until session reset.
 */
void np_pbm1064_t2_apply_thermal_throttle(np_pbm1064_t2_ctx_t *ctx,
                                             float throttle_fraction);

/*
 * Abort the combined T2 session immediately.
 * Disables all 1064nm channels and the 1170nm laser.
 */
np_pbm1064_status_t np_pbm1064_t2_abort(np_pbm1064_t2_ctx_t *ctx,
                                           np_pbm1064_fault_t   reason);

/*
 * Query T2 combined session stage.
 */
np_t2_stage_t np_pbm1064_t2_stage(const np_pbm1064_t2_ctx_t *ctx);

/*
 * Get a pointer to the completed UHDR combined session record.
 * Valid only after stage == NP_T2_STAGE_COMPLETE or NP_T2_STAGE_FAULT.
 */
const np_t2_combined_uhdr_record_t *np_pbm1064_t2_get_combined_record(
    const np_pbm1064_t2_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* NP_PBM1064_T2_COMBINED_H */
