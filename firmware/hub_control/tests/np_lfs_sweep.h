/*
 * np_lfs_sweep.h — power-cut sweep harness for the Rev 4 littlefs suites
 * Document: NP-SOUP-LFS-001 Rev 4 §13
 * SW item:  SW-02 host test support — IEC 62304 Class B
 *
 * HOST TEST CODE ONLY.
 *
 * The same method as np_lfs_powerloss_tests.c §12 — an uncut run counts the
 * medium-touching ops, then the mutation is re-run once per op index per tear
 * model from a byte-identical pre-state, and every attempt is followed by a
 * reboot and a full verification.  It is a separate harness, not a refactor of
 * that file's, so §12's recorded results stay results about the code that
 * produced them.  What it adds is that the pre-state works over a SPARSE
 * medium too (np_powerbd_bind_sparse), which the log-instance geometry needs.
 */

#ifndef NP_LFS_SWEEP_H
#define NP_LFS_SWEEP_H

#include <stdbool.h>

#include "np_lfs_powerbd.h"

typedef struct {
    long ops;          /* medium-touching ops in the uncut run  */
    long attempts;
    long cuts;         /* cuts that actually fired              */
    long missed_cuts;  /* armed indices that did not fire       */
    long violations;   /* verifier findings                     */
} np_sweep_result_t;

/* Returns the number of violations it found after one cut. */
typedef int (*np_sweep_verify_fn)(const char *what, long cut_at,
                                  np_powerbd_tear_t tear);

/*
 * `reboot` is called after every cut and before every verification: it must
 * discard every piece of RAM state the code under test holds (lfs_t, lock
 * depth, the store's registry) — that is what a power loss does.
 */
void np_sweep_bind(np_powerbd_t *bd, void (*reboot)(void));

/* Baseline once from a wiped medium, snapshot, then sweep `mutate`. */
np_sweep_result_t np_sweep_run(void (*baseline)(void), void (*mutate)(void),
                               const char *what, np_sweep_verify_fn verify);

/* Release the snapshot.  Call once at the end of a suite. */
void np_sweep_release(void);

#endif /* NP_LFS_SWEEP_H */
