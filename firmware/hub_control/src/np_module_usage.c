/*
 * NeurOne Hub Control Program — per-module running totals (REQ-FWHUB-93)
 * See np_module_usage.h.
 */

#include "np_module_usage.h"

#include <string.h>

#include "np_cfg_store.h"
#include "np_transport.h"

_Static_assert(NP_MU_TABLE_BYTES <= NP_CFG_REPLICA_TABLE_MAX_PAYLOAD,
               "the module usage table must fit the store's table record (np_cfg_store.h)");

/* ── State ─────────────────────────────────────────────────────────────────── */

typedef struct {
    bool     present;      /* fitted this session */
    bool     used;         /* driven this session */
    uint32_t used_ms;      /* drive time this session */
    uint32_t faults[NP_MU_FAULT_CODES + 1U];  /* since the last commit; [NP_MU_FAULT_CODES] = unmatched */
    uint8_t  fault_code[NP_MU_FAULT_CODES];   /* the codes the pending counts above belong to */
} pending_t;

typedef struct {
    bool              on;
    uint32_t          since_ms;
    np_hub_mod_type_t type;
} slot_t;

static np_mu_row_t s_rows[NP_MU_MAX_ROWS];
static pending_t   s_pend[NP_MU_MAX_ROWS];
static size_t      s_n;
static bool        s_loaded;
static uint32_t    s_dropped;
static slot_t      s_slot[NP_HUB_SLOT_MAX];
static uint8_t     s_buf[NP_MU_TABLE_BYTES];   /* encode / decode staging, runner only */

#define CRIT_ENTER() np_transport_hal_enter_critical()
#define CRIT_EXIT()  np_transport_hal_exit_critical()

/* ── Keys ──────────────────────────────────────────────────────────────────── */

np_mu_key_t np_mu_key_uid(const uint8_t uid[NP_MU_ID_LEN])
{
    np_mu_key_t k;
    memset(&k, 0, sizeof k);
    k.kind = (uint8_t)NP_MU_KEY_UID;
    if (uid != NULL) { memcpy(k.id, uid, NP_MU_ID_LEN); }
    return k;
}

np_mu_key_t np_mu_key_port(uint8_t slot, np_hub_mod_type_t type)
{
    np_mu_key_t k;
    memset(&k, 0, sizeof k);
    k.kind  = (uint8_t)NP_MU_KEY_PORT;
    k.id[0] = slot;
    k.id[1] = (uint8_t)type;
    return k;
}

static bool key_eq(const np_mu_key_t *a, const np_mu_key_t *b)
{
    return a->kind == b->kind && memcmp(a->id, b->id, NP_MU_ID_LEN) == 0;
}

static bool key_valid(const np_mu_key_t *k)
{
    return k != NULL && (k->kind == (uint8_t)NP_MU_KEY_UID || k->kind == (uint8_t)NP_MU_KEY_PORT);
}

static bool pending_any(const pending_t *p)
{
    if (p->present || p->used) { return true; }
    for (size_t i = 0U; i <= NP_MU_FAULT_CODES; i++) {
        if (p->faults[i] != 0U) { return true; }
    }
    return false;
}

/* The row for `key`, created if absent.  A full table gives up the row of the
 * module with the fewest sessions present that has nothing pending; NULL if
 * every row is in use this session.  Caller holds the critical section. */
static size_t find_or_add(const np_mu_key_t *key)
{
    for (size_t i = 0U; i < s_n; i++) {
        if (key_eq(&s_rows[i].key, key)) { return i; }
    }
    size_t at = s_n;
    if (s_n >= NP_MU_MAX_ROWS) {
        at = NP_MU_MAX_ROWS;
        for (size_t i = 0U; i < s_n; i++) {
            if (pending_any(&s_pend[i])) { continue; }
            if (at == NP_MU_MAX_ROWS || s_rows[i].sessions_present < s_rows[at].sessions_present) { at = i; }
        }
        if (at == NP_MU_MAX_ROWS) { return NP_MU_MAX_ROWS; }
    } else {
        s_n++;
    }
    memset(&s_rows[at], 0, sizeof s_rows[at]);
    memset(&s_pend[at], 0, sizeof s_pend[at]);
    s_rows[at].key = *key;
    return at;
}

