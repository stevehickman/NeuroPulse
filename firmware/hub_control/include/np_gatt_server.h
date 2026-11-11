/*
 * NeurOne Hub Control Program — BLE GATT Server (OI-WA-03, GitHub #381)
 * Document: NP-APP-ROADMAP-001 §5 (the characteristic table);
 *           NP-SW-CI-001 §4.8 (the platform-layer pattern)
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * The NeurOne custom service, as the hub publishes it: which characteristics
 * exist, what each allows, how long its value may be, and which hub function
 * answers it.  The BLE host stack is a platform concern behind two seams; the
 * table and every read, write and notification decision are here, in pure C,
 * where a host test can reach them.
 *
 * ── UUIDs ───────────────────────────────────────────────────────────────────
 * 4E455550-XXXX-1000-8000-00805F9B34FB, where XXXX is the 16-bit id below and
 * 0x0001 is the service.  The ids and the base come from the same generated
 * source as the apps' strings (common/npps/constants.json, GattIds), and
 * np_gatt_uuid128() builds the 16 bytes from the id, so the base is written once.
 *
 * ── What is published: only what the hub produces ───────────────────────────
 * A characteristic is in the table only if a hub function answers it.  The
 * apps know more UUIDs than this (session state, HRV, OTA, consumables …);
 * publishing those with nothing behind them would let the app believe it had
 * found a working hub, and it is the same false-green the platform layer's
 * traps exist to refuse.  Each is added with its producer.  Notably absent:
 *   - CVNS_PAD_STATUS  (0x0013) — the electrode-to-side mapping it reports is
 *     an open hardware item (OI-ACC-07 → OI-CVNSHW-01, -06)
 * (The protocol upload (0x0008) was absent until OI-SIMFW-49: it is published now, with
 * np_transport_feed() as its handler, OI-HUB-MAIN-01, REQ-FWHUB-80; see NP_GATT_PROP_STREAM.)
 * scripts/check-consumable-triggers.ts fails the build if the first appears in
 * firmware without its §2.3 row being updated, and if CONSUMABLE_STATUS
 * (published since 2026-09-25, np_consumables.h) disappears from it.
 *
 * ── Tier ────────────────────────────────────────────────────────────────────
 * The cervical characteristics are T2-only in the app table, and are published
 * on every unit here: the firmware tier gate is not in force (OI-UPG-08), so
 * there is no tier to publish by.  None of them can start stimulation: the
 * fault summary is information, the confirm write is accepted only while the
 * hub awaits one (REQ-CVNS-09), and the safety MCU owns every enable line.
 *
 * IEC 62304 Class B (SW-02).  Pure C, host-testable (np_gatt_server_tests).
 */

#ifndef NP_GATT_SERVER_H
#define NP_GATT_SERVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "np_hub_types.h"

/* ── Ids (the XXXX of the UUID) ──────────────────────────────────────────────────
 * NP_GATT_*_ID and the UUID base are generated from common/npps/constants.json (group GattIds), the
 * one source the apps read as well; only the characteristics this hub publishes are in the header. */
#include "../../common/include/np_app_wire_constants.h"

/* ── Properties ──────────────────────────────────────────────────────────────── */
#define NP_GATT_PROP_READ     0x01u
#define NP_GATT_PROP_WRITE    0x02u   /* write with response */
#define NP_GATT_PROP_NOTIFY   0x04u
/* Access needs an encrypted link: on one that is not, the read, write or notification is refused (ATT 0x0F,
 * Insufficient Encryption).  Set on every characteristic that carries a protocol, a session, a log, user data or an
 * identifier (CLAUDE.md section 23, NP-FW-HUB-001 section 2.5, REQ-FWHUB-81); first on DEVICE_SERIAL, a stable hardware
 * serial readable by any central in range being a tracking identifier.  Enforced twice: by the stack at the permission
 * (np_gatt_hal_register), and here, against the link state the stack reports (np_gatt_set_link_encrypted), so a host
 * test reaches the refusal.  A modifier, not an access mode, so it is only meaningful with READ or WRITE. */
