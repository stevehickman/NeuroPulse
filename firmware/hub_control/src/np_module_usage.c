/*
 * NeurOne Hub Control Program — per-module running totals (REQ-FWHUB-93)
 * See np_module_usage.h.
 *
 * Two tables:
 *   s_rows  the stored totals, one row per module.  Touched only by the boot
 *           path and the runner (np_mu_load, np_mu_session_end,
 *           np_mu_snapshot), never concurrently, so it needs no lock.
 *   s_pend  work not yet committed — this session's presence, use and drive
 *           time, and the faults, mates, dose and throttles since the last
 *           commit — keyed by module, so it is independent of where (or
 *           whether) the module has a row.  Any task may add a fault or a
 *           mate, so it is changed only inside the critical section.
 * A load therefore replaces s_rows without disturbing anything pending, and a
 * commit folds s_pend into s_rows, then writes s_rows.
 *
 * s_img is the stored record as it was read (or last written).  A commit
 * writes the version-1 fields of each row into it in place, so whatever a
 * newer firmware stored after them is carried through unchanged
 * (NP-FW-NVRAM-001 §7.2, the rule for the record); a row created here starts
 * with that tail zeroed.
 */

#include "np_module_usage.h"

#include <string.h>

#include "np_cfg_store.h"
#include "np_transport.h"

_Static_assert(NP_MU_TABLE_BYTES <= NP_CFG_REPLICA_TABLE_MAX_PAYLOAD,
               "the module usage table must fit the store's table record (np_cfg_store.h)");
_Static_assert(NP_MU_HDR_BYTES + NP_MU_MAX_ROWS * NP_MU_ROW_BYTES <= NP_MU_TABLE_BYTES,
               "every row must fit the record at the row length this firmware writes");
_Static_assert(NP_MU_ROW_MAX_BYTES >= NP_MU_ROW_BYTES && NP_MU_ROW_MAX_BYTES <= 255U,
               "the header's row length is one byte, and must admit this firmware's row");

/* ── State ─────────────────────────────────────────────────────────────────── */

typedef struct {
    np_mu_key_t key;          /* kind NP_MU_KEY_EMPTY = free entry */
    bool        present;      /* fitted this session */
    bool        used;         /* driven this session */
    uint32_t    used_ms;      /* drive time this session */
    uint8_t     fault_code[NP_MU_FAULT_CODES];
    uint32_t    faults[NP_MU_FAULT_CODES + 1U];   /* [NP_MU_FAULT_CODES] = codes with no entry */
    uint32_t    mates;
    uint32_t    dose;         /* mJ/cm² */
    uint32_t    throttles;
    uint8_t     flags;        /* NP_MU_FAULT_* to OR into the row */
    bool        lost;         /* a commit that should have added to this module was dropped */
} pending_t;

typedef struct {
    bool              on;
    uint32_t          since_ms;
    np_hub_mod_type_t type;
} slot_t;

static np_mu_row_t s_rows[NP_MU_MAX_ROWS];
static size_t      s_n;
static bool        s_loaded;
static uint32_t    s_dropped;
static pending_t   s_pend[NP_MU_MAX_ROWS];
static slot_t      s_slot[NP_HUB_SLOT_MAX];
static uint8_t     s_img[NP_MU_TABLE_BYTES];   /* the stored record's image, boot and runner only */
static uint8_t     s_img_ver;                  /* its header's version and row length */
static uint8_t     s_img_row_len;
static bool        s_fresh[NP_MU_MAX_ROWS];    /* row created since the image was read: tail zeroed at commit */

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

/* The pending entry for `key`, created if absent; NULL if the pending table is
 * full.  Caller holds the critical section. */
static pending_t *pend_for(const np_mu_key_t *key)
{
    pending_t *free_entry = NULL;
    for (size_t i = 0U; i < NP_MU_MAX_ROWS; i++) {
        if (s_pend[i].key.kind == (uint8_t)NP_MU_KEY_EMPTY) {
            if (free_entry == NULL) { free_entry = &s_pend[i]; }
        } else if (key_eq(&s_pend[i].key, key)) {
            return &s_pend[i];
        }
    }
    if (free_entry != NULL) {
        memset(free_entry, 0, sizeof *free_entry);
        free_entry->key = *key;
    }
    return free_entry;
}

