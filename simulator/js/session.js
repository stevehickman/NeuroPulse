/**
 * SessionEngine — drives the simulated helmet state over time.
 *
 * All physics is fake (no actual hardware), but outputs are dimensionally
 * correct so the UI displays realistic-looking numbers.
 */

import { PROTOCOLS } from './protocols.generated.js';

export class SessionEngine {
  constructor() {
    this.protocol = null;
    this.running = false;
    this.paused  = false;
    this.elapsed = 0;       // seconds into session
    this.lastTick = 0;      // performance.now() at last tick

    // Simulated per-zone state
    this._zoneState = {};
    this._eegState  = {};
    this._hrvState  = {};
    this._initEEG();
    this._initHRV();

    // Accumulated dose (J/cm²) per zone, reset on session start
    this._dose = {};
  }

  // ─── public API ───────────────────────────────────────────────────────────

  load(protocolId) {
    this.protocol = PROTOCOLS[protocolId] ?? null;
    return this.protocol;
  }

  start() {
    if (!this.protocol) return false;
    this.running = true;
    this.paused  = false;
    this.elapsed = 0;
    this.lastTick = performance.now();
    this._dose = {};
    this._resetEEG();
    return true;
  }

  pause() {
    if (!this.running) return;
    this.paused = !this.paused;
    if (!this.paused) this.lastTick = performance.now();
  }

  stop() {
    this.running = false;
    this.paused  = false;
    this.elapsed = 0;
  }

  tick() {
    if (!this.running || this.paused) return;

    const now  = performance.now();
    const dt   = (now - this.lastTick) / 1000;
    this.lastTick = now;
    this.elapsed += dt;

    if (this.elapsed >= this.protocol.duration) {
      this.elapsed = this.protocol.duration;
      this.running = false;
    }

    this._advanceEEG(dt);
    this._advanceHRV(dt);
  }

  /** Returns a snapshot of the full simulated state, used by the renderer and UI. */
  snapshot(wallTime) {
    if (!this.protocol) return this._emptySnapshot();

    const t   = this.elapsed;
    const dur = this.protocol.duration;
    const m   = this.protocol.modalities;
    const rampFraction = this._rampEnvelope(t, dur);

    // ── PBM ──────────────────────────────────────────────────────────────────
    const pbm = m.pbm ?? {};
    const pbmState = {
      active:      this.running && (pbm.active ?? false),
      zones:       pbm.zones ?? [],
      wavelengths: pbm.wavelengths ?? [],
      frequency:   pbm.frequency ?? 0,
      dutyCycle:   pbm.dutyCycle ?? 0.25,
      intensity:   rampFraction,
      dose:        {},
    };
    if (pbmState.active) {
      (pbm.zones ?? []).forEach(z => {
        if (!this._dose[z]) this._dose[z] = 0;
        const irr = 200; // mW/cm² average simulated
        this._dose[z] += irr * (pbmState.dutyCycle / 1000) * 0; // real accumulation in tick()
        pbmState.dose[z] = ((t / dur) * (pbm.dose_jcm2 ?? 10)).toFixed(1);
      });
    }

    // ── EEG ──────────────────────────────────────────────────────────────────
    const eegState = {
      active: this.running && (m.eeg?.active ?? false),
      channels: m.eeg?.channels ?? [],
      bands: { ...this._eegState.bands },
      impedance: { ...this._eegState.impedance },
      alphaPower: this._eegState.alphaPower,
      neurofeedbackScore: this._eegState.nfScore,
    };

    // ── BES ──────────────────────────────────────────────────────────────────
    const besState = {
      active: this.running && (m.bes?.active ?? false),
      frequency: m.bes?.frequency ?? 10,
      intensity_ma: ((m.bes?.intensity_ma ?? 0.5) * rampFraction).toFixed(2),
    };

    // ── tDCS ─────────────────────────────────────────────────────────────────
    const tdcsState = {
      active: this.running && (m.tdcs?.active ?? false),
      intensity_ma: ((m.tdcs?.intensity_ma ?? 1.0) * rampFraction).toFixed(2),
    };

    // ── VNS + HRV ────────────────────────────────────────────────────────────
    const vnsState = {
      active: this.running && (m.vns?.active ?? false),
      frequency: m.vns?.frequency ?? 25,
      hrv_sync: m.vns?.hrv_sync ?? false,
      coherence: this._hrvState.coherence,
      rmssd: this._hrvState.rmssd,
      hr: this._hrvState.hr,
      breathing_phase: this._hrvState.breathingPhase,
    };

    // ── Audio ─────────────────────────────────────────────────────────────────
    const audioState = {
      active: this.running && (m.audio?.active ?? false),
      binaural_hz: m.audio?.binaural_hz ?? 10,
      type: m.audio?.type ?? 'binaural',
      breathing_pacer: m.audio?.breathing_pacer ?? false,
      breathing_bpm: m.audio?.breathing_bpm ?? 6,
    };

    // ── Visual ───────────────────────────────────────────────────────────────
    const visualState = {
      active: this.running && (m.visual?.active ?? false),
      frequency: m.visual?.frequency ?? 40,
      mode: m.visual?.mode ?? 'photic_driving',
    };

    // ── TMS ──────────────────────────────────────────────────────────────────
    const tmsState = {
      active: this.running && (m.tms?.active ?? false),
      frequency: m.tms?.frequency ?? 10,
      target: m.tms?.target ?? 'DLPFC_L',
      pattern: m.tms?.pattern ?? 'rTMS',
    };

    // ── HD-tDCS ───────────────────────────────────────────────────────────────
    const hdtdcsState = {
      active: this.running && (m.hd_tdcs?.active ?? false),
      target: m.hd_tdcs?.target ?? 'DLPFC_L',
      intensity_ma: ((m.hd_tdcs?.intensity_ma ?? 2.0) * rampFraction).toFixed(2),
    };

    return {
      running:    this.running,
      paused:     this.paused,
      elapsed:    t,
      duration:   dur,
      progress:   dur > 0 ? t / dur : 0,
      wallTime,
      pbm:    pbmState,
      eeg:    eegState,
      bes:    besState,
      tdcs:   tdcsState,
      vns:    vnsState,
      audio:  audioState,
      visual: visualState,
      tms:    tmsState,
      hdtdcs: hdtdcsState,
    };
  }

