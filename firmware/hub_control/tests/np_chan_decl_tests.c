/*
 * NeurOne Hub Control — Electrical-Channel Declaration Host Tests
 * Document: NP-FW-HUB-001 Rev 2 §5.4; NP-FW-MMSOCK-001 §5.3 C-1 (OI-MMSOCK-02)
 *
 * np_chan_decl_build() decides which electrode areas, waveform classes and
 * phase durations the safety MCU is told at session start, and which of its
 * fail-closed geometry gates to arm. Until it was extracted from
 * np_runner_run() nothing tested it. The properties that matter:
 *
 *   - BES/tACS arms its own geometry gate and always sends its pad area
 *     (OI-MMSOCK-02);
 *   - ANY channel with an area is sent — the VNS / cervical-VNS / BES areas
 *     used to be dropped unless the session also held tDCS or HD-tDCS, so the
 *     MCU enforced its 25 cm² fallback on a 0.5 cm² auricular clip;
 *   - tDCS arms its gate even when it declares no area, and the smallest
 *     declared area wins (OI-CHARGE-04);
 *   - clinical tACS arms no gate (it shares CLIN_STIM and declares none).
 *
 * No FreeRTOS, no hardware. IEC 62304 Class B — SW-02 hub control.
 */

#include <stdio.h>
#include <string.h>

#include "../include/np_chan_decl.h"

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

static np_session_desc_t g_desc;

static void reset(void)
{
    memset(&g_desc, 0, sizeof g_desc);
}

static void add(np_hub_mod_type_t t, const void *params, uint16_t len)
{
    np_session_cmd_t *c = &g_desc.cmds[g_desc.cmd_count++];
    c->mod_type    = t;
    c->slot_id     = NP_HUB_SLOT_NONE;
    c->target_kind = NP_PROTO_TARGET_SLOT;
    c->params_len  = len;
    memcpy(c->params, params, len);
}

static void add_bes(uint16_t freq_mhz, uint8_t waveform)
{
    np_mod_bes_tacs_params_t p;
    memset(&p, 0, sizeof p);
    p.freq_mhz = freq_mhz; p.amplitude_ua = 1000U; p.waveform = waveform;
    add(NP_MOD_BES_TACS, &p, sizeof p);
}

static void add_tdcs(uint16_t area_mcm2)
{
    np_mod_tdcs_params_t p;
    memset(&p, 0, sizeof p);
    p.current_ua = 2000U; p.ramp_s = 30U; p.electrode_area_mcm2 = area_mcm2;
    add(NP_MOD_TDCS, &p, sizeof p);
}

static void add_vns(uint8_t pw_us)
{
    np_mod_vns_hrv_params_t p;
    memset(&p, 0, sizeof p);
    p.freq_mhz = 25000U; p.amplitude_ua = 2000U; p.pulse_width_us = pw_us;
    add(NP_MOD_VNS_HRV, &p, sizeof p);
}

static void test_bes_arms_its_gate_and_sends_its_area(void)
{
    np_chan_decl_t d;
    reset();
    add_bes(40000U, 0U);            /* 40 Hz sine, the only electrical command */
    np_chan_decl_build(&g_desc, &d);
    check(d.geom_bes, "BES/tACS arms GEOM_REQ_BES (OI-MMSOCK-02)");
    check(!d.geom_tdcs && !d.geom_clin_stim, "BES/tACS arms no other gate");
    check(d.area_mcm2[NP_SAFETY_CH_BES_TACS] == NP_BES_ELECTRODE_AREA_MCM2,
          "BES/tACS area is the fixed pad constant");
    check(d.send_limits, "BES-only session SENDS the area frame (was dropped)");
    check(d.wave_class[NP_SAFETY_CH_BES_TACS] == NP_CHARGE_WAVE_SINE,
          "sine waveform declared as SINE");
    check(d.phase_us[NP_SAFETY_CH_BES_TACS] == 12500UL, "40 Hz -> 12,500 us phase");
    check(d.send_waveforms, "waveform frame sent");
}

static void test_vns_only_session_sends_its_area(void)
{
    np_chan_decl_t d;
    reset();
    add_vns(0U);                    /* 0 -> 250 us default */
    np_chan_decl_build(&g_desc, &d);
    check(d.area_mcm2[NP_SAFETY_CH_VNS_HRV] == NP_VNS_ELECTRODE_AREA_MCM2,
          "VNS area is the 0.5 cm² clip constant");
    check(d.send_limits,
          "VNS-only session SENDS the area frame — else the MCU enforces 25 cm² (50x loose)");
    check(!d.geom_bes && !d.geom_tdcs && !d.geom_clin_stim, "VNS arms no geometry gate");
    check(d.phase_us[NP_SAFETY_CH_VNS_HRV] == 250UL, "VNS default pulse width 250 us");
}

static void test_cvns_only_session_sends_its_area(void)
{
    np_chan_decl_t d;
    np_mod_cvns_params_t p;
    reset();
    memset(&p, 0, sizeof p);
    p.freq_mhz = 25000U; p.amplitude_ua = 2000U; p.pulse_width_us = 1000U;
    add(NP_MOD_CVNS, &p, sizeof p);
    np_chan_decl_build(&g_desc, &d);
    check(d.area_mcm2[NP_SAFETY_CH_CVNS] == NP_CVNS_ELECTRODE_AREA_MCM2 && d.send_limits,
          "cervical-VNS-only session sends its area");
}

