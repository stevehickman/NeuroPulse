/*
 * NeurOne Zone Module Notification — socket-keyed module-status frames
 *
 * Supersedes the insertion-time bone-conduction announcement of NP-FW-ZA-001
 * Rev A §7–8. See the supersession note at the top of docs/np_fw_za_001.md.
 *
 * ── Why this unit exists ─────────────────────────────────────────────────────
 *
 * The old design announced "Frontal Left connected" through the mastoid
 * bone-conduction transducer at the instant a module was electrically seated.
 * Bone conduction only reaches the user while the transducer is pressed against
 * the head, and seating a module requires the helmet OFF the head — a worn head
 * is physically in the way of the socket. The confirmation therefore fired into
 * an empty room, at 5 zones and at 80 sockets alike.
 *
 * The confirmation is now delivered by the companion app, on the device the user
 * is actually holding. This unit is the producer side of that path: it turns
 * module insertion/removal events into the versioned, fragmented frames the
 * ZONE_MODULE_STATUS GATT characteristic carries, and hands each finished frame
 * to a transmit callback. It does not touch audio.
 *
 * The headset's own tone engine is NOT retired — it stays for announcements
 * delivered WHILE WORN (session-start configuration readback, in-session fault
 * alerts). See np_za_play_worn_cue() in np_zone_announce.h.
 *
 * ── Addressing ───────────────────────────────────────────────────────────────
 *
 * Events arrive socket-keyed, matching np_module_map's (socket_id : element_id)
 * scheme rather than the retired five-slot ZONE_ID ladder. Firmware socket ids
 * are 0-based (0 .. NP_HEXMAP_MAX_SOCKETS-1); the wire and the app are 1-based
 * (see the numbering-base note in np_module_map.h). This unit performs the +1 at
 * the encoder boundary — it is the boundary.
 *
 * ── Anatomy travels on the wire ──────────────────────────────────────────────
 *
 * Each record carries the socket's lobe and side. The socket lattice is
 * PROVISIONAL (NP-HEX-ZM-001 §3.4 — re-cut from 30 to 78 to 80 sockets already)
 * and the device holds the authoritative geometry table, so the app is never
 * given its own copy to drift out of sync with. The app speaks what the device
 * tells it a socket is called.
 *
 * ── Fragmentation ────────────────────────────────────────────────────────────
 *
 * A full snapshot of ~80 occupied sockets is ~324 bytes — far past the 20-byte
 * payload an ATT notification is guaranteed to carry. Frames therefore fragment:
 * each carries a fragment index, and the final one sets the LAST flag. The app
 * accumulates a snapshot and commits it only on LAST, so a partial sequence is
 * never mistaken for a complete inventory.
 *
 * ── Privacy ──────────────────────────────────────────────────────────────────
 *
 * Socket occupancy, module type, and module health are COMPONENT facts — SHDR
 * class, exactly as np_module_map.h classifies module UID. Nothing here is UHDR;
 * nothing is keyed to user identity.
 *
 * ── Class / testability ──────────────────────────────────────────────────────
 *
 * Pure C, no FreeRTOS and no HAL dependency — host-testable (np_zone_notify_tests).
 * IEC 62304 Class B (SW-02 hub control).
 */

#ifndef NP_ZONE_NOTIFY_H
#define NP_ZONE_NOTIFY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Frame format ─────────────────────────────────────────────────────────────
 *
 *   byte 0 : format version (NP_ZN_FORMAT_VERSION)
 *   byte 1 : flags (NP_ZN_FLAG_*)
 *   byte 2 : fragment index, 0-based
 *   byte 3 : record count in THIS fragment
 *   byte 4+: records, NP_ZN_RECORD_BYTES each
 *
 * Record layout:
 *   byte 0 : socket id, 1-BASED (1 .. NP_ZN_MAX_SOCKET_ID); 0 is never emitted
 *   byte 1 : module type (np_zn_module_type_t)
 *   byte 2 : bits [2:0] lobe, bits [4:3] side  (np_zn_lobe_t / np_zn_side_t)
 *   byte 3 : record flags (NP_ZN_REC_*)
 *
 * Version 1 is the first socket-keyed format. The retired 5-byte
 * one-byte-per-slot payload had no version byte at all, so a hub still speaking
 * it decodes as version 0 and is rejected rather than misread.
 */

