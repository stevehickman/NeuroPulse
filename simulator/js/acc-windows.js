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

/**
 * The schematics.  Each is a diagram of the accessory's parts as its hardware specification names them (NP-HW-VNSCLIP-001 A13.x,
 * NP-HW-NASAL-001 A12.x, NP-HW-CVNS-001 A14.x), with numbered markers on the drawing and a legend beneath it, so that no label
 * sits on a part.  They are not to scale and not a picture of a finished part: the specifications fix few dimensions.  Every one ends
 * at the hub's accessory port.  The parts that carry current or light carry `data-part` and light up from the module's record.
 * viewBox 200 x 176.
 */
const mark = (x, y, n) => `<g class="art-mark"><circle cx="${x}" cy="${y}" r="4.2" /><text x="${x}" y="${y + 2.4}">${n}</text></g>`;
const legend = (rows, y0) => rows.map(([n, text], i) => {
  const y = y0 + i * 9.4;
  return n === null
    ? `<text x="18" y="${y + 2.4}" class="art-legend-t">${text}</text>`
    : `${mark(10, y, n)}<text x="18" y="${y + 2.4}" class="art-legend-t">${text}</text>`;
}).join('');
const port = (x, y, label) => `
    <g class="art-port"><rect x="${x - 36}" y="${y}" width="72" height="14" rx="2" /><text x="${x}" y="${y + 9.4}" class="art-port-label">${label}</text></g>`;

