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
 * ── The two numbers below are duplicated, and the duplication is enforced ────
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

MEMORY
{
    /* Data Tightly Coupled Memory.  Length is the MCUX SDK's own declaration
     * for this part (m_data in MIMXRT1062xxxxx_flexspi_nor.ld, 0x20000000
     * LENGTH 0x00020000) rather than a NeurOne estimate.  See OI-SWCI-39:
     * nothing in this repository configures the FlexRAM partition that decides
     * how much DTCM actually exists, and the bootloader script's OCRAM figure
     * and the SDK's do not agree. */
    DTCM    (rwx) : ORIGIN = 0x20000000,         LENGTH = 128K

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
    /* region space and no image bytes.  Every other option needs a decision   */
    /* nobody has made: enlarging DTCM means configuring the FlexRAM partition */
    /* that NOTHING in this repository configures (OI-SWCI-39); shrinking      */
    /* configTOTAL_HEAP_SIZE or the pools is a product change made to fit a    */
    /* bring-up image, which is backwards.                                     */
    /*                                                                         */
    /* What it costs is real and is recorded rather than hidden: .bss in OCRAM */
    /* is not tightly coupled, so every access goes through the bus and the    */
    /* L1 D-cache instead of DTCM's single-cycle path.  Which buffers deserve  */
    /* DTCM is a partitioning decision for the platform layer to make with     */
    /* measurements behind it — OI-SWCI-42.  DTCM is left holding .data and    */
    /* the stack, which is why it now reads as nearly empty.                   */
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
    /* overflow descends through ~119 KiB of unallocated DTCM before it can    */
    /* reach anything, which is the difference between a fault that is         */
    /* debuggable and one that is not.                                         */
    /*                                                                         */
    /* This is the MSP: main() and every interrupt handler.  FreeRTOS task     */
    /* stacks come out of ucHeap, which is in .bss and therefore in OCRAM.     */
    /* configCHECK_FOR_STACK_OVERFLOW = 2 covers those and does NOT cover this */
    /* one, which is why the placement above is the protection it gets.        */
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
}
