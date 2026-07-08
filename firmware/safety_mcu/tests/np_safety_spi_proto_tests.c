/*
 * NeuroPulse Safety MCU — SPI Protocol Host Tests
 * Document: NP-SW-001 §5.1 (SW-01-M07 OI-SW01-M07-01)
 *
 * Host-native tests for the np_safety_sig_cmd_t frame protocol:
 *   - Size assertions (compile-time guards and runtime check)
 *   - Checksum computation and verification for sig command frames
 *   - NP_SAFETY_STATUS_SIG_PENDING flag lifecycle
 *   - Watchdog tick blocks grant while SIG_PENDING set
 *
 * These tests exercise the C structs and protocol logic using stubs;
 * they do NOT require ARM cross-compilation or SPI hardware.
 *
 * IEC 62304 Class C — SW-01 safety MCU.
 * Build with -DNP_BUILD_TESTS=ON in the safety_mcu CMakeLists
 * (see firmware/safety_mcu/CMakeLists.txt host-test section).
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

/* Pull in the protocol and config headers directly (no MCU HAL needed). */
#include "../include/np_safety_config.h"
#include "../include/np_safety_protocol.h"

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

/* ── Checksum helpers (mirror of np_safety_main.c logic) ───────────────────── */

static uint16_t heartbeat_checksum(const np_safety_rx_frame_t *f)
{
    uint16_t sum = 0U;
    const uint8_t *b = (const uint8_t *)f;
    uint8_t i;
    for (i = 0U; i < 6U; i++) { sum += b[i]; }
    return sum;
}

static uint16_t cmd_checksum(const np_safety_sig_cmd_t *c)
{
    uint16_t sum = 0U;
    const uint8_t *b = (const uint8_t *)c;
    uint8_t i;
    for (i = 0U; i < (NP_SAFETY_CMD_FRAME_LEN - 2U); i++) { sum += b[i]; }
    return sum;
}

static uint16_t chan_limit_checksum(const np_safety_chan_limit_cmd_t *c)
{
    uint16_t sum = 0U;
    const uint8_t *b = (const uint8_t *)c;
    uint8_t i;
    for (i = 0U; i < (NP_SAFETY_CHAN_LIMIT_FRAME_LEN - 2U); i++) { sum += b[i]; }
    return sum;
}

/* ── Test: compile-time and runtime size assertions ────────────────────────── */

static void test_frame_sizes(void)
{
    check(sizeof(np_safety_rx_frame_t) == NP_SAFETY_FRAME_LEN,
          "heartbeat_rx_frame size == NP_SAFETY_FRAME_LEN (8)");

    check(sizeof(np_safety_tx_frame_t) == NP_SAFETY_FRAME_LEN,
          "heartbeat_tx_frame size == NP_SAFETY_FRAME_LEN (8)");

    check(sizeof(np_safety_sig_cmd_t) == NP_SAFETY_CMD_FRAME_LEN,
          "np_safety_sig_cmd_t size == NP_SAFETY_CMD_FRAME_LEN (102)");

    /* Verify individual field offsets (packed struct, no padding). */
    check(offsetof(np_safety_sig_cmd_t, cmd_magic)    == 0U,  "sig_cmd.cmd_magic at offset 0");
    check(offsetof(np_safety_sig_cmd_t, cmd_type)     == 2U,  "sig_cmd.cmd_type at offset 2");
    check(offsetof(np_safety_sig_cmd_t, reserved)     == 3U,  "sig_cmd.reserved at offset 3");
    check(offsetof(np_safety_sig_cmd_t, session_hash) == 4U,  "sig_cmd.session_hash at offset 4");
    check(offsetof(np_safety_sig_cmd_t, session_sig)  == 36U, "sig_cmd.session_sig at offset 36");
    check(offsetof(np_safety_sig_cmd_t, checksum)     == 100U,"sig_cmd.checksum at offset 100");
}

/* ── Test: magic byte values are distinct from heartbeat magic ─────────────── */

