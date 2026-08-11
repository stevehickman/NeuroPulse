/*
 * NeurOne Safety MCU — SW-01 Platform Layer: clock tree and 1 ms timebase
 * Target: STM32G071 (Cortex-M0+, 64 MHz), bare metal, CMSIS registers only
 * Document: NP-SW-001 Rev A — SW-01 Class C; NP-SW-CI-001 §4.4 (OI-SWCI-17)
 *
 * Implements np_hal_clock_init(), np_hal_systick_init() and
 * np_hal_get_tick_ms() from np_safety_hal.h, plus SysTick_Handler.
 *
 * ── WHERE CLOCK INIT IS CALLED FROM (resolves OI-SWCI-19) ────────────────────
 * startup_stm32g071xx.s copies .data, zeroes .bss (skipping .fault_latch) and
 * branches straight to main().  It does NOT call SystemInit, and its header
 * comment claiming otherwise was wrong — the code was the record (OI-SWCI-19).
 *
 * Phase 7 resolves that item in the direction the existing code already chose:
 * clock bring-up stays a C function called from main(), and the stale four
 * words come out of the assembler comment.  np_safety_main.c already calls
 * np_hal_clock_init() as its first statement, before every other HAL call and
 * before every module init, which is exactly what np_safety_hal.h specifies
 * ("Called once, first, from main() before any other HAL call").
 *
 * The rejected alternative was adding a SystemInit call to Reset_Handler and
 * vendoring ST's system_stm32g0xx.c.  Rejected because (a) it would add a SOUP
 * translation unit to a Class C image for one function this file already
 * implements in twenty lines, against the OI-SWCI-06 decision to vendor headers
 * only, and (b) ST's SystemInit does not configure the PLL anyway — it would
 * leave the core at HSI16 and the 64 MHz claim unmet, so it would be ceremony
 * with no effect.  One call site, in C, in the file that owns the clock.
 *
 * CONSEQUENCE, stated rather than buried: between reset and np_hal_clock_init()
 * the core runs at HSI16 = 16 MHz.  Nothing in that window is timing-dependent
 * — it is .data copy, .bss zero, and the branch to main — but any future code
 * added to Reset_Handler would run at 16 MHz, not 64 MHz.
 *
 * ── 64 MHz FROM HSI16 ────────────────────────────────────────────────────────
 *   HSI16 = 16 MHz  →  PLLM = /1  →  16 MHz PLL input   (must be 2.66–16 MHz)
 *                   →  PLLN = ×8  →  128 MHz VCO        (must be 64–344 MHz)
 *                   →  PLLR = /2  →  64 MHz PLLRCLK     (must be ≤ 64 MHz)
 * = NP_SAFETY_SYSCLK_HZ exactly, which is asserted at compile time below rather
 * than left as a comment that could drift from the divider constants.
 *
 * Flash latency must be raised BEFORE the switch: at VOS range 1 the STM32G0
 * needs 2 wait states above 48 MHz.  Raising latency first and lowering it last
 * is the ordering rule for any frequency increase; getting it backwards is a
 * hard fault on the first flash fetch at the new speed.
 *
 * IEC 62304 Class C — MISRA C:2012.  C11, no GNU extensions.
 */

#include "np_hal_internal.h"

/* PLL dividers.  Named so the compile-time assertion below can check them
 * against NP_SAFETY_SYSCLK_HZ instead of trusting the arithmetic in a comment. */
#define NP_HAL_HSI_HZ    16000000UL
#define NP_HAL_PLL_M     1U      /* PLLM field is (M-1) */
#define NP_HAL_PLL_N     8U
#define NP_HAL_PLL_R     2U      /* PLLR field is (R-1) */

/* If a divider is ever retuned, this fails the Class C build rather than
 * silently running the watchdog, the cardiac window and the lockout at a
 * rescaled millisecond.  np_safety_hal.h calls the ms unit "load-bearing for
 * three Class C timings"; this is that statement made executable. */
