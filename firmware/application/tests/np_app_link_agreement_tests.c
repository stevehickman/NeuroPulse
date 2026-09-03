/*
 * NeurOne SW-02 — Linker Script Agreement Host Tests
 * Document: NP-SW-CI-001 §4.8 (phase 8, closes OI-SWCI-21),
 *           NP-SW-CI-001 §4.10 (phase 9, closes OI-SWCI-39)
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
 * Since §4.10 there is a SECOND fact with the same shape and a third copy: the
 * FlexRAM partition.  bootloader_imxrt1062.ld declares NP_FLEXRAM_*_BANKS,
 * np_config.h declares the same counts for the code that writes IOMUXC_GPR17,
 * and app_imxrt1062.ld declares NP_FLEXRAM_DTCM_SIZE, which is what the
 * application's DTCM region is linked against.  This suite reads all three.
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
 * The partition disagreement is worse in one specific way, which is why it is
 * checked here rather than left to the link: a DTCM region LARGER than the
 * banks np_flexram_apply_partition() actually assigns links cleanly on both
 * sides and produces an image that faults on its first access to memory that
 * does not exist.  Neither linker can see the C constant that decides it.
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

/* The bootloader's hardware configuration — the SAME header
 * np_flexram_apply_partition() writes IOMUXC_GPR17/16/14 from.  Included, not
 * text-parsed: for the FlexRAM partition the third copy of the fact is C rather
 * than a linker script, and the compiler is a better reader of C than this
 * file's parser is.  It contributes only #defines here; nothing in it is
 * dereferenced, so the absolute peripheral addresses it names stay inert on a
 * host.  NP-SW-CI-001 §4.10. */
#include "np_config.h"

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

/* ── FlexRAM partition (NP-SW-CI-001 §4.10, closes OI-SWCI-39) ─────────────────
 *
 * Three declarations of one decision, and none of the three can see the others:
 *
 *   bootloader_imxrt1062.ld   NP_FLEXRAM_{BANK_SIZE,BANK_COUNT,*_BANKS}
 *   np_config.h               the same, and what np_flexram_apply_partition()
 *                             actually writes to IOMUXC_GPR17/GPR14/GPR16
 *   app_imxrt1062.ld          NP_FLEXRAM_DTCM_SIZE — the DTCM region the
 *                             application's .data and MSP stack are linked into
 */

/* The header's counts are what the DEVICE gets.  Anything the two linker
 * scripts assert is checked against this, never against each other only —
 * otherwise the scripts could agree with one another and disagree with the
 * silicon, which is the state OI-SWCI-39 was raised about. */
static void test_partition_is_self_consistent_in_the_header(void)
{
    ASSERT((uint64_t)NP_FLEXRAM_ITCM_BANKS + NP_FLEXRAM_DTCM_BANKS +
               NP_FLEXRAM_OCRAM_BANKS == (uint64_t)NP_FLEXRAM_BANK_COUNT,
           "np_config.h's FlexRAM bank counts do not sum to "
           "NP_FLEXRAM_BANK_COUNT — part of the array would be unassigned");

    ASSERT(NP_FLEXRAM_DTCM_BANKS > 0U,
           "np_config.h assigns no DTCM banks — the application's .data and "
           "MSP stack would have nowhere to live, and its Reset_Handler "
           "touches both before its first C statement");

    ASSERT((uint64_t)NP_DTCM_SIZE ==
               (uint64_t)NP_FLEXRAM_DTCM_BANKS * NP_FLEXRAM_BANK_SIZE,
           "NP_DTCM_SIZE is not NP_FLEXRAM_DTCM_BANKS banks — the derived "
           "size has been overridden with a literal");

    ASSERT((uint64_t)NP_ITCM_SIZE ==
               (uint64_t)NP_FLEXRAM_ITCM_BANKS * NP_FLEXRAM_BANK_SIZE,
           "NP_ITCM_SIZE is not NP_FLEXRAM_ITCM_BANKS banks");
}

