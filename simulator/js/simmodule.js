/**
 * SimModule — one simulated physical module (NP-SIM-FW-001 §3.3, §8 step 1).
 *
 * The firmware process owns what a module IS (its state, what it answers); this
 * object mirrors that state in the browser and owns how the module LOOKS.  It
 * draws itself: HelmetModel hands it the socket's mesh each frame and takes
 * nothing back (see HelmetModel.update).
 *
 * It is fed the events the firmware bridge relays (MODULE_STATE, FW_LOG) and
 * invents none: a module is drawn driven only if the firmware's own record says the
 * sim module holds a commanded drive AND, while the safety MCU is in the loop, the
 * MCU has that module's enable line enabled (REQ-SIMFW-07, REQ-SIMFW-15).  With no
 * safety MCU attached the hub's command is all there is, and the page says so (see
 * SimModuleRegistry.banner).
 */
import { SafetyMcu, VERDICT } from './safety.js';

export const FAULT_FLASH_S = 1.5;

/**
 * The page numbers sockets by the socket map's id (1..80, hardware/np_socket_map_v1.json); the
 * firmware's socket_id is a 0-based INDEX (np_socket_dispatch.h, np_hex_addr_t.socket_id).
 * This is the one place the two meet (NP-SIM-FW-001 OI-SIMFW-06): a registry is keyed by
 * firmware index, and the helmet asks for a socket by its lattice id through here.
 */
export const latticeIdToFirmwareIndex = (id) => id - 1;

export class SimModule {
  constructor(id, kind, index) {
    this.id = id;
    this.kind = kind;
    this.index = index;
    this.present = true;
    this.driven = false;
    this.ntcC = null;
    this.curA = 0;
    this.curB = 0;
    this.lastFaultAtS = -Infinity;   // wall time of the last injected non-zero answer
    this.lastFault = null;           // { op, code }
    this.unscripted = new Set();     // ops that have only ever been answered by the default model
    this.detail = {};                // the module's own latest fields (amp_ua, left, right, stage, ...), as the firmware reported them
    this.safety = null;              // the registry's SafetyMcu; null (a module on its own) behaves as "not in the loop"
  }

  /** What the safety MCU says about this module's enable line: a VERDICT. */
  get gate() { return this.safety ? this.safety.gateFor(this.kind, this.detail) : { verdict: VERDICT.NOT_IN_LOOP, line: null }; }

  /**
   * Driven as the page draws it (REQ-SIMFW-07).  The hub commanded it, and, while the safety MCU is in the loop and gates this
   * module, the MCU has granted its line.  A module the MCU does not gate (audio, EEG) is drawn on the hub's command.
   */
  get drawnDriven() {
    const v = this.gate.verdict;
    return this.driven && v !== VERDICT.WITHHELD;
  }

  /** Commanded by the hub and withheld by the MCU's line: the case the page used to draw as driven. */
  get withheld() { return this.driven && this.gate.verdict === VERDICT.WITHHELD; }

  /** Folds one firmware event in.  `nowS` is the page's wall time in seconds. */
  apply(ev, nowS) {
    switch (ev.ev) {
      case 'result':
        if (stateEventKind(ev) === null) break;
        // falls through: a cervical VNS command's result carries the stage, like cvns_state
      case 'cvns_state': {
        // The cervical VNS harness reports its stage in its own event (no id): fold it into the module's detail.
        const { ev: _e, cmd: _c, code: _k, ...rest } = ev;
        this.detail = { ...this.detail, ...rest };
        break;
      }
      case 'module':
      case 'module_state': {
        const { ev: _e, id: _i, kind: _kd, index: _ix, ...rest } = ev;
        this.detail = { ...this.detail, ...rest };
        this.present = !!ev.present;
        if ('driven' in ev) this.driven = !!ev.driven;
        if ('cur_a' in ev) this.curA = ev.cur_a;
        if ('cur_b' in ev) this.curB = ev.cur_b;
        if ('ntc_c' in ev) this.ntcC = ev.ntc_c;
        break;
      }
      case 'hal':
        if (ev.source === 'injected' && ev.code !== 0) {
          this.lastFaultAtS = nowS;
          this.lastFault = { op: ev.op, code: ev.code };
        }
        if (ev.source === 'default') this.unscripted.add(ev.op);
        // a successful write is the module's own record of what it was commanded
        if (ev.op === 'socket_pwm_set' && ev.code === 0 && Array.isArray(ev.args)) {
          this.curA = ev.args[0];
          this.curB = ev.args[1];
          this.driven = ev.args[0] !== 0 || ev.args[1] !== 0;
        }
        break;
      default:
        break;
    }
  }

