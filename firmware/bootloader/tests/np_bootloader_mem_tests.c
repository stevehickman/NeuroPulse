/*
 * NeurOne Bootloader — Freestanding Memory Primitive Host Tests
 * Document: NP-SW-CI-001 Rev 2 §4.2 (Defect B), phase 1
 *
 * The bootloader's first host test target.  Exercises np_memcpy/np_memset —
 * the same source that provides memcpy/memset to the cross build via alias —
 * against the host libc as oracle.
 *
 * Why the oracle is the host's own memcpy/memset: these two functions have a
 * fully specified reference implementation that is already present, already
 * correct, and independent of anything in this repository.  Hand-written
 * expected buffers would be a second implementation with its own bugs.
 *
 * Every copy/fill test runs against a canary-framed buffer, so writing one
 * byte outside the requested range fails the test rather than passing
 * silently.  That matters more here than the copy itself: a wrong length in
 * the OTA bootloader corrupts a firmware image AFTER its Ed25519 signature has
 * been checked, and the device then boots the corrupted image.
 *
 *   T1  n == 0 writes nothing at all (both functions).
 *   T2  lengths 1, 2, 3, 4 and 8 — the short cases GCC would inline.
 *   T3  unaligned source, aligned destination.
 *   T4  aligned source, unaligned destination.
 *   T5  both operands unaligned.
 *   T6  a length that crosses a word boundary from an unaligned start.
 *   T7  every (src offset, dst offset, length) combination in a small matrix.
 *   T8  memset with a non-zero fill byte, including one with the high bit set.
 *   T9  memset truncates its int argument to unsigned char (C99 7.21.6.1).
 *   T10 a large copy and a large fill (8 KiB, comfortably over the 4 KiB
 *       minimum) at every start alignment.
 *   T11 both functions return their dst argument.
 *
 * ── What this suite does NOT prove (measured, not assumed) ─────────────────
 *
 * The suite was falsified against six deliberately broken implementations on
 * 2026-08-09.  Five were killed: off-by-one short copy, one-byte overrun,
 * memset masking its fill value, a do/while that writes on n == 0, and
 * returning the wrong pointer.  Assertion counts 359–1371.
 *
 * One SURVIVED, and it is worth naming rather than burying: a word-at-a-time
 * copy that casts both operands to uint32_t * without checking alignment.  Its
 * OUTPUT is byte-identical to a correct copy on any host that tolerates
 * unaligned access, so no output-comparison test can distinguish it — the
 * defect is undefined behaviour and a strict-alignment fault, not a wrong
 * byte.  The alignment obligation in np_mem.h is therefore verified at the
 * artifact level instead, by asserting the disassembled bodies contain only
 * byte-wise accesses:
 *
 *     arm-none-eabi-objdump -d np_bootloader.elf | sed -n '/<memcpy>:/,/^$/p'
 *
 * Measured 2026-08-09: memcpy emits exactly one ldrb.w and one strb.w, memset
 * exactly one strb.w, and neither body contains a bl/blx or any branch to a
 * function entry.  A UBSan (-fsanitize=alignment) host build would also catch
 * it; that is a broader CI change than this phase, not a substitute for the
 * disassembly check, which is the one that inspects what actually ships.
 *
 * Return convention: 0 = PASS, non-zero = failure count.
 */

#include "np_mem.h"

#include <stdio.h>
#include <string.h>

static int g_fail_count = 0;

#define ASSERT(cond, msg)                                            \
    do {                                                             \
        if (!(cond)) {                                              \
            printf("FAIL [%s:%d] %s\n", __func__, __LINE__, (msg)); \
            g_fail_count++;                                         \
        }                                                          \
    } while (0)

/*
 * Canary-framed scratch buffers.  PAD bytes on each side of the live region
 * are filled with CANARY and checked after every operation.
 */
#define BUF_LEN   8192U
#define PAD       16U
#define CANARY    0x5AU

static unsigned char g_dst[PAD + BUF_LEN + PAD];
static unsigned char g_ref[PAD + BUF_LEN + PAD];
static unsigned char g_src[BUF_LEN + 8U];

static void fill_source(void)
{
    for (size_t i = 0; i < sizeof(g_src); i++) {
        /* Non-repeating over any window shorter than 251 bytes. */
        g_src[i] = (unsigned char)((i * 251U + 17U) & 0xFFU);
    }
}

