/* Document: NP-FW-EMMC-002 Rev 1 §E.4 */
/*
 * NeurOne EDF+ Privacy Header — Anonymization-Pipeline File Gate
 * Document: NP-FW-EMMC-002 Rev 1 §E.4
 *
 * See np_edf_anon_gate.h for the contract.  Implements the per-file, exclude-
 * not-abort staging filter §E.4 requires: validate each candidate EDF+ header,
 * drop the ones that violate the §E.2 privacy policy, and keep going.  Nothing
 * about the pass is written to SHDR (OI-PRIV-38 (NP-PRIV-ANALYSIS-004) part 2;
 * see the header).
 *
 * No heap, no libc dependency — a straight bounded loop over the caller's
 * arrays.  The header check is np_edf_validate_research_header() (§E.5), under
 * the study's policy.  It was np_edf_validate_privacy_header(), the local §E.2
 * check, which passed exact dates and the device code (OI-PRIV-26 (NP-PRIV-ANALYSIS-004)).
 */

#include "np_edf_anon_gate.h"

np_edf_status_t np_edf_anon_gate_filter(const np_edf_header_t *headers,
                                        uint32_t               count,
                                        const np_edf_research_policy_t *policy,
                                        bool                  *included,
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

    /* With files to process, the two arrays and the policy are mandatory. */
    if (headers == NULL || included == NULL || policy == NULL) {
        return NP_EDF_ERR_INVALID_ARG;
    }

    uint32_t inc = 0U;
    uint32_t exc = 0U;

    for (uint32_t i = 0U; i < count; i++) {
        if (np_edf_validate_research_header(&headers[i], policy) == NP_EDF_OK) {
            /* A research header under this study's policy (§E.5). */
            included[i] = true;
            inc++;
        } else {
            /* §E.4: exclude this file and CONTINUE.  A header violation must
               never abort the whole extract, and leaves no SHDR record. */
            included[i] = false;
            exc++;
        }
    }

    if (result != NULL) {
        result->included = inc;
        result->excluded = exc;
    }

    /* A completed pass is a success even when files were excluded. */
    return NP_EDF_OK;
}
