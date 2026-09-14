/*
 * np_lfs_config.h — NeurOne build-time configuration for vendored littlefs
 * Document: NP-SOUP-LFS-001 Rev 2 §7.3; SOUP record firmware/vendor/littlefs/VERSION
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * This file is NOT part of the vendored component.  It is first-party NeurOne
 * code, owned by the hub control program on the FreeRTOSConfig.h precedent: one
 * owner, one source of truth, and the vendored directory stays a byte-exact
 * copy of an upstream tag.
 *
 * ── How it is reached ─────────────────────────────────────────────────────────
 * lfs_util.h includes whatever LFS_DEFINES names, before it defines anything of
 * its own.  The build therefore sets exactly ONE compile definition:
 *
 *     -DLFS_DEFINES=np_lfs_config.h
 *
 * and everything below is set here rather than on the command line.  That is a
 * correctness property, not tidiness: LFS_THREADSAFE adds two members to
 * struct lfs_config, so a translation unit built without it would disagree with
 * lfs.c about the LAYOUT of a struct they pass to each other — a disagreement
 * that links cleanly and then reads the wrong fields.  With the defines in a
 * header that lfs.h itself pulls in, a caller cannot see a different
 * configuration from the one the library was compiled with.
 *
 * ── The four decisions, and what each one costs ───────────────────────────────
 * Every choice here is recorded in firmware/vendor/littlefs/VERSION under
 * "Configuration", which is the SOUP-record half of the same statement.
 * np_lfs_config_tests.c asserts that this file and that record agree.
 */

#ifndef NP_LFS_CONFIG_H
#define NP_LFS_CONFIG_H

/* ── 1. Static allocation ──────────────────────────────────────────────────────
 * The main processor's heap is a fixed 64 KiB FreeRTOS pool (FreeRTOSConfig.h,
 * configTOTAL_HEAP_SIZE), sized for task and queue allocation at startup.
 * Filesystem caches must not come out of it, and a filesystem that can fail to
 * allocate at run time is a filesystem that can fail to mount at an arbitrary
 * later moment.
 *
 * With LFS_NO_MALLOC, lfs_malloc() returns NULL unconditionally.  Every buffer
 * must therefore be supplied by the caller in struct lfs_config — and a caller
 * that forgets one gets LFS_ERR_NOMEM from lfs_mount(), immediately and every
 * time, rather than a working filesystem on borrowed heap that fails in the
 * field.  np_lfs_instance.c supplies the three; np_lfs_config_validate()
 * refuses a config that does not.
 *
 * Consequence for callers: lfs_file_open() cannot be used, because it allocates
 * the per-file cache.  Use lfs_file_opencfg() with a caller-owned buffer of
 * NP_LFS_FILE_BUFFER_SIZE bytes.  This is a real constraint on the OI-LOG-05..07
 * glue and is stated here rather than discovered there.
 */
#define LFS_NO_MALLOC

/* ── 2. Assertions stay ON, retargeted ─────────────────────────────────────────
 * Assertions are the half of littlefs that checks littlefs.  lfs_init()'s
 * configuration checks — cache_size a factor of block_size, block_cycles != 0,
 * the four block-device callbacks non-NULL — ARE assertions, and compiling a
 * Class B filesystem with its own internal invariants disabled turns a
 * misconfiguration from a halt into undefined behaviour over stored values that
 * other code later trusts.  So LFS_ASSERT is redefined here rather than removed.
 *
 * On target the handler halts the main processor.  That is the established
 * NeurOne fault philosophy (NP-FW-HUB-001 §2, CLAUDE.md §4.2): halting stops the
 * SPI heartbeat, the STM32G071 safety MCU's 1.5 s watchdog fires, and all
 * stimulation is cut within <50 ms by hardware the main processor does not own.
 * It is the same discipline as np_freertos_assert_failed().
 *
 * On the host (NPTEST_HOST) it prints and abort()s, so a failure surfaces to
 * ctest instead of spinning a test runner forever.
 *
 * ── LFS_NO_ASSERT is deliberately NOT defined, and that is not the obvious
 *    choice.  Found by building it. ──────────────────────────────────────────
 * It looks like the tidy pairing: LFS_NO_ASSERT suppresses lfs_util.h's
 * <assert.h> include, and LFS_ASSERT above replaces what it disables.  It does
 * not work, because upstream treats LFS_NO_ASSERT as "assertions are compiled
 * out" rather than as "do not include <assert.h>":
 *
 *     lfs.c:507   #ifndef LFS_NO_ASSERT
 *                 static bool lfs_mlist_isopen(...)
 *     lfs.c:6178  LFS_ASSERT(!lfs_mlist_isopen(lfs->mlist, ...));
 *
 * Setting both leaves the CALL and removes the DEFINITION: an implicit
 * declaration at compile time and an undefined symbol at link — and only at
 * link, and only once something pulls lfs.o in, which is exactly the kind of
 * defect that surfaces a phase later.  Patching the vendored file is not
 * available (NP-SW-CI-001 §9: byte-exact SOUP), so the configuration yields.
 *
 * The cost is one unused <assert.h> include.  Nothing calls assert(), because
 * LFS_ASSERT above is defined before lfs_util.h tests for it.
 *
 * And the assertion this keeps alive is not an arbitrary one.  lfs_mlist_isopen
 * is the check that a file is not already open through a second handle — which
 * is precisely the condition behind the data-corruption defect that v2.11.3
 * fixes (upstream 488e84bb; see firmware/vendor/littlefs/VERSION).  Compiling it
 * out would remove the run-time detector for the failure the pinned version was
 * chosen for.
 */

