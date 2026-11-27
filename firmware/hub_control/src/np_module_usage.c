/*
 * NeurOne Hub Control Program — per-module running totals (REQ-FWHUB-93)
 * See np_module_usage.h.
 *
 * Two kinds of state:
 *   s_tab[]  the stored totals, one table per key kind (tiles by UID, ports by
 *            slot and type), each with one row per module.  The rows live only
 *            in the table's record image, in their stored layout: a row is
 *            decoded into a local np_mu_row_t when it is folded or reported
 *            and encoded straight back, so no decoded copy of the tables is
 *            kept resident (the hub's STAGING budget, firmware-cross-build.yml;
 *            OI-NVRAM-23).  Touched only by the boot path and the runner
 *            (np_mu_load, np_mu_session_end, np_mu_snapshot), never
 *            concurrently, so they need no lock.
 *   s_pend   work not yet committed — this session's presence, use, drive
 *            time and channel drive time, and the faults, mates and throttles
 *            since the last commit — keyed by module, so it is independent of
 *            where (or whether) the module has a row.  Any task may add a
 *            fault or a mate, so it is changed only inside the critical
 *            section.
 * A load therefore replaces a table's rows without disturbing anything
 * pending, and a commit folds s_pend into the rows, then writes each table.
 *
 * Each table's img is its stored record as it was read (or last written).  A
 * commit writes the version-1 fields of each row into it in place, so whatever
 * a newer firmware stored after them is carried through unchanged
 * (NP-FW-NVRAM-001 §7.2, the rule for the record); a row created here starts
 * with that tail zeroed.
 */

#include "np_module_usage.h"

#include <string.h>

#include "np_cfg_store.h"
#include "np_transport.h"

_Static_assert(NP_MU_TILE_TABLE_BYTES <= NP_CFG_REPLICA_TABLE_MAX_PAYLOAD &&
               NP_MU_PORT_TABLE_BYTES <= NP_CFG_REPLICA_TABLE_MAX_PAYLOAD,
               "each running-total table must fit the store's table record (np_cfg_store.h)");
_Static_assert(NP_MU_HDR_BYTES + NP_MU_TILE_ROWS * NP_MU_TILE_ROW_BYTES <= NP_MU_TILE_TABLE_BYTES &&
               NP_MU_HDR_BYTES + NP_MU_PORT_ROWS * NP_MU_PORT_ROW_BYTES <= NP_MU_PORT_TABLE_BYTES,
               "every row must fit its record at the row length this firmware writes");
_Static_assert(NP_MU_TILE_ROW_MAX_BYTES >= NP_MU_TILE_ROW_BYTES && NP_MU_TILE_ROW_MAX_BYTES <= NP_MU_ROW_LEN_FIELD_MAX &&
               NP_MU_PORT_ROW_MAX_BYTES >= NP_MU_PORT_ROW_BYTES && NP_MU_PORT_ROW_MAX_BYTES <= NP_MU_ROW_LEN_FIELD_MAX,
               "the header's row length is one byte, and must admit this firmware's rows");
_Static_assert(NP_MU_PORT_ROW_MAX_BYTES == NP_MU_ROW_LEN_FIELD_MAX,
               "the port record is as long as the header can describe (np_module_usage.h)");
_Static_assert(NP_MU_TILE_CH <= NP_MU_CH_MAX && NP_MU_PORT_CH <= 8U,
               "a port's lit channels are one byte of mask");

/* ── State ─────────────────────────────────────────────────────────────────── */

typedef struct {
    np_mu_key_t key;          /* kind NP_MU_KEY_EMPTY = free entry */
    bool        present;      /* fitted this session */
    bool        used;         /* driven this session */
    uint32_t    used_ms;      /* drive time this session */
    uint8_t     ch_used;      /* bit c: channel c driven this session */
    uint32_t    ch_ms[NP_MU_CH_MAX];   /* channel drive time this session */
    uint8_t     fault_code[NP_MU_FAULT_CODES];
    uint32_t    faults[NP_MU_FAULT_CODES + 1U];   /* [NP_MU_FAULT_CODES] = codes with no entry */
    uint32_t    mates;
    uint32_t    throttles;
    uint8_t     flags;        /* NP_MU_FAULT_* to OR into the row */
    bool        lost;         /* a commit that should have added to this module was dropped */
} pending_t;

