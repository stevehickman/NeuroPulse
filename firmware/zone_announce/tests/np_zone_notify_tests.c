/*
 * Host tests for np_zone_notify — the socket-keyed module-status frame encoder.
 *
 * These pin the wire contract the iOS app's GATTParser decodes. When a case here
 * changes, GATTParserTests.swift changes with it — that pairing is the point.
 */

#include "np_zone_notify.h"
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

/* ── Capture harness ──────────────────────────────────────────────────────── */

#define MAX_CAPTURED 64

static uint8_t g_frames[MAX_CAPTURED][255];
static uint8_t g_lens[MAX_CAPTURED];
static int     g_frame_count;
static bool    g_tx_should_fail;
static int     g_tx_fail_after;   /* fail on the Nth call (1-based); 0 = never */

static bool g_is_map[MAX_CAPTURED];

static bool capture_tx(const uint8_t *frame, uint8_t len, bool is_map, void *ctx)
{
    (void)ctx;
    if (g_tx_should_fail) {
        return false;
    }
    if (g_tx_fail_after > 0 && (g_frame_count + 1) >= g_tx_fail_after) {
        return false;
    }
    if (g_frame_count < MAX_CAPTURED) {
        memcpy(g_frames[g_frame_count], frame, len);
        g_lens[g_frame_count] = len;
        g_is_map[g_frame_count] = is_map;
    }
    g_frame_count++;
    return true;
}

static void reset_capture(uint8_t max_frame)
{
    memset(g_frames, 0, sizeof(g_frames));
    memset(g_lens, 0, sizeof(g_lens));
    memset(g_is_map, 0, sizeof(g_is_map));
    g_frame_count    = 0;
    g_tx_should_fail = false;
    g_tx_fail_after  = 0;
    (void)np_zone_notify_init(capture_tx, NULL, max_frame);
}

static np_zn_socket_state_t mk(uint8_t socket, np_zn_module_type_t type,
                               bool present, bool fault)
{
    np_zn_socket_state_t s;
    memset(&s, 0, sizeof(s));
    s.socket_id   = socket;
    s.module_type = type;
    s.present     = present;
    s.fault       = fault;
    return s;
}

static np_zn_socket_desc_t mkdesc(uint8_t socket, np_zn_lobe_t lobe,
                                  np_zn_side_t side, int16_t x, int16_t y,
                                  bool wired)
{
    np_zn_socket_desc_t d;
    memset(&d, 0, sizeof(d));
    d.socket_id = socket;
    d.lobe      = lobe;
    d.side      = side;
    d.x_mm      = x;
    d.y_mm      = y;
    d.wired     = wired;
    return d;
}

/* ── Record encoding ──────────────────────────────────────────────────────── */

static void test_status_record_layout(void)
{
    uint8_t buf[NP_ZN_STATUS_REC_BYTES];
    /* Firmware socket 11 (0-based) -> wire socket 12 (1-based). */
    np_zn_socket_state_t s = mk(11, NP_ZN_MODULE_EEG, true, false);

    CHECK(np_zone_notify_encode_status(&s, buf) == NP_ZN_OK, "encode ok");
    CHECK(buf[0] == 12u, "socket id is emitted 1-based (0-based 11 -> 12)");
    CHECK(buf[1] == (uint8_t)NP_ZN_MODULE_EEG, "module type in byte 1");
    CHECK((buf[2] & NP_ZN_REC_PRESENT) != 0u, "present flag in byte 2");
    CHECK((buf[2] & NP_ZN_REC_FAULT) == 0u, "fault flag clear");
    CHECK(NP_ZN_STATUS_REC_BYTES == 3u,
          "a status record is THREE bytes — anatomy lives in the socket map, "
          "because where a socket is cannot change when a module is swapped");
}

