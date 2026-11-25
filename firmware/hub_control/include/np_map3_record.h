/*
 * np_map3_record.h — the Map 3 journal record, and the reader that never
 *                    loses one to a version change
 * Document: NP-FW-NVRAM-001 Rev 3 §3.3.1.5, §4.2(b), §7.2 (D-5, D-7, D-24, D-25)
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * Map 3 is a RECORD, not a cache (§7.2): nothing else remembers the sessions
 * it holds, so the inventory blob's reject-and-rebuild policy would be an
 * irrecoverable loss here (RISK-NVRAM-01).  Its version rule is the opposite
 * one: a record may be extended at the TAIL only, and the reader accepts every
 * version, decodes the fields it knows and skips the ones it does not.
 *
 * ── The row (§3.3.1.5, 64 bytes at version 1) ─────────────────────────────────
 *
 *   off  size  field
 *     0     1  len              total record bytes, CRC included
 *     1     1  ver              record layout version, >= 1
 *     2     8  uid              module UID (SHDR class, never uploaded raw)
 *    10     4  seq              monotonic per record (§5.2)
 *    14     4  ordinal          session ordinal (§5.2)
 *    18     4  session_count
 *    22     4  dose_accum
 *    26     4  throttle_count
 *    30     1  fault_flags
 *    31     1  reserved         written 0, ignored on read, never reused
 *    32    28  reserved         written 0, ignored on read, never reused
 *    60   ...  (a later version's tail fields)
 *  len-4    4  crc32            over bytes [0, len-4): header ‖ uid ‖ payload
 *
 * RETIRED LAYOUT: NOT TO BE WRITTEN (OI-PRIV-38 (NP-PRIV-ANALYSIS-004), ruled by
 * the principal 2026-11-23, NP-FW-NVRAM-001 §8.5).  Map 3 becomes one running-
 * total row per module, with no per-session row and no `ordinal`.  This codec
 * describes the per-session row that ruling replaced; nothing writes it, and
 * its replacement is OI-NVRAM-20.
 *
 * All multi-byte fields are little-endian.  The row is 64 bytes so that four rows fill a 256-byte program unit and
 * none straddles one (NP-FW-NVRAM-001 §4.2(b) requirement 1; OI-UINT16-01, the principal, 2026-11-13).  The counts are
 * 32 bits and are stored as they are: no field holds a ceiling (OI-UINT16-01).  No hardware has shipped and no journal
 * has been written, so there is exactly one layout, and it is version 1.  `len` and `ver` make the row self-describing:
 * a reader steps by the record's own `len`, never by the size it expects, so a longer row written by newer firmware
 * neither shifts every record after it nor stops the scan (D-25).
 *
 * The CRC binds the UID (§4.2(b) item 2): a record recovered into the wrong
 * module's history fails validation instead of transferring one part's usage
 * to another.  It also covers `len` and `ver`, so a flipped length cannot
 * make the scanner walk into the middle of the next record and accept it.
 *
 * ── The version rule (§7.2 item 2) ────────────────────────────────────────────
 *
 *   1. Fields at offsets [2, 60) are fixed for ever.  A new version APPENDS
 *      before the CRC and raises `ver`; it never moves, resizes or reuses a
 *      version-1 field, including `reserved`.
 *   2. The reader accepts any ver >= 1.  A record newer than the reader is
 *      decoded for the fields the reader knows, and its tail is skipped.  A
 *      record older than the reader is decoded for the fields it carries; the
 *      reader's newer fields are reported absent, never fabricated as zero
 *      (D-7).
 *   3. A change that is not tail-additive needs a converter that runs before
 *      the new firmware's first write.  It is a defect otherwise.
 *
 * np_map3_record_tests is the CI check of this rule (OI-NVRAM-08): it writes a
 * journal under version n, reads it under n+1 and under n, and requires every
 * record back; it is falsified by running the same check against three
 * readers that break the rule and requiring each to fail.
 *
 * ── What this file is NOT ─────────────────────────────────────────────────────
 * Storage.  It encodes and scans bytes; np_cfg_store owns the file
 * (NP_CFG_FILE_MAP3, tail-additive) and reads it with
 * np_cfg_store_journal_read_rows(), stepping by np_map3_row_len().  Nor is it the journal-full policy
 * (§6.3), the watermarks (§6.1) or sync — those are Map 3's owner's, and they
 * are not written yet.  Its output is HISTORY: REQ-LFS-01 forbids reading a
 * limit out of it.
 */

#ifndef NP_MAP3_RECORD_H
#define NP_MAP3_RECORD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The layout version this firmware writes: the only one there is. */
#define NP_MAP3_VERSION          1U