typedef struct {
    bool              on;
    uint32_t          since_ms;
    np_hub_mod_type_t type;
    uint8_t           ch_on;                      /* lit channels */
    uint32_t          ch_since[NP_MU_PORT_CH];
} slot_t;

/* One stored table: its record's image, which holds its rows, and how it is
 * laid out.  Row i is at img + NP_MU_HDR_BYTES + i * img_row_len. */
typedef struct {
    np_cfg_file_t file;
    uint8_t       kind;       /* the key kind of every row */
    uint8_t       n_ch;       /* channel slots in a version-1 row */
    uint8_t       v1_row;     /* version-1 row length */
    uint8_t       row_max;    /* longest row the record holds */
    size_t        cap;        /* rows */
    size_t        bytes;      /* record length */
    uint8_t      *img;
    size_t        n;
    bool          loaded;
    uint8_t       img_ver;    /* the image header's version and row length */
    uint8_t       img_row_len;
} table_t;

static uint8_t s_tile_img[NP_MU_TILE_TABLE_BYTES];
static uint8_t s_port_img[NP_MU_PORT_TABLE_BYTES];

static table_t s_tab[NP_MU_TABLE_COUNT] = {
    [NP_MU_TABLE_TILES] = { NP_CFG_FILE_MODULE_USAGE, (uint8_t)NP_MU_KEY_UID, (uint8_t)NP_MU_TILE_CH,
                            (uint8_t)NP_MU_TILE_ROW_BYTES, (uint8_t)NP_MU_TILE_ROW_MAX_BYTES,
                            NP_MU_TILE_ROWS, NP_MU_TILE_TABLE_BYTES,
                            s_tile_img, 0U, false, 0U, 0U },
    [NP_MU_TABLE_PORTS] = { NP_CFG_FILE_MODULE_PORTS, (uint8_t)NP_MU_KEY_PORT, (uint8_t)NP_MU_PORT_CH,
                            (uint8_t)NP_MU_PORT_ROW_BYTES, (uint8_t)NP_MU_PORT_ROW_MAX_BYTES,
                            NP_MU_PORT_ROWS, NP_MU_PORT_TABLE_BYTES,
                            s_port_img, 0U, false, 0U, 0U },
};

static uint32_t  s_dropped;
static pending_t s_pend[NP_MU_MAX_ROWS];
static slot_t    s_slot[NP_HUB_SLOT_MAX];

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

/* The table a key's row lives in. */
static table_t *table_of(uint8_t kind)
{
    return (kind == (uint8_t)NP_MU_KEY_UID) ? &s_tab[NP_MU_TABLE_TILES] : &s_tab[NP_MU_TABLE_PORTS];
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

/* ── Codec ─────────────────────────────────────────────────────────────────── */

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Row layout, version 1, little-endian.  These offsets are fixed for ever: a
 * later version appends after the row's last channel slot and never moves,
 * resizes or reuses a field here (NP-FW-NVRAM-001 §7.2).
 *    0 kind | 1 fault_flags | 2 id[8] | 10 sessions_present | 14 present_s
 *   18 sessions_used | 22 used_s | 26 fault_code[4] | 30 fault_count[4]
 *   46 faults_other | 50 mate_cycles | 54 throttle_count | 58 detail_lost
 *   59 + 8c: channel c's on-time (s) | 63 + 8c: channel c's sessions
 *   (c < 3 in a tile row, 83 B; c < 4 in a port row, 91 B)
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
#define OFF_THROT    54U
#define OFF_LOST     58U
#define OFF_CH       59U
_Static_assert(OFF_CH == NP_MU_COMMON_BYTES && OFF_LOST + 1U == OFF_CH,
               "the fields every row has fill the bytes before the first channel slot");

/* Write the version-1 fields of `s` into the row at `r`, with `n_ch` channel
 * slots; bytes past them untouched. */
static void put_row(uint8_t *r, const np_mu_row_t *s, size_t n_ch)
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
    put32(&r[OFF_THROT], s->throttle_count);
    r[OFF_LOST]  = (s->detail_lost != 0U) ? 1U : 0U;
    for (size_t c = 0U; c < n_ch; c++) {
        put32(&r[OFF_CH + NP_MU_CH_BYTES * c], s->ch_on_s[c]);
        put32(&r[OFF_CH + NP_MU_CH_BYTES * c + 4U], s->ch_sessions[c]);
    }
}

