/*
 * NeurOne Per-Person UHDR Keys — file sealing host tests
 * Document: NP-FW-EMMC-002 §C.8.7 (OI-PRIV-34, NP-PRIV-ANALYSIS-004)
 *
 * Seals files under a profile with the NPTEST_HOST stubs (np_uhdr_key.c: TRNG,
 * GCM; np_uhdr_profile.c: slot table; np_uhdr_seal.c: file KDF) and opens them
 * again, then tampers with every part of the format.  The stubs authenticate,
 * so a wrong key or a changed chunk really fails the tag.
 *
 * Return convention: 0 = PASS, non-zero = failure count.
 */

#include "np_uhdr_key.h"
#include "np_uhdr_profile.h"
#include "np_uhdr_seal.h"

#include <stdio.h>
#include <string.h>

extern void np_uhdr_host_reset(void);
extern np_uhdr_status_t np_uhdr_hal_attempts_write(uint32_t failed);
extern void np_uhdr_host_profile_reset(void);

static int g_fail_count = 0;

#define ASSERT(cond, msg)                                            \
    do {                                                             \
        if (!(cond)) {                                              \
            printf("FAIL [%s:%d] %s\n", __func__, __LINE__, (msg)); \
            g_fail_count++;                                         \
        }                                                          \
    } while (0)

static const uint8_t CRED_ANA[] = { 'a', 'n', 'a' };
static const uint8_t CRED_BEN[] = { 'b', 'e', 'n' };

/* ── A RAM "file": header + chunks, as the medium would hold them ─────────── */

#define MAX_CHUNKS 16U

typedef struct {
    np_uhdr_seal_header_t header;
    uint8_t  chunk[MAX_CHUNKS][NP_UHDR_SEAL_BLOCK_BYTES];
    size_t   len[MAX_CHUNKS];
    size_t   count;
    unsigned fail_at;      /* sink fails on this chunk index + 1 (0 = never) */
} file_t;

static np_uhdr_status_t sink(void *ctx, const uint8_t *chunk, size_t len)
{
    file_t *f = (file_t *)ctx;
    if (f->fail_at != 0U && f->count + 1U == f->fail_at) {
        return NP_UHDR_ERR_CONFIG;
    }
    if (f->count >= MAX_CHUNKS || len > NP_UHDR_SEAL_BLOCK_BYTES) {
        return NP_UHDR_ERR_INVALID;
    }
    memcpy(f->chunk[f->count], chunk, len);
    f->len[f->count] = len;
    f->count++;
    return NP_UHDR_OK;
}

static uint8_t g_plain[4000];
static uint8_t g_back[sizeof(g_plain)];

static void fill_plain(void)
{
    for (size_t i = 0; i < sizeof(g_plain); i++) {
        g_plain[i] = (uint8_t)(i * 7U + 3U);
    }
}

/* Two people on one helmet, both profiles open. */
static void setup(np_uhdr_profile_ctx_t *ana, np_uhdr_profile_ctx_t *ben)
{
    uint8_t code[NP_UHDR_RECOVERY_CODE_LEN];
    np_uhdr_host_reset();
    np_uhdr_host_profile_reset();
    (void)np_uhdr_hal_attempts_write(0U);
    (void)np_uhdr_profile_create(0, CRED_ANA, sizeof(CRED_ANA), ana, code);
    (void)np_uhdr_profile_create(1, CRED_BEN, sizeof(CRED_BEN), ben, code);
    fill_plain();
}

static np_uhdr_status_t seal(const np_uhdr_profile_ctx_t *p, const uint8_t *data, size_t len,
                             file_t *f)
{
    np_uhdr_seal_writer_t w;
    memset(f, 0, sizeof(*f));
    np_uhdr_status_t st = np_uhdr_seal_writer_begin(&w, p, &f->header);
    if (st != NP_UHDR_OK) return st;
    /* Feed in uneven pieces so chunk boundaries fall mid-put. */
    size_t off = 0;
    while (off < len) {
        size_t n = (len - off < 333U) ? (len - off) : 333U;
        st = np_uhdr_seal_writer_put(&w, data + off, n, sink, f);
        if (st != NP_UHDR_OK) return st;
        off += n;
    }
    return np_uhdr_seal_writer_finish(&w, sink, f);
}

