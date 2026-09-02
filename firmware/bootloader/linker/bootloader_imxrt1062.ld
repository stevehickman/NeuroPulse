/*
 * NeurOne Bootloader Linker Script
 * Target: NXP i.MX RT1062 (Cortex-M7)
 * Document: NP-FW-EMMC-001 Rev A §8.1
 *
 * Memory layout:
 *   - Bootloader is loaded by ROM into OCRAM at 0x20200000
 *   - Total bootloader image must be ≤ 48 KiB (ROM load limit)
 *   - Application firmware is staged above the bootloader (_app_staging_start)
 *   - Stack grows downward from top of OCRAM
 *
 * i.MX RT1062 OCRAM: 0x20200000 – 0x2027FFFF (512 KiB)
 *
 * ── Which OCRAM, and why the size is now exact ───────────────────────────────
 * NP-SW-CI-001 §4.10 (OI-SWCI-39).  This is OCRAM2, the DEDICATED 512 KiB array.
 * It is NOT part of FlexRAM, so no ITCM/DTCM/OCRAM partition can move it or
 * take bytes out of it — which is what makes it safe for a bootloader running
 * from here to set that partition (src/np_flexram.c, called first in
 * Bootloader_Reset).
 *
 * The MCUX SDK's linker script for this part states 768 KiB (0x000C0000) at
 * this same base, and that was read as a disagreement with the 512 KiB below
 * when OI-SWCI-39 was raised.  It is not one: the SDK figure is the
 * DEFAULT-FUSE composite of OCRAM2 (512 KiB, here) plus the 256 KiB of FlexRAM
 * that the default partition maps immediately above it at 0x20280000.  The two
 * numbers never described the same memory.  NeurOne's partition gives FlexRAM
 * zero OCRAM banks, so OCRAM2 is the only OCRAM and this LENGTH is exact rather
 * than conservative.
 *
 * eMMC boot partition structure (ROM reads from here):
 *   Offset 0x000: IVT  (32 bytes)
 *   Offset 0x020: Boot data (16 bytes)
 *   Offset 0x030: Vector table (.np_vectors)
 *   Offset 0x200: Bootloader code + data
 */

MEMORY {
    /* OCRAM2 — 512 KiB: bootloader (low) + app staging (mid) + stack (high).
     * Exactly ONE region is declared, and that is load-bearing rather than
     * incidental: it is what makes "nothing of the bootloader's lives in a
     * FlexRAM bank" a property of this script instead of a promise in a
     * comment, and therefore what makes np_flexram_apply_partition() safe to
     * call from code that is running.  Adding a second region here without
     * revisiting src/np_flexram.c would break that argument silently. */
    OCRAM (rwx) : ORIGIN = 0x20200000, LENGTH = 512K
}

/* ── FlexRAM partition — NP-SW-CI-001 §4.10, closes OI-SWCI-39 ─────────────────
 *
 * FlexRAM is a SEPARATE 512 KiB array from the OCRAM2 region above: 16 banks of
 * 32 KiB, each assignable to ITCM, DTCM or the FlexRAM OCRAM aperture at
 * 0x20280000.  Nothing in this repository configured it until phase 9; these
 * symbols are the declaration, and src/np_flexram.c is the code that puts it in
 * force at B-0 of the boot sequence.
 *
 * They are restated in np_config.h (which the code writes to the registers from)
 * and NP_FLEXRAM_DTCM_SIZE in app_imxrt1062.ld (which the application is linked
 * against).  A linker script can neither include another one nor read a C
 * header, so — exactly as for the staging reservation in §4.3 — the duplication
 * is unavoidable and the answer is to make disagreement fail a test:
 * np_bootloader_flexram_tests and np_app_link_agreement_tests read this file,
 * the application's and np_config.h, and assert all three agree.
 */
NP_FLEXRAM_BANK_SIZE   = 32K;
NP_FLEXRAM_BANK_COUNT  = 16;
NP_FLEXRAM_ITCM_BANKS  = 0;
NP_FLEXRAM_DTCM_BANKS  = 16;
NP_FLEXRAM_OCRAM_BANKS = 0;

/* Stack at top of OCRAM */
_stack_top  = ORIGIN(OCRAM) + LENGTH(OCRAM);
_stack_size = 8K;
_stack_base = _stack_top - _stack_size;

/* Bootloader maximum image size (48 KiB): enforced by ASSERT below */
_bl_max_size = 48K;

/* ── Application staging area — the single source of truth ────────────────────
 * NP-SW-CI-001 §4.3 (Defect C), phase 2.
 *
 * The application is copied out of its eMMC bank into this region and executed
 * in place (np_main.c load_and_jump(); the XIP-from-eMMC alternative is
 * explicitly unimplemented).  So this is a hard ceiling on application image
 * size, not a scratch buffer.
 *
 * _app_load_offset is where staging begins: the bootloader's 48 KiB ROM load
 * limit rounded up to the next 64 KiB boundary, which also leaves room for
 * .usb_qh.  It is used BOTH as the .app_staging alignment and in the size
 * derivation below, so the offset exists once.
 *
 * The size is DERIVED from the two boundaries that actually bound the region.
 * It was previously the literal 448K, written inline as "512 - 64 = 448",
 * which omitted the 8 KiB stack this same script reserves at the top of OCRAM
 * — the region over-subscribed by exactly that 8 KiB and the bootloader did
 * not link.  Writing 440K here instead would fix today's arithmetic and fail
 * again the moment _stack_size or the offset moves.  Nothing below is a
 * literal, so there is no arithmetic left to get wrong:
 *
 *     512K (LENGTH) - 64K (_app_load_offset) - 8K (_stack_size) = 440K
 *
 * OCRAM is then 100.00% allocated by construction — bootloader, staging and
 * stack exactly fill it.  ld errors on OVERFLOW, so 100.00% links cleanly;
 * zero headroom is the intent, not a near miss.
 *
 * np_main.c imports _app_staging_start and _app_staging_size and derives both
 * its load destination and its runtime image-size ceiling from them, rather
 * than recomputing the same arithmetic in C.  That duplicate C computation was
 * the second, silent half of Defect C: it accepted images up to 448 KiB and
 * copied them over the live stack.  See src/np_app_image.c.
 */