/*
 * The bootloader script's OCRAM region is the DEDICATED OCRAM2 array, and this
 * is the test that keeps that true.  OCRAM2 ends exactly where the FlexRAM
 * OCRAM aperture begins, so as long as the partition assigns FlexRAM zero OCRAM
 * banks, the region below is the whole of OCRAM and its LENGTH is exact.
 * Assign one bank and it silently stops being — the aperture goes live, the
 * "512 KiB" becomes a description of a prefix, and nothing else in the build
 * would notice.
 */
static void test_bootloader_ocram_is_the_dedicated_ocram2(void)
{
    uint64_t boot_origin = 0U;
    uint64_t boot_len    = 0U;

    if (region_value(&g_boot, "OCRAM", "ORIGIN", &boot_origin) != 0) return;
    if (region_value(&g_boot, "OCRAM", "LENGTH", &boot_len) != 0) return;

    ASSERT(boot_origin == (uint64_t)NP_OCRAM_BASE,
           "the bootloader script and np_config.h disagree about NP_OCRAM_BASE");
    ASSERT(boot_len == (uint64_t)NP_OCRAM_SIZE,
           "the bootloader script and np_config.h disagree about NP_OCRAM_SIZE");

    ASSERT((uint64_t)NP_OCRAM_BASE + NP_OCRAM_SIZE ==
               (uint64_t)NP_FLEXRAM_OCRAM_BASE,
           "OCRAM2 does not end where the FlexRAM OCRAM aperture begins — the "
           "adjacency the 'OCRAM2 is the only OCRAM' claim rests on is gone");

    ASSERT(NP_FLEXRAM_OCRAM_BANKS == 0U,
           "FlexRAM is assigning OCRAM banks again — the aperture at "
           "NP_FLEXRAM_OCRAM_BASE is live and the bootloader script's OCRAM "
           "region no longer describes the whole of OCRAM");
}

/* Both linker scripts must state the partition the header states. */
static void test_bootloader_script_partition_agrees_with_the_header(void)
{
    struct { const char *name; uint64_t expected; } fields[] = {
        { "NP_FLEXRAM_BANK_SIZE",   (uint64_t)NP_FLEXRAM_BANK_SIZE   },
        { "NP_FLEXRAM_BANK_COUNT",  (uint64_t)NP_FLEXRAM_BANK_COUNT  },
        { "NP_FLEXRAM_ITCM_BANKS",  (uint64_t)NP_FLEXRAM_ITCM_BANKS  },
        { "NP_FLEXRAM_DTCM_BANKS",  (uint64_t)NP_FLEXRAM_DTCM_BANKS  },
        { "NP_FLEXRAM_OCRAM_BANKS", (uint64_t)NP_FLEXRAM_OCRAM_BANKS },
    };

    for (size_t i = 0U; i < sizeof(fields) / sizeof(fields[0]); i++) {
        uint64_t got = 0U;
        if (ld_value(&g_boot, fields[i].name, &got) != 0) continue;
        ASSERT(got == fields[i].expected,
               "the bootloader linker script and np_config.h disagree about a "
               "FlexRAM bank field");
        if (got != fields[i].expected) {
            printf("       %s: script %lu, np_config.h %lu\n",
                   fields[i].name, (unsigned long)got,
                   (unsigned long)fields[i].expected);
        }
    }
}

