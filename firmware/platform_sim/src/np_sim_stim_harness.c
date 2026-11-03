/*
 * NeurOne Firmware-in-the-Loop Simulator — stimulation harness (NP-SIM-FW-001 §8 step 3, REQ-SIMFW-13)
 * HOST ONLY.  Text commands that drive the REAL np_mod_stim.c (BES/tACS and tDCS), np_stim_xcheck.c,
 * np_ramp_plan.c and the stimulation half of np_safety_spi.c against sim electrode pairs.
 *
 * What is the device's code here: the driver's every entry point, the commanded-versus-delivered
 * cross-check, the ramp model, and the enable and commanded-current bookkeeping the safety MCU is
 * told.  What is this file's: the calls the session runner makes around them (np_session_runner.c
 * takes the telemetry sample and hands it to np_stim_xcheck_observe, and ticks the driver).  Those two
 * call sites are copied here, not run: the runner itself is not in the loop yet (gap, NP-SIM-FW-001 §12).
 *
 * Commands (pair numbers are the firmware's electrode_pair):
 *   stim_add <pair> <kohm>                       an electrode pair, with its impedance
 *   stim_absent|stim_present|stim_clear <pair>
 *   bes  <pair> <waveform> <freq_mhz> <amp_ua>   np_mod_stim_control on the BES/tACS slot
 *   tdcs <pair> <current_ua> <polarity> <ramp_s> <area_mcm2>   ... on the tDCS slot
 *   stim_stop bes|tdcs                           np_mod_stim_control with no parameters
 *   tdcs_pause                                   np_mod_stim_tdcs_pause
 *   advance <ms>                                 move the simulated clock, tick the driver
 *   sample bes|tdcs                              the runner's telemetry sample + cross-check
 *   stim_detect <slot>                           the registry's probe (17 = BES/tACS, 18 = tDCS)
 *   stim_shutdown bes|tdcs                       np_mod_stim_shutdown, the runner's end-of-session call
 *   stim_session_start                           np_stim_xcheck_reset, the runner's session-start call
 *   stim_ramp_up_ms <pair> <current_ua> <polarity> <ramp_s>   how long the runner would wait for this tDCS command
 *   stim_raw <slot> <len>                        a command of the wrong size, or for a slot that is not this driver's
 */
#include "np_sim_bus.h"
#include "np_sim_pbm_harness.h"

#include "np_hub_types.h"
#include "np_hub_config.h"
#include "np_ramp_plan.h"
#include "np_safety_spi.h"
#include "np_stim_xcheck.h"
#include "np_sw02_platform_hal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* The driver's entry points, as np_module_registry.c holds them. */
extern np_hub_status_t np_mod_stim_init(uint8_t slot);
extern np_hub_status_t np_mod_stim_control(uint8_t slot, const void *params, uint16_t len);
extern np_hub_status_t np_mod_stim_telemetry(uint8_t slot, np_telem_record_t *out);
extern void            np_mod_stim_tick(uint32_t now_ms);
extern bool            np_mod_stim_tdcs_pause(void);
extern void            np_mod_stim_tdcs_ramp(np_ramp_report_t *out);
extern np_hub_status_t np_mod_stim_detect(uint8_t slot, np_hub_mod_type_t *type_out);
extern np_hub_status_t np_mod_stim_shutdown(uint8_t slot);
extern uint32_t        np_mod_stim_tdcs_ramp_up_ms(const void *params, uint16_t len);

/* The runner's clock is session-relative: milliseconds since the session began, here since the harness was initialised. */
static uint32_t g_session_base_ms;
static uint32_t session_ms(void) { return np_sim_now_ms() - g_session_base_ms; }

/* The runner ticks the driver about every loop pass, so the driver's clock is current when a command arrives.  The same
 * here: tick before a command, or the driver would time a ramp from a clock that is stale by however long the scenario
 * idled. */
static void tick_driver(void) { np_mod_stim_tick(session_ms()); }

void np_simh_stim_init(void)
{
    g_session_base_ms = np_sim_now_ms();
    (void)np_mod_stim_init(NP_HUB_SLOT_BES_TACS);
    np_stim_xcheck_clear();
    np_safety_spi_clear_channel_currents();
    np_safety_spi_disable_all();
}

