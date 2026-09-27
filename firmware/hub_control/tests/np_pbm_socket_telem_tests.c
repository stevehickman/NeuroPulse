/*
 * NeurOne Hub Control — Socket-path PBM telemetry host tests (OI-FWHUB-10)
 * Document: NP-FW-HUB-001 Rev 15 §6.8
 *
 * Pins the principal's field rulings of 2026-09-27 (F1–F14) and the metering
 * that feeds them:
 *   - dose metering: one np_pbm_dose_tick() per driven socket per
 *     NP_PBM_DOSE_TICK_MS; a stopped socket is not ticked; a wavelength's limit
 *     latches that channel, each within a tick of the next;
 *   - an unreadable PD is "not measured", never zero; a base tile has no
 *     1064 nm row;
 *   - an NTC at NP_PBM_THERMAL_FAULT_C, or an unreadable one, latches every
 *     channel and counts one throttle;
 *   - no socket is driven unmetered: past NP_PST_MAX_TRACKED, tracking fails;
 *   - the socket record (0x1A) is UHDR only, byte for byte;
 *   - the per-session counts (0x88) are SHDR, carry no location, and are
 *     written even for a session with no PBM;
 *   - the idle health pass (0x87) writes one SHDR record per occupied PBM
 *     socket and none for an empty or non-PBM one, and WHAT IT WRITES DOES NOT
 *     DEPEND ON WHICH SOCKETS A SESSION LIT.
 *
 * The logger, its backend host model and the PBM dose model are real; the
 * module map, the socket driver and every HAL seam are doubles.
 * No FreeRTOS, no hardware. IEC 62304 Class B — SW-02 hub control.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "np_hub_types.h"
#include "np_module_map.h"
#include "np_pbm_socket_telem.h"
#include "np_session_log.h"
#include "np_log_backend.h"
#include "np_pbm_config.h"
#include "np_pbm_dose.h"
#include "np_pbm_hal.h"
#include "np_pbm_types.h"
#include "np_sw02_platform_hal.h"

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

/* ── Doubles: module map ───────────────────────────────────────────────────── */

typedef struct {
    np_module_uid_t uid;               /* zero = empty */
    uint8_t         elems[4];
    uint8_t         n_elems;
} fake_socket_t;

static fake_socket_t g_map[NP_HEXMAP_MAX_SOCKETS];

bool np_module_uid_is_zero(const np_module_uid_t *a)
{
    for (unsigned i = 0U; i < NP_HEXMAP_UID_LEN; i++) {
        if (a->b[i] != 0U) { return false; }
    }
    return true;
}

bool np_module_uid_equal(const np_module_uid_t *a, const np_module_uid_t *b)
{
    return memcmp(a->b, b->b, NP_HEXMAP_UID_LEN) == 0;
}

np_hub_status_t np_module_map_socket_uid(uint16_t socket_id, np_module_uid_t *uid_out)
{
    if (socket_id >= NP_HEXMAP_MAX_SOCKETS || np_module_uid_is_zero(&g_map[socket_id].uid)) {
        return NP_HUB_ERR_NOT_PRESENT;
    }
    *uid_out = g_map[socket_id].uid;
    return NP_HUB_OK;
}

np_hub_status_t np_module_map_resolve(np_hex_addr_t addr, np_physical_loc_t *out)
{
    const fake_socket_t *s = &g_map[addr.socket_id];
    if (np_module_uid_is_zero(&s->uid) || addr.element_id >= s->n_elems) {
        return NP_HUB_ERR_NOT_PRESENT;
    }
    out->x_mm = 0; out->y_mm = 0;
    out->elem_type = (np_elem_type_t)s->elems[addr.element_id];
    return NP_HUB_OK;
}

static void map_clear(void) { memset(g_map, 0, sizeof g_map); }