static void test_status_record_absent(void)
{
    uint8_t buf[NP_ZN_STATUS_REC_BYTES];
    np_zn_socket_state_t s = mk(4, NP_ZN_MODULE_EEG, false, false);
    CHECK(np_zone_notify_encode_status(&s, buf) == NP_ZN_OK, "encode ok");
    CHECK(buf[0] == 5u, "socket 4 -> wire 5");
    CHECK((buf[2] & NP_ZN_REC_PRESENT) == 0u, "absent socket: present clear");
    CHECK(buf[1] == (uint8_t)NP_ZN_MODULE_NONE,
          "absent socket reports type NONE, not the stale type");
}

static void test_status_record_fault_is_never_present(void)
{
    uint8_t buf[NP_ZN_STATUS_REC_BYTES];
    /* Caller says present AND fault. A module that failed identification must
     * never be advertised as usable — the app gates placement checks on this. */
    np_zn_socket_state_t s = mk(2, NP_ZN_MODULE_PBM_1064, true, true);
    CHECK(np_zone_notify_encode_status(&s, buf) == NP_ZN_OK, "encode ok");
    CHECK((buf[2] & NP_ZN_REC_FAULT) != 0u, "fault flag set");
    CHECK((buf[2] & NP_ZN_REC_PRESENT) == 0u,
          "fault forces present clear even when the caller said present");
    CHECK(buf[1] == (uint8_t)NP_ZN_MODULE_UNKNOWN,
          "faulted module reports UNKNOWN, not the type the caller guessed");
}

static void test_status_record_socket_range(void)
{
    uint8_t buf[NP_ZN_STATUS_REC_BYTES];
    /* 0-based 127 is the top of the domain -> wire 128, the largest legal id. */
    np_zn_socket_state_t top = mk(127, NP_ZN_MODULE_PBM_BASE, true, false);
    CHECK(np_zone_notify_encode_status(&top, buf) == NP_ZN_OK,
          "socket 127 (top of domain) encodes");
    CHECK(buf[0] == (uint8_t)NP_ZN_MAX_SOCKET_ID, "top socket -> wire 128");

    np_zn_socket_state_t over = mk(128, NP_ZN_MODULE_PBM_BASE, true, false);
    CHECK(np_zone_notify_encode_status(&over, buf) == NP_ZN_ERR_SOCKET_RANGE,
          "socket 128 is out of the 0-based domain and is REJECTED, not masked");

    CHECK(np_zone_notify_encode_status(NULL, buf) == NP_ZN_ERR_INVALID_ARG,
          "null state rejected");
    CHECK(np_zone_notify_encode_status(&top, NULL) == NP_ZN_ERR_INVALID_ARG,
          "null buffer rejected");
}

/* ── Socket map records (the static half of the split) ────────────────────── */

static void test_map_record_layout(void)
{
    uint8_t buf[NP_ZN_MAP_REC_BYTES];
    np_zn_socket_desc_t d = mkdesc(11, NP_ZN_LOBE_FRONTAL, NP_ZN_SIDE_LEFT,
                                   -42, 137, true);

    CHECK(np_zone_notify_encode_map(&d, buf) == NP_ZN_OK, "encode ok");
    CHECK(buf[0] == 12u, "socket id 1-based here too");
    CHECK((buf[1] & 0x07u) == (uint8_t)NP_ZN_LOBE_FRONTAL, "lobe in bits [2:0]");
    CHECK(((buf[1] >> 3) & 0x03u) == (uint8_t)NP_ZN_SIDE_LEFT, "side in bits [4:3]");
    CHECK((buf[2] & NP_ZN_MAP_WIRED) != 0u, "wired flag");
    /* int16 little-endian; -42 == 0xFFD6 */
    CHECK(buf[3] == 0xD6u && buf[4] == 0xFFu, "negative x_mm little-endian");
    CHECK(buf[5] == 137u && buf[6] == 0u, "positive y_mm little-endian");
}

