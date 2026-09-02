/*
 * NeurOne Hub Control — Protocol Parser Host Tests (NP Hub Protocol v2)
 *
 * Exercises the v2 command target block: the socket bitmap that replaced the
 * five-bit zone-module slot mask (NP-HEX-ZM-001 §4; np_module_map.h), its
 * validation, and the accessors that hand a parsed target to np_module_map.
 *
 * Ed25519 verification and the Config-partition HALs are stubbed — this suite
 * is about the command-body grammar, not about crypto.
 *
 * No FreeRTOS, no hardware. IEC 62304 Class B — SW-02 hub control.
 * Build with -DNP_BUILD_TESTS=ON (see firmware/hub_control/CMakeLists.txt).
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../include/np_protocol.h"

/* The doubles below define symbols declared in np_sw02_platform_hal.h.
 * Including it is what makes drift from the production contract a compile
 * error on the DEFINITION side as well as the caller side (OI-SWCI-40,
 * following OI-SWCI-18 on the SW-01 side). */
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

/* ── Stubbed dependencies ─────────────────────────────────────────────────────
 * The serial HAL reports NOT_PRESENT so the replay guard is skipped (the parser
 * only compares when the stored serial reads back). The verifier always accepts:
 * these tests are about the command body, and a real signature would have to be
 * recomputed for every mutated fixture, which would test the fixture builder
 * rather than the parser. */

np_hub_status_t np_proto_hal_get_device_serial(uint8_t *buf, size_t len)
{
    (void)buf; (void)len;
    return NP_HUB_ERR_NOT_PRESENT;
}

np_hub_status_t np_proto_hal_get_proto_pubkey(uint8_t *pub_key_out)
{
    memset(pub_key_out, 0, 32);
    return NP_HUB_OK;
}

int np_ed25519_verify(const uint8_t *msg, size_t msg_len,
                      const uint8_t *sig, const uint8_t *pub_key)
{
    (void)msg; (void)msg_len; (void)sig; (void)pub_key;
    return 0;   /* accept */
}

/* ── Blob builder ─────────────────────────────────────────────────────────────
 * Writes a v2 blob into a caller-owned buffer, one command at a time. Fields are
 * written through memcpy at explicit byte offsets rather than through the packed
 * structs, so a silent change to the struct layout is caught by the tests
 * instead of being mirrored into them. */

#define BLOB_CAP 4096

typedef struct {
    uint8_t buf[BLOB_CAP];
    size_t  len;        /* bytes written so far, excluding the signature */
    uint8_t cmd_count;
} blob_t;

static void blob_begin(blob_t *b, uint16_t version)
{
    memset(b, 0, sizeof *b);
    const uint32_t magic = NP_HUB_PROTO_MAGIC;
    memcpy(b->buf + 0, &magic, 4);
    memcpy(b->buf + 4, &version, 2);
    b->buf[6] = 0;                      /* flags */
    b->buf[7] = 0;                      /* cmd_count — patched by blob_finish */
    const uint32_t compiled_at = 1700000000UL;
    memcpy(b->buf + 24, &compiled_at, 4);
    const uint32_t duration = 60000UL;
    memcpy(b->buf + 60, &duration, 4);
    b->len = 64U;
}

static void blob_add_cmd(blob_t *b,
                         uint8_t mod_type, uint8_t slot_id,
                         uint32_t start_ms, uint32_t duration_ms,
                         uint8_t target_kind,
                         const uint8_t *target, uint8_t target_len,
                         const uint8_t *params, uint16_t params_len)
{
    uint8_t *p = b->buf + b->len;
    p[0] = mod_type;
    p[1] = slot_id;
    memcpy(p + 2, &start_ms, 4);
    memcpy(p + 6, &duration_ms, 4);
    memcpy(p + 10, &params_len, 2);
    p[12] = target_kind;
    p[13] = target_len;
    b->len += 14U;

    if (target_len > 0U) {
        memcpy(b->buf + b->len, target, target_len);
        b->len += target_len;
    }
    if (params_len > 0U) {
        memcpy(b->buf + b->len, params, params_len);
        b->len += params_len;
    }
    b->cmd_count++;
}

/* Patches cmd_count and appends the (ignored) signature. Returns total length. */
static size_t blob_finish(blob_t *b)
{
    b->buf[7] = b->cmd_count;
    memset(b->buf + b->len, 0, NP_HUB_PROTO_SIG_LEN);
    b->len += NP_HUB_PROTO_SIG_LEN;
    return b->len;
}

