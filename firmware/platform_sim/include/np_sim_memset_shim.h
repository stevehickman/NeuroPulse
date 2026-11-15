/*
 * Force-included on np_uhdr_key.c alone (NP-SIM-FW-001 §34, OI-SIMFW-52).  HOST ONLY.
 *
 * np_uhdr_key.c defines its own `static void memset_explicit` (the device's toolchain has none).  C23 and recent glibc declare a function of that name in
 * <string.h> (`void *memset_explicit(void *, int, size_t)`), and the twin compiles with _GNU_SOURCE, so on a host whose C library has it the two
 * conflict: "conflicting types for memset_explicit".  This header includes <string.h> first, so the library's declaration is already in, and
 * then renames the device's definition (a function-like macro, which is the only form the file uses), so the file's own calls and its own definition agree and the library's function is untouched.  The device
 * source is not edited (REQ-SIMFW-03) and nothing outside this one file is renamed.
 */
#ifndef NP_SIM_MEMSET_SHIM_H
#define NP_SIM_MEMSET_SHIM_H

#include <string.h>

#define memset_explicit(dst, ch, len) np_uhdr_key_local_memset_explicit(dst, ch, len)

#endif
