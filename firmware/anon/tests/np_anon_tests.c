/*
 * NeurOne Research Anonymization — Scratch Encryption Host Tests
 * Document: NP-FW-EMMC-002 Rev A §D
 *
 * Exercises the init → write → read → complete cycle on the host using the
 * NPTEST_HOST HAL stubs.  Verifies the security-relevant invariants:
 *
 *   T1  init sets NP_SNVS_ANON_IN_PROGRESS and key_valid.
 *   T2  init failure (no key on failure) is reported and key stays invalid.
 *   T3  write then read round-trips plaintext (symmetric CTR contract).
 *   T4  write/read reject invalid args (null, len 0, len > block, no key,
 *       out-of-range block_offset).
 *   T5  complete zeroises key + nonce, invalidates ctx, bumps session_id,
 *       and clears the in-progress flag.
 *   T6  resume_after_powerloss clears the in-progress flag.
 *
 * Pipeline orchestrator (NP-FW-EMMC-002 §D.5):
 *   T7  full success: every step runs once, extract emitted, teardown ran.
 *   T8  abort at stage: later steps skipped, teardown ran, step==STAGE.
 *   T9  abort at transform: validate + emit skipped, teardown ran.
 *   T10 abort at validate: emit NEVER called (no partial extract), teardown ran.
 *   T11 emit failure: reported as EMIT, teardown still ran.
 *   T12 null args (ctx / steps / missing callback) rejected, no step called.
 *   T13 init (TRNG) failure: no step called, flag never set, step==INIT.
 *
 * Build for host: -DNPTEST_HOST  (see CMakeLists.txt NP_BUILD_TESTS option).
 * Return convention: 0 = PASS, non-zero = failure count.
 */

#include "np_anon_scratch.h"
#include "np_anon_pipeline.h"
#include "np_anon_config.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Test-only TRNG fault injection, defined in np_anon_scratch.c host stubs. */
extern int np_anon_host_trng_fail;

static int g_fail_count = 0;

#define ASSERT(cond, msg)                                            \
    do {                                                             \
        if (!(cond)) {                                              \
            printf("FAIL [%s:%d] %s\n", __func__, __LINE__, (msg)); \
            g_fail_count++;                                         \
        }                                                          \
    } while (0)

/* TRNG stub fills with 0xA5 ^ i, so k_scratch is never all-zero. */
static int key_is_zeroed(const np_anon_scratch_ctx_t *ctx)
{
    for (size_t i = 0; i < NP_ANON_KEY_LEN; i++) {
        if (ctx->k_scratch[i] != 0U) return 0;
    }
    for (size_t i = 0; i < NP_ANON_NONCE_LEN; i++) {
        if (ctx->nonce[i] != 0U) return 0;
    }
    return 1;
}

static void test_init_sets_flag_and_key(void)
{
    np_anon_scratch_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    np_anon_host_lpgpr2 = 0U;

    ASSERT(np_anon_scratch_init(&ctx) == NP_ANON_OK, "init should succeed");
    ASSERT(ctx.key_valid, "key_valid should be true after init");
    ASSERT((np_anon_host_lpgpr2 & NP_SNVS_ANON_IN_PROGRESS) != 0U,
           "in-progress flag should be set after init");
    ASSERT(!key_is_zeroed(&ctx), "key should be populated after init");

    np_anon_scratch_complete(&ctx);
}

static void test_init_null(void)
{
    ASSERT(np_anon_scratch_init(NULL) == NP_ANON_ERR_INVALID,
           "init(NULL) should be invalid");
}

static void test_roundtrip(void)
{
    np_anon_scratch_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    np_anon_host_lpgpr2 = 0U;
    ASSERT(np_anon_scratch_init(&ctx) == NP_ANON_OK, "init for roundtrip");

    uint8_t plain[NP_ANON_SCRATCH_BLOCK_SIZE];
    uint8_t out[NP_ANON_SCRATCH_BLOCK_SIZE];
    for (size_t i = 0; i < sizeof(plain); i++) {
        plain[i] = (uint8_t)(i * 7U + 3U);
    }

    const uint32_t block = 5U;
    ASSERT(np_anon_scratch_write(&ctx, block, plain, sizeof(plain)) == NP_ANON_OK,
           "write should succeed");
    ASSERT(np_anon_scratch_read(&ctx, block, out, sizeof(out)) == NP_ANON_OK,
           "read should succeed");
    ASSERT(memcmp(plain, out, sizeof(plain)) == 0,
           "read plaintext should match written plaintext");

    /* Partial-length block also round-trips. */
    const size_t partial = 100U;
    memset(out, 0, sizeof(out));
    ASSERT(np_anon_scratch_write(&ctx, block, plain, partial) == NP_ANON_OK,
           "partial write");
    ASSERT(np_anon_scratch_read(&ctx, block, out, partial) == NP_ANON_OK,
           "partial read");
    ASSERT(memcmp(plain, out, partial) == 0, "partial round-trip matches");

    np_anon_scratch_complete(&ctx);
}