void np_lfs_assert_failed(const char *file, unsigned long line);

#define LFS_ASSERT(test) \
    do { if (!(test)) { np_lfs_assert_failed(__FILE__, (unsigned long)__LINE__); } } while (0)

/* ── 3. No logging of any kind ─────────────────────────────────────────────────
 * CLAUDE.md §17: firmware renders no text at all — the device speaks in tones,
 * LEDs and numeric status, and the app does the wording.  A printf() here would
 * be dead code on a target with no stdio, and a locale boundary the firmware
 * deliberately does not have.  These three also remove lfs_util.h's <stdio.h>
 * include entirely.
 *
 * LFS_TRACE is off by default (it needs LFS_YES_TRACE) and is deliberately not
 * enabled: the trace macros print every argument of every API call, which on
 * this device would put file names and offsets — UHDR-adjacent material — into
 * a text stream that has no defined destination.
 */
#define LFS_NO_DEBUG
#define LFS_NO_WARN
#define LFS_NO_ERROR

/* ── 4. Thread safety ON ───────────────────────────────────────────────────────
 * SW-02 is a FreeRTOS system, and the Config instance already has three
 * independent writers specified against it — np_module_map's "NPMP" blob, Map 3's
 * journal (NP-FW-NVRAM-001 §4.2) and ukmd.rec (NP-FW-EMMC-002 §C.3, D-22) —
 * whose callers are not one task.  littlefs is not internally serialised; with
 * LFS_THREADSAFE it calls cfg->lock / cfg->unlock around every public entry
 * point and the caller supplies a mutex.
 *
 * The alternative is to leave it off and assume single-task access.  That
 * assumption would be recorded nowhere, checkable nowhere, and false the first
 * time the log writer and a Config update ran on different tasks — which is the
 * shape of failure this whole SOUP record exists because of.
 *
 * The cost is one mutex take/give per API call and two pointers in lfs_config.
 * The risk it introduces is specific and is handled: with LFS_THREADSAFE a NULL
 * cfg->lock is a call through a null pointer on the FIRST API call, so
 * np_lfs_config_validate() refuses a config whose lock or unlock is NULL.  That
 * check is the difference between a rejected mount and a hard fault.
 */
#define LFS_THREADSAFE

/* ── Deliberately NOT defined ──────────────────────────────────────────────────
 * Named so a later reader does not read an omission as an oversight.
 *
 *   LFS_MULTIVERSION   would let NeurOne write an older on-disk format.  Every
 *                      instance is formatted and read by this same pinned
 *                      library; no foreign or downgraded media exists.  Adding
 *                      it would make the on-disk version a run-time variable,
 *                      which is a compatibility surface nothing needs.
 *   LFS_READONLY       every NeurOne instance is written.
 *   LFS_NO_INTRINSICS  the GCC builtins (ctz, popcount) are wanted on Cortex-M7;
 *                      the fallbacks exist for debugging.
 *   LFS_MALLOC/LFS_FREE  no allocator is substituted — see decision 1.
 *   LFS_CRC            lfs_util.c's table-driven lfs_crc() is used as vendored.
 *                      Redirecting it at firmware/crypto's CRC-32 would put a
 *                      first-party function inside the component's integrity
 *                      path for no gain, and would make the vendored subset's
 *                      "verified by deletion" claim for lfs_util.c false.
 */

/* lfs_util.h pulls these in for itself immediately after including this file, so
 * they are not needed by littlefs.  They are here so that this header is
 * self-contained when included directly — which np_lfs_instance.h and
 * np_lfs_config_tests.c both do, to read the configuration without dragging in
 * the whole filesystem. */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#endif /* NP_LFS_CONFIG_H */
