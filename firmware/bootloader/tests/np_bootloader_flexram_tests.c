/*
 * NeurOne Bootloader — FlexRAM Partition Encoding Host Tests
 * Document: NP-SW-CI-001 §4.10 (phase 9, closes OI-SWCI-39)
 *
 * ── What this suite is for ───────────────────────────────────────────────────
 *
 * np_flexram_apply_partition() writes three peripheral registers on silicon
 * nobody has run.  Nothing on a host can check that the writes have the effect
 * the reference manual describes.  What CAN be checked, and is the half most
 * likely to be quietly wrong, is the ENCODING: the bank-config word and the TCM
 * window codes are bit-packed by hand, and a field packed in the wrong order or
 * a size code off by one produces a device whose memory is not where the linker
 * put it — a failure that presents as a hard fault microseconds into boot, on a
 * board with no debugger attached yet, with nothing to point at this file.
 *
 * So the module is split the way np_app_image.c is: two pure functions that
 * compute the register values, checked here against oracles worked out by hand
 * from the bit layout, and one device-only function that stores them.  The
 * `#ifndef NP_FLEXRAM_HOST_TEST` guard is what makes the split real rather than
 * conventional.
 *
 * ── The oracle ───────────────────────────────────────────────────────────────
 *
 * GPR17 FLEXRAM_BANK_CFG is 16 two-bit fields, bank n in bits [2n+1:2n]:
 * 0b00 unused, 0b01 OCRAM, 0b10 DTCM, 0b11 ITCM.  Banks are filled OCRAM, then
 * DTCM, then ITCM from bank 0 upward.  The expected words below are written as
 * literals rather than recomputed from the same loop the code uses — a test
 * that recomputes the implementation is a test that agrees with any bug it
 * contains.
 *
 * GPR14 CM7_CFG{I,D}TCMSZ is a 4-bit code, size = 2^(code-1) KiB, valid 3..10
 * for 4 KiB..512 KiB, with 0 meaning a 0 KiB window.
 *
 * ── What this does NOT prove ─────────────────────────────────────────────────
 *
 * That the silicon honours the words, that INIT_DTCM_EN behaves as documented
 * at runtime rather than only out of reset, or that the partition survives to
 * the moment load_and_jump() hands over.  Those need a board.  The agreement
 * between these constants and the two linker scripts is a separate suite,
 * np_app_link_agreement_tests.
 *
 * Return convention: 0 = PASS, non-zero = failure count.
 */

#include <stdio.h>
#include <stdint.h>

#include "np_config.h"
#include "np_flexram.h"

static int g_fail_count = 0;

#define CHECK(cond, msg)                                             \
    do {                                                             \
        if (!(cond)) {                                               \
            printf("FAIL [%s:%d] %s\n", __func__, __LINE__, (msg));  \
            g_fail_count++;                                          \
        }                                                            \
    } while (0)

#define CHECK_U32(got, want, msg)                                            \
    do {                                                                     \
        uint32_t g_ = (got);                                                 \
        uint32_t w_ = (want);                                                \
        if (g_ != w_) {                                                      \
            printf("FAIL [%s:%d] %s: got 0x%08lX, want 0x%08lX\n",           \
                   __func__, __LINE__, (msg),                                \
                   (unsigned long)g_, (unsigned long)w_);                    \
            g_fail_count++;                                                  \
        }                                                                    \
    } while (0)

/* ── GPR17 bank-config encoding ──────────────────────────────────────────── */

static void test_bank_cfg_uniform_splits(void)
{
    /* 16 × 0b01 → 0x55555555; 16 × 0b10 → 0xAAAAAAAA; 16 × 0b11 → 0xFFFFFFFF. */
    CHECK_U32(np_flexram_bank_cfg(16U, 0U, 0U), 0x55555555UL, "all OCRAM");
    CHECK_U32(np_flexram_bank_cfg(0U, 16U, 0U), 0xAAAAAAAAUL, "all DTCM");
    CHECK_U32(np_flexram_bank_cfg(0U, 0U, 16U), 0xFFFFFFFFUL, "all ITCM");
    CHECK_U32(np_flexram_bank_cfg(0U, 0U, 0U),  0x00000000UL, "none assigned");
}

