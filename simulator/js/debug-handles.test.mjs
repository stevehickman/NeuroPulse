/**
 * Tests for the page's debug handles (NP-SIM-FW-001 REQ-SIMFW-27 (a)).  Run: node --test js/debug-handles.test.mjs
 */
import test from 'node:test';
import assert from 'node:assert';
import { debugHandlesEnabled, setDebugHandle } from './debug-handles.js';

// @verifies REQ-SIMFW-27#a
test('the flag is on only for debug=1 in the URL the page was loaded with', () => {
  for (const s of ['?debug=1', '?x=2&debug=1', 'debug=1']) assert.strictEqual(debugHandlesEnabled(s), true, s);
  for (const s of ['', '?', '?debug', '?debug=0', '?debug=true', '?debug=yes', '?Debug=1', '?debug=11', undefined, null]) {
    assert.strictEqual(debugHandlesEnabled(s), false, String(s));
  }
});

// @verifies REQ-SIMFW-27#a
test('without the flag no handle is on window, and a stale one is removed', () => {
  const win = {};
  assert.strictEqual(setDebugHandle(win, false, '__sim', { sendFirmware() {} }), false);
  assert.ok(!('__sim' in win));
  win.__accessoriesInUse = { windows: [] };
  setDebugHandle(win, false, '__accessoriesInUse', { windows: ['vnsClip'] });
  assert.ok(!('__accessoriesInUse' in win));
});

// @verifies REQ-SIMFW-27#a
test('with the flag the handle is the value given', () => {
  const win = {};
  const sim = { sendFirmware() {} };
  assert.strictEqual(setDebugHandle(win, true, '__sim', sim), true);
  assert.strictEqual(win.__sim, sim);
});
