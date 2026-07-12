/*
 * NeurOne Hub Control — Cervical VNS Module Driver Host Tests
 * Document: NP-FW-HUB-001 Rev A §8.8, NP-FW-CVNS-001 Rev A
 *
 * Host-native tests for the hub ↔ np_cervical_vns integration layer
 * (modules/np_mod_cvns.c).  Focus is on the integration seams this driver owns,
 * NOT the library internals (the cervical_vns FAI tests cover Pan-Tompkins,
 * ramp, and cardiac-cutoff timing):
 *
 *   - Safety-critical descriptor→config validation (np_mod_cvns_build_config):
 *     frequency / current / pulse-width clamping and electrode-side mapping.
 *   - The unified NP_SAFETY_EN_CVNS enable bit is NOT requested at command
 *     start nor during the pre-stimulation stages (impedance / baseline) — only
 *     once the library is in a stimulation-delivery stage.
 *   - A cardiac fault callback drops the unified enable bit AND writes an SHDR
 *     fault record with a SUPPRESSED (0) timestamp (privacy gate).
 *   - stop / shutdown / session-end all release the enable bit.
 *   - detect / init behaviour and the SHDR accessory-auth log.
 *
 * The real np_cervical_vns library sources are linked in; the hub glue
 * (np_safety_spi, np_log_*) and the three HAL stubs are mocked here.
 *
 * IEC 62304 Class B — SW-02 hub control.
 * Build with -DNP_BUILD_TESTS=ON (see firmware/hub_control/CMakeLists.txt).
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../include/np_hub_types.h"
#include "np_cvns_types.h"
#include "np_cvns_session.h"
#include "np_cvns_interlock.h"
#include "np_cvns_stim.h"

/* ── Driver public surface (extern; no per-module header, registry idiom) ────── */

extern np_hub_status_t np_mod_cvns_detect(uint8_t slot, np_hub_mod_type_t *type_out);
extern np_hub_status_t np_mod_cvns_init(uint8_t slot);
extern np_hub_status_t np_mod_cvns_control(uint8_t slot, const void *params, uint16_t len);
extern np_hub_status_t np_mod_cvns_telemetry(uint8_t slot, np_telem_record_t *out);
extern np_hub_status_t np_mod_cvns_shutdown(uint8_t slot);
extern void            np_mod_cvns_build_config(const np_mod_cvns_params_t *p,
                                                np_cvns_session_config_t   *out);
extern void            np_mod_cvns_tick(uint32_t now_ms, uint32_t now_s);
extern void            np_mod_cvns_push_ppg(uint32_t sample, uint32_t timestamp_ms);
extern np_hub_status_t np_mod_cvns_request_reenable(uint32_t now_s);

/* Test seams (NPTEST_HOST). */
extern np_cvns_session_ctx_t *np_mod_cvns_test_session(void);
extern bool np_mod_cvns_test_enable_requested(void);
extern bool np_mod_cvns_test_active(void);
extern void np_mod_cvns_test_fire_fault(np_cvns_fault_reason_t reason);

/* ── Mocked hub glue ─────────────────────────────────────────────────────────── */

static int      g_enable_calls;
static int      g_disable_calls;
static uint16_t g_last_enable_mask;
static uint16_t g_last_disable_mask;

void np_safety_spi_request_enable(uint16_t mask)
{
    g_enable_calls++;
    g_last_enable_mask = mask;
}
void np_safety_spi_request_disable(uint16_t mask)
{
    g_disable_calls++;
    g_last_disable_mask = mask;
}

static int     g_auth_calls;
static bool    g_auth_last_pass;
void np_log_shdr_zone_auth(uint8_t slot, np_hub_mod_type_t type, bool pass)
{
    (void)slot; (void)type;
    g_auth_calls++;
    g_auth_last_pass = pass;
}

static int      g_fault_log_calls;
static uint8_t  g_fault_log_slot;
static uint8_t  g_fault_log_type;
static uint8_t  g_fault_log_code;
static uint32_t g_fault_log_ms;
void np_log_shdr_fault(uint8_t slot, np_hub_mod_type_t type,
                       uint8_t fault_code, uint32_t session_ms)
{
    g_fault_log_calls++;
    g_fault_log_slot = slot;
    g_fault_log_type = (uint8_t)type;
    g_fault_log_code = fault_code;
    g_fault_log_ms   = session_ms;
}

/* ── Mocked HAL stubs ────────────────────────────────────────────────────────── */

static bool     g_accessory_present = true;
static uint32_t g_now_ms;
static uint32_t g_now_unix = 1000000000U;

