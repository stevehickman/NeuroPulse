/*
 * NeurOne Firmware-in-the-Loop Simulator — cervical VNS harness (NP-SIM-FW-001 §8 step 3, REQ-SIMFW-13)
 * HOST ONLY.  Text commands that drive the REAL np_mod_cvns.c, the cervical VNS library it integrates (session, cardiac
 * interlock with its Pan-Tompkins detector, stimulation ramp) and the re-enable manager against the sim accessory.
 *
 * The scenario plays two parts the device has other hardware for, and says so in every state event:
 *   - the safety MCU: `cvns_mcu` publishes a heartbeat status and `cvns_mcu_imp` its impedance verdict through the driver's own
 *     entry points, the way the safety-heartbeat task does.  There is no safety MCU in the loop (OI-SIMFW-09).
 *   - the PPG ISR: `cvns_run` pushes a synthetic pulse train through np_mod_cvns_push_ppg at 200 Hz while ticking the driver every
 *     100 ms, the way the runner does.  The train is deterministic: a smooth pulse per beat on a flat baseline.  It is a signal
 *     the detector can find, not a model of blood flow.
 *
 *   cvns_add <left_kohm> <right_kohm> <unix_base_s>     the accessory, with the hub-side impedance it measures
 *   cvns_absent | cvns_present | cvns_clear
 *   cvns_detect | cvns_init
 *   cvns <side> <freq_mhz> <amp_ua> <pw_us> <ramp_s> <baseline_req>   np_mod_cvns_control (side 0 right, 1 left, 2 both)
 *   cvns_mcu <granted_mask> <status>          the safety MCU's heartbeat reply (status 0 = OK)
 *   cvns_mcu_imp <left_kohm> <right_kohm>     the safety MCU's own impedance report
 *   cvns_run <ms> <bpm>                       advance, push PPG at <bpm> (0 flat signal, -1 no samples at all) and tick every 100 ms
 *   cvns_stop | cvns_suspend | cvns_shutdown
 *   cvns_reenable                             np_mod_cvns_request_reenable, the in-app confirmation
 *   cvns_sample | cvns_state | cvns_raw <len>
 *   cvns_reenable_pass 0|1                    gate 3's fresh impedance verdict
 */
#include "np_sim_bus.h"
#include "np_sim_pbm_harness.h"

#include "np_hub_types.h"
#include "np_hub_config.h"
#include "np_ramp_plan.h"
#include "np_safety_spi.h"
#include "np_sw02_platform_hal.h"
#include "np_cvns_interlock.h"
#include "np_cvns_session.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern np_hub_status_t np_mod_cvns_detect(uint8_t slot, np_hub_mod_type_t *type_out);
extern np_hub_status_t np_mod_cvns_init(uint8_t slot);
extern np_hub_status_t np_mod_cvns_control(uint8_t slot, const void *params, uint16_t len);
extern np_hub_status_t np_mod_cvns_telemetry(uint8_t slot, np_telem_record_t *out);
extern np_hub_status_t np_mod_cvns_shutdown(uint8_t slot);
extern void            np_mod_cvns_tick(uint32_t now_ms, uint32_t now_s);
extern void            np_mod_cvns_suspend(void);
extern void            np_mod_cvns_ramp(uint32_t now_ms, np_ramp_report_t *out);
extern bool            np_mod_cvns_active(void);
extern void            np_mod_cvns_push_ppg(uint32_t sample, uint32_t timestamp_ms);
extern np_hub_status_t np_mod_cvns_request_reenable(uint32_t now_s);
extern void            np_mod_cvns_set_heartbeat_status(uint16_t granted_mask, uint8_t mcu_status);
extern void            np_mod_cvns_set_mcu_impedance(const float kohm[], bool valid);
extern np_cvns_interlock_ctx_t *np_mod_cvns_test_interlock(void);
extern np_cvns_session_ctx_t   *np_mod_cvns_test_session(void);
extern bool                     np_mod_cvns_test_enable_requested(void);

#define CVNS_PPG_STEP_MS   5U       /* 200 Hz, NP_CVNS_PPG_SAMPLE_RATE_HZ */
#define CVNS_TICK_MS       100U     /* NP_CVNS_STIM_TICK_MS */

static double g_ms_since_beat;       /* the synthetic pulse train's phase, carried across cvns_run calls */

void np_simh_cvns_init(void) { g_ms_since_beat = 0.0; }

