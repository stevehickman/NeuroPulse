/*
 * NeurOne Hub Control Program — upload check (SWR-UP-01)
 * See np_upload_check.h.
 */

#include "np_upload_check.h"

#include <string.h>

#include "np_crypto.h"
#include "np_gatt_server.h"
#include "np_transport.h"   /* np_transport_hal_enter/exit_critical */

_Static_assert(NP_UPLOAD_CHECK_LEN == NP_GATT_UPLOAD_CHECK_LEN, "wire length is the generated constant");
_Static_assert(NP_UPLOAD_CHECK_LEN == 2U + NP_SHA256_SIZE, "status, reason, hash");

static uint8_t s_frame[NP_UPLOAD_CHECK_LEN];

void np_upload_check_init(void)
{
    np_transport_hal_enter_critical();
    memset(s_frame, 0, sizeof(s_frame));
    np_transport_hal_exit_critical();
}

void np_upload_check_received(const uint8_t *buf, size_t len)
{
    uint8_t digest[NP_SHA256_SIZE];
    if (buf == NULL || len == 0U || len > UINT32_MAX) {
        return;   /* nothing arrived; the previous report stands */
    }
    np_sha256(buf, (uint32_t)len, digest);

    np_transport_hal_enter_critical();
    s_frame[0] = (uint8_t)NP_UPLOAD_PENDING;
    s_frame[1] = 0U;
    memcpy(&s_frame[2], digest, NP_SHA256_SIZE);
    np_transport_hal_exit_critical();
}

void np_upload_check_result(np_hub_status_t load_rc)
{
    uint8_t out[NP_UPLOAD_CHECK_LEN];

    np_transport_hal_enter_critical();
    if (s_frame[0] != (uint8_t)NP_UPLOAD_PENDING) {
        np_transport_hal_exit_critical();
        return;   /* no hash to attach a verdict to */
    }
    if (load_rc == NP_HUB_OK) {
        s_frame[0] = (uint8_t)NP_UPLOAD_ACCEPTED;
        s_frame[1] = 0U;
    } else {
        s_frame[0] = (uint8_t)NP_UPLOAD_REFUSED;
        s_frame[1] = (uint8_t)((load_rc < 0) ? -(int)load_rc : (int)load_rc);
        if (s_frame[1] == 0U) {
            s_frame[1] = 0xFFU;   /* a refusal always names a non-zero reason */
        }
    }
    memcpy(out, s_frame, sizeof(out));
    np_transport_hal_exit_critical();

    (void)np_gatt_notify(NP_GATT_UPLOAD_CHECK_ID, out, sizeof(out));
}

np_hub_status_t np_upload_check_read(uint8_t *buf, size_t cap, size_t *len_out)
{
    if (buf == NULL || len_out == NULL || cap < NP_UPLOAD_CHECK_LEN) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    np_transport_hal_enter_critical();
    memcpy(buf, s_frame, NP_UPLOAD_CHECK_LEN);
    np_transport_hal_exit_critical();
    *len_out = NP_UPLOAD_CHECK_LEN;
    return NP_HUB_OK;
}
