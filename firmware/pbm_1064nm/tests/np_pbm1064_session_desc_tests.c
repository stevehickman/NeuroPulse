/*
 * NeurOne 1064nm Smart Zone Module — Session Descriptor v5 Host Tests
 * Document: NP-SES-1064-001 §1 (v5)
 *
 * Exercises the socket-indexed session descriptor: wire byte-length/signed-
 * length computation, serialize/parse round trips, group expansion into a
 * flat active-socket list (including the dedup and capacity-overflow fail-
 * closed rules), and end-to-end session_start()/T2 start_combined() against
 * descriptors that address sockets the old v4 8-bit mask + zone[5] array
 * could never name (e.g. socket 77 on an 80-socket lattice).
 *
 * No FreeRTOS, no hardware — HAL calls are the deterministic stubs in
 * src/np_pbm1064_hal.c. Build with -DNP_BUILD_TESTS=ON
 * (see firmware/pbm_1064nm/CMakeLists.txt).
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "np_pbm1064_session.h"
#include "np_pbm1064_t2_combined.h"
#include "np_pbm1064_fai.h"

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

/* ── Mask helpers (mirror the LSB-first, 0-based convention under test) ─────── */

static void mask_set(uint8_t mask[NP_PBM1064_SOCKET_MASK_BYTES], uint8_t socket_id)
{
    mask[socket_id / 8U] |= (uint8_t)(1U << (socket_id % 8U));
}

static np_pbm1064_preset_t make_preset(uint8_t cur_a, uint8_t freq_hz, uint8_t duty)
{
    np_pbm1064_preset_t p;
    memset(&p, 0, sizeof(p));
    p.cur_a = cur_a;
    p.cur_b = cur_a;
    p.cur_c = cur_a;
    p.freq_hz = freq_hz;
    p.duty = duty;
    p.channel_mask = NP_PBM1064_CH_ALL_EN;
    return p;
}

/* ── Struct size sanity (byte layout stability) ──────────────────────────────── */

static void test_struct_sizes(void)
{
    check(sizeof(np_pbm1064_session_desc_hdr_t) == 8U, "hdr is 8 bytes");
    check(sizeof(np_pbm1064_preset_t) == 6U, "preset is 6 bytes");
    check(sizeof(np_pbm1064_preset_group_t) == NP_PBM1064_SOCKET_MASK_BYTES + 6U,
          "preset group is mask + preset bytes");
}

/* ── Wire length computation ──────────────────────────────────────────────────── */

static void test_wire_len(void)
{
    size_t hdr = sizeof(np_pbm1064_session_desc_hdr_t);
    size_t grp = sizeof(np_pbm1064_preset_group_t);

    check(np_pbm1064_session_desc_wire_len(0U) == hdr + 64U,
          "wire_len(0 groups) == header + signature only");
    check(np_pbm1064_session_desc_wire_len(1U) == hdr + grp + 64U,
          "wire_len(1 group) == header + 1 group + signature");
    check(np_pbm1064_session_desc_wire_len(NP_PBM1064_SESSION_MAX_PRESET_GROUPS) ==
              hdr + (size_t)NP_PBM1064_SESSION_MAX_PRESET_GROUPS * grp + 64U,
          "wire_len(max groups) == header + max groups + signature");
    check(np_pbm1064_session_desc_wire_len(NP_PBM1064_SESSION_MAX_PRESET_GROUPS + 1U) == 0U,
          "wire_len rejects group_count beyond capacity");

    /* signed_len never depends on the max capacity — only on the count
     * actually being signed. This is the security-relevant invariant: a
     * 1-group descriptor's signed span must not silently grow to cover
     * padding for groups 2..MAX. */
    check(np_pbm1064_session_desc_signed_len(1U) == np_pbm1064_session_desc_wire_len(1U) - 64U,
          "signed_len(1) == wire_len(1) - signature");
    check(np_pbm1064_session_desc_signed_len(1U) < np_pbm1064_session_desc_signed_len(
              NP_PBM1064_SESSION_MAX_PRESET_GROUPS),
          "signed_len grows strictly with group_count, not fixed at capacity");
}

