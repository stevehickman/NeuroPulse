/* Document: NP-FW-EMMC-002 Rev 1 §E */
/*
 * NeurOne EDF+ Privacy Header — Host Unit Tests
 * Document: NP-FW-EMMC-002 Rev 1 §E.2 / OI-EMMC2-05
 *
 * Build for host: -DNPTEST_HOST (see CMakeLists.txt NP_BUILD_TESTS option).
 * Return convention: 0 = all PASS, non-zero = count of failed assertions.
 */

/* gmtime_r is POSIX, not C11; the writer's own gmtime is checked against it. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE   /* timegm, used to read back a written date */

#include "np_edf_writer.h"
#include "np_edf_anon_gate.h"
#include "np_edf_research.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/* ── Test infrastructure ─────────────────────────────────────────────────────── */

static int g_fail_count = 0;

#define ASSERT(cond, msg)                                            \
    do {                                                             \
        if (!(cond)) {                                               \
            printf("FAIL [%s:%d] %s\n", __func__, __LINE__, (msg)); \
            g_fail_count++;                                          \
        }                                                            \
    } while (0)

#define ASSERT_OK(expr) ASSERT((expr) == NP_EDF_OK, #expr " != NP_EDF_OK")
#define ASSERT_EQ(a, b) ASSERT((a) == (b), #a " != " #b)

/* ── Helpers ─────────────────────────────────────────────────────────────────── */

/* Builds a deterministic 32-byte UHDR token: byte 0 = seed, rest = 0xAB. */
static void make_token(uint8_t token[32], uint8_t seed)
{
    token[0] = seed;
    for (size_t i = 1U; i < 32U; i++) {
        token[i] = 0xABU;
    }
}

/* ── Test 1: generate + validate 100 headers ─────────────────────────────────── */

/* Session timestamps for the 100 files.  The writer converts them with its own
   libc-free gmtime (np_edf_writer.c), so the set pins the calendar edges that a
   hand-rolled conversion gets wrong: the epoch, 2000-02-29 (divisible by 400,
   leap), 2100-03-01 (divisible by 100, not leap), the last second of a day and
   of a year, and the last representable uint32 second (2106-02-07 06:28:15).
   The remaining slots are spread over 1970..2106 by a fixed LCG. */
static const uint32_t EDGE_TS[] = {
    0U,           /* 1970-01-01 00:00:00 */
    86399U,       /* 1970-01-01 23:59:59 */
    951782400U,   /* 2000-02-29 00:00:00 */
    951868799U,   /* 2000-02-29 23:59:59 */
    978307199U,   /* 2000-12-31 23:59:59 */
    1709208000U,  /* 2024-02-29 12:00:00 */
    1790208000U,  /* 2026-09-24 00:00:00 */
    4107542399U,  /* 2100-02-28 23:59:59 */
    4107542400U,  /* 2100-03-01 00:00:00 */
    0xFFFFFFFFU,  /* 2106-02-07 06:28:15 */
};

static uint32_t session_ts_for(int i)
{
    const int n_edge = (int)(sizeof(EDGE_TS) / sizeof(EDGE_TS[0]));
    if (i < n_edge) {
        return EDGE_TS[i];
    }
    uint32_t x = (uint32_t)i * 2654435761U;  /* Knuth multiplicative hash */
    return x ^ (x >> 13);
}

/* Checks startdate, starttime and the "Startdate DD-MMM-YYYY" subfield of
   local_recording_id against the host C library's gmtime_r. */
static void check_dates_against_libc(const np_edf_header_t *h, uint32_t ts)
{
    static const char *mon[12] = {
        "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
        "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
    };
    time_t t = (time_t)ts;
    struct tm tm;
    if (gmtime_r(&t, &tm) == NULL) {
        ASSERT(0, "host gmtime_r failed");
        return;
    }

    char want[32];
    snprintf(want, sizeof(want), "%02d.%02d.%02d",
             tm.tm_mday, tm.tm_mon + 1, (tm.tm_year + 1900) % 100);
    if (memcmp(h->startdate, want, NP_EDF_STARTDATE_LEN) != 0) {
        printf("  ts=%u startdate got '%.8s' want '%s'\n",
               (unsigned)ts, h->startdate, want);
        ASSERT(0, "startdate disagrees with gmtime_r");
    }

    snprintf(want, sizeof(want), "%02d.%02d.%02d",
             tm.tm_hour, tm.tm_min, tm.tm_sec);
    if (memcmp(h->starttime, want, NP_EDF_STARTTIME_LEN) != 0) {
        printf("  ts=%u starttime got '%.8s' want '%s'\n",
               (unsigned)ts, h->starttime, want);
        ASSERT(0, "starttime disagrees with gmtime_r");
    }

    snprintf(want, sizeof(want), "Startdate %02d-%s-%04d ",
             tm.tm_mday, mon[tm.tm_mon], tm.tm_year + 1900);
    if (memcmp(h->local_recording_id, want, strlen(want)) != 0) {
        printf("  ts=%u recording_id got '%.22s' want '%s'\n",
               (unsigned)ts, h->local_recording_id, want);
        ASSERT(0, "recording_id Startdate disagrees with gmtime_r");
    }
}

