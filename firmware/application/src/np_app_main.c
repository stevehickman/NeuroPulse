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
 * is 94 traps rather than 94 drivers, so the first platform call halts the
 * processor.  For this image that happens in main() below, at
 * np_platform_clock_init(), before np_hub_control_app_main() is entered at all.
 * Until 2026-09-02 the first trap reached was np_hal_get_device_session_count()
 * a few hundred instructions further in; the clock seam is ahead of it because
 * the boot contract genuinely breaks there first.
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
 *        → np_app_cache_assert_state() (SCB->CCR is the state this image was
 *                                      built for — I-cache on, D-cache off;
 *                                      §4.14)
 *        → np_app_dtcm_bss_clear()    (zeroes .dtcm_bss — the startup clears
 *                                      only one bss span; §4.12)
 *        → np_platform_clock_init()   (firmware/platform — TRAPS TODAY)
 *        → SystemCoreClockUpdate() and the core-clock check below
 *        → np_hub_control_app_main()  (firmware/hub_control)
 *        → creates five FreeRTOS tasks, vTaskStartScheduler(), never returns
 *
 * ── The core clock, and why this file checks it (OI-SWCI-41) ────────────────
 *
 * There is still no board-level clock configuration in that sequence, and the
 * reason has not changed: BOARD_BootClockRUN() lives in the MCUX SDK's board
 * files, which are not vendored because no NeurOne board exists to configure
 * for.  SystemInit() disables the watchdogs, enables the I-cache — the
 * I-cache ONLY, whatever its own comment says (§4.14.1) — and ASSIGNS
 * SystemCoreClock = DEFAULT_SYSTEM_CLOCK (528 MHz); assigning a variable is
 * not configuring a PLL, and 528 MHz is not the 600 MHz FreeRTOSConfig.h
 * declares as configCPU_CLOCK_HZ.
 *
 * What has changed is that the gap is no longer only prose.  Two things were
 * wrong with leaving it as prose:
 *
 *   1. It was the one missing driver the platform census did not count, because
 *      the startup path calls it rather than a module.  np_platform_clock_init()
 *      is now the 94th seam, so the distance to a runnable image includes it.
 *
 *   2. Nothing would have caught a clock that was configured WRONG.  The
 *      ARM_CM7 port defines configSYSTICK_CLOCK_HZ as configCPU_CLOCK_HZ, so
 *      the SysTick reload is (configCPU_CLOCK_HZ / configTICK_RATE_HZ) - 1 and
 *      every FreeRTOS interval in the system is measured in that unit —
 *      including the NP_SAFETY_HEARTBEAT_MS beat the safety MCU's 1.5 s
 *      watchdog is waiting on.  A core clock that is not configCPU_CLOCK_HZ
 *      does not fail; it silently rescales every timeout in the image by the
 *      ratio between them.  At the SDK's own default that ratio is 600/528, so
 *      a nominal 200 ms heartbeat would actually be sent every 227 ms and
 *      nothing anywhere would say so.
 *
 * So the clock is verified rather than assumed, and it is verified against the
 * silicon rather than against the driver's own claim: SystemCoreClockUpdate()
 * reads the CCM and CCM_ANALOG dividers and computes the core frequency the
 * part is actually running at.  That check needs no board file, which is why it
 * could be written today.  What still needs a board is the configuration step
 * it checks — that half of OI-SWCI-41 stays open.
 */

#include <stdint.h>

#include "FreeRTOSConfig.h"     /* configCPU_CLOCK_HZ — the assumed core clock */
#include "system_MIMXRT1062.h"  /* SystemCoreClock, SystemCoreClockUpdate()    */