/* ── Codec ─────────────────────────────────────────────────────────────────── */

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Row layout (NP_MU_ROW_BYTES = 52), little-endian:
 *    0 kind | 1 reserved 0 | 2 id[8] | 10 sessions_present | 14 present_s | 18 sessions_used | 22 used_s
 *   26 fault_code[4] | 30 fault_count[4] | 46 faults_other | 50 reserved 0 (2)
 * Header (NP_MU_HDR_BYTES = 4): version | row length | row count (2, LE). */
void np_mu_encode(const np_mu_row_t *rows, size_t n, uint8_t out[NP_MU_TABLE_BYTES])
{
    memset(out, 0, NP_MU_TABLE_BYTES);
    if (n > NP_MU_MAX_ROWS) { n = NP_MU_MAX_ROWS; }
    out[0] = (uint8_t)NP_MU_VERSION;
    out[1] = (uint8_t)NP_MU_ROW_BYTES;
    out[2] = (uint8_t)n;
    out[3] = (uint8_t)(n >> 8);
    for (size_t i = 0U; i < n; i++) {
        uint8_t *r = &out[NP_MU_HDR_BYTES + i * NP_MU_ROW_BYTES];
        const np_mu_row_t *s = &rows[i];
        r[0] = s->key.kind;
        memcpy(&r[2], s->key.id, NP_MU_ID_LEN);
        put32(&r[10], s->sessions_present);
        put32(&r[14], s->present_s);
        put32(&r[18], s->sessions_used);
        put32(&r[22], s->used_s);
        memcpy(&r[26], s->fault_code, NP_MU_FAULT_CODES);
        for (size_t c = 0U; c < NP_MU_FAULT_CODES; c++) { put32(&r[30 + 4U * c], s->fault_count[c]); }
        put32(&r[46], s->faults_other);
    }
}

bool np_mu_decode(const uint8_t in[NP_MU_TABLE_BYTES], np_mu_row_t *rows, size_t cap, size_t *n_out)
{
    if (in == NULL || rows == NULL || n_out == NULL) { return false; }
    const size_t n = (size_t)in[2] | ((size_t)in[3] << 8);
    if (in[0] != (uint8_t)NP_MU_VERSION || in[1] != (uint8_t)NP_MU_ROW_BYTES || n > NP_MU_MAX_ROWS || n > cap) {
        return false;
    }
    for (size_t i = 0U; i < n; i++) {
        const uint8_t *r = &in[NP_MU_HDR_BYTES + i * NP_MU_ROW_BYTES];
        np_mu_row_t *d = &rows[i];
        memset(d, 0, sizeof *d);
        d->key.kind = r[0];
        if (!key_valid(&d->key)) { return false; }
        memcpy(d->key.id, &r[2], NP_MU_ID_LEN);
        d->sessions_present = get32(&r[10]);
        d->present_s        = get32(&r[14]);
        d->sessions_used    = get32(&r[18]);
        d->used_s           = get32(&r[22]);
        memcpy(d->fault_code, &r[26], NP_MU_FAULT_CODES);
        for (size_t c = 0U; c < NP_MU_FAULT_CODES; c++) { d->fault_count[c] = get32(&r[30 + 4U * c]); }
        d->faults_other     = get32(&r[46]);
    }
    *n_out = n;
    return true;
}

/* ── Lifecycle ─────────────────────────────────────────────────────────────── */

void np_mu_init(void)
{
    memset(s_rows, 0, sizeof s_rows);
    memset(s_pend, 0, sizeof s_pend);
    memset(s_slot, 0, sizeof s_slot);
    s_n = 0U;
    s_loaded = false;
    s_dropped = 0U;
}

bool np_mu_loaded(void) { return s_loaded; }

uint32_t np_mu_sessions_dropped(void) { return s_dropped; }

/* Staging for np_mu_load() and np_mu_session_end().  Static, never on a
 * task's stack (each is several KiB); both functions run only on the boot
 * path before the runner starts, or on the runner task, never concurrently. */
static np_mu_row_t s_read[NP_MU_MAX_ROWS];
static np_mu_row_t s_keep_rows[NP_MU_MAX_ROWS];
static pending_t   s_keep_pend[NP_MU_MAX_ROWS];
static np_mu_row_t s_out[NP_MU_MAX_ROWS];

