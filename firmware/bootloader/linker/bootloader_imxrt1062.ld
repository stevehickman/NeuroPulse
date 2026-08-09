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
 * eMMC boot partition structure (ROM reads from here):
 *   Offset 0x000: IVT  (32 bytes)
 *   Offset 0x020: Boot data (16 bytes)
 *   Offset 0x030: Vector table (.np_vectors)
 *   Offset 0x200: Bootloader code + data
 */

MEMORY {
    /* OCRAM — 512 KiB: bootloader (low) + app staging (mid) + stack (high) */
    OCRAM (rwx) : ORIGIN = 0x20200000, LENGTH = 512K
}

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

    /* ── Discard C++ exception tables (not used) ─────────────────────────── */
    /DISCARD/ : {
        *(.ARM.exidx*)
        *(.ARM.extab*)
        *(.eh_frame*)
    }
}
