/*
 * NeurOne Bootloader — eMMC SANITIZE sequence, host tests (OI-PRIV-27, NP-PRIV-ANALYSIS-004 H7)
 * Document: NP-FW-EMMC-002 §B.3 (R-5..R-7a), §B.4
 *
 * np_emmc.c drives USDHC2 and cannot run on a host.  The sequence it runs is in
 * np_emmc_sanitize.h, and here it is driven against a recorder that plays the
 * card: what EXT_CSD says, whether CMD6 is accepted, and the R1 words CMD13
 * returns.  This checks the decisions, not the silicon: the SANITIZE time on the
 * selected part is bench work (FAI-RESET-01, OI-EMMC2-03).
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "np_emmc_sanitize.h"

static int g_fail;
#define CHECK(c, m) do { if (c) { printf("PASS: %s\n", m); } else { printf("FAIL: %s\n", m); g_fail++; } } while (0)

#define R1_TRAN_READY  ((NP_EMMC_R1_STATE_TRAN << NP_EMMC_R1_STATE_SHIFT) | NP_EMMC_R1_READY_FOR_DATA)
#define R1_PRG         (NP_EMMC_R1_STATE_PRG << NP_EMMC_R1_STATE_SHIFT)

typedef struct {
    uint8_t     sec_feature;        /* EXT_CSD[231]                        */
    np_status_t ext_csd_ret;
    np_status_t switch_ret;
    uint32_t    status_seq[8];      /* R1 words, the last one repeats      */
    np_status_t status_ret_seq[8];  /* per read; NP_OK when zero           */
    uint32_t    status_len;
    /* record */
    char        trace[64];          /* E ext_csd, W switch, S status       */
    uint32_t    n_switch;
    uint32_t    last_switch_arg;
    uint32_t    n_status;
    uint32_t    n_wait;
} card_t;

static void tr(card_t *c, char ch)
{
    size_t n = strlen(c->trace);
    if (n + 1U < sizeof(c->trace)) { c->trace[n] = ch; c->trace[n + 1U] = '\0'; }
}

static np_status_t f_ext_csd(void *ctx, uint8_t ext_csd[NP_EMMC_EXT_CSD_SIZE])
{
    card_t *c = ctx;
    tr(c, 'E');
    memset(ext_csd, 0, NP_EMMC_EXT_CSD_SIZE);
    ext_csd[NP_EMMC_EXTCSD_SEC_FEATURE_SUPPORT] = c->sec_feature;
    return c->ext_csd_ret;
}

static np_status_t f_switch(void *ctx, uint32_t arg)
{
    card_t *c = ctx;
    tr(c, 'W');
    c->n_switch++;
    c->last_switch_arg = arg;
    return c->switch_ret;
}

static np_status_t f_status(void *ctx, uint32_t *r1)
{
    card_t *c = ctx;
    uint32_t i = (c->n_status < c->status_len) ? c->n_status : c->status_len - 1U;
    if (c->n_status < 4U) tr(c, 'S');
    c->n_status++;
    *r1 = c->status_seq[i];
    return c->status_ret_seq[i];
}

static void f_wait(void *ctx) { ((card_t *)ctx)->n_wait++; }

static np_emmc_sanitize_ops_t ops_for(card_t *c)
{
    np_emmc_sanitize_ops_t o = { f_ext_csd, f_switch, f_status, f_wait, c };
    return o;
}

static card_t card_ok(void)
{
    card_t c;
    memset(&c, 0, sizeof c);
    c.sec_feature = NP_EMMC_SEC_SANITIZE;
    c.status_seq[0] = R1_PRG;
    c.status_seq[1] = R1_PRG;
    c.status_seq[2] = R1_TRAN_READY;
    c.status_len = 3U;
    return c;
}

/* @verifies REQ-SW02-13#a */
static void test_sanitize_is_cmd6_to_ext_csd_165(void)
{
    CHECK(np_emmc_switch_arg(165U, 1U) == 0x03A50100UL,
          "SANITIZE_START: CMD6 write-byte, index 165, value 1 = 0x03A50100");
    CHECK(np_emmc_switch_arg(179U, 0x48U) == 0x03B34800UL,
          "the same packing serves PARTITION_CONFIG (179) as np_emmc_switch_partition used");
    card_t c = card_ok();
    np_emmc_sanitize_ops_t o = ops_for(&c);
    CHECK(np_emmc_sanitize_run(&o, 10U) == NP_OK, "a card that goes PRG, PRG, TRAN completes");
    CHECK(c.n_switch == 1U && c.last_switch_arg == 0x03A50100UL,
          "exactly one CMD6, and it writes 1 to EXT_CSD[165]");
    CHECK(strcmp(c.trace, "EWSSS") == 0, "EXT_CSD read, then the switch, then status until TRAN");
    CHECK(c.n_wait == c.n_status, "every status read follows a poll wait");
}

