'use strict';
/**
 * Tests for the firmware bridge (NP-SIM-FW-001).  Run: node --test server/
 * The end-to-end test runs the real np_sim_pbm_spike when NP_SIM_FW_BIN names it,
 * and is skipped otherwise.
 */
const test = require('node:test');
const assert = require('node:assert');
const { FwBridge, BridgeError, messageToLines, eventToMessage, SAFETY_LINES } = require('./fw-bridge');
const { EventEmitter } = require('node:events');

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
test('a client string is never placed in an audio command line, and a valid one is one well-formed line', () => {
  const hostile = [
    { type: 'MODULE_FAIL', kind: 'audio', op: 'audio_binaural\nquit', code: -17, times: 1 },
    { type: 'MODULE_FAIL', kind: 'audio', op: 'vns_stim_set', code: -17, times: 1 },
    { type: 'MODULE_SET', kind: 'audio', patch: { add: '8\nquit' } },
    { type: 'FW_COMMAND', cmd: 'audio', mode: 0, carrier_hz: 70000, beat_mhz: 1, volume_pct: 1, bone: 0, adaptive: 0 },
    { type: 'FW_COMMAND', cmd: 'audio', mode: 0, carrier_hz: 200, beat_mhz: 1, volume_pct: '40\nquit', bone: 0, adaptive: 0 },
    { type: 'FW_COMMAND', cmd: 'audio_eeg_band', band: -1 },
    { type: 'FW_COMMAND', cmd: 'audio', mode: 0, carrier_hz: 200 },
  ];
  for (const m of hostile) {
    assert.throws(() => messageToLines(m), BridgeError, JSON.stringify(m));
  }
  assert.deepStrictEqual(messageToLines({ type: 'MODULE_SET', kind: 'audio', patch: { add: 8 } }), ['audio_add 8']);
  assert.deepStrictEqual(messageToLines({ type: 'MODULE_FAIL', kind: 'audio', op: 'audio_bone_stop', code: -17, times: 1 }), ['fail 0 audio_bone_stop -17 1']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'audio', mode: 0, carrier_hz: 200, beat_mhz: 10000, volume_pct: 40, bone: 1, adaptive: 0 }),
    ['audio 0 200 10000 40 1 0']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'audio_eeg_band', band: 2 }), ['audio_eeg_band 2']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'audio_stop' }), ['audio_stop']);
});

// @verifies REQ-SIMFW-12
test('a client string is never placed in a goggle command line, and a valid one is one well-formed line', () => {
  const hostile = [
    { type: 'MODULE_FAIL', kind: 'visual', op: 'vis_led_set\nquit', code: -17, times: 1 },
    { type: 'MODULE_FAIL', kind: 'visual', op: 'audio_binaural', code: -17, times: 1 },
    { type: 'MODULE_SET', kind: 'visual', patch: { add: { seated: 1, eye_open: true, mpe_ok: true } } },
    { type: 'MODULE_SET', kind: 'visual', patch: { sensor: { name: 'seated\nquit', value: true } } },
    { type: 'MODULE_SET', kind: 'visual', patch: { sensor: { name: 'mpe', value: 'true' } } },
    { type: 'FW_COMMAND', cmd: 'vis', mode: 0, freq_hz: 10, duty_pct: 50, wl: 0, zone_lo: 63, zone_hi: 0, emdr_mhz: 0, mode_f: 0 },
    { type: 'FW_COMMAND', cmd: 'vis', mode: 0, freq_hz: 256, duty_pct: 50, wl: 0, zone_lo: 63, zone_hi: 0, emdr_mhz: 0, mode_f: 0, shade: 0 },
    { type: 'FW_COMMAND', cmd: 'vis_lift\nquit' },
  ];
  for (const m of hostile) {
    assert.throws(() => messageToLines(m), BridgeError, JSON.stringify(m));
  }
  assert.deepStrictEqual(messageToLines({ type: 'MODULE_SET', kind: 'visual', patch: { add: { seated: true, eye_open: true, mpe_ok: true } } }), ['vis_add 1 1 1']);
  assert.deepStrictEqual(messageToLines({ type: 'MODULE_SET', kind: 'visual', patch: { sensor: { name: 'mpe', value: false } } }), ['vis_sensor mpe 0']);
  assert.deepStrictEqual(messageToLines({ type: 'MODULE_FAIL', kind: 'visual', op: 'vis_led_stop', code: -17, times: 1 }), ['fail 0 vis_led_stop -17 1']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'vis', mode: 0, freq_hz: 10, duty_pct: 50, wl: 0, zone_lo: 63, zone_hi: 0, emdr_mhz: 0, mode_f: 0, shade: 0 }),
    ['vis 0 10 50 0 63 0 0 0 0']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'vis_lift' }), ['vis_lift']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'vis_ppx' }), ['vis_ppx']);
});