/* OI-EMMC2-05: 100 files, each with its own token and session time, all of
   which must pass the §E.2 local validator and carry the correct EDF+ date/time.
   (The local header is not a research header: see the §E.5 tests below.) */
/* @verifies REQ-SW02-02 */
static void test_write_100_headers(void)
{
    int failures_before = g_fail_count;
    for (int i = 0; i < 100; i++) {
        uint8_t token[32];
        make_token(token, (uint8_t)i);
        uint32_t ts = session_ts_for(i);

        np_edf_header_t header;
        ASSERT_OK(np_edf_write_header(&header, token, "1.0.0", ts));
        ASSERT_OK(np_edf_validate_privacy_header(&header));
        check_dates_against_libc(&header, ts);
    }
    if (g_fail_count == failures_before) {
        printf("PASS: 100/100 header generation tests\n");
    } else {
        printf("FAIL: header generation (%d failures)\n",
               g_fail_count - failures_before);
    }
}

/* ── Test 2: validator rejects a real sex code ───────────────────────────────── */

/* @verifies REQ-SW02-02 */
static void test_validator_rejects_real_sex(void)
{
    uint8_t token[32];
    make_token(token, 0x11U);

    np_edf_header_t header;
    ASSERT_OK(np_edf_write_header(&header, token, "1.0.0", 0U));

    /* Corrupt only the sex field to a real code; leave everything else valid. */
    header.local_patient_id[NP_EDF_SEX_OFFSET] = 'M';

    ASSERT_EQ(np_edf_validate_privacy_header(&header), NP_EDF_ERR_PRIVACY_HEADER);
}

/* ── Test 3: validator rejects a real birthdate ──────────────────────────────── */

/* @verifies REQ-SW02-02 */
static void test_validator_rejects_real_dob(void)
{
    uint8_t token[32];
    make_token(token, 0x22U);

    np_edf_header_t header;
    ASSERT_OK(np_edf_write_header(&header, token, "1.0.0", 0U));

    /* Corrupt only the birthdate field; sex remains 'X'. */
    header.local_patient_id[NP_EDF_DOB_OFFSET] = '1';

    ASSERT_EQ(np_edf_validate_privacy_header(&header), NP_EDF_ERR_PRIVACY_HEADER);
}

/* ── Test 4: validator rejects a real patient name ───────────────────────────── */

/* @verifies REQ-SW02-02 */
static void test_validator_rejects_real_name(void)
{
    uint8_t token[32];
    make_token(token, 0x33U);

    np_edf_header_t header;
    ASSERT_OK(np_edf_write_header(&header, token, "1.0.0", 0U));

    /* Overwrite the patient-name subfield with a real name.  Sex ('X') and
       birthdate ('X') are left intact so the validator reaches the name check
       rather than short-circuiting on a prior field. */
    const char real_name[] = "John_Smith";
    size_t name_len = sizeof(real_name) - 1U;
    for (size_t i = 0U; i < name_len; i++) {
        header.local_patient_id[(size_t)NP_EDF_NAME_OFFSET + i] = real_name[i];
    }

    ASSERT_EQ(np_edf_validate_privacy_header(&header), NP_EDF_ERR_REAL_NAME);
    ASSERT(np_edf_header_contains_real_name(&header),
           "contains_real_name should be true for John_Smith");
}

/* ── Test 5: patient code prefix is "NP" ─────────────────────────────────────── */

/* @verifies REQ-SW02-02 */
static void test_patient_code_prefix(void)
{
    uint8_t token[32];
    make_token(token, 0x44U);

    np_edf_header_t header;
    ASSERT_OK(np_edf_write_header(&header, token, "1.0.0", 0U));

    ASSERT_EQ(header.local_patient_id[0], 'N');
    ASSERT_EQ(header.local_patient_id[1], 'P');
}