np_hub_status_t np_mu_load(void)
{
    size_t n = 0U;
    np_hub_status_t st = np_cfg_store_replicated_read(NP_CFG_FILE_MODULE_USAGE, s_buf, sizeof s_buf);
    if (st == NP_HUB_ERR_NOT_PRESENT) {
        n = 0U;                       /* never written: every total starts at zero */
    } else if (st != NP_HUB_OK) {
        return st;                    /* unknown is not zero */
    } else if (!np_mu_decode(s_buf, s_read, NP_MU_MAX_ROWS, &n)) {
        return NP_HUB_ERR_STORE_INTEGRITY;
    }

    CRIT_ENTER();
    /* Rows created in RAM before the load hold only pending work (their totals
     * are 0): keep that work and merge it under the rows just read. */
    const size_t keep_n = s_n;
    memcpy(s_keep_rows, s_rows, sizeof(np_mu_row_t) * keep_n);
    memcpy(s_keep_pend, s_pend, sizeof(pending_t) * keep_n);
    memcpy(s_rows, s_read, sizeof(np_mu_row_t) * n);
    memset(s_pend, 0, sizeof s_pend);
    s_n = n;
    for (size_t i = 0U; i < keep_n; i++) {
        const size_t at = find_or_add(&s_keep_rows[i].key);
        if (at < NP_MU_MAX_ROWS) { s_pend[at] = s_keep_pend[i]; }
    }
    s_loaded = true;
    CRIT_EXIT();
    return NP_HUB_OK;
}

void np_mu_session_begin(void)
{
    CRIT_ENTER();
    for (size_t i = 0U; i < s_n; i++) {
        s_pend[i].present = false;
        s_pend[i].used    = false;
        s_pend[i].used_ms = 0U;
    }
    memset(s_slot, 0, sizeof s_slot);
    CRIT_EXIT();
}

void np_mu_mark_present(const np_mu_key_t *key)
{
    if (!key_valid(key)) { return; }
    CRIT_ENTER();
    const size_t at = find_or_add(key);
    if (at < NP_MU_MAX_ROWS) { s_pend[at].present = true; }
    CRIT_EXIT();
}

/* Caller holds the critical section. */
static void add_used_locked(const np_mu_key_t *key, uint32_t on_ms)
{
    const size_t at = find_or_add(key);
    if (at >= NP_MU_MAX_ROWS) { return; }
    s_pend[at].present = true;     /* driven means fitted */
    s_pend[at].used    = true;
    s_pend[at].used_ms = (UINT32_MAX - s_pend[at].used_ms < on_ms) ? UINT32_MAX : s_pend[at].used_ms + on_ms;
}

void np_mu_add_used(const np_mu_key_t *key, uint32_t on_ms)
{
    if (!key_valid(key)) { return; }
    CRIT_ENTER();
    add_used_locked(key, on_ms);
    CRIT_EXIT();
}

void np_mu_slot_on(uint8_t slot, np_hub_mod_type_t type, uint32_t now_ms)
{
    if (slot >= NP_HUB_SLOT_MAX) { return; }
    CRIT_ENTER();
    if (!s_slot[slot].on) {
        s_slot[slot].on       = true;
        s_slot[slot].since_ms = now_ms;
        s_slot[slot].type     = type;
        const np_mu_key_t k = np_mu_key_port(slot, type);
        add_used_locked(&k, 0U);   /* driven, even if for under a tick */
    }
    CRIT_EXIT();
}

/* Caller holds the critical section. */
static void slot_off_locked(uint8_t slot, uint32_t now_ms)
{
    if (!s_slot[slot].on) { return; }
    const uint32_t on_ms = (now_ms >= s_slot[slot].since_ms) ? now_ms - s_slot[slot].since_ms : 0U;
    const np_mu_key_t k = np_mu_key_port(slot, s_slot[slot].type);
    add_used_locked(&k, on_ms);
    s_slot[slot].on = false;
}

void np_mu_slot_off(uint8_t slot, uint32_t now_ms)
{
    if (slot >= NP_HUB_SLOT_MAX) { return; }
    CRIT_ENTER();
    slot_off_locked(slot, now_ms);
    CRIT_EXIT();
}