static void test_bank_cfg_field_order(void)
{
    /* This is the test that would catch a reversed or byte-swapped packing,
     * which the uniform splits above cannot: they are palindromes. */

    /* One OCRAM bank, the rest DTCM: bank 0 is 0b01 and everything above it is
     * 0b10 → 0xAAAAAAA9, NOT 0x6AAAAAAA and NOT 0xAAAAAA95. */
    CHECK_U32(np_flexram_bank_cfg(1U, 15U, 0U), 0xAAAAAAA9UL,
              "one OCRAM bank must occupy the LOW two bits");

    /* One ITCM bank: ITCM fills last, so it is the HIGH pair. */
    CHECK_U32(np_flexram_bank_cfg(0U, 15U, 1U), 0xEAAAAAAAUL,
              "one ITCM bank must occupy the HIGH two bits");

    /* Half and half: banks 0-7 OCRAM, 8-15 DTCM. */
    CHECK_U32(np_flexram_bank_cfg(8U, 8U, 0U), 0xAAAA5555UL,
              "8 OCRAM + 8 DTCM");

    /* The i.MX RT1062 DEFAULT eFuse split — 256 KiB OCRAM, 128 KiB DTCM,
     * 128 KiB ITCM = 8 + 4 + 4 banks.  Present because it is the partition
     * that was silently in force before this module existed, and because it is
     * the one an oracle can be cross-checked against outside this repository:
     * banks 0-7 OCRAM (0x5555), 8-11 DTCM (0xAA), 12-15 ITCM (0xFF). */
    CHECK_U32(np_flexram_bank_cfg(8U, 4U, 4U), 0xFFAA5555UL,
              "the default fuse split");
}

static void test_bank_cfg_rejects_oversubscription(void)
{
    /* Inert, not truncated.  A partition that assigns no banks is obviously
     * wrong on a bench; one that silently drops the last two is not. */
    CHECK_U32(np_flexram_bank_cfg(16U, 1U, 0U), 0UL, "17 banks requested");
    CHECK_U32(np_flexram_bank_cfg(8U, 8U, 8U),  0UL, "24 banks requested");
    CHECK_U32(np_flexram_bank_cfg(255U, 255U, 255U), 0UL, "765 banks requested");
}

/* ── GPR14 TCM window size encoding ──────────────────────────────────────── */

static void test_tcm_size_code_legal_sizes(void)
{
    CHECK_U32(np_flexram_tcm_size_code(0U),             0UL,  "0 B → code 0");
    CHECK_U32(np_flexram_tcm_size_code(4U * 1024U),     3UL,  "4 KiB → 3");
    CHECK_U32(np_flexram_tcm_size_code(8U * 1024U),     4UL,  "8 KiB → 4");
    CHECK_U32(np_flexram_tcm_size_code(16U * 1024U),    5UL,  "16 KiB → 5");
    CHECK_U32(np_flexram_tcm_size_code(32U * 1024U),    6UL,  "32 KiB → 6");
    CHECK_U32(np_flexram_tcm_size_code(64U * 1024U),    7UL,  "64 KiB → 7");
    CHECK_U32(np_flexram_tcm_size_code(128U * 1024U),   8UL,  "128 KiB → 8");
    CHECK_U32(np_flexram_tcm_size_code(256U * 1024U),   9UL,  "256 KiB → 9");
    CHECK_U32(np_flexram_tcm_size_code(512U * 1024U),   10UL, "512 KiB → 10");
}

static void test_tcm_size_code_rejects_the_rest(void)
{
    /* Below the encodable floor: the field has no code for 1 or 2 KiB even
     * though both are whole numbers of KiB. */
    CHECK_U32(np_flexram_tcm_size_code(1U * 1024U),
              NP_FLEXRAM_TCM_SIZE_INVALID, "1 KiB is not encodable");
    CHECK_U32(np_flexram_tcm_size_code(2U * 1024U),
              NP_FLEXRAM_TCM_SIZE_INVALID, "2 KiB is not encodable");

    /* Not a power of two — including one bank, 32 KiB being encodable but
     * three banks not. */
    CHECK_U32(np_flexram_tcm_size_code(3U * 32U * 1024U),
              NP_FLEXRAM_TCM_SIZE_INVALID, "96 KiB is not a legal window");
    CHECK_U32(np_flexram_tcm_size_code(1U),
              NP_FLEXRAM_TCM_SIZE_INVALID, "1 B is not a legal window");

    /* Above the ceiling: FlexRAM is 512 KiB in total, so no larger window can
     * ever be backed. */
    CHECK_U32(np_flexram_tcm_size_code(1024U * 1024U),
              NP_FLEXRAM_TCM_SIZE_INVALID, "1 MiB exceeds the array");
}

/* ── The configured partition ────────────────────────────────────────────── */

/*
 * These assert the values the DEVICE will actually be given, not the encoder's
 * behaviour in general.  If the partition in np_config.h is ever changed, this
 * is where the change has to be made deliberately rather than inherited.
 */