/* ── Test 6: header struct is exactly 256 bytes ──────────────────────────────── */

/* @verifies REQ-SW02-02 */
static void test_header_size(void)
{
    ASSERT_EQ(sizeof(np_edf_header_t), (size_t)256);
}

/* ── Test 7: firmware version appears in the recording id ─────────────────────── */

/* @verifies REQ-SW02-02 */
static void test_fw_version_in_recording_id(void)
{
    uint8_t token[32];
    make_token(token, 0x55U);

    np_edf_header_t header;
    ASSERT_OK(np_edf_write_header(&header, token, "1.2.3", 0U));

    /* local_recording_id is fixed-width ASCII, not NUL-terminated.  Copy it to
       a NUL-terminated scratch buffer before substring searching. */
    char rec[NP_EDF_RECORDING_ID_LEN + 1];
    memcpy(rec, header.local_recording_id, NP_EDF_RECORDING_ID_LEN);
    rec[NP_EDF_RECORDING_ID_LEN] = '\0';

    ASSERT(strstr(rec, "NeurOne_v1.2.3") != NULL,
           "recording_id missing NeurOne_v1.2.3");
}

/* ── §E.4 anonymization-pipeline gate tests ──────────────────────────────────── */


/* The gate's policy in these tests: patient code 'X', weekly rounding. */
static const np_edf_research_policy_t GATE_POLICY = { NULL, 7U };

/* Writes a valid RESEARCH header for the given seed: the local header the
   writer makes, rewritten for research (§E.5) as the pipeline does before the
   gate.  The local header itself fails the gate (test_research_gate_refuses_local). */
static void make_valid_header(np_edf_header_t *h, uint8_t seed)
{
    uint8_t token[32];
    make_token(token, seed);
    np_edf_header_t local;
    ASSERT_OK(np_edf_write_header(&local, token, "1.0.0", 1790208000U + (uint32_t)seed * 3600U));
    ASSERT_OK(np_edf_make_research_header(&local, &GATE_POLICY, h));
}

/* Overwrites the patient-name subfield with a real name (a §E.2 violation). */
static void corrupt_name(np_edf_header_t *h)
{
    const char real_name[] = "Jane_Doe";
    size_t name_len = sizeof(real_name) - 1U;
    for (size_t i = 0U; i < name_len; i++) {
        h->local_patient_id[(size_t)NP_EDF_NAME_OFFSET + i] = real_name[i];
    }
}

/* Gate over a mixed set: files 1 (sex) and 4 (name) violate; the rest pass. */
/* @verifies REQ-SW02-02 */
static void test_anon_gate_mixed(void)
{
    np_edf_header_t headers[6];
    for (uint8_t i = 0U; i < 6U; i++) {
        make_valid_header(&headers[i], (uint8_t)(0x60U + i));
    }
    headers[1].local_patient_id[NP_EDF_SEX_OFFSET] = 'M';  /* real sex code */
    corrupt_name(&headers[4]);                             /* real name     */

    bool included[6];
    np_edf_gate_result_t result;

    np_edf_status_t st = np_edf_anon_gate_filter(headers, 6U, &GATE_POLICY, included, &result);

    /* A per-file exclusion is NOT a run failure — the gate must not abort. */
    ASSERT_EQ(st, NP_EDF_OK);
    ASSERT_EQ(result.included, (uint32_t)4);
    ASSERT_EQ(result.excluded, (uint32_t)2);

    ASSERT(included[0], "file 0 should be included");
    ASSERT(!included[1], "file 1 (bad sex) should be excluded");
    ASSERT(included[2], "file 2 should be included");
    ASSERT(included[3], "file 3 should be included");
    ASSERT(!included[4], "file 4 (real name) should be excluded");
    ASSERT(included[5], "file 5 should be included");
}

/* All-compliant set: every file included. */
/* @verifies REQ-SW02-02 */
static void test_anon_gate_all_pass(void)
{
    np_edf_header_t headers[5];
    for (uint8_t i = 0U; i < 5U; i++) {
        make_valid_header(&headers[i], (uint8_t)(0x70U + i));
    }

    bool included[5];
    np_edf_gate_result_t result;

    ASSERT_OK(np_edf_anon_gate_filter(headers, 5U, &GATE_POLICY, included, &result));
    ASSERT_EQ(result.included, (uint32_t)5);
    ASSERT_EQ(result.excluded, (uint32_t)0);
    for (int i = 0; i < 5; i++) {
        ASSERT(included[i], "all files should be included");
    }
}

