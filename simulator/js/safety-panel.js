/**
 * SafetyPanel — the safety MCU's state and a small control panel (NP-SIM-FW-001 §23, REQ-SIMFW-15).
 *
 * Two parts, kept apart on the page:
 *   - the STATE: attached or not, each enable line (requested by the hub, granted by the MCU, driven), the status flags and fault
 *     slot, the last cutoff and the watchdog; everything is the firmware's own `safety_state`, nothing is computed here but a label;
 *   - the TEST AID: buttons that play the two parts the device has other hardware for, the heartbeat task and the app (it signs a
 *     session with the harness's TEST root key), and that set the part's analogue inputs.  A run of it is not verification evidence
 *     (REQ-SIMFW-06), and the panel says so.
 *
 * Text is set with textContent: nothing a firmware event carries is parsed as HTML.
 */
import { LINES, SCRIPTS, TEST_AID, FAULTS, FAULTS_UNAVAILABLE, outageScript, OUTAGE_MIN_MS } from './safety.js';

const el = (tag, cls, text) => {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text !== undefined) e.textContent = text;
  return e;
};

export class SafetyPanel {
  /**
   * @param {HTMLElement} root  the panel's container
   * @param {{ registry: object, send: (msg: object) => boolean }} deps  `send` puts one FW_COMMAND on the bridge
   */
  constructor(root, { registry, send }) {
    this._root = root;
    this._reg = registry;
    this._send = send;
    this._queue = [];
    this._pump = null;
    this._tier = 1;
    this._lineSel = 'bes';
    this._active = new Set();     // faults injected that stay until cleared
    this._outageNote = '';
    this._build();
    this.render();
  }

  /** Sends a script's messages in order, a few milliseconds apart, so the bridge's one status refresh follows the last. */
  run(messages) {
    if (!messages) return;
    this._queue.push(...messages);
    if (this._pump === null) {
      this._pump = setInterval(() => {
        const m = this._queue.shift();
        if (m === undefined) { clearInterval(this._pump); this._pump = null; return; }
        if (!this._send(m)) { this._queue.length = 0; this._error('Not connected to the simulator server.'); }
      }, 8);
    }
  }

  showError(text) { this._error(text); }

  _error(text) { this._err.textContent = text; }

  _btn(label, title, onClick) {
    const b = el('button', 'btn safety-btn', label);
    b.type = 'button';
    if (title) b.title = title;
    b.addEventListener('click', () => { this._error(''); onClick(); });
    return b;
  }

  _row(label, ...children) {
    const r = el('div', 'safety-row');
    r.append(el('span', 'safety-row-label', label), ...children);
    return r;
  }

