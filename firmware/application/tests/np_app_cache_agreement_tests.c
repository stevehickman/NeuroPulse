/*
 * NeurOne SW-02 — L1 Cache State Agreement Host Tests
 * Document: NP-SW-CI-001 §4.14 (closes OI-SWCI-45)
 *
 * ── What this suite is for ───────────────────────────────────────────────────
 *
 * Three files in this tree state what the Cortex-M7's L1 caches are doing, and
 * until this file existed nothing compared them:
 *
 *   firmware/application/include/np_app_cache.h
 *       The decision.  NP_SW02_ICACHE_ENABLED 1, NP_SW02_DCACHE_ENABLED 0.
 *
 *   firmware/vendor/mcux_sdk/.../system_MIMXRT1062.c
 *       The establishment.  SystemInit() calls SCB_EnableICache() under
 *       #if __ICACHE_PRESENT and calls nothing else — while the comment
 *       immediately above it says "Enable instruction and data caches".
 *
 *   firmware/vendor/cmsis_core/VERSION
 *       The SOUP record, which got it right: "(The D-cache half is NOT enabled
 *       by SystemInit; nothing in-tree calls SCB_EnableDCache.)"
 *
 * The vendored comment is, as far as this record can tell, the origin of the
 * error: §4.8.5, §4.10.6, app_imxrt1062.ld and OI-SWCI-42 itself all went on to
 * describe `.bss` in OCRAM2 as reached "through the bus and the L1 D-cache".
 * Four NeurOne statements of a fact nobody had read out of the code, and a
 * fifth — the vendored comment — that they most likely all came from.  It is
 * NP-SW-CI-001 §4.8's "two statements compared by nothing" for the fifth
 * occurrence, which is why the fix here is a comparator and not a correction:
 * the divergence lives in byte-exact SOUP that §9 forbids patching.
 *
 * ── What it does NOT prove ──────────────────────────────────────────────────
 *
 * What the running part actually has enabled.  A host has no SCB.  That is a
 * property of the device and is checked where it can be: np_app_cache_assert_state()
 * reads SCB->CCR as the first statement of main() and halts on disagreement,
 * and the cross-build's cache-state census reads the linked image.  Same
 * division of labour, and the same reasoning, as np_app_clock_agreement_tests.
 *
 * Return convention: 0 = PASS, non-zero = failure count.
 */

#include <stdio.h>
#include <string.h>

#include "np_app_cache.h"          /* the decision, as the image sees it */
#include "np_test_strip_code.h"    /* strip_to_code() */

#if !defined(NP_APP_MAIN_C_PATH) || !defined(NP_MCUX_SYSTEM_C_PATH) || \
    !defined(NP_MCUX_STARTUP_S_PATH) || !defined(NP_CMSIS_VERSION_PATH)
#error "the four probe paths must be defined by the build"
#endif

static int g_fail_count = 0;

#define ASSERT(cond, msg)                                            \
    do {                                                             \
        if (!(cond)) {                                               \
            printf("FAIL [%s:%d] %s\n", __func__, __LINE__, (msg));  \
            g_fail_count++;                                          \
        }                                                            \
    } while (0)

/*
 * Read a whole file.  Returns NULL and counts a failure on any short read, so a
 * probe can never report "token absent" because it only saw a prefix — the
 * failure mode that would turn every case below into a silent pass.
 */
static char *slurp(const char *path, char *buf, size_t cap, const char *who)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        printf("FAIL [%s] cannot open %s\n", who, path);
        g_fail_count++;
        return NULL;
    }
    size_t n = fread(buf, 1U, cap - 1U, f);
    int    truncated = (feof(f) == 0);
    fclose(f);
    buf[n] = '\0';

    if (truncated) {
        printf("FAIL [%s] %s is larger than %u bytes — the probe would read a "
               "prefix and could miss what it is looking for\n",
               who, path, (unsigned)cap);
        g_fail_count++;
        return NULL;
    }
    return buf;
}

static char g_raw[1024U * 1024U];
static char g_code[1024U * 1024U];

/* ── 1. The decision, pinned ─────────────────────────────────────────────────
 *
 * Not a tautology over the header it includes: these two values are what every
 * other case here, np_app_cache_assert_state() on the device, and the CI census
 * over the image are all written against.  Changing either is a design event
 * (§4.14.3 names the four things it has to come with), so it stops here and is
 * looked at rather than being silently re-derived.
 */
