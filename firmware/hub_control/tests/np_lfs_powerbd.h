/*
 * np_lfs_powerbd.h — NeurOne power-loss-injecting test block device
 * Document: NP-SOUP-LFS-001 Rev 3 §7.2, §12 (closes OI-LFS-02)
 * SW item:  SW-02 host test support — IEC 62304 Class B
 *
 * HOST TEST CODE ONLY.  Nothing here is compiled into any target image; it has
 * no counterpart on the device and no entry in the SW-02 module inventory.
 *
 * ── Why NeurOne has its own, when upstream ships three ────────────────────────
 *
 * Upstream's bd/lfs_emubd.* does this job and is deliberately NOT vendored
 * (firmware/vendor/littlefs/VERSION, "Intentionally NOT vendored").  The reason
 * is recorded there and it is not a preference: the SOUP verification cell this
 * whole record replaced cited upstream's own test suite as NeurOne's
 * verification.  Vendoring that suite — emubd included — would make the citation
 * look discharged while nothing NeurOne wrote had been run.  OI-LFS-02 asks for
 * a NEURONE power-loss test, so the injector is NeurOne's too.
 *
 * The second reason is that emubd models a raw NOR/NAND part and NeurOne's
 * medium is an eMMC behind an XTS layer (NP-FW-EMMC-002 §C).  Neither device
 * models that.  What this one models is the CONTRACT in struct lfs_config —
 * read/prog/erase/sync with prog_size granularity — because that contract is
 * what claims L-1…L-4 are stated against.  Whether the eMMC + XTS stack honours
 * the contract is a different question, it is not answerable on a host, and it
 * is carried as OI-LFS-07 rather than silently folded in here.
 *
 * ── What "power loss" means here ──────────────────────────────────────────────
 *
 * A power loss is not an error return.  An error return leaves the caller
 * running, and littlefs would handle it; a power loss stops the processor
 * mid-instruction and nothing after it executes.  So the injector applies the
 * partial physical effect of the interrupted operation and then longjmp()s out
 * of littlefs entirely, back to the test.  The media array survives (it is the
 * eMMC); every byte of littlefs's RAM state does not (that is the reboot).
 *
 * Three tear models, because picking one would be picking the convenient one:
 *
 *   NP_POWERBD_TEAR_NONE     the operation did not reach the medium at all
 *   NP_POWERBD_TEAR_PARTIAL  a prefix of it landed, and the program page that
 *                            was in flight reads back indeterminate — modelled
 *                            as a 0x5A fill, not as erased 0xFF, because a
 *                            half-programmed page is not a blank one and an
 *                            0xFF fill would quietly make every torn write look
 *                            like a write that never started
 *   NP_POWERBD_TEAR_FULL     the operation completed and power was lost
 *                            immediately after it
 *
 * ── Ordering ──────────────────────────────────────────────────────────────────
 *
 * prog, erase and sync each consume one op index, in call order.  read does not:
 * a read leaves nothing behind, so cutting during one is indistinguishable from
 * cutting just before it.  Sweeping cut_at across [0, ops_in_an_uncut_run) is
 * therefore a sweep across every point at which this device could have changed
 * the medium — which is the coverage claim the suite makes, and the reason
 * np_powerbd_ops_seen() is public.
 */

#ifndef NP_LFS_POWERBD_H
#define NP_LFS_POWERBD_H

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>

#include "lfs.h"

typedef enum {
    NP_POWERBD_TEAR_NONE = 0,
    NP_POWERBD_TEAR_PARTIAL,
    NP_POWERBD_TEAR_FULL,
    NP_POWERBD_TEAR_MODEL_COUNT
} np_powerbd_tear_t;

typedef struct {
    /* The medium.  Caller-owned, and it is what survives a cut. */
    uint8_t    *media;
    lfs_size_t  block_size;
    lfs_size_t  block_count;
    lfs_size_t  prog_size;

    /* Injection state, reset by np_powerbd_power_cycle(). */
    long        ops_seen;   /* prog + erase + sync calls since the last cycle  */
    long        cut_at;     /* op index to cut at, or NP_POWERBD_NO_CUT        */
    np_powerbd_tear_t tear;
    bool        cut_fired;
    jmp_buf    *resume;     /* where the processor "stops" — see the header    */

    /* One bit per block, set by every prog and erase that reaches the medium —
     * torn ones included.  It exists so a sweep can restore its pre-state by
     * copying back only what the last attempt touched: the Config instance is
     * 16 MiB and a sweep runs thousands of attempts, so a whole-image restore
     * would make the run time a property of the partition size rather than of
     * the work under test. */
    uint8_t    *dirty;
    lfs_size_t  dirty_bytes;

    /* Counters, for the falsification cases.  Never reset by a power cycle:
     * a suite that never programmed anything must be able to say so. */
    long        total_progs;
    long        total_erases;
    long        total_syncs;
    long        total_reads;
    long        total_cuts;
} np_powerbd_t;

#define NP_POWERBD_NO_CUT  (-1L)

/*
 * Bind `bd` to `media` (block_size * block_count bytes, caller-owned) and fill
 * the four block-device callbacks and `context` of `cfg`.  Everything else in
 * `cfg` belongs to np_lfs_config_apply() and is not touched here — the instance
 * under test must be the EMMC-FS-01 instance, not a geometry invented for the
 * convenience of the test.
 *
 * Does NOT erase the medium; see np_powerbd_wipe().
 */
void np_powerbd_bind(np_powerbd_t *bd, struct lfs_config *cfg,
                     uint8_t *media, lfs_size_t block_size,
                     lfs_size_t block_count, lfs_size_t prog_size,
                     uint8_t *dirty, lfs_size_t dirty_bytes);

/* True if `block` has been programmed or erased since the last
 * np_powerbd_dirty_clear().  See np_powerbd_t::dirty. */
bool np_powerbd_block_dirty(const np_powerbd_t *bd, lfs_size_t block);

/* Forget which blocks have been touched.  Call it immediately after restoring
 * a pre-state, never before one. */
void np_powerbd_dirty_clear(np_powerbd_t *bd);

/* Set every byte of the medium to 0xFF — a factory-erased part. */
void np_powerbd_wipe(np_powerbd_t *bd);

/*
 * Reboot: clear the op counter and disarm the injector.  The medium is
 * untouched, which is the whole point of calling it.
 */
void np_powerbd_power_cycle(np_powerbd_t *bd);

/*
 * Arm the injector.  On the `cut_at`-th op after arming (0-based, counting prog,
 * erase and sync), apply `tear` and longjmp to *resume with value 1.
 * cut_at = NP_POWERBD_NO_CUT arms nothing and the run completes normally.
 */
void np_powerbd_arm(np_powerbd_t *bd, long cut_at, np_powerbd_tear_t tear,
                    jmp_buf *resume);

/* Ops consumed since the last power cycle. */
long np_powerbd_ops_seen(const np_powerbd_t *bd);

/* Did the armed cut actually fire?  A sweep in which this is never true has
 * tested nothing, and the suite asserts on it rather than trusting it. */
bool np_powerbd_cut_fired(const np_powerbd_t *bd);

/* Human-readable tear model name, for failure messages. */
const char *np_powerbd_tear_name(np_powerbd_tear_t tear);

#endif /* NP_LFS_POWERBD_H */