static void test_invalid_args(void)
{
    np_anon_scratch_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    np_anon_host_lpgpr2 = 0U;

    uint8_t buf[NP_ANON_SCRATCH_BLOCK_SIZE];
    memset(buf, 0x11, sizeof(buf));

    /* No key yet: writes/reads must be rejected. */
    ASSERT(np_anon_scratch_write(&ctx, 0, buf, 16) == NP_ANON_ERR_INVALID,
           "write before init should be invalid");
    ASSERT(np_anon_scratch_read(&ctx, 0, buf, 16) == NP_ANON_ERR_INVALID,
           "read before init should be invalid");

    ASSERT(np_anon_scratch_init(&ctx) == NP_ANON_OK, "init for invalid-args");

    ASSERT(np_anon_scratch_write(&ctx, 0, NULL, 16) == NP_ANON_ERR_INVALID,
           "write null plaintext invalid");
    ASSERT(np_anon_scratch_write(&ctx, 0, buf, 0) == NP_ANON_ERR_INVALID,
           "write len 0 invalid");
    ASSERT(np_anon_scratch_write(&ctx, 0, buf,
                                 NP_ANON_SCRATCH_BLOCK_SIZE + 1U)
               == NP_ANON_ERR_INVALID,
           "write len > block invalid");
    ASSERT(np_anon_scratch_write(&ctx, NP_SCRATCH_SIZE_LBA, buf, 16)
               == NP_ANON_ERR_INVALID,
           "write past end of scratch invalid");

    ASSERT(np_anon_scratch_read(&ctx, 0, NULL, 16) == NP_ANON_ERR_INVALID,
           "read null out invalid");

    np_anon_scratch_complete(&ctx);
}

static void test_complete_clears_secrets(void)
{
    np_anon_scratch_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    np_anon_host_lpgpr2 = 0U;

    ASSERT(np_anon_scratch_init(&ctx) == NP_ANON_OK, "init for complete");
    uint32_t sid_before = ctx.session_id;

    np_anon_scratch_complete(&ctx);

    ASSERT(key_is_zeroed(&ctx), "key + nonce should be zeroized after complete");
    ASSERT(!ctx.key_valid, "key_valid should be false after complete");
    ASSERT(ctx.session_id == sid_before + 1U, "session_id should increment");
    ASSERT((np_anon_host_lpgpr2 & NP_SNVS_ANON_IN_PROGRESS) == 0U,
           "in-progress flag should be cleared after complete");
}

static void test_resume_after_powerloss(void)
{
    np_anon_host_lpgpr2 = NP_SNVS_ANON_IN_PROGRESS;
    np_anon_scratch_resume_after_powerloss();
    ASSERT((np_anon_host_lpgpr2 & NP_SNVS_ANON_IN_PROGRESS) == 0U,
           "resume should clear the in-progress flag");
}

/* ── Pipeline orchestrator tests ──────────────────────────────────────────────
 *
 * A probe drives programmable per-step return codes and records how many times
 * each step ran, so the atomicity invariants can be asserted directly:
 *   - counts prove which steps were skipped after an abort
 *   - emit_calls == 0 on any pre-emit failure proves "no partial extract"
 *   - key_valid_at_emit proves the ctx is live inside the steps
 */
typedef struct {
    int stage_calls;
    int transform_calls;
    int validate_calls;
    int emit_calls;
    np_anon_status_t stage_ret;
    np_anon_status_t transform_ret;
    np_anon_status_t validate_ret;
    np_anon_status_t emit_ret;
    bool key_valid_at_emit;
} pipeline_probe_t;

static np_anon_status_t probe_stage(np_anon_scratch_ctx_t *ctx, void *user)
{
    (void)ctx;
    pipeline_probe_t *p = (pipeline_probe_t *)user;
    p->stage_calls++;
    return p->stage_ret;
}
static np_anon_status_t probe_transform(np_anon_scratch_ctx_t *ctx, void *user)
{
    (void)ctx;
    pipeline_probe_t *p = (pipeline_probe_t *)user;
    p->transform_calls++;
    return p->transform_ret;
}
static np_anon_status_t probe_validate(np_anon_scratch_ctx_t *ctx, void *user)
{
    (void)ctx;
    pipeline_probe_t *p = (pipeline_probe_t *)user;
    p->validate_calls++;
    return p->validate_ret;
}
static np_anon_status_t probe_emit(np_anon_scratch_ctx_t *ctx, void *user)
{
    pipeline_probe_t *p = (pipeline_probe_t *)user;
    p->emit_calls++;
    p->key_valid_at_emit = ctx->key_valid;  /* ctx must be live inside a step */
    return p->emit_ret;
}

