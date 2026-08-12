/*
 * NeurOne Zone Module Announcement — Zone ID Detection
 * Document: NP-FW-ZA-001 Rev 1 §6
 *
 * Implements the ZONE_ID resistor classification and the 3×100 ms debounce
 * specified in docs/reference/durability-maintenance.md §7.1 (RISK-18):
 *   "3× ADC reads at 100 ms intervals before FAULT; ≥ 2/3 must pass."
 *
 * Hardware: pin 18 of the 20-pin ZIF FPC connector (Hirose FH34S-20S-0.5SH).
 *   R_PU = 10 kΩ pull-up to 3.3 V on hub PCB.
 *   R_Z  = zone resistor to GND inside module (1% tolerance).
 *   Vout = 3.3 V × R_Z / (R_PU + R_Z)  →  LPADC1 channel 12.
 */

#include "np_zone_announce.h"
#include <string.h>

/* ── Internal: ADC-value → zone classification ──────────────────────────────── */

static np_zone_id_t classify_adc(uint16_t counts)
{
    if (counts >= NP_ZA_ADC_NO_MODULE_LO) {
        return NP_ZONE_NONE;
    }
    if (counts >= NP_ZA_ADC_ZM05_LO && counts <= NP_ZA_ADC_ZM05_HI) {
        return NP_ZONE_PARIETAL_RIGHT;
    }
    if (counts >= NP_ZA_ADC_ZM04_LO && counts <= NP_ZA_ADC_ZM04_HI) {
        return NP_ZONE_PARIETAL_LEFT;
    }
    if (counts >= NP_ZA_ADC_ZM03_LO && counts <= NP_ZA_ADC_ZM03_HI) {
        return NP_ZONE_VERTEX;
    }
    if (counts >= NP_ZA_ADC_ZM02_LO && counts <= NP_ZA_ADC_ZM02_HI) {
        return NP_ZONE_FRONTAL_RIGHT;
    }
    if (counts >= NP_ZA_ADC_ZM01_LO && counts <= NP_ZA_ADC_ZM01_HI) {
        return NP_ZONE_FRONTAL_LEFT;
    }
    /* ADC value below ZM01_LO (< 1024) — short to GND or bad contact. */
    return NP_ZONE_UNKNOWN;
}

/* ── Internal: majority vote over debounce reads ────────────────────────────── */

/*
 * Returns the zone that appears ≥ NP_ZA_DEBOUNCE_MAJORITY times.
 * If no zone meets the threshold, returns NP_ZONE_UNKNOWN.
 * Ignores NP_ZONE_NONE entries (absence votes don't count toward a zone).
 */
static np_zone_id_t majority_zone(const np_zone_id_t *zone_reads, uint8_t count)
{
    uint8_t tally[NP_ZONE_UNKNOWN + 1U];
    memset(tally, 0, sizeof(tally));

    for (uint8_t i = 0U; i < count; i++) {
        if (zone_reads[i] <= NP_ZONE_UNKNOWN) {
            tally[zone_reads[i]]++;
        }
    }

    for (uint8_t z = NP_ZONE_FRONTAL_LEFT; z <= NP_ZONE_PARIETAL_RIGHT; z++) {
        if (tally[z] >= NP_ZA_DEBOUNCE_MAJORITY) {
            return (np_zone_id_t)z;
        }
    }
    return NP_ZONE_UNKNOWN;
}

/* ── Public: poll one slot, advance debounce state machine ─────────────────── */

/*
 * Called by np_za_tick() for each slot that is in SETTLING or DEBOUNCING state.
 *
 * Returns:
 *   NP_ZA_OK           — debounce not yet complete (still collecting reads).
 *   NP_ZA_ERR_DEBOUNCE_FAIL — 3 reads taken but no majority zone agreed;
 *                              caller should log fault and return slot to IDLE.
 *   NP_ZA_ERR_ADC_TIMEOUT  — platform ADC read failed.
 *   NP_ZA_ERR_UNKNOWN_ZONE — majority zone is NP_ZONE_UNKNOWN (wiring fault).
 *
 * On success (debounce complete and majority agreed), *zone_out is set and
 * the caller advances the slot to ANNOUNCING state.
 */
np_za_status_t np_za_detect_poll(uint8_t slot_index,
                                  np_za_debounce_t *deb,
                                  uint32_t          now_ms,
                                  np_zone_id_t     *zone_out)
{
    if (!deb || !zone_out) {
        return NP_ZA_ERR_INVALID_ARG;
    }

    /* Not yet time for the next ADC read. */
    if (now_ms < deb->next_read_ms) {
        return NP_ZA_OK;
    }

    /* Take one ADC reading. */
    uint16_t counts = 0U;
    if (!np_za_platform_adc_read(slot_index, &counts)) {
        return NP_ZA_ERR_ADC_TIMEOUT;
    }

    uint8_t idx = deb->read_count;
    deb->raw_adc[idx] = counts;
    deb->zone[idx]    = classify_adc(counts);
    deb->read_count++;
    deb->next_read_ms = now_ms + NP_ZA_DEBOUNCE_INTERVAL_MS;

    /* Not all reads collected yet — return and wait for next call. */
    if (deb->read_count < NP_ZA_DEBOUNCE_READS) {
        return NP_ZA_OK;
    }

    /* All reads taken — evaluate majority. */
    np_zone_id_t majority = majority_zone(deb->zone, NP_ZA_DEBOUNCE_READS);

    if (majority == NP_ZONE_UNKNOWN) {
        *zone_out = NP_ZONE_UNKNOWN;
        return NP_ZA_ERR_UNKNOWN_ZONE;
    }

    /* Count how many reads agreed with the majority (for SHDR logging). */
    uint8_t agree = 0U;
    for (uint8_t i = 0U; i < NP_ZA_DEBOUNCE_READS; i++) {
        if (deb->zone[i] == majority) {
            agree++;
        }
    }

    if (agree < NP_ZA_DEBOUNCE_MAJORITY) {
        return NP_ZA_ERR_DEBOUNCE_FAIL;
    }

    *zone_out = majority;
    return NP_ZA_OK;
}

/*
 * Reset a debounce context for a new insertion attempt.
 */
void np_za_detect_reset(np_za_debounce_t *deb, uint32_t first_read_ms)
{
    memset(deb, 0, sizeof(*deb));
    deb->next_read_ms = first_read_ms;
}

/*
 * Quick single-read check for no-module condition (used for removal polling).
 * Returns true if pin reads as no module (≥ NP_ZA_ADC_NO_MODULE_LO).
 */
bool np_za_detect_absent(uint8_t slot_index)
{
    uint16_t counts = 0U;
    if (!np_za_platform_adc_read(slot_index, &counts)) {
        return false;   /* treat ADC error as "still present" (safe default)    */
    }
    return (counts >= NP_ZA_ADC_NO_MODULE_LO);
}

/*
 * Return human-readable zone name.  Always non-NULL.
 */
const char *np_za_zone_name(np_zone_id_t zone)
{
    switch (zone) {
    case NP_ZONE_FRONTAL_LEFT:   return "Frontal Left";
    case NP_ZONE_FRONTAL_RIGHT:  return "Frontal Right";
    case NP_ZONE_VERTEX:         return "Vertex";
    case NP_ZONE_PARIETAL_LEFT:  return "Parietal Left";
    case NP_ZONE_PARIETAL_RIGHT: return "Parietal Right";
    case NP_ZONE_NONE:           return "None";
    default:                     return "Unknown";
    }
}
