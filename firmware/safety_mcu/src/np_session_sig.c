/*
 * NeurOne Safety MCU — SW01-M07: Session Descriptor Signature Gate
 * Document: NP-SW-001 Rev 1, NP-FMEA-001 Rev 1 §SW01-M07
 *
 * Verifies the Ed25519 signature on the session descriptor before allowing
 * stimulation enable.  The main processor sends the 32-byte session hash
 * (SHA-256 of the serialized session descriptor) and the 64-byte Ed25519
 * signature via SPI before requesting enables.
 *
 * Ed25519 is provided by firmware/crypto (np_crypto static library, backed
 * by Monocypher 4.0.2 — RFC 8032 §5.1.7, SHA-512).  OI-SW01-M07-02 CLOSED.
 *
 * OI-SW01-M07-01 CLOSED: np_safety_sig_cmd_t command frame (102 bytes) delivers
 * hash + sig from hub to safety MCU.  np_session_sig_reset() sets
 * NP_SAFETY_STATUS_SIG_PENDING; verify clears it; watchdog tick blocks
 * granted_mask while SIG_PENDING is set.  See np_safety_protocol.h.
 *
 * OTP HAL stub: np_hal_otp_read_pubkey(buf, len) reads the 32-byte key.
 */

#include "np_crypto.h"
#include "np_safety_config.h"
#include "np_safety_hal.h"
#include "np_safety_protocol.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* HAL: np_hal_otp_read_pubkey — declared in np_safety_hal.h, where the
 * all-zero-means-unprovisioned sentinel is recorded.                        */

/* ── Module state ─────────────────────────────────────────────────────────── */
static uint8_t  s_pubkey[NP_ED25519_PUB_KEY_LEN];
static bool     s_pubkey_loaded;
static bool     s_session_verified;
static uint8_t  s_sig_fail_count;  /* consecutive Ed25519 failures this power-cycle */

np_safe_status_t np_session_sig_init(void)
{
    uint8_t i;
    bool    all_zero;

    np_hal_otp_read_pubkey(s_pubkey, NP_ED25519_PUB_KEY_LEN);

    /* All-zero public key means the OTP was never programmed (unprovisioned device).
     * Do not set s_pubkey_loaded; np_session_sig_verify() will fault with
     * NP_FAULT_SLOT_UNPROV on the first session attempt.                      */
    all_zero = true;
    for (i = 0U; i < NP_ED25519_PUB_KEY_LEN; i++) {
        if (s_pubkey[i] != 0U) { all_zero = false; break; }
    }
    if (!all_zero) {
        s_pubkey_loaded = true;
    }

    s_session_verified = false;
    s_sig_fail_count   = 0U;
    return NP_SAFE_OK;
}

/*
 * np_session_sig_reset — called when session_active transitions 0→1.
 * Marks SIG_PENDING in state so that np_spi_watchdog_tick() blocks the
 * granted_mask until the hub delivers the session descriptor signature
 * via a np_safety_sig_cmd_t command frame.
 */
void np_session_sig_reset(np_safety_state_t *state)
{
    s_session_verified  = false;
    state->status      |= NP_SAFETY_STATUS_SIG_PENDING;
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
        state->fault_slot = NP_FAULT_SLOT_UNPROV;  /* OTP never programmed */
        state->status    |= NP_SAFETY_STATUS_FAULT;
        return NP_SAFE_ERR_FAULT;
    }

    int ok = np_ed25519_verify(s_pubkey,
                               hash, NP_SESSION_HASH_LEN,
                               sig);
    if (ok != 0) {
        s_sig_fail_count++;
        state->status       |= NP_SAFETY_STATUS_FAULT | NP_SAFETY_STATUS_CUTOFF;
        state->granted_mask  = 0U;
        state->fault_slot    = NP_FAULT_SLOT_SIG_FAIL;
        /* Leave SIG_PENDING set — hub must not receive enables. */
        return NP_SAFE_ERR_FAULT;
    }

    s_sig_fail_count    = 0U;  /* reset on success */
    s_session_verified  = true;
    state->status      &= (uint8_t)~NP_SAFETY_STATUS_SIG_PENDING;
    return NP_SAFE_OK;
}

bool np_session_sig_is_verified(void)
{
    return s_session_verified;
}

/*
 * np_session_sig_reenable — called by np_safety_main BEFORE np_session_sig_reset()
 * on a session 0→1 transition when the previous session ended with a sig fault.
 *
 * Clears the FAULT+CUTOFF bits if the fail count is still below NP_SIG_FAIL_MAX.
 * After NP_SIG_FAIL_MAX consecutive failures the device is hard-locked — only
 * a power-cycle can clear s_sig_fail_count and re-enable the reset path.
 *
 * The caller must still call np_session_sig_reset() afterward to set SIG_PENDING
 * for the new session.
 */
void np_session_sig_reenable(np_safety_state_t *state)
{
    if (state->fault_slot != NP_FAULT_SLOT_SIG_FAIL) {
        return;  /* not a sig fault — don't touch other fault bits */
    }
    if (s_sig_fail_count >= NP_SIG_FAIL_MAX) {
        return;  /* hard lock — require device power-cycle */
    }
    state->status    &= (uint8_t)~(NP_SAFETY_STATUS_FAULT | NP_SAFETY_STATUS_CUTOFF);
    state->fault_slot = NP_FAULT_SLOT_NONE;
    /* np_session_sig_reset() will set SIG_PENDING for the new session */
}
