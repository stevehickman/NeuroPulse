/*
 * NeurOne Hub Control Program — Cervical VNS Offline-Fault Summary
 * Document: NP-SW-FAULTMSG-001 Rev 3 §9.6 (P3); NP-APP-ROADMAP-001 §5
 *           (CVNS_FAULT_STATUS 0x0014, CVNS_REENABLE_CONFIRM 0x0015,
 *           ACTIVE_USER 0x0016)
 *
 * What the hub tells the app, when it connects, about cervical VNS faults that
 * happened while no phone was connected — and the two writes the app answers
 * with.  In Mode 3 the device's only fault signal is the red LED (CLAUDE.md
 * §4.7); this is what turns that into "your heart rate changed" or "the left
 * gel pad lost contact" when the phone next connects.
 *
 * ── What this module owns ─────────────────────────────────────────────────────
 *   - A small persisted list of the most recent cervical fault stops, each
 *     tagged with the user who was named when it happened (np_cvfs_record_fault).
 *   - The hub's copy of the active user tag, persisted with it, so a fault in a
 *     Mode 3 session after a power cycle is still attributed to someone.
 *   - The CVNS_FAULT_STATUS frame builder (np_cvfs_build_frame) — byte-for-byte
 *     the frame the apps parse (CervicalFaultStatus.swift / .kt).
 *   - The ATT write handlers for 0x0015 and 0x0016.
 *   - Change detection: np_cvfs_poll() rebuilds the frame every heartbeat and
 *     hands it to the transport only when it changed.
 *
 * ── What it does NOT own ─────────────────────────────────────────────────────
 *   - Enforcement.  The safety MCU holds a cardiac cutoff per user across power
 *     loss on its own (NP-FW-CVNS-001 §5.4.1).  Nothing here can release
 *     stimulation; a lost or corrupt summary costs an explanation, not a
 *     control.
 *   - The BLE GATT server and the storage glue — the three np_cvfs_hal_* seams
 *     below, owed by the platform (OI-WA-03, OI-LFS-05), the same shape as
 *     np_transport.h's producer seam.
 *
 * ── Per-user scope (principal decision 2026-09-22) ──────────────────────────
 * A frame carries the ACTIVE user's records plus the unattributed ones (a fault
 * recorded while no user was named).  The unattributed ones are shown to
 * whoever is named, mirroring the safety MCU, where the person who confirms the
 * re-enable owns them.  Another named user's records are never in the frame.
 *
 * ── Privacy ──────────────────────────────────────────────────────────────────
 * UHDR-class: fault kinds and pad sides from the user's own sessions
 * (docs/reference/data-architecture-detail.md §5.1).  No heart-rate value and
 * no timestamp is stored or sent: a device session counter orders records.
 * Persisted to the UHDR partition only (np_cvfs_hal_save); never SHDR.
 *
 * ── Concurrency ──────────────────────────────────────────────────────────────
 * Mutators run in three tasks (the cervical module's session callback, the BLE
 * write handler, the heartbeat).  Every RAM mutation is inside the task-level
 * critical section np_transport_hal_enter/exit_critical.  Nothing blocking runs
 * inside it: persistence happens in np_cvfs_poll() from a snapshot.
 *
 * IEC 62304 Class B (SW-02).  Pure C, host-testable (np_cvns_fault_summary_tests).
 */

#ifndef NP_CVNS_FAULT_SUMMARY_H
#define NP_CVNS_FAULT_SUMMARY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "np_hub_types.h"
#include "np_cvns_types.h"

/* ── Wire format: CVNS_FAULT_STATUS (4 + 8n bytes, n ≤ 4) ────────────────────
 *   [0] version 0x01   [1] hub re-enable state (np_cvns_reenable_state_t, 0–4)
 *   [2] n              [3] flags — bit 0 active user withheld, bit 1 someone
 *                          on this device has an outstanding cardiac cutoff
 *   per record, oldest first: u32 LE device session counter · u8 fault kind
 *   (np_cvns_fault_reason_t 1–5) · u8 pad side mask (bit 0 left, bit 1 right;
 *   pad-contact faults only) · u16 reserved 0                                  */
#define NP_CVFS_WIRE_VERSION        0x01u
#define NP_CVFS_HEADER_LEN          4u
#define NP_CVFS_RECORD_LEN          8u
#define NP_CVFS_MAX_WIRE_RECORDS    4u
#define NP_CVFS_FRAME_MAX           (NP_CVFS_HEADER_LEN + \
                                     (NP_CVFS_MAX_WIRE_RECORDS * NP_CVFS_RECORD_LEN))

#define NP_CVFS_SIDE_LEFT           0x01u
#define NP_CVFS_SIDE_RIGHT          0x02u
#define NP_CVFS_FLAGS_MASK          0x03u   /* bits 0–1 of np_safety_nv_report_t */

/* Records kept across ALL users.  Eight covers four each for two people
 * sharing a device; older ones age out.  A record aged out before it was read
 * costs an explanation only — the safety MCU still holds the cutoff. */
#define NP_CVFS_STORE_RECORDS       8u

/* Persisted blob: version, current user, count, entries, CRC-32. */
#define NP_CVFS_BLOB_VERSION        0x01u
#define NP_CVFS_BLOB_ENTRY_LEN      10u     /* tag u32 · counter u32 · kind · mask */
#define NP_CVFS_BLOB_MAX            (1u + 4u + 1u + \
                                     (NP_CVFS_STORE_RECORDS * NP_CVFS_BLOB_ENTRY_LEN) + 4u)

