/*
 * NeurOne Bootloader — Application Staging Limit Host Tests
 * Document: NP-SW-CI-001 Rev D §4.3 (Defect C), phase 2
 *
 * Defect C was one wrong number written twice.  The linker script reserved
 * 448 KiB for application staging starting at the 64 KiB boundary — "512 - 64
 * = 448", omitting the 8 KiB stack at the top of OCRAM — and np_main.c derived
 * the same 448 KiB independently from NP_OCRAM_SIZE.  In the linker script that
 * was a link failure.  In C it was silent: load_and_jump() accepted an image up
 * to 448 KiB and copied it to the staging base, so anything in the 440–448 KiB
 * band overwrote the stack the bootloader was executing on, after Ed25519
 * verification had passed.
 *
 * This suite exists because fixing the link error alone leaves the C guard
 * untested.  It checks two things:
 *
 *   A. The predicate.  np_app_image_size_check() accepts exactly the limit,
 *      rejects limit + 1, rejects an empty image, and — the regression test for
 *      the defect itself — rejects every size in the old 440–448 KiB band.
 *
 *   B. Agreement.  The limit the predicate is checked against is the one the
 *      linker script actually reserves, read out of the script itself.  If the
 *      two ever diverge again, this suite fails.  That is the assertion that
 *      would have caught Defect C.
 *
 * ── Why this parses the linker script instead of grepping it ─────────────────
 *
 * A text probe over this repository's linker script would match the task's own
 * output: the comment block above _app_load_offset explains the defect and
 * therefore contains both "448" and "440", and the derivation comment contains
 * the string "_stack_size) = 440K".  A grep for `_stack_size *= *([0-9]+)K`
 * finds that comment.  So the script is comment-stripped first and the
 * assignments are read from the stripped text — the same discipline as parsing
 * a config file rather than regexing it.
 *
 * ── What this suite does NOT prove ───────────────────────────────────────────
 *
 * That the SHIPPING np_app_max_image_size() returns the linker's value.  It
 * cannot: a host build has no linker script, so np_app_image.c's symbol-reading
 * path is #ifdef'd out here (NP_APP_IMAGE_HOST_TEST) rather than fed a
 * stand-in — a stand-in would let this suite pass while the device binding was
 * broken, which is the failure mode it exists to prevent.  That binding is a
 * property of the linked artifact, not of any value a host can compute, and it
 * is verified there:
 *
 *     arm-none-eabi-nm np_bootloader | grep _app_staging
 *     arm-none-eabi-objdump -d np_bootloader | sed -n '/<np_app_max_image_size>:/,/^$/p'
 *
 * Measured 2026-08-09: _app_staging_size = 0x0006e000 (440 KiB),
 * _app_staging_end = _stack_base = 0x2027e000, and the compiled body of
 * np_app_max_image_size() loads the literal word 0x0006e000 placed by the
 * linker.  Same shape of compensating control as the §4.2 disassembly check.
 *
 * Return convention: 0 = PASS, non-zero = failure count.
 */

#include "np_app_image.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef NP_LINKER_SCRIPT_PATH
#error "NP_LINKER_SCRIPT_PATH must be defined by the build (path to bootloader_imxrt1062.ld)"
#endif

static int g_fail_count = 0;

#define ASSERT(cond, msg)                                            \
    do {                                                             \
        if (!(cond)) {                                              \
            printf("FAIL [%s:%d] %s\n", __func__, __LINE__, (msg)); \
            g_fail_count++;                                         \
        }                                                          \
    } while (0)

/* ── Linker script reader ─────────────────────────────────────────────────── */

#define LD_MAX_BYTES  (256U * 1024U)

static char g_ld_raw[LD_MAX_BYTES];
static char g_ld[LD_MAX_BYTES];   /* comment-stripped */

/* Read the script, then strip every C-style block comment.  GNU ld scripts have
 * no line-comment form, so that is the whole grammar to handle.  Comment bytes
 * become spaces rather than being deleted, so nothing on either side of a
 * comment is accidentally joined into one token. */
