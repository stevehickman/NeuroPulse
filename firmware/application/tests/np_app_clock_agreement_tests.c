/*
 * NeurOne SW-02 — Core Clock Agreement Host Tests
 * Document: NP-SW-CI-001 §4.11 (OI-SWCI-41)
 *
 * ── What this suite is for ───────────────────────────────────────────────────
 *
 * Two files in this tree own one fact — the frequency the i.MX RT1062 core runs
 * at — and they do not agree:
 *
 *   firmware/hub_control/include/FreeRTOSConfig.h
 *       configCPU_CLOCK_HZ = 600 MHz.  The ARM_CM7 port defines
 *       configSYSTICK_CLOCK_HZ as configCPU_CLOCK_HZ, so this number is the
 *       unit every FreeRTOS interval in the image is measured in.
 *
 *   firmware/vendor/mcux_sdk/devices/MIMXRT1062/system_MIMXRT1062.h
 *       DEFAULT_SYSTEM_CLOCK = 528 MHz.  This is what SystemInit() ASSIGNS to
 *       SystemCoreClock before any application code runs.
 *
 * Neither file establishes anything.  Assigning a variable does not configure a
 * PLL, and the function that would — BOARD_BootClockRUN() — lives in the SDK's
 * board files, which are not vendored because no NeurOne board exists to
 * configure for.  That is OI-SWCI-41, and the half of it that needs a board
 * stays open.
 *
 * ── What a disagreement would actually do ───────────────────────────────────
 *
 * Nothing at build time, and — this is the part that makes it dangerous —
 * nothing obviously wrong at run time either.  A core clock that is not
 * configCPU_CLOCK_HZ does not fault, hang or assert.  It rescales every tick,
 * every timeout and the NP_SAFETY_HEARTBEAT_MS beat by the ratio between the
 * two clocks, silently and uniformly, so nothing inside the system has a
 * correct interval to compare itself against.  This suite computes that
 * rescaling and asserts the resulting figures, so the consequence is a number
 * in the test output rather than a sentence in a document.
 *
 * The final case is the one worth reading: at the SDK's own default clock the
 * 200 ms heartbeat becomes 227 ms, and 227 ms is still comfortably inside the
 * safety MCU's 1.5 s watchdog.  The error therefore does NOT announce itself by
 * tripping the interlock that would otherwise catch it.  That is precisely why
 * np_app_main.c has to check the clock explicitly instead of trusting that a
 * wrong one would show up.
 *
 * ── What it does NOT prove ──────────────────────────────────────────────────
 *
 * That the part actually comes up at configCPU_CLOCK_HZ.  A host has no CCM and
 * no silicon.  That is a property of the running device and is asserted where
 * it can be: np_app_main.c calls np_platform_clock_init(), then
 * SystemCoreClockUpdate() — which recomputes SystemCoreClock from the CCM and
 * CCM_ANALOG dividers as they actually stand — and halts unless both agree with
 * configCPU_CLOCK_HZ.  Same division of labour, and the same reasoning, as
 * np_app_link_agreement_tests.
 *
 * Return convention: 0 = PASS, non-zero = failure count.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * FreeRTOSConfig.h is included rather than text-parsed, deliberately.
 * configCPU_CLOCK_HZ is defined TWICE in that file — once for the POSIX host
 * smoke test and once for the target — so a text probe would have to decide
 * which branch it was looking at, and would be reading the preprocessor's job
 * out of its hands.  Including it with NP_FREERTOS_POSIX_HOST undefined makes
 * the compiler resolve the branch, which is the same mechanism the target build
 * uses and therefore cannot disagree with it.
 *
 * Two names the config uses must exist first.  They are FreeRTOS kernel types,
 * and pulling in the kernel headers for two tokens would drag the whole port
 * into a host test that has no scheduler in it.  The TickType_t width is not
 * guessed: TICK_TYPE_WIDTH_32_BITS is given the value the kernel gives it, and
 * the first test case asserts the config actually selects it — so if the
 * configuration ever moved to a 16- or 64-bit tick, this file fails rather than
 * quietly computing with the wrong type.
 */
typedef uint32_t TickType_t;
#define TICK_TYPE_WIDTH_16_BITS  0
#define TICK_TYPE_WIDTH_32_BITS  1
#define TICK_TYPE_WIDTH_64_BITS  2

