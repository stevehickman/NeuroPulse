/*
 * NeurOne Safety MCU — SW-01 Platform Layer: stimulation enable GPIO
 * Target: STM32G071 (Cortex-M0+, 64 MHz), bare metal, CMSIS registers only
 * Document: NP-SW-001 Rev 1 — SW-01 Class C; NP-SW-CI-001 §4.4 (OI-SWCI-17)
 *           CLAUDE.md §4.2 (safety MCU owns all stimulation enable GPIO)
 *
 * Implements np_hal_gpio_init() and np_hal_gpio_write_pin() from
 * np_safety_hal.h.  This is the most safety-critical file in the platform
 * layer: np_gpio_mgr.c is the only module that calls into it, and between them
 * they are the entire path from a granted_mask bit to a stimulation driver.
 *
 * ── POLARITY (np_safety_hal.h, np_safety_config.h) ───────────────────────────
 * All ten enable lines are ACTIVE-LOW open-drain with external pull-ups:
 *
 *     state 1  →  pin HIGH  →  stimulation DISABLED  (safe)
 *     state 0  →  pin LOW   →  stimulation ENABLED
 *
 * Inverting this enables every stimulation channel at reset.  It is checked
 * behaviourally, not by review: np_hal_platform_tests.c compiles THIS FILE
 * against a fake register file and asserts on BSRR for both states.
 *
 * ── THE RESET WINDOW (answers np_safety_hal.h open question 10) ──────────────
 * np_safety_hal.h documents np_hal_gpio_init() as configuring "direction ONLY —
 * it does not set output levels", with np_gpio_mgr_init() setting the disabled
 * state immediately afterwards.  Implemented literally, that contract has a
 * hazard the header itself flags: between the MODER write and np_gpio_mgr_init()
 * the enable lines are held disabled "by external pull-ups alone".
 *
 * That is not what happens.  At reset ODR is 0.  Switching MODER to output with
 * ODR still 0 drives the pin LOW — which on an active-LOW line is a hard
 * ENABLE assertion on all ten channels, lasting until np_gpio_mgr_init() runs.
 * The external pull-up cannot save it: an open-drain output with ODR=0 sinks
 * against the pull-up and wins.  The window is short but it is a real,
 * unconditional, all-channel stimulation enable at every power-on.
 *
 * RESOLUTION: this driver writes BSRR to preset every enable pin HIGH BEFORE
 * switching its MODER to output.  With ODR=1 an open-drain pin is high-Z and
 * the external pull-up holds it HIGH, so the line is disabled continuously from
 * reset through to np_gpio_mgr_init() and there is no window at all.
 *
 * This is a deliberate, reviewed deviation from the letter of the header
 * ("does not set output levels").  It does not change the LOGICAL state the
 * header describes — disabled before, disabled after, disabled throughout — and
 * np_gpio_mgr_init() still writes all ten explicitly, so nothing downstream
 * depends on it.  np_safety_hal.h has been updated to record it.  The
 * alternative reading is conservative in wording and unsafe in silicon.
 *
 * ── WHAT THIS FILE STILL CANNOT ANSWER ───────────────────────────────────────
 * The external pull-up VALUE and the pad's power-on-reset high-Z window are
 * hardware facts recorded in no software document here (np_safety_hal.h open
 * question 10, second half).  This driver removes the FIRMWARE-created window;
 * it cannot remove the silicon one between the power rail rising and the first
 * instruction, which is a pull-up sizing question for the PCB review.  Recorded
 * as OI-SWCI-27.  No claim of bench validation is made anywhere in this file.
 *
 * IEC 62304 Class C — MISRA C:2012.  C11, no GNU extensions.
 */

#include "np_hal_internal.h"

/* The ten stimulation enable lines, in np_gpio_mgr.c's order.
 *
 * A TABLE, not ten inline statements, and specifically so that "configure every
 * enable line" and "preset every enable line HIGH" are literally the same list
 * traversed twice.  Two hand-maintained lists of ten pins that must agree is
 * the Defect C shape; the failure mode is a pin that gets configured as an
 * output but never preset, i.e. exactly the reset-window hazard above, on one
 * channel, invisibly.
 *
 * NOT const-qualified away into np_safety_config.h: the config header is shared
 * with host tests that must not see GPIO_TypeDef (np_safety_hal.h
 * SELF-CONTAINMENT note).  The port fields here are void* for the same reason
 * np_hal_gpio_write_pin's parameter is.
 */
