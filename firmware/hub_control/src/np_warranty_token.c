/*
 * NeurOne Hub Control Program — Warranty Token (OI-WA-03)
 * Document: NP-FW-EMMC-002 §A.2, §A.3
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * See np_warranty_token.h.
 */

#include "np_warranty_token.h"

#include <stdbool.h>
#include <string.h>

#include "np_cfg_store.h"
#include "np_session_log.h"

static uint8_t s_token[NP_WARRANTY_TOKEN_LEN];
static bool    s_have;

/* A token whose bytes are all the same is not a TRNG draw: a stuck or unclocked
 * source reads as all-0 or all-1.  The driver's health tests are the real
 * guard (§A.3); this one is cheap and catches the failure that matters most,
 * a fleet of devices sharing one token. */
static bool token_degenerate(const uint8_t *t)
{
    for (size_t i = 1U; i < NP_WARRANTY_TOKEN_LEN; i++) {
        if (t[i] != t[0]) {
            return false;
        }
    }
    return true;
}

static bool record_valid(const uint8_t *rec)
{
    for (size_t i = NP_WARRANTY_OFF_RESERVED; i < NP_WARRANTY_RECORD_LEN; i++) {
        if (rec[i] != 0U) {
            return false;
        }
    }
    return !token_degenerate(rec);
}

static np_hub_status_t provision(uint8_t rec[NP_WARRANTY_RECORD_LEN])
{
    memset(rec, 0, NP_WARRANTY_RECORD_LEN);
    if (np_warranty_hal_trng_generate(rec, NP_WARRANTY_TOKEN_LEN) != NP_HUB_OK ||
        token_degenerate(rec)) {
        return NP_HUB_ERR_GENERIC;
    }
    uint32_t n = np_log_session_count();
    rec[NP_WARRANTY_OFF_SESSION]      = (uint8_t)(n & 0xFFU);
    rec[NP_WARRANTY_OFF_SESSION + 1U] = (uint8_t)((n >> 8) & 0xFFU);
    rec[NP_WARRANTY_OFF_SESSION + 2U] = (uint8_t)((n >> 16) & 0xFFU);
    rec[NP_WARRANTY_OFF_SESSION + 3U] = (uint8_t)((n >> 24) & 0xFFU);

    /* Handed out only once it is durable.  A token the app registered and the
     * device then lost to a power cut would be the orphaning this module exists
     * to prevent, from the other side. */
    if (np_cfg_store_replicated_write(NP_CFG_FILE_WARRANTY_TOKEN, rec,
                                      NP_WARRANTY_RECORD_LEN) != NP_HUB_OK) {
        return NP_HUB_ERR_GENERIC;
    }
    return NP_HUB_OK;
}

np_hub_status_t np_warranty_token_get(uint8_t out[NP_WARRANTY_TOKEN_LEN])
{
    if (out == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    if (!s_have) {
        uint8_t rec[NP_WARRANTY_RECORD_LEN];
        np_hub_status_t st = np_cfg_store_replicated_read(
            NP_CFG_FILE_WARRANTY_TOKEN, rec, sizeof(rec));

        if (st == NP_HUB_OK) {
            if (!record_valid(rec)) {
                return NP_HUB_ERR_STORE_INTEGRITY;
            }
        } else if (st == NP_HUB_ERR_NOT_PRESENT) {
            /* Neither copy exists: this device has never had a token. */
            st = provision(rec);
            if (st != NP_HUB_OK) {
                memset(rec, 0, sizeof(rec));
                return st;
            }
        } else {
            /* Present but unreadable, or refused: NOT an absence. */
            return st;
        }
        memcpy(s_token, rec, NP_WARRANTY_TOKEN_LEN);
        s_have = true;
    }
    memcpy(out, s_token, NP_WARRANTY_TOKEN_LEN);
    return NP_HUB_OK;
}

void np_warranty_token_reset_cache(void)
{
    memset(s_token, 0, sizeof(s_token));
    s_have = false;
}
