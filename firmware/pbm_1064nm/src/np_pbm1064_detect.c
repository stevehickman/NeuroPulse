/*
 * NeuroPulse 1064nm Smart Zone Module — Smart Module Detection
 * Document: NP-FW-PBM1064-001 Rev A §4
 *
 * Distinguishes smart modules (3.3 kΩ ZONE_ID, ADC < 1100) from base modules
 * (10 k–220 kΩ, ADC 1100–4000) using the same 3×100 ms debounce policy as
 * the base module ladder (RISK-18).  On confirmed smart module: probes I2C at
 * address 0x30, then delegates bone conduction announcement to np_za.
 */

#include "np_pbm1064_detect.h"
#include "np_pbm1064_hal.h"
#include <string.h>

/* ── ADC classification ─────────────────────────────────────────────────────── */

np_slot_type_t np_pbm1064_classify_adc(uint16_t counts)
{
    if (counts >= NP_PBM1064_ADC_NO_MODULE_MIN) {
        return NP_SLOT_ABSENT;
    }
    if (counts <= NP_PBM1064_ADC_SMART_MAX) {
        return NP_SLOT_SMART;
    }
    /* 1100 ≤ counts < 4000 → one of the five base module resistor values. */
    return NP_SLOT_BASE_MODULE;
}

/* ── Debounce helpers ───────────────────────────────────────────────────────── */

static void debounce_reset(np_sm_slot_ctx_t *s, uint32_t first_read_ms)
{
    s->read_count    = 0;
    s->next_read_ms  = first_read_ms;
    memset(s->adc_reads, 0, sizeof(s->adc_reads));
}

/*
 * Evaluate 3 ADC readings.  Returns the agreed-upon slot type if ≥ 2/3 agree,
 * or NP_SLOT_ABSENT (treated as failure) if no majority.
 */
static np_slot_type_t debounce_majority(const np_sm_slot_ctx_t *s)
{
    uint8_t tally[3] = {0, 0, 0};  /* [ABSENT, SMART, BASE_MODULE] */

    for (uint8_t i = 0; i < NP_PBM1064_DEBOUNCE_READS; i++) {
        np_slot_type_t t = np_pbm1064_classify_adc(s->adc_reads[i]);
        if ((uint8_t)t < 3U) {
            tally[t]++;
        }
    }

    for (uint8_t t = 0; t < 3U; t++) {
        if (tally[t] >= NP_PBM1064_DEBOUNCE_MAJORITY) {
            return (np_slot_type_t)t;
        }
    }
    return NP_SLOT_ABSENT; /* no majority */
}

/* ── Public API ─────────────────────────────────────────────────────────────── */

void np_pbm1064_detect_init(np_pbm1064_detect_ctx_t *ctx,
                              np_pbm1064_remove_cb_t   remove_cb,
                              uint32_t                 device_session_count)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->remove_cb             = remove_cb;
    ctx->device_session_count  = device_session_count;

    /*
     * All slots start in NP_TIA_GAIN_HIGH (Rf = 47 kΩ).  The HAL boot-init
     * function must have already driven GAIN_SEL[0..4] LOW before this point.
     * The per-slot tia_gain field is redundant with the GPIO state but kept for
     * diagnostic logging and software test verification.
     */
    for (uint8_t i = 0; i < NP_PBM1064_ZONE_COUNT; i++) {
        ctx->slots[i].tia_gain = NP_TIA_GAIN_HIGH;
    }
}

