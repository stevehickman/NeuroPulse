/*
 * Host tests — the cervical VNS cardiac cutoff's sensitivity, the hub's half (REQ-FWHUB-74).
 *
 * The Config store is a one-record stub. Tested: the default is level 0 (the validated cutoff), a write is read back with the highest
 * level, a level above the highest is refused, the record is persisted from the poll and never from the write, it survives a
 * restart, an unreadable or unknown record leaves the default and is not overwritten, and a write made before the load wins. The
 * cutoff itself and the rule that raising is immediate and lowering waits are the safety MCU's (np_cardiac_interlock_tests).
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "np_cardiac_sens.h"
#include "np_cfg_store.h"
#include "np_spi_wire_types.h"

static int g_failures = 0;
static void check(int cond, const char *name)
{
    if (cond) { printf("PASS: %s\n", name); }
    else      { printf("FAIL: %s\n", name); g_failures++; }
}

void np_transport_hal_enter_critical(void) {}
void np_transport_hal_exit_critical(void) {}

static uint8_t         g_rec[2];
static bool            g_present;
static np_hub_status_t g_read_rc;
static int             g_writes;

np_hub_status_t np_cfg_store_replicated_read(np_cfg_file_t file, uint8_t *payload, size_t len)
{
    if (file != NP_CFG_FILE_CARDIAC_SENS || len != sizeof g_rec) { return NP_HUB_ERR_INVALID_ARG; }
    if (g_read_rc != NP_HUB_OK) { return g_read_rc; }
    if (!g_present) { return NP_HUB_ERR_NOT_PRESENT; }
    memcpy(payload, g_rec, len);
    return NP_HUB_OK;
}

np_hub_status_t np_cfg_store_replicated_write(np_cfg_file_t file, const uint8_t *payload, size_t len)
{
    g_writes++;
    if (file != NP_CFG_FILE_CARDIAC_SENS || len != sizeof g_rec) { return NP_HUB_ERR_INVALID_ARG; }
    memcpy(g_rec, payload, len);
    g_present = true;
    return NP_HUB_OK;
}

static void reset_store(void) { memset(g_rec, 0, sizeof g_rec); g_present = false; g_read_rc = NP_HUB_OK; g_writes = 0; }
static np_hub_status_t wr(uint8_t v) { return np_cardiac_sens_write(&v, 1U); }

/* @verifies REQ-FWHUB-74 */
static void test_the_default_is_the_validated_cutoff_and_a_read_gives_the_range(void)
{
    reset_store();
    np_cardiac_sens_init();
    uint8_t b[2]; size_t n = 0;
    check(np_cardiac_sens_level() == 0U && np_cardiac_sens_read(b, 2U, &n) == NP_HUB_OK && n == 2U && b[0] == 0U && b[1] == NP_SAFETY_CARDIAC_SENS_MAX,
          "cardiac sens: the default is level 0, the validated cutoff, and a read gives the highest level the helmet offers");
    np_cardiac_sens_poll();
    check(g_writes == 0, "cardiac sens: the default is not written to the store");
    check(np_cardiac_sens_read(b, 1U, &n) == NP_HUB_ERR_INVALID_ARG && np_cardiac_sens_read(NULL, 2U, &n) == NP_HUB_ERR_INVALID_ARG,
          "cardiac sens: a read into too small a buffer is refused");
}

/* @verifies REQ-FWHUB-74 */
static void test_a_write_is_taken_a_level_above_the_highest_is_refused_and_there_is_none_below_zero(void)
{
    reset_store();
    np_cardiac_sens_init();
    check(wr(NP_SAFETY_CARDIAC_SENS_MAX) == NP_HUB_OK && np_cardiac_sens_level() == NP_SAFETY_CARDIAC_SENS_MAX, "cardiac sens: the highest level is taken");
    check(wr(NP_SAFETY_CARDIAC_SENS_MAX + 1U) == NP_HUB_ERR_INVALID_ARG && wr(0xFFU) == NP_HUB_ERR_INVALID_ARG,
          "cardiac sens: a level above the highest, or a 'level below 0', is refused");
    check(np_cardiac_sens_level() == NP_SAFETY_CARDIAC_SENS_MAX, "cardiac sens: and a refused write changes nothing");
    const uint8_t two[2] = { 1, 0 };
    check(np_cardiac_sens_write(two, 2U) == NP_HUB_ERR_INVALID_ARG && np_cardiac_sens_write(NULL, 1U) == NP_HUB_ERR_INVALID_ARG,
          "cardiac sens: a write of the wrong length is refused");
    check(wr(0U) == NP_HUB_OK && np_cardiac_sens_level() == 0U, "cardiac sens: the hub takes a lowering (the safety MCU decides when it applies)");
}

