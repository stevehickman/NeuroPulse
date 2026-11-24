/*
 * NeurOne Bootloader — rollback floor on the eMMC's RPMB, host tests
 * OI-PRIV-32 (NP-PRIV-ANALYSIS-004); requirement REQ-SW02-16, with REQ-SW02-15
 * (NP-SW-001 §5.2.2)
 *
 * Runs the REAL np_rpmb.c (frame protocol), np_rollback_store_rpmb.c (the
 * store) and np_rollback.c (the admit / commit rule both install paths call)
 * against:
 *   - a fake RPMB device that implements the frame protocol (np_emmc_rpmb_ops
 *     below): key programming state, write counter, 256-byte half-sectors,
 *     the result register, and knobs that make it misbehave the ways a forged,
 *     replayed or failing device would;
 *   - a fake key (np_rpmb_key_get), a fake entropy source (np_entropy_get) and
 *     a fake partition switch (np_emmc_switch_partition);
 *   - an HMAC-SHA256 (np_rpmb_hmac_sha256) built here on np_signature.c's
 *     SHA-256, checked against RFC 4231 before anything relies on it, because
 *     the fake device and the code under test share it and a wrong HMAC would
 *     otherwise pass both sides.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "np_types.h"
#include "np_config.h"
#include "np_emmc.h"
#include "np_emmc_rpmb.h"
#include "np_entropy.h"
#include "np_rollback.h"
#include "np_rollback_rpmb.h"
#include "np_rollback_store.h"
#include "np_rpmb_key.h"
#include "np_rpmb_mac.h"
#include "np_signature.h"

static int g_fail;
#define CHECK(c, m) do { if (c) { printf("PASS: %s\n", m); } else { printf("FAIL: %s (%s:%d)\n", m, __FILE__, __LINE__); g_fail++; } } while (0)

/* ── HMAC-SHA256 (RFC 2104) on np_sha256, the test's oracle ───────────────── */
static void hmac_sha256(const uint8_t *key, size_t klen, const uint8_t *msg, size_t len, uint8_t out[32])
{
    uint8_t k[64] = {0};
    uint8_t buf[64 + 512];
    uint8_t inner[32];
    if (klen > 64U) { np_sha256(key, klen, k); } else { memcpy(k, key, klen); }
    for (int i = 0; i < 64; i++) buf[i] = (uint8_t)(k[i] ^ 0x36U);
    memcpy(&buf[64], msg, len);
    np_sha256(buf, 64U + len, inner);
    for (int i = 0; i < 64; i++) buf[i] = (uint8_t)(k[i] ^ 0x5cU);
    memcpy(&buf[64], inner, 32U);
    np_sha256(buf, 64U + 32U, out);
}

static int g_mac_fails;
np_status_t np_rpmb_hmac_sha256(const uint8_t key[32], const uint8_t *msg, size_t len, uint8_t mac[32])
{
    if (g_mac_fails) return NP_ERR_GENERIC;
    hmac_sha256(key, 32U, msg, len, mac);
    return NP_OK;
}

/* ── Fake key and entropy ─────────────────────────────────────────────────── */
static uint8_t  g_host_key[32];
static int      g_key_fails;
static int      g_entropy_fails;
static uint32_t g_entropy_ctr;

np_status_t np_rpmb_key_get(uint8_t key[32])
{
    if (g_key_fails) return NP_ERR_GENERIC;
    memcpy(key, g_host_key, 32U);
    return NP_OK;
}

np_status_t np_entropy_get(uint8_t *buf, size_t len)
{
    if (g_entropy_fails) return NP_ERR_GENERIC;
    for (size_t i = 0U; i < len; i++) buf[i] = (uint8_t)((g_entropy_ctr * 131U + i * 7U + 1U) & 0xFFU);
    g_entropy_ctr++;          /* a different nonce for every request */
    return NP_OK;
}

/* ── Fake partition switch ────────────────────────────────────────────────── */
static uint8_t g_part = NP_EMMC_USER_AREA;
np_status_t np_emmc_switch_partition(uint8_t part) { g_part = part; return NP_OK; }

/* ── Fake RPMB device ─────────────────────────────────────────────────────── */
#define DEV_BLOCKS 4U
static struct {
    int      key_programmed;
    uint8_t  key[32];
    uint32_t counter;
    uint8_t  blocks[DEV_BLOCKS][NP_RPMB_DATA_SIZE];
    uint16_t write_result;          /* result of the last authenticated write */
    uint16_t write_addr;
    uint8_t  resp[NP_RPMB_FRAME_SIZE];
    int      has_resp;
    int      writes;                /* committed authenticated writes */
    int      bus_accesses_outside_rpmb;
} g_dev;

