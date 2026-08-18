/* np_accel_shdr.h — SHDR accelerometer processing
 *
 * Document: NP-FW-EMMC-002 Rev 2 §G (standing rule) and §H (characterisation
 *           programme).  NP-PRIV-REM-001 STEP-10.
 * IEC 62304: SW-02 Class B (main processor).
 *
 * ── What this module is for ─────────────────────────────────────────────────
 *
 * The IMU is read between sessions.  §G permits exactly two derived booleans to
 * reach SHDR — drop_detected and maintenance_alert — and prohibits the raw
 * series, any per-event g value, orientation, per-event timestamps, and a
 * cumulative drop count.  That is the STANDING RULE and it is what this module
 * does by default, on every device, in every build.
 *
 * §H adds a bounded, consented, self-closing exception: during the
 * characterisation window an ENROLLED device additionally emits a COARSENED
 * histogram of impact peaks, because the two thresholds below are unvalidated
 * guesses and there is currently no way to learn better ones.  See §H.1.
 *
 * ── The expiry has no calendar, and that is not a simplification ────────────
 *
 * NP-MOD-ID-001 §7.3 bounds its own characterisation window at "24 months from
 * first fleet upload".  That is not implementable here.  The headset has no
 * battery, no coin cell and no VBAT rail (CLAUDE.md §4.5 — it is USB-C powered,
 * and Mode 3 runs off a power bank), so the i.MX RT1062 SNVS RTC has no backup
 * domain: wall time is LOST on every disconnect and is re-supplied by the phone
 * as a parameter (see np_edf_write_header()'s time_t argument, §E.3).  A
 * calendar expiry on this device is therefore both losable and settable
 * backwards, which is the opposite of fail-closed.
 *
 * So the window is denominated in the records it admits.
 * NP_ACCEL_CHAR_RECORD_BUDGET is a per-device ceiling on extended records, and
 * the counter that consumes it advances ONLY when an extended record is
 * emitted.  Three properties follow:
 *
 *   1. It is monotone by construction.  Nothing but the collection itself can
 *      consume the budget, and nothing can un-consume it.
 *   2. It needs no clock, so it survives power loss and ignores the phone.
 *   3. It IS the statistic.  §H.4's review gate needs M records per device;
 *      exhausting the budget and answering the question are the same act.
 *
 * Calendar duration then falls out in proportion to usage — a clinic device at
 * 12 sessions/day exhausts 512 records in ~43 days, a home device at 1/day in
 * ~17 months.  Both are inside NP-MOD-ID-001's 24-month intent, and the device
 * producing evidence fastest stops soonest, which is the correct direction.
 *
 * ── Fail-closed contract ────────────────────────────────────────────────────
 *
 * Extended fields are emitted ONLY when every one of these holds.  Any failure,
 * including an unreadable enrolment record, yields a STANDARD-ONLY record:
 *
 *   NP_ACCEL_CHAR_WINDOW_ENABLED == 1   (build-time, defaults to 0)
 *   enrolment record CRC valid          (corrupt ⇒ not enrolled)
 *   enrolment.enrolled == true          (affirmative opt-in, §H.3)
 *   enrolment.programme_id == NP_ACCEL_CHAR_PROGRAMME_ID
 *   enrolment.consent_epoch >= 1
 *   enrolment.records_emitted < NP_ACCEL_CHAR_RECORD_BUDGET
 *
 * The standard two booleans are produced identically in both cases.  §H.6's
 * non-coercion invariant is a property of this function: nothing a
 * non-participant receives is degraded by not participating.
 */

#ifndef NP_ACCEL_SHDR_H
#define NP_ACCEL_SHDR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Status ──────────────────────────────────────────────────────────────── */

typedef enum {
    NP_ACCEL_OK = 0,
    NP_ACCEL_ERR_ARG,
    NP_ACCEL_ERR_IMU,
    NP_ACCEL_ERR_WRITE
} np_accel_status_t;

/* ── §G.2 thresholds — UNVALIDATED PLACEHOLDERS ──────────────────────────────
 *
 * ⚠ NEITHER NUMBER IS AN ENGINEERING VALUE.  Both were chosen before any
 * hardware existed and neither has ever been compared against a device that
 * failed.  They are the reason the §H programme exists; do not cite them as
 * qualified limits, and do not tune them by judgement.  §H.4's review gate is
 * the only sanctioned path to replacing them.
 *
 * Recorded as placeholders in NP-FW-EMMC-002 §G.2 and in
 * docs/status/pending-decisions.md.  OI-EMMC2-09.
 */

/* PLACEHOLDER — unvalidated.  Peak resultant g above which a gap is scored as
 * containing a drop.  "free-fall-equivalent impact" was the stated derivation;
 * no drop test, no failure correlation. */
