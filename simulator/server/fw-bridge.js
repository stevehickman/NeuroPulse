/**
 * NeurOne Helmet Simulator — firmware bridge (NP-SIM-FW-001 §3.5, §8 step 1)
 *
 * Starts the host firmware process (firmware/platform_sim, np_sim_pbm_spike) and
 * relays between it and the WebSocket clients:
 *
 *   client MODULE_SET / MODULE_FAIL / MODULE_CLEAR / FW_COMMAND  ->  text line on the process's stdin
 *   process JSON line on stdout                                  ->  MODULE_STATE / FW_LOG message
 *
 * OPT-IN.  The server starts this only when NP_SIM_FW_BIN names the binary, and
 * then binds to 127.0.0.1 (OI-SIMFW-07): a client that can reach this bridge can
 * inject faults into a process running the hub's stimulation logic.
 *
 * REQ-SIMFW-12: every command line is BUILT from validated integers and
 * whitelisted names.  No string a client sent is ever placed in a command line,
 * so a client cannot add a second command with a newline (CWE-77).
 */

'use strict';

const { spawn } = require('child_process');
const { EventEmitter } = require('events');

const OPS = new Set(['socket_pwm_set', 'socket_ntc_read']);   // np_sim_op_name()
const STIM_OPS = new Set(['stim_dac_set', 'stim_dac_stop', 'stim_dc_set', 'stim_dc_ramp',
  'stim_read_impedance', 'stim_read_current', 'stim_read_charge']);   // np_sim_op_name(), the stimulation DAC
const EEG_OPS = new Set(['eeg_spi_write', 'eeg_spi_read', 'eeg_start', 'eeg_stop', 'eeg_read_impedance',
  'eeg_band_power', 'eeg_dominant_band', 'eeg_self_calibrate']);     // np_sim_op_name(), the ADS1299 front end
const VNS_OPS = new Set(['vns_impedance', 'vns_stim_set', 'vns_stim_stop', 'vns_ppg_start', 'vns_ppg_stop', 'vns_eeg_ref',
  'vns_rmssd', 'vns_coherence', 'vns_hr']);                          // np_sim_op_name(), the auricular clip
const AUDIO_OPS = new Set(['audio_codec_init', 'audio_binaural', 'audio_isochronic', 'audio_noise', 'audio_stop_planar',
  'audio_bone_set', 'audio_bone_stop', 'audio_mesh_impedance']);     // np_sim_op_name(), the audio path
const VIS_OPS = new Set(['vis_led_set', 'vis_led_stop', 'vis_eye_open', 'vis_seated', 'vis_emdr', 'vis_mpe', 'vis_hall_register']);   // the goggles
const VIS_SENSORS = new Set(['seated', 'eye', 'mpe']);
const INS_OPS = new Set(['ins_auth', 'ins_contact', 'ins_pwm_set', 'ins_pwm_stop', 'ins_pd_read']);   // the intranasal Y-probe
const INS_SENSORS = new Set(['auth', 'contact']);
const STUB_NAMES = new Set(['qeeg', 'tms', 'pbm_1170nm', 'clin_tacs', 'hd_tdcs', 'vibrotactile']);   // np_mod_t2_stubs.c
const STUB_OPS = new Set(['detect', 'init', 'control', 'telemetry', 'shutdown']);
const CVNS_OPS = new Set(['cvns_accessory', 'cvns_imp_measure_start', 'cvns_imp_measure_poll', 'cvns_reenable_imp_start', 'cvns_reenable_imp_poll']);   // the cervical VNS accessory
// The ten stimulation enable lines the safety MCU drives, by name, with the enable bit the hub requests and the MCU grants
// (NP_SAFETY_EN_*, firmware/common/include/np_spi_wire_types.h; the order is np_sim_safety_line_name()).  A client names lines;
// the mask on the command line is built here (REQ-SIMFW-12, REQ-SIMFW-15).
const SAFETY_LINES = Object.freeze({
  pbm_cranial: 1 << 0, bes: 1 << 5, tdcs: 1 << 6, vns: 1 << 7, visual: 1 << 8,
  intranasal: 1 << 9, cvns: 1 << 10, tms: 1 << 11, pbm_1170: 1 << 12, clin_stim: 1 << 13,
});
const SAFETY_FAULTS = new Set(['nv_erase', 'nv_program', 'spi_timeout', 'spi_flip', 'stall', 'systick', 'cmd_flip']);   // np_sim_safety_fault_t
const SAFETY_SIG = new Set(['good', 'bad']);
const SAFETY_GEOM = new Set(['clin_stim', 'tdcs', 'bes']);                // the three geometry gates of np_charge_monitor.c
// Commands that program the part's OTP: only before the safety MCU is attached, as manufacture does (REQ-UPG-02).
const SAFETY_PROGRAMMING = new Set(['safety_attach', 'safety_root_corrupt']);
const SAFETY_CHANNELS = 14;                                   // NP_SAFETY_MAX_CHANNELS
const SAFETY_MAX_RUN_MS = 60000;                              // one blocking harness call; unjustified-choices.md UC-127
const STIM_CHANNELS = new Set(['bes', 'tdcs']);                       // the driver's two slots
const MAX_SOCKET = 127;                                       // NP_SIM_MAX_MODULES - 1
const MAX_SOCKETS_PER_COMMAND = 128;                          // the 7-bit socket-address domain (NP_HEXMAP_MAX_SOCKETS)

