/**
 * The safety MCU as the page knows it (NP-SIM-FW-001 §23, REQ-SIMFW-07, REQ-SIMFW-15).
 *
 * The firmware process runs the device's own safety MCU sources (§22).  This file holds what the page does with that MCU's events:
 * it keeps the last `safety_state`, says whether the MCU is in the loop at all, decodes the status and fault slot, and gives the one
 * rule the drawing follows: a module is drawn as driven only while the MCU has its enable line enabled, when the MCU is in the loop.
 * It invents nothing: every field is the firmware's, and a line is "enabled" because the MCU's own GPIO model says it is LOW.
 *
 * Pure functions and one small class, no DOM and no three.js, so node can test them.
 */

/**
 * The ten stimulation enable lines, in the order of np_sim_safety_line_name() (the bit index of a `safety_state` event's `lines`),
 * with the enable bit the hub requests and the MCU grants (NP_SAFETY_EN_*, firmware/common/include/np_spi_wire_types.h).
 */
export const LINES = Object.freeze([
  { index: 0, name: 'pbm_cranial', label: 'PBM cranial',    bit: 0 },
  { index: 1, name: 'bes',         label: 'BES / tACS',     bit: 5 },
  { index: 2, name: 'tdcs',        label: 'tDCS',           bit: 6 },
  { index: 3, name: 'vns',         label: 'Auricular VNS',  bit: 7 },
  { index: 4, name: 'visual',      label: 'Visual',         bit: 8 },
  { index: 5, name: 'intranasal',  label: 'Intranasal',     bit: 9 },
  { index: 6, name: 'cvns',        label: 'Cervical VNS',   bit: 10 },
  { index: 7, name: 'tms',         label: 'TMS',            bit: 11 },
  { index: 8, name: 'pbm_1170',    label: 'PBM 1170 nm',    bit: 12 },
  { index: 9, name: 'clin_stim',   label: 'Clinical stim',  bit: 13 },
]);
const LINE_BY_NAME = new Map(LINES.map((l) => [l.name, l]));
const LINE_BY_BIT = new Map(LINES.map((l) => [l.bit, l]));

/**
 * Which line gates a module (what the module's driver requests with np_safety_spi_request_enable).  null: the driver requests none,
 * so the safety MCU does not gate it (the audio path and the EEG front end, np_hub_config.h NP_SAFETY_EN_AUDIO 0).  A stimulation
 * pair is on the BES / tACS line or the tDCS line by its channel, which its own state says (`dc`).
 */
export function lineNameFor(kind, detail = {}) {
  switch (kind) {
    case 'pbm_base': return 'pbm_cranial';
    case 'stim_pair': return detail.dc === true ? 'tdcs' : 'bes';
    case 'vns': return 'vns';
    case 'visual': return 'visual';
    case 'intranasal': return 'intranasal';
    case 'cvns': return 'cvns';
    default: return null;                       // audio, eeg
  }
}

/** NP_SAFETY_STATUS_* (firmware/common/include/np_spi_wire_types.h). */
export const STATUS_BITS = Object.freeze([
  { bit: 0x01, name: 'FAULT',       cutoff: true },
  { bit: 0x02, name: 'WATCHDOG',    cutoff: true },
  { bit: 0x04, name: 'CUTOFF',      cutoff: true },
  { bit: 0x08, name: 'IMPEDANCE',   cutoff: false },   // a contact check failed: a line is withheld, not cut
  { bit: 0x10, name: 'THERMAL',     cutoff: true },
  { bit: 0x20, name: 'CHARGE',      cutoff: true },
  { bit: 0x40, name: 'CARDIAC',     cutoff: true },
  { bit: 0x80, name: 'SIG_PENDING', cutoff: false },   // awaiting the session signature
]);

export function decodeStatus(status) {
  return STATUS_BITS.filter((b) => (status & b.bit) !== 0).map((b) => b.name);
}

/** Slots the simulator's scenarios name (docs/np_sim_fw_001.md §22). Any other slot below 14 is a channel (a bit position of an enable). */
const SLOT_NAMES = Object.freeze({
  0xFF: 'none', 0xFE: 'OTP unprovisioned', 0xFD: 'bad session signature', 0xF9: 'SysTick frozen against TIM2',
  0xF8: 'root key fails its CRC', 0xFA: 'flash cannot record a cutoff',
});

