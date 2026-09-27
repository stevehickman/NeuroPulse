/*
 * NeurOne Hub Control — Socket-path PBM telemetry, dose metering and the idle
 * tile health pass (OI-FWHUB-10)
 * Document: NP-FW-HUB-001 Rev 15 §6.8
 *
 * The slot path's telemetry (np_mod_pbm_telemetry(), np_telem_pbm_t) is five
 * zone entries wide and unreachable. This is the socket lattice's replacement.
 * What goes where follows the principal's field rulings of 2026-09-27 (§6.8,
 * F1–F14). They turn on one observation: at 80-socket resolution, any
 * per-socket value that exists only for lit sockets reconstructs the treatment
 * montage (CLAUDE.md §5.1 rule 2).
 *
 *   SESSION, per tracked socket → UHDR only (np_log_pbm_socket()):
 *     socket id (F1), cumulative dose (F2) and measured irradiance (F3) per
 *     wavelength, dose limit reached (F4), NTC now and peak (F7), thermal
 *     throttle (F8).  Raw PD counts (F5) and TIA gain (F6) are never stored.
 *   SESSION, no location → SHDR: per-session event counts (F13), written at
 *     every session end whatever ran; each driving fault as modality + code
 *     with no socket (F11, np_mod_pbm.c).
 *   IDLE, every OCCUPIED socket → SHDR (np_log_shdr_pbm_socket_health()):
 *     one latest-state row per tile — kind and UID change (F14), and the
 *     latest verdict of each maintenance test: I²C probe (F11b), calibration
 *     source (F12), NTC plausible, PD1 LED emission (F10).  Run from
 *     task_module_detect under the session lease, never from session data.
 *     No session count and no run identifier: SHDR holds the latest state per
 *     tile, not a log of who tested what, or when (§6.9, principal 2026-09-27).
 *
 * MAINTENANCE SELF-TESTS (§6.9).  The warranty owner selects tiles and tests
 * from the app's maintenance section (np_pst_maint_run()).  NON-EMITTING tests
 * — probe, calibration source, NTC plausibility — run here, on or off head.
 * The EMITTING test (LED emission, read on PD1) must run as a signed
 * maintenance session through the normal session path, and only with the
 * head-presence gate passing or the helmet in a detected dock (principal
 * 2026-09-27).  Neither the maintenance session kind, the gate's thresholds
 * (OI-BENCH-01) nor dock detection exists, so this build REFUSES the emitting
 * test (OI-FWHUB-20).  Nothing that could carry the wearer's biology (PD2,
 * electrode impedance, an NTC temperature) is a test output: the NTC test
 * reports a verdict and the reading is discarded.
 *
 * Dose metering: np_pbm_dose_tick() per driven socket every
 * NP_PBM_DOSE_TICK_MS, with the module's UID-keyed calibration (OI-HUB-C06),
 * loaded when the socket is first driven in the session.  A wavelength whose
 * limit is reached is latched off with np_mod_pbm_socket_disable_channel().
 * An NTC at or over NP_PBM_THERMAL_FAULT_C, or an unreadable one, latches
 * every channel off.  The 42 °C / 62 °C hardware limits below this processor
 * are unaffected.
 *
 * Runner context only for the session functions; task_module_detect, under
 * the lease, for the health and maintenance functions.  No lock of its own: the lease keeps
 * the two apart (REQ-FWHUB-03).  IEC 62304 Class B — SW-02 hub control.
 */

#ifndef NP_PBM_SOCKET_TELEM_H
#define NP_PBM_SOCKET_TELEM_H

#include "np_hub_types.h"

/* Sockets metered at once.  The PBM library's own session bound
 * (np_pbm_config.h); a drive past it is refused before the tile is lit, so no
 * socket is ever driven unmetered. */
#define NP_PST_MAX_TRACKED   32U   /* == NP_PBM_SESSION_MAX_ACTIVE_SOCKETS, asserted */