_app_load_offset  = 64K;
_app_staging_size = LENGTH(OCRAM) - _app_load_offset - _stack_size;

ENTRY(Bootloader_Reset)

SECTIONS {

    /* ── IVT at base of load address ──────────────────────────────────────── */
    .np_ivt : {
        KEEP(*(.np_ivt))
    } > OCRAM

    /* ── Boot data immediately after IVT ─────────────────────────────────── */
    .np_boot_data : {
        KEEP(*(.np_boot_data))
    } > OCRAM

    /* ── Minimal vector table ─────────────────────────────────────────────── */
    .np_vectors : {
        KEEP(*(.np_vectors))
    } > OCRAM

    /* ── Bootloader code ──────────────────────────────────────────────────── */
    .text : ALIGN(4) {
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
    } > OCRAM

    /* ── Read-write data ──────────────────────────────────────────────────── */
    .data : ALIGN(4) {
        *(.data)
        *(.data.*)
    } > OCRAM

    /* ── Zero-initialised BSS ─────────────────────────────────────────────── */
    .bss (NOLOAD) : ALIGN(4) {
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        _bss_end = .;
    } > OCRAM

    /* End of bootloader image */
    _bootloader_end = ALIGN(4);

    /* USB endpoint queue heads and transfer descriptors (2 KiB aligned) */
    .usb_qh (NOLOAD) : ALIGN(2048) {
        *(.usb_qh*)
    } > OCRAM

    /* Application firmware staging area (see _app_staging_size above).      */
    /* Both bounds are exported: np_main.c loads the image to               */
    /* _app_staging_start and rejects anything larger than _app_staging_size.*/
    .app_staging (NOLOAD) : ALIGN(_app_load_offset) {
        _app_staging_start = .;
        . += _app_staging_size;
        _app_staging_end   = .;
    } > OCRAM

    /* Stack at top of OCRAM (not placed in output, just verified) */
    .stack (NOLOAD) : ALIGN(8) {
        . += _stack_size;
    } > OCRAM

    /* ── Size checks ─────────────────────────────────────────────────────── */
    ASSERT((_bootloader_end - ORIGIN(OCRAM)) <= _bl_max_size,
           "ERROR: Bootloader image exceeds 48 KiB limit")

    /* The OCRAM map is exactly three regions and they must tile it with no  */
    /* gap and no overlap.  These two turn that from a comment into a law    */
    /* the link enforces (NP-SW-CI-001 §4.3).                               */

    /* Staging must begin at the declared offset.  If the bootloader plus    */
    /* .usb_qh ever grows past _app_load_offset, ALIGN() pushes staging to   */
    /* the NEXT 64 KiB boundary and the region overflows — this fires first, */
    /* and says why, instead of leaving a bare "overflowed by N bytes".      */
    ASSERT(_app_staging_start == ORIGIN(OCRAM) + _app_load_offset,
           "ERROR: .app_staging is not at _app_load_offset — bootloader + .usb_qh outgrew it")

    /* Staging must end exactly where the stack begins.  Under-reserving     */
    /* wastes OCRAM silently; over-reserving is Defect C — the staging area  */
    /* running into the stack the bootloader is executing on.               */
    ASSERT(_app_staging_end == _stack_base,
           "ERROR: .app_staging does not end at _stack_base — OCRAM over- or under-subscribed")

    /* ── FlexRAM partition checks (NP-SW-CI-001 §4.10) ─────────────────────── */

    /* Every bank is assigned.  A partition that leaves banks out is not        */
    /* invalid silicon-wise, but it is a decision to shrink the device's RAM,   */
    /* and it must not be reachable by editing one number and no other.         */
    ASSERT(NP_FLEXRAM_ITCM_BANKS + NP_FLEXRAM_DTCM_BANKS +
           NP_FLEXRAM_OCRAM_BANKS == NP_FLEXRAM_BANK_COUNT,
           "ERROR: FlexRAM banks do not sum to NP_FLEXRAM_BANK_COUNT — some of the array would be unassigned")

    /* This script's OCRAM region is OCRAM2 only.  If a FlexRAM OCRAM bank is   */
    /* ever assigned, the aperture at 0x20280000 becomes live and the map this  */
    /* script describes stops being the whole of OCRAM — at which point the     */
    /* region above, np_config.h and np_app_image.c all need revisiting         */
    /* together, which is what this ASSERT forces.                              */
    ASSERT(NP_FLEXRAM_OCRAM_BANKS == 0,
           "ERROR: FlexRAM OCRAM banks are no longer zero — the OCRAM region above describes OCRAM2 alone")

    /* The application links .data and its MSP stack into DTCM and its          */
    /* Reset_Handler touches both before its first C statement.  A partition    */
    /* with no DTCM produces an image that faults before main().                */
    ASSERT(NP_FLEXRAM_DTCM_BANKS > 0,
           "ERROR: FlexRAM DTCM banks is zero — the application's .data and MSP stack have nowhere to live")

    /* ── Discard C++ exception tables (not used) ─────────────────────────── */
    /DISCARD/ : {
        *(.ARM.exidx*)
        *(.ARM.extab*)
        *(.eh_frame*)
    }
}