// @verifies REQ-SIMFW-12
test('a client string is never placed in an intranasal or stub command line, and a valid one is one well-formed line', () => {
  const hostile = [
    { type: 'MODULE_FAIL', kind: 'intranasal', op: 'ins_pwm_set\nquit', code: -17, times: 1 },
    { type: 'MODULE_FAIL', kind: 'intranasal', op: 'vis_led_set', code: -17, times: 1 },
    { type: 'MODULE_FAIL', kind: 'intranasal', op: 'ins_pwm_set', code: -17, times: 1, after: '1\nquit' },
    { type: 'MODULE_SET', kind: 'intranasal', patch: { sensor: { name: 'auth\nquit', value: true } } },
    { type: 'MODULE_SET', kind: 'intranasal', patch: { pd: { side: 2, wl: 0, counts: 5 } } },
    { type: 'FW_COMMAND', cmd: 'ins', side: 0, freq_code: 40, duty: 300, cur_660: 1, cur_808: 1 },
    { type: 'FW_COMMAND', cmd: 'stub', name: 'tms\nquit', op: 'detect' },
    { type: 'FW_COMMAND', cmd: 'stub', name: 'tms', op: 'detect\nquit' },
    { type: 'FW_COMMAND', cmd: 'stub', name: 'vns', op: 'detect' },
  ];
  for (const m of hostile) {
    assert.throws(() => messageToLines(m), BridgeError, JSON.stringify(m));
  }
  assert.deepStrictEqual(messageToLines({ type: 'MODULE_SET', kind: 'intranasal', patch: { add: { auth: true, contact: true } } }), ['ins_add 1 1']);
  assert.deepStrictEqual(messageToLines({ type: 'MODULE_FAIL', kind: 'intranasal', op: 'ins_pwm_set', code: -17, times: 1, after: 1 }), ['fail 0 ins_pwm_set -17 1 1']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'ins', side: 0, freq_code: 40, duty: 20, cur_660: 60, cur_808: 70 }), ['ins 0 40 20 60 70']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'stub', name: 'tms', op: 'control' }), ['stub tms control']);
});

