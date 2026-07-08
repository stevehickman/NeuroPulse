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
     * Without the explicit address assignment the linker would place it
     * immediately after .bss (mid-SRAM), not at the protected top-64-byte
     * region.  The NOLOAD attribute means startup code does NOT zero this
     * section on warm reset — fault data survives until power-on reset. */
    .fault_latch (NOLOAD) :
    {
        . = _fault_latch_start;  /* pin to top 64 bytes of SRAM */
        . = ALIGN(4);
        KEEP(*(.fault_latch))
    } >SRAM

    /* ── Discard ────────────────────────────────────────────────────────── */
    /DISCARD/ :
    {
        libc.a(*)
        libm.a(*)
        libgcc.a(*)
    }

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