/* Misbehaviour knobs. */
static uint16_t g_force_result;      /* non-zero: every response carries it (re-MACed) */
static int      g_record;            /* keep a copy of every response */
static uint8_t  g_recorded[NP_RPMB_FRAME_SIZE];
static int      g_replay;            /* answer with the recorded copy instead */
static int      g_skip_commit;       /* accept a write, report OK, commit nothing */
static uint16_t g_resp_type_xor;     /* corrupt the response type (re-MACed) */
static uint16_t g_resp_addr_xor;     /* corrupt the response address (re-MACed) */
static int      g_replay_write_result; /* answer a result read with the recorded copy */
static int      g_drop_data;         /* move the counter, report OK, store nothing */

static void dev_mac(uint8_t f[NP_RPMB_FRAME_SIZE])
{
    hmac_sha256(g_dev.key, 32U, &f[NP_RPMB_OFF_DATA], NP_RPMB_MAC_SPAN, &f[NP_RPMB_OFF_MAC]);
}

static void dev_respond(const uint8_t req[NP_RPMB_FRAME_SIZE], uint16_t type, uint16_t result,
                        uint16_t addr, int with_data)
{
    memset(g_dev.resp, 0, sizeof(g_dev.resp));
    memcpy(&g_dev.resp[NP_RPMB_OFF_NONCE], &req[NP_RPMB_OFF_NONCE], NP_RPMB_NONCE_SIZE);
    np_rpmb_put32(g_dev.resp, NP_RPMB_OFF_COUNTER, g_dev.counter);
    np_rpmb_put16(g_dev.resp, NP_RPMB_OFF_ADDRESS, addr);
    np_rpmb_put16(g_dev.resp, NP_RPMB_OFF_RESULT, result);
    np_rpmb_put16(g_dev.resp, NP_RPMB_OFF_REQ_RESP, type);
    if (with_data && addr < DEV_BLOCKS) memcpy(&g_dev.resp[NP_RPMB_OFF_DATA], g_dev.blocks[addr], NP_RPMB_DATA_SIZE);
    if (g_dev.key_programmed) dev_mac(g_dev.resp);   /* no key, no MAC */
    g_dev.has_resp = 1;
}

static np_status_t dev_write_frame(void *ctx, const uint8_t f[NP_RPMB_FRAME_SIZE], bool reliable)
{
    (void)ctx;
    if (g_part != NP_EMMC_RPMB_ACCESS) { g_dev.bus_accesses_outside_rpmb++; return NP_ERR_EMMC_WRITE; }
    uint16_t req  = np_rpmb_get16(f, NP_RPMB_OFF_REQ_RESP);
    uint16_t addr = np_rpmb_get16(f, NP_RPMB_OFF_ADDRESS);
    uint16_t nokey = g_dev.key_programmed ? NP_RPMB_RESULT_OK : NP_RPMB_RESULT_KEY_NOT_PROGRAMMED;

    switch (req) {
    case NP_RPMB_REQ_COUNTER_READ:
        dev_respond(f, NP_RPMB_RESP_COUNTER_READ, nokey, 0U, 0);
        break;
    case NP_RPMB_REQ_DATA_READ:
        dev_respond(f, NP_RPMB_RESP_DATA_READ,
                    nokey != NP_RPMB_RESULT_OK ? nokey :
                    (addr < DEV_BLOCKS ? NP_RPMB_RESULT_OK : NP_RPMB_RESULT_ADDRESS_FAILURE), addr, 1);
        break;
    case NP_RPMB_REQ_DATA_WRITE: {
        uint8_t mac[32];
        g_dev.write_addr = addr;
        g_dev.has_resp = 0;
        if (!g_dev.key_programmed) { g_dev.write_result = NP_RPMB_RESULT_KEY_NOT_PROGRAMMED; break; }
        if (!reliable) { g_dev.write_result = NP_RPMB_RESULT_GENERAL_FAILURE; break; }
        hmac_sha256(g_dev.key, 32U, &f[NP_RPMB_OFF_DATA], NP_RPMB_MAC_SPAN, mac);
        if (memcmp(mac, &f[NP_RPMB_OFF_MAC], 32U) != 0) { g_dev.write_result = NP_RPMB_RESULT_AUTH_FAILURE; break; }
        if (np_rpmb_get32(f, NP_RPMB_OFF_COUNTER) != g_dev.counter) { g_dev.write_result = NP_RPMB_RESULT_COUNTER_FAILURE; break; }
        if (addr >= DEV_BLOCKS || np_rpmb_get16(f, NP_RPMB_OFF_BLOCK_COUNT) != 1U) { g_dev.write_result = NP_RPMB_RESULT_ADDRESS_FAILURE; break; }
        g_dev.write_result = NP_RPMB_RESULT_OK;
        if (!g_skip_commit) {
            if (!g_drop_data) memcpy(g_dev.blocks[addr], &f[NP_RPMB_OFF_DATA], NP_RPMB_DATA_SIZE);
            g_dev.counter++;
            g_dev.writes++;
        }
        break;
    }
    case NP_RPMB_REQ_RESULT_READ:
        dev_respond(f, NP_RPMB_RESP_DATA_WRITE, g_dev.write_result, g_dev.write_addr, 0);
        break;
    default:
        g_dev.has_resp = 0;
        return NP_ERR_EMMC_WRITE;
    }
    return NP_OK;
}

