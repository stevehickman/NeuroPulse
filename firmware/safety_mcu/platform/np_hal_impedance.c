/*
 * NeurOne Safety MCU — SW-01 Platform Layer: contact impedance measurement
 * Target: STM32G071 (Cortex-M0+, 64 MHz), bare metal, CMSIS registers only
 * Document: NP-SW-001 Rev A — SW-01 Class C; NP-SW-CI-001 §4.4 (OI-SWCI-17)
 *           CLAUDE.md §4.2 (VNS contact confirmation interlock); OI-CVNS-HUB-11
 *
 * Implements the four impedance symbols in np_safety_hal.h.  1 kHz AC contact
 * check, run while stimulation output is disabled, on four channels:
 *     0 = VNS_HRV   1 = tDCS   2 = BES_TACS   3 = CVNS
 *
 * ── READ THIS BEFORE TRUSTING THE NUMBERS THIS FILE RETURNS ──────────────────
 * This is the weakest driver in the platform layer, and the weakness is in the
 * hardware record, not in the code.
 *
 * np_safety_config.h specifies the MEASUREMENT (1 kHz, 50 ms, reject above
 * 10 kΩ) and specifies nothing about the analog front end that performs it:
 * there is no excitation source, no sense amplifier, no reference resistor and
 * no pin assignment for any of them anywhere in this repository.  Every other
 * driver here was written against a peripheral the config header names — SPI1,
 * TIM2, ADC1, GPIOA/B.  This one has no such anchor.
 *
 * What is implemented is therefore a real, complete driver against an
 * EXPLICITLY DECLARED provisional front end: TIM3 generates the 1 kHz
 * excitation, one ADC1 input per channel senses the divider node, and
 * NP_IMP_SENSE_R_OHM is the reference leg.  Every one of those is a placeholder
 * pending the analog design, is named in np_safety_config.h beside the other
 * provisional assignments rather than buried here, and is recorded as
 * OI-SWCI-34.  THE OHMS THIS FILE RETURNS ARE NOT CALIBRATED AND NO BENCH
 * MEASUREMENT STANDS BEHIND THEM.
 *
 * Why implement it at all rather than leave the four symbols undefined: the
 * enable gate around it is real and is exercised — np_impedance_check.c rejects
 * enable above NP_IMPEDANCE_MAX_OHM and that path is covered by
 * np_impedance_report_tests — and the fail-safe direction below is a genuine
 * safety property that holds regardless of what the analog front end turns out
 * to be.  A driver that returns "too high" whenever it cannot measure is
 * correct under every possible calibration.
 *
 * ── FAIL-SAFE DIRECTION ──────────────────────────────────────────────────────
 * np_impedance_check.c clears the channel's enable bit and raises
 * NP_SAFETY_STATUS_IMPEDANCE when z_ohm > NP_IMPEDANCE_MAX_OHM.  So HIGH is the
 * safe direction: every error path in this file returns NP_HAL_IMP_FAILSAFE_OHM,
 * which is above the limit and therefore refuses the enable.  Returning 0 on
 * error — the intuitive "nothing measured" value — would read as a perfect
 * contact and GRANT the enable.  Same shape as the ADC fail-safe in
 * np_hal_adc.c, opposite direction, for the same kind of reason: the consumer's
 * comparison decides which way is safe, not the driver's intuition.
 *
 * ── SETTLING-INTERVAL OWNERSHIP (answers np_safety_hal.h open question 4) ─────
 * The header records that NP_IMPEDANCE_TEST_MS (50 ms) exists but nothing says
 * whether the HAL enforces it or the caller must, and that "both readings are
 * consistent with the current code, because neither side implements it".
 *
 * Resolved: THE HAL OWNS IT.  np_hal_impedance_result_ready() returns false
 * until NP_IMPEDANCE_TEST_MS has elapsed since np_hal_impedance_start_test().
 * The HAL is chosen over the caller because the settling time is a property of
 * the analog front end — it is the electrode/gel time constant plus the filter
 * — and the caller cannot know it.  np_impedance_check.c already polls in a
 * non-blocking loop and already keeps s_test_start_ms, so this costs nothing on
 * the caller's side and makes the timing correct even if a future caller
 * forgets.  Note the interval is enforced in BOTH places as a result, which is
 * belt-and-braces rather than duplication: the HAL's copy is the binding one.
 *
 * IEC 62304 Class C — MISRA C:2012.  C11, no GNU extensions.
 */

