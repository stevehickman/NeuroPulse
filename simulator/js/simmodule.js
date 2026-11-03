/**
 * SimModule — one simulated physical module (NP-SIM-FW-001 §3.3, §8 step 1).
 *
 * The firmware process owns what a module IS (its state, what it answers); this
 * object mirrors that state in the browser and owns how the module LOOKS.  It
 * draws itself: HelmetModel hands it the socket's mesh each frame and takes
 * nothing back (see HelmetModel.update).
 *
 * It is fed the events the firmware bridge relays (MODULE_STATE, FW_LOG) and
 * invents none: a tile is drawn driven only if the firmware's own record says the
 * sim module holds a commanded drive.  REQ-SIMFW-07 (also requiring the safety
 * MCU's enable to be asserted) is not met by this spike: no safety MCU is in the
 * loop, and the page says so (see SimModuleRegistry.banner).
 */

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
  }

  /** Folds one firmware event in.  `nowS` is the page's wall time in seconds. */
  apply(ev, nowS) {
    switch (ev.ev) {
      case 'module':
      case 'module_state':
        this.present = !!ev.present;
        if ('driven' in ev) this.driven = !!ev.driven;
        if ('cur_a' in ev) this.curA = ev.cur_a;
        if ('cur_b' in ev) this.curB = ev.cur_b;
        if ('ntc_c' in ev) this.ntcC = ev.ntc_c;
        break;
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
    } else if (this.driven) {
      m.emissive.setHex(0xff3b1f);   // 660 nm family, as the zone path draws it
      m.emissiveIntensity = 0.6 + 0.9 * Math.min(1, Math.max(this.curA, this.curB) / 255);
    } else {
      m.emissive.setHex(0x334455);
      m.emissiveIntensity = 0.05;
    }
  }
}

export class SimModuleRegistry {
  constructor() {
    this._byIndex = new Map();
    this.log = [];                    // recent FW_LOG events, newest last
    this.sawSafetyMcu = false;        // no safety MCU is in the spike's loop
  }

  bySocket(index) { return this._byIndex.get(index); }
  byLatticeId(id) { return this._byIndex.get(latticeIdToFirmwareIndex(id)); }
  get size() { return this._byIndex.size; }

  /** One bridge message: MODULE_LIST, MODULE_STATE or FW_LOG. */
  onMessage(msg, nowS) {
    if (msg.type === 'MODULE_LIST') {
      for (const ev of msg.modules) this._applyTo(ev, nowS);
    } else if (msg.type === 'MODULE_STATE' || msg.type === 'FW_LOG') {
      this._applyTo(msg.event, nowS);
      if (msg.type === 'FW_LOG') {
        this.log.push(msg.event);
        if (this.log.length > 200) this.log.shift();
      }
    }
  }

  _applyTo(ev, nowS) {
    const index = typeof ev.index === 'number' ? ev.index : this._indexFromId(ev.id);
    if (index === null) return;
    let mod = this._byIndex.get(index);
    if (!mod && (ev.ev === 'module' || ev.ev === 'module_state')) {
      mod = new SimModule(ev.id, ev.kind ?? String(ev.id).split(':')[0], index);
      this._byIndex.set(index, mod);
    }
    if (mod) mod.apply(ev, nowS);
  }

  _indexFromId(id) {
    const m = /^[a-z_0-9]+:(\d+)$/.exec(String(id ?? ''));
    return m ? Number(m[1]) : null;
  }

  /** What the page must say while the safety MCU is not in the loop (REQ-SIMFW-07). */
  banner() {
    return this.size === 0
      ? ''
      : 'Firmware in the loop: hub only. No safety MCU is running, so a tile is shown driven '
        + 'when the hub commanded it, not when a safety MCU would have allowed it. Timing is indicative.';
  }
}
