/*
 * np_lfs_instance.h — EMMC-FS-01 Config-partition littlefs instance
 * Document: NP-SOUP-LFS-001 Rev 2 §7.3 (claim L-5), NP-FW-EMMC-001 EMMC-FS-01,
 *           NP-FW-NVRAM-001 Rev 2 §3.3.1, §4
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * First-party NeurOne code; not part of the vendored component.
 *
 * ── What this file is for ─────────────────────────────────────────────────────
 * NP-SOUP-LFS-001 §7.3 requires that "the EMMC-FS-01 instance parameters (L-5)
 * [are] asserted at mount rather than assumed."  littlefs's own LFS_ASSERTs in
 * lfs_init() check that a configuration is INTERNALLY CONSISTENT — that
 * cache_size divides block_size, that block_cycles is not zero.  They cannot
 * check that it is NEURONE'S configuration, because they have never heard of
 * EMMC-FS-01.  A config with block_size 512 and block_count 32,768 passes every
 * one of them and mounts a filesystem whose geometry no NeurOne document
 * describes.
 *
 * np_lfs_config_validate() is that missing half.  It is the check L-5 names, and
 * the OI-LOG-05..07 mount glue must call it before lfs_mount().
 *
 * ── Which instance ────────────────────────────────────────────────────────────
 * The CONFIG partition only (partition 4, 16 MiB, NP_CONFIG_SIZE_LBA).  The
 * UHDR and SHDR instances are np_lfs_log_instance.h (OI-LFS-05, closed).
 * Nothing here may be reused for them by analogy: block_count alone differs by
 * three orders of magnitude, which changes what lookahead_size can mean.
 *
 * ── CORRECTION (NP-SOUP-LFS-001 Rev 4 §13.1.2) — read before the next block ──
 * This header was written believing EMMC-FS-01 states five Config parameters
 * and is silent on the rest.  It is not: NP-FW-EMMC-001 §5.2 states all twelve
 * for Config (and for UHDR and SHDR), including cache_size 512,
 * lookahead_size 64, block_cycles 200, name_max 64 and attr_max 256 — and the
 * values below differ on all five.  Separately, the specified read/prog 256 is
 * below the 512-byte XTS data unit (EMMC-UHDR-05), and under a read-modify-
 * write tear it loses committed data.  Both are OI-LFS-10.  The values are
 * left exactly as they were, because every Config result in NP-SOUP-LFS-001
 * §12 and §13 was obtained against them; changing them is OI-LFS-10's
 * decision, and CI re-runs both suites against whatever it decides.
 */

#ifndef NP_LFS_INSTANCE_H
#define NP_LFS_INSTANCE_H

#include "np_hub_types.h"
#include "lfs.h"

/* ── The five EMMC-FS-01 owns (claim L-5) ─────────────────────────────────────
 * These are NOT NeurOne's to choose.  Changing one is a change to
 * NP-FW-EMMC-001, raised as an ECR, not an edit here — and it relocates the
 * ukmd.rec record, whose loss makes a user's UHDR permanently unmountable with
 * no NeurOne-held second copy (NP-FW-NVRAM-001 §3.3.1.1).
 *
 * block_count 4,096 x block_size 4,096 = 16,777,216 B = the whole partition.
 * There is no room inside it for anything the filesystem does not own; that is
 * the arithmetic behind D-21 and the reason EMMC-CFG-02's raw-write region is
 * void.
 */
#define NP_LFS_CFG_READ_SIZE        256u
#define NP_LFS_CFG_PROG_SIZE        256u
#define NP_LFS_CFG_BLOCK_SIZE       4096u
#define NP_LFS_CFG_BLOCK_COUNT      4096u
#define NP_LFS_CFG_FILE_MAX         65536u

