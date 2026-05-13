/*
 * NeuroPulse 1064nm Smart Zone Module — T2 Combined Session API
 * Document: NP-FW-PBM1064-001 Rev A §8
 *
 * Coordinates 1064nm smart zone modules with the T2 1170nm laser subsystem
 * (Issue #54 — API stubs used here until that module ships).
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

/* T2 combined session context. */
typedef struct {
    np_t2_stage_t             stage;
    np_pbm1064_session_ctx_t  pbm1064;    /* 1064nm module session context        */

    /* T2 1170nm subsystem state (stub until Issue #54) */
    bool     t2_1170_active;
    uint8_t  t2_1170_throttle_pct;       /* 0–100; 100=full, 0=off               */

    /* Thermal throttle cascade tracking */
    bool     throttle_1170_applied;
    bool     throttle_ch_c_applied;
    bool     throttle_ch_b_applied;
    bool     throttle_ch_a_applied;
    float    aggregate_thermal_load;

    /* Combined session timing */
    uint32_t session_start_ms;
    uint32_t last_tick_ms;

    /* Callbacks */
    np_pbm1064_session_end_cb_t session_end_cb;
    np_pbm1064_fault_cb_t       fault_cb;
} np_pbm1064_t2_ctx_t;

/*
 * Initialise T2 combined session context.
 */
void np_pbm1064_t2_init(np_pbm1064_t2_ctx_t         *ctx,
                          np_pbm1064_session_end_cb_t  end_cb,
                          np_pbm1064_fault_cb_t        fault_cb,
                          uint32_t                     device_session_count);

/*
 * Start a combined T2 session.  Both the 1064nm session and the 1170nm laser
 * ramp in parallel; safety MCU issues a single combined enable.
 */
np_pbm1064_status_t np_pbm1064_t2_start(
    np_pbm1064_t2_ctx_t               *ctx,
    const np_pbm1064_session_desc_t   *desc_1064);

/*
 * T2 combined session tick.  Call at NP_PBM1064_DOSE_TICK_MS intervals.
 * Evaluates thermal throttle cascade and delegates dose ticks.
 */
np_pbm1064_status_t np_pbm1064_t2_tick(np_pbm1064_t2_ctx_t *ctx,
                                          uint32_t now_ms);

/*
 * Apply the thermal throttle cascade.
 * Priority order (highest thermal load first):
 *   1. 1170nm T2 laser (np_pbm1064_hal_t2_throttle_request)
 *   2. 1064nm CH_C
 *   3. 808nm CH_B
 *   4. 660nm CH_A
 * throttle_fraction: 0.0–1.0 (fraction of current output to remove).
 */
void np_pbm1064_t2_apply_thermal_throttle(np_pbm1064_t2_ctx_t *ctx,
                                             float throttle_fraction);

/*
 * Abort combined T2 session immediately.
 */
np_pbm1064_status_t np_pbm1064_t2_abort(np_pbm1064_t2_ctx_t *ctx,
                                           np_pbm1064_fault_t   reason);

/*
 * Query T2 combined session stage.
 */
np_t2_stage_t np_pbm1064_t2_stage(const np_pbm1064_t2_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* NP_PBM1064_T2_COMBINED_H */
