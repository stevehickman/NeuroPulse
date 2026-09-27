/*
 * NeurOne Hub Control Program — HRV Session Record to UHDR (OI-HRV-03)
 * Document: NP-FW-HRV-001 Rev 3 §8.1, NP-FW-HUB-001 Rev 1 §6
 *
 * The HRV biofeedback library (firmware/hrv_biofeedback/) builds one
 * np_hrv_session_record_t at session end and hands it to its end_cb.  Until
 * OI-HRV-03 the hub's end_cb discarded it.  np_mod_vns.c's end_cb now commits
 * it here, into the session logger's UHDR stream: this session's file on the
 * UHDR partition, which np_uhdr_key_unlock() mounted AES-256-XTS under the
 * user's biometric-derived key.  The logger writes plaintext to the mounted
 * partition and never sees key material (np_log_backend.h, "Encryption
 * boundary"), which is the storage layer's AES-256-XTS write.
 *
 * Kept out of np_session_log.h so that header does not depend on the HRV
 * library's types; only np_session_log.c and its caller include this one.
 */

#ifndef NP_LOG_HRV_H
#define NP_LOG_HRV_H

#include "np_hrv_types.h"
#include "np_session_log.h"   /* NP_LOG_TAG_UHDR_HRV_SESSION */

/*
 * np_log_hrv_session — write one HRV session record to UHDR.
 *
 * UHDR ONLY: coherence, RMSSD, the R-R count and the taVNS count describe the
 * person (CLAUDE.md §5.1).  Nothing reaches SHDR; the SHDR coherence trend
 * slope is OI-HRV-04, not this record.
 *
 * Layout after NP_LOG_TAG_UHDR_HRV_SESSION, each field little-endian as the
 * target stores it: reason (1, np_hrv_status_t as int8), duration_s (4),
 * protocol (1), target_rate_bpm (4), mean_coherence (4), min_coherence (4),
 * max_coherence (4), mean_rmssd_ms (4), rr_sample_count (2),
 * tavns_stim_count (2) — 31 bytes with the tag.  Serialized field by field,
 * so struct padding and `reserved` never reach the medium.
 *
 * session_start_unix is NOT written: the library never sets it (the device
 * has no RTC backup, CLAUDE.md §4.5), and the session file's SESSION_START
 * record already carries the session's start time.  A zero written here
 * would read as a start time of 1970.
 *
 * The record goes into the session file open NOW, so the caller must call
 * this before np_log_session_end().  The runner stops every module before it
 * ends the log (np_session_runner.c), and stopping the VNS module is what
 * ends the HRV session.  Outside a session UHDR accepts no append
 * (NP_HUB_ERR_NO_SESSION) and the record is lost, never written into another
 * session's file (np_log_session_start() drains before it opens the next).
 *
 * Takes the logger lock: task context only, never from an ISR or the safety
 * heartbeat.  NULL writes nothing.
 */
void np_log_hrv_session(const np_hrv_session_record_t *rec,
                        np_hrv_status_t                reason);

#endif /* NP_LOG_HRV_H */
