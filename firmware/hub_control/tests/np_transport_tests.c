/*
 * NeurOne Hub Control — Protocol Transport Host Tests (OI-HUB-MAIN-01)
 * Document: NP-FW-HUB-001 Rev 1 §2
 *
 * Host-native tests for the length-prefixed reassembly + single-slot mailbox:
 *   - single-chunk and fragmented delivery reassemble identically
 *   - split length prefix
 *   - malformed length (0 / > NP_HUB_PROTO_BLOB_MAX) discarded
 *   - receive timeout when nothing is ready
 *   - busy backpressure (second blob refused until pickup)
 *   - reset() aborts a partial frame without corrupting the next
 *   - undersized receive buffer drops the blob and frees the mailbox
 *   - max-size blob round-trips through the buffers
 *
 * The wait/signal/critical HAL is host-modeled in np_transport.c (NPTEST_HOST):
 * wait() reports whether a blob is latched, so tests feed-then-receive.
 * IEC 62304 Class B — SW-02 hub control.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../include/np_transport.h"

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

/* Build a framed stream: 4-byte LE length prefix + `len` bytes of pattern. */
static size_t frame(uint8_t *out, size_t payload_len)
{
    out[0] = (uint8_t)(payload_len & 0xFFU);
    out[1] = (uint8_t)((payload_len >> 8) & 0xFFU);
    out[2] = (uint8_t)((payload_len >> 16) & 0xFFU);
    out[3] = (uint8_t)((payload_len >> 24) & 0xFFU);
    for (size_t i = 0; i < payload_len; i++) {
        out[NP_TRANSPORT_LEN_PREFIX + i] = (uint8_t)((i * 3U + 1U) & 0xFFU);
    }
    return NP_TRANSPORT_LEN_PREFIX + payload_len;
}

/* True if `buf` holds the pattern payload of `len` produced by frame(). */
static bool payload_ok(const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (buf[i] != (uint8_t)((i * 3U + 1U) & 0xFFU)) { return false; }
    }
    return true;
}

static uint8_t g_out[NP_HUB_PROTO_BLOB_MAX];

/* ── Tests ────────────────────────────────────────────────────────────────────── */

static void test_single_chunk(void)
{
    np_transport_init();
    uint8_t framed[4 + 64];
    size_t  n = frame(framed, 64U);
    check(np_transport_feed(framed, n) == NP_HUB_OK, "single: feed ok");

    size_t got = 0;
    check(np_hal_proto_queue_receive(g_out, sizeof g_out, &got, 10U) == NP_HUB_OK,
          "single: receive ok");
    check(got == 64U, "single: length correct");
    check(payload_ok(g_out, 64U), "single: payload correct");
}

static void test_fragmented(void)
{
    np_transport_init();
    uint8_t framed[4 + 200];
    size_t  n = frame(framed, 200U);
    /* Deliver in awkward fragments: 2, 5, 100, remainder. */
    check(np_transport_feed(framed, 2U) == NP_HUB_OK, "frag: chunk 1 (split prefix)");
    check(np_transport_feed(framed + 2, 5U) == NP_HUB_OK, "frag: chunk 2");
    check(np_transport_feed(framed + 7, 100U) == NP_HUB_OK, "frag: chunk 3");
    check(np_transport_feed(framed + 107, n - 107) == NP_HUB_OK, "frag: chunk 4");

    size_t got = 0;
    check(np_hal_proto_queue_receive(g_out, sizeof g_out, &got, 10U) == NP_HUB_OK,
          "frag: receive ok");
    check(got == 200U && payload_ok(g_out, 200U), "frag: reassembled payload correct");
}

static void test_bad_length_zero(void)
{
    np_transport_init();
    uint8_t prefix[4] = { 0, 0, 0, 0 };  /* length 0 */
    check(np_transport_feed(prefix, sizeof prefix) == NP_HUB_ERR_INVALID_ARG,
          "bad-zero: zero length rejected");
    size_t got = 0;
    check(np_hal_proto_queue_receive(g_out, sizeof g_out, &got, 0U) == NP_HUB_ERR_TIMEOUT,
          "bad-zero: nothing latched");
}

static void test_bad_length_too_big(void)
{
    np_transport_init();
    uint32_t big = (uint32_t)NP_HUB_PROTO_BLOB_MAX + 1U;
    uint8_t prefix[4] = {
        (uint8_t)(big & 0xFFU), (uint8_t)((big >> 8) & 0xFFU),
        (uint8_t)((big >> 16) & 0xFFU), (uint8_t)((big >> 24) & 0xFFU)
    };
    check(np_transport_feed(prefix, sizeof prefix) == NP_HUB_ERR_INVALID_ARG,
          "bad-big: over-max length rejected");
    /* After discard, a valid frame still parses. */
    uint8_t framed[4 + 16];
    size_t  n = frame(framed, 16U);
    check(np_transport_feed(framed, n) == NP_HUB_OK, "bad-big: recovers for next frame");
    size_t got = 0;
    check(np_hal_proto_queue_receive(g_out, sizeof g_out, &got, 10U) == NP_HUB_OK &&
          got == 16U, "bad-big: next frame received");
}