static bool has_pending(const np_mu_key_t *key)
{
    for (size_t i = 0U; i < NP_MU_MAX_ROWS; i++) {
        if (s_pend[i].key.kind != (uint8_t)NP_MU_KEY_EMPTY && key_eq(&s_pend[i].key, key)) { return true; }
    }
    return false;
}

/* The stored row for `key`, created if absent.  A full table gives up the row
 * of the module with the fewest sessions present that has nothing pending
 * (UC-208), and the newcomer's row starts with detail_lost set: the helmet has
 * forgotten a module, so it cannot say this one's totals start here for the
 * first time (NP-FW-NVRAM-001 §6.3).  NP_MU_MAX_ROWS if every row is pending.
 * Runner only, called with the critical section held so the pending table it
 * consults stands still. */
static size_t row_for(const np_mu_key_t *key)
{
    for (size_t i = 0U; i < s_n; i++) {
        if (key_eq(&s_rows[i].key, key)) { return i; }
    }
    size_t at = s_n;
    bool gave_up = false;
    if (s_n >= NP_MU_MAX_ROWS) {
        at = NP_MU_MAX_ROWS;
        for (size_t i = 0U; i < s_n; i++) {
            if (has_pending(&s_rows[i].key)) { continue; }
            if (at == NP_MU_MAX_ROWS || s_rows[i].sessions_present < s_rows[at].sessions_present) { at = i; }
        }
        if (at == NP_MU_MAX_ROWS) { return NP_MU_MAX_ROWS; }
        gave_up = true;
    } else {
        s_n++;
    }
    memset(&s_rows[at], 0, sizeof s_rows[at]);
    s_rows[at].key = *key;
    s_rows[at].detail_lost = gave_up ? 1U : 0U;
    s_fresh[at] = true;
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

/* Row layout, version 1 (NP_MU_ROW_BYTES = 64), little-endian.  These offsets
 * are fixed for ever: a later version appends after byte 63 and never moves,
 * resizes or reuses a field here, the reserved byte included (NP-FW-NVRAM-001
 * §7.2).
 *    0 kind | 1 fault_flags | 2 id[8] | 10 sessions_present | 14 present_s
 *   18 sessions_used | 22 used_s | 26 fault_code[4] | 30 fault_count[4]
 *   46 faults_other | 50 mate_cycles | 54 dose_accum | 58 throttle_count
 *   62 detail_lost | 63 reserved 0
 * Header (NP_MU_HDR_BYTES = 4): version | row length | row count (2, LE).
 * Rows follow at the header's row length; the rest of the record is zero. */
#define OFF_KIND      0U
#define OFF_FLAGS     1U
#define OFF_ID        2U
#define OFF_SP       10U
#define OFF_PS       14U
#define OFF_SU       18U
#define OFF_US       22U
#define OFF_FCODE    26U
#define OFF_FCOUNT   30U
#define OFF_FOTHER   46U
#define OFF_MATES    50U
#define OFF_DOSE     54U
#define OFF_THROT    58U
#define OFF_LOST     62U
_Static_assert(OFF_LOST + 2U == NP_MU_ROW_BYTES, "the version-1 fields and the reserved byte fill the row");

/* Write the version-1 fields of `s` into the row at `r`; bytes past them untouched. */
static void put_row(uint8_t *r, const np_mu_row_t *s)
{
    r[OFF_KIND]  = s->key.kind;
    r[OFF_FLAGS] = s->fault_flags;
    memcpy(&r[OFF_ID], s->key.id, NP_MU_ID_LEN);
    put32(&r[OFF_SP], s->sessions_present);
    put32(&r[OFF_PS], s->present_s);
    put32(&r[OFF_SU], s->sessions_used);
    put32(&r[OFF_US], s->used_s);
    memcpy(&r[OFF_FCODE], s->fault_code, NP_MU_FAULT_CODES);
    for (size_t c = 0U; c < NP_MU_FAULT_CODES; c++) { put32(&r[OFF_FCOUNT + 4U * c], s->fault_count[c]); }
    put32(&r[OFF_FOTHER], s->faults_other);
    put32(&r[OFF_MATES], s->mate_cycles);
    put32(&r[OFF_DOSE], s->dose_accum);
    put32(&r[OFF_THROT], s->throttle_count);
    r[OFF_LOST]  = (s->detail_lost != 0U) ? 1U : 0U;
}

static void get_row(const uint8_t *r, np_mu_row_t *d)
{
    memset(d, 0, sizeof *d);
    d->key.kind         = r[OFF_KIND];
    d->fault_flags      = r[OFF_FLAGS];
    memcpy(d->key.id, &r[OFF_ID], NP_MU_ID_LEN);
    d->sessions_present = get32(&r[OFF_SP]);
    d->present_s        = get32(&r[OFF_PS]);
    d->sessions_used    = get32(&r[OFF_SU]);
    d->used_s           = get32(&r[OFF_US]);
    memcpy(d->fault_code, &r[OFF_FCODE], NP_MU_FAULT_CODES);
    for (size_t c = 0U; c < NP_MU_FAULT_CODES; c++) { d->fault_count[c] = get32(&r[OFF_FCOUNT + 4U * c]); }
    d->faults_other     = get32(&r[OFF_FOTHER]);
    d->mate_cycles      = get32(&r[OFF_MATES]);
    d->dose_accum       = get32(&r[OFF_DOSE]);
    d->throttle_count   = get32(&r[OFF_THROT]);
    d->detail_lost      = (r[OFF_LOST] != 0U) ? 1U : 0U;
}

static void put_hdr(uint8_t *out, uint8_t ver, uint8_t row_len, size_t n)
{
    out[0] = ver;
    out[1] = row_len;
    out[2] = (uint8_t)n;
    out[3] = (uint8_t)(n >> 8);
}

void np_mu_encode(const np_mu_row_t *rows, size_t n, uint8_t out[NP_MU_TABLE_BYTES])
{
    memset(out, 0, NP_MU_TABLE_BYTES);
    if (n > NP_MU_MAX_ROWS) { n = NP_MU_MAX_ROWS; }
    put_hdr(out, (uint8_t)NP_MU_VERSION, (uint8_t)NP_MU_ROW_BYTES, n);
    for (size_t i = 0U; i < n; i++) { put_row(&out[NP_MU_HDR_BYTES + i * NP_MU_ROW_BYTES], &rows[i]); }
}

/* The header and key check np_mu_decode makes, and the version, row length and
 * row count it reads.  Any version >= 1 is read: the row length steps from row
 * to row, so a longer row of a newer firmware is read for the fields this one
 * knows (§7.2). */
static bool hdr_ok(const uint8_t *in, size_t cap, size_t *n, uint8_t *ver, uint8_t *row_len)
{
    *ver     = in[0];
    *row_len = in[1];
    *n       = (size_t)in[2] | ((size_t)in[3] << 8);
    if (*ver == 0U || *row_len < NP_MU_ROW_BYTES || *row_len > NP_MU_ROW_MAX_BYTES ||
        *n > NP_MU_MAX_ROWS || *n > cap) {
        return false;
    }
    for (size_t i = 0U; i < *n; i++) {
        const uint8_t kind = in[NP_MU_HDR_BYTES + i * *row_len];
        if (kind != (uint8_t)NP_MU_KEY_UID && kind != (uint8_t)NP_MU_KEY_PORT) { return false; }
    }
    return true;
}

/* Validates the whole record before writing anything into `rows`, so a refused
 * record leaves the caller's table as it was. */
bool np_mu_decode(const uint8_t in[NP_MU_TABLE_BYTES], np_mu_row_t *rows, size_t cap, size_t *n_out)
{
    if (in == NULL || rows == NULL || n_out == NULL) { return false; }
    size_t n;
    uint8_t ver, row_len;
    if (!hdr_ok(in, cap, &n, &ver, &row_len)) { return false; }
    for (size_t i = 0U; i < n; i++) { get_row(&in[NP_MU_HDR_BYTES + i * row_len], &rows[i]); }
    *n_out = n;
    return true;
}

/* Bring s_img up to date with s_rows: each row's version-1 fields in place at
 * the image's row length, a row created since the read zeroed first, and
 * everything past the last row zero.  The header keeps the higher of the
 * stored version and this firmware's, because the tails it carries are that
 * version's. */
static void image_update(void)
{
    const size_t rl = s_img_row_len;
    for (size_t i = 0U; i < s_n; i++) {
        uint8_t *r = &s_img[NP_MU_HDR_BYTES + i * rl];
        if (s_fresh[i]) { memset(r, 0, rl); s_fresh[i] = false; }
        put_row(r, &s_rows[i]);
    }
    const size_t used = NP_MU_HDR_BYTES + s_n * rl;
    memset(&s_img[used], 0, NP_MU_TABLE_BYTES - used);
    put_hdr(s_img, (s_img_ver > (uint8_t)NP_MU_VERSION) ? s_img_ver : (uint8_t)NP_MU_VERSION, (uint8_t)rl, s_n);
}

/* An empty image at this firmware's version and row length. */
static void image_empty(void)
{
    memset(s_img, 0, sizeof s_img);
    memset(s_fresh, 0, sizeof s_fresh);
    s_img_ver     = (uint8_t)NP_MU_VERSION;
    s_img_row_len = (uint8_t)NP_MU_ROW_BYTES;
}

/* ── Lifecycle ─────────────────────────────────────────────────────────────── */

void np_mu_init(void)
{
    memset(s_rows, 0, sizeof s_rows);
    memset(s_pend, 0, sizeof s_pend);
    memset(s_slot, 0, sizeof s_slot);
    image_empty();
    s_n = 0U;
    s_loaded = false;
    s_dropped = 0U;
}

bool np_mu_loaded(void) { return s_loaded; }

uint32_t np_mu_sessions_dropped(void) { return s_dropped; }

np_hub_status_t np_mu_load(void)
{
    const np_hub_status_t st = np_cfg_store_replicated_read(NP_CFG_FILE_MODULE_USAGE, s_img, sizeof s_img);
    if (st == NP_HUB_ERR_NOT_PRESENT) {
        image_empty();                /* never written: every total starts at zero */
        s_n = 0U;
    } else if (st != NP_HUB_OK) {
        image_empty();
        return st;                    /* unknown is not zero */
    } else {
        size_t n;
        uint8_t ver, row_len;
        if (!hdr_ok(s_img, NP_MU_MAX_ROWS, &n, &ver, &row_len)) {
            image_empty();
            return NP_HUB_ERR_STORE_INTEGRITY;
        }
        for (size_t i = 0U; i < n; i++) { get_row(&s_img[NP_MU_HDR_BYTES + i * row_len], &s_rows[i]); }
        s_n           = n;
        s_img_ver     = ver;
        s_img_row_len = row_len;
        memset(s_fresh, 0, sizeof s_fresh);
    }
    s_loaded = true;                  /* anything pending is in s_pend and is kept */
    return NP_HUB_OK;
}

void np_mu_session_begin(void)
{
    CRIT_ENTER();
    for (size_t i = 0U; i < NP_MU_MAX_ROWS; i++) {
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
    pending_t *p = pend_for(key);
    if (p != NULL) { p->present = true; }
    CRIT_EXIT();
}

/* Caller holds the critical section. */
static void add_used_locked(const np_mu_key_t *key, uint32_t on_ms)
{
    pending_t *p = pend_for(key);
    if (p == NULL) { return; }
    p->present = true;     /* driven means fitted */
    p->used    = true;
    p->used_ms = (UINT32_MAX - p->used_ms < on_ms) ? UINT32_MAX : p->used_ms + on_ms;
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

/* ── Mates, dose, throttles, the unseated flag ─────────────────────────────── */

static uint32_t sat_add(uint32_t a, uint32_t b) { return (UINT32_MAX - a < b) ? UINT32_MAX : a + b; }

typedef enum { PEND_MATE, PEND_DOSE, PEND_THROTTLE, PEND_UNSEATED } pend_field_t;

/* Add to one pending field of `key`, under the critical section. */
static void pend_add(const np_mu_key_t *key, pend_field_t f, uint32_t v)
{
    if (!key_valid(key)) { return; }
    CRIT_ENTER();
    pending_t *p = pend_for(key);
    if (p != NULL) {
        switch (f) {
        case PEND_MATE:     p->mates     = sat_add(p->mates, v);     break;
        case PEND_DOSE:     p->dose      = sat_add(p->dose, v);      break;
        case PEND_THROTTLE: p->throttles = sat_add(p->throttles, v); break;
        default:            p->flags    |= (uint8_t)NP_MU_FAULT_UNSEATED_AT_CLOSE; break;
        }
    }
    CRIT_EXIT();
}

void np_mu_note_mate(const np_mu_key_t *key)                      { pend_add(key, PEND_MATE, 1U); }
void np_mu_add_dose(const np_mu_key_t *key, uint32_t dose_mJ_cm2) { pend_add(key, PEND_DOSE, dose_mJ_cm2); }
void np_mu_note_throttle(const np_mu_key_t *key)                  { pend_add(key, PEND_THROTTLE, 1U); }
void np_mu_note_unseated_at_close(const np_mu_key_t *key)         { pend_add(key, PEND_UNSEATED, 0U); }

uint32_t np_mu_dose_mJ_cm2(const float *dose_J_cm2, size_t n)
{
    if (dose_J_cm2 == NULL) { return 0U; }
    double sum = 0.0;
    for (size_t i = 0U; i < n; i++) {
        const double d = (double)dose_J_cm2[i];
        if (d > 0.0 && d <= 1.0e30) { sum += d; }   /* NaN, negative and infinite add nothing */
    }
    const double mj = sum * 1000.0 + 0.5;
    return (mj >= (double)UINT32_MAX) ? UINT32_MAX : (uint32_t)mj;
}

void np_mu_note_fault(uint8_t slot, uint8_t mod_type, uint8_t code)
{
    if (slot >= NP_HUB_SLOT_MAX) { return; }   /* no module to charge it to */
    const np_mu_key_t k = np_mu_key_port(slot, (np_hub_mod_type_t)mod_type);
    CRIT_ENTER();
    pending_t *p = pend_for(&k);
    if (p != NULL) {
        size_t c;
        for (c = 0U; c < NP_MU_FAULT_CODES; c++) {
            if (p->faults[c] != 0U && p->fault_code[c] == code) { break; }
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
        /* Unknown is not zero: writing now would replace every module's lifetime with this one session.
         * The contributions are dropped, but each module keeps a pending entry marked lost, so the
         * next commit that succeeds sets detail_lost on its row (NP-FW-NVRAM-001 §6.3). */
        CRIT_ENTER();
        for (size_t i = 0U; i < NP_MU_MAX_ROWS; i++) {
            if (s_pend[i].key.kind == (uint8_t)NP_MU_KEY_EMPTY) { continue; }
            const np_mu_key_t k = s_pend[i].key;
            memset(&s_pend[i], 0, sizeof s_pend[i]);
            s_pend[i].key  = k;
            s_pend[i].lost = true;
        }
        s_dropped++;
        CRIT_EXIT();
        return NP_HUB_ERR_STORE_IO;
    }

    /* Fold the pending work into the rows.  The rows are the runner's; the
     * critical section holds the pending table still while it is read. */
    const uint32_t session_s = ms_to_s(session_ms);
    CRIT_ENTER();
    for (size_t i = 0U; i < NP_MU_MAX_ROWS; i++) {
        pending_t *p = &s_pend[i];
        if (p->key.kind == (uint8_t)NP_MU_KEY_EMPTY) { continue; }
        const size_t at = row_for(&p->key);
        if (at < NP_MU_MAX_ROWS) {
            np_mu_row_t *r = &s_rows[at];
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
            r->faults_other   = sat_add(r->faults_other, p->faults[NP_MU_FAULT_CODES]);
            r->mate_cycles    = sat_add(r->mate_cycles, p->mates);
            r->dose_accum     = sat_add(r->dose_accum, p->dose);
            r->throttle_count = sat_add(r->throttle_count, p->throttles);
            r->fault_flags   |= p->flags;
            if (p->lost) { r->detail_lost = 1U; }
        }
        memset(p, 0, sizeof *p);
    }
    CRIT_EXIT();

    image_update();
    return np_cfg_store_replicated_write(NP_CFG_FILE_MODULE_USAGE, s_img, sizeof s_img);
}

np_hub_status_t np_mu_snapshot(np_mu_row_t *out, size_t cap, size_t *n_out)
{
    if (out == NULL || n_out == NULL) { return NP_HUB_ERR_INVALID_ARG; }
    *n_out = 0U;
    if (!s_loaded) { return NP_HUB_ERR_STORE_IO; }   /* unknown, never an empty table (D-7) */
    const size_t n = (s_n < cap) ? s_n : cap;
    memcpy(out, s_rows, sizeof(np_mu_row_t) * n);
    *n_out = n;
    return NP_HUB_OK;
}