static void test_app_dtcm_matches_the_established_partition(void)
{
    uint64_t app_dtcm   = 0U;
    uint64_t dtcm_origin = 0U;

    if (ld_value(&g_app, "NP_FLEXRAM_DTCM_SIZE", &app_dtcm) != 0) return;

    ASSERT(app_dtcm == (uint64_t)NP_DTCM_SIZE,
           "the application is linked for a DTCM the partition does not "
           "establish — a region larger than the banks assigned links cleanly "
           "and faults on first access to memory that does not exist");
    if (app_dtcm != (uint64_t)NP_DTCM_SIZE) {
        printf("       app NP_FLEXRAM_DTCM_SIZE = %lu, "
               "%u banks x %u B = %lu\n",
               (unsigned long)app_dtcm, (unsigned)NP_FLEXRAM_DTCM_BANKS,
               (unsigned)NP_FLEXRAM_BANK_SIZE, (unsigned long)NP_DTCM_SIZE);
    }

    if (region_value(&g_app, "DTCM", "ORIGIN", &dtcm_origin) != 0) return;
    ASSERT(dtcm_origin == (uint64_t)NP_DTCM_BASE,
           "the application's DTCM region does not start at NP_DTCM_BASE");

    /* The MSP stack is pinned to the top of DTCM and must fit inside it. */
    uint64_t stack = 0U;
    if (ld_value(&g_app, "NP_APP_STACK_SIZE", &stack) != 0) return;
    ASSERT(stack < app_dtcm,
           "the application MSP stack does not fit inside DTCM");
}

/*
 * As for STAGING: the region line itself must be written in terms of the
 * checked constant.  Without this, every numeric assertion above can pass while
 * the MEMORY block carries a literal, and NP_FLEXRAM_DTCM_SIZE becomes
 * decoration that nothing links against.
 */
static void test_app_dtcm_region_uses_the_constant(void)
{
    const char *length = region_attr(&g_app, "DTCM", "LENGTH");

    ASSERT(length != NULL, "app script has no DTCM region with a LENGTH");
    if (length == NULL) return;

    ASSERT(strncmp(length, "NP_FLEXRAM_DTCM_SIZE", 20) == 0,
           "DTCM LENGTH is not NP_FLEXRAM_DTCM_SIZE — the checked constant is "
           "not what bounds the region");
}

/* ── §4.11 (OI-SWCI-42): the DTCM guarantee ──────────────────────────────────
 *
 * NP_DTCM_GUARANTEED_SIZE is not a fourth statement of the partition, so it is
 * not checked for equality against a bank count.  It is the DTCM that exists
 * with NO register write — the default eFuse split, which is also the figure
 * the MCUX SDK's linker scripts declare as m_data.  Two things have to hold for
 * it to mean anything:
 *
 *   1. it must not exceed the DTCM the partition establishes.  A "guarantee"
 *      larger than the real region would admit data into memory that does not
 *      exist under EITHER partition, which is worse than having no guarantee;
 *   2. it must be the SDK's 128 KiB.  If someone raises it to buy headroom,
 *      the constant stops describing the default partition and silently starts
 *      describing a preference — and the assert in the linker script, which
 *      reads correct either way, would then be enforcing nothing.
 */
static void test_dtcm_guarantee_is_the_default_efuse_dtcm(void)
{
    uint64_t guaranteed = 0U;
    uint64_t established = 0U;

    if (ld_value(&g_app, "NP_DTCM_GUARANTEED_SIZE", &guaranteed) != 0) return;

    ASSERT(guaranteed == 128U * 1024U,
           "NP_DTCM_GUARANTEED_SIZE is not 128 KiB — it is supposed to be the "
           "DTCM the DEFAULT eFuse partition provides (the MCUX SDK's m_data), "
           "not a headroom figure someone chose");
    if (guaranteed != 128U * 1024U) {
        printf("       NP_DTCM_GUARANTEED_SIZE = %lu, expected %u\n",
               (unsigned long)guaranteed, 128U * 1024U);
    }

    if (ld_value(&g_app, "NP_FLEXRAM_DTCM_SIZE", &established) != 0) return;
    ASSERT(guaranteed <= established,
           "NP_DTCM_GUARANTEED_SIZE exceeds the DTCM the partition establishes "
           "— data inside the 'guarantee' would then be outside the region");
}

/*
 * The guarantee is only load-bearing if the link enforces it.  The bound is
 * written as an ASSERT in the linker script rather than as a MEMORY region, so
 * nothing but this test notices if it is deleted — a region overflow would not
 * be reported, because .dtcm_bss growing past 128 KiB is still inside the
 * established 512 KiB DTCM and links perfectly cleanly.
 *
 * That is exactly the failure this case exists for: the mistake would not be
 * visible on the bench either, because it would work on any board where the
 * partition took effect and fail only on one where it did not.
 */
