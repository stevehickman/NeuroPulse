/*
 * NeurOne Bootloader — rollback-floor store on the eMMC's RPMB (OI-PRIV-32, NP-PRIV-ANALYSIS-004)
 * Requirements: REQ-SW02-15, REQ-SW02-16 (NP-SW-001 §5.2.2)
 *
 * The device implementation of np_rollback_store.h, replacing the store that
 * refused every read (np_rollback_store_unbuilt.c, removed). Where the floor
 * lives and its record: np_rollback_rpmb.h. The frame protocol and every check
 * on a response: np_emmc_rpmb.h.
 *
 * Every failure is NP_ERR_ROLLBACK_STORE, which np_rollback.c turns into
 * "refuse every image": no key (np_rpmb_key.h), no MAC primitive
 * (np_rpmb_mac.h), no nonce (np_entropy.h), an RPMB whose key is not
 * programmed (result 0x0007) or is not this key (MAC mismatch), a replayed or
 * stale response, any result error, or a half-sector that is not exactly a
 * floor record. There is no floor-0 path, in any build.
 *
 * Today three of those seams are unbuilt in the device build and fail, so the
 * device still refuses every image until the key source, OI-SW02-01 (the MAC)
 * and OI-SW02-03 (the entropy source) are done.
 */
#include "np_rollback_store.h"
#include "np_rollback_rpmb.h"
#include "np_emmc.h"
#include "np_emmc_rpmb.h"
#include "np_rpmb_key.h"

/* Select the RPMB, run `op`, and always select the user area again. The RPMB
 * is left selected on no path: every other caller of np_emmc_read/_write
 * assumes the user area. */
typedef np_status_t (*rpmb_op_t)(const np_rpmb_ops_t *ops, const uint8_t key[32], void *arg);

static np_status_t with_rpmb(rpmb_op_t op, void *arg)
{
    uint8_t key[NP_RPMB_KEY_SIZE];
    np_status_t ret = np_rpmb_key_get(key);
    if (ret == NP_OK) {
        ret = np_emmc_switch_partition((uint8_t)NP_EMMC_RPMB_ACCESS);
        if (ret == NP_OK) {
            ret = op(np_emmc_rpmb_ops(), key, arg);
        }
        if (np_emmc_switch_partition(NP_EMMC_USER_AREA) != NP_OK) {
            ret = NP_ERR_ROLLBACK_STORE;
        }
    }
    np_rpmb_wipe(key, sizeof(key));
    return (ret == NP_OK) ? NP_OK : NP_ERR_ROLLBACK_STORE;
}

static np_status_t op_read(const np_rpmb_ops_t *ops, const uint8_t key[32], void *arg)
{
    uint8_t data[NP_RPMB_DATA_SIZE];
    uint32_t *floor = (uint32_t *)arg;
    if (np_rpmb_read_block(ops, key, NP_ROLLBACK_RPMB_ADDRESS, data) != NP_OK) {
        return NP_ERR_ROLLBACK_STORE;
    }
    return np_rollback_rpmb_decode(data, floor) ? NP_OK : NP_ERR_ROLLBACK_STORE;
}

static np_status_t op_raise(const np_rpmb_ops_t *ops, const uint8_t key[32], void *arg)
{
    const uint32_t wanted = *(const uint32_t *)arg;
    uint8_t  data[NP_RPMB_DATA_SIZE];
    uint32_t stored = 0U;

    if (op_read(ops, key, &stored) != NP_OK) {
        return NP_ERR_ROLLBACK_STORE;   /* no record: never written from here */
    }
    if (wanted <= stored) {
        return NP_OK;                   /* the contract: never lowered */
    }
    np_rollback_rpmb_encode(wanted, data);
    if (np_rpmb_write_block(ops, key, NP_ROLLBACK_RPMB_ADDRESS, data) != NP_OK) {
        return NP_ERR_ROLLBACK_STORE;
    }
    /* Read it back, authenticated, before reporting the raise. */
    if (op_read(ops, key, &stored) != NP_OK || stored != wanted) {
        return NP_ERR_ROLLBACK_STORE;
    }
    return NP_OK;
}

np_status_t np_rollback_store_read(uint32_t *floor)
{
    uint32_t value = 0U;
    if (floor == NULL) {
        return NP_ERR_ROLLBACK_STORE;
    }
    np_status_t ret = with_rpmb(op_read, &value);
    if (ret == NP_OK) {
        *floor = value;                 /* written only on success */
    }
    return ret;
}

np_status_t np_rollback_store_raise(uint32_t floor)
{
    return with_rpmb(op_raise, &floor);
}
