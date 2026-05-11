/*
 * NeuroPulse Zone Module Announcement — Orchestration
 * Document: NP-FW-ZA-001 Rev A §8
 *
 * Drives five parallel slot state machines (one per physical zone socket).
 * Each machine progresses:
 *
 *   IDLE → SETTLING → DEBOUNCING → ANNOUNCING → ACTIVE
 *                                             ↓ (module removed)
 *                                           REMOVING → IDLE
 *
 * State transitions:
 *   IDLE       : ADC reads NP_ZONE_NONE (module absent).  Polls every
 *                NP_ZA_POLL_IDLE_MS.  On first non-NONE read: → SETTLING.
 *
 *   SETTLING   : Brief NP_ZA_INSERTION_SETTLE_MS delay to let FPC contacts
 *                settle before debounce ADC reads begin.  → DEBOUNCING.
 *
 *   DEBOUNCING : Calls np_za_detect_poll() at NP_ZA_DEBOUNCE_INTERVAL_MS
 *                intervals (3 reads, ≥ 2/3 majority required).
 *                On pass:  log to SHDR; fire insert_cb; → ANNOUNCING.
 *                On fail:  log SHDR fault; → IDLE (re-poll after one interval).
 *
 *   ANNOUNCING : Audio clip playing.  Calls np_za_audio_tick() via DMA ISR
 *                (not in this function).  When np_za_audio_busy() → false:
 *                fire insert_cb (announcement_done=true); → ACTIVE.
 *
 *   ACTIVE     : Module present and announced.  Polls for absence.
 *                On absence: → REMOVING.
 *
 *   REMOVING   : Confirms absence for NP_ZA_REMOVAL_DEBOUNCE_MS.
 *                On confirmed removal: fire remove_cb; → IDLE.
 *
 * Only one audio clip plays at a time (bone conduction is a single output).
 * If multiple insertions occur simultaneously, announcements are queued
 * in slot order (ZM-01 … ZM-05) and played back-to-back.
 *
 * SHDR entries (NP-FW-EMMC-001 Rev A §12): accessory authentication pass/fail
 * logged at DEBOUNCING exit.  No user biology — SHDR classification confirmed.
 */

#include "np_zone_announce.h"
#include <string.h>

/* ── Forward declarations for functions defined in other TUs ─────────────────── */
np_za_status_t np_za_detect_poll(uint8_t slot_index, np_za_debounce_t *deb,
                                  uint32_t now_ms, np_zone_id_t *zone_out);
void           np_za_detect_reset(np_za_debounce_t *deb, uint32_t first_read_ms);
bool           np_za_detect_absent(uint8_t slot_index);
np_za_status_t np_za_audio_play(np_za_ctx_t *ctx, np_zone_id_t zone);
void           np_za_audio_stop(np_za_ctx_t *ctx);
bool           np_za_audio_busy(void);

/* ── Static module context (single static allocation, no heap) ───────────────── */

static np_za_ctx_t s_ctx;

/* ── Announcement queue ──────────────────────────────────────────────────────── */

/* Slots waiting to announce (ring buffer, capacity = NP_ZONE_COUNT). */
static uint8_t  s_announce_queue[NP_ZONE_COUNT];
static uint8_t  s_queue_head;
static uint8_t  s_queue_tail;
static uint8_t  s_queue_count;

static void queue_push(uint8_t slot_index)
{
    if (s_queue_count >= NP_ZONE_COUNT) {
        return;  /* queue full — should not happen (only 5 slots) */
    }
    s_announce_queue[s_queue_tail] = slot_index;
    s_queue_tail = (s_queue_tail + 1U) % NP_ZONE_COUNT;
    s_queue_count++;
}

static bool queue_pop(uint8_t *slot_out)
{
    if (s_queue_count == 0U) {
        return false;
    }
    *slot_out = s_announce_queue[s_queue_head];
    s_queue_head = (s_queue_head + 1U) % NP_ZONE_COUNT;
    s_queue_count--;
    return true;
}