/* ── Socket-mask helper ───────────────────────────────────────────────────────
 * Sets bit `socket` (0-based firmware id) in a 16-byte bitmap. Deliberately
 * idempotent — see test_bitmap_makes_double_drive_unrepresentable. */
static void mask_set(uint8_t mask[NP_HUB_SOCKET_MASK_BYTES], unsigned socket)
{
    mask[socket / 8U] |= (uint8_t)(1U << (socket % 8U));
}

/* ── Layout ───────────────────────────────────────────────────────────────────── */

static void test_wire_layout(void)
{
    check(sizeof(np_proto_header_t) == 64U, "layout: header is 64 bytes");
    check(sizeof(np_proto_cmd_hdr_t) == 14U,
          "layout: v2 command header is 14 bytes (12 + target_kind + target_len)");
    check(NP_HUB_SOCKET_MASK_BYTES * 8U == 128U,
          "layout: socket bitmap covers the full 7-bit socket domain");
    check(NP_HUB_PROTO_VERSION == 0x0002U, "layout: protocol version is v2");
}

/* ── Slot-addressed commands still work ───────────────────────────────────────── */

static void test_slot_target_roundtrip(void)
{
    const uint8_t params[5] = { 0xFF, 6, 2, 0, 3 };
    blob_t b;
    blob_begin(&b, NP_HUB_PROTO_VERSION);
    blob_add_cmd(&b, NP_MOD_EEG, NP_HUB_SLOT_EEG,
                 0U, 0U, NP_PROTO_TARGET_SLOT, NULL, 0U, params, sizeof params);
    size_t len = blob_finish(&b);

    np_session_desc_t desc;
    np_hub_status_t rc = np_protocol_verify_and_parse(b.buf, len, &desc);

    check(rc == NP_HUB_OK, "slot: parses");
    check(desc.cmd_count == 1U, "slot: one command");
    check(desc.cmds[0].mod_type == NP_MOD_EEG, "slot: mod_type preserved");
    check(desc.cmds[0].slot_id == NP_HUB_SLOT_EEG,
          "slot: EEG addressed to slot 5 (the ADS1299), not slot 0 (the PBM driver)");
    check(desc.cmds[0].target_kind == NP_PROTO_TARGET_SLOT, "slot: target kind SLOT");
    check(desc.cmds[0].params_len == sizeof params, "slot: params_len preserved");
    check(memcmp(desc.cmds[0].params, params, sizeof params) == 0,
          "slot: params bytes preserved");
}

static void test_slot_command_has_no_sockets(void)
{
    blob_t b;
    blob_begin(&b, NP_HUB_PROTO_VERSION);
    blob_add_cmd(&b, NP_MOD_AUDIO, NP_HUB_SLOT_AUDIO,
                 0U, 0U, NP_PROTO_TARGET_SLOT, NULL, 0U, NULL, 0U);
    size_t len = blob_finish(&b);

    np_session_desc_t desc;
    check(np_protocol_verify_and_parse(b.buf, len, &desc) == NP_HUB_OK,
          "slot-accessors: parses");

    uint16_t out[8];
    uint16_t n = 0xFFFFU;
    check(np_protocol_socket_count(&desc.cmds[0]) == 0U,
          "slot-accessors: socket count is 0 on a slot command");
    check(!np_protocol_socket_is_set(&desc.cmds[0], 0U),
          "slot-accessors: socket 0 not set on a slot command");
    check(np_protocol_socket_expand(&desc.cmds[0], out, 8U, &n) == NP_HUB_OK && n == 0U,
          "slot-accessors: expand yields an empty list on a slot command");
}

/* ── Socket-addressed commands ────────────────────────────────────────────────── */

/* A representative frontal-left socket set: 11 firmware 0-based ids scattered
 * across the low sockets. These are FIXTURE values for exercising the bitmap
 * wire mechanics, deliberately NOT read from protocols/predefined/00-zones.npps
 * — the firmware has no access to that file, and coupling to it would break this
 * host test on every lattice re-cut. The app-side hubCompiler.test.ts validates
 * the actual zone lists against the real file. */
static const unsigned FRONTAL_LEFT_FW[] = { 0, 1, 3, 4, 6, 7, 10, 11, 12, 15, 16 };
#define FRONTAL_LEFT_N (sizeof FRONTAL_LEFT_FW / sizeof FRONTAL_LEFT_FW[0])