static int load_linker_script(void)
{
    FILE *f = fopen(NP_LINKER_SCRIPT_PATH, "rb");
    if (f == NULL) {
        printf("FAIL [%s] cannot open %s\n", __func__, NP_LINKER_SCRIPT_PATH);
        g_fail_count++;
        return -1;
    }

    size_t n = fread(g_ld_raw, 1U, sizeof(g_ld_raw) - 1U, f);
    int truncated = (feof(f) == 0);
    fclose(f);
    g_ld_raw[n] = '\0';

    if (truncated) {
        printf("FAIL [%s] linker script larger than %u bytes — parser would "
               "read a prefix and could miss an assignment\n",
               __func__, (unsigned)sizeof(g_ld_raw));
        g_fail_count++;
        return -1;
    }

    int in_comment = 0;
    size_t out = 0U;
    for (size_t i = 0U; i < n; i++) {
        if (!in_comment && g_ld_raw[i] == '/' && i + 1U < n && g_ld_raw[i + 1U] == '*') {
            in_comment = 1;
            g_ld[out++] = ' ';
            g_ld[out++] = ' ';
            i++;
            continue;
        }
        if (in_comment && g_ld_raw[i] == '*' && i + 1U < n && g_ld_raw[i + 1U] == '/') {
            in_comment = 0;
            g_ld[out++] = ' ';
            g_ld[out++] = ' ';
            i++;
            continue;
        }
        /* Keep newlines so failures can still be reasoned about by eye. */
        g_ld[out++] = in_comment ? ((g_ld_raw[i] == '\n') ? '\n' : ' ') : g_ld_raw[i];
    }
    g_ld[out] = '\0';

    ASSERT(in_comment == 0, "unterminated block comment in linker script");
    return 0;
}

/* Parse an ld size literal: digits with an optional K/M suffix. */
static int parse_ld_number(const char *p, uint64_t *out, const char **end)
{
    if (*p < '0' || *p > '9') return -1;

    uint64_t v = 0U;
    while (*p >= '0' && *p <= '9') {
        v = (v * 10U) + (uint64_t)(*p - '0');
        p++;
    }
    if (*p == 'K' || *p == 'k') { v *= 1024U;               p++; }
    else if (*p == 'M' || *p == 'm') { v *= 1024U * 1024U;  p++; }

    *out = v;
    if (end != NULL) *end = p;
    return 0;
}

static const char *skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

/*
 * Find `<name>` used as an assignment target — i.e. followed (after whitespace)
 * by a single '='.  Returns a pointer just past the '=', or NULL.  Requires a
 * non-identifier character before the name so `_stack_size` does not match
 * inside a longer identifier.
 */
static const char *find_assignment(const char *name)
{
    size_t nlen = strlen(name);
    const char *p = g_ld;

    while ((p = strstr(p, name)) != NULL) {
        int boundary_ok = (p == g_ld);
        if (!boundary_ok) {
            char c = p[-1];
            boundary_ok = !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                            (c >= '0' && c <= '9') || c == '_');
        }
        const char *q = skip_ws(p + nlen);
        if (boundary_ok && *q == '=' && q[1] != '=') {
            return q + 1;
        }
        p += nlen;
    }
    return NULL;
}

/* Read `<name> = <number>;` out of the stripped script. */
static int ld_symbol_value(const char *name, uint64_t *out)
{
    const char *rhs = find_assignment(name);
    if (rhs == NULL) return -1;
    return parse_ld_number(skip_ws(rhs), out, NULL);
}

/* Read the RHS of `<name> = ... ;` verbatim (stripped text). */
static int ld_symbol_expr(const char *name, char *buf, size_t buflen)
{
    const char *rhs = find_assignment(name);
    if (rhs == NULL) return -1;

    rhs = skip_ws(rhs);
    const char *semi = strchr(rhs, ';');
    if (semi == NULL) return -1;

    size_t len = (size_t)(semi - rhs);
    if (len >= buflen) len = buflen - 1U;
    memcpy(buf, rhs, len);
    buf[len] = '\0';
    return 0;
}

