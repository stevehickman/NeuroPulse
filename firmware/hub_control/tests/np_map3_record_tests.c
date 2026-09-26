/*
 * np_map3_record_tests.c — OI-NVRAM-08: a Map 3 journal written under version
 *                          n loses no record when read under n+1
 * Document: NP-FW-NVRAM-001 Rev 3 §7.2 (D-5, D-25), RISK-NVRAM-01
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * The inventory blob may discard itself on a version change, because it is a
 * cache and a re-poll rebuilds it.  Map 3 may not: it is the only record of
 * the sessions it holds.  §7.2 therefore gives it a tail-additive version rule,
 * and OI-NVRAM-08 asks for the CI check that the rule actually holds.
 *
 * ── The check ────────────────────────────────────────────────────────────────
 *
 * check_no_record_lost(reader) builds journal images and requires the reader
 * to return every row, with every version-1 field intact, in order:
 *
 *   S1 upgrade     rows written under v1 (np_map3_encode, the real writer),
 *                  read after the update.  The reader of n+1 is the same scan
 *                  code: version tolerance is a property of the scan, not a
 *                  per-version branch, which is what makes the check mean
 *                  anything before n+1 exists.
 *   S2 appended    v1 rows followed by rows the NEXT two versions would write
 *                  (+8 and +16 tail bytes), exactly as the journal looks after
 *                  an update adds rows to an unsynced history.
 *   S3 rollback    S2's image read by this reader, which knows only v1: the
 *                  boot-bank fallback case.  Newer rows are decoded for their
 *                  v1 fields, never dropped.
 *   S4 torn tail   S2 with a partial last row: costs that row only (L-4).
 *   S5 file bound  1,536 v1 rows fill the 49,152-byte file exactly (D-24) and
 *                  all of them come back.
 *
 * ── Falsified in both directions (NP-CONV-001 §8) ────────────────────────────
 *
 * The same check runs against the production reader, which must pass, and
 * against four readers that each break §7.2 in a way someone could plausibly
 * write, each of which must FAIL:
 *
 *   M1 reject-and-rebuild  the inventory blob's policy: a row of any other
 *                          version discards the journal.  RISK-NVRAM-01's cause.
 *   M2 fixed stride        steps by the size it would write, not by `len`: a
 *                          longer row desynchronises every row after it.
 *   M3 exact length        stops at a row whose `len` is not the v1 size.
 *   M4 version ceiling     "refuse rows from firmware newer than me": loses
 *                          everything an updated bank wrote once rolled back.
 *
 * A check that passes all four has no teeth.  A check that fails the real
 * reader is wrong.  Both are asserted, every run.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "np_map3_record.h"
#include "np_crypto.h"

static int g_fail_count;

#define ASSERT(cond, msg)                                                    \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, (msg));         \
            g_fail_count++;                                                  \
        }                                                                    \
    } while (0)

/* ── Journal images ────────────────────────────────────────────────────────── */

#define MAX_ROWS   1600U
#define IMG_BYTES  (MAX_ROWS * 48U)

static uint8_t       g_img[IMG_BYTES];
static np_map3_rec_t g_want[MAX_ROWS];
static np_map3_row_t g_got[MAX_ROWS];