static void build_frontal_left(blob_t *b, uint8_t mask[NP_HUB_SOCKET_MASK_BYTES])
{
    memset(mask, 0, NP_HUB_SOCKET_MASK_BYTES);
    for (unsigned i = 0U; i < FRONTAL_LEFT_N; i++) {
        mask_set(mask, FRONTAL_LEFT_FW[i]);
    }
    const uint8_t params[4] = { 40, 0x32, 200, 200 };
    blob_begin(b, NP_HUB_PROTO_VERSION);
    blob_add_cmd(b, NP_MOD_PBM_BASE, NP_HUB_SLOT_NONE, 0U, 0U,
                 NP_PROTO_TARGET_SOCKET_MASK, mask, NP_HUB_SOCKET_MASK_BYTES,
                 params, sizeof params);
}

static void test_socket_target_roundtrip(void)
{
    blob_t  b;
    uint8_t mask[NP_HUB_SOCKET_MASK_BYTES];
    build_frontal_left(&b, mask);
    size_t len = blob_finish(&b);

    np_session_desc_t desc;
    check(np_protocol_verify_and_parse(b.buf, len, &desc) == NP_HUB_OK,
          "socket: parses");
    check(desc.cmds[0].target_kind == NP_PROTO_TARGET_SOCKET_MASK,
          "socket: target kind SOCKET_MASK");
    check(memcmp(desc.cmds[0].socket_mask, mask, NP_HUB_SOCKET_MASK_BYTES) == 0,
          "socket: bitmap preserved byte for byte");
    check(desc.cmds[0].params_len == 4U,
          "socket: params_len survives the target block");
    check(desc.cmds[0].params[0] == 40U,
          "socket: params read from AFTER the target block, not from inside it");
}

static void test_socket_membership(void)
{
    blob_t  b;
    uint8_t mask[NP_HUB_SOCKET_MASK_BYTES];
    build_frontal_left(&b, mask);
    size_t len = blob_finish(&b);

    np_session_desc_t desc;
    (void)np_protocol_verify_and_parse(b.buf, len, &desc);
    const np_session_cmd_t *c = &desc.cmds[0];

    check(np_protocol_socket_count(c) == FRONTAL_LEFT_N,
          "membership: count matches the zone's socket list length");
    check(np_protocol_socket_is_set(c, 0U),
          "membership: socket 1 (NPPS) = 0 (fw) is in Frontal Left");
    check(np_protocol_socket_is_set(c, 16U),
          "membership: socket 17 (NPPS) = 16 (fw) is in Frontal Left");
    check(!np_protocol_socket_is_set(c, 2U),
          "membership: socket 3 (NPPS) = 2 (fw) is Frontal RIGHT, not selected");
    check(!np_protocol_socket_is_set(c, 77U),
          "membership: an occipital socket is not selected by a frontal zone");
}

static void test_socket_expand_matches_module_map_input(void)
{
    blob_t  b;
    uint8_t mask[NP_HUB_SOCKET_MASK_BYTES];
    build_frontal_left(&b, mask);
    size_t len = blob_finish(&b);

    np_session_desc_t desc;
    (void)np_protocol_verify_and_parse(b.buf, len, &desc);

    /* This is the array np_group_query_t.sockets points at. */
    uint16_t sockets[NP_HUB_SOCKET_MASK_BYTES * 8U];
    uint16_t n  = 0U;
    np_hub_status_t rc =
        np_protocol_socket_expand(&desc.cmds[0], sockets,
                                  (uint16_t)(sizeof sockets / sizeof *sockets), &n);

    check(rc == NP_HUB_OK, "expand: succeeds");
    check(n == FRONTAL_LEFT_N, "expand: yields one entry per selected socket");

    bool exact = (n == FRONTAL_LEFT_N);
    for (uint16_t i = 0U; exact && i < n; i++) {
        exact = (sockets[i] == (uint16_t)FRONTAL_LEFT_FW[i]);
    }
    check(exact, "expand: ascending 0-based ids match the zone's socket list");

    bool ascending = true;
    for (uint16_t i = 1U; i < n; i++) {
        if (sockets[i] <= sockets[i - 1U]) ascending = false;
    }
    check(ascending, "expand: strictly ascending (so duplicate-free)");
}

