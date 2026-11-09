/*
 * NeurOne Firmware-in-the-Loop Simulator — safety MCU harness (NP-SIM-FW-001 §22, OI-SIMFW-09, REQ-SIMFW-14)
 * HOST ONLY.  Text commands that put the device's own safety MCU (firmware/safety_mcu/src, SW-01 Class C) in the loop and drive it
 * through the hub's own np_safety_spi.c.  The scenario plays two parts the device has other hardware for, and says so:
 *   - the heartbeat task: `safety_beat` and `safety_run` call np_safety_spi_heartbeat the way np_hub_control_main.c's heartbeat
 *     task does (requested mask and commanded currents from the hub's own state, then the cervical VNS module's heartbeat status);
 *   - the app: `safety_sig` signs a session hash with a TEST root key the scenario programmed into the OTP.
 *
 *   safety_root <seed> [nocrc]           program the OTP root key from a test seed (before safety_attach)
 *   safety_root_corrupt                  flip one bit of the programmed key, so it no longer matches its CRC
 *   safety_tier <1|2> <hw_version>       program the signed tier record (before safety_attach); safety_uid <byte> sets the UID
 *   safety_attach                        start the MCU
 *   safety_adc <ch> <counts>             an NTC sense count (0..5; 4095 is cold)
 *   safety_imp <ch> <ohm>                a contact impedance (0 VNS_HRV, 1 tDCS, 2 BES_TACS, 3 CVNS); safety_cvns_el <e> <ohm>
 *   safety_rpeak                         an R-peak edge now
 *   safety_beat <state>                  one heartbeat (state: np_session_state_t value; 3 is RUNNING)
 *   safety_run <ms> <state> [bpm]        heartbeats every 200 ms for <ms>, R-peaks at <bpm> if given
 *   safety_sig good|bad                  the session signature command
 *   safety_limits <ch> <area_mcm2>       one channel's electrode area; safety_wave <ch> <class> <phase_us>
 *   safety_enable <mask> | safety_disable <mask> | safety_current <ch> <ua>   the hub's own request and commanded-current state
 *   safety_user <tag> | safety_hr <valid> <hr_x10> <age_ms> | safety_reenable 0|1
 *   safety_fault nv_erase|nv_program|spi_timeout|spi_flip|stall|systick <n> [arg]
 *   safety_status
 */
#include "np_sim_bus.h"
#include "np_sim_pbm_harness.h"
#include "np_sim_safety.h"

#include "np_hub_types.h"
#include "np_safety_spi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void np_mod_cvns_set_heartbeat_status(uint16_t granted_mask, uint8_t mcu_status);
extern void np_mod_cvns_set_mcu_impedance(const float kohm[], bool valid);

/* The owner's cardiac cutoff sensitivity (REQ-CVNS-14) is a hub setting read from the config store by np_cardiac_sens.c, which the
 * simulator does not link (OI-SIMFW-30).  The driver's heart-rate report asks for it, so the simulator answers 0: the cutoff as
 * validated, never a lower threshold than the owner asked for. */
uint8_t np_cardiac_sens_level(void) { return 0U; }

static double g_next_peak_ms;    /* the R-peak train's phase, carried across safety_run calls (absolute virtual ms) */
static int    g_peak_bpm;

void np_simh_safety_init(void) { g_next_peak_ms = 0.0; g_peak_bpm = 0; }

static void state_event(const char *ev, const char *cmd, int code)
{
    np_sim_safety_info_t i;
    np_sim_safety_info(&i);
    uint8_t tier_reason = 0U;
    bool tier_refused = false;
    (void)np_safety_spi_get_tier_report(&tier_reason, &tier_refused);
    np_sim_emit("{\"ev\":\"%s\",\"cmd\":\"%s\",\"code\":%d,\"t\":%u,\"attached\":%s,\"held_in_reset\":%s,\"iwdg_resets\":%u,"
                "\"iterations\":%u,\"requested\":%u,\"granted\":%u,\"status\":%u,\"fault_slot\":%u,\"lines\":%u,"
                "\"spi_overruns\":%u,\"spi_discarded\":%u,\"tier\":%u,\"tier_reason\":%u,\"tier_refused\":%s,\"safety_mcu\":\"in_loop\"}",
                ev, cmd, code, (unsigned)np_sim_now_ms(), i.attached ? "true" : "false", i.held_in_reset ? "true" : "false",
                (unsigned)i.iwdg_resets, (unsigned)i.iterations, (unsigned)np_safety_spi_get_requested_mask(),
                (unsigned)np_safety_spi_get_granted_mask(), (unsigned)np_safety_spi_get_status(), (unsigned)i.last_reply_fault_slot,
                (unsigned)i.lines_enabled_mask, (unsigned)i.spi_overruns, (unsigned)i.spi_discarded,
                (unsigned)np_safety_spi_get_tier(), (unsigned)tier_reason, tier_refused ? "true" : "false");
}