_Static_assert(((NP_HAL_HSI_HZ / NP_HAL_PLL_M) * NP_HAL_PLL_N) / NP_HAL_PLL_R
                   == NP_SAFETY_SYSCLK_HZ,
               "PLL dividers do not produce NP_SAFETY_SYSCLK_HZ");

/* SysTick is a 24-bit down-counter; a reload above 0xFFFFFF silently truncates
 * and the whole timebase is wrong by an unpredictable factor. */
#define NP_HAL_SYSTICK_RELOAD  ((NP_SAFETY_SYSCLK_HZ / NP_SAFETY_SYSTICK_HZ) - 1UL)

_Static_assert(NP_HAL_SYSTICK_RELOAD <= 0x00FFFFFFUL,
               "SysTick reload exceeds the 24-bit counter");

/* Bounded spin budget for each clock-tree ready flag.  A Class C main loop must
 * not contain an unbounded wait, and "PLL never locks" is a real silicon/board
 * failure, not a theoretical one.  On timeout the function returns with the
 * core still on HSI16: everything keeps working at 16 MHz with a timebase that
 * is 4× slow, which is why the condition is also latched for the caller.
 *
 * 200000 iterations of a 3-instruction loop at 16 MHz is ≳ 37 ms, far beyond
 * the few µs an HSI-sourced PLL needs to lock. */
#define NP_HAL_CLK_SPIN_MAX  200000UL

/* Set when any clock-tree wait timed out.  Read by np_hal_clock_degraded(),
 * which np_hal_systick_init() uses to pick the correct reload for the clock the
 * core is ACTUALLY running on — a 4×-slow millisecond would stretch the 1.5 s
 * SPI watchdog to 6 s and the <100 ms cardiac cutoff to 400 ms, which is a
 * safety-relevant miss, not a performance one. */
static bool s_clock_degraded = false;

static bool np_hal_wait_flag(const volatile uint32_t *reg, uint32_t mask, bool want_set)
{
    uint32_t spins = 0U;

    for (;;) {
        bool is_set = ((*reg & mask) != 0U);
        if (is_set == want_set) {
            return true;
        }
        spins++;
        if (spins >= NP_HAL_CLK_SPIN_MAX) {
            s_clock_degraded = true;
            return false;
        }
    }
}

bool np_hal_clock_degraded(void);
bool np_hal_clock_degraded(void)
{
    return s_clock_degraded;
}

void np_hal_clock_init(void)
{
    /* 1. Flash latency FIRST — 2 wait states for 48 < SYSCLK ≤ 64 MHz. */
    uint32_t acr = FLASH->ACR;
    acr &= ~FLASH_ACR_LATENCY;
    acr |= FLASH_ACR_LATENCY_1;              /* LATENCY = 0b010 = 2 WS */
    FLASH->ACR = acr;
    /* Read back until it takes: the reference manual requires confirming the
     * new latency before raising the clock, because the write is not immediate. */
    (void)np_hal_wait_flag(&FLASH->ACR, FLASH_ACR_LATENCY_1, true);

    /* 2. HSI16 on and stable.  It is the reset default, so this normally
     *    returns immediately; it is written out because "normally" is not a
     *    guarantee after a warm reset from an unknown clock state. */
    RCC->CR |= RCC_CR_HSION;
    (void)np_hal_wait_flag(&RCC->CR, RCC_CR_HSIRDY, true);

    /* 3. PLL must be OFF before PLLCFGR is writable. */
    RCC->CR &= ~RCC_CR_PLLON;
    (void)np_hal_wait_flag(&RCC->CR, RCC_CR_PLLRDY, false);

    /* 4. PLL: source HSI16, /M ×N /R, R output enabled. */
    RCC->PLLCFGR =
        RCC_PLLCFGR_PLLSRC_HSI
        | (((uint32_t)NP_HAL_PLL_M - 1UL) << RCC_PLLCFGR_PLLM_Pos)
        | (((uint32_t)NP_HAL_PLL_N)       << RCC_PLLCFGR_PLLN_Pos)
        | (((uint32_t)NP_HAL_PLL_R - 1UL) << RCC_PLLCFGR_PLLR_Pos)
        | RCC_PLLCFGR_PLLREN;

    /* 5. Start it and wait for lock. */
    RCC->CR |= RCC_CR_PLLON;
    if (!np_hal_wait_flag(&RCC->CR, RCC_CR_PLLRDY, true)) {
        /* PLL never locked.  Stay on HSI16 rather than switching to a clock
         * that is not running — switching to an unlocked PLL stops the core
         * dead, which would take the stimulation-enable owner offline entirely.
         * Degraded and alive beats stopped: the enable lines are held HIGH by
         * their external pull-ups either way, but only a running core can keep
         * servicing the watchdog and the interlocks. */
        return;
    }

    /* 6. Switch SYSCLK to PLLRCLK and confirm the switch actually happened. */
    {
        uint32_t cfgr = RCC->CFGR;
        cfgr &= ~RCC_CFGR_SW;
        cfgr |= RCC_CFGR_SW_1;               /* SW = 0b010 = PLLRCLK */
        RCC->CFGR = cfgr;
    }
    (void)np_hal_wait_flag(&RCC->CFGR, RCC_CFGR_SWS_1, true);
}

