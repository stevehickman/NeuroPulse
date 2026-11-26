/*
 * NeurOne Hub Control Program — per-module running totals (REQ-FWHUB-93)
 * Document: NP-FW-HUB-001 §6.11; NP-FW-NVRAM-001 §8.5 (the principal, 2026-11-23,
 *           amended 2026-11-24); docs/reference/data-architecture-detail.md §5.1
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * Every module the helmet has seen keeps running totals of its own wear, so its
 * place in its warranty lifetime is known and fleet maintenance can model it:
 *
 *   sessions_present, present_s   sessions the module was fitted for, and how
 *                                 long they ran.  Presence is exposure: hair,
 *                                 skin oil and sweat reach a fitted module
 *                                 whether or not it is driven.
 *   sessions_used, used_s         sessions the module was driven in, and for
 *                                 how long.  The count measures on/off and ramp
 *                                 cycles; the time measures drive, because
 *                                 sessions differ in length.
 *   faults                        how many times each fault code was logged
 *                                 against the module (four codes, then a count
 *                                 of any other).
 *   mate_cycles                   insertions the hub saw (a hot-plugged
 *                                 accessory; a tile, once the inventory poll
 *                                 decides mates under NP-MOD-ID-001 MODID-4a).
 *   dose_accum                    the metered optical dose of a tile, mJ/cm²,
 *                                 summed over its wavelengths.
 *   throttle_count                sessions in which a tile's thermal throttle
 *                                 tripped (the dose meter latches it once per
 *                                 socket per session).
 *   fault_flags                   sticky bits; bit 0 = closed at least once
 *                                 with the module no longer seated
 *                                 (NP-FW-NVRAM-001 §6.5.1, D-29).
 *   detail_lost                   1 when this row is known to fall short: it
 *                                 was created by giving up another module's
 *                                 row, or a commit that should have added to
 *                                 it was dropped (NP-FW-NVRAM-001 §6.3).
 *
 * This table IS Map 3 (NP-FW-NVRAM-001 §8.5, OI-NVRAM-20): one running-total row
 * per module, upserted.  The per-session journal (map3.jrn, np_map3_record) it
 * replaced was retired before a row was written, and its codec is removed.
 *
 * Who may see which field (§8.5, §8.1): the person's app reads every field at
 * a download and keeps the previous snapshot in Map 2.  The fleet gets the
 * totals under the upload's month anchor, and of those dose_accum and
 * throttle_count only under NP-MOD-ID-001 §7's opt-in; fault_flags bit 0
 * never leaves the app (§6.5.1).  The hub side of that upload is not built.
 *
 * ── Totals, never sessions (CLAUDE.md §5.1; NP-FW-NVRAM-001 §8.5) ──────────
 * One fixed-size record of every module's totals, upserted at session end
 * (NP_CFG_FILE_MODULE_USAGE, REPLICATED).  No row is ever appended, nothing is
 * stamped with a session or a time, and nothing is written to the SHDR log here:
 * a snapshot written per session would let the difference between two
 * snapshots say which module was used in which session, which is the
 * treatment's montage (UHDR).  The totals are SHDR, and reach the fleet only as
 * the upload's snapshot under its month anchor (§8.5), so the finest view
 * anyone but the person's own app has is the growth between two uploads.
 *
 * ── Which module a row belongs to ──────────────────────────────────────────
 *   NP_MU_KEY_UID   the module reports its own UID (the PBM tiles).  The row
 *                   follows the part from socket to socket.
 *   NP_MU_KEY_PORT  the accessory reports no identity the hub can read
 *                   (cervical and auricular VNS, intranasal probe, goggles,
 *                   audio, the stimulation electrodes, the T2 accessories).
 *                   The row is keyed by the slot and module type, and counts
 *                   whatever accessory was in that port.  A part-level total
 *                   for an accessory needs an identity on the accessory:
 *                   OI-FWHUB-38.
 *
 * ── Concurrency ────────────────────────────────────────────────────────────
 * The stored totals are touched only by the boot path and the runner task
 * (np_mu_load, np_mu_session_end, np_mu_snapshot), never concurrently.  Work
 * not yet committed is held in a second table, keyed by module, which any task
 * may add a fault to; it is changed only inside
 * np_transport_hal_enter/exit_critical, and the store is written outside it.
 *
 * Pure C, host-testable (np_module_usage_tests).
 */