/*
 * The socket-number <-> bit-position conversion contract (NUMBER-1).
 *
 * A socket NUMBER is 1-based project-wide (docs/np_hex_zm_001.md §3.3). The bit
 * position in this bitmap is NOT a socket number — it is index space, and bit 0
 * means socket 1. Every producer converts once on the way in and every consumer
 * adds the base back before quoting a socket number. The contract is what the
 * other two tests above assume; this one states it.
 *
 * The bytes below are a FROZEN FIXTURE, captured from the app-side producer
 * (app/web/src/lib/hubCompiler.ts socketBitmap(), via compileProtocol, reading
 * the 16-byte target block at offset 64+14) for the zone "Occipital Left" =
 * NPPS sockets 72,73,74,77,78. The identical hex is asserted in the iOS suite
 * (NPSocketMaskTests.testMaskBytesMatchTheWebEncoder), so all three producers
 * are pinned to one byte pattern. It is a fixture and not a read of
 * 00-zones.npps deliberately, for the reason given on FRONTAL_LEFT_FW above.
 */
#define NP_SOCKET_NUMBERING_BASE  1U   /* NUMBER-1; app-side NP_SOCKET_NUMBERING_BASE */

static const uint8_t OCCIPITAL_LEFT_MASK[NP_HUB_SOCKET_MASK_BYTES] = {
    /* bytes 0-7 clear; byte 8 = bit 71 (socket 72); byte 9 = bits 72,73,76,77 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint16_t OCCIPITAL_LEFT_NPPS[] = { 72U, 73U, 74U, 77U, 78U };
#define OCCIPITAL_LEFT_N (sizeof OCCIPITAL_LEFT_NPPS / sizeof OCCIPITAL_LEFT_NPPS[0])

static void test_numbering_base_conversion_contract(void)
{
    blob_t b;
    const uint8_t params[4] = { 40, 0x32, 200, 200 };
    blob_begin(&b, NP_HUB_PROTO_VERSION);
    blob_add_cmd(&b, NP_MOD_PBM_BASE, NP_HUB_SLOT_NONE, 0U, 0U,
                 NP_PROTO_TARGET_SOCKET_MASK, OCCIPITAL_LEFT_MASK,
                 NP_HUB_SOCKET_MASK_BYTES, params, sizeof params);
    size_t len = blob_finish(&b);

    np_session_desc_t desc;
    check(np_protocol_verify_and_parse(b.buf, len, &desc) == NP_HUB_OK,
          "numbering: an app-produced mask parses");

    const np_session_cmd_t *c = &desc.cmds[0];
    check(np_protocol_socket_count(c) == OCCIPITAL_LEFT_N,
          "numbering: the app-produced mask selects 5 sockets");

    uint16_t sockets[NP_HUB_SOCKET_MASK_BYTES * 8U];
    uint16_t n = 0U;
    (void)np_protocol_socket_expand(c, sockets,
                                    (uint16_t)(sizeof sockets / sizeof *sockets), &n);

    /* The contract: index + base == the socket number a human authored. */
    bool exact = (n == OCCIPITAL_LEFT_N);
    for (uint16_t i = 0U; exact && i < n; i++) {
        exact = ((uint16_t)(sockets[i] + NP_SOCKET_NUMBERING_BASE)
                 == OCCIPITAL_LEFT_NPPS[i]);
    }
    check(exact,
          "numbering: expanded index + NP_SOCKET_NUMBERING_BASE == the authored "
          "NPPS socket numbers 72,73,74,77,78");

    /* Stated the other way round, at the top edge of the zone — the one place a
     * base error is visible as a MISS rather than as a neighbouring hit. Socket
     * 78 is the highest in Occipital Left, so bit 77 is set and bit 78 is not.
     * A consumer that read this mask 1-based would look at bit 78, find nothing,
     * and quietly drop the socket; inside the zone the same error merely shifts
     * onto the adjacent tile, which is why the edge is the witness. */
    check(np_protocol_socket_is_set(c, 78U - NP_SOCKET_NUMBERING_BASE),
          "numbering: NPPS socket 78 is bit 77");
    check(!np_protocol_socket_is_set(c, 78U),
          "numbering: bit 78 is NOT set — reading this mask 1-based would lose the "
          "zone's highest socket entirely");
    check(!np_protocol_socket_is_set(c, 71U - NP_SOCKET_NUMBERING_BASE),
          "numbering: NPPS socket 71 is not in Occipital Left");
}

