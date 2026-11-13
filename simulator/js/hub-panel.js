/**
 * HubPanel — the hub's own tasks and the session the runner is running (NP-SIM-FW-001 §31, REQ-SIMFW-23).
 *
 * Two parts, kept apart on the page:
 *   - the STATE: the runner, the session's time and progress, the modalities active, the status LED and the hub's five tasks; every
 *     figure is the firmware's own `hub_state` (the runner's progress frame), nothing is computed here but a label;
 *   - the TEST AID: boot (with the safety MCU provisioned), a clock that moves virtual time, the signature step the hub's code never
 *     sends, and the app's pause, resume and stop writes.  A run of it is not verification evidence (REQ-SIMFW-06).
 *
 * The clock is the page's pacing: one virtual ms per wall ms while it is on, in steps of TICK_MS.  How wall time relates to virtual time is
 * an open question (OI-SIMFW-04, the principal's Q4); this is a choice made here, not an answer to it.
 *
 * Text is set with textContent: nothing a firmware event carries is parsed as HTML.
 */
import { HUB_SCRIPTS, TICK_MS } from './hub.js';

const el = (tag, cls, text) => {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text !== undefined) e.textContent = text;
  return e;
};

export class HubPanel {
  /** @param {{ hub: import('./hub.js').HubModel, send: (msg: object) => boolean, seen?: () => boolean }} deps  `seen`: the page is talking to a firmware process */
  constructor(root, { hub, send, seen = () => true }) {
    this._seen = seen;
    this._root = root;
    this._hub = hub;
    this._send = send;
    this._timer = null;
    this._build();
    this.render();
  }

  _run(messages) {
    for (const m of messages) if (!this._send(m)) { this._err.textContent = 'Not connected to the simulator server.'; return false; }
    this._err.textContent = '';
    return true;
  }

  _btn(label, title, onClick) {
    const b = el('button', 'btn safety-btn', label);
    b.type = 'button';
    b.title = title;
    b.addEventListener('click', onClick);
    this._aid.append(b);
    return b;
  }

  _build() {
    this._badge = el('span', 'safety-badge safety-out', 'Hub not running');
    this._note = el('p', 'safety-note', 'The hub\'s own five tasks run on a virtual clock that moves only while the clock below is on. A session starts by an upload from the app, not from this panel.');
    this._kv = el('div', 'safety-kvs');
    this._bar = el('progress'); this._bar.max = 1; this._bar.value = 0; this._bar.style.width = '100%';
    this._tasks = el('div', 'safety-note');
    this._aid = el('div', 'safety-aid');
    this._err = el('div', 'safety-note');
    this._boot = this._btn('Boot hub', 'Provision the safety MCU (TEST root key, T1, every contact in contact), then run the hub\'s own boot path and start its five tasks. Test aid, not verification evidence.', () => this._run(HUB_SCRIPTS.boot()));
    this._clock = this._btn('Clock: off', 'While on, virtual time moves one ms per wall ms, in steps of ' + TICK_MS + ' ms.', () => this._toggleClock());
    this._sign = this._btn('Sign session', 'Plays the session-signature step with the TEST root key: the hub\'s own code never sends it (OI-SIMFW-74).', () => this._run(HUB_SCRIPTS.sign()));
    this._pause = this._btn('Pause', 'The app\'s write to the hub\'s pause characteristic.', () => this._run(HUB_SCRIPTS.pause()));
    this._resume = this._btn('Resume', 'The app\'s write to the hub\'s pause characteristic.', () => this._run(HUB_SCRIPTS.resume()));
    this._stop = this._btn('Stop', 'The app\'s write to the hub\'s stop characteristic.', () => this._run(HUB_SCRIPTS.stop()));
    this._root.append(this._badge, this._note, this._kv, this._bar, this._tasks, this._aid, this._err);
  }

  _toggleClock() {
    if (this._timer !== null) { clearInterval(this._timer); this._timer = null; } else {
      this._timer = setInterval(() => { if (!this._run(HUB_SCRIPTS.tick())) this._toggleClock(); }, TICK_MS);
    }
    this._clock.textContent = this._timer !== null ? 'Clock: on' : 'Clock: off';
  }

  linkLost() { if (this._timer !== null) this._toggleClock(); }

  render() {
    const h = this._hub;
    this._root.hidden = !this._seen();
    this._badge.textContent = h.running ? 'Hub running' : 'Hub not running';
    this._badge.className = 'safety-badge ' + (h.running ? 'safety-in' : 'safety-out');
    this._kv.replaceChildren();
    const row = (k, v) => { const d = el('div', 'safety-kv'); d.append(el('span', 'safety-k', k), el('span', 'safety-v', v)); this._kv.append(d); };
    const s = h.state;
    row('Runner', h.runner);
    if (s) {
      row('Session time', `${(s.elapsed_ms / 1000).toFixed(1)} s of ${(s.total_ms / 1000).toFixed(1)} s`);
      row('Active now', h.activeModalities().join(', ') || 'none');
      row('Status LED', s.led);
      row('Hold cause', String(s.hold));
    }
    const p = h.progress();
    this._bar.value = p === null ? 0 : p;
    this._tasks.textContent = h.tasks.size ? 'Tasks: ' + [...h.tasks.values()].map((t) => `${t.name} (${t.state})`).join(', ') : '';
    this._pause.disabled = !h.canPause();
    this._resume.disabled = !h.canResume();
    this._stop.disabled = !h.canStop();
    this._boot.disabled = h.running;
  }
}
