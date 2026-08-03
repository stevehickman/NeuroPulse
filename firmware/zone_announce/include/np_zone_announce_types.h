/*
 * NeurOne Zone Module Announcement — Type Definitions
 * Document: NP-FW-ZA-001 Rev A §4
 */

#ifndef NP_ZONE_ANNOUNCE_TYPES_H
#define NP_ZONE_ANNOUNCE_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "np_zone_announce_config.h"

/* ── Zone identifiers ───────────────────────────────────────────────────────── */

/*
 * Five zone modules map to the five independently addressable PBM zones.
 * Zone names follow anatomical convention (EEG 10-20 system reference).
 *
 * ZM-01 Frontal Left  — over F3 / Fp1 region
 * ZM-02 Frontal Right — over F4 / Fp2 region
 * ZM-03 Vertex        — central crown, over Cz
 * ZM-04 Parietal Left — over P3
 * ZM-05 Parietal Right— over P4
 */
typedef enum {
    NP_ZONE_NONE            = 0,    /* no module detected (pin floating high)    */
    NP_ZONE_FRONTAL_LEFT    = 1,    /* ZM-01, 10 kΩ, ADC ≈ 2048                */
    NP_ZONE_FRONTAL_RIGHT   = 2,    /* ZM-02, 22 kΩ, ADC ≈ 2818                */
    NP_ZONE_VERTEX          = 3,    /* ZM-03, 47 kΩ, ADC ≈ 3378                */
    NP_ZONE_PARIETAL_LEFT   = 4,    /* ZM-04, 100 kΩ, ADC ≈ 3723               */
    NP_ZONE_PARIETAL_RIGHT  = 5,    /* ZM-05, 220 kΩ, ADC ≈ 3918               */
    NP_ZONE_UNKNOWN         = 6,    /* ADC value outside all expected ranges     */
} np_zone_id_t;

#define NP_ZONE_COUNT   5U          /* number of valid zone positions            */

/* ── Module status codes ────────────────────────────────────────────────────── */

typedef enum {
    NP_ZA_OK                    =  0,
    NP_ZA_ERR_INVALID_ARG       = -1,
    NP_ZA_ERR_ADC_TIMEOUT       = -2,
    NP_ZA_ERR_DEBOUNCE_FAIL     = -3,  /* ≥ 2/3 reads did not agree             */
    NP_ZA_ERR_AUDIO_BUSY        = -4,
    NP_ZA_ERR_AUDIO_INIT        = -5,
    NP_ZA_ERR_SAI_TIMEOUT       = -6,
    NP_ZA_ERR_UNKNOWN_ZONE      = -7,  /* zone resistor value not recognized     */
} np_za_status_t;

/* ── Debounce state ─────────────────────────────────────────────────────────── */

typedef struct {
    uint16_t    raw_adc[NP_ZA_DEBOUNCE_READS];  /* consecutive ADC readings      */
    np_zone_id_t zone[NP_ZA_DEBOUNCE_READS];    /* classified result per reading  */
    uint8_t     read_count;   /* reads completed so far (0 → NP_ZA_DEBOUNCE_READS)*/
    uint32_t    next_read_ms; /* system time for next ADC read                   */
} np_za_debounce_t;

/* ── Zone detection state machine ───────────────────────────────────────────── */

typedef enum {
    NP_ZA_STATE_IDLE        = 0,  /* polling: waiting for insertion             */
    NP_ZA_STATE_SETTLING    = 1,  /* brief settle delay after first detection   */
    NP_ZA_STATE_DEBOUNCING  = 2,  /* collecting 3 ADC reads                     */
    NP_ZA_STATE_ANNOUNCING  = 3,  /* playing bone conduction announcement        */
    NP_ZA_STATE_ACTIVE      = 4,  /* module confirmed present and announced      */
    NP_ZA_STATE_REMOVING    = 5,  /* removal detected, debouncing absence        */
} np_za_state_t;

/* ── Tone segment (DDS descriptor) ─────────────────────────────────────────── */

/*
 * A clip is an array of segments played sequentially.
 * freq_hz == 0 → silence for duration_ms.
 */
typedef struct {
    uint16_t freq_hz;       /* tone frequency; 0 = silence                      */
    uint16_t duration_ms;
} np_za_tone_segment_t;