static void test_receive_timeout(void)
{
    np_transport_init();
    size_t got = 0;
    check(np_hal_proto_queue_receive(g_out, sizeof g_out, &got, 5U) == NP_HUB_ERR_TIMEOUT,
          "timeout: no data → timeout");
}

static void test_busy_backpressure(void)
{
    np_transport_init();
    uint8_t a[4 + 32];
    uint8_t b[4 + 48];
    (void)frame(a, 32U);
    size_t nb = frame(b, 48U);
    check(np_transport_feed(a, sizeof a) == NP_HUB_OK, "busy: blob A completes");
    check(np_transport_feed(b, nb) == NP_HUB_ERR_SESSION_ACTIVE,
          "busy: blob B refused while A pending");

    size_t got = 0;
    check(np_hal_proto_queue_receive(g_out, sizeof g_out, &got, 10U) == NP_HUB_OK &&
          got == 32U, "busy: A received");
    /* Mailbox now free — resend B. */
    check(np_transport_feed(b, nb) == NP_HUB_OK, "busy: B accepted after pickup");
    check(np_hal_proto_queue_receive(g_out, sizeof g_out, &got, 10U) == NP_HUB_OK &&
          got == 48U && payload_ok(g_out, 48U), "busy: B received");
}

static void test_reset_aborts_partial(void)
{
    np_transport_init();
    uint8_t framed[4 + 100];
    size_t  n = frame(framed, 100U);
    /* Feed prefix + half the payload, then abort. */
    check(np_transport_feed(framed, 4U + 40U) == NP_HUB_OK, "reset: partial fed");
    np_transport_reset();
    /* A fresh frame must parse cleanly against the reset state. */
    uint8_t fresh[4 + 24];
    size_t  fn = frame(fresh, 24U);
    check(np_transport_feed(fresh, fn) == NP_HUB_OK, "reset: fresh frame fed");
    size_t got = 0;
    check(np_hal_proto_queue_receive(g_out, sizeof g_out, &got, 10U) == NP_HUB_OK &&
          got == 24U && payload_ok(g_out, 24U),
          "reset: fresh frame received (no stale bytes)");
    (void)n;
}

static void test_receive_buffer_too_small(void)
{
    np_transport_init();
    uint8_t framed[4 + 80];
    size_t  n = frame(framed, 80U);
    (void)np_transport_feed(framed, n);

    uint8_t small[40];
    size_t  got = 999;
    check(np_hal_proto_queue_receive(small, sizeof small, &got, 10U) ==
          NP_HUB_ERR_PARAMS_TOO_LONG, "small-buf: oversized blob rejected");
    /* Mailbox freed — a later receive must not re-deliver the dropped blob. */
    check(np_hal_proto_queue_receive(g_out, sizeof g_out, &got, 0U) == NP_HUB_ERR_TIMEOUT,
          "small-buf: mailbox freed after drop");
}

static void test_null_args(void)
{
    np_transport_init();
    size_t got = 0;
    check(np_hal_proto_queue_receive(NULL, 10U, &got, 0U) == NP_HUB_ERR_INVALID_ARG,
          "null: receive NULL buf rejected");
    check(np_hal_proto_queue_receive(g_out, sizeof g_out, NULL, 0U) == NP_HUB_ERR_INVALID_ARG,
          "null: receive NULL out-len rejected");
    check(np_transport_feed(NULL, 4U) == NP_HUB_ERR_INVALID_ARG,
          "null: feed NULL data rejected");
    check(np_transport_feed(NULL, 0U) == NP_HUB_OK, "null: zero-length feed is a no-op");
}

static void test_max_size_blob(void)
{
    np_transport_init();
    static uint8_t framed[NP_TRANSPORT_LEN_PREFIX + NP_HUB_PROTO_BLOB_MAX];
    size_t payload = NP_HUB_PROTO_BLOB_MAX;
    size_t n = frame(framed, payload);
    check(np_transport_feed(framed, n) == NP_HUB_OK, "max: full-size blob fed");
    size_t got = 0;
    check(np_hal_proto_queue_receive(g_out, sizeof g_out, &got, 10U) == NP_HUB_OK &&
          got == payload && payload_ok(g_out, payload),
          "max: full-size blob received intact");
}

int main(void)
{
    printf("── np_transport_tests (OI-HUB-MAIN-01) ──\n");

    test_single_chunk();
    test_fragmented();
    test_bad_length_zero();
    test_bad_length_too_big();
    test_receive_timeout();
    test_busy_backpressure();
    test_reset_aborts_partial();
    test_receive_buffer_too_small();
    test_null_args();
    test_max_size_blob();

    if (g_failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}
