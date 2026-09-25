/*
 * NeurOne Hub Control Program — Session Log eMMC Backing (OI-LOG-01..04)
 * Document: NP-FW-HUB-001 Rev 1 §6, NP-FW-EMMC-001 Rev 1 §12
 *
 * Implements the four session-logger HAL entry points (declared extern in
 * np_session_log.h) on top of a per-partition, block-coalescing append writer.
 * See np_log_backend.h for the encryption boundary and durability model.
 *
 * Records are handed down as PLAINTEXT; the UHDR/SHDR partitions are already
 * AES-256-XTS mounted, so the LittleFS-file HAL (OI-LOG-05..07) writes
 * ciphertext at rest without this layer ever seeing key material.
 */

#include "np_log_backend.h"
#include "np_config.h"       /* bootloader: NP_UHDR_SIZE_LBA, NP_SHDR_SIZE_LBA,
                              *             NP_EMMC_SECTOR_SIZE               */
#include <string.h>

/* ── Per-partition writer state ──────────────────────────────────────────────── */

typedef struct {
    np_log_part_t part;
    uint8_t  stage[NP_LOG_STAGE_BYTES];
    size_t   stage_pos;          /* bytes buffered in stage[] awaiting a block  */
    uint64_t bytes_committed;    /* bytes handed to np_log_hal_part_append()    */
    uint64_t capacity_bytes;     /* partition size in bytes                     */
    uint64_t segment_bytes;      /* bytes in the file currently open (OI-LFS-11)*/
    uint64_t segment_cap;        /* NP_LOG_SEGMENT_MAX_BYTES (host-overridable) */
    bool     opened;
    bool     faulted;            /* a write failed or the log is full — sticky
                                  * for SHDR; for UHDR, until the next session  */
} log_part_state_t;

static log_part_state_t s_uhdr;
static log_part_state_t s_shdr;

/* ── Init ─────────────────────────────────────────────────────────────────────── */

np_hub_status_t np_log_backend_init(void)
{
    memset(&s_uhdr, 0, sizeof s_uhdr);
    memset(&s_shdr, 0, sizeof s_shdr);
    s_uhdr.part = NP_LOG_PART_UHDR;
    s_shdr.part = NP_LOG_PART_SHDR;
    s_uhdr.capacity_bytes = (uint64_t)NP_UHDR_SIZE_LBA * (uint64_t)NP_EMMC_SECTOR_SIZE;
    s_shdr.capacity_bytes = (uint64_t)NP_SHDR_SIZE_LBA * (uint64_t)NP_EMMC_SECTOR_SIZE;
    s_uhdr.segment_cap    = NP_LOG_SEGMENT_MAX_BYTES;
    s_shdr.segment_cap    = NP_LOG_SEGMENT_MAX_BYTES;

    /* UHDR is per session and is not opened here — at boot it is not even
     * mounted (np_uhdr_key_unlock() mounts it).  OI-LFS-11. */
    np_hub_status_t rc_s = np_log_hal_part_open(NP_LOG_PART_SHDR, 0U);
    if (rc_s == NP_HUB_OK) { s_shdr.opened = true; } else { s_shdr.faulted = true; }
    return rc_s;
}

/* ── Internal write path ──────────────────────────────────────────────────────── */

/*
 * commit — hand exactly `len` bytes to the LittleFS-file HAL, guarding the
 * partition capacity first.  Any failure latches the partition faulted so no
 * further partial record is written after a torn write.
 */
static np_hub_status_t commit(log_part_state_t *st, const uint8_t *buf, size_t len)
{
    if (len == 0U) {
        return NP_HUB_OK;
    }
    if (st->bytes_committed + (uint64_t)len > st->capacity_bytes) {
        st->faulted = true;
        return NP_HUB_ERR_LOG_FULL;
    }
    /* OI-LFS-11: littlefs refuses to grow one file past file_max.  Refusing
     * here instead keeps the file's committed prefix whole and makes the
     * reason a status, not an lfs error from deep in the HAL. */
    if (st->segment_bytes + (uint64_t)len > st->segment_cap) {
        st->faulted = true;
        return NP_HUB_ERR_LOG_FULL;
    }
    np_hub_status_t rc = np_log_hal_part_append(st->part, buf, len);
    if (rc != NP_HUB_OK) {
        st->faulted = true;
        return rc;
    }
    st->bytes_committed += (uint64_t)len;
    st->segment_bytes   += (uint64_t)len;
    return NP_HUB_OK;
}