/* ── SHDR logging helper ─────────────────────────────────────────────────────── */

static void log_shdr(uint8_t slot_index, np_zone_id_t zone,
                     bool pass, uint8_t reads_agreed)
{
    if (!s_ctx.shdr_cb) {
        return;
    }
    np_za_shdr_auth_entry_t entry = {
        .zone_id              = (uint8_t)zone,
        .slot_index           = slot_index,
        .auth_result          = pass ? 0U : 1U,
        .debounce_reads_passed = reads_agreed,
        .session_count        = s_ctx.device_session_count,
    };
    s_ctx.shdr_cb(&entry);
}

/* ── Single-slot state machine ───────────────────────────────────────────────── */

static void tick_slot(uint8_t slot_index, uint32_t now_ms)
{
    np_za_slot_ctx_t *slot = &s_ctx.slots[slot_index];

    switch (slot->state) {

    /* ── IDLE: waiting for insertion ─────────────────────────────────────── */
    case NP_ZA_STATE_IDLE: {
        uint16_t counts = 0U;
        if (!np_za_platform_adc_read(slot_index, &counts)) {
            break;  /* ADC read failed — stay idle, retry next poll */
        }
        if (counts < NP_ZA_ADC_NO_MODULE_LO) {
            /* Possible module — enter settling delay. */
            slot->settle_until_ms = now_ms + NP_ZA_INSERTION_SETTLE_MS;
            slot->state           = NP_ZA_STATE_SETTLING;
        }
        break;
    }

    /* ── SETTLING: waiting for contact bounce to settle ─────────────────── */
    case NP_ZA_STATE_SETTLING: {
        if (now_ms < slot->settle_until_ms) {
            break;
        }
        /* Quick check: still present after settle? */
        if (np_za_detect_absent(slot_index)) {
            slot->state = NP_ZA_STATE_IDLE;
            break;
        }
        /* Begin debounce — first read happens immediately. */
        np_za_detect_reset(&slot->debounce, now_ms);
        slot->state = NP_ZA_STATE_DEBOUNCING;
        break;
    }

    /* ── DEBOUNCING: collecting 3 ADC reads ──────────────────────────────── */
    case NP_ZA_STATE_DEBOUNCING: {
        np_zone_id_t zone = NP_ZONE_NONE;
        np_za_status_t rc = np_za_detect_poll(slot_index, &slot->debounce,
                                               now_ms, &zone);

        if (rc == NP_ZA_OK && slot->debounce.read_count < NP_ZA_DEBOUNCE_READS) {
            break;  /* still collecting — wait */
        }

        /* Count agreeing reads for SHDR. */
        uint8_t agree = 0U;
        for (uint8_t i = 0U; i < NP_ZA_DEBOUNCE_READS; i++) {
            if (slot->debounce.zone[i] == zone) {
                agree++;
            }
        }

        if (rc != NP_ZA_OK || zone == NP_ZONE_NONE) {
            /* Debounce failed or module disappeared — log and return to IDLE. */
            log_shdr(slot_index, NP_ZONE_UNKNOWN, false, agree);
            slot->state = NP_ZA_STATE_IDLE;
            break;
        }

        /* UNKNOWN zone (unrecognised resistor value) — log fault, stay idle. */
        if (zone == NP_ZONE_UNKNOWN) {
            log_shdr(slot_index, NP_ZONE_UNKNOWN, false, agree);
            /* Queue error announcement (descending two-tone). */
            s_ctx.announcing_zone = NP_ZONE_UNKNOWN;
            queue_push(slot_index);
            slot->confirmed_zone = NP_ZONE_UNKNOWN;
            slot->state          = NP_ZA_STATE_ANNOUNCING;
            break;
        }

        /* Valid zone confirmed. */
        log_shdr(slot_index, zone, true, agree);
        slot->confirmed_zone = zone;
        slot->state          = NP_ZA_STATE_ANNOUNCING;

        /* Notify application before audio (enables PBM zone immediately). */
        if (s_ctx.insert_cb) {
            s_ctx.insert_cb(zone, false);
        }

        /* Queue this slot for audio announcement. */
        queue_push(slot_index);
        break;
    }

    /* ── ANNOUNCING: audio clip in progress ──────────────────────────────── */
    case NP_ZA_STATE_ANNOUNCING: {
        /* Audio is driven by DMA ISR; here we just check for completion. */
        if (np_za_audio_busy()) {
            break;
        }

        /* Current announcement finished — try next queued slot. */
        uint8_t next_slot;
        if (queue_pop(&next_slot)) {
            np_za_ctx_t *ctx = &s_ctx;
            np_za_audio_play(ctx, s_ctx.slots[next_slot].confirmed_zone);
            /* Stay in ANNOUNCING; the newly played slot will check its own state. */
        }

        /* This slot's announcement is done. */
        if (slot->confirmed_zone != NP_ZONE_UNKNOWN) {
            if (s_ctx.insert_cb) {
                s_ctx.insert_cb(slot->confirmed_zone, true);
            }
            slot->state = NP_ZA_STATE_ACTIVE;
        } else {
            slot->state = NP_ZA_STATE_IDLE;
        }
        break;
    }

    /* ── ACTIVE: module present and confirmed ────────────────────────────── */
    case NP_ZA_STATE_ACTIVE: {
        if (np_za_detect_absent(slot_index)) {
            if (!slot->removal_debounce_active) {
                slot->removal_debounce_active = true;
                slot->removal_since_ms        = now_ms;
            } else if ((now_ms - slot->removal_since_ms) >= NP_ZA_REMOVAL_DEBOUNCE_MS) {
                slot->state = NP_ZA_STATE_REMOVING;
            }
        } else {
            slot->removal_debounce_active = false;
        }
        break;
    }

    /* ── REMOVING: confirmed absent, cleaning up ─────────────────────────── */
    case NP_ZA_STATE_REMOVING: {
        np_zone_id_t removed = slot->confirmed_zone;
        memset(slot, 0, sizeof(*slot));  /* reset all slot state */
        slot->state = NP_ZA_STATE_IDLE;

        if (s_ctx.remove_cb && removed != NP_ZONE_NONE &&
            removed != NP_ZONE_UNKNOWN) {
            s_ctx.remove_cb(removed);
        }
        break;
    }

    default:
        slot->state = NP_ZA_STATE_IDLE;
        break;
    }
}