/* All-violating set: gate still returns OK (does not abort); all excluded. */
/* @verifies REQ-SW02-02 */
static void test_anon_gate_all_fail_does_not_abort(void)
{
    np_edf_header_t headers[3];
    for (uint8_t i = 0U; i < 3U; i++) {
        make_valid_header(&headers[i], (uint8_t)(0x80U + i));
        headers[i].local_patient_id[NP_EDF_DOB_OFFSET] = '9';  /* real DOB */
    }

    bool included[3];
    np_edf_gate_result_t result;

    np_edf_status_t st = np_edf_anon_gate_filter(headers, 3U, &GATE_POLICY, included, &result);
    ASSERT_EQ(st, NP_EDF_OK);            /* every file bad, still no abort */
    ASSERT_EQ(result.included, (uint32_t)0);
    ASSERT_EQ(result.excluded, (uint32_t)3);
    for (int i = 0; i < 3; i++) {
        ASSERT(!included[i], "all files should be excluded");
    }
}

/* Null / empty argument handling, and a NULL result. */
/* @verifies REQ-SW02-02 */
static void test_anon_gate_null_and_empty(void)
{
    np_edf_header_t headers[2];
    make_valid_header(&headers[0], 0x90U);
    make_valid_header(&headers[1], 0x91U);
    headers[1].local_patient_id[NP_EDF_SEX_OFFSET] = 'F';  /* violation */
    bool included[2];
    np_edf_gate_result_t result = { .included = 99, .excluded = 99 };

    /* count == 0 is a valid no-op; result is zeroed. */
    ASSERT_OK(np_edf_anon_gate_filter(headers, 0U, &GATE_POLICY, included, &result));
    ASSERT_EQ(result.included, (uint32_t)0);
    ASSERT_EQ(result.excluded, (uint32_t)0);

    /* Non-zero count with NULL arrays is rejected. */
    ASSERT_EQ(np_edf_anon_gate_filter(NULL, 2U, &GATE_POLICY, included, NULL),
              NP_EDF_ERR_INVALID_ARG);
    ASSERT_EQ(np_edf_anon_gate_filter(headers, 2U, &GATE_POLICY, NULL, NULL),
              NP_EDF_ERR_INVALID_ARG);
    ASSERT_EQ(np_edf_anon_gate_filter(headers, 2U, NULL, included, NULL),
              NP_EDF_ERR_INVALID_ARG);

    /* A NULL result must still process and exclude safely. */
    ASSERT_OK(np_edf_anon_gate_filter(headers, 2U, &GATE_POLICY, included, NULL));
    ASSERT(included[0], "file 0 should be included");
    ASSERT(!included[1], "file 1 should be excluded even with no result");
}

/* REQ-SW02-14 (OI-PRIV-38 (NP-PRIV-ANALYSIS-004) part 2): the gate's whole output
 * is its status, the include flags and the caller's tally.  It takes no SHDR
 * callback, so it can hand out no event, file index or code for SHDR: the
 * assignment below stops compiling if a parameter is added back, and the
 * retired event code must stay undefined.  The tally and flags are checked to be
 * all that changes for a set with exclusions. */
#ifdef NP_EDF_SHDR_EV_PRIVACY_HEADER_VIOLATION
#error "0xE1 is retired: the EDF+ gate writes nothing to SHDR (REQ-SW02-14)"
#endif
/* @verifies REQ-SW02-14 */
static void test_anon_gate_leaves_nothing_for_shdr(void)
{
    np_edf_status_t (*const gate)(const np_edf_header_t *, uint32_t,
                                  const np_edf_research_policy_t *, bool *,
                                  np_edf_gate_result_t *) = np_edf_anon_gate_filter;

    np_edf_header_t headers[4];
    for (uint8_t i = 0U; i < 4U; i++) {
        make_valid_header(&headers[i], (uint8_t)(0xA0U + i));
    }
    headers[2].local_patient_id[NP_EDF_SEX_OFFSET] = 'M';   /* one violation */
    np_edf_header_t before[4];
    memcpy(before, headers, sizeof headers);

    bool included[4];
    np_edf_gate_result_t result;
    ASSERT_OK(gate(headers, 4U, &GATE_POLICY, included, &result));
    ASSERT_EQ(result.included, (uint32_t)3);
    ASSERT_EQ(result.excluded, (uint32_t)1);
    ASSERT(!included[2] && included[0] && included[1] && included[3],
           "only the violating file is excluded");
    ASSERT(memcmp(before, headers, sizeof headers) == 0,
           "the gate reads the headers and writes nothing back");
}