#define NP_ACCEL_DROP_THRESHOLD_G          15.0f

/* PLACEHOLDER — unvalidated.  Number of drop-bearing gaps within the rolling
 * window below that raises maintenance_alert. */
#define NP_ACCEL_MAINT_THRESHOLD           3u

/* PLACEHOLDER — unvalidated, AND a correction of record.
 *
 * §G.2 Rev 1 specified this window as "3 drops in any rolling 7-DAY window".
 * That is not implementable on this device for the same reason the calendar
 * expiry is not: there is no clock that survives a disconnect.  The window is
 * therefore counted in SESSION GAPS, not days.  Seven gaps approximates seven
 * days only for a once-daily user; for a clinic at 12/day it is about half a
 * day.  The substitution is recorded rather than hidden because it changes what
 * maintenance_alert means, and §H.4 must resolve the unit along with the
 * values.  OI-EMMC2-10. */
#define NP_ACCEL_MAINT_WINDOW_GAPS         7u

/* Floor above which a sample run is scored as an impact event at all.  Below
 * this, ordinary handling dominates.  Also the lowest histogram bin edge. */
#define NP_ACCEL_EVENT_FLOOR_G             2.0f

/* ── §H characterisation-window constants ────────────────────────────────────
 *
 * ci/test_shdr_schema.py PARSES the three constants below out of this header
 * and requires the SHDR schema to agree with them.  A rename here without the
 * corresponding schema change fails the CHAR-01 gate rather than drifting.
 */

/* Build-time programme gate.  0 = the characterisation code path cannot run at
 * all.  This is CHAR-2's "fail-closed in firmware, not a server-side flag": a
 * collection flag that must be actively switched off is a flag that stays on.
 *
 * ── THIS VALUE AND THE SHDR SCHEMA MOVE TOGETHER, ALWAYS ────────────────────
 *
 * ci/test_shdr_schema.py CHAR-01 enforces the biconditional in both directions:
 *
 *   this == 1 and the schema HAS shdr_accel_characterisation   → PASS
 *   this == 0 and the schema does NOT have it                  → PASS
 *   this == 0 and the schema STILL has it                      → FAIL  (b)
 *   the table exists but a marker is weak or absent            → FAIL  (a)
 *
 * So the programme cannot be half-retired.  Closing the window means setting
 * this to 0 AND dropping the table in the same change, or CI blocks — which is
 * how "the data must not outlive the programme that consented to it" becomes a
 * mechanical property instead of a promise.  It was 0 before 2026-08-12, when
 * the principal adopted the programme; it returns to 0 at window close and §H
 * is retired whole.
 *
 * A 1 here does NOT by itself collect anything.  Collection additionally
 * requires a consented enrolment record (§H.3), which only the app writes and
 * only after an affirmative opt-in.  The two gates are independent by design:
 * this one bounds the PROGRAMME, that one bounds the DEVICE.  OI-EMMC2-12 is
 * BLOCKING on the enrolment copy before any device may be offered enrolment.
 *
 * DELIBERATELY NOT #ifndef-GUARDED.  A -D override would make the value the CI
 * gate parses out of this header a statement about nothing, since any build
 * could contradict it.  The host tests reach the same code through
 * np_accel_char_build_gate (see np_accel_shdr.c), which exists ONLY under
 * NPTEST_HOST and is a const fold of this macro in every shipping build. */
#define NP_ACCEL_CHAR_WINDOW_ENABLED       1

/* Programme generation.  A second, differently-scoped programme takes a new id,
 * so records from a closed programme can never be silently re-attributed to an
 * open one.  Enrolment consented to programme N does not authorise N+1. */
#define NP_ACCEL_CHAR_PROGRAMME_ID         1u

/* Per-device ceiling on extended records — the window itself.  See the header
 * note above for why this, and not a date. */
#define NP_ACCEL_CHAR_RECORD_BUDGET        512u

/* ── Coarsening (CHAR-A3) ────────────────────────────────────────────────────
 *
 * Per-event peak g NEVER leaves the device.  What leaves is a count per bin,
 * aggregated over one session gap.  This follows NP-MOD-ID-001 §6.3's principle
 * — coarsen before upload, keep the exact value locally — and it is chosen
 * because the review gate needs the DISTRIBUTION of impact severity, not any
 * individual impact.  A histogram answers "where should the threshold sit?"
 * while destroying the per-event trajectory that made the raw series
 * health-inferrable.
 *
 * Bin edges are ~1.45× ratio, log-spaced.  The 15.0 g placeholder falls inside
 * bin 5, [14.0, 21.0), with bin 4 immediately below it and bin 6 immediately
 * above: a programme that could not resolve either side of the number it exists
 * to test would be pointless.
 */
#define NP_ACCEL_CHAR_G_BIN_COUNT          8u

