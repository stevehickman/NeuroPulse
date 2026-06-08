/*
 * NeuroPulse Safety MCU — SW01-M04: Zone Thermal Interlock
 * Document: NP-SW-001 Rev A, NP-FMEA-001 Rev A §SW01-M04
 *
 * Reads NTC thermistors on ADC1 for all 5 zone modules + hub.
 * Cutoff at 62°C junction temperature (NP_NTC_CUTOFF_DEG_C).
 * Per IEC 60601 42°C case limit; 62°C junction corresponds to ~42°C
 * case surface given PCB thermal resistance.
 *
 * ADC readings are converted to °C using a 10kΩ NTC B=3950K table.
 * Polling rate: every ~10ms via main-loop SysTick tick counter.
 */

#include "np_safety_config.h"
#include "np_safety_protocol.h"
#include <stdint.h>
#include <stdbool.h>

/* ── HAL stub ────────────────────────────────────────────────────────────── */
extern uint32_t np_hal_get_tick_ms(void);
extern uint16_t np_hal_adc_read_channel(uint8_t channel); /* 0-4095, 12-bit */

/* ── NTC conversion (B=3950K, R25=10kΩ, Rdivider=10kΩ) ─────────────────── */
/* Lookup table: ADC counts → temperature °C (0..110°C range, 1°C steps)    */
/* Generated offline; values are approximate ADC counts at Vref=3.3V.        */
static const uint16_t k_ntc_adc[111] = {
    /* 0°C */ 3820, 3800, 3775, 3750, 3720, 3690, 3655, 3620, 3580, 3540,
    /*10°C */ 3495, 3448, 3398, 3346, 3290, 3232, 3170, 3107, 3040, 2972,
    /*20°C */ 2901, 2829, 2756, 2680, 2604, 2527, 2450, 2372, 2294, 2217,
    /*30°C */ 2140, 2065, 1990, 1917, 1846, 1776, 1708, 1641, 1577, 1515,
    /*40°C */ 1455, 1397, 1342, 1289, 1238, 1189, 1143, 1099, 1057, 1017,
    /*50°C */  979,  943,  909,  877,  846,  817,  789,  763,  738,  714,
    /*60°C */  692,  670,  650,  631,  612,  595,  578,  562,  547,  532,
    /*70°C */  519,  505,  493,  481,  469,  458,  447,  437,  427,  417,
    /*80°C */  408,  399,  390,  382,  374,  366,  359,  352,  345,  338,
    /*90°C */  332,  326,  320,  314,  308,  303,  298,  293,  288,  283,
    /*100°C*/  278,  274,  270,  265,  261,  257,  254,  250,  246,  243,  239
};

static uint8_t adc_to_celsius(uint16_t adc)
{
    /* Scan table from low index (cold) to high index (hot) */
    for (uint8_t t = 0U; t < 110U; t++) {
        if (adc >= k_ntc_adc[t + 1U]) {
            return t + 1U;
        }
    }
    return 110U;
}

/* ── Module state ────────────────────────────────────────────────────────── */
static uint32_t s_last_poll_ms = 0U;
static bool     s_cutoff[6];  /* one per zone + hub */

#define NP_THERMAL_POLL_MS 10U

np_safe_status_t np_thermal_interlock_init(void)
{
    for (uint8_t i = 0U; i < 6U; i++) {
        s_cutoff[i] = false;
    }
    s_last_poll_ms = 0U;
    return NP_SAFE_OK;
}

void np_thermal_interlock_tick(np_safety_state_t *state)
{
    uint32_t now = np_hal_get_tick_ms();
    if ((now - s_last_poll_ms) < NP_THERMAL_POLL_MS) {
        return;
    }
    s_last_poll_ms = now;

    for (uint8_t ch = 0U; ch < NP_NTC_CHANNEL_COUNT; ch++) {
        uint16_t adc = np_hal_adc_read_channel(ch);
        uint8_t  deg = adc_to_celsius(adc);

        if (deg >= NP_NTC_CUTOFF_DEG_C && !s_cutoff[ch]) {
            s_cutoff[ch] = true;

            if (ch < 5U) {
                /* Zone ch: clear corresponding PBM enable bit */
                state->granted_mask &= (uint16_t)~(1U << ch);
                state->fault_slot    = ch;
            } else {
                /* Hub NTC: cut all stimulation */
                state->granted_mask  = 0U;
                state->fault_slot    = 0xFEU; /* hub = 0xFE */
            }
            state->status |= NP_SAFETY_STATUS_THERMAL | NP_SAFETY_STATUS_CUTOFF;
        }
    }
}
