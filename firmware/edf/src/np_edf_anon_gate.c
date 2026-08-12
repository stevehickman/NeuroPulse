/* Document: NP-FW-EMMC-002 Rev 1 §E.4 */
/*
 * NeurOne EDF+ Privacy Header — Anonymization-Pipeline File Gate
 * Document: NP-FW-EMMC-002 Rev 1 §E.4
 *
 * See np_edf_anon_gate.h for the contract.  Implements the per-file, exclude-
 * not-abort staging filter §E.4 requires: validate each candidate EDF+ header,
 * drop the ones that violate the §E.2 privacy policy, log an SHDR fault per
 * exclusion (via injected callback), and keep going.
 *
 * No heap, no libc dependency — a straight bounded loop over the caller's
 * arrays.  The actual header check is np_edf_validate_privacy_header().
 */

#include "np_edf_anon_gate.h"
#include "np_edf_writer.h"   /* np_edf_validate_privacy_header */

np_edf_status_t np_edf_anon_gate_filter(const np_edf_header_t *headers,
                                        uint32_t               count,
                                        bool                  *included,
                                        np_edf_shdr_fault_cb   fault_cb,
                                        void                  *user,
                                        np_edf_gate_result_t  *result)
{
    /* Zero the tally up front so every return path leaves it well-defined. */
    if (result != NULL) {
        result->included = 0U;
        result->excluded = 0U;
    }

    /* An empty candidate set is a valid no-op — nothing to include or exclude. */
    if (count == 0U) {
        return NP_EDF_OK;
    }

    /* With files to process, the two arrays are mandatory. */
    if (headers == NULL || included == NULL) {
        return NP_EDF_ERR_INVALID_ARG;
    }

    uint32_t inc = 0U;
    uint32_t exc = 0U;

    for (uint32_t i = 0U; i < count; i++) {
        if (np_edf_validate_privacy_header(&headers[i]) == NP_EDF_OK) {
            /* Passes §E.2 — eligible for the extract. */
            included[i] = true;
            inc++;
        } else {
            /* §E.4: exclude this file, log the SHDR fault, and CONTINUE.
               A header violation must never abort the whole extract. */
            included[i] = false;
            exc++;
            if (fault_cb != NULL) {
                fault_cb(i, NP_EDF_SHDR_EV_PRIVACY_HEADER_VIOLATION, user);
            }
        }
    }

    if (result != NULL) {
        result->included = inc;
        result->excluded = exc;
    }

    /* A completed pass is a success even when files were excluded. */
    return NP_EDF_OK;
}
