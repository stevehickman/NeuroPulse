/* Document: NP-FW-EMMC-002 Rev A §E */
/*
 * NeurOne EDF+ Privacy Header — Writer
 * Document: NP-FW-EMMC-002 Rev A §E.2
 *
 * Produces a privacy-preserving EDF+ main header.  No real patient name, no
 * date of birth, no sex code ever appears: the local patient identification
 * field carries an opaque 16-char patient code ("NP" + 14 hex chars derived
 * from the UHDR token), with sex and birthdate fixed to 'X' and the patient
 * name fixed to "NeurOne_User".
 *
 * Host vs target string and time handling:
 *   - On NPTEST_HOST, <string.h> and <time.h> are available and used.
 *   - On target (ffreestanding), all string and date arithmetic is done by
 *     the file-local helpers below — no libc string/time dependency.
 *
 * No heap.  All scratch buffers are stack-allocated.  Header fields are
 * fixed-width ASCII and are NOT null-terminated (EDF requirement).
 */

#include "np_edf_writer.h"

/* This translation unit is fully self-contained: all string and date/time
   operations are file-local integer routines, so neither <string.h> nor
   <time.h> is needed on host or target.  Using a single integer gmtime path
   on both builds keeps the emitted header byte-identical everywhere. */

/* ── File-local string helpers (target-safe; reused on host) ─────────────────── */

/* memset replacement — file-local so the target build needs no libc string. */
static void np_edf_fill(char *dst, char value, size_t n)
{
    for (size_t i = 0U; i < n; i++) {
        dst[i] = value;
    }
}

/* memcpy replacement (bytes), file-local. */
static void np_edf_copy(char *dst, const char *src, size_t n)
{
    for (size_t i = 0U; i < n; i++) {
        dst[i] = src[i];
    }
}

/* strlen replacement for a bounded NUL-terminated C string.
   Returns the length, capped at max (does not read past max). */
static size_t np_edf_strnlen(const char *s, size_t max)
{
    size_t i = 0U;
    while (i < max && s[i] != '\0') {
        i++;
    }
    return i;
}

/* Writes a fixed-width ASCII, space-padded field.
   Copies up to `width` chars from src (a NUL-terminated string), then pads the
   remainder with spaces.  Never null-terminates.  src content beyond `width`
   is truncated. */
static void np_edf_write_field(char *field, const char *src, size_t width)
{
    size_t len = np_edf_strnlen(src, width);
    np_edf_copy(field, src, len);
    if (len < width) {
        np_edf_fill(field + len, ' ', width - len);
    }
}

/* ── Epoch -> calendar date/time (integer-only, target-safe) ─────────────────── */

typedef struct {
    int32_t year;   /* full year, e.g. 2026 */
    int32_t month;  /* 1..12                */
    int32_t day;    /* 1..31                */
    int32_t hour;   /* 0..23                */
    int32_t minute; /* 0..59                */
    int32_t second; /* 0..59                */
} np_edf_datetime_t;

/* Days in each month for a given leap-year flag. */
static int32_t np_edf_days_in_month(int32_t year, int32_t month)
{
    static const int32_t dim[12] = { 31, 28, 31, 30, 31, 30,
                                     31, 31, 30, 31, 30, 31 };
    if (month == 2) {
        bool leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
        return leap ? 29 : 28;
    }
    return dim[month - 1];
}

/* Converts seconds-since-epoch (UTC) to a broken-down calendar time.
   Proleptic Gregorian, valid for any uint32 epoch (1970..2106).  On host this
   matches gmtime_r; we use this single path on both host and target so the
   output is identical everywhere and independently verifiable. */
static void np_edf_gmtime(uint32_t epoch, np_edf_datetime_t *dt)
{
    uint32_t days = epoch / 86400U;
    uint32_t rem  = epoch % 86400U;

    dt->hour   = (int32_t)(rem / 3600U);
    dt->minute = (int32_t)((rem % 3600U) / 60U);
    dt->second = (int32_t)(rem % 60U);

    int32_t year = 1970;
    for (;;) {
        bool leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
        uint32_t year_days = leap ? 366U : 365U;
        if (days >= year_days) {
            days -= year_days;
            year++;
        } else {
            break;
        }
    }
    dt->year = year;

    int32_t month = 1;
    for (;;) {
        uint32_t mdays = (uint32_t)np_edf_days_in_month(year, month);
        if (days >= mdays) {
            days -= mdays;
            month++;
        } else {
            break;
        }
    }
    dt->month = month;
    dt->day   = (int32_t)days + 1;  /* day is 1-based */
}