export function describeSlot(slot) {
  if (slot === undefined || slot === null) return 'none';
  if (SLOT_NAMES[slot]) return SLOT_NAMES[slot];
  const line = LINE_BY_BIT.get(slot);
  if (line) return `channel ${slot} (${line.label})`;
  return `slot 0x${slot.toString(16).toUpperCase()}`;
}

/** What the drawing needs to know about a module's gate. */
export const VERDICT = Object.freeze({
  NOT_IN_LOOP: 'not_in_loop',   // no safety MCU: the hub's command is all there is
  UNGATED: 'ungated',           // the MCU is in the loop and the module's driver requests no enable
  GRANTED: 'granted',           // the MCU's line for this module is enabled
  WITHHELD: 'withheld',         // the MCU's line for this module is not enabled
});

export class SafetyMcu {
  constructor() {
    this.state = null;          // the last safety_state event (the firmware's own fields)
    this.lastBeatT = null;      // `t` of the last heartbeat command that the hub's link completed
    this.lastCutoff = null;     // { t, flags, slot, lines, pending } — see _track
    this.events = [];           // recent SAFETY_EVENTs (independent watchdog resets), newest last
    this._prev = null;
  }

  /** In the loop once a safety_state says the MCU is attached.  The bridge sends nothing before an attach, so the default is "not". */
  get inLoop() { return !!this.state && this.state.attached === true; }

  /** The firmware's own label (REQ-SIMFW-07): `in_loop` or `not_in_loop`. */
  get label() { return this.inLoop ? 'in_loop' : 'not_in_loop'; }

  /** Folds one event in: a `safety_state`, or a SAFETY_EVENT. */
  apply(ev) {
    if (ev.ev === 'safety_state') {
      this._track(ev);
      this.state = ev;
    } else if (ev.ev === 'safety_iwdg_reset' || ev.ev === 'safety_pin_unknown') {
      this.events.push(ev);
      if (this.events.length > 50) this.events.shift();
    }
  }

  /** A MODULE_LIST carries the last state a new page missed. */
  restore(ev) { if (ev && ev.ev === 'safety_state' && this.state === null) { this.state = ev; this._prev = ev; } }

  lineEnabled(name) {
    const l = LINE_BY_NAME.get(name);
    return !!l && this.inLoop && ((this.state.lines >>> l.index) & 1) === 1;
  }
  lineRequested(name) {
    const l = LINE_BY_NAME.get(name);
    return !!l && this.inLoop && ((this.state.requested >>> l.bit) & 1) === 1;
  }
  lineGranted(name) {
    const l = LINE_BY_NAME.get(name);
    return !!l && this.inLoop && ((this.state.granted >>> l.bit) & 1) === 1;
  }

  /**
   * The gate of one module.  Without the MCU the verdict is NOT_IN_LOOP and the page falls back to the hub's command, saying so.
   * @returns {{ verdict: string, line: string|null }}
   */
  gateFor(kind, detail) {
    if (!this.inLoop) return { verdict: VERDICT.NOT_IN_LOOP, line: null };
    const line = lineNameFor(kind, detail);
    if (line === null) return { verdict: VERDICT.UNGATED, line: null };
    return { verdict: this.lineEnabled(line) ? VERDICT.GRANTED : VERDICT.WITHHELD, line };
  }

  /** Virtual milliseconds since the end of the last heartbeat run, by the firmware's clock; null if none has been sent. */
  heartbeatAgeMs() {
    if (!this.inLoop || this.lastBeatT === null) return null;
    return (this.state.t - this.lastBeatT) >>> 0;
  }

  /** The independent watchdog: held in reset, and how many times it has reset the part. */
  get watchdog() {
    const s = this.state;
    return { heldInReset: !!s && s.held_in_reset === true, resets: s ? s.iwdg_resets : 0, flagInLastReply: !!s && (s.status & 0x02) !== 0 };
  }

  /** Everything the panel prints, from the state alone. */
  summary() {
    if (!this.inLoop) return { inLoop: false, label: 'not_in_loop' };
    const s = this.state;
    return {
      inLoop: true,
      label: 'in_loop',
      status: decodeStatus(s.status),
      slot: describeSlot(s.fault_slot),
      tier: s.tier,
      lines: LINES.map((l) => ({
        name: l.name, label: l.label,
        requested: this.lineRequested(l.name), granted: this.lineGranted(l.name), enabled: this.lineEnabled(l.name),
      })),
      heartbeatAgeMs: this.heartbeatAgeMs(),
      watchdog: this.watchdog,
      lastCutoff: this.lastCutoff,
      spi: { overruns: s.spi_overruns, discarded: s.spi_discarded },
      t: s.t,
    };
  }

