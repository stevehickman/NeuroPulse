/*
 * NeurOne Bootloader Linker Script
 * Target: NXP i.MX RT1062 (Cortex-M7)
 * Document: NP-FW-EMMC-001 Rev A §8.1
 *
 * Memory layout:
 *   - Bootloader is loaded by ROM into OCRAM at 0x20200000
 *   - Total bootloader image must be ≤ 48 KiB (ROM load limit)
 *   - Application firmware is loaded above the bootloader (NP_APP_LOAD_ADDR)
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

    /* Application firmware staging area starts at 64 KiB offset           */
    /* (NP_APP_LOAD_ADDR = NP_FW_LOAD_ADDR + 64 KiB = 0x20210000)         */
    .app_staging (NOLOAD) : ALIGN(65536) {
        /* Reserved for application firmware loaded from eMMC bank          */
        . += 448K;    /* 448 KiB for application (512 - 64 = 448 KiB)      */
    } > OCRAM

    /* Stack at top of OCRAM (not placed in output, just verified) */
    .stack (NOLOAD) : ALIGN(8) {
        . += _stack_size;
    } > OCRAM

    /* ── Size check ──────────────────────────────────────────────────────── */
    ASSERT((_bootloader_end - ORIGIN(OCRAM)) <= _bl_max_size,
           "ERROR: Bootloader image exceeds 48 KiB limit")

    /* ── Discard C++ exception tables (not used) ─────────────────────────── */
    /DISCARD/ : {
        *(.ARM.exidx*)
        *(.ARM.extab*)
        *(.eh_frame*)
    }
}