static void test_map_record_midline_and_unwired(void)
{
    uint8_t buf[NP_ZN_MAP_REC_BYTES];
    /* Midline is a real, distinct value — a midline socket belongs to BOTH
     * hemisphere zones of its lobe (np_module_map.h membership rule), so it must
     * not collide with "no side reported". */
    np_zn_socket_desc_t d = mkdesc(79, NP_ZN_LOBE_OCCIPITAL, NP_ZN_SIDE_MIDLINE,
                                   0, 0, false);
    CHECK(np_zone_notify_encode_map(&d, buf) == NP_ZN_OK, "encode ok");
    CHECK(buf[0] == 80u, "socket 79 -> wire 80 (top of the current lattice)");
    CHECK((buf[1] & 0x07u) == (uint8_t)NP_ZN_LOBE_OCCIPITAL, "occipital lobe");
    CHECK(((buf[1] >> 3) & 0x03u) == (uint8_t)NP_ZN_SIDE_MIDLINE, "midline side");
    CHECK((buf[2] & NP_ZN_MAP_WIRED) == 0u,
          "a socket in the geometry table that this shell does not wire");
}

static void test_map_record_socket_range(void)
{
    uint8_t buf[NP_ZN_MAP_REC_BYTES];
    np_zn_socket_desc_t over = mkdesc(200, NP_ZN_LOBE_FRONTAL, NP_ZN_SIDE_LEFT,
                                      0, 0, true);
    CHECK(np_zone_notify_encode_map(&over, buf) == NP_ZN_ERR_SOCKET_RANGE,
          "out-of-domain socket rejected in map records too");
    CHECK(np_zone_notify_encode_map(NULL, buf) == NP_ZN_ERR_INVALID_ARG,
          "null desc rejected");
}

static void test_socket_map_run(void)
{
    reset_capture(NP_ZN_ATT_FLOOR_BYTES);

    np_zn_socket_desc_t descs[80];
    for (uint8_t i = 0u; i < 80u; i++) {
        descs[i] = mkdesc(i, NP_ZN_LOBE_PARIETAL, NP_ZN_SIDE_LEFT,
                          (int16_t)i, (int16_t)(-i), true);
    }
    CHECK(np_zone_notify_socket_map(descs, 80u) == NP_ZN_OK, "80-socket map ok");

    /* (20 - 4) / 7 = 2 records per fragment -> 40 fragments. Paid ONCE per link. */
    CHECK(g_frame_count == 40, "80 map records at 2/fragment -> 40 fragments");

    int total = 0;
    for (int f = 0; f < g_frame_count; f++) {
        CHECK(g_is_map[f], "every fragment is routed to the SOCKET_MAP path");
        CHECK((g_frames[f][1] & NP_ZN_FLAG_MAP) != 0u, "and carries the MAP kind bit");
        CHECK((g_frames[f][1] & NP_ZN_FLAG_SNAPSHOT) != 0u, "a map is always a snapshot");
        CHECK(g_lens[f] <= NP_ZN_ATT_FLOOR_BYTES, "no fragment exceeds the cap");
        total += g_frames[f][3];
        bool is_last = (f == g_frame_count - 1);
        CHECK(((g_frames[f][1] & NP_ZN_FLAG_LAST) != 0u) == is_last,
              "LAST on the final fragment only");
    }
    CHECK(total == 80, "all 80 map records survive fragmentation");
}

static void test_status_frames_are_not_map_frames(void)
{
    /* The kind bit is what stops a decoder reading 7-byte map records as 3-byte
     * status records (or the reverse) and misparsing rather than failing. */
    reset_capture(NP_ZN_ATT_FLOOR_BYTES);
    np_zn_socket_state_t s = mk(0, NP_ZN_MODULE_EEG, true, false);
    CHECK(np_zone_notify_event(&s) == NP_ZN_OK, "event ok");
    CHECK(g_is_map[0] == false, "a status frame routes to ZONE_MODULE_STATUS");
    CHECK((g_frames[0][1] & NP_ZN_FLAG_MAP) == 0u, "MAP kind bit clear");
}

