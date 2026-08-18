/* np_accel_shdr.c — SHDR accelerometer processing
 *
 * Document: NP-FW-EMMC-002 Rev 2 §G, §H.  NP-PRIV-REM-001 STEP-10.
 * IEC 62304: SW-02 Class B.
 *
 * Contract, threat model and the reasoning behind the clock-free expiry are in
 * np_accel_shdr.h.  This file implements it.
 *
 * Sensitive-data zeroing uses memset_explicit() (project convention); the
 * guarded C11 fallback below mirrors firmware/anon/src/np_anon_scratch.c.
 */

#include "np_accel_shdr.h"

#include <string.h>

/* ── memset_explicit fallback (non-elidable) ─────────────────────────────────
 * C23 provides it; the firmware builds -std=c11 and newlib's __STDC_LIB_EXT1__
 * signalling is unreliable across the arm-none-eabi versions in use, so gate on
 * the version macro and let the platform opt in.  The volatile function pointer
 * is what stops the compiler proving the clear is dead and removing it. */
#if !defined(NP_SHDR_HAVE_MEMSET_EXPLICIT) && \
    !(defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L)
static void *(*const volatile np_shdr_memset_fn)(void *, int, size_t) = memset;
static void memset_explicit(void *dst, int ch, size_t len)
{
    np_shdr_memset_fn(dst, ch, len);
}
#endif

/* Histogram bin lower edges, ~1.45× ratio.  The 15.0 g placeholder falls inside
 * bin 5, [14.0, 21.0), with bins 4 and 6 either side of it, so the programme can
 * resolve either side of the number it exists to test.  Last bin is open-ended. */
const float np_accel_char_g_bin_edges[NP_ACCEL_CHAR_G_BIN_COUNT] = {
    2.0f, 3.0f, 4.5f, 6.5f, 9.5f, 14.0f, 21.0f, 31.0f
};

/* ── CRC-32 (IEEE 802.3, reflected) ─────────────────────────────────────────
 * Local and bit-wise: the enrolment record is 16 bytes checked once per gap, so
 * a table would cost more flash than it saves. */
static uint32_t np_shdr_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1u)));
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

uint32_t np_accel_char_enrolment_crc(const np_accel_char_enrolment_t *enr)
{
    if (enr == NULL) {
        return 0u;
    }
    /* Serialised explicitly rather than CRC'd over the struct: struct padding is
     * indeterminate, so hashing the raw bytes would make the CRC depend on the
     * compiler and a record written by one build could fail validation under
     * another.  Fail-closed makes that failure invisible — the device would
     * simply stop collecting and nothing would say why. */
    uint8_t buf[11];
    buf[0]  = enr->enrolled ? 1u : 0u;
    buf[1]  = (uint8_t)(enr->programme_id & 0xFFu);
    buf[2]  = (uint8_t)(enr->programme_id >> 8);
    buf[3]  = (uint8_t)(enr->consent_epoch & 0xFFu);
    buf[4]  = (uint8_t)((enr->consent_epoch >> 8) & 0xFFu);
    buf[5]  = (uint8_t)((enr->consent_epoch >> 16) & 0xFFu);
    buf[6]  = (uint8_t)((enr->consent_epoch >> 24) & 0xFFu);
    buf[7]  = (uint8_t)(enr->records_emitted & 0xFFu);
    buf[8]  = (uint8_t)((enr->records_emitted >> 8) & 0xFFu);
    buf[9]  = (uint8_t)((enr->records_emitted >> 16) & 0xFFu);
    buf[10] = (uint8_t)((enr->records_emitted >> 24) & 0xFFu);
    return np_shdr_crc32(buf, sizeof buf);
}

/* ── The build gate ─────────────────────────────────────────────────────────
 *
 * On a shipping build this is a const fold of NP_ACCEL_CHAR_WINDOW_ENABLED, so
 * with the macro at 0 every extended path below is dead code and is removed.
 * Under NPTEST_HOST it is writable, so ONE test source exercises both
 * configurations and neither the "suppressed" nor the "emitted" assertion can
 * pass vacuously — each is the other's control. */
#ifdef NPTEST_HOST
bool np_accel_char_build_gate = (NP_ACCEL_CHAR_WINDOW_ENABLED != 0);
#else
static const bool np_accel_char_build_gate = (NP_ACCEL_CHAR_WINDOW_ENABLED != 0);
#endif

/* ── The fail-closed decision point (§H.3) ───────────────────────────────────
 *
 * Every condition is checked here, in one place, and every failure returns the
 * same answer: no.  A caller cannot get a partially-open window. */
