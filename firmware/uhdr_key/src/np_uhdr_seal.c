/*
 * NeurOne Per-Person UHDR Keys — file sealing
 * Document: NP-FW-EMMC-002 §C.8.3, §C.8.7 (OI-PRIV-34, NP-PRIV-ANALYSIS-004)
 * IEC 62304 Class: SW-02 Class B (main processor)
 *
 * See np_uhdr_seal.h for the format.  Pure buffer logic plus, in NPTEST_HOST
 * builds, a SHA-256 stand-in for the file KDF seam.  The AEAD is the shared
 * np_uhdr_hal_aes_gcm_* seam (host-stubbed in np_uhdr_key.c).
 */

#include "np_uhdr_seal.h"
#include "np_uhdr_key.h"
#include "np_uhdr_key_internal.h"

#include <string.h>

typedef char np_uhdr_seal_header_size_check[
    (sizeof(np_uhdr_seal_header_t) == NP_UHDR_SEAL_HEADER_SIZE) ? 1 : -1];

#define SEAL_AAD_LEN  (NP_UHDR_SEAL_HEADER_SIZE + 8U + 1U)   /* 41 */
typedef char np_uhdr_seal_aad_fits[(SEAL_AAD_LEN <= NP_UHDR_AAD_MAX_LEN) ? 1 : -1];

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static void put_u64_be(uint8_t *out, uint64_t v)
{
    for (int i = 7; i >= 0; i--) {
        out[i] = (uint8_t)(v & 0xFFU);
        v >>= 8;
    }
}

static void build_nonce(uint64_t index, uint8_t nonce[NP_UHDR_GCM_NONCE_LEN])
{
    memset(nonce, 0, NP_UHDR_GCM_NONCE_LEN);
    put_u64_be(nonce + (NP_UHDR_GCM_NONCE_LEN - 8U), index);
}

static void build_aad(const np_uhdr_seal_header_t *h, uint64_t index, bool final,
                      uint8_t aad[SEAL_AAD_LEN])
{
    memcpy(aad, h, NP_UHDR_SEAL_HEADER_SIZE);
    put_u64_be(aad + NP_UHDR_SEAL_HEADER_SIZE, index);
    aad[NP_UHDR_SEAL_HEADER_SIZE + 8U] = final ? 1U : 0U;
}

static bool header_ok(const np_uhdr_seal_header_t *h)
{
    return h->magic == NP_UHDR_SEAL_MAGIC && h->format == NP_UHDR_SEAL_FORMAT &&
           h->chunk_pt_bytes == NP_UHDR_SEAL_CHUNK_PT_BYTES && h->reserved == 0U;
}

static np_uhdr_status_t derive_fk(const np_uhdr_profile_ctx_t *profile,
                                  const uint8_t file_id[NP_UHDR_FILE_ID_LEN],
                                  uint8_t fk[NP_UHDR_FILE_KEY_LEN])
{
    if (np_uhdr_hal_file_kdf(profile->pk, profile->profile_id, file_id, fk) != NP_UHDR_OK) {
        np_uhdr_secure_zero(fk, NP_UHDR_FILE_KEY_LEN);
        return NP_UHDR_ERR_KDF;
    }
    return NP_UHDR_OK;
}

/* Seal `len` bytes of w->buf as chunk w->index and hand it to the sink. */
static np_uhdr_status_t emit_chunk(np_uhdr_seal_writer_t *w, size_t len, bool final,
                                   np_uhdr_seal_sink_t sink, void *sink_ctx)
{
    uint8_t chunk[NP_UHDR_SEAL_BLOCK_BYTES];
    uint8_t nonce[NP_UHDR_GCM_NONCE_LEN];
    uint8_t aad[SEAL_AAD_LEN];

    build_nonce(w->index, nonce);
    build_aad(&w->header, w->index, final, aad);
    if (np_uhdr_hal_aes_gcm_encrypt(w->fk, nonce, aad, SEAL_AAD_LEN,
                                    w->buf, len, chunk, chunk + len) != NP_UHDR_OK) {
        return NP_UHDR_ERR_CRYPTO;
    }
    const np_uhdr_status_t st = sink(sink_ctx, chunk, len + NP_UHDR_GCM_TAG_LEN);
    memset(chunk, 0, sizeof(chunk));         /* ciphertext; tidy, not secret */
    if (st != NP_UHDR_OK) {
        return st;
    }
    w->index++;
    return NP_UHDR_OK;
}

/* ── Writing ──────────────────────────────────────────────────────────────── */

