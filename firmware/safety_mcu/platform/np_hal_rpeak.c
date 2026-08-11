/*
 * NeurOne Safety MCU — SW-01 Platform Layer: TIM2 timebase + R-peak capture
 * Target: STM32G071 (Cortex-M0+, 64 MHz), bare metal, CMSIS registers only
 * Document: NP-SW-001 Rev A — SW-01 Class C; NP-SW-CI-001 §4.4 (OI-SWCI-17)
 *           NP-FW-CVNS-001 Rev B §5; CLAUDE.md §4.2 (cervical VNS cardiac interlock)
 *
 * Implements np_hal_tim2_init(), np_hal_tim2_get_capture(),
 * np_hal_rpeak_edge_pending() and np_hal_rpeak_edge_clear(), plus the
 * EXTI4_15 interrupt handler.
 *
 * np_cardiac_interlock.c converts R-R intervals to BPM assuming each TIM2 count
 * is one microsecond, and cuts the CVNS enable on a >15 BPM change inside a 5 s
 * window.  The 1 MHz rate is therefore contract, not configuration, and it is
 * asserted at compile time below.
 *
 * ── EXTI CAPTURE, NOT TIMER INPUT CAPTURE — and why ──────────────────────────
 * np_safety_hal.h describes TIM2 "with input capture on RPEAK_IN (PA8)".  This
 * driver instead runs TIM2 as a plain free-running 1 MHz counter with NO
 * channel configured, and latches TIM2->CNT in an EXTI rising-edge handler on
 * PA8.
 *
 * The reason is a hardware question this repository cannot answer.  On STM32G0
 * the TIM2 channel inputs are routed to a specific set of pins through the
 * alternate-function matrix, and NOTHING IN THIS TREE RECORDS WHETHER PA8 IS
 * ONE OF THEM — the vendored CMSIS device headers carry register definitions
 * only, never AF tables, so the claim in np_safety_hal.h is unverifiable from
 * anything checked in.  If PA8 has no TIM2 route, a timer-input-capture driver
 * would configure an alternate function that silently never fires, and the
 * cardiac interlock would sit pre-baseline forever — which np_cardiac_interlock.c
 * treats as a conservative hold, so the failure would look like a working
 * safe-state rather than a dead input.
 *
 * The EXTI approach is correct under BOTH readings: every GPIO on the part can
 * drive an EXTI line, so this driver does not depend on the AF question being
 * resolved one way or the other.  Resolving it is a PCB-review item, recorded
 * as OI-SWCI-29, and if PA8 does turn out to have a TIM2 route this file can be
 * migrated to true input capture without touching the contract.
 *
 * The cost of EXTI-vs-hardware-capture is interrupt latency between the pin
 * edge and the CNT read: worst case a few tens of core cycles plus any higher-
 * or equal-priority handler in flight, so sub-microsecond at 64 MHz.  Against
 * R-R intervals of ~500 000–1 200 000 µs that is below one part in 10^5, i.e.
 * far under the ±1 BPM quantisation the interlock already tolerates.  It is not
 * a free substitution, but it is an immaterial one — and materially better than
 * an AF that may not exist.
 *
 * IEC 62304 Class C — MISRA C:2012.  C11, no GNU extensions.
 */

#include "np_hal_internal.h"

/* 64 MHz / (PSC+1) = 1 MHz. */
#define NP_HAL_TIM2_PSC  ((NP_SAFETY_SYSCLK_HZ / NP_CARDIAC_TIM_HZ) - 1UL)

_Static_assert((NP_SAFETY_SYSCLK_HZ % NP_CARDIAC_TIM_HZ) == 0UL,
               "TIM2 prescaler cannot produce NP_CARDIAC_TIM_HZ exactly");
_Static_assert(NP_HAL_TIM2_PSC <= 0xFFFFUL,
               "TIM2 prescaler exceeds its 16-bit field");

/* PA8 is EXTI line 8, which shares the EXTI4_15 vector. */
#define NP_HAL_RPEAK_EXTI_LINE  NP_RPEAK_IN_PIN     /* (1U << 8) — line == pin */

/* Capture state.  Both are written only by the handler and read only by the
 * main loop, both are naturally-aligned 32-bit / bool, and ARMv6-M guarantees
 * single-copy atomicity for aligned 32-bit access — the same argument set out
 * at length in np_hal_clock.c for s_tick_ms.  No masking is required. */
static volatile uint32_t s_capture_us   = 0U;
static volatile bool     s_edge_pending = false;