/* ── §E.5 research header (OI-PRIV-26 (NP-PRIV-ANALYSIS-004)) ───────────────────────────────────────── */

static int64_t days_of(uint32_t ts) { return (int64_t)(ts / 86400U); }

/* Puts `text` into a fixed-width field at `off`, without a NUL. */
static void poke(char *field, size_t off, const char *text)
{
    memcpy(field + off, text, strlen(text));
}

/* (e) A research header built from each of the 100 local headers passes the
   gate under four policies, keeps the EDF format fields, carries a start date
   that is blank or the Monday (or window start) on or before the session day,
   and leaves the local header untouched. */
/* @verifies REQ-SW02-12#e */
static void test_research_header_from_local(void)
{
    uint8_t rnd[NP_EDF_STUDY_CODE_RANDOM_BYTES] = { 1, 2, 3, 4, 5, 6, 7 };
    char code[NP_EDF_STUDY_CODE_LEN + 1];
    ASSERT_OK(np_edf_make_study_code(rnd, code));
    ASSERT(strcmp(code, "NS01020304050607") == 0, "study code format");

    const np_edf_research_policy_t policies[4] = {
        { NULL, 7U }, { code, 7U }, { NULL, 0U }, { code, 28U },
    };
    int dated_seen = 0;
    for (int i = 0; i < 100; i++) {
        uint8_t token[32];
        make_token(token, (uint8_t)i);
        uint32_t ts = session_ts_for(i);
        np_edf_header_t local, before, rh;
        ASSERT_OK(np_edf_write_header(&local, token, "1.0.0", ts));
        memcpy(local.reserved, "EDF+C", 5);
        memcpy(local.num_data_records, "120", 3);
        memcpy(local.duration, "1", 1);
        memcpy(local.num_signals, "9", 1);
        before = local;

        for (int p = 0; p < 4; p++) {
            ASSERT_OK(np_edf_make_research_header(&local, &policies[p], &rh));
            ASSERT_OK(np_edf_validate_research_header(&rh, &policies[p]));
            ASSERT(memcmp(&local, &before, sizeof(local)) == 0, "local header changed");
            ASSERT(memcmp(rh.starttime, "00.00.00", 8) == 0, "start time kept");
            ASSERT(memcmp(rh.reserved, local.reserved, NP_EDF_RESERVED_LEN) == 0 &&
                   memcmp(rh.num_data_records, local.num_data_records, NP_EDF_NUM_RECORDS_LEN) == 0 &&
                   memcmp(rh.duration, local.duration, NP_EDF_DURATION_LEN) == 0 &&
                   memcmp(rh.num_signals, local.num_signals, NP_EDF_NUM_SIGNALS_LEN) == 0 &&
                   memcmp(rh.header_bytes, local.header_bytes, NP_EDF_HEADER_BYTES_LEN) == 0 &&
                   memcmp(rh.version, local.version, NP_EDF_VERSION_LEN) == 0,
                   "format fields not kept");

            time_t t = (time_t)ts;
            struct tm tm;
            gmtime_r(&t, &tm);
            int year = tm.tm_year + 1900;
            bool blank = memcmp(rh.local_recording_id, "Startdate X ", 12) == 0;
            if (policies[p].date_rounding_days == 0U || year < 1985 || year > 2084) {
                ASSERT(blank, "date should be blank");
                ASSERT(memcmp(rh.startdate, "01.01.85", 8) == 0, "blank startdate");
                continue;
            }
            if (blank) {
                /* Only allowed when the window start falls before 1985. */
                ASSERT(year == 1985, "blank date for an in-range year");
                continue;
            }
            dated_seen++;
            /* The written date, read back through libc. */
            struct tm w;
            memset(&w, 0, sizeof(w));
            w.tm_mday = (rh.startdate[0] - '0') * 10 + (rh.startdate[1] - '0');
            w.tm_mon  = (rh.startdate[3] - '0') * 10 + (rh.startdate[4] - '0') - 1;
            int yy    = (rh.startdate[6] - '0') * 10 + (rh.startdate[7] - '0');
            w.tm_year = ((yy >= 85) ? 1900 + yy : 2000 + yy) - 1900;
            time_t wt = timegm(&w);
            int64_t wd = (int64_t)wt / 86400;
            int64_t sd = days_of(ts);
            int64_t n  = policies[p].date_rounding_days;
            ASSERT(wd <= sd && sd - wd < n, "date not the window start on or before the session");
            ASSERT(((wd - 4) % n) == 0, "window not anchored to Monday 1970-01-05");
            if (n == 7) {
                struct tm chk;
                gmtime_r(&wt, &chk);
                ASSERT(chk.tm_wday == 1, "weekly date is not a Monday");
            }
        }
    }
    ASSERT(dated_seen > 100, "too few dated cases exercised");
}

