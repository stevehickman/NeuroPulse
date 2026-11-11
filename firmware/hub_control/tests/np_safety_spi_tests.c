/*
 * NeurOne Hub Control — Safety SPI heartbeat host tests
 * Document: NP-FW-HUB-001 Rev 23 §7.2, §7.3 (REQ-FWHUB-11, REQ-FWHUB-17, REQ-FWHUB-18)
 *
 * Links the real np_safety_spi.c over a recording double for the SPI transfer.
 * The kernel is not linked: the two critical-section calls are no-ops here,
 * because every caller in these tests is single-threaded.
 *
 * What the tests hold the unit to:
 *   - session_status is a bit-flag byte built from the session state, never the
 *     state enum itself (REQ-FWHUB-11);
 *   - the heartbeat transmits the mask the session runner REQUESTED, and the
 *     mask the safety MCU granted never feeds back into it (REQ-FWHUB-17);
 *   - the session-signature command is its own frame, carries the hash and the
 *     signature, and requests nothing: only a heartbeat reply can say the
 *     signature was accepted (REQ-FWHUB-18, step 2 of §7.3).
 *
 * Not tested here: the runner's rule that no non-zero enable is requested
 * before SIG_PENDING clears. np_session_runner.c links into no host test.
 *
 * No FreeRTOS, no hardware.  IEC 62304 Class B — SW-02 hub control.
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "np_hub_config.h"
#include "np_hub_types.h"
#include "np_safety_spi.h"

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

/* ── Doubles ─────────────────────────────────────────────────────────────── */

void vPortEnterCritical(void) {}
void vPortExitCritical(void) {}
UBaseType_t xPortSetInterruptMask(void) { return 0; }
void vPortClearInterruptMask(UBaseType_t m) { (void)m; }

static uint8_t g_tx[NP_SAFETY_CMD_FRAME_LEN];
static uint8_t g_tx_len;
static int     g_transfers;
static uint8_t g_reply_status;
static uint16_t g_reply_granted;
static bool     g_corrupt_reply;
/* The withhold report (OI-SIMFW-61) the MCU double puts in window bytes [27..33]:
 * 0 none (zero bytes), 1 valid with g_wh_decl / g_wh_geom, 2 valid but with a bad checksum. */
static int      g_withhold_mode;
static uint16_t g_wh_decl, g_wh_geom;

static uint16_t sum(const uint8_t *b, size_t n)
{
    uint16_t s = 0U;
    for (size_t i = 0U; i < n; i++) { s = (uint16_t)(s + b[i]); }
    return s;
}

np_hub_status_t np_safety_hal_spi_transfer(const uint8_t *tx, uint8_t *rx, uint8_t len)
{
    memcpy(g_tx, tx, len);
    g_tx_len = len;
    g_transfers++;
    memset(rx, 0, len);
    if (len == NP_SAFETY_RX_EXT_FRAME_LEN) {
        np_safety_rx_frame_t r;
        memset(&r, 0, sizeof(r));
        r.status     = g_reply_status;
        r.granted_lo = (uint8_t)(g_reply_granted & 0xFFU);
        r.granted_hi = (uint8_t)(g_reply_granted >> 8);
        r.fault_slot = 0xFFU;
        r.checksum   = (uint16_t)(sum((const uint8_t *)&r, 6U) + (g_corrupt_reply ? 1U : 0U));
        memcpy(rx, &r, sizeof(r));
        if (g_withhold_mode != 0) {
            np_safety_withhold_report_t w;
            w.magic     = NP_SAFETY_WITHHOLD_REPORT_MAGIC;
            w.decl_mask = g_wh_decl;
            w.geom_mask = g_wh_geom;
            w.checksum  = (uint16_t)((uint16_t)w.magic + (uint16_t)(w.decl_mask & 0xFFU) +
                                     (uint16_t)(w.decl_mask >> 8) + (uint16_t)(w.geom_mask & 0xFFU) +
                                     (uint16_t)(w.geom_mask >> 8) + (g_withhold_mode == 2 ? 1U : 0U));
            memcpy(rx + NP_SAFETY_WITHHOLD_REPORT_OFFSET, &w, sizeof(w));
        }
    }
    return NP_HUB_OK;
}

static void reset(void)
{
    (void)np_safety_spi_init();
    g_transfers = 0;
    g_reply_status = NP_SAFETY_STATUS_OK;
    g_reply_granted = 0U;
    g_corrupt_reply = false;
    g_withhold_mode = 0;
    g_wh_decl = 0U;
    g_wh_geom = 0U;
    memset(g_tx, 0, sizeof(g_tx));
}

