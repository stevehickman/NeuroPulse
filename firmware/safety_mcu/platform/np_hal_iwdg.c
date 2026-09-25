/*
 * NeurOne Safety MCU — SW-01 Platform Layer: independent watchdog (IWDG)
 * Target: STM32G071 (Cortex-M0+, 64 MHz), bare metal, CMSIS registers only
 * Document: NP-FMEA-001 FMEA-M02-02/-04/-05; NP-RISK-002 OI-RISK2-05;
 *           RM0444 §28 (IWDG)
 *
 * Implements np_hal_iwdg_start() and np_hal_iwdg_refresh().
 *
 * WHY THIS EXISTS.  Every safety action on this MCU — the heartbeat-loss cutoff,
 * each interlock, the pin writer, the vault-feed cut — runs in one main polling
 * loop.  A hung loop runs none of them, and every enable pin holds its last
 * state, which may be ENABLED.  Until this driver, NP-FMEA-001 scored three rows
 * on an IWDG the firmware never configured.  The IWDG runs from the LSI,
 * independent of the system clock, and once started cannot be stopped except by
 * a reset; a reset re-enters the boot path, where the external pull-ups hold
 * every active-LOW enable disabled and np_hal_gpio_init() presets them HIGH.
 *
 * TIMEOUT.  t = 4 × 2^PR × (RLR + 1) / f_LSI.  PR = 3 (÷32) gives 1 ms per count
 * at the nominal 32 kHz, so RLR = NP_SAFETY_IWDG_TIMEOUT_MS − 1.
 *
 * NOT DONE HERE, deliberately: the window option (WINR) and the debug freeze
 * (DBG_APB_FZ1_DBG_IWDG_STOP).  A window needs a measured minimum loop time, and
 * a debug freeze is a bench-configuration decision, not firmware behaviour.
 *
 * IEC 62304 Class C — MISRA C:2012.  C11, no GNU extensions.
 */

#include "np_hal_internal.h"

#define NP_IWDG_KEY_START    0xCCCCUL   /* enable the IWDG (and the LSI)      */
#define NP_IWDG_KEY_ACCESS   0x5555UL   /* unlock PR / RLR                    */
#define NP_IWDG_KEY_REFRESH  0xAAAAUL   /* reload the down-counter            */

#define NP_IWDG_PR_DIV32     3UL        /* ÷32 → 1 ms per count at 32 kHz     */
#define NP_IWDG_RLR_MAX      0x0FFFUL

/* PVU/RVU clear within ~5 LSI cycles (~160 µs); this cap is many times that at
 * 64 MHz and exists only so the call is bounded (np_safety_hal.h question 1). */
#define NP_IWDG_UPDATE_SPIN_MAX  100000UL

_Static_assert(NP_SAFETY_IWDG_TIMEOUT_MS >= 1U &&
               (NP_SAFETY_IWDG_TIMEOUT_MS - 1U) <= NP_IWDG_RLR_MAX,
               "IWDG timeout does not fit RLR at the ÷32 prescaler");
_Static_assert(NP_SAFETY_IWDG_TIMEOUT_MS <= NP_SAFETY_WDG_TIMEOUT_MS,
               "the IWDG backstop must not be slower than the heartbeat watchdog");

void np_hal_iwdg_start(void)
{
    uint32_t spin = 0UL;

    IWDG->KR  = NP_IWDG_KEY_START;
    IWDG->KR  = NP_IWDG_KEY_ACCESS;
    IWDG->PR  = NP_IWDG_PR_DIV32;
    IWDG->RLR = (uint32_t)NP_SAFETY_IWDG_TIMEOUT_MS - 1UL;

    while ((IWDG->SR != 0UL) && (spin < NP_IWDG_UPDATE_SPIN_MAX)) {
        spin++;
    }

    IWDG->KR = NP_IWDG_KEY_REFRESH;
}

void np_hal_iwdg_refresh(void)
{
    IWDG->KR = NP_IWDG_KEY_REFRESH;
}
