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
const STIM_CHANNELS = new Set(['bes', 'tdcs']);                       // the driver's two slots
const MAX_SOCKET = 127;                                       // NP_SIM_MAX_MODULES - 1
const MAX_SOCKETS_PER_COMMAND = 128;                          // the 7-bit socket-address domain (NP_HEXMAP_MAX_SOCKETS)

class BridgeError extends Error {}

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
      return [`fail 0 ${eegOp(msg.op)} ${int(msg.code, -128, 127, 'code')} ${int(msg.times, -1, 1000000, 'times')}`];
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
      return [`fail ${p} ${stimOp(msg.op)} ${int(msg.code, -128, 127, 'code')} ${int(msg.times, -1, 1000000, 'times')}`];
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
  if (msg.type === 'FW_COMMAND') {
    const stim = stimCommandLines(msg) ?? eegCommandLines(msg);
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
      return [`fail ${s} ${op(msg.op)} ${int(msg.code, -128, 127, 'code')} ${int(msg.times, -1, 1000000, 'times')}`];
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
    default:
      return { type: 'FW_LOG', event: ev };
  }
}

class FwBridge extends EventEmitter {
  constructor({ bin, spawnFn = spawn } = {}) {
    super();
    if (typeof bin !== 'string' || bin === '') throw new BridgeError('bin is required');
    this._bin = bin;
    this._spawn = spawnFn;
    this._proc = null;
    this._buf = '';
    this._modules = new Map();   // id -> last module/module_state event, for MODULE_LIST
  }

  start() {
    this._proc = this._spawn(this._bin, [], { stdio: ['pipe', 'pipe', 'inherit'] });
    this._proc.stdout.setEncoding('utf8');
    this._proc.stdout.on('data', (chunk) => this._onData(chunk));
    this._proc.on('exit', (code) => this.emit('exit', code));
    return this;
  }

  stop() {
    if (this._proc) {
      try { this._proc.stdin.write('quit\n'); } catch { /* already gone */ }
      this._proc.kill();
      this._proc = null;
    }
  }

  /** Validates a client message and sends its lines.  Throws BridgeError. */
  handle(msg) {
    const lines = messageToLines(msg);
    for (const l of lines) this._proc.stdin.write(l + '\n');
    return lines.length > 0;
  }

  list() {
    return { type: 'MODULE_LIST', modules: [...this._modules.values()] };
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
      this.emit('message', eventToMessage(ev));
    }
  }
}

module.exports = { FwBridge, BridgeError, messageToLines, eventToMessage };