static void test_tdcs_rules_unchanged(void)
{
    np_chan_decl_t d;
    reset();
    add_tdcs(0U);                   /* declares no area */
    np_chan_decl_build(&g_desc, &d);
    check(d.geom_tdcs && d.area_mcm2[NP_SAFETY_CH_TDCS] == 0U,
          "tDCS with no area: gate armed, area left 0 (MCU never grants TDCS)");
    check(d.send_limits, "tDCS with no area still sends the frame");

    reset();
    add_tdcs(35000U);
    add_tdcs(25000U);
    add_tdcs(30000U);
    np_chan_decl_build(&g_desc, &d);
    check(d.area_mcm2[NP_SAFETY_CH_TDCS] == 25000U, "tDCS: smallest declared area wins");
    check(!d.geom_bes, "tDCS does not arm the BES gate");
}

static void test_hd_and_clinical_tacs(void)
{
    np_chan_decl_t d;
    np_mod_hd_tdcs_params_t hd;
    np_mod_clin_tacs_params_t ct;

    reset();
    memset(&ct, 0, sizeof ct);
    ct.freq_mhz = 10000U; ct.amplitude_ua = 2000U; ct.waveform = 0U;
    add(NP_MOD_CLIN_TACS, &ct, sizeof ct);
    np_chan_decl_build(&g_desc, &d);
    check(!d.geom_clin_stim && !d.send_limits,
          "clinical tACS alone: no gate, no area frame (declares no geometry)");
    check(d.send_waveforms, "clinical tACS: waveform frame sent");

    reset();
    memset(&hd, 0, sizeof hd);
    hd.montage = NP_HD_MONTAGE_RING_4X1; hd.current_ua = 2000U; hd.ramp_s = 30U;
    add(NP_MOD_HD_TDCS, &hd, sizeof hd);
    np_chan_decl_build(&g_desc, &d);
    check(d.geom_clin_stim &&
          d.area_mcm2[NP_SAFETY_CH_CLIN_STIM] == NP_HD_SMALL_ELECTRODE_AREA_MCM2,
          "HD-tDCS ring: CLIN_STIM gate armed with the 3.5 mm electrode area");
}

static void test_mixed_session_arms_each_gate_independently(void)
{
    np_chan_decl_t d;
    reset();
    add_tdcs(35000U);
    add_bes(6000U, 1U);             /* 6 Hz, biphasic square */
    add_vns(200U);
    np_chan_decl_build(&g_desc, &d);
    check(d.geom_tdcs && d.geom_bes && !d.geom_clin_stim,
          "tDCS + BES session arms exactly the tDCS and BES gates");
    check(d.area_mcm2[NP_SAFETY_CH_TDCS] == 35000U &&
          d.area_mcm2[NP_SAFETY_CH_BES_TACS] == NP_BES_ELECTRODE_AREA_MCM2 &&
          d.area_mcm2[NP_SAFETY_CH_VNS_HRV] == NP_VNS_ELECTRODE_AREA_MCM2,
          "each channel carries its own area");
    check(d.wave_class[NP_SAFETY_CH_BES_TACS] == NP_CHARGE_WAVE_PULSE,
          "square waveform declared as PULSE");
}

static void test_nothing_electrical(void)
{
    np_chan_decl_t d;
    np_mod_pbm_base_params_t p;
    reset();
    memset(&p, 0, sizeof p);
    p.cur_a = 0x40U;
    add(NP_MOD_PBM_BASE, &p, sizeof p);
    np_chan_decl_build(&g_desc, &d);
    check(!d.send_limits && !d.send_waveforms && !d.geom_bes && !d.geom_tdcs &&
          !d.geom_clin_stim, "PBM-only session declares nothing");

    np_chan_decl_build(NULL, &d);
    check(!d.send_limits && !d.send_waveforms, "NULL descriptor declares nothing");

    reset();
    {
        uint8_t shortp[2] = { 0, 0 };
        add(NP_MOD_BES_TACS, shortp, sizeof shortp);   /* truncated params */
    }
    np_chan_decl_build(&g_desc, &d);
    check(!d.geom_bes && !d.send_limits,
          "a BES command with truncated params is ignored, not half-read");
}

static void test_half_period(void)
{
    check(np_chan_decl_half_period_us(500U) == 1000000UL, "0.5 Hz -> 1,000,000 us");
    check(np_chan_decl_half_period_us(40000U) == 12500UL, "40 Hz -> 12,500 us");
    check(np_chan_decl_half_period_us(0U) == (uint32_t)NP_CHARGE_MAX_PHASE_US,
          "0 Hz -> clamp (strictest verdict)");
    check(np_chan_decl_half_period_us(1U) == (uint32_t)NP_CHARGE_MAX_PHASE_US,
          "1 mHz -> clamped");
}

int main(void)
{
    test_bes_arms_its_gate_and_sends_its_area();
    test_vns_only_session_sends_its_area();
    test_cvns_only_session_sends_its_area();
    test_tdcs_rules_unchanged();
    test_hd_and_clinical_tacs();
    test_mixed_session_arms_each_gate_independently();
    test_nothing_electrical();
    test_half_period();

    if (g_failures != 0) {
        printf("\n%d FAILURE(S)\n", g_failures);
        return 1;
    }
    printf("\nAll channel-declaration tests passed.\n");
    return 0;
}