bool np_accel_char_window_open(const np_accel_char_enrolment_t *enr)
{
    if (!np_accel_char_build_gate) {
        return false;                                   /* programme not built in */
    }
    if (enr == NULL) {
        return false;                                   /* no record ⇒ closed */
    }
    if (np_accel_char_enrolment_crc(enr) != enr->crc32) {
        return false;                                   /* corrupt ⇒ closed */
    }
    if (!enr->enrolled) {
        return false;                                   /* no affirmative opt-in */
    }
    if (enr->programme_id != NP_ACCEL_CHAR_PROGRAMME_ID) {
        return false;                                   /* consented to a different programme */
    }
    if (enr->consent_epoch < 1u) {
        return false;                                   /* epoch 0 is "never granted" */
    }
    if (enr->records_emitted >= NP_ACCEL_CHAR_RECORD_BUDGET) {
        return false;                                   /* window exhausted — the expiry */
    }
    return true;
}

/* ── Event scoring ──────────────────────────────────────────────────────────
 *
 * An impact event is a maximal run of consecutive samples at or above
 * NP_ACCEL_EVENT_FLOOR_G; its severity is the peak within that run.  Runs
 * rather than per-sample peaks, so one impact ringing over several samples is
 * counted once. */
static uint8_t np_accel_bin_for(float peak_g)
{
    uint8_t bin = 0;
    for (uint8_t i = 0; i < NP_ACCEL_CHAR_G_BIN_COUNT; i++) {
        if (peak_g >= np_accel_char_g_bin_edges[i]) {
            bin = i;
        }
    }
    return bin;
}

np_accel_status_t np_accel_shdr_evaluate(const float                  *samples,
                                         size_t                        n,
                                         np_accel_shdr_state_t        *state,
                                         np_accel_char_enrolment_t    *enr,
                                         np_shdr_accel_record_t       *out_std,
                                         np_shdr_accel_char_record_t  *out_char,
                                         bool                         *out_char_valid)
{
    if (state == NULL || out_std == NULL || out_char_valid == NULL) {
        return NP_ACCEL_ERR_ARG;
    }
    if (n > 0u && samples == NULL) {
        return NP_ACCEL_ERR_ARG;
    }

    *out_char_valid = false;
    memset(out_std, 0, sizeof *out_std);

    /* The window decision is taken BEFORE any scoring, so the extended path can
     * never be reached by a device that is not entitled to it, whatever the
     * samples look like. */
    const bool window_open = np_accel_char_window_open(enr);
    if (window_open && out_char != NULL) {
        memset(out_char, 0, sizeof *out_char);
    }

    /* ── Pass over the gap ─────────────────────────────────────────────────
     * Per-event peaks are consumed here and never stored.  Under §G only the
     * two booleans survive this loop; under §H a per-bin COUNT additionally
     * survives it.  In neither case does a g value, an axis, an orientation or
     * a within-gap position survive. */
    bool  drop_this_gap = false;
    bool  in_event      = false;
    float event_peak    = 0.0f;

    for (size_t i = 0; i <= n; i++) {
        const bool above = (i < n) && (samples[i] >= NP_ACCEL_EVENT_FLOOR_G);

        if (above) {
            if (!in_event) {
                in_event   = true;
                event_peak = samples[i];
            } else if (samples[i] > event_peak) {
                event_peak = samples[i];
            }
            continue;
        }

        if (!in_event) {
            continue;
        }

        /* Event closed — score it. */
        in_event = false;
        if (event_peak > NP_ACCEL_DROP_THRESHOLD_G) {
            drop_this_gap = true;
        }
        if (window_open && out_char != NULL) {
            const uint8_t bin = np_accel_bin_for(event_peak);
            if (out_char->g_bin[bin] < UINT16_MAX) {
                out_char->g_bin[bin]++;
            }
            if (out_char->impact_event_count < UINT16_MAX) {
                out_char->impact_event_count++;
            }
        }
        event_peak = 0.0f;
    }

    /* ── Standard record (§G.2) — identical on every device ────────────────
     * Rolling window is counted in SESSION GAPS, not days; see the header note
     * on NP_ACCEL_MAINT_WINDOW_GAPS. */
    state->drop_flags = (uint8_t)((state->drop_flags << 1) & 0x7Fu);
    if (drop_this_gap) {
        state->drop_flags |= 1u;
    }
    if (state->gaps_recorded < NP_ACCEL_MAINT_WINDOW_GAPS) {
        state->gaps_recorded++;
    }

    unsigned drops_in_window = 0;
    for (unsigned i = 0; i < NP_ACCEL_MAINT_WINDOW_GAPS; i++) {
        if (state->drop_flags & (1u << i)) {
            drops_in_window++;
        }
    }

    out_std->drop_detected     = drop_this_gap;
    out_std->maintenance_alert = (drops_in_window >= NP_ACCEL_MAINT_THRESHOLD);

    /* ── Extended record (§H.2) ────────────────────────────────────────────
     * The budget is consumed here and only here.  Because the counter advances
     * exactly when a record is emitted, it cannot be spent by anything other
     * than the collection it bounds, and it cannot be un-spent. */
    if (window_open && out_char != NULL) {
        enr->records_emitted++;
        enr->crc32 = np_accel_char_enrolment_crc(enr);

        out_char->programme_id  = NP_ACCEL_CHAR_PROGRAMME_ID;
        out_char->consent_epoch = enr->consent_epoch;
        out_char->record_seq    = enr->records_emitted;   /* 1-based */
        *out_char_valid         = true;
    }

    return NP_ACCEL_OK;
}