/* SHDR fault code for a socket drive the driver could not complete (F11).
 * Written with NP_HUB_SLOT_NONE: which socket is UHDR. */
#define NP_PBM_SHDR_EV_DRIVE_FAULT   0x02U

/* UHDR 0x1A flags byte. */
#define NP_PST_FLAG_PD_VALID_SHIFT    0U   /* bits 0–2: PDs read, per wavelength */
#define NP_PST_FLAG_LIMIT_SHIFT       3U   /* bits 3–5: dose limit reached       */
#define NP_PST_FLAG_THROTTLED         0x40U
#define NP_PST_FLAG_NTC_VALID         0x80U

/* Maintenance tests (§6.9).  Bit n of a test mask selects test n. */
typedef enum {
    NP_MAINT_T_PROBE = 0,    /* I²C probe of a smart tile's MCU      (F11b) */
    NP_MAINT_T_CAL   = 1,    /* the module's calibration source       (F12)  */
    NP_MAINT_T_NTC   = 2,    /* NTC reads, finite, below fault; value discarded */
    NP_MAINT_T_LED   = 3,    /* EMITTING: PD1 per wavelength          (F10)  */
    NP_MAINT_T_COUNT = 4
} np_maint_test_t;
#define NP_MAINT_TEST_ALL        0x0FU
#define NP_MAINT_TEST_EMITTING   (1U << NP_MAINT_T_LED)

/* A test's latest verdict, as the app shows it and 0x87 records it. */
typedef enum {
    NP_MAINT_V_NOT_RUN = 0,  /* never run on this tile since boot          */
    NP_MAINT_V_PASS    = 1,
    NP_MAINT_V_FAIL    = 2,
    NP_MAINT_V_NA      = 3,  /* does not apply (a base tile has no MCU)   */
    NP_MAINT_V_REFUSED = 4   /* not permitted now (emitting, OI-FWHUB-20) */
} np_maint_verdict_t;

#define NP_PST_PD1_NOT_MEASURED       0xFFU
#define NP_PST_HEALTH_UID_CHANGED     0x01U
#define NP_PST_HEALTH_FIRST_PASS      0x02U

/* One UHDR socket record (F1–F4, F7, F8). */
typedef struct {
    uint16_t socket_id;
    uint8_t  mod_type;                 /* NP_MOD_PBM_BASE / _SMART          */
    uint8_t  flags;                    /* NP_PST_FLAG_*                     */
    float    ntc_c;
    float    ntc_peak_c;
    float    dose_J_cm2[3];            /* 660, 808, 1064 nm                 */
    float    irradiance_mW_cm2[3];
} np_pst_socket_record_t;

/* One tile's latest state: the app's maintenance view and SHDR 0x87. */
typedef struct {
    uint16_t socket_id;
    uint8_t  mod_type;                    /* F14: tile kind                 */
    uint8_t  health_flags;                /* NP_PST_HEALTH_*                */
    uint8_t  verdict[NP_MAINT_T_COUNT];   /* np_maint_verdict_t, per test   */
    uint8_t  cal_source;                  /* np_cal_source_t       (F12)    */
    /* F10: PD1 × K_PD1 against the module's expected emission at the test
     * setpoint, percent.  Written only by the maintenance session's emitting
     * test (OI-FWHUB-20), whose setpoint, pulse and normaliser are not yet
     * defined, so today always NP_PST_PD1_NOT_MEASURED. */
    uint8_t  pd1_pct[3];
} np_pst_health_record_t;

/* Per-session counts, no location (F13). */
typedef struct {
    uint16_t throttle_events;          /* NTC over NP_PBM_THERMAL_FAULT_C mid-run */
    uint16_t predrive_refusals;        /* drive refused: NTC hot or unreadable    */
    uint16_t drive_faults;             /* driver could not complete a drive       */
} np_pst_counts_t;

