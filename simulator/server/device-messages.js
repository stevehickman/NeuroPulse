'use strict';
/**
 * What the server relays to its controllers of a display's TELEMETRY and SESSION_COMPLETE (NP-SIM-FW-001 REQ-SIMFW-30 (c),
 * OI-PRIV-42). Only the fields simulator/js/device-messages.js builds are kept, each only when its value has the type
 * that page module gives it, so a display that sends a UHDR value (a snapshot, a protocol, a duration) relays none.
 * Every other message type is returned unchanged.
 */

const PHASES = new Set(['running', 'paused', 'stopped']);
const SESSION_ID = /^sess_[0-9a-f]{16}$/;

const sessionId = (v) => (typeof v === 'string' && SESSION_ID.test(v) ? v : null);

function narrowDisplayMessage(msg) {
  switch (msg?.type) {
    case 'TELEMETRY':
      return {
        type: 'TELEMETRY',
        session_id: sessionId(msg.session_id),
        phase: PHASES.has(msg.phase) ? msg.phase : 'stopped',
        hub_temp_c: typeof msg.hub_temp_c === 'number' && Number.isFinite(msg.hub_temp_c) ? msg.hub_temp_c : null,
      };
    case 'SESSION_COMPLETE':
      return { type: 'SESSION_COMPLETE', session_id: sessionId(msg.session_id), uhdr_written: msg.uhdr_written === true };
    default:
      return msg;
  }
}

module.exports = { narrowDisplayMessage, PHASES, SESSION_ID };
