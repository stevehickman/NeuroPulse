/*
 * NeurOne Hub Control Program — Consumable Session Counts (OI-ACC-08, GitHub #381)
 * Document: NP-APP-ROADMAP-001 §5 (CONSUMABLE_STATUS 0x0007);
 *           docs/reference/commercial-model.md §2.3 (the Trigger column)
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * The producer behind CONSUMABLE_STATUS: one exposure count per consumable,
 * owned by the hub, reset by the app when the wearer replaces the part.
 *
 * ── The three rules OI-ACC-08 found unspecified (decided 2026-09-25, #381) ──
 *
 * (i) INCREMENT.  At the end of every session, each kind whose modality was
 *     DRIVEN in that session advances by one — the modality's bit in the
 *     session's mods_active_mask, which the runner sets only for a drive
 *     command its registry accepted (a stop, or a refused command, sets
 *     nothing).  An aborted session counts if the modality was driven before
 *     the abort: the part was exposed.
 *       kind 0  intranasal sleeves   ← NP_MOD_INTRANASAL
 *       kind 1  electrode hydrogel   ← NP_MOD_EEG        (8-ch semi-dry tips)
 *       kind 2  VNS clip pads        ← NP_MOD_VNS_HRV    (auricular clip)
 *       kind 3  audio cup foam       ← NP_MOD_AUDIO
 *     What each cannot see is the §2.3 row's business; one thing is this
 *     rule's own: the hub cannot sense the audio cups, so kind 3 counts
 *     sessions that drove audio, not sessions the cups were worn.
 *
 * (ii) EPOCH.  A count is "sessions since this part was last replaced".  A
 *     1-byte WRITE to 0x0007 naming the kind (0–3) zeroes it; the app sends it
 *     from Mark replaced.  The notification that follows carries the zero, so
 *     the app's absolute-overwrite (ConsumableTracker handleUpdatedCounts)
 *     agrees with the hub instead of restoring the old count.
 *
 * (iii) OWNER.  The hub.  The counts persist in the Config partition, so they
 *     survive an app reinstall, a second phone and a power cycle.
 *
 * ── Persistence ─────────────────────────────────────────────────────────────
 * NP_CFG_FILE_CONSUMABLES, REPLICATED (a count must not go backwards after a
 * torn write).  A session's increments are written from the runner's task at
 * session end, not left for a later poll: the intranasal sleeve's threshold is
 * one session, so an increment lost to an unplug straight after a session is
 * a missed prompt.  A reset is written by the next np_cons_poll().
 *
 * A store that is present but unreadable is NOT treated as zero — zero would
 * silently clear a pending sleeve block.  The module stays unloaded: READ
 * fails (the app keeps what it had), increments are held and applied once a
 * later poll loads the record, and resets are refused.
 *
 * ── Privacy ─────────────────────────────────────────────────────────────────
 * SHDR: "consumable session counts" (docs/reference/data-architecture-detail.md
 * §5.1) — unsigned counts, no timestamp.  The per-session modality mask they
 * are derived from is already in the SHDR session record.
 *
 * ── Concurrency ─────────────────────────────────────────────────────────────
 * Three callers: the runner task (session end), the BLE host task (READ, the
 * reset WRITE) and the heartbeat (poll).  RAM is mutated inside
 * np_transport_hal_enter/exit_critical; nothing blocks inside it.  Writes to
 * the store are serialised by a busy flag and a change sequence, so a slower
 * writer can never overwrite a newer value with an older snapshot.
 *
 * Pure C, host-testable (np_consumables_tests).
 */

#ifndef NP_CONSUMABLES_H
#define NP_CONSUMABLES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "np_hub_types.h"

#define NP_CONS_KIND_COUNT       4u
#define NP_CONS_WIRE_LEN         8u   /* READ/NOTIFY: 4 × u16 LE, kind order */
#define NP_CONS_RESET_LEN        1u   /* WRITE: the kind index               */
#define NP_CONS_COUNT_MAX        0xFFFFu  /* saturates; never wraps to zero  */

/* Persisted record: version, then 4 × u16 LE. */
#define NP_CONS_RECORD_VERSION   0x01u
#define NP_CONS_RECORD_LEN       (1u + (2u * NP_CONS_KIND_COUNT))

/* Load the persisted counts.  Absent (a new device) loads as zeros; present
 * but unreadable leaves the module unloaded and retried by np_cons_poll().
 * Call once at bring-up, before the heartbeat task starts. */
void np_cons_init(void);

/* A session ended; `mods_active_mask` is its np_session_uhdr_record_t mask.
 * Advances every kind whose modality bit is set, then persists at once. */
void np_cons_on_session_end(uint32_t mods_active_mask);

/* Once per heartbeat: load if not yet loaded, persist a pending change, and
 * notify CONSUMABLE_STATUS when the frame differs from the last one sent. */
void np_cons_poll(void);

/* ATT READ of 0x0007.  NP_HUB_ERR_NOT_PRESENT while unloaded; INVALID_ARG for
 * NULL or cap < NP_CONS_WIRE_LEN. */
np_hub_status_t np_cons_read(uint8_t *buf, size_t cap, size_t *len_out);

/* ATT WRITE of 0x0007: exactly one byte, a kind index 0–3.  Zeroes that count.
 * Refused (INVALID_ARG) for any other value; NP_HUB_ERR_NOT_PRESENT while
 * unloaded, so the app's write-with-response fails and it can retry. */
np_hub_status_t np_cons_on_reset_write(const uint8_t *data, size_t len);

#ifdef NPTEST_HOST
/* Host tests: forget all RAM state, as a reboot does. */
void np_cons_test_reset(void);
#endif

#endif /* NP_CONSUMABLES_H */