static void state_event(const char *ev, const char *cmd, int code)
{
    np_ramp_report_t r;
    np_mod_cvns_ramp(np_sim_now_ms(), &r);
    np_sim_emit("{\"ev\":\"%s\",\"cmd\":\"%s\",\"code\":%d,\"t\":%u,\"stage\":%d,\"interlock\":%d,\"active\":%s,"
                "\"enable_requested\":%s,\"requested_enables\":%u,\"granted_enables\":%u,\"ramp_phase\":%d,\"ramp_pct\":%u,"
                "\"rpeak_pulses\":%u,\"baseline_valid\":%s,\"baseline_beats\":%u,\"baseline_hr\":%.1f,\"abort_reason\":%u,\"fault_reason\":%u,\"safety_mcu\":\"scenario\"}",
                ev, cmd, code, (unsigned)np_sim_now_ms(), (int)np_cvns_session_stage(np_mod_cvns_test_session()),
                (int)np_cvns_interlock_state(np_mod_cvns_test_interlock()), np_mod_cvns_active() ? "true" : "false",
                np_mod_cvns_test_enable_requested() ? "true" : "false", (unsigned)np_safety_spi_get_requested_mask(),
                (unsigned)np_safety_spi_get_granted_mask(), (int)r.phase, (unsigned)r.pct,
                (unsigned)np_mod_cvns_test_interlock()->rpeak_pulses, np_mod_cvns_test_interlock()->baseline_valid ? "true" : "false",
                (unsigned)np_mod_cvns_test_interlock()->baseline_beats_accumulated, (double)np_mod_cvns_test_interlock()->baseline_hr_bpm,
                (unsigned)np_mod_cvns_test_session()->uhdr_record.abort_reason, (unsigned)np_mod_cvns_test_session()->uhdr_record.fault_reason);
}

void np_simh_cvns_status(void)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_CVNS, 0U);
    if (m == NULL) { return; }
    uint16_t cur[NP_SAFETY_MAX_CHANNELS];
    np_safety_spi_get_channel_currents(cur);
    np_sim_emit("{\"ev\":\"module_state\",\"id\":\"cvns:0\",\"present\":%s,\"driven\":%s,\"commanded_ua\":%u}",
                m->present ? "true" : "false", np_mod_cvns_test_enable_requested() ? "true" : "false",
                (unsigned)cur[NP_SAFETY_CH_CVNS]);
}

/* One 5 ms sample of the pulse train: a smooth pulse per beat on a flat baseline. */
static uint32_t ppg_sample(int bpm)
{
    if (bpm <= 0) { return 2000U; }
    const double period = 60000.0 / (double)bpm;
    g_ms_since_beat += (double)CVNS_PPG_STEP_MS;
    if (g_ms_since_beat >= period) { g_ms_since_beat -= period; }
    const double d = (g_ms_since_beat - 40.0) / 25.0;
    return (uint32_t)(2000.0 + 1500.0 * exp(-d * d));
}

