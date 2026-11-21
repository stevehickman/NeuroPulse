/* Document: NP-FW-EMMC-002 Rev 1 §E */
/*
 * NeurOne EDF+ Privacy Header — Validator
 * Document: NP-FW-EMMC-002 Rev 1 §E.2
 *
 * Confirms a LOCAL header (§E.2: the file kept in the person's UHDR) carries no
 * name, birth date or sex.  It does not make a header fit for research: a local
 * header keeps the exact start date and time and the device's "NP" code, which
 * is the same in every file.  The research gate (§E.4) uses
 * np_edf_validate_research_header() (np_edf_research.c, §E.5) instead
 * (OI-PRIV-26).
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
    /* The rest of the code is 14 lowercase hex chars, and the subfields are
       separated by single spaces (OI-PRIV-26: the whole field is checked, so
       text cannot ride in the code or the separators). */
    for (size_t i = 2U; i < (size_t)NP_EDF_PATIENT_CODE_LEN; i++) {
        const char c = header->local_patient_id[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return NP_EDF_ERR_PRIVACY_HEADER;
        }
    }
    if (header->local_patient_id[NP_EDF_PATIENT_CODE_LEN] != ' ' ||
        header->local_patient_id[NP_EDF_SEX_OFFSET + 1] != ' ' ||
        header->local_patient_id[NP_EDF_DOB_OFFSET + 1] != ' ') {
        return NP_EDF_ERR_PRIVACY_HEADER;
    }
    return NP_EDF_OK;
}