static void map_put(uint16_t sock, uint8_t uid_byte, bool smart)
{
    memset(&g_map[sock], 0, sizeof g_map[sock]);
    g_map[sock].uid.b[0] = uid_byte;
    g_map[sock].elems[0] = NP_ELEM_LED_660;
    g_map[sock].elems[1] = NP_ELEM_LED_808;
    g_map[sock].elems[2] = NP_ELEM_PD_FORWARD;
    g_map[sock].n_elems  = 3U;
    if (smart) {
        g_map[sock].elems[3] = NP_ELEM_LED_1064;
        g_map[sock].n_elems  = 4U;
    }
}

static void map_put_electrode(uint16_t sock, uint8_t uid_byte)
{
    memset(&g_map[sock], 0, sizeof g_map[sock]);
    g_map[sock].uid.b[0] = uid_byte;
    g_map[sock].elems[0] = NP_ELEM_EEG_ELECTRODE;
    g_map[sock].n_elems  = 1U;
}

/* ── Doubles: socket driver, HAL seams ─────────────────────────────────────── */

static uint8_t  g_latched[NP_HEXMAP_MAX_SOCKETS];
static unsigned g_disable_calls;

np_hub_status_t np_mod_pbm_socket_disable_channel(uint16_t socket_id, uint8_t ch_bit)
{
    g_latched[socket_id] |= ch_bit;
    g_disable_calls++;
    return NP_HUB_OK;
}

static float    g_ntc_c = 30.0f;
static bool     g_ntc_fail;
static unsigned g_ntc_reads;

np_hub_status_t np_mod_pbm_hal_socket_ntc_read(uint16_t socket_id, float *temp_c_out)
{
    (void)socket_id;
    g_ntc_reads++;
    if (g_ntc_fail) { return NP_HUB_ERR_GENERIC; }
    *temp_c_out = g_ntc_c;
    return NP_HUB_OK;
}

static bool     g_selftest_ok;
static uint16_t g_selftest_counts = 1000U;

np_hub_status_t np_mod_pbm_hal_socket_selftest_pd1(uint16_t socket_id, uint8_t wl_idx,
                                                   uint16_t *counts_out)
{
    (void)socket_id; (void)wl_idx;
    if (!g_selftest_ok) { return NP_HUB_ERR_GENERIC; }
    *counts_out = g_selftest_counts;
    return NP_HUB_OK;
}

static uint16_t g_pd1 = 1000U, g_pd2 = 750U;   /* ratio 1.333 = 660 nm nominal */
static bool     g_pd_fail;

bool np_pbm_hal_adc_read_pd(uint8_t slot, uint8_t pd_ch, uint16_t *counts_out)
{
    (void)slot;
    if (g_pd_fail) { return false; }
    *counts_out = (pd_ch == 0U) ? g_pd1 : g_pd2;
    return true;
}

static bool g_probe_ack = true;
bool np_pbm_hal_i2c_probe(uint8_t slot, uint8_t i2c_addr, uint32_t timeout_ms)
{
    (void)slot; (void)i2c_addr; (void)timeout_ms;
    return g_probe_ack;
}

np_pbm_status_t np_pbm_drive_set_duty(uint8_t slot, np_pbm_drv_slot_t *drv,
                                      uint8_t ch_mask, uint8_t duty)
{
    (void)slot; (void)drv; (void)ch_mask; (void)duty;
    return NP_PBM_OK;
}

/* ── Harness ───────────────────────────────────────────────────────────────── */

#define SOCK_REC_BYTES    40U
#define HEALTH_REC_BYTES  13U
#define COUNTS_REC_BYTES  11U

static np_session_uhdr_record_t g_rec;
static size_t g_u0, g_s0;