static void get_row(const uint8_t *r, np_mu_row_t *d, size_t n_ch)
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
    d->throttle_count   = get32(&r[OFF_THROT]);
    d->detail_lost      = (r[OFF_LOST] != 0U) ? 1U : 0U;
    for (size_t c = 0U; c < n_ch; c++) {
        d->ch_on_s[c]     = get32(&r[OFF_CH + NP_MU_CH_BYTES * c]);
        d->ch_sessions[c] = get32(&r[OFF_CH + NP_MU_CH_BYTES * c + 4U]);
    }
}

static void put_hdr(uint8_t *out, uint8_t ver, uint8_t row_len, size_t n)
{
    out[0] = ver;
    out[1] = row_len;
    out[2] = (uint8_t)n;
    out[3] = (uint8_t)(n >> 8);
}

static bool table_ok(np_mu_table_t t) { return (unsigned)t < (unsigned)NP_MU_TABLE_COUNT; }

size_t np_mu_table_bytes(np_mu_table_t t) { return table_ok(t) ? s_tab[t].bytes : 0U; }

void np_mu_encode(np_mu_table_t t, const np_mu_row_t *rows, size_t n, uint8_t *out)
{
    if (!table_ok(t) || out == NULL) { return; }
    const table_t *tb = &s_tab[t];
    memset(out, 0, tb->bytes);
    size_t k = 0U;
    for (size_t i = 0U; rows != NULL && i < n && k < tb->cap; i++) {
        if (rows[i].key.kind != tb->kind) { continue; }
        put_row(&out[NP_MU_HDR_BYTES + k * tb->v1_row], &rows[i], tb->n_ch);
        k++;
    }
    put_hdr(out, (uint8_t)NP_MU_VERSION, tb->v1_row, k);
}

/* The header and key check np_mu_decode makes, and the version, row length and
 * row count it reads.  Any version >= 1 is read: the row length steps from row
 * to row, so a longer row of a newer firmware is read for the fields this one
 * knows (§7.2). */
static bool hdr_ok(const table_t *tb, const uint8_t *in, size_t cap, size_t *n, uint8_t *ver, uint8_t *row_len)
{
    *ver     = in[0];
    *row_len = in[1];
    *n       = (size_t)in[2] | ((size_t)in[3] << 8);
    if (*ver == 0U || *row_len < tb->v1_row || *row_len > tb->row_max || *n > tb->cap || *n > cap) {
        return false;
    }
    for (size_t i = 0U; i < *n; i++) {
        if (in[NP_MU_HDR_BYTES + i * *row_len] != tb->kind) { return false; }
    }
    return true;
}

/* Validates the whole record before writing anything into `rows`, so a refused
 * record leaves the caller's table as it was. */
bool np_mu_decode(np_mu_table_t t, const uint8_t *in, np_mu_row_t *rows, size_t cap, size_t *n_out)
{
    if (!table_ok(t) || in == NULL || rows == NULL || n_out == NULL) { return false; }
    const table_t *tb = &s_tab[t];
    size_t n;
    uint8_t ver, row_len;
    if (!hdr_ok(tb, in, cap, &n, &ver, &row_len)) { return false; }
    for (size_t i = 0U; i < n; i++) { get_row(&in[NP_MU_HDR_BYTES + i * row_len], &rows[i], tb->n_ch); }
    *n_out = n;
    return true;
}

/* Row i of the table, in its image at the image's row length. */
static uint8_t *row_at(const table_t *tb, size_t i)
{
    return &tb->img[NP_MU_HDR_BYTES + i * tb->img_row_len];
}

