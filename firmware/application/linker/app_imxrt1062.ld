/*
 * NeurOne SW-02 Application Linker Script — np_application
 * Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
 * Document: NP-SW-CI-001 §4.8 (phase 8, closes OI-SWCI-21), NP-FW-HUB-001
 *
 * ── Why this file is NeurOne-authored and not the vendored SDK one ──────────
 *
 * Every MCUX SDK linker script for this part (MIMXRT1062xxxxx_flexspi_nor.ld and
 * its siblings) links .text at 0x60002400 for execution in place from FlexSPI
 * NOR flash.  NeurOne has no FlexSPI application image.  The bootloader reads
 * the application out of an eMMC bank into the OCRAM staging area and jumps
 * through the vector table it finds at the base of that area
 * (firmware/bootloader/src/np_main.c load_and_jump()).  So the SDK scripts are
 * not a starting point to adapt; they describe a different boot architecture.
 * firmware/vendor/mcux_sdk/VERSION records this as a deliberate non-vendoring.
 *
 * ── The contract this script has to satisfy ─────────────────────────────────
 *
 * load_and_jump() does exactly four things that constrain this file:
 *
 *   1. copies hdr.image_size bytes to np_app_staging_base()   → the loadable
 *      image must be position-fixed at that address, not relocatable;
 *   2. rejects image_size > np_app_max_image_size()           → the loadable
 *      image must fit the reservation;
 *   3. writes SCB->VTOR = app_dest                            → .isr_vector must
 *      be the FIRST thing in the image, at offset 0;
 *   4. reads word 0 as the initial SP and word 1 as the entry → which the SDK
 *      startup's __isr_vector already provides (__StackTop, Reset_Handler).
 *
 * ── The three numbers below are duplicated, and the duplication is enforced ──
 *
 * NP_APP_IMAGE_ORIGIN and NP_APP_IMAGE_LENGTH restate values owned by
 * firmware/bootloader/linker/bootloader_imxrt1062.ld:
 *
 *     NP_APP_IMAGE_ORIGIN = ORIGIN(OCRAM) + _app_load_offset
 *     NP_APP_IMAGE_LENGTH = _app_staging_size
 *                         = LENGTH(OCRAM) - _app_load_offset - _stack_size
 *
 * A linker script cannot include another one, so there is no way to write these
 * once.  What CAN be done is make disagreement fail a test rather than a
 * device: np_app_link_agreement_tests parses BOTH scripts, evaluates the
 * bootloader's arithmetic, and asserts the two agree.  Defect C
 * (NP-SW-CI-001 §4.3) was this exact duplication — the staging size written
 * once in the linker script and once in C — and it shipped an off-by-8-KiB
 * that copied an application over the stack it was running on.  The lesson
 * taken there was not "never duplicate", which is not always available; it was
 * "if you must duplicate, make something fail".
 *
 * NP_FLEXRAM_DTCM_SIZE below is the third, added by §4.10: it restates a bank
 * count owned by the same bootloader script AND by np_config.h, which is what
 * np_flexram_apply_partition() actually writes to IOMUXC_GPR17.  The test reads
 * all three.  The failure it prevents is worse than the staging one, because it
 * is not detectable at link time on either side: a DTCM region larger than the
 * partition establishes links cleanly and faults on first access to memory that
 * does not exist.
 */

NP_OCRAM_ORIGIN       = 0x20200000;
NP_APP_LOAD_OFFSET    = 64K;
NP_APP_IMAGE_ORIGIN   = NP_OCRAM_ORIGIN + NP_APP_LOAD_OFFSET;   /* 0x20210000 */
NP_APP_IMAGE_LENGTH   = 440K;                                   /* 512K - 64K - 8K */

/* Application stack.  Distinct from the bootloader's 8 KiB stack, which lives
 * at the top of OCRAM and is dead by the time this image runs — the SDK
 * startup sets MSP from __isr_vector[0] before anything else.  This is the
 * MSP stack used by main() and by every interrupt handler; FreeRTOS tasks get
 * their own stacks from the heap_4 pool in .bss. */
NP_APP_STACK_SIZE     = 8K;