static void test_socket_expand_overflow(void)
{
    blob_t  b;
    uint8_t mask[NP_HUB_SOCKET_MASK_BYTES];
    build_frontal_left(&b, mask);
    size_t len = blob_finish(&b);

    np_session_desc_t desc;
    (void)np_protocol_verify_and_parse(b.buf, len, &desc);

    uint16_t small[3];
    uint16_t n = 0U;
    np_hub_status_t rc = np_protocol_socket_expand(&desc.cmds[0], small, 3U, &n);

    check(rc == NP_HUB_ERR_CMD_TOO_MANY, "expand: overflow reports CMD_TOO_MANY");
    check(n == 3U, "expand: overflow reports the truncated count");
    check(small[0] == 0U && small[1] == 1U && small[2] == 3U,
          "expand: overflow still fills what fits, in order");
}

/*
 * The reason the target is a bitmap and not an address list. Under inclusive
 * zone membership a midline socket belongs to BOTH hemisphere zones of its lobe,
 * so a protocol naming a left AND a right zone names the shared midline sockets
 * twice. A list would carry the duplicate through to the driver — double J/cm²
 * on the same module. In a bitmap the duplicate cannot be expressed.
 */
static void test_bitmap_makes_double_drive_unrepresentable(void)
{
    /* Two overlapping FIXTURE sets (not read from 00-zones.npps): 11 sockets
     * each, sharing three (1, 5, 13). The union has 19 distinct sockets. This
     * exercises the bitmap-dedup mechanic; the app test checks the real zones. */
    static const unsigned FL[] = { 1, 2, 4, 5, 7, 8, 11, 12, 13, 16, 17 };
    static const unsigned FR[] = { 1, 3, 5, 6, 9, 10, 13, 14, 15, 18, 19 };

    uint8_t mask[NP_HUB_SOCKET_MASK_BYTES];
    memset(mask, 0, sizeof mask);
    for (unsigned i = 0U; i < sizeof FL / sizeof FL[0]; i++) mask_set(mask, FL[i] - 1U);
    for (unsigned i = 0U; i < sizeof FR / sizeof FR[0]; i++) mask_set(mask, FR[i] - 1U);

    blob_t b;
    blob_begin(&b, NP_HUB_PROTO_VERSION);
    blob_add_cmd(&b, NP_MOD_PBM_BASE, NP_HUB_SLOT_NONE, 0U, 0U,
                 NP_PROTO_TARGET_SOCKET_MASK, mask, NP_HUB_SOCKET_MASK_BYTES, NULL, 0U);
    size_t len = blob_finish(&b);

    np_session_desc_t desc;
    (void)np_protocol_verify_and_parse(b.buf, len, &desc);

    /* 11 + 11 entries in the two lists, but only 19 distinct sockets. */
    check(np_protocol_socket_count(&desc.cmds[0]) == 19U,
          "dedup: union of both frontal zones selects 19 sockets, not 22");
    check(np_protocol_socket_is_set(&desc.cmds[0], 0U) &&
          np_protocol_socket_is_set(&desc.cmds[0], 4U) &&
          np_protocol_socket_is_set(&desc.cmds[0], 12U),
          "dedup: the three midline sockets are present exactly once each");
}

/* ── Mixed blob: the offset arithmetic that the target block changes ──────────── */