// @verifies REQ-SIMFW-12
test('a client string is never placed in a cervical VNS command line, and a valid one is one well-formed line', () => {
  const hostile = [
    { type: 'MODULE_FAIL', kind: 'cvns', op: 'cvns_accessory\nquit', code: -17, times: 1 },
    { type: 'MODULE_FAIL', kind: 'cvns', op: 'ins_auth', code: -17, times: 1 },
    { type: 'MODULE_SET', kind: 'cvns', patch: { add: { left_kohm: 2.5, right_kohm: '2.5\nquit', unix_base_s: 1 } } },
    { type: 'MODULE_SET', kind: 'cvns', patch: { add: { left_kohm: 2.5, right_kohm: 2.5 } } },
    { type: 'FW_COMMAND', cmd: 'cvns_run', ms: 1000, bpm: '70\nquit' },
    { type: 'FW_COMMAND', cmd: 'cvns_run', ms: 1000, bpm: 301 },
    { type: 'FW_COMMAND', cmd: 'cvns_mcu', granted_mask: 65536, status: 0 },
    { type: 'FW_COMMAND', cmd: 'cvns', side: 2, freq_mhz: 10000, amp_ua: 1000, pw_us: 250, ramp_s: 10 },
    { type: 'FW_COMMAND', cmd: 'cvns_stop\nquit' },
  ];
  for (const m of hostile) {
    assert.throws(() => messageToLines(m), BridgeError, JSON.stringify(m));
  }
  assert.deepStrictEqual(messageToLines({ type: 'MODULE_SET', kind: 'cvns', patch: { add: { left_kohm: 2.5, right_kohm: 2.5, unix_base_s: 1700000000 } } }),
    ['cvns_add 2.5 2.5 1700000000']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'cvns', side: 2, freq_mhz: 10000, amp_ua: 1000, pw_us: 250, ramp_s: 10, baseline_req: 1 }),
    ['cvns 2 10000 1000 250 10 1']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'cvns_mcu', granted_mask: 1024, status: 0 }), ['cvns_mcu 1024 0']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'cvns_run', ms: 7600, bpm: 70 }), ['cvns_run 7600 70']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'cvns_run', ms: 1000, bpm: -1 }), ['cvns_run 1000 -1']);
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

// ── the safety MCU in the loop (NP-SIM-FW-001 §23, REQ-SIMFW-15) ───────────────────────────────────────────────────────────────

// @verifies REQ-SIMFW-12 REQ-SIMFW-15#a
test('a client string is never placed in a safety MCU command line', () => {
  const hostile = [
    { type: 'FW_COMMAND', cmd: 'safety_attach', seed: '7\nquit', tier: 1, uid: 9, hw_version: 1 },
    { type: 'FW_COMMAND', cmd: 'safety_attach', seed: 256, tier: 1, uid: 9, hw_version: 1 },
    { type: 'FW_COMMAND', cmd: 'safety_attach', seed: 7, tier: 3, uid: 9, hw_version: 1 },
    { type: 'FW_COMMAND', cmd: 'safety_attach', seed: 7, tier: 1, uid: 9, hw_version: 16 },
    { type: 'FW_COMMAND', cmd: 'safety_attach', seed: 7, tier: 1, hw_version: 1 },                       // uid missing
    { type: 'FW_COMMAND', cmd: 'safety_attach', tier: 0 },                                              // seed missing
    { type: 'FW_COMMAND', cmd: 'safety_beat', state: 8 },
    { type: 'FW_COMMAND', cmd: 'safety_beat', state: '3\nquit' },
    { type: 'FW_COMMAND', cmd: 'safety_run', ms: 0, state: 3 },
    { type: 'FW_COMMAND', cmd: 'safety_run', ms: 60001, state: 3 },
    { type: 'FW_COMMAND', cmd: 'safety_run', ms: 1000, state: 3, bpm: '70\nquit' },
    { type: 'FW_COMMAND', cmd: 'safety_run', ms: 1000, state: 3, bpm: 301 },
    { type: 'FW_COMMAND', cmd: 'safety_run', ms: 1.5, state: 3 },
    { type: 'FW_COMMAND', cmd: 'safety_sig', which: 'good\nquit' },
    { type: 'FW_COMMAND', cmd: 'safety_sig', which: 'ugly' },
    { type: 'FW_COMMAND', cmd: 'safety_sig' },
    { type: 'FW_COMMAND', cmd: 'safety_limits', channel: 14, area_mcm2: 100 },
    { type: 'FW_COMMAND', cmd: 'safety_limits', channel: 5, area_mcm2: 65536 },
    { type: 'FW_COMMAND', cmd: 'safety_wave', channel: 5, class: 256, phase_us: 0 },
    { type: 'FW_COMMAND', cmd: 'safety_wave', channel: 5, class: 2, phase_us: '1\nquit' },
    { type: 'FW_COMMAND', cmd: 'safety_enable', lines: ['bes\nquit'] },
    { type: 'FW_COMMAND', cmd: 'safety_enable', lines: ['constructor'] },                                 // not a line, though an Object key
    { type: 'FW_COMMAND', cmd: 'safety_enable', lines: [32] },                                           // a client mask is never accepted
    { type: 'FW_COMMAND', cmd: 'safety_enable', lines: [] },
    { type: 'FW_COMMAND', cmd: 'safety_enable', mask: 32 },
    { type: 'FW_COMMAND', cmd: 'safety_disable', lines: 'bes' },
    { type: 'FW_COMMAND', cmd: 'safety_current', channel: 5, ua: 65536 },
    { type: 'FW_COMMAND', cmd: 'safety_adc', channel: 6, counts: 100 },
    { type: 'FW_COMMAND', cmd: 'safety_adc', channel: 0, counts: 4096 },
    { type: 'FW_COMMAND', cmd: 'safety_imp', channel: 4, ohm: 5000 },
    { type: 'FW_COMMAND', cmd: 'safety_imp', channel: 2, ohm: '5000\nquit' },
    { type: 'FW_COMMAND', cmd: 'safety_imp', channel: 2, ohm: -1 },
    { type: 'FW_COMMAND', cmd: 'safety_cvns_el', electrode: 2, ohm: 2000 },
    { type: 'FW_COMMAND', cmd: 'safety_hr', valid: 1, hr_x10: 700, age_ms: 0 },                         // a number, not a boolean
    { type: 'FW_COMMAND', cmd: 'safety_hr', valid: true, hr_x10: 70000, age_ms: 0 },
    { type: 'FW_COMMAND', cmd: 'safety_user', tag: 4294967296 },
    { type: 'FW_COMMAND', cmd: 'safety_reenable', on: 'true' },
    { type: 'FW_COMMAND', cmd: 'safety_fault', kind: 'stall\nquit', n: 1 },
    { type: 'FW_COMMAND', cmd: 'safety_fault', kind: 'reboot', n: 1 },
    { type: 'FW_COMMAND', cmd: 'safety_fault', kind: 'stall', n: 1.5 },
  ];
  for (const m of hostile) {
    assert.throws(() => messageToLines(m), BridgeError, JSON.stringify(m));
  }
  // a command name is looked up, never forwarded: one that is not in the whitelist is refused
  for (const cmd of ['safety_rpeak\nquit', 'safety_anything', 'safety_status\nquit', 'safety_attach\nquit']) {
    assert.throws(() => messageToLines({ type: 'FW_COMMAND', cmd }), BridgeError, JSON.stringify(cmd));
  }
});

