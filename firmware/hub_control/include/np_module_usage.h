/*
 * NeurOne Hub Control Program — per-module running totals (REQ-FWHUB-93)
 * Document: NP-FW-HUB-001 §6.11; NP-FW-NVRAM-001 §3.3.1.6, §8.5 (the principal,
 *           2026-11-23, amended 2026-11-24 and 2026-11-26);
 *           docs/reference/data-architecture-detail.md §5.1
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
 *   throttle_count                sessions in which a tile's thermal throttle
 *                                 tripped (the dose meter latches it once per
 *                                 socket per session).
 *   ch_on_s, ch_sessions          per emitter drive channel: the time it was
 *                                 driven, and the sessions in which it was
 *                                 driven (not pulses).  Emitter wear, per
 *                                 emitter, as the principal ruled on
 *                                 2026-11-26 (NP-FW-NVRAM-001 §8.5, D-38).
 *   fault_flags                   sticky bits; bit 0 = closed at least once
 *                                 with the module no longer seated
 *                                 (NP-FW-NVRAM-001 §6.5.1, D-29).
 *   detail_lost                   1 when this row is known to fall short: it
 *                                 was created by giving up another module's
 *                                 row, or a commit that should have added to
 *                                 it was dropped (NP-FW-NVRAM-001 §6.3).
 *
 * Delivered dose is not here.  It is the person's, kept per wavelength for
 * their app only and never SHDR (the principal, 2026-11-26), and Config may
 * hold nothing of UHDR class (NP-FW-NVRAM-001 D-3), so the app builds it from
 * the UHDR session files (OI-NVRAM-29).  The dose_accum sum this row carried
 * until OI-NVRAM-23 is removed.
 *
 * These tables ARE Map 3 (NP-FW-NVRAM-001 §8.5, OI-NVRAM-20, OI-NVRAM-23): one
 * running-total row per module, upserted, in one record per key kind.  The
 * per-session journal (map3.jrn, np_map3_record) they replaced was retired
 * before a row was written; its codec is removed, and the file and the Config
 * store's journal API with it (OI-NVRAM-21).
 *
 * Who may see which field (§8.5, §8.1): the person's app reads every field at
 * a download and keeps the previous snapshot in Map 2.  The fleet gets the
 * totals under the upload's month anchor; of those, throttle_count and the
 * per-channel totals only from helmets opted in to NP-MOD-ID-001 §7's
 * characterisation, within its window, until the §7.4 review gate rules
 * (D-39); fault_flags bit 0 never leaves the app (§6.5.1).  The hub side of
 * that upload is not built.
 *
 * ── Totals, never sessions (CLAUDE.md §5.1; NP-FW-NVRAM-001 §8.5) ──────────
 * Fixed-size records of every module's totals, upserted at session end
 * (NP_CFG_FILE_MODULE_USAGE and NP_CFG_FILE_MODULE_PORTS, REPLICATED).  No row
 * is ever appended, nothing is stamped with a session or a time, and nothing
 * is written to the SHDR log here: a snapshot written per session would let
 * the difference between two snapshots say which module was used in which
 * session, which is the treatment's montage (UHDR).  The totals are SHDR, and
 * reach the fleet only as the upload's snapshot under its month anchor (§8.5),
 * so the finest view anyone but the person's own app has is the growth between
 * two uploads.
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

/* ── Emitter channels (NP-FW-NVRAM-001 §3.3.1.6, §8.5, D-38; OI-NVRAM-23) ─────
 * Each emitter's use is kept per drive channel, the finest the hub drives (the
 * principal, 2026-11-26).  Channel numbers:
 *   tile              c = wavelength index: 0 = 660, 1 = 808, 2 = 1064 nm
 *                     (NP_PBM_CH_A/B/C_EN; NP-HW-HEXTILE-001 §4.2).  A base
 *                     tile drives 0 and 1 only.
 *   intranasal probe  c = 2 × side + wavelength: 0 = left 660, 1 = left 808,
 *                     2 = right 660, 3 = right 808 (REQ-NASAL-04).
 *   1170 nm unit (T2) c = 0, counted as one channel (UC-220).
 *   goggles           none yet: what their zone bits drive is open
 *                     (OI-FWHUB-35), so no channel is counted (OI-NVRAM-31).
 *   other accessories none: they are not emitters.
 * NP_MU_TILE_CH is the tile's wavelength count, asserted against
 * NP_PBM_WL_COUNT where both are visible (np_pbm_socket_telem.c).
 * NP_MU_PORT_CH is the most channels of any port module whose channel map is
 * known: the intranasal probe's two sides × two wavelengths. */