#include "np_hal_internal.h"

#define NP_HAL_IMP_CHANNELS  4U

/* Above NP_IMPEDANCE_MAX_OHM (10 000) → the consumer refuses the enable.
 * Deliberately not 0xFFFFFFFF: a plainly out-of-band but finite value is easier
 * to recognise in a fault log than a saturated word that could equally be an
 * uninitialised read. */
#define NP_HAL_IMP_FAILSAFE_OHM  999999UL

_Static_assert(NP_HAL_IMP_FAILSAFE_OHM > NP_IMPEDANCE_MAX_OHM,
               "impedance fail-safe value must reject the enable");

/* 1 kHz excitation from the 64 MHz timer clock. */
#define NP_HAL_IMP_PSC  ((NP_SAFETY_SYSCLK_HZ / (NP_IMPEDANCE_TEST_HZ * 1000UL)) - 1UL)
#define NP_HAL_IMP_ARR  (1000UL - 1UL)

_Static_assert(((NP_HAL_IMP_PSC + 1UL) * (NP_HAL_IMP_ARR + 1UL))
                   == (NP_SAFETY_SYSCLK_HZ / NP_IMPEDANCE_TEST_HZ),
               "excitation timer does not produce NP_IMPEDANCE_TEST_HZ");

static uint32_t s_start_ms[NP_HAL_IMP_CHANNELS];
static bool     s_running[NP_HAL_IMP_CHANNELS];
static bool     s_configured = false;

/* Sense-node ADC input per impedance channel, and per CVNS electrode.
 * Provisional — see the file header and OI-SWCI-34. */
static const uint8_t k_imp_adc_ch[NP_HAL_IMP_CHANNELS] = {
    NP_IMP0_ADC_CH, NP_IMP1_ADC_CH, NP_IMP2_ADC_CH, NP_IMP3_ADC_CH
};
static const uint8_t k_cvns_adc_ch[NP_SAFETY_IMP_CVNS_ELECTRODES] = {
    NP_IMP_CVNS_L_ADC_CH, NP_IMP_CVNS_R_ADC_CH
};

/* Raw sense count → ohms through the reference divider.
 *
 *     Vsense = Vexc · Rref / (Rref + Zcontact)
 *  ⇒  Zcontact = Rref · (full_scale − count) / count
 *
 * Integer arithmetic throughout — this core has no FPU and the build is
 * -mfloat-abi=soft, so a float here would pull libgcc soft-float into a Class C
 * interlock path for no accuracy that matters against a 10 kΩ threshold. */
#define NP_HAL_IMP_FULL_SCALE  4095UL

static uint32_t np_hal_imp_count_to_ohm(uint32_t count)
{
    if (count == 0U) {
        /* Sense node at ground: open contact, or excitation not reaching the
         * electrode.  Both are "no contact" → fail-safe high. */
        return NP_HAL_IMP_FAILSAFE_OHM;
    }
    if (count >= NP_HAL_IMP_FULL_SCALE) {
        return 0U;                       /* dead short across the sense leg */
    }
    return ((uint32_t)NP_IMP_SENSE_R_OHM * (NP_HAL_IMP_FULL_SCALE - count)) / count;
}

static void np_hal_imp_configure(void)
{
    uint8_t i;

    if (s_configured) {
        return;
    }

    np_hal_port_clocks_enable();

    /* Sense nodes are analog inputs on ADC1, which np_hal_adc_init() has
     * already brought up — the impedance check runs after peripheral init in
     * np_safety_main.c, and sharing one ADC is deliberate: a second converter
     * would need its own calibration and its own fail-safe path. */
    for (i = 0U; i < NP_HAL_IMP_CHANNELS; i++) {
        np_hal_pin_config_analog(NP_IMP_SENSE_PORT, (uint16_t)(1U << k_imp_adc_ch[i]));
    }

    /* TIM3: 1 kHz excitation. */
    RCC->APBENR1 |= RCC_APBENR1_TIM3EN;
    (void)RCC->APBENR1;

    NP_IMP_EXC_TIM->CR1 = 0UL;
    NP_IMP_EXC_TIM->PSC = NP_HAL_IMP_PSC;
    NP_IMP_EXC_TIM->ARR = NP_HAL_IMP_ARR;
    NP_IMP_EXC_TIM->CNT = 0UL;
    NP_IMP_EXC_TIM->EGR = TIM_EGR_UG;
    NP_IMP_EXC_TIM->SR  = 0UL;

    s_configured = true;
}