// @verifies REQ-SIMFW-12 REQ-SIMFW-15#a
test('a valid safety MCU message becomes exactly the well-formed lines the harness takes', () => {
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'safety_attach', seed: 7, tier: 1, uid: 9, hw_version: 1 }),
    ['safety_root 7', 'safety_uid 9', 'safety_tier 1 1', 'safety_attach']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'safety_attach', seed: 7, tier: 0 }), ['safety_root 7', 'safety_attach'], 'tier 0: no tier record');
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'safety_beat', state: 3 }), ['safety_beat 3']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'safety_run', ms: 1000, state: 3 }), ['safety_run 1000 3']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'safety_run', ms: 1000, state: 3, bpm: 70 }), ['safety_run 1000 3 70']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'safety_sig', which: 'bad' }), ['safety_sig bad']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'safety_limits', channel: 5, area_mcm2: 1000 }), ['safety_limits 5 1000']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'safety_wave', channel: 5, class: 2, phase_us: 20000 }), ['safety_wave 5 2 20000']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'safety_enable', lines: ['bes'] }), ['safety_enable 32'], 'the mask is built here, from a name');
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'safety_enable', lines: ['pbm_cranial', 'cvns'] }), ['safety_enable 1025']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'safety_disable', lines: ['tdcs'] }), ['safety_disable 64']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'safety_current', channel: 10, ua: 1000 }), ['safety_current 10 1000']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'safety_adc', channel: 0, counts: 600 }), ['safety_adc 0 600']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'safety_imp', channel: 2, ohm: 5000 }), ['safety_imp 2 5000']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'safety_cvns_el', electrode: 1, ohm: 2000 }), ['safety_cvns_el 1 2000']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'safety_hr', valid: true, hr_x10: 700, age_ms: 0 }), ['safety_hr 1 700 0']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'safety_user', tag: 1 }), ['safety_user 1']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'safety_reenable', on: true }), ['safety_reenable 1']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'safety_fault', kind: 'stall', n: 1 }), ['safety_fault stall 1 0']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'safety_fault', kind: 'spi_flip', n: 3, arg: 4 }), ['safety_fault spi_flip 3 4']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'safety_rpeak' }), ['safety_rpeak']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'safety_status' }), ['safety_status']);
  assert.deepStrictEqual(messageToLines({ type: 'FW_COMMAND', cmd: 'safety_root_corrupt' }), ['safety_root_corrupt']);
  assert.deepStrictEqual(Object.keys(SAFETY_LINES), ['pbm_cranial', 'bes', 'tdcs', 'vns', 'visual', 'intranasal', 'cvns', 'tms', 'pbm_1170', 'clin_stim'],
    'ten lines, in the MCU\'s order');
});

