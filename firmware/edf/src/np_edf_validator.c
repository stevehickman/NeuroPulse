/* Document: NP-FW-EMMC-002 Rev A §E */
/*
 * NeuroPulse EDF+ Privacy Header — Validator
 * Document: NP-FW-EMMC-002 Rev A §E.2
 *
 * Confirms a header conforms to the privacy policy before any research extract
 * is transmitted.  This is the gate in the research anonymisation pipeline: a
 * header that fails here must not be written out.
 */

#include "np_edf_writer.h"

np_edf_status_t np_edf_validate_privacy_header(const np_edf_header_t *header)
{
    if (header == NULL) {
        return NP_EDF_ERR_INVALID_ARG;
    }
    /* Check sex field is 'X' */
    if (header->local_patient_id[NP_EDF_SEX_OFFSET] != 'X') {
        return NP_EDF_ERR_PRIVACY_HEADER;
    }
    /* Check birthdate field is 'X' */
    if (header->local_patient_id[NP_EDF_DOB_OFFSET] != 'X') {
        return NP_EDF_ERR_PRIVACY_HEADER;
    }
    /* Check patient name does not contain real name */
    if (np_edf_header_contains_real_name(header)) {
        return NP_EDF_ERR_REAL_NAME;
    }
    /* Check patient code starts with "NP" */
    if (header->local_patient_id[0] != 'N' || header->local_patient_id[1] != 'P') {
        return NP_EDF_ERR_PRIVACY_HEADER;
    }
    return NP_EDF_OK;
}