/* Data Tightly Coupled Memory, as ESTABLISHED by the bootloader rather than as
 * inherited from a default.  NP-SW-CI-001 §4.10 (closes OI-SWCI-39).
 *
 * This was 128 KiB, taken from the MCUX SDK's m_data — a figure that described
 * the DEFAULT eFuse partition, which nothing in this repository had chosen or
 * written down.  It is now 16 FlexRAM banks × 32 KiB, the partition
 * src/np_flexram.c puts in force at B-0, before this image is ever jumped to.
 *
 * Third statement of one fact (bootloader_imxrt1062.ld's NP_FLEXRAM_*_BANKS,
 * np_config.h's, and this).  np_app_link_agreement_tests reads all three and
 * fails on disagreement — the same treatment, for the same reason, as
 * NP_APP_IMAGE_LENGTH above. */
NP_FLEXRAM_DTCM_SIZE  = 512K;

/* ── The DTCM that exists WITHOUT any register write ─────────────────────────
 *
 * NP-SW-CI-001 §4.11 (closes OI-SWCI-42).
 *
 * NP_FLEXRAM_DTCM_SIZE above is the DTCM the bootloader ESTABLISHES, and it is
 * established by three stores to IOMUXC_GPR that no NeurOne board has yet run.
 * §4.10.6 declined to move `.bss` into it for exactly that reason, and that
 * caution was right.
 *
 * This constant is the OTHER figure, and the distinction is what makes §4.11
 * safe: 128 KiB at 0x20000000 is the DEFAULT eFuse partition — the value the
 * MCUX SDK's own linker scripts declare as m_data, and the value that was in
 * force for every build before phase 9 existed.  It requires no code to run and
 * no register to be written.
 *
 * So DTCM residency splits in two.  Anything inside the first 128 KiB is
 * correct whether np_flexram_apply_partition() worked, silently did nothing, or
 * was never called; anything beyond it is staked on the partition and must wait
 * for the bench.  `.data`, `.dtcm_bss` and the MSP stack are all inside the
 * guarantee, and the ASSERT at the foot of this file is what keeps them there —
 * so a future addition that would quietly cross the line fails the link with a
 * message naming this paragraph instead of failing on a bench.
 *
 * NOT a fourth statement of the partition (it is not the partition), so
 * np_app_link_agreement_tests checks it differently: it asserts this is <= the
 * established DTCM and equal to the SDK's m_data, rather than equal to a bank
 * count. */
NP_DTCM_GUARANTEED_SIZE = 128K;

MEMORY
{
    DTCM    (rwx) : ORIGIN = 0x20000000,         LENGTH = NP_FLEXRAM_DTCM_SIZE

    /* The bootloader's application staging reservation, as an image region. */
    /* rwx, not rx: .bss lives here too — see the .bss section below for the  */
    /* measurement that put it there.                                          */
    STAGING (rwx) : ORIGIN = NP_APP_IMAGE_ORIGIN, LENGTH = NP_APP_IMAGE_LENGTH
}

ENTRY(Reset_Handler)