function fakeProc() {
  const proc = new EventEmitter();
  proc.written = [];
  proc.stdin = { write: (l) => { proc.written.push(l); return true; } };
  proc.stdout = Object.assign(new EventEmitter(), { setEncoding() {} });
  proc.kill = () => {};
  return proc;
}

// @verifies REQ-SIMFW-15#a
test('the OTP is programmed once, before the safety MCU is attached, and never again from a client', () => {
  const proc = fakeProc();
  const bridge = new FwBridge({ bin: 'x', spawnFn: () => proc }).start();
  const attach = { type: 'FW_COMMAND', cmd: 'safety_attach', seed: 7, tier: 1, uid: 9, hw_version: 1 };
  assert.ok(bridge.handle(attach));
  assert.deepStrictEqual(proc.written, ['safety_root 7\n', 'safety_uid 9\n', 'safety_tier 1 1\n', 'safety_attach\n']);
  assert.throws(() => bridge.handle(attach), BridgeError, 'a second attach would reprogram the OTP under a running MCU');
  assert.throws(() => bridge.handle({ type: 'FW_COMMAND', cmd: 'safety_root_corrupt' }), BridgeError);
  assert.strictEqual(proc.written.length, 4, 'and nothing was written for either');
  assert.ok(bridge.handle({ type: 'FW_COMMAND', cmd: 'safety_beat', state: 3 }), 'the other commands still go');
});

// @verifies REQ-SIMFW-15#b
test('the safety MCU\'s events are routed to the page: state, independent-watchdog reset, unknown pin', () => {
  assert.strictEqual(eventToMessage({ ev: 'safety_state', attached: true }).type, 'SAFETY_STATE');
  assert.strictEqual(eventToMessage({ ev: 'safety_iwdg_reset', t: 1, resets: 1 }).type, 'SAFETY_EVENT');
  assert.strictEqual(eventToMessage({ ev: 'safety_pin_unknown', t: 1, pin: 3 }).type, 'SAFETY_EVENT');
  assert.strictEqual(eventToMessage({ ev: 'unrouted', symbol: 'np_safety_hal_spi_transfer' }).type, 'FW_LOG', 'the absence of the link stays a log line');
  assert.strictEqual(eventToMessage({ ev: 'safety_state', attached: true }).event.attached, true, 'and the event is carried whole');
});

// @verifies REQ-SIMFW-15#b
test('a page that connects late is sent the last safety state, and none before an attach', () => {
  const proc = fakeProc();
  const bridge = new FwBridge({ bin: 'x', spawnFn: () => proc }).start();
  assert.strictEqual(bridge.list().safety, null, 'no safety MCU: no state');
  const seen = [];
  bridge.on('message', (m) => seen.push(m));
  const ev = { ev: 'safety_state', cmd: 'status', attached: true, lines: 32, safety_mcu: 'in_loop' };
  proc.stdout.emit('data', JSON.stringify(ev) + '\n' + JSON.stringify({ ev: 'safety_iwdg_reset', t: 5, resets: 1 }) + '\n');
  assert.deepStrictEqual(seen.map((m) => m.type), ['SAFETY_STATE', 'SAFETY_EVENT']);
  assert.deepStrictEqual(bridge.list().safety, ev);
});

