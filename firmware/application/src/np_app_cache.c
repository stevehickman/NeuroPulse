/*
 * NeurOne SW-02 Application — the L1 cache state check
 * Document: NP-SW-CI-001 §4.14 (closes OI-SWCI-45)
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * ── Why a check and not a configuration step ────────────────────────────────
 *
 * The obvious alternative is to make main() ESTABLISH the state: disable the
 * D-cache if it is on, enable it if it is not.  That is worse here, and the
 * reason is about who ran first rather than about the register write.
 *
 * By the time main() is reached, three things have already executed under
 * whatever cache state the part came up in — the boot ROM, the NeurOne
 * bootloader, and the vendored Reset_Handler.  The bootloader's job in that
 * sequence is to copy this image into OCRAM2 with CPU stores (np_emmc.c reads
 * the FIFO in PIO; it does not use ADMA) and branch to it.  If the D-cache was
 * on for that copy and nothing cleaned it, the bytes being executed are not the
 * bytes that were written, and no register write from inside the running image
 * repairs that — it repairs the symptom's successor, silently, which is the
 * worst of the available outcomes.
 *
 * So the image refuses instead.  Finding SCB->CCR in a state this build was not
 * written for means the memory model the whole image reasons about is not the
 * one in force, and the honest response is the same one every other unmet
 * precondition on this processor gets: halt, stop the heartbeat, let the safety
 * MCU cut the enable lines (CLAUDE.md §4.2).
 *
 * ── Why it is safe to run this early ────────────────────────────────────────
 *
 * It reads one register and two compile-time constants.  It touches no static
 * this image has not already initialised: SCB->CCR is hardware, the constants
 * are in np_app_cache.h, and np_platform_unimplemented()'s own latch is an
 * ordinary `.bss` object that __STARTUP_CLEAR_BSS has already zeroed.  In
 * particular it does NOT depend on np_app_dtcm_bss_clear() having run, which is
 * why it can precede it.
 */

#include <stdint.h>

#include "fsl_device_registers.h"   /* SCB, SCB_CCR_IC_Msk, SCB_CCR_DC_Msk */

#include "np_app_cache.h"
#include "np_platform_trap.h"

void np_app_cache_assert_state(void)
{
    const uint32_t ccr = SCB->CCR;

    const unsigned dc = ((ccr & SCB_CCR_DC_Msk) != 0U) ? 1U : 0U;
    const unsigned ic = ((ccr & SCB_CCR_IC_Msk) != 0U) ? 1U : 0U;

    /*
     * D-cache first.  It is the half with a silent failure mode: an unexpected
     * ENABLED D-cache means this image's DMA-free, maintenance-free memory
     * model is wrong about the machine, and nothing downstream would notice.
     * An unexpected DISABLED D-cache (once the decision flips) is only slow.
     * Distinct trap labels because np_platform_unimplemented() latches the
     * string for a debugger attached after the halt, and "which cache" is the
     * whole content of the finding.
     */
    if (dc != (unsigned)NP_SW02_DCACHE_ENABLED) {
        np_platform_unimplemented("main:dcache-state-mismatch");
    }

    /*
     * The I-cache half is a check on the vendored startup rather than on the
     * bootloader: SystemInit() enables it a few instructions before main() is
     * entered, so a mismatch here means the startup did not run, ran a
     * different branch, or was built without __ICACHE_PRESENT — each of which
     * makes every other assumption about the boot path unsafe too.
     */
    if (ic != (unsigned)NP_SW02_ICACHE_ENABLED) {
        np_platform_unimplemented("main:icache-state-mismatch");
    }
}
