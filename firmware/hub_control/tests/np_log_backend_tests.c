/*
 * NeurOne Hub Control — Session Log eMMC Backing Host Tests (OI-LOG-01..04)
 * Document: NP-FW-HUB-001 Rev 1 §6, NP-FW-EMMC-001 Rev 1 §12
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
 *   - OI-LFS-11: UHDR one file per session (EMMC-UHDR-12/-13), created
 *     exclusively; the tail belongs to the file it was written in; the
 *     per-file cap (littlefs file_max) fails one session, not the next;
 *     SHDR stays one file
 *   - §6.5 at the logger: an EEG sample block lands after every UHDR record
 *     logged before it (buffered or still in the adaptation ring), and a full
 *     4 KiB buffer is appended before the sync that must cover it
 *   - OI-FWHUB-14: an EEG block logged from an ISR is refused before it
 *     touches any logger state
 *
 * No FreeRTOS, no hardware; the LittleFS-file HAL is host-modeled in
 * np_log_backend.c (NPTEST_HOST).  IEC 62304 Class B — SW-02 hub control.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../include/np_log_backend.h"
#include "../include/np_session_log.h"

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

/* Reset host capture + backend state to a clean baseline with a UHDR
 * session open (UHDR has no file outside a session — OI-LFS-11). */
