/*
 * NeurOne Safety MCU — SW-01 Platform Layer: non-volatile state flash
 * Target: STM32G071 (Cortex-M0+, 64 MHz), bare metal, CMSIS registers only
 * Document: NP-SW-FAULTMSG-001 P1 (OI-FAULTMSG-01); RM0444 §3.3 (flash
 *           program/erase), §3.7 (FLASH_KEYR/SR/CR)
 *
 * Implements the three np_hal_nv_* primitives declared in np_safety_hal.h:
 * a memory-mapped read, a page erase and a double-word program, over the two
 * 2 KB pages NP_NV_FIRST_PAGE .. +NP_NV_PAGE_COUNT-1 at the top of flash.  The
 * record log above them is src/np_nv_state.c; nothing here interprets a record.
 *
 * ── Why this driver is NOT in np_hal_platform_tests ─────────────────────────
 * That suite covers only drivers with no hardware handshake.  Erase and program
 * both wait on FLASH_SR.BSY1/CFGBSY, and a fake that always reports "not busy"
 * would exercise the happy path and skip every timeout and error branch while
 * reading as coverage — the reason clock, ADC and impedance are excluded too.
 * The logic that most needs testing, the fail-closed record decoding and the
 * rotation order, is in np_nv_state.c and is tested there (np_nv_state_tests).
 *
 * ── What this has not had: bench validation ─────────────────────────────────
 * Like every platform driver here (OI-SWCI-27..34), the register sequence is a
 * design-review artifact until it runs on silicon.  In particular: the NMI a
 * double-bit ECC error raises when a power-loss-torn double-word is read
 * (FLASH_ECCR.ECCD) is not handled here.  Recorded under OI-FAULTMSG-01.
 *
 * IEC 62304 Class C — MISRA C:2012.  C11, no GNU extensions.
 */

#include "np_hal_internal.h"

#define NP_HAL_FLASH_PAGE_SIZE   2048UL
#define NP_HAL_FLASH_KEY1        0x45670123UL
#define NP_HAL_FLASH_KEY2        0xCDEF89ABUL

/* Page erase is 40 ms maximum (DS12232).  During it the core stalls on any
 * flash fetch, so this loop mostly does not spin at all; the bound exists so a
 * controller that never clears BSY cannot hang the safety MCU forever.  It is
 * generous: at 64 MHz and ~10 cycles per iteration it is > 300 ms of spinning. */
#define NP_HAL_FLASH_SPIN_MAX    2000000UL

#define NP_HAL_FLASH_SR_ERRORS   (FLASH_SR_OPERR  | FLASH_SR_PROGERR | FLASH_SR_WRPERR | \
                                  FLASH_SR_PGAERR | FLASH_SR_SIZERR  | FLASH_SR_PGSERR | \
                                  FLASH_SR_MISERR | FLASH_SR_FASTERR | FLASH_SR_RDERR  | \
                                  FLASH_SR_OPTVERR)

_Static_assert((NP_NV_SLOTS_PER_PAGE * 8UL) == NP_HAL_FLASH_PAGE_SIZE,
               "an NV page is exactly one flash page of 64-bit slots");

static uint32_t slot_addr(uint8_t page, uint16_t slot)
{
    return FLASH_BASE +
           ((uint32_t)(NP_NV_FIRST_PAGE + (uint32_t)page) * NP_HAL_FLASH_PAGE_SIZE) +
           ((uint32_t)slot * 8UL);
}

static bool args_ok(uint8_t page, uint16_t slot)
{
    return (page < (uint8_t)NP_NV_PAGE_COUNT) && (slot < (uint16_t)NP_NV_SLOTS_PER_PAGE);
}

static bool wait_not_busy(void)
{
    uint32_t spins = 0U;
    while ((FLASH->SR & (FLASH_SR_BSY1 | FLASH_SR_CFGBSY)) != 0U) {
        spins++;
        if (spins >= NP_HAL_FLASH_SPIN_MAX) {
            return false;
        }
    }
    return true;
}

static bool unlock(void)
{
    if ((FLASH->CR & FLASH_CR_LOCK) != 0U) {
        FLASH->KEYR = NP_HAL_FLASH_KEY1;
        FLASH->KEYR = NP_HAL_FLASH_KEY2;
    }
    return (FLASH->CR & FLASH_CR_LOCK) == 0U;
}

static void lock(void)
{
    FLASH->CR |= FLASH_CR_LOCK;
}

/* Common preamble: controller idle, stale error flags cleared (write-1-to-clear),
 * control register unlocked.  RM0444 §3.3.7 / §3.3.8 step 1–2. */
static bool begin(void)
{
    if (!wait_not_busy()) {
        return false;
    }
    FLASH->SR = NP_HAL_FLASH_SR_ERRORS | FLASH_SR_EOP;
    return unlock();
}

void np_hal_nv_read_dword(uint8_t page, uint16_t slot, uint32_t *lo, uint32_t *hi)
{
    if ((lo == NULL) || (hi == NULL)) {
        return;
    }
    if (!args_ok(page, slot)) {
        /* Out-of-range reads as a non-erased, invalid record: the caller's
         * decoding then fails closed rather than seeing "never written". */
        *lo = 0U;
        *hi = 0U;
        return;
    }
    {
        const volatile uint32_t *p = (const volatile uint32_t *)(uintptr_t)slot_addr(page, slot);
        *lo = p[0];
        *hi = p[1];
    }
}

bool np_hal_nv_erase_page(uint8_t page)
{
    bool ok;

    if (page >= (uint8_t)NP_NV_PAGE_COUNT) {
        return false;
    }
    if (!begin()) {
        lock();
        return false;
    }

    FLASH->CR = (FLASH->CR & ~(FLASH_CR_PNB | FLASH_CR_PG | FLASH_CR_MER1)) |
                FLASH_CR_PER |
                (((uint32_t)NP_NV_FIRST_PAGE + (uint32_t)page) << FLASH_CR_PNB_Pos);
    FLASH->CR |= FLASH_CR_STRT;

    ok = wait_not_busy() && ((FLASH->SR & NP_HAL_FLASH_SR_ERRORS) == 0U);

    FLASH->CR &= ~(FLASH_CR_PER | FLASH_CR_PNB);
    lock();
    return ok;
}

bool np_hal_nv_program_dword(uint8_t page, uint16_t slot, uint32_t lo, uint32_t hi)
{
    bool ok;

    if (!args_ok(page, slot)) {
        return false;
    }
    if (!begin()) {
        lock();
        return false;
    }

    FLASH->CR |= FLASH_CR_PG;
    {
        /* RM0444 §3.3.8: write the first word, then the second; programming
         * starts on the second write.  The address is double-word aligned by
         * construction (slot * 8). */
        volatile uint32_t *p = (volatile uint32_t *)(uintptr_t)slot_addr(page, slot);
        p[0] = lo;
        p[1] = hi;
    }

    ok = wait_not_busy() && ((FLASH->SR & NP_HAL_FLASH_SR_ERRORS) == 0U);

    FLASH->CR &= ~FLASH_CR_PG;
    lock();
    return ok;
}
