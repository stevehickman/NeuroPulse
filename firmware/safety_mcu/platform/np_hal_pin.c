/*
 * NeurOne Safety MCU — SW-01 Platform Layer: GPIO pin configuration primitives
 * Target: STM32G071 (Cortex-M0+, 64 MHz), bare metal, CMSIS registers only
 * Document: NP-SW-001 Rev A — SW-01 Class C; NP-SW-CI-001 §4.4 (OI-SWCI-17)
 *
 * Not part of the SW-01 platform contract — these are the shared primitives the
 * seven contract-implementing translation units use.  See np_hal_internal.h.
 *
 * IEC 62304 Class C — MISRA C:2012.  C11, no GNU extensions.
 */

#include "np_hal_internal.h"

/* Two bits per pin: MODER, OSPEEDR, PUPDR.  Four bits per pin: AFR[0]/AFR[1]. */
#define NP_HAL_MODE_INPUT   0U
#define NP_HAL_MODE_OUTPUT  1U
#define NP_HAL_MODE_AF      2U
#define NP_HAL_MODE_ANALOG  3U

uint8_t np_hal_pin_pos(uint16_t pin_mask)
{
    uint8_t  pos = 0U;
    uint16_t m   = pin_mask;

    /* Reject zero and multi-bit masks.  Both are malformed np_safety_config.h
     * macros rather than runtime conditions, and returning INVALID makes every
     * caller skip configuration instead of configuring some other pin. */
    if ((m == 0U) || ((m & (uint16_t)(m - 1U)) != 0U)) {
        return NP_HAL_PIN_POS_INVALID;
    }

    while ((m & 1U) == 0U) {
        m = (uint16_t)(m >> 1);
        pos++;
    }
    return pos;
}

void np_hal_port_clocks_enable(void)
{
    /* GPIOA carries the cranial PBM enable (PA0), SPI1 (PA4..PA7) and the
     * R-peak input (PA8).  GPIOB carries the nine remaining enable lines
     * (PB0..PB8).  Both NTC banks live on these two ports as well. */
    RCC->IOPENR |= (RCC_IOPENR_GPIOAEN | RCC_IOPENR_GPIOBEN);

    /* Read back once: on STM32 the peripheral clock enable takes effect after a
     * short delay, and a register write issued in the next instruction can be
     * dropped.  The read is the documented barrier. */
    (void)RCC->IOPENR;
}

/* Set the two-bit field for `pos` in *reg to `val`. */
static void np_hal_set2(volatile uint32_t *reg, uint8_t pos, uint32_t val)
{
    uint32_t shift = (uint32_t)pos * 2U;
    uint32_t v     = *reg;
    v &= ~(3UL << shift);
    v |= (val & 3UL) << shift;
    *reg = v;
}

void np_hal_pin_config_output_od(void *port, uint16_t pin_mask)
{
    GPIO_TypeDef *p   = (GPIO_TypeDef *)port;
    uint8_t       pos = np_hal_pin_pos(pin_mask);

    if ((p == NULL) || (pos == NP_HAL_PIN_POS_INVALID)) {
        return;
    }

    /* Open drain BEFORE output mode.  If MODER became output while OTYPER still
     * said push-pull, the pin would actively drive for the few cycles between
     * the two writes.  On an active-LOW enable line whose ODR is 0 at reset,
     * that is a push-pull LOW — a hard enable assertion.  Ordering here is a
     * safety property, not a style preference. */
    p->OTYPER |= (uint32_t)pin_mask;

    /* No internal pull: the enable network uses external pull-ups. */
    np_hal_set2(&p->PUPDR, pos, NP_HAL_PUPD_NONE);

    /* Low speed.  These lines change at session cadence, not at MHz; the
     * slowest edge that still meets timing is the least EMI into a headset
     * whose whole design premise is measured shielding (CLAUDE.md §4.3). */
    np_hal_set2(&p->OSPEEDR, pos, 0U);

    np_hal_set2(&p->MODER, pos, NP_HAL_MODE_OUTPUT);
}

void np_hal_pin_config_input(void *port, uint16_t pin_mask, uint8_t pupd)
{
    GPIO_TypeDef *p   = (GPIO_TypeDef *)port;
    uint8_t       pos = np_hal_pin_pos(pin_mask);

    if ((p == NULL) || (pos == NP_HAL_PIN_POS_INVALID)) {
        return;
    }
    np_hal_set2(&p->PUPDR, pos, (uint32_t)pupd);
    np_hal_set2(&p->MODER, pos, NP_HAL_MODE_INPUT);
}

void np_hal_pin_config_analog(void *port, uint16_t pin_mask)
{
    GPIO_TypeDef *p   = (GPIO_TypeDef *)port;
    uint8_t       pos = np_hal_pin_pos(pin_mask);

    if ((p == NULL) || (pos == NP_HAL_PIN_POS_INVALID)) {
        return;
    }
    /* Analog mode with no pull is the lowest-leakage state for an NTC divider
     * node; an internal pull would sit in parallel with the 10 kΩ leg and shift
     * every temperature reading. */
    np_hal_set2(&p->PUPDR, pos, NP_HAL_PUPD_NONE);
    np_hal_set2(&p->MODER, pos, NP_HAL_MODE_ANALOG);
}

void np_hal_pin_config_af(void *port, uint16_t pin_mask, uint8_t af)
{
    GPIO_TypeDef *p   = (GPIO_TypeDef *)port;
    uint8_t       pos = np_hal_pin_pos(pin_mask);
    uint32_t      idx;
    uint32_t      shift;
    uint32_t      v;

    if ((p == NULL) || (pos == NP_HAL_PIN_POS_INVALID)) {
        return;
    }

    idx   = ((uint32_t)pos >= 8U) ? 1UL : 0UL;
    shift = ((uint32_t)pos & 7UL) * 4UL;

    v  = p->AFR[idx];
    v &= ~(0xFUL << shift);
    v |= ((uint32_t)af & 0xFUL) << shift;
    p->AFR[idx] = v;

    np_hal_set2(&p->PUPDR, pos, NP_HAL_PUPD_NONE);
    np_hal_set2(&p->OSPEEDR, pos, 2U);   /* high speed — SPI1 runs at hub clock */
    np_hal_set2(&p->MODER, pos, NP_HAL_MODE_AF);
}

bool np_hal_pin_read(void *port, uint16_t pin_mask)
{
    const GPIO_TypeDef *p = (const GPIO_TypeDef *)port;

    if (p == NULL) {
        return false;
    }
    return (p->IDR & (uint32_t)pin_mask) != 0U;
}
