/*
 * NeurOne Hub Control — Stimulation Driver Slot-Dispatch Host Tests
 *
 * np_mod_stim.c drives BES/tACS and tDCS from one piece of silicon across two
 * slots (NP_HUB_SLOT_BES_TACS / NP_HUB_SLOT_TDCS). Until the socket-addressing
 * migration it had no probe-table entry at all, so nothing ever called it and
 * its slot handling was never exercised: state_for_slot() ignored its argument
 * and returned the BES state for both slots, and detect() discriminated on
 * NP_HUB_SLOT_EEG + 1 — a slot number the driver was never assigned.
 *
 * These tests pin the property that matters: the SLOT decides which
 * stimulation channel is driven. Getting it wrong means DC priming parameters
 * delivered as an AC waveform, so it is not a thing to leave to inspection.
 *
 * No FreeRTOS, no hardware — the stim HAL and the safety/log seams are stubbed.
 * IEC 62304 Class B — SW-02 hub control.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../include/np_hub_types.h"

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

/* ── Driver under test ────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_stim_detect(uint8_t slot, np_hub_mod_type_t *type_out);
np_hub_status_t np_mod_stim_init(uint8_t slot);
np_hub_status_t np_mod_stim_control(uint8_t slot, const void *params, uint16_t len);
np_hub_status_t np_mod_stim_telemetry(uint8_t slot, np_telem_record_t *out);
np_hub_status_t np_mod_stim_shutdown(uint8_t slot);

/* ── HAL + seam stubs: record what the driver asked the hardware to do ────────── */

static int      g_dac_set_calls,  g_dac_stop_calls;
static int      g_dc_ramp_calls;
static uint16_t g_dac_freq_mhz,   g_dac_amp_ua;
static uint16_t g_dc_target_ua,   g_dc_ramp_s;
static uint16_t g_enabled_bits,   g_disabled_bits;

static void reset_stubs(void)
{
    g_dac_set_calls = g_dac_stop_calls = g_dc_ramp_calls = 0;
    g_dac_freq_mhz = g_dac_amp_ua = 0;
    g_dc_target_ua = g_dc_ramp_s = 0;
    g_enabled_bits = g_disabled_bits = 0;
}

np_hub_status_t np_mod_stim_hal_dac_set(uint8_t pair, uint8_t waveform,
                                        uint16_t freq_mhz, uint16_t amp_ua)
{
    (void)pair; (void)waveform;
    g_dac_set_calls++;
    g_dac_freq_mhz = freq_mhz;
    g_dac_amp_ua   = amp_ua;
    return NP_HUB_OK;
}

np_hub_status_t np_mod_stim_hal_dac_stop(uint8_t pair)
{
    (void)pair; g_dac_stop_calls++; return NP_HUB_OK;
}

np_hub_status_t np_mod_stim_hal_dc_set(uint8_t pair, uint16_t current_ua, uint8_t polarity)
{
    (void)pair; (void)current_ua; (void)polarity; return NP_HUB_OK;
}

np_hub_status_t np_mod_stim_hal_dc_ramp(uint8_t pair, uint16_t target_ua,
                                        uint8_t polarity, uint16_t ramp_s)
{
    (void)pair; (void)polarity;
    g_dc_ramp_calls++;
    g_dc_target_ua = target_ua;
    g_dc_ramp_s    = ramp_s;
    return NP_HUB_OK;
}

float    np_mod_stim_hal_read_impedance(uint8_t pair) { (void)pair; return 5.0f; }
float    np_mod_stim_hal_read_current(uint8_t pair)   { (void)pair; return 500.0f; }
uint16_t np_mod_stim_hal_read_charge(uint8_t pair)    { (void)pair; return 10U; }

void np_safety_spi_request_enable(uint16_t bits)  { g_enabled_bits  |= bits; }
void np_safety_spi_request_disable(uint16_t bits) { g_disabled_bits |= bits; }

