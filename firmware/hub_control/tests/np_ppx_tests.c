/*
 * Host tests — the photoparoxysmal detector, its "cannot see" rule and the policy that says what it may replace.
 *
 * The signals are SYNTHETIC: deterministic noise with spikes added on chosen channels at a chosen sample. These tests are the
 * harness a validation would use (REQ-FWHUB-70 (a)); they are not the validation, and the parameter set is provisional. What
 * they establish is that the detector does what np_ppx_detect.h says it does: which channels make a discharge generalised,
 * what the clinician's switch changes, when it cannot see, that it halts within a sample block of an event, and that it does
 * not halt on background alone (the false-halt count below is on synthetic noise only).
 */
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "np_ppx_detect.h"
#include "np_ppx_policy.h"

static int g_failures = 0;
static void check(int cond, const char *name)
{
    if (cond) { printf("PASS: %s\n", name); }
    else      { printf("FAIL: %s\n", name); g_failures++; }
}

#define NCH   8U
#define FS    500U
#define BLOCK 20U                         /* samples per block: 40 ms */

static uint32_t g_rng = 12345U;
static float urand(void) { g_rng = g_rng * 1664525U + 1013904223U; return (float)(g_rng >> 8) / 16777216.0f; }
static float noise(float sigma) { float s = 0.0f; for (int i = 0; i < 12; i++) { s += urand(); } return (s - 6.0f) * sigma; }

static int g_halts, g_blind_halts, g_gen_halts, g_occ_halts;
static void on_halt(np_ppx_verdict_t why)
{
    g_halts++;
    if (why == NP_PPX_BLIND) { g_blind_halts++; }
    if (why == NP_PPX_DISCHARGE_GENERALISED) { g_gen_halts++; }
    if (why == NP_PPX_DISCHARGE_OCCIPITAL) { g_occ_halts++; }
}

static np_ppx_params_t P;
static const np_ppx_roles_t ROLES = { .occipital_mask = 0x03U, .other_mask = 0xFCU };

static void start(bool stop_occ)
{
    np_ppx_params_provisional(&P);
    np_ppx_init(&P, ROLES, stop_occ);
    np_ppx_set_halt_fn(on_halt);
    g_halts = g_blind_halts = g_gen_halts = g_occ_halts = 0;
    g_rng = 12345U;
}

/* What a scenario does to channel `ch` at sample `n` (seconds*FS): returns the value to add. */
typedef struct {
    uint32_t spike_at;          /* sample of the event, UINT32_MAX for none */
    uint8_t  spike_mask;        /* channels that spike */
    float    spike_uv;
    uint8_t  flat_mask;  uint32_t flat_from;
    uint8_t  sat_mask;   uint32_t sat_from;
    uint8_t  loud_mask;  uint32_t loud_from;
} scn_t;

/* Run `seconds` of signal, in blocks; returns the sample at which the first halt happened (UINT32_MAX if none). */
static uint32_t run(const scn_t *sc, float seconds)
{
    const uint32_t total = (uint32_t)(seconds * (float)FS);
    float buf[BLOCK * NCH];
    uint32_t first_halt = UINT32_MAX;
    for (uint32_t n0 = 0U; n0 < total; n0 += BLOCK) {
        for (uint32_t s = 0U; s < BLOCK; s++) {
            const uint32_t n = n0 + s;
            for (uint32_t c = 0U; c < NCH; c++) {
                float v = noise(10.0f);
                if ((sc->flat_mask & (1U << c)) && n >= sc->flat_from) { v = 5.0f; }
                if ((sc->sat_mask & (1U << c)) && n >= sc->sat_from) { v = (n & 1U) ? 160000.0f : -160000.0f; }
                if ((sc->loud_mask & (1U << c)) && n >= sc->loud_from) { v = noise(400.0f); }
                if (n == sc->spike_at && (sc->spike_mask & (1U << c))) { v += sc->spike_uv; }
                buf[s * NCH + c] = v;
            }
        }
        const int before = g_halts;
        np_ppx_feed(buf, BLOCK, NCH);
        if (first_halt == UINT32_MAX && g_halts > before) { first_halt = n0 + BLOCK; }
    }
    return first_halt;
}

static scn_t quiet(void) { scn_t s; memset(&s, 0, sizeof s); s.spike_at = UINT32_MAX; s.flat_from = s.sat_from = s.loud_from = UINT32_MAX; return s; }

