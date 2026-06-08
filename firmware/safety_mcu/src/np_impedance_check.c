/*
 * NeuroPulse Safety MCU — SW01-M06: Contact Impedance Check
 * Document: NP-SW-001 Rev A, NP-FMEA-001 Rev A §SW01-M06
 *
 * Performs a 1kHz AC impedance check before enabling VNS and tDCS channels.
 * Injects a test current for NP_IMPEDANCE_TEST_MS and measures the
 * voltage response.  Rejects enable if Z > NP_IMPEDANCE_MAX_OHM.
 *
 * The safety MCU drives the test signal via a dedicated low-level test
 * current source.  The impedance measurement is performed while stimulation
 * output is disabled (pre-enable check, not during session).
 */

#include "np_safety_config.h"
#include "np_safety_protocol.h"
#include <stdint.h>
#include <stdbool.h>

/* ── HAL stubs ───────────────────────────────────────────────────────────── */
extern uint32_t np_hal_get_tick_ms(void);
extern void     np_hal_impedance_start_test(uint8_t channel);
extern bool     np_hal_impedance_result_ready(uint8_t channel);
extern uint32_t np_hal_impedance_read_ohm(uint8_t channel);

/* ── Module state ─────────────────────────────────────────────────────────── */
#define NP_IMP_CHANNELS     4U  /* VNS_HRV, tDCS, BES_TACS, CVNS */
static const uint8_t k_imp_slot[NP_IMP_CHANNELS] = {
    7U,  /* VNS_HRV  */
    6U,  /* tDCS     */
    5U,  /* BES_TACS */
    10U, /* CVNS     */
};
static const uint16_t k_imp_en_bit[NP_IMP_CHANNELS] = {
    NP_SAFETY_EN_VNS_HRV,
    NP_SAFETY_EN_TDCS,
    NP_SAFETY_EN_BES_TACS,
    NP_SAFETY_EN_CVNS,
};

static bool     s_test_pending[NP_IMP_CHANNELS];
static uint32_t s_test_start_ms[NP_IMP_CHANNELS];

np_safe_status_t np_impedance_check_init(void)
{
    for (uint8_t i = 0U; i < NP_IMP_CHANNELS; i++) {
        s_test_pending[i]  = false;
        s_test_start_ms[i] = 0U;
    }
    return NP_SAFE_OK;
}

/*
 * np_impedance_check_request — request a pre-enable impedance check for all
 * channels being added to the granted_mask this session.  Called by the main
 * loop when session_active transitions 0→1.
 */
void np_impedance_check_request(uint16_t requested_mask)
{
    for (uint8_t i = 0U; i < NP_IMP_CHANNELS; i++) {
        if ((requested_mask & k_imp_en_bit[i]) != 0U) {
            np_hal_impedance_start_test(i);
            s_test_pending[i]  = true;
            s_test_start_ms[i] = 0U;  /* HAL drives timing */
        }
    }
}

/*
 * np_impedance_check_poll — poll pending impedance tests.
 * Clears the enable bit for any channel that fails the check.
 */
void np_impedance_check_poll(np_safety_state_t *state)
{
    for (uint8_t i = 0U; i < NP_IMP_CHANNELS; i++) {
        if (!s_test_pending[i]) {
            continue;
        }
        if (np_hal_impedance_result_ready(i)) {
            s_test_pending[i] = false;
            uint32_t z_ohm    = np_hal_impedance_read_ohm(i);

            if (z_ohm > NP_IMPEDANCE_MAX_OHM) {
                state->granted_mask &= (uint16_t)~k_imp_en_bit[i];
                state->status       |= NP_SAFETY_STATUS_IMPEDANCE;
                state->fault_slot    = k_imp_slot[i];
            } else {
                /* Impedance check passed — clear the IMPEDANCE status bit so the
                 * hub does not see a persistent fault after a recheck succeeds.
                 * Without this clear, a single impedance failure latches the bit
                 * permanently across all subsequent session attempts. */
                state->status &= (uint8_t)~NP_SAFETY_STATUS_IMPEDANCE;
            }
        }
    }
}