/* Read the whole file; *out_len gets the plaintext length. */
static np_uhdr_status_t open_file(const np_uhdr_profile_ctx_t *p, const file_t *f,
                                  uint8_t *out, size_t *out_len)
{
    np_uhdr_seal_reader_t r;
    size_t total = 0;
    np_uhdr_status_t st = np_uhdr_seal_reader_begin(&r, p, &f->header);
    if (st != NP_UHDR_OK) return st;
    for (size_t i = 0; i < f->count; i++) {
        size_t n = 0;
        st = np_uhdr_seal_reader_chunk(&r, f->chunk[i], f->len[i], out + total, &n);
        if (st != NP_UHDR_OK) return st;
        total += n;
    }
    *out_len = total;
    return np_uhdr_seal_reader_finish(&r);
}

/* @verifies-none harness self-check: header layout and the derived chunk size */
static void test_sizes(void)
{
    ASSERT(sizeof(np_uhdr_seal_header_t) == NP_UHDR_SEAL_HEADER_SIZE, "header is 32 bytes");
    ASSERT(NP_UHDR_SEAL_CHUNK_PT_BYTES + NP_UHDR_GCM_TAG_LEN == NP_UHDR_SEAL_BLOCK_BYTES,
           "a sealed chunk is exactly one block");
}

/* @verifies REQ-EMMC2-07#a */
static void test_owner_reads_back_exactly(void)
{
    np_uhdr_profile_ctx_t ana, ben;
    file_t f;
    size_t n = 0;
    setup(&ana, &ben);

    ASSERT(seal(&ana, g_plain, sizeof(g_plain), &f) == NP_UHDR_OK, "seal 4000 bytes");
    ASSERT(f.count == (sizeof(g_plain) / NP_UHDR_SEAL_CHUNK_PT_BYTES) + 1U,
           "full chunks then one final chunk");
    for (size_t i = 0; i + 1U < f.count; i++) {
        ASSERT(f.len[i] == NP_UHDR_SEAL_BLOCK_BYTES, "every chunk but the last fills a block");
    }
    ASSERT(f.len[f.count - 1U] < NP_UHDR_SEAL_BLOCK_BYTES, "the final chunk is shorter");
    ASSERT(memcmp(f.chunk[0], g_plain, 64) != 0, "the medium does not hold the plaintext");

    ASSERT(open_file(&ana, &f, g_back, &n) == NP_UHDR_OK, "the owner opens it");
    ASSERT(n == sizeof(g_plain) && memcmp(g_back, g_plain, n) == 0, "byte for byte");

    /* Edge sizes: empty, and an exact multiple of a chunk (empty final chunk). */
    ASSERT(seal(&ana, g_plain, 0, &f) == NP_UHDR_OK && f.count == 1U &&
           f.len[0] == NP_UHDR_GCM_TAG_LEN, "an empty file is one tag-only final chunk");
    ASSERT(open_file(&ana, &f, g_back, &n) == NP_UHDR_OK && n == 0U, "and opens as empty");
    ASSERT(seal(&ana, g_plain, 2U * NP_UHDR_SEAL_CHUNK_PT_BYTES, &f) == NP_UHDR_OK &&
           f.count == 3U && f.len[2] == NP_UHDR_GCM_TAG_LEN,
           "an exact multiple ends with an empty final chunk");
    ASSERT(open_file(&ana, &f, g_back, &n) == NP_UHDR_OK &&
           n == 2U * NP_UHDR_SEAL_CHUNK_PT_BYTES &&
           memcmp(g_back, g_plain, n) == 0, "and opens exactly");
}

/* @verifies REQ-EMMC2-07#a */
static void test_another_profile_opens_nothing(void)
{
    np_uhdr_profile_ctx_t ana, ben;
    file_t f, g;
    size_t n = 0;
    setup(&ana, &ben);
    (void)seal(&ana, g_plain, 1000, &f);

    ASSERT(open_file(&ben, &f, g_back, &n) == NP_UHDR_ERR_AUTH,
           "Ben's profile cannot open Ana's file");

    /* A file whose header is swapped for another file's derives another key. */
    (void)seal(&ana, g_plain, 1000, &g);
    ASSERT(memcmp(f.header.file_id, g.header.file_id, NP_UHDR_FILE_ID_LEN) != 0,
           "each file has its own random id");
    memcpy(&f.header, &g.header, sizeof(f.header));
    ASSERT(open_file(&ana, &f, g_back, &n) == NP_UHDR_ERR_AUTH,
           "chunks under another file's header fail");

    np_uhdr_profile_ctx_t closed = ana;
    np_uhdr_profile_close(&closed);
    np_uhdr_seal_writer_t w;
    np_uhdr_seal_header_t h;
    ASSERT(np_uhdr_seal_writer_begin(&w, &closed, &h) == NP_UHDR_ERR_STATE,
           "nothing is written without an open profile");
    np_uhdr_seal_reader_t r;
    ASSERT(np_uhdr_seal_reader_begin(&r, &closed, &g.header) == NP_UHDR_ERR_STATE,
           "nothing is read without an open profile");
}

