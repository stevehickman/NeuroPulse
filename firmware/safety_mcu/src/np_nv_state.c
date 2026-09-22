/*
 * NeurOne Safety MCU — SW01-M09: Non-Volatile Safety State
 * Document: NP-SW-FAULTMSG-001 P1 (OI-FAULTMSG-01); NP-FW-CVNS-001 §5.4;
 *           NP-HW-CVNS-001 REQ-CVNS-09
 *
 * WHY THIS MODULE EXISTS
 * ----------------------
 * After a cervical VNS cardiac cutoff, re-enable needs the 30 s lockout, app
 * confirmation and a repeat impedance check.  That state used to live only in
 * RAM (np_cardiac_interlock.c), and the fault latch survives only a warm reset.
 * The headset has no battery (CLAUDE.md §4.5), so unplugging the power bank was
 * a power-on reset that erased the cutoff: in Mode 3 a new session could then
 * enable cervical stimulation with no app confirmation at all.  This module
 * keeps one bit — "a cutoff is awaiting acknowledgement" — in flash.
 *
 * RECORD FORMAT
 * -------------
 * Append-only log of 64-bit records across two 2 KB pages (NP_NV_*,
 * np_safety_config.h).  Each record:
 *   lo = NP_NV_MAGIC << 16 | seq    (seq: 16-bit, +1 per record)
 *   hi = NP_NV_VAL_SET | NP_NV_VAL_CLR
 * Erased flash reads 0xFFFFFFFF/0xFFFFFFFF.  Within a page, slot order is
 * write order; across pages, the higher seq is newer.
 *
 * FAIL-CLOSED DECODING
 * --------------------
 *   - Never-written storage (both pages erased) → not pending.  A factory-fresh
 *     unit must not be locked out.
 *   - The newest record of EITHER page is not a valid record (a write torn by
 *     power loss) → pending.  A torn write may have been a SET.
 *   - Otherwise → the value of the valid record with the highest seq.
 * A torn record that is NOT the newest in its page is ignored: a valid record
 * written after it supersedes it.
 *
 * WRITE ORDER — why a power loss can never lose a SET
 * ----------------------------------------------------
 *   1. If the page NOT holding the newest valid record has any content (a torn
 *      rotation, a stale residue), erase it.  Afterwards only one page is live.
 *   2. Append to the live page.  If it is full, write slot 0 of the (now empty)
 *      other page, and only then erase the full page.
 * A SET is never preceded by erasing the page that holds the latest record, so
 * there is no instant at which the newest persisted state is gone.
 *
 * WHAT THIS DOES NOT HANDLE (bench items, OI-FAULTMSG-01)
 * -------------------------------------------------------
 * On silicon, a double-word torn by power loss can carry a double-bit ECC error,
 * and reading it raises an NMI (FLASH_ECCR.ECCD).  This module's decoding
 * assumes the read returns the torn bits; the NMI path needs its own handling,
 * which has not been bench-tested.  Recorded, not assumed away.
 *
 * IEC 62304 Class C — MISRA C:2012.  C11, no GNU extensions.
 */

#include "np_nv_state.h"
#include "np_safety_config.h"
#include "np_safety_hal.h"
#include <stdbool.h>
#include <stdint.h>

#define NP_NV_MAGIC      0x4E43U        /* "NC" — NeurOne cardiac */
#define NP_NV_VAL_SET    0x53455421UL   /* "SET!" — cutoff awaiting acknowledgement */
#define NP_NV_VAL_CLR    0x434C5221UL   /* "CLR!" — acknowledged */
#define NP_NV_ERASED     0xFFFFFFFFUL
#define NP_NV_SEQ_MAX    0xFFFFU

_Static_assert(NP_NV_PAGE_COUNT == 2U, "rotation assumes exactly two pages");

typedef struct {
    uint16_t used;          /* slots before the first erased tail slot        */
    bool     newest_torn;   /* last used slot is not a valid record           */
    bool     has_valid;
    uint16_t max_seq;       /* highest seq among valid records                */
    bool     max_set;       /* value of that record                           */
} np_nv_page_scan_t;

static np_nv_page_scan_t s_scan[NP_NV_PAGE_COUNT];
static bool              s_pending;

static bool slot_erased(uint32_t lo, uint32_t hi)
{
    return (lo == NP_NV_ERASED) && (hi == NP_NV_ERASED);
}

static bool slot_valid(uint32_t lo, uint32_t hi)
{
    return ((lo >> 16) == NP_NV_MAGIC) &&
           ((hi == NP_NV_VAL_SET) || (hi == NP_NV_VAL_CLR));
}

