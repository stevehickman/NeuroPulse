/*
 * NeurOne 1064nm Smart Zone Module — Detection API
 * Document: NP-FW-PBM1064-001 Rev 1 §4
 */

#ifndef NP_PBM_DETECT_H
#define NP_PBM_DETECT_H

#include <stdint.h>
#include <stdbool.h>
#include "np_pbm_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Per-slot detection context array (one per physical slot, 0–4).
 * Static allocation; caller must not free or move.
 */
typedef struct {
    np_sm_slot_ctx_t slots[NP_PBM_ZONE_COUNT];
    np_pbm_remove_cb_t remove_cb;
    uint32_t               device_session_count;
} np_pbm_detect_ctx_t;

/* Initialize detection context. Must be called once before np_pbm_detect_tick(). */
void np_pbm_detect_init(np_pbm_detect_ctx_t *ctx,
                              np_pbm_remove_cb_t   remove_cb,
                              uint32_t                 device_session_count);

/*
 * Poll one tick for all 5 slots.  Call at NP_PBM_POLL_IDLE_MS intervals
 * from the detection FreeRTOS task.
 *
 * For each slot that completes the detection sequence (debounce + I2C probe),
 * the slot type and state are updated in ctx.  The announce HAL stub and the
 * insert callback fire inside this function.
 *
 * insert_cb: invoked when a smart module is confirmed.  Receives slot index.
 */
void np_pbm_detect_tick(np_pbm_detect_ctx_t *ctx,
                              void (*insert_cb)(uint8_t slot));

/*
 * Query current slot type (no side effects; safe to call any time).
 */
np_slot_type_t np_pbm_detect_slot_type(const np_pbm_detect_ctx_t *ctx,
                                             uint8_t slot);

/*
 * Return a bitmask of slots currently in NP_SM_ACTIVE state with
 * NP_SLOT_SMART type.  Bit 0 = slot 0, bit 4 = slot 4.
 */
uint8_t np_pbm_detect_smart_mask(const np_pbm_detect_ctx_t *ctx);

/*
 * Classify a raw ADC count as slot type.
 * Used by FAI-SM-01 and FAI-SM-03 without side effects.
 */
np_slot_type_t np_pbm_classify_adc(uint16_t counts);

#ifdef __cplusplus
}
#endif

#endif /* NP_PBM_DETECT_H */