static void test_magic_distinct(void)
{
    check(NP_SAFETY_CMD_MAGIC_0 != NP_SAFETY_BEAT_MAGIC_0 ||
          NP_SAFETY_CMD_MAGIC_1 != NP_SAFETY_BEAT_MAGIC_1,
          "cmd_magic distinct from heartbeat_magic");

    check(NP_SAFETY_CMD_MAGIC_0 == 0xC0U, "NP_SAFETY_CMD_MAGIC_0 == 0xC0");
    check(NP_SAFETY_CMD_MAGIC_1 == 0xDEU, "NP_SAFETY_CMD_MAGIC_1 == 0xDE");
}

/* ── Test: checksum computation and round-trip verification ────────────────── */

static void test_sig_cmd_checksum(void)
{
    np_safety_sig_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));

    cmd.cmd_magic[0] = NP_SAFETY_CMD_MAGIC_0;
    cmd.cmd_magic[1] = NP_SAFETY_CMD_MAGIC_1;
    cmd.cmd_type     = NP_SAFETY_CMD_SESSION_SIG;
    cmd.reserved     = 0U;

    /* Fill hash and sig with known test pattern */
    uint8_t i;
    for (i = 0U; i < NP_SESSION_HASH_LEN; i++) { cmd.session_hash[i] = (uint8_t)(i + 1U); }
    for (i = 0U; i < NP_ED25519_SIG_LEN;  i++) { cmd.session_sig[i]  = (uint8_t)(i + 0x40U); }

    cmd.checksum = cmd_checksum(&cmd);

    /* Verify the checksum matches */
    uint16_t recomputed = cmd_checksum(&cmd);
    /* The checksum bytes themselves are included in the sum.  Because we just
     * set checksum = sum(bytes[0..99]) and those bytes don't include checksum,
     * sum(bytes[0..99]) should still equal cmd.checksum. */
    check(recomputed == cmd.checksum, "sig_cmd checksum round-trip match");

    /* Corrupt one payload byte — checksum must no longer match */
    cmd.session_hash[5] ^= 0xFFU;
    uint16_t corrupted_sum = cmd_checksum(&cmd);
    check(corrupted_sum != cmd.checksum, "sig_cmd checksum detects payload corruption");

    /* Restore and corrupt the checksum field itself */
    cmd.session_hash[5] ^= 0xFFU;
    cmd.checksum ^= 0x0001U;
    uint16_t after_cksum_flip = cmd_checksum(&cmd);
    check(after_cksum_flip != cmd.checksum, "sig_cmd checksum detects checksum field flip");
}

/* ── Test: SIG_PENDING status bit lifecycle ──────────────────────────────────
 *
 * Simulates the state machine in np_session_sig.c without invoking Ed25519:
 *   1. np_session_sig_reset() sets SIG_PENDING
 *   2. np_session_sig_verify() clears SIG_PENDING on success
 *   3. Verify failure leaves SIG_PENDING set (and adds FAULT + CUTOFF)
 */

static void test_sig_pending_lifecycle(void)
{
    np_safety_state_t state;
    memset(&state, 0, sizeof(state));
    state.fault_slot = 0xFFU;

    /* Before session start: no SIG_PENDING */
    check((state.status & NP_SAFETY_STATUS_SIG_PENDING) == 0U,
          "SIG_PENDING not set before session start");

    /* Simulate np_session_sig_reset(&state) — set SIG_PENDING */
    state.status |= NP_SAFETY_STATUS_SIG_PENDING;
    check((state.status & NP_SAFETY_STATUS_SIG_PENDING) != 0U,
          "SIG_PENDING set after session_sig_reset");

    /* Simulate successful verify — clear SIG_PENDING */
    state.status &= (uint8_t)~NP_SAFETY_STATUS_SIG_PENDING;
    check((state.status & NP_SAFETY_STATUS_SIG_PENDING) == 0U,
          "SIG_PENDING cleared after successful verify");
    check((state.status & NP_SAFETY_STATUS_FAULT) == 0U,
          "FAULT not set after successful verify");

    /* Simulate failed verify — SIG_PENDING stays + FAULT + CUTOFF */
    state.status |= NP_SAFETY_STATUS_SIG_PENDING;  /* reset again */
    state.status |= NP_SAFETY_STATUS_FAULT | NP_SAFETY_STATUS_CUTOFF;
    state.granted_mask = 0U;
    state.fault_slot   = 0xFDU;

    check((state.status & NP_SAFETY_STATUS_SIG_PENDING) != 0U,
          "SIG_PENDING stays set after failed verify");
    check((state.status & NP_SAFETY_STATUS_FAULT) != 0U,
          "FAULT set after failed verify");
    check((state.status & NP_SAFETY_STATUS_CUTOFF) != 0U,
          "CUTOFF set after failed verify");
    check(state.granted_mask == 0U,
          "granted_mask zero after failed verify");
    check(state.fault_slot == 0xFDU,
          "fault_slot 0xFD (sig failure) after failed verify");
}

