/*
 * NeurOne Safety MCU — SW01-M09: Non-Volatile Safety State
 * Document: NP-SW-FAULTMSG-001 P1 (OI-FAULTMSG-01) and the per-user scope
 *           decision of 2026-09-22; NP-FW-CVNS-001 §5.4; NP-HW-CVNS-001 REQ-CVNS-09
 *
 * WHY THIS MODULE EXISTS
 * ----------------------
 * After a cervical VNS cardiac cutoff, re-enable needs the 30 s lockout, app
 * confirmation and a repeat impedance check.  That state used to live only in
 * RAM, and the headset has no battery (CLAUDE.md §4.5): unplugging the power
 * bank erased the cutoff, and in Mode 3 a new session could enable cervical
 * stimulation with no confirmation at all.  This module keeps it in flash.
 *
 * PER-USER SCOPE (principal, 2026-09-22)
 * --------------------------------------
 * A cutoff is held for the user who triggered it and nobody else.  The app
 * names the active user with an opaque tag (np_safety_user_cmd_t); the same
 * user is assumed until it names another, across sessions and power cycles.
 * Two kinds of outstanding cutoff cannot be attributed to one person and are
 * therefore withheld from EVERYONE until someone acknowledges them:
 *   - one recorded while no user had been named (NP_SAFETY_USER_UNSPECIFIED);
 *   - NP_SAFETY_USER_ANY — a torn record, an interrupted compaction, or a
 *     cutoff for a further user once NP_NV_MAX_PENDING are already held.
 * Acknowledging (the re-enable confirmation, run by a named person) clears that
 * person's entry and both unattributable kinds.
 *
 * RECORD LOG
 * ----------
 * Append-only 64-bit records across two 2 KB pages, replayed in order at boot:
 *   lo = NP_NV_MAGIC << 24 | type << 16 | seq     hi = user tag or count
 *   SET(tag)  tag now has an outstanding cutoff
 *   CLR(tag)  tag acknowledged (also clears UNSPECIFIED and ANY)
 *   USER(tag) active user changed
 *   RESET … COMMIT(n)   a complete snapshot of the state, n records between
 * Within a page, slot order is write order; pages replay in order of their
 * lowest seq, and a page holding only undecodable records replays last.
 *
 * COMPACTION — why a power loss never loses a cutoff
 * --------------------------------------------------
 * When the live page fills, the whole state is written as RESET … COMMIT to the
 * other (empty) page, and only then is the full page erased.  A snapshot counts
 * only once its COMMIT is down.  An UNCOMMITTED snapshot is never trusted to
 * replace the state: its records are merged in on top of what came before, and
 * ANY is set, because the write it was carrying may be missing.  If both pages
 * hold records at the next write (compaction interrupted before the erase), a
 * fresh committed snapshot is appended to the newer page first, then the older
 * page is erased.
 *
 * FAIL-CLOSED DECODING
 * --------------------
 *   - never-written storage → no user, nothing outstanding (factory-fresh)
 *   - an undecodable record → treated as SET(ANY) at its place in the log
 *   - an uncommitted snapshot → merged additively, plus ANY
 *
 * WHAT THIS DOES NOT HANDLE (bench items, OI-FAULTMSG-01)
 * -------------------------------------------------------
 * On silicon, reading a power-loss-torn double-word can raise a double-ECC NMI
 * (FLASH_ECCR.ECCD).  The decoding above assumes the read returns the torn
 * bits; the NMI path is not handled and has not been bench-tested.
 *
 * IEC 62304 Class C — MISRA C:2012.  C11, no GNU extensions.
 */

#include "np_nv_state.h"
#include "np_safety_config.h"
#include "np_safety_hal.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define NP_NV_MAGIC      0xA5U
#define NP_NV_T_SET      1U
#define NP_NV_T_CLR      2U
#define NP_NV_T_USER     3U
#define NP_NV_T_RESET    4U
#define NP_NV_T_COMMIT   5U
#define NP_NV_ERASED     0xFFFFFFFFUL
#define NP_NV_SEQ_MAX    0xFFFFU
/* RESET + USER + one SET per pending user + SET(ANY) + COMMIT */
#define NP_NV_SNAPSHOT_MAX  (NP_NV_MAX_PENDING + 4U)

