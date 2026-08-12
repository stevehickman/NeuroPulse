/* Document: NP-FW-EMMC-002 Rev 1 §E */
/*
 * NeurOne EDF+ Privacy Header — Writer and Validator API
 * Document: NP-FW-EMMC-002 Rev 1 §E
 */

#ifndef NP_EDF_WRITER_H
#define NP_EDF_WRITER_H

#include <stdint.h>
#include <stdbool.h>
#include "np_edf_types.h"

/* Fills a 256-byte EDF+ header with privacy-preserving values per NP-FW-EMMC-002 §E.2.
   uhdr_token: 32-byte binary UHDR token (first 7 bytes -> 14 hex chars used)
   fw_version: NeurOne firmware version string (e.g. "1.0.0")
   session_ts: UTC session start time (seconds since epoch) -- used for DD-MMM-YYYY in recording ID
               Pass 0 to use placeholder "01-JAN-1970".
*/
np_edf_status_t np_edf_write_header(np_edf_header_t *header,
                                     const uint8_t *uhdr_token,
                                     const char *fw_version,
                                     uint32_t session_ts);

/* Validates that a header conforms to NP-FW-EMMC-002 §E.2 privacy policy.
   Returns NP_EDF_OK if all privacy fields are correct.
   Returns NP_EDF_ERR_PRIVACY_HEADER or NP_EDF_ERR_REAL_NAME on violation. */
np_edf_status_t np_edf_validate_privacy_header(const np_edf_header_t *header);

/* Returns true if the patient_name portion of local_patient_id contains
   alphabetic characters that do not spell "NeurOne_User". */
bool np_edf_header_contains_real_name(const np_edf_header_t *header);

#endif /* NP_EDF_WRITER_H */