static void test_mixed_targets_offsets(void)
{
    uint8_t mask[NP_HUB_SOCKET_MASK_BYTES];
    memset(mask, 0, sizeof mask);
    mask_set(mask, 79U);                    /* NPPS socket 80, the highest wired */

    const uint8_t pbm_params[4]  = { 10, 0x32, 128, 128 };
    const uint8_t vis_params[3]  = { 1, 2, 3 };

    blob_t b;
    blob_begin(&b, NP_HUB_PROTO_VERSION);
    /* socket-addressed, with a target block */
    blob_add_cmd(&b, NP_MOD_PBM_BASE, NP_HUB_SLOT_NONE, 5000U, 1000U,
                 NP_PROTO_TARGET_SOCKET_MASK, mask, NP_HUB_SOCKET_MASK_BYTES,
                 pbm_params, sizeof pbm_params);
    /* slot-addressed, with none — the parser must not skip 16 phantom bytes */
    blob_add_cmd(&b, NP_MOD_VISUAL, NP_HUB_SLOT_VISUAL,
                 1000U, 0U, NP_PROTO_TARGET_SLOT, NULL, 0U,
                 vis_params, sizeof vis_params);
    size_t len = blob_finish(&b);

    np_session_desc_t desc;
    check(np_protocol_verify_and_parse(b.buf, len, &desc) == NP_HUB_OK,
          "mixed: parses a blob with both target kinds");
    check(desc.cmd_count == 2U, "mixed: two commands");

    /* Commands are sorted by start_ms, so the visual command (1000) comes first. */
    const np_session_cmd_t *vis = &desc.cmds[0];
    const np_session_cmd_t *pbm = &desc.cmds[1];

    check(vis->mod_type == NP_MOD_VISUAL && vis->start_ms == 1000U,
          "mixed: slot command sorted first and intact");
    check(vis->params_len == 3U && memcmp(vis->params, vis_params, 3) == 0,
          "mixed: slot command params intact after a preceding target block");
    check(pbm->mod_type == NP_MOD_PBM_BASE && pbm->start_ms == 5000U,
          "mixed: socket command intact");
    check(pbm->params_len == 4U && memcmp(pbm->params, pbm_params, 4) == 0,
          "mixed: socket command params intact");
    check(np_protocol_socket_is_set(pbm, 79U) &&
          np_protocol_socket_count(pbm) == 1U,
          "mixed: the highest wired socket (NPPS 80) round-trips");
    check(np_protocol_socket_count(vis) == 0U,
          "mixed: the slot command did not inherit the socket command's bitmap");
}

static void test_expected_len_accounts_for_target(void)
{
    blob_t  b;
    uint8_t mask[NP_HUB_SOCKET_MASK_BYTES];
    build_frontal_left(&b, mask);
    size_t len = blob_finish(&b);

    const np_proto_header_t *hdr = (const np_proto_header_t *)b.buf;
    size_t expected = np_protocol_compute_expected_len(
        hdr, b.buf + 64, len - 64U - NP_HUB_PROTO_SIG_LEN);

    check(expected == len,
          "expected-len: counts header + target + params + signature");
    check(expected == 64U + 14U + NP_HUB_SOCKET_MASK_BYTES + 4U + NP_HUB_PROTO_SIG_LEN,
          "expected-len: matches the hand-computed v2 layout");
}

/* ── Rejections ───────────────────────────────────────────────────────────────── */

static void test_rejects_v1_blob(void)
{
    blob_t b;
    blob_begin(&b, 0x0001U);
    blob_add_cmd(&b, NP_MOD_AUDIO, NP_HUB_SLOT_AUDIO,
                 0U, 0U, NP_PROTO_TARGET_SLOT, NULL, 0U, NULL, 0U);
    size_t len = blob_finish(&b);

    np_session_desc_t desc;
    check(np_protocol_verify_and_parse(b.buf, len, &desc) == NP_HUB_ERR_BAD_VERSION,
          "reject: a v1 blob is refused, not reinterpreted as v2");
}

static void test_rejects_retired_zone_slots(void)
{
    np_session_desc_t desc;

    /* Every retired zone slot, individually. Slot 0 is the one that matters
     * most: it is v1's 0x01 catch-all, and it is the PBM zone-0 driver. */
    for (uint8_t slot = 0U; slot < NP_HUB_ZONE_SLOT_COUNT; slot++) {
        blob_t b;
        blob_begin(&b, NP_HUB_PROTO_VERSION);
        blob_add_cmd(&b, NP_MOD_PBM_BASE, slot, 0U, 0U,
                     NP_PROTO_TARGET_SLOT, NULL, 0U, NULL, 0U);
        size_t len = blob_finish(&b);

        if (np_protocol_verify_and_parse(b.buf, len, &desc) != NP_HUB_ERR_INVALID_ARG) {
            check(0, "reject: a retired zone slot is refused");
            return;
        }
    }
    check(1, "reject: all five retired zone slots are refused");

    /* Above the slot domain entirely. */
    blob_t b2;
    blob_begin(&b2, NP_HUB_PROTO_VERSION);
    blob_add_cmd(&b2, NP_MOD_PBM_BASE, NP_HUB_SLOT_MAX, 0U, 0U,
                 NP_PROTO_TARGET_SLOT, NULL, 0U, NULL, 0U);
    size_t len2 = blob_finish(&b2);
    check(np_protocol_verify_and_parse(b2.buf, len2, &desc) == NP_HUB_ERR_INVALID_ARG,
          "reject: an out-of-domain slot id is refused");
}

/* The two stim slots that v1 had no way to name: slot_mask was a uint8_t and
 * dispatch shifted by the slot index, so nothing above slot 7 was reachable. */
