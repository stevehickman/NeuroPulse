/*
 * NeurOne Hub Control Program — Electrical-Channel Declarations to the Safety MCU
 * Document: NP-FW-HUB-001 Rev 2 §5.4; NP-FW-MMSOCK-001 §5.3 C-1 (OI-MMSOCK-02)
 *
 * See np_chan_decl.h. Extracted from np_runner_run() so the logic that arms
 * the safety MCU's geometry gates is host-tested (np_chan_decl_tests).
 * IEC 62304 Class B — SW-02 hub control.
 */

#include "np_chan_decl.h"
#include <string.h>

/*
 * np_chan_decl_half_period_us — the duration of ONE phase of a periodic waveform, in µs,
 * from its frequency in milli-Hz.
 *
 *   f Hz = freq_mhz / 1000, half period = 1/(2f) s = 500e6 / freq_mhz µs
 *
 * e.g. 0.5 Hz (freq_mhz = 500) → 1,000,000 µs; 40 Hz → 12,500 µs.
 *
 * A zero frequency would be a division by zero and is a malformed descriptor;
 * it returns the clamp ceiling, which is the LONGEST phase and therefore the
 * strictest per-phase verdict the safety MCU can reach — a malformed
 * descriptor fails closed rather than escaping the check.  (The MCU applies
 * the same clamp itself; this one keeps the wire value honest as well.)
 */
uint32_t np_chan_decl_half_period_us(uint16_t freq_mhz)
{
    uint32_t us;
    if (freq_mhz == 0U) {
        return (uint32_t)NP_CHARGE_MAX_PHASE_US;
    }
    us = 500000000UL / (uint32_t)freq_mhz;
    return (us > (uint32_t)NP_CHARGE_MAX_PHASE_US)
             ? (uint32_t)NP_CHARGE_MAX_PHASE_US : us;
}

/*
 * declare_phase — record a channel's phase duration, keeping the LONGEST when
 * several commands land on one channel.  Longest phase means largest per-phase
 * charge at a given amplitude, so keeping the maximum is the strict reading —
 * the same rule, in the opposite direction, as taking the SMALLEST declared
 * electrode area.  (The safety MCU applies this max independently; doing it
 * here too means the transmitted frame already says what the MCU will
 * conclude, so the two sides never disagree about what was declared.)
 */
static void declare_phase(uint32_t *phase_us, uint8_t channel, uint32_t us)
{
    if (us > phase_us[channel]) {
        phase_us[channel] = us;
    }
}