void np_log_shdr_fault(uint8_t slot, uint8_t mod_type, uint8_t code, uint32_t t)
{
    (void)slot; (void)mod_type; (void)code; (void)t;
}

/* ── Tests ────────────────────────────────────────────────────────────────────── */

static void test_detect_is_per_slot(void)
{
    np_hub_mod_type_t t = NP_MOD_NONE;

    check(np_mod_stim_detect(NP_HUB_SLOT_BES_TACS, &t) == NP_HUB_OK &&
          t == NP_MOD_BES_TACS,
          "detect: the BES slot reports NP_MOD_BES_TACS");

    t = NP_MOD_NONE;
    check(np_mod_stim_detect(NP_HUB_SLOT_TDCS, &t) == NP_HUB_OK &&
          t == NP_MOD_TDCS,
          "detect: the tDCS slot reports NP_MOD_TDCS");

    t = NP_MOD_NONE;
    check(np_mod_stim_detect(NP_HUB_SLOT_AUDIO, &t) == NP_HUB_ERR_NOT_PRESENT,
          "detect: a slot this driver does not own reports NOT_PRESENT");
}

/*
 * The core property. A tDCS command must reach the DC ramp path, not the AC
 * waveform path — those carry different parameter structs at the same offsets,
 * so a misrouted tDCS command would feed current_ua into freq_mhz.
 */
static void test_control_routes_by_slot(void)
{
    np_mod_stim_init(NP_HUB_SLOT_BES_TACS);

    /* tACS: 10 Hz, 800 µA, sinusoidal on pair 0. */
    np_mod_bes_tacs_params_t bes;
    memset(&bes, 0, sizeof bes);
    bes.electrode_pair = 0U;
    bes.freq_mhz       = 10000U;
    bes.amplitude_ua   = 800U;

    reset_stubs();
    check(np_mod_stim_control(NP_HUB_SLOT_BES_TACS, &bes, sizeof bes) == NP_HUB_OK,
          "control: a BES command on the BES slot is accepted");
    check(g_dac_set_calls == 1 && g_dc_ramp_calls == 0,
          "control: BES drives the AC waveform path, not the DC ramp");
    check(g_dac_freq_mhz == 10000U && g_dac_amp_ua == 800U,
          "control: BES parameters reach the DAC unmangled");
    check(g_enabled_bits == NP_SAFETY_EN_BES_TACS,
          "control: BES requests the BES safety-MCU enable bit only");

    /* tDCS: 1500 µA with a 60 s ramp on pair 1. */
    np_mod_tdcs_params_t tdcs;
    memset(&tdcs, 0, sizeof tdcs);
    tdcs.electrode_pair = 1U;
    tdcs.current_ua     = 1500U;
    tdcs.ramp_s         = 60U;

    reset_stubs();
    check(np_mod_stim_control(NP_HUB_SLOT_TDCS, &tdcs, sizeof tdcs) == NP_HUB_OK,
          "control: a tDCS command on the tDCS slot is accepted");
    check(g_dc_ramp_calls == 1 && g_dac_set_calls == 0,
          "control: tDCS drives the DC ramp path, not the AC waveform");
    check(g_dc_target_ua == 1500U && g_dc_ramp_s == 60U,
          "control: tDCS parameters reach the ramp unmangled");
    check(g_enabled_bits == NP_SAFETY_EN_TDCS,
          "control: tDCS requests the tDCS safety-MCU enable bit only");
}

static void test_stop_uses_the_right_channel(void)
{
    np_mod_stim_init(NP_HUB_SLOT_BES_TACS);

    np_mod_tdcs_params_t tdcs;
    memset(&tdcs, 0, sizeof tdcs);
    tdcs.electrode_pair = 1U;
    tdcs.current_ua     = 1000U;
    tdcs.ramp_s         = 30U;
    np_mod_stim_control(NP_HUB_SLOT_TDCS, &tdcs, sizeof tdcs);

    reset_stubs();
    check(np_mod_stim_control(NP_HUB_SLOT_TDCS, NULL, 0U) == NP_HUB_OK,
          "stop: a tDCS stop is accepted");
    check(g_disabled_bits == NP_SAFETY_EN_TDCS,
          "stop: a tDCS stop de-requests the tDCS bit, not the BES bit");
    check(g_dc_ramp_calls == 1 && g_dac_stop_calls == 0,
          "stop: a tDCS stop ramps down rather than cutting the AC waveform");
}