#define NP_MU_TILE_CH       3U
#define NP_MU_PORT_CH       4U
#define NP_MU_CH_MAX        NP_MU_PORT_CH   /* channel slots in np_mu_row_t */

/* ── Two records, one per key kind (NP-FW-NVRAM-001 §3.3.1.6, D-36) ─────────
 *   tiles  NP_CFG_FILE_MODULE_USAGE  one row per socket address (UID key)
 *   ports  NP_CFG_FILE_MODULE_PORTS  one row per hub slot (port key)
 * Each table holds one row for every module of its kind that can be fitted at
 * once.  A module that has left keeps its row until a newcomer of the same
 * kind needs the space; then the row of the least-present module of that kind
 * with nothing pending is given up (UC-208).  The rows differ by kind only in
 * their channel slots, so the 128 tile rows do not carry the probe's fourth
 * channel, and the 19 port rows can grow without them. */
typedef enum {
    NP_MU_TABLE_TILES = 0,
    NP_MU_TABLE_PORTS = 1,
    NP_MU_TABLE_COUNT = 2,
} np_mu_table_t;

#define NP_MU_TILE_ROWS     NP_HEXMAP_MAX_SOCKETS                  /* 128 */
#define NP_MU_PORT_ROWS     NP_HUB_SLOT_MAX                        /* 19  */
#define NP_MU_MAX_ROWS      (NP_MU_TILE_ROWS + NP_MU_PORT_ROWS)    /* both tables: 147 */

/* The rows this firmware writes, version 1, little-endian.  The fields every
 * row has take 59 bytes; each channel slot adds 8 (a 32-bit on-time and a
 * 32-bit session count, stored whole, OI-UINT16-01).  No record has been
 * written by any helmet, so this is the first layout; the 64-byte row of one
 * table that it replaces was never written either. */
#define NP_MU_COMMON_BYTES   59U
#define NP_MU_CH_BYTES       8U
#define NP_MU_TILE_ROW_BYTES (NP_MU_COMMON_BYTES + NP_MU_TILE_CH * NP_MU_CH_BYTES)   /* 83 */
#define NP_MU_PORT_ROW_BYTES (NP_MU_COMMON_BYTES + NP_MU_PORT_CH * NP_MU_CH_BYTES)   /* 91 */
#define NP_MU_HDR_BYTES      4U
#define NP_MU_VERSION        1U
/* The header's row length is one byte, so no version can describe a longer row. */
#define NP_MU_ROW_LEN_FIELD_MAX 255U

/* Each record has ONE length whatever its row length, so a later version that
 * grows a row is still read by this firmware (the store reads a fixed length)
 * and this firmware's rewrite carries the newer fields through (§7.2, the
 * version rule for the record).
 *
 * Tiles: 12,288 B, kept from the one-table record.  How much room to leave is
 * not derived (UC-213, OI-NVRAM-22 (b)); it stays at the store's existing
 * table size, because a longer record grows the store's three static buffers
 * with it.  128 rows of 83 B take 10,628 B, which leaves 12 B a row.
 *
 * Ports: as long as the header can describe, 4 + 19 × 255 = 4,849 B.  This is
 * derived, not chosen: no version can write a row longer than
 * NP_MU_ROW_LEN_FIELD_MAX without changing the header, so a record of this
 * length never needs another length while the header stands; and it is under
 * the store's existing table size, so it costs the store no RAM.  19 rows of
 * 91 B take 1,733 B, which leaves 164 B a row (20 more channel slots): room
 * for the goggles' channels once OI-FWHUB-35 says what they are, with no
 * converter. */