void np_chan_decl_build(const np_session_desc_t *desc, np_chan_decl_t *out)
{
    memset(out, 0, sizeof *out);
    if (desc == NULL) {
        return;
    }

    for (uint8_t i = 0U; i < desc->cmd_count; i++) {
        const np_session_cmd_t *c = &desc->cmds[i];
        if (c->mod_type == NP_MOD_HD_TDCS &&
            c->params_len >= sizeof(np_mod_hd_tdcs_params_t)) {
            const np_mod_hd_tdcs_params_t *p =
                (const np_mod_hd_tdcs_params_t *)(const void *)c->params;
            if (p->montage == NP_HD_MONTAGE_RING_4X1 ||
                p->montage == NP_HD_MONTAGE_BILATERAL_4X1) {
                out->area_mcm2[NP_SAFETY_CH_CLIN_STIM] =
                    NP_HD_SMALL_ELECTRODE_AREA_MCM2;
                out->geom_clin_stim = true;
            }
            /* HD-tDCS is DC — the per-session budget applies to it. */
            out->wave_class[NP_SAFETY_CH_CLIN_STIM] |= NP_CHARGE_WAVE_DC;
            out->send_waveforms = true;
        } else if (c->mod_type == NP_MOD_TDCS &&
                   c->params_len >= sizeof(np_mod_tdcs_params_t)) {
            const np_mod_tdcs_params_t *p =
                (const np_mod_tdcs_params_t *)(const void *)c->params;
            out->geom_tdcs = true;
            if (p->electrode_area_mcm2 > 0U &&
                (out->area_mcm2[NP_SAFETY_CH_TDCS] == 0U ||
                 p->electrode_area_mcm2 < out->area_mcm2[NP_SAFETY_CH_TDCS])) {
                out->area_mcm2[NP_SAFETY_CH_TDCS] = p->electrode_area_mcm2;
            }
            out->wave_class[NP_SAFETY_CH_TDCS] |= NP_CHARGE_WAVE_DC;
            out->send_waveforms = true;
        } else if (c->mod_type == NP_MOD_BES_TACS &&
                   c->params_len >= sizeof(np_mod_bes_tacs_params_t)) {
            const np_mod_bes_tacs_params_t *p =
                (const np_mod_bes_tacs_params_t *)(const void *)c->params;
            /* OI-MMSOCK-02: the pad is a fixed product part, so its area is
             * a device constant (OI-CHARGE-07) — but the MCU is no longer left
             * to assume it: the gate is armed and the area always sent.      */
            out->area_mcm2[NP_SAFETY_CH_BES_TACS] = NP_BES_ELECTRODE_AREA_MCM2;
            out->geom_bes = true;
            out->wave_class[NP_SAFETY_CH_BES_TACS] |=
                (p->waveform == 0U) ? NP_CHARGE_WAVE_SINE
                                    : NP_CHARGE_WAVE_PULSE;
            declare_phase(out->phase_us, NP_SAFETY_CH_BES_TACS,
                          np_chan_decl_half_period_us(p->freq_mhz));
            out->send_waveforms = true;
        } else if (c->mod_type == NP_MOD_CLIN_TACS &&
                   c->params_len >= sizeof(np_mod_clin_tacs_params_t)) {
            const np_mod_clin_tacs_params_t *p =
                (const np_mod_clin_tacs_params_t *)(const void *)c->params;
            /* Shares CLIN_STIM with HD-tDCS; classes OR together and the
             * MCU runs both checks.  Declares no geometry of its own, so
             * it never arms the OI-CHARGE-03 gate (unchanged behaviour). */
            out->wave_class[NP_SAFETY_CH_CLIN_STIM] |=
                (p->waveform == 0U) ? NP_CHARGE_WAVE_SINE
                                    : NP_CHARGE_WAVE_PULSE;
            declare_phase(out->phase_us, NP_SAFETY_CH_CLIN_STIM,
                          np_chan_decl_half_period_us(p->freq_mhz));
            out->send_waveforms = true;
        } else if (c->mod_type == NP_MOD_VNS_HRV &&
                   c->params_len >= sizeof(np_mod_vns_hrv_params_t)) {
            const np_mod_vns_hrv_params_t *p =
                (const np_mod_vns_hrv_params_t *)(const void *)c->params;
            uint32_t pw = (p->pulse_width_us == 0U)
                            ? 250UL : (uint32_t)p->pulse_width_us;
            out->area_mcm2[NP_SAFETY_CH_VNS_HRV] = NP_VNS_ELECTRODE_AREA_MCM2;
            out->wave_class[NP_SAFETY_CH_VNS_HRV] |= NP_CHARGE_WAVE_PULSE;
            declare_phase(out->phase_us, NP_SAFETY_CH_VNS_HRV, pw);
            out->send_waveforms = true;
        } else if (c->mod_type == NP_MOD_CVNS &&
                   c->params_len >= sizeof(np_mod_cvns_params_t)) {
            const np_mod_cvns_params_t *p =
                (const np_mod_cvns_params_t *)(const void *)c->params;
            uint32_t pw = (p->pulse_width_us == 0U)
                            ? 250UL : (uint32_t)p->pulse_width_us;
            out->area_mcm2[NP_SAFETY_CH_CVNS] = NP_CVNS_ELECTRODE_AREA_MCM2;
            out->wave_class[NP_SAFETY_CH_CVNS] |= NP_CHARGE_WAVE_PULSE;
            declare_phase(out->phase_us, NP_SAFETY_CH_CVNS, pw);
            out->send_waveforms = true;
        } else {
            /* modality drives no electrode */
        }
    }

    /* Rule 1 (np_chan_decl.h): any channel with an area is sent. Before the
     * extraction only an HD-tDCS or tDCS session sent the frame, and the fixed
     * VNS / cervical-VNS / BES areas were silently dropped everywhere else. */
    for (uint8_t ch = 0U; ch < NP_SAFETY_MAX_CHANNELS; ch++) {
        if (out->area_mcm2[ch] != 0U) {
            out->send_limits = true;
        }
    }
    /* A gated channel whose area is 0 (a tDCS command declaring none) still
     * needs the frame so the other channels' areas land; its own gate stays
     * shut because the MCU never receives a non-zero area for it. */
    if (out->geom_clin_stim || out->geom_tdcs || out->geom_bes) {
        out->send_limits = true;
    }
}