static void test_declared_state(void)
{
    ASSERT(NP_SW02_DCACHE_ENABLED == 0,
           "NP_SW02_DCACHE_ENABLED is no longer 0. Enabling the D-cache needs "
           "MPU region attributes, DMA cache maintenance, a handover contract "
           "(OI-SWCI-15) and a board to measure on — see NP-SW-CI-001 §4.14.3 "
           "and update this suite, the CI census and the record together");

    ASSERT(NP_SW02_ICACHE_ENABLED == 1,
           "NP_SW02_ICACHE_ENABLED is no longer 1. SystemInit() enables the "
           "I-cache before main() is entered, so the image cannot expect it off "
           "without the vendored startup changing too");
}

/* ── 2. The vendored startup establishes exactly what the header declares ────
 *
 * The comparator that did not exist.  It reads the SDK file's CODE, not its
 * prose, which is the whole point: the prose says the opposite.
 */
static void test_vendored_startup_matches_the_decision(void)
{
    if (slurp(NP_MCUX_SYSTEM_C_PATH, g_raw, sizeof(g_raw), __func__) == NULL) {
        return;
    }
    strip_to_code(g_raw, strlen(g_raw), g_code);

    const int enables_i = (strstr(g_code, "SCB_EnableICache(")  != NULL);
    const int enables_d = (strstr(g_code, "SCB_EnableDCache(")  != NULL);
    const int disables_d = (strstr(g_code, "SCB_DisableDCache(") != NULL);

    ASSERT(enables_i == (NP_SW02_ICACHE_ENABLED != 0),
           "the vendored SystemInit() no longer agrees with "
           "NP_SW02_ICACHE_ENABLED about the instruction cache");

    ASSERT(enables_d == (NP_SW02_DCACHE_ENABLED != 0),
           "the vendored SystemInit() and NP_SW02_DCACHE_ENABLED disagree about "
           "the data cache. If the SDK now enables it: the image has no MPU "
           "configuration and no cache maintenance anywhere, so every DMA buffer "
           "in .bss would be write-back write-allocate under the default system "
           "address map — re-open OI-SWCI-45 before taking that bump. If the "
           "constant was flipped instead: nothing in the tree turns the D-cache "
           "on, so the check in main() would halt every boot (NP-SW-CI-001 "
           "§4.14.3 lists what enabling it has to come with)");

    ASSERT(!disables_d,
           "the vendored SystemInit() now calls SCB_DisableDCache(); the D-cache "
           "would be off by an SDK action rather than by NeurOne's decision, and "
           "§4.14's account of who establishes the state is stale");

    printf("      vendored SystemInit(): I-cache enable %s, D-cache enable %s\n",
           enables_i ? "present" : "ABSENT", enables_d ? "PRESENT" : "absent");
}

/* ── 3. The known divergence inside the vendored file ────────────────────────
 *
 * The comment above that call says "Enable instruction and data caches" and the
 * code enables one.  This case asserts the divergence is STILL THERE, which
 * reads oddly until you ask what it buys: the vendored tree is byte-exact and
 * pinned (§9), so the only way this fails is a tag bump — and a tag bump that
 * silently resolves the divergence, in either direction, is exactly the event
 * §4.14.1's account of where the error came from needs to be re-read against.
 */
static void test_vendored_comment_divergence_is_still_known(void)
{
    if (slurp(NP_MCUX_SYSTEM_C_PATH, g_raw, sizeof(g_raw), __func__) == NULL) {
        return;
    }

    ASSERT(strstr(g_raw, "Enable instruction and data caches") != NULL,
           "the vendored startup's \"Enable instruction and data caches\" "
           "comment is gone. It is the most likely origin of the four documents "
           "that described a D-cache SystemInit() never enabled — if the SDK tag "
           "moved, re-read NP-SW-CI-001 §4.14.1 against the new file");
}

/* ── 4. The second bss-class initialiser, and that it is still not ours ──────
 *
 * §4.12.4 said the vendored startup "cannot be taught about a second region".
 * It can: __STARTUP_INITIALIZE_NONCACHEDATA is a switch the same file already
 * tests for, in the same #ifdef idiom the build uses twice.  §4.14.4 corrects
 * that, and this case keeps both halves of the correction honest — the switch
 * exists, and this build does not define it, so .dtcm_bss is still cleared by
 * first-party code and NOT by a mechanism meant for uncached DMA memory.
 */