class BridgeError extends Error {}

/** An optional `after`: answer that many calls normally before the injected response starts ("fail the second write"). */
function afterTail(msg) {
  return msg.after === undefined ? '' : ` ${Number.isInteger(msg.after) && msg.after >= 0 && msg.after <= 1000000 ? msg.after : (() => { throw new BridgeError('after must be an integer in [0, 1000000]'); })()}`;
}

function int(v, lo, hi, what) {
  if (!Number.isInteger(v) || v < lo || v > hi) {
    throw new BridgeError(`${what} must be an integer in [${lo}, ${hi}]`);
  }
  return v;
}

function num(v, lo, hi, what) {
  if (typeof v !== 'number' || !Number.isFinite(v) || v < lo || v > hi) {
    throw new BridgeError(`${what} must be a number in [${lo}, ${hi}]`);
  }
  return v;
}

function op(v) {
  if (typeof v !== 'string' || !OPS.has(v)) {
    throw new BridgeError(`op must be one of ${[...OPS].join(', ')}`);
  }
  return v;   // a member of the whitelist, never the client's string
}

function stimOp(v) {
  if (typeof v !== 'string' || !STIM_OPS.has(v)) {
    throw new BridgeError(`op must be one of ${[...STIM_OPS].join(', ')}`);
  }
  return v;
}

function eegOp(v) {
  if (typeof v !== 'string' || !EEG_OPS.has(v)) {
    throw new BridgeError(`op must be one of ${[...EEG_OPS].join(', ')}`);
  }
  return v;
}

/** The EEG front end's messages: one part, no address. */
function eegModuleLines(msg) {
  switch (msg.type) {
    case 'MODULE_SET': {
      if (msg.patch === undefined || typeof msg.patch !== 'object') {
        throw new BridgeError('MODULE_SET needs a patch object');
      }
      const lines = [];
      if ('add' in msg.patch) {
        const id = msg.patch.add?.id;
        lines.push(id === undefined ? 'eeg_add' : `eeg_add ${int(id, 0, 255, 'add.id')}`);
      }
      if ('present' in msg.patch) {
        if (typeof msg.patch.present !== 'boolean') throw new BridgeError('present must be a boolean');
        lines.push(msg.patch.present ? 'eeg_present' : 'eeg_absent');
      }
      if ('impedance' in msg.patch) {
        const i = msg.patch.impedance;
        lines.push(`eeg_impedance ${int(i?.channel, 0, 7, 'impedance.channel')} ${num(i?.kohm, 0, 100000, 'impedance.kohm')}`);
      }
      if ('band' in msg.patch) {
        const b = msg.patch.band;
        lines.push(`eeg_band ${int(b?.band, 0, 4, 'band.band')} ${num(b?.db, -200, 200, 'band.db')}`);
      }
      if ('reading' in msg.patch) {
        const r = msg.patch.reading;
        lines.push(`reading 0 ${eegOp(r?.op)} ${num(r?.value, -1000000, 1000000, 'reading value')}`);
      }
      if (lines.length === 0) throw new BridgeError('MODULE_SET patch has no recognised field');
      return lines;
    }
    case 'MODULE_FAIL':
      return [`fail 0 ${eegOp(msg.op)} ${int(msg.code, -128, 127, 'code')} ${int(msg.times, -1, 1000000, 'times')}${afterTail(msg)}`];
    case 'MODULE_CLEAR':
      return ['eeg_clear'];
    default:
      return [];
  }
}

/** FW_COMMANDs for the EEG driver. */
function eegCommandLines(msg) {
  switch (msg.cmd) {
    case 'eeg':
      return [`eeg ${int(msg.channel_mask, 0, 255, 'channel_mask')} ${int(msg.gain, 0, 255, 'gain')} ${int(msg.notch, 0, 255, 'notch')} ${int(msg.ref_mode, 0, 255, 'ref_mode')} ${int(msg.adaptive_out, 0, 255, 'adaptive_out')}`];
    case 'eeg_init': case 'eeg_detect': case 'eeg_stop': case 'eeg_shutdown': case 'eeg_sample': case 'eeg_regs':
      return [msg.cmd];                       // a member of the whitelist, never the client's string
    default:
      return null;
  }
}

function vnsOp(v) {
  if (typeof v !== 'string' || !VNS_OPS.has(v)) {
    throw new BridgeError(`op must be one of ${[...VNS_OPS].join(', ')}`);
  }
  return v;
}

