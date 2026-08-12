/*
 * NeurOne Bootloader — Freestanding C Runtime Memory Primitives
 * Document: NP-SW-CI-001 Rev 2 §4.2 (Defect B), phase 1
 *
 * Rationale for the whole file is in np_mem.h.  What follows is the part a
 * reader has to get right before touching this code.
 *
 * ── Why this translation unit is compiled specially ────────────────────────
 *
 * At -O2 GCC runs -ftree-loop-distribute-patterns, which recognises a naive
 * byte-copy or byte-fill loop and rewrites it into a call to memcpy/memset —
 * INCLUDING when that loop is the body of memcpy/memset itself.  The result
 * links cleanly, disassembles to something that looks reasonable at a glance,
 * and recurses until the 8 KiB bootloader stack is gone.
 *
 * Measured on this repo's toolchain (ARM GNU 14.2.1, cortex-m7 hard-float,
 * 2026-08-09), compiling exactly the loops below:
 *
 *     -O2                                   → emits a call to memset
 *     -O2 -fno-builtin                      → emits nothing
 *     -O2 -ffreestanding -fno-builtin       → emits nothing
 *
 * So on THIS compiler the bootloader's existing -fno-builtin already
 * suppresses the rewrite.  That is a GCC implementation detail — the pass
 * looks up an implicit builtin declaration that -fno-builtin withholds — and
 * it is not a property this file should depend on.  It also does not hold for
 * the host-test build, which is a different compiler at different flags.
 * np_mem.c is therefore compiled with -fno-tree-loop-distribute-patterns in
 * BOTH builds, applied per-source rather than to the whole target so that no
 * other bootloader code loses an optimisation it is entitled to.
 *
 * See firmware/bootloader/CMakeLists.txt and firmware/CMakeLists.txt for the
 * two places that flag is attached, and firmware/bootloader/tests for the
 * disassembly-based regression check.  A successful link proves nothing here:
 * verify with
 *     arm-none-eabi-objdump -d np_bootloader.elf | sed -n '/<memcpy>:/,/^$/p'
 * and confirm the body branches to neither memcpy nor memset.
 *
 * ── Why byte-at-a-time ──────────────────────────────────────────────────────
 *
 * This is the OTA bootloader.  load_and_jump() stages a firmware image that
 * has already passed its Ed25519 signature check; a subtly wrong copy here
 * corrupts the image AFTER verification, undetectably, and the device boots
 * it.  A word-at-a-time copy would need alignment classification, head/tail
 * peeling and a misaligned-operand path — three chances to be wrong — to save
 * time on copies that are, in this binary, at most 256 bytes (image header) or
 * 128 bytes (SHA-512 block).  The bulk image transfer does not come through
 * here at all; np_emmc_read() writes to the staging area directly.  The
 * simplest implementation that is obviously correct is the right trade.
 */

#include "np_mem.h"

void *np_memcpy(void *dst, const void *src, size_t n)
{
    unsigned char       *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

    while (n > 0U) {
        *d = *s;
        d++;
        s++;
        n--;
    }

    return dst;
}

void *np_memset(void *dst, int c, size_t n)
{
    unsigned char      *d = (unsigned char *)dst;
    const unsigned char v = (unsigned char)c;   /* C99 7.21.6.1 conversion */

    while (n > 0U) {
        *d = v;
        d++;
        n--;
    }

    return dst;
}

#if !defined(NP_MEM_HOST_TEST)
/*
 * The ABI names the compiler emits calls to, aliased onto the definitions
 * above so both names resolve to one address — the shipped function and the
 * tested function are the same object.
 *
 * Excluded from the host-test build for two reasons: defining memcpy/memset
 * there would collide with the host libc that the test uses as its oracle,
 * and Mach-O supports only weak aliases, so this attribute does not compile on
 * macOS at all.  The host tests call np_memcpy/np_memset directly, which has
 * the additional benefit that the host compiler cannot substitute a builtin
 * for the call and quietly test its own libc instead of this code.
 *
 * The declarations carry the exact standard prototypes, restrict included, so
 * they stay consistent with GCC's builtin knowledge (-Wbuiltin-declaration-
 * mismatch) and so callers are held to the no-overlap contract at every call
 * site.  restrict is a top-level parameter qualifier and is ignored for type
 * compatibility, so the definitions above need not repeat it.
 *
 * DO NOT add `memmove` here as an alias of np_memcpy to clear a future link
 * error.  memmove must tolerate overlap and np_memcpy does not; aliasing them
 * would convert a loud, safe undefined-symbol failure into silent corruption
 * of a firmware image that has ALREADY passed its signature check.  If memmove
 * becomes unresolved, write memmove.
 */
extern void *memcpy(void *restrict dst, const void *restrict src, size_t n)
    __attribute__((alias("np_memcpy")));

extern void *memset(void *dst, int c, size_t n)
    __attribute__((alias("np_memset")));
#endif /* !NP_MEM_HOST_TEST */