void np_simh_safety_status(void)
{
    if (np_sim_safety_attached()) { state_event("safety_state", "status", 0); }
}

/* One heartbeat, as the heartbeat task sends it: the hub's own requested mask and commanded currents, the result published to the
 * cervical VNS module. */
static int beat(np_session_state_t state)
{
    uint16_t cur[NP_SAFETY_MAX_CHANNELS];
    np_safety_spi_get_channel_currents(cur);
    const np_hub_status_t rc = np_safety_spi_heartbeat(state, np_safety_spi_get_requested_mask(), cur, (uint8_t)NP_SAFETY_MAX_CHANNELS);
    np_mod_cvns_set_heartbeat_status(np_safety_spi_get_granted_mask(), np_safety_spi_get_status());
    float mcu_imp[NP_SAFETY_IMP_CVNS_ELECTRODES];
    bool  valid = false;
    (void)np_safety_spi_get_cvns_impedance(mcu_imp, &valid);
    np_mod_cvns_set_mcu_impedance(mcu_imp, valid);
    return (int)rc;
}

static bool parse_seed(const char *a, uint8_t seed[32])
{
    char *end = NULL;
    const unsigned long v = strtoul(a, &end, 0);
    if (end == a || v > 0xFFUL) { return false; }
    for (unsigned i = 0U; i < 32U; i++) { seed[i] = (uint8_t)(v + i); }
    return true;
}