  /**
   * Draws the module onto its socket mesh.  Sets colour, emissive and intensity
   * completely every frame, so nothing the zone path left behind survives.
   * `mesh.material` needs color/emissive objects with setHex().
   */
  render(mesh, nowS) {
    const m = mesh.material;
    if (!this.present) {
      m.color.setHex(0x050508);
      m.emissive.setHex(0x000000);
      m.emissiveIntensity = 0;
      return;
    }
    m.color.setHex(0x14141f);
    if (nowS - this.lastFaultAtS < FAULT_FLASH_S) {
      // an injected failure answered this tile within the last moment: magenta
      m.emissive.setHex(0xff2d9a);
      m.emissiveIntensity = 1.2;
    } else if (this.drawnDriven) {
      m.emissive.setHex(0xff3b1f);   // 660 nm family, as the zone path draws it
      m.emissiveIntensity = 0.6 + 0.9 * Math.min(1, Math.max(this.curA, this.curB) / 255);
    } else if (this.withheld) {
      // commanded by the hub, line not enabled by the safety MCU: a dim amber, not the red of a module that is delivering
      m.emissive.setHex(0x8a6d00);
      m.emissiveIntensity = 0.25;
    } else {
      m.emissive.setHex(0x334455);
      m.emissiveIntensity = 0.05;
    }
  }
}

/** Tiles are the module kind the page's 3D socket meshes draw; every other kind is an accessory or a fixed part. */
export const TILE_KIND = 'pbm_base';

/**
 * Events that carry a module's state without naming it: they belong to the one module of that kind.  The cervical VNS harness reports
 * its stage in a `cvns_state` event after a run and in the `result` of each command it takes (a stop, a fault), both with a `stage`.
 */
export function stateEventKind(ev) {
  if (ev.ev === 'cvns_state') return 'cvns';
  if (ev.ev === 'result' && typeof ev.cmd === 'string' && ev.cmd.startsWith('cvns') && 'stage' in ev) return 'cvns';
  return null;
}

export class SimModuleRegistry {
  constructor() {
    // Keyed by `${kind}:${index}`: the firmware bus is keyed by (kind, index), so a stimulation pair, the EEG front end and a
    // tile in socket 0 are three modules, not one.
    this._byKey = new Map();
    this.log = [];                    // recent FW_LOG events, newest last
    this.safety = new SafetyMcu();    // the safety MCU as the page knows it; not in the loop until a safety_state says attached
    this.sawFirmware = false;         // the bridge has said anything: the page is talking to a firmware process
  }

  /** The safety MCU is in the loop (the firmware's own `safety_mcu` label). */
  get sawSafetyMcu() { return this.safety.inLoop; }

  bySocket(index) { return this._byKey.get(`${TILE_KIND}:${index}`); }
  byLatticeId(id) { return this.bySocket(latticeIdToFirmwareIndex(id)); }
  /** Every module of a kind, in index order. */
  byKind(kind) {
    return [...this._byKey.values()].filter((m) => m.kind === kind).sort((a, b) => a.index - b.index);
  }
  get size() { return this._byKey.size; }

  /** One bridge message: MODULE_LIST, MODULE_STATE or FW_LOG. */
  onMessage(msg, nowS) {
    if (msg.type === 'MODULE_LIST') {
      this.sawFirmware = true;
      this.safety.restore(msg.safety);
      for (const ev of msg.modules) this._applyTo(ev, nowS);
    } else if (msg.type === 'SAFETY_STATE' || msg.type === 'SAFETY_EVENT') {
      this.sawFirmware = true;
      this.safety.apply(msg.event);
    } else if (msg.type === 'MODULE_STATE' || msg.type === 'FW_LOG') {
      this._applyTo(msg.event, nowS);
      if (msg.type === 'FW_LOG') {
        this.log.push(msg.event);
        if (this.log.length > 200) this.log.shift();
      }
    }
  }

  _applyTo(ev, nowS) {
    const stateKind = stateEventKind(ev);
    const parsed = stateKind !== null ? { kind: stateKind, index: 0 } : this._identify(ev);
    if (parsed === null) return;
    const key = `${parsed.kind}:${parsed.index}`;
    let mod = this._byKey.get(key);
    if (!mod && (ev.ev === 'module' || ev.ev === 'module_state')) {
      mod = new SimModule(ev.id, parsed.kind, parsed.index);
      mod.safety = this.safety;
      this._byKey.set(key, mod);
    }
    if (mod) mod.apply(ev, nowS);
  }

  /** The (kind, index) an event names: from its id ("stim_pair:0"), with the index and kind fields taking precedence. */
  _identify(ev) {
    const m = /^([a-z_0-9]+):(\d+)$/.exec(String(ev.id ?? ''));
    const kind = typeof ev.kind === 'string' ? ev.kind : (m ? m[1] : null);
    const index = typeof ev.index === 'number' ? ev.index : (m ? Number(m[2]) : null);
    return kind === null || index === null ? null : { kind, index };
  }

  /**
   * What the page must say while the safety MCU is not in the loop (REQ-SIMFW-07): the note is visible whenever a module is attached
   * and no MCU is.  With the MCU in the loop the note says what the drawing now follows.
   */
  banner() {
    if (this.size === 0) return '';
    return this.safety.inLoop
      ? 'Safety MCU in the loop: a module is shown driven only while the MCU has its enable line enabled. Timing is virtual.'
      : 'Safety MCU not in loop: a module is shown driven when the hub commanded it, not when a safety MCU would have allowed it. '
        + 'Timing is indicative.';
  }
}