/* @verifies REQ-FWHUB-67 */
static void test_quiet_background_is_seen_and_never_halts(void)
{
    start(true);
    scn_t s = quiet();
    check(run(&s, 3.0f) == UINT32_MAX && g_halts == 0, "quiet: nothing halts");
    check(np_ppx_seeing() && np_ppx_verdict() == NP_PPX_CLEAR, "quiet: after the warm-up the detector sees and the verdict is clear");
    check(np_ppx_channel_trusted(0U) && np_ppx_channel_trusted(7U), "quiet: every channel is trusted");

    /* The false-halt count on synthetic background alone: 30 runs of 60 s, different noise each. */
    int false_halts = 0;
    for (uint32_t seed = 1U; seed <= 30U; seed++) {
        start(true);
        g_rng = seed * 7919U;
        if (run(&s, 60.0f) != UINT32_MAX) { false_halts++; }
    }
    check(false_halts == 0, "quiet: no false halt in 30 minutes of synthetic background (30 runs x 60 s, white noise 10 uV)");
}

/* @verifies REQ-FWHUB-67 */
static void test_a_generalised_discharge_halts_within_one_block(void)
{
    start(true);
    scn_t s = quiet();
    s.spike_at = 5U * FS + 7U; s.spike_mask = 0x1DU /* ch0, ch2, ch3, ch4 */; s.spike_uv = 200.0f;
    const uint32_t h = run(&s, 6.0f);
    check(h != UINT32_MAX && g_gen_halts == 1 && g_halts == 1, "generalised: one halt, and it names a generalised discharge");
    check(h != UINT32_MAX && h - s.spike_at <= BLOCK, "generalised: the halt comes in the block that holds the event (40 ms or less, the 200 ms budget is the whole path)");
    check(np_ppx_verdict() == NP_PPX_DISCHARGE_GENERALISED, "generalised: the verdict stays latched");
}

/* @verifies REQ-FWHUB-67 */
static void test_the_occipital_switch_decides_an_occipital_only_discharge(void)
{
    scn_t s = quiet();
    s.spike_at = 4U * FS; s.spike_mask = 0x01U /* ch0 only */; s.spike_uv = 200.0f;
    start(true);
    check(run(&s, 5.0f) != UINT32_MAX && g_occ_halts == 1, "occipital: with the switch on, a discharge on an occipital channel alone halts");
    start(false);
    check(run(&s, 5.0f) == UINT32_MAX && g_halts == 0, "occipital: with the switch off it does not");
    s.spike_mask = 0x0DU;     /* ch0, ch2, ch3 */
    start(false);
    check(run(&s, 5.0f) != UINT32_MAX && g_gen_halts == 1, "occipital: and a generalised discharge halts whatever the switch says");
    scn_t t = quiet();
    t.spike_at = 4U * FS; t.spike_mask = 0x20U /* ch5 alone */; t.spike_uv = 200.0f;
    start(true);
    check(run(&t, 5.0f) == UINT32_MAX && g_halts == 0, "occipital: one non-occipital channel alone is neither generalised nor occipital");
}

/* @verifies REQ-FWHUB-69 */
static void test_it_does_not_see_until_warm_and_with_no_roles(void)
{
    start(true);
    scn_t s = quiet();
    (void)run(&s, 0.5f);
    check(!np_ppx_seeing() && np_ppx_verdict() == NP_PPX_BLIND, "blind: still warming up after 0.5 s");
    (void)run(&s, 1.5f);
    check(np_ppx_seeing(), "blind: sees once the warm-up is over");

    np_ppx_params_provisional(&P);
    np_ppx_roles_t none = { 0U, 0U };
    np_ppx_init(&P, none, true);
    (void)run(&s, 3.0f);
    check(!np_ppx_seeing(), "blind: with no channel roles set nothing can be seen, however good the signal");
    np_ppx_roles_t only_occ = { 0x03U, 0U };
    np_ppx_init(&P, only_occ, true);
    (void)run(&s, 3.0f);
    check(!np_ppx_seeing(), "blind: occipital channels with none to judge generalisation by cannot see");
}

/* @verifies REQ-FWHUB-69 */
static void test_a_detector_that_could_see_and_cannot_now_halts(void)
{
    scn_t s = quiet();
    s.flat_mask = 0x03U; s.flat_from = 4U * FS;           /* both occipital leads go flat */
    start(true);
    check(run(&s, 14.0f) != UINT32_MAX && g_blind_halts == 1 && g_halts == 1, "blind: losing the occipital channels halts, once");
    check(!np_ppx_seeing(), "blind: and it no longer sees");

    s = quiet(); s.sat_mask = 0x0FU; s.sat_from = 4U * FS; /* a saturated run takes out occipital and two others */
    start(true);
    check(run(&s, 7.0f) != UINT32_MAX && g_blind_halts == 1, "blind: saturation halts");

    s = quiet(); s.loud_mask = 0xFFU; s.loud_from = 4U * FS; /* noise far above the ceiling everywhere */
    start(true);
    check(run(&s, 8.0f) != UINT32_MAX && g_halts == 1, "blind: noise above the ceiling halts (as a discharge or as blindness: either way the drive stops)");

    s = quiet(); s.flat_mask = 0xFCU; s.flat_from = 4U * FS; /* every non-occipital lead goes flat */
    start(true);
    check(run(&s, 14.0f) != UINT32_MAX && g_blind_halts == 1, "blind: with nothing to judge generalisation by, it halts");

    s = quiet(); s.flat_mask = 0x04U; s.flat_from = 4U * FS; /* one of six other leads: still enough to see */
    start(true);
    check(run(&s, 14.0f) == UINT32_MAX && np_ppx_seeing(), "blind: one lost lead of six does not blind it");
}