#include "FreeRTOSConfig.h"       /* configCPU_CLOCK_HZ, configTICK_RATE_HZ    */
#include "np_hub_config.h"        /* NP_SAFETY_HEARTBEAT_MS, _WATCHDOG_MS      */
#include "system_MIMXRT1062.h"    /* DEFAULT_SYSTEM_CLOCK                      */

#ifndef NP_APP_MAIN_C_PATH
#error "NP_APP_MAIN_C_PATH must be defined by the build"
#endif

static int g_fail_count = 0;

#define ASSERT(cond, msg)                                            \
    do {                                                             \
        if (!(cond)) {                                               \
            printf("FAIL [%s:%d] %s\n", __func__, __LINE__, (msg));  \
            g_fail_count++;                                          \
        }                                                            \
    } while (0)

/* SysTick's LOAD register is 24 bits wide on every Cortex-M. */
#define SYSTICK_LOAD_MAX  0x00FFFFFFUL

/* ── 1. The configuration this file computes against ─────────────────────────
 *
 * Pins the two constants rather than reading them and shrugging.  Both are
 * numbers other things were decided against — the 600 MHz part number in
 * CLAUDE.md §4.1, the vendored SDK version in firmware/vendor/mcux_sdk/VERSION
 * — so either of them moving is a design event that should stop here and be
 * looked at, not a value this suite silently re-derives its expectations from.
 */
static void test_declared_constants(void)
{
    ASSERT(configTICK_TYPE_WIDTH_IN_BITS == TICK_TYPE_WIDTH_32_BITS,
           "FreeRTOSConfig.h no longer selects a 32-bit tick; the TickType_t "
           "typedef at the top of this file is then wrong");

    ASSERT(configCPU_CLOCK_HZ == 600000000UL,
           "configCPU_CLOCK_HZ moved. It is the unit every FreeRTOS interval "
           "is measured in and the value np_app_main.c checks the silicon "
           "against — update NP-SW-CI-001 §4.11 and this suite together");

    ASSERT(configTICK_RATE_HZ == 1000U,
           "configTICK_RATE_HZ moved; the tick and heartbeat arithmetic below "
           "is stated in whole milliseconds on the assumption of a 1 ms tick");

    ASSERT(DEFAULT_SYSTEM_CLOCK == 528000000UL,
           "The vendored SDK's DEFAULT_SYSTEM_CLOCK moved. This is the clock "
           "SystemInit() assigns before any application code runs, so it is "
           "the figure the mis-timing cases below are computed from");
}

/* ── 2. SysTick can actually express the configured tick ─────────────────────
 *
 * A hardware limit, not a style rule.  The reload value is written to a 24-bit
 * register; a configuration that overflows it does not fail to build and does
 * not fail to run — the port writes the low 24 bits and the system ticks at a
 * frequency nobody chose.  At 600 MHz / 1 kHz there are 8 bits of headroom, but
 * the check is cheap and the failure it guards against is silent.
 */
static void test_systick_reload_is_representable(void)
{
    ASSERT((configCPU_CLOCK_HZ % configTICK_RATE_HZ) == 0UL,
           "configCPU_CLOCK_HZ is not an exact multiple of configTICK_RATE_HZ; "
           "the integer division in the port would drop the remainder and the "
           "tick would drift against wall-clock time forever");

    uint32_t counts_per_tick = (uint32_t)(configCPU_CLOCK_HZ / configTICK_RATE_HZ);
    uint32_t reload          = counts_per_tick - 1U;

    ASSERT(counts_per_tick == 600000UL, "counts per tick is not 600,000");
    ASSERT(reload == 599999UL,          "SysTick reload is not 599,999");
    ASSERT(reload <= SYSTICK_LOAD_MAX,
           "SysTick reload does not fit the 24-bit LOAD register; the port "
           "would silently tick at the wrong rate");
}

