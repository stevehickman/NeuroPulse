/**
 * The two messages the simulated hub sends its controllers about a session, built from nothing but what may cross the
 * device API (NP-SIM-FW-001 REQ-SIMFW-31, OI-PRIV-42).
 *
 * No UHDR value crosses the device API (CLAUDE.md §5.1; data-architecture-detail.md §5.1): not the protocol or its
 * parameters, the modalities, the dose, the EEG, HRV or neurofeedback values, the impedance check, the adaptation
 * events, and not the session's time stamps or duration. What is left:
 *
 *   TELEMETRY         { session_id, phase, hub_temp_c }   phase is 'running', 'paused' or 'stopped';
 *                                                       hub_temp_c is null because the simulator models no hub NTC
 *   SESSION_COMPLETE  { session_id, uhdr_written }       uhdr_written is false: the page writes no UHDR record
 *
 * NP-API-001 §4.3 and §4.4 name more fields than these (duration_s, ts_ms, elapsed_s, impedance_ok, bes_charge_uc_cm2,
 * adaptation_events_since_last, and the gated eeg_bands, hrv_rmssd_ms, coherence_x100, pbm_dose_j_cm2). §5.1 classes
 * each of them UHDR, and the simulator has no NP-API-001 §2.3 gate, so they are left out until the specification is
 * reconciled (OI-PRIV-81, docs/human-input/oi-priv-81-device-api-uhdr-fields.md).
 *
 * The server keeps only these fields of whatever a display sends (simulator/server/device-messages.js), so a page that
 * sends more still delivers no more.
 */

/**
 * A random identifier for one session. It names nothing but that session: no protocol, person, device or time.
 * 8 bytes because it only has to differ between sessions a controller sees: by the birthday bound, two of 2^32
 * sessions share one with probability about one half, and a simulator run sees a few dozen.
 */
export function newSessionId(random = (n) => crypto.getRandomValues(new Uint8Array(n))) {
  return 'sess_' + Array.from(random(8), (b) => b.toString(16).padStart(2, '0')).join('');
}

/** The session's phase as the device API states it, from SessionEngine.snapshot(). */
export function phaseOf(snap) {
  if (!snap?.running) return 'stopped';
  return snap.paused ? 'paused' : 'running';
}

export function telemetryMessage(sessionId, snap) {
  return { type: 'TELEMETRY', session_id: sessionId ?? null, phase: phaseOf(snap), hub_temp_c: null };
}

export function sessionCompleteMessage(sessionId) {
  return { type: 'SESSION_COMPLETE', session_id: sessionId ?? null, uhdr_written: false };
}