static void fresh(void)
{
    np_log_test_reset();
    (void)np_log_backend_init();
    np_log_init(0U);
    np_adapt_log_reset();
    memset(&g_rec, 0, sizeof g_rec);
    np_log_session_start(&g_rec);        /* session 1: a UHDR file is open */
    np_pst_session_reset();
    memset(g_latched, 0, sizeof g_latched);
    g_disable_calls = 0U;
    g_ntc_c = 30.0f; g_ntc_fail = false; g_ntc_reads = 0U;
    g_pd1 = 1000U; g_pd2 = 750U; g_pd_fail = false;
    g_u0 = np_log_test_captured_len(NP_LOG_PART_UHDR);
    g_s0 = np_log_test_captured_len(NP_LOG_PART_SHDR);
}

static size_t uhdr_new(void) { return np_log_test_captured_len(NP_LOG_PART_UHDR) - g_u0; }
static size_t shdr_new(void) { return np_log_test_captured_len(NP_LOG_PART_SHDR) - g_s0; }
static const uint8_t *uhdr_at(void) { return np_log_test_captured(NP_LOG_PART_UHDR) + g_u0; }
static const uint8_t *shdr_at(void) { return np_log_test_captured(NP_LOG_PART_SHDR) + g_s0; }

/* Start the tick clock at 0, then run `n` dose ticks. */
static void ticks(unsigned n)
{
    np_pst_dose_poll(0U);
    for (unsigned i = 1U; i <= n; i++) {
        np_pst_dose_poll(i * NP_PBM_DOSE_TICK_MS);
    }
}

/* ── Session side ──────────────────────────────────────────────────────────── */

static void test_metering_and_uhdr_record(void)
{
    fresh();
    check(np_pst_track(5U, NP_MOD_PBM_SMART) == NP_HUB_OK, "meter: a smart socket is tracked");
    ticks(1U);
    np_pst_socket_record_t r;
    check(np_pst_get_record(5U, &r) && r.dose_J_cm2[NP_WL_660NM] > 0.0f &&
          r.irradiance_mW_cm2[NP_WL_660NM] > 119.0f &&
          r.irradiance_mW_cm2[NP_WL_660NM] < 121.0f &&
          (r.flags & 0x07U) == 0x07U && (r.flags & NP_PST_FLAG_NTC_VALID) &&
          r.ntc_c == 30.0f && r.ntc_peak_c == 30.0f,
          "meter: one tick integrates dose at the calibrated irradiance, all three PDs read");

    float d1 = r.dose_J_cm2[NP_WL_660NM];
    np_pst_dose_poll(NP_PBM_DOSE_TICK_MS + NP_PBM_DOSE_TICK_MS / 2U);   /* half a tick */
    (void)np_pst_get_record(5U, &r);
    check(r.dose_J_cm2[NP_WL_660NM] == d1, "meter: no tick before NP_PBM_DOSE_TICK_MS has elapsed");
    np_pst_dose_poll(5U * NP_PBM_DOSE_TICK_MS);      /* owes 200, 300, 400, 500 */
    (void)np_pst_get_record(5U, &r);
    check(r.dose_J_cm2[NP_WL_660NM] > 4.9f * d1 && r.dose_J_cm2[NP_WL_660NM] < 5.1f * d1,
          "meter: a late poll catches up the elapsed ticks");

    np_pst_log_telemetry(1234U);
    np_log_flush();
    const uint8_t *u = uhdr_at();
    uint32_t ms; float dose0; float ntc;
    memcpy(&ms, u + 1U, 4U);
    memcpy(&ntc, u + 8U, 4U);
    memcpy(&dose0, u + 16U, 4U);
    check(uhdr_new() == SOCK_REC_BYTES && u[0] == NP_LOG_TAG_UHDR_PBM_SOCKET &&
          ms == 1234U && u[5] == 5U && u[6] == NP_MOD_PBM_SMART && u[7] == r.flags &&
          ntc == 30.0f && dose0 == r.dose_J_cm2[NP_WL_660NM],
          "uhdr 0x1A: tag, session_ms, socket, kind, flags, NTC, dose — 40 bytes");
    check(shdr_new() == 0U, "uhdr 0x1A: the socket record puts nothing in SHDR (F1–F8)");
}

