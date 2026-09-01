/*
 * NeurOne SW-02 — Linker Script Agreement Host Tests
 * Document: NP-SW-CI-001 §4.8 (phase 8, closes OI-SWCI-21)
 *
 * ── What this suite is for ───────────────────────────────────────────────────
 *
 * The bootloader reserves an application staging area in OCRAM and the
 * application image must be linked to sit exactly inside it.  Two files own
 * that one fact:
 *
 *   firmware/bootloader/linker/bootloader_imxrt1062.ld
 *       OCRAM ORIGIN/LENGTH, _app_load_offset, _stack_size,
 *       _app_staging_size = LENGTH(OCRAM) - _app_load_offset - _stack_size
 *
 *   firmware/application/linker/app_imxrt1062.ld
 *       NP_OCRAM_ORIGIN, NP_APP_LOAD_OFFSET, NP_APP_IMAGE_LENGTH
 *
 * A GNU ld script cannot include another one, so the second file has no way to
 * derive its numbers from the first.  The duplication is unavoidable; what is
 * avoidable is it being silent.
 *
 * That is not a hypothetical.  Defect C (NP-SW-CI-001 §4.3) was this exact
 * shape — the staging size written once in the linker script and once in C —
 * and the C copy omitted the 8 KiB stack, so the bootloader accepted images
 * that overwrote the stack it was running on, after signature verification had
 * passed.  The lesson recorded there was not "never duplicate", which is not
 * always on offer.  It was: if you must duplicate, make something fail.
 *
 * ── What a disagreement would actually do ───────────────────────────────────
 *
 * Nothing at build time, which is the point.  Both scripts link cleanly on
 * their own.  The failure is at boot: np_app_image_size_check() would reject a
 * valid image (too small a reservation), or load_and_jump() would copy an image
 * over the bootloader's own stack (too large a one), or the application would
 * be linked for an address the bootloader never stages it at and would fault on
 * its first absolute reference.  None of those is visible in either file alone.
 *
 * ── What it does NOT prove ──────────────────────────────────────────────────
 *
 * That the linked np_application.elf actually lands where the script says.  A
 * host has no linker script and no ARM linker.  That is a property of the
 * artifact and is asserted by the script itself, at link time
 * (`ASSERT(__isr_vector_start == NP_APP_IMAGE_ORIGIN, ...)`), and is checkable
 * on the artifact:
 *
 *     arm-none-eabi-nm build/cross/application/np_application.elf | grep __isr_vector
 *
 * Same division of labour, and the same reasoning, as
 * np_bootloader_app_image_tests.
 *
 * Return convention: 0 = PASS, non-zero = failure count.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef NP_BOOTLOADER_LD_PATH
#error "NP_BOOTLOADER_LD_PATH must be defined by the build"
#endif
#ifndef NP_APPLICATION_LD_PATH
#error "NP_APPLICATION_LD_PATH must be defined by the build"
#endif

static int g_fail_count = 0;

#define ASSERT(cond, msg)                                            \
    do {                                                             \
        if (!(cond)) {                                               \
            printf("FAIL [%s:%d] %s\n", __func__, __LINE__, (msg));  \
            g_fail_count++;                                          \
        }                                                            \
    } while (0)

/* ── Linker script reader ─────────────────────────────────────────────────────
 *
 * Comment-stripped before parsing, for the reason np_bootloader_app_image_tests
 * records: both of these scripts EXPLAIN the arithmetic in their comments, so a
 * bare text probe would happily match the prose describing a number rather than
 * the assignment that sets it.  Comment bytes become spaces so nothing on
 * either side is joined into one token.
 */

#define LD_MAX_BYTES  (256U * 1024U)

typedef struct {
    char        text[LD_MAX_BYTES];   /* comment-stripped */
    const char *path;
} ld_script_t;

static ld_script_t g_boot;
static ld_script_t g_app;