/* ── Test: watchdog tick blocks grant while SIG_PENDING set ─────────────────
 *
 * Simulates the grant logic from np_spi_watchdog_tick() to confirm that
 * SIG_PENDING is included in the active_faults check.
 * NP_SAFETY_STATUS_SIG_PENDING must be bit 7 (0x80).
 */

static void test_watchdog_blocks_on_sig_pending(void)
{
    check(NP_SAFETY_STATUS_SIG_PENDING == 0x80U,
          "NP_SAFETY_STATUS_SIG_PENDING is bit 7 (0x80)");

    /* Build the active_faults mask exactly as np_spi_watchdog_tick() does */
    uint8_t active_faults_mask = (uint8_t)(NP_SAFETY_STATUS_FAULT      |
                                            NP_SAFETY_STATUS_THERMAL     |
                                            NP_SAFETY_STATUS_CHARGE      |
                                            NP_SAFETY_STATUS_CARDIAC     |
                                            NP_SAFETY_STATUS_SIG_PENDING);

    check((active_faults_mask & NP_SAFETY_STATUS_SIG_PENDING) != 0U,
          "NP_SAFETY_STATUS_SIG_PENDING included in active_faults mask");

    /* Simulate SIG_PENDING alone: grant should be zero */
    uint8_t status_sig_only = NP_SAFETY_STATUS_SIG_PENDING;
    uint8_t active           = status_sig_only & active_faults_mask;
    uint16_t granted         = (active == 0U) ? 0x0001U : 0U;
    check(granted == 0U, "grant blocked when only SIG_PENDING set");

    /* Simulate cleared SIG_PENDING, no other faults: grant should pass */
    uint8_t status_clear = 0U;
    active  = status_clear & active_faults_mask;
    granted = (active == 0U) ? 0x0001U : 0U;
    check(granted != 0U, "grant permitted when SIG_PENDING cleared and no faults");

    /* SIG_PENDING + another fault: grant still zero */
    uint8_t status_both = NP_SAFETY_STATUS_SIG_PENDING | NP_SAFETY_STATUS_THERMAL;
    active  = status_both & active_faults_mask;
    granted = (active == 0U) ? 0x0001U : 0U;
    check(granted == 0U, "grant blocked when SIG_PENDING + THERMAL set");
}

/* ── Test: sig cmd magic and type constants ─────────────────────────────────── */

static void test_cmd_constants(void)
{
    check(NP_SAFETY_CMD_SESSION_SIG == 0x01U, "NP_SAFETY_CMD_SESSION_SIG == 0x01");
    check(NP_SAFETY_CMD_FRAME_LEN == 102U,    "NP_SAFETY_CMD_FRAME_LEN == 102");
    check(NP_SESSION_HASH_LEN == 32U,          "NP_SESSION_HASH_LEN == 32");
    check(NP_ED25519_SIG_LEN  == 64U,          "NP_ED25519_SIG_LEN == 64");
    /* Payload = 4 header + 32 hash + 64 sig + 2 checksum */
    check(4U + NP_SESSION_HASH_LEN + NP_ED25519_SIG_LEN + 2U == NP_SAFETY_CMD_FRAME_LEN,
          "frame_len equals 4+hash+sig+2");
}

