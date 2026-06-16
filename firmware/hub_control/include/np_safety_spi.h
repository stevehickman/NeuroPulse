/*
 * NeuroPulse Hub Control Program — Safety MCU SPI Interface
 * Document: NP-FW-HUB-001 Rev A §7
 *
 * Two SPI frame types (hub is always SPI master):
 *
 *   Heartbeat (8 bytes, every 200ms):
 *     np_safety_spi_heartbeat() → np_safety_hal_spi_transfer(len=8)
 *
 *   Session signature command (102 bytes, once per session before enables):
 *     np_safety_spi_send_session_sig() → np_safety_hal_spi_transfer(len=102)
 *     Hub sends this BEFORE the heartbeat that first requests a non-zero
 *     enable mask.  Safety MCU reply during the 102-byte transfer is discarded
 *     by the hub; definitive result is NP_SAFETY_STATUS_SIG_PENDING bit in the
 *     subsequent heartbeat reply (cleared = verified, still set = rejected).
 *
 * HAL stub OI-SAF-01: np_safety_hal_spi_transfer(tx, rx, len)
 *   Full-duplex SPI3 transfer; chip-select managed by HAL.
 *   STM32G071 is SPI slave, i.MX RT1062 is SPI master.
 *   len = NP_SAFETY_FRAME_LEN (8) for heartbeat;
 *   len = NP_SAFETY_CMD_FRAME_LEN (102) for sig command.
 *
 * OI-SW01-M07-01 CLOSED.
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

/*
 * np_safety_spi_send_session_sig — deliver the session descriptor hash and
 * Ed25519 signature to the safety MCU via a 102-byte command frame.
 *
 * MUST be called after np_safety_spi_heartbeat() has been called with
 * session_state = NP_SESSION_RUNNING (so the safety MCU has seen the 0→1
 * session_active transition and set NP_SAFETY_STATUS_SIG_PENDING) and BEFORE
 * calling np_safety_spi_request_enable() with any non-zero channel mask.
 *
 * hash: 32-byte SHA-256 of the serialised session descriptor (signed portion).
 * sig:  64-byte Ed25519 signature from the app-signed protocol blob.
 *
 * Returns NP_HUB_OK if the SPI transfer completed successfully.
 * Returns NP_HUB_ERR_TIMEOUT on SPI transfer failure.
 *
 * The MCU's SIG_PENDING bit in the next heartbeat reply confirms acceptance.
 * If SIG_PENDING is still set after this call, the signature was rejected
 * (hub should abort the session and not request enables).
 */
np_hub_status_t np_safety_spi_send_session_sig(const uint8_t *hash,
                                                const uint8_t *sig);

/* ── HAL stub ─────────────────────────────────────────────────────────────────── */

/* OI-SAF-01: full-duplex SPI exchange.
 *   len = NP_SAFETY_FRAME_LEN (8)     → heartbeat exchange
 *   len = NP_SAFETY_CMD_FRAME_LEN (102) → session sig command
 * Hub is SPI master; chip-select managed by HAL.                             */
extern np_hub_status_t np_safety_hal_spi_transfer(const uint8_t *tx,
                                                   uint8_t       *rx,
                                                   uint8_t        len);

#endif /* NP_SAFETY_SPI_H */
