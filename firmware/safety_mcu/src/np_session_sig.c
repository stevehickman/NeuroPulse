/*
 * NeuroPulse Safety MCU — SW01-M07: Session Descriptor Signature Gate
 * Document: NP-SW-001 Rev A, NP-FMEA-001 Rev A §SW01-M07
 *
 * Verifies the Ed25519 signature on the session descriptor before allowing
 * stimulation enable.  The main processor sends the 32-byte session hash
 * (SHA-256 of the serialised session descriptor) and the 64-byte Ed25519
 * signature via SPI before requesting enables.
 *
 * The verification uses a self-contained Ed25519 implementation (same
 * approach as the bootloader's np_signature.c).  The manufacturing root
 * public key is burned into OTP at device provisioning.
 *
 * OTP HAL stub: np_hal_otp_read_pubkey(buf, len) reads the 32-byte key.
 */

#include "np_safety_config.h"
#include "np_safety_protocol.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ── HAL stubs ───────────────────────────────────────────────────────────── */
extern void np_hal_otp_read_pubkey(uint8_t *buf, uint8_t len);

/* Ed25519 verify implementation.
 * OI-SW01-M07-02: This symbol must be provided by the safety MCU build.
 * The bootloader has a self-contained RFC 8032 §5.1.7 implementation in
 * firmware/bootloader/src/np_signature.c, but that file is NOT in the
 * safety MCU build (SAFETY_SOURCES in CMakeLists.txt).  The symbol must be
 * either:
 *   (a) copied / refactored into a shared crypto library linked by both
 *       the bootloader and the safety MCU, or
 *   (b) reimplemented here as a safety-MCU-specific module.
 * Until this is resolved the ARM build will fail to link.
 *
 * ALSO NOTE — OI-SW01-M07-01: even when np_ed25519_verify is resolved,
 * np_session_sig_is_verified() is never consulted by np_spi_watchdog_tick().
 * The granted_mask is set without any signature check.  See np_spi_watchdog.c
 * and np_safety_main.c — the grant path must be updated to gate on
 * np_session_sig_is_verified() once the SPI command delivery frame is designed.
 *
 * Both OI-SW01-M07-01 and OI-SW01-M07-02 are BLOCKING for production builds. */
extern int np_ed25519_verify(const uint8_t *pubkey,
                              const uint8_t *msg,   uint32_t msg_len,
                              const uint8_t *sig);

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
