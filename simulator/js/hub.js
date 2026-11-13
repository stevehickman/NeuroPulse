/**
 * The hub's own tasks, as the page knows them (NP-SIM-FW-001 §31, REQ-SIMFW-23).
 *
 * `HubModel` keeps the last `hub_state` the firmware process reported: the runner's state, session time, the part and the modalities
 * active, the status LED and the hold cause.  All of it is the runner's own progress frame (SWR-SS-07), decoded in the firmware; nothing is
 * computed here but a label.  The page does not start, stop or pace a session by any command of its own: a session starts by an upload to
 * the hub's transport and ends by the app's writes to the hub's characteristics, and the test aid below plays only the app's writes and the
 * hardware the device has other parts for (the safety MCU's OTP, the contacts).
 *
 * `HUB_SCRIPTS` build bridge messages, and every one of them passes the bridge's own validation (tested).
 */

/** np_hub_mod_type_t (np_hub_types.h): the bit n of a progress frame's active mask names the modality of type n. */
export const MODALITY_BITS = Object.freeze({
  1: 'PBM base', 2: 'PBM smart', 3: 'Intranasal', 4: 'EEG', 5: 'BES / tACS', 6: 'tDCS', 7: 'VNS', 8: 'Audio', 9: 'Visual', 10: 'Cervical VNS',
  11: 'qEEG (T2)', 12: 'TMS (T2)', 13: 'Deep PBM (T2)', 14: 'Clinical tACS (T2)', 15: 'HD-tDCS (T2)', 16: 'Vibrotactile',
});

/** The GATT ids of the hub's own characteristics (np_app_wire_constants.h): the stop and the pause write. */
export const GATT = Object.freeze({ stop: 15, pause: 26, progress: 25, uploadCheck: 24 });

/** The values the app writes to them (np_session_stop.h). */
export const WRITE = Object.freeze({ stop: '01', pause: '01', resume: '02', panic: '03' });

/** How far one tick of the page's clock moves virtual time, and how often the page asks: one virtual ms per wall ms, the page's own pacing (UC-179). */
export const TICK_MS = 250;

const fw = (cmd, rest = {}) => ({ type: 'FW_COMMAND', cmd, ...rest });

export const HUB_SCRIPTS = Object.freeze({
  /** A powered-on helmet: the safety MCU provisioned (the TEST root key, a T1 record) with every contact in contact, then the hub's own boot. */
  boot: (mode = 'admit') => [
    fw('safety_attach', { seed: 7, tier: 1, uid: 9, hw_version: 1 }),
    fw('safety_imp', { channel: 0, ohm: 5000 }), fw('safety_imp', { channel: 1, ohm: 5000 }),
    fw('safety_imp', { channel: 2, ohm: 5000 }), fw('safety_imp', { channel: 3, ohm: 5000 }),
    fw('hub_boot', { mode }),
    fw('hub_status'),
  ],
  tick: (ms = TICK_MS) => [fw('hub_run', { ms })],
  /** The app's own writes to the hub's characteristics. */
  pause:  () => [fw('gatt_write', { id: GATT.pause, hex: WRITE.pause })],
  resume: () => [fw('gatt_write', { id: GATT.pause, hex: WRITE.resume })],
  stop:   () => [fw('gatt_write', { id: GATT.stop, hex: WRITE.stop })],
  /** The session signature step, which the hub's own code never sends (OI-SIMFW-74): the test aid plays it with the TEST root key. */
  sign: () => [fw('safety_sig', { which: 'good' })],
});

export class HubModel {
  constructor() {
    this.state = null;      // the last hub_state event
    this.status = null;     // the last hub_status event
    this.tasks = new Map();
  }

  /** The hub's tasks have run: a hub_state has been seen. */
  get running() { return this.state !== null; }

  get runner() { return this.state?.runner ?? 'not running'; }

  restore(ev) { if (ev) this.state = ev; }

  apply(ev) {
    if (ev.ev === 'hub_state') this.state = ev;
    else if (ev.ev === 'hub_status') { this.status = ev; this.tasks.clear(); }
    else if (ev.ev === 'hub_task') this.tasks.set(ev.name, ev);
    else if (ev.ev === 'hub_led' && this.state) this.state = { ...this.state, led: ev.led };
  }

  onMessage(msg) {
    if (msg.type === 'MODULE_LIST') this.restore(msg.hub);
    else if (msg.type === 'HUB_STATE') this.apply(msg.event);
  }

  /** The modalities the runner says are active now, by name. */
  activeModalities() {
    const mask = this.state?.active_mask ?? 0;
    const out = [];
    for (const [bit, name] of Object.entries(MODALITY_BITS)) if ((mask >> Number(bit)) & 1 && !out.includes(name)) out.push(name);
    return out;
  }

  /** Fraction of the session the runner reports done, or null when it reports no length. */
  progress() {
    const s = this.state;
    if (!s || !s.total_ms) return null;
    return Math.max(0, Math.min(1, s.elapsed_ms / s.total_ms));
  }

  /** Whether a pause, a resume or a stop would be accepted by the runner now (the hub refuses it otherwise; this only labels the buttons). */
  canPause() { return this.runner === 'running'; }
  canResume() { return this.runner === 'paused'; }
  canStop() { return this.runner === 'running' || this.runner === 'paused'; }
}