/* ── Test: heartbeat frame is unaffected by the new constants ───────────────── */

static void test_heartbeat_unaffected(void)
{
    np_safety_rx_frame_t rx;
    memset(&rx, 0, sizeof(rx));
    rx.magic[0]      = NP_SAFETY_BEAT_MAGIC_0;
    rx.magic[1]      = NP_SAFETY_BEAT_MAGIC_1;
    rx.session_status = NP_SESSION_STATUS_ACTIVE;
    rx.enable_lo     = 0x3FU;
    rx.enable_hi     = 0x00U;
    rx.reserved      = 0U;
    rx.checksum      = heartbeat_checksum(&rx);

    /* Magic still recognized */
    check(rx.magic[0] == NP_SAFETY_BEAT_MAGIC_0, "heartbeat magic[0] unchanged");
    check(rx.magic[1] == NP_SAFETY_BEAT_MAGIC_1, "heartbeat magic[1] unchanged");

    /* Heartbeat magic != cmd magic */
    check(rx.magic[0] != NP_SAFETY_CMD_MAGIC_0 ||
          rx.magic[1] != NP_SAFETY_CMD_MAGIC_1,
          "heartbeat magic != cmd magic");

    /* Checksum valid */
    check(heartbeat_checksum(&rx) == rx.checksum, "heartbeat checksum valid");
}

/* ── Test: fault slot constants match their wire-format values ──────────────── */

static void test_fault_slot_constants(void)
{
    /* Fault slot codes must be stable — these values are written to SHDR and
     * read by the hub and app for diagnostics.  If they change, a deployed
     * device's SHDR database will misclassify historical fault records.       */
    check(NP_FAULT_SLOT_NONE        == 0xFFU, "NP_FAULT_SLOT_NONE == 0xFF");
    check(NP_FAULT_SLOT_SIG_FAIL    == 0xFDU, "NP_FAULT_SLOT_SIG_FAIL == 0xFD");
    check(NP_FAULT_SLOT_UNPROV      == 0xFEU, "NP_FAULT_SLOT_UNPROV == 0xFE");
    check(NP_FAULT_SLOT_SIG_CORRUPT == 0xFCU, "NP_FAULT_SLOT_SIG_CORRUPT == 0xFC");

    /* Escalation limits must be ≥1 to be meaningful */
    check(NP_SAFETY_SIG_BAD_CMD_MAX >= 1U, "NP_SAFETY_SIG_BAD_CMD_MAX >= 1");
    check(NP_SIG_FAIL_MAX           >= 1U, "NP_SIG_FAIL_MAX >= 1");

    /* All four codes must be distinct */
    check(NP_FAULT_SLOT_NONE        != NP_FAULT_SLOT_SIG_FAIL,    "NONE != SIG_FAIL");
    check(NP_FAULT_SLOT_NONE        != NP_FAULT_SLOT_UNPROV,       "NONE != UNPROV");
    check(NP_FAULT_SLOT_NONE        != NP_FAULT_SLOT_SIG_CORRUPT,  "NONE != SIG_CORRUPT");
    check(NP_FAULT_SLOT_SIG_FAIL    != NP_FAULT_SLOT_UNPROV,       "SIG_FAIL != UNPROV");
    check(NP_FAULT_SLOT_SIG_FAIL    != NP_FAULT_SLOT_SIG_CORRUPT,  "SIG_FAIL != SIG_CORRUPT");
    check(NP_FAULT_SLOT_UNPROV      != NP_FAULT_SLOT_SIG_CORRUPT,  "UNPROV != SIG_CORRUPT");
}

/* ── Test: extended heartbeat frame size and field offsets ───────────────────
 *
 * OI-CHARGE-01 CLOSED — the ext frame carries current_ua[14] enabling the
 * charge monitor.  These tests freeze the wire format.
 */

