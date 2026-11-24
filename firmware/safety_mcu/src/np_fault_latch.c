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
 *   offset 6:  uint32_t count   (total fault transitions since last power-on reset; 32-bit, never saturates)
 *   offset 10: uint32_t tick_ms (SysTick timestamp of last fault)
 *   offset 14: uint32_t dev_count (transitions of the DEVICE-CLASS bits only; the reported count, OI-PRIV-40)
 *   offset 18: uint8_t  pad[46] (reserved)
 *
 * dev_count was carved from pad, which init and clear zero and nothing else
 * writes, so a latch left by the previous image reads dev_count 0: the magic is
 * unchanged because no field an older image wrote changed meaning.
 *
 * OI-UINT16-01: `count` was 16 bits and stopped at 65535.  The latch lives in non-cleared SRAM, so a reset after a
 * firmware swap finds the previous image's latch: NP_FAULT_LATCH_MAGIC was changed with this layout so that an
 * old-layout latch is never read as a new one (it reads as no latch).
 *
 * Privacy classification of the latch fields for any hub-facing read
 * (NP-FW-EMMC-001 Rev 2 §12; docs/status/pending-decisions.md §13.4 "Fault latch extended SPI command
 * privacy gate", re-resolved 2026-08-12; narrowed by the principal's ruling of
 * 2026-11-23 under OI-PRIV-40 (NP-PRIV-ANALYSIS-004), data-architecture-detail.md
 * §5.1 "In-session electrode impedance check"): the report carries `status`
 * projected onto the device-class bits and the count of transitions of those
 * bits, and nothing else.  The in-session impedance result is UHDR and the
 * cardiac bit is not a device-class bit, so neither reaches the report by bit,
 * slot or count.  `slot` and `tick_ms` are not reported.  A future dedicated
 * read command must marshal through np_fault_latch_build_report(), which
 * returns one fixed-shape record with no status-dependent behaviour.  See the
 * block comment above that function.  (The 8-byte heartbeat reply frame still
 * carries the raw status and slot to the hub: that is the device-internal
 * control path the hub's drivers act on, not an SHDR write.)
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
    uint32_t count;
    uint32_t tick_ms;
    uint32_t dev_count;
    uint8_t  pad[46];
} np_fault_latch_t;
_Static_assert(sizeof(np_fault_latch_t) == NP_FAULT_LATCH_BYTES, "the fault latch is 64 bytes: the layout comment above and the linker section size depend on it");

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
     * persistent fault is active would run s_latch.count up by one every
     * iteration (it once saturated at 65535 in ~65 s, freezing the fault event counter and hiding subsequent distinct
     * fault events in the SHDR audit trail).  Change-gating means count tracks
     * "distinct fault transitions" rather than "loop iterations since fault". */
    bool status_changed = (s_latch.status != state->status) ||
                          (s_latch.slot   != state->fault_slot);

    /* Reporting bookkeeping only (OI-PRIV-40): a transition of the device-class
     * bits.  A change of the impedance bit, the cardiac bit or the slot alone is
     * not one, so the reported count cannot carry them.  Nothing reads
     * dev_count but np_fault_latch_build_report(); no control decision uses it. */
    bool device_changed = ((uint8_t)(s_latch.status ^ state->status) &
                           NP_FAULT_LATCH_DEVICE_BITS) != 0U;

    s_latch.magic  = NP_FAULT_LATCH_MAGIC;
    s_latch.status = state->status;
    s_latch.slot   = state->fault_slot;

    if (status_changed) {
        s_latch.tick_ms = np_hal_get_tick_ms();
        s_latch.count++;     /* 32-bit: a count never saturates (OI-UINT16-01) */
    }
    if (device_changed) {
        s_latch.dev_count++; /* 32-bit, as count */
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

/* ── SHDR read marshalling (NP-FW-EMMC-001 Rev 2 §12; REQ-SW01-06) ───────────
 *
 * The current 8-byte MCU→hub reply frame carries `status` and `fault_slot` only;
 * nothing else in the latch leaves SRAM today.  When a future dedicated
 * fault-latch SPI read command is designed to let the hub retrieve the latch, it
 * MUST build its payload from np_fault_latch_build_report() instead of touching
 * s_latch directly.  That bakes the privacy rulings into enforceable code
 * rather than prose.
 *
 *   status  → SHDR, PROJECTED onto the device-class bits (FAULT, WATCHDOG,
 *             CUTOFF, THERMAL, CHARGE, SIG_PENDING; the bits
 *             scripts/check-redaction-shape.ts classes "device").  The
 *             in-session impedance bit is UHDR (the principal, 2026-11-23,
 *             OI-PRIV-40 (NP-PRIV-ANALYSIS-004)): the check measures through
 *             the wearer and cannot tell a device fault from placement.  The
 *             cardiac bit is not device-class either; the cardiac cutoff still
 *             shows as CUTOFF, and the hub's SHDR fault log records it by name
 *             (NP_CVNS_SHDR_EV_CUTOFF → fault_log.fault_type
 *             'CVNS_CARDIAC_INTERLOCK_CUTOFF': one event for every cause, which
 *             SHDR never learns, OI-PRIV-41 Q2) under the safety-interlock rule.
 *
 *   count   → SHDR unconditionally: the number of transitions of the
 *             device-class bits since power-on (np_fault_latch_commit keeps it
 *             as dev_count).  A change of the impedance bit, the cardiac bit or
 *             the slot alone does not move it.  This is what fleet predictive
 *             maintenance consumes (NP-PMS-001 §5: "fault latch counts").
 *
 *   slot    → NOT REPORTED (OI-PRIV-40).  The latch keeps one slot, and the
 *             last writer wins: the impedance check writes its channel's slot
 *             without a device-class bit, a device-class bit can rise without
 *             writing the slot (SIG_PENDING, WATCHDOG), and the thermal
 *             interlock runs before the impedance poll in the same main-loop
 *             iteration.  So no slot can be attributed to a device-class
 *             transition without sometimes being the one the impedance check
 *             wrote.  Masking the bit and keeping the slot would read
 *             "OK with a slot", the impedance failure and its channel
 *             (CLAUDE.md §5.1 rule 2).
 *
 *   tick_ms → NOT REPORTED.  Device-internal only.  Fault event timing is UHDR:
 *             ci/shdr/shdr_fleet_schema.sql `fault_log` has no timing column and
 *             the hub logger np_log_shdr_fault() discards its `session_ms`
 *             argument for EVERY caller.
 *
 * WHY ONE FIXED SHAPE — the retired design and the defect it created (2026-08-12):
 * `count` and `tick_ms` were once two INDEPENDENT accessors, and `tick_ms` was
 * zeroed only when the latched fault carried the cardiac bit.  That made the
 * observable pair (count > 0, tick_ms == 0) a one-bit cardiac oracle.  A
 * redaction applied conditionally on a sensitive predicate leaks that
 * predicate; it must be unconditional, or the predicate must not be inferable
 * from the pattern of redaction.  The projection here is the same for every
 * fault: one mask, applied always; one counter, kept always.  An impedance-only
 * transition therefore reports exactly as no transition does.
 *
 * IEC 62304 Class C: this is a reporting-path change only.  No stimulation
 * enable, interlock threshold, cutoff timing or control decision is altered;
 * np_fault_latch_commit() gained only the dev_count bookkeeping that this
 * function reads, and _clear()/_init() are untouched.
 *
 * Invalid latch yields the same fixed shape (status OK, count 0),
 * indistinguishable from a valid latch that has recorded no fault.
 */
void np_fault_latch_build_report(np_fault_latch_report_t *out)
{
    if (out == NULL) {
        return;
    }
    /* The mask is written out here, not through NP_FAULT_LATCH_DEVICE_BITS, so
     * scripts/check-redaction-shape.ts can see that only device-class bits pass
     * (rule 6); the _Static_assert below holds the two equal. */
    out->status = np_fault_latch_get_status() &
                  (NP_SAFETY_STATUS_FAULT | NP_SAFETY_STATUS_WATCHDOG |
                   NP_SAFETY_STATUS_CUTOFF | NP_SAFETY_STATUS_THERMAL |
                   NP_SAFETY_STATUS_CHARGE | NP_SAFETY_STATUS_SIG_PENDING);
    out->count  = (s_latch.magic == NP_FAULT_LATCH_MAGIC) ? s_latch.dev_count : 0U;
}
_Static_assert(NP_FAULT_LATCH_DEVICE_BITS ==
               (NP_SAFETY_STATUS_FAULT | NP_SAFETY_STATUS_WATCHDOG |
                NP_SAFETY_STATUS_CUTOFF | NP_SAFETY_STATUS_THERMAL |
                NP_SAFETY_STATUS_CHARGE | NP_SAFETY_STATUS_SIG_PENDING),
               "the report's mask and the commit's device-class mask are one set (REQ-SW01-06)");

#if defined(NPTEST_HOST)
/* ── Host models only: not built into the device image, not a reporting path ──
 * The simulator models a warm reset by keeping what the part's retained SRAM
 * keeps: the whole latch, byte for byte (np_sim_safety_hal.c keep_latch /
 * restore_latch).  It used to read the latch through the report, which no
 * longer carries the slot or the raw count. */
void np_fault_latch_host_save(uint8_t out[NP_FAULT_LATCH_BYTES])
{
    memcpy(out, &s_latch, NP_FAULT_LATCH_BYTES);
}

void np_fault_latch_host_restore(const uint8_t in[NP_FAULT_LATCH_BYTES])
{
    memcpy(&s_latch, in, NP_FAULT_LATCH_BYTES);
}

uint32_t np_fault_latch_host_raw_count(void)
{
    return (s_latch.magic == NP_FAULT_LATCH_MAGIC) ? s_latch.count : 0U;
}
#endif
