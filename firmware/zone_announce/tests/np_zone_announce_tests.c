/*
 * Host tests for the np_zone_announce slot state machine.
 *
 * The point of these is the bug-2 claim, asserted behaviourally rather than by
 * inspection: a confirmed insertion must notify the COMPANION APP and must NOT
 * start bone-conduction audio. Rev A did the opposite, at a moment when the
 * transducer is not touching the user (seating a module requires the helmet off
 * the head), so the confirmation was inaudible by construction.
 *
 * The platform HAL and the audio backend are stubbed here, which is also what
 * makes this module host-testable for the first time — it was ARM-cross-only.
 */

#include "np_zone_announce.h"
#include "np_zone_notify.h"
#include "np_zone_announce_config.h"
#include <stdio.h>
#include <string.h>

static int g_failures;
static int g_checks;

#define CHECK(cond, msg)                                                        \
    do {                                                                        \
        g_checks++;                                                             \
        if (!(cond)) {                                                          \
            printf("FAIL %s:%d — %s\n", __FILE__, __LINE__, (msg));             \
            g_failures++;                                                       \
        }                                                                       \
    } while (0)

/* ── Platform HAL stubs ───────────────────────────────────────────────────── */

/* ADC counts each slot reports. NP_ZA_ADC_NO_MODULE_LO+ means empty. */
static uint16_t g_adc[NP_ZONE_COUNT];
static uint32_t g_now_ms;

/* Set true by the SAI stubs if anything ever tries to make a sound. */
static bool g_audio_started;
static int  g_sai_init_calls;

bool np_za_platform_adc_read(uint8_t slot_index, uint16_t *out_counts)
{
    if (slot_index >= NP_ZONE_COUNT) { return false; }
    *out_counts = g_adc[slot_index];
    return true;
}

bool np_za_platform_sai_init(int16_t *dma_buf, uint16_t buf_samples)
{
    (void)dma_buf; (void)buf_samples;
    g_sai_init_calls++;
    return true;
}

void np_za_platform_sai_start(void) { g_audio_started = true; }
void np_za_platform_sai_stop(void)  { }

uint32_t np_za_platform_now_ms(void) { return g_now_ms; }

/* ── Notify capture ───────────────────────────────────────────────────────── */

static uint8_t g_frames[64][255];
static uint8_t g_lens[64];
static int     g_frame_count;

static bool capture_tx(const uint8_t *frame, uint8_t len, bool is_map, void *ctx)
{
    (void)ctx; (void)is_map;
    if (g_frame_count < 64) {
        memcpy(g_frames[g_frame_count], frame, len);
        g_lens[g_frame_count] = len;
    }
    g_frame_count++;
    return true;
}

/* ── Application callbacks ────────────────────────────────────────────────── */

static int g_insert_pre, g_insert_post, g_remove_calls, g_shdr_calls;
static int g_shdr_fail_calls;

static void on_insert(np_zone_id_t zone, bool done)
{
    (void)zone;
    if (done) { g_insert_post++; } else { g_insert_pre++; }
}

static void on_remove(np_zone_id_t zone) { (void)zone; g_remove_calls++; }

static void on_shdr(const np_za_shdr_auth_entry_t *entry)
{
    g_shdr_calls++;
    if (entry->auth_result != 0U) { g_shdr_fail_calls++; }
}

/* ── Harness ──────────────────────────────────────────────────────────────── */

static void reset_all(void)
{
    for (uint8_t i = 0; i < NP_ZONE_COUNT; i++) {
        g_adc[i] = 4095u;   /* every socket empty */
    }
    g_now_ms          = 1000u;
    g_audio_started   = false;
    g_sai_init_calls  = 0;
    g_frame_count     = 0;
    g_insert_pre = g_insert_post = g_remove_calls = 0;
    g_shdr_calls = g_shdr_fail_calls = 0;

    memset(g_frames, 0, sizeof(g_frames));
    memset(g_lens, 0, sizeof(g_lens));

    (void)np_zone_notify_init(capture_tx, NULL, NP_ZN_ATT_FLOOR_BYTES);
    (void)np_za_init(on_insert, on_remove, on_shdr, 42u);
}

/* Advance the state machine far enough for settle + 3 debounce reads. */
static void run_ticks(int count, uint32_t step_ms)
{
    for (int i = 0; i < count; i++) {
        np_za_tick(g_now_ms);
        g_now_ms += step_ms;
    }
}

