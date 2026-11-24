/*
 * NeurOne Bootloader — eMMC RPMB frame protocol (OI-PRIV-32, NP-PRIV-ANALYSIS-004)
 * Requirement: REQ-SW02-16 (NP-SW-001 §5.2.2). Layout, sources and contract:
 * np_emmc_rpmb.h.
 *
 * Pure apart from three seams: the transport (np_rpmb_ops_t), the MAC
 * (np_rpmb_mac.h) and the nonce source (np_entropy.h). Host-tested against a
 * fake RPMB device (tests/np_bootloader_rpmb_tests.c).
 */
#include "np_emmc_rpmb.h"
#include "np_entropy.h"
#include "np_rpmb_mac.h"

bool np_rpmb_ct_equal(const uint8_t *a, const uint8_t *b, size_t n)
{
    uint8_t diff = 0U;
    for (size_t i = 0U; i < n; i++) {
        diff |= (uint8_t)(a[i] ^ b[i]);
    }
    return diff == 0U;
}

void np_rpmb_wipe(void *p, size_t n)
{
    volatile uint8_t *v = (volatile uint8_t *)p;
    for (size_t i = 0U; i < n; i++) {
        v[i] = 0U;
    }
}

static void frame_clear(uint8_t f[NP_RPMB_FRAME_SIZE])
{
    for (uint32_t i = 0U; i < NP_RPMB_FRAME_SIZE; i++) {
        f[i] = 0U;
    }
}

static bool ops_ok(const np_rpmb_ops_t *ops, const uint8_t *key)
{
    return ops != NULL && ops->write_frame != NULL && ops->read_frame != NULL && key != NULL;
}

/* MAC of a frame, written into its key/MAC field. */
static np_status_t frame_sign(const uint8_t key[NP_RPMB_KEY_SIZE], uint8_t f[NP_RPMB_FRAME_SIZE])
{
    uint8_t mac[NP_RPMB_MAC_SIZE];
    if (np_rpmb_hmac_sha256(key, &f[NP_RPMB_OFF_DATA], NP_RPMB_MAC_SPAN, mac) != NP_OK) {
        return NP_ERR_ROLLBACK_STORE;
    }
    for (uint32_t i = 0U; i < NP_RPMB_MAC_SIZE; i++) {
        f[NP_RPMB_OFF_MAC + i] = mac[i];
    }
    return NP_OK;
}

/* The checks every response gets: its type, its result, its MAC. */
static np_status_t response_check(const uint8_t key[NP_RPMB_KEY_SIZE],
                                  const uint8_t f[NP_RPMB_FRAME_SIZE],
                                  uint16_t expected_resp)
{
    uint8_t mac[NP_RPMB_MAC_SIZE];

    if (np_rpmb_get16(f, NP_RPMB_OFF_REQ_RESP) != expected_resp) {
        return NP_ERR_ROLLBACK_STORE;
    }
    /* Any result but OK, the counter-expired flag included, is an error. */
    if (np_rpmb_get16(f, NP_RPMB_OFF_RESULT) != NP_RPMB_RESULT_OK) {
        return NP_ERR_ROLLBACK_STORE;
    }
    if (np_rpmb_hmac_sha256(key, &f[NP_RPMB_OFF_DATA], NP_RPMB_MAC_SPAN, mac) != NP_OK) {
        return NP_ERR_ROLLBACK_STORE;
    }
    if (!np_rpmb_ct_equal(mac, &f[NP_RPMB_OFF_MAC], NP_RPMB_MAC_SIZE)) {
        return NP_ERR_ROLLBACK_STORE;
    }
    return NP_OK;
}

/* Send a request carrying a fresh nonce, read the response, and check it
 * answers that request: type, result, MAC, the same nonce, the same address. */
static np_status_t nonce_exchange(const np_rpmb_ops_t *ops,
                                  const uint8_t key[NP_RPMB_KEY_SIZE],
                                  uint16_t req, uint16_t resp, uint16_t address,
                                  uint8_t f[NP_RPMB_FRAME_SIZE])
{
    uint8_t nonce[NP_RPMB_NONCE_SIZE];

    if (np_entropy_get(nonce, sizeof(nonce)) != NP_OK) {
        return NP_ERR_ROLLBACK_STORE;
    }
    frame_clear(f);
    for (uint32_t i = 0U; i < NP_RPMB_NONCE_SIZE; i++) {
        f[NP_RPMB_OFF_NONCE + i] = nonce[i];
    }
    np_rpmb_put16(f, NP_RPMB_OFF_ADDRESS, address);
    np_rpmb_put16(f, NP_RPMB_OFF_REQ_RESP, req);

    if (ops->write_frame(ops->ctx, f, false) != NP_OK) {
        return NP_ERR_ROLLBACK_STORE;
    }
    frame_clear(f);
    if (ops->read_frame(ops->ctx, f) != NP_OK) {
        return NP_ERR_ROLLBACK_STORE;
    }
    np_status_t ret = response_check(key, f, resp);
    if (ret != NP_OK) {
        return ret;
    }
    if (!np_rpmb_ct_equal(nonce, &f[NP_RPMB_OFF_NONCE], NP_RPMB_NONCE_SIZE)) {
        return NP_ERR_ROLLBACK_STORE;   /* a response to some other request */
    }
    if (np_rpmb_get16(f, NP_RPMB_OFF_ADDRESS) != address) {
        return NP_ERR_ROLLBACK_STORE;
    }
    return NP_OK;
}

