/*
 * NeurOne Hub Control Program — BLE GATT Server (OI-WA-03, GitHub #381)
 * Document: NP-APP-ROADMAP-001 §5
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * See np_gatt_server.h.
 */

#include "np_gatt_server.h"

#include <string.h>

#include "np_cvns_fault_summary.h"
#include "np_warranty_token.h"

/* 4E455550-0000-1000-8000-00805F9B34FB, least-significant byte first; the id
 * (the second group) goes in bytes 10–11. */
static const uint8_t k_uuid_base[16] = {
    0xFBu, 0x34u, 0x9Bu, 0x5Fu, 0x80u, 0x00u, 0x00u, 0x80u,
    0x00u, 0x10u, 0x00u, 0x00u, 0x50u, 0x55u, 0x45u, 0x4Eu,
};

/* ── Handlers ───────────────────────────────────────────────────────────────── */

static np_hub_status_t read_warranty_token(uint8_t *buf, size_t cap, size_t *len_out)
{
    if (cap < NP_WARRANTY_TOKEN_LEN) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    np_hub_status_t st = np_warranty_token_get(buf);
    if (st == NP_HUB_OK) {
        *len_out = NP_WARRANTY_TOKEN_LEN;
    }
    return st;
}

/* ── The table ───────────────────────────────────────────────────────────────
 * One row per characteristic the hub answers.  Add a row only with its
 * producer (np_gatt_server.h, "only what the hub produces"). */
static const np_gatt_char_t k_table[] = {
    { NP_GATT_ID_WARRANTY_TOKEN, NP_GATT_PROP_READ,
      (uint8_t)NP_WARRANTY_TOKEN_LEN, read_warranty_token, NULL },

    { NP_GATT_ID_CVNS_FAULT_STATUS, NP_GATT_PROP_READ | NP_GATT_PROP_NOTIFY,
      (uint8_t)NP_CVFS_FRAME_MAX, np_cvfs_read, NULL },

    { NP_GATT_ID_CVNS_REENABLE_CONFIRM, NP_GATT_PROP_WRITE,
      1u, NULL, np_cvfs_on_reenable_confirm_write },

    { NP_GATT_ID_ACTIVE_USER, NP_GATT_PROP_WRITE,
      4u, NULL, np_cvfs_on_active_user_write },
};

#define K_TABLE_LEN (sizeof(k_table) / sizeof(k_table[0]))

_Static_assert(NP_CVFS_FRAME_MAX <= NP_GATT_VALUE_MAX,
               "NP_GATT_VALUE_MAX must hold the CVNS_FAULT_STATUS frame");
_Static_assert(NP_WARRANTY_TOKEN_LEN <= NP_GATT_VALUE_MAX,
               "NP_GATT_VALUE_MAX must hold the warranty token");

static const np_gatt_char_t *find(uint16_t id)
{
    for (size_t i = 0U; i < K_TABLE_LEN; i++) {
        if (k_table[i].id == id) {
            return &k_table[i];
        }
    }
    return NULL;
}

/* ── API ────────────────────────────────────────────────────────────────────── */

void np_gatt_uuid128(uint16_t id, uint8_t out[16])
{
    memcpy(out, k_uuid_base, sizeof(k_uuid_base));
    out[10] = (uint8_t)(id & 0xFFu);
    out[11] = (uint8_t)(id >> 8);
}

const np_gatt_char_t *np_gatt_table(size_t *count_out)
{
    if (count_out != NULL) {
        *count_out = K_TABLE_LEN;
    }
    return k_table;
}

np_hub_status_t np_gatt_init(void)
{
    return np_gatt_hal_register(k_table, K_TABLE_LEN);
}

np_att_status_t np_gatt_on_read(uint16_t id, uint16_t offset,
                                uint8_t *buf, size_t cap, size_t *len_out)
{
    const np_gatt_char_t *c = find(id);
    if (c == NULL) {
        return NP_ATT_ATTRIBUTE_NOT_FOUND;
    }
    if ((c->props & NP_GATT_PROP_READ) == 0U || c->read == NULL) {
        return NP_ATT_READ_NOT_PERMITTED;
    }
    if (buf == NULL || len_out == NULL) {
        return NP_ATT_UNLIKELY_ERROR;
    }

    uint8_t value[NP_GATT_VALUE_MAX];
    size_t  len = 0U;
    np_hub_status_t st = c->read(value, sizeof(value), &len);
    if (st != NP_HUB_OK) {
        memset(value, 0, sizeof(value));
        return NP_ATT_APP_UNAVAILABLE;
    }
    if (len > c->max_len) {
        /* A producer overran its own row: a defect, never a value to send. */
        memset(value, 0, sizeof(value));
        return NP_ATT_UNLIKELY_ERROR;
    }
    if (offset > len) {
        memset(value, 0, sizeof(value));
        return NP_ATT_INVALID_OFFSET;
    }
    size_t n = len - offset;
    if (n > cap) {
        n = cap;   /* the stack asks again from offset + n */
    }
    memcpy(buf, value + offset, n);
    *len_out = n;
    memset(value, 0, sizeof(value));
    return NP_ATT_OK;
}

np_att_status_t np_gatt_on_write(uint16_t id, const uint8_t *data, size_t len)
{
    const np_gatt_char_t *c = find(id);
    if (c == NULL) {
        return NP_ATT_ATTRIBUTE_NOT_FOUND;
    }
    if ((c->props & NP_GATT_PROP_WRITE) == 0U || c->write == NULL) {
        return NP_ATT_WRITE_NOT_PERMITTED;
    }
    if (data == NULL || len != c->max_len) {
        return NP_ATT_INVALID_VALUE_LENGTH;
    }
    return (c->write(data, len) == NP_HUB_OK) ? NP_ATT_OK : NP_ATT_APP_REFUSED;
}

np_hub_status_t np_gatt_notify(uint16_t id, const uint8_t *data, size_t len)
{
    const np_gatt_char_t *c = find(id);
    if (c == NULL || (c->props & NP_GATT_PROP_NOTIFY) == 0U ||
        data == NULL || len == 0U || len > c->max_len) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    np_gatt_hal_notify(id, data, len);
    return NP_HUB_OK;
}

/* ── np_cvns_fault_summary's notification seam ──────────────────────────────
 * Declared in np_cvns_fault_summary.h, and until this server existed a platform
 * trap.  It is not a driver: it is a row of the table above, so it is defined
 * here and the SW-02 census drops by one (np_sw02_platform_hal.h). */
void np_cvfs_hal_notify(const uint8_t *frame, size_t len)
{
    (void)np_gatt_notify(NP_GATT_ID_CVNS_FAULT_STATUS, frame, len);
}