/* ── Tests ────────────────────────────────────────────────────────────────── */

static void test_insertion_notifies_app_and_plays_no_audio(void)
{
    reset_all();
    g_adc[0] = 2048u;    /* ZM-01 Frontal Left */

    run_ticks(10, NP_ZA_DEBOUNCE_INTERVAL_MS);

    CHECK(g_frame_count >= 1, "a confirmed insertion emits an app notification");
    CHECK(g_audio_started == false,
          "BUG 2: insertion must NOT start bone-conduction audio — the helmet is "
          "off the head at this moment and the user cannot hear it");
    CHECK(g_sai_init_calls == 0, "the audio peripheral is not even initialised");
    CHECK(g_insert_pre == 1, "insert_cb(done=false) fires once");
    CHECK(g_insert_post == 1, "insert_cb(done=true) fires once, after the notify");
    CHECK(g_shdr_calls == 1, "SHDR auth logging is preserved");
    CHECK(g_shdr_fail_calls == 0, "a good module logs a pass");

    /* The frame must name socket 1 (0-based slot 0 -> 1-based wire) and say present. */
    CHECK(g_frames[0][0] == NP_ZN_FORMAT_VERSION, "versioned frame");
    CHECK(g_frames[0][NP_ZN_HEADER_BYTES] == 1u, "slot 0 -> wire socket 1");
    CHECK((g_frames[0][NP_ZN_HEADER_BYTES + 2] & NP_ZN_REC_PRESENT) != 0u,
          "record marked present");
    CHECK(g_lens[0] == NP_ZN_HEADER_BYTES + NP_ZN_STATUS_REC_BYTES,
          "a change costs header + ONE 3-byte record — no anatomy repeated");
}

static void test_removal_notifies_app(void)
{
    reset_all();
    g_adc[0] = 2048u;
    run_ticks(10, NP_ZA_DEBOUNCE_INTERVAL_MS);
    int after_insert = g_frame_count;

    g_adc[0] = 4095u;    /* pulled */
    run_ticks(10, NP_ZA_REMOVAL_DEBOUNCE_MS + 10u);

    CHECK(g_frame_count > after_insert, "removal emits its own notification");
    CHECK(g_remove_calls == 1, "remove_cb fires once");

    const uint8_t *last = g_frames[g_frame_count - 1];
    CHECK((last[NP_ZN_HEADER_BYTES + 2] & NP_ZN_REC_PRESENT) == 0u,
          "removal clears present — a stale 'present' would let a placement gate "
          "pass on a module that is no longer there");
    CHECK(g_audio_started == false, "removal plays no audio either");
}

static void test_unknown_module_reports_once_not_every_poll(void)
{
    /* Rev A was rate-limited only incidentally, by the ~700 ms error clip it
     * played in ANNOUNCING. With the audio gone, an unidentifiable module that
     * stays seated would re-notify and re-write SHDR on every poll forever. */
    reset_all();
    g_adc[0] = 100u;     /* below ZM01_LO — unrecognised / short to GND */

    run_ticks(60, NP_ZA_DEBOUNCE_INTERVAL_MS);

    CHECK(g_frame_count == 1,
          "a stuck unidentifiable module notifies exactly once, not per poll");
    CHECK(g_shdr_fail_calls == 1,
          "and burns exactly one SHDR flash write, not one per poll");
    CHECK((g_frames[0][NP_ZN_HEADER_BYTES + 2] & NP_ZN_REC_FAULT) != 0u,
          "the record carries the fault bit");
    CHECK((g_frames[0][NP_ZN_HEADER_BYTES + 2] & NP_ZN_REC_PRESENT) == 0u,
          "a faulted module is never reported present");
    CHECK(g_insert_pre == 0 && g_insert_post == 0,
          "a faulted module never fires insert_cb");
}

static void test_fault_latch_clears_on_real_removal(void)
{
    reset_all();
    g_adc[0] = 100u;                       /* bad module seated */
    run_ticks(30, NP_ZA_DEBOUNCE_INTERVAL_MS);
    CHECK(g_frame_count == 1, "one fault report");

    g_adc[0] = 4095u;                      /* user removes it */
    run_ticks(5, NP_ZA_DEBOUNCE_INTERVAL_MS);

    g_adc[0] = 100u;                       /* bad module re-inserted */
    run_ticks(30, NP_ZA_DEBOUNCE_INTERVAL_MS);
    CHECK(g_frame_count == 2,
          "the latch clears on a real removal, so a re-insertion reports again");
}

