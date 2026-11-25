/*
 * Host tests for np_audio_synth.c (NP-HW-AUDIO-001 REQ-AUDIO-29): a carrier gated at a pulse rate.
 *
 * The figures below are TEST INPUTS, not product values.  The duty cycle, the edge ramp and the sample rate have no
 * stated source (OI-AUDIOHW-14, OI-AUDIOHW-18); each test says what it asserts about them and no more.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "np_audio_synth.h"

static int g_failures;
static void check(int cond, const char *name)
{
    if (cond) { printf("PASS: %s\n", name); } else { printf("FAIL: %s\n", name); g_failures++; }
}

#define FS 48000U
static np_audio_synth_cfg_t base(void)
{
    np_audio_synth_cfg_t c = { FS, 440U, 40000U, 500U, 2000U, 20000 };
    return c;
}

/* @verifies REQ-AUDIO-29 */
static void test_the_envelope_repeats_at_the_pulse_rate_and_is_silent_between_pulses(void)
{
    np_audio_synth_cfg_t c = base();             /* 40 Hz, 50 % on: 12.5 ms on, 12.5 ms off */
    np_audio_synth_t s;
    check(np_audio_synth_init(&s, &c) == NP_HUB_OK, "a stated configuration is accepted");
    enum { N = FS };                              /* one second */
    static int16_t buf[N];
    np_audio_synth_render(&s, buf, N);
    /* Count pulses: a pulse starts when a 1 ms window with any signal follows a 1 ms window with none. */
    int pulses = 0, prev = 0;
    long silent_in_off = 0, off_samples = 0;
    const int W = (int)(FS / 1000U);
    for (uint32_t i = 0; i + W <= N; i += W) {
        int any = 0;
        for (int k = 0; k < W; k++) { if (buf[i + k] != 0) { any = 1; } }
        if (any && !prev) { pulses++; }
        prev = any;
    }
    check(pulses == 40, "one second holds 40 pulses at 40 Hz");
    /* The second half of each period is off (duty 50 %): every sample there is zero. */
    uint32_t period = FS / 40U;
    for (uint32_t i = 0; i < N; i++) {
        uint32_t pos = i % period;
        if (pos > period / 2U + 2U && pos < period - 2U) { off_samples++; if (buf[i] == 0) { silent_in_off++; } }
    }
    check(off_samples > 0 && silent_in_off == off_samples, "every sample between pulses is zero");
}

/* @verifies REQ-AUDIO-29 */
static void test_the_pulsed_tone_is_the_carrier_and_never_exceeds_the_amplitude(void)
{
    np_audio_synth_cfg_t c = base();
    np_audio_synth_t s;
    (void)np_audio_synth_init(&s, &c);
    enum { N = FS / 4 };
    static int16_t buf[N];
    np_audio_synth_render(&s, buf, N);
    int peak = 0;
    for (uint32_t i = 0; i < N; i++) { int a = abs(buf[i]); if (a > peak) { peak = a; } }
    check(peak <= 20000 && peak >= 19000, "the peak reaches the stated amplitude and never passes it");
    /* With the gate always open (duty 100 %, a hard edge) one second is the bare carrier: 440 Hz is 880 sign changes. */
    np_audio_synth_cfg_t open_gate = base();
    open_gate.duty_permille = 1000U;
    open_gate.ramp_us = 0U;
    (void)np_audio_synth_init(&s, &open_gate);
    static int16_t one_second[FS];
    np_audio_synth_render(&s, one_second, FS);
    int cross = 0;
    for (uint32_t i = 1; i < FS; i++) { if ((one_second[i - 1] < 0) != (one_second[i] < 0)) { cross++; } }
    check(cross >= 878 && cross <= 882, "with the gate open the tone is the carrier, 440 Hz");
}

/* @verifies REQ-AUDIO-29 */
static void test_a_configuration_that_cannot_be_rendered_is_refused_and_changes_nothing(void)
{
    np_audio_synth_t s;
    memset(&s, 0x5A, sizeof(s));
    np_audio_synth_t before = s;
    np_audio_synth_cfg_t c;
    int ok = 1;
    c = base(); c.sample_rate_hz = 0;       ok &= np_audio_synth_init(&s, &c) == NP_HUB_ERR_INVALID_ARG;
    c = base(); c.carrier_hz = 0;           ok &= np_audio_synth_init(&s, &c) == NP_HUB_ERR_INVALID_ARG;
    c = base(); c.rate_mhz = 0;             ok &= np_audio_synth_init(&s, &c) == NP_HUB_ERR_INVALID_ARG;
    c = base(); c.duty_permille = 0;        ok &= np_audio_synth_init(&s, &c) == NP_HUB_ERR_INVALID_ARG;
    c = base(); c.duty_permille = 1001;     ok &= np_audio_synth_init(&s, &c) == NP_HUB_ERR_INVALID_ARG;
    c = base(); c.amplitude = 0;            ok &= np_audio_synth_init(&s, &c) == NP_HUB_ERR_INVALID_ARG;
    c = base(); c.carrier_hz = FS / 2U;     ok &= np_audio_synth_init(&s, &c) == NP_HUB_ERR_INVALID_ARG;   /* Nyquist */
    c = base(); c.rate_mhz = FS * 500U;     ok &= np_audio_synth_init(&s, &c) == NP_HUB_ERR_INVALID_ARG;
    c = base(); c.ramp_us = 7000U;          ok &= np_audio_synth_init(&s, &c) == NP_HUB_ERR_INVALID_ARG;   /* > half of 12.5 ms */
    check(ok, "each unrenderable configuration is refused with INVALID_ARG");
    check(memcmp(&s, &before, sizeof(s)) == 0, "a refused configuration leaves the state untouched");
    c = base(); c.ramp_us = 0;
    check(np_audio_synth_init(&s, &c) == NP_HUB_OK, "a hard edge (ramp 0) is a stated shape and is accepted");
    c = base(); c.duty_permille = 1000; c.ramp_us = 0;
    check(np_audio_synth_init(&s, &c) == NP_HUB_OK, "a duty of 100 % is a stated shape and is accepted");
}

/* @verifies REQ-AUDIO-29 */
static void test_rendering_is_continuous_across_calls(void)
{
    np_audio_synth_cfg_t c = base();
    np_audio_synth_t a, b;
    (void)np_audio_synth_init(&a, &c);
    (void)np_audio_synth_init(&b, &c);
    enum { N = 4800 };
    static int16_t whole[N], parts[N];
    np_audio_synth_render(&a, whole, N);
    np_audio_synth_render(&b, parts, 1000);
    np_audio_synth_render(&b, parts + 1000, 3800);
    check(memcmp(whole, parts, sizeof(whole)) == 0, "two renders in a row equal one render of the same length");
}

int main(void)
{
    test_the_envelope_repeats_at_the_pulse_rate_and_is_silent_between_pulses();
    test_the_pulsed_tone_is_the_carrier_and_never_exceeds_the_amplitude();
    test_a_configuration_that_cannot_be_rendered_is_refused_and_changes_nothing();
    test_rendering_is_continuous_across_calls();
    printf("%s (%d failures)\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures);
    return g_failures == 0 ? 0 : 1;
}