/* @verifies REQ-EMMC2-07#b */
static void test_every_chunk_is_bound_to_its_place(void)
{
    np_uhdr_profile_ctx_t ana, ben;
    file_t f, orig;
    size_t n = 0;
    setup(&ana, &ben);
    (void)seal(&ana, g_plain, 2000, &orig);   /* 4 full chunks + final */

    memcpy(&f, &orig, sizeof(f));
    f.chunk[1][10] ^= 0x01U;
    ASSERT(open_file(&ana, &f, g_back, &n) == NP_UHDR_ERR_AUTH, "a flipped byte fails");

    memcpy(&f, &orig, sizeof(f));
    memcpy(f.chunk[1], orig.chunk[2], NP_UHDR_SEAL_BLOCK_BYTES);
    memcpy(f.chunk[2], orig.chunk[1], NP_UHDR_SEAL_BLOCK_BYTES);
    ASSERT(open_file(&ana, &f, g_back, &n) == NP_UHDR_ERR_AUTH, "swapped chunks fail");

    memcpy(&f, &orig, sizeof(f));
    for (size_t i = 1; i + 1U < f.count; i++) {        /* drop chunk 1 */
        memcpy(f.chunk[i], orig.chunk[i + 1U], NP_UHDR_SEAL_BLOCK_BYTES);
        f.len[i] = orig.len[i + 1U];
    }
    f.count--;
    ASSERT(open_file(&ana, &f, g_back, &n) == NP_UHDR_ERR_AUTH, "a dropped chunk fails");

    memcpy(&f, &orig, sizeof(f));
    memcpy(f.chunk[2], orig.chunk[1], NP_UHDR_SEAL_BLOCK_BYTES);
    ASSERT(open_file(&ana, &f, g_back, &n) == NP_UHDR_ERR_AUTH, "a duplicated chunk fails");

    memcpy(&f, &orig, sizeof(f));
    f.header.reserved = 0U;
    f.header.file_id[0] ^= 0x80U;
    ASSERT(open_file(&ana, &f, g_back, &n) == NP_UHDR_ERR_AUTH, "a changed header fails");

    memcpy(&f, &orig, sizeof(f));
    f.header.chunk_pt_bytes = 100U;
    np_uhdr_seal_reader_t r;
    ASSERT(np_uhdr_seal_reader_begin(&r, &ana, &f.header) == NP_UHDR_ERR_INVALID,
           "a malformed header is refused before any key is derived");

    /* Appending after the final chunk is refused. */
    memcpy(&f, &orig, sizeof(f));
    memcpy(f.chunk[f.count], orig.chunk[0], NP_UHDR_SEAL_BLOCK_BYTES);
    f.len[f.count] = NP_UHDR_SEAL_BLOCK_BYTES;
    f.count++;
    ASSERT(open_file(&ana, &f, g_back, &n) == NP_UHDR_ERR_STATE,
           "nothing may follow the final chunk");
}

/* @verifies REQ-EMMC2-07#c */
static void test_truncation_is_refused(void)
{
    np_uhdr_profile_ctx_t ana, ben;
    file_t f;
    size_t n = 0;
    setup(&ana, &ben);
    (void)seal(&ana, g_plain, 2000, &f);

    f.count--;                                    /* cut the final chunk off */
    ASSERT(open_file(&ana, &f, g_back, &n) == NP_UHDR_ERR_AUTH,
           "a file cut after a full chunk is refused (no final chunk)");

    /* A full chunk passed off as final (cut to a short length) fails its tag. */
    (void)seal(&ana, g_plain, 2000, &f);
    f.len[1] = 300U;
    f.count = 2U;
    ASSERT(open_file(&ana, &f, g_back, &n) == NP_UHDR_ERR_AUTH,
           "a shortened full chunk cannot pose as the final one");
}