typedef struct {
    void    *port;
    uint16_t pin;
} np_hal_en_line_t;

#define NP_HAL_EN_LINE_COUNT  10U

static const np_hal_en_line_t k_en_lines[NP_HAL_EN_LINE_COUNT] = {
    { NP_EN_PBM_CRANIAL_PORT, NP_EN_PBM_CRANIAL_PIN },
    { NP_EN_BES_PORT,         NP_EN_BES_PIN         },
    { NP_EN_TDCS_PORT,        NP_EN_TDCS_PIN        },
    { NP_EN_VNS_PORT,         NP_EN_VNS_PIN         },
    { NP_EN_VISUAL_PORT,      NP_EN_VISUAL_PIN      },
    { NP_EN_INTRANASAL_PORT,  NP_EN_INTRANASAL_PIN  },
    { NP_EN_CVNS_PORT,        NP_EN_CVNS_PIN        },
    { NP_EN_TMS_PORT,         NP_EN_TMS_PIN         },
    { NP_EN_PBM_1170_PORT,    NP_EN_PBM_1170_PIN    },
    { NP_EN_CLIN_STIM_PORT,   NP_EN_CLIN_STIM_PIN   },
};

void np_hal_gpio_init(void)
{
    uint8_t i;

    np_hal_port_clocks_enable();

    for (i = 0U; i < NP_HAL_EN_LINE_COUNT; i++) {
        GPIO_TypeDef *p = (GPIO_TypeDef *)k_en_lines[i].port;

        /* ORDER IS THE SAFETY PROPERTY — see the file header.
         *
         * 1. BSRR set  → ODR = 1.  Harmless while the pin is still in its reset
         *                analog/high-Z mode; it only pre-loads the level.
         * 2. MODER      → output open-drain.  The pin becomes high-Z (not
         *                driven LOW), and the external pull-up holds it HIGH =
         *                DISABLED from this instant onward.
         *
         * Reversing these two steps asserts every stimulation enable for the
         * duration of the loop.  Do not "simplify" this by configuring first. */
        p->BSRR = (uint32_t)k_en_lines[i].pin;

        np_hal_pin_config_output_od(k_en_lines[i].port, k_en_lines[i].pin);
    }
}

void np_hal_gpio_write_pin(void *port, uint16_t pin, int state)
{
    GPIO_TypeDef *p = (GPIO_TypeDef *)port;

    if (p == NULL) {
        return;
    }

    /* `state` DOMAIN — answers np_safety_hal.h open question 6.
     *
     * The header records that every caller passes exactly 0 or 1 and that
     * whether any non-zero value means HIGH was unstated.  Defined here as:
     *
     *     state == 0   → LOW  → ENABLED
     *     state != 0   → HIGH → DISABLED
     *
     * "Non-zero is HIGH" is the safe half of the ambiguity: on an active-LOW
     * line, HIGH is the disabled state, so an unexpected value (a corrupted
     * boolean, a widened enum, a future caller passing 2) fails toward
     * stimulation OFF.  The opposite convention — "only 1 is HIGH" — would make
     * every unexpected value an ENABLE.  Per the conservative-default rule this
     * is not a coin flip; there is a correct side and this is it.
     *
     * BSRR is used rather than read-modify-write on ODR because BSRR is atomic
     * in one store.  np_gpio_mgr_apply() runs from the main loop while SysTick
     * and EXTI4_15 interrupts are live; an ODR read-modify-write interrupted
     * between the read and the write could publish a stale mask for the other
     * nine lines.  On ARMv6-M there is no LDREX/STREX to fix that afterwards,
     * so the atomic register is not an optimisation — it is the only correct
     * option available on this core.
     *
     * BSRR layout: bits [15:0] SET (drive HIGH), bits [31:16] RESET (drive LOW).
     */
    if (state != 0) {
        p->BSRR = (uint32_t)pin;                    /* HIGH → DISABLED */
    } else {
        p->BSRR = ((uint32_t)pin) << 16;            /* LOW  → ENABLED  */
    }
}
