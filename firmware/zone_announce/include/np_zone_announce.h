/*
 * NeuroPulse Zone Module Announcement — Public API
 * Document: NP-FW-ZA-001 Rev A §5
 *
 * Implements RISK-15 Layer 5 (five-layer zone module keying): bone conduction
 * audio announcement on module insertion ("Frontal Left connected", etc.).
 *
 * Usage:
 *   1. np_za_init()        — call once at firmware startup.
 *   2. np_za_tick()        — call from a 50 ms periodic task (all slots).
 *   3. np_za_audio_tick()  — call from DMA half-transfer ISR to refill buffer.
 *
 * Platform HAL stubs (np_za_platform_*) must be implemented by the FW team
 * for the target i.MX RT1062 peripherals (LPADC1, SAI3, eDMA channel 5).
 * See OI-ZA-01 through OI-ZA-04 in NP-FW-ZA-001 §10.
 */

#ifndef NP_ZONE_ANNOUNCE_H
#define NP_ZONE_ANNOUNCE_H

#include "np_zone_announce_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Lifecycle ──────────────────────────────────────────────────────────────── */

/*
 * Initialise the zone announce module.
 *
 * @param insert_cb         Called on confirmed insertion (twice: before and
 *                          after audio — see np_za_insert_cb_t).
 * @param remove_cb         Called on confirmed removal.
 * @param shdr_cb           Called on each auth event for SHDR partition write.
 * @param device_session_count  Current unsigned session counter from SHDR.
 * @return NP_ZA_OK or error code.
 */
np_za_status_t np_za_init(np_za_insert_cb_t insert_cb,
                           np_za_remove_cb_t remove_cb,
                           np_za_shdr_cb_t   shdr_cb,
                           uint32_t          device_session_count);

/*
 * Tear down audio hardware (call on system sleep or shutdown).
 */
void np_za_deinit(void);

/* ── Main tick (call from 50 ms periodic task) ──────────────────────────────── */

/*
 * Drive all five slot detection state machines.
 * @param now_ms  Current system time in milliseconds (from FreeRTOS tick or
 *                hardware timer).
 */
void np_za_tick(uint32_t now_ms);

/* ── Audio DMA ISR hook ──────────────────────────────────────────────────────── */

/*
 * Called from the eDMA half-transfer and transfer-complete ISRs.
 * Fills the next half of the DMA ping-pong buffer with synthesised samples.
 * @param half  0 = fill first half, 1 = fill second half.
 */
void np_za_audio_tick(uint8_t half);

/* ── Query ──────────────────────────────────────────────────────────────────── */

/*
 * Return the zone currently confirmed in the given physical slot (0-based).
 * Returns NP_ZONE_NONE if the slot is empty.
 */
np_zone_id_t np_za_get_zone(uint8_t slot_index);

/*
 * Return human-readable zone name (for debug logging / app display).
 * Always returns a valid null-terminated string.
 */
const char *np_za_zone_name(np_zone_id_t zone);

/* ── Platform HAL — implemented by the FW team (OI-ZA-01 through OI-ZA-04) ─── */

/*
 * OI-ZA-01: Trigger a single LPADC conversion on the ZONE_ID channel and
 * return the raw 12-bit result.  Must complete within 1 ms.
 * Returns false on timeout.
 */
bool np_za_platform_adc_read(uint8_t slot_index, uint16_t *out_counts);

/*
 * OI-ZA-02: Initialise SAI3 for 8 kHz, 16-bit, stereo I2S output routed to
 * the bone conduction amplifier.  Configure eDMA channel NP_ZA_SAI_DMA_CHANNEL
 * for circular ping-pong transfer from np_za_ctx.dma_buf.
 * Returns false on failure.
 */
bool np_za_platform_sai_init(int16_t *dma_buf, uint16_t buf_samples);

/*
 * OI-ZA-03: Start the SAI3 DMA transfer (enable FIFO, start eDMA).
 * Call after np_za_platform_sai_init() and buffer pre-fill.
 */
void np_za_platform_sai_start(void);

/*
 * OI-ZA-04: Stop SAI3 DMA transfer and tri-state the output to avoid DC bias
 * through the bone conduction piezo element.
 */
void np_za_platform_sai_stop(void);

/*
 * Return current system time in milliseconds.
 * Typically wraps xTaskGetTickCount() × portTICK_PERIOD_MS.
 */
uint32_t np_za_platform_now_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* NP_ZONE_ANNOUNCE_H */