static void stim_result(const char *cmd, int code)
{
    np_sim_emit("{\"ev\":\"result\",\"cmd\":\"%s\",\"code\":%d,\"requested_enables\":%u,\"granted_enables\":%u,"
                "\"safety_mcu\":\"not_in_loop\"}",
                cmd, code, (unsigned)np_safety_spi_get_requested_mask(), (unsigned)np_safety_spi_get_granted_mask());
}

static bool slot_from(const char *name, uint8_t *slot)
{
    if (name == NULL) { return false; }
    if (strcmp(name, "bes") == 0)  { *slot = NP_HUB_SLOT_BES_TACS; return true; }
    if (strcmp(name, "tdcs") == 0) { *slot = NP_HUB_SLOT_TDCS;     return true; }
    return false;
}

void np_simh_stim_status(void)
{
    uint16_t cur[NP_SAFETY_MAX_CHANNELS];
    for (unsigned p = 0; p < NP_SIM_MAX_MODULES; p++) {
        np_sim_module_t *m = np_sim_find(NP_SIM_KIND_STIM_PAIR, (uint16_t)p);
        if (m == NULL) { continue; }
        np_sim_emit("{\"ev\":\"module_state\",\"id\":\"%s:%u\",\"present\":%s,\"driven\":%s,\"dc\":%s,"
                    "\"amp_ua\":%u,\"dc_to_ua\":%u,\"impedance_kohm\":%.1f}",
                    np_sim_kind_name(m->kind), p, m->present ? "true" : "false", m->driven ? "true" : "false",
                    m->stim_dc ? "true" : "false", m->stim_amp_ua, m->stim_dc_to_ua, (double)m->impedance_kohm);
    }
    np_safety_spi_get_channel_currents(cur);
    np_sim_emit("{\"ev\":\"stim_channels\",\"bes_commanded_ua\":%u,\"tdcs_commanded_ua\":%u,"
                "\"bes_xcheck_latched\":%s,\"tdcs_xcheck_latched\":%s}",
                (unsigned)cur[NP_SAFETY_CH_BES_TACS], (unsigned)cur[NP_SAFETY_CH_TDCS],
                np_stim_xcheck_latched(NP_SAFETY_CH_BES_TACS) ? "true" : "false",
                np_stim_xcheck_latched(NP_SAFETY_CH_TDCS) ? "true" : "false");
}