/* ── 3. What the SDK's own default clock would do to every interval ──────────
 *
 * The image is linked with a reload computed from configCPU_CLOCK_HZ.  If the
 * part is running at DEFAULT_SYSTEM_CLOCK instead — which is the state
 * SystemInit() leaves SystemCoreClock claiming, and no code in this tree moves
 * the PLLs away from whatever the boot ROM left — then each of those reload
 * counts takes longer than the tick was specified to take, by exactly the ratio
 * between the two clocks.
 *
 * 600,000 counts at 528 MHz = 1,136,363 ns, against a specified 1,000,000 ns.
 * Every timeout in the system is 13.6% long, and nothing in the system knows.
 */
static void test_mistimed_tick_at_sdk_default_clock(void)
{
    uint64_t counts_per_tick = (uint64_t)configCPU_CLOCK_HZ / (uint64_t)configTICK_RATE_HZ;

    uint64_t nominal_tick_ns = 1000000000ULL / (uint64_t)configTICK_RATE_HZ;
    uint64_t actual_tick_ns  = (counts_per_tick * 1000000000ULL) / (uint64_t)DEFAULT_SYSTEM_CLOCK;

    ASSERT(nominal_tick_ns == 1000000ULL, "nominal tick is not 1 ms");
    ASSERT(actual_tick_ns == 1136363ULL,
           "a tick at the SDK default clock is not 1,136,363 ns");
    ASSERT(actual_tick_ns > nominal_tick_ns,
           "the mis-timed tick is not longer than the specified one");

    /* Stated as parts per million so the figure survives a change of clocks. */
    uint64_t error_ppm = ((actual_tick_ns - nominal_tick_ns) * 1000000ULL) / nominal_tick_ns;
    ASSERT(error_ppm == 136363ULL,
           "the tick error is not 136,363 ppm (13.6%) at the SDK default clock");

    printf("      tick: specified %llu ns, actual %llu ns at %lu Hz (+%llu ppm)\n",
           (unsigned long long)nominal_tick_ns,
           (unsigned long long)actual_tick_ns,
           (unsigned long)DEFAULT_SYSTEM_CLOCK,
           (unsigned long long)error_ppm);
}

/* ── 4. The same error on the interval the safety MCU is watching ────────────
 *
 * This is the case the check in np_app_main.c exists for.
 *
 * task_safety_heartbeat waits pdMS_TO_TICKS(NP_SAFETY_HEARTBEAT_MS) between
 * beats, so its period is expressed purely in ticks and inherits the tick's
 * error exactly.  At the SDK default clock a 200 ms beat is sent every 227 ms.
 *
 * The assertion that matters is the last one: 227 ms is still far inside the
 * safety MCU's 1.5 s watchdog, so the mis-timing does not trip the interlock.
 * The system would run, indefinitely, with every interval in it wrong and every
 * independent check of those intervals satisfied.  A failure that the existing
 * safeguards cannot see is a failure that needs its own check, and that is the
 * argument for the one in main().
 */
static void test_mistimed_safety_heartbeat(void)
{
    uint64_t counts_per_tick = (uint64_t)configCPU_CLOCK_HZ / (uint64_t)configTICK_RATE_HZ;
    uint64_t beat_ticks      = (uint64_t)NP_SAFETY_HEARTBEAT_MS;  /* 1 ms tick */

    uint64_t actual_beat_us =
        (beat_ticks * counts_per_tick * 1000000ULL) / (uint64_t)DEFAULT_SYSTEM_CLOCK;

    ASSERT(NP_SAFETY_HEARTBEAT_MS == 200U, "the heartbeat period is not 200 ms");
    ASSERT(actual_beat_us == 227272ULL,
           "a 200 ms heartbeat at the SDK default clock is not 227,272 us");

    ASSERT(actual_beat_us < ((uint64_t)NP_SAFETY_WATCHDOG_MS * 1000ULL),
           "this test's premise has changed: the mis-timed heartbeat now "
           "EXCEEDS the watchdog, so the error would announce itself");

    printf("      heartbeat: specified %u ms, actual %llu us at %lu Hz "
           "(watchdog %u ms — not tripped, so the error is silent)\n",
           (unsigned)NP_SAFETY_HEARTBEAT_MS,
           (unsigned long long)actual_beat_us,
           (unsigned long)DEFAULT_SYSTEM_CLOCK,
           (unsigned)NP_SAFETY_WATCHDOG_MS);
}