static void test_high_slots_are_addressable(void)
{
    static const uint8_t HIGH[] = {
        NP_HUB_SLOT_VNS_HRV, NP_HUB_SLOT_INTRANASAL, NP_HUB_SLOT_CVNS,
        NP_HUB_SLOT_QEEG, NP_HUB_SLOT_TMS, NP_HUB_SLOT_PBM_1170NM,
        NP_HUB_SLOT_CLIN_TACS, NP_HUB_SLOT_HD_TDCS, NP_HUB_SLOT_VIBROTACTILE,
        NP_HUB_SLOT_BES_TACS, NP_HUB_SLOT_TDCS,
    };
    bool all_ok = true;
    for (unsigned i = 0U; i < sizeof HIGH / sizeof HIGH[0]; i++) {
        blob_t b;
        blob_begin(&b, NP_HUB_PROTO_VERSION);
        blob_add_cmd(&b, NP_MOD_TDCS, HIGH[i], 0U, 0U,
                     NP_PROTO_TARGET_SLOT, NULL, 0U, NULL, 0U);
        size_t len = blob_finish(&b);

        np_session_desc_t desc;
        if (np_protocol_verify_and_parse(b.buf, len, &desc) != NP_HUB_OK ||
            desc.cmds[0].slot_id != HIGH[i]) {
            all_ok = false;
        }
    }
    check(all_ok, "slots: every slot above 7 is addressable (v1 could not name them)");
}

static void test_rejects_slot_id_on_socket_target(void)
{
    uint8_t mask[NP_HUB_SOCKET_MASK_BYTES];
    memset(mask, 0, sizeof mask);
    mask_set(mask, 0U);

    blob_t b;
    blob_begin(&b, NP_HUB_PROTO_VERSION);
    blob_add_cmd(&b, NP_MOD_PBM_BASE, NP_HUB_SLOT_EEG, 0U, 0U,
                 NP_PROTO_TARGET_SOCKET_MASK, mask, NP_HUB_SOCKET_MASK_BYTES, NULL, 0U);
    size_t len = blob_finish(&b);

    np_session_desc_t desc;
    check(np_protocol_verify_and_parse(b.buf, len, &desc) == NP_HUB_ERR_INVALID_ARG,
          "reject: a socket-addressed command may not also name a slot");
}

static void test_rejects_bad_target_kind(void)
{
    blob_t b;
    blob_begin(&b, NP_HUB_PROTO_VERSION);
    blob_add_cmd(&b, NP_MOD_PBM_BASE, NP_HUB_SLOT_NONE, 0U, 0U,
                 0x7FU /* undefined kind */, NULL, 0U, NULL, 0U);
    size_t len = blob_finish(&b);

    np_session_desc_t desc;
    check(np_protocol_verify_and_parse(b.buf, len, &desc) == NP_HUB_ERR_INVALID_ARG,
          "reject: an unknown target kind is refused, not skipped");
}

static void test_rejects_target_len_mismatch(void)
{
    uint8_t short_mask[NP_HUB_SOCKET_MASK_BYTES];
    memset(short_mask, 0, sizeof short_mask);
    mask_set(short_mask, 3U);

    /* Socket kind with a 15-byte block: a producer that disagrees about the
     * socket domain. Zero-extending would silently drop sockets 120-127. */
    blob_t b;
    blob_begin(&b, NP_HUB_PROTO_VERSION);
    blob_add_cmd(&b, NP_MOD_PBM_BASE, NP_HUB_SLOT_NONE, 0U, 0U,
                 NP_PROTO_TARGET_SOCKET_MASK, short_mask,
                 NP_HUB_SOCKET_MASK_BYTES - 1U, NULL, 0U);
    size_t len = blob_finish(&b);

    np_session_desc_t desc;
    check(np_protocol_verify_and_parse(b.buf, len, &desc) == NP_HUB_ERR_INVALID_ARG,
          "reject: short socket mask is refused, not zero-extended");

    /* Slot kind carrying a target block it has no business carrying. */
    blob_t b2;
    blob_begin(&b2, NP_HUB_PROTO_VERSION);
    blob_add_cmd(&b2, NP_MOD_AUDIO, NP_HUB_SLOT_AUDIO, 0U, 0U,
                 NP_PROTO_TARGET_SLOT, short_mask, NP_HUB_SOCKET_MASK_BYTES, NULL, 0U);
    size_t len2 = blob_finish(&b2);

    check(np_protocol_verify_and_parse(b2.buf, len2, &desc) == NP_HUB_ERR_INVALID_ARG,
          "reject: slot kind with a non-empty target block is refused");
}