#define NP_ZN_FORMAT_VERSION   0x01u
#define NP_ZN_HEADER_BYTES     4u
#define NP_ZN_RECORD_BYTES     4u

/* Largest 1-based socket id the wire can carry. Firmware ids are 0-based over
 * NP_HEXMAP_MAX_SOCKETS (128), so the 1-based domain is 1..128 — which is
 * exactly a uint8_t's usable range above zero. Pinned by a static assert in
 * np_zone_notify.c against NP_HEXMAP_MAX_SOCKETS. */
#define NP_ZN_MAX_SOCKET_ID    128u

/* Smallest frame buffer that can hold a header plus one record. */
#define NP_ZN_MIN_FRAME_BYTES  (NP_ZN_HEADER_BYTES + NP_ZN_RECORD_BYTES)

/* The payload an ATT notification carries on the default 23-byte MTU. Any
 * transmit path must accept at least this much; the encoder is exercised at
 * exactly this size in the host tests. */
#define NP_ZN_ATT_FLOOR_BYTES  20u

/* ── Frame flags (byte 1) ─────────────────────────────────────────────────── */

/* This fragment belongs to a full-inventory snapshot. Clear = incremental delta
 * naming only the sockets that changed. */
#define NP_ZN_FLAG_SNAPSHOT    0x01u
/* Final fragment of this frame sequence. A snapshot is committed by the app only
 * when a fragment carrying this flag arrives. */
#define NP_ZN_FLAG_LAST        0x02u

/* ── Record flags (record byte 3) ─────────────────────────────────────────── */

/* A module is seated and confirmed in this socket. Clear = socket is empty
 * (a removal, or an empty socket in a snapshot). */
#define NP_ZN_REC_PRESENT      0x01u
/* The module in this socket failed identification (unrecognised type, bad
 * contact, failed debounce). PRESENT is clear whenever FAULT is set: an
 * unidentified module is never reported as usable. */
#define NP_ZN_REC_FAULT        0x02u

/* ── Module type ──────────────────────────────────────────────────────────────
 * Tile types from NP-HEX-ZM-001 §4a. Values are frozen once shipped on the wire
 * — append only, never renumber. */

typedef enum {
    NP_ZN_MODULE_NONE       = 0,  /* socket empty                              */
    NP_ZN_MODULE_PBM_BASE   = 1,  /* T1-A — 660/808 nm base PBM tile           */
    NP_ZN_MODULE_EEG        = 2,  /* T1-B — dual-rated electrode + reduced PBM */
    NP_ZN_MODULE_PBM_1064   = 3,  /* T1-C — 1064 nm smart PBM tile             */
    NP_ZN_MODULE_PBM_1170   = 4,  /* T2-D — 1170 nm deep-PBM laser tile        */
    NP_ZN_MODULE_UNKNOWN    = 255 /* seated but not identified                 */
} np_zn_module_type_t;

/* ── Anatomy ──────────────────────────────────────────────────────────────────
 * Mirrors np_lobe_t / np_side_t in np_module_map.h. Duplicated rather than
 * included so this unit stays free of the hub_control include path and can be
 * host-compiled standalone; np_zone_notify.c static-asserts the two agree. */

typedef enum {
    NP_ZN_LOBE_NONE      = 0,
    NP_ZN_LOBE_FRONTAL   = 1,
    NP_ZN_LOBE_TEMPORAL  = 2,
    NP_ZN_LOBE_PARIETAL  = 3,
    NP_ZN_LOBE_OCCIPITAL = 4,
} np_zn_lobe_t;