bool np_simh_stim_line(const char *cmd, char *a, char *b, char *c, char *d, char *e)
{
    uint8_t slot;
    if (strcmp(cmd, "stim_add") == 0 && a && b) {
        (void)np_sim_add(NP_SIM_KIND_STIM_PAIR, (uint16_t)atoi(a), (float)atof(b));
    } else if (strcmp(cmd, "stim_absent") == 0 && a) {
        (void)np_sim_set_present(NP_SIM_KIND_STIM_PAIR, (uint16_t)atoi(a), false);
    } else if (strcmp(cmd, "stim_present") == 0 && a) {
        (void)np_sim_set_present(NP_SIM_KIND_STIM_PAIR, (uint16_t)atoi(a), true);
    } else if (strcmp(cmd, "stim_clear") == 0 && a) {
        (void)np_sim_clear(NP_SIM_KIND_STIM_PAIR, (uint16_t)atoi(a));
    } else if (strcmp(cmd, "bes") == 0 && a && b && c && d) {
        const np_mod_bes_tacs_params_t p = { .electrode_pair = (uint8_t)atoi(a), .waveform = (uint8_t)atoi(b),
                                             .freq_mhz = (uint16_t)atoi(c), .amplitude_ua = (uint16_t)atoi(d),
                                             .phase_deg = 0U };
        tick_driver();
        stim_result(cmd, (int)np_mod_stim_control(NP_HUB_SLOT_BES_TACS, &p, (uint16_t)sizeof p));
    } else if (strcmp(cmd, "tdcs") == 0 && a && b && c && d && e) {
        const np_mod_tdcs_params_t p = { .electrode_pair = (uint8_t)atoi(a), .current_ua = (uint16_t)atoi(b),
                                         .polarity = (uint8_t)atoi(c), .ramp_s = (uint16_t)atoi(d),
                                         .electrode_area_mcm2 = (uint16_t)atoi(e) };
        tick_driver();
        stim_result(cmd, (int)np_mod_stim_control(NP_HUB_SLOT_TDCS, &p, (uint16_t)sizeof p));
    } else if (strcmp(cmd, "stim_stop") == 0 && slot_from(a, &slot)) {
        tick_driver();
        stim_result(cmd, (int)np_mod_stim_control(slot, NULL, 0U));
    } else if (strcmp(cmd, "tdcs_pause") == 0) {
        tick_driver();
        const bool still = np_mod_stim_tdcs_pause();
        stim_result(cmd, still ? 1 : 0);          /* 1: ramping down, enable held; 0: off */
    } else if (strcmp(cmd, "advance") == 0 && a) {
        np_sim_advance_ms((uint32_t)atol(a));
        tick_driver();
        np_ramp_report_t r;
        np_mod_stim_tdcs_ramp(&r);
        np_sim_emit("{\"ev\":\"clock\",\"t\":%u,\"tdcs_ramp_phase\":%d,\"tdcs_ramp_pct\":%u,\"tdcs_ramp_remaining_ms\":%u}",
                    (unsigned)np_sim_now_ms(), (int)r.phase, (unsigned)r.pct, (unsigned)r.remaining_ms);
    } else if (strcmp(cmd, "sample") == 0 && slot_from(a, &slot)) {
        /* np_session_runner.c: telemetry, then the cross-check on the record */
        np_telem_record_t rec;
        memset(&rec, 0, sizeof rec);
        rec.session_ms = session_ms();
        const np_hub_status_t rc = np_mod_stim_telemetry(slot, &rec);
        bool tripped = false;
        if (rc == NP_HUB_OK) { tripped = np_stim_xcheck_observe(&rec, session_ms()); }
        np_sim_emit("{\"ev\":\"stim_sample\",\"slot\":%u,\"code\":%d,\"current_ua\":%.1f,\"impedance_kohm\":%.2f,"
                    "\"charge_uc\":%u,\"ramp_active\":%s,\"divergence_flagged\":%s}",
                    (unsigned)slot, (int)rc,
                    isfinite(rec.data.stim.current_ua) ? (double)rec.data.stim.current_ua : -1.0,
                    isfinite(rec.data.stim.impedance_kohm) ? (double)rec.data.stim.impedance_kohm : -1.0,
                    (unsigned)rec.data.stim.charge_uC, rec.data.stim.ramp_active ? "true" : "false",
                    tripped ? "true" : "false");
    } else if (strcmp(cmd, "stim_detect") == 0 && a) {
        np_hub_mod_type_t t = NP_MOD_NONE;
        const np_hub_status_t rc = np_mod_stim_detect((uint8_t)atoi(a), &t);
        np_sim_emit("{\"ev\":\"stim_detect\",\"slot\":%d,\"code\":%d,\"mod_type\":%d}", atoi(a), (int)rc, (int)t);
    } else if (strcmp(cmd, "stim_shutdown") == 0 && slot_from(a, &slot)) {
        tick_driver();
        stim_result(cmd, (int)np_mod_stim_shutdown(slot));
    } else if (strcmp(cmd, "stim_session_start") == 0) {
        np_stim_xcheck_reset();
        stim_result(cmd, 0);
    } else if (strcmp(cmd, "stim_ramp_up_ms") == 0 && a && b && c && d) {
        const np_mod_tdcs_params_t p = { .electrode_pair = (uint8_t)atoi(a), .current_ua = (uint16_t)atoi(b),
                                         .polarity = (uint8_t)atoi(c), .ramp_s = (uint16_t)atoi(d),
                                         .electrode_area_mcm2 = 1U };
        np_sim_emit("{\"ev\":\"stim_ramp_up\",\"ms\":%u,\"bad_length_ms\":%u}",
                    (unsigned)np_mod_stim_tdcs_ramp_up_ms(&p, (uint16_t)sizeof p),
                    (unsigned)np_mod_stim_tdcs_ramp_up_ms(&p, 1U));
    } else if (strcmp(cmd, "stim_raw") == 0 && a && b) {
        uint8_t zeros[64];
        memset(zeros, 0, sizeof zeros);
        const unsigned len = (unsigned)atoi(b);
        tick_driver();
        stim_result(cmd, (int)np_mod_stim_control((uint8_t)atoi(a), zeros, (uint16_t)(len > sizeof zeros ? sizeof zeros : len)));
    } else {
        return false;
    }
    return true;
}