  _build() {
    const r = this._root;
    r.textContent = '';
    this._badge = el('div', 'safety-badge');
    this._badge.setAttribute('role', 'status');
    this._badge.dataset.testid = 'safety-badge';
    this._note = el('p', 'safety-note');
    this._state = el('div', 'safety-state');
    this._state.dataset.testid = 'safety-state';

    const aid = el('div', 'safety-aid');
    aid.append(el('h4', 'safety-aid-title', 'Test aid'));
    aid.append(el('p', 'safety-note',
      'Plays the heartbeat task and the app, with the simulator\'s TEST root key. A simulator test aid, not verification evidence (REQ-SIMFW-06). '
      + 'Order: Attach (also puts every contact in contact), Beat, Sign, then Prepare (a declaration sent before the session starts is wiped by the session-start reset). For the cervical VNS, Request before the first Beat.'));

    const tier = el('select', 'select safety-select');
    tier.setAttribute('aria-label', 'Tier record programmed into the OTP');
    for (const [v, t] of [[1, 'T1 unit'], [2, 'T2 unit']]) { const o = el('option', null, t); o.value = String(v); tier.append(o); }
    tier.addEventListener('change', () => { this._tier = Number(tier.value); });
    this._attach = this._btn('Attach safety MCU', 'Programs the TEST root key and tier into the part\'s OTP, then starts the MCU. Once only.',
      () => this.run(SCRIPTS.attach(this._tier)));
    aid.append(this._row('Attach', tier, this._attach));

    aid.append(this._row('Heartbeat',
      this._btn('Beat 1 s', 'Heartbeats every 200 ms for 1 s of virtual time, session RUNNING', () => this.run(SCRIPTS.heartbeat(1000))),
      this._btn('Beat 5 s', null, () => this.run(SCRIPTS.heartbeat(5000))),
      this._btn('Lose it 2 s', 'Advance 2 s of virtual time with no heartbeat: the MCU\'s watchdog cuts the lines',
        () => this.run(SCRIPTS.heartbeatLoss(2000)))));

    aid.append(this._row('Session',
      this._btn('Sign', 'The app signs the session hash with the TEST root key', () => this.run(SCRIPTS.signSession('good'))),
      this._btn('Bad signature', 'One bit of the signature flipped', () => this.run(SCRIPTS.signSession('bad')))));

    const line = el('select', 'select safety-select');
    line.setAttribute('aria-label', 'Enable line');
    for (const l of LINES) { const o = el('option', null, l.label); o.value = l.name; line.append(o); }
    line.value = this._lineSel;
    line.addEventListener('change', () => { this._lineSel = line.value; });
    aid.append(this._row('Line', line,
      this._btn('Prepare', 'Contact, declared waveform and area (where the line is electrical) and the enable request, with the scenarios\' values',
        () => { const s = SCRIPTS.prepareLine(this._lineSel); if (s) this.run(s); else this._error('The page does not know what this line needs; request it by hand.'); }),
      this._btn('Request', 'The hub requests the line\'s enable', () => this.run(SCRIPTS.request(this._lineSel))),
      this._btn('Release', null, () => this.run(SCRIPTS.release(this._lineSel)))));

    aid.append(this._row('Hub command',
      this._btn('BES on', 'Command stimulation pair 0 as BES / tACS through the hub\'s driver', () => this.run(SCRIPTS.hubStim('bes'))),
      this._btn('BES off', null, () => this.run(SCRIPTS.hubStimStop('bes'))),
      this._btn('tDCS on', null, () => this.run(SCRIPTS.hubStim('tdcs'))),
      this._btn('tDCS off', null, () => this.run(SCRIPTS.hubStimStop('tdcs')))));

    aid.append(this._row('Thermal',
      this._btn('Hot NTC', 'Cranial NTC reads hotter than the 62 C row of the MCU\'s table', () => this.run(SCRIPTS.thermal(TEST_AID.hotAdcCounts))),
      this._btn('Cold NTC', null, () => this.run(SCRIPTS.thermal(TEST_AID.coldAdcCounts)))));

    aid.append(this._row('Cardiac',
      this._btn('Request cVNS', 'Request the cervical VNS line and command a current, before the first beat so the contact check runs at session start (attach as a T2 unit); then Prepare on that line declares its waveform', () => this.run(SCRIPTS.cvnsPrepare())),
      this._btn('70 bpm', 'A steady 70 bpm for the baseline', () => this.run(SCRIPTS.cardiacBaseline())),
      this._btn('Step to 100', 'A step of 30 bpm', () => this.run(SCRIPTS.cardiacStep()))));

    this._err = el('div', 'safety-error');
    this._err.setAttribute('role', 'alert');
    aid.append(this._err);

    r.append(this._badge, this._note, this._state, aid, this._buildFaults());
  }