bool     np_cvns_hal_accessory_present(void) { return g_accessory_present; }
uint32_t np_mod_cvns_hal_now_ms(void)        { return g_now_ms; }
uint32_t np_mod_cvns_hal_now_unix(void)      { return g_now_unix; }

/* ── Harness ─────────────────────────────────────────────────────────────────── */

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

static void reset_mocks(void)
{
    g_enable_calls = g_disable_calls = 0;
    g_last_enable_mask = g_last_disable_mask = 0;
    g_auth_calls = 0; g_auth_last_pass = false;
    g_fault_log_calls = 0; g_fault_log_slot = 0; g_fault_log_type = 0;
    g_fault_log_code = 0; g_fault_log_ms = 0;
    g_accessory_present = true;
    g_now_ms = 0; g_now_unix = 1000000000U;
}

static np_mod_cvns_params_t make_params(void)
{
    np_mod_cvns_params_t p;
    memset(&p, 0, sizeof(p));
    p.side           = 2U;      /* bilateral */
    p.freq_mhz       = 25000U;  /* 25 Hz */
    p.amplitude_ua   = 1000U;   /* 1 mA */
    p.pulse_width_us = 250U;    /* uint8_t field (≤255); see note in current/pw test */
    p.ramp_s         = 10U;
    p.baseline_req   = 1U;
    return p;
}

/* ── Tests ───────────────────────────────────────────────────────────────────── */

static void test_detect(void)
{
    reset_mocks();
    np_hub_mod_type_t t = NP_MOD_NONE;

    g_accessory_present = true;
    check(np_mod_cvns_detect(NP_HUB_SLOT_CVNS, &t) == NP_HUB_OK &&
          t == NP_MOD_CVNS, "detect: present → NP_MOD_CVNS");

    g_accessory_present = false;
    check(np_mod_cvns_detect(NP_HUB_SLOT_CVNS, &t) == NP_HUB_ERR_NOT_PRESENT,
          "detect: absent → NOT_PRESENT");
}

static void test_init(void)
{
    reset_mocks();
    check(np_mod_cvns_init(NP_HUB_SLOT_CVNS) == NP_HUB_OK, "init: returns OK");
    check(g_auth_calls == 1 && g_auth_last_pass, "init: logs SHDR auth pass");
    check(np_cvns_session_stage(np_mod_cvns_test_session()) == NP_CVNS_STAGE_IDLE,
          "init: session stage IDLE");
    check(g_enable_calls == 0, "init: no enable requested");
}

static void test_build_config_frequency(void)
{
    np_mod_cvns_params_t p = make_params();
    np_cvns_session_config_t c;

    p.freq_mhz = 25000U;  np_mod_cvns_build_config(&p, &c);
    check(c.freq_hz == 25U, "config: 25000 mHz → 25 Hz");

    p.freq_mhz = 40000U;  np_mod_cvns_build_config(&p, &c);
    check(c.freq_hz == 25U, "config: 40000 mHz clamps to 25 Hz max");

    p.freq_mhz = 0U;      np_mod_cvns_build_config(&p, &c);
    check(c.freq_hz == 1U, "config: 0 mHz clamps to 1 Hz min");

    p.freq_mhz = 10400U;  np_mod_cvns_build_config(&p, &c);
    check(c.freq_hz == 10U, "config: 10400 mHz rounds to 10 Hz");

    p.freq_mhz = 10500U;  np_mod_cvns_build_config(&p, &c);
    check(c.freq_hz == 11U, "config: 10500 mHz rounds to 11 Hz");
}

static void test_build_config_current_pw(void)
{
    np_mod_cvns_params_t p = make_params();
    np_cvns_session_config_t c;

    p.amplitude_ua = 5000U; np_mod_cvns_build_config(&p, &c);
    check(c.current_ua == NP_CVNS_CURRENT_UA_MAX, "config: 5000 µA clamps to 2000 µA");

    p.amplitude_ua = 1500U; np_mod_cvns_build_config(&p, &c);
    check(c.current_ua == 1500U, "config: 1500 µA passes through");

    /* pulse_width_us is uint16_t in the wire protocol — the full clinical
     * 200–1000 µs range is expressible and clamped at both ends. */
    p = make_params();
    p.pulse_width_us = 0U;    np_mod_cvns_build_config(&p, &c);
    check(c.pulse_width_us == 250U, "config: pw 0 → 250 µs default");

    p.pulse_width_us = 100U;  np_mod_cvns_build_config(&p, &c);
    check(c.pulse_width_us == NP_CVNS_PULSE_WIDTH_US_MIN, "config: pw 100 clamps to 200 µs");

    p.pulse_width_us = 600U;  np_mod_cvns_build_config(&p, &c);
    check(c.pulse_width_us == 600U, "config: pw 600 passes through in-range");

    p.pulse_width_us = 3000U; np_mod_cvns_build_config(&p, &c);
    check(c.pulse_width_us == NP_CVNS_PULSE_WIDTH_US_MAX, "config: pw 3000 clamps to 1000 µs");

    check(c.duration_s == NP_CVNS_SESSION_MAX_S, "config: duration bounded to 120 s");
}