const BIN = process.env.NP_SIM_FW_BIN;

// @verifies REQ-SIMFW-11
test('end to end: the simulator refuses a PBM drive as the device does, so an armed fault is never reached', { skip: !BIN }, async () => {
  // OI-SIMFW-11: the production power governor refuses every load (OI-FWHUB-09) and the simulator keeps it.
  // The driver, rollback and injection paths behind it are covered by np_sim_pbm_tests, which open the governor
  // from C; the bridge has no way to.
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

  const result = seen.find((m) => m.type === 'FW_LOG' && m.event.ev === 'result' && m.event.cmd === 'drive');
  assert.strictEqual(result.event.code, -18, 'the firmware refuses with NP_HUB_ERR_POWER_BUDGET');
  assert.strictEqual(result.event.active, 0, 'and no socket is active');
  const hal = seen.filter((m) => m.type === 'FW_LOG' && m.event.ev === 'hal');
  assert.strictEqual(hal.filter((m) => m.event.source === 'injected').length, 0, 'no write reached a tile, so the armed fault never fired');
  const states = seen.filter((m) => m.type === 'MODULE_STATE' && m.event.ev === 'module_state');
  assert.ok(states.every((m) => m.event.driven === false), 'every tile is dark');
  assert.ok(!seen.some((m) => m.type === 'FW_LOG' && m.event.ev === 'sim_policy'), 'and no policy override is announced');
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
test('end to end: the real EEG driver configures the sim front end, reports a lost stop, and a second stop retries it (OI-SIMFW-16, closed)', { skip: !BIN }, async () => {
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
  await wait(300);
  const states = () => seen.filter((m) => m.type === 'MODULE_STATE' && m.event.id === 'eeg:0');
  const stops = () => seen.filter((m) => m.type === 'FW_LOG' && m.event.ev === 'result' && m.event.cmd === 'eeg_stop');
  assert.strictEqual(stops()[0].event.code, -17, 'the driver reports the lost stop');
  assert.strictEqual(stops()[0].event.eeg_running, true, 'while the converter still runs');
  assert.strictEqual(states().pop().event.driven, true, 'and the module state says so');
  bridge.handle({ type: 'FW_COMMAND', cmd: 'eeg_stop' });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'status' });
  await wait(300);
  bridge.stop();
  assert.strictEqual(stops()[1].event.code, 0, 'a second stop retries and lands');
  assert.strictEqual(stops()[1].event.eeg_running, false, 'the converter has stopped');
  assert.strictEqual(states().pop().event.driven, false, 'and the module state says so');
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

// @verifies REQ-SIMFW-11
test('end to end: the real audio driver plays, adapts to an EEG band, and a lost stop is visible', { skip: !BIN }, async () => {
  const bridge = new FwBridge({ bin: BIN }).start();
  const seen = [];
  bridge.on('message', (m) => seen.push(m));
  const wait = (ms) => new Promise((r) => setTimeout(r, ms));
  bridge.handle({ type: 'MODULE_SET', kind: 'audio', patch: { add: 8 } });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'audio', mode: 0, carrier_hz: 200, beat_mhz: 6000, volume_pct: 40, bone: 0, adaptive: 1 });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'audio_eeg_band', band: 2 });
  bridge.handle({ type: 'MODULE_FAIL', kind: 'audio', op: 'audio_stop_planar', code: -17, times: 1 });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'audio_stop' });
  await wait(500);
  bridge.stop();
  const results = seen.filter((m) => m.type === 'FW_LOG' && m.event.ev === 'result').map((m) => m.event);
  assert.strictEqual(results.find((r) => r.cmd === 'audio_eeg_band').beat_mhz, 10000, 'the beat follows the alpha band');
  const stop = results.find((r) => r.cmd === 'audio_stop');
  assert.strictEqual(stop.code, 0, 'the driver reports the stop done');
  assert.strictEqual(stop.mode, 1, 'while the planar drivers still play');
});

