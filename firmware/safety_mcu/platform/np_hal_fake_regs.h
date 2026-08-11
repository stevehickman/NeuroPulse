/*
 * NeurOne Safety MCU — SW-01 Platform Layer: host-test register file
 * Document: NP-SW-001 Rev A — SW-01 Class C; NP-SW-CI-001 §4.4 (OI-SWCI-17)
 *
 * TEST-ONLY.  Included by np_hal_internal.h if and only if NP_HAL_HOST_TEST is
 * defined, which only np_hal_platform_tests does.  The cross build defines
 * STM32G071xx and never defines NP_HAL_HOST_TEST; np_hal_internal.h #errors if
 * both are set, so this file cannot reach the firmware image.
 *
 * ── WHY THIS EXISTS ──────────────────────────────────────────────────────────
 * np_safety_hal.h states the problem it could not solve: the ABI is
 * compiler-checked, but the BEHAVIOURAL contract — "units, polarity, blocking,
 * buffer layout" — is prose, and "a driver that implements
 * np_hal_gpio_write_pin() with 1 = ENABLED has an identical signature, compiles
 * clean, links clean, and inverts every stimulation enable line."
 *
 * That is a hazard a linked image does not address.  Phase 7's platform layer
 * links, which proves the symbols exist; it does not prove the polarity is
 * right.  Substituting plain-C structs for the CMSIS peripheral instances lets
 * a host test compile the REAL driver sources — not a re-implementation, not a
 * mock, the shipped .c files — and assert on the register writes they actually
 * perform.  Inverting the polarity in np_hal_gpio.c fails a test instead of
 * shipping.
 *
 * ── WHAT IT DOES AND DOES NOT MODEL ──────────────────────────────────────────
 * These are DUMB storage structs.  They record writes; they do not emulate the
 * peripheral.  Writing SPI_CR1_SPE does not start a transfer, ADSTART sets no
 * EOC, and PLLON sets no PLLRDY.  So this file supports tests of the driver's
 * DECISIONS — which register, which bit, which order, which value — and cannot
 * support tests of anything that waits on hardware to respond.  Drivers with
 * hardware handshakes (np_hal_clock.c, np_hal_adc.c, np_hal_impedance.c) are
 * deliberately NOT in the host-test target for that reason: a fake that always
 * reports "ready" would test the happy path and silently skip every timeout
 * branch, which is worse than no coverage because it reads as coverage.
 *
 * Field layouts match the STM32G0 reference manual by NAME and by the offsets
 * this firmware relies on; they are not byte-exact peripheral maps and must not
 * be treated as such.
 */

#ifndef NP_HAL_FAKE_REGS_H
#define NP_HAL_FAKE_REGS_H

#if !defined(NP_HAL_HOST_TEST)
#  error "np_hal_fake_regs.h is test-only; it must not be reachable in a target build"
#endif

#include <stdint.h>

/* ── GPIO ─────────────────────────────────────────────────────────────────── */
typedef struct {
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t LCKR;
    volatile uint32_t AFR[2];
    volatile uint32_t BRR;
} GPIO_TypeDef;

extern GPIO_TypeDef np_hal_fake_gpioa;
extern GPIO_TypeDef np_hal_fake_gpiob;
#define GPIOA  (&np_hal_fake_gpioa)
#define GPIOB  (&np_hal_fake_gpiob)

/* ── RCC ──────────────────────────────────────────────────────────────────── */
typedef struct {
    volatile uint32_t CR;
    volatile uint32_t CFGR;
    volatile uint32_t PLLCFGR;
    volatile uint32_t IOPENR;
    volatile uint32_t APBENR1;
    volatile uint32_t APBENR2;
} RCC_TypeDef;

extern RCC_TypeDef np_hal_fake_rcc;
#define RCC  (&np_hal_fake_rcc)

