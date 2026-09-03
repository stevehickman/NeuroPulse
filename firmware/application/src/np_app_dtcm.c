/*
 * NeurOne SW-02 Application — DTCM-resident data, and its zeroing
 * Document: NP-SW-CI-001 §4.11 (closes OI-SWCI-42), §4.8 (phase 8), §4.10 (phase 9)
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * ── What this file is for ───────────────────────────────────────────────────
 *
 * Phase 8 placed ALL of .bss in the OCRAM2 staging reservation, on the
 * reasoning that the static footprint does not fit the 128 KiB of DTCM the
 * MCUX SDK declares.  The footprint does not fit.  The conclusion does not
 * follow, and §4.11 is the measurement that shows why: DTCM held .data —
 * 304 bytes — plus a .stack pinned at the far end of the region, and nothing
 * else.  Of the 131,072 B that exist at 0x20000000 under the DEFAULT eFuse
 * partition, before any register is written, 130,768 were unallocated.  The
 * choice was never "all of .bss in DTCM or none of it".
 *
 * ── Why ucHeap, specifically ────────────────────────────────────────────────
 *
 * It is the one buffer whose placement can be argued without a profiler, and
 * this image cannot be profiled — its platform layer traps a few hundred
 * instructions into main() (§4.8).  Every FreeRTOS task stack and every
 * dynamic allocation in the system is cut from ucHeap by heap_4, so its
 * memory class is the memory class of nearly every local variable and every
 * call frame on the device.  That is a property of the calling convention,
 * not an assumption about the workload — which is exactly what distinguishes
 * it from the rest of .bss, where "which buffers are hot" genuinely does need
 * the bench.
 *
 * What it costs to be wrong is also asymmetric.  ucHeap is .bss-class storage:
 * NOLOAD, so it adds no image bytes, and heap_4 builds its own free list in
 * prvHeapInit() rather than relying on the contents.
 *
 * ── Why this is safe WITHOUT the FlexRAM partition (OI-SWCI-39) ─────────────
 *
 * 128 KiB of DTCM at 0x20000000 is the DEFAULT eFuse partition — the figure
 * the MCUX SDK's own linker scripts carry as m_data, and the one every build
 * here was linked against before phase 9 existed.  It requires no code to run
 * and no register to be written.  ucHeap (65,536 B) and .data (304 B) occupy
 * 65,840 B of that window and leave 65,232 B.  So np_flexram_apply_partition()
 * could work, silently do nothing, or never be called, and this placement is
 * correct in all three cases.
 *
 * The linker script states that as an assertion rather than a hope:
 * NP_DTCM_GUARANTEED_SIZE is 128 KiB, and .data + .dtcm_bss are asserted to
 * end inside it.  Note the MSP stack is deliberately NOT inside the guarantee
 * — §4.10.6 pinned it to the top of the ESTABLISHED 512 KiB, and §4.11.3 keeps
 * it there, because that is what makes a partition which failed to take effect
 * a deterministic fault: the first `bl` in Reset_Handler pushes to an address
 * that does not exist under the default partition, so the image dies at reset,
 * before .data is copied and before any buffer is read.  That check is
 * upstream of every DTCM resident, which is why a ucHeap inside the guarantee
 * adds no exposure to a failure that has already happened.
 *
 * ── Why the zeroing is here and not in the startup ──────────────────────────
 *
 * The vendored SDK startup zeroes exactly ONE span, __bss_start__ to
 * __bss_end__ (startup_MIMXRT1062.S, under __STARTUP_CLEAR_BSS).  It cannot be
 * taught about a second region: firmware/vendor/mcux_sdk/ is byte-exact under
 * the §9 in-tree rule, and patching it is precisely what that rule forbids.
 *
 * So a second bss-class region in DTCM is only correct if first-party code
 * zeroes it, and that is np_app_dtcm_bss_clear() below, called as the first
 * statement of main().  "First statement of main()" is early enough and is
 * checkable rather than argued: this image enters main() directly (__START is
 * defined to main in the application CMakeLists), __libc_init_array never
 * runs, and the linker script's .init_array comes out empty — so there is no C
 * code that can run before it.
 *
 * The failure this prevents is silent.  A static in .dtcm_bss that nothing
 * zeroed holds whatever the last image left in that RAM, which on a warm reset
 * is plausible-looking data rather than obvious garbage.
 */

#include <stdint.h>
#include <stddef.h>          /* size_t — configTOTAL_HEAP_SIZE casts to it */

#include "FreeRTOSConfig.h"     /* configTOTAL_HEAP_SIZE */
#include "np_app_dtcm.h"

/*
 * The FreeRTOS heap.
 *
 * Declared by heap_4.c as `extern uint8_t ucHeap[configTOTAL_HEAP_SIZE]` under
 * configAPPLICATION_ALLOCATED_HEAP == 1, which FreeRTOSConfig.h sets for the
 * device build only.  The name, type and extent are heap_4's; only the section
 * is ours.  A mismatch in size is a compile error at heap_4.c's declaration,
 * not a silent overlay, because both sides derive it from configTOTAL_HEAP_SIZE.
 *
 * Alignment: heap_4 aligns internally from the address it is given
 * (portBYTE_ALIGNMENT, 8 on this port), so it does not require an aligned
 * array.  It is aligned to 8 anyway so the first block needs no adjustment and
 * the usable heap is the whole array rather than the array minus a few bytes.
 */
uint8_t ucHeap[configTOTAL_HEAP_SIZE]
    __attribute__((section(".dtcm_bss"), aligned(8)));

/*
 * Region bounds, defined by app_imxrt1062.ld.  uint8_t rather than a typed
 * object: these are addresses, and taking &symbol is the only defined way to
 * read a linker-provided value.
 */
extern uint8_t __dtcm_bss_start__[];
extern uint8_t __dtcm_bss_end__[];

void np_app_dtcm_bss_clear(void)
{
    uint8_t *p   = __dtcm_bss_start__;
    uint8_t *end = __dtcm_bss_end__;

    /* Byte-wise rather than word-wise on purpose.  The linker script aligns
     * both bounds, so a word loop would be correct today and would become
     * incorrect silently if an unaligned member were ever added.  This region
     * is zeroed once, at boot, and its size is tens of kilobytes; the cost of
     * not depending on that alignment is immeasurable next to the cost of
     * depending on it wrongly. */
    while (p < end) {
        *p++ = 0U;
    }
}