/* @verifies REQ-FWHUB-74 */
static void test_the_level_is_persisted_by_the_poll_and_survives_a_restart(void)
{
    reset_store();
    np_cardiac_sens_init();
    (void)wr(2U);
    check(g_writes == 0, "cardiac sens: the write does not touch the store");
    np_cardiac_sens_poll();
    check(g_writes == 1 && g_rec[0] == 0x01U && g_rec[1] == 2U, "cardiac sens: the poll persists it");
    np_cardiac_sens_poll();
    check(g_writes == 1, "cardiac sens: and only once");
    np_cardiac_sens_init();
    check(np_cardiac_sens_level() == 2U, "cardiac sens: and it survives a restart");
}

/* @verifies REQ-FWHUB-74 */
static void test_an_unreadable_or_unknown_record_leaves_the_validated_cutoff_and_is_not_overwritten(void)
{
    reset_store();
    g_present = true; g_rec[0] = 0x01; g_rec[1] = 2U;
    g_read_rc = NP_HUB_ERR_GENERIC;
    np_cardiac_sens_init();
    check(np_cardiac_sens_level() == 0U, "cardiac sens: an unreadable record leaves level 0");
    np_cardiac_sens_poll();
    check(g_writes == 0, "cardiac sens: nothing is written over an unreadable record");
    g_read_rc = NP_HUB_OK;
    np_cardiac_sens_poll();
    check(np_cardiac_sens_level() == 2U, "cardiac sens: once it can be read, it is loaded");

    reset_store();
    g_present = true; g_rec[0] = 0x09; g_rec[1] = 1U;
    np_cardiac_sens_init();
    check(np_cardiac_sens_level() == 0U, "cardiac sens: a record of another version is not believed");
    reset_store();
    g_present = true; g_rec[0] = 0x01; g_rec[1] = (uint8_t)(NP_SAFETY_CARDIAC_SENS_MAX + 1U);
    np_cardiac_sens_init();
    check(np_cardiac_sens_level() == 0U, "cardiac sens: a stored level the firmware does not offer is not believed");
    np_cardiac_sens_poll();
    check(g_writes == 0, "cardiac sens: and neither record is overwritten");
}

/* @verifies REQ-FWHUB-74 */
static void test_a_write_made_before_the_load_finished_wins(void)
{
    reset_store();
    g_present = true; g_rec[0] = 0x01; g_rec[1] = 2U;
    g_read_rc = NP_HUB_ERR_GENERIC;
    np_cardiac_sens_init();
    (void)wr(1U);
    g_read_rc = NP_HUB_OK;
    np_cardiac_sens_poll();
    check(np_cardiac_sens_level() == 1U, "cardiac sens: a write made before the load finished is not overwritten by the stored value");
}

int main(void)
{
    test_the_default_is_the_validated_cutoff_and_a_read_gives_the_range();
    test_a_write_is_taken_a_level_above_the_highest_is_refused_and_there_is_none_below_zero();
    test_the_level_is_persisted_by_the_poll_and_survives_a_restart();
    test_an_unreadable_or_unknown_record_leaves_the_validated_cutoff_and_is_not_overwritten();
    test_a_write_made_before_the_load_finished_wins();
    if (g_failures == 0) { printf("ALL PASS (0 failures)\n"); return 0; }
    printf("FAILED (%d failure%s)\n", g_failures, g_failures == 1 ? "" : "s");
    return 1;
}
