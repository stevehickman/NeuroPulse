/**
 * Accessory windows (NP-SIM-FW-001 §21): one window per accessory that is not attached to the helmet, along the perimeter of the
 * helmet window, shown only while the accessory is in use.  Which accessories, where, and when are in accessories.js; this file is
 * the DOM and the drawings.
 *
 * Each window draws what the module reports and invents nothing: a side is lit if the firmware module's record says that side is
 * driven, a stage is named as the firmware's session says.  In standalone mode it draws the session snapshot instead.
 */
import { WINDOW_ACCESSORIES, windowRects, CVNS_STAGE_NAMES } from './accessories.js';

const NS = 'http://www.w3.org/2000/svg';

/** The drawings.  Each has a viewBox of 160 x 96 and marks the parts that light up with `data-part`. */
const ART = {
  vnsClip: `
    <g class="art-line">
      <path d="M40 14 C16 14 12 44 22 62 C28 74 38 76 40 86" /><path d="M40 14 C52 14 56 28 50 40 C46 48 40 50 42 60" />
      <path d="M120 14 C144 14 148 44 138 62 C132 74 122 76 120 86" /><path d="M120 14 C108 14 104 28 110 40 C114 48 120 50 118 60" />
    </g>
    <g class="art-part" data-part="left"><rect x="30" y="52" width="22" height="9" rx="3" /><circle class="led" cx="41" cy="56.5" r="2.6" /></g>
    <g class="art-part" data-part="right"><rect x="108" y="52" width="22" height="9" rx="3" /><circle class="led" cx="119" cy="56.5" r="2.6" /></g>
    <text x="40" y="94" class="art-label">L</text><text x="120" y="94" class="art-label">R</text>
    <path class="art-line" d="M80 4 L80 26" /><circle class="art-dot" cx="80" cy="30" r="3" />`,
  intranasal: `
    <g class="art-line">
      <ellipse cx="62" cy="22" rx="11" ry="7" /><ellipse cx="98" cy="22" rx="11" ry="7" />
      <path d="M80 92 L80 66 C80 58 62 52 62 40 L62 28" /><path d="M80 66 C80 58 98 52 98 40 L98 28" />
    </g>
    <g class="art-part" data-part="left"><circle class="led" cx="62" cy="32" r="3.4" /></g>
    <g class="art-part" data-part="right"><circle class="led" cx="98" cy="32" r="3.4" /></g>
    <text x="40" y="26" class="art-label">L</text><text x="112" y="26" class="art-label">R</text>`,
  cvns: `
    <g class="art-line">
      <path d="M52 6 L52 70 C52 84 62 90 80 90 C98 90 108 84 108 70 L108 6" />
    </g>
    <g class="art-part" data-part="pads">
      <ellipse class="pad" cx="52" cy="44" rx="9" ry="14" /><ellipse class="pad" cx="108" cy="44" rx="9" ry="14" />
    </g>
    <path class="art-line" d="M52 58 C52 72 66 76 80 76 C94 76 108 72 108 58" />
    <rect class="art-line" x="72" y="92" width="16" height="4" rx="1" />`,
};

const el = (tag, cls, text) => {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text !== undefined) e.textContent = text;
  return e;
};