_Static_assert(NP_NV_PAGE_COUNT == 2U, "compaction assumes exactly two pages");
_Static_assert(NP_NV_SNAPSHOT_MAX < NP_NV_SLOTS_PER_PAGE, "a snapshot fits a page");

typedef struct {
    uint32_t current;
    uint32_t pending[NP_NV_MAX_PENDING];
    uint8_t  n;
    bool     any;
} np_nv_view_t;

typedef struct {
    uint16_t used;       /* slots up to and including the last non-erased one */
    bool     has_valid;
    uint16_t min_seq;
    uint16_t max_seq;
} np_nv_page_t;

static np_nv_page_t s_page[NP_NV_PAGE_COUNT];
static np_nv_view_t s_view;

/* ── View operations ────────────────────────────────────────────────────────── */

static void view_clear(np_nv_view_t *v)
{
    (void)memset(v, 0, sizeof(*v));
    v->current = NP_SAFETY_USER_UNSPECIFIED;
}

static bool view_has(const np_nv_view_t *v, uint32_t tag)
{
    uint8_t i;
    for (i = 0U; i < v->n; i++) {
        if (v->pending[i] == tag) {
            return true;
        }
    }
    return false;
}

static void view_set(np_nv_view_t *v, uint32_t tag)
{
    if (tag == NP_SAFETY_USER_ANY) {
        v->any = true;
    } else if (!view_has(v, tag)) {
        if (v->n < (uint8_t)NP_NV_MAX_PENDING) {
            v->pending[v->n] = tag;
            v->n++;
        } else {
            v->any = true;          /* table full: fail closed, never drop it */
        }
    } else {
        /* already outstanding */
    }
}

static void view_remove(np_nv_view_t *v, uint32_t tag)
{
    uint8_t i = 0U;
    while (i < v->n) {
        if (v->pending[i] == tag) {
            v->n--;
            v->pending[i] = v->pending[v->n];
        } else {
            i++;
        }
    }
}

static void view_clr(np_nv_view_t *v, uint32_t tag)
{
    view_remove(v, tag);
    view_remove(v, NP_SAFETY_USER_UNSPECIFIED);
    v->any = false;
}

/* Fold an abandoned (uncommitted) snapshot into the running state: keep
 * everything either side says is outstanding, and add ANY. */
static void view_merge_abandoned(np_nv_view_t *main_v, const np_nv_view_t *grp)
{
    uint8_t i;
    for (i = 0U; i < grp->n; i++) {
        view_set(main_v, grp->pending[i]);
    }
    main_v->any = true;
}

static bool view_equal(const np_nv_view_t *a, const np_nv_view_t *b)
{
    uint8_t i;
    if ((a->current != b->current) || (a->any != b->any) || (a->n != b->n)) {
        return false;
    }
    for (i = 0U; i < a->n; i++) {
        if (!view_has(b, a->pending[i])) {
            return false;
        }
    }
    return true;
}

/* ── Record encoding ────────────────────────────────────────────────────────── */

static bool rec_valid(uint32_t lo, uint32_t hi, uint8_t *type_out, uint16_t *seq_out)
{
    uint8_t type = (uint8_t)((lo >> 16) & 0xFFU);
    (void)hi;
    if (((lo >> 24) != NP_NV_MAGIC) || (type < NP_NV_T_SET) || (type > NP_NV_T_COMMIT)) {
        return false;
    }
    *type_out = type;
    *seq_out  = (uint16_t)(lo & 0xFFFFU);
    return true;
}

static uint32_t rec_lo(uint8_t type, uint16_t seq)
{
    return ((uint32_t)NP_NV_MAGIC << 24) | ((uint32_t)type << 16) | (uint32_t)seq;
}

/* ── Scan and replay ────────────────────────────────────────────────────────── */

static void scan_page(uint8_t page)
{
    uint16_t slot;
    np_nv_page_t *p = &s_page[page];

    p->used      = 0U;
    p->has_valid = false;
    p->min_seq   = 0U;
    p->max_seq   = 0U;

    for (slot = 0U; slot < NP_NV_SLOTS_PER_PAGE; slot++) {
        uint32_t lo;
        uint32_t hi;
        uint8_t  type;
        uint16_t seq;
        np_hal_nv_read_dword(page, slot, &lo, &hi);
        if ((lo == NP_NV_ERASED) && (hi == NP_NV_ERASED)) {
            continue;
        }
        p->used = (uint16_t)(slot + 1U);
        if (rec_valid(lo, hi, &type, &seq)) {
            if (!p->has_valid || (seq < p->min_seq)) { p->min_seq = seq; }
            if (!p->has_valid || (seq > p->max_seq)) { p->max_seq = seq; }
            p->has_valid = true;
        }
    }
}

