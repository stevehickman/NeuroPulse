/*
 * NeurOne Safety MCU — SW01-M08: Non-Volatile Fault Latch
 * Document: NP-SW-001 Rev 1, NP-FMEA-001 Rev 1 §SW01-M08
 *
 * Stores fault state in the .fault_latch section (top 64 bytes of SRAM,
 * explicitly NOT zeroed by startup code on warm reset).
 *
 * A power-on reset clears this region; a watchdog reset or soft reset does
 * not, so a fault that triggers a reset will persist until the app explicitly
 * clears it.  This prevents silent fault recovery from hiding safety events
 * from the user.
 *
 * Fault latch layout (64 bytes):
 *   offset 0:  uint32_t magic   (NP_FAULT_LATCH_MAGIC if latch is valid)
 *   offset 4:  uint8_t  status  (NP_SAFETY_STATUS_* flags at time of fault)
 *   offset 5:  uint8_t  slot    (fault_slot at time of fault)
 *   offset 6:  uint16_t count   (total faults since last power-on reset)
 *   offset 8:  uint32_t tick_ms (SysTick timestamp of last fault)
 *   offset 12: uint8_t  pad[52] (reserved)
 *
 * Privacy classification of the latch fields for any hub-facing read
 * (NP-FW-EMMC-001 Rev 2 §12; docs/status/pending-decisions.md §13.4 "Fault latch extended SPI command
 * privacy gate", re-resolved 2026-08-12): `status` and `slot` are already
 * surfaced in the 8-byte reply frame and are SHDR device-condition data.
 * `count` is not in that frame and is SHDR.  `tick_ms` is NOT SHDR-reportable
 * at all — fault event timing is UHDR.  A future dedicated read command must
 * marshal the reportable fields through np_fault_latch_build_report(), which
 * returns one fixed-shape record with no status-dependent behaviour.  See the
 * block comment above that function.
 */

#include "np_safety_config.h"
#include "np_safety_hal.h"
#include "np_safety_protocol.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ── Fault latch structure in non-cleared SRAM ───────────────────────────── */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t  status;
    uint8_t  slot;
    uint16_t count;
    uint32_t tick_ms;
    uint8_t  pad[52];
} np_fault_latch_t;

/* Placed in the .fault_latch section by the linker script (NOLOAD, no zero).
 * Host unit tests (NPTEST_HOST) build this translation unit without the section
 * attribute — Mach-O rejects a bare (segment-less) section name, and the host
 * tests exercise the fault-classification/marshalling logic, not warm-reset
 * persistence.  The target image build (no NPTEST_HOST) is unchanged. */
#if defined(NPTEST_HOST)
#define NP_FAULT_LATCH_SECTION
#else
#define NP_FAULT_LATCH_SECTION __attribute__((section(".fault_latch")))
#endif

static NP_FAULT_LATCH_SECTION np_fault_latch_t s_latch;

/* HAL: np_hal_get_tick_ms — declared in np_safety_hal.h.  Its value lands in
 * s_latch.tick_ms, which stays device-internal: it is read by MCU-side timing
 * logic and is never marshalled to the hub.  Fault event timing is UHDR
 * (CLAUDE.md §5.1).                                                         */

np_safe_status_t np_fault_latch_init(bool *prior_fault_out)
{
    *prior_fault_out = false;

    if (s_latch.magic == NP_FAULT_LATCH_MAGIC) {
        /* Warm reset with valid latch: prior fault persists */
        *prior_fault_out = (s_latch.status != NP_SAFETY_STATUS_OK);
    } else {
        /* Power-on reset or first boot: initialize latch */
        memset(&s_latch, 0, sizeof(s_latch));
        s_latch.magic = NP_FAULT_LATCH_MAGIC;
        s_latch.slot  = 0xFFU;
    }
    return NP_SAFE_OK;
}

void np_fault_latch_commit(const np_safety_state_t *state)
{
    /* Only increment count and update tick_ms when fault status or slot
     * actually changes.  Committing on every main-loop iteration while a
     * persistent fault is active saturates s_latch.count (UINT16_MAX = 65535)
     * in ~65 s, freezing the fault event counter and hiding subsequent distinct
     * fault events in the SHDR audit trail.  Change-gating means count tracks
     * "distinct fault transitions" rather than "loop iterations since fault". */
    bool status_changed = (s_latch.status != state->status) ||
                          (s_latch.slot   != state->fault_slot);

    s_latch.magic  = NP_FAULT_LATCH_MAGIC;
    s_latch.status = state->status;
    s_latch.slot   = state->fault_slot;

    if (status_changed) {
        s_latch.tick_ms = np_hal_get_tick_ms();
        if (s_latch.count < UINT16_MAX) {
            s_latch.count++;
        }
    }
}

