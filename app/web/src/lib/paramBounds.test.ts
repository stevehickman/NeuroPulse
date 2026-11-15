import { describe, it, expect } from 'vitest';
import { readFileSync, readdirSync } from 'node:fs';
import { join } from 'node:path';
import {
  allBounds,
  controlKind,
  needsMagnifier,
  fineRange,
  inBounds,
  stepsAcross,
  SLIDER_TRACK_PX,
  PARAM_BOUNDS,
  INTERVAL_BOUNDS,
  type ParamBound,
  type Basis,
} from './paramBounds';

const BASES: Basis[] = ['natural', 'hardware', 'practical', 'safety', 'unestablished'];
const slider = (min: number, max: number, step: number): ParamBound => ({
  min: { value: min, basis: 'natural', source: 'test' },
  max: { value: max, basis: 'natural', source: 'test' },
  step,
});

describe('the bounds file', () => {
  // @verifies SWR-UI-01#a
  it('gives every edge a basis and a source, and explains every side left open', () => {
    const all = allBounds();
    expect(all.length).toBeGreaterThan(60);
    for (const { key, bound } of all) {
      expect(bound.step, `${key} step`).toBeGreaterThan(0);
      for (const edge of [bound.min, bound.max]) {
        if (!edge) continue;
        expect(BASES, `${key} basis`).toContain(edge.basis);
        expect(edge.source.trim().length, `${key} source`).toBeGreaterThan(0);
      }
      if (bound.min && bound.max) expect(bound.min.value, `${key} min < max`).toBeLessThan(bound.max.value);
      if (!bound.min || !bound.max) {
        expect((bound.unbounded ?? '').trim().length, `${key} says why a side is open`).toBeGreaterThan(0);
      }
    }
  });

  // @verifies SWR-UI-01#a
  it('keeps the bounds that were removed as open sides, each with its reason', () => {
    const bound = (b: ParamBound): ParamBound => b; // the table keeps its literal types, so read a side that may be absent through the interface
    expect(bound(PARAM_BOUNDS.audio.carrier).max).toBeUndefined();
    expect(bound(PARAM_BOUNDS.audio.beatRate).max).toBeUndefined();
    expect(bound(PARAM_BOUNDS.visual.emdrCadence).max).toBeUndefined();
    expect(bound(PARAM_BOUNDS.tdcs.ramp).max).toBeUndefined();
    expect(bound(INTERVAL_BOUNDS.onSeconds).max).toBeUndefined();
    for (const b of [PARAM_BOUNDS.audio.carrier, PARAM_BOUNDS.pbmTranscranial.frequency, PARAM_BOUNDS.vns.frequency]) {
      expect(b.unbounded).toMatch(/\S/);
    }
  });

  // @verifies SWR-UI-01#a
  it('is the only place a component states a range', () => {
    const dir = join(import.meta.dirname, '..', 'components');
    const offenders: string[] = [];
    for (const f of readdirSync(dir).filter(n => n.endsWith('.tsx'))) {
      if (f === 'ParamInputs.tsx') continue; // it passes the file's numbers on to the inputs
      const text = readFileSync(join(dir, f), 'utf8');
      if (/\b(min|max)=\{-?\d/.test(text) || /\b(min|max)="-?\d/.test(text)) offenders.push(`${f}: a numeric min or max`);
      if (/type="range"/.test(text)) offenders.push(`${f}: its own slider`);
    }
    expect(offenders).toEqual([]);
  });
});

describe('which control a range gets', () => {
  // @verifies SWR-UI-01#b
  it('is a slider only when both sides are bounded, and a typed field otherwise', () => {
    expect(controlKind(slider(0, 85, 0.5))).toBe('slider');
    expect(controlKind({ min: slider(0, 1, 1).min, step: 1 })).toBe('field');
    expect(controlKind({ max: slider(0, 1, 1).max, step: 1 })).toBe('field');
    expect(controlKind({ step: 1 })).toBe('field');
    expect(stepsAcross({ step: 1 })).toBe(Infinity);
  });

  // @verifies SWR-UI-01#b
  it('follows from the table: the audio carrier and the tDCS ramp are typed, the volume and the duty are sliders', () => {
    expect(controlKind(PARAM_BOUNDS.audio.carrier)).toBe('field');
    expect(controlKind(PARAM_BOUNDS.tdcs.ramp)).toBe('field');
    expect(controlKind(PARAM_BOUNDS.audio.volume)).toBe('slider');
    expect(controlKind(PARAM_BOUNDS.pbmTranscranial.duty)).toBe('slider');
  });
});

describe('the magnifier', () => {
  // @verifies SWR-UI-01#d
  it('is needed exactly when the range has more steps than the track has pixels', () => {
    expect(needsMagnifier(slider(0, SLIDER_TRACK_PX, 1))).toBe(false);
    expect(needsMagnifier(slider(0, SLIDER_TRACK_PX + 1, 1))).toBe(true);
    expect(needsMagnifier(PARAM_BOUNDS.pbmTranscranial.duty)).toBe(false); // 25 steps
    expect(needsMagnifier(PARAM_BOUNDS.pbmTranscranial.irradiance)).toBe(true); // 400 steps
  });

  // @verifies SWR-UI-01#d
  it('shows a window of one step per pixel around the value, inside the bounds and on the step grid', () => {
    const b = PARAM_BOUNDS.pbmTranscranial.irradiance; // 0..400, step 1
    for (const v of [0, 3, 137, 250, 399, 400]) {
      const w = fineRange(v, b);
      expect(w.max - w.min, `window at ${v}`).toBe(SLIDER_TRACK_PX);
      expect(w.min).toBeGreaterThanOrEqual(0);
      expect(w.max).toBeLessThanOrEqual(400);
      expect(v).toBeGreaterThanOrEqual(w.min);
      expect(v).toBeLessThanOrEqual(w.max);
    }
    // A fractional step stays on its grid.
    const f = fineRange(30.3, slider(0, 100, 0.05));
    expect(Math.abs(f.min / 0.05 - Math.round(f.min / 0.05))).toBeLessThan(1e-6);
    expect(f.max - f.min).toBeCloseTo(SLIDER_TRACK_PX * 0.05, 6);
  });

  // @verifies SWR-UI-01#d
  it('leaves a range that already fits whole', () => {
    expect(fineRange(10, slider(0, 25, 1))).toEqual({ min: 0, max: 25 });
  });
});

describe('a value outside its bounds', () => {
  // @verifies SWR-UI-01#f
  it('is reported as outside, and an open side never fails', () => {
    expect(inBounds(86, PARAM_BOUNDS.audio.volume)).toBe(false);
    expect(inBounds(-1, PARAM_BOUNDS.audio.carrier)).toBe(false);
    expect(inBounds(1e9, PARAM_BOUNDS.audio.carrier)).toBe(true);
  });
});