/* The row: header 2 + fields 56 + CRC 4 = 64 bytes (the layout above). */
#define NP_MAP3_ROW_BYTES        64U
#define NP_MAP3_FIELDS_END       60U   /* first byte after the version-1 fields */

/* The program unit is the NAND's (NP-FW-NVRAM-001 §3.3.1.5: 256 B), so it is the foundational value and the row size is held to it. */
#define NP_MAP3_PROGRAM_UNIT_BYTES 256U
_Static_assert(NP_MAP3_PROGRAM_UNIT_BYTES % NP_MAP3_ROW_BYTES == 0U, "a row would straddle a program unit");
#define NP_MAP3_CRC_BYTES        4U

/* `len` is one byte, so no version can exceed this. */
#define NP_MAP3_MAX_BYTES        255U

/* §3.3.1.5 / D-24: the file bound and the row count it gives. */
#define NP_MAP3_FILE_BYTES       49152U
#define NP_MAP3_ROWS             (NP_MAP3_FILE_BYTES / NP_MAP3_ROW_BYTES)   /* 768 */

#define NP_MAP3_UID_LEN          8U

/*
 * fault_flags bit 0 (NP-FW-NVRAM-001 Rev 7 §6.5.1, D-29): the hub closed this
 * row after the module had stopped answering on its socket, so the session
 * was interrupted by extraction and U1 never made its session-end Map 4
 * write.  Reconciliation leaves the row out of what Map 4 is expected to
 * hold, so the shortfall is not reported as an anomaly.  Device- and
 * app-internal: never uploaded (§8.1).  The other seven bits are unassigned.
 */
#define NP_MAP3_FAULT_UNSEATED_AT_CLOSE  0x01U

typedef struct {
    uint8_t  uid[NP_MAP3_UID_LEN];
    uint32_t seq;
    uint32_t ordinal;
    uint32_t session_count;
    uint32_t dose_accum;
    uint32_t throttle_count;
    uint8_t  fault_flags;
} np_map3_rec_t;

/* One decoded row, as the scanner hands it over. */
typedef struct {
    np_map3_rec_t rec;
    uint8_t       ver;         /* the version it was written under          */
    uint8_t       len;         /* its length on the medium                  */
    uint8_t       tail_len;    /* bytes of later-version fields skipped      */
    uint32_t      offset;      /* byte offset of the row in the journal     */
} np_map3_row_t;

/* Why a scan stopped. */
typedef enum {
    NP_MAP3_END_CLEAN = 0,     /* consumed every byte                           */
    NP_MAP3_END_TORN,          /* a partial row at the tail (L-4: costs one row) */
    NP_MAP3_END_BAD_LEN,       /* `len` below the row, or past the end           */
    NP_MAP3_END_BAD_CRC,       /* the row does not verify                        */
    NP_MAP3_END_BAD_VER,       /* ver == 0: never written by any firmware        */
    NP_MAP3_END_STOPPED,       /* the callback asked to stop                     */
} np_map3_end_t;

typedef struct {
    uint32_t      rows;        /* valid prefix, in rows                          */
    uint32_t      bytes;       /* valid prefix, in bytes                         */
    np_map3_end_t end;
} np_map3_scan_t;

/* Return false to stop the scan early. */
typedef bool (*np_map3_row_fn)(const np_map3_row_t *row, void *ctx);

/*
 * Encode `rec` as a version-NP_MAP3_VERSION row into out[NP_MAP3_ROW_BYTES].
 * Returns the number of bytes written.
 */
size_t np_map3_encode(const np_map3_rec_t *rec, uint8_t *out, size_t cap);

/*
 * Scan a journal image: the valid prefix of `buf`, in order.  `cb` may be
 * NULL to count only.  Never reads past `len`.  The first row that does not
 * verify ends the valid prefix, and nothing after it is trusted (§4.2(b)): a
 * torn tail costs one row and every row before it survives.
 *
 * The version of a row NEVER ends the scan, except ver == 0, which no
 * firmware writes.  That is the whole of D-25, and OI-NVRAM-08 checks it.
 */
void np_map3_scan(const uint8_t *buf, size_t len, np_map3_row_fn cb, void *ctx,
                  np_map3_scan_t *out);

/*
 * The length of the verified row that starts at `p`, with `left` bytes to the
 * end of the journal, or 0 when no verified row starts there.  It applies
 * exactly the test np_map3_scan() applies, and it is the step Map 3 hands to
 * np_cfg_store_journal_read_rows() so the store walks a journal of mixed row
 * lengths by each row's own `len` (OI-NVRAM-16).
 */
size_t np_map3_row_len(const uint8_t *p, size_t left);

#endif /* NP_MAP3_RECORD_H */