static void test_multiple_slots_each_notify(void)
{
    /* Rev A serialised these through a bone-conduction queue because the
     * transducer is a single output. Frames have no such constraint. */
    reset_all();
    g_adc[0] = 2048u;   /* Frontal Left  */
    g_adc[1] = 2818u;   /* Frontal Right */
    g_adc[2] = 3378u;   /* Vertex        */

    run_ticks(12, NP_ZA_DEBOUNCE_INTERVAL_MS);

    CHECK(g_frame_count == 3, "three insertions produce three notifications");
    CHECK(g_insert_pre == 3, "and three insert callbacks");
    CHECK(g_audio_started == false, "still no audio, however many seat at once");
}

static void test_worn_cue_still_plays(void)
{
    /* The tone engine is retained for while-worn announcements. If this ever
     * stops working, the fix above has over-reached and deleted a capability it
     * was only supposed to re-target. */
    reset_all();
    CHECK(np_za_play_worn_cue(NP_ZONE_FRONTAL_LEFT) == NP_ZA_OK,
          "the while-worn cue API drives the retained tone engine");
    CHECK(g_audio_started == true, "and it actually starts the audio path");
    CHECK(g_sai_init_calls == 1, "initialising SAI on first use");
}

static void test_socket_map_published_once_carries_anatomy(void)
{
    /* The static half of the split: anatomy is published here, at link time, and
     * never again. Every later change about the same socket costs three bytes. */
    reset_all();
    CHECK(np_za_publish_socket_map() == NP_ZA_OK, "map publishes");
    CHECK(g_frame_count >= 1, "at least one map fragment");
    CHECK((g_frames[0][1] & NP_ZN_FLAG_MAP) != 0u, "carries the MAP kind bit");

    /* First record: slot 0 == ZM-01 Frontal Left. */
    const uint8_t *rec = &g_frames[0][NP_ZN_HEADER_BYTES];
    CHECK(rec[0] == 1u, "slot 0 -> wire socket 1");
    CHECK((rec[1] & 0x07u) == (uint8_t)NP_ZN_LOBE_FRONTAL, "frontal lobe");
    CHECK(((rec[1] >> 3) & 0x03u) == (uint8_t)NP_ZN_SIDE_LEFT, "left side");
    CHECK((rec[2] & NP_ZN_MAP_WIRED) != 0u, "wired in this shell");

    int mapped = 0;
    for (int f = 0; f < g_frame_count; f++) { mapped += g_frames[f][3]; }
    CHECK(mapped == (int)NP_ZONE_COUNT, "every socket described exactly once");

    /* An insertion afterwards must not re-send any of that. */
    int after_map = g_frame_count;
    g_adc[1] = 2818u;
    run_ticks(10, NP_ZA_DEBOUNCE_INTERVAL_MS);
    CHECK(g_frame_count == after_map + 1, "one change -> one frame");
    CHECK(g_lens[after_map] == NP_ZN_HEADER_BYTES + NP_ZN_STATUS_REC_BYTES,
          "and it is 7 bytes total, not carrying geometry again");
}

static void test_get_zone_reports_active_slot(void)
{
    reset_all();
    g_adc[0] = 2048u;
    run_ticks(10, NP_ZA_DEBOUNCE_INTERVAL_MS);

    CHECK(np_za_get_zone(0) == NP_ZONE_FRONTAL_LEFT, "slot 0 reports its zone");
    CHECK(np_za_get_zone(1) == NP_ZONE_NONE, "an empty slot reports NONE");
    CHECK(np_za_get_zone(NP_ZONE_COUNT) == NP_ZONE_NONE, "out-of-range slot is safe");
}

int main(void)
{
    test_insertion_notifies_app_and_plays_no_audio();
    test_removal_notifies_app();
    test_unknown_module_reports_once_not_every_poll();
    test_fault_latch_clears_on_real_removal();
    test_multiple_slots_each_notify();
    test_worn_cue_still_plays();
    test_socket_map_published_once_carries_anatomy();
    test_get_zone_reports_active_slot();

    printf("np_zone_announce_tests: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