np_status_t np_rpmb_read_counter(const np_rpmb_ops_t *ops,
                                 const uint8_t key[NP_RPMB_KEY_SIZE],
                                 uint32_t *counter)
{
    uint8_t f[NP_RPMB_FRAME_SIZE];
    if (!ops_ok(ops, key) || counter == NULL) {
        return NP_ERR_INVALID_ARG;
    }
    np_status_t ret = nonce_exchange(ops, key, NP_RPMB_REQ_COUNTER_READ,
                                     NP_RPMB_RESP_COUNTER_READ, 0U, f);
    if (ret == NP_OK) {
        *counter = np_rpmb_get32(f, NP_RPMB_OFF_COUNTER);
    }
    return ret;
}

np_status_t np_rpmb_read_block(const np_rpmb_ops_t *ops,
                               const uint8_t key[NP_RPMB_KEY_SIZE],
                               uint16_t address,
                               uint8_t data[NP_RPMB_DATA_SIZE])
{
    uint8_t f[NP_RPMB_FRAME_SIZE];
    if (!ops_ok(ops, key) || data == NULL) {
        return NP_ERR_INVALID_ARG;
    }
    /* Block count is left 0 in the read request, as U-Boot's rpmb.c does;
     * the JESD84-B51 wording for it is part of OI-SW02-02. */
    np_status_t ret = nonce_exchange(ops, key, NP_RPMB_REQ_DATA_READ,
                                     NP_RPMB_RESP_DATA_READ, address, f);
    if (ret == NP_OK) {
        for (uint32_t i = 0U; i < NP_RPMB_DATA_SIZE; i++) {
            data[i] = f[NP_RPMB_OFF_DATA + i];
        }
    }
    return ret;
}

np_status_t np_rpmb_write_block(const np_rpmb_ops_t *ops,
                                const uint8_t key[NP_RPMB_KEY_SIZE],
                                uint16_t address,
                                const uint8_t data[NP_RPMB_DATA_SIZE])
{
    uint8_t  f[NP_RPMB_FRAME_SIZE];
    uint32_t counter = 0U;

    if (!ops_ok(ops, key) || data == NULL) {
        return NP_ERR_INVALID_ARG;
    }
    np_status_t ret = np_rpmb_read_counter(ops, key, &counter);
    if (ret != NP_OK) {
        return ret;
    }
    if (counter == UINT32_MAX) {
        return NP_ERR_ROLLBACK_STORE;   /* expired: no write can be accepted */
    }

    /* The authenticated write request, under a reliable write. */
    frame_clear(f);
    for (uint32_t i = 0U; i < NP_RPMB_DATA_SIZE; i++) {
        f[NP_RPMB_OFF_DATA + i] = data[i];
    }
    np_rpmb_put32(f, NP_RPMB_OFF_COUNTER, counter);
    np_rpmb_put16(f, NP_RPMB_OFF_ADDRESS, address);
    np_rpmb_put16(f, NP_RPMB_OFF_BLOCK_COUNT, 1U);
    np_rpmb_put16(f, NP_RPMB_OFF_REQ_RESP, NP_RPMB_REQ_DATA_WRITE);
    ret = frame_sign(key, f);
    if (ret != NP_OK) {
        return ret;
    }
    if (ops->write_frame(ops->ctx, f, true) != NP_OK) {
        return NP_ERR_ROLLBACK_STORE;
    }

    /* The result read request: everything zero but the type. */
    frame_clear(f);
    np_rpmb_put16(f, NP_RPMB_OFF_REQ_RESP, NP_RPMB_REQ_RESULT_READ);
    if (ops->write_frame(ops->ctx, f, false) != NP_OK) {
        return NP_ERR_ROLLBACK_STORE;
    }
    frame_clear(f);
    if (ops->read_frame(ops->ctx, f) != NP_OK) {
        return NP_ERR_ROLLBACK_STORE;
    }
    ret = response_check(key, f, NP_RPMB_RESP_DATA_WRITE);
    if (ret != NP_OK) {
        return ret;
    }
    /* Committed means the counter moved by exactly one, at this address. */
    if (np_rpmb_get32(f, NP_RPMB_OFF_COUNTER) != counter + 1U ||
        np_rpmb_get16(f, NP_RPMB_OFF_ADDRESS) != address) {
        return NP_ERR_ROLLBACK_STORE;
    }
    return NP_OK;
}
