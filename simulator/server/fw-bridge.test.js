'use strict';
/**
 * Tests for the firmware bridge (NP-SIM-FW-001 Rev 2).  Run: node --test server/
 * The end-to-end test runs the real np_sim_pbm_spike when NP_SIM_FW_BIN names it,
 * and is skipped otherwise.
 */
const test = require('node:test');
const assert = require('node:assert');
const { FwBridge, BridgeError, messageToLines, eventToMessage } = require('./fw-bridge');

// @verifies REQ-SIMFW-12
test('a client string is never placed in a command line', () => {
  const hostile = [
    { type: 'MODULE_FAIL', socket: 1, op: 'socket_pwm_set\nquit', code: -17, times: 1 },
    { type: 'MODULE_FAIL', socket: '1\nquit', op: 'socket_pwm_set', code: -17, times: 1 },
    { type: 'MODULE_FAIL', socket: 1, op: 'socket_pwm_set', code: '-17 1\nquit', times: 1 },
    { type: 'MODULE_SET', socket: 1, patch: { reading: { op: 'socket_ntc_read\nquit', value: 70 } } },
    { type: 'MODULE_SET', socket: 1, patch: { add: '34\nquit' } },
    { type: 'FW_COMMAND', cmd: 'drive\nquit', sockets: [1] },
    { type: 'FW_COMMAND', cmd: 'drive', sockets: ['1\nquit'] },
    { type: 'FW_COMMAND', cmd: 'drive', sockets: [1.5] },
    { type: 'FW_COMMAND', cmd: 'drive', sockets: [] },
    { type: 'MODULE_CLEAR', socket: 128 },
    { type: 'MODULE_CLEAR', socket: -1 },
  ];
  for (const m of hostile) {
    assert.throws(() => messageToLines(m), BridgeError, JSON.stringify(m));
  }
});

// @verifies REQ-SIMFW-12
test('a valid message becomes exactly one well-formed line per command', () => {
  assert.deepStrictEqual(
    messageToLines({ type: 'MODULE_FAIL', socket: 1, op: 'socket_pwm_set', code: -17, times: 1 }),
    ['fail 1 socket_pwm_set -17 1']);
  assert.deepStrictEqual(
    messageToLines({ type: 'FW_COMMAND', cmd: 'drive', sockets: [0, 1, 2] }), ['drive 0,1,2']);
  assert.deepStrictEqual(
    messageToLines({ type: 'MODULE_SET', socket: 2, patch: { reading: { op: 'socket_ntc_read', value: 70 } } }),
    ['reading 2 socket_ntc_read 70']);
  for (const l of messageToLines({ type: 'MODULE_SET', socket: 3, patch: { add: 34, present: true } })) {
    assert.ok(!/[\r\n]/.test(l), 'no embedded newline');
  }
  assert.deepStrictEqual(messageToLines({ type: 'SESSION_START' }), [], 'not the bridge\'s message');
});

test('events are routed: identity and state to MODULE_STATE, the rest to FW_LOG', () => {
  assert.strictEqual(eventToMessage({ ev: 'module', id: 'pbm_base:1' }).type, 'MODULE_STATE');
  assert.strictEqual(eventToMessage({ ev: 'module_state' }).type, 'MODULE_STATE');
  assert.strictEqual(eventToMessage({ ev: 'inject' }).type, 'MODULE_STATE');
  assert.strictEqual(eventToMessage({ ev: 'hal' }).type, 'FW_LOG');
  assert.strictEqual(eventToMessage({ ev: 'fw_log' }).type, 'FW_LOG');
  assert.strictEqual(eventToMessage({ ev: 'unrouted' }).type, 'FW_LOG');
});

const BIN = process.env.NP_SIM_FW_BIN;

// @verifies REQ-SIMFW-11
test('end to end: a fault injected over the bridge reaches one tile and the rollback shows', { skip: !BIN }, async () => {
  const bridge = new FwBridge({ bin: BIN }).start();
  const seen = [];
  bridge.on('message', (m) => seen.push(m));
  const wait = (ms) => new Promise((r) => setTimeout(r, ms));
  for (const s of [0, 1, 2]) bridge.handle({ type: 'MODULE_SET', socket: s, patch: { add: 34 } });
  bridge.handle({ type: 'MODULE_FAIL', socket: 1, op: 'socket_pwm_set', code: -17, times: 1 });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'drive', sockets: [0, 1, 2] });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'status' });
  await wait(400);
  bridge.stop();

  const hal = seen.filter((m) => m.type === 'FW_LOG' && m.event.ev === 'hal');
  const injected = hal.filter((m) => m.event.source === 'injected' && m.event.code === -17);
  assert.strictEqual(injected.length, 1, 'exactly one injected answer');
  assert.strictEqual(injected[0].event.id, 'pbm_base:1', 'and it is on the tile that was armed');
  const result = seen.find((m) => m.type === 'FW_LOG' && m.event.ev === 'result' && m.event.cmd === 'drive');
  assert.strictEqual(result.event.code, -17, 'the firmware reports the driver fault');
  assert.strictEqual(result.event.active, 0, 'and no socket is left active');
  const states = seen.filter((m) => m.type === 'MODULE_STATE' && m.event.ev === 'module_state');
  assert.strictEqual(states.length, 3);
  assert.ok(states.every((m) => m.event.driven === false), 'every tile is dark after the rollback');
  assert.strictEqual(bridge.list().modules.length, 3, 'MODULE_LIST holds the three tiles');
});