static int ld_load(ld_script_t *s, const char *path)
{
    static char raw[LD_MAX_BYTES];

    s->path = path;

    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        printf("FAIL [%s] cannot open %s\n", __func__, path);
        g_fail_count++;
        return -1;
    }

    size_t n = fread(raw, 1U, sizeof(raw) - 1U, f);
    int truncated = (feof(f) == 0);
    fclose(f);
    raw[n] = '\0';

    if (truncated) {
        printf("FAIL [%s] %s larger than %u bytes — the parser would read a "
               "prefix and could miss an assignment\n",
               __func__, path, (unsigned)sizeof(raw));
        g_fail_count++;
        return -1;
    }

    int    in_comment = 0;
    size_t out = 0U;
    for (size_t i = 0U; i < n; i++) {
        if (!in_comment && raw[i] == '/' && i + 1U < n && raw[i + 1U] == '*') {
            in_comment = 1;
            s->text[out++] = ' ';
            s->text[out++] = ' ';
            i++;
            continue;
        }
        if (in_comment && raw[i] == '*' && i + 1U < n && raw[i + 1U] == '/') {
            in_comment = 0;
            s->text[out++] = ' ';
            s->text[out++] = ' ';
            i++;
            continue;
        }
        s->text[out++] = in_comment ? ((raw[i] == '\n') ? '\n' : ' ') : raw[i];
    }
    s->text[out] = '\0';

    if (in_comment != 0) {
        printf("FAIL [%s] unterminated block comment in %s\n", __func__, path);
        g_fail_count++;
        return -1;
    }
    return 0;
}

static const char *skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

static int is_ident_char(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '_';
}

/* Parse an ld numeric literal: 0x-hex, or decimal with an optional K/M suffix. */
static int parse_ld_number(const char *p, uint64_t *out, const char **end)
{
    uint64_t v = 0U;

    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') ||
              (*p >= 'A' && *p <= 'F'))) {
            return -1;
        }
        while (1) {
            char c = *p;
            uint64_t d;
            if      (c >= '0' && c <= '9') d = (uint64_t)(c - '0');
            else if (c >= 'a' && c <= 'f') d = (uint64_t)(c - 'a') + 10U;
            else if (c >= 'A' && c <= 'F') d = (uint64_t)(c - 'A') + 10U;
            else break;
            v = (v * 16U) + d;
            p++;
        }
    } else if (*p >= '0' && *p <= '9') {
        while (*p >= '0' && *p <= '9') {
            v = (v * 10U) + (uint64_t)(*p - '0');
            p++;
        }
        if      (*p == 'K' || *p == 'k') { v *= 1024U;          p++; }
        else if (*p == 'M' || *p == 'm') { v *= 1024U * 1024U;  p++; }
    } else {
        return -1;
    }

    *out = v;
    if (end != NULL) *end = p;
    return 0;
}

/*
 * Find `<name>` used as an assignment target — the name, then whitespace, then
 * a single '='.  A non-identifier character is required before the name so
 * `_stack_size` cannot match inside a longer identifier and `NP_OCRAM_ORIGIN`
 * cannot be found by searching for `ORIGIN`.  Returns just past the '='.
 */
static const char *find_assignment(const ld_script_t *s, const char *name)
{
    size_t      nlen = strlen(name);
    const char *p    = s->text;

    while ((p = strstr(p, name)) != NULL) {
        int boundary_ok = (p == s->text) || !is_ident_char(p[-1]);
        const char *q = skip_ws(p + nlen);
        if (boundary_ok && !is_ident_char(*(p + nlen)) &&
            *q == '=' && q[1] != '=') {
            return q + 1;
        }
        p += nlen;
    }
    return NULL;
}