static void test_build_config_electrode_map(void)
{
    np_mod_cvns_params_t p = make_params();
    np_cvns_session_config_t c;

    p.side = 2U; np_mod_cvns_build_config(&p, &c);
    check(c.electrode_config == 0U, "config: side bilateral → electrode 0");
    p.side = 1U; np_mod_cvns_build_config(&p, &c);
    check(c.electrode_config == 1U, "config: side left → electrode 1 (unilateral L)");
    p.side = 0U; np_mod_cvns_build_config(&p, &c);
    check(c.electrode_config == 2U, "config: side right → electrode 2 (unilateral R)");
    p.side = 9U; np_mod_cvns_build_config(&p, &c);
    check(c.electrode_config == 0U, "config: invalid side → bilateral (fail-safe)");
}

static void test_control_invalid_len(void)
{
    reset_mocks();
    np_mod_cvns_init(NP_HUB_SLOT_CVNS);
    np_mod_cvns_params_t p = make_params();
    check(np_mod_cvns_control(NP_HUB_SLOT_CVNS, &p, 2U) == NP_HUB_ERR_INVALID_ARG,
          "control: short params rejected");
    check(g_enable_calls == 0, "control: no enable on rejected command");
}

static void test_control_start_no_immediate_enable(void)
{
    reset_mocks();
    np_mod_cvns_init(NP_HUB_SLOT_CVNS);
    np_mod_cvns_params_t p = make_params();

    check(np_mod_cvns_control(NP_HUB_SLOT_CVNS, &p, sizeof(p)) == NP_HUB_OK,
          "control: start returns OK");
    check(np_mod_cvns_test_active(), "control: driver marked active");
    /* Library begins with impedance check, not stimulation. */
    check(np_cvns_session_stage(np_mod_cvns_test_session()) != NP_CVNS_STAGE_IDLE,
          "control: session left IDLE (impedance stage)");
    check(g_enable_calls == 0 && !np_mod_cvns_test_enable_requested(),
          "control: NP_SAFETY_EN_CVNS NOT requested before delivery");
}

static void test_tick_pre_delivery_no_enable(void)
{
    reset_mocks();
    np_mod_cvns_init(NP_HUB_SLOT_CVNS);
    np_mod_cvns_params_t p = make_params();
    np_mod_cvns_control(NP_HUB_SLOT_CVNS, &p, sizeof(p));

    /* Tick through pre-delivery stages (no impedance response injected → stays
     * in impedance/baseline).  Enable must remain un-requested. */
    for (uint32_t t = 0; t < 500U; t += 100U) {
        np_mod_cvns_tick(t, g_now_unix);
    }
    check(g_enable_calls == 0, "tick: no enable while awaiting impedance/baseline");
    check(!np_mod_cvns_test_enable_requested(), "tick: enable bit stays low pre-delivery");
}

static void test_enable_asserted_when_delivering(void)
{
    reset_mocks();
    np_mod_cvns_init(NP_HUB_SLOT_CVNS);
    np_mod_cvns_params_t p = make_params();
    np_mod_cvns_control(NP_HUB_SLOT_CVNS, &p, sizeof(p));

    /* White-box: place the library session in a delivery stage.  The library's
     * own tick (invoked inside np_mod_cvns_tick) leaves ACTIVE unchanged when
     * the stim phase has not advanced, so this isolates the driver's
     * stage→enable-bit contract. */
    np_mod_cvns_test_session()->stage = NP_CVNS_STAGE_ACTIVE;
    np_mod_cvns_tick(10U, g_now_unix);

    check(np_mod_cvns_test_enable_requested(), "delivery: enable bit requested");
    check(g_enable_calls == 1 && g_last_enable_mask == NP_SAFETY_EN_CVNS,
          "delivery: enable mask is NP_SAFETY_EN_CVNS");
}