SECTIONS
{
    /* ── Vector table — MUST be at offset 0 of the image (VTOR target) ────── */
    /* ALIGN is the region origin's own alignment; stated so a future move of  */
    /* NP_APP_LOAD_OFFSET to a non-1KiB boundary fails here rather than at     */
    /* the first interrupt.  ARMv7-M requires VTOR aligned to the table size   */
    /* rounded up to a power of two; 16 + 240 entries is 1024 bytes.          */
    .isr_vector : ALIGN(1024) {
        __isr_vector_start = .;
        KEEP(*(.isr_vector))
        __isr_vector_end = .;
    } > STAGING

    .text : ALIGN(4) {
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)
        KEEP(*(.init))
        KEEP(*(.fini))
    } > STAGING

    /* Init/fini arrays.  Nothing in this image calls __libc_init_array (the   */
    /* startup enters main() directly — see __START in the application         */
    /* CMakeLists), so these are kept only so the symbols resolve if a         */
    /* toolchain emits references; they are expected to be empty.              */
    .ARM.exidx : { *(.ARM.exidx* .gnu.linkonce.armexidx.*) } > STAGING

    /* ── Build-class marker ───────────────────────────────────────────────── */
    /* A string in the image saying what this image is.  np_application is a   */
    /* bring-up artifact: its platform layer is firmware/platform/, which is   */
    /* traps rather than drivers (NP-SW-CI-001 §4.8).  The marker exists so    */
    /* the statement travels with the binary and can be checked by `strings`   */
    /* on a file that has been separated from its build log.                   */
    /* It sits BEFORE __etext, inside the loadable region: a section placed    */
    /* after .data would be given an LMA that overlaps .data's own AT() LMA,   */
    /* and ld reports that as an overlap rather than moving it.                */
    .np_build_note : ALIGN(4) { KEEP(*(.np_build_note)) } > STAGING

    .preinit_array : { PROVIDE_HIDDEN(__preinit_array_start = .);
                       KEEP(*(.preinit_array*))
                       PROVIDE_HIDDEN(__preinit_array_end = .); } > STAGING
    .init_array    : { PROVIDE_HIDDEN(__init_array_start = .);
                       KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array*))
                       PROVIDE_HIDDEN(__init_array_end = .); } > STAGING
    .fini_array    : { PROVIDE_HIDDEN(__fini_array_start = .);
                       KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array*))
                       PROVIDE_HIDDEN(__fini_array_end = .); } > STAGING

    /* ── Initialised data: runs in DTCM, ships inside the STAGING image ───── */
    /* AT> STAGING rather than AT(<address>): with the region form, ld tracks  */
    /* the load-address counter itself, so .bss below starts after .data's     */
    /* LMA instead of on top of it.  The explicit-address form produced        */
    /* exactly that overlap and ld reported it rather than moving anything.    */
    .data : ALIGN(4) {
        __data_start__ = .;
        *(.data)
        *(.data.*)
        . = ALIGN(4);
        __data_end__ = .;
    } > DTCM AT> STAGING

    /* The SDK Reset_Handler copies .data from __etext.  LOADADDR() is the     */
    /* address ld actually chose, so this cannot drift from the AT> above.     */
    __etext = LOADADDR(.data);

    /* ── DTCM-resident zero-initialised data ──────────────────────────────── */
    /*                                                                         */
    /* NP-SW-CI-001 §4.11 (closes OI-SWCI-42).  Holds ucHeap — the heap_4 pool */
    /* every FreeRTOS task stack is cut from — and nothing else today.         */
    /*                                                                         */
    /* NOLOAD, so it costs region space and no image bytes, exactly as .bss    */
    /* does.  It is a SEPARATE output section from .bss rather than a slice of */
    /* it because the two are in different memory regions and ld's            */
    /* __bss_start__/__bss_end__ pair has to stay contiguous: the vendored SDK */
    /* startup zeroes that one span and only that one span.                    */
    /*                                                                         */
    /* WHICH MEANS THIS REGION IS NOT ZEROED BY THE STARTUP.  It is zeroed by  */
    /* np_app_dtcm_bss_clear(), the first statement of main().  The bounds     */
    /* below are the symbols that function reads; renaming them without        */
    /* changing src/np_app_dtcm.c breaks the LINK rather than the device,      */
    /* which is the property worth having.                                     */
    /*                                                                         */
    /* Why ucHeap and not the rest of .bss: the argument is in np_app_dtcm.c   */
    /* and §4.11.  In short — it is the one buffer whose placement can be      */
    /* argued without a profiler, because every task stack in the system comes */
    /* out of it, and that is a property of the calling convention rather than */
    /* an assumption about the workload.  The rest of .bss genuinely does need */
    /* the bench, and stays where §4.10.6 put it.                              */
    .dtcm_bss (NOLOAD) : ALIGN(8) {
        __dtcm_bss_start__ = .;
        *(.dtcm_bss)
        *(.dtcm_bss.*)
        . = ALIGN(4);
        __dtcm_bss_end__ = .;
    } > DTCM

    /* ── .lpsdr4: declared by code, homed by nothing until now ────────────── */
    /*                                                                         */
    /* NP-SW-CI-001 §4.11.2.  np_hd_session.c places s_source_power (9,788 B)  */
    /* in a section called ".lpsdr4" and its comment said the name "names a    */
    /* region in the ARM linker script".  It did not.  No linker script in     */
    /* this repository declared .lpsdr4, or any SDRAM region, or any address   */
    /* at 0x80000000 — and nothing configures the SEMC controller that would   */
    /* make external SDRAM answer at all.                                      */
    /*                                                                         */
    /* It was invisible because --gc-sections drops it: np_hd_session is       */
    /* unreachable from the entry point today.  Measured with --no-gc-sections */
    /* (2026-09-03, arm-none-eabi-gcc 13.2.1), ld's orphan placement put       */
    /* .lpsdr4 at VMA 0x20000554 — inside DTCM, immediately after .data — with */
    /* an LMA inside the staging image.  Nothing would ever have copied it:    */
    /* Reset_Handler copies only __etext..__data_end__, and __STARTUP_CLEAR_BSS */
    /* zeroes only __bss_start__..__bss_end__.  A buffer annotated "put me in  */
    /* the 32 MB external SDRAM" would have silently become 9,788 B of DTCM    */
    /* holding whatever was there before, the moment sLORETA became reachable. */
    /*                                                                         */
    /* Giving it an output section is what stops it being an orphan; the       */
    /* ASSERT below is what stops the section being a place for the problem to */
    /* hide.  Empty today and the link is unaffected.  Non-empty and the link  */
    /* FAILS, naming the decision that has to be made first.  Deliberately no  */
    /* KEEP(): keeping it would defeat the --gc-sections that makes it empty,  */
    /* and fire the assert on a buffer nothing uses.                           */
    .lpsdr4 (NOLOAD) : {
        *(.lpsdr4)
        *(.lpsdr4.*)
    } > DTCM

    /* ── Zero-initialised data, incl. the FreeRTOS heap_4 pool ─────────────── */
    /*                                                                         */
    /* In STAGING, not DTCM, and that is a MEASURED placement rather than a    */
    /* preference.  SW-02's static footprint with every module linked is       */
    /* 162,440 B of .data + .bss (measured 2026-09-01, arm-none-eabi-gcc       */
    /* 13.2.1, -O2 -ffunction-sections -fdata-sections --gc-sections).  The    */
    /* MCUX SDK declares 128 KiB of DTCM for this part, so .bss in DTCM        */
    /* overflows the region by 31,368 B and does not link.  The four largest   */
    /* contributors are ucHeap (65,536 — configTOTAL_HEAP_SIZE), HRV's         */
    /* s_session_pool (51,804), and np_module_map's s_map + s_nvram_scratch    */
    /* (22,536 + 22,412).                                                      */
    /*                                                                         */
    /* Putting it here needs no assumption that is not already in the          */
    /* bootloader's map: the application owns the whole 440 KiB reservation    */
    /* once it is running — the bootloader has jumped and is dead — and the    */
    /* loadable image is only ~48 KiB of it.  .bss is NOLOAD, so it costs      */
    /* region space and no image bytes.                                        */
    /*                                                                         */
    /* ── Why it STAYS here now that DTCM is 512 KiB (NP-SW-CI-001 §4.10) ───── */
    /*                                                                         */
    /* The paragraph above used to end "enlarging DTCM means configuring the   */
    /* FlexRAM partition that NOTHING in this repository configures            */
    /* (OI-SWCI-39)".  That blocker is gone: the partition is established, DTCM */
    /* is 524,288 B, and 162,436 B of .data + .bss would fit it three times     */
    /* over.  The placement is nonetheless UNCHANGED, deliberately, and the     */
    /* reason is bring-up risk rather than arithmetic.                          */
    /*                                                                         */
    /* DTCM exists only if three register writes to silicon nobody has yet run  */
    /* do what the reference manual says.  With .bss here, a partition that     */
    /* does not take effect costs .data and the MSP stack — and fails loudly    */
    /* and immediately, inside Reset_Handler, before FreeRTOS starts.  With     */
    /* .bss there, the same failure would also take ucHeap, every task stack    */
    /* cut from it, and every static buffer in the image, and would present as  */
    /* arbitrary corruption at an arbitrary later moment.  Staking the whole    */
    /* data image on an untested register write, on the first board, to recover */
    /* single-cycle access to buffers nobody has profiled, is the wrong trade   */
    /* in that order.                                                          */
    /*                                                                         */
    /* What it costs is real and is recorded rather than hidden: .bss in OCRAM2 */
    /* is not tightly coupled, so every access goes through the bus and the    */
    /* L1 D-cache instead of DTCM's single-cycle path.  Which buffers move —   */
    /* and it is now "which", not "which few", since rationing is no longer    */
    /* forced — remains OI-SWCI-42, and what gates it is now one bench         */
    /* confirmation that the partition took effect, not a decision nobody has  */
    /* made.  DTCM is left holding .data and the stack, which is why it reads  */
    /* as nearly empty.                                                        */
    .bss (NOLOAD) : ALIGN(32) {
        __bss_start__ = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
    } > STAGING

    /* ── Application MSP stack, at the TOP of DTCM ────────────────────────── */
    /*                                                                         */
    /* The address is explicit rather than "wherever the location counter got  */
    /* to", and the difference is a guard band.  Left to follow .data, the     */
    /* stack would sit a few hundred bytes above it and the first overflow     */
    /* would land in initialised data — silent corruption of whichever         */
    /* variable happened to be there.  Pinned to the top of DTCM, the same     */
    /* overflow descends through ~503 KiB of unallocated DTCM before it can    */
    /* reach anything, which is the difference between a fault that is         */
    /* debuggable and one that is not.  (~119 KiB before §4.10 established the  */
    /* partition and DTCM grew from the SDK's default-fuse 128 KiB; the band   */
    /* is a consequence of the region size, not a target, so it moves with it.)*/
    /*                                                                         */
    /* This is the MSP: main() and every interrupt handler.  FreeRTOS task     */
    /* stacks come out of ucHeap, which §4.11 moved into .dtcm_bss — so task   */
    /* stacks are in DTCM now and this one still is.                           */
    /* configCHECK_FOR_STACK_OVERFLOW = 2 covers those and does NOT cover this */
    /* one, which is why the placement above is the protection it gets.        */
    /*                                                                         */
    /* NOTE this section is pinned OUTSIDE NP_DTCM_GUARANTEED_SIZE — at the    */
    /* top of the ESTABLISHED 512 KiB, not the default-fuse 128 KiB.  §4.10.6  */
    /* chose that deliberately and §4.11.3 keeps it, because it is what makes  */
    /* a failed partition a deterministic fault instead of a subtle one: the   */
    /* first `bl` in Reset_Handler pushes to an address that does not exist    */
    /* under the default partition, so the image dies at reset, before .data   */
    /* is copied and before any buffer is read.  That check is upstream of     */
    /* every other DTCM resident, which is why .dtcm_bss below it can be       */
    /* inside the guarantee without the guarantee having to carry the stack.   */
    .stack (ORIGIN(DTCM) + LENGTH(DTCM) - NP_APP_STACK_SIZE) (NOLOAD) : {
        __StackLimit = .;
        . += NP_APP_STACK_SIZE;
        __StackTop = .;
    } > DTCM

    /DISCARD/ : {
        *(.ARM.extab*)
        *(.eh_frame*)
        *(.comment)
    }

    /* ── Checks the link enforces, so nothing here is only a comment ──────── */

    /* (3) above: the vector table is the first byte of the image. */
    ASSERT(__isr_vector_start == NP_APP_IMAGE_ORIGIN,
           "ERROR: .isr_vector is not at the base of the staging image — SCB->VTOR would point at the wrong table")

    /* (2) above: what the bootloader will refuse, refuse here instead.  The   */
    /* MEMORY block already errors on STAGING overflow, but that counts .bss   */
    /* too — and .bss is NOT part of the image the bootloader copies.  This is */
    /* the bound that actually matters to load_and_jump(): the LOADABLE bytes, */
    /* which end at .data's load address plus its size.                        */
    __image_end = LOADADDR(.data) + SIZEOF(.data);

    ASSERT((__image_end - NP_APP_IMAGE_ORIGIN) <= NP_APP_IMAGE_LENGTH,
           "ERROR: loadable image exceeds the bootloader's staging reservation (440 KiB)")

    /* .bss must begin at or after the end of the loadable image.  If it did   */
    /* not, the bootloader's copy would land on top of it — harmlessly today,  */
    /* because Reset_Handler copies .data out before zeroing .bss, but only    */
    /* by accident of ordering.  Assert the property rather than rely on it.   */
    ASSERT(__bss_start__ >= __image_end,
           "ERROR: .bss overlaps the loadable image — the bootloader's copy would land inside it")

    /* With the stack pinned to the top of DTCM, a region overflow would NOT   */
    /* be reported for .data growing into it — ld would simply place both and  */
    /* they would overlap.  Assert the separation directly instead.            */
    ASSERT(__StackTop == ORIGIN(DTCM) + LENGTH(DTCM),
           "ERROR: the MSP stack is not at the top of DTCM")
    ASSERT(__data_end__ <= __StackLimit,
           "ERROR: .data has grown into the MSP stack — DTCM is over-subscribed")

    /* ── §4.11 (OI-SWCI-42): the DTCM data residents stay inside the DTCM ──── */
    /* that exists without any register write.                                 */
    /*                                                                         */
    /* .data and .dtcm_bss grow upward from ORIGIN(DTCM) and grow with the     */
    /* codebase; the MSP stack is one pinned 8 KiB object at the top with one  */
    /* owner.  Bounding the growable residents by the DEFAULT-eFuse 128 KiB    */
    /* keeps the partition's role down to "where the stack sits", so falling   */
    /* back to the default partition would be a one-line change of the stack   */
    /* address rather than a re-plan of the data layout.  Without this, the    */
    /* only bound on .dtcm_bss is the established 512 KiB, and a buffer moved  */
    /* into DTCM at, say, 200 KiB would be staked on the partition with        */
    /* nothing saying so.                                                      */
    ASSERT(__dtcm_bss_end__ <= ORIGIN(DTCM) + NP_DTCM_GUARANTEED_SIZE,
           "ERROR: .data + .dtcm_bss exceed the 128 KiB DTCM guaranteed by the DEFAULT eFuse partition — this data would exist only if np_flexram_apply_partition() took effect (NP-SW-CI-001 section 4.11)")

    /* .dtcm_bss must not collide with the pinned stack.  Implied by the       */
    /* guarantee assert while the stack is at 512 KiB, and NOT implied if the  */
    /* stack is ever moved down to the top of the guarantee — so it is stated  */
    /* rather than inferred.                                                   */
    ASSERT(__dtcm_bss_end__ <= __StackLimit,
           "ERROR: .dtcm_bss has grown into the MSP stack")

    /* .dtcm_bss is NOT zeroed by the vendored startup, which clears only      */
    /* __bss_start__..__bss_end__.  np_app_dtcm_bss_clear() reads these two    */
    /* symbols; if the section were ever emptied to nothing the function would */
    /* still be correct (start == end, no iterations), so the check that       */
    /* matters is ordering, not emptiness.                                     */
    ASSERT(__dtcm_bss_start__ <= __dtcm_bss_end__,
           "ERROR: .dtcm_bss bounds are inverted — np_app_dtcm_bss_clear() would not zero the region")

    /* ── §4.11.2: .lpsdr4 has a section so it cannot orphan, and must be ───── */
    /* empty because no external SDRAM is established.                         */
    /*                                                                         */
    /* Nothing configures the SEMC controller, no linker script declares an    */
    /* SDRAM region, and no address at 0x80000000 is mapped.  Until one of     */
    /* those changes, code that asks for .lpsdr4 is asking for memory this     */
    /* image does not have, and the honest answer is to refuse to link rather  */
    /* than to place it somewhere plausible.                                   */
    ASSERT(SIZEOF(.lpsdr4) == 0,
           "ERROR: something was placed in .lpsdr4, but no external LPSDR4 SDRAM is established — nothing configures the SEMC controller and no linker script declares an SDRAM region. Give the data a real home (DTCM, .bss) or establish SDRAM first (NP-SW-CI-001 section 4.11.2)")
}