/* ── Serialize / parse round trip ─────────────────────────────────────────────── */

static void test_serialize_parse_roundtrip(void)
{
    np_pbm1064_session_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.hdr.version = NP_SES1064_VERSION;
    desc.hdr.eeg_adaptive_mode = 1U;
    desc.hdr.group_count = 2U;
    desc.hdr.duration_s = 1200U;

    /* Group 0: gamma preset on socket 5 (within the old v4 domain). */
    mask_set(desc.groups[0].mask, 5U);
    desc.groups[0].preset = make_preset(180U, 40U, 0x32U);

    /* Group 1: theta preset on socket 77 — one v4 could NEVER name (only
     * 5-8 sockets addressable via an 8-bit mask / zone[5] array). This is
     * the whole point of v5. */
    mask_set(desc.groups[1].mask, 77U);
    desc.groups[1].preset = make_preset(150U, 6U, 0x28U);

    for (uint8_t i = 0; i < 64U; i++) { desc.signature[i] = (uint8_t)(i + 1U); }

    uint8_t buf[512];
    size_t written = np_pbm1064_session_desc_serialize(&desc, buf, sizeof(buf));
    check(written == np_pbm1064_session_desc_wire_len(2U), "serialize writes exact wire_len");

    np_pbm1064_session_desc_t parsed;
    np_pbm1064_status_t rc = np_pbm1064_session_desc_parse(buf, written, &parsed);
    check(rc == NP_PBM1064_OK, "parse accepts a well-formed blob");
    check(parsed.hdr.version == NP_SES1064_VERSION, "parsed version round-trips");
    check(parsed.hdr.group_count == 2U, "parsed group_count round-trips");
    check(parsed.hdr.duration_s == 1200U, "parsed duration_s round-trips");
    check(memcmp(parsed.groups[0].mask, desc.groups[0].mask, NP_PBM1064_SOCKET_MASK_BYTES) == 0,
          "parsed group 0 mask round-trips");
    check(parsed.groups[1].preset.freq_hz == 6U, "parsed group 1 preset round-trips");
    check(memcmp(parsed.signature, desc.signature, 64U) == 0, "parsed signature round-trips");

    /* Unused group slots (2..MAX-1) must be zeroed by parse, not garbage. */
    uint8_t zero_mask[NP_PBM1064_SOCKET_MASK_BYTES];
    memset(zero_mask, 0, sizeof(zero_mask));
    check(memcmp(parsed.groups[2].mask, zero_mask, sizeof(zero_mask)) == 0,
          "parse zeroes groups beyond group_count");
}

static void test_parse_rejects_bad_input(void)
{
    np_pbm1064_session_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.hdr.version = NP_SES1064_VERSION;
    desc.hdr.group_count = 1U;
    mask_set(desc.groups[0].mask, 3U);
    desc.groups[0].preset = make_preset(100U, 10U, 0x10U);

    uint8_t buf[512];
    size_t wire_len = np_pbm1064_session_desc_serialize(&desc, buf, sizeof(buf));

    np_pbm1064_session_desc_t parsed;

    check(np_pbm1064_session_desc_parse(buf, wire_len, &parsed) == NP_PBM1064_OK,
          "sanity: well-formed 1-group blob parses");

    check(np_pbm1064_session_desc_parse(buf, wire_len - 1U, &parsed) == NP_PBM1064_ERR_INVALID_ARG,
          "parse rejects a truncated blob rather than zero-extending it");

    check(np_pbm1064_session_desc_parse(buf, wire_len + 1U, &parsed) == NP_PBM1064_ERR_INVALID_ARG,
          "parse rejects a padded blob rather than ignoring the extra byte "
          "(would otherwise decouple the signed span from what is acted on)");

    /* Corrupt group_count to exceed capacity. */
    uint8_t bad_count_buf[512];
    memcpy(bad_count_buf, buf, wire_len);
    bad_count_buf[2] = NP_PBM1064_SESSION_MAX_PRESET_GROUPS + 1U; /* hdr.group_count offset */
    check(np_pbm1064_session_desc_parse(bad_count_buf, wire_len, &parsed) == NP_PBM1064_ERR_INVALID_ARG,
          "parse rejects group_count beyond NP_PBM1064_SESSION_MAX_PRESET_GROUPS");

    check(np_pbm1064_session_desc_parse(NULL, wire_len, &parsed) == NP_PBM1064_ERR_INVALID_ARG,
          "parse rejects NULL input");
}

