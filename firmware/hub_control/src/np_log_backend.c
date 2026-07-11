/*
 * NeurOne Hub Control Program — Session Log eMMC Backing (OI-LOG-01..04)
 * Document: NP-FW-HUB-001 Rev A §6, NP-FW-EMMC-001 Rev A §12
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
    bool     opened;
    bool     faulted;            /* sticky: a write failed or the log is full   */
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

    np_hub_status_t rc_u = np_log_hal_part_open(NP_LOG_PART_UHDR);
    if (rc_u == NP_HUB_OK) { s_uhdr.opened = true; } else { s_uhdr.faulted = true; }

    np_hub_status_t rc_s = np_log_hal_part_open(NP_LOG_PART_SHDR);
    if (rc_s == NP_HUB_OK) { s_shdr.opened = true; } else { s_shdr.faulted = true; }

    /* Report the first failure so bring-up can surface a logging fault. */
    return (rc_u != NP_HUB_OK) ? rc_u : rc_s;
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
    np_hub_status_t rc = np_log_hal_part_append(st->part, buf, len);
    if (rc != NP_HUB_OK) {
        st->faulted = true;
        return rc;
    }
    st->bytes_committed += (uint64_t)len;
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

typedef struct {
    uint8_t  buf[HOST_CAP_BYTES];
    size_t   len;
    unsigned syncs;
    bool     fail_next;
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

np_hub_status_t np_log_hal_part_open(np_log_part_t part)
{
    (void)part;
    return NP_HUB_OK;
}

np_hub_status_t np_log_hal_part_append(np_log_part_t part, const uint8_t *buf, size_t len)
{
    host_part_t *h = host_for(part);
    if (h->fail_next) {
        h->fail_next = false;
        return NP_HUB_ERR_GENERIC;
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
    host_for(part)->syncs++;
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

void np_log_test_fail_next_append(np_log_part_t part)
{
    host_for(part)->fail_next = true;
}

void np_log_test_set_capacity(np_log_part_t part, uint64_t bytes)
{
    state_for(part)->capacity_bytes = bytes;
}

#endif /* NPTEST_HOST */