  /**
   * The fault panel (REQ-SIMFW-16): one button per fault the system is built to detect, each an input to the firmware and never a
   * verdict.  A fault that stays until undone shows a Clear button; whether the MCU then recovers is the firmware's.
   */
  _buildFaults() {
    const box = el('div', 'safety-faults');
    box.dataset.testid = 'fault-panel';
    box.append(el('h4', 'safety-aid-title', 'Fault panel'));
    box.append(el('p', 'safety-note',
      'Each button sets what a pin, the link or a sensor would carry; the safety MCU\'s own sources decide what follows, and whether it recovers is theirs. '
      + 'A simulator test aid, not verification evidence (REQ-SIMFW-06).'));
    this._faultBtns = new Map();
    const chan = el('select', 'select safety-select');
    chan.setAttribute('aria-label', 'Contact channel');
    for (const [v, t] of [[0, 'Contact 0'], [1, 'Contact 1'], [2, 'Contact 2'], [3, 'Contact 3 (cVNS)']]) { const o = el('option', null, t); o.value = String(v); chan.append(o); }
    this._chan = chan;
    const groups = new Map();
    for (const f of FAULTS) { if (!groups.has(f.group)) groups.set(f.group, []); groups.get(f.group).push(f); }
    for (const [g, list] of groups) {
      const kids = [];
      for (const f of list) {
        const b = this._btn(f.label, f.title, () => this._inject(f));
        b.dataset.fault = f.id;
        this._faultBtns.set(f.id, b);
        kids.push(b);
        if (f.clear) {
          const c = this._btn('Clear', `Undo: ${f.label}`, () => this._clear(f));
          c.dataset.clear = f.id; c.hidden = true;
          this._faultBtns.set(`clear:${f.id}`, c);
          kids.push(c);
        }
        if (f.id === 'contact_lost') kids.push(chan);
      }
      box.append(this._row(g, ...kids));
    }
    const un = el('ul', 'safety-unavailable');
    un.setAttribute('aria-label', 'Faults this page cannot cause');
    for (const u of FAULTS_UNAVAILABLE) un.append(el('li', null, `${u.label}: ${u.why}`));
    box.append(el('p', 'safety-note', 'Not available here:'), un);
    this._outage = el('p', 'safety-note');
    this._outage.dataset.testid = 'fault-outage';
    this._outage.textContent = 'A real loss of this page\'s link to the simulator is played as a heartbeat loss of the same length when it returns.';
    box.append(this._outage);
    return box;
  }

  _inject(f) {
    const arg = f.id === 'contact_lost' ? Number(this._chan.value) : undefined;
    if (f.needs === 'attached' && !this._reg.safety.inLoop) { this._error('Attach the safety MCU first: this fault is an input to it.'); return; }
    if (f.needs === 'unattached' && this._reg.safety.inLoop) { this._error('This fault is set on a part before it starts; reload the simulator for a fresh part.'); return; }
    if (f.clear) this._active.add(f.id === 'contact_lost' ? `${f.id}:${arg}` : f.id);
    this.run(f.id === 'key_corrupt' ? f.script(this._tier) : f.script(arg));
    this.render();
  }

  _clear(f) {
    const keys = [...this._active].filter((k) => k === f.id || k.startsWith(`${f.id}:`));
    for (const k of keys) {
      const arg = k.includes(':') ? Number(k.split(':')[1]) : undefined;
      this.run(f.clear(arg));
      this._active.delete(k);
    }
    this.render();
  }

  /** The page's link to the simulator went down (a real outage); the hub's heartbeat task is behind that link. */
  linkLost() { this._outageNote = 'Link to the simulator lost: no heartbeat is being sent.'; this._showOutage(); }

  /** The link came back: play the wall time lost as a heartbeat loss, if the MCU is in the loop and the outage was long enough to be one. */
  linkRestored(lostAt, backAt) {
    const sc = this._reg.safety.inLoop ? outageScript(lostAt, backAt) : null;
    if (sc) {
      this._outageNote = `Link lost for ${Math.round((backAt - lostAt) / 1000)} s: played as a heartbeat loss of that length (capped at one harness call).`;
      this.run(sc);
    } else {
      this._outageNote = `Link lost for ${Math.round((backAt - lostAt) / 1000)} s: ${this._reg.safety.inLoop ? `under ${OUTAGE_MIN_MS / 1000} s, not a heartbeat loss` : 'no safety MCU in the loop'}.`;
    }
    this._showOutage();
  }