/* (a) The gate refuses the local header itself: its "NP" code is the same in
   every extract.  It also refuses another study's code, and a study code where
   the policy asks for 'X'.  The research header never carries token bytes. */
/* @verifies REQ-SW02-12#a */
static void test_research_gate_refuses_local(void)
{
    uint8_t token[32];
    make_token(token, 0xA1U);
    np_edf_header_t local, rh;
    ASSERT_OK(np_edf_write_header(&local, token, "1.0.0", 1790208000U));

    const np_edf_research_policy_t px = { NULL, 7U };
    ASSERT_EQ(np_edf_validate_research_header(&local, &px), NP_EDF_ERR_RESEARCH_LINKAGE);

    bool inc[1];
    np_edf_gate_result_t res;
    ASSERT_OK(np_edf_anon_gate_filter(&local, 1U, &px, inc, &res));
    ASSERT(!inc[0], "local header must be excluded from a research extract");
    ASSERT_EQ(res.excluded, (uint32_t)1);

    uint8_t ra[NP_EDF_STUDY_CODE_RANDOM_BYTES] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70 };
    uint8_t rb[NP_EDF_STUDY_CODE_RANDOM_BYTES] = { 0x11, 0x21, 0x31, 0x41, 0x51, 0x61, 0x71 };
    char ca[NP_EDF_STUDY_CODE_LEN + 1], cb[NP_EDF_STUDY_CODE_LEN + 1];
    ASSERT_OK(np_edf_make_study_code(ra, ca));
    ASSERT_OK(np_edf_make_study_code(rb, cb));
    const np_edf_research_policy_t pa = { ca, 7U }, pb = { cb, 7U };

    ASSERT_OK(np_edf_make_research_header(&local, &pa, &rh));
    ASSERT_OK(np_edf_validate_research_header(&rh, &pa));
    ASSERT_EQ(np_edf_validate_research_header(&rh, &pb), NP_EDF_ERR_RESEARCH_LINKAGE);
    ASSERT_EQ(np_edf_validate_research_header(&rh, &px), NP_EDF_ERR_RESEARCH_LINKAGE);

    /* The token-derived code ("a1abab...") appears nowhere in the research header. */
    char hex[NP_EDF_TOKEN_HEX_CHARS + 1];
    memcpy(hex, local.local_patient_id + 2, NP_EDF_TOKEN_HEX_CHARS);
    hex[NP_EDF_TOKEN_HEX_CHARS] = '\0';
    char flat[NP_EDF_HEADER_SIZE + 1];
    memcpy(flat, &rh, NP_EDF_HEADER_SIZE);
    flat[NP_EDF_HEADER_SIZE] = '\0';
    ASSERT(strstr(flat, hex) == NULL, "research header carries the device code");

    /* A malformed study code is refused as a policy. */
    const np_edf_research_policy_t bad = { "NP0102030405060a", 7U };
    ASSERT_EQ(np_edf_validate_research_header(&rh, &bad), NP_EDF_ERR_INVALID_ARG);
    ASSERT_EQ(np_edf_make_research_header(&local, &bad, &rh), NP_EDF_ERR_INVALID_ARG);
}

/* (b) Dates: an exact day, a time of day, two date fields that disagree, a date
   where the policy asks for blank, and a rounding finer than a week are all
   refused. */
