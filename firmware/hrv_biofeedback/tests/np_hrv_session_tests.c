/*
 * NeurOne HRV Biofeedback — taVNS Configuration Host Tests
 * Document: NP-FW-HRV-001 Rev 1 §8, §10; OI-HRV-05; NP-SW-CI-001 OI-SWCI-05
 *
 * The first host test target for firmware/hrv_biofeedback.  OI-SWCI-05 asked
 * whether this library warranted one; the answer arrived as a defect:
 *
 *   np_hrv_tavns_init() rejected an out-of-range frequency by returning early,
 *   BEFORE it overwrote its module-static enable callback, rate and current.
 *   np_hrv_session_start() ignored the return and started the session anyway.
 *   So a taVNS session asking for 30 Hz (a shipped protocol did — GitHub #386)
 *   ran with the PREVIOUS session's callback armed at the PREVIOUS session's
 *   rate and current, and the first inspiration onset fired it.
 *
 * T1 reproduces that at the module that holds the statics, with a recording
 * callback.  T2 checks the session refuses to start.  T3 pins the accepted
 * window so a fix that refused everything would fail too.
 *
 * Return convention: 0 = PASS, non-zero = failure count.
 */

#include "np_hrv_session.h"
#include "np_hrv_tavns_sync.h"

#include <stdio.h>
#include <string.h>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL [%s:%d] %s\n", __func__, __LINE__, (m)); g_fail++; } \
                         else { printf("PASS: %s\n", (m)); } } while (0)

/* ── Recording callbacks ──────────────────────────────────────────────────── */
static int      g_enables;
static uint16_t g_last_freq;
static uint16_t g_last_ua;

static void rec_enable(uint16_t f, uint16_t ua) { g_enables++; g_last_freq = f; g_last_ua = ua; }
static void rec_disable(void) { }

/* Feed R-R intervals that shorten by 20 ms/beat: a clear inspiration onset,
 * well past NP_TAVNS_SLOPE_HYSTERESIS, over more than the slope window. */
static void drive_inspiration(np_tavns_state_t *t)
{
    uint16_t rr = 1000U;
    for (uint32_t i = 0U; i < 2U * NP_TAVNS_INSP_SLOPE_WIN; i++) {
        np_hrv_tavns_process_rr(t, rr, 1000U * i);
        rr = (uint16_t)(rr - 20U);
    }
}

/* T1 — a rejected configuration leaves nothing armed from before it. */
static void test_rejected_init_disarms_previous(void)
{
    np_tavns_state_t t;

    /* Control: a valid configuration does fire on inspiration. */
    g_enables = 0;
    CHECK(np_hrv_tavns_init(&t, 10U, 800U, rec_enable, rec_disable) == NP_HRV_OK,
          "valid 10 Hz / 800 uA configuration accepted");
    drive_inspiration(&t);
    CHECK(g_enables >= 1 && g_last_freq == 10U && g_last_ua == 800U,
          "control: inspiration onset fires the enable at the configured rate");

    /* The defect: 30 Hz is refused, and must not leave 10 Hz armed. */
    CHECK(np_hrv_tavns_init(&t, 30U, 800U, rec_enable, rec_disable) == NP_HRV_ERR_INVALID_ARG,
          "30 Hz refused");
    g_enables = 0;
    memset(&t, 0, sizeof(t));          /* the session pool's zeroed state */
    drive_inspiration(&t);
    CHECK(g_enables == 0,
          "after a refused init, inspiration does not fire the previous session's enable");

    /* Same for an out-of-range current. */
    (void)np_hrv_tavns_init(&t, 10U, 800U, rec_enable, rec_disable);
    CHECK(np_hrv_tavns_init(&t, 10U, 2001U, rec_enable, rec_disable) == NP_HRV_ERR_INVALID_ARG,
          "2001 uA refused");
    g_enables = 0;
    memset(&t, 0, sizeof(t));
    drive_inspiration(&t);
    CHECK(g_enables == 0, "after a refused current, nothing previous is armed");
}