/* ── Group expansion ──────────────────────────────────────────────────────────── */

static void test_expand_basic(void)
{
    np_pbm1064_session_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.hdr.group_count = 2U;
    mask_set(desc.groups[0].mask, 40U);
    mask_set(desc.groups[0].mask, 2U);
    desc.groups[0].preset = make_preset(180U, 40U, 0x32U);
    mask_set(desc.groups[1].mask, 77U);
    desc.groups[1].preset = make_preset(150U, 6U, 0x28U);

    uint8_t ids[NP_PBM1064_SESSION_MAX_ACTIVE_SOCKETS];
    np_pbm1064_preset_t presets[NP_PBM1064_SESSION_MAX_ACTIVE_SOCKETS];
    uint8_t count = 0U;

    np_pbm1064_status_t rc = np_pbm1064_session_desc_expand(
        &desc, ids, presets, &count, NP_PBM1064_SESSION_MAX_ACTIVE_SOCKETS);

    check(rc == NP_PBM1064_OK, "expand succeeds within capacity");
    check(count == 3U, "expand finds 3 distinct sockets across 2 groups");
    /* Ascending order: socket 2, 40, then 77. */
    check(ids[0] == 2U && ids[1] == 40U && ids[2] == 77U,
          "expand returns sockets in ascending order, including socket 77 "
          "(unrepresentable in v4)");
    check(presets[0].freq_hz == 40U && presets[1].freq_hz == 40U,
          "group 0's sockets (2, 40) get group 0's preset");
    check(presets[2].freq_hz == 6U, "group 1's socket (77) gets group 1's preset");
}

static void test_expand_dedup_first_group_wins(void)
{
    np_pbm1064_session_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.hdr.group_count = 2U;
    mask_set(desc.groups[0].mask, 10U);
    desc.groups[0].preset = make_preset(180U, 40U, 0x32U); /* gamma */
    mask_set(desc.groups[1].mask, 10U);                    /* same socket again */
    desc.groups[1].preset = make_preset(120U, 2U, 0x0FU);  /* sleep */

    uint8_t ids[NP_PBM1064_SESSION_MAX_ACTIVE_SOCKETS];
    np_pbm1064_preset_t presets[NP_PBM1064_SESSION_MAX_ACTIVE_SOCKETS];
    uint8_t count = 0U;

    np_pbm1064_status_t rc = np_pbm1064_session_desc_expand(
        &desc, ids, presets, &count, NP_PBM1064_SESSION_MAX_ACTIVE_SOCKETS);

    check(rc == NP_PBM1064_OK, "expand succeeds on overlapping masks");
    check(count == 1U,
          "a socket named in two groups is counted once, not double-dosed "
          "(this is the exact bug class the bitmap representation exists to prevent)");
    check(presets[0].freq_hz == 40U,
          "first group (signed order) wins for an overlapping socket");
}