#define NP_GATT_PROP_ENC      0x08u
/* With WRITE: the characteristic takes a byte stream, not a value.  A write of 1..NP_GATT_STREAM_WRITE_MAX bytes is
 * accepted whatever the row's write_len (0 for such a row), and every write is handed to the handler as it arrives,
 * in order.  Only the protocol upload (0x0008) is one: the apps write the transport's framing across as many writes as
 * the blob needs, and np_transport.c reassembles it (REQ-FWHUB-04, REQ-FWHUB-80). */
#define NP_GATT_PROP_STREAM   0x10u

/* The longest attribute value ATT allows, and so the longest write the hub accepts to a STREAM characteristic
 * (Bluetooth Core v5.3 Vol 3 Part F §3.2.9).  The apps cut the upload at this size (BleTransfer.MAX_WRITE_SIZE). */
#define NP_GATT_STREAM_WRITE_MAX 512u

/* Largest value any characteristic here carries (CVNS_FAULT_STATUS, 36 B). */
#define NP_GATT_VALUE_MAX     36u

/* ── ATT results (Bluetooth Core v5.3 Vol 3 Part F §3.4.1.1) ──────────────────
 * What np_gatt_on_read()/np_gatt_on_write() return; the platform passes the
 * value through as the ATT error response, or sends success for 0. */
typedef enum {
    NP_ATT_OK                     = 0x00,
    NP_ATT_READ_NOT_PERMITTED     = 0x02,
    NP_ATT_WRITE_NOT_PERMITTED    = 0x03,
    NP_ATT_INVALID_OFFSET         = 0x07,
    NP_ATT_ATTRIBUTE_NOT_FOUND    = 0x0A,
    NP_ATT_INVALID_VALUE_LENGTH   = 0x0D,
    NP_ATT_UNLIKELY_ERROR         = 0x0E,
    /* The characteristic has NP_GATT_PROP_ENC and the link is not encrypted (Core v5.3 Vol 3 Part F §3.4.1.1). */
    NP_ATT_INSUFFICIENT_ENCRYPTION = 0x0F,
    /* Application error: the hub refused the value (a write outside the state
     * that accepts it, or a value its handler rejects).  The app's
     * write-with-response fails, which is what np_cvns_fault_summary.h's
     * handlers are documented to produce. */
    NP_ATT_APP_REFUSED            = 0x80,
    /* Application error: the value exists on the device but cannot be produced
     * now (a stored record unreadable).  Retrying may succeed. */
    NP_ATT_APP_UNAVAILABLE        = 0x81,
    /* Application error: a pause (or a second resume) was refused because the protocol is ramping up.  SESSION_PROGRESS
     * says how far the ramp is, so the app can tell the person why and how long (SWR-SS-05).  A stop is not refused. */
    NP_ATT_APP_RAMPING            = 0x82,
    /* Application error: a write to a STREAM characteristic was refused because the hub still holds the last blob
     * (NP_HUB_ERR_SESSION_ACTIVE, np_transport.h).  The bytes were dropped; wait for the hub to take it and write again
     * from the start of the stream. */
    NP_ATT_APP_BUSY               = 0x83,
} np_att_status_t;

typedef np_hub_status_t (*np_gatt_read_fn)(uint8_t *buf, size_t cap, size_t *len_out);
typedef np_hub_status_t (*np_gatt_write_fn)(const uint8_t *data, size_t len);

typedef struct {
    uint16_t          id;
    uint8_t           props;     /* NP_GATT_PROP_* */
    uint8_t           max_len;   /* longest READ / NOTIFY value, bytes         */
    uint8_t           write_len; /* exact length of every WRITE; 0 without WRITE */
    np_gatt_read_fn   read;      /* non-NULL iff props has READ  */
    np_gatt_write_fn  write;     /* non-NULL iff props has WRITE */
} np_gatt_char_t;