/** The auricular clip's messages: one clip, no address. */
function vnsModuleLines(msg) {
  switch (msg.type) {
    case 'MODULE_SET': {
      if (msg.patch === undefined || typeof msg.patch !== 'object') {
        throw new BridgeError('MODULE_SET needs a patch object');
      }
      const lines = [];
      if ('add' in msg.patch) lines.push(`vns_add ${num(msg.patch.add, 0, 1000000, 'add (impedance_ohm)')}`);
      if ('present' in msg.patch) {
        if (typeof msg.patch.present !== 'boolean') throw new BridgeError('present must be a boolean');
        lines.push(msg.patch.present ? 'vns_present' : 'vns_absent');
      }
      if ('readouts' in msg.patch) {
        const r = msg.patch.readouts;
        lines.push(`vns_readouts ${num(r?.rmssd_ms, 0, 10000, 'rmssd_ms')} ${num(r?.coherence, 0, 100, 'coherence')} ${num(r?.hr_bpm, 0, 400, 'hr_bpm')}`);
      }
      if ('reading' in msg.patch) {
        const r = msg.patch.reading;
        lines.push(`reading 0 ${vnsOp(r?.op)} ${num(r?.value, -1000000, 1000000, 'reading value')}`);
      }
      if (lines.length === 0) throw new BridgeError('MODULE_SET patch has no recognised field');
      return lines;
    }
    case 'MODULE_FAIL':
      return [`fail 0 ${vnsOp(msg.op)} ${int(msg.code, -128, 127, 'code')} ${int(msg.times, -1, 1000000, 'times')}${afterTail(msg)}`];
    case 'MODULE_CLEAR':
      return ['vns_clear'];
    default:
      return [];
  }
}

/** FW_COMMANDs for the auricular VNS driver. */
function vnsCommandLines(msg) {
  switch (msg.cmd) {
    case 'vns':
      return [`vns ${int(msg.side, 0, 255, 'side')} ${int(msg.freq_mhz, 0, 4294967295, 'freq_mhz')} ${int(msg.amp_ua, 0, 65535, 'amp_ua')} ` +
              `${int(msg.pw_us, 0, 65535, 'pw_us')} ${int(msg.ppg, 0, 255, 'ppg')} ${int(msg.eeg_ref, 0, 255, 'eeg_ref')} ${int(msg.hrv_proto, 0, 255, 'hrv_proto')}`];
    case 'vns_init': case 'vns_detect': case 'vns_stop': case 'vns_shutdown': case 'vns_sample':
      return [msg.cmd];                       // a member of the whitelist, never the client's string
    default:
      return null;
  }
}

function audioOp(v) {
  if (typeof v !== 'string' || !AUDIO_OPS.has(v)) {
    throw new BridgeError(`op must be one of ${[...AUDIO_OPS].join(', ')}`);
  }
  return v;
}

/** The audio path's messages: one path, no address. */
function audioModuleLines(msg) {
  switch (msg.type) {
    case 'MODULE_SET': {
      if (msg.patch === undefined || typeof msg.patch !== 'object') {
        throw new BridgeError('MODULE_SET needs a patch object');
      }
      const lines = [];
      if ('add' in msg.patch) lines.push(`audio_add ${num(msg.patch.add, 0, 1000000, 'add (mesh_ohm)')}`);
      if ('present' in msg.patch) {
        if (typeof msg.patch.present !== 'boolean') throw new BridgeError('present must be a boolean');
        lines.push(msg.patch.present ? 'audio_present' : 'audio_absent');
      }
      if ('reading' in msg.patch) {
        const r = msg.patch.reading;
        lines.push(`reading 0 ${audioOp(r?.op)} ${num(r?.value, -1000000, 1000000, 'reading value')}`);
      }
      if (lines.length === 0) throw new BridgeError('MODULE_SET patch has no recognised field');
      return lines;
    }
    case 'MODULE_FAIL':
      return [`fail 0 ${audioOp(msg.op)} ${int(msg.code, -128, 127, 'code')} ${int(msg.times, -1, 1000000, 'times')}${afterTail(msg)}`];
    case 'MODULE_CLEAR':
      return ['audio_clear'];
    default:
      return [];
  }
}

/** FW_COMMANDs for the audio driver. */
function audioCommandLines(msg) {
  switch (msg.cmd) {
    case 'audio':
      return [`audio ${int(msg.mode, 0, 255, 'mode')} ${int(msg.carrier_hz, 0, 65535, 'carrier_hz')} ${int(msg.beat_mhz, 0, 65535, 'beat_mhz')} ` +
              `${int(msg.level_half_db, 0, 255, 'level_half_db')} ${int(msg.bone, 0, 255, 'bone')} ${int(msg.adaptive, 0, 255, 'adaptive')}`];
    case 'audio_eeg_band':
      return [`audio_eeg_band ${int(msg.band, 0, 255, 'band')}`];
    case 'audio_init': case 'audio_detect': case 'audio_stop': case 'audio_shutdown': case 'audio_sample':
      return [msg.cmd];                       // a member of the whitelist, never the client's string
    default:
      return null;
  }
}

function visOp(v) {
  if (typeof v !== 'string' || !VIS_OPS.has(v)) {
    throw new BridgeError(`op must be one of ${[...VIS_OPS].join(', ')}`);
  }
  return v;
}

const bit = (v, what) => {
  if (typeof v !== 'boolean') throw new BridgeError(`${what} must be a boolean`);
  return v ? 1 : 0;
};

