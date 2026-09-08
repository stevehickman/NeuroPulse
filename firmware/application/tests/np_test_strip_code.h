/*
 * NeurOne SW-02 — strip a C source to what the compiler would see
 * Document: NP-SW-CI-001 §4.11 (where it was written), §4.14 (where it moved)
 *
 * ── Why this exists, and why it is shared ───────────────────────────────────
 *
 * Two host suites in this directory probe np_app_main.c for the checks the
 * record says the boot path performs.  A probe of raw text does not work, and
 * that is not a hypothetical: the first version of the clock suite's boot-path
 * case PASSED against a main() with the check deleted, because np_app_main.c
 * explains every check at length in its header comment and each token the probe
 * searched for was sitting in the prose.  It was caught by mutating the file it
 * reads.  So a probe has to strip what the compiler would strip before looking.
 *
 * String and character literal BODIES go too, not just comments: a string is as
 * good a hiding place for a token as a comment, and np_app_main.c has a long
 * build-note string sitting in the middle of its code.
 *
 * It is a header rather than a copy in each suite because the alternative was
 * two copies of forty lines of state machine that must behave identically for
 * either suite's result to mean anything — which is the drift this document
 * spends §4.3 and §4.8.3 arguing against wherever it can be avoided.  Here it
 * can: both consumers are single-file targets in one directory, so a quoted
 * include needs no build wiring at all.
 *
 * ── What it does not do ─────────────────────────────────────────────────────
 *
 * It is not a preprocessor.  It does not evaluate #if, expand macros, or splice
 * continuation lines, and it does not attempt trigraphs or raw strings.  It
 * removes comments and literal bodies, replacing every removed byte with a
 * space (never deleting it) so nothing on either side of a comment is joined
 * into one token.  That is exactly enough for "does this identifier appear in
 * code, and in what order", which is all either caller asks.
 */

#ifndef NP_TEST_STRIP_CODE_H
#define NP_TEST_STRIP_CODE_H

#include <stddef.h>

/* `out` must have room for n + 1 bytes. */
static void strip_to_code(const char *in, size_t n, char *out)
{
    enum { CODE, BLOCK, LINE, STR, CHR } st = CODE;
    size_t o = 0U;

    for (size_t i = 0U; i < n; i++) {
        char c = in[i];
        char d = (i + 1U < n) ? in[i + 1U] : '\0';

        switch (st) {
        case CODE:
            if (c == '/' && d == '*') { st = BLOCK; out[o++] = ' '; out[o++] = ' '; i++; continue; }
            if (c == '/' && d == '/') { st = LINE;  out[o++] = ' '; out[o++] = ' '; i++; continue; }
            if (c == '"')  { st = STR; out[o++] = ' '; continue; }
            if (c == '\'') { st = CHR; out[o++] = ' '; continue; }
            out[o++] = c;
            continue;

        case BLOCK:
            if (c == '*' && d == '/') { st = CODE; out[o++] = ' '; out[o++] = ' '; i++; continue; }
            out[o++] = (c == '\n') ? '\n' : ' ';
            continue;

        case LINE:
            if (c == '\n') { st = CODE; out[o++] = '\n'; continue; }
            out[o++] = ' ';
            continue;

        case STR:
        case CHR:
            /* A backslash escapes the next byte, including the closing quote. */
            if (c == '\\' && i + 1U < n) { out[o++] = ' '; out[o++] = ' '; i++; continue; }
            if ((st == STR && c == '"') || (st == CHR && c == '\'')) { st = CODE; }
            out[o++] = (c == '\n') ? '\n' : ' ';
            continue;
        }
    }
    out[o] = '\0';
}

#endif /* NP_TEST_STRIP_CODE_H */