/* ── API ─────────────────────────────────────────────────────────────────────── */

/* The 128-bit UUID for `id`, least-significant byte first — the order BLE
 * stacks and the ATT wire use. */
void np_gatt_uuid128(uint16_t id, uint8_t out[16]);

/* The published table and its length.  Const: nothing edits it at run time. */
const np_gatt_char_t *np_gatt_table(size_t *count_out);

/*
 * np_gatt_init — register the service with the BLE stack (np_gatt_hal_register).
 * Call once at bring-up, after the modules the handlers call are initialised
 * (np_cvfs_init) and before the scheduler starts.
 */
np_hub_status_t np_gatt_init(void);

/*
 * np_gatt_on_read — the stack's READ / READ BLOB callback.  Produces the whole
 * value, then copies from `offset` (a long read arrives as several calls with
 * increasing offsets; each rebuilds the value, so a value that changes between
 * them is the app's usual long-read race, not a torn buffer here).
 */
np_att_status_t np_gatt_on_read(uint16_t id, uint16_t offset,
                                uint8_t *buf, size_t cap, size_t *len_out);

/*
 * np_gatt_on_write — the stack's WRITE REQUEST callback.  Every write here is
 * fixed-length (the row's write_len) except a STREAM characteristic's, which takes 1..NP_GATT_STREAM_WRITE_MAX
 * bytes: a value of any other length is refused before the handler sees it.  Prepared (long) writes are not
 * supported, and are not needed.
 */
np_att_status_t np_gatt_on_write(uint16_t id, const uint8_t *data, size_t len);

/*
 * np_gatt_set_link_encrypted — the stack's security callback: the link to the central is now encrypted (true) or not
 * (false).  The state starts false and fails closed: until the stack says the link is encrypted, every NP_GATT_PROP_ENC
 * characteristic refuses a read, a write and a notification.  "Encrypted" is all this says; whether the pairing that made
 * the key was authenticated (a passkey or a numeric comparison, not Just Works) is not decided here (OI-SIMFW-72).
 */
void np_gatt_set_link_encrypted(bool encrypted);

/*
 * np_gatt_on_disconnect — the stack's link-down callback.  A stream write that was cut off part way would leave the
 * transport holding half a blob, and the next link's first bytes would be read as the rest of it: this drops the partial
 * reassembly (np_transport_reset), and keeps a blob that was completed and not yet taken (REQ-FWHUB-80).  It also
 * clears the link state: the next link is not encrypted until the stack says so.
 */
void np_gatt_on_disconnect(void);

/*
 * np_gatt_notify — send `len` bytes as a notification of `id`.  Refuses an id
 * without NOTIFY and an over-length value (NP_HUB_ERR_INVALID_ARG).  A no-op
 * at the platform when nothing is subscribed.  On an ENC characteristic with the link not encrypted it sends nothing
 * and reports success: there is no one entitled to receive it, which is the same as nothing subscribed.
 */
np_hub_status_t np_gatt_notify(uint16_t id, const uint8_t *data, size_t len);

/* ── Platform seams (trap-defined in firmware/platform/src/np_platform_stub.c) ─ */

/* Register the service and `count` characteristics with the BLE host stack,
 * with a CCCD on each NOTIFY characteristic.  Called once. */
extern np_hub_status_t np_gatt_hal_register(const np_gatt_char_t *table, size_t count);

/* Notify every subscribed central.  Sends the WHOLE value or nothing: if the
 * negotiated ATT MTU cannot carry `len` bytes the platform must skip the
 * notification (the app reads the characteristic on connect and on its own
 * refresh), never truncate it — a truncated frame is a different frame. */
extern void np_gatt_hal_notify(uint16_t id, const uint8_t *data, size_t len);

#endif /* NP_GATT_SERVER_H */