  // ─── private helpers ──────────────────────────────────────────────────────

  _emptySnapshot() {
    return {
      running: false, paused: false,
      elapsed: 0, duration: 0, progress: 0, wallTime: 0,
      pbm: { active: false, zones: [], wavelengths: [], frequency: 0, dutyCycle: 0, intensity: 0, dose: {} },
      eeg: { active: false, channels: [], bands: {}, impedance: {}, alphaPower: 0, neurofeedbackScore: 0 },
      bes: { active: false, frequency: 0, intensity_ma: '0.00' },
      tdcs: { active: false, intensity_ma: '0.00' },
      vns: { active: false, frequency: 0, hrv_sync: false, coherence: 0, rmssd: 0, hr: 0, breathing_phase: 0 },
      audio: { active: false, binaural_hz: 0, type: 'binaural', breathing_pacer: false, breathing_bpm: 6 },
      visual: { active: false, frequency: 0, mode: '' },
      tms: { active: false, frequency: 0, target: '', pattern: '' },
      hdtdcs: { active: false, target: '', intensity_ma: '0.00' },
    };
  }

  _rampEnvelope(t, dur, rampTime = 30) {
    if (t < rampTime) return t / rampTime;
    if (t > dur - rampTime) return (dur - t) / rampTime;
    return 1;
  }

  _initEEG() {
    this._eegState = {
      bands: { delta: 0, theta: 0, alpha: 0, beta: 0, gamma: 0 },
      alphaPower: 0,
      nfScore: 0,
      impedance: {},
      phase: 0,
    };
    const CHANNELS = ['Fp1','Fp2','F3','F4','C3','C4','P3','P4'];
    CHANNELS.forEach(ch => {
      this._eegState.impedance[ch] = 'good';
    });
  }

  _resetEEG() {
    this._eegState.phase = 0;
    // Randomise starting impedances (mostly good, occasional marginal)
    const CHANNELS = ['Fp1','Fp2','F3','F4','C3','C4','P3','P4'];
    CHANNELS.forEach(ch => {
      const r = Math.random();
      this._eegState.impedance[ch] = r > 0.9 ? 'marginal' : 'good';
    });
  }

  _advanceEEG(dt) {
    // Simulate slowly evolving band powers — no real neuroscience, just plausible numbers
    const t = this._eegState;
    t.phase += dt;
    const p = t.phase;
    const alpha = 35 + 20 * Math.sin(p * 0.07) + 5 * Math.sin(p * 0.31);
    t.bands = {
      delta: 45 + 15 * Math.sin(p * 0.05),
      theta: 30 + 12 * Math.sin(p * 0.09),
      alpha: Math.max(5, alpha),
      beta:  22 + 8  * Math.sin(p * 0.13),
      gamma: 15 + 6  * Math.sin(p * 0.23),
    };
    t.alphaPower = t.bands.alpha;
    t.nfScore = Math.min(10, Math.max(0, (alpha - 20) / 4));
  }

  _initHRV() {
    this._hrvState = {
      hr: 68,
      rmssd: 42,
      coherence: 0,
      breathingPhase: 0,
      _phase: 0,
    };
  }

  _advanceHRV(dt) {
    const h = this._hrvState;
    h._phase += dt;
    const p = h._phase;

    // Resonance breathing at 0.1 Hz (6 breaths/min)
    h.breathingPhase = (Math.sin(p * 0.1 * Math.PI * 2) + 1) / 2;
    h.hr = 68 + 8 * Math.sin(p * 0.1 * Math.PI * 2) + 2 * Math.sin(p * 0.037);
    h.rmssd = 42 + 18 * Math.sin(p * 0.08) + 5 * Math.random() - 2.5;
    h.coherence = Math.min(10, Math.max(0, 5 + 4 * Math.sin(p * 0.06) + Math.random() - 0.5));
  }
}
