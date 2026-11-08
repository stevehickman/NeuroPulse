/*
 * Host tests — the photoparoxysmal detector's clinician switches (REQ-FWHUB-71).
 *
 * The Config store is a one-record stub. Tested: the defaults are the protective ones, a write is read back with the evidence
 * level beside it, a bit the firmware does not know is refused, the record is persisted from the poll and never from the
 * write, it survives a restart, an unreadable or unknown record leaves the protective defaults and is never overwritten, and
 * the asymmetry: protecting more reaches a detector already watching, protecting less waits for the next session.
 */
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "np_cfg_store.h"
#include "np_ppx_config.h"
#include "np_ppx_detect.h"
#include "np_ppx_policy.h"

static int g_failures = 0;
static void check(int cond, const char *name)
{
    if (cond) { printf("PASS: %s\n", name); }
    else      { printf("FAIL: %s\n", name); g_failures++; }
}

void np_transport_hal_enter_critical(void) {}
void np_transport_hal_exit_critical(void) {}

static uint8_t         g_rec[3];
static bool            g_present;
static np_hub_status_t g_read_rc;
static int             g_writes;

np_hub_status_t np_cfg_store_replicated_read(np_cfg_file_t file, uint8_t *payload, size_t len)
{
    if (file != NP_CFG_FILE_PPX_CONFIG || len != sizeof g_rec) { return NP_HUB_ERR_INVALID_ARG; }
    if (g_read_rc != NP_HUB_OK) { return g_read_rc; }
    if (!g_present) { return NP_HUB_ERR_NOT_PRESENT; }
    memcpy(payload, g_rec, len);
    return NP_HUB_OK;
}

np_hub_status_t np_cfg_store_replicated_write(np_cfg_file_t file, const uint8_t *payload, size_t len)
{
    g_writes++;
    if (file != NP_CFG_FILE_PPX_CONFIG || len != sizeof g_rec) { return NP_HUB_ERR_INVALID_ARG; }
    memcpy(g_rec, payload, len);
    g_present = true;
    return NP_HUB_OK;
}

static void reset_store(void)
{
    memset(g_rec, 0, sizeof g_rec); g_present = false; g_read_rc = NP_HUB_OK; g_writes = 0;
    np_ppx_config_t d = { .stop_on_occipital_only = true, .manual_override = false };
    np_ppx_policy_init(NP_PPX_EVIDENCE_NONE, d);
}

static np_hub_status_t wr(uint8_t flags) { return np_ppx_config_write(&flags, 1U); }

/* @verifies REQ-FWHUB-71 */
static void test_the_defaults_are_the_protective_ones(void)
{
    reset_store();
    np_ppx_config_init();
    check(np_ppx_config_flags() == NP_PPX_CFG_STOP_OCCIPITAL, "config: with nothing stored, stop-on-occipital is on and the override is off");
    check(np_ppx_policy_config().stop_on_occipital_only && !np_ppx_policy_config().manual_override, "config: and the policy has the same");
    np_ppx_config_poll();
    check(g_writes == 0, "config: the defaults are not written to the store");
}

/* @verifies REQ-FWHUB-71 */
static void test_a_write_is_read_back_with_the_evidence_level_and_persisted_by_poll(void)
{
    reset_store();
    np_ppx_config_init();
    check(wr(0U) == NP_HUB_OK && np_ppx_config_flags() == 0U, "config: a write is taken");
    uint8_t b[2]; size_t n = 0;
    check(np_ppx_config_read(b, sizeof b, &n) == NP_HUB_OK && n == 2U && b[0] == 0U && b[1] == 0U,
          "config: a read gives the flags and the evidence level, none today");
    check(wr(NP_PPX_CFG_OVERRIDE) == NP_HUB_ERR_INVALID_ARG && np_ppx_config_flags() == 0U,
          "config: the override cannot be set while the evidence level is none");
    np_ppx_config_t c = { .stop_on_occipital_only = true, .manual_override = false };
    np_ppx_policy_init(NP_PPX_EVIDENCE_BENCH, c);
    check(np_ppx_config_read(b, sizeof b, &n) == NP_HUB_OK && b[1] == 1U, "config: and the level follows the policy's (bench is 1)");
    check(wr(NP_PPX_CFG_OVERRIDE) == NP_HUB_OK && np_ppx_config_flags() == NP_PPX_CFG_OVERRIDE, "config: with bench evidence it can");
    check(g_writes == 0, "config: the write itself does not touch the store");
    np_ppx_config_poll();
    check(g_writes == 1, "config: the poll persists it");
    np_ppx_config_poll();
    check(g_writes == 1, "config: and only once");
    np_ppx_config_init();    /* a restart, the evidence level still bench */
    check(np_ppx_config_flags() == NP_PPX_CFG_OVERRIDE && np_ppx_policy_config().manual_override && !np_ppx_policy_config().stop_on_occipital_only,
          "config: the switches survive a restart and reach the policy");
    np_ppx_policy_init(NP_PPX_EVIDENCE_NONE, c);
    np_ppx_config_init();    /* a restart on firmware that has no evidence: the stored override waits, it is not applied */
    check(!np_ppx_policy_config().manual_override && g_rec[1] == NP_PPX_CFG_OVERRIDE,
          "config: a stored override is not applied while the evidence is none, and the record is left as it was");
}

