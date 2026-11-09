/*
 * NeurOne Hub Control — Audio Driver Host Tests
 * Document: NP-FW-HUB-001 §8.5, NP-HW-AUDIO-001 REQ-AUDIO-19 / REQ-AUDIO-18 (OI-SIMFW-19)
 *
 * Two behaviours of np_mod_audio.c found by the firmware-in-the-loop simulator
 * (NP-SIM-FW-001 §16):
 *   1. REQ-AUDIO-19: level_half_db is the ABSOLUTE level, dB SPL x 2, with a ceiling of 85 dB SPL
 *      (170).  The driver once passed 200 straight to the planar drivers and the bone transducer.
 *      A command above the ceiling is refused (NP_HUB_ERR_INVALID_ARG) before any state changes or output is
 *      commanded, and a running command is left as it was.
 *   2. REQ-AUDIO-18: EEG-adaptive mode retunes the BINAURAL carrier (NP-FW-HUB-001 §8.5).
 *      np_mod_audio_set_eeg_band() re-applied binaural whatever the mode, so an
 *      isochronic or noise session became binaural, and an "off" command started a
 *      binaural tone.  Only a running binaural command (mode 0) is retuned.
 *
 * Audio carries no safety-MCU enable (NP_SAFETY_EN_AUDIO = 0, REQ-AUDIO-09); this is a
 * Class B driver fix and no Class C file is touched.  No FreeRTOS, no hardware.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "np_hub_types.h"
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

np_hub_status_t np_mod_audio_init(uint8_t slot);
np_hub_status_t np_mod_audio_control(uint8_t slot, const void *params, uint16_t len);
void            np_mod_audio_set_eeg_band(uint8_t band);

/* ── Doubles: what the driver asked the output stage to do ───────────────────── */

typedef struct {
    int      binaural_calls, iso_calls, noise_calls, stop_calls, bone_set_calls, bone_stop_calls;
    uint16_t carrier_hz, beat_mhz;
    uint8_t  planar_vol, bone_vol, max_vol_seen;
} audio_log_t;

static audio_log_t g;

static void note_vol(uint8_t v) { if (v > g.max_vol_seen) { g.max_vol_seen = v; } }

np_hub_status_t np_mod_audio_hal_codec_init(void) { return NP_HUB_OK; }
void np_mod_audio_hal_set_binaural(uint16_t carrier_hz, uint16_t beat_mhz, uint8_t level_half_db)
{
    g.binaural_calls++; g.carrier_hz = carrier_hz; g.beat_mhz = beat_mhz; g.planar_vol = level_half_db;
    note_vol(level_half_db);
}
void np_mod_audio_hal_set_isochronic(uint16_t freq_hz, uint8_t level_half_db)
{
    g.iso_calls++; g.carrier_hz = freq_hz; g.planar_vol = level_half_db; note_vol(level_half_db);
}
void np_mod_audio_hal_set_noise(uint8_t type, uint8_t level_half_db)
{
    (void)type; g.noise_calls++; g.planar_vol = level_half_db; note_vol(level_half_db);
}
void np_mod_audio_hal_stop_planar(void) { g.stop_calls++; }
void np_mod_audio_hal_bone_set(uint16_t freq_hz, uint8_t level_half_db)
{
    (void)freq_hz; g.bone_set_calls++; g.bone_vol = level_half_db; note_vol(level_half_db);
}
void  np_mod_audio_hal_bone_stop(void) { g.bone_stop_calls++; }
float np_mod_audio_hal_mesh_impedance(void) { return 0.0f; }

/* ── Helpers ──────────────────────────────────────────────────────────────────── */

static void reset(void)
{
    memset(&g, 0, sizeof(g));
    (void)np_mod_audio_init(NP_HUB_SLOT_AUDIO);
}

static np_hub_status_t cmd(uint8_t mode, uint8_t level, uint8_t bone, uint8_t adaptive)
{
    np_mod_audio_params_t p;
    memset(&p, 0, sizeof(p));
    p.mode = mode;
    p.carrier_hz = 440U;
    p.beat_mhz = 6000U;
    p.level_half_db = level;
    p.bone_conduct_en = bone;
    p.eeg_adaptive = adaptive;
    return np_mod_audio_control(NP_HUB_SLOT_AUDIO, &p, (uint16_t)sizeof(p));
}

/* ── REQ-AUDIO-19: the ceiling ────────────────────────────────────────────────────── */