static np_status_t dev_read_frame(void *ctx, uint8_t f[NP_RPMB_FRAME_SIZE])
{
    (void)ctx;
    if (g_part != NP_EMMC_RPMB_ACCESS) { g_dev.bus_accesses_outside_rpmb++; return NP_ERR_EMMC_READ; }
    if (!g_dev.has_resp) return NP_ERR_EMMC_READ;
    memcpy(f, g_dev.resp, NP_RPMB_FRAME_SIZE);
    if (g_force_result) { np_rpmb_put16(f, NP_RPMB_OFF_RESULT, g_force_result); dev_mac(f); }
    if (g_resp_type_xor) { np_rpmb_put16(f, NP_RPMB_OFF_REQ_RESP, (uint16_t)(np_rpmb_get16(f, NP_RPMB_OFF_REQ_RESP) ^ g_resp_type_xor)); dev_mac(f); }
    if (g_resp_addr_xor) { np_rpmb_put16(f, NP_RPMB_OFF_ADDRESS, (uint16_t)(np_rpmb_get16(f, NP_RPMB_OFF_ADDRESS) ^ g_resp_addr_xor)); dev_mac(f); }
    if (g_record) memcpy(g_recorded, f, NP_RPMB_FRAME_SIZE);
    if (g_replay || (g_replay_write_result &&
                     np_rpmb_get16(f, NP_RPMB_OFF_REQ_RESP) == NP_RPMB_RESP_DATA_WRITE)) {
        memcpy(f, g_recorded, NP_RPMB_FRAME_SIZE);
    }
    g_dev.has_resp = 0;
    return NP_OK;
}

const np_rpmb_ops_t *np_emmc_rpmb_ops(void)
{
    static const np_rpmb_ops_t FAKE_OPS = { dev_write_frame, dev_read_frame, NULL };
    return &FAKE_OPS;
}

/* ── World set-up ─────────────────────────────────────────────────────────── */
/* A provisioned device: key programmed (the host's), floor record written as
 * the factory step would. */
static void reset_world(uint32_t floor)
{
    memset(&g_dev, 0, sizeof(g_dev));
    for (int i = 0; i < 32; i++) g_host_key[i] = (uint8_t)(0xA0 + i);
    memcpy(g_dev.key, g_host_key, 32U);
    g_dev.key_programmed = 1;
    g_dev.counter = 41U;
    np_rollback_rpmb_encode(floor, g_dev.blocks[NP_ROLLBACK_RPMB_ADDRESS]);
    g_key_fails = g_entropy_fails = g_mac_fails = 0;
    g_force_result = 0U; g_record = g_replay = g_skip_commit = 0;
    g_resp_type_xor = g_resp_addr_xor = 0U;
    g_replay_write_result = g_drop_data = 0;
    g_part = NP_EMMC_USER_AREA;
}

static uint32_t dev_floor(void)
{
    uint32_t f = 0xDEADBEEFU;
    (void)np_rollback_rpmb_decode(g_dev.blocks[NP_ROLLBACK_RPMB_ADDRESS], &f);
    return f;
}

/* Every image refused: the lowest, a middle and the highest version. */
static int refuses_every_image(void)
{
    return np_rollback_admit(0U) == NP_ERR_ROLLBACK_STORE &&
           np_rollback_admit(1000U) == NP_ERR_ROLLBACK_STORE &&
           np_rollback_admit(UINT32_MAX) == NP_ERR_ROLLBACK_STORE;
}

