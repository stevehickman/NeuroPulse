/*
 * NeuroPulse Safety MCU — SW01-M08: Non-Volatile Fault Latch
 * Document: NP-SW-001 Rev A, NP-FMEA-001 Rev A §SW01-M08
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
 */

#include "np_safety_config.h"
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

/* Placed in the .fault_latch section by the linker script (NOLOAD, no zero). */
static __attribute__((section(".fault_latch")))
np_fault_latch_t s_latch;

/* ── HAL stub ────────────────────────────────────────────────────────────── */
extern uint32_t np_hal_get_tick_ms(void);

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