static void test_map_validates_before_emitting(void)
{
    reset_capture(NP_ZN_ATT_FLOOR_BYTES);
    np_zn_socket_desc_t descs[3];
    descs[0] = mkdesc(0,   NP_ZN_LOBE_FRONTAL, NP_ZN_SIDE_LEFT, 0, 0, true);
    descs[1] = mkdesc(200, NP_ZN_LOBE_FRONTAL, NP_ZN_SIDE_LEFT, 0, 0, true);
    descs[2] = mkdesc(2,   NP_ZN_LOBE_FRONTAL, NP_ZN_SIDE_LEFT, 0, 0, true);

    CHECK(np_zone_notify_socket_map(descs, 3u) == NP_ZN_ERR_SOCKET_RANGE,
          "a bad socket anywhere fails the whole map");
    CHECK(g_frame_count == 0, "and emits NOTHING — no half-sent geometry");
}

static void test_empty_socket_map(void)
{
    reset_capture(NP_ZN_ATT_FLOOR_BYTES);
    CHECK(np_zone_notify_socket_map(NULL, 0u) == NP_ZN_OK, "empty map ok");
    CHECK(g_frame_count == 1, "one header-only fragment");
    CHECK(g_lens[0] == NP_ZN_HEADER_BYTES, "header only");
    CHECK(g_is_map[0], "still routed as a map");
}

/* ── init ─────────────────────────────────────────────────────────────────── */

static void test_init_validation(void)
{
    CHECK(np_zone_notify_init(NULL, NULL, 20u) == NP_ZN_ERR_INVALID_ARG,
          "null tx rejected");
    CHECK(np_zone_notify_init(capture_tx, NULL, NP_ZN_MIN_FRAME_BYTES - 1u)
              == NP_ZN_ERR_FRAME_TOO_SMALL,
          "frame cap below header+1 record rejected");
    CHECK(np_zone_notify_init(capture_tx, NULL, NP_ZN_MIN_FRAME_BYTES) == NP_ZN_OK,
          "exactly header+1 record accepted");
    CHECK(np_zone_notify_init(capture_tx, NULL, NP_ZN_ATT_FLOOR_BYTES) == NP_ZN_OK,
          "the 20-byte ATT floor is accepted");
}

/* ── Delta events ─────────────────────────────────────────────────────────── */

static void test_event_single_frame(void)
{
    reset_capture(NP_ZN_ATT_FLOOR_BYTES);
    np_zn_socket_state_t s = mk(11, NP_ZN_MODULE_EEG, true, false);
    CHECK(np_zone_notify_event(&s) == NP_ZN_OK, "event ok");
    CHECK(g_frame_count == 1, "one insertion -> exactly one frame");
    CHECK(g_lens[0] == NP_ZN_HEADER_BYTES + NP_ZN_STATUS_REC_BYTES, "header + 1 record");
    CHECK(g_frames[0][0] == NP_ZN_FORMAT_VERSION, "version byte");
    CHECK((g_frames[0][1] & NP_ZN_FLAG_SNAPSHOT) == 0u, "delta, not snapshot");
    CHECK((g_frames[0][1] & NP_ZN_FLAG_LAST) != 0u, "delta is self-terminating");
    CHECK(g_frames[0][2] == 0u, "fragment index 0");
    CHECK(g_frames[0][3] == 1u, "record count 1");
    CHECK(g_frames[0][NP_ZN_HEADER_BYTES] == 12u, "record carries wire socket 12");
}

static void test_event_removal(void)
{
    reset_capture(NP_ZN_ATT_FLOOR_BYTES);
    np_zn_socket_state_t s = mk(11, NP_ZN_MODULE_NONE, false, false);
    CHECK(np_zone_notify_event(&s) == NP_ZN_OK, "removal event ok");
    CHECK(g_frame_count == 1, "one frame");
    CHECK((g_frames[0][NP_ZN_HEADER_BYTES + 3] & NP_ZN_REC_PRESENT) == 0u,
          "removal clears the present bit");
}

