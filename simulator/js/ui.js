/**
 * UIManager — owns all DOM panels and bridges them to the helmet model and
 * session engine.  No direct Three.js access; communicates via callbacks.
 */

import { WL, MODALITY_COLORS } from './colors.js';
import { PROTOCOLS } from './protocols.generated.js';
import { ZONES } from './sockets.generated.js';

// Real named zones (protocols/predefined/00-zones.npps), not the retired
// 5-slot ZM-01..05 model — see docs/np_hex_zm_001.md.
const ZONE_NAMES = ZONES.map(z => z.name);

const WL_CHOICES_T1 = ['660nm', '808nm', '1064nm'];
const WL_CHOICES_T2 = ['660nm', '808nm', '1064nm', '1170nm'];

// ─────────────────────────────────────────────────────────────────────────────

export class UIManager {
  /**
   * @param {{
   *   onZoneChange: (id, cfg) => void,
   *   onAccessoryChange: (name, visible) => void,
   *   onProtocolSelect: (id) => string|null,
   *   onStart: () => void,
   *   onPause: () => void,
   *   onStop: () => void,
   *   onViewChange: (view) => void,
   *   onDisplayModeChange: (mode) => void,
   * }} callbacks
   */
  constructor(callbacks) {
    this.cb = callbacks;
    this._zoneState = {};
    ZONE_NAMES.forEach(name => {
      this._zoneState[name] = { installed: false, wavelengths: [], frequency: 10 };
    });

    this._buildZoneConfig();
    this._buildAccessoryConfig();
    this._buildWavelengthLegend();
    this._buildProtocolSelector();
    this._buildModalityStatus();
    this._buildMetricsPanel();
    this._bindSessionControls();
    this._bindViewControls();
    this._bindDisplayModeControls();
    this._buildTimeline();
  }

  // ─── public ────────────────────────────────────────────────────────────────

  updateFromSnapshot(snap) {
    this._updateSessionTimer(snap);
    this._updateProgress(snap);
    this._updateMetrics(snap);
    this._updateModalityStatus(snap);
    this._updateSessionButtons(snap);
    this._updateTimelineCursor(snap);
    this._updateBreathingRing(snap);
  }

  setZoneLabelPositions(positions) {
    const container = document.getElementById('zone-labels');
    if (!container) return;
    positions.forEach(({ id, name, installed, screenX, screenY, visible }) => {
      let el = document.getElementById(`zone-label-${id}`);
      if (!el) {
        el = document.createElement('div');
        el.id = `zone-label-${id}`;
        el.className = 'zone-label';
        container.appendChild(el);
      }
      el.textContent = `${id}\n${name}`;
      el.style.left  = `${screenX}px`;
      el.style.top   = `${screenY}px`;
      el.style.opacity = visible ? '1' : '0';
      el.classList.toggle('installed', installed);
    });
  }

  // ─── zone configuration ────────────────────────────────────────────────────

