/*
 * NeurOne Safety MCU — SW-01 Platform Layer: ADC1 (NTC thermal sense)
 * Target: STM32G071 (Cortex-M0+, 64 MHz), bare metal, CMSIS registers only
 * Document: NP-SW-001 Rev A — SW-01 Class C; NP-SW-CI-001 §4.4 (OI-SWCI-17)
 *           CLAUDE.md §4.2 (IEC 60601 42 °C limit, 62 °C junction cutoff)
 *
 * Implements np_hal_adc_init() and np_hal_adc_read_channel().
 *
 * Six NTC sense domains: five cranial + one hub (NP_NTC_CHANNEL_COUNT).
 * np_thermal_interlock.c maps counts to °C through a 10 kΩ / B=3950 K divider
 * table and cuts the cranial PBM enable at NP_NTC_CUTOFF_DEG_C (62 °C).
 *
 * ── THE FAIL-SAFE VALUE IS MEASURED, NOT CHOSEN ──────────────────────────────
 * np_safety_hal.h open question 3 records that out-of-range behaviour was never
 * specified.  Picking a return value for the error paths therefore requires
 * knowing which DIRECTION is safe, and that is a property of the consumer's
 * lookup table, not of this file.
 *
 * k_ntc_adc[] in np_thermal_interlock.c is DESCENDING — hotter reads lower.
 * adc_to_celsius() scans from index 0 upward returning the first t+1 whose
 * table entry the reading is at or above, so:
 *
 *     count 4095 (rail high) →   1 °C  → no cutoff  → stimulation CONTINUES
 *     count    0 (rail low)  → 110 °C  → cutoff     → stimulation CUT
 *
 * So zero is the fail-safe direction and full-scale is the dangerous one, which
 * is the opposite of the intuition that "0 means nothing was read".  A driver
 * that returned 0xFFFF on a timeout would clamp to a cold reading and suppress
 * the thermal interlock on the exact fault it was reporting.
 *
 * NP_HAL_ADC_FAILSAFE_COUNT is 0 for that reason, and the reasoning is checked
 * rather than trusted: np_hal_platform_tests.c links the REAL
 * np_thermal_interlock.c against a HAL double returning this constant and
 * asserts the cranial PBM enable bit is actually cleared.  If the table is ever
 * regenerated in the other orientation, that test fails.
 *
 * ── BLOCKING AND WCET (np_safety_hal.h open question 1) ──────────────────────
 * np_hal_adc_read_channel() BLOCKS for one conversion.
 *   ADC clock  = SYSCLK/4 via PRESC = 16 MHz  (asserted below)
 *   Sample time = 160.5 ADC cycles (max) — an NTC divider through a 10 kΩ
 *                 source needs a long sample window to charge Csample
 *   Conversion  = 12.5 cycles at 12-bit resolution
 *   Total       = 173 cycles / 16 MHz ≈ 10.8 µs per channel
 *
 * np_thermal_interlock_tick() reads all six channels every NP_THERMAL_POLL_MS
 * (10 ms), so the worst case is ≈ 65 µs of the 10 ms period — 0.65 %.  That is
 * inside the budget for a loop that must also service a 1.5 s SPI watchdog and
 * a <100 ms cardiac cutoff.  The poll is additionally capped by
 * NP_HAL_ADC_SPIN_MAX so a stuck EOC cannot hang the loop at all.
 *
 * IEC 62304 Class C — MISRA C:2012.  C11, no GNU extensions.
 */

#include "np_hal_internal.h"

/* See the file header: 0 maps to 110 °C through the consumer's table, which
 * cuts.  Do not "improve" this to 0xFFFF or to a mid-scale value. */
#define NP_HAL_ADC_FAILSAFE_COUNT  0U

/* Bounded wait for one conversion.  At 64 MHz a 10.8 µs conversion is ~690
 * core cycles; 20000 loop iterations is >100× that and still bounded. */
#define NP_HAL_ADC_SPIN_MAX  20000UL

/* Sampling-time selector SMP = 0b111 = 160.5 ADC clock cycles (longest). */
#define NP_HAL_ADC_SMP_160C5  7UL

/* ADC prescaler CKMODE = PCLK/4 in ADC_CFGR2[31:30]. */
#define NP_HAL_ADC_CKMODE_PCLK_DIV4  (2UL << 30)

