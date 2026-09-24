/*
 * NeurOne Safety MCU — SW01-M10: Tier Identity Gate
 * Document: NP-REG-UPG-001 Rev 3 §7.5 (REQ-UPG-01, REQ-UPG-02), OI-UPG-01;
 *           NP-PWRSRC-001 D-9 (device-bound signed entitlement), RISK-PWRSRC-10
 *
 * WHAT THIS MODULE IS FOR
 * ───────────────────────
 * A T1 unit is a wellness unit for its whole life (CLAUDE.md §1).  For most T2
 * parts that is a hardware fact — the part is simply not there.  It is NOT a
 * hardware fact for three: the T2-D 1170 nm tile fits any socket, the A14
 * cervical VNS accessory plugs into the hub accessory port every T1 hub has,
 * and the qEEG cap may reach the PAN (NP-REG-UPG-001 §3.3).  Physical absence
 * is therefore not the gate, and before this module nothing was: the only tier
 * signal was NP_PROTO_FLAG_T2_TIER, which the app computes and no firmware read.
 *
 * So the gate lives HERE, in the partition that owns the enable lines
 * (CLAUDE.md §4.2), keyed to a signed tier identity the app cannot write:
 *
 *   init   Read the 72-byte record from OTP (written once, at manufacture —
 *          REQ-UPG-02) and the part's factory UID.  Verify the Ed25519
 *          signature by the tier authority over domain || record body || UID.
 *          Only a record that verifies, for THIS device, naming T2, makes the
 *          unit T2.  Everything else — no authority key in the image, a blank
 *          window, a malformed record, a bad signature, a record copied from
 *          another unit, a validly signed T1 record — makes it T1.  Fail closed.
 *
 *   gate   Every main-loop iteration: on a unit that is not T2, strip
 *          NP_SAFETY_EN_T2_MASK from granted_mask.  Runs after every interlock
 *          that could grant and before charge accumulation and the GPIO write,
 *          so a withheld line is never energised and never accrues charge.
 *
 *   report Tell the hub the tier, why, and whether a T2 enable was withheld on
 *          this beat — so the refusal can be presented as F4 (NP-PWRSRC-001
 *          §6.3), never as a missing module.
 *
 * WHAT IT IS NOT
 * ──────────────
 * Not a fault.  A T1 unit asking for a T2 line gets that line withheld and
 * every T1 line it asked for still granted — the same shape as the cardiac
 * interlock withholding only cervical VNS.  Faulting would take a T1 owner's
 * whole session away for a request no T1 hardware can serve.
 *
 * Not re-read.  OTP is immutable and the verdict is taken once at power-on.
 * The one thing that CAN change at run time is RAM, so the verdict is held as a
 * pattern word and its complement: a flipped bit in either reads as "not T2".
 *
 * WHY THIS WIDENS CLASS C (NP-REG-UPG-001 OI-UPG-01, cost (i))
 * ───────────────────────────────────────────────────────────────
 * A commercial distinction now sits in the IEC 62304 Class C partition.  It is
 * here because REQ-UPG-01 requires the refusal to be made where the enables are
 * owned, and D-9 already accepted that scope for F4.  The module is kept small
 * and stateless after init so it adds as little Class C surface as it can.
 *
 * IEC 62304 Class C — MISRA C:2012.  C11, no GNU extensions.
 */

#include "np_crypto.h"
#include "np_safety_config.h"
#include "np_safety_hal.h"
#include "np_safety_protocol.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ── Verdict encoding ─────────────────────────────────────────────────────────
 * Not a bool.  A single-bit flip turns `false` into `true`; it cannot turn a
 * pattern word AND its independently stored complement into the T2 pair.     */
#define NP_TIER_T2_WORD      0x5AC3E12DUL

static const uint8_t k_authority[NP_ED25519_PUB_KEY_LEN] =
    NP_TIER_AUTHORITY_PUBKEY_INIT;

static uint32_t s_t2_word;
static uint32_t s_t2_word_inv;
static uint8_t  s_reason;
static uint8_t  s_record_tier;   /* tier code the record named (diagnostic only) */

static bool all_bytes_are(const uint8_t *b, uint8_t len, uint8_t v)
{
    uint8_t i;
    for (i = 0U; i < len; i++) {
        if (b[i] != v) {
            return false;
        }
    }
    return true;
}

static void set_verdict(bool is_t2, uint8_t reason, uint8_t record_tier)
{
    s_t2_word     = is_t2 ? NP_TIER_T2_WORD : 0UL;
    s_t2_word_inv = ~s_t2_word;
    s_reason      = reason;
    s_record_tier = record_tier;
}

/*
 * np_tier_identity_init — establish the unit's tier once, at power-on.
 *
 * The order of the checks is the order of the diagnoses, most basic first, so
 * the reason reported is the FIRST thing wrong: an image with no authority key
 * reports that even on a unit whose record is also blank.
 */