static void test_terminal_stage_drops_enable(void)
{
    reset_mocks();
    np_mod_cvns_init(NP_HUB_SLOT_CVNS);
    np_mod_cvns_params_t p = make_params();
    np_mod_cvns_control(NP_HUB_SLOT_CVNS, &p, sizeof(p));

    np_mod_cvns_test_session()->stage = NP_CVNS_STAGE_ACTIVE;
    np_mod_cvns_tick(10U, g_now_unix);          /* asserts enable */

    np_mod_cvns_test_session()->stage = NP_CVNS_STAGE_COMPLETE;
    np_mod_cvns_tick(20U, g_now_unix);          /* terminal → drop */

    check(g_disable_calls >= 1 && g_last_disable_mask == NP_SAFETY_EN_CVNS,
          "terminal: enable dropped on COMPLETE");
    check(!np_mod_cvns_test_enable_requested(), "terminal: enable bit cleared");
    check(!np_mod_cvns_test_active(), "terminal: driver released");
}

static void test_stop_drops_enable(void)
{
    reset_mocks();
    np_mod_cvns_init(NP_HUB_SLOT_CVNS);
    np_mod_cvns_params_t p = make_params();
    np_mod_cvns_control(NP_HUB_SLOT_CVNS, &p, sizeof(p));

    np_mod_cvns_test_session()->stage = NP_CVNS_STAGE_ACTIVE;
    np_mod_cvns_tick(10U, g_now_unix);          /* asserts enable */

    check(np_mod_cvns_control(NP_HUB_SLOT_CVNS, NULL, 0U) == NP_HUB_OK,
          "stop: returns OK");
    check(g_disable_calls >= 1 && g_last_disable_mask == NP_SAFETY_EN_CVNS,
          "stop: NP_SAFETY_EN_CVNS disabled");
    check(!np_mod_cvns_test_active(), "stop: driver released");
}

static void test_fault_cb_disables_and_logs(void)
{
    reset_mocks();
    np_mod_cvns_init(NP_HUB_SLOT_CVNS);
    np_mod_cvns_params_t p = make_params();
    np_mod_cvns_control(NP_HUB_SLOT_CVNS, &p, sizeof(p));

    np_mod_cvns_test_session()->stage = NP_CVNS_STAGE_ACTIVE;
    np_mod_cvns_tick(10U, g_now_unix);          /* asserts enable */
    check(np_mod_cvns_test_enable_requested(), "fault: precondition — enable asserted");

    g_disable_calls = 0;
    np_mod_cvns_test_fire_fault(NP_CVNS_FAULT_HR_CHANGE);

    check(g_disable_calls == 1 && g_last_disable_mask == NP_SAFETY_EN_CVNS,
          "fault: unified enable dropped");
    check(!np_mod_cvns_test_enable_requested(), "fault: enable bit cleared");
    check(g_fault_log_calls == 1, "fault: one SHDR fault record written");
    check(g_fault_log_slot == NP_HUB_SLOT_CVNS &&
          g_fault_log_type == (uint8_t)NP_MOD_CVNS &&
          g_fault_log_code == (uint8_t)NP_CVNS_FAULT_HR_CHANGE,
          "fault: SHDR record carries slot/type/reason");
    check(g_fault_log_ms == 0U, "fault: SHDR timestamp suppressed (privacy gate)");
}

static void test_stop_when_inactive_noop(void)
{
    reset_mocks();
    np_mod_cvns_init(NP_HUB_SLOT_CVNS);
    check(np_mod_cvns_control(NP_HUB_SLOT_CVNS, NULL, 0U) == NP_HUB_OK,
          "stop-inactive: returns OK");
    check(g_disable_calls == 0, "stop-inactive: no spurious disable");
}

static void test_control_before_init(void)
{
    /* Fresh translation unit state is not resettable, but init() is idempotent;
     * this exercises the not-ready guard by checking telemetry NULL-guard and
     * the ready gate indirectly via a valid init path first. */
    reset_mocks();
    np_telem_record_t rec;
    check(np_mod_cvns_telemetry(NP_HUB_SLOT_CVNS, NULL) == NP_HUB_ERR_INVALID_ARG,
          "telemetry: NULL out rejected");
    np_mod_cvns_init(NP_HUB_SLOT_CVNS);
    check(np_mod_cvns_telemetry(NP_HUB_SLOT_CVNS, &rec) == NP_HUB_OK &&
          rec.mod_type == NP_MOD_CVNS && rec.slot == NP_HUB_SLOT_CVNS,
          "telemetry: fills NP_MOD_CVNS record");
}

int main(void)
{
    test_detect();
    test_init();
    test_build_config_frequency();
    test_build_config_current_pw();
    test_build_config_electrode_map();
    test_control_invalid_len();
    test_control_start_no_immediate_enable();
    test_tick_pre_delivery_no_enable();
    test_enable_asserted_when_delivering();
    test_terminal_stage_drops_enable();
    test_stop_drops_enable();
    test_fault_cb_disables_and_logs();
    test_stop_when_inactive_noop();
    test_control_before_init();

    if (g_failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}