/* ── Public API ──────────────────────────────────────────────────────────────── */

np_za_status_t np_za_init(np_za_insert_cb_t insert_cb,
                           np_za_remove_cb_t remove_cb,
                           np_za_shdr_cb_t   shdr_cb,
                           uint32_t          device_session_count)
{
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.insert_cb            = insert_cb;
    s_ctx.remove_cb            = remove_cb;
    s_ctx.shdr_cb              = shdr_cb;
    s_ctx.device_session_count = device_session_count;

    s_queue_head  = 0U;
    s_queue_tail  = 0U;
    s_queue_count = 0U;

    return NP_ZA_OK;
}

void np_za_deinit(void)
{
    np_za_audio_stop(&s_ctx);
    memset(&s_ctx, 0, sizeof(s_ctx));
}

void np_za_tick(uint32_t now_ms)
{
    for (uint8_t i = 0U; i < NP_ZONE_COUNT; i++) {
        tick_slot(i, now_ms);
    }

    /* Start queued audio if nothing is currently playing. */
    if (!np_za_audio_busy() && s_queue_count > 0U) {
        uint8_t slot;
        if (queue_pop(&slot)) {
            np_za_audio_play(&s_ctx, s_ctx.slots[slot].confirmed_zone);
        }
    }
}

np_zone_id_t np_za_get_zone(uint8_t slot_index)
{
    if (slot_index >= NP_ZONE_COUNT) {
        return NP_ZONE_NONE;
    }
    if (s_ctx.slots[slot_index].state == NP_ZA_STATE_ACTIVE) {
        return s_ctx.slots[slot_index].confirmed_zone;
    }
    return NP_ZONE_NONE;
}