static int ld_value(const ld_script_t *s, const char *name, uint64_t *out)
{
    const char *rhs = find_assignment(s, name);
    if (rhs == NULL) {
        printf("FAIL [%s] no assignment to %s in %s\n", __func__, name, s->path);
        g_fail_count++;
        return -1;
    }
    if (parse_ld_number(skip_ws(rhs), out, NULL) != 0) {
        printf("FAIL [%s] %s in %s is not a numeric literal — this parser "
               "deliberately does not evaluate expressions\n",
               __func__, name, s->path);
        g_fail_count++;
        return -1;
    }
    return 0;
}

/*
 * Read `ORIGIN`/`LENGTH` from the MEMORY declaration of a named region: find
 * the region name, then the next `ORIGIN`/`LENGTH` after it.
 */
static const char *region_attr(const ld_script_t *s, const char *region,
                               const char *attr)
{
    const char *p = s->text;
    size_t      rlen = strlen(region);

    while ((p = strstr(p, region)) != NULL) {
        int boundary_ok = (p == s->text) || !is_ident_char(p[-1]);
        if (boundary_ok && !is_ident_char(*(p + rlen))) {
            /* A region declaration has a ':' before its ORIGIN.  Requiring one
             * rejects the uses of the same name inside ORIGIN(OCRAM). */
            const char *colon = strchr(p, ':');
            const char *found = strstr(p, attr);
            if (colon != NULL && found != NULL && colon < found) {
                const char *q = skip_ws(found + strlen(attr));
                if (*q == '=') return skip_ws(q + 1);
            }
        }
        p += rlen;
    }
    return NULL;
}

static int region_value(const ld_script_t *s, const char *region,
                        const char *attr, uint64_t *out)
{
    const char *rhs = region_attr(s, region, attr);
    if (rhs == NULL) {
        printf("FAIL [%s] no %s for region %s in %s\n",
               __func__, attr, region, s->path);
        g_fail_count++;
        return -1;
    }
    if (parse_ld_number(rhs, out, NULL) != 0) {
        printf("FAIL [%s] %s of %s in %s is not a numeric literal\n",
               __func__, attr, region, s->path);
        g_fail_count++;
        return -1;
    }
    return 0;
}

/* ── Tests ───────────────────────────────────────────────────────────────── */

/*
 * The application script's STAGING region must be expressed IN TERMS OF the
 * two constants this suite checks.  Without this, someone could satisfy every
 * numeric assertion below while the region line itself carried a literal, and
 * the constants would be decoration.
 */
static void test_app_region_uses_the_constants(void)
{
    const char *origin = region_attr(&g_app, "STAGING", "ORIGIN");
    const char *length = region_attr(&g_app, "STAGING", "LENGTH");

    ASSERT(origin != NULL && length != NULL,
           "app script has no STAGING region with ORIGIN and LENGTH");
    if (origin == NULL || length == NULL) return;

    ASSERT(strncmp(origin, "NP_APP_IMAGE_ORIGIN", 19) == 0,
           "STAGING ORIGIN is not NP_APP_IMAGE_ORIGIN — the checked constants "
           "are not what places the image");
    ASSERT(strncmp(length, "NP_APP_IMAGE_LENGTH", 19) == 0,
           "STAGING LENGTH is not NP_APP_IMAGE_LENGTH — the checked constants "
           "are not what bounds the image");
}

static void test_ocram_base_agrees(void)
{
    uint64_t boot_origin = 0U;
    uint64_t app_origin  = 0U;

    if (region_value(&g_boot, "OCRAM", "ORIGIN", &boot_origin) != 0) return;
    if (ld_value(&g_app, "NP_OCRAM_ORIGIN", &app_origin) != 0) return;

    ASSERT(boot_origin == app_origin,
           "the two scripts disagree about where OCRAM starts");
    if (boot_origin != app_origin) {
        printf("       bootloader ORIGIN(OCRAM) = 0x%08lx, "
               "application NP_OCRAM_ORIGIN = 0x%08lx\n",
               (unsigned long)boot_origin, (unsigned long)app_origin);
    }
}

