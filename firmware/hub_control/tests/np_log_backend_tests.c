/*
 * NeurOne Hub Control — Session Log eMMC Backing Host Tests (OI-LOG-01..04)
 * Document: NP-FW-HUB-001 Rev A §6, NP-FW-EMMC-001 Rev A §12
 *
 * Host-native tests for the block-coalescing append writer that backs the four
 * np_log_hal_* entry points:
 *   - append/flush round-trip; exact byte fidelity per partition
 *   - block coalescing (auto-commit at NP_LOG_STAGE_BYTES; sub-block tail only
 *     written on flush)
 *   - large-append passthrough (EEG sample blocks bypass staging)
 *   - UHDR/SHDR isolation (independent streams + sync counts)
 *   - write-failure fault latch (torn write stops further records)
 *   - capacity guard → NP_HUB_ERR_LOG_FULL
 *
 * No FreeRTOS, no hardware; the LittleFS-file HAL is host-modeled in
 * np_log_backend.c (NPTEST_HOST).  IEC 62304 Class B — SW-02 hub control.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../include/np_log_backend.h"

static int g_failures = 0;

static void check(int cond, const char *name)
{
    if (cond) {
        printf("PASS: %s\n", name);
    } else {
        printf("FAIL: %s\n", name);
        g_failures++;
    }
}

/* Reset host capture + backend state to a clean, opened baseline. */
static void fresh(void)
{
    np_log_test_reset();
    (void)np_log_backend_init();
}

static bool cap_equals(np_log_part_t part, const uint8_t *expect, size_t len)
{
    return np_log_test_captured_len(part) == len &&
           memcmp(np_log_test_captured(part), expect, len) == 0;
}

/* ── Tests ────────────────────────────────────────────────────────────────────── */

static void test_init_ok(void)
{
    np_log_test_reset();
    check(np_log_backend_init() == NP_HUB_OK, "init: opens both partitions");
    check(np_log_test_captured_len(NP_LOG_PART_UHDR) == 0U,
          "init: UHDR starts empty");
    check(np_log_test_captured_len(NP_LOG_PART_SHDR) == 0U,
          "init: SHDR starts empty");
}

static void test_append_flush_roundtrip(void)
{
    fresh();
    const uint8_t rec[] = { 0x10, 0xDE, 0xAD, 0xBE, 0xEF };
    check(np_log_hal_uhdr_append(rec, sizeof rec) == NP_HUB_OK,
          "roundtrip: append ok");
    /* Sub-block record is buffered, not yet written. */
    check(np_log_test_captured_len(NP_LOG_PART_UHDR) == 0U,
          "roundtrip: sub-block buffered before flush");
    check(np_log_hal_uhdr_flush() == NP_HUB_OK, "roundtrip: flush ok");
    check(cap_equals(NP_LOG_PART_UHDR, rec, sizeof rec),
          "roundtrip: captured bytes match input");
    check(np_log_test_sync_count(NP_LOG_PART_UHDR) == 1U,
          "roundtrip: one sync issued");
}

static void test_coalesce_small_appends(void)
{
    fresh();
    uint8_t expect[600];
    for (size_t i = 0; i < sizeof expect; i++) {
        expect[i] = (uint8_t)(i & 0xFFU);
        uint8_t b = expect[i];
        (void)np_log_hal_uhdr_append(&b, 1U);
    }
    /* One full block (512) auto-committed; 88-byte tail still staged. */
    check(np_log_test_captured_len(NP_LOG_PART_UHDR) == NP_LOG_STAGE_BYTES,
          "coalesce: one block auto-committed at 512 bytes");
    check(np_log_hal_uhdr_flush() == NP_HUB_OK, "coalesce: flush tail");
    check(cap_equals(NP_LOG_PART_UHDR, expect, sizeof expect),
          "coalesce: full 600-byte stream intact after flush");
}

static void test_large_append_passthrough(void)
{
    fresh();
    uint8_t big[1100];
    for (size_t i = 0; i < sizeof big; i++) { big[i] = (uint8_t)((i * 7U) & 0xFFU); }
    check(np_log_hal_uhdr_append(big, sizeof big) == NP_HUB_OK,
          "passthrough: large append ok");
    /* Two whole blocks (1024) written straight through; 76-byte tail staged. */
    check(np_log_test_captured_len(NP_LOG_PART_UHDR) == 2U * NP_LOG_STAGE_BYTES,
          "passthrough: whole blocks bypass staging");
    check(np_log_hal_uhdr_flush() == NP_HUB_OK, "passthrough: flush tail");
    check(cap_equals(NP_LOG_PART_UHDR, big, sizeof big),
          "passthrough: 1100-byte block intact");
}