/** The goggles' messages: one pair, no address. */
function visModuleLines(msg) {
  switch (msg.type) {
    case 'MODULE_SET': {
      if (msg.patch === undefined || typeof msg.patch !== 'object') {
        throw new BridgeError('MODULE_SET needs a patch object');
      }
      const lines = [];
      if ('add' in msg.patch) {
        const a = msg.patch.add;
        lines.push(`vis_add ${bit(a?.seated, 'add.seated')} ${bit(a?.eye_open, 'add.eye_open')} ${bit(a?.mpe_ok, 'add.mpe_ok')}`);
      }
      if ('present' in msg.patch) {
        if (typeof msg.patch.present !== 'boolean') throw new BridgeError('present must be a boolean');
        lines.push(msg.patch.present ? 'vis_present' : 'vis_absent');
      }
      if ('sensor' in msg.patch) {
        const t = msg.patch.sensor;
        if (typeof t?.name !== 'string' || !VIS_SENSORS.has(t.name)) throw new BridgeError(`sensor.name must be one of ${[...VIS_SENSORS].join(', ')}`);
        lines.push(`vis_sensor ${t.name} ${bit(t.value, 'sensor.value')}`);
      }
      if ('reading' in msg.patch) {
        const r = msg.patch.reading;
        lines.push(`reading 0 ${visOp(r?.op)} ${num(r?.value, -1000000, 1000000, 'reading value')}`);
      }
      if (lines.length === 0) throw new BridgeError('MODULE_SET patch has no recognised field');
      return lines;
    }
    case 'MODULE_FAIL':
      return [`fail 0 ${visOp(msg.op)} ${int(msg.code, -128, 127, 'code')} ${int(msg.times, -1, 1000000, 'times')}${afterTail(msg)}`];
    case 'MODULE_CLEAR':
      return ['vis_clear'];
    default:
      return [];
  }
}

/** FW_COMMANDs for the visual driver. */
function visCommandLines(msg) {
  const u8 = (v, what) => int(v, 0, 255, what);
  switch (msg.cmd) {
    case 'vis':
      return [`vis ${u8(msg.mode, 'mode')} ${u8(msg.freq_hz, 'freq_hz')} ${u8(msg.duty_pct, 'duty_pct')} ${u8(msg.wl, 'wl')} ` +
              `${u8(msg.zone_lo, 'zone_lo')} ${u8(msg.zone_hi, 'zone_hi')} ${u8(msg.emdr_mhz, 'emdr_mhz')} ${u8(msg.mode_f, 'mode_f')} ${u8(msg.shade, 'shade')}`];
    case 'vis_init': case 'vis_detect': case 'vis_stop': case 'vis_shutdown': case 'vis_sample': case 'vis_lift':
    case 'vis_lift_silent': case 'vis_lift_during_command': case 'vis_lower': case 'vis_ppx':
      return [msg.cmd];                       // a member of the whitelist, never the client's string
    default:
      return null;
  }
}

function insOp(v) {
  if (typeof v !== 'string' || !INS_OPS.has(v)) {
    throw new BridgeError(`op must be one of ${[...INS_OPS].join(', ')}`);
  }
  return v;
}

/** The intranasal Y-probe's messages: one probe, no address. */
function insModuleLines(msg) {
  switch (msg.type) {
    case 'MODULE_SET': {
      if (msg.patch === undefined || typeof msg.patch !== 'object') {
        throw new BridgeError('MODULE_SET needs a patch object');
      }
      const lines = [];
      if ('add' in msg.patch) {
        const a = msg.patch.add;
        lines.push(`ins_add ${bit(a?.auth, 'add.auth')} ${bit(a?.contact, 'add.contact')}`);
      }
      if ('present' in msg.patch) {
        if (typeof msg.patch.present !== 'boolean') throw new BridgeError('present must be a boolean');
        lines.push(msg.patch.present ? 'ins_present' : 'ins_absent');
      }
      if ('sensor' in msg.patch) {
        const t = msg.patch.sensor;
        if (typeof t?.name !== 'string' || !INS_SENSORS.has(t.name)) throw new BridgeError(`sensor.name must be one of ${[...INS_SENSORS].join(', ')}`);
        lines.push(`ins_sensor ${t.name} ${bit(t.value, 'sensor.value')}`);
      }
      if ('pd' in msg.patch) {
        const d = msg.patch.pd;
        lines.push(`ins_pd ${int(d?.side, 0, 1, 'pd.side')} ${int(d?.wl, 0, 1, 'pd.wl')} ${int(d?.counts, 0, 65535, 'pd.counts')}`);
      }
      if ('reading' in msg.patch) {
        const r = msg.patch.reading;
        lines.push(`reading 0 ${insOp(r?.op)} ${num(r?.value, -1000000, 1000000, 'reading value')}`);
      }
      if (lines.length === 0) throw new BridgeError('MODULE_SET patch has no recognised field');
      return lines;
    }
    case 'MODULE_FAIL':
      return [`fail 0 ${insOp(msg.op)} ${int(msg.code, -128, 127, 'code')} ${int(msg.times, -1, 1000000, 'times')}${afterTail(msg)}`];
    case 'MODULE_CLEAR':
      return ['ins_clear'];
    default:
      return [];
  }
}