static void test_unreadable_pd_is_not_measured(void)
{
    fresh();
    (void)np_pst_track(9U, NP_MOD_PBM_SMART);
    g_pd_fail = true;
    ticks(3U);
    np_pst_socket_record_t r;
    (void)np_pst_get_record(9U, &r);
    check((r.flags & 0x07U) == 0U && r.dose_J_cm2[NP_WL_660NM] == 0.0f &&
          r.irradiance_mW_cm2[NP_WL_660NM] == 0.0f,
          "not measured: unreadable PDs clear PD_VALID and book no dose");
}

static void test_base_tile_has_no_1064_row(void)
{
    fresh();
    (void)np_pst_track(2U, NP_MOD_PBM_BASE);
    ticks(2U);
    np_pst_socket_record_t r;
    (void)np_pst_get_record(2U, &r);
    check(r.dose_J_cm2[NP_WL_808NM] > 0.0f && r.dose_J_cm2[NP_WL_1064NM] == 0.0f &&
          (r.flags & (1U << NP_WL_1064NM)) == 0U,
          "base tile: 660/808 metered, the 1064 nm row is not measured and books nothing");
}

static void test_dose_limit_latches_each_channel(void)
{
    fresh();
    (void)np_pst_track(4U, NP_MOD_PBM_SMART);
    g_pd1 = 65535U; g_pd2 = 1U;                       /* every limit in one tick */
    ticks(1U);
    check(g_latched[4] == NP_PBM_CH_A_EN, "limit: the first tick latches 660 nm (CH_A)");
    ticks(2U);
    check(g_latched[4] == (NP_PBM_CH_A_EN | NP_PBM_CH_B_EN | NP_PBM_CH_C_EN),
          "limit: 808 and 1064 follow, one tick each — no limit missed for longer");
    np_pst_socket_record_t r;
    (void)np_pst_get_record(4U, &r);
    check(((r.flags >> NP_PST_FLAG_LIMIT_SHIFT) & 0x07U) == 0x07U,
          "limit: the UHDR record says which limits were reached (F4)");
}

static void test_thermal_throttle(void)
{
    fresh();
    (void)np_pst_track(6U, NP_MOD_PBM_BASE);
    g_ntc_c = (float)NP_PBM_THERMAL_FAULT_C;
    ticks(1U);
    unsigned reads = g_ntc_reads;
    ticks(3U);
    np_pst_counts_t c;
    np_pst_get_counts(&c);
    np_pst_socket_record_t r;
    (void)np_pst_get_record(6U, &r);
    check(g_latched[6] == NP_PBM_CH_ALL_EN && c.throttle_events == 1U &&
          (r.flags & NP_PST_FLAG_THROTTLED) && r.ntc_peak_c == (float)NP_PBM_THERMAL_FAULT_C,
          "thermal: NTC at the fault threshold latches every channel, counts one throttle");
    check(g_ntc_reads == reads, "thermal: a throttled socket is dark and is not ticked again");

    fresh();
    (void)np_pst_track(6U, NP_MOD_PBM_BASE);
    g_ntc_fail = true;
    ticks(1U);
    np_pst_get_counts(&c);
    check(g_latched[6] == NP_PBM_CH_ALL_EN && c.throttle_events == 1U,
          "thermal: an unreadable NTC is treated as a hot one");
}

static void test_stopped_socket_not_ticked(void)
{
    fresh();
    (void)np_pst_track(8U, NP_MOD_PBM_SMART);
    ticks(1U);
    np_pst_untrack_driving(8U);
    unsigned reads = g_ntc_reads;
    np_pst_dose_poll(10U * NP_PBM_DOSE_TICK_MS);
    np_pst_socket_record_t r;
    check(g_ntc_reads == reads && np_pst_get_record(8U, &r) && r.dose_J_cm2[0] > 0.0f,
          "stop: a stopped socket is not ticked, and its dose stays in the session");
    float d = r.dose_J_cm2[0];
    (void)np_pst_track(8U, NP_MOD_PBM_SMART);          /* re-drive, same session */
    np_pst_dose_poll(11U * NP_PBM_DOSE_TICK_MS);
    (void)np_pst_get_record(8U, &r);
    check(r.dose_J_cm2[0] > d, "stop: a re-drive in the same session keeps accumulating");
}