/* ── Tests ────────────────────────────────────────────────────────────────── */

/* @verifies-none harness self-check: the HMAC the fake device and the seam share matches RFC 4231 */
static void test_hmac_oracle_matches_rfc4231(void)
{
    static const uint8_t RFC4231_TC1[32] = {
        0xb0,0x34,0x4c,0x61,0xd8,0xdb,0x38,0x53,0x5c,0xa8,0xaf,0xce,0xaf,0x0b,0xf1,0x2b,
        0x88,0x1d,0xc2,0x00,0xc9,0x83,0x3d,0xa7,0x26,0xe9,0x37,0x6c,0x2e,0x32,0xcf,0xf7 };
    static const uint8_t RFC4231_TC2[32] = {
        0x5b,0xdc,0xc1,0x46,0xbf,0x60,0x75,0x4e,0x6a,0x04,0x24,0x26,0x08,0x95,0x75,0xc7,
        0x5a,0x00,0x3f,0x08,0x9d,0x27,0x39,0x83,0x9d,0xec,0x58,0xb9,0x64,0xec,0x38,0x43 };
    uint8_t k1[20], out[32];
    memset(k1, 0x0b, sizeof(k1));
    hmac_sha256(k1, sizeof(k1), (const uint8_t *)"Hi There", 8U, out);
    CHECK(memcmp(out, RFC4231_TC1, 32U) == 0, "HMAC oracle: RFC 4231 test case 1");
    hmac_sha256((const uint8_t *)"Jefe", 4U, (const uint8_t *)"what do ya want for nothing?", 28U, out);
    CHECK(memcmp(out, RFC4231_TC2, 32U) == 0, "HMAC oracle: RFC 4231 test case 2");
}

/* @verifies REQ-SW02-16#c */
static void test_raise_through_rpmb(void)
{
    uint32_t f = 0U;
    reset_world(5U);
    CHECK(np_rollback_store_read(&f) == NP_OK && f == 5U, "the floor reads from the RPMB record");
    CHECK(np_rollback_admit(5U) == NP_OK && np_rollback_admit(4U) == NP_ERR_ROLLBACK,
          "admit compares against the RPMB floor");
    CHECK(np_rollback_commit(9U) == NP_OK, "a confirmed newer release raises the floor");
    CHECK(dev_floor() == 9U && g_dev.counter == 42U && g_dev.writes == 1,
          "the device holds the new floor and its write counter moved by one");
    CHECK(np_rollback_admit(8U) == NP_ERR_ROLLBACK && np_rollback_admit(9U) == NP_OK,
          "the raised floor refuses the release it replaced");
    CHECK(g_part == NP_EMMC_USER_AREA && g_dev.bus_accesses_outside_rpmb == 0,
          "every frame moved with the RPMB selected, and the user area is selected after");
}

/* @verifies REQ-SW02-16#c */
static void test_floor_never_goes_lower(void)
{
    uint32_t f = 0U;
    reset_world(20U);
    CHECK(np_rollback_store_raise(19U) == NP_OK && np_rollback_store_raise(20U) == NP_OK,
          "raising to a lower or equal value reports OK");
    CHECK(dev_floor() == 20U && g_dev.writes == 0 && g_dev.counter == 41U,
          "and writes nothing: the floor and the write counter are unchanged");
    CHECK(np_rollback_commit(3U) == NP_OK && dev_floor() == 20U && g_dev.writes == 0,
          "committing an older version through the rule writes nothing either");
    CHECK(np_rollback_store_raise(UINT32_MAX) == NP_OK && dev_floor() == UINT32_MAX,
          "the highest version is a valid raise");
    CHECK(np_rollback_store_raise(0U) == NP_OK && np_rollback_store_read(&f) == NP_OK && f == UINT32_MAX,
          "and nothing lowers it afterwards");
}