/* @verifies REQ-FWHUB-71 */
static void test_bad_writes_are_refused_and_change_nothing(void)
{
    reset_store();
    np_ppx_config_init();
    const uint8_t two[2] = { 1, 0 };
    check(wr(0x04U) == NP_HUB_ERR_INVALID_ARG && wr(0x80U) == NP_HUB_ERR_INVALID_ARG, "config: a bit the firmware does not know is refused");
    check(np_ppx_config_write(two, 2U) == NP_HUB_ERR_INVALID_ARG && np_ppx_config_write(NULL, 1U) == NP_HUB_ERR_INVALID_ARG,
          "config: so is a write of the wrong length");
    check(np_ppx_config_flags() == NP_PPX_CFG_DEFAULT, "config: and nothing changed");
    uint8_t b[1]; size_t n = 0;
    check(np_ppx_config_read(b, 1U, &n) == NP_HUB_ERR_INVALID_ARG, "config: a read into too small a buffer is refused");
}

/* @verifies REQ-FWHUB-71 */
static void test_an_unreadable_or_unknown_record_leaves_the_protective_defaults_and_is_not_overwritten(void)
{
    reset_store();
    g_present = true; g_rec[0] = 0x02; g_rec[1] = NP_PPX_CFG_OVERRIDE;
    g_read_rc = NP_HUB_ERR_GENERIC;
    np_ppx_config_init();
    check(np_ppx_config_flags() == NP_PPX_CFG_DEFAULT && !np_ppx_policy_config().manual_override,
          "config: an unreadable record leaves the defaults, so the override is not on");
    np_ppx_config_poll();
    check(g_writes == 0, "config: nothing is written over an unreadable record");
    g_read_rc = NP_HUB_OK;
    np_ppx_config_poll();
    check(np_ppx_config_flags() == 0U && !np_ppx_policy_config().manual_override, "config: once it can be read, it is loaded (its override waits for evidence)");

    reset_store();
    g_present = true; g_rec[0] = 0x02; g_rec[1] = 0U;                       /* both switches stored off */
    np_ppx_config_init();
    check(np_ppx_config_flags() == 0U && !np_ppx_policy_config().stop_on_occipital_only && !np_ppx_policy_config().manual_override,
          "config: a stored record is loaded exactly, not widened (both off stays both off)");

    reset_store();
    g_present = true; g_rec[0] = 0x07; g_rec[1] = NP_PPX_CFG_OVERRIDE;     /* another version */
    np_ppx_config_init();
    check(np_ppx_config_flags() == NP_PPX_CFG_DEFAULT, "config: a record of another version is not believed");
    reset_store();
    g_present = true; g_rec[0] = 0x02; g_rec[1] = 0x44;                    /* bits this firmware does not know */
    np_ppx_config_init();
    check(np_ppx_config_flags() == NP_PPX_CFG_DEFAULT, "config: a record with unknown bits is not believed");
    np_ppx_config_poll();
    check(g_writes == 0, "config: and neither is overwritten");
}

/* @verifies REQ-FWHUB-71 */
static void test_a_write_made_before_the_load_finished_wins(void)
{
    reset_store();
    g_present = true; g_rec[0] = 0x02; g_rec[1] = NP_PPX_CFG_STOP_OCCIPITAL;
    g_read_rc = NP_HUB_ERR_GENERIC;
    np_ppx_config_init();
    (void)wr(0U);
    g_read_rc = NP_HUB_OK;
    np_ppx_config_poll();
    check(np_ppx_config_flags() == 0U, "config: a write made before the load finished is not overwritten by the stored value");
}