void np_mu_note_fault(uint8_t slot, uint8_t mod_type, uint8_t code)
{
    if (slot >= NP_HUB_SLOT_MAX) { return; }   /* no module to charge it to */
    const np_mu_key_t k = np_mu_key_port(slot, (np_hub_mod_type_t)mod_type);
    CRIT_ENTER();
    const size_t at = find_or_add(&k);
    if (at < NP_MU_MAX_ROWS) {
        pending_t *p = &s_pend[at];
        size_t c;
        for (c = 0U; c < NP_MU_FAULT_CODES; c++) {
            if (p->fault_code[c] == code && p->faults[c] != 0U) { break; }
        }
        if (c == NP_MU_FAULT_CODES) {
            for (c = 0U; c < NP_MU_FAULT_CODES; c++) {
                if (p->faults[c] == 0U) { p->fault_code[c] = code; break; }
            }
        }
        p->faults[c]++;   /* c == NP_MU_FAULT_CODES: no entry left, counted as unmatched */
    }
    CRIT_EXIT();
}

/* ── Commit ────────────────────────────────────────────────────────────────── */

static uint32_t sat_add(uint32_t a, uint32_t b) { return (UINT32_MAX - a < b) ? UINT32_MAX : a + b; }

/* ms → whole seconds, rounded half up. */
static uint32_t ms_to_s(uint32_t ms) { return ms / 1000U + ((ms % 1000U) >= 500U ? 1U : 0U); }

/* Add one pending fault count to the row: its code's entry, a free entry, or
 * faults_other. */
static void row_add_fault(np_mu_row_t *r, uint8_t code, uint32_t count)
{
    size_t c;
    for (c = 0U; c < NP_MU_FAULT_CODES; c++) {
        if (r->fault_count[c] != 0U && r->fault_code[c] == code) { break; }
    }
    if (c == NP_MU_FAULT_CODES) {
        for (c = 0U; c < NP_MU_FAULT_CODES; c++) {
            if (r->fault_count[c] == 0U) { r->fault_code[c] = code; break; }
        }
    }
    if (c == NP_MU_FAULT_CODES) {
        r->faults_other = sat_add(r->faults_other, count);
    } else {
        r->fault_count[c] = sat_add(r->fault_count[c], count);
    }
}

np_hub_status_t np_mu_session_end(uint32_t session_ms)
{
    CRIT_ENTER();
    for (uint8_t s = 0U; s < NP_HUB_SLOT_MAX; s++) { slot_off_locked(s, session_ms); }
    CRIT_EXIT();

    if (!s_loaded && np_mu_load() != NP_HUB_OK) {
        /* Unknown is not zero: writing now would replace every module's lifetime with this one session. */
        CRIT_ENTER();
        for (size_t i = 0U; i < s_n; i++) { memset(&s_pend[i], 0, sizeof s_pend[i]); }
        s_dropped++;
        CRIT_EXIT();
        return NP_HUB_ERR_STORE_IO;
    }

    size_t n;
    CRIT_ENTER();
    const uint32_t session_s = ms_to_s(session_ms);
    for (size_t i = 0U; i < s_n; i++) {
        np_mu_row_t *r = &s_rows[i];
        pending_t   *p = &s_pend[i];
        if (p->present) {
            r->sessions_present = sat_add(r->sessions_present, 1U);
            r->present_s        = sat_add(r->present_s, session_s);
        }
        if (p->used) {
            r->sessions_used = sat_add(r->sessions_used, 1U);
            r->used_s        = sat_add(r->used_s, ms_to_s(p->used_ms));
        }
        for (size_t c = 0U; c < NP_MU_FAULT_CODES; c++) {
            if (p->faults[c] != 0U) { row_add_fault(r, p->fault_code[c], p->faults[c]); }
        }
        r->faults_other = sat_add(r->faults_other, p->faults[NP_MU_FAULT_CODES]);
        memset(p, 0, sizeof *p);
    }
    n = s_n;
    memcpy(s_out, s_rows, sizeof(np_mu_row_t) * n);
    CRIT_EXIT();

    np_mu_encode(s_out, n, s_buf);
    return np_cfg_store_replicated_write(NP_CFG_FILE_MODULE_USAGE, s_buf, sizeof s_buf);
}

size_t np_mu_snapshot(np_mu_row_t *out, size_t cap)
{
    if (out == NULL) { return 0U; }
    CRIT_ENTER();
    const size_t n = (s_n < cap) ? s_n : cap;
    memcpy(out, s_rows, sizeof(np_mu_row_t) * n);
    CRIT_EXIT();
    return n;
}