static void test_load_offset_agrees(void)
{
    uint64_t boot_off = 0U;
    uint64_t app_off  = 0U;

    if (ld_value(&g_boot, "_app_load_offset", &boot_off) != 0) return;
    if (ld_value(&g_app, "NP_APP_LOAD_OFFSET", &app_off) != 0) return;

    ASSERT(boot_off == app_off,
           "the two scripts disagree about where staging begins — the image "
           "would be linked for an address the bootloader does not stage it at");
    if (boot_off != app_off) {
        printf("       bootloader _app_load_offset = %lu, "
               "application NP_APP_LOAD_OFFSET = %lu\n",
               (unsigned long)boot_off, (unsigned long)app_off);
    }
}

/*
 * The one that would have caught Defect C, in its new location: the
 * application's declared image length must equal the bootloader's DERIVED
 * staging size, recomputed here from the three terms the bootloader script
 * actually declares rather than read from the comment that explains them.
 */
static void test_image_length_equals_derived_staging_size(void)
{
    uint64_t ocram_len = 0U;
    uint64_t load_off  = 0U;
    uint64_t stack_sz  = 0U;
    uint64_t app_len   = 0U;

    if (region_value(&g_boot, "OCRAM", "LENGTH", &ocram_len) != 0) return;
    if (ld_value(&g_boot, "_app_load_offset", &load_off) != 0) return;
    if (ld_value(&g_boot, "_stack_size", &stack_sz) != 0) return;
    if (ld_value(&g_app, "NP_APP_IMAGE_LENGTH", &app_len) != 0) return;

    ASSERT(ocram_len > load_off + stack_sz,
           "bootloader OCRAM cannot hold its own reservations");
    if (ocram_len <= load_off + stack_sz) return;

    uint64_t derived = ocram_len - load_off - stack_sz;

    ASSERT(app_len == derived,
           "application NP_APP_IMAGE_LENGTH does not equal the bootloader's "
           "staging reservation — this is the Defect C shape (NP-SW-CI-001 "
           "§4.3), one number owned by two files");
    if (app_len != derived) {
        printf("       bootloader: %lu (OCRAM) - %lu (offset) - %lu (stack) "
               "= %lu\n       application NP_APP_IMAGE_LENGTH = %lu\n",
               (unsigned long)ocram_len, (unsigned long)load_off,
               (unsigned long)stack_sz, (unsigned long)derived,
               (unsigned long)app_len);
    }
}

/*
 * The bootloader's own staging size must still be DERIVED, not a literal.  If
 * someone "simplifies" _app_staging_size to 440K, the test above keeps passing
 * and the derivation that makes the whole arrangement safe is gone.
 */
static void test_bootloader_staging_size_is_still_derived(void)
{
    const char *rhs = find_assignment(&g_boot, "_app_staging_size");

    ASSERT(rhs != NULL, "bootloader script has no _app_staging_size assignment");
    if (rhs == NULL) return;

    uint64_t literal = 0U;
    ASSERT(parse_ld_number(skip_ws(rhs), &literal, NULL) != 0,
           "_app_staging_size is a literal again — it must stay derived from "
           "LENGTH(OCRAM), _app_load_offset and _stack_size, which is the fix "
           "Defect C received");
}

int main(void)
{
    printf("NeurOne SW-02 linker-script agreement tests "
           "(NP-SW-CI-001 §4.8)\n");

    if (ld_load(&g_boot, NP_BOOTLOADER_LD_PATH) != 0 ||
        ld_load(&g_app,  NP_APPLICATION_LD_PATH) != 0) {
        printf("\n%d failure(s)\n", g_fail_count);
        return g_fail_count;
    }

    test_app_region_uses_the_constants();
    test_ocram_base_agrees();
    test_load_offset_agrees();
    test_image_length_equals_derived_staging_size();
    test_bootloader_staging_size_is_still_derived();

    if (g_fail_count == 0) {
        printf("PASS — both linker scripts agree about the staging area\n");
    } else {
        printf("\n%d failure(s)\n", g_fail_count);
    }
    return g_fail_count;
}