/* ── The reservation, as the linker script computes it ────────────────────── */

static uint64_t g_ocram_length    = 0U;
static uint64_t g_app_load_offset = 0U;
static uint64_t g_stack_size      = 0U;
static uint64_t g_staging_size    = 0U;

/*
 * T1 — the script still declares the three inputs, and staging is derived from
 * them rather than written as a literal.
 */
static void test_linker_script_shape(void)
{
    /* OCRAM LENGTH lives inside the MEMORY block: `... LENGTH = 512K`. */
    ASSERT(ld_symbol_value("LENGTH", &g_ocram_length) == 0,
           "could not read OCRAM LENGTH from linker script");
    ASSERT(ld_symbol_value("_app_load_offset", &g_app_load_offset) == 0,
           "could not read _app_load_offset from linker script");
    ASSERT(ld_symbol_value("_stack_size", &g_stack_size) == 0,
           "could not read _stack_size from linker script");

    ASSERT(g_ocram_length == 512U * 1024U,   "OCRAM LENGTH is not 512 KiB");
    ASSERT(g_app_load_offset == 64U * 1024U, "_app_load_offset is not 64 KiB");
    ASSERT(g_stack_size == 8U * 1024U,       "_stack_size is not 8 KiB");

    /* _app_staging_size must be an EXPRESSION over those three, not a number.
     * A literal is what Defect C was: correct on the day it was written and
     * unable to follow the thing it describes. */
    char expr[512];
    ASSERT(ld_symbol_expr("_app_staging_size", expr, sizeof(expr)) == 0,
           "could not read the _app_staging_size assignment");
    ASSERT(strstr(expr, "LENGTH") != NULL,
           "_app_staging_size does not reference LENGTH(OCRAM)");
    ASSERT(strstr(expr, "_app_load_offset") != NULL,
           "_app_staging_size does not reference _app_load_offset");
    ASSERT(strstr(expr, "_stack_size") != NULL,
           "_app_staging_size does not reference _stack_size");

    int has_digit = 0;
    for (const char *p = expr; *p != '\0'; p++) {
        if (*p >= '0' && *p <= '9') has_digit = 1;
    }
    ASSERT(has_digit == 0,
           "_app_staging_size contains a numeric literal — it must be derived");

    /* And the section body must consume the symbol, not its own arithmetic. */
    ASSERT(strstr(g_ld, ". += _app_staging_size;") != NULL,
           ".app_staging does not reserve _app_staging_size");

    g_staging_size = g_ocram_length - g_app_load_offset - g_stack_size;
}

/*
 * T2 — the derived reservation is 440 KiB, and the three regions tile OCRAM
 * exactly.  This is the arithmetic Defect C got wrong.
 */
static void test_reservation_value(void)
{
    ASSERT(g_staging_size == 440U * 1024U,
           "linker reservation is not 440 KiB");
    ASSERT(g_app_load_offset + g_staging_size + g_stack_size == g_ocram_length,
           "app_load_offset + staging + stack does not equal OCRAM LENGTH");

    /* The old value, stated as what it was: one stack short. */
    ASSERT(g_staging_size + g_stack_size == 448U * 1024U,
           "reservation + stack is not the old 448 KiB figure — check the premise");
}

/*
 * T3 — the boundary.  Exactly the limit is accepted; one byte more is not.
 */