_Static_assert(NP_NTC_CHANNEL_COUNT == 6U,
               "NTC channel map below covers exactly six sense domains");

/*
 * Logical NTC sense-domain index → (port, pin, ADC input channel).
 *
 * A TABLE, not arithmetic on the index — np_safety_hal.h is explicit that the
 * channel index "is NOT a module slot id and NOT an enable-bit position", and
 * np_safety_config.h says the same about sense domains.  Any expression that
 * derives an ADC channel from the domain index would re-create exactly the
 * defect NP-HW-HUB-001 Rev C §7.2 removed from np_thermal_interlock.c, where an
 * index was used as a bit position and cut nothing.
 *
 * PIN ASSIGNMENT IS PROVISIONAL, on the same footing as every other GPIO
 * assignment in np_safety_config.h ("provisional pending PCB layout (G1 gate)").
 * Constrained by what is already spoken for: PA0 is the cranial PBM enable,
 * PA4..PA7 are SPI1, PA8 is R-peak, PB0..PB8 are the nine other enable lines.
 * PA1/PA2/PA3 and PB10/PB11/PB12 are what remains with an ADC1 input.
 * Recorded as OI-SWCI-28 for the PCB review; NOT presented as a hardware fact.
 */
typedef struct {
    void    *port;
    uint16_t pin;
    uint8_t  adc_ch;
} np_hal_ntc_map_t;

static const np_hal_ntc_map_t k_ntc_map[NP_NTC_CHANNEL_COUNT] = {
    { NP_NTC0_PORT, NP_NTC0_PIN, NP_NTC0_ADC_CH },   /* cranial sense domain 0 */
    { NP_NTC1_PORT, NP_NTC1_PIN, NP_NTC1_ADC_CH },   /* cranial sense domain 1 */
    { NP_NTC2_PORT, NP_NTC2_PIN, NP_NTC2_ADC_CH },   /* cranial sense domain 2 */
    { NP_NTC3_PORT, NP_NTC3_PIN, NP_NTC3_ADC_CH },   /* cranial sense domain 3 */
    { NP_NTC4_PORT, NP_NTC4_PIN, NP_NTC4_ADC_CH },   /* cranial sense domain 4 */
    { NP_NTC5_PORT, NP_NTC5_PIN, NP_NTC5_ADC_CH },   /* hub                    */
};

static bool s_adc_ready = false;

static bool np_hal_adc_spin(const volatile uint32_t *reg, uint32_t mask, bool want_set)
{
    uint32_t spins = 0U;

    for (;;) {
        bool is_set = ((*reg & mask) != 0U);
        if (is_set == want_set) {
            return true;
        }
        spins++;
        if (spins >= NP_HAL_ADC_SPIN_MAX) {
            return false;
        }
    }
}