/* @verifies REQ-EMMC2-07#d */
static void test_name_is_the_random_id_only(void)
{
    np_uhdr_profile_ctx_t ana, ben;
    file_t f, g;
    char name[NP_UHDR_FILE_NAME_LEN + 1U];
    setup(&ana, &ben);
    (void)seal(&ana, g_plain, 10, &f);
    (void)seal(&ben, g_plain, 10, &g);

    np_uhdr_seal_file_name(f.header.file_id, name);
    ASSERT(strlen(name) == NP_UHDR_FILE_NAME_LEN, "32 hex characters");
    char expect[3];
    snprintf(expect, sizeof(expect), "%02x", f.header.file_id[0]);
    ASSERT(name[0] == expect[0] && name[1] == expect[1], "the name is the hex of the id");
    ASSERT(memcmp(f.header.file_id, g.header.file_id, NP_UHDR_FILE_ID_LEN) != 0,
           "two people's files share no name");
    ASSERT(memcmp(f.header.file_id, ana.profile_id, NP_UHDR_FILE_ID_LEN) != 0,
           "the id is not the profile id");
    const uint8_t *hb = (const uint8_t *)&f.header;
    int has_profile = 0;
    for (size_t off = 0; off + NP_UHDR_PROFILE_ID_LEN <= sizeof(f.header); off++) {
        if (memcmp(hb + off, ana.profile_id, NP_UHDR_PROFILE_ID_LEN) == 0) has_profile = 1;
    }
    ASSERT(!has_profile, "the clear header does not carry the owner's profile id");
}

/* @verifies REQ-EMMC2-07#e */
static void test_keys_are_zeroed(void)
{
    np_uhdr_profile_ctx_t ana, ben;
    file_t f;
    np_uhdr_seal_writer_t w;
    np_uhdr_seal_header_t h;
    uint8_t zero[NP_UHDR_FILE_KEY_LEN] = { 0 };
    setup(&ana, &ben);

    (void)np_uhdr_seal_writer_begin(&w, &ana, &h);
    memset(&f, 0, sizeof(f));
    (void)np_uhdr_seal_writer_put(&w, g_plain, 100, sink, &f);
    (void)np_uhdr_seal_writer_finish(&w, sink, &f);
    ASSERT(!w.open && memcmp(w.fk, zero, sizeof(zero)) == 0, "finish zeroes the file key");

    (void)np_uhdr_seal_writer_begin(&w, &ana, &h);
    np_uhdr_seal_writer_abort(&w);
    ASSERT(!w.open && memcmp(w.fk, zero, sizeof(zero)) == 0, "abort zeroes it");

    memset(&f, 0, sizeof(f));
    f.fail_at = 1U;
    (void)np_uhdr_seal_writer_begin(&w, &ana, &h);
    ASSERT(np_uhdr_seal_writer_put(&w, g_plain, 1000, sink, &f) == NP_UHDR_ERR_CONFIG,
           "a sink failure is reported");
    ASSERT(!w.open && memcmp(w.fk, zero, sizeof(zero)) == 0, "and zeroes the key");
    ASSERT(np_uhdr_seal_writer_put(&w, g_plain, 1, sink, &f) == NP_UHDR_ERR_STATE,
           "a failed writer takes nothing more");

    np_uhdr_seal_reader_t r;
    size_t n = 0;
    (void)seal(&ana, g_plain, 600, &f);
    (void)np_uhdr_seal_reader_begin(&r, &ana, &f.header);
    f.chunk[0][0] ^= 1U;
    ASSERT(np_uhdr_seal_reader_chunk(&r, f.chunk[0], f.len[0], g_back, &n) == NP_UHDR_ERR_AUTH,
           "a bad chunk fails");
    ASSERT(!r.open && memcmp(r.fk, zero, sizeof(zero)) == 0, "and zeroes the reader's key");
    ASSERT(np_uhdr_seal_reader_finish(&r) == NP_UHDR_ERR_AUTH, "the reader stays failed");
}

int main(void)
{
    test_sizes();
    test_owner_reads_back_exactly();
    test_another_profile_opens_nothing();
    test_every_chunk_is_bound_to_its_place();
    test_truncation_is_refused();
    test_name_is_the_random_id_only();
    test_keys_are_zeroed();

    if (g_fail_count == 0) {
        printf("PASS: all np_uhdr_seal tests\n");
    } else {
        printf("FAILED: %d assertion(s)\n", g_fail_count);
    }
    return g_fail_count;
}
