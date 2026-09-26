/*
 * NeurOne Hub Control — Session Lease Host Tests
 * Document: NP-FW-HUB-001 §2.2 (REQ-FWHUB-03, OI-FWHUB-17)
 *
 * Rev 8 kept module detection off a session with a state check re-read before
 * each probe.  A session could still start between that read and the detect(),
 * because task_module_detect is the lowest priority.  The lease replaces the
 * check with exclusion, and these tests pin the two halves of it:
 *   1. once a session holds the lease, no probe begins;
 *   2. a claim cannot complete while a probe is in progress: it takes the SAME
 *      lock the probe holds, so on target it waits for that probe to end.
 * Plus: a second claim is refused, release re-opens probing, and no path
 * leaves the lock held except the one that must (a granted probe).
 *
 * The lock is a stub that records its depth.  Single-threaded, a lock() at
 * depth 1 cannot block, so it records CONTENTION instead: that is the event
 * which, on target, is the claimant blocking behind the probe.
 *
 * No FreeRTOS, no hardware.  IEC 62304 Class B — SW-02 hub control.
 *
 * Falsification (NP-CONV-001 §8): each mutant below fails the case named.
 *   - np_lease_claim() without take()/give()        → contention case
 *   - np_lease_probe_begin() ignoring s_held       → "no probe during a session"
 *   - np_lease_probe_begin() giving before return  → contention case
 *   - np_lease_claim() not checking s_held          → second-claim case
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "np_session_lease.h"

static int g_failures = 0;

static void check(int cond, const char *name)
{
    if (cond) {
        printf("PASS: %s\n", name);
    } else {
        printf("FAIL: %s\n", name);
        g_failures++;
    }
}

/* ── Stub lock ───────────────────────────────────────────────────────────────── */

static int  s_depth;
static bool s_contended;
static bool s_underflow;

static void stub_lock(void)
{
    if (s_depth > 0) {
        s_contended = true;     /* on target: this caller blocks here */
    }
    s_depth++;
}

static void stub_unlock(void)
{
    if (s_depth == 0) {
        s_underflow = true;
        return;
    }
    s_depth--;
}

static void fresh(void)
{
    s_depth     = 0;
    s_contended = false;
    s_underflow = false;
    np_lease_init(stub_lock, stub_unlock);
}

/* ── Cases ───────────────────────────────────────────────────────────────────── */

static void test_idle_probe_holds_the_lock(void)
{
    fresh();
    check(!np_lease_held(), "idle: no lease after init");
    check(np_lease_probe_begin(), "idle: a probe is granted");
    check(s_depth == 1, "idle: the granted probe holds the lock for its duration");
    np_lease_probe_end();
    check(s_depth == 0 && !s_underflow, "idle: probe_end releases it exactly once");
}

static void test_no_probe_during_a_session(void)
{
    fresh();
    check(np_lease_claim() == NP_HUB_OK, "session: the claim is granted");
    check(s_depth == 0, "session: the claim does not keep the lock");
    check(np_lease_held(), "session: the lease is held");
    check(!np_lease_probe_begin(), "session: no probe begins while the lease is held");
    check(s_depth == 0 && !s_underflow,
          "session: a refused probe leaves nothing held");
}

static void test_claim_waits_for_a_probe_in_progress(void)
{
    fresh();
    check(np_lease_probe_begin(), "race: the detect task's probe starts first");
    s_contended = false;
    (void)np_lease_claim();     /* the load arriving mid-probe */
    check(s_contended,
          "race: the claim takes the lock the probe holds, so on target it "
          "waits for the probe to end (OI-FWHUB-17)");
    np_lease_probe_end();
    check(!np_lease_probe_begin(),
          "race: once the claim completes, the next probe is refused");
    check(s_depth == 0 && !s_underflow, "race: lock balanced afterwards");
}

static void test_second_claim_refused(void)
{
    fresh();
    check(np_lease_claim() == NP_HUB_OK, "second: first claim granted");
    check(np_lease_claim() == NP_HUB_ERR_SESSION_ACTIVE,
          "second: a second claim is refused as SESSION_ACTIVE");
    check(np_lease_held(), "second: the first session still holds the lease");
    check(s_depth == 0 && !s_underflow, "second: the refusal leaves nothing held");
}

static void test_release_reopens_probing(void)
{
    fresh();
    (void)np_lease_claim();
    np_lease_release();
    check(!np_lease_held(), "release: the lease is free");
    check(np_lease_probe_begin(), "release: probing resumes after the session");
    np_lease_probe_end();
    np_lease_release();         /* idempotent */
    check(!np_lease_held() && s_depth == 0 && !s_underflow,
          "release: a second release is harmless");
    check(np_lease_claim() == NP_HUB_OK, "release: the next session can claim");
}

static void test_unlocked_before_init(void)
{
    /* Before np_lease_init() installs a lock (host, or boot before the
     * scheduler), the lease still decides — it is a plain flag. */
    np_lease_init(NULL, NULL);
    check(np_lease_claim() == NP_HUB_OK, "nolock: claim granted");
    check(!np_lease_probe_begin(), "nolock: a probe is still refused while held");
    np_lease_release();
    check(np_lease_probe_begin(), "nolock: and granted once released");
    np_lease_probe_end();
}

int main(void)
{
    printf("── np_session_lease_tests (OI-FWHUB-17) ──\n");

    test_idle_probe_holds_the_lock();
    test_no_probe_during_a_session();
    test_claim_waits_for_a_probe_in_progress();
    test_second_claim_refused();
    test_release_reopens_probing();
    test_unlocked_before_init();

    if (g_failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}
