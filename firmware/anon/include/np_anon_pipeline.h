/*
 * NeurOne Research Anonymization — Atomic Pipeline Orchestrator
 * Document: NP-FW-EMMC-002 Rev A §D.5 (atomic pipeline) + §D.6 (power-loss)
 *
 * Sequences one anonymization run as an all-or-nothing transaction over the
 * encrypted Scratch partition:
 *
 *   1. np_anon_scratch_init()      generate per-run key; set in-progress flag
 *   2. stage_uhdr()                read UHDR → decrypt with UKMD → encrypted Scratch
 *   3. transform()                 k-anonymity / l-diversity / DP noise on Scratch
 *   4. validate()                  header-field + l-diversity check on the extract
 *   5. emit()                      encrypt extract w/ study pubkey → output queue
 *   6. np_anon_scratch_complete()  zero key, SANITIZE Scratch, clear flag
 *
 * The two safety invariants this module exists to guarantee (§D.5):
 *
 *   A. NO PARTIAL EXTRACT.  emit() (step 5, the ONLY step permitted to place
 *      bytes in the output queue) is reached only when stage → transform →
 *      validate all succeed.  Any earlier failure aborts before emit is ever
 *      called, so a partially-processed extract can never reach the queue.
 *
 *   B. ALWAYS TEAR DOWN.  np_anon_scratch_complete() (key-zero + SANITIZE +
 *      clear NP_SNVS_ANON_IN_PROGRESS) runs on BOTH the success and the abort
 *      path, so a failed run never leaves a live key in SRAM or plaintext-
 *      derived ciphertext on the Scratch partition.  If power is lost before
 *      teardown, the flag stays set and the bootloader SANITIZEs on next boot
 *      (np_anon_scratch_resume_after_powerloss()).
 *
 * The transform stages themselves (UHDR decrypt, k-anonymity, DP noise, extract
 * encryption) live in other modules (NP-FW-ANON-001, uhdr_key, edf).  They are
 * injected as step callbacks so the orchestrator's atomicity contract can be
 * built and verified independently of those modules.
 *
 * No heap.  The caller owns the np_anon_scratch_ctx_t; the orchestrator drives
 * its init/complete internally — the caller must NOT wrap this call in its own
 * init/complete.
 */

#ifndef NP_ANON_PIPELINE_H
#define NP_ANON_PIPELINE_H

#include "np_anon_types.h"
#include "np_anon_scratch.h"

/* ── Pipeline step identity (for user notification) ───────────────────────────
 *
 * On abort the run reports which step failed so the app can surface an accurate
 * "anonymization failed, please retry" message (§D.5).  NP_ANON_STEP_NONE means
 * every step succeeded.
 *
 * PRIVACY (SHDR routing): this step identity is app-/UHDR-side control flow — it
 * drives the user's retry prompt.  Do NOT write per-device step-level failure
 * counts to SHDR fleet telemetry.  NP_ANON_STEP_VALIDATE specifically means
 * "this device's data could not be k-anonymised / l-diversified"; a device-linked
 * count of validate failures weakly signals that the wearer is a re-identification
 * outlier (small anonymity set) — a health-adjacent inference under WA MHMD and
 * GDPR Art. 9.  If a device-health signal is needed in SHDR, log a coarse boolean
 * (anonymisation_failed) without the stage.  See CLAUDE.md §5.1 classification.
 */
typedef enum {
    NP_ANON_STEP_NONE      = 0,  /* run completed; extract emitted             */
    NP_ANON_STEP_INIT      = 1,  /* np_anon_scratch_init (key generation)      */
    NP_ANON_STEP_STAGE     = 2,  /* stage_uhdr                                 */
    NP_ANON_STEP_TRANSFORM = 3,  /* transform                                  */
    NP_ANON_STEP_VALIDATE  = 4,  /* validate                                   */
    NP_ANON_STEP_EMIT      = 5   /* emit                                       */
} np_anon_step_t;

/* ── Step callbacks ───────────────────────────────────────────────────────────
 *
 * Each callback receives the live scratch context (use np_anon_scratch_write /
 * np_anon_scratch_read to stage and re-read encrypted blocks) and the caller's
 * opaque user pointer.  Each returns NP_ANON_OK to advance the pipeline or any
 * error code to abort the whole run.  A NULL callback is rejected before the
 * run starts (NP_ANON_ERR_INVALID) — every stage is mandatory.
 *
 * validate CONTRACT (privacy-critical): this orchestrator enforces step
 * ORDERING (validate runs before emit), NOT anonymisation SUFFICIENCY.  It does
 * not check that the extract meets any threshold.  validate MUST itself enforce
 * the NP-FW-ANON-001 thresholds (k>=10, l-diversity l>=3, DP epsilon<=1.0) and
 * confirm the study descriptor's HIPAA Expert-Determination signature, returning
 * a non-OK status if the extract must not be released.  A weak or stubbed
 * validate silently defeats the whole control while every orchestrator test
 * still passes — "pipeline ran" does NOT mean "properly anonymised".
 *
 * emit CONTRACT (safety-critical): emit is the sole step that writes to the
 * output queue and is called only after validate succeeds.  emit itself must be
 * internally atomic — it must append the whole extract or nothing, because once
 * the orchestrator returns there is no rollback of the output queue.  The
 * orchestrator guarantees emit is never *reached* on a bad extract; emit is
 * responsible for not leaving a half-written record if its own append fails
 * (stage-then-atomic-commit; an interrupted emit must leave zero visible bytes).
 */
typedef struct {
    np_anon_status_t (*stage_uhdr)(np_anon_scratch_ctx_t *ctx, void *user);
    np_anon_status_t (*transform)(np_anon_scratch_ctx_t *ctx, void *user);
    np_anon_status_t (*validate)(np_anon_scratch_ctx_t *ctx, void *user);
    np_anon_status_t (*emit)(np_anon_scratch_ctx_t *ctx, void *user);
    void *user;
} np_anon_pipeline_steps_t;

/*
 * np_anon_pipeline_run
 *   Execute the full §D.5 atomic pipeline against `ctx`.
 *
 *   ctx    — caller-owned scratch context (stack or static).  Initialised and
 *            completed internally; its prior contents are overwritten by init.
 *   steps  — the four mandatory step callbacks + user pointer.
 *   failed_step — optional out-param (may be NULL); on return, set to the step
 *            that failed, or NP_ANON_STEP_NONE on full success.
 *
 *   Behaviour:
 *     - If init fails, returns the init error WITHOUT running teardown (init
 *       leaves no live key and no in-progress flag on failure) and no step
 *       callback is invoked.
 *     - If any of stage/transform/validate fails, later steps (up to and
 *       including emit) are skipped, then teardown runs.
 *     - Teardown (np_anon_scratch_complete) runs after init succeeds, on both
 *       the success and the abort path.
 *
 *   Returns NP_ANON_OK only when every step succeeded and the extract was
 *   emitted; otherwise returns the failing step's status.  Returns
 *   NP_ANON_ERR_INVALID (without touching ctx) if ctx, steps, or any step
 *   callback pointer is NULL.
 */
np_anon_status_t np_anon_pipeline_run(np_anon_scratch_ctx_t *ctx,
                                      const np_anon_pipeline_steps_t *steps,
                                      np_anon_step_t *failed_step);

#endif /* NP_ANON_PIPELINE_H */
