/*
 * NeurOne Bootloader — Freestanding C Runtime Memory Primitives
 * Document: NP-SW-CI-001 Rev 2 §4.2 (Defect B), phase 1
 *
 * The bootloader links with -nostdlib -nostartfiles -nodefaultlibs, so no libc
 * is present.  A freestanding C implementation is nevertheless REQUIRED to
 * provide memcpy, memmove, memset and memcmp: the compiler emits calls to them
 * for aggregate assignment and array initialisation, and -ffreestanding /
 * -fno-builtin do not change that.  Those flags stop GCC assuming library
 * SEMANTICS for calls the programmer writes; they do not stop GCC emitting
 * these four.  Providing them is the obligation of a freestanding environment,
 * not a workaround for one.
 *
 * Only memcpy and memset are provided.  They are the only two the bootloader
 * currently leaves unresolved (19 and 7 references respectively, measured at
 * origin/main 5a05533).  memmove and memcmp are deliberately NOT implemented:
 * adding untested code to a Class B bootloader ahead of a demonstrated need is
 * worse than adding it when CI shows the need.
 *
 * The names below are the testable entry points.  In the cross build the
 * standard ABI names memcpy/memset are attached to these same addresses by
 * alias attributes in np_mem.c, so the function that ships and the function
 * the host tests exercise are one object, not two copies.
 */

#ifndef NP_MEM_H
#define NP_MEM_H

#include <stddef.h>

/*
 * Copy n bytes from src to dst and return dst.
 *
 * Makes no alignment assumption about either operand — the eMMC read path does
 * not guarantee one — and is safe for n == 0.
 *
 * UNDEFINED BEHAVIOUR if the regions overlap.  This is deliberately NOT a
 * memmove: silently tolerating overlap would hide the caller's bug rather than
 * surface it.  A caller that needs overlap handling is itself the defect.
 */
void *np_memcpy(void *dst, const void *src, size_t n);

/*
 * Write the low-order byte of c to each of the first n bytes of dst and return
 * dst.  c is converted to unsigned char per C99 7.21.6.1.  Safe for n == 0 and
 * makes no alignment assumption.
 */
void *np_memset(void *dst, int c, size_t n);

#endif /* NP_MEM_H */