static void test_event_bad_socket_emits_nothing(void)
{
    reset_capture(NP_ZN_ATT_FLOOR_BYTES);
    np_zn_socket_state_t s = mk(200, NP_ZN_MODULE_EEG, true, false);
    CHECK(np_zone_notify_event(&s) == NP_ZN_ERR_SOCKET_RANGE, "bad socket rejected");
    CHECK(g_frame_count == 0,
          "a rejected event emits NO frame — no header for the app to un-see");
}

static void test_event_tx_failure_reported(void)
{
    reset_capture(NP_ZN_ATT_FLOOR_BYTES);
    g_tx_should_fail = true;
    np_zn_socket_state_t s = mk(1, NP_ZN_MODULE_EEG, true, false);
    CHECK(np_zone_notify_event(&s) == NP_ZN_ERR_TX, "tx failure surfaces");
}

/* ── Snapshots + fragmentation ────────────────────────────────────────────── */

static void test_snapshot_empty(void)
{
    reset_capture(NP_ZN_ATT_FLOOR_BYTES);
    CHECK(np_zone_notify_snapshot(NULL, 0u) == NP_ZN_OK, "empty snapshot ok");
    CHECK(g_frame_count == 1, "empty inventory still emits one fragment");
    CHECK(g_lens[0] == NP_ZN_HEADER_BYTES, "header only");
    CHECK(g_frames[0][3] == 0u, "zero records");
    CHECK((g_frames[0][1] & NP_ZN_FLAG_SNAPSHOT) != 0u, "snapshot flag");
    CHECK((g_frames[0][1] & NP_ZN_FLAG_LAST) != 0u, "and it is the last");
}

static void test_snapshot_fragments_at_att_floor(void)
{
    /* 80 sockets x 4 bytes = 320 bytes of records — far past any single ATT
     * notification. This is the case that forced fragmentation into the format. */
    reset_capture(NP_ZN_ATT_FLOOR_BYTES);

    np_zn_socket_state_t states[80];
    for (uint8_t i = 0u; i < 80u; i++) {
        states[i] = mk(i, NP_ZN_MODULE_PBM_BASE, true, false);
    }
    CHECK(np_zone_notify_snapshot(states, 80u) == NP_ZN_OK, "80-socket snapshot ok");

    /* (20 - 4) / 3 = 5 records per fragment -> 16 fragments. Four fewer than the
     * 4-byte record needed, purely from moving anatomy into the socket map. */
    CHECK(g_frame_count == 16, "80 status records at 5/fragment -> 16 fragments");

    int total_records = 0;
    for (int f = 0; f < g_frame_count; f++) {
        CHECK(g_lens[f] <= NP_ZN_ATT_FLOOR_BYTES, "no fragment exceeds the cap");
        CHECK(g_frames[f][0] == NP_ZN_FORMAT_VERSION, "every fragment versioned");
        CHECK((g_frames[f][1] & NP_ZN_FLAG_SNAPSHOT) != 0u, "snapshot flag on all");
        CHECK(g_frames[f][2] == (uint8_t)f, "fragment index ascends");
        total_records += g_frames[f][3];

        bool is_last = (f == g_frame_count - 1);
        CHECK(((g_frames[f][1] & NP_ZN_FLAG_LAST) != 0u) == is_last,
              "LAST set on the final fragment only");
    }
    CHECK(total_records == 80, "all 80 records survive fragmentation");

    /* Spot-check ordering across the fragment boundary: fragment 1 record 0 is
     * firmware socket 4 -> wire 5. */
    CHECK(g_frames[1][NP_ZN_HEADER_BYTES] == 6u,
          "records stay in order across fragments (frag 1 starts at socket 5 -> wire 6)");
}

static void test_snapshot_exact_fit(void)
{
    /* Exactly 5 records at the ATT floor — one full fragment, no empty trailer. */
    reset_capture(NP_ZN_ATT_FLOOR_BYTES);
    np_zn_socket_state_t states[5];
    for (uint8_t i = 0u; i < 5u; i++) {
        states[i] = mk(i, NP_ZN_MODULE_EEG, true, false);
    }
    CHECK(np_zone_notify_snapshot(states, 5u) == NP_ZN_OK, "exact-fit snapshot ok");
    CHECK(g_frame_count == 1, "an exact fit does not emit a trailing empty fragment");
    CHECK((g_frames[0][1] & NP_ZN_FLAG_LAST) != 0u, "single fragment is LAST");
}