void np_pbm1064_detect_tick(np_pbm1064_detect_ctx_t *ctx,
                              void (*insert_cb)(uint8_t slot))
{
    uint32_t now_ms = np_pbm1064_hal_now_ms();

    for (uint8_t slot = 0; slot < NP_PBM1064_ZONE_COUNT; slot++) {
        np_sm_slot_ctx_t *s = &ctx->slots[slot];

        switch (s->state) {
        case NP_SM_IDLE: {
            uint16_t counts = 0;
            if (!np_pbm1064_hal_adc_read_zone_id(slot, &counts)) {
                break;
            }
            np_slot_type_t t = np_pbm1064_classify_adc(counts);
            if (t == NP_SLOT_ABSENT) {
                break;
            }
            /* Insertion detected — begin settle delay. */
            s->settle_until_ms = now_ms + NP_PBM1064_INSERTION_SETTLE_MS;
            s->state = NP_SM_SETTLING;
            break;
        }

        case NP_SM_SETTLING:
            if (now_ms < s->settle_until_ms) {
                break;
            }
            debounce_reset(s, now_ms);
            s->state = NP_SM_DEBOUNCING;
            /* fall through to take first ADC read immediately */
            /* fallthrough */

        case NP_SM_DEBOUNCING: {
            if (now_ms < s->next_read_ms) {
                break;
            }
            uint16_t counts = 0;
            if (!np_pbm1064_hal_adc_read_zone_id(slot, &counts)) {
                /* ADC timeout — restart from IDLE */
                s->state = NP_SM_IDLE;
                break;
            }
            s->adc_reads[s->read_count++] = counts;
            s->next_read_ms = now_ms + NP_PBM1064_DEBOUNCE_INTERVAL_MS;

            if (s->read_count < NP_PBM1064_DEBOUNCE_READS) {
                break;
            }

            /* Debounce complete — evaluate majority. */
            np_slot_type_t majority = debounce_majority(s);
            if (majority == NP_SLOT_BASE_MODULE) {
                /* Standard base module confirmed; I2C stays disabled. */
                s->slot_type = NP_SLOT_BASE_MODULE;
                s->state     = NP_SM_ACTIVE;
                if (insert_cb) {
                    insert_cb(slot);
                }
            } else if (majority == NP_SLOT_SMART) {
                /*
                 * Smart module confirmed by ZONE_ID debounce.
                 * Assert TIA gain switch BEFORE enabling I2C mux (NP-HW-HUB-001
                 * Rev B §5.1): InGaAs PD2× responsivity saturates the TIA at
                 * Rf = 47 kΩ; switch to Rf = 22 kΩ first.
                 */
                np_pbm1064_hal_tia_gain_set(slot, NP_TIA_GAIN_LOW);
                s->tia_gain = NP_TIA_GAIN_LOW;
                np_pbm1064_hal_i2c_mux_enable(slot, true);
                s->i2c_probed = false;
                s->state      = NP_SM_I2C_PROBING;
            } else {
                /* No majority or absent — return to IDLE. */
                s->state = NP_SM_IDLE;
            }
            break;
        }

        case NP_SM_I2C_PROBING: {
            bool ack = np_pbm1064_hal_i2c_probe(slot, NP_PBM1064_I2C_ADDR,
                                                  NP_PBM1064_I2C_PROBE_TIMEOUT_MS);
            if (!ack) {
                /* NAK or timeout — log SHDR fault, disable I2C mux and reset TIA
                 * gain to default, then back to IDLE. */
                np_pbm1064_shdr_fault_entry_t fe = {
                    .device_session_count = ctx->device_session_count,
                    .slot                 = slot,
                    .channel              = 0xFF,
                    .fault_reason         = NP_PBM1064_FAULT_I2C_LOST,
                    .status_reg_value     = 0xFF,
                };
                np_pbm1064_hal_shdr_log_fault(&fe);
                np_pbm1064_hal_i2c_mux_enable(slot, false);
                np_pbm1064_hal_tia_gain_set(slot, NP_TIA_GAIN_HIGH);
                s->tia_gain = NP_TIA_GAIN_HIGH;
                s->state    = NP_SM_IDLE;
                break;
            }
            s->slot_type  = NP_SLOT_SMART;
            s->i2c_probed = true;
            s->state      = NP_SM_ANNOUNCING;
            np_pbm1064_hal_zone_announce(slot);
            /* insert_cb fires after announcement audio. */
            if (insert_cb) {
                insert_cb(slot);
            }
            s->state = NP_SM_ACTIVE;
            break;
        }

        case NP_SM_ANNOUNCING:
            /* Announcement is handled asynchronously by np_za; state advances in
             * NP_SM_I2C_PROBING after zone_announce() returns. */
            break;

        case NP_SM_ACTIVE: {
            /* Poll for removal every NP_PBM1064_POLL_ACTIVE_MS. */
            if (now_ms < s->next_read_ms) {
                break;
            }
            s->next_read_ms = now_ms + NP_PBM1064_POLL_ACTIVE_MS;

            uint16_t counts = 0;
            if (!np_pbm1064_hal_adc_read_zone_id(slot, &counts)) {
                break;
            }
            if (np_pbm1064_classify_adc(counts) == NP_SLOT_ABSENT) {
                if (!s->removal_debounce_active) {
                    s->removal_debounce_active = true;
                    s->removal_since_ms = now_ms;
                }
                if (now_ms - s->removal_since_ms >= NP_PBM1064_REMOVAL_DEBOUNCE_MS) {
                    s->state = NP_SM_REMOVING;
                }
            } else {
                s->removal_debounce_active = false;
            }
            break;
        }

        case NP_SM_REMOVING: {
            /* Verify absence one more time before signalling removal. */
            uint16_t counts = 0;
            np_pbm1064_hal_adc_read_zone_id(slot, &counts);
            if (np_pbm1064_classify_adc(counts) != NP_SLOT_ABSENT) {
                /* Re-insertion mid-debounce — stay ACTIVE. */
                s->removal_debounce_active = false;
                s->state = NP_SM_ACTIVE;
                break;
            }
            if (s->slot_type == NP_SLOT_SMART) {
                /* Disable I2C mux first, then reset TIA gain to default (NP-HW-HUB-001
                 * Rev B §5.2): restores Rf = 47 kΩ ready for a subsequent base module. */
                np_pbm1064_hal_i2c_mux_enable(slot, false);
                np_pbm1064_hal_tia_gain_set(slot, NP_TIA_GAIN_HIGH);
            }
            if (ctx->remove_cb) {
                ctx->remove_cb(slot);
            }
            memset(s, 0, sizeof(*s));
            s->state    = NP_SM_IDLE;
            s->tia_gain = NP_TIA_GAIN_HIGH;
            break;
        }
        }
    }
}

np_slot_type_t np_pbm1064_detect_slot_type(const np_pbm1064_detect_ctx_t *ctx,
                                             uint8_t slot)
{
    if (slot >= NP_PBM1064_ZONE_COUNT) {
        return NP_SLOT_ABSENT;
    }
    return ctx->slots[slot].slot_type;
}

uint8_t np_pbm1064_detect_smart_mask(const np_pbm1064_detect_ctx_t *ctx)
{
    uint8_t mask = 0;
    for (uint8_t i = 0; i < NP_PBM1064_ZONE_COUNT; i++) {
        if (ctx->slots[i].state     == NP_SM_ACTIVE &&
            ctx->slots[i].slot_type == NP_SLOT_SMART) {
            mask |= (uint8_t)(1U << i);
        }
    }
    return mask;
}
