/*
 * NeurOne Zone Module Notification — socket map + socket-keyed status frames
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
 * is actually holding. This unit is the producer side of that path. It does not
 * touch audio.
 *
 * The headset's own tone engine is NOT retired — it stays for announcements
 * delivered WHILE WORN (session-start configuration readback, in-session fault
 * alerts). See np_za_play_worn_cue() in np_zone_announce.h.
 *
 * ── Two characteristics, two shapes: the static/dynamic split ────────────────
 *
 * A socket's physical location is a PERMANENT property of the helmet. It does
 * not change when a module is inserted or removed, so it does not belong in a
 * change notification.
 *
 *   SOCKET MAP  — read ONCE when the app links to the helmet. The authoritative
 *                 description of where every socket physically IS: its position
 *                 in 3-space (see the coordinate frame below) and whether the
 *                 shell wires it at all. The helmet is the single source of
 *                 truth for this and the app never keeps its own copy of the
 *                 lattice.
 *
 *   STATUS      — emitted on every insertion/removal/fault. Carries ONLY what
 *                 actually changed: socket id, module type, flags. Three bytes.
 *
 * The payoff of the split is that the map can grow — 10-20 registration,
 * per-socket element counts, surface normals, whatever later needs — WITHOUT
 * changing the size of anything sent on a hot path. Static data is paid for once
 * per link; dynamic data stays minimal forever.
 *
 * ── Addressing ───────────────────────────────────────────────────────────────
 *
 * Both shapes are socket-keyed, matching np_module_map's (socket_id : element_id)
 * scheme rather than the retired five-slot ZONE_ID ladder. Firmware socket ids
 * are 0-based (0 .. NP_HEXMAP_MAX_SOCKETS-1); the wire and the app are 1-based
 * (see the numbering-base note in np_module_map.h). This unit performs the +1 at
 * the encoder boundary — it is the boundary.
 *
 * ── Fragmentation ────────────────────────────────────────────────────────────
 *
 * Neither a full socket map (~80 × 7 bytes) nor a full status snapshot fits the
 * 20-byte payload an ATT notification is guaranteed to carry. Frames therefore
 * fragment: each carries a fragment index, and the final one sets the LAST flag.
 * The app accumulates and commits only on LAST, so a partial sequence is never
 * mistaken for a complete inventory.
 *
 * ── Privacy ──────────────────────────────────────────────────────────────────
 *
 * Socket geometry is a device property; socket occupancy, module type and module
 * health are COMPONENT facts — SHDR class, exactly as np_module_map.h classifies
 * module UID. Nothing here is UHDR; nothing is keyed to user identity.
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

/* ── Common frame header ──────────────────────────────────────────────────────
 *
 *   byte 0 : format version (NP_ZN_FORMAT_VERSION)
 *   byte 1 : flags (NP_ZN_FLAG_*), including the frame KIND bit
 *   byte 2 : fragment index, 0-based
 *   byte 3 : record count in THIS fragment
 *   byte 4+: records — NP_ZN_STATUS_REC_BYTES or NP_ZN_MAP_REC_BYTES each,
 *            according to the kind bit
 *
 * Version 2 is the static/dynamic split: status records dropped from 4 bytes to
 * 3 when anatomy moved into the socket map. Version 1 (status records carrying
 * lobe|side inline) was never deployed to hardware. Version 0 is the retired
 * 5-byte one-byte-per-slot payload, which had no version byte at all — so a hub
 * still speaking it decodes as version 0 and is rejected rather than misread.
 */

#define NP_ZN_FORMAT_VERSION    0x02u
#define NP_ZN_HEADER_BYTES      4u

/* Status record: socket id (1-based), module type, flags. */
#define NP_ZN_STATUS_REC_BYTES  3u

/* Map record: socket id (1-based), flags, then x_mm, y_mm, z_mm as int16
 * little-endian. See the coordinate frame note below. */
#define NP_ZN_MAP_REC_BYTES     8u

