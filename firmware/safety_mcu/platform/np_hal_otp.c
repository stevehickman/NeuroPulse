/*
 * NeurOne Safety MCU — SW-01 Platform Layer: OTP root public key
 * Target: STM32G071 (Cortex-M0+, 64 MHz), bare metal, CMSIS registers only
 * Document: NP-SW-001 Rev A — SW-01 Class C; NP-SW-CI-001 §4.4 (OI-SWCI-17)
 *           CLAUDE.md §4.2 (session protocol cryptographically signed)
 *
 * Implements np_hal_otp_read_pubkey(): reads the manufacturing root Ed25519
 * public key out of one-time-programmable memory for np_session_sig_init().
 *
 * ── THE ERASED-STATE TRANSLATION — the reason this file is not a memcpy ──────
 * np_safety_hal.h fixes the contract: "An ALL-ZERO key is the agreed sentinel
 * for 'OTP was never programmed' (unprovisioned device)", and np_session_sig.c
 * faults with NP_FAULT_SLOT_UNPROV on that sentinel.
 *
 * Erased STM32 OTP does not read as zero.  It reads as 0xFF — flash's erased
 * state is all-ones on this part as on every other NOR flash.  A driver that
 * copied the OTP window verbatim would hand np_session_sig_init() 32 bytes of
 * 0xFF on a factory-fresh device, which is not the agreed sentinel, so the
 * unprovisioned check would not fire.  The device would instead carry a
 * syntactically valid but meaningless public key and fail Ed25519 verification
 * at session start — surfacing as NP_FAULT_SLOT_SIG_FAIL ("this session's
 * signature is bad", escalating to a hard lock after NP_SIG_FAIL_MAX) instead
 * of NP_FAULT_SLOT_UNPROV ("this device was never provisioned").
 *
 * Both are faults and both refuse stimulation, so this is not a safety hole —
 * it is a diagnosis hole, and an expensive one: an unprovisioned unit returned
 * from the line would present as a crypto failure rather than a missing
 * manufacturing step.  So the driver TRANSLATES: an all-0xFF window is
 * reported as the all-zero sentinel the contract names.
 *
 * Only the fully-erased pattern is translated.  A partially-programmed window
 * is passed through unchanged and will fail verification, which is correct: a
 * half-written key is a corrupt device, not an unprovisioned one, and
 * flattening it to the unprovisioned sentinel would hide that difference.
 *
 * ── FAILURE SIGNALLING (np_safety_hal.h open question 8) ─────────────────────
 * The function returns void, so a genuine read error is indistinguishable from
 * an unprogrammed device — both surface as the all-zero sentinel.  That remains
 * true and is not fixable without changing the signature, which phase 7 must
 * not do (it would drift the test doubles).  It is mitigated by the fact that
 * OTP on this part is memory-mapped: there is no bus transaction to fail, no
 * status register to poll, and no error path distinct from "the bytes read back
 * are not a key".  Recorded rather than silently accepted.
 *
 * IEC 62304 Class C — MISRA C:2012.  C11, no GNU extensions.
 */

#include "np_hal_internal.h"

/*
 * STM32G071 OTP area: 1 KB, memory-mapped, read like any flash address.
 *
 * The key is placed at the BASE of the OTP window by the manufacturing step.
 * That placement is a production-programming contract, not a silicon fact, so
 * it is named here rather than buried in a cast, and it is the one number in
 * this file a provisioning change would have to update.  Recorded as OI-SWCI-30
 * so the manufacturing flow and this constant are reviewed together.
 */
/* #ifndef so np_hal_fake_regs.h can retarget the window at a normal array for
 * the host test — a process cannot map 0x1FFF7000, and the erased-state
 * translation above is exactly the logic that most needs a test.  The guard is
 * inert in the cross build, where nothing defines it. */
#ifndef NP_HAL_OTP_BASE
#define NP_HAL_OTP_BASE        0x1FFF7000UL
#endif
#define NP_HAL_OTP_SIZE        1024U
#define NP_HAL_OTP_PUBKEY_OFF  0U

/* Erased NOR flash reads all-ones. */
#define NP_HAL_OTP_ERASED_BYTE  0xFFU

_Static_assert(NP_ED25519_PUB_KEY_LEN <= NP_HAL_OTP_SIZE,
               "Ed25519 public key does not fit in the OTP window");

void np_hal_otp_read_pubkey(uint8_t *buf, uint8_t len)
{
    const volatile uint8_t *otp =
        (const volatile uint8_t *)(NP_HAL_OTP_BASE + NP_HAL_OTP_PUBKEY_OFF);
    uint8_t n;
    uint8_t i;
    bool    erased;

    if (buf == NULL) {
        return;
    }

    /* Bound the copy to the contracted key length regardless of what the caller
     * passes.  np_safety_hal.h says len "is always NP_ED25519_PUB_KEY_LEN (32);
     * the single caller passes it" — but "always" is a statement about today's
     * one call site, and s_pubkey in np_session_sig.c is exactly 32 bytes.  A
     * larger len would overrun that buffer, which on this part sits in 36 KB of
     * SRAM alongside the safety state.  Clamping costs one comparison. */
    n = (len < (uint8_t)NP_ED25519_PUB_KEY_LEN) ? len
                                                : (uint8_t)NP_ED25519_PUB_KEY_LEN;

    erased = true;
    for (i = 0U; i < n; i++) {
        uint8_t b = otp[i];
        if (b != NP_HAL_OTP_ERASED_BYTE) {
            erased = false;
        }
        buf[i] = b;
    }

    /* Fully-erased window → emit the agreed all-zero unprovisioned sentinel.
     * See the file header for why a verbatim copy would be a diagnosis defect. */
    if (erased) {
        for (i = 0U; i < n; i++) {
            buf[i] = 0U;
        }
    }
}