/* ── I/O path ────────────────────────────────────────────────────────────────
 *
 * Platform seams.  On the host build these are stubs so the module is testable
 * without an IMU or an eMMC; on target they are supplied by the hub HAL. */

#ifndef NPTEST_HOST
extern np_accel_status_t np_imu_read_gap_resultants(float *dst, size_t cap, size_t *out_n);
extern np_accel_status_t np_shdr_write_accel_record(const np_shdr_accel_record_t *rec);
extern np_accel_status_t np_shdr_write_accel_char_record(const np_shdr_accel_char_record_t *rec);
extern np_accel_status_t np_config_read_char_enrolment(np_accel_char_enrolment_t *enr);
extern np_accel_status_t np_config_write_char_enrolment(const np_accel_char_enrolment_t *enr);
extern np_accel_status_t np_config_read_accel_state(np_accel_shdr_state_t *state);
extern np_accel_status_t np_config_write_accel_state(const np_accel_shdr_state_t *state);
#else
np_accel_status_t np_imu_read_gap_resultants(float *dst, size_t cap, size_t *out_n);
np_accel_status_t np_shdr_write_accel_record(const np_shdr_accel_record_t *rec);
np_accel_status_t np_shdr_write_accel_char_record(const np_shdr_accel_char_record_t *rec);
np_accel_status_t np_config_read_char_enrolment(np_accel_char_enrolment_t *enr);
np_accel_status_t np_config_write_char_enrolment(const np_accel_char_enrolment_t *enr);
np_accel_status_t np_config_read_accel_state(np_accel_shdr_state_t *state);
np_accel_status_t np_config_write_accel_state(const np_accel_shdr_state_t *state);
#endif

/* One gap's worth of resultant magnitudes.  SRAM only — §G.6 requires that no
 * intermediate value reaches any partition. */
#define NP_ACCEL_GAP_SAMPLE_CAP   512u

np_accel_status_t np_accel_shdr_process_session_gap(void)
{
    static float raw[NP_ACCEL_GAP_SAMPLE_CAP];

    np_accel_shdr_state_t        state = {0};
    np_accel_char_enrolment_t    enr   = {0};
    np_shdr_accel_record_t       std_rec;
    np_shdr_accel_char_record_t  char_rec;
    bool                         char_valid = false;
    size_t                       n = 0;

    /* A failed enrolment read leaves enr zeroed, which np_accel_char_window_open
     * rejects on the CRC.  Not enrolled is the safe default and it is what an
     * unreadable record produces — the error is not swallowed, it is answered. */
    const bool have_enrolment = (np_config_read_char_enrolment(&enr) == NP_ACCEL_OK);
    (void)np_config_read_accel_state(&state);

    np_accel_status_t st = np_imu_read_gap_resultants(raw, NP_ACCEL_GAP_SAMPLE_CAP, &n);
    if (st != NP_ACCEL_OK) {
        memset_explicit(raw, 0, sizeof raw);
        return st;
    }

    st = np_accel_shdr_evaluate(raw, n, &state,
                                have_enrolment ? &enr : NULL,
                                &std_rec, &char_rec, &char_valid);

    /* Zeroed before anything is written, so the raw buffer cannot be live while
     * an eMMC transaction is in flight, and on every path out of this function. */
    memset_explicit(raw, 0, sizeof raw);

    if (st != NP_ACCEL_OK) {
        return st;
    }

    (void)np_config_write_accel_state(&state);

    st = np_shdr_write_accel_record(&std_rec);
    if (st != NP_ACCEL_OK) {
        return st;
    }

    if (char_valid) {
        /* Budget is persisted BEFORE the record is written.  If power is lost
         * between the two, the device has spent a record it did not send —
         * which shortens the window.  The other order would let a reset-loop
         * emit unbounded records against the same budget slot, which lengthens
         * it.  Fail-closed picks the direction that can only under-collect. */
        (void)np_config_write_char_enrolment(&enr);
        (void)np_shdr_write_accel_char_record(&char_rec);
    }

    return NP_ACCEL_OK;
}
