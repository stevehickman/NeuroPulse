/*
 * NeurOne SW-02 Application — the L1 cache state this image is built for
 * Document: NP-SW-CI-001 §4.14 (closes OI-SWCI-45)
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * ── The one place that says what the caches are doing ───────────────────────
 *
 * The Cortex-M7 in this part has both L1 caches.  Exactly one of them is on:
 *
 *   I-cache  ENABLED   by SystemInit() in the vendored MCUX startup, under
 *                      #if __ICACHE_PRESENT.  Instruction fetch from the OCRAM2
 *                      staging reservation is cached.
 *   D-cache  DISABLED  by nothing — it is off out of reset and no code in this
 *                      repository turns it on.  Every data access to OCRAM2 is
 *                      an UNCACHED bus access.
 *
 * That asymmetry was recorded backwards four times (§4.8.5, §4.10.6, the
 * linker script, and OI-SWCI-42 itself all described `.bss` as reached "through
 * the L1 D-cache"), which is what OI-SWCI-45 was raised for.  The reason it
 * could be written four times is that nothing in the tree stated the cache
 * state as a fact a check could read.  This header is that statement, and
 * np_app_cache_assert_state() is what compares it against the silicon.
 *
 * ── Why the D-cache stays off (the decision, NP-SW-CI-001 §4.14.3) ──────────
 *
 * Not because caching is unwanted — because enabling it is four pieces of work
 * this repository cannot yet do, and doing none of them while turning it on
 * produces the one failure class that does not announce itself:
 *
 *   1. MPU region attributes.  No MPU region is configured anywhere in this
 *      tree, so the ARMv7-M DEFAULT system address map is in force, and it
 *      makes the whole of 0x20000000..0x3FFFFFFF — OCRAM2 included — Normal,
 *      write-back, write-allocate.  That is the most dangerous attribute a DMA
 *      buffer can have, and it is what every buffer in `.bss` would get.
 *   2. Cache maintenance around every DMA buffer.  There is none, because
 *      there is no DMA yet: firmware/platform/ is 94 traps.  The buffers that
 *      will face DMA first are already identifiable and already in OCRAM2 —
 *      np_session_log.c's s_uhdr_buf/s_shdr_buf, np_transport.c's s_blob and
 *      np_hub_control_main.c's g_proto_buf, 20,480 B between them.
 *   3. A handover contract.  The bootloader stages this image into OCRAM2 with
 *      CPU stores (np_emmc.c is PIO, not ADMA) and jumps.  With a D-cache on
 *      and no clean before the jump, those stores sit in dirty lines and the
 *      instruction fetch reads stale memory.  That is OI-SWCI-15, open since
 *      2026-08-09 and asking for exactly this.
 *   4. A board.  There is no measurement of what the cache would buy, because
 *      this image traps a few hundred instructions into main() and no NeurOne
 *      board exists to run it on.
 *
 * And the value of enabling it shrank the day before the item was raised.
 * §4.12 moved ucHeap into DTCM, and TCM does not go through L1 at all — so
 * every FreeRTOS task stack and every dynamic allocation is already on a
 * single-cycle path that no cache decision can improve.  What is left in
 * cacheable memory is `.bss` (88,404 B) and `.rodata` (17,416 B).
 *
 * ── What "enabled" would mean when it changes ───────────────────────────────
 *
 * Flip NP_SW02_DCACHE_ENABLED to 1 and this header stops agreeing with the
 * boot path, the host suite and the CI census all at once — deliberately.  The
 * change that flips it is the change that adds the MPU configuration, moves
 * the DMA-facing buffers into the SDK's `NonCacheable` section (the vendored
 * startup already carries the initialiser for it — see §4.14.4), and settles
 * OI-SWCI-15's handover.  This constant is the last line of that change, not
 * the first.
 */

#ifndef NP_APP_CACHE_H
#define NP_APP_CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The state this image is BUILT FOR, not a request to establish it.  Nothing
 * here writes SCB->CCR; these two values are what np_app_cache_assert_state()
 * measures the running part against.
 *
 * 1 = the cache is expected to be on when main() is entered.
 * 0 = it is expected to be off.
 */
#define NP_SW02_ICACHE_ENABLED  1
#define NP_SW02_DCACHE_ENABLED  0

/*
 * Compare SCB->CCR against the two constants above and halt if either differs.
 *
 * Called as the FIRST statement of main().  It does not configure anything: an
 * image whose memory model does not match the machine it woke up on cannot fix
 * that by writing a register, because whatever ran before it has already been
 * running under the other model — the bootloader's staging copy included.
 *
 * The halt is np_platform_unimplemented(), which masks interrupts and spins,
 * stopping the 200 ms SPI heartbeat so the safety MCU cuts every stimulation
 * enable line inside 1.5 s (CLAUDE.md §4.2).  Same designed failure as the
 * core-clock mismatch check beside it in main().
 */
void np_app_cache_assert_state(void);

#ifdef __cplusplus
}
#endif

#endif /* NP_APP_CACHE_H */
