/*
 * NeurOne Hub Control Program — App Protocol Transport (OI-HUB-MAIN-01)
 * Document: NP-FW-HUB-001 Rev 1 §2
 *
 * Reassembles a complete, length-prefixed protocol blob from the fragmented
 * chunks delivered by either transport and hands it to the hub control task:
 *
 *   - BLE 5.3 LE GATT: the app writes the blob to a protocol characteristic in
 *     ATT-MTU-sized fragments (typically 20–512 bytes each).
 *   - USB-C CDC-ACM:   the app streams the blob over the bulk endpoint.
 *
 * Wire framing (identical on both transports):
 *
 *     ┌───────────────────────────┬────────────────────────────────────────┐
 *     │ length prefix (4 B, LE)   │ protocol blob (`length` bytes)          │
 *     └───────────────────────────┴────────────────────────────────────────┘
 *
 * `length` is the exact byte count of the signed protocol blob that follows
 * (np_proto_header_t + commands + Ed25519 signature); it must be in
 * [1, NP_HUB_PROTO_BLOB_MAX].  The blob's own signature is verified later by
 * np_protocol_verify_and_parse(); this layer only reassembles bytes.
 *
 * Producer side (BLE write handler / CDC RX) calls np_transport_feed(); the
 * single consumer (task_protocol_rx, via np_hal_proto_queue_receive) blocks
 * until one whole blob is ready.  A single-slot mailbox holds one completed
 * blob: the producer is refused (NP_HUB_ERR_SESSION_ACTIVE) until the consumer
 * takes it, which naturally backpressures a client that pipelines protocols.
 */

#ifndef NP_TRANSPORT_H
#define NP_TRANSPORT_H

#include "np_hub_types.h"

/* Length prefix width, in bytes (little-endian uint32). */
#define NP_TRANSPORT_LEN_PREFIX 4U

/*
 * np_transport_init — reset reassembly + mailbox state and initialize the
 * consumer wait primitive.  Call once at hub bring-up before starting tasks.
 */
void np_transport_init(void);

/*
 * np_transport_feed — producer entry point (BLE GATT write handler / USB-C CDC
 * RX callback).  Appends `len` bytes of the framed stream.  When a whole blob
 * has arrived it is latched into the mailbox and the consumer is signalled.
 *
 *   NP_HUB_OK               bytes accepted (blob may or may not be complete)
 *   NP_HUB_ERR_INVALID_ARG  data==NULL with len>0, or a malformed length prefix
 *                           (0 or > NP_HUB_PROTO_BLOB_MAX) — the partial frame
 *                           is discarded and reassembly restarts
 *   NP_HUB_ERR_SESSION_ACTIVE  a completed blob is still awaiting pickup; the
 *                           new bytes are dropped (client must wait)
 *
 * Contract: called from a task context — the BLE host-stack callback task or
 * the USB-C CDC RX task — NOT a raw ISR.  The mailbox handoff is guarded by a
 * task-level critical section (np_transport_hal_enter/exit_critical); most BLE
 * and USB stacks already defer received data to a task, so this holds.
 */
np_hub_status_t np_transport_feed(const uint8_t *data, size_t len);

/*
 * np_transport_reset — abort any in-progress reassembly (e.g. on BLE disconnect
 * or CDC line-state drop).  A completed-but-unread blob is preserved.
 */
void np_transport_reset(void);

/*
 * np_hal_proto_queue_receive — consumer entry point (OI-HUB-MAIN-01).  Blocks
 * up to `timeout_ms` for one complete blob, copies it into `buf`, and writes the
 * byte count to *recv_len_out.  Consuming the blob frees the mailbox for the
 * next one.
 *
 *   NP_HUB_OK                  a blob was delivered
 *   NP_HUB_ERR_TIMEOUT         no blob within timeout_ms
 *   NP_HUB_ERR_INVALID_ARG     buf/recv_len_out NULL
 *   NP_HUB_ERR_PARAMS_TOO_LONG buf_len smaller than the completed blob
 */
np_hub_status_t np_hal_proto_queue_receive(uint8_t  *buf,
                                           size_t    buf_len,
                                           size_t   *recv_len_out,
                                           uint32_t  timeout_ms);

/* ── Platform HAL (OI-HUB-MAIN-01a..e) ────────────────────────────────────────
 *
 * Implemented by the platform on target (FreeRTOS binary semaphore + task-level
 * critical section), host-modeled in np_transport.c under NPTEST_HOST.
 */

/* OI-HUB-MAIN-01a: create the consumer wait primitive.  Called by
 * np_transport_init() before any task is running. */
extern void np_transport_hal_init(void);

/* OI-HUB-MAIN-01b: block the calling task until signalled or timeout elapses.
 * Returns true if signalled (a blob is ready), false on timeout. */
extern bool np_transport_hal_wait(uint32_t timeout_ms);

/* OI-HUB-MAIN-01c: signal the waiting consumer that a blob is ready.
 * Called from np_transport_feed() in task context. */
extern void np_transport_hal_signal(void);

/* OI-HUB-MAIN-01d/e: guard the shared reassembly/mailbox state.  Task-level
 * critical section — feed() and receive() run in different tasks. */
extern void np_transport_hal_enter_critical(void);
extern void np_transport_hal_exit_critical(void);

#endif /* NP_TRANSPORT_H */