void np_fault_latch_clear(void)
{
    memset(&s_latch, 0, sizeof(s_latch));
    s_latch.magic = NP_FAULT_LATCH_MAGIC;
    s_latch.slot  = 0xFFU;
}

uint8_t np_fault_latch_get_status(void)
{
    return (s_latch.magic == NP_FAULT_LATCH_MAGIC) ? s_latch.status
                                                    : NP_SAFETY_STATUS_OK;
}

uint8_t np_fault_latch_get_slot(void)
{
    return (s_latch.magic == NP_FAULT_LATCH_MAGIC) ? s_latch.slot : 0xFFU;
}

/* ── SHDR read marshalling (NP-FW-EMMC-001 Rev 2 §12) ─────────────────────────
 *
 * The current 8-byte MCU→hub reply frame carries `status` and `fault_slot` only;
 * `count` and `tick_ms` never leave SRAM today.  When a future dedicated
 * fault-latch SPI read command is designed to let the hub retrieve the latch, it
 * MUST build its payload from np_fault_latch_build_report() instead of touching
 * s_latch directly.  That bakes the docs/status/pending-decisions.md §13.4
 * privacy-gate resolution into enforceable code rather than prose.
 *
 *   status  → SHDR.  Already in the reply frame.  It carries the fault KIND,
 *             including NP_SAFETY_STATUS_CARDIAC.  This disclosure is deliberate
 *             and pre-existing: the hub's own SHDR fault log already records the
 *             cardiac cutoff as a first-class named event
 *             (NP_CVNS_SHDR_EV_CUTOFF 0xC1 → fault_log.fault_type
 *             'CVNS_HR_CUTOFF', ci/shdr/shdr_fleet_schema.sql), per the locked
 *             "safety interlock log → SHDR" boundary rule (CLAUDE.md §5.1).
 *
 *   slot    → SHDR.  Already in the reply frame.  Device topology, no biology.
 *
 *   count   → SHDR unconditionally.  A tally of DISTINCT fault transitions since
 *             power-on (see np_fault_latch_commit change-gating); device-
 *             condition only, no biology, no timestamp — mirrors the SHDR
 *             "device session count".  This is the field fleet predictive
 *             maintenance actually consumes (NP-PMS-001 §5: "fault latch counts").
 *
 *   tick_ms → NOT REPORTED.  Device-internal only.  Fault event timing is UHDR:
 *             ci/shdr/shdr_fleet_schema.sql `fault_log` has no timing column and
 *             states "Precise fault event timing, where it exists at all, is
 *             UHDR under the user's key", and the hub logger np_log_shdr_fault()
 *             already discards its `session_ms` argument for EVERY caller.  This
 *             function is consistent with that existing unconditional rule.
 *
 * WHY THIS SHAPE — the retired design and the defect it created (2026-08-12):
 * Until now `count` and `tick_ms` were two INDEPENDENT accessors, and `tick_ms`
 * was zeroed only when the latched fault carried NP_SAFETY_STATUS_CARDIAC.  That
 * made the observable pair (count > 0, tick_ms == 0) a one-bit cardiac oracle,
 * readable with certainty and needing no correlation against anything — strictly
 * worse than not redacting at all, because a bare relative SysTick value is
 * meaningless without a session record SHDR does not hold, whereas the redaction
 * PATTERN was self-interpreting.  A redaction applied conditionally on a
 * sensitive predicate leaks that predicate; it must be unconditional, or the
 * predicate must not be inferable from the pattern of redaction.
 *
 * The fix is therefore structural, not a different constant: ONE fixed-shape
 * record, every member populated for every fault type, no branch anywhere in
 * this path that reads the status word.  A field cannot be present for one fault
 * kind and absent for another when the struct shape is fixed at compile time.
 *
 * IEC 62304 Class C: this is a reporting-path change only.  No stimulation
 * enable, interlock threshold, cutoff timing, or latch-commit behaviour is
 * altered; np_fault_latch_commit()/_clear()/_init() are untouched.
 *
 * Invalid latch yields the same fixed shape (status OK, slot 0xFF, count 0) —
 * indistinguishable from a valid latch that has recorded no fault, and carrying
 * no sensitive predicate either way.
 */
void np_fault_latch_build_report(np_fault_latch_report_t *out)
{
    if (out == NULL) {
        return;
    }
    /* Derived from the existing accessors so the invalid-latch sentinels stay
     * single-sourced.  No member consults NP_SAFETY_STATUS_CARDIAC. */
    out->status = np_fault_latch_get_status();
    out->slot   = np_fault_latch_get_slot();
    out->count  = (s_latch.magic == NP_FAULT_LATCH_MAGIC) ? s_latch.count : 0U;
}
