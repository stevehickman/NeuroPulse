/*
 * NeurOne Research Anonymization — Atomic Pipeline Orchestrator
 * Document: NP-FW-EMMC-002 Rev A §D.5 (atomic pipeline) + §D.6 (power-loss)
 *
 * See np_anon_pipeline.h for the contract and the two safety invariants
 * (no partial extract; always tear down).  This file holds no secrets of its
 * own — all key material lives in the np_anon_scratch_ctx_t and is destroyed by
 * np_anon_scratch_complete(), which this orchestrator always calls once init
 * has succeeded.
 */

#include "np_anon_pipeline.h"

#include <stddef.h>

/* Record the failing step for the caller, tolerating a NULL out-param. */
static void set_failed_step(np_anon_step_t *out, np_anon_step_t step)
{
    if (out != NULL) {
        *out = step;
    }
}

np_anon_status_t np_anon_pipeline_run(np_anon_scratch_ctx_t *ctx,
                                      const np_anon_pipeline_steps_t *steps,
                                      np_anon_step_t *failed_step)
{
    /* Default the out-param before any early return. */
    set_failed_step(failed_step, NP_ANON_STEP_NONE);

    /* Every stage is mandatory — reject a partially-wired pipeline up front so
     * a missing callback can never silently skip (e.g.) validation. */
    if (ctx == NULL || steps == NULL ||
        steps->stage_uhdr == NULL || steps->transform == NULL ||
        steps->validate == NULL || steps->emit == NULL) {
        return NP_ANON_ERR_INVALID;
    }

    /* Step 1 (§D.5.1): generate the per-run key and set the in-progress flag.
     * On failure init leaves no live key and no flag set, so there is nothing
     * to tear down — return the error directly without calling complete(). */
    np_anon_status_t st = np_anon_scratch_init(ctx);
    if (st != NP_ANON_OK) {
        set_failed_step(failed_step, NP_ANON_STEP_INIT);
        return st;
    }

    /* Steps 2–5 (§D.5.2–.5): the ordered stage table, run under the in-progress
     * flag.  The loop stops at the first non-OK step, so a later stage — and
     * crucially emit, the sole output-queue writer — is entered only when every
     * earlier stage returned NP_ANON_OK.  That short-circuit is the mechanism
     * enforcing invariant A: no partial extract ever reaches the output queue.
     * The table order is the privacy contract; do not reorder.  fn and id are
     * co-located per row so the two can never drift out of sync. */
    const struct {
        np_anon_status_t (*fn)(np_anon_scratch_ctx_t *, void *);
        np_anon_step_t   id;
    } stages[] = {
        { steps->stage_uhdr, NP_ANON_STEP_STAGE     },
        { steps->transform,  NP_ANON_STEP_TRANSFORM },
        { steps->validate,   NP_ANON_STEP_VALIDATE  },
        { steps->emit,       NP_ANON_STEP_EMIT      },
    };

    for (size_t i = 0; i < sizeof(stages) / sizeof(stages[0]); i++) {
        st = stages[i].fn(ctx, steps->user);
        if (st != NP_ANON_OK) {
            set_failed_step(failed_step, stages[i].id);
            break;
        }
    }

    /* Step 6 (§D.5.6) / abort teardown — invariant B.  Runs on every path once
     * init has succeeded: zeroes the SRAM key, SANITIZEs the Scratch partition,
     * and clears NP_SNVS_ANON_IN_PROGRESS.  Best-effort and void — secret
     * destruction is unconditional even if SANITIZE reports an error. */
    np_anon_scratch_complete(ctx);

    return st;
}
