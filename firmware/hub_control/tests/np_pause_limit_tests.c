/*
 * Host tests — the owner's pause time limit (SWR-SS-05).
 *
 * The Config store is a one-record stub.  What is tested: the default is no limit, a write is read back, the record is
 * persisted from the poll and never from the write, a limit survives a restart, and a stored record the hub cannot read
 * is never overwritten with the default.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "np_pause_limit.h"
#include "np_cfg_store.h"

static int g_failures = 0;
static void check(int cond, const char *name)
{
    if (cond) { printf("PASS: %s\n", name); }
    else      { printf("FAIL: %s\n", name); g_failures++; }
}

void np_transport_hal_enter_critical(void) {}
void np_transport_hal_exit_critical(void) {}

static uint8_t         g_rec[5];
static bool            g_present;
static np_hub_status_t g_read_rc;    /* forced read outcome, NP_HUB_OK = natural */
static int             g_writes;

np_hub_status_t np_cfg_store_replicated_read(np_cfg_file_t file, uint8_t *payload, size_t len)
{
    if (file != NP_CFG_FILE_PAUSE_LIMIT || len != sizeof g_rec) { return NP_HUB_ERR_INVALID_ARG; }
    if (g_read_rc != NP_HUB_OK) { return g_read_rc; }
    if (!g_present) { return NP_HUB_ERR_NOT_PRESENT; }
    memcpy(payload, g_rec, len);
    return NP_HUB_OK;
}

np_hub_status_t np_cfg_store_replicated_write(np_cfg_file_t file, const uint8_t *payload, size_t len)
{
    g_writes++;
    if (file != NP_CFG_FILE_PAUSE_LIMIT || len != sizeof g_rec) { return NP_HUB_ERR_INVALID_ARG; }
    memcpy(g_rec, payload, len);
    g_present = true;
    return NP_HUB_OK;
}

static void reset_store(void) { memset(g_rec, 0, sizeof g_rec); g_present = false; g_read_rc = NP_HUB_OK; g_writes = 0; }

static uint32_t wire_read(void)
{
    uint8_t b[4]; size_t n = 0;
    if (np_pause_limit_read(b, sizeof b, &n) != NP_HUB_OK || n != 4U) { return 0xFFFFFFFFU; }
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static np_hub_status_t wire_write(uint32_t v)
{
    const uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) };
    return np_pause_limit_write(b, sizeof b);
}

/* @verifies SWR-SS-05 */
static void test_default_is_no_limit(void)
{
    reset_store();
    np_pause_limit_init();
    check(np_pause_limit_seconds() == NP_PAUSE_LIMIT_NONE && wire_read() == 0U,
          "pause limit: with nothing stored the limit is none");
    np_pause_limit_poll();
    check(g_writes == 0, "pause limit: the default is not written to the store");
}

/* @verifies SWR-SS-05 */
static void test_write_is_read_back_and_persisted_by_poll(void)
{
    reset_store();
    np_pause_limit_init();
    check(wire_write(900U) == NP_HUB_OK && wire_read() == 900U && np_pause_limit_seconds() == 900U,
          "pause limit: a write is read back");
    check(g_writes == 0, "pause limit: the write itself does not touch the store");
    np_pause_limit_poll();
    check(g_writes == 1, "pause limit: the poll persists it");
    np_pause_limit_poll();
    check(g_writes == 1, "pause limit: and only once");

    np_pause_limit_init();   /* a restart */
    check(np_pause_limit_seconds() == 900U, "pause limit: the limit survives a restart");

    check(wire_write(0U) == NP_HUB_OK && np_pause_limit_seconds() == 0U, "pause limit: writing 0 returns to no limit");
    np_pause_limit_poll();
    np_pause_limit_init();
    check(np_pause_limit_seconds() == 0U, "pause limit: no limit survives a restart too");
}

/* @verifies SWR-SS-05 */
static void test_bad_writes_are_refused(void)
{
    reset_store();
    np_pause_limit_init();
    const uint8_t b[3] = {1, 2, 3};
    check(np_pause_limit_write(b, sizeof b) == NP_HUB_ERR_INVALID_ARG && np_pause_limit_write(NULL, 4) == NP_HUB_ERR_INVALID_ARG,
          "pause limit: a write of the wrong length is refused");
    check(np_pause_limit_seconds() == 0U, "pause limit: and changes nothing");
}

/* @verifies SWR-SS-05 */
static void test_unreadable_record_is_never_overwritten_by_the_default(void)
{
    reset_store();
    g_present = true; g_rec[0] = 0x01; g_rec[1] = 60;
    g_read_rc = NP_HUB_ERR_GENERIC;
    np_pause_limit_init();
    check(np_pause_limit_seconds() == 0U, "pause limit: an unreadable record leaves the default in force");
    np_pause_limit_poll();
    check(g_writes == 0, "pause limit: nothing is written over an unreadable record");
    g_read_rc = NP_HUB_OK;
    np_pause_limit_poll();
    check(np_pause_limit_seconds() == 60U, "pause limit: once it reads, the stored limit applies");
}

int main(void)
{
    test_default_is_no_limit();
    test_write_is_read_back_and_persisted_by_poll();
    test_bad_writes_are_refused();
    test_unreadable_record_is_never_overwritten_by_the_default();
    if (g_failures != 0) { printf("\n%d FAILURE(S)\n", g_failures); return 1; }
    printf("\nAll np_pause_limit tests passed.\n");
    return 0;
}
