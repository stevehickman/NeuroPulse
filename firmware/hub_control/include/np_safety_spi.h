/*
 * NeurOne Hub Control Program — Safety MCU SPI Interface
 * Document: NP-FW-HUB-001 Rev A §7
 *
 * Two SPI frame types (hub is always SPI master):
 *
 *   Extended heartbeat (38 bytes, every 200ms):
 *     np_safety_spi_heartbeat() → np_safety_hal_spi_transfer(len=38)
 *     Hub sends np_safety_tx_ext_frame_t (magic + enables + channel_count +
 *     checksum + current_ua[14] + ext_checksum); MCU simultaneously sends back
 *     the 8-byte MCU reply frame; hub reads the first 8 bytes and discards the
 *     rest (30 SPI-slave zero bytes).
 *     OI-CHARGE-01 CLOSED — frame carries current_ua[14] enabling charge monitor.
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
 *   len = NP_SAFETY_RX_EXT_FRAME_LEN (38) for heartbeat;
 *   len = NP_SAFETY_CMD_FRAME_LEN (102) for sig command.
 *
 * OI-SW01-M07-01 CLOSED.
 * OI-CHARGE-01   CLOSED.
 */

#ifndef NP_SAFETY_SPI_H
#define NP_SAFETY_SPI_H

#include "np_hub_types.h"

/* ── session_status byte construction ────────────────────────────────────────── */

/*
 * np_safety_session_status_bits — map the hub session state plus per-frame
 * flags into the heartbeat session_status BIT FLAGS the safety MCU parses.
 *
 * NEVER write an np_session_state_t enum value into session_status directly:
 * NP_SESSION_RUNNING (=3) would assert ACTIVE|CVNS_REENABLE and
 * NP_SESSION_PAUSED (=4) would assert GEOM_REQUIRED.  (This was a live bug —
 * fixed with OI-CVNS-HUB-01.)
 *
 * ACTIVE maps from RUNNING, PAUSED, and STOPPING: the MCU resets per-session
 * state (session signature, charge accumulator, impedance checks) on the
 * ACTIVE 0→1 transition, so PAUSED must remain "active" or a pause/resume
 * cycle would zero the charge accumulator mid-session (charge-limit
 * circumvention).  STOPPING remains active through ramp-down; ACTIVE drops
 * when the runner reaches COMPLETE/FAULT/IDLE.
 *
 * Pure and header-inline so it is host-testable without FreeRTOS.
 */
static inline uint8_t np_safety_session_status_bits(np_session_state_t state,
                                                    bool geom_required,
                                                    bool cvns_reenable)
{
    uint8_t bits = 0U;
    if (state == NP_SESSION_RUNNING ||
        state == NP_SESSION_PAUSED  ||
        state == NP_SESSION_STOPPING) {
        bits |= (uint8_t)NP_SESSION_STATUS_ACTIVE;
    }
    if (cvns_reenable) {
        bits |= (uint8_t)NP_SESSION_STATUS_CVNS_REENABLE;
    }
    if (geom_required) {
        bits |= (uint8_t)NP_SESSION_STATUS_GEOM_REQUIRED;
    }
    return bits;
}

/* ── API ─────────────────────────────────────────────────────────────────────── */

/*
 * np_safety_spi_init — configure the SPI peripheral and drive GAIN_SEL[0..4]
 * LOW before the zone detection task starts (OI-PBM-HW-01 sequencing).
 * Returns NP_HUB_OK or NP_HUB_ERR_GENERIC on peripheral init failure.
 */
np_hub_status_t np_safety_spi_init(void);

/*
 * np_safety_spi_heartbeat — send one 38-byte extended heartbeat frame and
 * receive the 8-byte safety MCU reply.  Updates the internal granted-enable
 * bitmask and status flags.
 * Called from the dedicated heartbeat task every NP_SAFETY_HEARTBEAT_MS.
 *
 * session_state:         current session state (NP_SESSION_IDLE / NP_SESSION_RUNNING).
 * requested_enable_mask: bitmask of channels the session runner wants enabled.
 * current_ua:            array of commanded current magnitudes in µA per channel
 *                        (SHDR — commanded from session descriptor, NOT ADC-measured).
 *                        Must point to at least channel_count entries; may be NULL
 *                        when channel_count == 0 (e.g. no active session).
 * channel_count:         number of valid entries in current_ua (0–NP_SAFETY_MAX_CHANNELS).
 *
 * Returns NP_HUB_OK if the exchange completed and the MCU replied NP_SAFETY_STATUS_OK.
 * Returns NP_HUB_ERR_SAFETY_FAULT if the MCU reported a fault.
 * Returns NP_HUB_ERR_TIMEOUT on SPI timeout (safety MCU unresponsive).
 */