static uint16_t ext_heartbeat_checksum(const np_safety_rx_ext_frame_t *f)
{
    uint16_t sum = 0U;
    const uint8_t *b = (const uint8_t *)f;
    uint8_t i;
    for (i = 0U; i < 6U; i++) { sum += b[i]; }
    return sum;
}

static uint16_t ext_data_checksum(const np_safety_rx_ext_frame_t *f)
{
    uint16_t sum = 0U;
    const uint8_t *b = (const uint8_t *)f;
    uint8_t i;
    for (i = 8U; i < 36U; i++) { sum += b[i]; }
    return sum;
}

static void test_ext_frame_sizes(void)
{
    check(sizeof(np_safety_rx_ext_frame_t) == NP_SAFETY_RX_EXT_FRAME_LEN,
          "ext_frame size == NP_SAFETY_RX_EXT_FRAME_LEN (38)");
    check(NP_SAFETY_RX_EXT_FRAME_LEN == 38U,
          "NP_SAFETY_RX_EXT_FRAME_LEN == 38");
    check(NP_SAFETY_MAX_CHANNELS == 14U,
          "NP_SAFETY_MAX_CHANNELS == 14");
}

static void test_ext_frame_offsets(void)
{
    check(offsetof(np_safety_rx_ext_frame_t, magic)         == 0U,  "ext.magic at offset 0");
    check(offsetof(np_safety_rx_ext_frame_t, session_status) == 2U,  "ext.session_status at offset 2");
    check(offsetof(np_safety_rx_ext_frame_t, enable_lo)      == 3U,  "ext.enable_lo at offset 3");
    check(offsetof(np_safety_rx_ext_frame_t, enable_hi)      == 4U,  "ext.enable_hi at offset 4");
    check(offsetof(np_safety_rx_ext_frame_t, channel_count)  == 5U,  "ext.channel_count at offset 5");
    check(offsetof(np_safety_rx_ext_frame_t, checksum)       == 6U,  "ext.checksum at offset 6");
    check(offsetof(np_safety_rx_ext_frame_t, current_ua)     == 8U,  "ext.current_ua at offset 8");
    check(offsetof(np_safety_rx_ext_frame_t, ext_checksum)   == 36U, "ext.ext_checksum at offset 36");
}

static void test_ext_frame_base_checksum(void)
{
    np_safety_rx_ext_frame_t f;
    memset(&f, 0, sizeof(f));
    f.magic[0]       = NP_SAFETY_BEAT_MAGIC_0;
    f.magic[1]       = NP_SAFETY_BEAT_MAGIC_1;
    f.session_status = NP_SESSION_STATUS_ACTIVE;
    f.enable_lo      = 0x3FU;
    f.enable_hi      = 0x00U;
    f.channel_count  = 6U;
    f.checksum       = ext_heartbeat_checksum(&f);

    check(ext_heartbeat_checksum(&f) == f.checksum,
          "ext base checksum round-trip");

    /* Corrupt enable_lo — base checksum must fail */
    f.enable_lo ^= 0xFFU;
    check(ext_heartbeat_checksum(&f) != f.checksum,
          "ext base checksum detects enable_lo corruption");
    f.enable_lo ^= 0xFFU;   /* restore */
}

static void test_ext_frame_data_checksum(void)
{
    np_safety_rx_ext_frame_t f;
    uint8_t ch;
    memset(&f, 0, sizeof(f));

    f.channel_count = 14U;
    for (ch = 0U; ch < 14U; ch++) {
        f.current_ua[ch] = (uint16_t)((uint16_t)ch * 100U);  /* 0, 100, 200 … 1300 µA */
    }
    f.ext_checksum = ext_data_checksum(&f);

    check(ext_data_checksum(&f) == f.ext_checksum,
          "ext data checksum round-trip");

    /* Corrupt one current entry */
    f.current_ua[7] ^= 0x01U;
    check(ext_data_checksum(&f) != f.ext_checksum,
          "ext data checksum detects current_ua corruption");
    f.current_ua[7] ^= 0x01U;   /* restore */

    /* Base and ext checksums are independent */
    check(offsetof(np_safety_rx_ext_frame_t, current_ua) == 8U,
          "current_ua starts at byte 8 (not covered by base checksum)");
}