/* Largest 1-based socket id the wire can carry. Firmware ids are 0-based over
 * NP_HEXMAP_MAX_SOCKETS (128), so the 1-based domain is 1..128 — which is
 * exactly a uint8_t's usable range above zero. Pinned by a static assert in
 * np_zone_notify.c against NP_HEXMAP_MAX_SOCKETS. */
#define NP_ZN_MAX_SOCKET_ID     128u

/* Smallest frame buffer that can hold a header plus one record of either kind. */
#define NP_ZN_MIN_FRAME_BYTES   (NP_ZN_HEADER_BYTES + NP_ZN_MAP_REC_BYTES)

/* The payload an ATT notification carries on the default 23-byte MTU. Any
 * transmit path must accept at least this much; the encoder is exercised at
 * exactly this size in the host tests. */
#define NP_ZN_ATT_FLOOR_BYTES   20u

/* ── Frame flags (byte 1) ─────────────────────────────────────────────────── */

/* This fragment belongs to a full snapshot. Clear = incremental delta naming
 * only the sockets that changed. Socket-map frames are always snapshots. */
#define NP_ZN_FLAG_SNAPSHOT     0x01u
/* Final fragment of this frame sequence. A snapshot is committed by the app only
 * when a fragment carrying this flag arrives. */
#define NP_ZN_FLAG_LAST         0x02u
/* Frame KIND: set = socket-map records, clear = status records. The two shapes
 * differ in size, so a decoder that assumed the wrong one would misparse rather
 * than fail. The kind is on the wire so it can be checked, not inferred from
 * which characteristic happened to deliver it. */
#define NP_ZN_FLAG_MAP          0x04u

/* ── Status record flags (status record byte 2) ───────────────────────────── */

/* A module is seated and confirmed in this socket. Clear = socket is empty
 * (a removal, or an empty socket in a snapshot). */
#define NP_ZN_REC_PRESENT       0x01u
/* The module in this socket failed identification (unrecognised type, bad
 * contact, failed debounce). PRESENT is clear whenever FAULT is set: an
 * unidentified module is never reported as usable. */
#define NP_ZN_REC_FAULT         0x02u

/* ── Map record flags (map record byte 1) ─────────────────────────────────── */

/* This socket is physically wired in this shell. Clear = the address exists in
 * the geometry table but the shell does not populate it. */
#define NP_ZN_MAP_WIRED         0x01u

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

/* ── Coordinate frame ─────────────────────────────────────────────────────────
 *
 * A socket's PHYSICAL LOCATION is its position in 3-space, in millimetres, in
 * AIRCRAFT BODY AXES, with the origin at the centre of the shell the helmet
 * forms — approximately an oblate spheroid, though see the datum note below:
 *
 *     +x  forward   (toward the face)
 *     +y  right     (toward the right ear)
 *     +z  down      (toward the neck)
 *
 * Right-handed — the same convention an aircraft uses for fore/aft, right/left
 * and up/down. The vault the sockets tile sits ABOVE the origin, so every socket
 * has z <= 0 and the crown is the most negative z.
 *
 * ── There is no lobe and no side on this wire ────────────────────────────────
 *
 * Neither is a property of the hardware, and nothing in the system derives one
 * (ZONE-1, 2026-07-30). A socket's anatomical meaning is its ZONE MEMBERSHIP,
 * authored by a human in protocols/predefined/00-zones.npps, where zones are
 * arbitrary, overlapping, user-extensible sets of sockets. The helmet does not
 * know about zones and must not pretend to: it reports where each socket
 * physically is, and the app resolves that to whatever zones name it.
 *
 * Coordinates rather than an index because a position is checkable against the
 * shell, survives a lattice re-cut with its meaning intact, and makes no claim
 * the hardware is not entitled to make.
 *
 * ── Datum: the values are interim, the FRAME is not ──────────────────────────
 *
 * The frame above is settled. The numbers filling it are not: today they come
 * from an ellipsoid fitted to the measured extents, and the principal has ruled
 * (2026-07-31) that the ellipsoid is a convenient DESCRIPTION rather than a fact
 * about the shell. The reference of record is the measured interior scan
 * (cad/HelmetScan.3dm) until exact CAD drawings exist.
 *
 * This wire format does not change when that datum does — which is the point of
 * sending coordinates. A socket map re-derived from the scan is the same eight
 * bytes per socket with better numbers in them. See the header of
 * scripts/sync-socket-map.ts for what blocks that move. */