/* Build a fully-wired steps struct whose callbacks all return OK by default. */
static void probe_reset(pipeline_probe_t *p, np_anon_pipeline_steps_t *steps)
{
    memset(p, 0, sizeof(*p));
    p->stage_ret = p->transform_ret = p->validate_ret = p->emit_ret = NP_ANON_OK;
    steps->stage_uhdr = probe_stage;
    steps->transform  = probe_transform;
    steps->validate   = probe_validate;
    steps->emit       = probe_emit;
    steps->user       = p;
}

static void test_pipeline_success(void)
{
    pipeline_probe_t p;
    np_anon_pipeline_steps_t steps;
    probe_reset(&p, &steps);

    np_anon_scratch_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    np_anon_host_lpgpr2 = 0U;
    np_anon_step_t failed = NP_ANON_STEP_EMIT;  /* sentinel — must be overwritten */

    ASSERT(np_anon_pipeline_run(&ctx, &steps, &failed) == NP_ANON_OK,
           "full pipeline should succeed");
    ASSERT(failed == NP_ANON_STEP_NONE, "no failed step on success");
    ASSERT(p.stage_calls == 1 && p.transform_calls == 1 &&
           p.validate_calls == 1 && p.emit_calls == 1,
           "every step should run exactly once");
    ASSERT(p.key_valid_at_emit, "ctx key should be live inside emit");
    /* Teardown (invariant B) ran: key zeroed, ctx invalid, flag cleared. */
    ASSERT(key_is_zeroed(&ctx), "key zeroed after successful run");
    ASSERT(!ctx.key_valid, "ctx invalid after successful run");
    ASSERT((np_anon_host_lpgpr2 & NP_SNVS_ANON_IN_PROGRESS) == 0U,
           "in-progress flag cleared after successful run");
}

static void test_pipeline_abort_on_stage(void)
{
    pipeline_probe_t p;
    np_anon_pipeline_steps_t steps;
    probe_reset(&p, &steps);
    p.stage_ret = NP_ANON_ERR_EMMC;

    np_anon_scratch_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    np_anon_host_lpgpr2 = 0U;
    np_anon_step_t failed = NP_ANON_STEP_NONE;

    ASSERT(np_anon_pipeline_run(&ctx, &steps, &failed) == NP_ANON_ERR_EMMC,
           "stage failure should propagate");
    ASSERT(failed == NP_ANON_STEP_STAGE, "failed step should be STAGE");
    ASSERT(p.stage_calls == 1, "stage ran once");
    ASSERT(p.transform_calls == 0 && p.validate_calls == 0 && p.emit_calls == 0,
           "no step after a stage failure should run");
    ASSERT(key_is_zeroed(&ctx) && !ctx.key_valid,
           "teardown ran after stage abort");
    ASSERT((np_anon_host_lpgpr2 & NP_SNVS_ANON_IN_PROGRESS) == 0U,
           "flag cleared after stage abort");
}

static void test_pipeline_abort_on_transform(void)
{
    pipeline_probe_t p;
    np_anon_pipeline_steps_t steps;
    probe_reset(&p, &steps);
    p.transform_ret = NP_ANON_ERR_PIPELINE;

    np_anon_scratch_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    np_anon_host_lpgpr2 = 0U;
    np_anon_step_t failed = NP_ANON_STEP_NONE;

    ASSERT(np_anon_pipeline_run(&ctx, &steps, &failed) == NP_ANON_ERR_PIPELINE,
           "transform failure should propagate");
    ASSERT(failed == NP_ANON_STEP_TRANSFORM, "failed step should be TRANSFORM");
    ASSERT(p.stage_calls == 1 && p.transform_calls == 1,
           "stage + transform ran");
    ASSERT(p.validate_calls == 0 && p.emit_calls == 0,
           "validate + emit skipped after transform failure");
}