const ART = {
  // Auricular VNS / HRV clip.
  vnsClip: `
    <g class="art-line">
      <path d="M44 4 C22 4 16 30 24 48 C29 58 36 60 38 68" /><path d="M44 4 C55 4 58 16 53 27 C50 34 44 36 46 44" />
      <path d="M156 4 C178 4 184 30 176 48 C171 58 164 60 162 68" /><path d="M156 4 C145 4 142 16 147 27 C150 34 156 36 154 44" />
    </g>
    <g class="art-part" data-part="left">
      <rect class="body" x="26" y="38" width="34" height="17" rx="5" />
      <rect class="pad" x="29" y="42" width="9" height="9" rx="1.5" />
      <circle class="led" cx="47" cy="46.5" r="2.5" /><circle class="pd" cx="54" cy="46.5" r="2.5" />
    </g>
    <g class="art-part" data-part="right">
      <rect class="body" x="140" y="38" width="34" height="17" rx="5" />
      <rect class="pad" x="162" y="42" width="9" height="9" rx="1.5" />
      <circle class="led" cx="153" cy="46.5" r="2.5" /><circle class="pd" cx="146" cy="46.5" r="2.5" />
    </g>
    <g class="art-line"><path d="M43 55 L43 74 L84 74 L84 86" /><path d="M157 55 L157 74 L116 74 L116 86" /></g>
    ${mark(20, 36, 1)}${mark(33, 62, 2)}${mark(50, 62, 3)}${mark(180, 36, 1)}${mark(167, 62, 2)}${mark(150, 62, 3)}
    <text x="100" y="14" class="art-label">L · R</text>
    ${port(100, 86, 'HUB ACCESSORY PORT')}
    ${legend([
      [1, 'spring clip, one per ear (A13.1)'],
      [2, 'contacts + hydrogel pad (A13.2, A13.5);'],
      [null, 'the contacts are also the A1/A2 reference (A13.4)'],
      [3, 'PPG emitter ● and detector ○, 808–830 nm (A13.3)'],
      [null, '6-pin cable to the hub port (A13.6)'],
    ], 118)}`,

  // Intranasal bilateral Y-probe.
  intranasal: `
    <g class="art-line">
      <ellipse cx="62" cy="10" rx="11" ry="6" /><ellipse cx="138" cy="10" rx="11" ry="6" />
      <path d="M62 18 L62 42 C62 56 100 58 100 68" /><path d="M138 18 L138 42 C138 56 100 58 100 68" />
      <path d="M100 68 L100 86" />
    </g>
    <g class="art-sleeve"><rect x="54" y="18" width="16" height="28" rx="6" /><rect x="130" y="18" width="16" height="28" rx="6" /></g>
    <g class="art-line"><path d="M55 34 L69 34" /><path d="M55 39 L69 39" /><path d="M131 34 L145 34" /><path d="M131 39 L145 39" /></g>
    <g class="art-part" data-part="left"><circle class="led" cx="59" cy="25" r="2.7" /><circle class="led led2" cx="65" cy="25" r="2.7" /></g>
    <g class="art-part" data-part="right"><circle class="led" cx="135" cy="25" r="2.7" /><circle class="led led2" cx="141" cy="25" r="2.7" /></g>
    <rect class="art-fill" x="58" y="42" width="8" height="4" rx="1" /><rect class="art-fill" x="134" y="42" width="8" height="4" rx="1" />
    <circle class="art-fill" cx="100" cy="62" r="5.5" />
    <circle class="art-dot" cx="94" cy="78" r="1.7" /><circle class="art-dot" cx="100" cy="78" r="1.7" /><circle class="art-dot" cx="106" cy="78" r="1.7" />
    ${mark(40, 25, 1)}${mark(40, 37, 2)}${mark(76, 44, 3)}${mark(40, 50, 4)}${mark(116, 62, 5)}${mark(116, 78, 6)}
    <text x="14" y="12" class="art-label">L</text><text x="186" y="12" class="art-label">R</text>
    ${port(100, 88, 'HUB ACCESSORY PORT')}
    ${legend([
      [1, 'tip: 660 nm + 808 nm per probe (A12.1)'],
      [2, 'depth-stop rings, 15 / 20 / 25 mm (A12.3)'],
      [3, 'photodiode: contact and dose, per probe (A12.4)'],
      [4, 'hygiene sleeve, single use (A12.6)'],
      [5, 'Y-junction, silicone over-mould (A12.2)'],
      [6, 'optical code + pogo-pin sleeve ID (A12.5)'],
    ], 114)}`,

  // Cervical VNS accessory.
  cvns: `
    <g class="art-line">
      <path d="M26 4 C26 24 42 34 58 34" /><path d="M174 4 C174 24 158 34 142 34" />
      <path d="M58 34 L58 70 C58 78 78 82 100 82 C122 82 142 78 142 70 L142 34" />
    </g>
    <g class="art-line art-dim"><path d="M100 4 L100 34" /><path d="M95 5 L105 5" /><path d="M95 33 L105 33" /></g>
    <g class="art-part" data-part="pads">
      <ellipse class="pad" cx="58" cy="52" rx="8.5" ry="13" /><ellipse class="pad" cx="142" cy="52" rx="8.5" ry="13" />
    </g>
    <path class="art-line art-housing" d="M58 64 C58 74 78 76 100 76 C122 76 142 74 142 64" />
    <path class="art-line" d="M100 82 L100 90" />
    ${mark(40, 52, 1)}${mark(160, 52, 1)}${mark(100, 62, 2)}${mark(112, 90, 3)}
    <text x="112" y="21" class="art-note-t" text-anchor="start">2 cm</text>
    ${port(100, 90, 'HUB ACCESSORY PORT')}
    ${legend([
      [1, 'gel electrodes ×2, over the carotid sheath,'],
      [null, '2 cm below the angle of the jaw (A14.2, A14.3)'],
      [2, 'neck module, silicone over-moulded (A14.1)'],
      [3, '4-conductor shielded cable (A14.4)'],
      [null, 'current source and sense are in the hub (A14.6)'],
    ], 118)}`,
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
      return [(snap?.intranasal?.wavelengths ?? []).join(' + '), snap?.intranasal?.frequency ? `${snap.intranasal.frequency} Hz` : null].filter(Boolean).join(' · ') || 'bilateral';
    }
    case 'cvns': {
      if (st.source !== 'firmware') return [snap?.cvns?.frequency ? `${snap.cvns.frequency} Hz` : null, snap?.cvns?.intensity_ma ? `${snap.cvns.intensity_ma} mA` : null].filter(Boolean).join(' · ') || 'session active';
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
      svg.setAttribute('viewBox', '0 0 200 176');
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