/* @verifies REQ-SW02-12#b */
static void test_research_dates(void)
{
    const np_edf_research_policy_t p7 = { NULL, 7U }, p0 = { NULL, 0U },
                                   p14 = { NULL, 14U }, p3 = { NULL, 3U };
    np_edf_header_t local, rh, h;
    uint8_t token[32];
    make_token(token, 0xB2U);
    /* 2026-09-24 is a Thursday; its week starts Monday 2026-09-21. */
    ASSERT_OK(np_edf_write_header(&local, token, "1.0.0", 1790208000U + 13U * 3600U));
    ASSERT_OK(np_edf_make_research_header(&local, &p7, &rh));
    ASSERT(memcmp(rh.startdate, "21.09.26", 8) == 0, "weekly date");
    ASSERT(memcmp(rh.local_recording_id, "Startdate 21-SEP-2026 X X X ", 28) == 0, "rec date");

    h = rh; poke(h.startdate, 0, "24.09.26"); poke(h.local_recording_id, 10, "24-SEP-2026");
    ASSERT_EQ(np_edf_validate_research_header(&h, &p7), NP_EDF_ERR_RESEARCH_DATE);

    h = rh; poke(h.starttime, 0, "13.00.00");
    ASSERT_EQ(np_edf_validate_research_header(&h, &p7), NP_EDF_ERR_RESEARCH_DATE);

    h = rh; poke(h.startdate, 0, "28.09.26");   /* disagrees with the recording ID */
    ASSERT_EQ(np_edf_validate_research_header(&h, &p7), NP_EDF_ERR_RESEARCH_DATE);

    ASSERT_EQ(np_edf_validate_research_header(&rh, &p0), NP_EDF_ERR_RESEARCH_DATE);

    /* Monday 2026-09-21 is day 20717; (20717 - 4) % 14 == 7, so it is a weekly
       window start but not a 14-day one: refused under p14, whose window
       starts on Monday 2026-09-14. */
    ASSERT_EQ(np_edf_validate_research_header(&rh, &p14), NP_EDF_ERR_RESEARCH_DATE);
    np_edf_header_t r14;
    ASSERT_OK(np_edf_make_research_header(&local, &p14, &r14));
    ASSERT_OK(np_edf_validate_research_header(&r14, &p14));
    ASSERT(memcmp(r14.startdate, "14.09.26", 8) == 0, "14-day window start");

    /* A blank date with any other header startdate is refused. */
    np_edf_header_t blank;
    ASSERT_OK(np_edf_make_research_header(&local, &p0, &blank));
    ASSERT_OK(np_edf_validate_research_header(&blank, &p7));    /* blank always passes */
    h = blank; poke(h.startdate, 0, "24.09.26");
    ASSERT_EQ(np_edf_validate_research_header(&h, &p7), NP_EDF_ERR_RESEARCH_DATE);

    /* Rounding finer than a week is not a policy. */
    ASSERT_EQ(np_edf_validate_research_header(&rh, &p3), NP_EDF_ERR_INVALID_ARG);
    ASSERT_EQ(np_edf_make_research_header(&local, &p3, &h), NP_EDF_ERR_INVALID_ARG);
    bool inc[1];
    ASSERT_OK(np_edf_anon_gate_filter(&rh, 1U, &p3, inc, NULL));
    ASSERT(!inc[0], "a sub-week policy must exclude every file");
}

/* (c) The recording ID is the fixed form: a clinician or hospital name, the
   equipment text of the local header, or trailing text is refused. */
/* @verifies REQ-SW02-12#c */
static void test_research_recording_id(void)
{
    const np_edf_research_policy_t p7 = { NULL, 7U };
    np_edf_header_t local, rh, h;
    uint8_t token[32];
    make_token(token, 0xC3U);
    ASSERT_OK(np_edf_write_header(&local, token, "1.0.0", 1790208000U));
    ASSERT_OK(np_edf_make_research_header(&local, &p7, &rh));

    h = rh; poke(h.local_recording_id, 22, "St_Marys X X");        /* hospital */
    ASSERT_EQ(np_edf_validate_research_header(&h, &p7), NP_EDF_ERR_RECORDING_ID);
    h = rh; poke(h.local_recording_id, 24, "Dr_Smith X");          /* technician */
    ASSERT_EQ(np_edf_validate_research_header(&h, &p7), NP_EDF_ERR_RECORDING_ID);
    h = rh; poke(h.local_recording_id, 60, "Jane");                /* trailing */
    ASSERT_EQ(np_edf_validate_research_header(&h, &p7), NP_EDF_ERR_RECORDING_ID);
    h = rh; memcpy(h.local_recording_id, local.local_recording_id, NP_EDF_RECORDING_ID_LEN);
    ASSERT_EQ(np_edf_validate_research_header(&h, &p7), NP_EDF_ERR_RECORDING_ID);
    h = rh; poke(h.local_recording_id, 0, "Recorded ");
    ASSERT_EQ(np_edf_validate_research_header(&h, &p7), NP_EDF_ERR_RECORDING_ID);
}