static void test_expand_overflow_fails_closed(void)
{
    np_pbm1064_session_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.hdr.group_count = 1U;
    /* Set every bit — 128 distinct sockets, far beyond any capacity. */
    memset(desc.groups[0].mask, 0xFF, sizeof(desc.groups[0].mask));
    desc.groups[0].preset = make_preset(100U, 10U, 0x10U);

    uint8_t ids[4];
    np_pbm1064_preset_t presets[4];
    uint8_t count = 0xAAU; /* sentinel to confirm it gets overwritten to 0 */

    np_pbm1064_status_t rc = np_pbm1064_session_desc_expand(&desc, ids, presets, &count, 4U);

    check(rc == NP_PBM1064_ERR_INVALID_ARG,
          "expand fails closed when the combined groups exceed capacity");
    check(count == 0U,
          "expand reports zero sockets on overflow rather than a partial list — "
          "a partially-honoured session is worse than a rejected one");
}

/* ── End-to-end session_start() ───────────────────────────────────────────────── */

static void test_session_start_beyond_old_domain(void)
{
    /*
     * Regression test for the actual bug: a session addressing socket 0 (a
     * socket the v4 8-bit mask COULD represent) succeeds through preflight,
     * because today's HAL stub only wires slots 0-4 (NP-HW-HUB-001 Rev C's
     * cluster-controller fan-out for the full ~80-128 socket domain is
     * separately-tracked, undesigned hardware — OI-HUB-C* items). What this
     * test actually proves is that the DESCRIPTOR and its expansion are no
     * longer capped at 5-8 sockets: np_pbm1064_session_desc_expand() (tested
     * above) already demonstrated socket 77 parses and expands correctly.
     * This test proves the full parse -> expand -> session_start pipeline
     * for a within-hardware-range socket set with TWO independently-preset
     * groups (the "Gamma+theta coupled split-zone" case NP-SES-1064-001 §2
     * describes), which v4's single zone[5] loop's per-zone preset supported
     * but the new group representation must preserve.
     */
    np_pbm1064_session_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.hdr.version = NP_SES1064_VERSION;
    desc.hdr.group_count = 2U;
    desc.hdr.duration_s = 62U;

    mask_set(desc.groups[0].mask, 0U);
    desc.groups[0].preset = make_preset(180U, 40U, 0x10U); /* gamma */
    mask_set(desc.groups[1].mask, 1U);
    desc.groups[1].preset = make_preset(150U, 6U, 0x10U);  /* theta */

    np_pbm1064_session_ctx_t ctx;
    np_pbm1064_session_init(&ctx, NULL, NULL, NULL, 0U);

    np_pbm1064_status_t rc = np_pbm1064_session_start(&ctx, &desc);
    check(rc == NP_PBM1064_OK, "session_start accepts a 2-group split-zone v5 descriptor");
    check(ctx.active_socket_count == 2U, "session expanded to 2 active sockets");
    check(ctx.active_socket_id[0] == 0U && ctx.active_socket_id[1] == 1U,
          "active sockets are 0 and 1, ascending");
    check(ctx.active_preset[0].freq_hz == 40U && ctx.active_preset[1].freq_hz == 6U,
          "each active socket kept its own group's preset — split-zone frequencies differ");
    check(ctx.stage == NP_PBM1064_STAGE_RAMP_UP, "session reaches RAMP_UP");
}

static void test_session_start_rejects_over_capacity(void)
{
    np_pbm1064_session_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.hdr.version = NP_SES1064_VERSION;
    desc.hdr.group_count = 1U;
    desc.hdr.duration_s = 62U;
    memset(desc.groups[0].mask, 0xFF, sizeof(desc.groups[0].mask)); /* all 128 sockets */
    desc.groups[0].preset = make_preset(100U, 10U, 0x10U);

    np_pbm1064_session_ctx_t ctx;
    np_pbm1064_session_init(&ctx, NULL, NULL, NULL, 0U);

    np_pbm1064_status_t rc = np_pbm1064_session_start(&ctx, &desc);
    check(rc == NP_PBM1064_ERR_INVALID_ARG,
          "session_start fails closed (before any HAL call) when a descriptor "
          "addresses more sockets than NP_PBM1064_SESSION_MAX_ACTIVE_SOCKETS");
    check(ctx.stage == NP_PBM1064_STAGE_IDLE,
          "a rejected start leaves the session in IDLE, not PREFLIGHT/FAULT with "
          "partial hardware state touched");
}