static void test_snapshot_validates_before_emitting(void)
{
    /* One bad socket in the middle. A snapshot is an inventory claim; emitting a
     * partial run would leave the app accumulating a sequence that never ends. */
    reset_capture(NP_ZN_ATT_FLOOR_BYTES);
    np_zn_socket_state_t states[3];
    states[0] = mk(0,   NP_ZN_MODULE_EEG, true, false);
    states[1] = mk(200, NP_ZN_MODULE_EEG, true, false);
    states[2] = mk(2,   NP_ZN_MODULE_EEG, true, false);

    CHECK(np_zone_notify_snapshot(states, 3u) == NP_ZN_ERR_SOCKET_RANGE,
          "bad socket anywhere fails the whole snapshot");
    CHECK(g_frame_count == 0, "and emits NOTHING — no half-sent inventory");
}

static void test_snapshot_aborts_on_tx_failure(void)
{
    reset_capture(NP_ZN_ATT_FLOOR_BYTES);
    g_tx_fail_after = 2;   /* second fragment fails */
    np_zn_socket_state_t states[8];
    for (uint8_t i = 0u; i < 8u; i++) {
        states[i] = mk(i, NP_ZN_MODULE_EEG, true, false);
    }
    CHECK(np_zone_notify_snapshot(states, 8u) == NP_ZN_ERR_TX,
          "transmit failure mid-run is reported");
}

static void test_snapshot_null_guard(void)
{
    reset_capture(NP_ZN_ATT_FLOOR_BYTES);
    CHECK(np_zone_notify_snapshot(NULL, 3u) == NP_ZN_ERR_INVALID_ARG,
          "non-zero count with null array rejected");
}

/* ── Format constants ─────────────────────────────────────────────────────── */

static void test_format_constants(void)
{
    /* These are the numbers GATTParser.swift decodes against. If one moves, the
     * Swift side must move in the same commit. */
    CHECK(NP_ZN_HEADER_BYTES == 4u, "header is 4 bytes");
    CHECK(NP_ZN_STATUS_REC_BYTES == 3u, "status record is 3 bytes");
    CHECK(NP_ZN_MAP_REC_BYTES == 7u, "map record is 7 bytes");
    CHECK(NP_ZN_FORMAT_VERSION == 0x02u, "format version 2 (static/dynamic split)");
    CHECK(NP_ZN_FLAG_MAP == 0x04u, "map kind bit");
    CHECK(NP_ZN_MAP_WIRED == 0x01u, "map wired flag");
    CHECK(NP_ZN_MAX_SOCKET_ID == 128u, "socket domain is the full 7-bit field");
    CHECK(NP_ZN_FLAG_SNAPSHOT == 0x01u && NP_ZN_FLAG_LAST == 0x02u, "frame flags");
    CHECK(NP_ZN_REC_PRESENT == 0x01u && NP_ZN_REC_FAULT == 0x02u, "record flags");
}

int main(void)
{
    test_format_constants();
    test_status_record_layout();
    test_status_record_absent();
    test_status_record_fault_is_never_present();
    test_status_record_socket_range();
    test_map_record_layout();
    test_map_record_midline_and_unwired();
    test_map_record_socket_range();
    test_socket_map_run();
    test_status_frames_are_not_map_frames();
    test_map_validates_before_emitting();
    test_empty_socket_map();
    test_init_validation();
    test_event_single_frame();
    test_event_removal();
    test_event_bad_socket_emits_nothing();
    test_event_tx_failure_reported();
    test_snapshot_empty();
    test_snapshot_fragments_at_att_floor();
    test_snapshot_exact_fit();
    test_snapshot_validates_before_emitting();
    test_snapshot_aborts_on_tx_failure();
    test_snapshot_null_guard();

    printf("np_zone_notify_tests: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
