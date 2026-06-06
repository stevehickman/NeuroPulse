/* Document: NP-FW-EMMC-002 Rev A §E */
/*
 * NeuroPulse EDF+ Privacy Header — Host Unit Tests
 * Document: NP-FW-EMMC-002 Rev A §E.2 / OI-EMMC2-05
 *
 * Build for host: -DNPTEST_HOST (see CMakeLists.txt NP_BUILD_TESTS option).
 * Return convention: 0 = all PASS, non-zero = count of failed assertions.
 */

#include "np_edf_writer.h"

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

    ASSERT(strstr(rec, "NeuroPulse_v1.2.3") != NULL,
           "recording_id missing NeuroPulse_v1.2.3");
}

/* ── Main ────────────────────────────────────────────────────────────────────── */

int main(void)
{
    printf("=== NP-FW-EMMC-002 Rev A §E — EDF+ Privacy Header Unit Tests ===\n");

    test_write_100_headers();
    test_validator_rejects_real_sex();
    test_validator_rejects_real_dob();
    test_validator_rejects_real_name();
    test_patient_code_prefix();
    test_header_size();
    test_fw_version_in_recording_id();

    printf("=== Results: %d total failure(s) ===\n", g_fail_count);
    if (g_fail_count == 0) {
        printf("SOFTWARE TESTS: PASS\n");
    } else {
        printf("FAIL — %d assertion(s) failed.\n", g_fail_count);
    }
    return g_fail_count;
}