static np_hub_status_t append(log_part_state_t *st, const uint8_t *buf, size_t len)
{
    if (len == 0U) {
        return NP_HUB_OK;
    }
    if (buf == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    if (!st->opened && !st->faulted && st->part == NP_LOG_PART_UHDR) {
        /* No session: UHDR has nowhere to go (OI-LFS-11). */
        return NP_HUB_ERR_NO_SESSION;
    }
    if (st->faulted || !st->opened) {
        /* Logging is down (init failed, full, or a prior torn write). */
        return NP_HUB_ERR_LOG_FULL;
    }

    /* 1. Top up the current partial block. */
    if (st->stage_pos > 0U) {
        size_t room = NP_LOG_STAGE_BYTES - st->stage_pos;
        size_t n    = (len < room) ? len : room;
        memcpy(st->stage + st->stage_pos, buf, n);
        st->stage_pos += n;
        buf += n;
        len -= n;
        if (st->stage_pos == NP_LOG_STAGE_BYTES) {
            np_hub_status_t rc = commit(st, st->stage, NP_LOG_STAGE_BYTES);
            st->stage_pos = 0U;
            if (rc != NP_HUB_OK) {
                return rc;
            }
        }
    }

    /* 2. Pass whole blocks straight through — avoids copying large EEG sample
     *    blocks (12 KB/s) through the staging buffer. */
    while (len >= NP_LOG_STAGE_BYTES) {
        np_hub_status_t rc = commit(st, buf, NP_LOG_STAGE_BYTES);
        if (rc != NP_HUB_OK) {
            return rc;
        }
        buf += NP_LOG_STAGE_BYTES;
        len -= NP_LOG_STAGE_BYTES;
    }

    /* 3. Stage the sub-block remainder (stage_pos is 0 here). */
    if (len > 0U) {
        memcpy(st->stage + st->stage_pos, buf, len);
        st->stage_pos += len;
    }
    return NP_HUB_OK;
}

/*
 * flush — commit any buffered tail (exact byte count, no padding) then issue a
 * durable block-device sync.  An append-mode file cannot rewrite its tail, so
 * flush writes exactly what is buffered and resets the staging cursor; the next
 * append continues immediately after.
 */
static np_hub_status_t flush(log_part_state_t *st)
{
    if (st->faulted || !st->opened) {
        return NP_HUB_ERR_LOG_FULL;
    }
    if (st->stage_pos > 0U) {
        np_hub_status_t rc = commit(st, st->stage, st->stage_pos);
        st->stage_pos = 0U;
        if (rc != NP_HUB_OK) {
            return rc;
        }
    }
    return np_log_hal_part_sync(st->part);
}

/* ── UHDR session files (OI-LFS-11) ───────────────────────────────────────────── */

np_hub_status_t np_log_backend_session_end(void)
{
    log_part_state_t *st = &s_uhdr;
    if (!st->opened) {
        st->stage_pos = 0U;
        return NP_HUB_OK;
    }
    /* The tail belongs to THIS file: flush it before closing. */
    np_hub_status_t rc = flush(st);
    np_hub_status_t rc_close = np_log_hal_part_close(st->part);
    st->opened        = false;
    st->stage_pos     = 0U;
    st->segment_bytes = 0U;
    return (rc != NP_HUB_OK) ? rc : rc_close;
}

np_hub_status_t np_log_backend_session_begin(uint64_t session_counter)
{
    log_part_state_t *st = &s_uhdr;

    /* A session that was never ended still owns its tail.  Its end status is
     * deliberately not returned: it describes the previous file, and a caller
     * reading it as "the new session failed" would refuse a session whose file
     * opened correctly. */
    (void)np_log_backend_session_end();

    st->faulted       = false;     /* a new file: the previous one's fault does
                                    * not carry over */
    st->segment_bytes = 0U;
    np_hub_status_t rc = np_log_hal_part_open(st->part, session_counter);
    if (rc != NP_HUB_OK) {
        st->faulted = true;        /* exclusive create failed — never fall back
                                    * to appending to an existing file */
        return rc;
    }
    st->opened = true;
    return NP_HUB_OK;
}

/* ── Public HAL entry points (OI-LOG-01..04) ──────────────────────────────────── */

np_hub_status_t np_log_hal_uhdr_append(const uint8_t *buf, size_t len)
{
    return append(&s_uhdr, buf, len);
}

np_hub_status_t np_log_hal_shdr_append(const uint8_t *buf, size_t len)
{
    return append(&s_shdr, buf, len);
}

np_hub_status_t np_log_hal_uhdr_flush(void)
{
    return flush(&s_uhdr);
}

np_hub_status_t np_log_hal_shdr_flush(void)
{
    return flush(&s_shdr);
}

/* ── Host-test stubs + hooks (NPTEST_HOST) ────────────────────────────────────── */

#ifdef NPTEST_HOST

#define HOST_CAP_BYTES 65536U

#define HOST_MAX_FILES 16U

typedef struct {
    uint8_t  buf[HOST_CAP_BYTES];
    size_t   len;
    unsigned syncs;
    size_t   synced_len;   /* len when the last sync was issued */
    bool     fail_next;
    /* OI-LFS-11: which files exist and how much of the capture each holds. */
    bool     open;
    unsigned opens;
    unsigned closes;
    uint64_t segments[HOST_MAX_FILES];
    size_t   seg_start[HOST_MAX_FILES];
    unsigned n_segments;
} host_part_t;

static host_part_t h_uhdr;
static host_part_t h_shdr;

static host_part_t *host_for(np_log_part_t p)
{
    return (p == NP_LOG_PART_UHDR) ? &h_uhdr : &h_shdr;
}

static log_part_state_t *state_for(np_log_part_t p)
{
    return (p == NP_LOG_PART_UHDR) ? &s_uhdr : &s_shdr;
}

np_hub_status_t np_log_hal_part_open(np_log_part_t part, uint64_t segment)
{
    host_part_t *h = host_for(part);
    if (part == NP_LOG_PART_UHDR) {
        /* Exclusive create: an existing session file is an error. */
        for (unsigned i = 0U; i < h->n_segments; i++) {
            if (h->segments[i] == segment) {
                return NP_HUB_ERR_LOG_EXISTS;
            }
        }
    }
    if (h->n_segments < HOST_MAX_FILES) {
        h->segments[h->n_segments]  = segment;
        h->seg_start[h->n_segments] = h->len;
        h->n_segments++;
    }
    h->open = true;
    h->opens++;
    return NP_HUB_OK;
}

np_hub_status_t np_log_hal_part_close(np_log_part_t part)
{
    host_part_t *h = host_for(part);
    h->open = false;
    h->closes++;
    return NP_HUB_OK;
}

np_hub_status_t np_log_hal_part_append(np_log_part_t part, const uint8_t *buf, size_t len)
{
    host_part_t *h = host_for(part);
    if (h->fail_next) {
        h->fail_next = false;
        return NP_HUB_ERR_GENERIC;
    }
    if (!h->open) {
        return NP_HUB_ERR_GENERIC;   /* a write with no file open is a bug */
    }
    if (h->len + len > HOST_CAP_BYTES) {
        return NP_HUB_ERR_GENERIC;
    }
    memcpy(h->buf + h->len, buf, len);
    h->len += len;
    return NP_HUB_OK;
}

np_hub_status_t np_log_hal_part_sync(np_log_part_t part)
{
    host_part_t *h = host_for(part);
    h->syncs++;
    h->synced_len = h->len;   /* a sync covers exactly what was appended before it */
    return NP_HUB_OK;
}

void np_log_test_reset(void)
{
    memset(&h_uhdr, 0, sizeof h_uhdr);
    memset(&h_shdr, 0, sizeof h_shdr);
}

size_t np_log_test_captured_len(np_log_part_t part)
{
    return host_for(part)->len;
}

const uint8_t *np_log_test_captured(np_log_part_t part)
{
    return host_for(part)->buf;
}

unsigned np_log_test_sync_count(np_log_part_t part)
{
    return host_for(part)->syncs;
}

size_t np_log_test_synced_len(np_log_part_t part)
{
    return host_for(part)->synced_len;
}

void np_log_test_fail_next_append(np_log_part_t part)
{
    host_for(part)->fail_next = true;
}

void np_log_test_set_capacity(np_log_part_t part, uint64_t bytes)
{
    state_for(part)->capacity_bytes = bytes;
}

void np_log_test_set_segment_cap(np_log_part_t part, uint64_t bytes)
{
    state_for(part)->segment_cap = bytes;
}

unsigned np_log_test_open_count(np_log_part_t part)
{
    return host_for(part)->opens;
}

unsigned np_log_test_close_count(np_log_part_t part)
{
    return host_for(part)->closes;
}

uint64_t np_log_test_last_segment(np_log_part_t part)
{
    host_part_t *h = host_for(part);
    return (h->n_segments > 0U) ? h->segments[h->n_segments - 1U] : 0U;
}

size_t np_log_test_segment_len(np_log_part_t part, unsigned nth)
{
    host_part_t *h = host_for(part);
    if (nth >= h->n_segments) {
        return 0U;
    }
    size_t end = (nth + 1U < h->n_segments) ? h->seg_start[nth + 1U] : h->len;
    return end - h->seg_start[nth];
}

#endif /* NPTEST_HOST */