np_uhdr_status_t np_uhdr_seal_writer_begin(np_uhdr_seal_writer_t *w,
                                           const np_uhdr_profile_ctx_t *profile,
                                           np_uhdr_seal_header_t *header_out)
{
    if (w == NULL || profile == NULL || header_out == NULL) {
        return NP_UHDR_ERR_INVALID;
    }
    memset(w, 0, sizeof(*w));
    if (!profile->open) {
        return NP_UHDR_ERR_STATE;            /* no open profile, no UHDR write */
    }

    w->header.magic          = NP_UHDR_SEAL_MAGIC;
    w->header.format         = NP_UHDR_SEAL_FORMAT;
    w->header.chunk_pt_bytes = NP_UHDR_SEAL_CHUNK_PT_BYTES;
    w->header.reserved       = 0U;
    if (np_uhdr_hal_trng_generate(w->header.file_id, NP_UHDR_FILE_ID_LEN) != NP_UHDR_OK) {
        memset(w, 0, sizeof(*w));
        return NP_UHDR_ERR_TRNG;
    }
    const np_uhdr_status_t st = derive_fk(profile, w->header.file_id, w->fk);
    if (st != NP_UHDR_OK) {
        np_uhdr_seal_writer_abort(w);
        return st;
    }
    w->open = true;
    memcpy(header_out, &w->header, sizeof(*header_out));
    return NP_UHDR_OK;
}

np_uhdr_status_t np_uhdr_seal_writer_put(np_uhdr_seal_writer_t *w,
                                         const uint8_t *data, size_t len,
                                         np_uhdr_seal_sink_t sink, void *sink_ctx)
{
    if (w == NULL || sink == NULL || (data == NULL && len > 0U)) {
        return NP_UHDR_ERR_INVALID;
    }
    if (!w->open) {
        return NP_UHDR_ERR_STATE;
    }
    while (len > 0U) {
        const size_t room = NP_UHDR_SEAL_CHUNK_PT_BYTES - w->fill;
        const size_t n = (len < room) ? len : room;
        memcpy(w->buf + w->fill, data, n);
        w->fill += n;
        data += n;
        len -= n;
        if (w->fill == NP_UHDR_SEAL_CHUNK_PT_BYTES) {
            const np_uhdr_status_t st =
                emit_chunk(w, NP_UHDR_SEAL_CHUNK_PT_BYTES, false, sink, sink_ctx);
            if (st != NP_UHDR_OK) {
                np_uhdr_seal_writer_abort(w);
                return st;
            }
            np_uhdr_secure_zero(w->buf, sizeof(w->buf));
            w->fill = 0U;
        }
    }
    return NP_UHDR_OK;
}

np_uhdr_status_t np_uhdr_seal_writer_finish(np_uhdr_seal_writer_t *w,
                                            np_uhdr_seal_sink_t sink, void *sink_ctx)
{
    if (w == NULL || sink == NULL) {
        return NP_UHDR_ERR_INVALID;
    }
    if (!w->open) {
        return NP_UHDR_ERR_STATE;
    }
    /* The tail is always shorter than a full chunk (a full one was emitted by
     * put), so the final chunk is always shorter than a block on the medium. */
    const np_uhdr_status_t st = emit_chunk(w, w->fill, true, sink, sink_ctx);
    np_uhdr_seal_writer_abort(w);            /* zero key and buffer either way */
    return st;
}

void np_uhdr_seal_writer_abort(np_uhdr_seal_writer_t *w)
{
    if (w == NULL) {
        return;
    }
    np_uhdr_secure_zero(w->fk, sizeof(w->fk));
    np_uhdr_secure_zero(w->buf, sizeof(w->buf));
    w->fill = 0U;
    w->open = false;
}

/* ── Reading ──────────────────────────────────────────────────────────────── */

static void reader_close(np_uhdr_seal_reader_t *r)
{
    np_uhdr_secure_zero(r->fk, sizeof(r->fk));
    r->open = false;
}

np_uhdr_status_t np_uhdr_seal_reader_begin(np_uhdr_seal_reader_t *r,
                                           const np_uhdr_profile_ctx_t *profile,
                                           const np_uhdr_seal_header_t *header)
{
    if (r == NULL || profile == NULL || header == NULL) {
        return NP_UHDR_ERR_INVALID;
    }
    memset(r, 0, sizeof(*r));
    if (!profile->open) {
        return NP_UHDR_ERR_STATE;
    }
    if (!header_ok(header)) {
        return NP_UHDR_ERR_INVALID;
    }
    memcpy(&r->header, header, sizeof(r->header));
    const np_uhdr_status_t st = derive_fk(profile, header->file_id, r->fk);
    if (st != NP_UHDR_OK) {
        reader_close(r);
        return st;
    }
    r->open = true;
    return NP_UHDR_OK;
}