  /**
   * Notes the last cutoff.  A cutoff is a status flag that rises (FAULT, WATCHDOG, CUTOFF, THERMAL, CHARGE, CARDIAC), the independent
   * watchdog holding the MCU in reset, or an enable line dropping while the hub still requests it.  After a heartbeat loss the line
   * drops before any reply can say why (the reply travels with the next heartbeat), so that cutoff is first `pending` and takes its
   * flags from the first reply that carries any.
   */
  _track(ev) {
    const prev = this._prev;
    if (ev.cmd === 'safety_beat' || ev.cmd === 'safety_run') { if (ev.code === 0) this.lastBeatT = ev.t; }
    const cutBits = STATUS_BITS.filter((b) => b.cutoff).reduce((m, b) => m | b.bit, 0);
    const newFlags = (ev.status & cutBits) & ~(prev ? prev.status & cutBits : 0);
    const dropped = LINES.filter((l) => prev && ((prev.lines >>> l.index) & 1) === 1 && ((ev.lines >>> l.index) & 1) === 0);
    const droppedWhileRequested = dropped.filter((l) => ((ev.requested >>> l.bit) & 1) === 1);
    const reset = ev.held_in_reset === true && !(prev && prev.held_in_reset === true);
    const flagNames = decodeStatus(ev.status & cutBits);
    if (reset) {
      this.lastCutoff = { t: ev.t, flags: ['INDEPENDENT_WATCHDOG'], slot: describeSlot(ev.fault_slot), lines: dropped.map((l) => l.name), pending: false };
    } else if (newFlags !== 0) {
      this.lastCutoff = { t: ev.t, flags: flagNames, slot: describeSlot(ev.fault_slot), lines: dropped.map((l) => l.name), pending: false };
    } else if (droppedWhileRequested.length > 0) {
      this.lastCutoff = { t: ev.t, flags: flagNames, slot: describeSlot(ev.fault_slot), lines: droppedWhileRequested.map((l) => l.name), pending: flagNames.length === 0 };
    } else if (this.lastCutoff && this.lastCutoff.pending && flagNames.length > 0) {
      this.lastCutoff = { ...this.lastCutoff, flags: flagNames, slot: describeSlot(ev.fault_slot), pending: false };
    }
    this._prev = ev;
  }
}

// ── the page's part of the protocol: the heartbeat task and the app ────────────────────────────────────────────────────────────────
//
// Every script is a list of FW_COMMAND messages the page sends in order.  None of it is the device's: the heartbeat task and the app
// are other hardware, played here with the TEST root key the harness programs (seed 7), and a run of it is not verification evidence
// (REQ-SIMFW-06).  The numbers are the harness's own (np_sim_safety_tests.c), listed in unjustified-choices.md.

export const TEST_AID = Object.freeze({
  seed: 7, tier: 1, uid: 9, hw_version: 1,        // the scenarios' TEST root key seed and tier record (REQ-SIMFW-08)
  runningState: 3,                                // NP_SESSION_RUNNING
  contactOhm: 5000,                               // a contact inside the impedance window, as the scenarios set it
  hotAdcCounts: 600,                              // hotter than the 62 C row of the MCU's NTC table
  coldAdcCounts: 4095,                            // the HAL's unset reading: a cold, open sensor
  baselineBpm: 70, stepBpm: 100,                  // the cardiac scenarios' rates
  baselineSeconds: 30,                            // the cardiac scenarios' steady 70 bpm before the step
  stepSeconds: 3,                                 // the step is cut within the next second's heartbeat; three is margin
  cvnsContactOhm: 2000, cvnsChannel: 10, cvnsMicroamps: 1000, cvnsPhaseUs: 200,
  chargeBalancedClass: 2, dcClass: 1,
});

const fw = (cmd, rest = {}) => ({ type: 'FW_COMMAND', cmd, ...rest });