static void make_rec(np_map3_rec_t *r, uint32_t i)
{
    memset(r, 0, sizeof(*r));
    for (unsigned k = 0U; k < NP_MAP3_UID_LEN; k++) {
        r->uid[k] = (uint8_t)(0xA0U + (i % 80U) + k * 7U);   /* 80 sockets */
    }
    r->seq            = 1000U + i;
    r->ordinal        = 5000U + i / 3U;
    r->session_count  = (uint16_t)(i + 1U);
    r->dose_accum     = 0x10000U * i + 17U;
    r->throttle_count = (uint16_t)(i % 5U);
    r->fault_flags    = (uint8_t)(i & 0x0FU);
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/*
 * What firmware version `ver` would write, built the only way §7.2 permits:
 * the version-1 row, unchanged, with `tail` bytes appended before the CRC.
 * This is the model of n+1 the check needs, and it is built from the real
 * v1 encoder rather than a copy of its layout.
 */
static size_t encode_future(const np_map3_rec_t *r, uint8_t ver, uint8_t tail,
                            uint8_t *out)
{
    size_t n = np_map3_encode(r, out, NP_MAP3_V1_BYTES);
    if (n != NP_MAP3_V1_BYTES) {
        return 0U;
    }
    size_t len = NP_MAP3_V1_BYTES + tail;
    /* Move nothing: fields [2, 28) stay where they are.  The tail sits
     * between them and the CRC. */
    for (size_t k = 0U; k < tail; k++) {
        out[NP_MAP3_V1_FIELDS_END + k] = (uint8_t)(0x5AU ^ k ^ ver);
    }
    out[0] = (uint8_t)len;
    out[1] = ver;
    put_u32(&out[len - NP_MAP3_CRC_BYTES],
            np_crc32(out, (uint32_t)(len - NP_MAP3_CRC_BYTES)));
    return len;
}

typedef struct {
    size_t   bytes;
    uint32_t rows;
} img_t;

/* n_v1 rows at version 1, then n_v2 at version 2 (+8), then n_v3 at 3 (+16). */
static img_t build(uint32_t n_v1, uint32_t n_v2, uint32_t n_v3)
{
    img_t im = { 0U, 0U };
    for (uint32_t i = 0U; i < n_v1 + n_v2 + n_v3; i++) {
        make_rec(&g_want[i], i);
        size_t n;
        if (i < n_v1) {
            n = np_map3_encode(&g_want[i], &g_img[im.bytes], NP_MAP3_V1_BYTES);
        } else if (i < n_v1 + n_v2) {
            n = encode_future(&g_want[i], 2U, 8U, &g_img[im.bytes]);
        } else {
            n = encode_future(&g_want[i], 3U, 16U, &g_img[im.bytes]);
        }
        im.bytes += n;
        im.rows++;
    }
    return im;
}

/* ── Readers ───────────────────────────────────────────────────────────────── */

/* A reader returns the rows it recovered, in order. */
typedef uint32_t (*reader_fn)(const uint8_t *buf, size_t len, np_map3_row_t *out,
                              uint32_t cap);

typedef struct {
    np_map3_row_t *out;
    uint32_t       cap;
    uint32_t       n;
} collect_t;

static bool collect(const np_map3_row_t *row, void *ctx)
{
    collect_t *c = (collect_t *)ctx;
    if (c->n >= c->cap) {
        return false;
    }
    c->out[c->n++] = *row;
    return true;
}

/* The production reader. */
static uint32_t reader_real(const uint8_t *buf, size_t len, np_map3_row_t *out,
                            uint32_t cap)
{
    collect_t c = { out, cap, 0U };
    np_map3_scan_t s;
    np_map3_scan(buf, len, collect, &c, &s);
    return c.n;
}

/* M1 — reject-and-rebuild: any row of another version discards the journal. */
static uint32_t reader_reject_rebuild(const uint8_t *buf, size_t len,
                                      np_map3_row_t *out, uint32_t cap)
{
    uint32_t n = reader_real(buf, len, out, cap);
    for (uint32_t i = 0U; i < n; i++) {
        if (out[i].ver != NP_MAP3_VERSION) {
            return 0U;
        }
    }
    return n;
}

/* M2 — fixed stride: steps by the v1 size and checks the CRC where a v1 row
 * would have it.  Written without np_map3_scan, as a hand-rolled reader would
 * be. */
static uint32_t reader_fixed_stride(const uint8_t *buf, size_t len,
                                    np_map3_row_t *out, uint32_t cap)
{
    uint32_t n = 0U;
    for (size_t off = 0U; off + NP_MAP3_V1_BYTES <= len && n < cap;
         off += NP_MAP3_V1_BYTES) {
        const uint8_t *p = buf + off;
        uint32_t crc = (uint32_t)p[28] | ((uint32_t)p[29] << 8) |
                       ((uint32_t)p[30] << 16) | ((uint32_t)p[31] << 24);
        if (crc != np_crc32(p, NP_MAP3_V1_BYTES - NP_MAP3_CRC_BYTES)) {
            break;
        }
        /* Decode exactly as the real scan would, from this 32-byte window. */
        uint8_t tmp[NP_MAP3_V1_BYTES];
        memcpy(tmp, p, sizeof(tmp));
        tmp[0] = (uint8_t)NP_MAP3_V1_BYTES;
        if (reader_real(tmp, sizeof(tmp), &out[n], 1U) != 1U) {
            break;
        }
        n++;
    }
    return n;
}

/* M3 — exact length: stops at the first row that is not the v1 size. */
static uint32_t reader_exact_len(const uint8_t *buf, size_t len,
                                 np_map3_row_t *out, uint32_t cap)
{
    uint32_t n = reader_real(buf, len, out, cap);
    for (uint32_t i = 0U; i < n; i++) {
        if (out[i].len != NP_MAP3_V1_BYTES) {
            return i;
        }
    }
    return n;
}

/* M4 — version ceiling: stops at the first row newer than this reader. */
static uint32_t reader_version_ceiling(const uint8_t *buf, size_t len,
                                       np_map3_row_t *out, uint32_t cap)
{
    uint32_t n = reader_real(buf, len, out, cap);
    for (uint32_t i = 0U; i < n; i++) {
        if (out[i].ver > NP_MAP3_VERSION) {
            return i;
        }
    }
    return n;
}

/* ── The check ─────────────────────────────────────────────────────────────── */

static bool same_v1_fields(const np_map3_rec_t *a, const np_map3_rec_t *b)
{
    return memcmp(a->uid, b->uid, NP_MAP3_UID_LEN) == 0 && a->seq == b->seq &&
           a->ordinal == b->ordinal && a->session_count == b->session_count &&
           a->dose_accum == b->dose_accum &&
           a->throttle_count == b->throttle_count &&
           a->fault_flags == b->fault_flags;
}

/* Every expected row back, in order, v1 fields intact.  Reports the first
 * shortfall under `what` when `loud`. */
static bool all_back(reader_fn rd, const img_t *im, size_t bytes, uint32_t want,
                     const char *what, bool loud)
{
    uint32_t got = rd(g_img, bytes, g_got, MAX_ROWS);
    if (got != want) {
        if (loud) {
            printf("    %s: %u of %u rows recovered\n", what, got, want);
        }
        return false;
    }
    for (uint32_t i = 0U; i < want; i++) {
        if (!same_v1_fields(&g_got[i].rec, &g_want[i])) {
            if (loud) {
                printf("    %s: row %u recovered with wrong fields\n", what, i);
            }
            return false;
        }
    }
    (void)im;
    return true;
}

static bool check_no_record_lost(reader_fn rd, bool loud)
{
    bool ok = true;
    img_t im;

    /* S1 — upgrade: a v1 journal read after the update. */
    im = build(40U, 0U, 0U);
    ok &= all_back(rd, &im, im.bytes, im.rows, "S1 upgrade", loud);

    /* S2 — the updated firmware appends to the unsynced history. */
    im = build(40U, 20U, 5U);
    ok &= all_back(rd, &im, im.bytes, im.rows, "S2 appended", loud);

    /* S3 — rollback: the same image, read by a reader that knows only v1.
     * For the production reader the tail must be reported, not decoded. */
    ok &= all_back(rd, &im, im.bytes, im.rows, "S3 rollback", loud);

    /* S4 — a torn last row costs that row and nothing before it. */
    ok &= all_back(rd, &im, im.bytes - 5U, im.rows - 1U, "S4 torn tail", loud);

    /* S5 — the D-24 bound: 1,536 v1 rows, exactly 49,152 bytes. */
    im = build(NP_MAP3_V1_ROWS, 0U, 0U);
    if (im.bytes != NP_MAP3_FILE_BYTES) {
        if (loud) {
            printf("    S5 bound: %zu bytes, D-24 says %u\n", im.bytes,
                   NP_MAP3_FILE_BYTES);
        }
        ok = false;
    }
    ok &= all_back(rd, &im, im.bytes, im.rows, "S5 file bound", loud);

    return ok;
}

/* ── Tests ─────────────────────────────────────────────────────────────────── */

static void test_the_production_reader_loses_nothing(void)
{
    printf("[OI-NVRAM-08] a journal written under v1 loses no record under v1+1\n");
    ASSERT(check_no_record_lost(reader_real, true),
           "the production reader lost a record across a version change");

    /* And S3's detail: newer rows carry their tail length, older ones none. */
    img_t im = build(2U, 1U, 1U);
    uint32_t n = reader_real(g_img, im.bytes, g_got, MAX_ROWS);
    ASSERT(n == 4U, "mixed image did not yield four rows");
    ASSERT(g_got[0].ver == 1U && g_got[0].tail_len == 0U, "v1 row misreported");
    ASSERT(g_got[2].ver == 2U && g_got[2].tail_len == 8U, "v2 tail not reported");
    ASSERT(g_got[3].ver == 3U && g_got[3].tail_len == 16U, "v3 tail not reported");
}

static void test_the_check_fails_every_rule_breaking_reader(void)
{
    printf("[OI-NVRAM-08] falsification: the check fails every reader that "
           "breaks §7.2\n");
    ASSERT(!check_no_record_lost(reader_reject_rebuild, false),
           "M1 reject-and-rebuild passed the check — it has no teeth");
    ASSERT(!check_no_record_lost(reader_fixed_stride, false),
           "M2 fixed stride passed the check — it has no teeth");
    ASSERT(!check_no_record_lost(reader_exact_len, false),
           "M3 exact length passed the check — it has no teeth");
    ASSERT(!check_no_record_lost(reader_version_ceiling, false),
           "M4 version ceiling passed the check — it has no teeth");

    /* And the other half: each mutant is RIGHT on a pure-v1 journal.  Without
     * this, a mutant that was simply broken would falsify nothing about
     * versioning. */
    img_t im = build(40U, 0U, 0U);
    ASSERT(all_back(reader_reject_rebuild, &im, im.bytes, im.rows, "M1 v1", true),
           "M1 fails on a pure-v1 journal — it is broken, not version-intolerant");
    ASSERT(all_back(reader_fixed_stride, &im, im.bytes, im.rows, "M2 v1", true),
           "M2 fails on a pure-v1 journal — it is broken, not version-intolerant");
    ASSERT(all_back(reader_exact_len, &im, im.bytes, im.rows, "M3 v1", true),
           "M3 fails on a pure-v1 journal — it is broken, not version-intolerant");
    ASSERT(all_back(reader_version_ceiling, &im, im.bytes, im.rows, "M4 v1", true),
           "M4 fails on a pure-v1 journal — it is broken, not version-intolerant");
}

static void test_a_row_that_does_not_verify_ends_the_prefix(void)
{
    printf("[§4.2(b)] a bad row ends the valid prefix; nothing after it is trusted\n");
    img_t im = build(10U, 0U, 0U);
    np_map3_scan_t s;

    g_img[5U * NP_MAP3_V1_BYTES + 12U] ^= 0x01U;   /* a payload bit in row 5 */
    np_map3_scan(g_img, im.bytes, NULL, NULL, &s);
    ASSERT(s.rows == 5U && s.end == NP_MAP3_END_BAD_CRC,
           "a corrupted payload was accepted, or ended the scan in the wrong place");

    im = build(10U, 0U, 0U);
    g_img[5U * NP_MAP3_V1_BYTES] = 40U;              /* a flipped length */
    np_map3_scan(g_img, im.bytes, NULL, NULL, &s);
    ASSERT(s.rows == 5U && s.end == NP_MAP3_END_BAD_CRC,
           "a corrupted length walked the scan into the next row");

    im = build(10U, 0U, 0U);
    g_img[5U * NP_MAP3_V1_BYTES] = 8U;               /* shorter than any row */
    np_map3_scan(g_img, im.bytes, NULL, NULL, &s);
    ASSERT(s.rows == 5U && s.end == NP_MAP3_END_BAD_LEN, "an impossible length was accepted");

    /* ver == 0 is written by no firmware, even under a valid CRC. */
    im = build(10U, 0U, 0U);
    uint8_t *p = &g_img[5U * NP_MAP3_V1_BYTES];
    p[1] = 0U;
    put_u32(&p[28], np_crc32(p, 28U));
    np_map3_scan(g_img, im.bytes, NULL, NULL, &s);
    ASSERT(s.rows == 5U && s.end == NP_MAP3_END_BAD_VER, "a version-0 row was accepted");

    /* The UID is under the CRC: a row moved to another module's UID fails. */
    im = build(10U, 0U, 0U);
    g_img[5U * NP_MAP3_V1_BYTES + 2U] ^= 0xFFU;
    np_map3_scan(g_img, im.bytes, NULL, NULL, &s);
    ASSERT(s.rows == 5U, "a row with a rewritten UID verified");

    /* An empty journal is clean and holds nothing. */
    np_map3_scan(g_img, 0U, NULL, NULL, &s);
    ASSERT(s.rows == 0U && s.end == NP_MAP3_END_CLEAN, "an empty journal was not clean");
}

static void test_the_v1_row_is_the_budgeted_row(void)
{
    printf("[§3.3.1.5] the v1 row is 32 bytes and none straddles a 256-byte "
           "program unit\n");
    ASSERT(NP_MAP3_V1_BYTES == 32U, "the v1 row is not 32 bytes (D-24)");
    ASSERT(256U % NP_MAP3_V1_BYTES == 0U, "a v1 row can straddle a program unit");
    ASSERT(NP_MAP3_V1_ROWS == 1536U, "the file does not hold 1,536 v1 rows (D-24)");

    np_map3_rec_t r;
    make_rec(&r, 7U);
    uint8_t buf[NP_MAP3_V1_BYTES];
    ASSERT(np_map3_encode(&r, buf, sizeof(buf) - 1U) == 0U,
           "the encoder wrote into a buffer too small for a row");
    ASSERT(np_map3_encode(&r, buf, sizeof(buf)) == NP_MAP3_V1_BYTES, "encode failed");
    ASSERT(buf[0] == NP_MAP3_V1_BYTES && buf[1] == NP_MAP3_VERSION,
           "the header does not describe the row");
    ASSERT(buf[27] == 0U, "the reserved byte is not written 0");
}

int main(void)
{
    printf("np_map3_record_tests — NP-FW-NVRAM-001 Rev 3 §7.2 (OI-NVRAM-08)\n");

    test_the_v1_row_is_the_budgeted_row();
    test_a_row_that_does_not_verify_ends_the_prefix();
    test_the_production_reader_loses_nothing();
    test_the_check_fails_every_rule_breaking_reader();

    if (g_fail_count == 0) {
        printf("PASS\n");
    } else {
        printf("%d FAILURE(S)\n", g_fail_count);
    }
    return g_fail_count;
}