/* Lower edge of each bin, in g.  The last bin is open-ended. */
extern const float np_accel_char_g_bin_edges[NP_ACCEL_CHAR_G_BIN_COUNT];

/* ── Records ─────────────────────────────────────────────────────────────── */

/* §G.2 — the standing SHDR record.  Written once per between-session gap, on
 * every device, enrolled or not.  Two booleans and nothing else. */
typedef struct {
    bool    drop_detected;
    bool    maintenance_alert;
    uint8_t reserved[6];        /* pad to 8 bytes — future use */
} np_shdr_accel_record_t;

/* §H.2 — the extended record.  Emitted ONLY under the fail-closed contract in
 * this file's header comment.  Carries its own consent and programme markers so
 * a row cannot exist in SHDR without them (they are NOT NULL CHECK-constrained
 * in the fleet schema, which is what makes an unconsented row unstorable rather
 * than merely unsent). */
typedef struct {
    uint16_t programme_id;                          /* == NP_ACCEL_CHAR_PROGRAMME_ID */
    uint32_t consent_epoch;                         /* ≥1; bumped on each grant */
    uint32_t record_seq;                            /* 1..NP_ACCEL_CHAR_RECORD_BUDGET */
    uint16_t g_bin[NP_ACCEL_CHAR_G_BIN_COUNT];      /* counts, coarsened */
    uint16_t impact_event_count;                    /* == Σ g_bin; explicit so bin saturation is detectable */
} np_shdr_accel_char_record_t;

/* Device-local enrolment state.  Lives in the Config partition under the SHDR
 * key, alongside the warranty token (§A.3).  Cleared by factory reset step R-7,
 * so enrolment does not survive a change of custody — a consent decision by a
 * previous registrant must not keep authorising collection for a new one
 * (NP-MOD-ID-001 §A.4). */
typedef struct {
    bool     enrolled;
    uint16_t programme_id;
    uint32_t consent_epoch;
    uint32_t records_emitted;   /* monotone; consumed only by emission */
    uint32_t crc32;             /* over the preceding fields — corrupt ⇒ not enrolled */
} np_accel_char_enrolment_t;

/* Rolling maintenance state.  Persisted with the standard record path; it is
 * NOT part of enrolment and is maintained on every device. */
typedef struct {
    uint8_t  drop_flags;        /* bitmap, one bit per recent gap */
    uint8_t  gaps_recorded;     /* saturating, ≤ NP_ACCEL_MAINT_WINDOW_GAPS */
} np_accel_shdr_state_t;

/* ── API ─────────────────────────────────────────────────────────────────── */

/* Build gate, as the code actually reads it.
 *
 * On a shipping build this is `static const bool` folded from
 * NP_ACCEL_CHAR_WINDOW_ENABLED, so the extended path is dead code the optimiser
 * removes.  Under NPTEST_HOST it is a writable global so one test source can be
 * run against both configurations — see np_accel_shdr.c. */
#ifdef NPTEST_HOST
extern bool np_accel_char_build_gate;
#endif

/* CRC over the enrolment record, excluding the crc32 field itself. */
uint32_t np_accel_char_enrolment_crc(const np_accel_char_enrolment_t *enr);

/* True only when the record is internally consistent AND every §H.3 condition
 * holds.  A NULL, corrupt, wrong-programme, zero-epoch or budget-exhausted
 * record all return false — this is the fail-closed decision point. */
bool np_accel_char_window_open(const np_accel_char_enrolment_t *enr);

/* Pure evaluator.  Scores one gap's worth of resultant-g samples.
 *
 *   samples/n     — resultant g magnitudes for the gap (SRAM only)
 *   state         — rolling window; updated in place
 *   enr           — enrolment record, or NULL for a non-enrolled device
 *   out_std       — always written
 *   out_char      — written only when *out_char_valid is set true
 *
 * Separated from the I/O path so both modes are host-testable without an IMU
 * or an eMMC. */
np_accel_status_t np_accel_shdr_evaluate(const float                  *samples,
                                         size_t                        n,
                                         np_accel_shdr_state_t        *state,
                                         np_accel_char_enrolment_t    *enr,
                                         np_shdr_accel_record_t       *out_std,
                                         np_shdr_accel_char_record_t  *out_char,
                                         bool                         *out_char_valid);

/* Reads the IMU for the between-session gap, calls the evaluator, writes the
 * SHDR record(s), and zeroes the raw buffer with memset_explicit on EVERY exit
 * path — including the error paths.  Called by np_hub_session_log after session
 * close and after a boot with no preceding session (§G.6). */
np_accel_status_t np_accel_shdr_process_session_gap(void);

#ifdef __cplusplus
}
#endif

#endif /* NP_ACCEL_SHDR_H */
