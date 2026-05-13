#include "np_pbm1064.h"
#include "np_pbm1064_config.h"
#include "np_zone_announce.h"   /* bone conduction callback */
#include <string.h>

/* HAL stubs — implemented by platform team (OI-PBM-01, OI-PBM-02) */
extern uint16_t np_adc_read_pd(uint8_t zone, uint8_t pd_channel);
extern np_err_t np_i2c_write(uint8_t slot, uint8_t addr, uint8_t reg, uint8_t val);
extern np_err_t np_i2c_read(uint8_t slot, uint8_t addr, uint8_t reg, uint8_t *val);
extern void     np_delay_ms(uint32_t ms);
extern uint32_t np_tick_ms(void);

/* ZONE_ID ADC channel — same LPADC1 ch12 as base module (NP-FW-ZA-001 §2.1) */
#define ZONE_ID_ADC_CHANNEL  12u

static uint16_t read_zone_id_adc(uint8_t slot)
{
    /* Stub: platform team maps slot index to LPADC1 multiplexer setting (OI-PBM-01) */
    return np_adc_read_pd(slot, ZONE_ID_ADC_CHANNEL);
}

static np_slot_type_t classify_zone_id(uint16_t adc, uint8_t slot)
{
    if (adc >= 4000u) {
        return NP_SLOT_EMPTY;
    }
    if (adc < NP_PBM1064_SMART_MODULE_ADC_THRESHOLD) {
        return NP_SLOT_SMART_MODULE;
    }
    /* Base module range — zone_announce firmware handles classification */
    (void)slot;
    return NP_SLOT_BASE_MODULE;
}

/* 3-read debounce per RISK-18: ≥2/3 consistent reads required */
static np_slot_type_t debounce_zone_id(uint8_t slot)
{
    np_slot_type_t reads[NP_PBM1064_DEBOUNCE_READS];
    uint8_t i;

    np_delay_ms(NP_PBM1064_INSERTION_SETTLE_MS);

    for (i = 0; i < NP_PBM1064_DEBOUNCE_READS; i++) {
        reads[i] = classify_zone_id(read_zone_id_adc(slot), slot);
        if (i < NP_PBM1064_DEBOUNCE_READS - 1u) {
            np_delay_ms(NP_PBM1064_DEBOUNCE_INTERVAL_MS);
        }
    }

    /* Majority vote */
    uint8_t smart_count = 0u, base_count = 0u, empty_count = 0u;
    for (i = 0; i < NP_PBM1064_DEBOUNCE_READS; i++) {
        if (reads[i] == NP_SLOT_SMART_MODULE) smart_count++;
        else if (reads[i] == NP_SLOT_BASE_MODULE) base_count++;
        else if (reads[i] == NP_SLOT_EMPTY) empty_count++;
    }

    if (smart_count >= NP_PBM1064_DEBOUNCE_MAJORITY) return NP_SLOT_SMART_MODULE;
    if (base_count  >= NP_PBM1064_DEBOUNCE_MAJORITY) return NP_SLOT_BASE_MODULE;
    if (empty_count >= NP_PBM1064_DEBOUNCE_MAJORITY) return NP_SLOT_EMPTY;
    return NP_SLOT_FAULT;
}

bool np_pbm1064_i2c_probe(uint8_t slot)
{
    uint8_t dummy = 0u;
    uint32_t start = np_tick_ms();
    np_err_t err;

    do {
        err = np_i2c_read(slot, NP_PBM1064_I2C_ADDR, NP_PBM1064_REG_STATUS, &dummy);
        if (err == NP_OK) {
            return true;
        }
    } while ((np_tick_ms() - start) < NP_PBM1064_I2C_PROBE_TIMEOUT_MS);

    return false;
}

void np_pbm1064_detect_all(np_pbm1064_session_t *session)
{
    uint8_t slot;

    session->smart_module_mask = 0u;

    for (slot = 0u; slot < NP_PBM1064_ZONE_COUNT; slot++) {
        np_slot_type_t type = debounce_zone_id(slot);
        session->zone[slot].slot_type = type;

        if (type == NP_SLOT_SMART_MODULE) {
            /* Enable LPI2C3 GPIO mux for this slot, then probe driver IC */
            if (np_pbm1064_i2c_probe(slot)) {
                session->smart_module_mask |= (uint8_t)(1u << slot);
                session->zone[slot].driver_ready = true;
                /* Bone conduction announcement via zone_announce callback:
                 * pass slot index so np_zone_announce uses position-based name */
                np_zone_announce_smart_module(slot);
            } else {
                session->zone[slot].slot_type = NP_SLOT_FAULT;
                session->zone[slot].driver_ready = false;
                np_zone_announce_error(slot);
            }
        } else {
            session->zone[slot].driver_ready = false;
        }
    }
}