/* ── DDS synthesiser state ──────────────────────────────────────────────────── */

typedef struct {
    uint32_t phase_acc;     /* Q32 phase accumulator                             */
    uint32_t phase_inc;     /* Q32 phase increment = (freq × 2^32) / Fs         */
    int16_t  amplitude;     /* current amplitude (Q15, ramped on start/stop)     */
    int16_t  target_amp;    /* target amplitude (set from NP_ZA_AUDIO_AMPLITUDE) */
    uint32_t ramp_step;     /* amplitude increment per sample during ramp        */
} np_za_dds_t;

/* ── Clip player state ──────────────────────────────────────────────────────── */

typedef struct {
    const np_za_tone_segment_t *segments;   /* pointer to segment array in flash */
    uint8_t  seg_count;     /* total segments in clip                            */
    uint8_t  seg_idx;       /* current segment index                             */
    uint32_t seg_samples_remaining;  /* samples left in current segment          */
    bool     active;
} np_za_clip_player_t;

/* ── SHDR authentication log entry ─────────────────────────────────────────── */

/*
 * Logged to SHDR partition per NP-FW-EMMC-001 Rev A §12.
 * Fields: accessory authentication pass/fail only — no user biology.
 */
typedef struct {
    uint8_t  zone_id;       /* np_zone_id_t value (1–5, 6=unknown)              */
    uint8_t  slot_index;    /* physical slot (0-based; equals zone_id−1 if valid)*/
    uint8_t  auth_result;   /* 0 = pass, 1 = fail (unknown zone)                */
    uint8_t  debounce_reads_passed; /* how many of 3 reads agreed               */
    uint32_t session_count; /* device session count at time of insertion         */
} np_za_shdr_auth_entry_t;

/* ── Callback types ─────────────────────────────────────────────────────────── */

/*
 * Called by the announce module when a zone module is successfully identified.
 * The application uses this to enable the corresponding PBM zone.
 * announcement_done is true on the second call (after audio has finished).
 */
typedef void (*np_za_insert_cb_t)(np_zone_id_t zone, bool announcement_done);

/*
 * Called when a confirmed module is removed.
 */
typedef void (*np_za_remove_cb_t)(np_zone_id_t zone);

/*
 * Called on SHDR write events — caller routes the entry to the SHDR partition.
 */
typedef void (*np_za_shdr_cb_t)(const np_za_shdr_auth_entry_t *entry);

/* ── Per-slot context (one per physical zone slot) ──────────────────────────── */

typedef struct {
    np_za_state_t       state;
    np_zone_id_t        confirmed_zone;   /* zone after successful debounce      */
    np_za_debounce_t    debounce;
    uint32_t            settle_until_ms;  /* system time when settle ends        */
    uint32_t            removal_since_ms; /* system time when removal first seen */
    bool                removal_debounce_active;
    /*
     * A failed identification returns the slot to IDLE, which immediately
     * re-detects the still-seated module and fails again — an unbounded loop for
     * as long as the bad module stays in. Rev A was rate-limited only
     * incidentally, by the ~700 ms error clip it played in ANNOUNCING; with the
     * audio gone that accident is gone too. This latch reports each fault ONCE
     * and is cleared only when the slot reads genuinely empty, so a stuck module
     * cannot flood the app with notifications or burn SHDR flash writes.
     */
    bool                fault_reported;
} np_za_slot_ctx_t;

/* ── Module-level context (single instance, static allocation) ─────────────── */

typedef struct {
    np_za_slot_ctx_t    slots[NP_ZONE_COUNT];
    np_za_dds_t         dds;
    np_za_clip_player_t player;
    int16_t             dma_buf[NP_ZA_DMA_BUF_SAMPLES]; /* ping-pong DMA buffer */
    np_zone_id_t        announcing_zone; /* zone currently being announced        */
    np_za_insert_cb_t   insert_cb;
    np_za_remove_cb_t   remove_cb;
    np_za_shdr_cb_t     shdr_cb;
    bool                audio_initialized;
    uint32_t            device_session_count; /* from SHDR, set at init          */
} np_za_ctx_t;

#endif /* NP_ZONE_ANNOUNCE_TYPES_H */