/* @verifies REQ-SW02-13#a */
static void test_unsupported_part_fails_closed(void)
{
    card_t c = card_ok();
    c.sec_feature = (uint8_t)(0xFFU & ~NP_EMMC_SEC_SANITIZE);  /* every other bit */
    np_emmc_sanitize_ops_t o = ops_for(&c);
    CHECK(np_emmc_sanitize_run(&o, 10U) == NP_ERR_EMMC_SANITIZE_UNSUPPORTED,
          "SEC_SANITIZE (231 bit 6) clear: refused, even with the secure-erase bits set");
    CHECK(c.n_switch == 0U, "no command is sent to a part that does not advertise SANITIZE");

    card_t d = card_ok();
    d.ext_csd_ret = NP_ERR_EMMC_READ;
    o = ops_for(&d);
    CHECK(np_emmc_sanitize_run(&o, 10U) == NP_ERR_EMMC_READ && d.n_switch == 0U,
          "an unreadable EXT_CSD is an error, never taken as support");
}

/* @verifies REQ-SW02-13#c */
static void test_failures_are_errors_never_success(void)
{
    card_t c = card_ok();
    c.switch_ret = NP_ERR_TIMEOUT;
    np_emmc_sanitize_ops_t o = ops_for(&c);
    CHECK(np_emmc_sanitize_run(&o, 10U) == NP_ERR_TIMEOUT && c.n_status == 0U,
          "a CMD6 that fails is returned, and nothing is polled");

    c = card_ok();
    c.status_seq[1] = R1_TRAN_READY | NP_EMMC_R1_SWITCH_ERROR;
    o = ops_for(&c);
    CHECK(np_emmc_sanitize_run(&o, 10U) == NP_ERR_EMMC_WRITE, "SWITCH_ERROR in R1 is a failure");

    c = card_ok();
    c.status_seq[1] = R1_PRG | (1UL << 19);   /* ERROR */
    o = ops_for(&c);
    CHECK(np_emmc_sanitize_run(&o, 10U) == NP_ERR_EMMC_WRITE, "the general ERROR bit is a failure");

    c = card_ok();
    c.status_seq[0] = R1_PRG;
    c.status_len = 1U;   /* never leaves PRG */
    o = ops_for(&c);
    CHECK(np_emmc_sanitize_run(&o, 7U) == NP_ERR_TIMEOUT, "a SANITIZE that never finishes times out");
    CHECK(c.n_status == 7U, "the wait is bounded: exactly max_polls status reads");

    c = card_ok();
    c.status_seq[0] = (5UL << NP_EMMC_R1_STATE_SHIFT);   /* DATA: not a state SANITIZE leaves */
    c.status_len = 1U;
    o = ops_for(&c);
    CHECK(np_emmc_sanitize_run(&o, 10U) == NP_ERR_EMMC_WRITE, "an unexpected card state is a failure");

    c = card_ok();
    c.status_seq[0] = (NP_EMMC_R1_STATE_TRAN << NP_EMMC_R1_STATE_SHIFT);  /* TRAN, not ready */
    c.status_seq[1] = R1_TRAN_READY;
    c.status_len = 2U;
    o = ops_for(&c);
    CHECK(np_emmc_sanitize_run(&o, 10U) == NP_OK && c.n_status == 2U,
          "TRAN without READY_FOR_DATA is not yet done");

    c = card_ok();
    c.status_ret_seq[0] = NP_ERR_TIMEOUT;   /* busy on the bus: polled again */
    c.status_seq[0] = R1_TRAN_READY;        /* ignored: the read failed      */
    c.status_seq[1] = R1_PRG;
    c.status_seq[2] = R1_TRAN_READY;
    o = ops_for(&c);
    CHECK(np_emmc_sanitize_run(&o, 10U) == NP_OK && c.n_status == 3U,
          "a failed status read is counted and polled again, and its word is not trusted");

    CHECK(np_emmc_sanitize_run(NULL, 10U) == NP_ERR_INVALID_ARG, "no ops: refused");
    c = card_ok();
    o = ops_for(&c);
    CHECK(np_emmc_sanitize_run(&o, 0U) == NP_ERR_INVALID_ARG && c.n_switch == 0U,
          "a zero bound is refused before anything is sent");
}

/* @verifies REQ-SW02-13#c */
static void test_poll_bound_is_derived_from_the_timeout(void)
{
    CHECK(np_emmc_sanitize_max_polls(NP_EMMC_SANITIZE_TIMEOUT_MS, NP_EMMC_SANITIZE_POLL_MS) == 24000U,
          "240 s at 10 ms is 24000 polls");
    CHECK(np_emmc_sanitize_max_polls(25U, 10U) == 3U, "a remainder rounds up: the bound never runs short");
    CHECK(np_emmc_sanitize_max_polls(100U, 0U) == 0U, "a zero interval gives zero, which the run refuses");
}

int main(void)
{
    test_sanitize_is_cmd6_to_ext_csd_165();
    test_unsupported_part_fails_closed();
    test_failures_are_errors_never_success();
    test_poll_bound_is_derived_from_the_timeout();
    if (g_fail != 0) { printf("FAILED (%d)\n", g_fail); return 1; }
    printf("OK\n");
    return 0;
}