/* Replay order: pages with valid records by lowest seq; a page holding only
 * undecodable records last (it can only be the newest write, torn). */
static void page_order(uint8_t order[NP_NV_PAGE_COUNT])
{
    bool swap = false;
    if (s_page[0].has_valid && s_page[1].has_valid) {
        swap = s_page[1].min_seq < s_page[0].min_seq;
    } else if (!s_page[0].has_valid && s_page[1].has_valid) {
        swap = true;
    } else {
        swap = false;
    }
    order[0] = swap ? 1U : 0U;
    order[1] = swap ? 0U : 1U;
}

static uint16_t max_seq_all(bool *any_valid)
{
    uint16_t m = 0U;
    uint8_t  p;
    *any_valid = false;
    for (p = 0U; p < NP_NV_PAGE_COUNT; p++) {
        if (s_page[p].has_valid) {
            if (!*any_valid || (s_page[p].max_seq > m)) { m = s_page[p].max_seq; }
            *any_valid = true;
        }
    }
    return m;
}

static void apply(np_nv_view_t *v, uint8_t type, uint32_t tag)
{
    if (type == NP_NV_T_SET) {
        view_set(v, tag);
    } else if (type == NP_NV_T_CLR) {
        view_clr(v, tag);
    } else if (type == NP_NV_T_USER) {
        v->current = tag;
    } else {
        /* RESET / COMMIT are handled by the replay loop */
    }
}

static void replay(np_nv_view_t *out)
{
    uint8_t      order[NP_NV_PAGE_COUNT];
    uint8_t      k;
    bool         in_group = false;
    uint32_t     group_count = 0U;
    np_nv_view_t group;

    view_clear(out);
    view_clear(&group);
    page_order(order);

    for (k = 0U; k < NP_NV_PAGE_COUNT; k++) {
        uint8_t  page = order[k];
        uint16_t slot;
        for (slot = 0U; slot < s_page[page].used; slot++) {
            uint32_t lo;
            uint32_t hi;
            uint8_t  type;
            uint16_t seq;
            np_hal_nv_read_dword(page, slot, &lo, &hi);
            if ((lo == NP_NV_ERASED) && (hi == NP_NV_ERASED)) {
                continue;
            }
            if (!rec_valid(lo, hi, &type, &seq)) {
                /* Undecodable: may have been a SET — fail closed. */
                if (in_group) { group.any = true; group_count++; }
                else          { out->any  = true; }
                continue;
            }
            if (type == NP_NV_T_RESET) {
                if (in_group) { view_merge_abandoned(out, &group); }
                view_clear(&group);
                group.current = out->current;
                in_group    = true;
                group_count = 0U;
            } else if (type == NP_NV_T_COMMIT) {
                if (in_group && (hi == group_count)) {
                    *out = group;                    /* complete snapshot */
                } else if (in_group) {
                    view_merge_abandoned(out, &group);
                } else {
                    /* stray COMMIT: nothing to commit */
                }
                in_group = false;
            } else if (in_group) {
                apply(&group, type, hi);
                group_count++;
            } else {
                apply(out, type, hi);
            }
        }
    }
    if (in_group) {
        view_merge_abandoned(out, &group);           /* never committed */
    }
}

static void scan_all(void)
{
    uint8_t p;
    for (p = 0U; p < NP_NV_PAGE_COUNT; p++) {
        scan_page(p);
    }
}

/* ── Writing ────────────────────────────────────────────────────────────────── */

static bool program_verify(uint8_t page, uint16_t slot, uint32_t lo, uint32_t hi)
{
    uint32_t rlo;
    uint32_t rhi;
    if (!np_hal_nv_program_dword(page, slot, lo, hi)) {
        return false;
    }
    np_hal_nv_read_dword(page, slot, &rlo, &rhi);
    return (rlo == lo) && (rhi == hi);
}

