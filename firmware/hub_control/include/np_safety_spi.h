/*
 * NeuroPulse Hub Control Program — Safety MCU SPI Interface
 * Document: NP-FW-HUB-001 Rev A §7
 *
 * The STM32G071 safety MCU owns all stimulation GPIO enable lines.
 * The i.MX RT1062 main processor sends a heartbeat every 200ms over SPI.
 * If the safety MCU does not receive a heartbeat within 1500ms, it cuts
 * all stimulation.  This file provides the heartbeat task API and the
 * enable-request interface used by the session runner.
 *
 * HAL stub OI-SAF-01: np_safety_hal_spi_transfer(tx, rx, len)
 *   Full-duplex SPI3 transfer; chip-select managed by HAL.
 *   STM32G071 is SPI slave, i.MX RT1062 is SPI master.
 */

#ifndef NP_SAFETY_SPI_H
#define NP_SAFETY_SPI_H

#include "np_hub_types.h"

/* ── API ─────────────────────────────────────────────────────────────────────── */

/*
 * np_safety_spi_init — configure the SPI peripheral and drive GAIN_SEL[0..4]
 * LOW before the zone detection task starts (OI-PBM-HW-01 sequencing).
 * Returns NP_HUB_OK or NP_HUB_ERR_GENERIC on peripheral init failure.
 */
np_hub_status_t np_safety_spi_init(void);

/*
 * np_safety_spi_heartbeat — send one heartbeat frame and receive the safety
 * MCU reply.  Updates the internal granted-enable bitmask and status flags.
 * Called from the dedicated heartbeat task every NP_SAFETY_HEARTBEAT_MS.
 *
 * Returns NP_HUB_OK if the exchange completed and the MCU replied NP_SAFETY_STATUS_OK.
 * Returns NP_HUB_ERR_SAFETY_FAULT if the MCU reported a fault.
 * Returns NP_HUB_ERR_TIMEOUT on SPI timeout (safety MCU unresponsive).
 */
np_hub_status_t np_safety_spi_heartbeat(np_session_state_t session_state,
                                          uint16_t           requested_enable_mask);

/*
 * np_safety_spi_request_enable — request enable of stimulation channels.
 * The enable is only effective after the next heartbeat grants it.
 * Updates the requested_enable_mask used in subsequent heartbeat frames.
 */
void np_safety_spi_request_enable(uint16_t channel_mask);

/*
 * np_safety_spi_request_disable — clear bits from the requested_enable_mask.
 * Safety MCU will de-assert the corresponding GPIO lines on next heartbeat.
 */
void np_safety_spi_request_disable(uint16_t channel_mask);

/*
 * np_safety_spi_disable_all — atomically set requested_enable_mask to 0.
 * Used on session abort or fault.
 */
void np_safety_spi_disable_all(void);

/*
 * np_safety_spi_get_granted_mask — thread-safe read of the last-granted
 * enable bitmask (returned by the safety MCU in the most recent heartbeat).
 */
uint16_t np_safety_spi_get_granted_mask(void);

/*
 * np_safety_spi_get_status — return the safety MCU status byte from the
 * most recent heartbeat reply.
 */
uint8_t np_safety_spi_get_status(void);

/* ── HAL stub ─────────────────────────────────────────────────────────────────── */

/* OI-SAF-01: full-duplex SPI exchange; len = NP_SAFETY_FRAME_LEN bytes. */
extern np_hub_status_t np_safety_hal_spi_transfer(const uint8_t *tx,
                                                   uint8_t       *rx,
                                                   uint8_t        len);

#endif /* NP_SAFETY_SPI_H */
