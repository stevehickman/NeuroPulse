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

  /**
   * Why the MCU did not grant lines the hub requested, from its withhold report (OI-SIMFW-61): the lines its waveform-declaration gate
   * and its electrode-geometry gate hold off, by name.  `valid` is false when the last reply carried no valid report (then both lists
   * are empty); an event that carries none of the fields, as one from an older process, reads as no report.  The masks are the
   * firmware's, decoded here and nothing more.
   * @returns {{ valid: boolean, declaration: string[], geometry: string[] }}
   */
  get withheld() {
    const s = this.state;
    if (!this.inLoop || !s || s.withhold_valid !== true) return { valid: false, declaration: [], geometry: [] };
    const names = (mask) => LINES.filter((l) => ((mask >>> l.bit) & 1) === 1).map((l) => l.name);
    return { valid: true, declaration: names(s.withhold_decl >>> 0), geometry: names(s.withhold_geom >>> 0) };
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
      withheld: this.withheld,
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
  attach: (tier = TEST_AID.tier, corruptKey = false) => [
    fw('safety_attach', { seed: TEST_AID.seed, tier, uid: TEST_AID.uid, hw_version: TEST_AID.hw_version, ...(corruptKey ? { corrupt_key: true } : {}) }),
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

// ── the fault panel: every fault the system is built to detect, as the page can cause it (NP-SIM-FW-001 §23, REQ-SIMFW-16) ──────────────
//
// A fault here is an input to the firmware, never a verdict: the page sets what the part's pins, its link or its sensors would carry,
// and the safety MCU's own sources decide what follows.  Nothing in this table says what a fault should do, so a recovery that the
// firmware does (a heartbeat loss ends on the first live beat) or does not do (a latched fault) is the firmware's, and the table
// follows it when the firmware changes.  Every entry is a list of FW_COMMANDs the bridge validates.
const FAULT_RUN_MS = 3000;            // long enough for the longest detector to act (the 1.5 s heartbeat watchdog, the 2 s tick check); UC-135
const FAULT_BAD_FRAMES = 10;          // how many heartbeat frames or command frames arrive damaged; UC-135
const FAULT_OPEN_OHM = 1000000;       // a contact that has come away; far outside the impedance window; UC-135
const run = (ms) => fw('safety_run', { ms, state: TEST_AID.runningState });
const NEEDS = Object.freeze({ ANY: 'any', ATTACHED: 'attached', UNATTACHED: 'unattached' });

/**
 * @typedef {{ id: string, group: string, label: string, title: string, needs: string, script: (arg?: any) => object[], clear?: () => object[] }} FaultDef
 * `clear` undoes an injected condition that stays until it is undone (a frozen clock, a stalled loop, a contact left open); a fault
 * that is a one-off event has none.  Whether the MCU then recovers is the firmware's.
 */
export const FAULTS = Object.freeze([
  { id: 'heartbeat_loss', group: 'Link', label: 'Heartbeat lost', needs: NEEDS.ATTACHED,
    title: 'The simulated hub stops sending its 200 ms heartbeat for 2 s of virtual time (not a loss of power to this computer)',
    script: () => SCRIPTS.heartbeatLoss(2000) },
  { id: 'frame_corrupt', group: 'Link', label: 'Damaged heartbeat frames', needs: NEEDS.ATTACHED,
    title: `The next ${FAULT_BAD_FRAMES} heartbeat frames arrive with a damaged magic byte, then the hub keeps beating. Shows under Last cutoff; the MCU recovers when frames are good again`,
    script: () => [fw('safety_fault', { kind: 'spi_flip', n: FAULT_BAD_FRAMES, arg: 0 }), run(FAULT_RUN_MS), fw('status')] },
  { id: 'spi_timeout', group: 'Link', label: 'SPI transfer times out', needs: NEEDS.ATTACHED,
    title: `The next ${FAULT_BAD_FRAMES} SPI transfers time out`,
    script: () => [fw('safety_fault', { kind: 'spi_timeout', n: FAULT_BAD_FRAMES, arg: 0 }), run(FAULT_RUN_MS), fw('status')] },
  { id: 'cmd_corrupt', group: 'Link', label: 'Damaged command frames', needs: NEEDS.ATTACHED,
    title: 'Use before Sign. The next three command frames arrive with their first byte inverted and the app sends the session signature three times; three corrupt frames in a row is what the MCU counts. Once a session is signed this has no effect.',
    script: () => [fw('safety_fault', { kind: 'cmd_flip', n: 3, arg: 0 }), ...SCRIPTS.signSession('good'), ...SCRIPTS.signSession('good'), ...SCRIPTS.signSession('good')] },
  { id: 'bad_signature', group: 'Session', label: 'Bad session signature', needs: NEEDS.ATTACHED,
    title: 'The app signs the session with one bit of the signature flipped', script: () => SCRIPTS.signSession('bad') },
  { id: 'key_corrupt', group: 'Part', label: 'Root key fails its CRC', needs: NEEDS.UNATTACHED,
    title: 'Attaches a part whose programmed root key has one bit flipped; the MCU starts in a fault',
    script: (tier = TEST_AID.tier) => SCRIPTS.attach(tier, true) },
  { id: 'systick', group: 'Part', label: 'SysTick frozen', needs: NEEDS.ATTACHED,
    title: 'The millisecond clock stops while the independent timer runs; the MCU compares the two',
    script: () => [fw('safety_fault', { kind: 'systick', n: 1, arg: 0 }), run(FAULT_RUN_MS), fw('status')],
    clear: () => [fw('safety_fault', { kind: 'systick', n: 0, arg: 0 }), fw('status')] },
  { id: 'stall', group: 'Part', label: 'Main loop stalled', needs: NEEDS.ATTACHED,
    title: 'The MCU\'s main loop stops running; only its independent watchdog can act',
    script: () => [fw('safety_fault', { kind: 'stall', n: 1, arg: 0 }), run(FAULT_RUN_MS), fw('status')],
    clear: () => [fw('safety_fault', { kind: 'stall', n: 0, arg: 0 }), fw('status')] },
  { id: 'nv_fail', group: 'Part', label: 'Flash will not write', needs: NEEDS.ATTACHED,
    title: 'Every flash erase and program fails from now on, so a cutoff cannot be recorded; pair with a thermal or cardiac fault',
    script: () => [fw('safety_fault', { kind: 'nv_erase', n: 100, arg: 0 }), fw('safety_fault', { kind: 'nv_program', n: 100, arg: 0 }), fw('status')],
    clear: () => [fw('safety_fault', { kind: 'nv_erase', n: 0, arg: 0 }), fw('safety_fault', { kind: 'nv_program', n: 0, arg: 0 }), fw('status')] },
  { id: 'thermal', group: 'Sensors', label: 'Cranial NTC too hot', needs: NEEDS.ATTACHED,
    title: 'The cranial NTC reads hotter than the 62 C row of the MCU\'s table', script: () => SCRIPTS.thermal(TEST_AID.hotAdcCounts),
    clear: () => SCRIPTS.thermal(TEST_AID.coldAdcCounts) },
  { id: 'ntc_open', group: 'Sensors', label: 'NTC reads open', needs: NEEDS.ATTACHED,
    title: 'The cranial NTC reads as an open sensor (the HAL\'s unset reading, 4095 counts). In the firmware as built this is not detected: see OI-SIMFW-41', script: () => SCRIPTS.thermal(TEST_AID.coldAdcCounts) },
  { id: 'contact_lost', group: 'Sensors', label: 'Electrode contact lost', needs: NEEDS.ATTACHED,
    title: 'The chosen channel\'s contact impedance goes far outside the window. The MCU checks contact at session start, so use this before the first Beat; later it has no effect (np_impedance_check.c has no re-check)',
    script: (channel = 0) => [...SCRIPTS.contact(channel, FAULT_OPEN_OHM), run(FAULT_RUN_MS), fw('status')],
    clear: (channel = 0) => SCRIPTS.contact(channel, channel === 3 ? TEST_AID.cvnsContactOhm : TEST_AID.contactOhm) },
  { id: 'cvns_electrode', group: 'Sensors', label: 'cVNS electrode off', needs: NEEDS.ATTACHED,
    title: 'Electrode 0 of the cervical VNS pair comes away. Cervical VNS only: set the line up as for Heart rate steps up, with the baseline',
    script: () => [fw('safety_cvns_el', { electrode: 0, ohm: FAULT_OPEN_OHM }), run(FAULT_RUN_MS), fw('status')],
    clear: () => [fw('safety_cvns_el', { electrode: 0, ohm: TEST_AID.cvnsContactOhm }), fw('status')] },
  { id: 'cardiac_step', group: 'Sensors', label: 'Heart rate steps up', needs: NEEDS.ATTACHED,
    title: 'After a steady 70 bpm baseline, a step to 100 bpm. Cervical VNS only: attach a T2 unit, Request cVNS, Beat, Sign, then Prepare on that line first',
    script: () => [...SCRIPTS.cardiacBaseline().slice(0, -1), ...SCRIPTS.cardiacStep()] },
  { id: 'rpeak_stale', group: 'Sensors', label: 'R-peak signal lost', needs: NEEDS.ATTACHED,
    title: 'The heart-rate feed reports no valid beat. Cervical VNS only: set the line up as for Heart rate steps up, with the baseline',
    script: () => [fw('safety_hr', { valid: false, hr_x10: 0, age_ms: 65535 }), run(FAULT_RUN_MS), fw('status')] },
]);
const FAULT_BY_ID = new Map(FAULTS.map((f) => [f.id, f]));
export const faultById = (id) => FAULT_BY_ID.get(id) ?? null;

/** Faults the system is meant to detect that the page cannot cause, and why.  Shown on the panel so a missing button is not read as a pass. */
export const FAULTS_UNAVAILABLE = Object.freeze([
  { label: 'Photoparoxysmal EEG', why: 'The detector is not built (OI-FWHUB-29); the goggles refuse 3-30 Hz photic driving outright (REQ-FWHUB-65).' },
  { label: 'Brown-out or loss of USB-C power', why: 'The MCU has no supply-monitor input in the harness. A real loss of the page\'s own link is mapped to a heartbeat loss instead (below).' },
  { label: 'OTP unprovisioned', why: 'No page command programs an all-zero key.' },
]);

/**
 * A real outage of what the simulated hub stands on: the page's link to the firmware process.  The hub's heartbeat task lives behind
 * that link, so while it is down no heartbeat is sent; the virtual clock moves only when the page commands it, so on reconnect the
 * page advances it by the wall time that was lost, with no heartbeat.  It is a heartbeat loss as the MCU sees it, and the firmware
 * decides what follows.  Capped at one harness call (the bridge's run limit).  Not a loss of power to this computer, which nothing
 * on a page can observe, and which would stop the simulation.
 */
export const OUTAGE_MIN_MS = 1500;     // the MCU's watchdog timeout (NP_SAFETY_WATCHDOG_MS): a shorter outage would not be one; UC-135
export const OUTAGE_MAX_MS = 60000;
export function outageScript(lostAtWallMs, backAtWallMs) {
  const ms = Math.min(OUTAGE_MAX_MS, Math.max(0, Math.round(backAtWallMs - lostAtWallMs)));
  return ms >= OUTAGE_MIN_MS ? SCRIPTS.heartbeatLoss(ms) : null;
}