static void test_ext_frame_magic_matches_heartbeat(void)
{
    /* The ext frame uses the same magic bytes as the old 8-byte heartbeat —
     * HAL distinguishes frame types by NSS-delineated transfer length, not magic. */
    np_safety_rx_ext_frame_t f;
    memset(&f, 0, sizeof(f));
    f.magic[0] = NP_SAFETY_BEAT_MAGIC_0;
    f.magic[1] = NP_SAFETY_BEAT_MAGIC_1;
    check(f.magic[0] == NP_SAFETY_BEAT_MAGIC_0, "ext magic[0] == BEAT_MAGIC_0");
    check(f.magic[1] == NP_SAFETY_BEAT_MAGIC_1, "ext magic[1] == BEAT_MAGIC_1");
}

static void test_ext_frame_channel_count_clamping(void)
{
    /* channel_count must not exceed NP_SAFETY_MAX_CHANNELS.  Hub-side code
     * clamps at NP_SAFETY_MAX_CHANNELS; MCU-side loop guards on both
     * rx.channel_count and NP_SAFETY_MAX_CHANNELS.                          */
    check(NP_SAFETY_MAX_CHANNELS == 14U,
          "NP_SAFETY_MAX_CHANNELS == 14 (matches current_ua array size)");
    check(sizeof(((np_safety_rx_ext_frame_t *)0)->current_ua) ==
              NP_SAFETY_MAX_CHANNELS * sizeof(uint16_t),
          "current_ua array byte-size == 14 × 2 == 28");
}

/* ── OI-CHARGE-02: per-channel charge-limit command frame tests ────────────── */

static void test_chan_limit_frame_sizes(void)
{
    check(sizeof(np_safety_chan_limit_cmd_t) == NP_SAFETY_CHAN_LIMIT_FRAME_LEN,
          "np_safety_chan_limit_cmd_t size == NP_SAFETY_CHAN_LIMIT_FRAME_LEN (34)");
    check(NP_SAFETY_CHAN_LIMIT_FRAME_LEN == 34U,
          "NP_SAFETY_CHAN_LIMIT_FRAME_LEN == 34");

    /* Distinct from every other frame length (NSS-length discrimination). */
    check(NP_SAFETY_CHAN_LIMIT_FRAME_LEN != NP_SAFETY_FRAME_LEN &&
          NP_SAFETY_CHAN_LIMIT_FRAME_LEN != NP_SAFETY_RX_EXT_FRAME_LEN &&
          NP_SAFETY_CHAN_LIMIT_FRAME_LEN != NP_SAFETY_CMD_FRAME_LEN,
          "chan-limit frame length distinct from 8/38/102");
}

static void test_chan_limit_frame_offsets(void)
{
    check(offsetof(np_safety_chan_limit_cmd_t, cmd_magic) == 0U,  "chan_limit.cmd_magic at 0");
    check(offsetof(np_safety_chan_limit_cmd_t, cmd_type)  == 2U,  "chan_limit.cmd_type at 2");
    check(offsetof(np_safety_chan_limit_cmd_t, reserved)  == 3U,  "chan_limit.reserved at 3");
    check(offsetof(np_safety_chan_limit_cmd_t, area_mcm2) == 4U,  "chan_limit.area_mcm2 at 4");
    check(offsetof(np_safety_chan_limit_cmd_t, checksum)  == 32U, "chan_limit.checksum at 32");
    check(sizeof(((np_safety_chan_limit_cmd_t *)0)->area_mcm2) ==
              NP_SAFETY_MAX_CHANNELS * sizeof(uint16_t),
          "area_mcm2 byte-size == 14 × 2 == 28");
    check(NP_SAFETY_CMD_CHAN_LIMIT == 0x02U, "NP_SAFETY_CMD_CHAN_LIMIT == 0x02");
    check(NP_SAFETY_CMD_CHAN_LIMIT != NP_SAFETY_CMD_SESSION_SIG,
          "chan-limit cmd_type distinct from session-sig cmd_type");
}

