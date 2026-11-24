/* Document: NP-FW-EMMC-002 Rev 1 §E */
/*
 * NeurOne EDF+ Privacy Header — Layout Constants
 * Document: NP-FW-EMMC-002 Rev 1 §E (EDF+ patient header policy)
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

/* 0xE1 was NP_EDF_SHDR_EV_PRIVACY_HEADER_VIOLATION, an SHDR fault written once
   per EDF+ file the research-extract gate excluded.  RETIRED 2026-11-21
   (OI-PRIV-38 (NP-PRIV-ANALYSIS-004) part 2): any such record tells the fleet
   that this device ran a research extract.  The gate now writes nothing to SHDR.
   The code is not reused for anything else. */

/* ── Research header (NP-FW-EMMC-002 §E.5, OI-PRIV-26 (NP-PRIV-ANALYSIS-004)) ───────────────────────
   A file that goes into a research extract carries a different header from the
   person's own local file (§E.2): no device-derived code, and no date or time
   finer than the study's rounding.  The local file is unchanged. */

/* Finest date rounding a research header may carry, in days.  CLAUDE.md §5.3
   ("date rounding >= 1 week", the principal's locked decision; no external
   derivation, UC-201); NP-FW-ANON-001 §4 enforces the same floor on the
   study descriptor's date_rounding_days. */
#define NP_EDF_RESEARCH_MIN_ROUNDING_DAYS   7U

/* Rounding windows are counted from Monday 1970-01-05 (day 4 after the epoch),
   so a 7-day window is an ISO 8601 week, which starts on Monday
   (NP-FW-ANON-001 FAI-ANON-06: week values align to ISO week boundaries). */
#define NP_EDF_RESEARCH_ANCHOR_DAY          4

/* Per-study patient code: "NS" + 14 lowercase hex chars from 7 TRNG bytes.
   The prefix differs from the local "NP" code so the two can never be confused;
   the bytes are random per (device, study) and never derived from the token.
   Why 7 bytes: the width is fixed to match the local code (16 chars), so the
   research header keeps the local field layout; unlinkability comes from the
   bytes being random and per study, not from the width. */
#define NP_EDF_STUDY_CODE_LEN               16
#define NP_EDF_STUDY_CODE_RANDOM_BYTES      7

/* Unknown start date, the EDF+ convention (EDF+ spec §2.1.3.2 "Startdate X";
   the header startdate field cannot be blank, and 01.01.85 is the first date
   its two-digit year can express under the 1985 clipping rule, §2.1.3.3). */
#define NP_EDF_BLANK_STARTDATE              "01.01.85"
#define NP_EDF_RESEARCH_STARTTIME           "00.00.00"

/* EDF two-digit-year clipping window (EDF+ spec §2.1.3.3): 85..99 -> 19yy,
   00..84 -> 20yy.  A date outside it cannot be written, so it is blanked. */
#define NP_EDF_CLIP_YEAR_MIN                1985
#define NP_EDF_CLIP_YEAR_MAX                2084

#endif /* NP_EDF_CONFIG_H */
