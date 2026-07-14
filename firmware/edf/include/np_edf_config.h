/* Document: NP-FW-EMMC-002 Rev A §E */
/*
 * NeurOne EDF+ Privacy Header — Layout Constants
 * Document: NP-FW-EMMC-002 Rev A §E (EDF+ patient header policy)
 * Target: NXP i.MX RT1062 (Cortex-M7) — also host-compilable for tests.
 *
 * EDF+ header layout per the EDF+ specification §2.1.3.  The 256-byte fixed
 * header precedes the per-signal header section.  This module owns only the
 * 256-byte main header — the privacy-relevant fields live in the local
 * patient identification field (80 chars).
 *
 * Privacy policy (NP-FW-EMMC-002 §E.2): no real patient name, no date of birth,
 * no sex code.  The local patient id encodes an opaque 16-char patient code
 * ("NP" + 14 hex chars derived from the UHDR token) with sex and birthdate
 * fields set to 'X' and the patient-name subfield fixed to "NeurOne_User".
 */

#ifndef NP_EDF_CONFIG_H
#define NP_EDF_CONFIG_H

/* EDF+ header is exactly 256 bytes */
#define NP_EDF_HEADER_SIZE          256

/* Field widths in the header (all ASCII, space-padded) */
#define NP_EDF_VERSION_LEN           8   /* "0       " */
#define NP_EDF_PATIENT_ID_LEN       80
#define NP_EDF_RECORDING_ID_LEN     80
#define NP_EDF_STARTDATE_LEN         8   /* DD.MM.YY */
#define NP_EDF_STARTTIME_LEN         8   /* HH.MM.SS */
#define NP_EDF_HEADER_BYTES_LEN      8   /* ASCII number of header bytes */
#define NP_EDF_RESERVED_LEN         44
#define NP_EDF_NUM_RECORDS_LEN       8
#define NP_EDF_DURATION_LEN          8
#define NP_EDF_NUM_SIGNALS_LEN       4

/* Offsets within the 256-byte header */
#define NP_EDF_OFF_VERSION           0
#define NP_EDF_OFF_PATIENT_ID        8
#define NP_EDF_OFF_RECORDING_ID     88
#define NP_EDF_OFF_STARTDATE       168
#define NP_EDF_OFF_STARTTIME       176
#define NP_EDF_OFF_HEADER_BYTES    184
#define NP_EDF_OFF_RESERVED        192
#define NP_EDF_OFF_NUM_RECORDS     236
#define NP_EDF_OFF_DURATION        244
#define NP_EDF_OFF_NUM_SIGNALS     252

/* Privacy policy: NP prefix + 14 hex chars from first 7 bytes of UHDR token */
#define NP_EDF_TOKEN_HEX_CHARS      14
#define NP_EDF_PATIENT_CODE_LEN     16  /* "NP" + 14 hex chars */

/* Number of UHDR-token bytes consumed to form the 14 hex chars (2 chars/byte) */
#define NP_EDF_TOKEN_BYTES_USED     7

/* Offsets within local patient id field (EDF+ §2.1.3.1 subfields):
   format: "patient_code sex birthdate patient_name" space-separated */
/* These are character offsets within the 80-char patient_id field */
#define NP_EDF_SEX_OFFSET           17   /* after "NP" + 14 hex + " " */
#define NP_EDF_DOB_OFFSET           19   /* after sex + " " */
#define NP_EDF_NAME_OFFSET          21   /* after birthdate + " " */

/* Fixed patient-name subfield value (no real name ever written). */
#define NP_EDF_PATIENT_NAME         "NeurOne_User"

/* SHDR fault event code (NP-FW-EMMC-002 §E.4): logged when a candidate EDF+
   file is EXCLUDED from a research extract for a §E.2 privacy-header violation.
   The edf module does not call the SHDR logger directly (it stays host-testable
   and hub-independent); the anonymization-pipeline integration site maps this
   code onto np_log_shdr_fault(). Distinct from the CVNS 0xC1..0xC7 range in
   np_hub_config.h. The event carries NO user biology — only a device-condition
   code and an opaque file ordinal. */
#define NP_EDF_SHDR_EV_PRIVACY_HEADER_VIOLATION   0xE1U

#endif /* NP_EDF_CONFIG_H */