np_uhdr_status_t np_uhdr_seal_reader_chunk(np_uhdr_seal_reader_t *r,
                                           const uint8_t *chunk, size_t len,
                                           uint8_t *pt_out, size_t *pt_len)
{
    if (r == NULL || chunk == NULL || pt_out == NULL || pt_len == NULL) {
        return NP_UHDR_ERR_INVALID;
    }
    *pt_len = 0U;
    if (!r->open || r->final_seen) {
        reader_close(r);
        return NP_UHDR_ERR_STATE;            /* nothing may follow the final chunk */
    }
    if (len < NP_UHDR_GCM_TAG_LEN || len > NP_UHDR_SEAL_BLOCK_BYTES) {
        reader_close(r);
        return NP_UHDR_ERR_INVALID;
    }
    const bool final = (len < NP_UHDR_SEAL_BLOCK_BYTES);
    const size_t ct_len = len - NP_UHDR_GCM_TAG_LEN;
    uint8_t nonce[NP_UHDR_GCM_NONCE_LEN];
    uint8_t aad[SEAL_AAD_LEN];
    build_nonce(r->index, nonce);
    build_aad(&r->header, r->index, final, aad);

    const np_uhdr_status_t st = np_uhdr_hal_aes_gcm_decrypt(r->fk, nonce, aad, SEAL_AAD_LEN,
                                                            chunk, ct_len, chunk + ct_len,
                                                            pt_out);
    if (st != NP_UHDR_OK) {
        np_uhdr_secure_zero(pt_out, ct_len);
        reader_close(r);
        return (st == NP_UHDR_ERR_AUTH) ? NP_UHDR_ERR_AUTH : NP_UHDR_ERR_CRYPTO;
    }
    *pt_len = ct_len;
    r->index++;
    r->final_seen = final;
    return NP_UHDR_OK;
}

np_uhdr_status_t np_uhdr_seal_reader_finish(np_uhdr_seal_reader_t *r)
{
    if (r == NULL) {
        return NP_UHDR_ERR_INVALID;
    }
    const bool complete = r->open && r->final_seen;
    reader_close(r);
    return complete ? NP_UHDR_OK : NP_UHDR_ERR_AUTH;   /* truncated: refused */
}

/* ── Naming ───────────────────────────────────────────────────────────────── */

void np_uhdr_seal_file_name(const uint8_t file_id[NP_UHDR_FILE_ID_LEN],
                            char out[NP_UHDR_FILE_NAME_LEN + 1U])
{
    static const char HEX_DIGITS[] = "0123456789abcdef";
    for (size_t i = 0; i < NP_UHDR_FILE_ID_LEN; i++) {
        out[2U * i]      = HEX_DIGITS[(file_id[i] >> 4) & 0x0FU];
        out[2U * i + 1U] = HEX_DIGITS[file_id[i] & 0x0FU];
    }
    out[NP_UHDR_FILE_NAME_LEN] = '\0';
}

/* ── Host test stub (NPTEST_HOST) ────────────────────────────────────────────
 * NOT compiled into target firmware.  FK = SHA-256(PK || profile id || file id
 * || "NPFK").  Not cryptography; it makes another profile or another file id
 * derive a different key so the tags fail on host CI.
 */
#ifdef NPTEST_HOST

#include "np_crypto.h"

np_uhdr_status_t np_uhdr_hal_file_kdf(const uint8_t pk[NP_UHDR_PROFILE_KEY_LEN],
                                      const uint8_t profile_id[NP_UHDR_PROFILE_ID_LEN],
                                      const uint8_t file_id[NP_UHDR_FILE_ID_LEN],
                                      uint8_t fk_out[NP_UHDR_FILE_KEY_LEN])
{
    if (pk == NULL || profile_id == NULL || file_id == NULL || fk_out == NULL) {
        return NP_UHDR_ERR_INVALID;
    }
    uint8_t buf[NP_UHDR_PROFILE_KEY_LEN + NP_UHDR_PROFILE_ID_LEN + NP_UHDR_FILE_ID_LEN + 4U];
    size_t off = 0;
    memcpy(buf + off, pk, NP_UHDR_PROFILE_KEY_LEN);        off += NP_UHDR_PROFILE_KEY_LEN;
    memcpy(buf + off, profile_id, NP_UHDR_PROFILE_ID_LEN); off += NP_UHDR_PROFILE_ID_LEN;
    memcpy(buf + off, file_id, NP_UHDR_FILE_ID_LEN);       off += NP_UHDR_FILE_ID_LEN;
    memcpy(buf + off, "NPFK", 4U);
    np_sha256(buf, (uint32_t)sizeof(buf), fk_out);
    np_uhdr_secure_zero(buf, sizeof(buf));
    return NP_UHDR_OK;
}

#endif /* NPTEST_HOST */