static void test_rejects_truncated_target(void)
{
    blob_t  b;
    uint8_t mask[NP_HUB_SOCKET_MASK_BYTES];
    build_frontal_left(&b, mask);
    size_t len = blob_finish(&b);

    /* Chop the last 8 bytes of the target block out of the body by shrinking the
     * blob; the header still claims a full command. */
    np_session_desc_t desc;
    check(np_protocol_verify_and_parse(b.buf, len - 8U, &desc) != NP_HUB_OK,
          "reject: a body too short for its declared target is refused");
}

static void test_rejects_trailing_body_bytes(void)
{
    /* cmd_count under-counts: two commands present, header says one. Because
     * commands sort by start_ms, the dropped tail of an interval protocol is
     * exactly its STOP commands — stimulation with no scheduled stop. */
    blob_t b;
    blob_begin(&b, NP_HUB_PROTO_VERSION);
    blob_add_cmd(&b, NP_MOD_AUDIO, NP_HUB_SLOT_AUDIO, 0U, 1000U,
                 NP_PROTO_TARGET_SLOT, NULL, 0U, (const uint8_t *)"\1\2\3", 3U);
    blob_add_cmd(&b, NP_MOD_AUDIO, NP_HUB_SLOT_AUDIO, 1000U, 0U,
                 NP_PROTO_TARGET_SLOT, NULL, 0U, NULL, 0U);
    b.cmd_count = 1U;                       /* lie about the count */
    size_t len = blob_finish(&b);

    np_session_desc_t desc;
    check(np_protocol_verify_and_parse(b.buf, len, &desc) != NP_HUB_OK,
          "reject: trailing body bytes (an under-counted cmd_count) are refused");
}

static void test_rejects_stop_deadline_overflow(void)
{
    /* start_ms + duration_ms wrapping to 0 would read as the runner's
     * "no stop pending" sentinel, discarding the duration entirely. */
    blob_t b;
    blob_begin(&b, NP_HUB_PROTO_VERSION);
    blob_add_cmd(&b, NP_MOD_AUDIO, NP_HUB_SLOT_AUDIO,
                 0xFFFFFF00UL, 0x00000100UL,
                 NP_PROTO_TARGET_SLOT, NULL, 0U, NULL, 0U);
    size_t len = blob_finish(&b);

    np_session_desc_t desc;
    check(np_protocol_verify_and_parse(b.buf, len, &desc) == NP_HUB_ERR_INVALID_ARG,
          "reject: a stop deadline that wraps uint32 is refused");

    /* The largest non-wrapping pair still parses. */
    blob_t b2;
    blob_begin(&b2, NP_HUB_PROTO_VERSION);
    blob_add_cmd(&b2, NP_MOD_AUDIO, NP_HUB_SLOT_AUDIO,
                 0xFFFFFF00UL, 0x000000FFUL,
                 NP_PROTO_TARGET_SLOT, NULL, 0U, NULL, 0U);
    size_t len2 = blob_finish(&b2);
    check(np_protocol_verify_and_parse(b2.buf, len2, &desc) == NP_HUB_OK,
          "accept: a stop deadline of exactly UINT32_MAX is fine");
}

/* ── Runner ───────────────────────────────────────────────────────────────────── */

int main(void)
{
    test_wire_layout();

    test_slot_target_roundtrip();
    test_slot_command_has_no_sockets();

    test_socket_target_roundtrip();
    test_socket_membership();
    test_socket_expand_matches_module_map_input();
    test_numbering_base_conversion_contract();
    test_socket_expand_overflow();
    test_bitmap_makes_double_drive_unrepresentable();

    test_mixed_targets_offsets();
    test_expected_len_accounts_for_target();

    test_high_slots_are_addressable();

    test_rejects_v1_blob();
    test_rejects_retired_zone_slots();
    test_rejects_slot_id_on_socket_target();
    test_rejects_bad_target_kind();
    test_rejects_target_len_mismatch();
    test_rejects_truncated_target();
    test_rejects_trailing_body_bytes();
    test_rejects_stop_deadline_overflow();

    if (g_failures == 0) {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d TEST(S) FAILED\n", g_failures);
    return 1;
}