/* ── the asymmetry, against a real detector ─────────────────────────────────── */

static uint32_t g_rng = 4242U;
static float urand(void) { g_rng = g_rng * 1664525U + 1013904223U; return (float)(g_rng >> 8) / 16777216.0f; }
static float noise(float sigma) { float s = 0.0f; for (int i = 0; i < 12; i++) { s += urand(); } return (s - 6.0f) * sigma; }
static int g_halts;
static void on_halt(np_ppx_verdict_t why) { (void)why; g_halts++; }

/* `seconds` of quiet signal; `spike_at` (sample, or -1) puts a step of `amp` uV on channel 0 (occipital) alone. */
static void feed_amp(float seconds, long spike_at, float amp)
{
    const long total = (long)(seconds * 500.0f);
    float blk[20 * 8];
    for (long n0 = 0; n0 < total; n0 += 20) {
        for (long s = 0; s < 20; s++) {
            for (long c = 0; c < 8; c++) {
                float v = noise(10.0f);
                if (n0 + s == spike_at && c == 0) { v += amp; }
                blk[s * 8 + c] = v;
            }
        }
        np_ppx_feed(blk, 20U, 8U);
    }
}

static void feed(float seconds, long spike_at) { feed_amp(seconds, spike_at, 200.0f); }

static void start_detector(bool stop_occ)
{
    np_ppx_params_t p; np_ppx_params_provisional(&p);
    np_ppx_roles_t roles = { 0x03U, 0xFCU };
    np_ppx_init(&p, roles, stop_occ);
    np_ppx_set_halt_fn(on_halt);
    g_halts = 0; g_rng = 4242U;
    feed(3.0f, -1);
}

/* @verifies REQ-FWHUB-71 */
static void test_protecting_more_reaches_a_running_detector_and_protecting_less_waits_for_the_next_session(void)
{
    reset_store();
    np_ppx_config_init();
    (void)wr(0U);                              /* off */
    start_detector(np_ppx_policy_config().stop_on_occipital_only);
    feed(1.0f, 100);
    check(g_halts == 0, "asymmetry: with the switch off, an occipital-only discharge does not halt");

    start_detector(false);
    (void)wr(NP_PPX_CFG_STOP_OCCIPITAL);       /* on, in a session already running */
    feed(1.0f, 100);
    check(g_halts == 1, "asymmetry: turning it on reaches the detector already watching");

    start_detector(true);
    (void)wr(0U);                              /* off, in a session already running */
    feed(1.0f, 100);
    check(g_halts == 1, "asymmetry: turning it off does not reach into a session already watching");

    start_detector(np_ppx_policy_config().stop_on_occipital_only);    /* the next session reads the policy afresh */
    feed(1.0f, 100);
    check(g_halts == 0, "asymmetry: and the next session honours it");
    np_ppx_set_halt_fn(NULL);
}

/* @verifies REQ-FWHUB-68 */
static void test_sensitivity_defaults_to_the_validated_setting_and_cannot_go_below_it(void)
{
    reset_store();
    np_ppx_config_init();
    uint8_t b[2]; size_t n = 0;
    check(np_ppx_sensitivity_level() == 0U && np_ppx_sensitivity_read(b, 2U, &n) == NP_HUB_OK && n == 2U && b[0] == 0U && b[1] == NP_PPX_SENS_MAX,
          "sensitivity: the default is level 0, the validated setting, and a read gives the highest level the helmet offers");
    const uint8_t hi = NP_PPX_SENS_MAX, over = NP_PPX_SENS_MAX + 1U, neg = 0xFFU;
    check(np_ppx_sensitivity_write(&hi, 1U) == NP_HUB_OK && np_ppx_sensitivity_level() == NP_PPX_SENS_MAX && np_ppx_policy_config().sensitivity == NP_PPX_SENS_MAX,
          "sensitivity: the highest level is taken and reaches the policy");
    check(np_ppx_sensitivity_write(&over, 1U) == NP_HUB_ERR_INVALID_ARG && np_ppx_sensitivity_write(&neg, 1U) == NP_HUB_ERR_INVALID_ARG,
          "sensitivity: a level above the highest, or a 'level below 0', is refused");
    check(np_ppx_sensitivity_level() == NP_PPX_SENS_MAX, "sensitivity: and a refused write changes nothing");
    const uint8_t two[2] = { 1, 0 };
    check(np_ppx_sensitivity_write(two, 2U) == NP_HUB_ERR_INVALID_ARG && np_ppx_sensitivity_write(NULL, 1U) == NP_HUB_ERR_INVALID_ARG &&
          np_ppx_sensitivity_read(b, 1U, &n) == NP_HUB_ERR_INVALID_ARG, "sensitivity: a write of the wrong length, or a read into too small a buffer, is refused");
}

