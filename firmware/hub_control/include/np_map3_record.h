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
 * ── The row (§3.3.1.5, 32 bytes at version 1) ─────────────────────────────────
 *
 *   off  size  field
 *     0     1  len              total record bytes, CRC included
 *     1     1  ver              record layout version, >= 1
 *     2     8  uid              module UID (SHDR class, never uploaded raw)
 *    10     4  seq              monotonic per record (§5.2)
 *    14     4  ordinal          session ordinal (§5.2)
 *    18     2  session_count
 *    20     4  dose_accum
 *    24     2  throttle_count
 *    26     1  fault_flags
 *    27     1  reserved         written 0, ignored on read
 *    28   ...  (a later version's tail fields)
 *  len-4    4  crc32            over bytes [0, len-4): header ‖ uid ‖ payload
 *
 * All multi-byte fields are little-endian.  `len` and `ver` occupy the two
 * bytes §3.3.1.5 left as padding (30 B of fields padded to 32), so version 1 is
 * exactly the budgeted row.  They are what make the row self-describing: a
 * reader steps by the record's own `len`, never by the size it expects, so a
 * longer row written by newer firmware neither shifts every record after it
 * nor stops the scan (D-25).
 *
 * The CRC binds the UID (§4.2(b) item 2): a record recovered into the wrong
 * module's history fails validation instead of transferring one part's usage
 * to another.  It also covers `len` and `ver`, so a flipped length cannot
 * make the scanner walk into the middle of the next record and accept it.
 *
 * ── The version rule (§7.2 item 2) ────────────────────────────────────────────
 *
 *   1. Fields at offsets [2, 28) are fixed for ever.  A new version APPENDS
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
 * (NP_CFG_FILE_MAP3, tail-additive).  Nor is it the journal-full policy
 * (§6.3), the watermarks (§6.1) or sync — those are Map 3's owner's, and they
 * are not written yet.  Its output is HISTORY: REQ-LFS-01 forbids reading a
 * limit out of it.
 */

#ifndef NP_MAP3_RECORD_H
#define NP_MAP3_RECORD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The layout version this firmware writes. */
#define NP_MAP3_VERSION          1U

/* Version-1 row: header 2 + fields 26 + CRC 4. */
#define NP_MAP3_V1_BYTES         32U
#define NP_MAP3_V1_FIELDS_END    28U   /* first byte after the v1 fields */
#define NP_MAP3_CRC_BYTES        4U

/* `len` is one byte, so no version can exceed this. */
#define NP_MAP3_MAX_BYTES        255U

/* §3.3.1.5 / D-24: the file bound and the row count it gives at version 1. */
#define NP_MAP3_FILE_BYTES       49152U
#define NP_MAP3_V1_ROWS          (NP_MAP3_FILE_BYTES / NP_MAP3_V1_BYTES)   /* 1,536 */

#define NP_MAP3_UID_LEN          8U

/* The fields version 1 defines.  A later version adds members after these. */
typedef struct {
    uint8_t  uid[NP_MAP3_UID_LEN];
    uint32_t seq;
    uint32_t ordinal;
    uint16_t session_count;
    uint32_t dose_accum;
    uint16_t throttle_count;
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
    NP_MAP3_END_BAD_LEN,       /* `len` below the v1 row, or past the end        */
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
 * Encode `rec` as a version-NP_MAP3_VERSION row into out[NP_MAP3_V1_BYTES].
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

#endif /* NP_MAP3_RECORD_H */