static uint8_t status_of_last_beat(void)
{
    return ((const np_safety_tx_ext_frame_t *)g_tx)->session_status;
}

/* ── REQ-FWHUB-11 ────────────────────────────────────────────────────────── */

/* @verifies REQ-FWHUB-11 */
static void test_status_bits_are_flags_not_the_enum(void)
{
    const uint8_t flags = (uint8_t)(NP_SESSION_STATUS_CVNS_REENABLE |
                                    NP_SESSION_STATUS_GEOM_REQUIRED |
                                    NP_SESSION_STATUS_GEOM_REQ_TDCS |
                                    NP_SESSION_STATUS_GEOM_REQ_BES);
    /* RUNNING (=3) and PAUSED (=4) would read as flag bits if written raw. */
    check(np_safety_session_status_bits(NP_SESSION_RUNNING, false, false, false, false)
              == (uint8_t)NP_SESSION_STATUS_ACTIVE,
          "RUNNING sets ACTIVE and nothing else (not CVNS_REENABLE)");
    check(np_safety_session_status_bits(NP_SESSION_PAUSED, false, false, false, false)
              == (uint8_t)NP_SESSION_STATUS_ACTIVE,
          "PAUSED sets ACTIVE and nothing else (not GEOM_REQUIRED)");
    check(np_safety_session_status_bits(NP_SESSION_STOPPING, false, false, false, false)
              == (uint8_t)NP_SESSION_STATUS_ACTIVE,
          "STOPPING stays ACTIVE through ramp-down");
    check(np_safety_session_status_bits(NP_SESSION_IDLE, false, false, false, false) == 0U,
          "IDLE sets no bit");
    check(np_safety_session_status_bits(NP_SESSION_COMPLETE, false, false, false, false) == 0U,
          "COMPLETE sets no bit (its enum value has bit 3 set)");
    check(np_safety_session_status_bits(NP_SESSION_FAULT, false, false, false, false) == 0U,
          "FAULT drops ACTIVE");
    check(np_safety_session_status_bits(NP_SESSION_IDLE, true, true, true, true) == flags,
          "each flag argument sets exactly its own bit, with no session state");
}

/* @verifies REQ-FWHUB-11 */
static void test_status_byte_on_the_wire_is_the_builder_output(void)
{
    reset();
    np_safety_spi_set_geom_required_tdcs(true);
    (void)np_safety_spi_heartbeat(NP_SESSION_RUNNING, 0U, NULL, 0U);
    const uint8_t want_bits = np_safety_session_status_bits(
        NP_SESSION_RUNNING, false, false, true, false);
    check(status_of_last_beat() == np_safety_session_status_with_seq(want_bits, 0U),
          "the heartbeat's session_status is bits() then with_seq()");
    check((status_of_last_beat() & NP_SESSION_STATUS_SEQ_MASK) == 0U,
          "the first beat carries sequence 0");
    (void)np_safety_spi_heartbeat(NP_SESSION_RUNNING, 0U, NULL, 0U);
    check((status_of_last_beat() & ~NP_SESSION_STATUS_SEQ_MASK) == want_bits,
          "the flag bits are unchanged by the sequence counter");
}

/* ── REQ-FWHUB-17 ────────────────────────────────────────────────────────── */

/* @verifies REQ-FWHUB-17 */
static void test_heartbeat_sends_the_requested_mask(void)
{
    reset();
    (void)np_safety_spi_heartbeat(NP_SESSION_RUNNING, 0x0105U, NULL, 0U);
    const np_safety_tx_ext_frame_t *f = (const np_safety_tx_ext_frame_t *)g_tx;
    check(f->enable_lo == 0x05U && f->enable_hi == 0x01U,
          "the enable bytes are the mask passed to the heartbeat");
    check(f->checksum == sum(g_tx, 6U), "the base checksum covers bytes 0..5");
}

/* @verifies REQ-FWHUB-17 */
static void test_granted_mask_never_feeds_back_into_the_request(void)
{
    reset();
    g_reply_granted = 0x00FFU;   /* the MCU grants more than was asked for */
    (void)np_safety_spi_heartbeat(NP_SESSION_RUNNING, 0x0001U, NULL, 0U);
    check(np_safety_spi_get_granted_mask() == 0x00FFU, "the granted mask is read back");
    (void)np_safety_spi_heartbeat(NP_SESSION_RUNNING, 0x0001U, NULL, 0U);
    const np_safety_tx_ext_frame_t *f = (const np_safety_tx_ext_frame_t *)g_tx;
    check(f->enable_lo == 0x01U && f->enable_hi == 0U,
          "the next beat still transmits the requested mask, not the granted echo");

    g_reply_granted = 0U;        /* the MCU withdraws the grant */
    (void)np_safety_spi_heartbeat(NP_SESSION_RUNNING, 0x0001U, NULL, 0U);
    (void)np_safety_spi_heartbeat(NP_SESSION_RUNNING, 0x0001U, NULL, 0U);
    check(((const np_safety_tx_ext_frame_t *)g_tx)->enable_lo == 0x01U,
          "a withdrawn grant does not lower what is requested");
}