/* ── API ─────────────────────────────────────────────────────────────────────── */

/*
 * np_cvfs_init — load the persisted list (np_cvfs_hal_load).  A missing, short,
 * wrong-version or CRC-failing blob starts EMPTY with no user named: the summary
 * is information, and a half-believed record would be wrong information.  Call
 * once at bring-up, before the heartbeat task starts.
 */
void np_cvfs_init(void);

/*
 * np_cvfs_record_fault — a cervical session stopped on `kind`.  Attributed to
 * the current user (0 = unattributed).  `side_mask` is kept for
 * NP_CVNS_FAULT_IMPEDANCE only and forced to 0 otherwise.  NP_CVNS_FAULT_NONE
 * is rejected (NP_HUB_ERR_INVALID_ARG).  Marks the list for persistence on the
 * next np_cvfs_poll().
 */
np_hub_status_t np_cvfs_record_fault(uint32_t               session_counter,
                                     np_cvns_fault_reason_t kind,
                                     uint8_t                side_mask);

/*
 * np_cvfs_pad_side_mask — which pads a pad-contact fault names, from the
 * session record's per-side impedances: a side whose impedance exceeds
 * NP_CVNS_IMPEDANCE_MAX_KOHM, or is not a finite positive number, is failing.
 * If neither side is out of window the fault still happened, so both are named.
 */
uint8_t np_cvfs_pad_side_mask(float left_kohm, float right_kohm);

/* The user tag faults are attributed to; 0 until the app names someone. */
uint32_t np_cvfs_current_user(void);

/*
 * np_cvfs_set_user — the heartbeat has forwarded `tag` to the safety MCU
 * (between sessions).  From now on, faults are attributed to it, and the frame
 * carries its records.  Persisted on the next poll.  0 and 0xFFFFFFFF ignored.
 */
void np_cvfs_set_user(uint32_t tag);

/*
 * np_cvfs_build_frame — build CVNS_FAULT_STATUS for the current user.
 * `nv_valid` false (no valid safety-MCU report yet) sends flags 0: the flags
 * drive a warning, and the safety MCU enforces regardless.
 *   NP_HUB_OK / NP_HUB_ERR_INVALID_ARG (NULL, or cap < NP_CVFS_FRAME_MAX)
 */
np_hub_status_t np_cvfs_build_frame(uint8_t  reenable_state,
                                    bool     nv_valid,
                                    uint8_t  nv_flags,
                                    uint8_t *buf,
                                    size_t   cap,
                                    size_t  *len_out);

/*
 * np_cvfs_poll — call once per heartbeat.  Persists a pending change
 * (np_cvfs_hal_save, outside the critical section), rebuilds the frame, and
 * calls np_cvfs_hal_notify() only when it differs from the last one published.
 * A failed save is retried on the next poll.
 */
void np_cvfs_poll(uint8_t reenable_state, bool nv_valid, uint8_t nv_flags);

/*
 * np_cvfs_read — the last frame np_cvfs_poll() built, for an ATT READ.
 * Returns NP_HUB_ERR_INVALID_ARG for NULL or cap < NP_CVFS_FRAME_MAX.
 * Before the first poll it returns an empty frame (state idle, n 0, flags 0).
 */
np_hub_status_t np_cvfs_read(uint8_t *buf, size_t cap, size_t *len_out);

/*
 * ATT write handlers — the BLE stack calls these with the written value.
 * Return value maps to the ATT response: NP_HUB_OK → write accepted; anything
 * else → an application error, so the app's write-with-response fails.
 *
 * np_cvfs_on_active_user_write (0x0016): exactly 4 bytes, little-endian tag,
 *   not 0 (unspecified) nor 0xFFFFFFFF (any).  Posts the tag via
 *   np_hub_set_active_user(); the heartbeat forwards it to the safety MCU
 *   between sessions and then calls np_cvfs_set_user().
 * np_cvfs_on_reenable_confirm_write (0x0015): exactly 1 byte, 0x01.  Forwards
 *   to np_hub_cvns_reenable_confirm(), which accepts it only while awaiting a
 *   confirmation (REQ-CVNS-09).
 */
np_hub_status_t np_cvfs_on_active_user_write(const uint8_t *data, size_t len);
np_hub_status_t np_cvfs_on_reenable_confirm_write(const uint8_t *data, size_t len);

/* ── Hub entry points the write handlers call (defined in np_hub_control_main.c) */
np_hub_status_t np_hub_set_active_user(uint32_t user_tag);
np_hub_status_t np_hub_cvns_reenable_confirm(void);

/* ── Platform seams (OI-WA-03 BLE GATT server; OI-LFS-05 UHDR storage) ────────
 * Trap-defined in firmware/platform/src/np_platform_stub.c until the drivers
 * exist; host-modeled in the tests.                                           */

/* Load the persisted blob from the UHDR partition.  NP_HUB_OK with *len_out =
 * 0 means "nothing stored yet". */
extern np_hub_status_t np_cvfs_hal_load(uint8_t *buf, size_t cap, size_t *len_out);

/* Replace the persisted blob, durably (write + sync).  UHDR partition only. */
extern np_hub_status_t np_cvfs_hal_save(const uint8_t *buf, size_t len);

/* Notify subscribed centrals of a new CVNS_FAULT_STATUS value.  No-op when
 * nothing is connected or subscribed; the app also READs on connect. */
extern void np_cvfs_hal_notify(const uint8_t *frame, size_t len);

#endif /* NP_CVNS_FAULT_SUMMARY_H */
