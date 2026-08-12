/*
 * NeurOne Zone Module Announcement — Orchestration
 * Document: NP-FW-ZA-001 Rev 1 §8, as amended by the 2026-07-28 supersession
 * note at the top of docs/superseded/np_fw_za_001.md.
 *
 * ── The insertion cue no longer plays here ───────────────────────────────────
 *
 * Rev 1 routed a confirmed insertion through NP_ZA_STATE_ANNOUNCING, which
 * played a bone-conduction clip at the mastoid. That could never be heard:
 * seating a module requires the helmet OFF the head, and bone conduction only
 * reaches a user whose head the transducer is pressed against. The cue fired the
 * instant the module was electrically seated — with the transducer touching
 * nobody. This was true of the original five-zone design, not a consequence of
 * the hex-tile redesign.
 *
 * A confirmed insertion now notifies the COMPANION APP over the module-status
 * GATT characteristic (np_zone_notify.h), which speaks the confirmation on the
 * device the user is holding. ANNOUNCING is gone from the insertion path.
 *
 * The tone engine itself is NOT retired — np_za_play_worn_cue() keeps it
 * available for announcements delivered while the helmet IS worn (session-start
 * configuration readback, in-session fault alerts).
 *
 * Drives five parallel slot state machines (one per physical zone socket).
 *
 *   IDLE → SETTLING → DEBOUNCING → ACTIVE
 *                                    ↓ (module removed)
 *                                  REMOVING → IDLE
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
 *                On pass:  log to SHDR; notify the app; fire insert_cb; → ACTIVE.
 *                On fail:  log SHDR fault; notify the app with the fault bit;
 *                          → IDLE (re-poll after one interval).
 *
 *   ACTIVE     : Module present and reported.  Polls for absence.
 *                On absence: → REMOVING.
 *
 *   REMOVING   : Confirms absence for NP_ZA_REMOVAL_DEBOUNCE_MS.
 *                On confirmed removal: notify the app; fire remove_cb; → IDLE.
 *
 * ── Socket addressing (transitional) ─────────────────────────────────────────
 *
 * The ZONE_ID resistor ladder this detector polls is itself retired
 * (NP-HEX-ZM-001 replaced it with np_module_map's UID auto-inventory). Porting
 * detection onto that is blocked on OI-HEXMAP-02 (the module I2C/1-wire
 * inventory_fn does not exist yet). Until it lands, slot index doubles as socket
 * index through np_za_slot_socket_id(), so the notify path is already
 * socket-keyed and np_module_map can be swapped in underneath without touching
 * the wire format or the app.
 *
 * SHDR entries (NP-FW-EMMC-001 Rev 1 §12): accessory authentication pass/fail
 * logged at DEBOUNCING exit.  No user biology — SHDR classification confirmed.
 */

#include "np_zone_announce.h"
#include "np_zone_notify.h"
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

/* ── App notification helpers ────────────────────────────────────────────────
 *
 * The Rev 1 announcement queue lived here: it serialised bone-conduction clips
 * because the transducer is a single output and five slots could confirm at
 * once. Frames have no such constraint — each socket's change is its own delta —
 * so the queue is gone with the insertion-time audio it existed to schedule.
 */

/* Socket id for a legacy slot index. Identity while the ZONE_ID ladder is still
 * the detector; the seam exists so np_module_map's real socket ids drop in here
 * (OI-HEXMAP-02) without touching the notify path or the wire format. */
static uint8_t np_za_slot_socket_id(uint8_t slot_index)
{
    return slot_index;
}

/*
 * The helmet's permanent socket geometry, published ONCE when the app links.
 *
 * Anatomy lives here, not in the per-change status record: where a socket IS
 * cannot change when a module is swapped, so transmitting it on every insertion
 * would be paying repeatedly for a fixed fact. The app reads this map at link
 * time and thereafter needs only (socket, type, flags) per change.
 *
 * These five entries are the retired ZONE_ID ladder's fixed positions, which is
 * all this transitional detector can describe. When detection moves to
 * np_module_map (OI-HEXMAP-02) this table is replaced wholesale by that module's
 * np_socket_geom_t geometry — same wire format, ~80 entries instead of 5, and
 * real x/y from the shell CAD.
 */
static const np_zn_socket_desc_t s_legacy_socket_map[NP_ZONE_COUNT] = {
    /* Positions in aircraft body axes, mm, origin at the shell ellipsoid centre
     * (+x forward, +y right, +z down). These five are the retired ZONE_ID
     * ladder's fixed positions, approximated on the scan-grounded surface — the
     * ladder itself only reports WHICH of five, so anything finer would be
     * invented. Real per-socket coordinates arrive with np_module_map's geometry
     * table (OI-HEXMAP-02); hardware/np_socket_map.json is where they are
     * derived today. */
    /* slot 0 — ZM-01 Frontal Left  */ { 0u,  119, -44, -38, true },
    /* slot 1 — ZM-02 Frontal Right */ { 1u,  119,  44, -38, true },
    /* slot 2 — ZM-03 Vertex        */ { 2u,    0,   0, -157, true },
    /* slot 3 — ZM-04 Parietal Left */ { 3u,  -60, -76, -119, true },
    /* slot 4 — ZM-05 Parietal Right*/ { 4u,  -60,  76, -119, true },
};