/* ── 5. The boot path still contains the check ───────────────────────────────
 *
 * A probe of np_app_main.c's CODE, and the emphasis is not decoration: the
 * first version of this case searched the file as raw text and passed against a
 * boot path with the check deleted, because np_app_main.c EXPLAINS the check at
 * length in its header comment and every token the probe looked for was sitting
 * in the prose.  It was caught by mutating the file it reads.  That is the same
 * defect np_bootloader_app_image_tests records for the linker scripts, and the
 * same fix: strip what the compiler would strip before looking.
 *
 * Comments and string literals are both removed — a string is as good a hiding
 * place for a token as a comment, and this file's own build note is a long
 * string sitting in the middle of the code.
 *
 * Its limits are worth stating plainly even so.  It proves the tokens appear in
 * code, and that the clock call precedes the handover to np_hub_control_app_main().
 * It does not prove they are reachable or correctly wired to each other; nothing
 * on a host can.  The cross-build cannot supply it either — --whole-archive puts
 * np_platform_clock_init() in the image whether or not anything calls it, so the
 * ELF is not evidence that main() calls it.
 *
 * What it does buy is that the claim in NP-SW-CI-001 §4.11 and the claim in the
 * code cannot drift apart unnoticed: deleting the check while leaving the
 * document asserting it now fails a test rather than passing in silence.
 */

/* Emit only what the compiler would see as code: no comments, no string or
 * character literal bodies.  Bytes are replaced with spaces rather than removed
 * so nothing on either side of a comment is joined into one token. */
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

static void test_boot_path_verifies_the_clock(void)
{
    static char raw[512U * 1024U];
    static char code[512U * 1024U];

    FILE *f = fopen(NP_APP_MAIN_C_PATH, "rb");
    if (f == NULL) {
        printf("FAIL [%s] cannot open %s\n", __func__, NP_APP_MAIN_C_PATH);
        g_fail_count++;
        return;
    }
    size_t n = fread(raw, 1U, sizeof(raw) - 1U, f);
    int truncated = (feof(f) == 0);
    fclose(f);
    raw[n] = '\0';

    if (truncated) {
        printf("FAIL [%s] %s larger than %u bytes — the probe would read a "
               "prefix and could miss the check\n",
               __func__, NP_APP_MAIN_C_PATH, (unsigned)sizeof(raw));
        g_fail_count++;
        return;
    }

    strip_to_code(raw, n, code);

    const char *clock_init = strstr(code, "np_platform_clock_init(");
    const char *core_upd   = strstr(code, "SystemCoreClockUpdate(");
    const char *cfg_hz     = strstr(code, "configCPU_CLOCK_HZ");
    const char *handover   = strstr(code, "np_hub_control_app_main(");

    ASSERT(clock_init != NULL,
           "np_app_main.c no longer calls np_platform_clock_init() in code; "
           "the boot path has no clock configuration step at all");
    ASSERT(core_upd != NULL,
           "np_app_main.c no longer calls SystemCoreClockUpdate() in code; the "
           "clock would be taken on the driver's word instead of read from the "
           "CCM");
    ASSERT(cfg_hz != NULL,
           "np_app_main.c no longer compares anything against "
           "configCPU_CLOCK_HZ; the established clock is unverified");
    ASSERT(handover != NULL,
           "np_app_main.c no longer calls np_hub_control_app_main(); this probe "
           "is anchored to a boot path that no longer exists");

    if (clock_init != NULL && core_upd != NULL && cfg_hz != NULL && handover != NULL) {
        ASSERT(clock_init < core_upd,
               "the clock is read back before it is configured");
        ASSERT(core_upd < handover && cfg_hz < handover,
               "the clock check runs AFTER control is handed to "
               "np_hub_control_app_main(); by then the tasks that depend on the "
               "tick have already been created");
    }
}

int main(void)
{
    printf("np_app_clock_agreement_tests (NP-SW-CI-001 §4.11, OI-SWCI-41)\n");

    test_declared_constants();
    test_systick_reload_is_representable();
    test_mistimed_tick_at_sdk_default_clock();
    test_mistimed_safety_heartbeat();
    test_boot_path_verifies_the_clock();

    if (g_fail_count == 0) {
        printf("PASS: core clock declarations agree with the boot-path check\n");
    } else {
        printf("FAILED: %d assertion(s)\n", g_fail_count);
    }
    return g_fail_count;
}
