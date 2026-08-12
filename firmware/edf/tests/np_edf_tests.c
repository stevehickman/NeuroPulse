/* Document: NP-FW-EMMC-002 Rev 1 §E */
/*
 * NeurOne EDF+ Privacy Header — Host Unit Tests
 * Document: NP-FW-EMMC-002 Rev 1 §E.2 / OI-EMMC2-05
 *
 * Build for host: -DNPTEST_HOST (see CMakeLists.txt NP_BUILD_TESTS option).
 * Return convention: 0 = all PASS, non-zero = count of failed assertions.
 */

#include "np_edf_writer.h"
#include "np_edf_anon_gate.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

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

static void test_write_100_headers(void)
{
    int failures_before = g_fail_count;
    for (int i = 0; i < 100; i++) {
        uint8_t token[32];
        make_token(token, (uint8_t)i);

        np_edf_header_t header;
        ASSERT_OK(np_edf_write_header(&header, token, "1.0.0", (uint32_t)0));
        ASSERT_OK(np_edf_validate_privacy_header(&header));
    }
    if (g_fail_count == failures_before) {
        printf("PASS: 100/100 header generation tests\n");
    } else {
        printf("FAIL: header generation (%d failures)\n",
               g_fail_count - failures_before);
    }
}

/* ── Test 2: validator rejects a real sex code ───────────────────────────────── */

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

static void test_header_size(void)
{
    ASSERT_EQ(sizeof(np_edf_header_t), (size_t)256);
}

/* ── Test 7: firmware version appears in the recording id ─────────────────────── */

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

#define MAX_FAULTS 16

typedef struct {
    uint32_t indices[MAX_FAULTS];
    uint8_t  codes[MAX_FAULTS];
    int      n;
} fault_recorder_t;

/* np_edf_shdr_fault_cb: records each excluded file's index + event code. */
static void record_fault(uint32_t file_index, uint8_t ev_code, void *user)
{
    fault_recorder_t *r = (fault_recorder_t *)user;
    if (r->n < MAX_FAULTS) {
        r->indices[r->n] = file_index;
        r->codes[r->n]   = ev_code;
        r->n++;
    }
}

/* Writes a valid privacy header for the given seed. */
static void make_valid_header(np_edf_header_t *h, uint8_t seed)
{
    uint8_t token[32];
    make_token(token, seed);
    ASSERT_OK(np_edf_write_header(h, token, "1.0.0", 0U));
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
static void test_anon_gate_mixed(void)
{
    np_edf_header_t headers[6];
    for (uint8_t i = 0U; i < 6U; i++) {
        make_valid_header(&headers[i], (uint8_t)(0x60U + i));
    }
    headers[1].local_patient_id[NP_EDF_SEX_OFFSET] = 'M';  /* real sex code */
    corrupt_name(&headers[4]);                             /* real name     */

    bool included[6];
    fault_recorder_t rec = { .n = 0 };
    np_edf_gate_result_t result;

    np_edf_status_t st = np_edf_anon_gate_filter(headers, 6U, included,
                                                 record_fault, &rec, &result);

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

    /* Exactly the two violating files fire a fault, with the right code. */
    ASSERT_EQ(rec.n, 2);
    ASSERT_EQ(rec.indices[0], (uint32_t)1);
    ASSERT_EQ(rec.indices[1], (uint32_t)4);
    ASSERT_EQ(rec.codes[0], (uint8_t)NP_EDF_SHDR_EV_PRIVACY_HEADER_VIOLATION);
    ASSERT_EQ(rec.codes[1], (uint8_t)NP_EDF_SHDR_EV_PRIVACY_HEADER_VIOLATION);
}

/* All-compliant set: every file included, no faults. */
static void test_anon_gate_all_pass(void)
{
    np_edf_header_t headers[5];
    for (uint8_t i = 0U; i < 5U; i++) {
        make_valid_header(&headers[i], (uint8_t)(0x70U + i));
    }

    bool included[5];
    fault_recorder_t rec = { .n = 0 };
    np_edf_gate_result_t result;

    ASSERT_OK(np_edf_anon_gate_filter(headers, 5U, included,
                                      record_fault, &rec, &result));
    ASSERT_EQ(result.included, (uint32_t)5);
    ASSERT_EQ(result.excluded, (uint32_t)0);
    ASSERT_EQ(rec.n, 0);
    for (int i = 0; i < 5; i++) {
        ASSERT(included[i], "all files should be included");
    }
}

/* All-violating set: gate still returns OK (does not abort); all excluded. */
static void test_anon_gate_all_fail_does_not_abort(void)
{
    np_edf_header_t headers[3];
    for (uint8_t i = 0U; i < 3U; i++) {
        make_valid_header(&headers[i], (uint8_t)(0x80U + i));
        headers[i].local_patient_id[NP_EDF_DOB_OFFSET] = '9';  /* real DOB */
    }

    bool included[3];
    fault_recorder_t rec = { .n = 0 };
    np_edf_gate_result_t result;

    np_edf_status_t st = np_edf_anon_gate_filter(headers, 3U, included,
                                                 record_fault, &rec, &result);
    ASSERT_EQ(st, NP_EDF_OK);            /* every file bad, still no abort */
    ASSERT_EQ(result.included, (uint32_t)0);
    ASSERT_EQ(result.excluded, (uint32_t)3);
    ASSERT_EQ(rec.n, 3);
    for (int i = 0; i < 3; i++) {
        ASSERT(!included[i], "all files should be excluded");
    }
}

/* Null / empty argument handling, and NULL fault_cb tolerance. */
static void test_anon_gate_null_and_empty(void)
{
    np_edf_header_t headers[2];
    make_valid_header(&headers[0], 0x90U);
    make_valid_header(&headers[1], 0x91U);
    headers[1].local_patient_id[NP_EDF_SEX_OFFSET] = 'F';  /* violation */
    bool included[2];
    np_edf_gate_result_t result = { .included = 99, .excluded = 99 };

    /* count == 0 is a valid no-op; result is zeroed. */
    ASSERT_OK(np_edf_anon_gate_filter(headers, 0U, included, NULL, NULL, &result));
    ASSERT_EQ(result.included, (uint32_t)0);
    ASSERT_EQ(result.excluded, (uint32_t)0);

    /* Non-zero count with NULL arrays is rejected. */
    ASSERT_EQ(np_edf_anon_gate_filter(NULL, 2U, included, NULL, NULL, NULL),
              NP_EDF_ERR_INVALID_ARG);
    ASSERT_EQ(np_edf_anon_gate_filter(headers, 2U, NULL, NULL, NULL, NULL),
              NP_EDF_ERR_INVALID_ARG);

    /* NULL fault_cb + NULL result must still process and exclude safely. */
    ASSERT_OK(np_edf_anon_gate_filter(headers, 2U, included, NULL, NULL, NULL));
    ASSERT(included[0], "file 0 should be included");
    ASSERT(!included[1], "file 1 should be excluded even with no fault_cb");
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

    printf("=== Results: %d total failure(s) ===\n", g_fail_count);
    if (g_fail_count == 0) {
        printf("SOFTWARE TESTS: PASS\n");
    } else {
        printf("FAIL — %d assertion(s) failed.\n", g_fail_count);
    }
    return g_fail_count;
}