static void test_configured_partition(void)
{
    CHECK((uint32_t)NP_FLEXRAM_ITCM_BANKS + NP_FLEXRAM_DTCM_BANKS +
              NP_FLEXRAM_OCRAM_BANKS == (uint32_t)NP_FLEXRAM_BANK_COUNT,
          "the configured bank counts do not sum to NP_FLEXRAM_BANK_COUNT");

    CHECK(NP_FLEXRAM_DTCM_BANKS > 0U,
          "the configured partition has no DTCM — the application's .data and "
          "MSP stack are linked into it and its Reset_Handler touches both "
          "before the first C statement");

    CHECK_U32(np_flexram_bank_cfg((uint8_t)NP_FLEXRAM_OCRAM_BANKS,
                                  (uint8_t)NP_FLEXRAM_DTCM_BANKS,
                                  (uint8_t)NP_FLEXRAM_ITCM_BANKS),
              0xAAAAAAAAUL,
              "the configured partition is no longer all-DTCM — if that is "
              "intended, update this expectation and NP_FLEXRAM_DTCM_SIZE in "
              "app_imxrt1062.ld together");

    /* Both window sizes must be encodable.  An unencodable one would be
     * written to GPR14 as 0xF, which is a reserved code, not a large window. */
    CHECK(np_flexram_tcm_size_code(NP_DTCM_SIZE) != NP_FLEXRAM_TCM_SIZE_INVALID,
          "NP_DTCM_SIZE has no CM7_CFGDTCMSZ encoding");
    CHECK(np_flexram_tcm_size_code(NP_ITCM_SIZE) != NP_FLEXRAM_TCM_SIZE_INVALID,
          "NP_ITCM_SIZE has no CM7_CFGITCMSZ encoding");

    CHECK_U32(np_flexram_tcm_size_code(NP_DTCM_SIZE), 10UL,
              "the DTCM window code for 512 KiB");
    CHECK_U32(np_flexram_tcm_size_code(NP_ITCM_SIZE), 0UL,
              "the ITCM window code for 0 banks");
}

/*
 * The register field masks must be wide enough for the codes stored in them,
 * and must not overlap.  Both are transcribed from the reference manual rather
 * than taken from the vendored MCUX header, which does not define these fields
 * at all — so nothing but this checks them.
 */
static void test_gpr14_field_layout(void)
{
    CHECK_U32(NP_GPR14_CFGITCMSZ_MASK, 0x000F0000UL, "CM7_CFGITCMSZ mask");
    CHECK_U32(NP_GPR14_CFGDTCMSZ_MASK, 0x00F00000UL, "CM7_CFGDTCMSZ mask");

    CHECK((NP_GPR14_CFGITCMSZ_MASK & NP_GPR14_CFGDTCMSZ_MASK) == 0UL,
          "the two GPR14 TCM size fields overlap");

    /* The largest legal code, 10, must fit each field after shifting. */
    CHECK(((10UL << NP_GPR14_CFGITCMSZ_SHIFT) & ~NP_GPR14_CFGITCMSZ_MASK) == 0UL,
          "code 10 does not fit CM7_CFGITCMSZ");
    CHECK(((10UL << NP_GPR14_CFGDTCMSZ_SHIFT) & ~NP_GPR14_CFGDTCMSZ_MASK) == 0UL,
          "code 10 does not fit CM7_CFGDTCMSZ");

    /* GPR16's three control bits are distinct and all below CM7_INIT_VTOR,
     * which occupies 31:7 and must be preserved by the read-modify-write in
     * np_flexram_apply_partition(). */
    CHECK((NP_GPR16_INIT_ITCM_EN | NP_GPR16_INIT_DTCM_EN |
           NP_GPR16_FLEXRAM_BANK_CFG_SEL) == 0x7UL,
          "the three GPR16 control bits are not bits 0, 1 and 2");
}

int main(void)
{
    printf("NeurOne bootloader FlexRAM partition tests "
           "(NP-SW-CI-001 §4.10)\n");

    test_bank_cfg_uniform_splits();
    test_bank_cfg_field_order();
    test_bank_cfg_rejects_oversubscription();
    test_tcm_size_code_legal_sizes();
    test_tcm_size_code_rejects_the_rest();
    test_configured_partition();
    test_gpr14_field_layout();

    if (g_fail_count == 0) {
        printf("PASS: FlexRAM partition encodes to ITCM %u / DTCM %u / "
               "OCRAM %u banks (GPR17 = 0x%08lX)\n",
               (unsigned)NP_FLEXRAM_ITCM_BANKS,
               (unsigned)NP_FLEXRAM_DTCM_BANKS,
               (unsigned)NP_FLEXRAM_OCRAM_BANKS,
               (unsigned long)np_flexram_bank_cfg(
                   (uint8_t)NP_FLEXRAM_OCRAM_BANKS,
                   (uint8_t)NP_FLEXRAM_DTCM_BANKS,
                   (uint8_t)NP_FLEXRAM_ITCM_BANKS));
    } else {
        printf("\n%d failure(s)\n", g_fail_count);
    }
    return g_fail_count;
}