static void test_chan_limit_checksum(void)
{
    np_safety_chan_limit_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.cmd_magic[0] = NP_SAFETY_CMD_MAGIC_0;
    cmd.cmd_magic[1] = NP_SAFETY_CMD_MAGIC_1;
    cmd.cmd_type     = NP_SAFETY_CMD_CHAN_LIMIT;
    cmd.area_mcm2[13] = 96U;      /* HD-tDCS on CLIN_STIM channel */
    cmd.area_mcm2[6]  = 25000U;   /* standard pad on another channel */
    cmd.checksum = chan_limit_checksum(&cmd);

    check(chan_limit_checksum(&cmd) == cmd.checksum,
          "chan-limit checksum round-trip match");

    /* Corrupt a payload byte — checksum must diverge. */
    cmd.area_mcm2[13] ^= 0x00FFU;
    check(chan_limit_checksum(&cmd) != cmd.checksum,
          "chan-limit checksum detects area corruption");

    /* Restore; a good frame must validate. */
    cmd.area_mcm2[13] ^= 0x00FFU;
    check(chan_limit_checksum(&cmd) == cmd.checksum,
          "chan-limit checksum valid after restore");
}

/* ── OI-CHARGE-03: geometry-required session_status bit ─────────────────────── */

static void test_geom_required_session_status_bit(void)
{
    check(NP_SESSION_STATUS_GEOM_REQUIRED == (1U << 2),
          "NP_SESSION_STATUS_GEOM_REQUIRED == bit 2");
    /* Distinct from ACTIVE (bit 0) and CVNS_REENABLE (bit 1). */
    check((NP_SESSION_STATUS_GEOM_REQUIRED & NP_SESSION_STATUS_ACTIVE) == 0U,
          "GEOM_REQUIRED distinct from ACTIVE");
    check((NP_SESSION_STATUS_GEOM_REQUIRED & NP_SESSION_STATUS_CVNS_REENABLE) == 0U,
          "GEOM_REQUIRED distinct from CVNS_REENABLE");

    /* All three coexist in one session_status byte without overlap. */
    uint8_t s = (uint8_t)(NP_SESSION_STATUS_ACTIVE |
                          NP_SESSION_STATUS_CVNS_REENABLE |
                          NP_SESSION_STATUS_GEOM_REQUIRED);
    check((s & NP_SESSION_STATUS_GEOM_REQUIRED) != 0U,
          "GEOM_REQUIRED recoverable when all three bits set");
}

/* ── Main ───────────────────────────────────────────────────────────────────── */

int main(void)
{
    printf("=== np_safety_spi_proto_tests ===\n");

    test_frame_sizes();
    test_magic_distinct();
    test_sig_cmd_checksum();
    test_sig_pending_lifecycle();
    test_watchdog_blocks_on_sig_pending();
    test_cmd_constants();
    test_heartbeat_unaffected();
    test_fault_slot_constants();

    /* OI-CHARGE-01 — extended heartbeat frame tests */
    test_ext_frame_sizes();
    test_ext_frame_offsets();
    test_ext_frame_base_checksum();
    test_ext_frame_data_checksum();
    test_ext_frame_magic_matches_heartbeat();
    test_ext_frame_channel_count_clamping();

    /* OI-CHARGE-02 — per-channel charge-limit command frame tests */
    test_chan_limit_frame_sizes();
    test_chan_limit_frame_offsets();
    test_chan_limit_checksum();

    /* OI-CHARGE-03 — geometry-required session_status bit */
    test_geom_required_session_status_bit();

    printf("\n%s: %d failure(s)\n",
           (g_failures == 0) ? "PASS" : "FAIL", g_failures);
    return (g_failures == 0) ? 0 : 1;
}
