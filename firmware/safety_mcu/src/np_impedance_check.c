/*
 * NeurOne Safety MCU — SW01-M06: Contact Impedance Check
 * Document: NP-SW-001 Rev 1, NP-FMEA-001 Rev 1 §SW01-M06
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
#include "np_safety_hal.h"
#include "np_safety_protocol.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* HAL: np_hal_get_tick_ms, np_hal_impedance_start_test / _result_ready /
 * _read_ohm / _read_cvns_electrode_ohm — all declared in np_safety_hal.h,
 * where the ohm units, the 0..3 channel indexing and the OI-CVNS-HUB-11
 * additive-not-gating property of the per-electrode read are recorded.     */

/* ── Module state ─────────────────────────────────────────────────────────── */
#define NP_IMP_CHANNELS     4U  /* VNS_HRV, tDCS, BES_TACS, CVNS */
#define NP_IMP_CVNS_INDEX   3U  /* index of CVNS within the arrays below        */
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

/* OI-CVNS-HUB-11: latest per-electrode CVNS impedance for the hub cross-check. */
static uint16_t s_cvns_kohm_x100[NP_SAFETY_IMP_CVNS_ELECTRODES];
static bool     s_cvns_report_valid;   /* a CVNS measurement completed this session */

/* ohm → kΩ×100 with round-to-nearest, clamped to uint16 range. */
static uint16_t ohm_to_kohm_x100(uint32_t ohm)
{
    uint32_t v = (ohm * 100U + 500U) / 1000U;   /* ohm → kΩ×100, rounded */
    if (v > 0xFFFFU) { v = 0xFFFFU; }
    return (uint16_t)v;
}

np_safe_status_t np_impedance_check_init(void)
{
    for (uint8_t i = 0U; i < NP_IMP_CHANNELS; i++) {
        s_test_pending[i]  = false;
        s_test_start_ms[i] = 0U;
    }
    for (uint8_t e = 0U; e < NP_SAFETY_IMP_CVNS_ELECTRODES; e++) {
        s_cvns_kohm_x100[e] = 0U;
    }
    s_cvns_report_valid = false;
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
    /* OI-CVNS-HUB-11: a fresh CVNS check is starting — invalidate the previous
     * per-electrode report until this measurement completes, so the hub never
     * cross-validates against a stale reading from a prior session. */
    if ((requested_mask & NP_SAFETY_EN_CVNS) != 0U) {
        s_cvns_report_valid = false;
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

            /* OI-CVNS-HUB-11: for the CVNS channel, capture the per-electrode
             * impedance for the hub cross-validation report.  This is a separate
             * per-electrode measurement additive to (not replacing) the
             * single-value enable gate above, so the Class C interlock decision
             * is unchanged.  Captured regardless of pass/fail so the hub can
             * cross-check even a marginal reading. */
            if (i == NP_IMP_CVNS_INDEX) {
                for (uint8_t e = 0U; e < NP_SAFETY_IMP_CVNS_ELECTRODES; e++) {
                    s_cvns_kohm_x100[e] =
                        ohm_to_kohm_x100(np_hal_impedance_read_cvns_electrode_ohm(e));
                }
                s_cvns_report_valid = true;
            }
        }
    }
}

/*
 * np_impedance_check_build_cvns_report — populate the extended MCU→hub impedance
 * report (OI-CVNS-HUB-11) carried in the spare bytes of the heartbeat window.
 *
 * Fills *out with magic + valid flag + per-electrode kΩ×100 + checksum when a
 * CVNS measurement has completed this session; otherwise clears *out (magic 0)
 * so the hub ignores it.  Returns true iff the report carries a valid reading.
 */
bool np_impedance_check_build_cvns_report(np_safety_imp_report_t *out)
{
    uint16_t sum = 0U;
    const uint8_t *b;
    uint8_t i;

    if (out == NULL) {
        return false;
    }

    if (!s_cvns_report_valid) {
        /* No measurement yet — zero the whole report so magic == 0 and the hub
         * treats the tail as unpopulated. */
        for (i = 0U; i < NP_SAFETY_IMP_REPORT_LEN; i++) {
            ((uint8_t *)out)[i] = 0U;
        }
        return false;
    }

    out->magic = NP_SAFETY_IMP_REPORT_MAGIC;
    out->flags = NP_SAFETY_IMP_FLAG_CVNS_VALID;
    for (i = 0U; i < NP_SAFETY_IMP_CVNS_ELECTRODES; i++) {
        out->cvns_kohm_x100[i] = s_cvns_kohm_x100[i];
    }

    /* Checksum over bytes [0..5] (all fields except the trailing checksum). */
    b = (const uint8_t *)out;
    for (i = 0U; i < (NP_SAFETY_IMP_REPORT_LEN - 2U); i++) {
        sum += b[i];
    }
    out->checksum = sum;
    return true;
}