np_hub_status_t np_safety_spi_heartbeat(np_session_state_t  session_state,
                                          uint16_t            requested_enable_mask,
                                          const uint16_t     *current_ua,
                                          uint8_t             channel_count);

/*
 * np_safety_spi_set_geom_required — declare (or clear) that the running session
 * needs an electrode-geometry override (OI-CHARGE-03 fail-safe gate).
 *
 * While set, every heartbeat carries NP_SESSION_STATUS_GEOM_REQUIRED so the
 * safety MCU keeps CLIN_STIM (HD-tDCS) out of granted_mask until it has applied
 * a valid electrode-area command (np_safety_spi_send_channel_limits()).  This
 * makes a lost/delayed area command fail-CLOSED instead of fail-open.
 *
 * The session runner sets this true (before the first CLIN_STIM enable request)
 * for HD-tDCS ring/bilateral montages, and it is cleared on session end/abort
 * (also cleared by np_safety_spi_disable_all()).
 */
void np_safety_spi_set_geom_required(bool required);

/*
 * np_safety_spi_set_cvns_reenable — assert (or clear) the CVNS re-enable bit
 * carried in every heartbeat (OI-CVNS-HUB-01).
 *
 * The heartbeat task mirrors np_cvns_reenable_bit_active() into this flag once
 * per beat — the np_cvns_reenable state machine is the ONLY legitimate source.
 * Cleared by np_safety_spi_disable_all() on session abort/end (fail-safe).
 */
void np_safety_spi_set_cvns_reenable(bool active);

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
 * np_safety_spi_get_requested_mask — read the currently-requested enable
 * bitmask (accumulated via request_enable/request_disable).  The heartbeat
 * task transmits THIS mask every beat — transmitting the granted-mask echo
 * would mean new enable requests never reach the safety MCU.
 */
uint16_t np_safety_spi_get_requested_mask(void);

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
 * hash: 32-byte SHA-256 of the serialized session descriptor (signed portion).
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

/*
 * np_safety_spi_send_channel_limits — deliver per-channel electrode geometry to
 * the safety MCU via a 34-byte command frame (OI-CHARGE-02).
 *
 * The safety MCU converts each area into a per-electrode charge limit using its
 * resident 40µC/cm² density constant.  This function transmits electrode AREA
 * only — never a pre-computed limit — so the density constant stays on the
 * Class C safety MCU.
 *
 * area_mcm2: array of per-channel electrode areas in milli-cm² (1 unit =
 *            0.001 cm²).  Entry n applies to safety channel n (= NP_SAFETY_EN_*
 *            bit position).  A value of 0 leaves that channel's limit at its
 *            session-reset default (25cm² pad → 1000µC).  Areas should be
 *            floored (truncated) so the derived limit never exceeds 40µC/cm².
 * count:     number of valid entries (1–NP_SAFETY_MAX_CHANNELS).
 *
 * MUST be called during session setup, BEFORE requesting enable of any
 * small-electrode channel (notably NP_SAFETY_EN_CLIN_STIM for HD-tDCS).
 *
 * Returns NP_HUB_OK if the SPI transfer completed; NP_HUB_ERR_TIMEOUT on
 * transfer failure; NP_HUB_ERR_INVALID_ARG on a NULL/zero/oversized count.
 */
np_hub_status_t np_safety_spi_send_channel_limits(const uint16_t *area_mcm2,
                                                   uint8_t         count);

/* ── HAL stub ─────────────────────────────────────────────────────────────────── */

/* OI-SAF-01: full-duplex SPI exchange.
 *   len = NP_SAFETY_RX_EXT_FRAME_LEN (38) → extended heartbeat exchange
 *   len = NP_SAFETY_CMD_FRAME_LEN (102)   → session sig command
 * Hub is SPI master; chip-select managed by HAL.                             */
extern np_hub_status_t np_safety_hal_spi_transfer(const uint8_t *tx,
                                                   uint8_t       *rx,
                                                   uint8_t        len);

#endif /* NP_SAFETY_SPI_H */