/* @verifies REQ-FWHUB-17 */
static void test_a_corrupt_reply_zeroes_the_grant_not_the_request(void)
{
    reset();
    np_safety_spi_request_enable(0x0002U);
    g_reply_granted = 0x0002U;
    check(np_safety_spi_heartbeat(NP_SESSION_RUNNING, 0x0002U, NULL, 0U) == NP_HUB_OK,
          "a good reply is accepted");
    check(np_safety_spi_get_granted_mask() == 0x0002U, "a good reply is granted");
    g_corrupt_reply = true;
    check(np_safety_spi_heartbeat(NP_SESSION_RUNNING, 0x0002U, NULL, 0U) == NP_HUB_ERR_SAFETY_FAULT,
          "a reply with a bad checksum is a safety fault");
    check(np_safety_spi_get_granted_mask() == 0U, "a bad reply drops the stale grant");
    check(np_safety_spi_get_requested_mask() == 0x0002U,
          "the requested mask is the runner's, whatever the MCU replies");
}

/* ── REQ-FWHUB-18 (step 2 of §7.3) ───────────────────────────────────────── */

/* @verifies REQ-FWHUB-18#a */
static void test_session_sig_is_its_own_frame_and_requests_nothing(void)
{
    uint8_t hash[NP_SESSION_HASH_LEN];
    uint8_t sig[NP_ED25519_SIG_SIZE];
    for (size_t i = 0U; i < sizeof(hash); i++) { hash[i] = (uint8_t)(0x10U + i); }
    for (size_t i = 0U; i < sizeof(sig); i++)  { sig[i]  = (uint8_t)(0x80U + i); }

    reset();
    check(np_safety_spi_send_session_sig(hash, sig) == NP_HUB_OK, "the command transfer succeeds");
    check(g_tx_len == NP_SAFETY_CMD_FRAME_LEN, "the command is the 102-byte frame, not a heartbeat");
    const np_safety_sig_cmd_t *c = (const np_safety_sig_cmd_t *)g_tx;
    check(c->cmd_magic[0] == NP_SAFETY_CMD_MAGIC_0 && c->cmd_magic[1] == NP_SAFETY_CMD_MAGIC_1,
          "the command magic is set");
    check(c->cmd_type == NP_SAFETY_CMD_SESSION_SIG, "the command type is SESSION_SIG");
    check(memcmp(c->session_hash, hash, sizeof(hash)) == 0, "the 32-byte hash is carried");
    check(memcmp(c->session_sig, sig, sizeof(sig)) == 0, "the 64-byte signature is carried");
    check(c->checksum == sum(g_tx, NP_SAFETY_CMD_FRAME_LEN - 2U),
          "the checksum covers everything before it");
    check(np_safety_spi_get_requested_mask() == 0U,
          "sending a signature requests no enable");
    check(np_safety_spi_get_granted_mask() == 0U,
          "sending a signature grants nothing: the verdict is the next heartbeat's");
}

/* @verifies REQ-FWHUB-17 */
static void test_fault_and_cutoff_replies_are_safety_faults(void)
{
    reset();
    g_reply_status = NP_SAFETY_STATUS_FAULT;
    check(np_safety_spi_heartbeat(NP_SESSION_RUNNING, 0U, NULL, 0U) == NP_HUB_ERR_SAFETY_FAULT,
          "a FAULT reply is reported as a safety fault");
    g_reply_status = NP_SAFETY_STATUS_CUTOFF;
    check(np_safety_spi_heartbeat(NP_SESSION_RUNNING, 0U, NULL, 0U) == NP_HUB_ERR_SAFETY_FAULT,
          "a CUTOFF reply is reported as a safety fault");
}