static void test_boundary(void)
{
    const uint32_t limit = (uint32_t)g_staging_size;

    ASSERT(np_app_image_size_check(limit, limit) == NP_OK,
           "an image of exactly the staging size was rejected");
    ASSERT(np_app_image_size_check(limit - 1U, limit) == NP_OK,
           "an image one byte under the limit was rejected");
    ASSERT(np_app_image_size_check(limit + 1U, limit) == NP_ERR_IMAGE_TOO_LARGE,
           "an image one byte over the limit was accepted");
    ASSERT(np_app_image_size_check(1U, limit) == NP_OK,
           "a one-byte image was rejected");
    ASSERT(np_app_image_size_check(UINT32_MAX, limit) == NP_ERR_IMAGE_TOO_LARGE,
           "UINT32_MAX was accepted");
}

/*
 * T4 — an empty image is rejected.  It has no vector table, so there is no
 * stack pointer or entry point to jump to.
 */
static void test_empty_image(void)
{
    const uint32_t limit = (uint32_t)g_staging_size;

    ASSERT(np_app_image_size_check(0U, limit) == NP_ERR_IMAGE_TOO_LARGE,
           "a zero-length image was accepted");
    ASSERT(np_app_image_size_check(0U, 0U) == NP_ERR_IMAGE_TOO_LARGE,
           "a zero-length image was accepted against a zero-size area");
}

/*
 * T5 — the regression test for Defect C itself.  The old guard's limit was
 * 448 KiB, so every size in (440 KiB, 448 KiB] was accepted and copied over
 * the 8 KiB stack.  Every one of them must now be rejected.  Stepped at 512
 * bytes (the eMMC sector size, so each step is a real image size) plus both
 * exact endpoints.
 */
static void test_stack_overwrite_band_rejected(void)
{
    const uint32_t limit = (uint32_t)g_staging_size;      /* 440 KiB */
    const uint32_t old   = (uint32_t)(g_staging_size + g_stack_size); /* 448 KiB */

    ASSERT(np_app_image_size_check(limit + 1U, limit) == NP_ERR_IMAGE_TOO_LARGE,
           "440 KiB + 1 accepted — the stack-overwrite band is still open");
    ASSERT(np_app_image_size_check(old, limit) == NP_ERR_IMAGE_TOO_LARGE,
           "the old 448 KiB limit is still accepted");

    for (uint32_t sz = limit + 512U; sz <= old; sz += 512U) {
        if (np_app_image_size_check(sz, limit) != NP_ERR_IMAGE_TOO_LARGE) {
            printf("FAIL [%s] image of %u bytes accepted — overwrites the stack\n",
                   __func__, sz);
            g_fail_count++;
        }
    }
}

/*
 * T6 — the predicate is pure: same arguments, same answer, and the limit is
 * never read from anywhere but the argument.  Checked by driving it with a
 * limit that has nothing to do with this platform.
 */
static void test_predicate_is_pure(void)
{
    ASSERT(np_app_image_size_check(100U, 100U) == NP_OK,
           "arbitrary limit: exact fit rejected");
    ASSERT(np_app_image_size_check(101U, 100U) == NP_ERR_IMAGE_TOO_LARGE,
           "arbitrary limit: over-limit accepted");
    ASSERT(np_app_image_size_check(100U, 100U) == np_app_image_size_check(100U, 100U),
           "np_app_image_size_check is not deterministic");
}

int main(void)
{
    if (load_linker_script() != 0) {
        printf("FAILED: %d assertion(s)\n", g_fail_count);
        return g_fail_count == 0 ? 1 : g_fail_count;
    }

    test_linker_script_shape();
    test_reservation_value();
    test_boundary();
    test_empty_image();
    test_stack_overwrite_band_rejected();
    test_predicate_is_pure();

    if (g_fail_count == 0) {
        printf("PASS: all np_bootloader_app_image tests "
               "(staging %llu KiB = %llu - %llu - %llu)\n",
               (unsigned long long)(g_staging_size / 1024U),
               (unsigned long long)(g_ocram_length / 1024U),
               (unsigned long long)(g_app_load_offset / 1024U),
               (unsigned long long)(g_stack_size / 1024U));
    } else {
        printf("FAILED: %d assertion(s)\n", g_fail_count);
    }
    return g_fail_count;
}
