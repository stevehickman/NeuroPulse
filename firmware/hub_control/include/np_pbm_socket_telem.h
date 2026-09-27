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
 *     tile kind and UID change (F14), I²C probe (F11b), calibration source
 *     (F12), PD1-only self-test (F10).  Run from task_module_detect under the
 *     session lease, never from session data, so what it records has nothing
 *     to do with which sockets were lit.  The self-test does not emit until
 *     OI-FWHUB-20 says it may (np_sw02_platform_hal.h).
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
 * the lease, for the health functions.  No lock of its own: the lease keeps
 * the two apart (REQ-FWHUB-03).  IEC 62304 Class B — SW-02 hub control.
 */

#ifndef NP_PBM_SOCKET_TELEM_H
#define NP_PBM_SOCKET_TELEM_H

#include "np_hub_types.h"

/* Sockets metered at once.  The PBM library's own session bound
 * (np_pbm_config.h); a drive past it is refused before the tile is lit, so no
 * socket is ever driven unmetered. */
#define NP_PST_MAX_TRACKED   32U   /* == NP_PBM_SESSION_MAX_ACTIVE_SOCKETS, asserted */

/* F10's normaliser: the PD1 self-test reading, times the module's K_PD1, as a
 * percentage of this.  UNVALIDATED PLACEHOLDER — no derivation exists, because
 * the self-test's setpoint and pulse are not defined (OI-FWHUB-20).  Nothing
 * is compared against it on the device; it only scales the recorded value. */
#define NP_PST_SELFTEST_NOMINAL_MW_CM2  100.0f

/* SHDR fault code for a socket drive the driver could not complete (F11).
 * Written with NP_HUB_SLOT_NONE: which socket is UHDR. */
#define NP_PBM_SHDR_EV_DRIVE_FAULT   0x02U

/* UHDR 0x1A flags byte. */
#define NP_PST_FLAG_PD_VALID_SHIFT    0U   /* bits 0–2: PDs read, per wavelength */
#define NP_PST_FLAG_LIMIT_SHIFT       3U   /* bits 3–5: dose limit reached       */
#define NP_PST_FLAG_THROTTLED         0x40U
#define NP_PST_FLAG_NTC_VALID         0x80U

/* SHDR 0x87 values. */
#define NP_PST_PROBE_FAIL             0U
#define NP_PST_PROBE_PASS             1U
#define NP_PST_PROBE_NONE             0xFFU  /* base tile: no on-module MCU      */
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

/* One SHDR tile-health record (F10, F11b, F12, F14). */
typedef struct {
    uint16_t socket_id;
    uint8_t  mod_type;                 /* F14: tile kind                    */
    uint8_t  health_flags;             /* NP_PST_HEALTH_*                   */
    uint8_t  probe;                    /* NP_PST_PROBE_*        (F11b)      */
    uint8_t  cal_source;               /* np_cal_source_t       (F12)       */
    uint8_t  pd1_pct[3];               /* F10; NP_PST_PD1_NOT_MEASURED      */
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

/* Check one socket and, if it is occupied, write its SHDR health record.
 * Returns true when the pass is complete (the last socket was checked).  One
 * socket per call so the lease is held for one socket at a time. */
bool np_pst_health_step(void);

#endif /* NP_PBM_SOCKET_TELEM_H */