/* ── 1 ms timebase ────────────────────────────────────────────────────────────
 *
 * ATOMICITY — answers np_safety_hal.h open question 2.
 *
 * The header records that Cortex-M0+ has no LDREX/STREX and asks whether the
 * counter read is torn-read-safe, noting that a torn read of the watchdog
 * timebase is a spurious or suppressed stimulation cutoff.
 *
 * It is safe, and for an architectural reason rather than a lucky one: ARMv6-M
 * guarantees single-copy atomicity for naturally-aligned 32-bit loads and
 * stores.  s_tick_ms is a naturally-aligned uint32_t, the handler's only
 * operation on it is a single aligned store, and the reader's only operation is
 * a single aligned load.  Neither can be observed half-updated.  LDREX/STREX
 * would be needed for a read-modify-write shared between contexts; there is
 * none here, because the increment happens only in the handler.
 *
 * That is why the counter is a plain uint32_t and not, say, a 64-bit tick: a
 * 64-bit counter WOULD tear on this core, and would need an interrupt mask or a
 * seqlock around every read.  The 49.7-day wrap that the 32-bit choice accepts
 * is handled by every caller taking unsigned differences (`now - then`), which
 * np_safety_hal.h requires and which all five call sites do.
 */
static volatile uint32_t s_tick_ms = 0U;

void SysTick_Handler(void);
void SysTick_Handler(void)
{
    /* Single aligned 32-bit store — see the atomicity note above. */
    s_tick_ms = s_tick_ms + 1U;
}

void np_hal_systick_init(void)
{
    /* If the PLL never locked the core is on HSI16, so the 64 MHz reload would
     * produce a 4 ms "millisecond" and stretch every Class C timeout by 4×.
     * Pick the reload for the clock the core is actually running on. */
    uint32_t reload = s_clock_degraded
                          ? ((NP_HAL_HSI_HZ / NP_SAFETY_SYSTICK_HZ) - 1UL)
                          : NP_HAL_SYSTICK_RELOAD;

    SysTick->CTRL = 0UL;                 /* stop before reconfiguring */
    SysTick->LOAD = reload;
    SysTick->VAL  = 0UL;                 /* clears COUNTFLAG as a side effect */

    /* CLKSOURCE=processor clock, TICKINT=1, ENABLE=1.  SysTick's exception
     * priority is left at reset (lowest): the tick must never pre-empt the SPI
     * or R-peak handlers, and a tick delayed by a few µs is invisible at 1 ms
     * resolution while a delayed R-peak capture is a wrong BPM. */
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk
                    | SysTick_CTRL_TICKINT_Msk
                    | SysTick_CTRL_ENABLE_Msk;
}

uint32_t np_hal_get_tick_ms(void)
{
    return s_tick_ms;
}
