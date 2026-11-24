/*
 * NeurOne Per-Person UHDR Keys — file sealing
 * Document: NP-FW-EMMC-002 §C.8.3, §C.8.7 (OI-PRIV-34, NP-PRIV-ANALYSIS-004)
 * IEC 62304 Class: SW-02 Class B (main processor)
 *
 * Increment 2 of §C.8: each UHDR file is sealed on its own, under a key that
 * only its owner's profile key yields, so it is a self-contained object that
 * can be copied and placed on any helmet without being readable (§C.8.1, the
 * principal's Q2 ruling).  This module works on buffers only.  It writes no
 * file and changes no mount: the log backend starts sealing with increment 3,
 * when the UHDR partition moves to the per-person store.
 *
 * A sealed file is:
 *
 *   header   NP_UHDR_SEAL_HEADER_SIZE bytes, in the clear: magic, format,
 *            the file's random id, the chunk size.  Authenticated by every
 *            chunk (it is in each chunk's AAD).
 *   chunks   each NP_UHDR_SEAL_BLOCK_BYTES on the medium: up to
 *            NP_UHDR_SEAL_CHUNK_PT_BYTES of ciphertext and a GCM tag.  Every
 *            chunk but the last is full; the last is shorter (possibly just a
 *            tag) and carries the FINAL flag in its AAD.
 *
 *   file key FK = file_kdf(PK, profile id, file id), one per file, so chunk
 *            nonces (the chunk index) never repeat under one key.
 *   nonce    4 zero bytes || chunk index, big-endian 64-bit.
 *   AAD      header || chunk index (64-bit BE) || final flag (1 byte).
 *
 * So a changed, reordered, dropped, duplicated or appended chunk fails its
 * tag; a file cut after any chunk is refused because no FINAL chunk is seen;
 * a file moved under another profile, or given another file's header, derives
 * a different FK and opens nothing.
 *
 * The file's NAME is the hex of its random id: it carries no session count,
 * no profile and no time (§C.8.3; compare OI-PRIV-41, the shared counter).
 *
 * The construction (AES-256-GCM chunks with a final flag, in the manner of
 * the STREAM construction) and the file KDF are the cryptography reviewer's
 * to confirm (OI-EMMC2-02).
 */

#ifndef NP_UHDR_SEAL_H
#define NP_UHDR_SEAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "np_uhdr_key_config.h"
#include "np_uhdr_key_types.h"
#include "np_uhdr_profile.h"

/* ── Sizes (§C.8.7) ──────────────────────────────────────────────────────────
 * The foundational value is the sealed chunk on the medium, one block of the
 * log backend's staging unit (NP_LOG_STAGE_BYTES = 512, np_log_backend.h), so
 * each staged write is exactly one sealed chunk.  The two are tied by a
 * compile-time check when the backend starts sealing (increment 3); until then
 * this module has no caller.  The chunk's plaintext is derived from it.
 */
#define NP_UHDR_SEAL_BLOCK_BYTES     512U
#define NP_UHDR_SEAL_CHUNK_PT_BYTES  (NP_UHDR_SEAL_BLOCK_BYTES - NP_UHDR_GCM_TAG_LEN)

#define NP_UHDR_FILE_ID_LEN          16U
#define NP_UHDR_FILE_KEY_LEN         32U
#define NP_UHDR_FILE_NAME_LEN        (2U * NP_UHDR_FILE_ID_LEN)   /* hex, no NUL */

#define NP_UHDR_SEAL_MAGIC           0x4653504EU   /* "NPSF", little-endian */
#define NP_UHDR_SEAL_FORMAT          1U

/* The clear header of a sealed file.  Packed, byte-exact:
 *   4 + 4 + 16 + 4 + 4 = 32 bytes. */
typedef struct __attribute__((packed)) {
    uint32_t magic;                          /* NP_UHDR_SEAL_MAGIC           */
    uint32_t format;                         /* NP_UHDR_SEAL_FORMAT          */
    uint8_t  file_id[NP_UHDR_FILE_ID_LEN];   /* random; the file's name      */
    uint32_t chunk_pt_bytes;                 /* NP_UHDR_SEAL_CHUNK_PT_BYTES  */
    uint32_t reserved;                       /* zero                         */
} np_uhdr_seal_header_t;

#define NP_UHDR_SEAL_HEADER_SIZE     32U

/* Where a writer hands each sealed chunk (ciphertext then tag, `len` bytes:
 * NP_UHDR_SEAL_BLOCK_BYTES for every chunk but the last).  A non-OK return
 * aborts the writer. */
typedef np_uhdr_status_t (*np_uhdr_seal_sink_t)(void *sink_ctx,
                                                const uint8_t *chunk, size_t len);