bool np_simh_cvns_line(const char *cmd, char *a, char *b, char *c, char *d, char *e)
{
    np_sim_module_t *m = np_sim_find(NP_SIM_KIND_CVNS, 0U);
    if (strcmp(cmd, "cvns_add") == 0 && a && b && c) {
        m = np_sim_add(NP_SIM_KIND_CVNS, 0U, 0.0f);
        if (m != NULL) {
            m->cvns_kohm[0] = (float)atof(a); m->cvns_kohm[1] = (float)atof(b);
            m->cvns_unix_base_s = (uint32_t)strtoul(c, NULL, 10);
            m->cvns_reenable_pass = true;
        }
    } else if (strcmp(cmd, "cvns_absent") == 0) {
        (void)np_sim_set_present(NP_SIM_KIND_CVNS, 0U, false);
    } else if (strcmp(cmd, "cvns_present") == 0) {
        (void)np_sim_set_present(NP_SIM_KIND_CVNS, 0U, true);
    } else if (strcmp(cmd, "cvns_clear") == 0) {
        (void)np_sim_clear(NP_SIM_KIND_CVNS, 0U);
    } else if (strcmp(cmd, "cvns_reenable_pass") == 0 && a && m != NULL) {
        m->cvns_reenable_pass = atoi(a) != 0;
    } else if (strcmp(cmd, "cvns_detect") == 0) {
        np_hub_mod_type_t t = NP_MOD_NONE;
        const np_hub_status_t rc = np_mod_cvns_detect(NP_HUB_SLOT_CVNS, &t);
        np_sim_emit("{\"ev\":\"cvns_detect\",\"code\":%d,\"mod_type\":%d}", (int)rc, (int)t);
    } else if (strcmp(cmd, "cvns_init") == 0) {
        state_event("result", cmd, (int)np_mod_cvns_init(NP_HUB_SLOT_CVNS));
    } else if (strcmp(cmd, "cvns") == 0 && a && b && c && d && e) {
        int ramp = 0, base = 0;                                  /* e is the rest of the line: <ramp_s> <baseline_req> */
        if (sscanf(e, "%d %d", &ramp, &base) != 2) { return false; }
        const np_mod_cvns_params_t p = { .side = (uint8_t)atoi(a), .freq_mhz = (uint16_t)atoi(b), .amplitude_ua = (uint16_t)atoi(c),
                                         .pulse_width_us = (uint16_t)atoi(d), .ramp_s = (uint16_t)ramp, .baseline_req = (uint8_t)base };
        state_event("result", cmd, (int)np_mod_cvns_control(NP_HUB_SLOT_CVNS, &p, (uint16_t)sizeof p));
    } else if (strcmp(cmd, "cvns_mcu") == 0 && a && b) {
        np_mod_cvns_set_heartbeat_status((uint16_t)strtoul(a, NULL, 0), (uint8_t)atoi(b));
    } else if (strcmp(cmd, "cvns_mcu_imp") == 0 && a && b) {
        const float k[2] = { (float)atof(a), (float)atof(b) };
        np_mod_cvns_set_mcu_impedance(k, true);
    } else if (strcmp(cmd, "cvns_run") == 0 && a && b) {
        const long total = atol(a);
        const int bpm = atoi(b);
        for (long t = 0; t < total; t += (long)CVNS_PPG_STEP_MS) {
            np_sim_advance_ms(CVNS_PPG_STEP_MS);
            if (bpm >= 0) { np_mod_cvns_push_ppg(ppg_sample(bpm), np_sim_now_ms()); }
            if (((t + (long)CVNS_PPG_STEP_MS) % (long)CVNS_TICK_MS) == 0L) {
                np_mod_cvns_tick(np_sim_now_ms(), np_mod_cvns_hal_now_unix());
            }
        }
        state_event("cvns_state", cmd, 0);
    } else if (strcmp(cmd, "cvns_state") == 0) {
        state_event("cvns_state", cmd, 0);
    } else if (strcmp(cmd, "cvns_stop") == 0) {
        state_event("result", cmd, (int)np_mod_cvns_control(NP_HUB_SLOT_CVNS, NULL, 0U));
    } else if (strcmp(cmd, "cvns_shutdown") == 0) {
        state_event("result", cmd, (int)np_mod_cvns_shutdown(NP_HUB_SLOT_CVNS));
    } else if (strcmp(cmd, "cvns_suspend") == 0) {
        np_mod_cvns_suspend();
        state_event("result", cmd, 0);
    } else if (strcmp(cmd, "cvns_reenable") == 0) {
        state_event("result", cmd, (int)np_mod_cvns_request_reenable(np_mod_cvns_hal_now_unix()));
    } else if (strcmp(cmd, "cvns_raw") == 0 && a) {
        uint8_t zeros[16];
        memset(zeros, 0, sizeof zeros);
        const unsigned len = (unsigned)atoi(a);
        state_event("result", cmd, (int)np_mod_cvns_control(NP_HUB_SLOT_CVNS, zeros, (uint16_t)(len > sizeof zeros ? sizeof zeros : len)));
    } else if (strcmp(cmd, "cvns_sample") == 0) {
        np_telem_record_t rec;
        memset(&rec, 0, sizeof rec);
        const np_hub_status_t rc = np_mod_cvns_telemetry(NP_HUB_SLOT_CVNS, &rec);
        np_sim_emit("{\"ev\":\"cvns_sample\",\"code\":%d,\"current_ua\":%.0f,\"impedance_kohm\":%.2f,\"ramp_active\":%s}",
                    (int)rc, (double)rec.data.stim.current_ua, (double)rec.data.stim.impedance_kohm,
                    rec.data.stim.ramp_active ? "true" : "false");
    } else {
        return false;
    }
    return true;
}