/* Whether row `r` (in an image) is `key`'s. */
static bool row_is(const uint8_t *r, const np_mu_key_t *key)
{
    return r[OFF_KIND] == key->kind && memcmp(&r[OFF_ID], key->id, NP_MU_ID_LEN) == 0;
}

/* The stored row for `key` in table `t`, created if absent.  A full table
 * gives up the row of the module with the fewest sessions present that has
 * nothing pending (UC-208), and the newcomer's row starts with detail_lost
 * set: the helmet has forgotten a module, so it cannot say this one's totals
 * start here for the first time (NP-FW-NVRAM-001 §6.3).  A row created here
 * is zeroed at the image's row length first, so whatever a newer firmware's
 * version keeps after the version-1 fields starts at zero (§7.2).  t->cap if
 * every row is pending.  Runner only, called with the critical section held
 * so the pending table it consults stands still. */
static size_t row_for(table_t *t, const np_mu_key_t *key)
{
    for (size_t i = 0U; i < t->n; i++) {
        if (row_is(row_at(t, i), key)) { return i; }
    }
    size_t at = t->n;
    bool gave_up = false;
    if (t->n >= t->cap) {
        at = t->cap;
        uint32_t least = 0U;
        for (size_t i = 0U; i < t->n; i++) {
            const uint8_t *r = row_at(t, i);
            np_mu_key_t k;
            k.kind = r[OFF_KIND];
            memcpy(k.id, &r[OFF_ID], NP_MU_ID_LEN);
            if (has_pending(&k)) { continue; }
            const uint32_t present = get32(&r[OFF_SP]);
            if (at == t->cap || present < least) { at = i; least = present; }
        }
        if (at == t->cap) { return t->cap; }
        gave_up = true;
    } else {
        t->n++;
    }
    uint8_t *r = row_at(t, at);
    memset(r, 0, t->img_row_len);
    r[OFF_KIND] = key->kind;
    memcpy(&r[OFF_ID], key->id, NP_MU_ID_LEN);
    r[OFF_LOST] = gave_up ? 1U : 0U;
    return at;
}

/* Finish the table's image for a write: everything past the last row zero,
 * and the header.  The header keeps the higher of the stored version and this
 * firmware's, because the tails it carries are that version's. */
static void image_update(table_t *tb)
{
    const size_t used = NP_MU_HDR_BYTES + tb->n * tb->img_row_len;
    memset(&tb->img[used], 0, tb->bytes - used);
    put_hdr(tb->img, (tb->img_ver > (uint8_t)NP_MU_VERSION) ? tb->img_ver : (uint8_t)NP_MU_VERSION,
            tb->img_row_len, tb->n);
}

/* An empty image at this firmware's version and row length. */
static void image_empty(table_t *tb)
{
    memset(tb->img, 0, tb->bytes);
    tb->img_ver     = (uint8_t)NP_MU_VERSION;
    tb->img_row_len = tb->v1_row;
}

/* ── Lifecycle ─────────────────────────────────────────────────────────────── */

void np_mu_init(void)
{
    for (size_t t = 0U; t < NP_MU_TABLE_COUNT; t++) {
        table_t *tb = &s_tab[t];
        image_empty(tb);
        tb->n = 0U;
        tb->loaded = false;
    }
    memset(s_pend, 0, sizeof s_pend);
    memset(s_slot, 0, sizeof s_slot);
    s_dropped = 0U;
}

bool np_mu_table_loaded(np_mu_table_t t) { return table_ok(t) && s_tab[t].loaded; }

bool np_mu_loaded(void) { return s_tab[NP_MU_TABLE_TILES].loaded && s_tab[NP_MU_TABLE_PORTS].loaded; }

uint32_t np_mu_sessions_dropped(void) { return s_dropped; }