  _showOutage() { if (this._outage) this._outage.textContent = this._outageNote; }

  /** Redraws the state from the registry's SafetyMcu. */
  render() {
    const sum = this._reg.safety.summary();
    this._root.hidden = !this._reg.sawFirmware;
    this._root.dataset.inLoop = sum.inLoop ? 'true' : 'false';
    this._badge.textContent = sum.inLoop ? 'Safety MCU: in loop' : 'Safety MCU: not in loop';
    this._badge.className = `safety-badge ${sum.inLoop ? 'safety-in' : 'safety-out'}`;
    this._attach.disabled = sum.inLoop;
    if (this._faultBtns) {
      for (const f of FAULTS) {
        const b = this._faultBtns.get(f.id);
        b.disabled = f.needs === 'attached' ? !sum.inLoop : f.needs === 'unattached' ? sum.inLoop : false;
        const c = this._faultBtns.get(`clear:${f.id}`);
        if (c) c.hidden = ![...this._active].some((k) => k === f.id || k.startsWith(`${f.id}:`));
      }
    }
    this._note.textContent = sum.inLoop
      ? 'The device\'s own safety MCU sources are running. A module is drawn driven only while the MCU has its line enabled.'
      : 'No safety MCU is attached: a module is drawn driven when the hub commanded it, not when a safety MCU would have allowed it.';
    this._state.textContent = '';
    if (!sum.inLoop) return;

    const tbl = el('table', 'safety-lines');
    tbl.setAttribute('aria-label', 'Safety MCU line states');
    const head = el('tr');
    for (const h of ['Line', 'Hub requests', 'MCU grants', 'Line']) head.append(el('th', null, h));
    tbl.append(head);
    for (const l of sum.lines) {
      const tr = el('tr');
      tr.dataset.line = l.name;
      tr.dataset.enabled = l.enabled ? 'true' : 'false';
      tr.dataset.granted = l.granted ? 'true' : 'false';
      tr.append(el('td', null, l.label), el('td', null, l.requested ? 'yes' : '-'), el('td', null, l.granted ? 'granted' : (l.requested ? 'withheld' : '-')),
        el('td', l.enabled ? 'safety-on' : 'safety-off', l.enabled ? 'ENABLED' : 'off'));
      tbl.append(tr);
    }
    this._state.append(tbl);

    const kv = (k, v, id) => { const d = el('div', 'safety-kv'); d.append(el('span', 'safety-k', k), el('span', 'safety-v', v)); if (id) d.dataset.testid = id; return d; };
    const wd = sum.watchdog;
    const age = sum.heartbeatAgeMs;
    this._state.append(
      kv('Status flags', sum.status.length ? sum.status.join(' ') : 'OK', 'safety-status'),
      kv('Fault slot', sum.slot, 'safety-slot'),
      kv('Heartbeat', age === null ? 'none sent' : `${age} virtual ms since the last run ended`, 'safety-heartbeat'),
      kv('Watchdog', `${wd.flagInLastReply ? 'WATCHDOG flag in the last reply' : 'no flag in the last reply'}; independent watchdog ${wd.heldInReset ? 'HOLDING THE MCU IN RESET' : 'running'} (${wd.resets} resets)`, 'safety-watchdog'),
      kv('Last cutoff', this._cutoffText(sum.lastCutoff), 'safety-cutoff'),
      kv('Tier record', sum.tier === 0 ? 'not reported yet' : `T${sum.tier}`, 'safety-tier'),
    );
  }

  _cutoffText(c) {
    if (!c) return 'none seen';
    const what = c.flags.length ? c.flags.join(' + ') : 'reason pending (the reply comes with the next heartbeat)';
    return `${what}; slot ${c.slot}; lines ${c.lines.length ? c.lines.join(', ') : 'none dropped'}; at t=${c.t}`;
  }
}
