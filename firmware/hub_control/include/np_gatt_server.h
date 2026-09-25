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
 * 0x0001 is the service.  These are the strings both apps already carry
 * (GATTCharacteristics.swift NPUUID, GattUuids.kt); np_gatt_uuid128() builds
 * the 16 bytes from the id, so the base is written once.
 *
 * ── What is published: only what the hub produces ───────────────────────────
 * A characteristic is in the table only if a hub function answers it.  The
 * apps know more UUIDs than this (session state, HRV, OTA, consumables …);
 * publishing those with nothing behind them would let the app believe it had
 * found a working hub, and it is the same false-green the platform layer's
 * traps exist to refuse.  Each is added with its producer.  Notably absent:
 *   - CONSUMABLE_STATUS (0x0007) — no increment rule or reset path (OI-ACC-08)
 *   - CVNS_PAD_STATUS  (0x0013) — the electrode-to-side mapping it reports is
 *     an open hardware item (OI-ACC-07 → OI-CVNSHW-01, -06)
 *   - the protocol upload (0x0008) — np_transport_feed() is its handler, and
 *     binding it is a separate change (OI-HUB-MAIN-01)
 * scripts/check-consumable-triggers.ts fails the build if either of the first
 * two appears in firmware without its §2.3 row being updated.
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

/* ── Ids (the XXXX of the UUID) ──────────────────────────────────────────────── */
#define NP_GATT_ID_SERVICE                0x0001u
#define NP_GATT_ID_WARRANTY_TOKEN         0x0010u  /* READ 32 B — OI-WA-03        */
#define NP_GATT_ID_CVNS_FAULT_STATUS      0x0014u  /* READ/NOTIFY 4+8n — FAULTMSG */
#define NP_GATT_ID_CVNS_REENABLE_CONFIRM  0x0015u  /* WRITE 1 B                   */
#define NP_GATT_ID_ACTIVE_USER            0x0016u  /* WRITE 4 B                   */

/* ── Properties ──────────────────────────────────────────────────────────────── */
#define NP_GATT_PROP_READ     0x01u
#define NP_GATT_PROP_WRITE    0x02u   /* write with response */
#define NP_GATT_PROP_NOTIFY   0x04u

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
    /* Application error: the hub refused the value (a write outside the state
     * that accepts it, or a value its handler rejects).  The app's
     * write-with-response fails, which is what np_cvns_fault_summary.h's
     * handlers are documented to produce. */
    NP_ATT_APP_REFUSED            = 0x80,
    /* Application error: the value exists on the device but cannot be produced
     * now (a stored record unreadable).  Retrying may succeed. */
    NP_ATT_APP_UNAVAILABLE        = 0x81,
} np_att_status_t;

typedef np_hub_status_t (*np_gatt_read_fn)(uint8_t *buf, size_t cap, size_t *len_out);
typedef np_hub_status_t (*np_gatt_write_fn)(const uint8_t *data, size_t len);

typedef struct {
    uint16_t          id;
    uint8_t           props;     /* NP_GATT_PROP_* */
    uint8_t           max_len;   /* bytes; also the exact length of every write */
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
 * fixed-length: a value of any other length is refused before the handler
 * sees it.  Prepared (long) writes are not supported, and are not needed.
 */
np_att_status_t np_gatt_on_write(uint16_t id, const uint8_t *data, size_t len);

/*
 * np_gatt_notify — send `len` bytes as a notification of `id`.  Refuses an id
 * without NOTIFY and an over-length value (NP_HUB_ERR_INVALID_ARG).  A no-op
 * at the platform when nothing is subscribed.
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
