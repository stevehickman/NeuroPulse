/*
 * NeurOne Hub Control Program — App Protocol Transport (OI-HUB-MAIN-01)
 * Document: NP-FW-HUB-001 Rev 1 §2
 *
 * Length-prefixed reassembly of a protocol blob arriving in fragments over BLE
 * GATT or USB-C CDC, into a single-slot mailbox drained by task_protocol_rx.
 * See np_transport.h for the wire framing and the producer/consumer contract.
 *
 * The reassembly + mailbox logic here is transport-agnostic and host-tested; the
 * wait/signal/critical primitives (OI-HUB-MAIN-01a..e) are the only platform
 * boundary.
 */

#include "np_transport.h"
#include <string.h>

/* ── Shared reassembly + mailbox state (guarded by the critical-section HAL) ──── */

static uint8_t s_blob[NP_HUB_PROTO_BLOB_MAX]; /* reassembly + latched blob        */
static uint8_t s_hdr[NP_TRANSPORT_LEN_PREFIX];/* length-prefix accumulator         */
static size_t  s_hdr_got;                     /* prefix bytes received so far      */
static size_t  s_expected;                    /* blob length once prefix parsed    */
static size_t  s_received;                    /* blob bytes received so far        */
static bool    s_complete;                    /* a whole blob awaits pickup        */
static size_t  s_complete_len;                /* latched blob length               */

/* Reset only the reassembly cursors — never touches s_blob content or the
 * completed-mailbox latch.  Caller holds the critical section. */
static void reset_frame(void)
{
    s_hdr_got  = 0U;
    s_expected = 0U;
    s_received = 0U;
}

/* ── Init ─────────────────────────────────────────────────────────────────────── */

void np_transport_init(void)
{
    np_transport_hal_init();
    reset_frame();
    s_complete     = false;
    s_complete_len = 0U;
}

/* ── Producer ─────────────────────────────────────────────────────────────────── */

np_hub_status_t np_transport_feed(const uint8_t *data, size_t len)
{
    if (len == 0U) {
        return NP_HUB_OK;
    }
    if (data == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    np_transport_hal_enter_critical();

    /* Single-slot mailbox: refuse new bytes until the consumer takes the last
     * completed blob.  Backpressures a client that pipelines protocols. */
    if (s_complete) {
        np_transport_hal_exit_critical();
        return NP_HUB_ERR_SESSION_ACTIVE;
    }

    np_hub_status_t rc = NP_HUB_OK;
    bool signal = false;

    while (len > 0U) {
        /* Phase 1 — accumulate the 4-byte little-endian length prefix. */
        if (s_hdr_got < NP_TRANSPORT_LEN_PREFIX) {
            size_t need = NP_TRANSPORT_LEN_PREFIX - s_hdr_got;
            size_t n    = (len < need) ? len : need;
            memcpy(s_hdr + s_hdr_got, data, n);
            s_hdr_got += n;
            data      += n;
            len       -= n;

            if (s_hdr_got == NP_TRANSPORT_LEN_PREFIX) {
                s_expected = (size_t)s_hdr[0]
                           | ((size_t)s_hdr[1] << 8)
                           | ((size_t)s_hdr[2] << 16)
                           | ((size_t)s_hdr[3] << 24);
                if (s_expected == 0U || s_expected > NP_HUB_PROTO_BLOB_MAX) {
                    reset_frame();               /* discard the malformed frame */
                    rc = NP_HUB_ERR_INVALID_ARG;
                    break;
                }
            }
            continue;
        }

        /* Phase 2 — collect the blob payload. */
        size_t need = s_expected - s_received;
        size_t n    = (len < need) ? len : need;
        memcpy(s_blob + s_received, data, n);
        s_received += n;
        data       += n;
        len        -= n;

        if (s_received == s_expected) {
            s_complete_len = s_expected;
            s_complete     = true;
            reset_frame();
            signal = true;
            /* Any trailing bytes belong to a pipelined next frame; drop them —
             * the mailbox is full and the client must wait for pickup. */
            break;
        }
    }

    np_transport_hal_exit_critical();

    if (signal) {
        np_transport_hal_signal();
    }
    return rc;
}

void np_transport_reset(void)
{
    np_transport_hal_enter_critical();
    reset_frame();
    np_transport_hal_exit_critical();
}

/* ── Consumer (OI-HUB-MAIN-01) ────────────────────────────────────────────────── */

np_hub_status_t np_hal_proto_queue_receive(uint8_t  *buf,
                                           size_t    buf_len,
                                           size_t   *recv_len_out,
                                           uint32_t  timeout_ms)
{
    if (buf == NULL || recv_len_out == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    if (!np_transport_hal_wait(timeout_ms)) {
        return NP_HUB_ERR_TIMEOUT;
    }

    np_transport_hal_enter_critical();

    if (!s_complete) {
        /* Spurious wake with no blob latched. */
        np_transport_hal_exit_critical();
        return NP_HUB_ERR_TIMEOUT;
    }

    size_t          n  = s_complete_len;
    np_hub_status_t rc;
    if (n > buf_len) {
        rc = NP_HUB_ERR_PARAMS_TOO_LONG;
    } else {
        memcpy(buf, s_blob, n);
        *recv_len_out = n;
        rc = NP_HUB_OK;
    }

    /* Free the mailbox regardless — a too-large blob is dropped, not retried. */
    s_complete     = false;
    s_complete_len = 0U;
    reset_frame();

    np_transport_hal_exit_critical();
    return rc;
}

/* ── Host stubs (NPTEST_HOST) ─────────────────────────────────────────────────── */

#ifdef NPTEST_HOST

/* Single-threaded deterministic model: the "wait" simply reports whether a blob
 * is already latched, so tests feed-then-receive without real blocking. */
void np_transport_hal_init(void)           { }
bool np_transport_hal_wait(uint32_t t)     { (void)t; return s_complete; }
void np_transport_hal_signal(void)         { }
void np_transport_hal_enter_critical(void) { }
void np_transport_hal_exit_critical(void)  { }

#else  /* target: FreeRTOS binary semaphore + task-level critical section */

#include "FreeRTOS.h"
#include "semphr.h"

static SemaphoreHandle_t s_blob_ready;

void np_transport_hal_init(void)
{
    if (s_blob_ready == NULL) {
        s_blob_ready = xSemaphoreCreateBinary();
        configASSERT(s_blob_ready != NULL);
    }
}

bool np_transport_hal_wait(uint32_t timeout_ms)
{
    if (s_blob_ready == NULL) {
        return false;
    }
    /* A UINT32_MAX timeout means block forever — task_protocol_rx passes
     * portMAX_DELAY, which equals UINT32_MAX for the 32-bit-tick target.  Route
     * it straight to the port's max-delay value; pdMS_TO_TICKS() would overflow
     * it into a short finite wait instead.  INCLUDE_vTaskSuspend=1 makes the
     * indefinite block valid.  (Comparing against UINT32_MAX rather than
     * portMAX_DELAY avoids a width mismatch under the 64-bit-tick POSIX port.) */
    TickType_t ticks = (timeout_ms == UINT32_MAX)
                           ? portMAX_DELAY
                           : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(s_blob_ready, ticks) == pdTRUE;
}

void np_transport_hal_signal(void)
{
    if (s_blob_ready != NULL) {
        (void)xSemaphoreGive(s_blob_ready);
    }
}

void np_transport_hal_enter_critical(void)
{
    taskENTER_CRITICAL();
}

void np_transport_hal_exit_critical(void)
{
    taskEXIT_CRITICAL();
}

#endif /* NPTEST_HOST */