/* @verifies REQ-SW02-16#b */
static void test_blank_rpmb_refuses_every_image(void)
{
    uint32_t f = 77U;
    reset_world(5U);
    g_dev.key_programmed = 0;            /* a new or replaced eMMC: no key */
    memset(g_dev.blocks, 0, sizeof(g_dev.blocks));
    CHECK(np_rollback_store_read(&f) == NP_ERR_ROLLBACK_STORE && f == 77U,
          "blank RPMB (result 0x0007): the read is an error and reports no value");
    CHECK(refuses_every_image(), "blank RPMB: every image is refused, none reads as floor 0");
    CHECK(np_rollback_commit(9U) == NP_ERR_ROLLBACK_STORE && g_dev.writes == 0,
          "blank RPMB: a confirmed release cannot write a first floor");
    CHECK(g_part == NP_EMMC_USER_AREA, "blank RPMB: the user area is selected again");

    /* The key is programmed, but no floor record was ever written. */
    reset_world(5U);
    memset(g_dev.blocks[NP_ROLLBACK_RPMB_ADDRESS], 0, NP_RPMB_DATA_SIZE);
    CHECK(refuses_every_image(), "keyed RPMB with no floor record: every image is refused");
    CHECK(np_rollback_store_raise(9U) == NP_ERR_ROLLBACK_STORE && g_dev.writes == 0,
          "keyed RPMB with no floor record: the bootloader does not write a first record");

    /* A record that is not exactly a floor record. */
    reset_world(5U);
    g_dev.blocks[NP_ROLLBACK_RPMB_ADDRESS][200] = 1U;
    CHECK(refuses_every_image(), "a half-sector that is not exactly a floor record refuses every image");
    reset_world(5U);
    g_dev.blocks[NP_ROLLBACK_RPMB_ADDRESS][NP_ROLLBACK_RPMB_OFF_FORMAT + 1U] = 2U;
    CHECK(refuses_every_image(), "a record of another format refuses every image");
}

/* @verifies REQ-SW02-16#b */
static void test_wrong_key_refuses_every_image(void)
{
    reset_world(5U);
    g_dev.key[0] ^= 0x01U;               /* the eMMC was programmed with another key */
    CHECK(refuses_every_image(), "wrong key (the response MAC does not verify): every image is refused");
    CHECK(np_rollback_commit(9U) == NP_ERR_ROLLBACK_STORE && g_dev.writes == 0 && dev_floor() == 5U,
          "wrong key: no raise is written");

    /* A response whose data was altered after the device MACed it. */
    reset_world(5U);
    g_record = 1;
    uint32_t f = 0U;
    (void)np_rollback_store_read(&f);
    g_record = 0;
    g_recorded[NP_RPMB_OFF_DATA + NP_ROLLBACK_RPMB_OFF_FLOOR + 3U] = 1U;  /* floor 5 -> 1 */
    g_replay = 1;
    CHECK(refuses_every_image(), "a frame altered in transit fails its MAC and refuses every image");
}

/* @verifies REQ-SW02-16#a */
static void test_replayed_read_response_is_refused(void)
{
    uint32_t f = 0U;
    reset_world(5U);
    g_record = 1;
    CHECK(np_rollback_store_read(&f) == NP_OK && f == 5U, "a genuine response reports floor 5");
    g_record = 0;                         /* the attacker keeps that response */
    CHECK(np_rollback_commit(9U) == NP_OK && dev_floor() == 9U, "the floor is then raised to 9");
    g_replay = 1;                         /* and replays the floor-5 response */
    CHECK(np_rollback_store_read(&f) == NP_ERR_ROLLBACK_STORE && f == 5U,
          "replayed response (stale nonce, genuine MAC): refused, the old floor is not reported");
    CHECK(np_rollback_admit(6U) == NP_ERR_ROLLBACK_STORE,
          "replay: the image the old floor would admit is refused");
    CHECK(refuses_every_image(), "replay: every image is refused");
}

/* @verifies REQ-SW02-16#a */
static void test_wrong_type_or_address_is_refused(void)
{
    reset_world(5U);
    g_resp_type_xor = 0x0600U;            /* 0x0400 -> 0x0200: a counter response */
    CHECK(refuses_every_image(), "a response of another type, correctly MACed, refuses every image");
    reset_world(5U);
    g_resp_addr_xor = 0x0001U;            /* the record of another address */
    CHECK(refuses_every_image(), "a response for another address, correctly MACed, refuses every image");
}