/* @verifies REQ-FWHUB-68 */
static void test_sensitivity_is_persisted_by_the_poll_and_survives_a_restart(void)
{
    reset_store();
    np_ppx_config_init();
    const uint8_t two = 2U;
    (void)np_ppx_sensitivity_write(&two, 1U);
    check(g_writes == 0, "sensitivity: the write does not touch the store");
    np_ppx_config_poll();
    check(g_writes == 1 && g_rec[2] == 2U, "sensitivity: the poll persists it, in the record beside the flags");
    np_ppx_config_init();
    check(np_ppx_sensitivity_level() == 2U && np_ppx_policy_config().sensitivity == 2U, "sensitivity: and it survives a restart");

    reset_store();
    g_present = true; g_rec[0] = 0x02; g_rec[1] = NP_PPX_CFG_STOP_OCCIPITAL; g_rec[2] = 9U;      /* a level this firmware does not offer */
    np_ppx_config_init();
    check(np_ppx_sensitivity_level() == 0U, "sensitivity: a stored level the firmware does not offer is not believed (the validated setting stands)");
    np_ppx_config_poll();
    check(g_writes == 0, "sensitivity: and the record is not overwritten");
}

/* @verifies REQ-FWHUB-68 */
static void test_raising_sensitivity_reaches_a_running_detector_and_lowering_waits_for_the_next_session(void)
{
    /* A step of 120 uV: a spike at level 0 (threshold 8 x the background), not noticed at level 0 but caught at the highest level. */
    reset_store();
    np_ppx_config_init();
    start_detector(true);
    g_halts = 0;
    feed_amp(1.0f, 100, 70.0f);
    check(g_halts == 0, "sensitivity: a 70 uV step on the occipital channel is below the validated threshold and does not halt");

    start_detector(true);
    const uint8_t hi = NP_PPX_SENS_MAX;
    (void)np_ppx_sensitivity_write(&hi, 1U);       /* raised in a session already running */
    feed_amp(1.0f, 100, 70.0f);
    check(g_halts == 1, "sensitivity: raising it reaches the detector already watching, and the same step now halts");

    start_detector(true);
    (void)np_ppx_sensitivity_write(&hi, 1U);
    const uint8_t zero = 0U;
    (void)np_ppx_sensitivity_write(&zero, 1U);     /* lowered in a session already running */
    check(np_ppx_sensitivity() == NP_PPX_SENS_MAX, "sensitivity: lowering it does not reach a detector already watching");
    start_detector(true);                           /* the next session starts at the validated setting ... */
    check(np_ppx_sensitivity() == 0U, "sensitivity: the next session starts at level 0");
    np_ppx_set_sensitivity(np_ppx_policy_config().sensitivity);   /* ... and its start sets the configured level, now 0 */
    feed_amp(1.0f, 100, 70.0f);
    check(g_halts == 0, "sensitivity: and honours the lowered setting");
    np_ppx_set_halt_fn(NULL);
}

int main(void)
{
    test_the_defaults_are_the_protective_ones();
    test_a_write_is_read_back_with_the_evidence_level_and_persisted_by_poll();
    test_bad_writes_are_refused_and_change_nothing();
    test_an_unreadable_or_unknown_record_leaves_the_protective_defaults_and_is_not_overwritten();
    test_a_write_made_before_the_load_finished_wins();
    test_protecting_more_reaches_a_running_detector_and_protecting_less_waits_for_the_next_session();
    test_sensitivity_defaults_to_the_validated_setting_and_cannot_go_below_it();
    test_sensitivity_is_persisted_by_the_poll_and_survives_a_restart();
    test_raising_sensitivity_reaches_a_running_detector_and_lowering_waits_for_the_next_session();
    if (g_failures == 0) { printf("ALL PASS (0 failures)\n"); return 0; }
    printf("FAILED (%d failure%s)\n", g_failures, g_failures == 1 ? "" : "s");
    return 1;
}