void np_hal_adc_init(void)
{
    uint8_t i;

    s_adc_ready = false;

    RCC->APBENR2 |= RCC_APBENR2_ADCEN;
    (void)RCC->APBENR2;

    np_hal_port_clocks_enable();
    for (i = 0U; i < NP_NTC_CHANNEL_COUNT; i++) {
        np_hal_pin_config_analog(k_ntc_map[i].port, k_ntc_map[i].pin);
    }

    /* Deselect and stop anything a warm reset left running.  ADCAL below is
     * only writable with ADEN = 0, and ADEN is only clearable through ADDIS. */
    if ((NP_NTC_ADC_INSTANCE->CR & ADC_CR_ADEN) != 0U) {
        if ((NP_NTC_ADC_INSTANCE->CR & ADC_CR_ADSTART) != 0U) {
            NP_NTC_ADC_INSTANCE->CR |= ADC_CR_ADSTP;
            (void)np_hal_adc_spin(&NP_NTC_ADC_INSTANCE->CR, ADC_CR_ADSTP, false);
        }
        NP_NTC_ADC_INSTANCE->CR |= ADC_CR_ADDIS;
        (void)np_hal_adc_spin(&NP_NTC_ADC_INSTANCE->CR, ADC_CR_ADEN, false);
    }

    /* Clock: PCLK/4 = 16 MHz.  Synchronous to PCLK rather than the async ADC
     * clock so the 10.8 µs WCET in the file header is derived from a frequency
     * this firmware controls, not from an independent RC oscillator. */
    NP_NTC_ADC_INSTANCE->CFGR2 =
        (NP_NTC_ADC_INSTANCE->CFGR2 & ~(3UL << 30)) | NP_HAL_ADC_CKMODE_PCLK_DIV4;

    /* Voltage regulator up, then its startup time.  The regulator needs ~20 µs;
     * the bounded spin below is a time-shaped delay, not a flag wait, because
     * there is no ready bit for it. */
    NP_NTC_ADC_INSTANCE->CR |= ADC_CR_ADVREGEN;
    {
        volatile uint32_t d = 0U;
        while (d < 4000UL) { d++; }        /* > 20 µs at 64 MHz */
    }

    /* Self-calibration.  Must run with ADEN = 0 (enforced above). */
    NP_NTC_ADC_INSTANCE->CR |= ADC_CR_ADCAL;
    if (!np_hal_adc_spin(&NP_NTC_ADC_INSTANCE->CR, ADC_CR_ADCAL, false)) {
        return;                             /* stays !s_adc_ready → fail-safe */
    }

    /* 12-bit, right-aligned, single conversion, software trigger.  RES = 00 and
     * ALIGN = 0 are the reset values and are written explicitly because
     * np_safety_hal.h makes "raw 12-bit, 0..4095" part of the contract. */
    NP_NTC_ADC_INSTANCE->CFGR1 = 0UL;

    /* Longest sample time on both channel groups — a 10 kΩ NTC divider is a
     * high-impedance source and undersampling it reads systematically LOW,
     * which through the descending table reads systematically HOT.  That errs
     * toward cutting, but it would still be a wrong temperature. */
    NP_NTC_ADC_INSTANCE->SMPR =
        NP_HAL_ADC_SMP_160C5 | (NP_HAL_ADC_SMP_160C5 << 4);

    /* Enable and wait for ADRDY. */
    NP_NTC_ADC_INSTANCE->ISR = ADC_ISR_ADRDY;      /* w1c any stale flag */
    NP_NTC_ADC_INSTANCE->CR |= ADC_CR_ADEN;
    if (!np_hal_adc_spin(&NP_NTC_ADC_INSTANCE->ISR, ADC_ISR_ADRDY, true)) {
        return;
    }

    s_adc_ready = true;
}

uint16_t np_hal_adc_read_channel(uint8_t channel)
{
    uint32_t raw;

    /* Out-of-range index — np_safety_hal.h open question 3.  Fail-safe is a
     * reading the consumer will treat as HOT; see the file header. */
    if (channel >= NP_NTC_CHANNEL_COUNT) {
        return NP_HAL_ADC_FAILSAFE_COUNT;
    }

    /* Init failed or never ran.  A thermal interlock reading an unconfigured
     * ADC must not conclude "cold". */
    if (!s_adc_ready) {
        return NP_HAL_ADC_FAILSAFE_COUNT;
    }

    /* Single channel selection.  CHSELR is a bitmap in the G0's sequencer;
     * writing exactly one bit makes the sequence one conversion long. */
    NP_NTC_ADC_INSTANCE->ISR    = ADC_ISR_EOC | ADC_ISR_EOS;
    NP_NTC_ADC_INSTANCE->CHSELR = (1UL << k_ntc_map[channel].adc_ch);
    if (!np_hal_adc_spin(&NP_NTC_ADC_INSTANCE->ISR, ADC_ISR_CCRDY, true)) {
        return NP_HAL_ADC_FAILSAFE_COUNT;
    }

    NP_NTC_ADC_INSTANCE->CR |= ADC_CR_ADSTART;
    if (!np_hal_adc_spin(&NP_NTC_ADC_INSTANCE->ISR, ADC_ISR_EOC, true)) {
        /* Stuck conversion: stop it so the next call is not wedged behind it,
         * and report the fail-safe count. */
        NP_NTC_ADC_INSTANCE->CR |= ADC_CR_ADSTP;
        (void)np_hal_adc_spin(&NP_NTC_ADC_INSTANCE->CR, ADC_CR_ADSTP, false);
        return NP_HAL_ADC_FAILSAFE_COUNT;
    }

    raw = NP_NTC_ADC_INSTANCE->DR & 0x0FFFUL;   /* reading DR clears EOC */
    return (uint16_t)raw;
}