static void test_eeg_header_then_block(void)
{
    /* Mirrors np_log_eeg_sample_block: a 7-byte header then a large sample
     * block, appended separately.  Must coalesce contiguously. */
    fresh();
    uint8_t stream[7 + 900];
    for (size_t i = 0; i < sizeof stream; i++) { stream[i] = (uint8_t)(0xA0U ^ i); }
    check(np_log_hal_uhdr_append(stream, 7U) == NP_HUB_OK, "eeg: header append");
    check(np_log_hal_uhdr_append(stream + 7, sizeof stream - 7) == NP_HUB_OK,
          "eeg: sample block append");
    check(np_log_hal_uhdr_flush() == NP_HUB_OK, "eeg: flush");
    check(cap_equals(NP_LOG_PART_UHDR, stream, sizeof stream),
          "eeg: header and block contiguous");
}

static void test_uhdr_shdr_isolation(void)
{
    fresh();
    const uint8_t u[] = { 0x10, 0x01, 0x02, 0x03 };
    const uint8_t s[] = { 0x80, 0xAA, 0xBB };
    (void)np_log_hal_uhdr_append(u, sizeof u);
    (void)np_log_hal_shdr_append(s, sizeof s);
    (void)np_log_hal_uhdr_flush();
    (void)np_log_hal_shdr_flush();
    check(cap_equals(NP_LOG_PART_UHDR, u, sizeof u), "isolation: UHDR stream correct");
    check(cap_equals(NP_LOG_PART_SHDR, s, sizeof s), "isolation: SHDR stream correct");
    check(np_log_test_sync_count(NP_LOG_PART_UHDR) == 1U &&
          np_log_test_sync_count(NP_LOG_PART_SHDR) == 1U,
          "isolation: independent sync counts");
}

static void test_flush_syncs_when_idle(void)
{
    fresh();
    const uint8_t rec[] = { 0x11, 0x42 };
    (void)np_log_hal_shdr_append(rec, sizeof rec);
    (void)np_log_hal_shdr_flush();   /* sync #1, commits tail */
    check(np_log_hal_shdr_flush() == NP_HUB_OK, "idle-flush: returns ok with nothing pending");
    check(np_log_test_sync_count(NP_LOG_PART_SHDR) == 2U,
          "idle-flush: still issues a sync barrier");
}

static void test_write_failure_latches_fault(void)
{
    fresh();
    np_log_test_fail_next_append(NP_LOG_PART_UHDR);
    uint8_t block[NP_LOG_STAGE_BYTES];
    memset(block, 0x5A, sizeof block);
    /* Full block forces a part_append, which the stub fails once. */
    check(np_log_hal_uhdr_append(block, sizeof block) == NP_HUB_ERR_GENERIC,
          "fault: torn write surfaces the HAL error");
    /* Fault is sticky — no further records accepted. */
    uint8_t one = 0x10;
    check(np_log_hal_uhdr_append(&one, 1U) == NP_HUB_ERR_LOG_FULL,
          "fault: subsequent append rejected");
    check(np_log_hal_uhdr_flush() == NP_HUB_ERR_LOG_FULL,
          "fault: flush rejected while faulted");
    /* SHDR is unaffected by a UHDR fault. */
    check(np_log_hal_shdr_append(&one, 1U) == NP_HUB_OK,
          "fault: SHDR unaffected by UHDR fault");
}

static void test_capacity_full(void)
{
    fresh();
    np_log_test_set_capacity(NP_LOG_PART_SHDR, 1000U);
    uint8_t block[NP_LOG_STAGE_BYTES];
    memset(block, 0x22, sizeof block);
    check(np_log_hal_shdr_append(block, sizeof block) == NP_HUB_OK,
          "capacity: first block within capacity");
    /* 512 + 512 = 1024 > 1000 → refuse. */
    check(np_log_hal_shdr_append(block, sizeof block) == NP_HUB_ERR_LOG_FULL,
          "capacity: exceeding partition size returns LOG_FULL");
    uint8_t one = 0x80;
    check(np_log_hal_shdr_append(&one, 1U) == NP_HUB_ERR_LOG_FULL,
          "capacity: fault latched after full");
}

static void test_null_and_zero_args(void)
{
    fresh();
    check(np_log_hal_uhdr_append(NULL, 4U) == NP_HUB_ERR_INVALID_ARG,
          "args: NULL buf with len>0 rejected");
    check(np_log_hal_uhdr_append(NULL, 0U) == NP_HUB_OK,
          "args: zero-length append is a no-op");
    check(np_log_test_captured_len(NP_LOG_PART_UHDR) == 0U,
          "args: no bytes written by no-op");
}

int main(void)
{
    printf("── np_log_backend_tests (OI-LOG-01..04) ──\n");

    test_init_ok();
    test_append_flush_roundtrip();
    test_coalesce_small_appends();
    test_large_append_passthrough();
    test_eeg_header_then_block();
    test_uhdr_shdr_isolation();
    test_flush_syncs_when_idle();
    test_write_failure_latches_fault();
    test_capacity_full();
    test_null_and_zero_args();

    if (g_failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}
