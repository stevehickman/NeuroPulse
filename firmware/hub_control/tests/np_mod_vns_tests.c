/*
 * NeurOne Hub Control — Auricular VNS Driver Contact-Confirmation Host Tests
 * Document: NP-FW-HUB-001 §8.4, NP-HW-VNSCLIP-001 REQ-VNSC-07 (OI-SIMFW-17)
 *
 * REQ-VNSC-07: the clip's contact impedance must fall inside [500, 5000] ohm
 * before np_mod_vns_control() requests an enable.  The driver refused with
 * `imp < min || imp > max`, which is false for NaN, so a non-finite reading
 * enabled stimulation (found by the firmware-in-the-loop simulator,
 * NP-SIM-FW-001 §15).  The check now refuses anything that is not a finite
 * number inside the range.
 *
 * What these tests pin:
 *   1. NaN, +infinity and -infinity are refused with the status an out-of-range
 *      reading gets, and nothing is started: no stimulator write, no enable
 *      request, no published current, no PPG, no EEG reference routing, no HRV
 *      session;
 *   2. the range edges and a mid-range value still enable (the fix refuses
 *      nothing it used to admit), and an out-of-range value refuses with the
 *      same status;
 *   3. a non-finite reading is reported to telemetry as 0 ohm, not as a cast
 *      NaN (undefined behaviour);
 *   4. detection does not report a clip on a non-finite reading.
 *
 * The device build uses -ffast-math, under which isfinite() and a NaN comparison
 * may be folded to a constant.  The driver therefore tests the exponent bits.
 * This host build does not use -ffast-math; the driver was also compiled and
 * run against these tests with `-O2 -ffast-math` to confirm the check survives.
 *
 * No FreeRTOS, no hardware.  IEC 62304 Class B — SW-02 hub control.
 */

#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "np_hub_types.h"
#include "np_hrv_session.h"
#include "np_hrv_types.h"
#include "np_log_hrv.h"
#include "np_safety_spi.h"

/* The doubles below define symbols declared in np_sw02_platform_hal.h, so
 * drift from the production contract is a compile error here too
 * (OI-SWCI-40). */
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

/* ── Driver under test (no per-module header; registry idiom) ────────────────── */

np_hub_status_t np_mod_vns_detect(uint8_t slot, np_hub_mod_type_t *type_out);
np_hub_status_t np_mod_vns_init(uint8_t slot);
np_hub_status_t np_mod_vns_control(uint8_t slot, const void *params, uint16_t len);
np_hub_status_t np_mod_vns_telemetry(uint8_t slot, np_telem_record_t *out);

/* ── Doubles: what the driver asked the hardware to do ───────────────────────── */

static float    g_impedance;
static int      g_stim_set_calls, g_ppg_starts, g_eeg_route_calls, g_hrv_creates;
static uint16_t g_req_mask;
static uint16_t g_published_ua;

void np_safety_spi_request_enable(uint16_t bits)  { g_req_mask |= bits; }
void np_safety_spi_request_disable(uint16_t bits) { g_req_mask &= (uint16_t)~bits; }
void np_safety_spi_set_channel_current(uint8_t channel, uint16_t current_ua)
{
    if (channel == NP_SAFETY_CH_VNS_HRV) { g_published_ua = current_ua; }
}

float np_mod_vns_hal_impedance_check(void) { return g_impedance; }
np_hub_status_t np_mod_vns_hal_stim_set(uint8_t side, uint32_t freq_mhz,
                                        uint16_t amp_ua, uint16_t pw_us)
{
    (void)side; (void)freq_mhz; (void)amp_ua; (void)pw_us;
    g_stim_set_calls++;
    return NP_HUB_OK;
}
np_hub_status_t np_mod_vns_hal_stim_stop(uint8_t side) { (void)side; return NP_HUB_OK; }
void  np_mod_vns_hal_ppg_start(void)          { g_ppg_starts++; }
void  np_mod_vns_hal_ppg_stop(void)           { }
void  np_mod_vns_hal_eeg_ref_route(bool en)   { (void)en; g_eeg_route_calls++; }
float np_mod_vns_hal_get_rmssd(void)          { return 0.0f; }
float np_mod_vns_hal_get_coherence(void)      { return 0.0f; }
float np_mod_vns_hal_get_hr_bpm(void)         { return 0.0f; }
uint32_t np_mod_vns_hal_now_ms(void)          { return 1000U; }

/* The HRV session library and the UHDR logger are not under test: stand-ins. */
static int g_dummy_session;
np_hrv_session_t *np_hrv_session_create(const np_hrv_session_config_t *config,
                                        np_hrv_display_cb_t display_cb, np_hrv_session_end_cb_t end_cb,
                                        uint32_t now_ms)
{
    (void)config; (void)display_cb; (void)end_cb; (void)now_ms;
    g_hrv_creates++;
    return (np_hrv_session_t *)&g_dummy_session;
}
np_hrv_status_t np_hrv_session_start(np_hrv_session_t *sess, uint32_t now_ms)
{
    (void)sess; (void)now_ms;
    return NP_HRV_OK;
}
void np_hrv_session_stop(np_hrv_session_t *sess, uint32_t now_ms)    { (void)sess; (void)now_ms; }
void np_hrv_session_destroy(np_hrv_session_t *sess)                  { (void)sess; }
void np_log_hrv_session(const np_hrv_session_record_t *rec, np_hrv_status_t reason)
{
    (void)rec; (void)reason;
}

/* ── Helpers ──────────────────────────────────────────────────────────────────── */

static void reset(float imp)
{
    g_impedance = imp;
    g_stim_set_calls = g_ppg_starts = g_eeg_route_calls = g_hrv_creates = 0;
    g_req_mask = 0U;
    g_published_ua = 0U;
    (void)np_mod_vns_init(NP_HUB_SLOT_VNS_HRV);
    g_eeg_route_calls = 0;
}