static np_hub_status_t load_table(table_t *tb)
{
    const np_hub_status_t st = np_cfg_store_replicated_read(tb->file, tb->img, tb->bytes);
    if (st == NP_HUB_ERR_NOT_PRESENT) {
        image_empty(tb);              /* never written: every total starts at zero */
        tb->n = 0U;
    } else if (st != NP_HUB_OK) {
        image_empty(tb);
        return st;                    /* unknown is not zero */
    } else {
        size_t n;
        uint8_t ver, row_len;
        if (!hdr_ok(tb, tb->img, tb->cap, &n, &ver, &row_len)) {
            image_empty(tb);
            return NP_HUB_ERR_STORE_INTEGRITY;
        }
        tb->n           = n;          /* the rows stay where they are, in the image */
        tb->img_ver     = ver;
        tb->img_row_len = row_len;
    }
    tb->loaded = true;                /* anything pending is in s_pend and is kept */
    return NP_HUB_OK;
}

np_hub_status_t np_mu_load(void)
{
    np_hub_status_t first = NP_HUB_OK;
    for (size_t t = 0U; t < NP_MU_TABLE_COUNT; t++) {
        const np_hub_status_t st = load_table(&s_tab[t]);
        if (first == NP_HUB_OK) { first = st; }
    }
    return first;
}