  /** DOM-safe id fragment for a zone name ("Vault (excl. Occipital)" -> "vault-excl-occipital"). */
  _slug(name) {
    return name.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/(^-|-$)/g, '');
  }

  _buildZoneConfig() {
    const container = document.getElementById('zone-config');
    if (!container) return;

    ZONE_NAMES.forEach(name => {
      const id = this._slug(name);
      const card = document.createElement('div');
      card.className = 'zone-card';
      card.id = `zone-card-${id}`;

      const header = document.createElement('div');
      header.className = 'zone-card-header';

      const toggle = document.createElement('label');
      toggle.className = 'toggle';

      const chk = document.createElement('input');
      chk.type = 'checkbox';
      chk.id   = `zone-chk-${id}`;
      chk.addEventListener('change', () => this._onZoneToggle(name));

      const slider = document.createElement('span');
      slider.className = 'toggle-slider';

      toggle.append(chk, slider);

      const label = document.createElement('span');
      label.className = 'zone-label-text';
      label.textContent = name;

      header.append(toggle, label);
      card.appendChild(header);

      // Wavelength selector (hidden until installed)
      const wlWrap = document.createElement('div');
      wlWrap.className = 'zone-wl-wrap hidden';
      wlWrap.id = `zone-wl-${id}`;

      const wlLabel = document.createElement('div');
      wlLabel.className = 'zone-sub-label';
      wlLabel.textContent = 'Wavelengths';

      const wlPicker = document.createElement('div');
      wlPicker.className = 'wl-picker';
      wlPicker.id = `zone-wl-picker-${id}`;

      WL_CHOICES_T1.forEach(wl => {
        const btn = document.createElement('button');
        btn.className = 'wl-btn';
        btn.dataset.wl = wl;
        btn.dataset.zone = id;
        btn.title = WL[wl]?.description ?? wl;
        btn.style.setProperty('--wl-color', WL[wl]?.hex ?? '#ff4400');
        btn.textContent = wl;
        btn.addEventListener('click', () => this._onWLToggle(name, wl, btn));
        wlPicker.appendChild(btn);
      });

      // Frequency selector
      const freqLabel = document.createElement('div');
      freqLabel.className = 'zone-sub-label';
      freqLabel.style.marginTop = '8px';
      freqLabel.textContent = 'PBM Frequency';

      const freqWrap = document.createElement('div');
      freqWrap.className = 'freq-wrap';

      const freqSel = document.createElement('select');
      freqSel.className = 'select-sm';
      freqSel.id = `zone-freq-${id}`;
      const PRESETS = [
        { label: 'CW (continuous)', value: 0 },
        { label: '2 Hz — Delta', value: 2 },
        { label: '6 Hz — Theta memory', value: 6 },
        { label: '10 Hz — Alpha', value: 10 },
        { label: '20 Hz — Beta / Focus', value: 20 },
        { label: '40 Hz — Gamma', value: 40 },
      ];
      PRESETS.forEach(p => {
        const opt = document.createElement('option');
        opt.value = p.value;
        opt.textContent = p.label;
        if (p.value === 10) opt.selected = true;
        freqSel.appendChild(opt);
      });
      freqSel.addEventListener('change', () => this._onFreqChange(name));
      freqWrap.appendChild(freqSel);

      wlWrap.append(wlLabel, wlPicker, freqLabel, freqWrap);
      card.appendChild(wlWrap);

      container.appendChild(card);
    });
  }

  _onZoneToggle(name) {
    const id = this._slug(name);
    const chk = document.getElementById(`zone-chk-${id}`);
    const wlWrap = document.getElementById(`zone-wl-${id}`);
    const card  = document.getElementById(`zone-card-${id}`);
    const inst  = chk.checked;

    this._zoneState[name].installed = inst;
    wlWrap?.classList.toggle('hidden', !inst);
    card?.classList.toggle('zone-active', inst);

    this.cb.onZoneChange(name, { ...this._zoneState[name] });
  }

  _onWLToggle(name, wl, btn) {
    const state = this._zoneState[name];
    const idx = state.wavelengths.indexOf(wl);
    if (idx >= 0) {
      state.wavelengths.splice(idx, 1);
      btn.classList.remove('active');
    } else {
      state.wavelengths.push(wl);
      btn.classList.add('active');
    }
    this.cb.onZoneChange(name, { ...state });
  }

  _onFreqChange(name) {
    const id = this._slug(name);
    const sel = document.getElementById(`zone-freq-${id}`);
    this._zoneState[name].frequency = Number(sel.value);
    this.cb.onZoneChange(name, { ...this._zoneState[name] });
  }

  // ─── accessories ───────────────────────────────────────────────────────────

  _buildAccessoryConfig() {
    const container = document.getElementById('accessory-config');
    if (!container) return;

    const ACCESSORIES = [
      { key: 'goggles',     label: 'Visual Goggles',      sub: '108 micro-LEDs/lens · EMDR · Mode F' },
      { key: 'vnsClip',     label: 'VNS + HRV Clip',      sub: 'Auricular · PPG · 1–25 Hz' },
      { key: 'hapticPad',   label: 'Mastoid Haptic Pad',   sub: '40 Hz LRA · Provisional' },
      { key: 'intranasal',  label: 'Intranasal Probe',     sub: 'Y-probe · 660 + 808 nm · bilateral' },
      { key: 'tmsCoil',     label: 'TMS Figure-8 Coil',   sub: 'T2 · 0.1–0.5 T · rTMS + TBS · DLPFC_L' },
    ];

    ACCESSORIES.forEach(({ key, label, sub }) => {
      const row = document.createElement('div');
      row.className = 'accessory-row';

      const toggle = document.createElement('label');
      toggle.className = 'toggle';

      const chk = document.createElement('input');
      chk.type = 'checkbox';
      chk.addEventListener('change', () => this.cb.onAccessoryChange(key, chk.checked));

      const slider = document.createElement('span');
      slider.className = 'toggle-slider';
      toggle.append(chk, slider);

      const text = document.createElement('div');
      text.className = 'accessory-text';
      text.innerHTML = `<span class="accessory-name">${label}</span><span class="accessory-sub">${sub}</span>`;

      row.append(toggle, text);
      container.appendChild(row);
    });
  }

  // ─── wavelength legend ─────────────────────────────────────────────────────

  _buildWavelengthLegend() {
    const container = document.getElementById('wavelength-legend');
    if (!container) return;

    Object.entries(WL).forEach(([key, def]) => {
      const row = document.createElement('div');
      row.className = 'legend-row';

      const swatch = document.createElement('span');
      swatch.className = 'legend-swatch';
      swatch.style.background = def.hex;
      swatch.style.boxShadow = `0 0 6px ${def.glowHex}`;

      const text = document.createElement('span');
      text.className = 'legend-text';
      text.textContent = def.label;
      text.title = def.description;

      const badge = document.createElement('span');
      badge.className = def.isVisible ? 'legend-badge visible' : 'legend-badge nir';
      badge.textContent = def.isVisible ? 'VISIBLE' : 'NIR';

      row.append(swatch, text, badge);
      container.appendChild(row);
    });
  }

  // ─── protocol selector ────────────────────────────────────────────────────

  _buildProtocolSelector() {
    const sel  = document.getElementById('protocol-select');
    const info = document.getElementById('protocol-info');
    if (!sel) return;

    Object.values(PROTOCOLS).forEach(p => {
      const opt = document.createElement('option');
      opt.value = p.id;
      opt.textContent = `[${p.tier}] ${p.name}`;
      sel.appendChild(opt);
    });

    sel.addEventListener('change', () => {
      const id = sel.value;
      if (!id) { if (info) info.innerHTML = ''; return; }
      this.cb.onProtocolSelect(id);
      const p = PROTOCOLS[id];
      if (info && p) {
        const mods = Object.entries(p.modalities)
          .filter(([, v]) => v.active)
          .map(([k]) => {
            const mc = MODALITY_COLORS[k];
            return `<span class="mod-pill" style="--mc:${mc?.hex ?? '#aaa'}">${mc?.label ?? k}</span>`;
          })
          .join('');
        info.innerHTML = `
          <p class="prot-desc">${p.description}</p>
          <div class="prot-duration">⏱ ${this._fmtDuration(p.duration)}</div>
          <div class="prot-mods">${mods}</div>`;

        // Auto-configure zones from protocol
        this._applyProtocolToZones(p);
      }
    });
  }

  _applyProtocolToZones(p) {
    const pbm = p.modalities.pbm;
    if (!pbm) return;

    ZONE_NAMES.forEach(name => {
      const id = this._slug(name);
      const inst = pbm.zones?.includes(name) ?? false;
      const chk  = document.getElementById(`zone-chk-${id}`);
      if (chk) chk.checked = inst;

      // Trigger wavelength pre-selection
      this._zoneState[name].installed   = inst;
      this._zoneState[name].wavelengths = inst ? [...(pbm.wavelengths ?? [])] : [];
      this._zoneState[name].frequency   = pbm.frequency ?? 10;

      document.getElementById(`zone-wl-${id}`)?.classList.toggle('hidden', !inst);
      document.getElementById(`zone-card-${id}`)?.classList.toggle('zone-active', inst);

      // Reflect wavelength buttons
      if (inst) {
        const picker = document.getElementById(`zone-wl-picker-${id}`);
        picker?.querySelectorAll('.wl-btn').forEach(btn => {
          btn.classList.toggle('active', this._zoneState[name].wavelengths.includes(btn.dataset.wl));
        });

        const freqSel = document.getElementById(`zone-freq-${id}`);
        if (freqSel) freqSel.value = String(pbm.frequency ?? 10);
      }

      this.cb.onZoneChange(name, { ...this._zoneState[name] });
    });
  }

  // ─── modality status ─────────────────────────────────────────────────────

  _buildModalityStatus() {
    const container = document.getElementById('modality-status');
    if (!container) return;

    const MODAL_DEFS = [
      { key: 'pbm',    snap: s => s.pbm.active    },
      { key: 'eeg',    snap: s => s.eeg.active    },
      { key: 'bes',    snap: s => s.bes.active    },
      { key: 'tdcs',   snap: s => s.tdcs.active   },
      { key: 'vns',    snap: s => s.vns.active    },
      { key: 'audio',  snap: s => s.audio.active  },
      { key: 'visual', snap: s => s.visual.active },
      { key: 'tms',    snap: s => s.tms.active    },
      { key: 'haptic', snap: _ => false },
    ];

    MODAL_DEFS.forEach(({ key }) => {
      const mc = MODALITY_COLORS[key];
      const pill = document.createElement('div');
      pill.className = 'modality-pill';
      pill.id = `mod-pill-${key}`;
      pill.style.setProperty('--mc', mc?.hex ?? '#667');
      pill.innerHTML = `<span class="mod-dot"></span><span>${mc?.label ?? key}</span>`;
      container.appendChild(pill);
    });
  }

  _updateModalityStatus(snap) {
    const modMap = {
      pbm:    snap.pbm.active,
      eeg:    snap.eeg.active,
      bes:    snap.bes.active,
      tdcs:   snap.tdcs.active,
      vns:    snap.vns.active,
      audio:  snap.audio.active,
      visual: snap.visual.active,
      tms:    snap.tms.active,
      haptic: false,
    };
    Object.entries(modMap).forEach(([key, active]) => {
      document.getElementById(`mod-pill-${key}`)?.classList.toggle('active', active);
    });
  }

  // ─── metrics panel ────────────────────────────────────────────────────────

  _buildMetricsPanel() {
    const container = document.getElementById('metrics-panel');
    if (!container) return;

    const METRICS = [
      { id: 'met-alpha',  label: 'Alpha power',    unit: 'µV²/Hz', icon: '🧠' },
      { id: 'met-nf',     label: 'NF score',        unit: '/10',    icon: '◎' },
      { id: 'met-hr',     label: 'Heart rate',      unit: 'bpm',    icon: '♡' },
      { id: 'met-hrv',    label: 'RMSSD',           unit: 'ms',     icon: '~' },
      { id: 'met-coh',    label: 'HRV coherence',   unit: '/10',    icon: '◉' },
      { id: 'met-dose1',  label: 'PBM dose',        unit: 'J/cm²',  icon: '☀', labelId: 'met-dose1-label' },
    ];

    METRICS.forEach(({ id, label, unit, icon, labelId }) => {
      const row = document.createElement('div');
      row.className = 'metric-row';
      const labelHtml = labelId
        ? `<span class="metric-label" id="${labelId}">${label}</span>`
        : `<span class="metric-label">${label}</span>`;
      row.innerHTML = `
        <span class="metric-icon">${icon}</span>
        ${labelHtml}
        <span class="metric-value" id="${id}">—</span>
        <span class="metric-unit">${unit}</span>`;
      container.appendChild(row);
    });

    // HRV breathing ring
    const ringWrap = document.createElement('div');
    ringWrap.className = 'breathing-ring-wrap';
    ringWrap.innerHTML = `
      <div class="breathing-ring-label">Breathing pacer</div>
      <div class="breathing-ring-outer" id="breathing-outer">
        <div class="breathing-ring-inner" id="breathing-inner"></div>
      </div>`;
    container.appendChild(ringWrap);
  }

  _updateMetrics(snap) {
    const v = (id, val) => {
      const el = document.getElementById(id);
      if (el) el.textContent = val;
    };

    // Readouts show the frozen last value while paused (session.js keeps
    // `configured` true independent of pause) and only blank to "—" once the
    // session actually stops (snap.running false) or the protocol never used
    // that modality. `active` (in-use, false while paused) drives the LED/pill
    // animations elsewhere, not these numeric readouts.
    const holding = snap.running;

    if (holding && snap.eeg.configured) {
      v('met-alpha', snap.eeg.alphaPower.toFixed(1));
      v('met-nf',    snap.eeg.neurofeedbackScore.toFixed(1));
    } else {
      v('met-alpha', '—'); v('met-nf', '—');
    }

    if (holding && snap.vns.configured) {
      v('met-hr',  snap.vns.hr.toFixed(0));
      v('met-hrv', snap.vns.rmssd.toFixed(1));
      v('met-coh', snap.vns.coherence.toFixed(1));
    } else {
      v('met-hr', '—'); v('met-hrv', '—'); v('met-coh', '—');
    }

    // Real protocols can target several named zones at once (00-zones.npps);
    // show the dose of the first targeted zone rather than a fixed "ZM-01".
    const firstZone = snap.pbm.zones?.[0];
    const labelEl = document.getElementById('met-dose1-label');
    if (holding && snap.pbm.configured && firstZone && snap.pbm.dose[firstZone] !== undefined) {
      if (labelEl) labelEl.textContent = `PBM dose (${firstZone})`;
      v('met-dose1', snap.pbm.dose[firstZone]);
    } else {
      if (labelEl) labelEl.textContent = 'PBM dose';
      v('met-dose1', '—');
    }
  }

  _updateBreathingRing(snap) {
    const outer = document.getElementById('breathing-outer');
    const inner = document.getElementById('breathing-inner');
    if (!outer || !inner) return;

    const active = snap.vns.active && snap.audio.breathing_pacer;
    outer.classList.toggle('breathing-active', active);

    if (active) {
      // breathing_phase 0→1→0 maps to scale 0.4→1.0
      const scale = 0.4 + 0.6 * snap.vns.breathing_phase;
      inner.style.transform = `scale(${scale.toFixed(3)})`;
    } else {
      inner.style.transform = 'scale(0.4)';
    }
  }

  // ─── session controls ─────────────────────────────────────────────────────

  _bindSessionControls() {
    document.getElementById('btn-play')?.addEventListener('click', () => this.cb.onStart());
    document.getElementById('btn-pause')?.addEventListener('click', () => this.cb.onPause());
    document.getElementById('btn-stop')?.addEventListener('click', () => this.cb.onStop());
  }

  _updateSessionButtons(snap) {
    const play  = document.getElementById('btn-play');
    const pause = document.getElementById('btn-pause');
    const stop  = document.getElementById('btn-stop');
    if (!play) return;

    if (!snap.running) {
      play.disabled  = false;
      play.textContent = '▶ Start';
      pause.disabled = true;
      stop.disabled  = true;
    } else if (snap.paused) {
      play.disabled  = true;
      pause.textContent = '▶ Resume';
      pause.disabled = false;
      stop.disabled  = false;
    } else {
      play.disabled  = true;
      pause.textContent = '⏸ Pause';
      pause.disabled = false;
      stop.disabled  = false;
    }
  }

  _updateSessionTimer(snap) {
    const el = document.getElementById('session-timer');
    if (!el) return;
    el.textContent = this._fmtTime(snap.elapsed);
    if (snap.running && !snap.paused) el.classList.add('running');
    else el.classList.remove('running');
  }

  _updateProgress(snap) {
    const bar = document.getElementById('session-progress');
    if (!bar) return;
    bar.style.width = `${(snap.progress * 100).toFixed(1)}%`;
  }

  // ─── view controls ────────────────────────────────────────────────────────

  _bindViewControls() {
    const scope = document.getElementById('viewport-wrap');
    scope?.querySelectorAll('.view-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        scope.querySelectorAll('.view-btn').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        this.cb.onViewChange(btn.dataset.view);
      });
    });
  }

  // ─── display mode (floating / on mannequin) ─────────────────────────────

  _bindDisplayModeControls() {
    const scope = document.getElementById('display-mode-controls');
    scope?.querySelectorAll('.view-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        scope.querySelectorAll('.view-btn').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        this.cb.onDisplayModeChange(btn.dataset.displayMode);
      });
    });
  }

  // ─── timeline ─────────────────────────────────────────────────────────────

  _buildTimeline() {
    // Timeline will be populated when a protocol is selected
  }

  populateTimeline(protocol) {
    const track = document.getElementById('timeline-track');
    if (!track || !protocol) return;
    track.innerHTML = '';

    const dur = protocol.duration;

    protocol.phases.forEach(phase => {
      const pct = ((phase.end - phase.start) / dur * 100).toFixed(2);
      const left = (phase.start / dur * 100).toFixed(2);
      const seg = document.createElement('div');
      seg.className = 'tl-segment';
      seg.style.left  = `${left}%`;
      seg.style.width = `${pct}%`;
      seg.style.background = phase.color;
      seg.title = `${phase.label} (${this._fmtTime(phase.start)} – ${this._fmtTime(phase.end)})`;

      const lbl = document.createElement('span');
      lbl.className = 'tl-label';
      lbl.textContent = phase.label;
      seg.appendChild(lbl);

      track.appendChild(seg);
    });

    // Cursor line
    let cursor = document.getElementById('tl-cursor-line');
    if (!cursor) {
      cursor = document.createElement('div');
      cursor.id = 'tl-cursor-line';
      cursor.className = 'tl-cursor';
      track.appendChild(cursor);
    }
  }

  _updateTimelineCursor(snap) {
    const cursor = document.getElementById('tl-cursor-line');
    if (!cursor) return;
    cursor.style.left = `${(snap.progress * 100).toFixed(2)}%`;
    const label = document.getElementById('timeline-cursor');
    if (label) label.textContent = this._fmtTime(snap.elapsed);
  }

  // ─── utilities ────────────────────────────────────────────────────────────

  _fmtTime(sec) {
    const m = Math.floor(sec / 60);
    const s = Math.floor(sec % 60);
    return `${m}:${String(s).padStart(2, '0')}`;
  }

  _fmtDuration(sec) {
    const m = Math.floor(sec / 60);
    return `${m} min`;
  }
}