/** The one-line statement of what an accessory is doing, from the firmware's fields or the session's. */
export function describe(id, st, snap) {
  const m = st.modules?.[0]?.detail ?? {};
  const mA = (ua) => `${(ua / 1000).toFixed(ua >= 10000 ? 0 : 1)} mA`;
  switch (id) {
    case 'vnsClip': {
      if (st.source === 'firmware') {
        const side = m.left && m.right ? 'both ears' : m.left ? 'left ear' : m.right ? 'right ear' : 'no side';
        return [side, m.amp_ua ? mA(m.amp_ua) : null, m.ppg_on ? 'PPG on' : null].filter(Boolean).join(' · ');
      }
      return [`${snap?.vns?.frequency ?? ''} Hz`, snap?.vns?.hrv_sync ? 'HRV sync' : null].filter(Boolean).join(' · ');
    }
    case 'intranasal': {
      if (st.source === 'firmware') {
        const side = m.left && m.right ? 'both nostrils' : m.left ? 'left nostril' : m.right ? 'right nostril' : 'no side';
        return `${side} · 660 + 808 nm`;
      }
      return '660 + 808 nm · bilateral (demo)';
    }
    case 'cvns': {
      const stage = typeof m.stage === 'number' ? CVNS_STAGE_NAMES[m.stage] ?? `stage ${m.stage}` : null;
      const ma = typeof m.commanded_ua === 'number' && m.commanded_ua > 0 ? mA(m.commanded_ua) : null;
      return [stage, ma].filter(Boolean).join(' · ') || 'session active';
    }
    default:
      return '';
  }
}

/** Which parts of a window's drawing are lit, and in what tone, for a state. */
export function lit(id, st, snap) {
  const m = st.modules?.[0]?.detail ?? {};
  if (id === 'cvns') {
    const stage = m.stage;
    const tone = stage === 7 ? 'fault' : (stage === 3 || stage === 4 || stage === 5) ? 'on' : 'wait';
    return { pads: tone };
  }
  if (st.source === 'firmware') return { left: m.left ? 'on' : null, right: m.right ? 'on' : null };
  return { left: 'on', right: 'on' };   // standalone: the session does not say which side
}

export class AccessoryWindows {
  /** @param {HTMLElement} layer an absolutely positioned layer filling the helmet window */
  constructor(layer) {
    this.layer = layer;
    this._wins = new Map();
    for (const a of WINDOW_ACCESSORIES) {
      const root = el('section', 'accwin');
      root.dataset.acc = a.id;
      root.hidden = true;
      root.setAttribute('aria-label', `${a.label}, in use`);
      const head = el('header', 'accwin-head');
      head.append(el('span', 'accwin-title', a.label), el('span', 'accwin-tag', 'tethered to the hub'));
      const svg = document.createElementNS(NS, 'svg');
      svg.setAttribute('viewBox', '0 0 160 100');
      svg.setAttribute('class', 'accwin-art');
      svg.setAttribute('role', 'img');
      svg.setAttribute('aria-hidden', 'true');
      svg.innerHTML = ART[a.id] ?? '';
      const status = el('div', 'accwin-status');
      root.append(head, svg, status);
      layer.append(root);
      this._wins.set(a.id, { root, svg, status });
    }
  }

  /** Position every window for a viewport of this size (the slots depend only on the size). */
  layout(vw, vh) {
    const rects = windowRects(vw, vh);
    for (const [id, w] of this._wins) {
      const r = rects.get(id);
      Object.assign(w.root.style, { left: `${r.x}px`, top: `${r.y}px`, width: `${r.w}px`, height: `${r.h}px` });
      w.root.dataset.slot = r.slot;
    }
  }

  /** Show the windows of the accessories in use, hide the rest, and say what each is doing. */
  update(states, snap) {
    for (const [id, w] of this._wins) {
      const st = states.get(id);
      const on = !!st && st.inUse;
      if (w.root.hidden === on) w.root.hidden = !on;
      if (!on) continue;
      const parts = lit(id, st, snap);
      for (const node of w.svg.querySelectorAll('[data-part]')) {
        const tone = parts[node.dataset.part] ?? null;
        node.classList.toggle('on', tone === 'on');
        node.classList.toggle('wait', tone === 'wait');
        node.classList.toggle('fault', tone === 'fault');
      }
      const text = describe(id, st, snap);
      if (w.status.textContent !== text) w.status.textContent = text;
    }
  }

  /** The ids of the windows showing now (for tests and the page's self-check). */
  visibleIds() {
    return [...this._wins].filter(([, w]) => !w.root.hidden).map(([id]) => id);
  }
}