#define RCC_IOPENR_GPIOAEN    (1UL << 0)
#define RCC_IOPENR_GPIOBEN    (1UL << 1)
#define RCC_APBENR1_TIM2EN    (1UL << 0)
#define RCC_APBENR1_TIM3EN    (1UL << 1)
#define RCC_APBENR2_SYSCFGEN  (1UL << 0)
#define RCC_APBENR2_ADCEN     (1UL << 20)
#define RCC_APBENR2_SPI1EN    (1UL << 12)

/* ── SPI ──────────────────────────────────────────────────────────────────── */
typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t SR;
    volatile uint32_t DR;
} SPI_TypeDef;

extern SPI_TypeDef np_hal_fake_spi1;
#define SPI1  (&np_hal_fake_spi1)

#define SPI_CR1_SPE      (1UL << 6)
#define SPI_CR2_DS_Pos   8U
#define SPI_CR2_FRXTH    (1UL << 12)
#define SPI_CR2_RXNEIE   (1UL << 6)
#define SPI_SR_RXNE      (1UL << 0)
#define SPI_SR_OVR       (1UL << 6)

/* ── Timers ───────────────────────────────────────────────────────────────── */
typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t SR;
    volatile uint32_t EGR;
    volatile uint32_t CNT;
    volatile uint32_t PSC;
    volatile uint32_t ARR;
} TIM_TypeDef;

extern TIM_TypeDef np_hal_fake_tim2;
extern TIM_TypeDef np_hal_fake_tim3;
#define TIM2  (&np_hal_fake_tim2)
#define TIM3  (&np_hal_fake_tim3)

#define TIM_CR1_CEN  (1UL << 0)
#define TIM_EGR_UG   (1UL << 0)

/* ── ADC ──────────────────────────────────────────────────────────────────── */
typedef struct {
    volatile uint32_t ISR;
    volatile uint32_t CR;
    volatile uint32_t CFGR1;
    volatile uint32_t CFGR2;
    volatile uint32_t SMPR;
    volatile uint32_t CHSELR;
    volatile uint32_t DR;
} ADC_TypeDef;

extern ADC_TypeDef np_hal_fake_adc1;
#define ADC1  (&np_hal_fake_adc1)

#define ADC_ISR_ADRDY   (1UL << 0)
#define ADC_ISR_EOC     (1UL << 2)
#define ADC_ISR_EOS     (1UL << 3)
#define ADC_ISR_CCRDY   (1UL << 13)
#define ADC_CR_ADEN     (1UL << 0)
#define ADC_CR_ADDIS    (1UL << 1)
#define ADC_CR_ADSTART  (1UL << 2)
#define ADC_CR_ADSTP    (1UL << 4)
#define ADC_CR_ADCAL    (1UL << 31)
#define ADC_CR_ADVREGEN (1UL << 28)

/* ── EXTI / NVIC ──────────────────────────────────────────────────────────── */
typedef struct {
    volatile uint32_t RTSR1;
    volatile uint32_t FTSR1;
    volatile uint32_t RPR1;
    volatile uint32_t IMR1;
    volatile uint32_t EXTICR[4];
} EXTI_TypeDef;

extern EXTI_TypeDef np_hal_fake_exti;
#define EXTI  (&np_hal_fake_exti)

typedef enum { SPI1_IRQn = 25, EXTI4_15_IRQn = 7 } IRQn_Type;
#define NVIC_EnableIRQ(x)        ((void)(x))
#define NVIC_ClearPendingIRQ(x)  ((void)(x))

/* ── OTP window ───────────────────────────────────────────────────────────────
 * np_hal_otp.c reads an absolute address, which a host process cannot map.  It
 * guards its base with #ifndef, so pointing it at a normal array here lets the
 * REAL translation logic — including the all-0xFF → all-zero sentinel rule —
 * run against bytes a test controls. */
extern uint8_t np_hal_fake_otp[1024];
#define NP_HAL_OTP_BASE  ((uintptr_t)np_hal_fake_otp)

#endif /* NP_HAL_FAKE_REGS_H */
