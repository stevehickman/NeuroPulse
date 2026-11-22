/* Document: NP-FW-EMMC-002 §E.5 */
/*
 * NeurOne EDF+ Privacy Header — Research Header (OI-PRIV-26 (NP-PRIV-ANALYSIS-004))
 * Document: NP-FW-EMMC-002 §E.5; CLAUDE.md §5.3
 *
 * Two headers, two paths:
 *
 *   - LOCAL (§E.2, np_edf_write_header): the file kept in the person's UHDR and
 *     exported by the person.  A valid EDF+ header with the exact start date and
 *     time and the device's "NP" patient code.  It is the person's own record and
 *     is unchanged here.
 *
 *   - RESEARCH (§E.5, this module): the header a file carries when it goes into a
 *     research extract.  CLAUDE.md §5.3 requires date rounding of at least a week
 *     and no linkage, so the research header has
 *       * patient code 'X', or a per-study random code ("NS" + 14 hex) — never
 *         the device "NP" code, which is the same in every extract;
 *       * sex, birth date and name all 'X';
 *       * a start date that is blank ("Startdate X", header 01.01.85) or floored
 *         to the study's rounding window, and a start time of 00.00.00;
 *       * a recording ID of exactly "Startdate <date|X> X X X" (no hospital,
 *         technician or equipment text).
 *     It is still a valid EDF+ header, so research tools read it.
 *
 * np_edf_validate_research_header() is the §E.4 gate's check.  It parses every
 * byte of the 256-byte main header and accepts only the forms above.
 *
 * Not covered here (NP-FW-EMMC-002 §E.5, OI-PRIV-26 (NP-PRIV-ANALYSIS-004) remainder): the per-signal
 * header records and the EDF+ annotation channel that follow the main header.
 */

#ifndef NP_EDF_RESEARCH_H
#define NP_EDF_RESEARCH_H

#include <stdint.h>
#include <stdbool.h>
#include "np_edf_types.h"

/* The study's header policy.  One per study, from the signed descriptor and the
 * device's per-study record.
 *   study_code          NULL: the patient code must be 'X'.  Otherwise a
 *                       NUL-terminated per-study code made by
 *                       np_edf_make_study_code(); the header's code must be
 *                       'X' or exactly this.
 *   date_rounding_days  0: the start date must be blank.  Otherwise at least
 *                       NP_EDF_RESEARCH_MIN_ROUNDING_DAYS (the descriptor's
 *                       date_rounding_days); 1..6 is refused. */
typedef struct {
    const char *study_code;
    uint16_t    date_rounding_days;
} np_edf_research_policy_t;

/* Writes "NS" + 14 lowercase hex chars of `random_bytes` (NUL-terminated) into
 * out[NP_EDF_STUDY_CODE_LEN + 1].  The caller draws the bytes from the TRNG once
 * per (device, study) and keeps them only in that study's record; they must
 * never be derived from the UHDR or warranty token. */
np_edf_status_t np_edf_make_study_code(const uint8_t random_bytes[NP_EDF_STUDY_CODE_RANDOM_BYTES],
                                       char out[NP_EDF_STUDY_CODE_LEN + 1]);

/* Builds the research header for a local header.  The format fields (version,
 * header bytes, reserved, record count, duration, signal count) are copied; the
 * identification fields, start date and start time are rewritten under
 * `policy`.  A local date that cannot be read, or whose rounded value falls
 * outside 1985..2084, is written blank.  The result passes
 * np_edf_validate_research_header(out, policy) whenever the copied format
 * fields are valid EDF. */
np_edf_status_t np_edf_make_research_header(const np_edf_header_t *local,
                                            const np_edf_research_policy_t *policy,
                                            np_edf_header_t *out);

/* Returns NP_EDF_OK only if `header` is a research header under `policy`:
 *   NP_EDF_ERR_INVALID_ARG       header or policy NULL, a malformed study code,
 *                                or a rounding of 1..6 days;
 *   NP_EDF_ERR_RESEARCH_LINKAGE  the patient code is neither 'X' nor the study's;
 *   NP_EDF_ERR_PRIVACY_HEADER    any other byte of the patient-ID field, or a
 *                                format field, is not the fixed form;
 *   NP_EDF_ERR_RECORDING_ID      the recording ID is not the fixed form;
 *   NP_EDF_ERR_RESEARCH_DATE     the date or time is finer than the policy, or
 *                                the two date fields disagree. */
np_edf_status_t np_edf_validate_research_header(const np_edf_header_t *header,
                                                const np_edf_research_policy_t *policy);

#endif /* NP_EDF_RESEARCH_H */