/** FW_COMMANDs for the intranasal driver, and for the T2 and accessory stubs. */
function insCommandLines(msg) {
  const u8 = (v, what) => int(v, 0, 255, what);
  switch (msg.cmd) {
    case 'ins':
      return [`ins ${u8(msg.side, 'side')} ${u8(msg.freq_code, 'freq_code')} ${u8(msg.duty, 'duty')} ${u8(msg.cur_660, 'cur_660')} ${u8(msg.cur_808, 'cur_808')}`];
    case 'ins_init': case 'ins_detect': case 'ins_stop': case 'ins_shutdown': case 'ins_sample':
      return [msg.cmd];                       // a member of the whitelist, never the client's string
    case 'stub':
      if (typeof msg.name !== 'string' || !STUB_NAMES.has(msg.name)) throw new BridgeError(`name must be one of ${[...STUB_NAMES].join(', ')}`);
      if (typeof msg.op !== 'string' || !STUB_OPS.has(msg.op)) throw new BridgeError(`op must be one of ${[...STUB_OPS].join(', ')}`);
      return [`stub ${msg.name} ${msg.op}`];
    default:
      return null;
  }
}

function cvnsOp(v) {
  if (typeof v !== 'string' || !CVNS_OPS.has(v)) {
    throw new BridgeError(`op must be one of ${[...CVNS_OPS].join(', ')}`);
  }
  return v;
}

/** The cervical VNS accessory's messages: one accessory, no address. */
function cvnsModuleLines(msg) {
  switch (msg.type) {
    case 'MODULE_SET': {
      if (msg.patch === undefined || typeof msg.patch !== 'object') {
        throw new BridgeError('MODULE_SET needs a patch object');
      }
      const lines = [];
      if ('add' in msg.patch) {
        const a = msg.patch.add;
        lines.push(`cvns_add ${num(a?.left_kohm, 0, 100000, 'add.left_kohm')} ${num(a?.right_kohm, 0, 100000, 'add.right_kohm')} ${int(a?.unix_base_s, 0, 4294967295, 'add.unix_base_s')}`);
      }
      if ('present' in msg.patch) {
        if (typeof msg.patch.present !== 'boolean') throw new BridgeError('present must be a boolean');
        lines.push(msg.patch.present ? 'cvns_present' : 'cvns_absent');
      }
      if ('reenable_pass' in msg.patch) lines.push(`cvns_reenable_pass ${bit(msg.patch.reenable_pass, 'reenable_pass')}`);
      if ('reading' in msg.patch) {
        const r = msg.patch.reading;
        lines.push(`reading 0 ${cvnsOp(r?.op)} ${num(r?.value, -1000000, 1000000, 'reading value')}`);
      }
      if (lines.length === 0) throw new BridgeError('MODULE_SET patch has no recognised field');
      return lines;
    }
    case 'MODULE_FAIL':
      return [`fail 0 ${cvnsOp(msg.op)} ${int(msg.code, -128, 127, 'code')} ${int(msg.times, -1, 1000000, 'times')}${afterTail(msg)}`];
    case 'MODULE_CLEAR':
      return ['cvns_clear'];
    default:
      return [];
  }
}

/** FW_COMMANDs for the cervical VNS driver; the scenario plays the safety MCU (`cvns_mcu`) and the PPG ISR (`cvns_run`). */
function cvnsCommandLines(msg) {
  switch (msg.cmd) {
    case 'cvns':
      return [`cvns ${int(msg.side, 0, 255, 'side')} ${int(msg.freq_mhz, 0, 65535, 'freq_mhz')} ${int(msg.amp_ua, 0, 65535, 'amp_ua')} ` +
              `${int(msg.pw_us, 0, 65535, 'pw_us')} ${int(msg.ramp_s, 0, 65535, 'ramp_s')} ${int(msg.baseline_req, 0, 255, 'baseline_req')}`];
    case 'cvns_mcu':
      return [`cvns_mcu ${int(msg.granted_mask, 0, 65535, 'granted_mask')} ${int(msg.status, 0, 255, 'status')}`];
    case 'cvns_mcu_imp':
      return [`cvns_mcu_imp ${num(msg.left_kohm, 0, 100000, 'left_kohm')} ${num(msg.right_kohm, 0, 100000, 'right_kohm')}`];
    case 'cvns_run':
      return [`cvns_run ${int(msg.ms, 0, 600000, 'ms')} ${int(msg.bpm, -1, 300, 'bpm')}`];
    case 'cvns_init': case 'cvns_detect': case 'cvns_stop': case 'cvns_suspend': case 'cvns_shutdown': case 'cvns_reenable':
    case 'cvns_sample': case 'cvns_state':
      return [msg.cmd];                       // a member of the whitelist, never the client's string
    default:
      return null;
  }
}

/** The mask of a list of line names; a name that is not one of the ten lines is refused. */
function safetyLineMask(v) {
  if (!Array.isArray(v) || v.length < 1 || v.length > Object.keys(SAFETY_LINES).length) {
    throw new BridgeError(`lines must be a list of 1..${Object.keys(SAFETY_LINES).length} line names`);
  }
  let mask = 0;
  for (const n of v) {
    if (typeof n !== 'string' || !Object.hasOwn(SAFETY_LINES, n)) throw new BridgeError(`line must be one of ${Object.keys(SAFETY_LINES).join(', ')}`);
    mask |= SAFETY_LINES[n];
  }
  return mask;
}

