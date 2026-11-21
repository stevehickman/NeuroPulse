/* Document: NP-FW-EMMC-002 §E.5 */
/*
 * NeurOne EDF+ Privacy Header — Research Header (OI-PRIV-26 (NP-PRIV-ANALYSIS-004))
 * Document: NP-FW-EMMC-002 §E.5; CLAUDE.md §5.3
 *
 * See np_edf_research.h for the contract.  No heap, no libc: the target build
 * is freestanding, so every string and date routine is file-local.
 */

#include "np_edf_research.h"

/* ── Small helpers ───────────────────────────────────────────────────────────── */

static void rs_fill(char *dst, char value, size_t n)
{
    for (size_t i = 0U; i < n; i++) {
        dst[i] = value;
    }
}

static void rs_copy(char *dst, const char *src, size_t n)
{
    for (size_t i = 0U; i < n; i++) {
        dst[i] = src[i];
    }
}

static bool rs_equal(const char *a, const char *b, size_t n)
{
    for (size_t i = 0U; i < n; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

static bool rs_all_spaces(const char *s, size_t n)
{
    for (size_t i = 0U; i < n; i++) {
        if (s[i] != ' ') {
            return false;
        }
    }
    return true;
}

static bool rs_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static bool rs_is_lower_hex(char c)
{
    return rs_is_digit(c) || (c >= 'a' && c <= 'f');
}

/* Copies a NUL-terminated literal of known length into a fixed field and pads it
   with spaces.  `len` must not exceed `width`. */
static void rs_put(char *field, size_t width, const char *src, size_t len)
{
    rs_copy(field, src, len);
    rs_fill(field + len, ' ', width - len);
}

/* ── Calendar (proleptic Gregorian, days since 1970-01-01) ───────────────────── */

static bool rs_leap(int32_t y)
{
    return ((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0);
}

static int32_t rs_days_in_month(int32_t y, int32_t m)
{
    static const int32_t DIM[12] = { 31, 28, 31, 30, 31, 30,
                                     31, 31, 30, 31, 30, 31 };
    if (m == 2) {
        return rs_leap(y) ? 29 : 28;
    }
    return DIM[m - 1];
}

/* H. Hinnant's days_from_civil / civil_from_days, integer only. */
static int32_t rs_days_from_civil(int32_t y, int32_t m, int32_t d)
{
    y -= (m <= 2) ? 1 : 0;
    const int32_t era = ((y >= 0) ? y : (y - 399)) / 400;
    const int32_t yoe = y - (era * 400);
    const int32_t doy = ((153 * (m + ((m > 2) ? -3 : 9))) + 2) / 5 + d - 1;
    const int32_t doe = (yoe * 365) + (yoe / 4) - (yoe / 100) + doy;
    return (era * 146097) + doe - 719468;
}

static void rs_civil_from_days(int32_t z, int32_t *y, int32_t *m, int32_t *d)
{
    z += 719468;
    const int32_t era = ((z >= 0) ? z : (z - 146096)) / 146097;
    const int32_t doe = z - (era * 146097);
    const int32_t yoe = (doe - (doe / 1460) + (doe / 36524) - (doe / 146096)) / 365;
    const int32_t doy = doe - ((365 * yoe) + (yoe / 4) - (yoe / 100));
    const int32_t mp  = ((5 * doy) + 2) / 153;
    *d = doy - (((153 * mp) + 2) / 5) + 1;
    *m = mp + ((mp < 10) ? 3 : -9);
    *y = (yoe + (era * 400)) + ((*m <= 2) ? 1 : 0);
}

static const char MONTHS[12][4] = {
    "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
    "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
};

static int32_t rs_two(const char *s)
{
    return ((s[0] - '0') * 10) + (s[1] - '0');
}

/* Parses "DD.MM.YY".  Returns false unless the shape is exact. */
static bool rs_parse_startdate(const char *s, int32_t *dd, int32_t *mm, int32_t *yy)
{
    if (!rs_is_digit(s[0]) || !rs_is_digit(s[1]) || s[2] != '.' ||
        !rs_is_digit(s[3]) || !rs_is_digit(s[4]) || s[5] != '.' ||
        !rs_is_digit(s[6]) || !rs_is_digit(s[7])) {
        return false;
    }
    *dd = rs_two(&s[0]);
    *mm = rs_two(&s[3]);
    *yy = rs_two(&s[6]);
    return true;
}

/* Parses "DD-MMM-YYYY".  Returns false unless the shape is exact. */
static bool rs_parse_recdate(const char *s, int32_t *dd, int32_t *mm, int32_t *yyyy)
{
    if (!rs_is_digit(s[0]) || !rs_is_digit(s[1]) || s[2] != '-' || s[6] != '-' ||
        !rs_is_digit(s[7]) || !rs_is_digit(s[8]) ||
        !rs_is_digit(s[9]) || !rs_is_digit(s[10])) {
        return false;
    }
    int32_t month = 0;
    for (int32_t i = 0; i < 12; i++) {
        if (rs_equal(&s[3], MONTHS[i], 3U)) {
            month = i + 1;
            break;
        }
    }
    if (month == 0) {
        return false;
    }
    *dd   = rs_two(&s[0]);
    *mm   = month;
    *yyyy = (rs_two(&s[7]) * 100) + rs_two(&s[9]);
    return true;
}

static bool rs_valid_date(int32_t y, int32_t m, int32_t d)
{
    return y >= NP_EDF_CLIP_YEAR_MIN && y <= NP_EDF_CLIP_YEAR_MAX &&
           m >= 1 && m <= 12 && d >= 1 && d <= rs_days_in_month(y, m);
}

/* Floors `days` to the start of its rounding window (windows counted from the
   Monday anchor), for a positive window length `n`. */
static int32_t rs_floor_window(int32_t days, int32_t n)
{
    const int32_t rel = days - NP_EDF_RESEARCH_ANCHOR_DAY;
    const int32_t q   = (rel >= 0) ? (rel / n) : -(((-rel) + n - 1) / n);
    return (q * n) + NP_EDF_RESEARCH_ANCHOR_DAY;
}

/* ── Policy ──────────────────────────────────────────────────────────────────── */

static bool rs_study_code_ok(const char *code)
{
    if (code[0] != 'N' || code[1] != 'S') {
        return false;
    }
    for (size_t i = 2U; i < (size_t)NP_EDF_STUDY_CODE_LEN; i++) {
        if (!rs_is_lower_hex(code[i])) {
            return false;
        }
    }
    return code[NP_EDF_STUDY_CODE_LEN] == '\0';
}

static bool rs_policy_ok(const np_edf_research_policy_t *policy)
{
    if (policy == NULL) {
        return false;
    }
    if (policy->date_rounding_days != 0U &&
        policy->date_rounding_days < NP_EDF_RESEARCH_MIN_ROUNDING_DAYS) {
        return false;
    }
    if (policy->study_code != NULL && !rs_study_code_ok(policy->study_code)) {
        return false;
    }
    return true;
}

np_edf_status_t np_edf_make_study_code(const uint8_t random_bytes[NP_EDF_STUDY_CODE_RANDOM_BYTES],
                                       char out[NP_EDF_STUDY_CODE_LEN + 1])
{
    static const char HEX[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
                                  '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    if (random_bytes == NULL || out == NULL) {
        return NP_EDF_ERR_INVALID_ARG;
    }
    out[0] = 'N';
    out[1] = 'S';
    for (size_t b = 0U; b < (size_t)NP_EDF_STUDY_CODE_RANDOM_BYTES; b++) {
        out[2U + (b * 2U)]      = HEX[(random_bytes[b] >> 4) & 0x0FU];
        out[2U + (b * 2U) + 1U] = HEX[random_bytes[b] & 0x0FU];
    }
    out[NP_EDF_STUDY_CODE_LEN] = '\0';
    return NP_EDF_OK;
}

/* ── Builder ─────────────────────────────────────────────────────────────────── */

/* Reads the local header's start date as days since the epoch.  The four-digit
   year comes from the recording ID when it agrees with the startdate field;
   otherwise the EDF 1985 clipping rule is applied to the two-digit year. */
static bool rs_local_days(const np_edf_header_t *local, int32_t *days)
{
    int32_t dd = 0, mm = 0, yy = 0;
    if (!rs_parse_startdate(local->startdate, &dd, &mm, &yy)) {
        return false;
    }
    int32_t year = (yy >= 85) ? (1900 + yy) : (2000 + yy);

    static const char PREFIX[] = "Startdate ";
    const char *rid = local->local_recording_id;
    int32_t rd = 0, rm = 0, ry = 0;
    if (rs_equal(rid, PREFIX, sizeof(PREFIX) - 1U) &&
        rs_parse_recdate(rid + (sizeof(PREFIX) - 1U), &rd, &rm, &ry) &&
        rd == dd && rm == mm && (ry % 100) == yy) {
        year = ry;
    }
    if (!rs_valid_date(year, mm, dd)) {
        return false;
    }
    *days = rs_days_from_civil(year, mm, dd);
    return true;
}

np_edf_status_t np_edf_make_research_header(const np_edf_header_t *local,
                                            const np_edf_research_policy_t *policy,
                                            np_edf_header_t *out)
{
    if (local == NULL || out == NULL || !rs_policy_ok(policy)) {
        return NP_EDF_ERR_INVALID_ARG;
    }

    np_edf_header_t h;
    rs_fill((char *)&h, ' ', sizeof(h));

    /* Format fields: copied, then checked by the validator. */
    rs_copy(h.version,          local->version,          NP_EDF_VERSION_LEN);
    rs_copy(h.header_bytes,     local->header_bytes,     NP_EDF_HEADER_BYTES_LEN);
    rs_copy(h.reserved,         local->reserved,         NP_EDF_RESERVED_LEN);
    rs_copy(h.num_data_records, local->num_data_records, NP_EDF_NUM_RECORDS_LEN);
    rs_copy(h.duration,         local->duration,         NP_EDF_DURATION_LEN);
    rs_copy(h.num_signals,      local->num_signals,      NP_EDF_NUM_SIGNALS_LEN);

    /* Patient ID: "<code|X> X X X". */
    {
        char pid[NP_EDF_STUDY_CODE_LEN + 6];
        size_t pos = 0U;
        if (policy->study_code != NULL) {
            rs_copy(pid, policy->study_code, NP_EDF_STUDY_CODE_LEN);
            pos = NP_EDF_STUDY_CODE_LEN;
        } else {
            pid[pos++] = 'X';
        }
        rs_copy(pid + pos, " X X X", 6U);
        pos += 6U;
        rs_put(h.local_patient_id, NP_EDF_PATIENT_ID_LEN, pid, pos);
    }

    /* Date: floored to the window, or blank. */
    bool    dated = false;
    int32_t y = 0, m = 0, d = 0;
    if (policy->date_rounding_days != 0U) {
        int32_t days = 0;
        if (rs_local_days(local, &days)) {
            rs_civil_from_days(rs_floor_window(days, (int32_t)policy->date_rounding_days),
                               &y, &m, &d);
            dated = rs_valid_date(y, m, d);
        }
    }

    if (dated) {
        char rid[27];
        rs_copy(rid, "Startdate ", 10U);
        rid[10] = (char)('0' + (d / 10));
        rid[11] = (char)('0' + (d % 10));
        rid[12] = '-';
        rs_copy(&rid[13], MONTHS[m - 1], 3U);
        rid[16] = '-';
        rid[17] = (char)('0' + ((y / 1000) % 10));
        rid[18] = (char)('0' + ((y / 100) % 10));
        rid[19] = (char)('0' + ((y / 10) % 10));
        rid[20] = (char)('0' + (y % 10));
        rs_copy(&rid[21], " X X X", 6U);
        rs_put(h.local_recording_id, NP_EDF_RECORDING_ID_LEN, rid, sizeof(rid));

        h.startdate[0] = (char)('0' + (d / 10));
        h.startdate[1] = (char)('0' + (d % 10));
        h.startdate[2] = '.';
        h.startdate[3] = (char)('0' + (m / 10));
        h.startdate[4] = (char)('0' + (m % 10));
        h.startdate[5] = '.';
        h.startdate[6] = (char)('0' + ((y / 10) % 10));
        h.startdate[7] = (char)('0' + (y % 10));
    } else {
        static const char BLANK_RID[] = "Startdate X X X X";
        rs_put(h.local_recording_id, NP_EDF_RECORDING_ID_LEN,
               BLANK_RID, sizeof(BLANK_RID) - 1U);
        rs_copy(h.startdate, NP_EDF_BLANK_STARTDATE, NP_EDF_STARTDATE_LEN);
    }
    rs_copy(h.starttime, NP_EDF_RESEARCH_STARTTIME, NP_EDF_STARTTIME_LEN);

    *out = h;
    return NP_EDF_OK;
}

/* ── Validator ───────────────────────────────────────────────────────────────── */

/* A numeric format field: digits, '.', '-' and spaces only. */
static bool rs_numeric_field(const char *s, size_t n)
{
    for (size_t i = 0U; i < n; i++) {
        const char c = s[i];
        if (!rs_is_digit(c) && c != '.' && c != '-' && c != ' ') {
            return false;
        }
    }
    return true;
}

static bool rs_format_fields_ok(const np_edf_header_t *h)
{
    if (h->version[0] != '0' || !rs_all_spaces(&h->version[1], NP_EDF_VERSION_LEN - 1U)) {
        return false;
    }
    /* reserved: blank, or "EDF+C" / "EDF+D" then blank (the EDF+ reserved field). */
    if (!rs_all_spaces(h->reserved, NP_EDF_RESERVED_LEN)) {
        if (!(rs_equal(h->reserved, "EDF+C", 5U) || rs_equal(h->reserved, "EDF+D", 5U)) ||
            !rs_all_spaces(&h->reserved[5], NP_EDF_RESERVED_LEN - 5U)) {
            return false;
        }
    }
    return rs_numeric_field(h->header_bytes,     NP_EDF_HEADER_BYTES_LEN) &&
           rs_numeric_field(h->num_data_records, NP_EDF_NUM_RECORDS_LEN) &&
           rs_numeric_field(h->duration,         NP_EDF_DURATION_LEN) &&
           rs_numeric_field(h->num_signals,      NP_EDF_NUM_SIGNALS_LEN);
}

np_edf_status_t np_edf_validate_research_header(const np_edf_header_t *header,
                                                const np_edf_research_policy_t *policy)
{
    if (header == NULL || !rs_policy_ok(policy)) {
        return NP_EDF_ERR_INVALID_ARG;
    }

    /* 1. Patient ID: every one of the 80 bytes. */
    const char *pid = header->local_patient_id;
    size_t rest = 0U;
    if (pid[0] == 'X' && pid[1] == ' ') {
        rest = 1U;
    } else if (policy->study_code != NULL &&
               rs_equal(pid, policy->study_code, NP_EDF_STUDY_CODE_LEN) &&
               pid[NP_EDF_STUDY_CODE_LEN] == ' ') {
        rest = NP_EDF_STUDY_CODE_LEN;
    } else {
        /* The device "NP" code, another study's code, or anything else. */
        return NP_EDF_ERR_RESEARCH_LINKAGE;
    }
    if (!rs_equal(pid + rest, " X X X", 6U) ||
        !rs_all_spaces(pid + rest + 6U, NP_EDF_PATIENT_ID_LEN - rest - 6U)) {
        return NP_EDF_ERR_PRIVACY_HEADER;
    }

    if (!rs_format_fields_ok(header)) {
        return NP_EDF_ERR_PRIVACY_HEADER;
    }

    /* 2. Recording ID: "Startdate X X X X" or "Startdate DD-MMM-YYYY X X X". */
    const char *rid = header->local_recording_id;
    if (!rs_equal(rid, "Startdate ", 10U)) {
        return NP_EDF_ERR_RECORDING_ID;
    }
    bool blank = false;
    int32_t rd = 0, rm = 0, ry = 0;
    if (rs_equal(rid + 10, "X X X X", 7U) &&
        rs_all_spaces(rid + 17, NP_EDF_RECORDING_ID_LEN - 17U)) {
        blank = true;
    } else if (rs_parse_recdate(rid + 10, &rd, &rm, &ry) &&
               rs_equal(rid + 21, " X X X", 6U) &&
               rs_all_spaces(rid + 27, NP_EDF_RECORDING_ID_LEN - 27U)) {
        blank = false;
    } else {
        return NP_EDF_ERR_RECORDING_ID;
    }

    /* 3. Start time is never kept. */
    if (!rs_equal(header->starttime, NP_EDF_RESEARCH_STARTTIME, NP_EDF_STARTTIME_LEN)) {
        return NP_EDF_ERR_RESEARCH_DATE;
    }

    /* 4. Start date: blank, or a window start that both fields agree on. */
    if (blank) {
        return rs_equal(header->startdate, NP_EDF_BLANK_STARTDATE, NP_EDF_STARTDATE_LEN)
                   ? NP_EDF_OK : NP_EDF_ERR_RESEARCH_DATE;
    }
    if (policy->date_rounding_days == 0U || !rs_valid_date(ry, rm, rd)) {
        return NP_EDF_ERR_RESEARCH_DATE;
    }
    int32_t sd = 0, sm = 0, sy = 0;
    if (!rs_parse_startdate(header->startdate, &sd, &sm, &sy) ||
        sd != rd || sm != rm || sy != (ry % 100)) {
        return NP_EDF_ERR_RESEARCH_DATE;
    }
    const int32_t days = rs_days_from_civil(ry, rm, rd);
    if (rs_floor_window(days, (int32_t)policy->date_rounding_days) != days) {
        return NP_EDF_ERR_RESEARCH_DATE;
    }
    return NP_EDF_OK;
}
