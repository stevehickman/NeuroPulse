/*
 * NeuroPulse Safety MCU — SW01-M07: Session Descriptor Signature Gate
 * Document: NP-SW-001 Rev A, NP-FMEA-001 Rev A §SW01-M07
 *
 * Verifies the Ed25519 signature on the session descriptor before allowing
 * stimulation enable.  The main processor sends the 32-byte session hash
 * (SHA-256 of the serialised session descriptor) and the 64-byte Ed25519
 * signature via SPI before requesting enables.
 *
 * Ed25519 is provided by firmware/crypto (np_crypto static library, backed
 * by Monocypher 4.0.2 — RFC 8032 §5.1.7, SHA-512).  OI-SW01-M07-02 CLOSED.
 *
 * REMAINING OPEN ITEM — OI-SW01-M07-01: np_session_sig_is_verified() is
 * never consulted by np_spi_watchdog_tick().  The grant path must be updated
 * to gate on np_session_sig_is_verified() once the SPI command delivery frame
 * (96 bytes: 32-byte hash + 64-byte sig) is designed.  BLOCKING for production.
 *
 * OTP HAL stub: np_hal_otp_read_pubkey(buf, len) reads the 32-byte key.
 */

#include "np_crypto.h"
#include "np_safety_config.h"
#include "np_safety_protocol.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ── HAL stubs ───────────────────────────────────────────────────────────── */
extern void np_hal_otp_read_pubkey(uint8_t *buf, uint8_t len);

/* ── Module state ─────────────────────────────────────────────────────────── */
static uint8_t s_pubkey[NP_ED25519_PUB_KEY_LEN];
static bool    s_pubkey_loaded;
static bool    s_session_verified;

np_safe_status_t np_session_sig_init(void)
{
    np_hal_otp_read_pubkey(s_pubkey, NP_ED25519_PUB_KEY_LEN);
    s_pubkey_loaded    = true;
    s_session_verified = false;
    return NP_SAFE_OK;
}

/*
 * np_session_sig_reset — called when session_active transitions 0→1.
 * Requires the main processor to supply and verify a new session hash + sig
 * before enables are granted.
 */
void np_session_sig_reset(void)
{
    s_session_verified = false;
}

/*
 * np_session_sig_verify — called when the main processor supplies the 32-byte
 * session hash and 64-byte signature via a dedicated SPI command frame.
 *
 * Returns NP_SAFE_OK if verification passes; NP_SAFE_ERR_FAULT otherwise.
 * Sets state->status NP_SAFETY_STATUS_FAULT on failure.
 */
np_safe_status_t np_session_sig_verify(np_safety_state_t *state,
                                        const uint8_t *hash,
                                        const uint8_t *sig)
{
    if (!s_pubkey_loaded) {
        state->status |= NP_SAFETY_STATUS_FAULT;
        return NP_SAFE_ERR_FAULT;
    }

    int ok = np_ed25519_verify(s_pubkey,
                               hash, NP_SESSION_HASH_LEN,
                               sig);
    if (ok != 0) {
        state->status       |= NP_SAFETY_STATUS_FAULT | NP_SAFETY_STATUS_CUTOFF;
        state->granted_mask  = 0U;
        state->fault_slot    = 0xFDU;  /* 0xFD = signature failure */
        return NP_SAFE_ERR_FAULT;
    }

    s_session_verified = true;
    return NP_SAFE_OK;
}

bool np_session_sig_is_verified(void)
{
    return s_session_verified;
}
