'use strict';
/**
 * Tests for the firmware bridge (NP-SIM-FW-001).  Run: node --test server/
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
    { type: 'FW_COMMAND', cmd: 'report', version: '1\nquit' },
    { type: 'FW_COMMAND', cmd: 'report', version: 256 },
    { type: 'FW_COMMAND', cmd: 'report', version: 1.5 },
    { type: 'FW_COMMAND', cmd: 'report' },
    { type: 'MODULE_CLEAR', socket: 128 },
    { type: 'MODULE_CLEAR', socket: -1 },
  ];
  for (const m of hostile) {
    assert.throws(() => messageToLines(m), BridgeError, JSON.stringify(m));
  }
});

// @verifies REQ-SIMFW-12
test('a client string is never placed in a stimulation command line', () => {
  const hostile = [
    { type: 'MODULE_FAIL', kind: 'stim_pair', pair: 0, op: 'stim_dac_set\nquit', code: -17, times: 1 },
    { type: 'MODULE_FAIL', kind: 'stim_pair', pair: 0, op: 'socket_pwm_set', code: -17, times: 1 },   // another kind's op
    { type: 'MODULE_FAIL', kind: 'stim_pair', pair: '0\nquit', op: 'stim_dac_set', code: -17, times: 1 },
    { type: 'MODULE_SET', kind: 'stim_pair', pair: 0, patch: { add: '5\nquit' } },
    { type: 'MODULE_SET', kind: 'stim_pair', pair: 0, patch: { reading: { op: 'stim_read_current\nquit', value: 5 } } },
    { type: 'MODULE_CLEAR', kind: 'stim_pair', pair: 128 },
    { type: 'FW_COMMAND', cmd: 'bes', pair: 0, waveform: 0, freq_mhz: 10000, amp_ua: '500\nquit' },
    { type: 'FW_COMMAND', cmd: 'bes', pair: 0, waveform: 0, freq_mhz: 70000, amp_ua: 500 },
    { type: 'FW_COMMAND', cmd: 'tdcs', pair: 0, current_ua: 1000, polarity: 0, ramp_s: 30 },          // area missing
    { type: 'FW_COMMAND', cmd: 'tdcs', pair: 0, current_ua: 1.5, polarity: 0, ramp_s: 30, area_mcm2: 1 },
    { type: 'FW_COMMAND', cmd: 'stim_stop', channel: 'bes\nquit' },
    { type: 'FW_COMMAND', cmd: 'stim_stop', channel: 'vns' },
    { type: 'FW_COMMAND', cmd: 'advance', ms: -1 },
    { type: 'FW_COMMAND', cmd: 'advance', ms: '10\nquit' },
  ];
  for (const m of hostile) {
    assert.throws(() => messageToLines(m), BridgeError, JSON.stringify(m));
  }
});

// @verifies REQ-SIMFW-12
test('a valid stimulation message becomes exactly one well-formed line', () => {
  assert.deepStrictEqual(messageToLines({ type: 'MODULE_SET', kind: 'stim_pair', pair: 1, patch: { add: 5 } }), ['stim_add 1 5']);
  assert.deepStrictEqual(messageToLines({ type: 'MODULE_FAIL', kind: 'stim_pair', pair: 1, op: 'stim_dc_ramp', code: -17, times: 2 }),
    ['fail 1 stim_dc_ramp -17 2']);
  assert.deepStrictEqual(messageToLines({ type: 'MODULE_SET', kind: 'stim_pair', pair: 1, patch: { reading: { op: 'stim_read_current', value: 900 } } }),
    ['reading 1 stim_read_current 900']);
  assert.deepStrictEqual(messageToLines({ type: 'MODULE_CLEAR', kind: 'stim_pair', pair: 1 }), ['stim_clear 1']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'bes', pair: 0, waveform: 0, freq_mhz: 10000, amp_ua: 500 }),
    ['bes 0 0 10000 500']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'tdcs', pair: 0, current_ua: 1000, polarity: 0, ramp_s: 30, area_mcm2: 25000 }),
    ['tdcs 0 1000 0 30 25000']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'stim_stop', channel: 'tdcs' }), ['stim_stop tdcs']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'advance', ms: 15000 }), ['advance 15000']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'sample', channel: 'bes' }), ['sample bes']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'tdcs_pause' }), ['tdcs_pause']);
  // the tile messages are unchanged: no kind means a tile
  assert.deepStrictEqual(messageToLines({ type: 'MODULE_CLEAR', socket: 3 }), ['clear 3']);
});

// @verifies REQ-SIMFW-12
test('a client string is never placed in an EEG command line', () => {
  const hostile = [
    { type: 'MODULE_FAIL', kind: 'eeg', op: 'eeg_start\nquit', code: -17, times: 1 },
    { type: 'MODULE_FAIL', kind: 'eeg', op: 'stim_dac_set', code: -17, times: 1 },
    { type: 'MODULE_SET', kind: 'eeg', patch: { add: { id: '62\nquit' } } },
    { type: 'MODULE_SET', kind: 'eeg', patch: { impedance: { channel: 8, kohm: 5 } } },
    { type: 'MODULE_SET', kind: 'eeg', patch: { impedance: { channel: 1, kohm: '5\nquit' } } },
    { type: 'MODULE_SET', kind: 'eeg', patch: { band: { band: 5, db: 1 } } },
    { type: 'MODULE_SET', kind: 'eeg', patch: { reading: { op: 'eeg_read_impedance\nquit', value: 5 } } },
    { type: 'FW_COMMAND', cmd: 'eeg', channel_mask: 256, gain: 6, notch: 0, ref_mode: 0, adaptive_out: 0 },
    { type: 'FW_COMMAND', cmd: 'eeg', channel_mask: '255\nquit', gain: 6, notch: 0, ref_mode: 0, adaptive_out: 0 },
    { type: 'FW_COMMAND', cmd: 'eeg', channel_mask: 255, gain: 6 },
  ];
  for (const m of hostile) {
    assert.throws(() => messageToLines(m), BridgeError, JSON.stringify(m));
  }
});

// @verifies REQ-SIMFW-12
test('a valid EEG message becomes exactly one well-formed line', () => {
  assert.deepStrictEqual(messageToLines({ type: 'MODULE_SET', kind: 'eeg', patch: { add: {} } }), ['eeg_add']);
  assert.deepStrictEqual(messageToLines({ type: 'MODULE_SET', kind: 'eeg', patch: { add: { id: 62 } } }), ['eeg_add 62']);
  assert.deepStrictEqual(messageToLines({ type: 'MODULE_SET', kind: 'eeg', patch: { impedance: { channel: 2, kohm: 12.5 } } }), ['eeg_impedance 2 12.5']);
  assert.deepStrictEqual(messageToLines({ type: 'MODULE_SET', kind: 'eeg', patch: { band: { band: 2, db: 4 } } }), ['eeg_band 2 4']);
  assert.deepStrictEqual(messageToLines({ type: 'MODULE_FAIL', kind: 'eeg', op: 'eeg_stop', code: -17, times: 1 }), ['fail 0 eeg_stop -17 1']);
  assert.deepStrictEqual(messageToLines({ type: 'MODULE_CLEAR', kind: 'eeg' }), ['eeg_clear']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'eeg', channel_mask: 15, gain: 6, notch: 1, ref_mode: 0, adaptive_out: 0 }),
    ['eeg 15 6 1 0 0']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'eeg_sample' }), ['eeg_sample']);
});

// @verifies REQ-SIMFW-12
test('a client string is never placed in a VNS command line, and a valid one is one well-formed line', () => {
  const hostile = [
    { type: 'MODULE_FAIL', kind: 'vns', op: 'vns_stim_set\nquit', code: -17, times: 1 },
    { type: 'MODULE_FAIL', kind: 'vns', op: 'eeg_start', code: -17, times: 1 },
    { type: 'MODULE_SET', kind: 'vns', patch: { add: '1500\nquit' } },
    { type: 'MODULE_SET', kind: 'vns', patch: { readouts: { rmssd_ms: 40, coherence: 101, hr_bpm: 60 } } },
    { type: 'FW_COMMAND', cmd: 'vns', side: 0, freq_mhz: 25000, amp_ua: 2000, pw_us: 250, ppg: 1, eeg_ref: 1 },
    { type: 'FW_COMMAND', cmd: 'vns', side: 0, freq_mhz: 25000, amp_ua: '2000\nquit', pw_us: 250, ppg: 1, eeg_ref: 1, hrv_proto: 0 },
    { type: 'FW_COMMAND', cmd: 'vns', side: 0, freq_mhz: 4294967296, amp_ua: 2000, pw_us: 250, ppg: 1, eeg_ref: 1, hrv_proto: 0 },
  ];
  for (const m of hostile) {
    assert.throws(() => messageToLines(m), BridgeError, JSON.stringify(m));
  }
  assert.deepStrictEqual(messageToLines({ type: 'MODULE_SET', kind: 'vns', patch: { add: 1500 } }), ['vns_add 1500']);
  assert.deepStrictEqual(messageToLines({ type: 'MODULE_SET', kind: 'vns', patch: { readouts: { rmssd_ms: 42.5, coherence: 6.5, hr_bpm: 64 } } }),
    ['vns_readouts 42.5 6.5 64']);
  assert.deepStrictEqual(messageToLines({ type: 'MODULE_FAIL', kind: 'vns', op: 'vns_stim_stop', code: -17, times: 1 }), ['fail 0 vns_stim_stop -17 1']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'vns', side: 2, freq_mhz: 25000, amp_ua: 2000, pw_us: 250, ppg: 1, eeg_ref: 0, hrv_proto: 1 }),
    ['vns 2 25000 2000 250 1 0 1']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'vns_stop' }), ['vns_stop']);
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
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'report', version: 2 }), ['report 2']);
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

// @verifies REQ-SIMFW-11
test('end to end: a tDCS ramp runs the real driver and an over-delivery on one pair is flagged', { skip: !BIN }, async () => {
  const bridge = new FwBridge({ bin: BIN }).start();
  const seen = [];
  bridge.on('message', (m) => seen.push(m));
  const wait = (ms) => new Promise((r) => setTimeout(r, ms));
  for (const p of [0, 1]) bridge.handle({ type: 'MODULE_SET', kind: 'stim_pair', pair: p, patch: { add: 5 } });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'tdcs', pair: 0, current_ua: 1000, polarity: 0, ramp_s: 30, area_mcm2: 25000 });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'advance', ms: 15000 });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'sample', channel: 'tdcs' });
  bridge.handle({ type: 'MODULE_SET', kind: 'stim_pair', pair: 0, patch: { reading: { op: 'stim_read_current', value: 1900 } } });
  for (let i = 0; i < 4; i++) bridge.handle({ type: 'FW_COMMAND', cmd: 'sample', channel: 'tdcs' });
  await wait(500);
  bridge.stop();

  const samples = seen.filter((m) => m.type === 'FW_LOG' && m.event.ev === 'stim_sample').map((m) => m.event);
  assert.strictEqual(samples[0].current_ua, 500, 'halfway up the 30 s ramp the DAC delivers half the target');
  assert.strictEqual(samples.filter((s) => s.divergence_flagged).length, 1, 'the pinned over-delivery is flagged exactly once');
  const result = seen.find((m) => m.type === 'FW_LOG' && m.event.ev === 'result' && m.event.cmd === 'tdcs');
  assert.strictEqual(result.event.code, 0);
  assert.strictEqual(result.event.safety_mcu, 'not_in_loop', 'and the result says the safety MCU is not in the loop');
  assert.strictEqual(bridge.list().modules.length, 2, 'MODULE_LIST holds the two pairs');
});

// @verifies REQ-SIMFW-11
test('end to end: the real EEG driver configures the sim front end and a lost stop is visible', { skip: !BIN }, async () => {
  const bridge = new FwBridge({ bin: BIN }).start();
  const seen = [];
  bridge.on('message', (m) => seen.push(m));
  const wait = (ms) => new Promise((r) => setTimeout(r, ms));
  bridge.handle({ type: 'MODULE_SET', kind: 'eeg', patch: { add: {} } });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'eeg_init' });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'eeg', channel_mask: 15, gain: 6, notch: 0, ref_mode: 0, adaptive_out: 0 });
  bridge.handle({ type: 'MODULE_FAIL', kind: 'eeg', op: 'eeg_stop', code: -17, times: 1 });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'eeg_stop' });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'status' });
  await wait(500);
  bridge.stop();
  const stop = seen.find((m) => m.type === 'FW_LOG' && m.event.ev === 'result' && m.event.cmd === 'eeg_stop');
  assert.strictEqual(stop.event.code, 0, 'the driver reports the stop done');
  assert.strictEqual(stop.event.eeg_running, true, 'while the converter still runs');
  const state = seen.filter((m) => m.type === 'MODULE_STATE' && m.event.id === 'eeg:0').pop();
  assert.strictEqual(state.event.driven, true, 'and the module state says so');
});

// @verifies REQ-SIMFW-11
test('end to end: the real VNS driver and HRV library run, and a clip out of contact is refused', { skip: !BIN }, async () => {
  const bridge = new FwBridge({ bin: BIN }).start();
  const seen = [];
  bridge.on('message', (m) => seen.push(m));
  const wait = (ms) => new Promise((r) => setTimeout(r, ms));
  bridge.handle({ type: 'MODULE_SET', kind: 'vns', patch: { add: 1500 } });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'vns', side: 0, freq_mhz: 25000, amp_ua: 2000, pw_us: 250, ppg: 1, eeg_ref: 0, hrv_proto: 1 });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'vns_stop' });
  bridge.handle({ type: 'MODULE_SET', kind: 'vns', patch: { reading: { op: 'vns_impedance', value: 100 } } });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'vns', side: 0, freq_mhz: 25000, amp_ua: 2000, pw_us: 250, ppg: 0, eeg_ref: 0, hrv_proto: 0 });
  await wait(500);
  bridge.stop();
  const results = seen.filter((m) => m.type === 'FW_LOG' && m.event.ev === 'result' && m.event.cmd === 'vns').map((m) => m.event);
  assert.strictEqual(results[0].code, 0);
  assert.strictEqual(results[0].stimulating, true, 'in contact the clip stimulates');
  assert.notStrictEqual(results[1].code, 0, 'out of contact the command is refused');
  assert.strictEqual(results[1].stimulating, false);
  const hrv = seen.filter((m) => m.type === 'FW_LOG' && m.event.fn === 'np_log_hrv_session');
  assert.strictEqual(hrv.length, 1, 'the real HRV session ended once, with the stop');
});