static void fresh(void)
{
    np_log_test_reset();
    (void)np_log_backend_init();
    (void)np_log_backend_session_begin(1U);
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
    check(np_log_backend_init() == NP_HUB_OK, "init: opens the SHDR log");
    check(np_log_test_open_count(NP_LOG_PART_SHDR) == 1U &&
          np_log_test_open_count(NP_LOG_PART_UHDR) == 0U,
          "init: SHDR opened, UHDR NOT opened at boot (not mounted until unlock)");
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

/* ── OI-LFS-11: one UHDR file per session ────────────────────────────────── */

static void test_uhdr_needs_a_session(void)
{
    np_log_test_reset();
    (void)np_log_backend_init();
    uint8_t one = 0x01;
    check(np_log_hal_uhdr_append(&one, 1U) == NP_HUB_ERR_NO_SESSION,
          "session: UHDR append outside a session → NO_SESSION");
    check(np_log_hal_shdr_append(&one, 1U) == NP_HUB_OK,
          "session: SHDR needs no session");
}

static void test_one_file_per_session(void)
{
    np_log_test_reset();
    (void)np_log_backend_init();
    uint8_t a[100];
    uint8_t b[700];
    memset(a, 0xA1, sizeof a);
    memset(b, 0xB2, sizeof b);

    check(np_log_backend_session_begin(41U) == NP_HUB_OK, "files: session 41 begins");
    check(np_log_test_last_segment(NP_LOG_PART_UHDR) == 41U,
          "files: file named by the session counter (EMMC-UHDR-12)");
    (void)np_log_hal_uhdr_append(a, sizeof a);          /* staged, not written */
    check(np_log_backend_session_end() == NP_HUB_OK, "files: session 41 ends");
    check(np_log_test_segment_len(NP_LOG_PART_UHDR, 0U) == sizeof a,
          "files: session end flushed the staged tail into ITS file");
    check(np_log_test_close_count(NP_LOG_PART_UHDR) == 1U,
          "files: session end closes the file");

    check(np_log_backend_session_begin(42U) == NP_HUB_OK, "files: session 42 begins");
    (void)np_log_hal_uhdr_append(b, sizeof b);
    (void)np_log_backend_session_end();
    check(np_log_test_segment_len(NP_LOG_PART_UHDR, 0U) == sizeof a &&
          np_log_test_segment_len(NP_LOG_PART_UHDR, 1U) == sizeof b,
          "files: each session's bytes are in its own file");
    check(np_log_test_open_count(NP_LOG_PART_SHDR) == 1U,
          "files: SHDR stays one file across sessions");

    uint8_t one = 0x01;
    check(np_log_hal_uhdr_append(&one, 1U) == NP_HUB_ERR_NO_SESSION,
          "files: UHDR closed again after session end");
}

static void test_begin_without_end_keeps_the_tail(void)
{
    np_log_test_reset();
    (void)np_log_backend_init();
    uint8_t a[30];
    memset(a, 0x3C, sizeof a);
    (void)np_log_backend_session_begin(7U);
    (void)np_log_hal_uhdr_append(a, sizeof a);          /* staged in session 7 */
    check(np_log_backend_session_begin(8U) == NP_HUB_OK,
          "tail: a new session begins without the old one ending");
    check(np_log_test_segment_len(NP_LOG_PART_UHDR, 0U) == sizeof a &&
          np_log_test_segment_len(NP_LOG_PART_UHDR, 1U) == 0U,
          "tail: session 7's staged bytes went to session 7's file, not 8's");
}

static void test_reused_counter_fails_closed(void)
{
    np_log_test_reset();
    (void)np_log_backend_init();
    uint8_t a[600];
    memset(a, 0x77, sizeof a);
    (void)np_log_backend_session_begin(5U);
    (void)np_log_hal_uhdr_append(a, sizeof a);
    (void)np_log_backend_session_end();
    size_t before = np_log_test_segment_len(NP_LOG_PART_UHDR, 0U);

    /* e.g. the counter was not persisted across a reboot */
    check(np_log_backend_session_begin(5U) == NP_HUB_ERR_LOG_EXISTS,
          "reuse: an existing session file is refused (LOG_EXISTS), not reopened");
    check(np_log_hal_uhdr_append(a, sizeof a) == NP_HUB_ERR_LOG_FULL,
          "reuse: that session's UHDR appends fail closed");
    check(np_log_test_segment_len(NP_LOG_PART_UHDR, 0U) == before,
          "reuse: the earlier session's file is untouched");
    check(np_log_backend_session_begin(6U) == NP_HUB_OK &&
          np_log_hal_uhdr_append(a, 10U) == NP_HUB_OK,
          "reuse: the next session with a fresh counter logs normally");
}

static void test_per_file_cap(void)
{
    np_log_test_reset();
    (void)np_log_backend_init();
    uint8_t block[NP_LOG_STAGE_BYTES];
    memset(block, 0x19, sizeof block);
    (void)np_log_backend_session_begin(10U);
    /* Stand-in for littlefs's 2 GiB file_max, which a host buffer cannot hold. */
    np_log_test_set_segment_cap(NP_LOG_PART_UHDR, 1000U);
    check(np_log_hal_uhdr_append(block, sizeof block) == NP_HUB_OK,
          "cap: first block fits the file");
    check(np_log_hal_uhdr_append(block, sizeof block) == NP_HUB_ERR_LOG_FULL,
          "cap: a block past the per-file cap → LOG_FULL");
    check(np_log_test_segment_len(NP_LOG_PART_UHDR, 0U) == NP_LOG_STAGE_BYTES,
          "cap: the file's committed prefix is intact");
    check(np_log_backend_session_begin(11U) == NP_HUB_OK &&
          np_log_hal_uhdr_append(block, sizeof block) == NP_HUB_OK,
          "cap: the fault ends with the session — the next file is fresh");
    check(NP_LOG_SEGMENT_MAX_BYTES == 2147483647ULL,
          "cap: the real cap is littlefs's file_max (EMMC-FS-01)");
}

/* ── OI-LFS-11 at the logger: the session count names the file ──────────── */

static np_session_uhdr_record_t g_rec;

static void test_logger_names_files_by_session_count(void)
{
    np_log_test_reset();
    (void)np_log_backend_init();
    np_log_init(10U);                                   /* boot value from SHDR */
    memset(&g_rec, 0, sizeof g_rec);
    np_session_shdr_record_t shdr;
    memset(&shdr, 0, sizeof shdr);

    np_log_session_start(&g_rec);
    check(np_log_session_count() == 11U &&
          np_log_test_last_segment(NP_LOG_PART_UHDR) == 11U,
          "logger: the count increments at session start and names the file");
    np_log_session_end(&g_rec, &shdr);
    check(np_log_test_close_count(NP_LOG_PART_UHDR) == 1U,
          "logger: session end closes the session's file");
    check(np_log_test_segment_len(NP_LOG_PART_UHDR, 0U) > 0U,
          "logger: start and end records are in the session's file");

    np_log_session_start(&g_rec);
    np_log_session_end(&g_rec, &shdr);
    check(np_log_test_last_segment(NP_LOG_PART_UHDR) == 12U &&
          np_log_test_segment_len(NP_LOG_PART_UHDR, 1U) > 0U,
          "logger: the next session gets the next file");

    /* The SHDR session-end record carries THIS session's count. */
    uint32_t count = 0U;
    const uint8_t *cap = np_log_test_captured(NP_LOG_PART_SHDR);
    size_t n = np_log_test_captured_len(NP_LOG_PART_SHDR);
    bool found = false;
    for (size_t i = 0U; i + 5U <= n; i++) {
        if (cap[i] == NP_LOG_TAG_SHDR_SESSION_END) {
            memcpy(&count, cap + i + 1U, sizeof count);
            found = true;                                 /* keep the last one */
        }
    }
    check(found && count == 12U,
          "logger: the SHDR session-end record carries the session's count");
}

static void test_logger_steps_past_a_stale_count(void)
{
    /* Continues from the previous test's files (11, 12): a "reboot" whose
     * SHDR-held count was never advanced (EMMC-SHDR-09 persistence is not
     * implemented).  The files survive the reboot; the logger's RAM does not. */
    (void)np_log_backend_init();
    np_log_init(10U);
    np_session_shdr_record_t shdr;
    memset(&shdr, 0, sizeof shdr);

    np_log_session_start(&g_rec);
    check(np_log_session_count() == 13U &&
          np_log_test_last_segment(NP_LOG_PART_UHDR) == 13U,
          "stale: sessions 11 and 12 exist, so the count steps to 13");
    check(np_log_test_segment_len(NP_LOG_PART_UHDR, 0U) > 0U &&
          np_log_test_segment_len(NP_LOG_PART_UHDR, 1U) > 0U,
          "stale: the existing session files were not reopened or truncated");
    uint8_t one = 0x01;
    check(np_log_hal_uhdr_append(&one, 1U) == NP_HUB_OK,
          "stale: the session logs normally in its new file");
    np_log_session_end(&g_rec, &shdr);
}

/* ── OI-LFS-12: commit before create, and a reboot seeded from the record ─ */

static uint32_t g_committed[8];
static unsigned g_n_commits;
static unsigned g_opens_at_commit[8];

static np_hub_status_t fake_commit(uint32_t count)
{
    if (g_n_commits < 8U) {
        g_committed[g_n_commits]       = count;
        g_opens_at_commit[g_n_commits] = np_log_test_open_count(NP_LOG_PART_UHDR);
        g_n_commits++;
    }
    return NP_HUB_OK;
}

static void test_count_committed_before_file(void)
{
    np_session_shdr_record_t shdr;
    memset(&shdr, 0, sizeof shdr);
    np_log_test_reset();
    (void)np_log_backend_init();
    np_log_init(20U);
    g_n_commits = 0U;
    np_log_set_count_commit(fake_commit);

    np_log_session_start(&g_rec);
    check(g_n_commits == 1U && g_committed[0] == 21U,
          "persist: the new count is committed at session start");
    check(g_opens_at_commit[0] == 0U &&
          np_log_test_open_count(NP_LOG_PART_UHDR) == 1U,
          "persist: committed BEFORE its session file was created");
    np_log_session_end(&g_rec, &shdr);

    /* Reboot, seeded from what was committed: no collision, no stepping. */
    (void)np_log_backend_init();
    np_log_init(g_committed[g_n_commits - 1U]);
    unsigned opens_before = np_log_test_open_count(NP_LOG_PART_UHDR);
    np_log_session_start(&g_rec);
    check(np_log_session_count() == 22U &&
          np_log_test_open_count(NP_LOG_PART_UHDR) == opens_before + 1U,
          "persist: seeded from the committed count, the next session opens "
          "first time with the next number");
    np_log_session_end(&g_rec, &shdr);
    np_log_set_count_commit(NULL);
}

/* ── §6.5 at the logger: time order and append-before-sync ─────────────────── */

/* Serialized record sizes, as np_session_log.c writes them. */
#define START_REC_BYTES (1U + NP_HUB_PROTO_UUID_LEN + sizeof(uint32_t) + sizeof(uint32_t))
#define VNS_REC_BYTES   (1U + sizeof(uint32_t) + 3U * sizeof(float) + sizeof(uint16_t))
#define ADAPT_REC_BYTES (1U + sizeof(np_adaptation_event_t))
#define ZONE_REC_BYTES  (1U + sizeof(uint32_t) + 3U)
#define EEG_HDR_BYTES   7U
#define OPEN_REC_BYTES  (1U + sizeof(uint32_t))
#define CMD_HDR_BYTES   (1U + sizeof(uint32_t) + 4U + sizeof(uint16_t))

static void logger_session_open(void)
{
    np_log_test_reset();
    (void)np_log_backend_init();
    np_log_init(0U);
    np_adapt_log_reset();
    memset(&g_rec, 0, sizeof g_rec);
    np_log_session_start(&g_rec);                       /* session 1, start record durable */
}

static void log_vns(uint32_t session_ms)
{
    np_telem_record_t t;
    memset(&t, 0, sizeof t);
    t.mod_type   = NP_MOD_VNS_HRV;
    t.session_ms = session_ms;
    np_log_telemetry(&t);
}

static void test_eeg_block_keeps_time_order(void)
{
    logger_session_open();
    log_vns(100U);                                      /* buffered in s_uhdr_buf */
    np_adaptation_event_t ev;
    memset(&ev, 0, sizeof ev);
    ev.session_ms = 150U;
    (void)np_adapt_log_event(&ev);                      /* queued in the adaptation ring */

    uint8_t samples[2U * NP_EEG_CHANNELS * NP_EEG_SAMPLE_BYTES];
    for (size_t i = 0; i < sizeof samples; i++) { samples[i] = (uint8_t)(0x40U + i); }
    np_log_eeg_sample_block(samples, 2U, 200U);         /* logged last */
    np_log_flush();

    const uint8_t *cap = np_log_test_captured(NP_LOG_PART_UHDR);
    const size_t vns_at   = START_REC_BYTES;
    const size_t adapt_at = vns_at + VNS_REC_BYTES;
    const size_t eeg_at   = adapt_at + ADAPT_REC_BYTES;
    check(np_log_test_captured_len(NP_LOG_PART_UHDR) ==
              eeg_at + EEG_HDR_BYTES + sizeof samples,
          "order: every record reached the session file exactly once");
    check(cap[0] == NP_LOG_TAG_UHDR_SESSION_START &&
          cap[vns_at] == NP_LOG_TAG_UHDR_VNS_HRV,
          "order: a buffered record logged before an EEG block precedes it in the file");
    check(cap[adapt_at] == NP_LOG_TAG_UHDR_ADAPT_EVENT,
          "order: an adaptation event queued before an EEG block precedes it in the file");
    const uint8_t hdr[EEG_HDR_BYTES] = { 0xEEU, 0U, 0U, 0U, 200U, 0U, 2U };
    check(memcmp(cap + eeg_at, hdr, sizeof hdr) == 0 &&
          memcmp(cap + eeg_at + EEG_HDR_BYTES, samples, sizeof samples) == 0,
          "order: the EEG block follows them, header then samples, intact");
}

static void test_flush_syncs_eeg_blocks(void)
{
    logger_session_open();
    log_vns(100U);
    uint8_t samples[NP_EEG_CHANNELS * NP_EEG_SAMPLE_BYTES];
    memset(samples, 0x5E, sizeof samples);
    np_log_eeg_sample_block(samples, 1U, 200U);         /* leaves s_uhdr_buf empty */
    np_log_flush();
    size_t logged = START_REC_BYTES + VNS_REC_BYTES + EEG_HDR_BYTES + sizeof samples;
    check(np_log_test_synced_len(NP_LOG_PART_UHDR) == logged,
          "durability: the periodic flush syncs EEG blocks even with the "
          "record buffer empty");
}

static bool s_fake_in_isr;
static bool fake_in_isr(void) { return s_fake_in_isr; }

static void test_eeg_block_refused_from_isr(void)
{
    /* OI-FWHUB-14: from an ISR the call must leave every piece of logger state
     * as it found it — the record buffer, the adaptation ring and the backend
     * staging a task may be part-way through. */
    logger_session_open();
    log_vns(100U);                                      /* buffered, not handed down */
    np_adaptation_event_t ev;
    memset(&ev, 0, sizeof ev);
    ev.session_ms = 150U;
    (void)np_adapt_log_event(&ev);                      /* queued in the ring */
    const size_t before = np_log_test_captured_len(NP_LOG_PART_UHDR);

    uint8_t samples[NP_EEG_CHANNELS * NP_EEG_SAMPLE_BYTES];
    memset(samples, 0x3C, sizeof samples);
    np_log_set_isr_check(fake_in_isr);
    s_fake_in_isr = true;
    np_hub_status_t rc = np_log_eeg_sample_block(samples, 1U, 200U);
    check(rc == NP_HUB_ERR_GENERIC, "isr: a block logged from an ISR is refused");
    check(np_log_test_captured_len(NP_LOG_PART_UHDR) == before,
          "isr: the refusal hands nothing down (buffer, ring and block all held back)");

    s_fake_in_isr = false;
    np_log_flush();
    check(np_log_test_captured_len(NP_LOG_PART_UHDR) ==
              before + VNS_REC_BYTES + ADAPT_REC_BYTES,
          "isr: the task-side records are intact and the refused block never lands");
    check(np_log_eeg_sample_block(samples, 1U, 300U) == NP_HUB_OK,
          "isr: from task context the same call is accepted");
    check(np_log_eeg_sample_block(NULL, 1U, 300U) == NP_HUB_ERR_INVALID_ARG,
          "isr: a NULL block is INVALID_ARG");
    np_log_set_isr_check(NULL);
}

static void test_uhdr_overflow_syncs_after_append(void)
{
    /* Records are serialized field by field, so the overflow can fall inside
     * a record: the sync must cover every byte handed down before it — every
     * complete earlier record and the head of the one that overflowed. */
    logger_session_open();
    unsigned syncs0 = np_log_test_sync_count(NP_LOG_PART_UHDR);
    /* Session start already made its record durable (OI-FMEA-09), so the
     * buffer starts empty and the overflow is measured from what is there. */
    const size_t base = np_log_test_captured_len(NP_LOG_PART_UHDR);
    size_t logged = START_REC_BYTES;                            /* bytes logged so far */
    size_t before_record = 0U;
    size_t handed_down = 0U;
    bool overflowed = false;
    for (unsigned i = 0U; i < 1000U && !overflowed; i++) {
        before_record = logged;
        log_vns(i);
        logged += VNS_REC_BYTES;
        if (np_log_test_sync_count(NP_LOG_PART_UHDR) != syncs0) {
            overflowed  = true;
            handed_down = np_log_test_captured_len(NP_LOG_PART_UHDR);
        }
    }
    check(base == START_REC_BYTES && overflowed &&
          np_log_test_sync_count(NP_LOG_PART_UHDR) == syncs0 + 1U &&
          handed_down - base > 4096U - VNS_REC_BYTES &&
          handed_down - base <= 4096U,
          "durability: filling the 4 KiB UHDR buffer hands it down with one sync");
    check(np_log_test_synced_len(NP_LOG_PART_UHDR) == handed_down &&
          handed_down >= before_record,
          "durability: the overflow sync covers every UHDR byte handed down "
          "before it (appended first, then synced)");
    np_log_flush();
    check(np_log_test_captured_len(NP_LOG_PART_UHDR) == logged,
          "durability: no UHDR byte lost or duplicated across the overflow");
}

static void test_shdr_overflow_syncs_after_append(void)
{
    /* Records are serialized field by field, so the overflow can fall inside
     * a record: the sync must cover every byte handed down before it — every
     * complete earlier record and the head of the one that overflowed. */
    logger_session_open();
    unsigned syncs0 = np_log_test_sync_count(NP_LOG_PART_SHDR);
    /* The session-open marker is already durable (OI-FMEA-09). */
    const size_t base = np_log_test_captured_len(NP_LOG_PART_SHDR);
    size_t logged = OPEN_REC_BYTES;                /* bytes logged so far */
    size_t before_record = 0U;
    size_t handed_down = 0U;
    bool overflowed = false;
    for (unsigned i = 0U; i < 1000U && !overflowed; i++) {
        before_record = logged;
        np_log_shdr_zone_auth((uint8_t)i, NP_MOD_PBM_BASE, true);
        logged += ZONE_REC_BYTES;
        if (np_log_test_sync_count(NP_LOG_PART_SHDR) != syncs0) {
            overflowed  = true;
            handed_down = np_log_test_captured_len(NP_LOG_PART_SHDR);
        }
    }
    check(base == OPEN_REC_BYTES && overflowed &&
          np_log_test_sync_count(NP_LOG_PART_SHDR) == syncs0 + 1U &&
          handed_down - base > 4096U - ZONE_REC_BYTES &&
          handed_down - base <= 4096U,
          "durability: filling the 4 KiB SHDR buffer hands it down with one sync");
    check(np_log_test_synced_len(NP_LOG_PART_SHDR) == handed_down &&
          handed_down >= before_record,
          "durability: the overflow sync covers every SHDR byte handed down "
          "before it (appended first, then synced)");
    np_log_flush();
    check(np_log_test_captured_len(NP_LOG_PART_SHDR) == logged,
          "durability: no SHDR byte lost or duplicated across the overflow");
}

/* OI-FWHUB-15: the runner calls np_log_session_end() and THEN np_log_flush(). */
static void test_session_end_keeps_queued_adapt_events(void)
{
    logger_session_open();
    np_adaptation_event_t ev;
    memset(&ev, 0, sizeof ev);
    ev.session_ms = 900U;
    (void)np_adapt_log_event(&ev);                      /* still queued at session end */
    np_session_shdr_record_t shdr;
    memset(&shdr, 0, sizeof shdr);
    np_log_session_end(&g_rec, &shdr);                  /* runner order */
    np_log_flush();

    const uint8_t *cap = np_log_test_captured(NP_LOG_PART_UHDR);
    check(np_log_test_segment_len(NP_LOG_PART_UHDR, 0U) >
              START_REC_BYTES + ADAPT_REC_BYTES &&
          cap[START_REC_BYTES] == NP_LOG_TAG_UHDR_ADAPT_EVENT,
          "session end: an adaptation event queued at session end is in its "
          "session's file");
    check(cap[START_REC_BYTES + ADAPT_REC_BYTES] == NP_LOG_TAG_UHDR_SESSION_END,
          "session end: it precedes the session-end record");

    np_log_session_start(&g_rec);                       /* the next session */
    np_log_flush();
    check(np_log_test_segment_len(NP_LOG_PART_UHDR, 1U) == START_REC_BYTES,
          "session end: nothing from the previous session leaks into the next file");
    np_session_shdr_record_t shdr2;
    memset(&shdr2, 0, sizeof shdr2);
    np_log_session_end(&g_rec, &shdr2);
}

/* ── OI-FMEA-09: the dirty-session marker and the commanded-dose record ─────── */

static bool shdr_has_count_record(uint8_t tag, uint32_t want)
{
    const uint8_t *cap = np_log_test_captured(NP_LOG_PART_SHDR);
    size_t n = np_log_test_captured_len(NP_LOG_PART_SHDR);
    for (size_t i = 0U; i + 5U <= n; i++) {
        uint32_t c;
        memcpy(&c, cap + i + 1U, sizeof c);
        if (cap[i] == tag && c == want) {
            return true;
        }
    }
    return false;
}

static void test_session_open_marker_is_durable_before_return(void)
{
    np_log_test_reset();
    (void)np_log_backend_init();
    np_log_init(40U);
    np_adapt_log_reset();
    memset(&g_rec, 0, sizeof g_rec);
    np_log_session_start(&g_rec);

    const uint8_t *cap = np_log_test_captured(NP_LOG_PART_SHDR);
    uint32_t count = 0U;
    memcpy(&count, cap + 1U, sizeof count);
    check(np_log_test_captured_len(NP_LOG_PART_SHDR) == OPEN_REC_BYTES &&
          cap[0] == NP_LOG_TAG_SHDR_SESSION_OPEN && count == 41U,
          "dirty marker: session start writes SHDR SESSION_OPEN with this "
          "session's count, and nothing else");
    check(np_log_test_synced_len(NP_LOG_PART_SHDR) == OPEN_REC_BYTES,
          "dirty marker: it is SYNCED before session start returns, i.e. "
          "before any stimulation can begin");
    check(np_log_test_synced_len(NP_LOG_PART_UHDR) == START_REC_BYTES,
          "dirty marker: the UHDR start record is synced too, so a session "
          "file with a start and no end is itself a dirty marker");

    /* Power lost here: no end record. What survives says so. */
    check(!shdr_has_count_record(NP_LOG_TAG_SHDR_SESSION_END, 41U),
          "dirty marker: an interrupted session leaves OPEN with no END");

    np_session_shdr_record_t shdr;
    memset(&shdr, 0, sizeof shdr);
    np_log_session_end(&g_rec, &shdr);
    check(shdr_has_count_record(NP_LOG_TAG_SHDR_SESSION_OPEN, 41U) &&
          shdr_has_count_record(NP_LOG_TAG_SHDR_SESSION_END, 41U),
          "dirty marker: a clean session pairs OPEN and END on one count");
}

static void test_command_record_layout(void)
{
    logger_session_open();
    const size_t base = np_log_test_captured_len(NP_LOG_PART_UHDR);

    np_session_cmd_t cmd;
    memset(&cmd, 0, sizeof cmd);
    cmd.mod_type    = NP_MOD_TDCS;
    cmd.slot_id     = NP_HUB_SLOT_TDCS;
    cmd.target_kind = NP_PROTO_TARGET_SLOT;
    cmd.params_len  = 3U;
    cmd.params[0] = 0xA1; cmd.params[1] = 0xA2; cmd.params[2] = 0xA3;
    np_log_command(&cmd, 0x01020304U, true);

    np_session_cmd_t sock;
    memset(&sock, 0, sizeof sock);
    sock.mod_type    = NP_MOD_PBM_BASE;
    sock.slot_id     = NP_HUB_SLOT_NONE;
    sock.target_kind = NP_PROTO_TARGET_SOCKET_MASK;
    sock.params_len  = 0U;                             /* a stop */
    memset(sock.socket_mask, 0x5C, sizeof sock.socket_mask);
    np_log_command(&sock, 7U, false);
    np_log_flush();

    const uint8_t *cap = np_log_test_captured(NP_LOG_PART_UHDR) + base;
    size_t n = np_log_test_captured_len(NP_LOG_PART_UHDR) - base;
    uint32_t ms;
    uint16_t plen;
    memcpy(&ms, cap + 1U, sizeof ms);
    memcpy(&plen, cap + 9U, sizeof plen);
    check(cap[0] == NP_LOG_TAG_UHDR_COMMAND && ms == 0x01020304U &&
          cap[5] == NP_MOD_TDCS && cap[6] == NP_PROTO_TARGET_SLOT &&
          cap[7] == NP_HUB_SLOT_TDCS && cap[8] == 1U && plen == 3U &&
          cap[11] == 0xA1 && cap[12] == 0xA2 && cap[13] == 0xA3,
          "command: tag, session_ms, mod, target kind, slot, accepted, "
          "params_len and the signed params, in that order");

    const uint8_t *c2 = cap + CMD_HDR_BYTES + 3U;
    check(c2[0] == NP_LOG_TAG_UHDR_COMMAND && c2[8] == 0U &&
          c2[6] == NP_PROTO_TARGET_SOCKET_MASK &&
          c2[CMD_HDR_BYTES] == 0x5C &&
          c2[CMD_HDR_BYTES + NP_HUB_SOCKET_MASK_BYTES - 1U] == 0x5C,
          "command: a refused command is recorded as refused, and a socket "
          "command carries its socket mask");
    check(n == 2U * CMD_HDR_BYTES + 3U + NP_HUB_SOCKET_MASK_BYTES,
          "command: exactly the two records, nothing more");
    check(np_log_test_captured_len(NP_LOG_PART_SHDR) == OPEN_REC_BYTES,
          "command: the commanded dose is UHDR only; nothing reaches SHDR");

    np_session_cmd_t bad = cmd;
    bad.params_len = NP_HUB_PROTO_PARAMS_MAX + 1U;
    size_t before = np_log_test_captured_len(NP_LOG_PART_UHDR);
    np_log_command(&bad, 0U, true);
    np_log_command(NULL, 0U, true);
    np_log_flush();
    check(np_log_test_captured_len(NP_LOG_PART_UHDR) == before,
          "command: an out-of-range params_len or NULL writes nothing");
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
    test_uhdr_needs_a_session();
    test_one_file_per_session();
    test_begin_without_end_keeps_the_tail();
    test_reused_counter_fails_closed();
    test_per_file_cap();
    test_logger_names_files_by_session_count();
    test_logger_steps_past_a_stale_count();
    test_count_committed_before_file();
    test_eeg_block_keeps_time_order();
    test_flush_syncs_eeg_blocks();
    test_eeg_block_refused_from_isr();
    test_uhdr_overflow_syncs_after_append();
    test_shdr_overflow_syncs_after_append();
    test_session_end_keeps_queued_adapt_events();
    test_session_open_marker_is_durable_before_return();
    test_command_record_layout();

    if (g_failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}
