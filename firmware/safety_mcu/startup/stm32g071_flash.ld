/*
 * NeurOne Safety MCU — Linker Script
 * Target: STM32G071KB (128 KB Flash, 36 KB SRAM)
 * Document: NP-SW-001 Rev A — SW-01 Class C
 *
 * Memory map:
 *   Flash  0x08000000  128 KB  — code + read-only data
 *   SRAM   0x20000000   36 KB  — stack + data + BSS + fault latch region
 *
 * The fault latch (SW01-M08) occupies the top 64 bytes of SRAM.
 * This region survives a warm reset (power-on reset clears it).
 */

ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 128K
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 36K
}

/* Top of stack = top of SRAM (minus fault latch region) */
_estack = ORIGIN(SRAM) + LENGTH(SRAM) - 64;

/* Fault latch occupies the top 64 bytes; warm-reset-persistent */
_fault_latch_start = ORIGIN(SRAM) + LENGTH(SRAM) - 64;
_fault_latch_end   = ORIGIN(SRAM) + LENGTH(SRAM);

SECTIONS
{
    /* ── Interrupt vector table ─────────────────────────────────────────── */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

    /* ── Code ───────────────────────────────────────────────────────────── */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        _etext = .;
    } >FLASH

    /* ── Read-only data ─────────────────────────────────────────────────── */
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } >FLASH

    /* ARM exception tables */
    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
    .ARM        : {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } >FLASH

    /* ── Initialised data (LMA in Flash, VMA in SRAM) ───────────────────── */
    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } >SRAM AT> FLASH

    /* ── Zero-initialised data ──────────────────────────────────────────── */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } >SRAM

    /* ── Fault latch (top 64 bytes, warm-reset-persistent) ──────────────── */
    /* Pinned to _fault_latch_start so it is not placed after .bss by default.
     * Without the explicit address the linker would place it immediately after
     * .bss (mid-SRAM), not at the protected top-64-byte region.  The NOLOAD
     * attribute means startup code does NOT zero this section on warm reset —
     * fault data survives until power-on reset.
     *
     * ── PHASE 7: THE ADDRESS MOVED TO THE OUTPUT-SECTION LINE ─────────────
     * This section previously pinned itself with `. = _fault_latch_start;`
     * INSIDE the braces.  That form does not link.  Assigning the location
     * counter an absolute address inside an output section that also has a
     * `>SRAM` region makes ld compute the section's SIZE from the location
     * counter delta, so a 64-byte region read as ~0x20000008 bytes:
     *
     *   ld: section `.fault_latch' will not fit in region `SRAM'
     *   ld: region `SRAM' overflowed by 536870984 bytes
     *
     * This was never observed because nothing had ever linked: Defect E
     * (NP-SW-CI-001 §4.4) stopped the image at 24 undefined np_hal_* symbols,
     * upstream of the linker script.  Supplying the platform layer exposed a
     * SECOND, independent link blocker sitting behind the first — the image
     * would not have linked even with all 25 symbols defined.
     *
     * The fix is the idiomatic form: the address goes on the output-section
     * line, where it sets the section's VMA without touching the size
     * calculation.  PLACEMENT AND SEMANTICS ARE UNCHANGED — same address, same
     * NOLOAD, same KEEP, same region.  Verified by `nm` on the linked image:
     * s_latch lands at 0x20008FC0 = ORIGIN(SRAM) + LENGTH(SRAM) - 64.        */
    .fault_latch _fault_latch_start (NOLOAD) :
    {
        . = ALIGN(4);
        KEEP(*(.fault_latch))
    } >SRAM

    /* ── Stack headroom assertion ───────────────────────────────────────── */
    /* The MEMORY block catches a section that overflows SRAM outright, but it
     * cannot catch .data + .bss growing INTO the descending stack — both fit
     * the region right up until the moment the stack corrupts .bss at runtime.
     * On a Class C part with 36 KB and a Monocypher SHA-512 context on the
     * stack, that margin is worth asserting at link time rather than
     * discovering as a hard fault.  Fails the link, which fails the CI job. */
    _stack_min_size = 4K;
    ASSERT(_ebss <= (_estack - _stack_min_size),
           "SRAM exhausted: .data + .bss leave less than 4K for the stack")

    /* ── Discard ────────────────────────────────────────────────────────── */
    /DISCARD/ :
    {
        libc.a(*)
        libm.a(*)
        libgcc.a(*)
    }

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