// @verifies REQ-SIMFW-11
test('end to end: the real goggle driver emits, and the Hall edge cuts it through the driver\'s own handler', { skip: !BIN }, async () => {
  const bridge = new FwBridge({ bin: BIN }).start();
  const seen = [];
  bridge.on('message', (m) => seen.push(m));
  const wait = (ms) => new Promise((r) => setTimeout(r, ms));
  bridge.handle({ type: 'MODULE_SET', kind: 'visual', patch: { add: { seated: true, eye_open: true, mpe_ok: true } } });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'vis_init' });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'vis', mode: 0, freq_hz: 2, duty_pct: 50, wl: 0, zone_lo: 63, zone_hi: 0, emdr_mhz: 0, mode_f: 0, shade: 0 });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'vis_lift' });
  await wait(500);
  bridge.stop();
  const results = seen.filter((m) => m.type === 'FW_LOG' && m.event.ev === 'result').map((m) => m.event);
  assert.strictEqual(results.find((r) => r.cmd === 'vis').emitting, true, 'the goggles emit');
  const lift = results.find((r) => r.cmd === 'vis_lift');
  assert.strictEqual(lift.emitting, false, 'and the lift edge turned them off');
  assert.strictEqual(lift.edges_delivered, 1);
  assert.ok(seen.some((m) => m.type === 'FW_LOG' && m.event.ev === 'hall_edge' && m.event.handler_registered === true), 'through a handler the driver registered');
});

// @verifies REQ-SIMFW-11
test('end to end: a bilateral intranasal command that fails on its second side stops the first, so nothing is driven (OI-SIMFW-21, fixed), and a T2 stub refuses', { skip: !BIN }, async () => {
  const bridge = new FwBridge({ bin: BIN }).start();
  const seen = [];
  bridge.on('message', (m) => seen.push(m));
  const wait = (ms) => new Promise((r) => setTimeout(r, ms));
  bridge.handle({ type: 'MODULE_SET', kind: 'intranasal', patch: { add: { auth: true, contact: true } } });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'ins_init' });
  bridge.handle({ type: 'MODULE_FAIL', kind: 'intranasal', op: 'ins_pwm_set', code: -17, times: 1, after: 1 });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'ins', side: 0, freq_code: 40, duty: 20, cur_660: 60, cur_808: 70 });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'stub', name: 'tms', op: 'control' });
  await wait(500);
  bridge.stop();
  const r = seen.filter((m) => m.type === 'FW_LOG' && m.event.ev === 'result' && m.event.cmd === 'ins').map((m) => m.event)[0];
  assert.notStrictEqual(r.code, 0, 'the command fails');
  assert.strictEqual(r.left_on, false, 'the left side that was written is stopped again, not left driven');
  assert.strictEqual(r.right_on, false, 'and the right side never started');
  const stub = seen.find((m) => m.type === 'FW_LOG' && m.event.ev === 'stub').event;
  assert.ok(stub.code < 0, 'the TMS stub refuses');
  assert.strictEqual(stub.requested_enables & 0x800, 0, 'and requests no TMS enable');
});

