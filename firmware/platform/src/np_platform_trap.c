/*
 * NeurOne SW-02 Platform Trap — implementation
 * Document: NP-SW-CI-001 §4.8
 *
 * See include/np_platform_trap.h for why this is a halt and not a return value.
 */

#include "np_platform_trap.h"

/*
 * Volatile and file-scope so the compiler cannot fold the store away and a
 * debugger can read it after the halt.  Not written to SHDR: this is a
 * bring-up artifact, and a symbol name is not device-health telemetry.
 */
volatile const char *np_platform_last_unimplemented_symbol;

void np_platform_unimplemented(const char *symbol)
{
    np_platform_last_unimplemented_symbol = symbol;

    /* Mask interrupts, then spin.
     *
     * Masking first is what makes the halt reach the safety MCU: with
     * interrupts enabled, SysTick keeps running and the FreeRTOS scheduler
     * keeps switching, so task_safety_heartbeat would carry on sending a
     * heartbeat that says the main processor is healthy while it is not.  The
     * heartbeat has to stop for the 1.5 s watchdog to fire (CLAUDE.md §4.2).
     *
     * __disable_irq() from CMSIS is deliberately not used here: this file is
     * the one piece of the platform layer that must work identically in the
     * host-test build, where there is no CMSIS and no PRIMASK.
     */
#if defined(__ARM_ARCH) && !defined(NPTEST_HOST)
    __asm volatile ("cpsid i" ::: "memory");
    for (;;) {
        __asm volatile ("wfi");
    }
#else
    for (;;) {
        /* Host build: no interrupts to mask and no WFI.  The spin is what the
         * host test observes; it never calls this outside a death test. */
    }
#endif
}