bool np_simh_safety_line(const char *cmd, char *a, char *b, char *c)
{
    if (strncmp(cmd, "safety_", 7) != 0) { return false; }
    if (strcmp(cmd, "safety_root") == 0 && a) {
        uint8_t seed[32];
        if (!parse_seed(a, seed)) { return true; }
        np_sim_safety_otp_program_root(seed, !(b && strcmp(b, "nocrc") == 0));
    } else if (strcmp(cmd, "safety_root_corrupt") == 0) {
        np_sim_safety_otp_corrupt_root_bit();
    } else if (strcmp(cmd, "safety_uid") == 0 && a) {
        uint8_t uid[12];
        memset(uid, (int)strtoul(a, NULL, 0), sizeof uid);
        np_sim_safety_set_uid(uid);
    } else if (strcmp(cmd, "safety_tier") == 0 && a && b) {
        np_sim_safety_otp_program_tier((uint8_t)atoi(a), (uint8_t)atoi(b));
    } else if (strcmp(cmd, "safety_attach") == 0) {
        const bool ok = np_sim_safety_attach();
        state_event("safety_state", cmd, ok ? 0 : -1);
    } else if (strcmp(cmd, "safety_adc") == 0 && a && b) {
        np_sim_safety_set_adc((uint8_t)atoi(a), (uint16_t)atoi(b));
    } else if (strcmp(cmd, "safety_imp") == 0 && a && b) {
        np_sim_safety_set_impedance_ohm((uint8_t)atoi(a), (uint32_t)strtoul(b, NULL, 10));
    } else if (strcmp(cmd, "safety_cvns_el") == 0 && a && b) {
        np_sim_safety_set_cvns_electrode_ohm((uint8_t)atoi(a), (uint32_t)strtoul(b, NULL, 10));
    } else if (strcmp(cmd, "safety_rpeak") == 0) {
        np_sim_safety_rpeak();
    } else if (strcmp(cmd, "safety_beat") == 0 && a) {
        state_event("safety_state", cmd, beat((np_session_state_t)atoi(a)));
    } else if (strcmp(cmd, "safety_run") == 0 && a && b) {
        const long total = atol(a);
        const int bpm = c ? atoi(c) : 0;
        const double period = bpm > 0 ? 60000.0 / (double)bpm : 0.0;
        if (bpm != g_peak_bpm) {                      /* a new rate starts its train one period after now */
            g_peak_bpm = bpm;
            g_next_peak_ms = (double)np_sim_now_ms() + period;
        }
        int rc = 0;
        for (long t = 0; t < total; t += 200) {
            rc = beat((np_session_state_t)atoi(b));
            for (int ms = 0; ms < 200 && (t + ms) < total; ms++) {
                np_sim_advance_ms(1U);
                if (period > 0.0 && (double)np_sim_now_ms() >= g_next_peak_ms) { np_sim_safety_rpeak(); g_next_peak_ms += period; }
            }
        }
        state_event("safety_state", cmd, rc);
    } else if (strcmp(cmd, "safety_sig") == 0 && a) {
        uint8_t hash[32], sig[64];
        for (unsigned i = 0U; i < 32U; i++) { hash[i] = (uint8_t)(0x5AU + i); }
        if (!np_sim_safety_sign_session(hash, sig)) { memset(sig, 0, sizeof sig); }
        if (strcmp(a, "bad") == 0) { sig[10] ^= 0x01U; }
        state_event("safety_state", cmd, (int)np_safety_spi_send_session_sig(hash, sig));
    } else if (strcmp(cmd, "safety_limits") == 0 && a && b) {
        uint16_t area[NP_SAFETY_MAX_CHANNELS];
        memset(area, 0, sizeof area);
        const unsigned ch = (unsigned)atoi(a);
        if (ch < NP_SAFETY_MAX_CHANNELS) { area[ch] = (uint16_t)atoi(b); }
        state_event("safety_state", cmd, (int)np_safety_spi_send_channel_limits(area, (uint8_t)NP_SAFETY_MAX_CHANNELS));
    } else if (strcmp(cmd, "safety_wave") == 0 && a && b && c) {
        uint8_t  cls[NP_SAFETY_MAX_CHANNELS];
        uint32_t ph[NP_SAFETY_MAX_CHANNELS];
        memset(cls, 0, sizeof cls); memset(ph, 0, sizeof ph);
        const unsigned ch = (unsigned)atoi(a);
        if (ch < NP_SAFETY_MAX_CHANNELS) { cls[ch] = (uint8_t)atoi(b); ph[ch] = (uint32_t)strtoul(c, NULL, 10); }
        state_event("safety_state", cmd, (int)np_safety_spi_send_channel_waveforms(cls, ph, (uint8_t)NP_SAFETY_MAX_CHANNELS));
    } else if (strcmp(cmd, "safety_user") == 0 && a) {
        state_event("safety_state", cmd, (int)np_safety_spi_send_active_user((uint32_t)strtoul(a, NULL, 0)));
    } else if (strcmp(cmd, "safety_hr") == 0 && a && b && c) {
        state_event("safety_state", cmd, (int)np_safety_spi_send_hr_report(atoi(a) != 0, (uint16_t)atoi(b), (uint16_t)atoi(c),
                                                                                   0U /* sens_level: the cutoff as validated, REQ-CVNS-14 */));
    } else if (strcmp(cmd, "safety_enable") == 0 && a) {                    /* what a modality driver does beside its own command */
        np_safety_spi_request_enable((uint16_t)strtoul(a, NULL, 0));
    } else if (strcmp(cmd, "safety_disable") == 0 && a) {
        np_safety_spi_request_disable((uint16_t)strtoul(a, NULL, 0));
    } else if (strcmp(cmd, "safety_current") == 0 && a && b) {
        np_safety_spi_set_channel_current((uint8_t)atoi(a), (uint16_t)atoi(b));
    } else if (strcmp(cmd, "safety_reenable") == 0 && a) {
        np_safety_spi_set_cvns_reenable(atoi(a) != 0);
    } else if (strcmp(cmd, "safety_fault") == 0 && a && b) {
        const uint32_t n = (uint32_t)strtoul(b, NULL, 10);
        const uint32_t arg = c ? (uint32_t)strtoul(c, NULL, 10) : 0U;
        if      (strcmp(a, "nv_erase") == 0)    { np_sim_safety_inject(NP_SIM_SAFETY_FAULT_NV_ERASE, n, arg); }
        else if (strcmp(a, "nv_program") == 0)  { np_sim_safety_inject(NP_SIM_SAFETY_FAULT_NV_PROGRAM, n, arg); }
        else if (strcmp(a, "spi_timeout") == 0) { np_sim_safety_inject(NP_SIM_SAFETY_FAULT_SPI_TIMEOUT, n, arg); }
        else if (strcmp(a, "spi_flip") == 0)    { np_sim_safety_inject(NP_SIM_SAFETY_FAULT_SPI_FLIP, n, arg); }
        else if (strcmp(a, "stall") == 0)       { np_sim_safety_inject(NP_SIM_SAFETY_FAULT_STALL, n, arg); }
        else if (strcmp(a, "systick") == 0)     { np_sim_safety_inject(NP_SIM_SAFETY_FAULT_SYSTICK_FREEZE, n, arg); }
    } else if (strcmp(cmd, "safety_status") == 0) {
        state_event("safety_state", cmd, 0);
    }
    return true;
}