// What each module needs to be placed, driven and stopped.  Values are the scenarios' own (UC-128); the visual command is EMDR because the
// goggles refuse photic driving at 3-30 Hz until the detector exists (REQ-FWHUB-65).  Modules are keyed as the registry keys them.
const HUB_MODULES = Object.freeze({
  pbm: { label: 'PBM tile (socket 1)',
    place: [{ type: 'MODULE_SET', socket: 0, patch: { add: 30 } }], on: [fw('drive', { sockets: [0] })], off: [fw('stop', { sockets: [0] })] },
  eeg: { label: 'EEG front end',
    place: [{ type: 'MODULE_SET', kind: 'eeg', patch: { add: {} } }, fw('eeg_init')],
    on: [fw('eeg', { channel_mask: 255, gain: 6, notch: 1, ref_mode: 0, adaptive_out: 0 })], off: [fw('eeg_stop')] },
  vns: { label: 'Auricular VNS clip',
    place: [{ type: 'MODULE_SET', kind: 'vns', patch: { add: 5000 } }, fw('vns_init')],
    on: [fw('vns', { side: 0, freq_mhz: 25000, amp_ua: 2000, pw_us: 250, ppg: 1, eeg_ref: 1, hrv_proto: 0 })], off: [fw('vns_stop')] },
  audio: { label: 'Audio (planar + bone)',
    place: [{ type: 'MODULE_SET', kind: 'audio', patch: { add: 1000 } }, fw('audio_init')],
    on: [fw('audio', { mode: 0, carrier_hz: 200, beat_mhz: 10000, level_half_db: 80, bone: 0, adaptive: 0 })], off: [fw('audio_stop')] },
  visual: { label: 'Visual goggles (EMDR)',
    place: [{ type: 'MODULE_SET', kind: 'visual', patch: { add: { seated: true, eye_open: true, mpe_ok: true } } }, fw('vis_init')],
    on: [fw('vis', { mode: 1, freq_hz: 0, duty_pct: 50, wl: 0, zone_lo: 63, zone_hi: 0, emdr_mhz: 100, mode_f: 0, shade: 0 })], off: [fw('vis_stop')] },
  intranasal: { label: 'Intranasal probe',
    place: [{ type: 'MODULE_SET', kind: 'intranasal', patch: { add: { auth: true, contact: true } } }, fw('ins_init')],
    on: [fw('ins', { side: 0, freq_code: 1, duty: 20, cur_660: 10, cur_808: 10 })], off: [fw('ins_stop')] },
  cvns: { label: 'Cervical VNS (T2)',
    place: [{ type: 'MODULE_SET', kind: 'cvns', patch: { add: { left_kohm: 2, right_kohm: 2, unix_base_s: 0 } } }, fw('cvns_init')],
    on: [fw('cvns', { side: 0, freq_mhz: 10000, amp_ua: 1000, pw_us: 250, ramp_s: 10, baseline_req: 1 })], off: [fw('cvns_stop')] },
});
export const HUB_MODULE_LIST = Object.freeze(Object.entries(HUB_MODULES).map(([id, m]) => ({ id, label: m.label })));

