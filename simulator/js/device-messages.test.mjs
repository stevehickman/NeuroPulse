/**
 * Tests for what the device API says about a session (NP-SIM-FW-001 REQ-SIMFW-31, OI-PRIV-42).
 * Run: node --test js/device-messages.test.mjs
 */
import test from 'node:test';
import assert from 'node:assert';
import { createRequire } from 'node:module';
import { newSessionId, phaseOf, telemetryMessage, sessionCompleteMessage } from './device-messages.js';

const require = createRequire(import.meta.url);
const { narrowDisplayMessage, SESSION_ID } = require('../server/device-messages.js');

// A snapshot as SessionEngine.snapshot() returns it, with every UHDR-class value the old messages carried.
const SNAP = {
  running: true, paused: false, elapsed: 47.3, duration: 1200, progress: 0.039, wallTime: 12.5,
  pbm: { active: true, zones: ['Frontal Left'], dose: { 'Frontal Left': '9.3' } },
  eeg: { active: true, bands: { alpha: 38.2 }, impedance: { Fp1: 'good' }, neurofeedbackScore: 7.1 },
  vns: { active: true, coherence: 6.8, rmssd: 44.1, hr: 63, breathing_phase: 0.4 },
  tdcs: { active: false, intensity_ma: '0.00' },
};

// @verifies REQ-SIMFW-31#a
test('SESSION_COMPLETE carries the session id and uhdr_written, and nothing else', () => {
  const id = newSessionId();
  const m = sessionCompleteMessage(id);
  assert.deepStrictEqual(m, { type: 'SESSION_COMPLETE', session_id: id, uhdr_written: false });
  assert.ok(!('uhdr_summary' in m) && !('shdr_summary' in m) && !('duration_s' in m));
});

// @verifies REQ-SIMFW-31#b
test('TELEMETRY carries the session id, the phase and hub_temp_c, never the snapshot', () => {
  const id = newSessionId();
  const m = telemetryMessage(id, SNAP);
  assert.deepStrictEqual(m, { type: 'TELEMETRY', session_id: id, phase: 'running', hub_temp_c: null });
  assert.strictEqual(phaseOf({ running: true, paused: true }), 'paused');
  assert.strictEqual(phaseOf({ running: false, paused: false }), 'stopped');
  assert.strictEqual(phaseOf(undefined), 'stopped');
});

// @verifies REQ-SIMFW-31#a
test('the session id is random and names nothing else', () => {
  const ids = new Set(Array.from({ length: 64 }, () => newSessionId()));
  assert.strictEqual(ids.size, 64);
  for (const id of ids) assert.match(id, SESSION_ID);
  assert.strictEqual(newSessionId(() => new Uint8Array([0, 1, 2, 3, 0xfc, 0xfd, 0xfe, 0xff])), 'sess_00010203fcfdfeff');
});

// @verifies REQ-SIMFW-31#c
test('the server relays only those fields, whatever a display sends', () => {
  const id = newSessionId();
  // The messages as the simulator used to send them.
  const oldTelemetry = { type: 'TELEMETRY', timestamp: 1716000000000, snap: SNAP, session_id: id, phase: 'paused' };
  assert.deepStrictEqual(narrowDisplayMessage(oldTelemetry), { type: 'TELEMETRY', session_id: id, phase: 'paused', hub_temp_c: null });
  const oldComplete = { type: 'SESSION_COMPLETE',
    uhdr_summary: { protocol: '01-gamma-focus', duration_s: 1200, hrv_rmssd_final_ms: 44.1 },
    shdr_summary: { protocol_id: '01-gamma-focus', modalities_active: ['pbm', 'eeg'], session_count_delta: 1 },
    duration_s: 1200 };
  assert.deepStrictEqual(narrowDisplayMessage(oldComplete), { type: 'SESSION_COMPLETE', session_id: null, uhdr_written: false });

  // A value of the wrong type, or a session id that could carry text, is not relayed.
  assert.deepStrictEqual(narrowDisplayMessage({ type: 'TELEMETRY', session_id: 'sess_01-gamma-focus', phase: 'ramping', hub_temp_c: '36.8' }),
    { type: 'TELEMETRY', session_id: null, phase: 'stopped', hub_temp_c: null });
  assert.deepStrictEqual(narrowDisplayMessage({ type: 'SESSION_COMPLETE', session_id: id, uhdr_written: 'yes' }),
    { type: 'SESSION_COMPLETE', session_id: id, uhdr_written: false });

  // What the page builds passes unchanged.
  for (const m of [telemetryMessage(id, SNAP), telemetryMessage(null, {}), sessionCompleteMessage(id)]) {
    assert.deepStrictEqual(narrowDisplayMessage(m), m);
  }
  // Other message types are not this rule's.
  const fault = { type: 'FAULT', code: 'X' };
  assert.strictEqual(narrowDisplayMessage(fault), fault);
});