/**
 * FW_COMMANDs for the safety MCU in the loop (NP-SIM-FW-001 §22, §23).  The page plays the two parts the device has other hardware
 * for, the heartbeat task and the app, with a TEST root key; none of it is verification evidence (REQ-SIMFW-06).
 */
function safetyCommandLines(msg) {
  const u8 = (v, what) => int(v, 0, 255, what);
  const ch = (v) => int(v, 0, SAFETY_CHANNELS - 1, 'channel');
  const state = (v) => int(v, 0, 7, 'state');                 // np_session_state_t
  switch (msg.cmd) {
    case 'safety_attach': {
      // The OTP is programmed first, as manufacture does, then the MCU starts.  tier 0 programs no tier record.
      const lines = [`safety_root ${u8(msg.seed, 'seed')}`];
      const tier = int(msg.tier, 0, 2, 'tier');
      if (tier !== 0) lines.push(`safety_uid ${u8(msg.uid, 'uid')}`, `safety_tier ${tier} ${int(msg.hw_version, 1, 15, 'hw_version')}`);
      // A fault of manufacture the page can play: one bit of the programmed key flipped, so it no longer matches its CRC.
      if (msg.corrupt_key !== undefined && bit(msg.corrupt_key, 'corrupt_key')) lines.push('safety_root_corrupt');
      lines.push('safety_attach');
      return lines;
    }
    case 'safety_beat':
      return [`safety_beat ${state(msg.state)}`];
    case 'safety_run': {
      const tail = msg.bpm === undefined ? '' : ` ${int(msg.bpm, 0, 300, 'bpm')}`;
      return [`safety_run ${int(msg.ms, 1, SAFETY_MAX_RUN_MS, 'ms')} ${state(msg.state)}${tail}`];
    }
    case 'safety_sig':
      if (typeof msg.which !== 'string' || !SAFETY_SIG.has(msg.which)) throw new BridgeError(`which must be one of ${[...SAFETY_SIG].join(', ')}`);
      return [`safety_sig ${msg.which}`];
    case 'safety_limits':
      return [`safety_limits ${ch(msg.channel)} ${int(msg.area_mcm2, 0, 65535, 'area_mcm2')}`];
    case 'safety_wave':
      return [`safety_wave ${ch(msg.channel)} ${u8(msg.class, 'class')} ${int(msg.phase_us, 0, 4294967295, 'phase_us')}`];
    case 'safety_enable': case 'safety_disable':
      return [`${msg.cmd} ${safetyLineMask(msg.lines)}`];
    case 'safety_current':
      return [`safety_current ${ch(msg.channel)} ${int(msg.ua, 0, 65535, 'ua')}`];
    case 'safety_adc':
      return [`safety_adc ${int(msg.channel, 0, 5, 'channel')} ${int(msg.counts, 0, 4095, 'counts')}`];
    case 'safety_imp':
      return [`safety_imp ${int(msg.channel, 0, 3, 'channel')} ${int(msg.ohm, 0, 4294967295, 'ohm')}`];
    case 'safety_cvns_el':
      return [`safety_cvns_el ${int(msg.electrode, 0, 1, 'electrode')} ${int(msg.ohm, 0, 4294967295, 'ohm')}`];
    case 'safety_hr':
      return [`safety_hr ${bit(msg.valid, 'valid')} ${int(msg.hr_x10, 0, 65535, 'hr_x10')} ${int(msg.age_ms, 0, 65535, 'age_ms')}`];
    case 'safety_user':
      return [`safety_user ${int(msg.tag, 0, 4294967295, 'tag')}`];
    case 'safety_reenable':
      return [`safety_reenable ${bit(msg.on, 'on')}`];
    case 'safety_geom':
      if (typeof msg.channel !== 'string' || !SAFETY_GEOM.has(msg.channel)) throw new BridgeError(`channel must be one of ${[...SAFETY_GEOM].join(', ')}`);
      return [`safety_geom ${msg.channel} ${bit(msg.on, 'on')}`];
    case 'safety_fault':
      if (typeof msg.kind !== 'string' || !SAFETY_FAULTS.has(msg.kind)) throw new BridgeError(`kind must be one of ${[...SAFETY_FAULTS].join(', ')}`);
      return [`safety_fault ${msg.kind} ${int(msg.n, 0, 1000000, 'n')} ${int(msg.arg ?? 0, 0, 65535, 'arg')}`];
    case 'safety_rpeak': case 'safety_status': case 'safety_root_corrupt':
      return [msg.cmd];                       // a member of the whitelist, never the client's string
    default:
      return null;
  }
}

function stimChannel(v) {
  if (typeof v !== 'string' || !STIM_CHANNELS.has(v)) {
    throw new BridgeError(`channel must be one of ${[...STIM_CHANNELS].join(', ')}`);
  }
  return v;
}