void np_hal_impedance_start_test(uint8_t channel)
{
    /* Out-of-range — np_safety_hal.h open question 3.  Start nothing; the
     * matching _result_ready() will stay false and _read_ohm() will report the
     * fail-safe value, so an out-of-range request can never grant an enable. */
    if (channel >= NP_HAL_IMP_CHANNELS) {
        return;
    }

    np_hal_imp_configure();

    /* Excitation on for the duration of the test window. */
    NP_IMP_EXC_TIM->CR1 |= TIM_CR1_CEN;

    s_start_ms[channel] = np_hal_get_tick_ms();
    s_running[channel]  = true;
}

bool np_hal_impedance_result_ready(uint8_t channel)
{
    uint32_t now;

    if (channel >= NP_HAL_IMP_CHANNELS) {
        return false;
    }
    if (!s_running[channel]) {
        return false;
    }

    /* Unsigned subtraction — wrap-correct across the 49.7-day tick rollover,
     * which np_safety_hal.h requires of every timebase consumer. */
    now = np_hal_get_tick_ms();
    if ((now - s_start_ms[channel]) < (uint32_t)NP_IMPEDANCE_TEST_MS) {
        return false;                    /* settling — the HAL owns this wait */
    }
    return true;
}

/* One settled conversion on a named ADC input.  Returns false if the sample
 * could not be taken, which every caller maps to the fail-safe ohm value. */
static bool np_hal_imp_sample(uint8_t adc_ch, uint32_t *count_out)
{
    uint32_t spins = 0U;

    if (!s_configured) {
        return false;
    }

    NP_NTC_ADC_INSTANCE->ISR    = ADC_ISR_EOC | ADC_ISR_EOS;
    NP_NTC_ADC_INSTANCE->CHSELR = (1UL << adc_ch);
    while ((NP_NTC_ADC_INSTANCE->ISR & ADC_ISR_CCRDY) == 0U) {
        spins++;
        if (spins >= 20000UL) { return false; }
    }

    NP_NTC_ADC_INSTANCE->CR |= ADC_CR_ADSTART;
    spins = 0U;
    while ((NP_NTC_ADC_INSTANCE->ISR & ADC_ISR_EOC) == 0U) {
        spins++;
        if (spins >= 20000UL) {
            NP_NTC_ADC_INSTANCE->CR |= ADC_CR_ADSTP;
            return false;
        }
    }

    *count_out = NP_NTC_ADC_INSTANCE->DR & 0x0FFFUL;
    return true;
}

uint32_t np_hal_impedance_read_ohm(uint8_t channel)
{
    uint32_t count = 0U;

    if (channel >= NP_HAL_IMP_CHANNELS) {
        return NP_HAL_IMP_FAILSAFE_OHM;
    }
    if (!s_running[channel]) {
        /* Read without a completed test.  Not an enable-granting condition. */
        return NP_HAL_IMP_FAILSAFE_OHM;
    }

    s_running[channel] = false;

    /* Excitation off once the last channel finishes.  Left simple: the check
     * runs pre-session with all stimulation disabled, so an extra millisecond
     * of 1 kHz on the sense network is not a hazard. */
    if (!np_hal_imp_sample(k_imp_adc_ch[channel], &count)) {
        return NP_HAL_IMP_FAILSAFE_OHM;
    }
    return np_hal_imp_count_to_ohm(count);
}

uint32_t np_hal_impedance_read_cvns_electrode_ohm(uint8_t electrode)
{
    uint32_t count = 0U;

    if (electrode >= (uint8_t)NP_SAFETY_IMP_CVNS_ELECTRODES) {
        return NP_HAL_IMP_FAILSAFE_OHM;
    }

    /* PRIVACY (CLAUDE.md §5.1, np_safety_hal.h): raw per-electrode ohms are
     * UHDR.  They are returned to np_impedance_check.c, which converts them to
     * kΩ×100 for the device-internal hub cross-validation report and never
     * writes them to SHDR — only the divergence FLAG reaches SHDR.  This driver
     * therefore neither logs nor retains the value; it is computed on demand
     * and returned.  Do not add a cache here. */
    if (!np_hal_imp_sample(k_cvns_adc_ch[electrode], &count)) {
        return NP_HAL_IMP_FAILSAFE_OHM;
    }
    return np_hal_imp_count_to_ohm(count);
}