/* @verifies REQ-SW02-16#c */
static void test_stale_write_counter_is_not_a_raise(void)
{
    reset_world(5U);
    g_skip_commit = 1;                    /* reports OK, commits nothing, counter unmoved */
    CHECK(np_rollback_commit(9U) == NP_ERR_ROLLBACK_STORE,
          "a write whose result shows the counter unmoved is not reported as a raise");
    CHECK(dev_floor() == 5U, "and the floor is unchanged");

    /* A replayed write result (stale counter, genuine MAC) answering a write
     * the device did commit: only the counter check can see it. */
    {
        reset_world(5U);
        uint8_t data[NP_RPMB_DATA_SIZE];
        np_rollback_rpmb_encode(9U, data);
        g_part = NP_EMMC_RPMB_ACCESS;
        g_record = 1;
        CHECK(np_rpmb_write_block(np_emmc_rpmb_ops(), g_host_key, NP_ROLLBACK_RPMB_ADDRESS, data) == NP_OK,
              "a genuine authenticated write, its result frame recorded");
        g_record = 0;
        g_part = NP_EMMC_USER_AREA;
    }
    g_replay_write_result = 1;             /* the next write's result is that old frame */
    CHECK(np_rollback_commit(11U) == NP_ERR_ROLLBACK_STORE,
          "a replayed write result (counter not one above the request's) is refused");
    g_replay_write_result = 0;

    /* The counter moved but the data was not stored: only the read-back sees it. */
    reset_world(5U);
    g_drop_data = 1;
    CHECK(np_rollback_commit(9U) == NP_ERR_ROLLBACK_STORE && dev_floor() == 5U,
          "a write whose read-back does not return the new floor is not reported as a raise");
}

/* @verifies REQ-SW02-16#b */
static void test_any_result_error_refuses_every_image(void)
{
    static const uint16_t RESULTS[] = {
        NP_RPMB_RESULT_GENERAL_FAILURE, NP_RPMB_RESULT_AUTH_FAILURE, NP_RPMB_RESULT_COUNTER_FAILURE,
        NP_RPMB_RESULT_ADDRESS_FAILURE, NP_RPMB_RESULT_WRITE_FAILURE, NP_RPMB_RESULT_READ_FAILURE,
        NP_RPMB_RESULT_KEY_NOT_PROGRAMMED, NP_RPMB_RESULT_COUNTER_EXPIRED,
        (uint16_t)(NP_RPMB_RESULT_COUNTER_EXPIRED | NP_RPMB_RESULT_WRITE_FAILURE),
    };
    int all_refused = 1, none_written = 1;
    for (size_t i = 0U; i < sizeof(RESULTS) / sizeof(RESULTS[0]); i++) {
        reset_world(5U);
        g_force_result = RESULTS[i];
        if (!refuses_every_image()) all_refused = 0;
        if (np_rollback_commit(9U) != NP_ERR_ROLLBACK_STORE || dev_floor() != 5U) none_written = 0;
    }
    CHECK(all_refused, "every non-zero result, the counter-expired flag included, refuses every image");
    CHECK(none_written, "and no raise is reported under any of them");

    /* A result error only on the write's result frame. */
    reset_world(5U);
    g_dev.counter = UINT32_MAX;            /* the counter can take no more writes */
    CHECK(np_rollback_commit(9U) == NP_ERR_ROLLBACK_STORE && dev_floor() == 5U,
          "an expired write counter: the raise fails and the floor is unchanged");
}

/* @verifies REQ-SW02-16#d */
static void test_missing_key_mac_or_entropy_refuses_every_image(void)
{
    reset_world(5U);
    g_key_fails = 1;
    CHECK(refuses_every_image(), "no RPMB key: every image is refused");
    reset_world(5U);
    g_mac_fails = 1;
    CHECK(refuses_every_image(), "no MAC primitive: every image is refused");
    reset_world(5U);
    g_entropy_fails = 1;
    CHECK(refuses_every_image(), "no entropy for the nonce: every image is refused");
    CHECK(np_rollback_commit(9U) == NP_ERR_ROLLBACK_STORE && dev_floor() == 5U,
          "no entropy: nothing is written");
    CHECK(g_part == NP_EMMC_USER_AREA, "the user area is selected again after a failure");
}

int main(void)
{
    test_hmac_oracle_matches_rfc4231();
    test_raise_through_rpmb();
    test_floor_never_goes_lower();
    test_blank_rpmb_refuses_every_image();
    test_wrong_key_refuses_every_image();
    test_replayed_read_response_is_refused();
    test_wrong_type_or_address_is_refused();
    test_stale_write_counter_is_not_a_raise();
    test_any_result_error_refuses_every_image();
    test_missing_key_mac_or_entropy_refuses_every_image();
    if (g_fail != 0) { printf("FAILED (%d)\n", g_fail); return 1; }
    printf("OK\n");
    return 0;
}