#define NP_MU_TILE_TABLE_BYTES   12288U
#define NP_MU_PORT_TABLE_BYTES   (NP_MU_HDR_BYTES + NP_MU_PORT_ROWS * NP_MU_ROW_LEN_FIELD_MAX)  /* 4,849 */
#define NP_MU_TILE_ROW_MAX_BYTES ((NP_MU_TILE_TABLE_BYTES - NP_MU_HDR_BYTES) / NP_MU_TILE_ROWS) /* 95  */
#define NP_MU_PORT_ROW_MAX_BYTES ((NP_MU_PORT_TABLE_BYTES - NP_MU_HDR_BYTES) / NP_MU_PORT_ROWS) /* 255 */
#define NP_MU_TABLE_MAX_BYTES    NP_MU_TILE_TABLE_BYTES   /* the longer of the two */

/* fault_flags bits.  Bit 0 is the only one defined (NP-FW-NVRAM-001 D-29). */
#define NP_MU_FAULT_UNSEATED_AT_CLOSE  0x01U

typedef enum {
    NP_MU_KEY_EMPTY = 0,
    NP_MU_KEY_UID   = 1,   /* id = the module's UID; a tile row */
    NP_MU_KEY_PORT  = 2,   /* id[0] = slot, id[1] = np_hub_mod_type_t, rest 0; a port row */
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
    uint32_t    throttle_count;
    uint8_t     fault_flags;                     /* NP_MU_FAULT_* */
    uint8_t     detail_lost;                     /* 0 or 1 */
    /* Per drive channel (numbering above).  A tile row stores NP_MU_TILE_CH of
     * them and reads the rest as 0; a port row stores NP_MU_PORT_CH. */
    uint32_t    ch_on_s[NP_MU_CH_MAX];           /* seconds the channel was driven */
    uint32_t    ch_sessions[NP_MU_CH_MAX];       /* sessions in which it was driven */
} np_mu_row_t;

np_mu_key_t np_mu_key_uid(const uint8_t uid[NP_MU_ID_LEN]);
np_mu_key_t np_mu_key_port(uint8_t slot, np_hub_mod_type_t type);

/* Forget everything in RAM (boot, and before each host test). */
void np_mu_init(void);

/* Read both tables from the Config store.  An absent record is an empty
 * table.  A record that exists but cannot be read leaves its table unloaded:
 * it is not taken as zero, which would restart every module's lifetime.  Each
 * table loads on its own.  NP_HUB_OK when both are loaded, else the first
 * failure. */
np_hub_status_t np_mu_load(void);
bool np_mu_loaded(void);                       /* both tables */
bool np_mu_table_loaded(np_mu_table_t t);

/* A session starts: forget the previous session's presence, use and on-time. */
void np_mu_session_begin(void);

/* The module is fitted for this session.  Repeats are idempotent. */
void np_mu_mark_present(const np_mu_key_t *key);

/* An accessory slot is driven, at session time `now_ms` (the session clock,
 * which stands still while paused), with the emitter channels in `ch_mask`
 * lit (bit c = channel c of the port numbering above; 0 for a module that is
 * not an emitter or whose channel map is not known).  The slot's own interval
 * runs from its first start; a later call while it is on changes only which
 * channels are lit: a channel that goes dark closes its interval at `now_ms`,
 * and a channel newly lit opens one.  Off closes the slot and every channel;
 * off with nothing on does nothing. */
void np_mu_slot_on(uint8_t slot, np_hub_mod_type_t type, uint8_t ch_mask, uint32_t now_ms);
void np_mu_slot_off(uint8_t slot, uint32_t now_ms);

/* A module was driven for `on_ms` in this session (the PBM tiles, from the
 * dose meter).  Marks it present and used. */