/* @verifies REQ-FWHUB-69 */
static void test_a_detector_that_never_saw_does_not_halt_by_itself(void)
{
    /* Refusing to START is the caller's (np_ppx_seeing() is false); a halt is for a drive that was being watched. */
    scn_t s = quiet(); s.flat_mask = 0xFFU; s.flat_from = 0U;
    start(true);
    check(run(&s, 4.0f) == UINT32_MAX && g_halts == 0 && !np_ppx_seeing(), "blind: a detector that never saw raises no halt, and reports that it cannot see");
}

/* @verifies REQ-FWHUB-67 */
static void test_one_halt_per_session_and_init_clears_it(void)
{
    start(true);
    scn_t s = quiet();
    s.spike_at = 3U * FS; s.spike_mask = 0x1DU; s.spike_uv = 200.0f;
    (void)run(&s, 4.0f);
    s.spike_at = 5U * FS + 3U;
    (void)run(&s, 6.0f);     /* a second event in a fresh pass of the same detector would be a second halt without the latch */
    check(g_halts >= 1, "latch: the first discharge halts");
    const int after_first = g_halts;
    scn_t again = quiet(); again.spike_at = 1U * FS; again.spike_mask = 0x1DU; again.spike_uv = 200.0f;
    (void)run(&again, 2.0f);
    check(g_halts == after_first, "latch: once latched, a later discharge does not halt again");
    np_ppx_init(&P, ROLES, true);
    check(np_ppx_verdict() == NP_PPX_BLIND, "latch: init clears the latch (it must warm up again)");
}

/* @verifies REQ-FWHUB-70 */
static void test_the_policy_lifts_the_refusal_only_with_the_evidence_for_it(void)
{
    const np_ppx_config_t off = { .stop_on_occipital_only = true, .manual_override = false };
    const np_ppx_config_t on  = { .stop_on_occipital_only = true, .manual_override = true };
    np_ppx_policy_init(NP_PPX_EVIDENCE_NONE, off);
    check(!np_ppx_zone_permitted(), "policy: no evidence, no override: refused");
    np_ppx_policy_init(NP_PPX_EVIDENCE_NONE, on);
    check(!np_ppx_zone_permitted(), "policy: no evidence refuses whatever the override says");
    np_ppx_policy_init(NP_PPX_EVIDENCE_BENCH, off);
    check(!np_ppx_zone_permitted(), "policy: bench evidence without the override: refused");
    np_ppx_policy_init(NP_PPX_EVIDENCE_BENCH, on);
    check(np_ppx_zone_permitted(), "policy: bench evidence with the clinician's override: permitted");
    np_ppx_policy_init(NP_PPX_EVIDENCE_CLINICAL, off);
    check(np_ppx_zone_permitted(), "policy: a clinical study replaces the refusal outright");
    np_ppx_policy_init((np_ppx_evidence_t)99, on);
    check(!np_ppx_zone_permitted(), "policy: an evidence level it does not know is refused");
    np_ppx_policy_init(NP_PPX_EVIDENCE_IN_FORCE, off);
    check(NP_PPX_EVIDENCE_IN_FORCE == NP_PPX_EVIDENCE_NONE && !np_ppx_zone_permitted(), "policy: as shipped, the evidence is NONE and the refusal stands");
    np_ppx_policy_set_config(on);
    check(np_ppx_policy_config().manual_override && np_ppx_policy_config().stop_on_occipital_only, "policy: the configuration is read back");
}

int main(void)
{
    test_quiet_background_is_seen_and_never_halts();
    test_a_generalised_discharge_halts_within_one_block();
    test_the_occipital_switch_decides_an_occipital_only_discharge();
    test_it_does_not_see_until_warm_and_with_no_roles();
    test_a_detector_that_could_see_and_cannot_now_halts();
    test_a_detector_that_never_saw_does_not_halt_by_itself();
    test_one_halt_per_session_and_init_clears_it();
    test_the_policy_lifts_the_refusal_only_with_the_evidence_for_it();
    if (g_failures == 0) { printf("ALL PASS (0 failures)\n"); return 0; }
    printf("FAILED (%d failure%s)\n", g_failures, g_failures == 1 ? "" : "s");
    return 1;
}