static void frame_reset(void)
{
    memset(g_dst, CANARY, sizeof(g_dst));
    memset(g_ref, CANARY, sizeof(g_ref));
}

static int canaries_intact(const unsigned char *buf)
{
    for (size_t i = 0; i < PAD; i++) {
        if (buf[i] != CANARY) return 0;
        if (buf[PAD + BUF_LEN + i] != CANARY) return 0;
    }
    return 1;
}

/*
 * One copy case: copy len bytes from g_src + src_off to (live region) +
 * dst_off using np_memcpy, and the same using libc memcpy into the reference
 * frame.  The two frames must be byte-identical over their whole extent,
 * which checks the copied range, the untouched surroundings and the canaries
 * in a single comparison.
 */
static void check_copy(size_t src_off, size_t dst_off, size_t len)
{
    char msg[96];

    frame_reset();
    void *ret     = np_memcpy(g_dst + PAD + dst_off, g_src + src_off, len);
    void *ref_ret = memcpy(g_ref + PAD + dst_off, g_src + src_off, len);

    snprintf(msg, sizeof(msg), "copy src_off=%zu dst_off=%zu len=%zu", src_off, dst_off, len);

    ASSERT(memcmp(g_dst, g_ref, sizeof(g_dst)) == 0, msg);
    ASSERT(canaries_intact(g_dst), msg);
    ASSERT(ret == (void *)(g_dst + PAD + dst_off), msg);
    ASSERT(ref_ret == (void *)(g_ref + PAD + dst_off), msg);
}

static void check_fill(size_t dst_off, int c, size_t len)
{
    char msg[96];

    frame_reset();
    void *ret     = np_memset(g_dst + PAD + dst_off, c, len);
    void *ref_ret = memset(g_ref + PAD + dst_off, c, len);

    snprintf(msg, sizeof(msg), "fill dst_off=%zu c=0x%x len=%zu", dst_off, (unsigned)c, len);

    ASSERT(memcmp(g_dst, g_ref, sizeof(g_dst)) == 0, msg);
    ASSERT(canaries_intact(g_dst), msg);
    ASSERT(ret == (void *)(g_dst + PAD + dst_off), msg);
    ASSERT(ref_ret == (void *)(g_ref + PAD + dst_off), msg);
}

/* T1 — zero length must touch nothing, at every alignment. */
static void test_zero_length(void)
{
    for (size_t off = 0; off < 8U; off++) {
        check_copy(off, off, 0U);
        check_fill(off, 0xA5, 0U);
    }

    /*
     * Explicit, independent of the frame comparison: a zero-length call to a
     * buffer that is entirely canary must leave it entirely canary.
     */
    frame_reset();
    (void)np_memcpy(g_dst + PAD, g_src, 0U);
    (void)np_memset(g_dst + PAD, 0x00, 0U);
    for (size_t i = 0; i < sizeof(g_dst); i++) {
        ASSERT(g_dst[i] == CANARY, "zero-length call wrote a byte");
    }
}

/* T2 — the short lengths a compiler would ordinarily inline. */
static void test_short_lengths(void)
{
    static const size_t lens[] = { 1U, 2U, 3U, 4U, 8U };

    for (size_t i = 0; i < sizeof(lens) / sizeof(lens[0]); i++) {
        check_copy(0U, 0U, lens[i]);
        check_fill(0U, 0x3C, lens[i]);
    }
}

/* T3/T4/T5 — unaligned source, unaligned destination, and both. */
static void test_unaligned_operands(void)
{
    /* Live regions start at PAD == 16, so offset 0 is 4/8-byte aligned. */
    check_copy(1U, 0U, 32U);    /* T3: src unaligned, dst aligned            */
    check_copy(3U, 0U, 32U);
    check_copy(0U, 1U, 32U);    /* T4: src aligned,   dst unaligned          */
    check_copy(0U, 3U, 32U);
    check_copy(1U, 1U, 32U);    /* T5: both unaligned, same phase            */
    check_copy(1U, 2U, 32U);    /* T5: both unaligned, different phase       */
    check_copy(3U, 5U, 32U);
}