static void test_noncacheable_startup_switch(void)
{
    if (slurp(NP_MCUX_STARTUP_S_PATH, g_raw, sizeof(g_raw), __func__) == NULL) {
        return;
    }

    ASSERT(strstr(g_raw, "__STARTUP_INITIALIZE_NONCACHEDATA") != NULL,
           "the vendored startup no longer carries the NonCacheable "
           "initialiser; NP-SW-CI-001 §4.14.4 names it as the mechanism the "
           "D-cache decision would use for DMA buffers");

    ASSERT(strstr(g_raw, "__noncachedata_init_end__") != NULL,
           "the NonCacheable initialiser no longer copies an init image; "
           "§4.14.4's reason for NOT reusing it for .dtcm_bss is then wrong");
}

/* ── 5. The SOUP record agrees ───────────────────────────────────────────────
 *
 * firmware/vendor/cmsis_core/VERSION is the one place that already had this
 * right, in as many words, while four other documents had it wrong. Comparing
 * it here is what makes that a checked agreement rather than a coincidence.
 */
static void test_cmsis_soup_record_agrees(void)
{
    if (slurp(NP_CMSIS_VERSION_PATH, g_raw, sizeof(g_raw), __func__) == NULL) {
        return;
    }

    ASSERT(strstr(g_raw, "D-cache half is NOT enabled by SystemInit") != NULL,
           "firmware/vendor/cmsis_core/VERSION no longer records that the "
           "D-cache is not enabled by SystemInit. That statement is the SOUP "
           "record for cachel1_armv7.h and one of the three the decision in "
           "NP-SW-CI-001 §4.14 is held between");
}

/* ── 6. The boot path still checks the state ─────────────────────────────────
 *
 * A probe of np_app_main.c's CODE, for the same reason the clock suite probes
 * code: this file's own header comment names every symbol below.
 *
 * Its limits are the clock suite's limits.  It proves the call appears in code
 * and where it appears relative to the rest of the boot sequence; it does not
 * prove reachability, and the cross-build cannot supply that either, because
 * --whole-archive puts symbols in the image whether or not anything calls them.
 * What it buys is that deleting the check while leaving §4.14 asserting it
 * fails a test instead of passing in silence.
 */
static void test_boot_path_checks_the_cache_state(void)
{
    if (slurp(NP_APP_MAIN_C_PATH, g_raw, sizeof(g_raw), __func__) == NULL) {
        return;
    }
    strip_to_code(g_raw, strlen(g_raw), g_code);

    const char *cache_chk = strstr(g_code, "np_app_cache_assert_state(");
    const char *dtcm_clr  = strstr(g_code, "np_app_dtcm_bss_clear(");
    const char *clock     = strstr(g_code, "np_platform_clock_init(");
    const char *handover  = strstr(g_code, "np_hub_control_app_main(");

    ASSERT(cache_chk != NULL,
           "np_app_main.c no longer calls np_app_cache_assert_state() in code; "
           "the L1 cache state the image is built for is unverified and "
           "NP-SW-CI-001 §4.14 is asserting a check that is not there");
    ASSERT(dtcm_clr != NULL,
           "np_app_main.c no longer calls np_app_dtcm_bss_clear(); this probe "
           "is anchored to a boot path that no longer exists");
    ASSERT(clock != NULL && handover != NULL,
           "np_app_main.c no longer contains the clock step or the handover; "
           "this probe is anchored to a boot path that no longer exists");

    if (cache_chk != NULL && dtcm_clr != NULL && clock != NULL && handover != NULL) {
        ASSERT(cache_chk < dtcm_clr,
               "the cache-state check no longer precedes the .dtcm_bss clear — "
               "the image would make its first bulk write under a memory model "
               "it has not checked (§4.14.5)");
        ASSERT(cache_chk < clock && cache_chk < handover,
               "the cache-state check runs after the clock step or after control "
               "is handed to np_hub_control_app_main()");
    }
}

int main(void)
{
    printf("np_app_cache_agreement_tests (NP-SW-CI-001 §4.14, OI-SWCI-45)\n");

    test_declared_state();
    test_vendored_startup_matches_the_decision();
    test_vendored_comment_divergence_is_still_known();
    test_noncacheable_startup_switch();
    test_cmsis_soup_record_agrees();
    test_boot_path_checks_the_cache_state();

    if (g_fail_count == 0) {
        printf("PASS: the declared L1 cache state, the vendored startup, the "
               "SOUP record and the boot-path check all agree\n");
    } else {
        printf("FAILED: %d assertion(s)\n", g_fail_count);
    }
    return g_fail_count;
}
