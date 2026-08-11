/*
 * NeurOne Safety MCU — SW-01 Platform Layer, internal helpers
 * Target: STM32G071 (Cortex-M0+, 64 MHz), bare metal, CMSIS registers only
 * Document: NP-SW-001 Rev A — SW-01 Class C; NP-SW-CI-001 §4.4 (Defect E, OI-SWCI-17)
 *
 * PRIVATE TO firmware/safety_mcu/platform/.  Nothing in src/ or tests/ includes
 * this file, and nothing here is part of the SW-01 platform contract — that is
 * np_safety_hal.h, which stays declaration-only and CMSIS-free so the six host
 * tests can build natively.  This header is the opposite: it is CMSIS-bound on
 * purpose, and it exists so seven driver translation units share one correct
 * implementation of "configure a pin" instead of seven hand-rolled copies of the
 * same MODER/OTYPER/PUPDR/AFR arithmetic.  That is the Defect C shape (one value,
 * several places) and it is avoided here rather than repaired later.
 *
 * ── HOST-TESTABILITY (NP_HAL_HOST_TEST) ──────────────────────────────────────
 * Defining NP_HAL_HOST_TEST substitutes a plain-C fake register file for the
 * CMSIS device header, so a host test can compile the REAL driver sources and
 * assert on the register writes they perform.  This is deliberate and it is the
 * strongest evidence available before silicon exists: the alternative — reading
 * the driver and agreeing with it — is exactly the "prose is a design-review
 * artifact, not a diagnostic" failure np_safety_hal.h warns about.
 *
 * The fake is TEST-ONLY.  It is never compiled into the firmware image: the
 * cross build defines STM32G071xx and never defines NP_HAL_HOST_TEST, and the
 * two are mutually exclusive by the #error below.
 *
 * IEC 62304 Class C — MISRA C:2012.  C11, no GNU extensions.
 */

#ifndef NP_HAL_INTERNAL_H
#define NP_HAL_INTERNAL_H

#if defined(NP_HAL_HOST_TEST) && defined(STM32G071xx)
#  error "NP_HAL_HOST_TEST and STM32G071xx are mutually exclusive"
#endif

#if defined(NP_HAL_HOST_TEST)
/* Supplies GPIO_TypeDef/RCC/SPI/TIM/ADC/EXTI/FLASH types and the GPIOA/GPIOB
 * instance names that np_safety_config.h's NP_EN_*_PORT macros expand to.
 * Must precede np_safety_config.h only in the sense that it must precede the
 * first USE of those names; macro definition alone needs nothing declared. */
#  include "np_hal_fake_regs.h"
#endif

#include "np_safety_config.h"
#include "np_safety_hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ── Pin geometry ─────────────────────────────────────────────────────────────
 *
 * Every NP_*_PIN macro in np_safety_config.h is a single-bit MASK, `(1U << n)`,
 * not a pin number.  BSRR and IDR want the mask; MODER, OTYPER, OSPEEDR, PUPDR
 * and AFR want the position n.  np_hal_pin_pos() is the one conversion.
 *
 * Implemented as a shift loop rather than __builtin_ctz because Cortex-M0+ is
 * ARMv6-M, which has no CLZ instruction — the builtin would expand to a libgcc
 * call inside pin configuration.  Init-time only; the loop runs at most 16
 * iterations and never executes after np_hal_gpio_init() returns.
 *
 * A zero or multi-bit mask is a programming error in np_safety_config.h, not a
 * runtime condition.  It returns NP_HAL_PIN_POS_INVALID and every caller treats
 * that as "configure nothing", so a malformed macro leaves the pin in its reset
 * state (analog, high-impedance) rather than configuring an arbitrary pin.
 */
#define NP_HAL_PIN_POS_INVALID  0xFFU

uint8_t np_hal_pin_pos(uint16_t pin_mask);

/* ── GPIO configuration primitives ────────────────────────────────────────────
 *
 * All take a single-bit pin mask.  All are idempotent.  None of them changes
 * ODR — output level is the caller's business, which matters enormously for the
 * ten active-LOW enable lines (see np_hal_gpio.c).
 */

/* Pull-up/pull-down selector for np_hal_pin_config_input(). */
#define NP_HAL_PUPD_NONE   0U
#define NP_HAL_PUPD_UP     1U
#define NP_HAL_PUPD_DOWN   2U

void np_hal_port_clocks_enable(void);

/* Open-drain output, no internal pull.  The ten stimulation enable lines are
 * open-drain with EXTERNAL pull-ups to Vcc (np_safety_config.h header), so an
 * internal pull would fight the external network and change the rise time. */
void np_hal_pin_config_output_od(void *port, uint16_t pin_mask);

void np_hal_pin_config_input(void *port, uint16_t pin_mask, uint8_t pupd);
void np_hal_pin_config_analog(void *port, uint16_t pin_mask);
void np_hal_pin_config_af(void *port, uint16_t pin_mask, uint8_t af);

/* Read one pin's live input level.  IDR reflects the pad regardless of MODER,
 * so this is valid for a pin configured as an alternate function — which is how
 * np_hal_spi.c watches SPI1 NSS (PA4) without spending a second pin or an
 * EXTI line on it. */
bool np_hal_pin_read(void *port, uint16_t pin_mask);

/* ── Cross-TU platform hooks ──────────────────────────────────────────────────
 *
 * np_hal_rpeak_isr_hook() is called from the EXTI4_15 interrupt handler, which
 * lives in np_hal_rpeak.c.  Declared here rather than made static so the ISR
 * ownership is visible in one place: EXTI lines 4..15 are a SHARED vector, and
 * PA8 (R-peak) is the only line on it that this firmware enables.  PA4 is SPI1
 * NSS and is deliberately NOT an EXTI source — np_hal_spi.c polls its level
 * instead, so this vector has exactly one owner.  If a second EXTI 4..15 source
 * is ever added, it is added here, not by a second handler definition (which
 * would be a duplicate-symbol link error — the loud outcome).
 */
void np_hal_rpeak_isr_hook(void);

#endif /* NP_HAL_INTERNAL_H */