export const SCRIPTS = {
  // Attach also puts every contact in contact: a contact check that fails at session start does not recover in the session
  // (np_impedance_check.c has no auto-retry), so the contacts are set before the first heartbeat, as the scenarios do.
  attach: (tier = TEST_AID.tier) => [
    fw('safety_attach', { seed: TEST_AID.seed, tier, uid: TEST_AID.uid, hw_version: TEST_AID.hw_version }),
    fw('safety_imp', { channel: 0, ohm: TEST_AID.contactOhm }), fw('safety_imp', { channel: 1, ohm: TEST_AID.contactOhm }),
    fw('safety_imp', { channel: 2, ohm: TEST_AID.contactOhm }), fw('safety_imp', { channel: 3, ohm: TEST_AID.cvnsContactOhm }),
    fw('safety_cvns_el', { electrode: 0, ohm: TEST_AID.cvnsContactOhm }), fw('safety_cvns_el', { electrode: 1, ohm: TEST_AID.cvnsContactOhm }),
    fw('status')],
  /**
   * What a line needs before the MCU grants it, from the scenarios (np_sim_safety_tests.c): contact, a declared waveform and electrode
   * area for an electrical channel, and the enable request the driver makes.  null: the page does not know what the line needs.
   */
  prepareLine: (line) => {
    switch (line) {
      case 'bes':  return [
        fw('safety_wave', { channel: 5, class: TEST_AID.chargeBalancedClass, phase_us: 20000 }), fw('safety_limits', { channel: 5, area_mcm2: 1000 }),
        fw('safety_enable', { lines: ['bes'] }), fw('status')];
      case 'tdcs': return [
        fw('safety_wave', { channel: 6, class: TEST_AID.dcClass, phase_us: 0 }), fw('safety_limits', { channel: 6, area_mcm2: 25000 }),
        fw('safety_enable', { lines: ['tdcs'] }), fw('status')];
      case 'cvns': return [fw('safety_wave', { channel: TEST_AID.cvnsChannel, class: TEST_AID.chargeBalancedClass, phase_us: TEST_AID.cvnsPhaseUs }), fw('status')];
      case 'pbm_cranial': case 'visual': case 'intranasal': return [fw('safety_enable', { lines: [line] }), fw('status')];
      default: return null;
    }
  },
  /** The hub's own command for a stimulation pair, as a modality driver would send it (the page plays a controller). */
  hubStim: (channel) => channel === 'tdcs'
    ? [{ type: 'MODULE_SET', kind: 'stim_pair', pair: 0, patch: { add: 5 } }, fw('tdcs', { pair: 0, current_ua: 1000, polarity: 0, ramp_s: 30, area_mcm2: 25000 }), fw('status')]
    : [{ type: 'MODULE_SET', kind: 'stim_pair', pair: 0, patch: { add: 5 } }, fw('bes', { pair: 0, waveform: 0, freq_mhz: 10000, amp_ua: 500 }), fw('status')],
  /**
   * The hub's own commands for every other module, as a controller or the app would send them (NP-SIM-FW-001 §23, OI-SIMFW-28).  `place`
   * is the user putting the part on the helmet (the simulated hardware); `on` and `off` are the driver commands.  A PBM drive is refused
   * by the production power governor (OI-SIMFW-11) and the page shows the refusal, as the device would.  null: not a module of the table.
   */
  hubModule: (kind, phase) => {
    const t = HUB_MODULES[kind];
    if (!t) return null;
    const out = phase === 'place' ? t.place : phase === 'on' ? t.on : t.off;
    return [...out, fw('status')];
  },
  hubStimStop: (channel) => [fw('stim_stop', { channel }), fw('status')],
  heartbeat: (ms) => [fw('safety_run', { ms, state: TEST_AID.runningState }), fw('status')],
  heartbeatLoss: (ms) => [fw('advance', { ms }), fw('status')],
  signSession: (which) => [fw('safety_sig', { which }), fw('safety_run', { ms: 400, state: TEST_AID.runningState }), fw('status')],
  request: (line) => [fw('safety_enable', { lines: [line] }), fw('status')],
  release: (line) => [fw('safety_disable', { lines: [line] }), fw('status')],
  contact: (channel, ohm) => [fw('safety_imp', { channel, ohm }), fw('status')],
  declare: (channel, cls, phaseUs, areaMcm2) => [
    fw('safety_wave', { channel, class: cls, phase_us: phaseUs }), fw('safety_limits', { channel, area_mcm2: areaMcm2 }), fw('status')],
  thermal: (counts) => [fw('safety_adc', { channel: 0, counts }), fw('safety_run', { ms: 400, state: TEST_AID.runningState }), fw('status')],
  cardiacBaseline: () => {
    const out = [];
    for (let i = 0; i < TEST_AID.baselineSeconds; i++) {
      out.push(fw('safety_hr', { valid: true, hr_x10: TEST_AID.baselineBpm * 10, age_ms: 0 }));
      out.push(fw('safety_run', { ms: 1000, state: TEST_AID.runningState, bpm: TEST_AID.baselineBpm }));
    }
    return [...out, fw('status')];
  },
  cardiacStep: () => {
    const out = [];
    for (let i = 0; i < TEST_AID.stepSeconds; i++) {
      out.push(fw('safety_hr', { valid: true, hr_x10: TEST_AID.stepBpm * 10, age_ms: 0 }));
      out.push(fw('safety_run', { ms: 1000, state: TEST_AID.runningState, bpm: TEST_AID.stepBpm }));
    }
    return [...out, fw('status')];
  },
  // What the scenarios give the cervical VNS line before they run the baseline: the contact check on the hub's side, both
  // electrodes in contact, the enable requested (the contact check runs at session start), a commanded current, a declared waveform.
  // What the scenarios do before the first heartbeat: request the line (so the contact check runs at session start) and command a current.
  // The waveform is declared after the session starts, with Prepare on the cervical VNS line.
  cvnsPrepare: () => [fw('safety_enable', { lines: ['cvns'] }), fw('safety_current', { channel: TEST_AID.cvnsChannel, ua: TEST_AID.cvnsMicroamps }), fw('status')],
};