/* (d) Every byte of the patient-ID field is checked, in both validators, and
   text in the reserved or numeric format fields is refused by the research one. */
/* @verifies REQ-SW02-12#d */
static void test_whole_patient_field(void)
{
    const np_edf_research_policy_t p7 = { NULL, 7U };
    np_edf_header_t local, rh, h;
    uint8_t token[32];
    make_token(token, 0xD4U);
    ASSERT_OK(np_edf_write_header(&local, token, "1.0.0", 1790208000U));
    ASSERT_OK(np_edf_make_research_header(&local, &p7, &rh));

    /* Local §E.2: a name after the first space used to pass. */
    h = local; poke(h.local_patient_id, NP_EDF_NAME_OFFSET + 12, " Jane Doe");
    ASSERT_EQ(np_edf_validate_privacy_header(&h), NP_EDF_ERR_REAL_NAME);
    h = local; poke(h.local_patient_id, 79, "7");
    ASSERT_EQ(np_edf_validate_privacy_header(&h), NP_EDF_ERR_REAL_NAME);
    h = local; poke(h.local_patient_id, 2, "JaneDoe");             /* text in the code */
    ASSERT_EQ(np_edf_validate_privacy_header(&h), NP_EDF_ERR_PRIVACY_HEADER);
    h = local; h.local_patient_id[NP_EDF_SEX_OFFSET + 1] = '_';    /* separator */
    ASSERT_EQ(np_edf_validate_privacy_header(&h), NP_EDF_ERR_PRIVACY_HEADER);
    ASSERT_OK(np_edf_validate_privacy_header(&local));

    /* Research §E.5. */
    h = rh; poke(h.local_patient_id, 70, "Jane");
    ASSERT_EQ(np_edf_validate_research_header(&h, &p7), NP_EDF_ERR_PRIVACY_HEADER);
    h = rh; poke(h.local_patient_id, 6, "NeurOne_User");           /* name subfield */
    ASSERT_EQ(np_edf_validate_research_header(&h, &p7), NP_EDF_ERR_PRIVACY_HEADER);
    h = rh; poke(h.local_patient_id, 2, "M");                      /* sex */
    ASSERT_EQ(np_edf_validate_research_header(&h, &p7), NP_EDF_ERR_PRIVACY_HEADER);
    h = rh; poke(h.reserved, 0, "EDF+C Jane");
    ASSERT_EQ(np_edf_validate_research_header(&h, &p7), NP_EDF_ERR_PRIVACY_HEADER);
    h = rh; poke(h.num_signals, 0, "J");
    ASSERT_EQ(np_edf_validate_research_header(&h, &p7), NP_EDF_ERR_PRIVACY_HEADER);
    h = rh; poke(h.version, 1, "x");
    ASSERT_EQ(np_edf_validate_research_header(&h, &p7), NP_EDF_ERR_PRIVACY_HEADER);
}

/* ── Main ────────────────────────────────────────────────────────────────────── */

int main(void)
{
    printf("=== NP-FW-EMMC-002 Rev 1 §E — EDF+ Privacy Header Unit Tests ===\n");

    test_write_100_headers();
    test_validator_rejects_real_sex();
    test_validator_rejects_real_dob();
    test_validator_rejects_real_name();
    test_patient_code_prefix();
    test_header_size();
    test_fw_version_in_recording_id();
    test_anon_gate_mixed();
    test_anon_gate_all_pass();
    test_anon_gate_all_fail_does_not_abort();
    test_anon_gate_null_and_empty();
    test_anon_gate_leaves_nothing_for_shdr();
    test_research_header_from_local();
    test_research_gate_refuses_local();
    test_research_dates();
    test_research_recording_id();
    test_whole_patient_field();

    printf("=== Results: %d total failure(s) ===\n", g_fail_count);
    if (g_fail_count == 0) {
        printf("SOFTWARE TESTS: PASS\n");
    } else {
        printf("FAIL — %d assertion(s) failed.\n", g_fail_count);
    }
    return g_fail_count;
}