#ifndef NP_MODULE_USAGE_H
#define NP_MODULE_USAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "np_hub_types.h"
#include "np_hub_config.h"    /* NP_HUB_SLOT_MAX */
#include "np_module_map.h"    /* NP_HEXMAP_MAX_SOCKETS */

#define NP_MU_ID_LEN        8U     /* a module UID (NP_HEXMAP_UID_LEN) */
#define NP_MU_FAULT_CODES   4U     /* fault codes counted by name per module */
/* One row for every module that can be fitted at once: every socket address
 * (NP_HEXMAP_MAX_SOCKETS) and every hub slot (NP_HUB_SLOT_MAX).  A module that
 * has left the helmet keeps its row until a newcomer needs the space; then the
 * row of the least-present module with nothing pending is given up (its totals
 * are already in the fleet's snapshots and the app's Map 2). */
#define NP_MU_MAX_ROWS      (NP_HEXMAP_MAX_SOCKETS + NP_HUB_SLOT_MAX)
/* The row this firmware writes: 64 bytes, version 1 (NP-FW-NVRAM-001 §3.3.1.6).
 * No record has been written by any helmet, so this is the first layout. */
#define NP_MU_ROW_BYTES     64U
#define NP_MU_HDR_BYTES     4U
#define NP_MU_VERSION       1U
/* The stored record has ONE length whatever the row length, so a later version
 * that grows the row is still read by this firmware (the store reads a fixed
 * length) and this firmware's rewrite carries the newer fields through
 * (§7.2, version rule for the record).  12,288 B holds every row at up to
 * NP_MU_ROW_MAX_BYTES; at 64 B the rows take 9,412 B of it.  How much room to
 * leave is not derived (UC-213, OI-NVRAM-22). */
#define NP_MU_TABLE_BYTES   12288U
#define NP_MU_ROW_MAX_BYTES ((NP_MU_TABLE_BYTES - NP_MU_HDR_BYTES) / NP_MU_MAX_ROWS)   /* 83 at 147 rows */

/* fault_flags bits.  Bit 0 is the only one defined (NP-FW-NVRAM-001 D-29). */
#define NP_MU_FAULT_UNSEATED_AT_CLOSE  0x01U

typedef enum {
    NP_MU_KEY_EMPTY = 0,
    NP_MU_KEY_UID   = 1,   /* id = the module's UID */
    NP_MU_KEY_PORT  = 2,   /* id[0] = slot, id[1] = np_hub_mod_type_t, rest 0 */
} np_mu_key_kind_t;

typedef struct {
    uint8_t kind;                  /* np_mu_key_kind_t */
    uint8_t id[NP_MU_ID_LEN];
} np_mu_key_t;

typedef struct {
    np_mu_key_t key;
    uint32_t    sessions_present;
    uint32_t    present_s;
    uint32_t    sessions_used;
    uint32_t    used_s;
    uint8_t     fault_code[NP_MU_FAULT_CODES];   /* 0 = unused entry */
    uint32_t    fault_count[NP_MU_FAULT_CODES];
    uint32_t    faults_other;                    /* faults whose code found no entry */
    uint32_t    mate_cycles;
    uint32_t    dose_accum;                      /* mJ/cm², summed over wavelengths */
    uint32_t    throttle_count;
    uint8_t     fault_flags;                     /* NP_MU_FAULT_* */
    uint8_t     detail_lost;                     /* 0 or 1 */
} np_mu_row_t;

np_mu_key_t np_mu_key_uid(const uint8_t uid[NP_MU_ID_LEN]);
np_mu_key_t np_mu_key_port(uint8_t slot, np_hub_mod_type_t type);

/* Forget everything in RAM (boot, and before each host test). */
void np_mu_init(void);

/* Read the table from the Config store.  An absent record is an empty table.
 * A record that exists but cannot be read leaves the module unloaded: it is
 * not taken as zero, which would restart every module's lifetime. */
np_hub_status_t np_mu_load(void);
bool np_mu_loaded(void);

/* A session starts: forget the previous session's presence, use and on-time. */
void np_mu_session_begin(void);