/* Everything the command can start: PPG, EEG routing, HRV session, stimulator. */
static np_hub_status_t drive(void)
{
    np_mod_vns_hrv_params_t p;
    memset(&p, 0, sizeof(p));
    p.side = 0U;
    p.freq_mhz = 25000U;
    p.amplitude_ua = 2000U;
    p.pulse_width_us = 250U;
    p.ppg_enable = 1U;
    p.eeg_ref_enable = 1U;
    p.hrv_proto = 1U;
    return np_mod_vns_control(NP_HUB_SLOT_VNS_HRV, &p, (uint16_t)sizeof(p));
}

static bool nothing_started(void)
{
    return g_stim_set_calls == 0 && g_ppg_starts == 0 && g_eeg_route_calls == 0 &&
           g_hrv_creates == 0 && (g_req_mask & NP_SAFETY_EN_VNS_HRV) == 0U &&
           g_published_ua == 0U;
}

static float make_float(uint32_t bits)
{
    float f;
    memcpy(&f, &bits, sizeof(f));
    return f;
}

/* ── Tests ────────────────────────────────────────────────────────────────────── */

/* @verifies REQ-VNSC-07 */
static void test_a_non_finite_impedance_is_refused_and_starts_nothing(void)
{
    const float cases[] = {
        make_float(0x7FC00000UL),   /* quiet NaN                */
        make_float(0xFFC00000UL),   /* quiet NaN, sign set      */
        make_float(0x7F800001UL),   /* signalling NaN           */
        make_float(0x7F800000UL),   /* +infinity                */
        make_float(0xFF800000UL),   /* -infinity                */
    };
    static const char *const CASE_NAMES[] = {
        "quiet NaN", "negative quiet NaN", "signalling NaN", "+infinity", "-infinity",
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char msg[96];
        reset(cases[i]);
        np_hub_status_t rc = drive();
        snprintf(msg, sizeof(msg), "%s: refused with the out-of-range status", CASE_NAMES[i]);
        check(rc == NP_HUB_ERR_SAFETY_REJECTED, msg);
        snprintf(msg, sizeof(msg), "%s: no stimulator write, enable, current, PPG, EEG routing or HRV session", CASE_NAMES[i]);
        check(nothing_started(), msg);
    }
}

/* @verifies REQ-VNSC-07 */
static void test_the_range_still_enables_inside_and_refuses_outside(void)
{
    const float inside[]  = { 500.0f, 1200.0f, 5000.0f };
    const float outside[] = { 0.0f, 499.9f, 5000.1f, 1.0e9f };
    for (unsigned i = 0; i < sizeof(inside) / sizeof(inside[0]); i++) {
        reset(inside[i]);
        check(drive() == NP_HUB_OK && g_stim_set_calls == 1 &&
              (g_req_mask & NP_SAFETY_EN_VNS_HRV) != 0U,
              "in range (500, 1200, 5000 ohm): enables, as before the fix");
    }
    for (unsigned i = 0; i < sizeof(outside) / sizeof(outside[0]); i++) {
        reset(outside[i]);
        check(drive() == NP_HUB_ERR_SAFETY_REJECTED && nothing_started(),
              "out of range (0, 499.9, 5000.1, 1e9 ohm): refused with the same status, nothing started");
    }
}

/* @verifies REQ-VNSC-07 */
static void test_a_non_finite_reading_is_reported_as_zero_ohm(void)
{
    np_telem_record_t rec;
    memset(&rec, 0xAA, sizeof(rec));
    reset(make_float(0x7FC00000UL));
    (void)drive();
    check(np_mod_vns_telemetry(NP_HUB_SLOT_VNS_HRV, &rec) == NP_HUB_OK &&
          rec.data.vns_hrv.impedance_ohm == 0U && !rec.data.vns_hrv.contact_confirmed,
          "telemetry: a NaN read is 0 ohm and contact is not confirmed");
    reset(1200.0f);
    (void)drive();
    check(np_mod_vns_telemetry(NP_HUB_SLOT_VNS_HRV, &rec) == NP_HUB_OK &&
          rec.data.vns_hrv.impedance_ohm == 1200U && rec.data.vns_hrv.contact_confirmed,
          "telemetry: a finite read is reported as read");
}

/* @verifies REQ-VNSC-07 */
static void test_detection_does_not_report_a_clip_on_a_non_finite_reading(void)
{
    np_hub_mod_type_t t = NP_MOD_NONE;
    g_impedance = make_float(0x7FC00000UL);
    check(np_mod_vns_detect(NP_HUB_SLOT_VNS_HRV, &t) == NP_HUB_ERR_NOT_PRESENT,
          "detect: a NaN read is not a present clip");
    g_impedance = make_float(0x7F800000UL);
    check(np_mod_vns_detect(NP_HUB_SLOT_VNS_HRV, &t) == NP_HUB_ERR_NOT_PRESENT,
          "detect: an infinite read is not a present clip");
    g_impedance = 1200.0f;
    check(np_mod_vns_detect(NP_HUB_SLOT_VNS_HRV, &t) == NP_HUB_OK && t == NP_MOD_VNS_HRV,
          "detect: a finite in-range read is a present clip");
}

int main(void)
{
    test_a_non_finite_impedance_is_refused_and_starts_nothing();
    test_the_range_still_enables_inside_and_refuses_outside();
    test_a_non_finite_reading_is_reported_as_zero_ohm();
    test_detection_does_not_report_a_clip_on_a_non_finite_reading();
    printf("%s (%d failures)\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures);
    return g_failures == 0 ? 0 : 1;
}