/*
 * Report one socket's change to the companion app. This is what replaced the
 * bone-conduction announcement: the confirmation is delivered where the user
 * actually is, on the phone or tablet they are holding during setup.
 *
 * Three bytes on the wire. The app resolves the socket to a place using the map
 * it already holds.
 *
 * Failure to transmit is deliberately non-fatal to the state machine — a module
 * that is seated IS seated whether or not the app heard about it. The next
 * snapshot re-synchronises the app.
 */
static void notify_app(uint8_t slot_index, bool present, bool fault)
{
    np_zn_socket_state_t st;
    memset(&st, 0, sizeof(st));
    st.socket_id = np_za_slot_socket_id(slot_index);
    /* The ZONE_ID ladder reports POSITION only — it cannot distinguish a T1-A
     * base tile from a T1-B electrode tile. Report the type as unidentified
     * rather than guessing one; the app then confirms position without claiming
     * a type. Real types arrive with np_module_map's UID inventory
     * (OI-HEXMAP-02). Note this is UNKNOWN-but-present, which is a different
     * record than fault (present clear) — a seated module we simply can't name. */
    st.module_type = present ? NP_ZN_MODULE_UNKNOWN : NP_ZN_MODULE_NONE;
    st.present     = present;
    st.fault       = fault;

    (void)np_zone_notify_event(&st);
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
        if (counts >= NP_ZA_ADC_NO_MODULE_LO) {
            /* Socket genuinely empty — arm the fault latch for the next module.
             * Only a real removal clears it, so a stuck unidentifiable module
             * reports once rather than on every poll. */
            slot->fault_reported = false;
            break;
        }
        if (slot->fault_reported) {
            /* Same unidentified module still seated. Already reported; do not
             * re-run debounce, re-notify the app, or re-write SHDR. */
            break;
        }
        /* Possible module — enter settling delay. */
        slot->settle_until_ms = now_ms + NP_ZA_INSERTION_SETTLE_MS;
        slot->state           = NP_ZA_STATE_SETTLING;
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
            notify_app(slot_index, /*present=*/false, /*fault=*/true);
            slot->fault_reported = true;   /* report once; see the latch note */
            slot->state = NP_ZA_STATE_IDLE;
            break;
        }

        /* UNKNOWN zone (unrecognized resistor value) — log fault, tell the app,
         * stay idle. Rev 1 played a descending two-tone error cue on the
         * bone-conduction transducer here; the user could not hear that either,
         * for the same reason. The app surfaces the fault instead. */
        if (zone == NP_ZONE_UNKNOWN) {
            log_shdr(slot_index, NP_ZONE_UNKNOWN, false, agree);
            notify_app(slot_index, /*present=*/false, /*fault=*/true);
            slot->fault_reported = true;   /* report once; see the latch note */
            slot->confirmed_zone = NP_ZONE_NONE;
            slot->state          = NP_ZA_STATE_IDLE;
            break;
        }

        /* Valid zone confirmed. */
        log_shdr(slot_index, zone, true, agree);
        slot->confirmed_zone = zone;
        slot->state          = NP_ZA_STATE_ACTIVE;

        /* Notify the application first so it can enable the PBM zone
         * immediately (announcement_done=false preserves the Rev 1 contract). */
        if (s_ctx.insert_cb) {
            s_ctx.insert_cb(zone, false);
        }

        /* Deliver the user-facing confirmation to the companion app. This is
         * where Rev 1 queued a bone-conduction clip nobody could hear. */
        notify_app(slot_index, /*present=*/true, /*fault=*/false);

        /* announcement_done=true now means "the user-facing confirmation has
         * been dispatched to the app", not "the headset finished a clip". */
        if (s_ctx.insert_cb) {
            s_ctx.insert_cb(zone, true);
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

        if (removed != NP_ZONE_NONE && removed != NP_ZONE_UNKNOWN) {
            /* Tell the app the socket is empty so its map clears — a stale
             * "present" would let a placement gate pass on a module that is no
             * longer there. */
            notify_app(slot_index, /*present=*/false, /*fault=*/false);
            if (s_ctx.remove_cb) {
                s_ctx.remove_cb(removed);
            }
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
    /* No audio is scheduled from here any more. Insertion confirmations go to
     * the companion app (notify_app); the tone engine is driven only by
     * np_za_play_worn_cue(), which its caller gates on the helmet being worn. */
}

np_za_status_t np_za_publish_socket_map(void)
{
    np_zn_status_t rc = np_zone_notify_socket_map(s_legacy_socket_map,
                                                  (uint16_t)NP_ZONE_COUNT);
    return (rc == NP_ZN_OK) ? NP_ZA_OK : NP_ZA_ERR_INVALID_ARG;
}

np_za_status_t np_za_play_worn_cue(np_zone_id_t cue)
{
    if (np_za_audio_busy()) {
        return NP_ZA_ERR_AUDIO_BUSY;
    }
    return np_za_audio_play(&s_ctx, cue);
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