/* ── The four this header believed EMMC-FS-01 does not state — it does; see the
 *    CORRECTION above and OI-LFS-10.  The reasoning below is kept as written. ─
 *
 * cache_size 256.  Must be a multiple of read_size and prog_size and a factor of
 * block_size (lfs_init).  256 is the smallest value satisfying all three, and
 * size is what it costs: littlefs holds one read cache, one program cache, and
 * one cache per OPEN FILE, all of which are static here.  A larger cache buys
 * fewer block accesses on sequential reads that this device does not do — the
 * Config partition's traffic is a 14,012-byte blob and 32-byte journal appends.
 * It also bounds inline_max, so files at or below 256 B live in metadata: Map 3
 * records (32 B) and ukmd.rec (192 B) are both inlineable, which is the cheap
 * case for exactly the two the design writes most.
 *
 * lookahead_size 512.  The lookahead buffer is a bitmap, one bit per block, so
 * 512 B covers 4,096 blocks — the WHOLE partition in a single allocator pass.
 * The alternative (upstream's example uses 16 B) is repeated traversals during
 * allocation, i.e. more reads and a longer window in which a power loss lands
 * mid-allocation.  512 B of static RAM to make the allocator single-pass on a
 * partition this small is not a close call.
 *
 * block_cycles 500.  Set EXPLICITLY, as NP-SOUP-LFS-001 §7.3 requires, because
 * the two adjacent values are both wrong: 0 is rejected outright by lfs_init(),
 * and -1 DISABLES block-level wear levelling — which is claim L-6, the claim
 * EMMC-HW-01's 30,000 P/E budget is spent against.  500 is the midpoint of
 * upstream's suggested 100-1000: lower values move metadata more often (more
 * writes, more even wear), higher values fewer.  With 4,096 blocks and the write
 * volumes in NP-FW-NVRAM-001 §3.5, neither end of the range binds, so the
 * midpoint is chosen for having no argument against it rather than for a
 * calculation this data does not support.
 *
 * Static buffers.  LFS_NO_MALLOC is in force (np_lfs_config.h), so all three
 * buffers are supplied here from .bss.  1,024 B total, plus 256 B per open file
 * that the caller owns.
 */
#define NP_LFS_CFG_CACHE_SIZE       256u
#define NP_LFS_CFG_LOOKAHEAD_SIZE   512u
#define NP_LFS_CFG_BLOCK_CYCLES     500

/* Per-file cache size for lfs_file_opencfg().  LFS_NO_MALLOC means lfs_file_open()
 * cannot be used at all; every open must carry a caller-owned buffer of exactly
 * this size.  It is cache_size by definition, named separately so the glue reads
 * as the requirement rather than as a coincidence. */
#define NP_LFS_FILE_BUFFER_SIZE     NP_LFS_CFG_CACHE_SIZE

/* Total static RAM this instance costs, for the FlexRAM budget in
 * NP-SW-CI-001 §4.10: read cache + program cache + lookahead. */
#define NP_LFS_STATIC_RAM_BYTES \
    (NP_LFS_CFG_CACHE_SIZE + NP_LFS_CFG_CACHE_SIZE + NP_LFS_CFG_LOOKAHEAD_SIZE)

/*
 * Fill the EMMC-FS-01-owned fields of `cfg`, and point its three buffer members
 * at this module's static storage.
 *
 * What it deliberately does NOT touch, because none of it is this module's to
 * decide: context, read/prog/erase/sync (OI-LOG-05..07, the platform block-device
 * glue over the XTS-mounted partition) and lock/unlock (LFS_THREADSAFE — the
 * caller's mutex).  The caller sets those, then calls np_lfs_config_validate().
 *
 * Returns NP_HUB_ERR_INVALID_ARG if cfg is NULL, otherwise NP_HUB_OK.
 */
np_hub_status_t np_lfs_config_apply(struct lfs_config *cfg);

/*
 * The mount-time assertion NP-SOUP-LFS-001 §7.3 asks for.  Call it immediately
 * before lfs_mount()/lfs_format() and refuse to mount on anything but NP_HUB_OK.
 *
 * Checks, in this order:
 *   1. every EMMC-FS-01 parameter equals the value above    → L-5
 *   2. block_cycles is the value above, and in particular is neither 0 nor -1
 *   3. the three static buffers are non-NULL                → LFS_NO_MALLOC is
 *      actually satisfied rather than silently falling through to LFS_ERR_NOMEM
 *   4. read/prog/erase/sync are non-NULL                    → the glue is bound
 *   5. lock/unlock are non-NULL                             → LFS_THREADSAFE
 *      calls them on the first API call; NULL here is a hard fault, not an error
 *
 * Returns NP_HUB_OK, or NP_HUB_ERR_INVALID_ARG for a NULL cfg, or
 * NP_HUB_ERR_BAD_VERSION for a configuration that is not this one — "the media
 * geometry is not the geometry this firmware was specified against" is a version
 * disagreement, and it is reported as one rather than flattened into GENERIC.
 */
np_hub_status_t np_lfs_config_validate(const struct lfs_config *cfg);

#endif /* NP_LFS_INSTANCE_H */
