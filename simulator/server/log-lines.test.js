/**
 * Tests for the server's client log lines (NP-SIM-FW-001 REQ-SIMFW-27 (b)).  Run: node --test server/log-lines.test.js
 */
'use strict';
const test = require('node:test');
const assert = require('node:assert');
const fs = require('fs');
const path = require('path');
const { logDetailEnabled, connectedLine, helloLine, sessionStartLine } = require('./log-lines');

// @verifies REQ-SIMFW-27#b
test('the server source never reads or prints a client address', () => {
  const src = fs.readFileSync(path.join(__dirname, 'index.js'), 'utf8');
  for (const name of ['remoteAddress', 'remotePort', 'x-forwarded-for', 'x-real-ip']) {
    assert.ok(!src.includes(name), `index.js names ${name}`);
  }
  assert.strictEqual(connectedLine(3), '[NP-SIM] Client connected  (3 total)');
});

// @verifies REQ-SIMFW-27#b
test('version and protocol ID are printed only with NP_SIM_DEBUG=1', () => {
  for (const env of [{}, { NP_SIM_DEBUG: '0' }, { NP_SIM_DEBUG: 'true' }, undefined]) assert.strictEqual(logDetailEnabled(env), false);
  assert.strictEqual(logDetailEnabled({ NP_SIM_DEBUG: '1' }), true);

  assert.strictEqual(helloLine('display', '2.1.0', false), '[NP-SIM] CLIENT_HELLO  role=display');
  assert.strictEqual(sessionStartLine('focus-alpha', false), '[NP-SIM] SESSION_START');
  assert.strictEqual(helloLine('display', '2.1.0', true), '[NP-SIM] CLIENT_HELLO  role=display  version=2.1.0');
  assert.strictEqual(sessionStartLine('focus-alpha', true), '[NP-SIM] SESSION_START  protocol=focus-alpha');
  assert.strictEqual(sessionStartLine(undefined, true), '[NP-SIM] SESSION_START  protocol=raw');
});
