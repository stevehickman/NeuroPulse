/*
 * NeurOne SW-02 Application — image entry point
 * Document: NP-SW-CI-001 §4.8 (phase 8, closes OI-SWCI-21), NP-FW-HUB-001
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * ── What np_application is ──────────────────────────────────────────────────
 *
 * The SW-02 executable target.  Until it existed, the cross-build archived
 * thirteen static libraries and linked exactly one ELF — the bootloader — so
 * every SW-02 module was verified as *compiling* and nothing verified that they
 * LINK.  That is OI-SWCI-21, and it made the main-firmware leg's "zero
 * unresolved symbols" true and vacuous: no SW-02 link ran, so nothing could be
 * unresolved.
 *
 * ── What it is not ──────────────────────────────────────────────────────────
 *
 * It is not runnable firmware, and the image says so about itself: see the
 * .np_build_note string below.  Its platform layer is firmware/platform/, which
 * is 93 traps rather than 93 drivers, so the first platform call halts the
 * processor.  For this image that happens inside np_hub_control_app_main(), at
 * np_hal_get_device_session_count(), a few hundred instructions in.
 *
 * That is the intended behaviour, not a limitation being tolerated.  A halted
 * main processor stops the 200 ms SPI heartbeat, and the safety MCU's 1.5 s
 * watchdog then cuts every stimulation enable line (CLAUDE.md §4.2).  The
 * failure is loud, immediate, and lands in the state the system is designed to
 * fail into.
 *
 * ── The boot contract this file sits inside ────────────────────────────────
 *
 *   ROM  → bootloader (OCRAM 0x20200000, np_main.c)
 *        → reads this image from an eMMC bank into 0x20210000
 *        → SCB->VTOR = 0x20210000, MSP = word[0], branch to word[1]
 *   Reset_Handler (vendored startup_MIMXRT1062.S)
 *        → SystemInit()  (vendored system_MIMXRT1062.c)
 *        → copy .data from its LMA in the staging image to DTCM
 *        → zero .bss    (__STARTUP_CLEAR_BSS)
 *        → branch to __START, which the build defines as main()
 *   main()  → this file
 *        → np_hub_control_app_main()  (firmware/hub_control)
 *        → creates five FreeRTOS tasks, vTaskStartScheduler(), never returns
 *
 * There is no board-level clock configuration step in that sequence, and its
 * absence is deliberate rather than overlooked: BOARD_BootClockRUN() lives in
 * the MCUX SDK's board files, which are not vendored because no NeurOne board
 * exists to configure for.  SystemInit() sets SystemCoreClock to the SDK's
 * DEFAULT_SYSTEM_CLOCK and disables the watchdogs; it does not bring the PLLs
 * up to the 600 MHz FreeRTOSConfig.h assumes.  Recorded as OI-SWCI-41.
 */

#include <stdint.h>

#include "np_hub_types.h"       /* np_hub_control_app_main() */
#include "np_platform_trap.h"

/*
 * A statement about what this binary is, carried inside the binary.
 *
 * Build logs get separated from artifacts.  A .bin recovered from a share, a
 * ticket or a bench PC six months from now has no build log attached, and the
 * one question that matters about it — is this a real image or a link-only
 * bring-up artifact — is not answerable from its size or its symbols.  It is
 * answerable from `strings np_application.bin | grep NEURONE-BUILD-NOTE`.
 *
 * KEEP()d by the linker script into .np_build_note so --gc-sections cannot
 * discard it for having no reference.
 */
__attribute__((used, section(".np_build_note")))
static const char np_build_note[] =
    "NEURONE-BUILD-NOTE: np_application (SW-02) is a BRING-UP IMAGE. "
    "Its platform layer is firmware/platform/, which traps on every call "
    "instead of driving hardware (NP-SW-CI-001 section 4.8). "
    "This image links; it does not run. Do not flash to a device.";

int main(void)
{
    np_hub_control_app_main();

    /*
     * Unreachable: np_hub_control_app_main() ends in vTaskStartScheduler()
     * followed by its own for(;;).  Reaching here would mean the scheduler
     * returned, which FreeRTOS does only on a heap failure at startup — a
     * condition configUSE_MALLOC_FAILED_HOOK already routes to
     * np_freertos_assert_failed().  Trapping rather than returning keeps the
     * main() → newlib exit path out of the image entirely, and gives this case
     * the same halt-and-let-the-safety-MCU-cut behaviour as every other
     * unhandled condition on this processor.
     */
    np_platform_unimplemented("main:scheduler-returned");
}