typedef struct {
    uint8_t               fk[NP_UHDR_FILE_KEY_LEN];      /* SRAM only        */
    np_uhdr_seal_header_t header;
    uint8_t               buf[NP_UHDR_SEAL_CHUNK_PT_BYTES];
    size_t                fill;
    uint64_t              index;
    bool                  open;
} np_uhdr_seal_writer_t;

typedef struct {
    uint8_t               fk[NP_UHDR_FILE_KEY_LEN];      /* SRAM only        */
    np_uhdr_seal_header_t header;
    uint64_t              index;
    bool                  open;
    bool                  final_seen;
} np_uhdr_seal_reader_t;

/* ── Writing ──────────────────────────────────────────────────────────────── */

/*
 * Start a new sealed file owned by the open `profile`: draws a random file id,
 * derives the file key and fills `header_out`, which the caller writes first.
 * NP_UHDR_ERR_STATE if the profile is not open.
 */
np_uhdr_status_t np_uhdr_seal_writer_begin(np_uhdr_seal_writer_t *w,
                                           const np_uhdr_profile_ctx_t *profile,
                                           np_uhdr_seal_header_t *header_out);

/* Append plaintext; every full chunk is sealed and handed to `sink`. */
np_uhdr_status_t np_uhdr_seal_writer_put(np_uhdr_seal_writer_t *w,
                                         const uint8_t *data, size_t len,
                                         np_uhdr_seal_sink_t sink, void *sink_ctx);

/* Seal and hand over the FINAL chunk (the buffered tail, possibly empty) and
 * close the writer, zeroing its key.  A file without it is refused on read. */
np_uhdr_status_t np_uhdr_seal_writer_finish(np_uhdr_seal_writer_t *w,
                                            np_uhdr_seal_sink_t sink, void *sink_ctx);

/* Abandon a writer: zero its key and buffer.  Safe on a closed or NULL writer. */
void np_uhdr_seal_writer_abort(np_uhdr_seal_writer_t *w);

/* ── Reading ──────────────────────────────────────────────────────────────── */

/*
 * Start reading a sealed file with the open `profile`: checks the header and
 * derives the file key.  Opening succeeds for any well-formed header; a wrong
 * profile is detected at the first chunk (its tag fails).
 */
np_uhdr_status_t np_uhdr_seal_reader_begin(np_uhdr_seal_reader_t *r,
                                           const np_uhdr_profile_ctx_t *profile,
                                           const np_uhdr_seal_header_t *header);

/*
 * Open the next chunk, in order.  `len` is the chunk's length on the medium
 * (a full block, or shorter for the final chunk).  Writes `len - tag` bytes of
 * plaintext to `pt_out` (room for NP_UHDR_SEAL_CHUNK_PT_BYTES) and their count
 * to `*pt_len`.  Any failure closes the reader and zeroes its key.
 */
np_uhdr_status_t np_uhdr_seal_reader_chunk(np_uhdr_seal_reader_t *r,
                                           const uint8_t *chunk, size_t len,
                                           uint8_t *pt_out, size_t *pt_len);

/* End reading: NP_UHDR_OK only if the FINAL chunk was read (no truncation).
 * Always zeroes the key and closes the reader. */
np_uhdr_status_t np_uhdr_seal_reader_finish(np_uhdr_seal_reader_t *r);

/* ── Naming ───────────────────────────────────────────────────────────────── */

/* The file's name: lowercase hex of its random id, NP_UHDR_FILE_NAME_LEN
 * characters, NUL-terminated in `out` (room for NP_UHDR_FILE_NAME_LEN + 1). */
void np_uhdr_seal_file_name(const uint8_t file_id[NP_UHDR_FILE_ID_LEN],
                            char out[NP_UHDR_FILE_NAME_LEN + 1U]);

/* ── Platform seam (NP-FW-EMMC-002 §C.8.7; OI-UHDRK-14) ─────────────────────
 * FK = KDF(PK, profile id, file id), 32 bytes.  The construction (for example
 * HKDF-SHA-256 with PK as the key and the ids as info) is the reviewer's
 * (OI-EMMC2-02).  Trap-defined on target until the CAAM KDF is wired.
 */
extern np_uhdr_status_t np_uhdr_hal_file_kdf(const uint8_t pk[NP_UHDR_PROFILE_KEY_LEN],
                                             const uint8_t profile_id[NP_UHDR_PROFILE_ID_LEN],
                                             const uint8_t file_id[NP_UHDR_FILE_ID_LEN],
                                             uint8_t fk_out[NP_UHDR_FILE_KEY_LEN]);

#endif /* NP_UHDR_SEAL_H */