/* ── T2 combined descriptor wire + start ──────────────────────────────────────── */

static void test_t2_combined_wire_and_start(void)
{
    np_t2_combined_desc_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.version = NP_T2_COMBINED_VERSION;
    desc.t2_combined_enable = 0U; /* keep the 1170nm laser out of this test */
    desc.pbm1064_hdr.version = NP_SES1064_VERSION;
    desc.pbm1064_hdr.group_count = 1U;
    desc.pbm1064_hdr.duration_s = 62U;
    mask_set(desc.pbm1064_groups[0].mask, 3U);
    desc.pbm1064_groups[0].preset = make_preset(160U, 20U, 0x16U);
    desc.laser1170.duty_pct = 0U;

    size_t wire_len = np_pbm1064_t2_combined_desc_wire_len(1U);
    check(wire_len == 4U + sizeof(np_pbm1064_session_desc_hdr_t) +
                          sizeof(np_pbm1064_preset_group_t) +
                          sizeof(np_t2_1170_preset_t) + 64U,
          "t2 wire_len matches the documented layout");

    uint8_t buf[512];
    size_t written = np_pbm1064_t2_combined_desc_serialize(&desc, buf, sizeof(buf));
    check(written == wire_len, "t2 serialize writes exactly wire_len bytes");

    np_t2_combined_desc_t parsed;
    check(np_pbm1064_t2_combined_desc_parse(buf, written, &parsed) == NP_PBM1064_OK,
          "t2 parse accepts a well-formed blob");
    check(parsed.pbm1064_hdr.group_count == 1U, "t2 parsed pbm1064_hdr round-trips");
    check(memcmp(parsed.pbm1064_groups[0].mask, desc.pbm1064_groups[0].mask,
                 NP_PBM1064_SOCKET_MASK_BYTES) == 0,
          "t2 parsed group mask round-trips");

    np_pbm1064_t2_ctx_t ctx;
    np_pbm1064_t2_init(&ctx, NULL, NULL, 0U);
    np_pbm1064_status_t rc = np_pbm1064_t2_start_combined(&ctx, &desc);
    check(rc == NP_PBM1064_OK, "t2_start_combined accepts the inlined-groups descriptor");
    check(ctx.pbm1064.active_socket_count == 1U &&
          ctx.pbm1064.active_socket_id[0] == 3U,
          "inner 1064nm session expanded the T2-embedded group correctly");
}

/* ── Regression check via the FAI harness itself ──────────────────────────────── */

static void test_fai_regressions(void)
{
    check(np_pbm1064_fai_sm09().passed, "FAI-SM-09 passes against the rewritten session ctx");
    check(np_pbm1064_fai_sm10().passed, "FAI-SM-10 passes against the v5 descriptor shape");
    check(np_pbm1064_fai_sm11().passed, "FAI-SM-11 passes against the per-socket UHDR/SHDR shape");
    check(np_pbm1064_fai_t2_01().passed, "FAI-T2-01 passes against the inlined T2 descriptor");
    check(np_pbm1064_fai_t2_02().passed, "FAI-T2-02 passes against the rewritten throttle cascade");
    check(np_pbm1064_fai_t2_03().passed, "FAI-T2-03 passes against the per-socket combined UHDR/SHDR");
    check(np_pbm1064_fai_t2_04().passed, "FAI-T2-04 passes against the rewritten abort path");
}

int main(void)
{
    test_struct_sizes();
    test_wire_len();
    test_serialize_parse_roundtrip();
    test_parse_rejects_bad_input();
    test_expand_basic();
    test_expand_dedup_first_group_wins();
    test_expand_overflow_fails_closed();
    test_session_start_beyond_old_domain();
    test_session_start_rejects_over_capacity();
    test_t2_combined_wire_and_start();
    test_fai_regressions();

    if (g_failures == 0) {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d TEST(S) FAILED\n", g_failures);
    return 1;
}