static void test_dtcm_guarantee_is_enforced_by_an_assert(void)
{
    ASSERT(strstr(g_app.text, "__dtcm_bss_end__ <= ORIGIN(DTCM) + NP_DTCM_GUARANTEED_SIZE") != NULL,
           "the application script does not ASSERT .dtcm_bss inside "
           "NP_DTCM_GUARANTEED_SIZE — the constant would be decoration, and a "
           "buffer moved past 128 KiB would link cleanly and exist only if the "
           "partition took effect");
}

/*
 * .dtcm_bss is bss-class storage the vendored SDK startup does NOT zero: it
 * clears exactly __bss_start__..__bss_end__, and firmware/vendor/mcux_sdk/ is
 * byte-exact so it cannot be taught otherwise.  np_app_dtcm_bss_clear() is what
 * zeroes it, and it reads these two symbols.
 *
 * Checking the symbols exist is the half a host test can do — the link itself
 * catches a rename (ld reports the undefined symbol from the ASSERT expression,
 * verified by mutation 2026-09-03).  What this adds is the REASON, so a future
 * reader deleting the section does not have to rediscover that the startup
 * only zeroes one span.
 */
static void test_dtcm_bss_has_bounds_for_the_clear_function(void)
{
    ASSERT(strstr(g_app.text, "__dtcm_bss_start__ = .") != NULL,
           "app script defines no __dtcm_bss_start__ — np_app_dtcm_bss_clear() "
           "could not zero the region, and the vendored startup does not");
    ASSERT(strstr(g_app.text, "__dtcm_bss_end__ = .") != NULL,
           "app script defines no __dtcm_bss_end__");
}

/*
 * §4.11.2 — .lpsdr4 must have an output section AND be asserted empty.
 *
 * The section is what stops it being an orphan; the assert is what stops the
 * section from becoming a quiet home for it.  Measured 2026-09-03: with
 * --gc-sections disabled, ld's orphan placement put .lpsdr4 at VMA 0x20000554,
 * inside DTCM, with an LMA nothing copies from.  Both halves are needed, so
 * both are checked.
 */
static void test_lpsdr4_is_homed_and_asserted_empty(void)
{
    ASSERT(strstr(g_app.text, ".lpsdr4") != NULL,
           "app script declares no .lpsdr4 output section — the section would "
           "be an orphan and ld would place it silently, inside DTCM");
    ASSERT(strstr(g_app.text, "SIZEOF(.lpsdr4) == 0") != NULL,
           "app script does not ASSERT .lpsdr4 empty — no external SDRAM is "
           "established, so anything landing there is in memory the image does "
           "not have");
}

int main(void)
{
    printf("NeurOne SW-02 linker-script agreement tests "
           "(NP-SW-CI-001 §4.8, §4.10, §4.11)\n");

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

    /* §4.10 — the FlexRAM partition, across all three declarations. */
    test_partition_is_self_consistent_in_the_header();
    test_bootloader_ocram_is_the_dedicated_ocram2();
    test_bootloader_script_partition_agrees_with_the_header();
    test_app_dtcm_matches_the_established_partition();
    test_app_dtcm_region_uses_the_constant();

    /* §4.11 — DTCM residency: the guarantee, its enforcement, and .lpsdr4. */
    test_dtcm_guarantee_is_the_default_efuse_dtcm();
    test_dtcm_guarantee_is_enforced_by_an_assert();
    test_dtcm_bss_has_bounds_for_the_clear_function();
    test_lpsdr4_is_homed_and_asserted_empty();

    if (g_fail_count == 0) {
        printf("PASS — both linker scripts and np_config.h agree about the staging area, the FlexRAM partition and DTCM residency\n");
    } else {
        printf("\n%d failure(s)\n", g_fail_count);
    }
    return g_fail_count;
}