/** The stimulation DAC's messages: an electrode pair in place of a socket (NP-SIM-FW-001 REQ-SIMFW-13). */
function stimModuleLines(msg) {
  const p = int(msg.pair, 0, MAX_SOCKET, 'pair');
  switch (msg.type) {
    case 'MODULE_SET': {
      if (msg.patch === undefined || typeof msg.patch !== 'object') {
        throw new BridgeError('MODULE_SET needs a patch object');
      }
      const lines = [];
      if ('add' in msg.patch) lines.push(`stim_add ${p} ${num(msg.patch.add, 0, 100000, 'add (impedance_kohm)')}`);
      if ('present' in msg.patch) {
        if (typeof msg.patch.present !== 'boolean') throw new BridgeError('present must be a boolean');
        lines.push(`${msg.patch.present ? 'stim_present' : 'stim_absent'} ${p}`);
      }
      if ('reading' in msg.patch) {
        const r = msg.patch.reading;
        lines.push(`reading ${p} ${stimOp(r?.op)} ${num(r?.value, -1000000, 1000000, 'reading value')}`);
      }
      if (lines.length === 0) throw new BridgeError('MODULE_SET patch has no recognised field');
      return lines;
    }
    case 'MODULE_FAIL':
      return [`fail ${p} ${stimOp(msg.op)} ${int(msg.code, -128, 127, 'code')} ${int(msg.times, -1, 1000000, 'times')}${afterTail(msg)}`];
    case 'MODULE_CLEAR':
      return [`stim_clear ${p}`];
    default:
      return [];
  }
}

/** FW_COMMANDs for the stimulation driver. */
function stimCommandLines(msg) {
  const u16 = (v, what) => int(v, 0, 65535, what);
  switch (msg.cmd) {
    case 'bes':
      return [`bes ${int(msg.pair, 0, 255, 'pair')} ${int(msg.waveform, 0, 255, 'waveform')} ${u16(msg.freq_mhz, 'freq_mhz')} ${u16(msg.amp_ua, 'amp_ua')}`];
    case 'tdcs':
      return [`tdcs ${int(msg.pair, 0, 255, 'pair')} ${u16(msg.current_ua, 'current_ua')} ${int(msg.polarity, 0, 255, 'polarity')} ${u16(msg.ramp_s, 'ramp_s')} ${u16(msg.area_mcm2, 'area_mcm2')}`];
    case 'stim_stop':
      return [`stim_stop ${stimChannel(msg.channel)}`];
    case 'stim_shutdown':
      return [`stim_shutdown ${stimChannel(msg.channel)}`];
    case 'sample':
      return [`sample ${stimChannel(msg.channel)}`];
    case 'tdcs_pause':
      return ['tdcs_pause'];
    case 'stim_session_start':
      return ['stim_session_start'];
    case 'advance':
      return [`advance ${int(msg.ms, 0, 3600000, 'ms')}`];
    default:
      return null;
  }
}

function sockets(v) {
  if (!Array.isArray(v) || v.length < 1 || v.length > MAX_SOCKETS_PER_COMMAND) {
    throw new BridgeError(`sockets must be a list of 1..${MAX_SOCKETS_PER_COMMAND} integers`);
  }
  return v.map((s) => int(s, 0, MAX_SOCKET, 'socket')).join(',');
}

/**
 * Turns a client message into firmware command lines.  Throws BridgeError for
 * anything it does not recognise or cannot validate.  Returns [] for a message
 * that is not the bridge's.
 */
function messageToLines(msg) {
  if (msg.kind === 'stim_pair') return stimModuleLines(msg);
  if (msg.kind === 'eeg') return eegModuleLines(msg);
  if (msg.kind === 'vns') return vnsModuleLines(msg);
  if (msg.kind === 'audio') return audioModuleLines(msg);
  if (msg.kind === 'visual') return visModuleLines(msg);
  if (msg.kind === 'intranasal') return insModuleLines(msg);
  if (msg.kind === 'cvns') return cvnsModuleLines(msg);
  if (msg.type === 'FW_COMMAND') {
    const stim = stimCommandLines(msg) ?? eegCommandLines(msg) ?? vnsCommandLines(msg) ?? audioCommandLines(msg) ?? visCommandLines(msg) ?? insCommandLines(msg) ?? cvnsCommandLines(msg) ?? safetyCommandLines(msg);
    if (stim !== null) return stim;
  }
  switch (msg.type) {
    case 'MODULE_SET': {
      const s = int(msg.socket, 0, MAX_SOCKET, 'socket');
      if (msg.patch === undefined || typeof msg.patch !== 'object') {
        throw new BridgeError('MODULE_SET needs a patch object');
      }
      const lines = [];
      if ('add' in msg.patch) {
        lines.push(`add ${s} ${num(msg.patch.add, -50, 200, 'add (ntc_c)')}`);
      }
      if ('present' in msg.patch) {
        if (typeof msg.patch.present !== 'boolean') throw new BridgeError('present must be a boolean');
        lines.push(`${msg.patch.present ? 'present' : 'absent'} ${s}`);
      }
      if ('reading' in msg.patch) {
        const r = msg.patch.reading;
        lines.push(`reading ${s} ${op(r?.op)} ${num(r?.value, -50, 500, 'reading value')}`);
      }
      if (lines.length === 0) throw new BridgeError('MODULE_SET patch has no recognised field');
      return lines;
    }
    case 'MODULE_FAIL': {
      const s = int(msg.socket, 0, MAX_SOCKET, 'socket');
      // codes are np_hub_status_t (-28..0); times < 0 means until cleared
      return [`fail ${s} ${op(msg.op)} ${int(msg.code, -128, 127, 'code')} ${int(msg.times, -1, 1000000, 'times')}${afterTail(msg)}`];
    }
    case 'MODULE_CLEAR':
      return [`clear ${int(msg.socket, 0, MAX_SOCKET, 'socket')}`];
    case 'FW_COMMAND': {
      // spike only: drive the dispatcher so a scenario can run without the session runner
      if (msg.cmd !== 'drive' && msg.cmd !== 'stop' && msg.cmd !== 'status' && msg.cmd !== 'report') {
        throw new BridgeError('cmd must be drive, stop, status or report');
      }
      if (msg.cmd === 'report') {
        // the safety MCU's report of the helmet's hardware version (1..15 on a real helmet; 0 and above are tried as given)
        return [`report ${int(msg.version, 0, 255, 'version')}`];
      }
      return msg.cmd === 'status' ? ['status'] : [`${msg.cmd} ${sockets(msg.sockets)}`];
    }
    default:
      return [];
  }
}