/* T1b — the gate opens on inspiration only.  Pins the direction of the dRR
 * fix: steady or lengthening R-R (expiration) must never enable. */
static void test_gate_direction(void)
{
    np_tavns_state_t t;
    uint16_t rr;

    (void)np_hrv_tavns_init(&t, 10U, 800U, rec_enable, rec_disable);
    g_enables = 0;
    for (uint32_t i = 0U; i < 4U * NP_TAVNS_INSP_SLOPE_WIN; i++) {
        np_hrv_tavns_process_rr(&t, 900U, 1000U * i);
    }
    CHECK(g_enables == 0, "steady R-R never opens the gate");

    (void)np_hrv_tavns_init(&t, 10U, 800U, rec_enable, rec_disable);
    g_enables = 0;
    rr = 800U;
    for (uint32_t i = 0U; i < 2U * NP_TAVNS_INSP_SLOPE_WIN; i++) {
        np_hrv_tavns_process_rr(&t, rr, 1000U * i);
        rr = (uint16_t)(rr + 20U);
    }
    CHECK(g_enables == 0, "lengthening R-R (expiration) never opens the gate");
}

/* ── Session level ────────────────────────────────────────────────────────── */
static void end_cb(const np_hrv_session_record_t *r, np_hrv_status_t s) { (void)r; (void)s; }

static np_hrv_status_t start_with(uint16_t freq_hz, uint16_t ua, bool *running)
{
    np_hrv_session_config_t cfg;
    np_hrv_coherence_result_t coh;

    memset(&cfg, 0, sizeof(cfg));
    cfg.protocol         = NP_HRV_PROTO_TAVNS_SYNC;
    cfg.duration_s       = 600U;
    cfg.tavns_freq_hz    = freq_hz;
    cfg.tavns_current_ua = ua;

    np_hrv_session_t *s = np_hrv_session_create(&cfg, NULL, end_cb, 0U);
    np_hrv_status_t st = np_hrv_session_start(s, 0U);
    /* NO_SESSION means "not running"; NOT_READY means running, no data yet. */
    *running = (np_hrv_session_get_coherence(s, &coh) != NP_HRV_ERR_NO_SESSION);
    np_hrv_session_stop(s, 1000U);
    np_hrv_session_destroy(s);
    return st;
}

/* T2 — the session refuses to start on a refused stimulus. */
static void test_session_refuses_bad_tavns(void)
{
    bool running;

    CHECK(start_with(30U, 0U, &running) == NP_HRV_ERR_INVALID_ARG && !running,
          "a 30 Hz taVNS session is refused and never runs (GitHub #386's value)");
    CHECK(start_with(26U, 0U, &running) == NP_HRV_ERR_INVALID_ARG && !running,
          "26 Hz, one past the ceiling, is refused");
    CHECK(start_with(0U, 2001U, &running) == NP_HRV_ERR_INVALID_ARG && !running,
          "an out-of-range current is refused");
    CHECK(start_with(0U, 50U, &running) == NP_HRV_ERR_INVALID_ARG && !running,
          "a current below 100 uA is refused");
}

/* T3 — the accepted window is still accepted. */
static void test_session_accepts_window(void)
{
    bool running;

    CHECK(start_with(0U, 0U, &running) == NP_HRV_OK && running,
          "0 / 0 selects the defaults and runs");
    CHECK(start_with(1U, 100U, &running) == NP_HRV_OK && running,
          "1 Hz / 100 uA, the lower edges, run");
    CHECK(start_with(NP_TAVNS_DEFAULT_FREQ_HZ, NP_TAVNS_MAX_CURRENT_UA, &running) == NP_HRV_OK && running,
          "25 Hz / 2000 uA, the upper edges, run");
}

int main(void)
{
    printf("=== np_hrv_session_tests (OI-HRV-05) ===\n");
    test_rejected_init_disarms_previous();
    test_gate_direction();
    test_session_refuses_bad_tavns();
    test_session_accepts_window();
    printf("=== %s ===\n", g_fail == 0 ? "ALL PASS" : "FAILURES PRESENT");
    return g_fail;
}