/* Three-letter uppercase month abbreviations (index 0 == JAN). */
static const char *np_edf_month_abbrev(int32_t month_1_to_12)
{
    static const char *names[12] = {
        "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
        "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
    };
    if (month_1_to_12 < 1 || month_1_to_12 > 12) {
        return "JAN";  /* defensive: never reached for valid datetimes */
    }
    return names[month_1_to_12 - 1];
}

/* Builds "DD-MMM-YYYY" (11 chars + NUL) into out[12]. */
static void np_edf_format_recording_date(const np_edf_datetime_t *dt, char out[12])
{
    out[0] = (char)('0' + (dt->day / 10));
    out[1] = (char)('0' + (dt->day % 10));
    out[2] = '-';
    const char *mon = np_edf_month_abbrev(dt->month);
    out[3] = mon[0];
    out[4] = mon[1];
    out[5] = mon[2];
    out[6] = '-';
    out[7]  = (char)('0' + ((dt->year / 1000) % 10));
    out[8]  = (char)('0' + ((dt->year / 100) % 10));
    out[9]  = (char)('0' + ((dt->year / 10) % 10));
    out[10] = (char)('0' + (dt->year % 10));
    out[11] = '\0';
}

/* Lowercase hex digit for a 4-bit nibble. */
static char np_edf_hex_digit(uint8_t nibble)
{
    static const char hex[16] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
    };
    return hex[nibble & 0x0FU];
}

/* ── Patient-name comparison ─────────────────────────────────────────────────── */

/* Returns true if the patient-name portion of local_patient_id contains
   alphabetic characters that do not spell "NeurOne_User".

   The name subfield runs from NP_EDF_NAME_OFFSET to the next space or to the
   end of the 80-char field.  An empty/all-space name is treated as compliant
   (no real name present).  Anything that matches "NeurOne_User" exactly is
   compliant.  Any other content that contains at least one alphabetic
   character is treated as a real name. */
bool np_edf_header_contains_real_name(const np_edf_header_t *header)
{
    if (header == NULL) {
        /* No header to inspect — treat as a violation so callers fail safe. */
        return true;
    }

    const char *pid = header->local_patient_id;

    /* Extract the name token: from NAME_OFFSET up to first space or field end. */
    size_t start = (size_t)NP_EDF_NAME_OFFSET;
    size_t end   = start;
    while (end < (size_t)NP_EDF_PATIENT_ID_LEN && pid[end] != ' ') {
        end++;
    }
    size_t name_len = end - start;

    /* Empty name (field padded with spaces from NAME_OFFSET onward) is fine. */
    if (name_len == 0U) {
        return false;
    }

    /* Compare against the fixed compliant value. */
    static const char expected[] = NP_EDF_PATIENT_NAME;
    size_t expected_len = sizeof(expected) - 1U;  /* exclude NUL */

    if (name_len == expected_len) {
        bool equal = true;
        for (size_t i = 0U; i < name_len; i++) {
            if (pid[start + i] != expected[i]) {
                equal = false;
                break;
            }
        }
        if (equal) {
            return false;  /* exactly "NeurOne_User" — compliant */
        }
    }

    /* Not the expected value: a real name only if it contains a letter. */
    for (size_t i = 0U; i < name_len; i++) {
        char c = pid[start + i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            return true;
        }
    }
    return false;
}

/* ── Header writer ───────────────────────────────────────────────────────────── */