/* ── Session side (runner) ───────────────────────────────────────────────── */

/* Session load: forget every tracked socket and zero the counts. */
void np_pst_session_reset(void);

/* Called by np_mod_pbm_socket_drive() BEFORE the tile is lit.  Tracks the
 * socket for this session (idempotent across re-drives; dose is cumulative
 * for the session) and loads its module's calibration on first use.
 * NP_HUB_ERR_GENERIC when NP_PST_MAX_TRACKED sockets are already tracked:
 * the caller must not drive it. */
np_hub_status_t np_pst_track(uint16_t socket_id, np_hub_mod_type_t mod_type);

/* Called by np_mod_pbm_socket_stop(): the socket is dark, so the dose tick
 * skips it.  Its record and dose stay for the rest of the session. */
void np_pst_untrack_driving(uint16_t socket_id);

void np_pst_note_predrive_refusal(void);
void np_pst_note_drive_fault(void);

/* Every runner iteration: one np_pbm_dose_tick() per driven socket for each
 * NP_PBM_DOSE_TICK_MS elapsed (at most NP_PST_TICK_CATCHUP_MAX per call). */
#define NP_PST_TICK_CATCHUP_MAX  10U
void np_pst_dose_poll(uint32_t now_ms);

/* The runner's telemetry interval: one UHDR record per tracked socket. */
void np_pst_log_telemetry(uint32_t session_ms);

void np_pst_get_counts(np_pst_counts_t *out);
bool np_pst_get_record(uint16_t socket_id, np_pst_socket_record_t *out);

/* ── Idle side (task_module_detect, under the session lease) ─────────────── */

/* Ask for a pass over every socket.  Pending from boot. */
void np_pst_health_request(void);
bool np_pst_health_pending(void);

/* Run the NON-EMITTING tests on one socket and, if it is occupied by a PBM
 * tile, write its latest-state SHDR record.  Returns true when the pass is
 * complete.  One socket per call so the lease is held for one socket at a
 * time.  The pass never emits: a tile's LED verdict and PD1 values are the
 * latest from a maintenance run, or NOT_RUN. */
bool np_pst_health_step(void);

/* ── Maintenance (app request; task_module_detect context) ──────────────── */

typedef struct {
    uint8_t socket_mask[NP_HUB_SOCKET_MASK_BYTES];   /* bit n = socket n */
    uint8_t test_mask;                               /* NP_MAINT_TEST_*  */
} np_maint_request_t;

/* Called once per selected, occupied PBM socket with its updated state. */
typedef void (*np_maint_result_fn)(const np_pst_health_record_t *rec, void *ctx);

/*
 * np_pst_maint_run — the warranty owner's self-test request (§6.9).
 *
 * Runs each selected test on each selected socket that holds a PBM tile, one
 * socket per session-lease hold, and reports each socket's updated state
 * through `on_result`.  Empty and non-PBM sockets are skipped.  Emitting tests
 * are refused (NP_MAINT_V_REFUSED): they need a signed maintenance session
 * under the head-presence gate or a detected dock (OI-FWHUB-20).  The latest
 * state is kept in RAM and a health pass is requested, so SHDR receives the
 * whole lattice's latest state, never a record of the subset tested.
 *
 * Returns NP_HUB_OK; NP_HUB_ERR_INVALID_ARG for NULL or an empty selection;
 * NP_HUB_ERR_SESSION_ACTIVE if a session holds the lease (the run stops at the
 * first socket it could not claim; sockets already tested keep their result).
 *
 * WHO MAY CALL: the app offers this only to the warranty owner (principal
 * 2026-09-27).  The hub cannot tell who is holding the phone, and the gate is
 * not a safety control — no test here emits or drives current.
 */
np_hub_status_t np_pst_maint_run(const np_maint_request_t *req,
                                 np_maint_result_fn        on_result,
                                 void                     *ctx);

#endif /* NP_PBM_SOCKET_TELEM_H */
