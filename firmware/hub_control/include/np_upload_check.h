/*
 * NeurOne Hub Control Program — upload check (SWR-UP-01, OI-UI-KMP-10 U1)
 * Document: NP-SW-001 §6.3 (SWR-UP-01, SWR-UP-03); NP-FW-HUB-001 §4
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * After every protocol upload the app has to know whether the helmet holds the protocol it sent.
 * The app takes a hash of the exact bytes it sent; the hub hashes the buffer it received, BEFORE
 * parsing, and reports that hash with an accept or refuse status over UPLOAD_CHECK (READ/NOTIFY).
 * The app compares the two.  The hub is safe without this (the signature check already rejects a bad
 * blob, np_runner_load); what was missing is the app learning the outcome, so that it can know whether
 * a protocol has been used and can tell a corrupted or truncated upload from a success.
 *
 * Wire (NP_GATT_UPLOAD_CHECK_LEN = 34 bytes):
 *   [0]      status   NP_UPLOAD_NONE / _ACCEPTED / _REFUSED / _PENDING
 *   [1]      reason   0 unless REFUSED; then the refusal's np_hub_status_t code as a positive number
 *   [2..33]  SHA-256 of the received bytes (all zero while status is NONE)
 *
 * PENDING is the window between the hash and the verdict.  An app that reads PENDING reads again; it
 * never sends the protocol again to resolve an unknown outcome (SWR-UP-03), because the hub refuses a
 * second protocol while one is loaded or running.
 *
 * A truncated upload never completes at the transport, so no verdict is produced for it: the report
 * still describes the previous upload, and the app sees a hash that is not the one it sent.
 *
 * The hash is of protocol bytes the person authored.  It carries no user biology and goes to the
 * user's own app only.
 */

#ifndef NP_UPLOAD_CHECK_H
#define NP_UPLOAD_CHECK_H

#include <stddef.h>
#include <stdint.h>

#include "np_hub_types.h"

#define NP_UPLOAD_CHECK_LEN   34U

typedef enum {
    NP_UPLOAD_NONE     = 0,   /* nothing received since power-on */
    NP_UPLOAD_ACCEPTED = 1,
    NP_UPLOAD_REFUSED  = 2,
    NP_UPLOAD_PENDING  = 3,   /* hashed, verdict not yet reached */
} np_upload_status_t;

/* Forget the last upload (power-on). */
void np_upload_check_init(void);

/*
 * np_upload_check_received — hash the received buffer.  Call BEFORE the buffer is parsed or verified,
 * so the hash is of the bytes that arrived and not of anything derived from them.  Status becomes PENDING.
 */
void np_upload_check_received(const uint8_t *buf, size_t len);

/*
 * np_upload_check_result — record the verdict of np_runner_load for the buffer just hashed, and notify.
 * NP_HUB_OK is ACCEPTED; anything else is REFUSED with that code as the reason.
 */
void np_upload_check_result(np_hub_status_t load_rc);

/* The GATT read of UPLOAD_CHECK. */
np_hub_status_t np_upload_check_read(uint8_t *buf, size_t cap, size_t *len_out);

#endif /* NP_UPLOAD_CHECK_H */