/**
 * Maps one JSON event from the firmware process to the WebSocket message that
 * carries it.  Module identity and state go as MODULE_STATE, everything the
 * firmware or the sim layer says about a call goes as FW_LOG.
 */
function eventToMessage(ev) {
  switch (ev.ev) {
    case 'module':
    case 'module_state':
    case 'inject':
      return { type: 'MODULE_STATE', event: ev };
    case 'safety_state':                       // the safety MCU's status, fault slot and enable lines (NP-SIM-FW-001 §23)
      return { type: 'SAFETY_STATE', event: ev };
    case 'safety_iwdg_reset':                  // the independent watchdog reset the MCU
    case 'safety_pin_unknown':                 // the MCU drove a pin the HAL has no line for
      return { type: 'SAFETY_EVENT', event: ev };
    default:
      return { type: 'FW_LOG', event: ev };
  }
}

class FwBridge extends EventEmitter {
  /**
   * `autoStatus`: milliseconds after a command to ask the firmware for the state of every module (0 = never).  The page learns what a
   * module is doing from `module_state` events, and a command changes state without the firmware volunteering it, so the real server
   * asks.  Tests that count events leave it off.
   */
  constructor({ bin, spawnFn = spawn, autoStatus = 0 } = {}) {
    super();
    this._autoStatus = autoStatus;
    this._statusTimer = null;
    if (typeof bin !== 'string' || bin === '') throw new BridgeError('bin is required');
    this._bin = bin;
    this._spawn = spawnFn;
    this._proc = null;
    this._buf = '';
    this._modules = new Map();   // id -> last module/module_state event, for MODULE_LIST
    this._safety = null;         // the last safety_state event; null while no safety MCU has been attached
    this._safetyRequested = false;   // an attach has been accepted: the OTP is no longer programmable from a client
  }

  start() {
    this._proc = this._spawn(this._bin, [], { stdio: ['pipe', 'pipe', 'inherit'] });
    this._proc.stdout.setEncoding('utf8');
    this._proc.stdout.on('data', (chunk) => this._onData(chunk));
    this._proc.on('exit', (code) => this.emit('exit', code));
    return this;
  }

  stop() {
    if (this._statusTimer !== null) { clearTimeout(this._statusTimer); this._statusTimer = null; }
    if (this._proc) {
      try { this._proc.stdin.write('quit\n'); } catch { /* already gone */ }
      this._proc.kill();
      this._proc = null;
    }
  }

  /** Validates a client message and sends its lines.  Throws BridgeError. */
  handle(msg) {
    if (msg.type === 'FW_COMMAND' && SAFETY_PROGRAMMING.has(msg.cmd) && this._safetyRequested) {
      throw new BridgeError('the safety MCU is already attached: the OTP is programmed before attach only');
    }
    const lines = messageToLines(msg);
    if (msg.type === 'FW_COMMAND' && msg.cmd === 'safety_attach') this._safetyRequested = true;
    for (const l of lines) this._proc.stdin.write(l + '\n');
    if (this._autoStatus > 0 && lines.some((l) => l !== 'status')) this._scheduleStatus();
    return lines.length > 0;
  }

  _scheduleStatus() {
    if (this._statusTimer !== null) return;
    this._statusTimer = setTimeout(() => {
      this._statusTimer = null;
      try { this._proc?.stdin.write('status\n'); } catch { /* the process is gone */ }
    }, this._autoStatus);
  }

  list() {
    return { type: 'MODULE_LIST', modules: [...this._modules.values()], safety: this._safety };
  }

  _onData(chunk) {
    this._buf += chunk;
    let i;
    while ((i = this._buf.indexOf('\n')) >= 0) {
      const line = this._buf.slice(0, i);
      this._buf = this._buf.slice(i + 1);
      let ev;
      try { ev = JSON.parse(line); } catch { continue; }   // not an event
      if (ev.ev === 'module' || ev.ev === 'module_state') this._modules.set(ev.id, ev);
      if (ev.ev === 'safety_state') this._safety = ev;
      this.emit('message', eventToMessage(ev));
    }
  }
}

module.exports = { FwBridge, BridgeError, messageToLines, eventToMessage, SAFETY_LINES };