static uint16_t snapshot_len(const np_nv_view_t *v)
{
    return (uint16_t)(3U + v->n + (v->any ? 1U : 0U));   /* RESET USER … COMMIT */
}

/* Write `v` as RESET … COMMIT starting at (page, slot). */
static bool write_snapshot(uint8_t page, uint16_t slot, uint16_t *seq, const np_nv_view_t *v)
{
    uint8_t  i;
    uint32_t count = 0U;

    if (!program_verify(page, slot, rec_lo(NP_NV_T_RESET, *seq), 0U)) { return false; }
    slot++; (*seq)++;
    if (!program_verify(page, slot, rec_lo(NP_NV_T_USER, *seq), v->current)) { return false; }
    slot++; (*seq)++; count++;
    for (i = 0U; i < v->n; i++) {
        if (!program_verify(page, slot, rec_lo(NP_NV_T_SET, *seq), v->pending[i])) { return false; }
        slot++; (*seq)++; count++;
    }
    if (v->any) {
        if (!program_verify(page, slot, rec_lo(NP_NV_T_SET, *seq), NP_SAFETY_USER_ANY)) { return false; }
        slot++; (*seq)++; count++;
    }
    return program_verify(page, slot, rec_lo(NP_NV_T_COMMIT, *seq), count);
}

static bool write_change(uint8_t type, uint32_t tag)
{
    np_nv_view_t now;
    np_nv_view_t want;
    uint8_t      order[NP_NV_PAGE_COUNT];
    bool         any_valid;
    uint16_t     seq;
    bool         ok;

    scan_all();
    replay(&now);
    want = now;
    apply(&want, type, tag);

    seq = (uint16_t)(max_seq_all(&any_valid) + (any_valid ? 1U : 0U));
    if (any_valid && (seq > (uint16_t)(NP_NV_SEQ_MAX - NP_NV_SNAPSHOT_MAX))) {
        /* 65 535 records is unreachable in the life of a device; refused rather
         * than wrapped, because a wrapped seq would reorder the log. */
        return false;
    }

    page_order(order);

    if ((s_page[0].used > 0U) && (s_page[1].used > 0U)) {
        /* Compaction was interrupted before the older page was erased: commit a
         * complete snapshot of the intended state to the newer page, then erase
         * the older one. */
        uint8_t newer = order[1];
        uint8_t older = order[0];
        if ((uint32_t)s_page[newer].used + snapshot_len(&want) > NP_NV_SLOTS_PER_PAGE) {
            return false;
        }
        ok = write_snapshot(newer, s_page[newer].used, &seq, &want);
        if (ok) {
            (void)np_hal_nv_erase_page(older);
        }
    } else {
        uint8_t live  = (s_page[1].used > 0U) ? 1U : 0U;
        uint8_t other = (uint8_t)(1U - live);
        if (s_page[live].used < NP_NV_SLOTS_PER_PAGE) {
            ok = program_verify(live, s_page[live].used, rec_lo(type, seq), tag);
        } else {
            /* Compact: the new page first, the full page erased only after. */
            ok = write_snapshot(other, 0U, &seq, &want);
            if (ok) {
                (void)np_hal_nv_erase_page(live);
            }
        }
    }

    scan_all();
    replay(&s_view);
    return ok && view_equal(&s_view, &want);
}

/* ── Public API ─────────────────────────────────────────────────────────────── */

np_safe_status_t np_nv_state_init(void)
{
    scan_all();
    replay(&s_view);
    return NP_SAFE_OK;
}

uint32_t np_nv_current_user(void)
{
    return s_view.current;
}

bool np_nv_cardiac_blocked(uint32_t user)
{
    if (s_view.any || view_has(&s_view, NP_SAFETY_USER_UNSPECIFIED)) {
        return true;                     /* unattributable: withheld from everyone */
    }
    if (user == NP_SAFETY_USER_UNSPECIFIED) {
        return s_view.n > 0U;            /* unknown user: cannot rule them out */
    }
    return view_has(&s_view, user);
}

bool np_nv_cardiac_outstanding(void)
{
    return s_view.any || (s_view.n > 0U);
}

bool np_nv_cardiac_set(uint32_t user, bool pending)
{
    return write_change(pending ? NP_NV_T_SET : NP_NV_T_CLR, user);
}

bool np_nv_set_current_user(uint32_t user)
{
    return write_change(NP_NV_T_USER, user);
}