// @verifies REQ-SIMFW-11
test('end to end: the real cervical VNS flow gets from the baseline through the grant to steady delivery (OI-SIMFW-22, fixed)', { skip: !BIN }, async () => {
  const bridge = new FwBridge({ bin: BIN }).start();
  const seen = [];
  bridge.on('message', (m) => seen.push(m));
  const wait = (ms) => new Promise((r) => setTimeout(r, ms));
  bridge.handle({ type: 'MODULE_SET', kind: 'cvns', patch: { add: { left_kohm: 2.5, right_kohm: 2.5, unix_base_s: 1700000000 } } });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'cvns_init' });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'cvns', side: 2, freq_mhz: 10000, amp_ua: 1000, pw_us: 250, ramp_s: 10, baseline_req: 1 });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'cvns_mcu', granted_mask: 1024, status: 0 });
  bridge.handle({ type: 'FW_COMMAND', cmd: 'cvns_run', ms: 22000, bpm: 70 });
  await wait(800);
  bridge.stop();
  const st = seen.filter((m) => m.type === 'FW_LOG' && m.event.ev === 'cvns_state').map((m) => m.event).pop();
  assert.strictEqual(st.baseline_valid, true, 'the real detector found the 70 BPM baseline');
  assert.strictEqual(st.stage, 4, 'and the session is in steady delivery (ACTIVE)');
  assert.strictEqual(st.abort_reason, 0, 'with no abort reason');
  assert.strictEqual(st.enable_requested, true);
  assert.strictEqual(st.safety_mcu, 'scenario', 'the state says the safety MCU is the scenario');
});

// @verifies REQ-SIMFW-15#a REQ-SIMFW-15#b
test('end to end: through the bridge the safety MCU grants a BES line, then the heartbeat is lost and it cuts it', { skip: !BIN }, async () => {
  const bridge = new FwBridge({ bin: BIN }).start();
  const seen = [];
  bridge.on('message', (m) => seen.push(m));
  const wait = (ms) => new Promise((r) => setTimeout(r, ms));
  const send = (m) => bridge.handle(m);
  send({ type: 'FW_COMMAND', cmd: 'status' });
  await wait(100);
  assert.strictEqual(seen.filter((m) => m.type === 'SAFETY_STATE').length, 0, 'no safety state before an attach');
  send({ type: 'FW_COMMAND', cmd: 'safety_attach', seed: 7, tier: 1, uid: 9, hw_version: 1 });
  send({ type: 'FW_COMMAND', cmd: 'safety_imp', channel: 2, ohm: 5000 });
  send({ type: 'MODULE_SET', kind: 'stim_pair', pair: 0, patch: { add: 5 } });
  send({ type: 'FW_COMMAND', cmd: 'safety_run', ms: 1000, state: 3 });
  send({ type: 'FW_COMMAND', cmd: 'safety_sig', which: 'good' });
  send({ type: 'FW_COMMAND', cmd: 'safety_run', ms: 400, state: 3 });
  // the declarations come after the session has started: the session-start reset wipes an earlier one (np_sim_safety_tests.c)
  send({ type: 'FW_COMMAND', cmd: 'safety_wave', channel: 5, class: 2, phase_us: 20000 });
  send({ type: 'FW_COMMAND', cmd: 'safety_limits', channel: 5, area_mcm2: 1000 });
  send({ type: 'FW_COMMAND', cmd: 'bes', pair: 0, waveform: 0, freq_mhz: 10000, amp_ua: 500 });
  send({ type: 'FW_COMMAND', cmd: 'safety_run', ms: 1000, state: 3 });
  await wait(300);
  const states = () => seen.filter((m) => m.type === 'SAFETY_STATE').map((m) => m.event);
  const granted = states().pop();
  assert.strictEqual(granted.attached, true);
  assert.strictEqual(granted.safety_mcu, 'in_loop');
  assert.strictEqual((granted.lines >> 1) & 1, 1, 'the BES line (index 1) is enabled');
  assert.strictEqual((granted.granted >> 5) & 1, 1, 'and granted (bit 5)');
  send({ type: 'FW_COMMAND', cmd: 'advance', ms: 2500 });
  send({ type: 'FW_COMMAND', cmd: 'status' });
  await wait(300);
  bridge.stop();
  const cut = states().pop();
  assert.strictEqual((cut.lines >> 1) & 1, 0, 'with no heartbeat the MCU cuts the BES line');
  assert.strictEqual((cut.requested >> 5) & 1, 1, 'while the hub still requests it');
  assert.strictEqual(bridge.list().safety.lines, cut.lines, 'and MODULE_LIST carries that state');
});