void np_mu_session_begin(void)
{
    CRIT_ENTER();
    for (size_t i = 0U; i < NP_MU_MAX_ROWS; i++) {
        s_pend[i].present = false;
        s_pend[i].used    = false;
        s_pend[i].used_ms = 0U;
        s_pend[i].ch_used = 0U;
        memset(s_pend[i].ch_ms, 0, sizeof s_pend[i].ch_ms);
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

static uint32_t sat_add(uint32_t a, uint32_t b) { return (UINT32_MAX - a < b) ? UINT32_MAX : a + b; }

/* Caller holds the critical section. */
static void add_used_locked(const np_mu_key_t *key, uint32_t on_ms)
{
    pending_t *p = pend_for(key);
    if (p == NULL) { return; }
    p->present = true;     /* driven means fitted */
    p->used    = true;
    p->used_ms = sat_add(p->used_ms, on_ms);
}

/* Caller holds the critical section; `ch` already checked against the key's kind. */
static void add_channel_locked(const np_mu_key_t *key, uint8_t ch, uint32_t on_ms)
{
    pending_t *p = pend_for(key);
    if (p == NULL) { return; }
    p->present   = true;
    p->ch_used  |= (uint8_t)(1U << ch);
    p->ch_ms[ch] = sat_add(p->ch_ms[ch], on_ms);
}

void np_mu_add_used(const np_mu_key_t *key, uint32_t on_ms)
{
    if (!key_valid(key)) { return; }
    CRIT_ENTER();
    add_used_locked(key, on_ms);
    CRIT_EXIT();
}

void np_mu_add_channel_used(const np_mu_key_t *key, uint8_t ch, uint32_t on_ms)
{
    if (!key_valid(key)) { return; }
    if (ch >= table_of(key->kind)->n_ch) { return; }
    CRIT_ENTER();
    add_channel_locked(key, ch, on_ms);
    CRIT_EXIT();
}

/* Close channel `c` of slot `slot` at `now_ms`.  Caller holds the critical section. */
static void slot_ch_off_locked(uint8_t slot, uint8_t c, uint32_t now_ms)
{
    slot_t *s = &s_slot[slot];
    const uint32_t on_ms = (now_ms >= s->ch_since[c]) ? now_ms - s->ch_since[c] : 0U;
    const np_mu_key_t k = np_mu_key_port(slot, s->type);
    add_channel_locked(&k, c, on_ms);
    s->ch_on &= (uint8_t)~(1U << c);
}

void np_mu_slot_on(uint8_t slot, np_hub_mod_type_t type, uint8_t ch_mask, uint32_t now_ms)
{
    if (slot >= NP_HUB_SLOT_MAX) { return; }
    ch_mask &= (uint8_t)((1U << NP_MU_PORT_CH) - 1U);
    CRIT_ENTER();
    slot_t *s = &s_slot[slot];
    if (!s->on) {
        s->on       = true;
        s->since_ms = now_ms;
        s->type     = type;
        s->ch_on    = 0U;
        const np_mu_key_t k = np_mu_key_port(slot, type);
        add_used_locked(&k, 0U);   /* driven, even if for under a tick */
    }
    for (uint8_t c = 0U; c < NP_MU_PORT_CH; c++) {
        const uint8_t bit = (uint8_t)(1U << c);
        if ((s->ch_on & bit) != 0U && (ch_mask & bit) == 0U) {
            slot_ch_off_locked(slot, c, now_ms);              /* went dark */
        } else if ((s->ch_on & bit) == 0U && (ch_mask & bit) != 0U) {
            s->ch_on      |= bit;                              /* newly lit */
            s->ch_since[c] = now_ms;
            const np_mu_key_t k = np_mu_key_port(slot, s->type);
            add_channel_locked(&k, c, 0U);                     /* driven, even if for under a tick */
        }
    }
    CRIT_EXIT();
}

/* Caller holds the critical section. */
static void slot_off_locked(uint8_t slot, uint32_t now_ms)
{
    slot_t *s = &s_slot[slot];
    if (!s->on) { return; }
    for (uint8_t c = 0U; c < NP_MU_PORT_CH; c++) {
        if ((s->ch_on & (1U << c)) != 0U) { slot_ch_off_locked(slot, c, now_ms); }
    }
    const uint32_t on_ms = (now_ms >= s->since_ms) ? now_ms - s->since_ms : 0U;
    const np_mu_key_t k = np_mu_key_port(slot, s->type);
    add_used_locked(&k, on_ms);
    s->on = false;
}

void np_mu_slot_off(uint8_t slot, uint32_t now_ms)
{
    if (slot >= NP_HUB_SLOT_MAX) { return; }
    CRIT_ENTER();
    slot_off_locked(slot, now_ms);
    CRIT_EXIT();
}

/* ── Mates, throttles, the unseated flag ───────────────────────────────────── */

typedef enum { PEND_MATE, PEND_THROTTLE, PEND_UNSEATED } pend_field_t;

/* Add to one pending field of `key`, under the critical section. */
static void pend_add(const np_mu_key_t *key, pend_field_t f, uint32_t v)
{
    if (!key_valid(key)) { return; }
    CRIT_ENTER();
    pending_t *p = pend_for(key);
    if (p != NULL) {
        switch (f) {
        case PEND_MATE:     p->mates     = sat_add(p->mates, v);     break;
        case PEND_THROTTLE: p->throttles = sat_add(p->throttles, v); break;
        default:            p->flags    |= (uint8_t)NP_MU_FAULT_UNSEATED_AT_CLOSE; break;
        }
    }
    CRIT_EXIT();
}

void np_mu_note_mate(const np_mu_key_t *key)              { pend_add(key, PEND_MATE, 1U); }
void np_mu_note_throttle(const np_mu_key_t *key)          { pend_add(key, PEND_THROTTLE, 1U); }
void np_mu_note_unseated_at_close(const np_mu_key_t *key) { pend_add(key, PEND_UNSEATED, 0U); }

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

/* Fold one pending entry into its row. */
static void fold(np_mu_row_t *r, const pending_t *p, uint32_t session_s, size_t n_ch)
{
    if (p->present) {
        r->sessions_present = sat_add(r->sessions_present, 1U);
        r->present_s        = sat_add(r->present_s, session_s);
    }
    if (p->used) {
        r->sessions_used = sat_add(r->sessions_used, 1U);
        r->used_s        = sat_add(r->used_s, ms_to_s(p->used_ms));
    }
    for (size_t c = 0U; c < n_ch; c++) {
        if ((p->ch_used & (1U << c)) != 0U) {
            r->ch_sessions[c] = sat_add(r->ch_sessions[c], 1U);
            r->ch_on_s[c]     = sat_add(r->ch_on_s[c], ms_to_s(p->ch_ms[c]));
        }
    }
    for (size_t c = 0U; c < NP_MU_FAULT_CODES; c++) {
        if (p->faults[c] != 0U) { row_add_fault(r, p->fault_code[c], p->faults[c]); }
    }
    r->faults_other   = sat_add(r->faults_other, p->faults[NP_MU_FAULT_CODES]);
    r->mate_cycles    = sat_add(r->mate_cycles, p->mates);
    r->throttle_count = sat_add(r->throttle_count, p->throttles);
    r->fault_flags   |= p->flags;
    if (p->lost) { r->detail_lost = 1U; }
}

np_hub_status_t np_mu_session_end(uint32_t session_ms)
{
    CRIT_ENTER();
    for (uint8_t s = 0U; s < NP_HUB_SLOT_MAX; s++) { slot_off_locked(s, session_ms); }
    CRIT_EXIT();

    np_hub_status_t result = NP_HUB_OK;
    bool dropped = false;
    for (size_t t = 0U; t < NP_MU_TABLE_COUNT; t++) {
        table_t *tb = &s_tab[t];
        if (tb->loaded) { continue; }
        const np_hub_status_t st = load_table(tb);
        if (st == NP_HUB_OK) { continue; }
        if (result == NP_HUB_OK) { result = st; }
        /* Unknown is not zero: writing now would replace every module's lifetime with this one session.
         * The contributions to this table are dropped, but each module keeps a pending entry marked lost,
         * so the next commit that succeeds sets detail_lost on its row (NP-FW-NVRAM-001 §6.3). */
        CRIT_ENTER();
        for (size_t i = 0U; i < NP_MU_MAX_ROWS; i++) {
            if (s_pend[i].key.kind != tb->kind) { continue; }
            const np_mu_key_t k = s_pend[i].key;
            memset(&s_pend[i], 0, sizeof s_pend[i]);
            s_pend[i].key  = k;
            s_pend[i].lost = true;
        }
        CRIT_EXIT();
        dropped = true;
    }
    if (dropped) { s_dropped++; }

    /* Fold the pending work into the rows of every loaded table.  The rows are
     * the runner's; the critical section holds the pending table still while
     * it is read. */
    const uint32_t session_s = ms_to_s(session_ms);
    CRIT_ENTER();
    for (size_t i = 0U; i < NP_MU_MAX_ROWS; i++) {
        pending_t *p = &s_pend[i];
        if (p->key.kind == (uint8_t)NP_MU_KEY_EMPTY) { continue; }
        table_t *tb = table_of(p->key.kind);
        if (!tb->loaded) { continue; }               /* held, marked lost, for the next commit */
        const size_t at = row_for(tb, &p->key);
        if (at < tb->cap) {
            /* Decode the one row, fold, and encode it straight back in place: its version-1 fields
             * only, so a newer firmware's tail after them is carried through (§7.2). */
            np_mu_row_t r;
            uint8_t *stored = row_at(tb, at);
            get_row(stored, &r, tb->n_ch);
            fold(&r, p, session_s, tb->n_ch);
            put_row(stored, &r, tb->n_ch);
        }
        memset(p, 0, sizeof *p);
    }
    CRIT_EXIT();

    for (size_t t = 0U; t < NP_MU_TABLE_COUNT; t++) {
        table_t *tb = &s_tab[t];
        if (!tb->loaded) { continue; }
        image_update(tb);
        const np_hub_status_t st = np_cfg_store_replicated_write(tb->file, tb->img, tb->bytes);
        if (st != NP_HUB_OK && result == NP_HUB_OK) { result = st; }
    }
    return result;
}

np_hub_status_t np_mu_snapshot(np_mu_row_t *out, size_t cap, size_t *n_out)
{
    if (out == NULL || n_out == NULL) { return NP_HUB_ERR_INVALID_ARG; }
    *n_out = 0U;
    if (!np_mu_loaded()) { return NP_HUB_ERR_STORE_IO; }   /* unknown, never an empty table (D-7) */
    size_t k = 0U;
    for (size_t t = 0U; t < NP_MU_TABLE_COUNT; t++) {
        const table_t *tb = &s_tab[t];
        const size_t n = (tb->n < cap - k) ? tb->n : cap - k;
        for (size_t i = 0U; i < n; i++) { get_row(row_at(tb, i), &out[k + i], tb->n_ch); }
        k += n;
    }
    *n_out = k;
    return NP_HUB_OK;
}
