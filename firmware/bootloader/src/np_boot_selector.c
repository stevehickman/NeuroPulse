/*
 * NeurOne Bootloader — Boot Bank Selector (SNVS_LPGPR0)
 * Document: NP-FW-EMMC-001 Rev A §8.2
 */

#include "np_boot_selector.h"
#include "np_config.h"

/* ── Internal helpers ─────────────────────────────────────────────────────── */

static inline uint32_t snvs_read(void)
{
    return NP_SNVS_LPGPR0;
}

static inline void snvs_write(uint32_t val)
{
    NP_SNVS_LPGPR0 = val;
    /* Read back to ensure the peripheral write completes before proceeding.  */
    (void)NP_SNVS_LPGPR0;
}

/* ── Public API ───────────────────────────────────────────────────────────── */

np_bank_t np_boot_selector_get_bank(void)
{
    uint32_t reg = snvs_read();
    return ((reg & NP_SNVS_BANK_MASK) == NP_SNVS_BANK_B) ? NP_BANK_B : NP_BANK_A;
}

np_bank_t np_boot_selector_other_bank(np_bank_t bank)
{
    return (bank == NP_BANK_A) ? NP_BANK_B : NP_BANK_A;
}

uint8_t np_boot_selector_get_attempts(void)
{
    uint32_t reg = snvs_read();
    return (uint8_t)((reg & NP_SNVS_ATTEMPTS_MASK) >> NP_SNVS_ATTEMPTS_SHIFT);
}

void np_boot_selector_inc_attempts(void)
{
    uint32_t reg      = snvs_read();
    uint8_t  attempts = (uint8_t)((reg & NP_SNVS_ATTEMPTS_MASK) >> NP_SNVS_ATTEMPTS_SHIFT);

    /* Clamp at NP_BOOT_MAX_ATTEMPTS so the field never overflows its 6 bits. */
    if (attempts < NP_BOOT_MAX_ATTEMPTS) {
        attempts++;
    }
    reg &= ~NP_SNVS_ATTEMPTS_MASK;
    reg |= ((uint32_t)attempts << NP_SNVS_ATTEMPTS_SHIFT) & NP_SNVS_ATTEMPTS_MASK;
    snvs_write(reg);
}

void np_boot_selector_clear_ota_pending(void)
{
    uint32_t reg = snvs_read();
    reg &= ~(NP_SNVS_OTA_PENDING | NP_SNVS_ATTEMPTS_MASK);
    snvs_write(reg);
}

void np_boot_selector_stage_ota(np_bank_t target)
{
    uint32_t reg = snvs_read();

    /* Set target bank */
    reg &= ~NP_SNVS_BANK_MASK;
    reg |= (target == NP_BANK_B) ? NP_SNVS_BANK_B : NP_SNVS_BANK_A;

    /* Clear attempt counter */
    reg &= ~NP_SNVS_ATTEMPTS_MASK;

    /* Set OTA_PENDING */
    reg |= NP_SNVS_OTA_PENDING;

    snvs_write(reg);
}

void np_boot_selector_confirm_boot(void)
{
    /* Clear OTA_PENDING and attempt counter; BOOT_BANK is already the new bank. */
    uint32_t reg = snvs_read();
    reg &= ~(NP_SNVS_OTA_PENDING | NP_SNVS_ATTEMPTS_MASK);
    snvs_write(reg);
}

bool np_boot_selector_ota_pending(void)
{
    return (snvs_read() & NP_SNVS_OTA_PENDING) != 0U;
}

bool np_boot_selector_dfu_forced(void)
{
    return (snvs_read() & NP_SNVS_DFU_FORCED) != 0U;
}

void np_boot_selector_clear_dfu_forced(void)
{
    uint32_t reg = snvs_read();
    reg &= ~NP_SNVS_DFU_FORCED;
    snvs_write(reg);
}

void np_boot_selector_rollback(np_bank_t stable_bank)
{
    uint32_t reg = snvs_read();

    /* Switch bank back to stable */
    reg &= ~NP_SNVS_BANK_MASK;
    reg |= (stable_bank == NP_BANK_B) ? NP_SNVS_BANK_B : NP_SNVS_BANK_A;

    /* Clear OTA_PENDING and attempt counter */
    reg &= ~(NP_SNVS_OTA_PENDING | NP_SNVS_ATTEMPTS_MASK);

    snvs_write(reg);
}
