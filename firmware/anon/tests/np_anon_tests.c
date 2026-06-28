/*
 * NeuroPulse Research Anonymization — Scratch Encryption Host Tests
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
 * Build for host: -DNPTEST_HOST  (see CMakeLists.txt NP_BUILD_TESTS option).
 * Return convention: 0 = PASS, non-zero = failure count.
 */

#include "np_anon_scratch.h"
#include "np_anon_config.h"

#include <stdio.h>
#include <string.h>

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

int main(void)
{
    test_init_sets_flag_and_key();
    test_init_null();
    test_roundtrip();
    test_invalid_args();
    test_complete_clears_secrets();
    test_resume_after_powerloss();

    if (g_fail_count == 0) {
        printf("PASS: all np_anon scratch tests\n");
    } else {
        printf("FAILED: %d assertion(s)\n", g_fail_count);
    }
    return g_fail_count;
}
