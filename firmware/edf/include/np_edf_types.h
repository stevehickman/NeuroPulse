/* Document: NP-FW-EMMC-002 Rev A §E */
/*
 * NeuroPulse EDF+ Privacy Header — Type Definitions
 * Document: NP-FW-EMMC-002 Rev A §E
 */

#ifndef NP_EDF_TYPES_H
#define NP_EDF_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "np_edf_config.h"

/* ── Status codes ───────────────────────────────────────────────────────────── */
typedef enum {
    NP_EDF_OK = 0,
    NP_EDF_ERR_INVALID_ARG    = -1,
    NP_EDF_ERR_PRIVACY_HEADER = -2,   /* validator found PII field violation */
    NP_EDF_ERR_REAL_NAME      = -3,   /* patient_name contains alphabetics beyond NeuroPulse_User */
    NP_EDF_ERR_TOKEN_LEN      = -4,
} np_edf_status_t;

/* 256-byte EDF+ header buffer — raw ASCII, NOT null-terminated.
 *
 * Field order and widths follow the EDF+ specification §2.1.3 exactly, so the
 * struct overlays the on-disk header byte-for-byte.  Packed to guarantee no
 * padding is inserted between fields (the compile-time check below enforces the
 * 256-byte total).
 */
typedef struct __attribute__((packed)) {
    char version[NP_EDF_VERSION_LEN];
    char local_patient_id[NP_EDF_PATIENT_ID_LEN];
    char local_recording_id[NP_EDF_RECORDING_ID_LEN];
    char startdate[NP_EDF_STARTDATE_LEN];
    char starttime[NP_EDF_STARTTIME_LEN];
    char header_bytes[NP_EDF_HEADER_BYTES_LEN];
    char reserved[NP_EDF_RESERVED_LEN];
    char num_data_records[NP_EDF_NUM_RECORDS_LEN];
    char duration[NP_EDF_DURATION_LEN];
    char num_signals[NP_EDF_NUM_SIGNALS_LEN];
} np_edf_header_t;

/* Compile-time guarantee the packed header is exactly 256 bytes. */
typedef char _np_edf_hdr_size_check[(sizeof(np_edf_header_t) == NP_EDF_HEADER_SIZE) ? 1 : -1];

#endif /* NP_EDF_TYPES_H */
