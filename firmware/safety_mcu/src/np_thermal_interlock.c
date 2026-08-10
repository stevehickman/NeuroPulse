/*
 * NeurOne Safety MCU — SW01-M04: Thermal Interlock
 * Document: NP-SW-001 Rev A, NP-FMEA-001 Rev A §SW01-M04
 *
 * Reads NTC thermistors on ADC1 for all 5 cranial thermal sense domains + hub.
 * A sense domain is the shell region one thermistor actually senses — a fixed
 * hardware property, NOT a zone (zones are authored socket sets in
 * 00-zones.npps and can be re-cut with no hardware change; NP-HW-HUB-001
 * Rev C §4.5.1).  Domain index is therefore not a slot id and not an enable-bit
 * position: every cranial domain cuts the one NP_SAFETY_EN_PBM_CRANIAL bit.
 * Cutoff at 62°C junction temperature (NP_NTC_CUTOFF_DEG_C).
 * Per IEC 60601 42°C case limit; 62°C junction corresponds to ~42°C
 * case surface given PCB thermal resistance.
 *
 * ADC readings are converted to °C using a 10kΩ NTC B=3950K table.
 * Polling rate: every ~10ms via main-loop SysTick tick counter.
 */

#include "np_safety_config.h"
#include "np_safety_hal.h"
#include "np_safety_protocol.h"
#include <stdint.h>
#include <stdbool.h>

/* HAL: np_hal_get_tick_ms (ms), np_hal_adc_read_channel (0-4095, 12-bit) —
 * both declared in np_safety_hal.h.                                        */

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
static bool     s_cutoff[NP_NTC_CHANNEL_COUNT];  /* one per sense domain + hub */

#define NP_THERMAL_POLL_MS 10U

np_safe_status_t np_thermal_interlock_init(void)
{
    for (uint8_t i = 0U; i < NP_NTC_CHANNEL_COUNT; i++) {
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

            if (ch < NP_NTC_CRANIAL_CHANNELS) {
                /* Cranial sense domain over-temp: cut ALL cranial PBM.
                 *
                 * This was `granted_mask &= ~(1U << ch)` — the domain index used
                 * as an enable-bit position, valid only while bits 0–4 were the
                 * five per-zone PBM enables.  Under NP-HW-HUB-001 Rev C §7.2 the
                 * cranial lattice has ONE enable bit, so that expression would
                 * clear reserved bits 1–4 for domains 1–4 and cut nothing at all.
                 *
                 * Cutting the whole lattice on any single domain over-temp is the
                 * §7.2 accepted consequence, not a widening of the response: the
                 * safety MCU is the interlock, and over-cutting is a usability
                 * cost, never a hazard.  Selective per-tile throttling lives
                 * elsewhere — the 62 °C hardware current throttle and the
                 * Class B duty=0 path (NP-FW-PBM1064-001 §6.5).
                 *
                 * fault_slot carries the sense-domain index for diagnosis.  It is
                 * NOT a module slot: slots 0–4 are retired and the parser rejects
                 * them, so no other path can write 0–4 here.                    */
                state->granted_mask &= (uint16_t)~NP_SAFETY_EN_PBM_CRANIAL;
                state->fault_slot    = ch;
            } else {
                /* Hub NTC: cut all stimulation */
                state->granted_mask  = 0U;
                state->fault_slot    = NP_FAULT_SLOT_HUB_NTC;
            }
            state->status |= NP_SAFETY_STATUS_THERMAL | NP_SAFETY_STATUS_CUTOFF;
        } else if (deg <= NP_NTC_REARM_DEG_C && s_cutoff[ch]) {
            /* Cooling recovery with 7°C hysteresis: a channel that has fallen
             * back below the re-arm threshold clears its own cutoff latch.
             * Without this, a single transient over-temp reading would keep the
             * cranial lattice (or, for the hub NTC, the whole device) disabled
             * until a power-cycle, because np_spi_watchdog_tick keeps forcing
             * granted_mask=0 while NP_SAFETY_STATUS_THERMAL is set.  Re-granting
             * the enable bit is left to np_spi_watchdog_tick, which recomputes
             * granted_mask from requested_mask once no interlock is asserting.  */
            s_cutoff[ch] = false;
        }
    }

    /* Clear the module-wide THERMAL status only when NO channel remains latched,
     * so one sense domain cooling down cannot unblock the mask while another is
     * still over-temp.  CUTOFF is cleared by np_spi_watchdog_tick on the next valid
     * heartbeat once no other interlock is asserting it. */
    bool any_cutoff = false;
    for (uint8_t ch = 0U; ch < NP_NTC_CHANNEL_COUNT; ch++) {
        if (s_cutoff[ch]) {
            any_cutoff = true;
            break;
        }
    }
    if (!any_cutoff) {
        state->status &= (uint8_t)~NP_SAFETY_STATUS_THERMAL;
    }
}