np_safe_status_t np_tier_identity_init(void)
{
    uint8_t rec[NP_TIER_RECORD_LEN];
    uint8_t uid[NP_DEVICE_UID_LEN];
    uint8_t msg[NP_TIER_SIG_MSG_LEN];
    uint8_t tier;

    set_verdict(false, NP_TIER_REASON_NO_AUTHORITY, NP_TIER_T1);

    if (all_bytes_are(k_authority, (uint8_t)NP_ED25519_PUB_KEY_LEN, 0U)) {
        return NP_SAFE_OK;              /* T1: no key can verify anything */
    }

    np_hal_otp_read_tier_record(rec, (uint8_t)NP_TIER_RECORD_LEN);

    if (all_bytes_are(rec, (uint8_t)NP_TIER_RECORD_LEN, 0xFFU)) {
        set_verdict(false, NP_TIER_REASON_BLANK, NP_TIER_T1);
        return NP_SAFE_OK;              /* T1: identity never written */
    }

    tier = rec[5];
    if (rec[0] != NP_TIER_RECORD_MAGIC_0 || rec[1] != NP_TIER_RECORD_MAGIC_1 ||
        rec[2] != NP_TIER_RECORD_MAGIC_2 || rec[3] != NP_TIER_RECORD_MAGIC_3 ||
        rec[4] != NP_TIER_RECORD_VERSION ||
        rec[6] != 0U || rec[7] != 0U ||
        (tier != NP_TIER_T1 && tier != NP_TIER_T2)) {
        set_verdict(false, NP_TIER_REASON_FORMAT, NP_TIER_T1);
        return NP_SAFE_OK;              /* T1: not a record this image knows */
    }

    np_hal_read_device_uid(uid, (uint8_t)NP_DEVICE_UID_LEN);

    (void)memcpy(msg, NP_TIER_SIG_DOMAIN, NP_TIER_SIG_DOMAIN_LEN);
    (void)memcpy(&msg[NP_TIER_SIG_DOMAIN_LEN], rec, NP_TIER_RECORD_BODY_LEN);
    (void)memcpy(&msg[NP_TIER_SIG_DOMAIN_LEN + NP_TIER_RECORD_BODY_LEN],
                 uid, NP_DEVICE_UID_LEN);

    if (np_ed25519_verify(k_authority, msg, (uint32_t)NP_TIER_SIG_MSG_LEN,
                          &rec[NP_TIER_RECORD_BODY_LEN]) != 0) {
        /* Includes a genuine record from ANOTHER unit: its UID is not ours. */
        set_verdict(false, NP_TIER_REASON_SIGNATURE, NP_TIER_T1);
        return NP_SAFE_OK;
    }

    set_verdict(tier == NP_TIER_T2, NP_TIER_REASON_OK, tier);
    return NP_SAFE_OK;
}

/* True only for the intact T2 pattern pair. */
bool np_tier_identity_is_t2(void)
{
    return (s_t2_word == NP_TIER_T2_WORD) &&
           (s_t2_word_inv == (uint32_t)~NP_TIER_T2_WORD);
}

/*
 * np_tier_identity_gate — withhold every T2 enable line on a unit that is not
 * T2.  Called every main-loop iteration, after np_spi_watchdog_tick() (the only
 * place a grant is made) and before charge accumulation and np_gpio_mgr_apply().
 * Nothing after it adds a bit to granted_mask; the charge tick only removes.
 */
void np_tier_identity_gate(np_safety_state_t *state)
{
    if (!np_tier_identity_is_t2()) {
        state->granted_mask &= (uint16_t)~(uint16_t)NP_SAFETY_EN_T2_MASK;
    }
}

/*
 * np_tier_identity_build_report — the MISO tier report for this beat.
 * `requested_mask` is this beat's request; REFUSED is set when it asks for a
 * T2 line the gate withholds.  Fixed shape: every field is populated on every
 * beat, whatever the verdict.
 */
void np_tier_identity_build_report(np_safety_tier_report_t *out,
                                   uint16_t                 requested_mask)
{
    const bool is_t2 = np_tier_identity_is_t2();

    if (out == NULL) {
        return;
    }
    out->magic  = NP_SAFETY_TIER_REPORT_MAGIC;
    out->tier   = is_t2 ? NP_TIER_T2 : NP_TIER_T1;
    out->flags  = (!is_t2 &&
                   ((requested_mask & (uint16_t)NP_SAFETY_EN_T2_MASK) != 0U))
                      ? (uint8_t)NP_SAFETY_TIER_FLAG_REFUSED : 0U;
    out->reason = s_reason;
    out->checksum = (uint16_t)((uint16_t)out->magic + (uint16_t)out->tier +
                               (uint16_t)out->flags + (uint16_t)out->reason);
}

/* Diagnostic accessors — final acceptance and the host test. */
uint8_t np_tier_identity_reason(void)      { return s_reason; }
uint8_t np_tier_identity_record_tier(void) { return s_record_tier; }