/* The module is fitted for this session.  Repeats are idempotent. */
void np_mu_mark_present(const np_mu_key_t *key);

/* An accessory slot starts or stops driving, at session time `now_ms` (the
 * session clock, which stands still while paused).  A slot that is already on
 * stays on from its first start; off with nothing on does nothing. */
void np_mu_slot_on(uint8_t slot, np_hub_mod_type_t type, uint32_t now_ms);
void np_mu_slot_off(uint8_t slot, uint32_t now_ms);

/* A module was driven for `on_ms` in this session (the PBM tiles, from the
 * dose meter).  Marks it present and used. */
void np_mu_add_used(const np_mu_key_t *key, uint32_t on_ms);

/* A fault was logged.  Counted against the accessory port's row when `slot`
 * is an accessory slot; a fault with no slot (a PBM fault carries none, F11)
 * belongs to no module and is not counted here — it is in the fault log. */
void np_mu_note_fault(uint8_t slot, uint8_t mod_type, uint8_t code);

/* A mate: the module was inserted, as the caller decided (an accessory slot
 * whose occupant changed while the hub ran; a tile under MODID-4a, once the
 * inventory poll has a caller).  Added at the next commit; a mate noted and
 * never committed before power is lost is not counted (a lower bound). */
void np_mu_note_mate(const np_mu_key_t *key);

/* A tile's metered dose this session, mJ/cm² summed over wavelengths, and
 * whether its thermal throttle tripped this session (one count per call). */
void np_mu_add_dose(const np_mu_key_t *key, uint32_t dose_mJ_cm2);
void np_mu_note_throttle(const np_mu_key_t *key);

/* The module was no longer seated when its session closed (D-29).  Sets
 * fault_flags bit 0 for good. */
void np_mu_note_unseated_at_close(const np_mu_key_t *key);

/* Σ dose_J_cm2[i] over `n` wavelengths, in whole mJ/cm² (rounded half up).
 * One sum, because NP-FW-NVRAM-001 §8.5 names one dose_accum (UC-214).  A
 * negative or non-finite entry adds nothing; a sum past UINT32_MAX is
 * UINT32_MAX. */
uint32_t np_mu_dose_mJ_cm2(const float *dose_J_cm2, size_t n);

/* The session ends after `session_ms` of session time: close every open slot
 * interval, add this session to every present and used module's totals and to
 * the faults since the last commit, and write the table.  If the table is
 * not loaded and still cannot be read, nothing is written and this session's
 * totals are dropped (counted by np_mu_sessions_dropped()); a write never
 * overwrites a record it could not read.  Every module whose contribution was
 * dropped gets detail_lost on its row at the next commit that succeeds. */
np_hub_status_t np_mu_session_end(uint32_t session_ms);

uint32_t np_mu_sessions_dropped(void);

/* The download's snapshot (NP-FW-NVRAM-001 §6): copy up to `cap` rows of the
 * stored table into `out` and their number into *n_out.  NP_HUB_ERR_STORE_IO,
 * with *n_out = 0, when the table is not loaded: unknown is reported as
 * unknown, never as an empty table (D-7).  A read changes nothing on the
 * helmet: no watermark, no acknowledgement, no row retired. */
np_hub_status_t np_mu_snapshot(np_mu_row_t *out, size_t cap, size_t *n_out);

/* Encode and decode the stored record (NP_MU_TABLE_BYTES).  Little-endian.
 * The header (version, row length, row count) makes it self-describing.
 * np_mu_encode writes a fresh version-1 record.  np_mu_decode accepts any
 * version >= 1 and any row length from NP_MU_ROW_BYTES to NP_MU_ROW_MAX_BYTES,
 * decodes the version-1 fields at their fixed offsets and ignores the rest of
 * each row; it refuses the record whole (leaving `rows` untouched) on version
 * 0, a row length out of that range, more rows than `cap` or NP_MU_MAX_ROWS,
 * or a row whose key kind is unknown. */
void np_mu_encode(const np_mu_row_t *rows, size_t n, uint8_t out[NP_MU_TABLE_BYTES]);
bool np_mu_decode(const uint8_t in[NP_MU_TABLE_BYTES], np_mu_row_t *rows, size_t cap, size_t *n_out);

#endif /* NP_MODULE_USAGE_H */
