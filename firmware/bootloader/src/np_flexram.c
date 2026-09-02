/*
 * NeurOne Bootloader — i.MX RT1062 FlexRAM Partition
 * Document: NP-SW-CI-001 §4.10 (closes OI-SWCI-39)
 *
 * See include/np_flexram.h for the decision, why it lives in the bootloader,
 * and why reassigning banks from code that is running is safe here.
 */

#include "np_flexram.h"
#include "np_config.h"

/* Two-bit bank codes, GPR17 FLEXRAM_BANK_CFG.  Same values as the MCUX SDK's
 * kFLEXRAM_Bank* enum (fsl_flexram_allocate.h), which is the encoding the
 * silicon defines; named here because the bootloader includes no SDK header. */
#define NP_FLEXRAM_BANK_UNUSED  0U
#define NP_FLEXRAM_BANK_OCRAM   1U
#define NP_FLEXRAM_BANK_DTCM    2U
#define NP_FLEXRAM_BANK_ITCM    3U

uint32_t np_flexram_bank_cfg(uint8_t ocram_banks,
                             uint8_t dtcm_banks,
                             uint8_t itcm_banks)
{
    uint32_t total = (uint32_t)ocram_banks + (uint32_t)dtcm_banks +
                     (uint32_t)itcm_banks;

    /* Over-subscribed: return the all-unused word rather than a truncated
     * split.  A partition that assigns no banks is inert and obviously wrong;
     * one that silently drops the last few is neither. */
    if (total > NP_FLEXRAM_BANK_COUNT) {
        return 0UL;
    }

    uint32_t cfg = 0UL;

    for (uint32_t i = 0U; i < NP_FLEXRAM_BANK_COUNT; i++) {
        uint32_t code;

        if (i < (uint32_t)ocram_banks) {
            code = NP_FLEXRAM_BANK_OCRAM;
        } else if (i < ((uint32_t)ocram_banks + (uint32_t)dtcm_banks)) {
            code = NP_FLEXRAM_BANK_DTCM;
        } else if (i < total) {
            code = NP_FLEXRAM_BANK_ITCM;
        } else {
            code = NP_FLEXRAM_BANK_UNUSED;
        }

        cfg |= code << (i * 2U);
    }

    return cfg;
}

uint32_t np_flexram_tcm_size_code(uint32_t size_bytes)
{
    /* A zero-size window is code 0, and is how a TCM interface that carries
     * nothing is expressed — NeurOne's ITCM. */
    if (size_bytes == 0U) {
        return 0UL;
    }

    /* size = 2^(code-1) KiB, code 3..10 → 4 KiB .. 512 KiB.  Walking the
     * legal sizes rather than computing a logarithm rejects every illegal
     * input (non-power-of-two, 1 KiB, 2 KiB, > 512 KiB) by construction
     * instead of by a separate range check that could disagree. */
    uint32_t size = 4U * 1024U;

    for (uint32_t code = 3U; code <= 10U; code++) {
        if (size_bytes == size) {
            return code;
        }
        size *= 2U;
    }

    return NP_FLEXRAM_TCM_SIZE_INVALID;
}

#ifndef NP_FLEXRAM_HOST_TEST

void np_flexram_apply_partition(void)
{
    /* ── Stage: bank assignment ──────────────────────────────────────────── */
    /* Not yet in force — GPR16's FLEXRAM_BANK_CFG_SEL below is what makes the
     * controller read this word instead of the eFuse value. */
    NP_IOMUXC_GPR17 = np_flexram_bank_cfg((uint8_t)NP_FLEXRAM_OCRAM_BANKS,
                                          (uint8_t)NP_FLEXRAM_DTCM_BANKS,
                                          (uint8_t)NP_FLEXRAM_ITCM_BANKS);

    /* ── Stage: TCM window sizes ─────────────────────────────────────────── */
    /* Read-modify-write: GPR14 carries the four ACMP trim/sync fields in its
     * low bits, which are none of this module's business. */
    uint32_t gpr14 = NP_IOMUXC_GPR14;
    gpr14 &= ~(uint32_t)(NP_GPR14_CFGITCMSZ_MASK | NP_GPR14_CFGDTCMSZ_MASK);
    gpr14 |= (np_flexram_tcm_size_code(NP_ITCM_SIZE) << NP_GPR14_CFGITCMSZ_SHIFT)
             & NP_GPR14_CFGITCMSZ_MASK;
    gpr14 |= (np_flexram_tcm_size_code(NP_DTCM_SIZE) << NP_GPR14_CFGDTCMSZ_SHIFT)
             & NP_GPR14_CFGDTCMSZ_MASK;
    NP_IOMUXC_GPR14 = gpr14;

    /* ── Commit ──────────────────────────────────────────────────────────── */
    /* Read-modify-write again, and for a sharper reason: GPR16 bits 31:7 are
     * CM7_INIT_VTOR.  A blind store would clear the reset vector-table address
     * the ROM left there, and the symptom would appear at the NEXT reset, in a
     * different image, with nothing to connect it to this write.
     *
     * INIT_ITCM_EN is cleared rather than left alone: the partition gives ITCM
     * zero banks, so an enabled ITCM interface would be a 0 KiB window that a
     * stray instruction fetch could still address. */
    uint32_t gpr16 = NP_IOMUXC_GPR16;
    gpr16 &= ~(uint32_t)(NP_GPR16_INIT_ITCM_EN | NP_GPR16_INIT_DTCM_EN);
    if (NP_FLEXRAM_ITCM_BANKS > 0U) {
        gpr16 |= NP_GPR16_INIT_ITCM_EN;
    }
    if (NP_FLEXRAM_DTCM_BANKS > 0U) {
        gpr16 |= NP_GPR16_INIT_DTCM_EN;
    }
    gpr16 |= NP_GPR16_FLEXRAM_BANK_CFG_SEL;
    NP_IOMUXC_GPR16 = gpr16;

    /* The memory map has just changed underneath the core.  Drain the writes
     * and flush the pipeline before anything can issue an access against the
     * old one — the same barrier pair np_main.c uses after moving VTOR. */
    __asm volatile("dsb" ::: "memory");
    __asm volatile("isb" ::: "memory");
}

#endif /* NP_FLEXRAM_HOST_TEST */