/* @verifies REQ-CVNS-14 */
static void test_the_hr_report_carries_the_cardiac_sensitivity_level_and_a_bad_level_is_sent_as_the_validated_cutoff(void)
{
    reset();
    check(np_safety_spi_send_hr_report(true, 700U, 40U, 2U) == NP_HUB_OK && g_tx_len == NP_SAFETY_HR_REPORT_FRAME_LEN,
          "sens: the report is the 12-byte heart-rate frame");
    const np_safety_hr_report_cmd_t *c = (const np_safety_hr_report_cmd_t *)g_tx;
    check(c->cmd_type == NP_SAFETY_CMD_HR_REPORT && c->cardiac_sens == 2U && c->hr_x10 == 700U && c->age_ms == 40U,
          "sens: the level is in the word that was reserved, beside the heart rate");
    check(c->checksum == sum(g_tx, NP_SAFETY_HR_REPORT_FRAME_LEN - 2U), "sens: and the checksum covers it");
    reset();
    (void)np_safety_spi_send_hr_report(false, 0U, 0U, 1U);
    c = (const np_safety_hr_report_cmd_t *)g_tx;
    check(c->cardiac_sens == 1U && (c->flags & NP_SAFETY_HR_FLAG_VALID) == 0U, "sens: the level is sent with no valid heart rate too");
    reset();
    (void)np_safety_spi_send_hr_report(true, 700U, 40U, 0U);
    check(((const np_safety_hr_report_cmd_t *)g_tx)->cardiac_sens == 0U, "sens: level 0 is the word an earlier hub sent, 0");
    reset();
    (void)np_safety_spi_send_hr_report(true, 700U, 40U, (uint8_t)(NP_SAFETY_CARDIAC_SENS_MAX + 1U));
    check(((const np_safety_hr_report_cmd_t *)g_tx)->cardiac_sens == 0U,
          "sens: a level above the maximum is sent as 0, the validated cutoff, never as a smaller threshold than was asked");
}

/* ── The withhold report (OI-SIMFW-61): parsed, validated, never stale ─────── */

/* @verifies REQ-FWHUB-82#b */
static void test_the_withhold_report_is_parsed_and_a_bad_one_is_refused(void)
{
    uint16_t decl = 0xAAAAU, geom = 0xAAAAU;

    reset();
    check(!np_safety_spi_get_withhold_report(&decl, &geom) && decl == 0U && geom == 0U,
          "withhold: before any beat there is no report and both masks read 0");

    g_withhold_mode = 1;
    g_wh_decl = 0x0140U;
    g_wh_geom = 0x2000U;
    (void)np_safety_spi_heartbeat(NP_SESSION_RUNNING, 0x2140U, NULL, 0U);
    check(np_safety_spi_get_withhold_report(&decl, &geom) && decl == 0x0140U && geom == 0x2000U,
          "withhold: a valid report's two masks reach the getter");

    g_wh_decl = 0U;
    g_wh_geom = 0U;
    (void)np_safety_spi_heartbeat(NP_SESSION_RUNNING, 0x2140U, NULL, 0U);
    check(np_safety_spi_get_withhold_report(&decl, &geom) && decl == 0U && geom == 0U,
          "withhold: a valid report of no withhold is valid, with both masks 0");

    g_withhold_mode = 2;
    g_wh_decl = 0x0140U;
    (void)np_safety_spi_heartbeat(NP_SESSION_RUNNING, 0x2140U, NULL, 0U);
    check(!np_safety_spi_get_withhold_report(&decl, &geom) && decl == 0U && geom == 0U,
          "withhold: a report whose checksum is wrong is refused and reads as no report");

    g_withhold_mode = 1;
    (void)np_safety_spi_heartbeat(NP_SESSION_RUNNING, 0x2140U, NULL, 0U);
    g_withhold_mode = 0;
    (void)np_safety_spi_heartbeat(NP_SESSION_RUNNING, 0x2140U, NULL, 0U);
    check(!np_safety_spi_get_withhold_report(&decl, &geom) && decl == 0U && geom == 0U,
          "withhold: a beat that carries no report drops the last one, so a stale cause is never shown");

    check(np_safety_spi_get_withhold_report(NULL, NULL) == false, "withhold: NULL outputs are accepted");
}

int main(void)
{
    printf("Hub safety SPI host tests (REQ-FWHUB-11, -17, -18)\n");
    test_status_bits_are_flags_not_the_enum();
    test_status_byte_on_the_wire_is_the_builder_output();
    test_heartbeat_sends_the_requested_mask();
    test_granted_mask_never_feeds_back_into_the_request();
    test_a_corrupt_reply_zeroes_the_grant_not_the_request();
    test_session_sig_is_its_own_frame_and_requests_nothing();
    test_fault_and_cutoff_replies_are_safety_faults();
    test_the_hr_report_carries_the_cardiac_sensitivity_level_and_a_bad_level_is_sent_as_the_validated_cutoff();
    test_the_withhold_report_is_parsed_and_a_bad_one_is_refused();
    printf("\n%s (%d failure%s)\n", g_failures ? "FAILED" : "PASSED",
           g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