static void test_no_socket_driven_unmetered(void)
{
    fresh();
    bool all = true;
    for (uint16_t s = 0U; s < NP_PST_MAX_TRACKED; s++) {
        all = all && (np_pst_track(s, NP_MOD_PBM_BASE) == NP_HUB_OK);
    }
    check(all && np_pst_track(100U, NP_MOD_PBM_BASE) != NP_HUB_OK,
          "pool: past NP_PST_MAX_TRACKED a socket cannot be tracked, so it is not driven");
    check(np_pst_track(3U, NP_MOD_PBM_SMART) == NP_HUB_OK,
          "pool: a socket already tracked can be re-driven when the pool is full");
    np_pst_session_reset();
    check(np_pst_track(100U, NP_MOD_PBM_BASE) == NP_HUB_OK &&
          !np_pst_get_record(3U, &(np_pst_socket_record_t){0}),
          "pool: session load forgets every socket");
}

static void test_session_counts_shdr(void)
{
    fresh();
    np_pst_note_predrive_refusal();
    np_pst_note_drive_fault();
    np_pst_note_drive_fault();
    np_pst_counts_t c;
    np_pst_get_counts(&c);
    np_log_shdr_pbm_session_counts(&c);
    np_log_flush();
    const uint8_t *s = shdr_at();
    uint32_t cnt; uint16_t thr, pre, flt;
    memcpy(&cnt, s + 1U, 4U); memcpy(&thr, s + 5U, 2U);
    memcpy(&pre, s + 7U, 2U); memcpy(&flt, s + 9U, 2U);
    check(shdr_new() == COUNTS_REC_BYTES && s[0] == NP_LOG_TAG_SHDR_PBM_COUNTS &&
          cnt == 1U && thr == 0U && pre == 1U && flt == 2U && uhdr_new() == 0U,
          "shdr 0x88: counts with the session count and no location, 11 bytes (F13)");

    fresh();
    np_log_shdr_pbm_session_counts(NULL);                /* a session with no PBM */
    np_log_flush();
    check(shdr_new() == COUNTS_REC_BYTES && shdr_at()[0] == NP_LOG_TAG_SHDR_PBM_COUNTS,
          "shdr 0x88: written the same shape for a session with no PBM (rule 2)");
}

/* ── Idle side ─────────────────────────────────────────────────────────────── */

static void run_pass(void)
{
    np_pst_health_request();
    for (unsigned i = 0U; i <= NP_HEXMAP_MAX_SOCKETS && !np_pst_health_step(); i++) { }
}

/* The SHDR bytes one pass writes. */
static size_t pass_bytes(uint8_t *out, size_t cap)
{
    size_t s0 = np_log_test_captured_len(NP_LOG_PART_SHDR);
    run_pass();
    np_log_flush();
    size_t n = np_log_test_captured_len(NP_LOG_PART_SHDR) - s0;
    if (n > cap) { n = cap; }
    memcpy(out, np_log_test_captured(NP_LOG_PART_SHDR) + s0, n);
    return n;
}