/* @verifies REQ-AUDIO-19#b */
static void test_a_level_above_the_ceiling_reaches_no_transducer(void)
{
    static const uint8_t OVER[] = { 171U, 200U, 255U };
    for (unsigned i = 0; i < sizeof(OVER); i++) {
        for (uint8_t mode = 0U; mode <= 4U; mode++) {
            reset();
            np_hub_status_t r = cmd(mode, OVER[i], 1U, 0U);
            check(r == NP_HUB_ERR_INVALID_ARG, "a level above 85 dB SPL is refused with INVALID_ARG");
            check(g.binaural_calls + g.iso_calls + g.noise_calls + g.bone_set_calls == 0 &&
                  g.max_vol_seen == 0U,
                  "a level above 85 dB SPL reaches neither the planar drivers nor the bone transducer");
        }
    }
}

/* @verifies REQ-AUDIO-19#b */
static void test_a_refused_level_leaves_a_running_command_as_it_was(void)
{
    reset();
    check(cmd(0U, 40U, 1U, 1U) == NP_HUB_OK, "a valid command starts");
    int stops = g.stop_calls;
    check(cmd(1U, 200U, 0U, 0U) == NP_HUB_ERR_INVALID_ARG, "an over-range command is refused");
    check(g.stop_calls == stops && g.bone_stop_calls == 0 && g.iso_calls == 0,
          "the refusal stops nothing and starts nothing");
    np_mod_audio_set_eeg_band(2U);
    check(g.binaural_calls == 2 && g.planar_vol == 40U && g.beat_mhz == 10000U,
          "the earlier binaural command is still the running one, adaptive and at its own volume");
}

/* @verifies REQ-AUDIO-19#b */
static void test_the_range_edges_still_play(void)
{
    reset();
    check(cmd(0U, 170U, 1U, 0U) == NP_HUB_OK && g.planar_vol == 170U && g.bone_vol == 170U,
          "exactly 85 dB SPL plays on both paths");
    reset();
    check(cmd(2U, 0U, 1U, 0U) == NP_HUB_OK && g.noise_calls == 1 && g.planar_vol == 0U,
          "0 is accepted and passed on as 0");
    reset();
    check(cmd(3U, 55U, 0U, 0U) == NP_HUB_OK && g.planar_vol == 55U, "a mid-range value is passed unchanged");
}

/* ── REQ-AUDIO-18: the band update ────────────────────────────────────────────── */

/* @verifies REQ-AUDIO-18 */
static void test_a_band_update_does_not_turn_another_mode_into_binaural(void)
{
    for (uint8_t mode = 1U; mode <= 4U; mode++) {
        reset();
        check(cmd(mode, 30U, 0U, 1U) == NP_HUB_OK, "a non-binaural adaptive command starts");
        int before = g.binaural_calls;
        np_mod_audio_set_eeg_band(2U);
        check(g.binaural_calls == before, "a band update starts no binaural tone under a non-binaural mode");
    }
}

/* @verifies REQ-AUDIO-18 */
static void test_a_band_update_still_retunes_a_binaural_session(void)
{
    reset();
    check(cmd(0U, 30U, 0U, 1U) == NP_HUB_OK && g.binaural_calls == 1, "a binaural adaptive command starts");
    np_mod_audio_set_eeg_band(2U);
    check(g.binaural_calls == 2 && g.beat_mhz == 10000U && g.carrier_hz == 440U && g.planar_vol == 30U,
          "alpha retunes to 10 Hz at the commanded carrier and volume");
    np_mod_audio_set_eeg_band(4U);
    check(g.beat_mhz == 40000U, "gamma retunes to 40 Hz");
    np_mod_audio_set_eeg_band(9U);
    check(g.binaural_calls == 3, "a band that is not one of the five changes nothing");
    reset();
    (void)cmd(0U, 30U, 0U, 0U);
    np_mod_audio_set_eeg_band(2U);
    check(g.binaural_calls == 1, "a command without adaptive ignores band updates");
}

/* @verifies REQ-AUDIO-18 */
static void test_a_band_update_after_a_stop_plays_nothing(void)
{
    reset();
    (void)cmd(0U, 30U, 0U, 1U);
    (void)np_mod_audio_control(NP_HUB_SLOT_AUDIO, NULL, 0U);
    int before = g.binaural_calls;
    np_mod_audio_set_eeg_band(2U);
    check(g.binaural_calls == before, "no tone after a stop");
}

int main(void)
{
    test_a_level_above_the_ceiling_reaches_no_transducer();
    test_a_refused_level_leaves_a_running_command_as_it_was();
    test_the_range_edges_still_play();
    test_a_band_update_does_not_turn_another_mode_into_binaural();
    test_a_band_update_still_retunes_a_binaural_session();
    test_a_band_update_after_a_stop_plays_nothing();
    printf("%s (%d failures)\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures);
    return g_failures == 0 ? 0 : 1;
}