typedef enum {
    NP_ZN_SIDE_MIDLINE = 0,   /* straddles the midline — in BOTH hemisphere zones */
    NP_ZN_SIDE_LEFT    = 1,
    NP_ZN_SIDE_RIGHT   = 2,
} np_zn_side_t;

/* ── One socket's state, as the caller reports it ─────────────────────────── */

typedef struct {
    uint8_t             socket_id;   /* 0-BASED firmware id; encoder emits +1   */
    np_zn_module_type_t module_type;
    np_zn_lobe_t        lobe;
    np_zn_side_t        side;
    bool                present;
    bool                fault;
} np_zn_socket_state_t;

/* ── Status ───────────────────────────────────────────────────────────────── */

typedef enum {
    NP_ZN_OK               =  0,
    NP_ZN_ERR_INVALID_ARG  = -1,
    NP_ZN_ERR_FRAME_TOO_SMALL = -2,  /* max_frame_bytes < NP_ZN_MIN_FRAME_BYTES */
    NP_ZN_ERR_SOCKET_RANGE = -3,     /* socket id outside the addressable domain */
    NP_ZN_ERR_TX           = -4,     /* transmit callback reported failure       */
} np_zn_status_t;

/* ── Transmit callback ────────────────────────────────────────────────────────
 * Called once per finished fragment, in order. `len` is always
 * ≥ NP_ZN_HEADER_BYTES and ≤ the max_frame_bytes given at init. Return false to
 * abort the sequence (the encoder stops and returns NP_ZN_ERR_TX). The buffer is
 * owned by the encoder and is valid only for the duration of the call. */
typedef bool (*np_zn_tx_fn)(const uint8_t *frame, uint8_t len, void *ctx);

/* ── API ──────────────────────────────────────────────────────────────────── */

/*
 * np_zone_notify_init — bind the transmit callback and the per-frame size cap.
 *
 *   max_frame_bytes : largest frame the transport will accept. Must be
 *                     ≥ NP_ZN_MIN_FRAME_BYTES; pass the negotiated ATT payload
 *                     (NP_ZN_ATT_FLOOR_BYTES when unknown).
 *
 * Returns NP_ZN_ERR_FRAME_TOO_SMALL if the cap cannot hold one record.
 */
np_zn_status_t np_zone_notify_init(np_zn_tx_fn tx, void *tx_ctx,
                                   uint8_t max_frame_bytes);

/*
 * np_zone_notify_event — report ONE socket's change as an incremental delta.
 *
 * Emits a single non-snapshot frame with the LAST flag set. This is the call the
 * insertion/removal detector makes; it replaces the bone-conduction announcement
 * that used to fire here.
 *
 * A state with fault set is emitted with PRESENT clear regardless of the
 * caller's present flag — an unidentified module is never announced as usable.
 */
np_zn_status_t np_zone_notify_event(const np_zn_socket_state_t *state);

/*
 * np_zone_notify_snapshot — emit the full inventory as an ordered fragment run.
 *
 * Every fragment carries NP_ZN_FLAG_SNAPSHOT; the final one adds NP_ZN_FLAG_LAST.
 * A zero-length snapshot still emits one empty header-only fragment, so the app
 * can distinguish "helmet reports no modules" from "nothing arrived".
 *
 * Sent on connection and after any event the hub could not deliver incrementally.
 */
np_zn_status_t np_zone_notify_snapshot(const np_zn_socket_state_t *states,
                                       uint16_t count);

/*
 * np_zone_notify_encode_record — write one record into buf (≥ NP_ZN_RECORD_BYTES).
 * Exposed for host tests and for callers assembling frames themselves.
 * Returns NP_ZN_ERR_SOCKET_RANGE if socket_id ≥ NP_ZN_MAX_SOCKET_ID.
 */
np_zn_status_t np_zone_notify_encode_record(const np_zn_socket_state_t *state,
                                            uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif /* NP_ZONE_NOTIFY_H */