void np_mu_add_used(const np_mu_key_t *key, uint32_t on_ms);

/* Channel `ch` of a module was driven for `on_ms` in this session (a tile's
 * wavelength, from the dose meter's ticks with that channel lit).  Marks the
 * module present and the channel driven this session.  A channel past the
 * module's kind (NP_MU_TILE_CH for a tile, NP_MU_PORT_CH for a port) is
 * ignored. */
void np_mu_add_channel_used(const np_mu_key_t *key, uint8_t ch, uint32_t on_ms);

/* A fault was logged.  Counted against the accessory port's row when `slot`
 * is an accessory slot; a fault with no slot (a PBM fault carries none, F11)
 * belongs to no module and is not counted here — it is in the fault log. */
void np_mu_note_fault(uint8_t slot, uint8_t mod_type, uint8_t code);

/* A mate: the module was inserted, as the caller decided (an accessory slot
 * whose occupant changed while the hub ran; a tile under MODID-4a, once the
 * inventory poll has a caller).  Added at the next commit; a mate noted and
 * never committed before power is lost is not counted (a lower bound). */
void np_mu_note_mate(const np_mu_key_t *key);

/* A tile's thermal throttle tripped this session (one count per call). */
void np_mu_note_throttle(const np_mu_key_t *key);

/* The module was no longer seated when its session closed (D-29).  Sets
 * fault_flags bit 0 for good. */
void np_mu_note_unseated_at_close(const np_mu_key_t *key);

/* The session ends after `session_ms` of session time: close every open slot
 * and channel interval, add this session to every present and used module's
 * totals and to the faults since the last commit, and write both tables.  If
 * a table is not loaded and still cannot be read, nothing is written to it and
 * the contributions of its modules are dropped (counted by
 * np_mu_sessions_dropped(), once per session end that dropped any); a write
 * never overwrites a record it could not read.  Every module whose
 * contribution was dropped gets detail_lost on its row at the next commit that
 * succeeds.  The other table commits as usual.  NP_HUB_OK only when both
 * tables were written. */
np_hub_status_t np_mu_session_end(uint32_t session_ms);

uint32_t np_mu_sessions_dropped(void);

/* The download's snapshot (NP-FW-NVRAM-001 §6): copy up to `cap` rows of the
 * stored tables, tiles first, into `out` and their number into *n_out.
 * NP_HUB_ERR_STORE_IO, with *n_out = 0, when either table is not loaded:
 * unknown is reported as unknown, never as an empty table (D-7).  A read
 * changes nothing on the helmet: no watermark, no acknowledgement, no row
 * retired. */
np_hub_status_t np_mu_snapshot(np_mu_row_t *out, size_t cap, size_t *n_out);

/* The record length of table `t` (NP_MU_TILE_TABLE_BYTES or
 * NP_MU_PORT_TABLE_BYTES); 0 for a table that does not exist. */
size_t np_mu_table_bytes(np_mu_table_t t);

/* Encode and decode table `t`'s stored record (np_mu_table_bytes(t) bytes).
 * Little-endian.  The header (version, row length, row count) makes it
 * self-describing.  np_mu_encode writes a fresh version-1 record at the
 * table's version-1 row length; a row whose key is not the table's kind is
 * skipped.  np_mu_decode accepts any version >= 1 and any row length from the
 * table's version-1 row to its NP_MU_*_ROW_MAX_BYTES, decodes the version-1
 * fields at their fixed offsets and ignores the rest of each row; it refuses
 * the record whole (leaving `rows` untouched) on version 0, a row length out
 * of that range, more rows than `cap` or the table holds, or a row whose key
 * kind is not the table's. */
void np_mu_encode(np_mu_table_t t, const np_mu_row_t *rows, size_t n, uint8_t *out);
bool np_mu_decode(np_mu_table_t t, const uint8_t *in, np_mu_row_t *rows, size_t cap, size_t *n_out);

#endif /* NP_MODULE_USAGE_H */