/* ── One socket's PERMANENT description (socket map) ───────────────────────── */

typedef struct {
    uint8_t socket_id;   /* 0-BASED firmware id; encoder emits +1               */
    int16_t x_mm;        /* forward positive                                     */
    int16_t y_mm;        /* right positive                                       */
    int16_t z_mm;        /* DOWN positive — vault sockets are negative           */
    bool    wired;       /* shell physically populates this socket               */
} np_zn_socket_desc_t;

/* ── One socket's CHANGEABLE state (status) ───────────────────────────────── */

typedef struct {
    uint8_t             socket_id;   /* 0-BASED firmware id; encoder emits +1   */
    np_zn_module_type_t module_type;
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
 * owned by the encoder and is valid only for the duration of the call.
 *
 * `is_map` tells the transport which characteristic the fragment belongs on:
 * true = SOCKET_MAP, false = ZONE_MODULE_STATUS. */
typedef bool (*np_zn_tx_fn)(const uint8_t *frame, uint8_t len, bool is_map,
                            void *ctx);

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
 * np_zone_notify_socket_map — publish the helmet's permanent socket geometry.
 *
 * Sent ONCE when the app links (and again only if the geometry table itself is
 * replaced, e.g. after an OTA that re-cuts the lattice). Every fragment carries
 * SNAPSHOT|MAP; the final one adds LAST.
 *
 * This is what removed anatomy from the status hot path: the app learns where
 * socket 12 is exactly once, then every later change about socket 12 costs three
 * bytes. Adding fields here (10-20 labels, element counts) does not grow any
 * status frame.
 */
np_zn_status_t np_zone_notify_socket_map(const np_zn_socket_desc_t *descs,
                                         uint16_t count);

/*
 * np_zone_notify_event — report ONE socket's change as an incremental delta.
 *
 * Emits a single status frame with the LAST flag set and SNAPSHOT clear. This is
 * the call the insertion/removal detector makes; it replaces the bone-conduction
 * announcement that used to fire here.
 *
 * A state with fault set is emitted with PRESENT clear regardless of the
 * caller's present flag — an unidentified module is never announced as usable.
 */
np_zn_status_t np_zone_notify_event(const np_zn_socket_state_t *state);

/*
 * np_zone_notify_snapshot — emit the full occupancy inventory as an ordered
 * fragment run. Every fragment carries SNAPSHOT (MAP clear); the final one adds
 * LAST.
 *
 * A zero-length snapshot still emits one empty header-only fragment, so the app
 * can distinguish "helmet reports no modules" from "nothing arrived".
 *
 * Sent on connection and after any event the hub could not deliver incrementally.
 */
np_zn_status_t np_zone_notify_snapshot(const np_zn_socket_state_t *states,
                                       uint16_t count);

/*
 * Encode one record of each kind into buf (≥ the matching *_REC_BYTES).
 * Exposed for host tests and for callers assembling frames themselves.
 * Return NP_ZN_ERR_SOCKET_RANGE if socket_id ≥ NP_ZN_MAX_SOCKET_ID.
 */
np_zn_status_t np_zone_notify_encode_status(const np_zn_socket_state_t *state,
                                            uint8_t *buf);
np_zn_status_t np_zone_notify_encode_map(const np_zn_socket_desc_t *desc,
                                         uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif /* NP_ZONE_NOTIFY_H */