static void scan_page(uint8_t page, np_nv_page_scan_t *out)
{
    uint16_t slot;
    uint16_t last_used = 0U;
    bool     any       = false;

    out->used        = 0U;
    out->newest_torn = false;
    out->has_valid   = false;
    out->max_seq     = 0U;
    out->max_set     = false;

    /* The used region is everything up to the LAST non-erased slot.  Scanning
     * the whole page rather than stopping at the first erased slot means a hole
     * (which the append order never creates) cannot hide a later record. */
    for (slot = 0U; slot < NP_NV_SLOTS_PER_PAGE; slot++) {
        uint32_t lo;
        uint32_t hi;
        np_hal_nv_read_dword(page, slot, &lo, &hi);
        if (slot_erased(lo, hi)) {
            continue;
        }
        any       = true;
        last_used = slot;
        if (slot_valid(lo, hi)) {
            uint16_t seq = (uint16_t)(lo & 0xFFFFU);
            if (!out->has_valid || (seq >= out->max_seq)) {
                out->max_seq = seq;
                out->max_set = (hi == NP_NV_VAL_SET);
            }
            out->has_valid = true;
        }
    }

    if (any) {
        uint32_t lo;
        uint32_t hi;
        out->used = (uint16_t)(last_used + 1U);
        np_hal_nv_read_dword(page, last_used, &lo, &hi);
        out->newest_torn = !slot_valid(lo, hi);
    }
}

static void scan_all(void)
{
    uint8_t p;
    for (p = 0U; p < NP_NV_PAGE_COUNT; p++) {
        scan_page(p, &s_scan[p]);
    }
}

/* Page holding the newest valid record; page 0 if none is valid. */
static uint8_t live_page(void)
{
    if (s_scan[1].has_valid &&
        (!s_scan[0].has_valid || (s_scan[1].max_seq > s_scan[0].max_seq))) {
        return 1U;
    }
    return 0U;
}

static bool decode_pending(void)
{
    uint8_t p;
    bool    any_valid = false;

    for (p = 0U; p < NP_NV_PAGE_COUNT; p++) {
        if (s_scan[p].newest_torn) {
            return true;            /* fail closed: a torn newest write may be a SET */
        }
        if (s_scan[p].has_valid) {
            any_valid = true;
        }
    }
    if (!any_valid) {
        return false;               /* never written: factory-fresh */
    }
    return s_scan[live_page()].max_set;
}

np_safe_status_t np_nv_state_init(void)
{
    scan_all();
    s_pending = decode_pending();
    return NP_SAFE_OK;
}

bool np_nv_cardiac_pending(void)
{
    return s_pending;
}

static bool program_and_verify(uint8_t page, uint16_t slot, uint32_t lo, uint32_t hi)
{
    uint32_t rlo;
    uint32_t rhi;
    if (!np_hal_nv_program_dword(page, slot, lo, hi)) {
        return false;
    }
    np_hal_nv_read_dword(page, slot, &rlo, &rhi);
    return (rlo == lo) && (rhi == hi);
}

bool np_nv_cardiac_pending_set(bool pending)
{
    uint8_t  live;
    uint8_t  other;
    uint16_t seq;
    uint32_t lo;
    uint32_t hi = pending ? NP_NV_VAL_SET : NP_NV_VAL_CLR;
    bool     ok;

    scan_all();
    live  = live_page();
    other = (uint8_t)(1U - live);

    if (!s_scan[live].has_valid && !s_scan[other].has_valid) {
        /* No valid record anywhere: clear any torn residue in both pages. */
        if ((s_scan[0].used > 0U) && !np_hal_nv_erase_page(0U)) { return false; }
        if ((s_scan[1].used > 0U) && !np_hal_nv_erase_page(1U)) { return false; }
        seq = 1U;
        s_scan[0].used = 0U;
        live  = 0U;
        other = 1U;
    } else {
        if (s_scan[live].max_seq == NP_NV_SEQ_MAX) {
            /* 65 535 records is ~32 000 cutoffs: unreachable in the life of a
             * device, and refused rather than wrapped, because a wrapped seq
             * would make the newest record look like the oldest. */
            return false;
        }
        seq = (uint16_t)(s_scan[live].max_seq + 1U);
        /* Step 1: only one live page. */
        if ((s_scan[other].used > 0U) && !np_hal_nv_erase_page(other)) {
            return false;
        }
    }

    lo = ((uint32_t)NP_NV_MAGIC << 16) | (uint32_t)seq;

    if (s_scan[live].used < NP_NV_SLOTS_PER_PAGE) {
        ok = program_and_verify(live, s_scan[live].used, lo, hi);
    } else {
        /* Step 2: rotate.  Write the new page first; erase the old one only
         * after the new record is safely down.  A failed erase of the old page
         * leaves it as residue that step 1 removes on the next write. */
        ok = program_and_verify(other, 0U, lo, hi);
        if (ok) {
            (void)np_hal_nv_erase_page(live);
        }
    }

    scan_all();
    s_pending = decode_pending();
    return ok && (s_pending == pending);
}