/* T6 — a length crossing a word boundary from an unaligned start. */
static void test_word_boundary_crossing(void)
{
    /* dst_off 1 with len 3 spans bytes 1..3 and stops exactly at the word
     * boundary; len 4..7 step across it; len 7 from offset 1 spans two
     * boundaries. */
    for (size_t len = 3U; len <= 9U; len++) {
        check_copy(1U, 1U, len);
        check_fill(1U, 0x77, len);
    }
    /* Same, one byte before an 8-byte boundary. */
    check_copy(7U, 7U, 5U);
    check_fill(7U, 0x77, 5U);
}

/* T7 — small exhaustive matrix over offsets and lengths. */
static void test_offset_matrix(void)
{
    for (size_t src_off = 0; src_off < 8U; src_off++) {
        for (size_t dst_off = 0; dst_off < 8U; dst_off++) {
            for (size_t len = 0; len <= 17U; len++) {
                check_copy(src_off, dst_off, len);
            }
        }
    }

    for (size_t dst_off = 0; dst_off < 8U; dst_off++) {
        for (size_t len = 0; len <= 17U; len++) {
            check_fill(dst_off, 0x9B, len);
        }
    }
}

/* T8 — non-zero fill bytes, including one with the high bit set. */
static void test_nonzero_fill(void)
{
    static const int fills[] = { 0x01, 0x42, 0x7F, 0x80, 0xAA, 0xFF };

    for (size_t i = 0; i < sizeof(fills) / sizeof(fills[0]); i++) {
        check_fill(0U, fills[i], 64U);
        check_fill(3U, fills[i], 33U);

        frame_reset();
        (void)np_memset(g_dst + PAD, fills[i], 64U);
        for (size_t j = 0; j < 64U; j++) {
            ASSERT(g_dst[PAD + j] == (unsigned char)fills[i], "fill byte wrong");
        }
    }
}

/* T9 — the int argument is truncated to unsigned char, not stored as an int. */
static void test_fill_value_truncation(void)
{
    /* 0x1234 truncates to 0x34; -1 truncates to 0xFF. */
    check_fill(0U, 0x1234, 16U);
    check_fill(0U, -1, 16U);
    check_fill(1U, 0x0100, 16U);   /* truncates to 0x00 */

    frame_reset();
    (void)np_memset(g_dst + PAD, 0x1234, 16U);
    for (size_t i = 0; i < 16U; i++) {
        ASSERT(g_dst[PAD + i] == 0x34U, "memset did not truncate to unsigned char");
    }

    frame_reset();
    (void)np_memset(g_dst + PAD, -1, 16U);
    for (size_t i = 0; i < 16U; i++) {
        ASSERT(g_dst[PAD + i] == 0xFFU, "memset of -1 did not produce 0xFF");
    }
}

/* T10 — large copy and large fill, at every start alignment. */
static void test_large_transfers(void)
{
    for (size_t off = 0; off < 8U; off++) {
        check_copy(off, off, BUF_LEN - 8U);       /* 8184 bytes  */
        check_copy(off, (off + 3U) % 8U, 4096U);  /* exactly 4 KiB, mixed phase */
        check_fill(off, 0xC3, BUF_LEN - 8U);
    }
}

/* T11 — both functions return dst.  (check_copy/check_fill assert this on
 * every case; this is the explicit, standalone statement of the contract.) */
static void test_return_values(void)
{
    frame_reset();
    ASSERT(np_memcpy(g_dst + PAD, g_src, 16U) == (void *)(g_dst + PAD),
           "np_memcpy did not return dst");
    ASSERT(np_memset(g_dst + PAD, 0x11, 16U) == (void *)(g_dst + PAD),
           "np_memset did not return dst");
    ASSERT(np_memcpy(g_dst + PAD, g_src, 0U) == (void *)(g_dst + PAD),
           "np_memcpy(n=0) did not return dst");
    ASSERT(np_memset(g_dst + PAD, 0x11, 0U) == (void *)(g_dst + PAD),
           "np_memset(n=0) did not return dst");
}

int main(void)
{
    fill_source();

    test_zero_length();
    test_short_lengths();
    test_unaligned_operands();
    test_word_boundary_crossing();
    test_offset_matrix();
    test_nonzero_fill();
    test_fill_value_truncation();
    test_large_transfers();
    test_return_values();

    if (g_fail_count == 0) {
        printf("PASS: all np_bootloader_mem tests\n");
    } else {
        printf("FAILED: %d assertion(s)\n", g_fail_count);
    }
    return g_fail_count;
}