static void test_health_pass(void)
{
    fresh();
    map_clear();
    map_put(3U, 0xA1U, true);
    map_put(7U, 0xB2U, false);
    map_put_electrode(9U, 0xC3U);
    g_selftest_ok = false;
    g_probe_ack   = true;

    size_t u0 = np_log_test_captured_len(NP_LOG_PART_UHDR);
    uint8_t b[256];
    size_t n = pass_bytes(b, sizeof b);
    check(n == 2U * HEALTH_REC_BYTES && b[0] == NP_LOG_TAG_SHDR_PBM_TILE_HEALTH &&
          b[5] == 3U && b[HEALTH_REC_BYTES + 5U] == 7U,
          "shdr 0x87: one record per occupied PBM socket, none for empty or electrode sockets");
    check(b[6] == NP_MOD_PBM_SMART && b[8] == NP_PST_PROBE_PASS &&
          b[HEALTH_REC_BYTES + 6U] == NP_MOD_PBM_BASE &&
          b[HEALTH_REC_BYTES + 8U] == NP_PST_PROBE_NONE,
          "shdr 0x87: tile kind (F14) and the probe (F11b) — a base tile has none");
    check(b[7] == NP_PST_HEALTH_FIRST_PASS && b[9] == NP_CAL_DEFAULT &&
          b[10] == NP_PST_PD1_NOT_MEASURED && b[12] == NP_PST_PD1_NOT_MEASURED,
          "shdr 0x87: first pass, calibration source (F12), self-test not measured (OI-FWHUB-20)");
    check(np_log_test_captured_len(NP_LOG_PART_UHDR) == u0,
          "shdr 0x87: the health pass writes nothing to UHDR");

    g_selftest_ok = true;                               /* 1000 counts × 0.12 = 120 % of 100 */
    g_map[3].uid.b[0] = 0xA9U;                          /* a different module at socket 3 */
    n = pass_bytes(b, sizeof b);
    check(n == 2U * HEALTH_REC_BYTES && b[7] == NP_PST_HEALTH_UID_CHANGED &&
          b[HEALTH_REC_BYTES + 7U] == 0U,
          "shdr 0x87: a swapped module is flagged; an unchanged one is not");
    check(b[10] == 120U && b[12] == 88U &&
          b[HEALTH_REC_BYTES + 10U] == 120U && b[HEALTH_REC_BYTES + 12U] == NP_PST_PD1_NOT_MEASURED,
          "shdr 0x87: PD1 self-test × K_PD1 as % of nominal (F10); a base tile has no 1064 test");
}

/* The property the idle pass exists for: what it writes is the same whatever a
 * session lit. */
static void test_health_pass_ignores_sessions(void)
{
    fresh();
    map_clear();
    map_put(3U, 0xA1U, true);
    map_put(7U, 0xB2U, false);
    map_put(40U, 0xD4U, true);
    g_selftest_ok = true;
    uint8_t warm[256];
    (void)pass_bytes(warm, sizeof warm);                /* first-pass flags out of the way */

    uint8_t idle[256], after[256];
    size_t n_idle = pass_bytes(idle, sizeof idle);

    (void)np_pst_track(7U, NP_MOD_PBM_BASE);            /* a session lights socket 7 */
    ticks(5U);
    np_pst_untrack_driving(7U);
    size_t s_mid = np_log_test_captured_len(NP_LOG_PART_SHDR);
    np_log_flush();
    check(np_log_test_captured_len(NP_LOG_PART_SHDR) == s_mid,
          "no leak: driving a socket writes nothing to SHDR by itself");
    size_t n_after = pass_bytes(after, sizeof after);
    check(n_idle == 3U * HEALTH_REC_BYTES && n_after == n_idle &&
          memcmp(idle, after, n_idle) == 0,
          "no leak: the health pass writes the same bytes before and after a session lit socket 7");
}

int main(void)
{
    printf("── np_pbm_socket_telem_tests (OI-FWHUB-10) ──\n");

    test_metering_and_uhdr_record();
    test_unreadable_pd_is_not_measured();
    test_base_tile_has_no_1064_row();
    test_dose_limit_latches_each_channel();
    test_thermal_throttle();
    test_stopped_socket_not_ticked();
    test_no_socket_driven_unmetered();
    test_session_counts_shdr();
    test_health_pass();
    test_health_pass_ignores_sessions();

    if (g_failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}