void np_hal_tim2_init(void)
{
    RCC->APBENR1 |= RCC_APBENR1_TIM2EN;
    (void)RCC->APBENR1;

    NP_CARDIAC_TIM->CR1  = 0UL;                 /* stop before reconfiguring */
    NP_CARDIAC_TIM->PSC  = NP_HAL_TIM2_PSC;
    /* TIM2 is 32-bit on STM32G0.  Full-scale ARR gives a 4294.97 s wrap; every
     * caller takes unsigned differences, so the wrap is transparent. */
    NP_CARDIAC_TIM->ARR  = 0xFFFFFFFFUL;
    NP_CARDIAC_TIM->CNT  = 0UL;
    NP_CARDIAC_TIM->EGR  = TIM_EGR_UG;          /* latch PSC immediately */
    NP_CARDIAC_TIM->SR   = 0UL;                 /* clear the UG-induced flag */
    NP_CARDIAC_TIM->CR1  = TIM_CR1_CEN;

    /* CAPTURE VALUE BEFORE THE FIRST EDGE — np_safety_hal.h open question 5.
     *
     * The header notes np_cardiac_interlock.c carries an s_first_beat_seen
     * guard "which suggests the author expected garbage, but that is inference,
     * not contract".  Defined here: zero until the first edge.  The module's
     * guard means nothing consumes it before then, so this is belt-and-braces
     * rather than load-bearing — but "0" is a defined value that a future
     * reader can reason about, and uninitialised SRAM is not. */
    s_capture_us   = 0U;
    s_edge_pending = false;

    /* ── R-peak input on PA8 ────────────────────────────────────────────────
     * Pull-DOWN, because NP_RPEAK_PULSE_MS documents an ACTIVE-HIGH 5 ms pulse.
     * A floating input on a cardiac path would capture noise as heartbeats and
     * synthesise R-R intervals; pulling toward the inactive level means a
     * disconnected or un-driven line reads as "no beat", which the interlock
     * treats as a conservative pre-baseline hold. */
    np_hal_port_clocks_enable();
    np_hal_pin_config_input(NP_RPEAK_IN_PORT, NP_RPEAK_IN_PIN, NP_HAL_PUPD_DOWN);

    RCC->APBENR2 |= RCC_APBENR2_SYSCFGEN;
    (void)RCC->APBENR2;

    /* EXTI8 source = port A.  EXTICR[2] covers lines 8..11, one byte per line;
     * port A is 0, which is the reset value, but it is written explicitly
     * because relying on a reset value survives exactly until someone adds a
     * second EXTI user. */
    EXTI->EXTICR[2] = EXTI->EXTICR[2] & ~(0xFFUL << 0);

    EXTI->RTSR1 |=  (uint32_t)NP_HAL_RPEAK_EXTI_LINE;   /* rising edge  */
    EXTI->FTSR1 &= ~(uint32_t)NP_HAL_RPEAK_EXTI_LINE;   /* not falling  */
    EXTI->RPR1   =  (uint32_t)NP_HAL_RPEAK_EXTI_LINE;   /* w1c any stale pend */
    EXTI->IMR1  |=  (uint32_t)NP_HAL_RPEAK_EXTI_LINE;   /* unmask       */

    NVIC_ClearPendingIRQ(EXTI4_15_IRQn);
    NVIC_EnableIRQ(EXTI4_15_IRQn);
}

void np_hal_rpeak_isr_hook(void)
{
    /* Latch the counter FIRST, then publish the flag.  A reader that saw
     * s_edge_pending true before s_capture_us was updated would compute an R-R
     * interval from the PREVIOUS beat's timestamp — a doubled interval, i.e. a
     * halved BPM, i.e. a spurious >15 BPM delta and a spurious cardiac cutoff.
     * Store order is the correctness property here, not merely tidy. */
    s_capture_us   = NP_CARDIAC_TIM->CNT;
    s_edge_pending = true;
}

void EXTI4_15_IRQHandler(void);
void EXTI4_15_IRQHandler(void)
{
    /* Shared vector for EXTI lines 4..15.  PA8 is the only line this firmware
     * enables on it — SPI1 NSS (PA4) is deliberately polled rather than made an
     * EXTI source, so this handler has exactly one owner.  See
     * np_hal_internal.h and np_hal_spi.c. */
    if ((EXTI->RPR1 & (uint32_t)NP_HAL_RPEAK_EXTI_LINE) != 0U) {
        EXTI->RPR1 = (uint32_t)NP_HAL_RPEAK_EXTI_LINE;   /* w1c before the read */
        np_hal_rpeak_isr_hook();
    }
}

uint32_t np_hal_tim2_get_capture(void)
{
    return s_capture_us;
}

bool np_hal_rpeak_edge_pending(void)
{
    return s_edge_pending;
}

void np_hal_rpeak_edge_clear(void)
{
    /* np_safety_hal.h requires the caller to read np_hal_tim2_get_capture()
     * BEFORE clearing, and np_cardiac_interlock.c does exactly that.  Clearing
     * only the flag — never the capture — keeps the last timestamp readable, so
     * a caller that clears first still gets the previous beat rather than zero. */
    s_edge_pending = false;
}