np_edf_status_t np_edf_write_header(np_edf_header_t *header,
                                     const uint8_t *uhdr_token,
                                     const char *fw_version,
                                     uint32_t session_ts)
{
    if (header == NULL || uhdr_token == NULL || fw_version == NULL) {
        return NP_EDF_ERR_INVALID_ARG;
    }

    /* 1. Space-pad the entire 256-byte header (EDF requirement). */
    np_edf_fill((char *)header, ' ', NP_EDF_HEADER_SIZE);

    /* 2. Version: digit 0 followed by 7 spaces. */
    np_edf_write_field(header->version, "0", NP_EDF_VERSION_LEN);

    /* 3. Patient code: "NP" + 14 lowercase hex chars of token bytes 0..6. */
    char patient_code[NP_EDF_PATIENT_CODE_LEN + 1];
    patient_code[0] = 'N';
    patient_code[1] = 'P';
    for (size_t b = 0U; b < (size_t)NP_EDF_TOKEN_BYTES_USED; b++) {
        uint8_t byte = uhdr_token[b];
        patient_code[2U + (b * 2U)]      = np_edf_hex_digit((uint8_t)(byte >> 4));
        patient_code[2U + (b * 2U) + 1U] = np_edf_hex_digit((uint8_t)(byte & 0x0FU));
    }
    patient_code[NP_EDF_PATIENT_CODE_LEN] = '\0';

    /* 4. local_patient_id: "<code> X X NeurOne_User", space-padded to 80.
          Built into a NUL-terminated scratch buffer then copied without NUL. */
    char patient_id[NP_EDF_PATIENT_ID_LEN + 1];
    {
        size_t pos = 0U;
        np_edf_copy(patient_id, patient_code, NP_EDF_PATIENT_CODE_LEN);
        pos += NP_EDF_PATIENT_CODE_LEN;
        patient_id[pos++] = ' ';
        patient_id[pos++] = 'X';   /* sex */
        patient_id[pos++] = ' ';
        patient_id[pos++] = 'X';   /* birthdate */
        patient_id[pos++] = ' ';
        {
            static const char name[] = NP_EDF_PATIENT_NAME;
            size_t name_len = sizeof(name) - 1U;
            np_edf_copy(patient_id + pos, name, name_len);
            pos += name_len;
        }
        patient_id[pos] = '\0';
        np_edf_write_field(header->local_patient_id, patient_id,
                           NP_EDF_PATIENT_ID_LEN);
    }

    /* 5. Broken-down session time (placeholder 1970 epoch when ts == 0). */
    np_edf_datetime_t dt;
    np_edf_gmtime(session_ts, &dt);

    /* 6. local_recording_id:
          "Startdate <DD-MMM-YYYY> NeurOne X NeurOne_v<fw_version>". */
    {
        char rec_date[12];
        np_edf_format_recording_date(&dt, rec_date);

        char recording_id[NP_EDF_RECORDING_ID_LEN + 1];
        size_t pos = 0U;

        static const char prefix[] = "Startdate ";
        size_t prefix_len = sizeof(prefix) - 1U;
        np_edf_copy(recording_id, prefix, prefix_len);
        pos += prefix_len;

        size_t date_len = np_edf_strnlen(rec_date, sizeof(rec_date));
        np_edf_copy(recording_id + pos, rec_date, date_len);
        pos += date_len;

        static const char mid[] = " NeurOne X NeurOne_v";
        size_t mid_len = sizeof(mid) - 1U;
        np_edf_copy(recording_id + pos, mid, mid_len);
        pos += mid_len;

        /* Append fw_version, truncating to the remaining field width. */
        size_t remaining = (size_t)NP_EDF_RECORDING_ID_LEN - pos;
        size_t fw_len = np_edf_strnlen(fw_version, remaining);
        np_edf_copy(recording_id + pos, fw_version, fw_len);
        pos += fw_len;

        recording_id[pos] = '\0';
        np_edf_write_field(header->local_recording_id, recording_id,
                           NP_EDF_RECORDING_ID_LEN);
    }

    /* 7. startdate: DD.MM.YY (EDF clipping convention: two-digit year). */
    {
        char *sd = header->startdate;
        sd[0] = (char)('0' + (dt.day / 10));
        sd[1] = (char)('0' + (dt.day % 10));
        sd[2] = '.';
        sd[3] = (char)('0' + (dt.month / 10));
        sd[4] = (char)('0' + (dt.month % 10));
        sd[5] = '.';
        sd[6] = (char)('0' + ((dt.year / 10) % 10));
        sd[7] = (char)('0' + (dt.year % 10));
    }

    /* 8. starttime: HH.MM.SS (all zeros when session_ts == 0). */
    {
        char *st = header->starttime;
        st[0] = (char)('0' + (dt.hour / 10));
        st[1] = (char)('0' + (dt.hour % 10));
        st[2] = '.';
        st[3] = (char)('0' + (dt.minute / 10));
        st[4] = (char)('0' + (dt.minute % 10));
        st[5] = '.';
        st[6] = (char)('0' + (dt.second / 10));
        st[7] = (char)('0' + (dt.second % 10));
    }

    /* 9. header_bytes: "256" left-aligned, space-padded to 8 chars. */
    np_edf_write_field(header->header_bytes, "256", NP_EDF_HEADER_BYTES_LEN);

    /* The remaining numeric fields (reserved, num_data_records, duration,
       num_signals) are left space-padded — they are owned by the EDF signal
       writer, not the privacy-header writer.  Space-padding is the valid
       EDF placeholder until that writer fills them. */

    return NP_EDF_OK;
}