#include "np_app_cache.h"       /* np_app_cache_assert_state() */
#include "np_app_dtcm.h"        /* np_app_dtcm_bss_clear() */
#include "np_hub_types.h"       /* np_hub_control_app_main() */
#include "np_platform_trap.h"
#include "np_sw02_platform_hal.h" /* np_platform_clock_init() — declared once  */

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
    /*
     * FIRST, before anything else — NP-SW-CI-001 §4.14 (closes OI-SWCI-45).
     *
     * SCB->CCR against the two constants in np_app_cache.h: I-cache on,
     * D-cache off.  It is ahead of the .dtcm_bss clear below because it is a
     * statement about the MEMORY SYSTEM, and the clear is the first bulk write
     * this image makes under it.  Neither call depends on the other — the
     * clear stores to DTCM, which never goes through L1 whatever CCR says, and
     * this check reads one register and two compile-time constants — so the
     * order is not correctness but consequence: if the machine is not the one
     * this image was built for, the right next instruction is a halt, not
     * 64 KiB of stores.
     *
     * Nothing here configures a cache, and §4.14.3 is why: everything that ran
     * before main() — the ROM, the bootloader's PIO staging copy of this very
     * image, Reset_Handler — already ran under whatever state is in force, so
     * an image that "fixed" CCR would be repairing the successor of a fault it
     * had already suffered.
     */
    np_app_cache_assert_state();

    /*
     * THEN the second region the startup does not zero — NP-SW-CI-001 §4.12
     * (closes OI-SWCI-42).
     *
     * .dtcm_bss holds ucHeap and is bss-class storage the vendored startup does
     * NOT zero: startup_MIMXRT1062.S clears exactly one span for THIS image,
     * __bss_start__ to __bss_end__.  (It carries a second bss-class
     * initialiser, __STARTUP_INITIALIZE_NONCACHEDATA, which this build does not
     * define and which is not this region's job — it initialises the SDK's
     * NonCacheable section, the D-cache-era home for DMA buffers, and copies an
     * init image .dtcm_bss has none of.  §4.14.4.)
     *
     * These two calls are the earliest C code in the image and that is
     * checkable rather than asserted: __START is defined to main in the
     * application CMakeLists, so __libc_init_array never runs, and the linker
     * script's .init_array comes out empty.  Nothing can run before them.
     *
     * AHEAD OF THE CLOCK STEP BELOW, and the order is a decision rather than an
     * accident of two changes landing together.  This call is not application
     * logic; it is the tail of the startup's memory initialisation, finishing
     * the job __STARTUP_CLEAR_BSS could only do for one region.  Every later
     * line — the clock driver included, once one exists — is entitled to assume
     * statics read as zero, and a driver that touched anything in .dtcm_bss
     * before it was cleared would be reading the previous image's RAM.  Nothing
     * here depends on the clock: it is a store loop over a known address range,
     * correct at whatever frequency the part happens to be running.
     */
    np_app_dtcm_bss_clear();

    /*
     * Bring the core clock to configCPU_CLOCK_HZ, then prove it got there.
     *
     * This traps today — firmware/platform/ has no driver for it — so nothing
     * below runs yet.  The check is written now regardless, because the moment
     * a board arrives the driver lands underneath a call site that already
     * verifies it, rather than being trusted by a boot path that never looked.
     */
    uint32_t established_hz = np_platform_clock_init();

    /*
     * Ask the silicon, not the driver.  SystemCoreClockUpdate() walks the CCM
     * and CCM_ANALOG dividers and recomputes SystemCoreClock from the registers
     * as they actually stand; it is not a second copy of the driver's opinion.
     *
     * Checking both figures is deliberate.  established_hz catches a driver
     * that configured one frequency and reported another; SystemCoreClock
     * catches a driver that reported honestly and did not achieve it.  They are
     * different failures and neither implies the other.
     */
    SystemCoreClockUpdate();

    if (established_hz != configCPU_CLOCK_HZ || SystemCoreClock != configCPU_CLOCK_HZ) {
        /*
         * Halt rather than continue on a clock that is not the one every
         * FreeRTOS interval is computed against.  Continuing would not fail —
         * that is the danger.  It would run, with every tick, timeout and the
         * safety heartbeat rescaled by the ratio between the two clocks, and
         * with no symptom that points at the clock.  Halting instead stops the
         * SPI heartbeat and lets the safety MCU cut every stimulation enable
         * line inside 1.5 s (CLAUDE.md §4.2), which is the designed failure.
         */
        np_platform_unimplemented("main:core-clock-mismatch");
    }

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