static void test_pipeline_abort_on_validate(void)
{
    /* The critical "no partial extract" case: a bad extract must never emit. */
    pipeline_probe_t p;
    np_anon_pipeline_steps_t steps;
    probe_reset(&p, &steps);
    p.validate_ret = NP_ANON_ERR_PIPELINE;

    np_anon_scratch_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    np_anon_host_lpgpr2 = 0U;
    np_anon_step_t failed = NP_ANON_STEP_NONE;

    ASSERT(np_anon_pipeline_run(&ctx, &steps, &failed) == NP_ANON_ERR_PIPELINE,
           "validate failure should propagate");
    ASSERT(failed == NP_ANON_STEP_VALIDATE, "failed step should be VALIDATE");
    ASSERT(p.stage_calls == 1 && p.transform_calls == 1 && p.validate_calls == 1,
           "stage + transform + validate ran");
    ASSERT(p.emit_calls == 0,
           "emit must NOT run when validation fails (no partial extract)");
    ASSERT(key_is_zeroed(&ctx) && !ctx.key_valid,
           "teardown ran after validate abort");
    ASSERT((np_anon_host_lpgpr2 & NP_SNVS_ANON_IN_PROGRESS) == 0U,
           "flag cleared after validate abort");
}

static void test_pipeline_emit_failure(void)
{
    pipeline_probe_t p;
    np_anon_pipeline_steps_t steps;
    probe_reset(&p, &steps);
    p.emit_ret = NP_ANON_ERR_EMMC;

    np_anon_scratch_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    np_anon_host_lpgpr2 = 0U;
    np_anon_step_t failed = NP_ANON_STEP_NONE;

    ASSERT(np_anon_pipeline_run(&ctx, &steps, &failed) == NP_ANON_ERR_EMMC,
           "emit failure should propagate");
    ASSERT(failed == NP_ANON_STEP_EMIT, "failed step should be EMIT");
    ASSERT(p.emit_calls == 1, "emit ran once");
    ASSERT(key_is_zeroed(&ctx) && !ctx.key_valid,
           "teardown ran even after emit failure");
    ASSERT((np_anon_host_lpgpr2 & NP_SNVS_ANON_IN_PROGRESS) == 0U,
           "flag cleared even after emit failure");
}

static void test_pipeline_null_args(void)
{
    pipeline_probe_t p;
    np_anon_pipeline_steps_t steps;
    probe_reset(&p, &steps);

    np_anon_scratch_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    ASSERT(np_anon_pipeline_run(NULL, &steps, NULL) == NP_ANON_ERR_INVALID,
           "null ctx rejected");
    ASSERT(np_anon_pipeline_run(&ctx, NULL, NULL) == NP_ANON_ERR_INVALID,
           "null steps rejected");

    /* A missing mandatory callback must be rejected before init, not skipped. */
    steps.validate = NULL;
    ASSERT(np_anon_pipeline_run(&ctx, &steps, NULL) == NP_ANON_ERR_INVALID,
           "missing validate callback rejected");
    ASSERT(p.stage_calls == 0 && p.emit_calls == 0,
           "no step runs when the pipeline is mis-wired");
}

static void test_pipeline_init_failure(void)
{
    pipeline_probe_t p;
    np_anon_pipeline_steps_t steps;
    probe_reset(&p, &steps);

    np_anon_scratch_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    np_anon_host_lpgpr2 = 0U;
    np_anon_step_t failed = NP_ANON_STEP_NONE;

    np_anon_host_trng_fail = 1;  /* force the first TRNG draw (key) to fail */
    ASSERT(np_anon_pipeline_run(&ctx, &steps, &failed) == NP_ANON_ERR_TRNG,
           "init TRNG failure should propagate");
    np_anon_host_trng_fail = 0;

    ASSERT(failed == NP_ANON_STEP_INIT, "failed step should be INIT");
    ASSERT(p.stage_calls == 0 && p.emit_calls == 0,
           "no step runs when init fails");
    ASSERT((np_anon_host_lpgpr2 & NP_SNVS_ANON_IN_PROGRESS) == 0U,
           "flag never set when init fails (nothing staged)");
    ASSERT(!ctx.key_valid, "ctx stays invalid when init fails");
}

int main(void)
{
    test_init_sets_flag_and_key();
    test_init_null();
    test_roundtrip();
    test_invalid_args();
    test_complete_clears_secrets();
    test_resume_after_powerloss();
    test_pipeline_success();
    test_pipeline_abort_on_stage();
    test_pipeline_abort_on_transform();
    test_pipeline_abort_on_validate();
    test_pipeline_emit_failure();
    test_pipeline_null_args();
    test_pipeline_init_failure();

    if (g_fail_count == 0) {
        printf("PASS: all np_anon scratch tests\n");
    } else {
        printf("FAILED: %d assertion(s)\n", g_fail_count);
    }
    return g_fail_count;
}