/* Independent state: stopping one channel must not stop the other. */
static void test_channels_hold_independent_state(void)
{
    np_mod_stim_init(NP_HUB_SLOT_BES_TACS);

    np_mod_bes_tacs_params_t bes;
    memset(&bes, 0, sizeof bes);
    bes.freq_mhz = 10000U;
    bes.amplitude_ua = 500U;
    np_mod_stim_control(NP_HUB_SLOT_BES_TACS, &bes, sizeof bes);

    np_mod_tdcs_params_t tdcs;
    memset(&tdcs, 0, sizeof tdcs);
    tdcs.current_ua = 1000U;
    tdcs.ramp_s = 30U;
    np_mod_stim_control(NP_HUB_SLOT_TDCS, &tdcs, sizeof tdcs);

    /* Stop tDCS only; BES must still be active, so its stop must still act. */
    np_mod_stim_control(NP_HUB_SLOT_TDCS, NULL, 0U);

    reset_stubs();
    np_mod_stim_control(NP_HUB_SLOT_BES_TACS, NULL, 0U);
    check(g_dac_stop_calls == 1 && g_disabled_bits == NP_SAFETY_EN_BES_TACS,
          "state: stopping tDCS left BES running, and its own stop still acts");
}

/*
 * Exact length, not "at least". A payload that is not exactly the struct is a
 * producer disagreeing about the wire contract; reinterpreting it delivers the
 * wrong parameters. This is what pins hubCompiler.ts's encodeBESTacs to 7
 * bytes — np_mod_bes_tacs_params_t is packed and has no alignment padding.
 */
static void test_param_length_is_exact(void)
{
    np_mod_stim_init(NP_HUB_SLOT_BES_TACS);

    check(sizeof(np_mod_bes_tacs_params_t) == 7U,
          "layout: np_mod_bes_tacs_params_t is 7 bytes (packed, no padding)");
    check(sizeof(np_mod_tdcs_params_t) == 6U,
          "layout: np_mod_tdcs_params_t is 6 bytes");

    uint8_t buf[16];
    memset(buf, 0, sizeof buf);

    check(np_mod_stim_control(NP_HUB_SLOT_BES_TACS, buf,
                              sizeof(np_mod_bes_tacs_params_t) + 1U)
          == NP_HUB_ERR_INVALID_ARG,
          "length: an over-long BES payload is refused, not truncated");
    check(np_mod_stim_control(NP_HUB_SLOT_BES_TACS, buf,
                              sizeof(np_mod_bes_tacs_params_t) - 1U)
          == NP_HUB_ERR_INVALID_ARG,
          "length: a short BES payload is refused");
    check(np_mod_stim_control(NP_HUB_SLOT_TDCS, buf,
                              sizeof(np_mod_tdcs_params_t) + 1U)
          == NP_HUB_ERR_INVALID_ARG,
          "length: an over-long tDCS payload is refused");
}

static void test_unowned_slot_is_refused(void)
{
    np_mod_bes_tacs_params_t bes;
    memset(&bes, 0, sizeof bes);
    check(np_mod_stim_control(NP_HUB_SLOT_VISUAL, &bes, sizeof bes)
          == NP_HUB_ERR_INVALID_ARG,
          "control: a slot this driver does not own is refused");
}

int main(void)
{
    test_detect_is_per_slot();
    test_control_routes_by_slot();
    test_stop_uses_the_right_channel();
    test_channels_hold_independent_state();
    test_param_length_is_exact();
    test_unowned_slot_is_refused();

    if (g_failures == 0) {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d TEST(S) FAILED\n", g_failures);
    return 1;
}
