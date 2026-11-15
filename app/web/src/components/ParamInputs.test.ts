import { describe, it, expect } from 'vitest';
import { createElement } from 'react';
import { renderToStaticMarkup } from 'react-dom/server';
import { ParamGrid, ParamInput, RangeEditor, SelectField, CheckboxField } from './ParamInputs';
import { PARAM_BOUNDS } from '../lib/paramBounds';

const noop = () => {};
const html = (el: Parameters<typeof renderToStaticMarkup>[0]) => renderToStaticMarkup(el);

describe('RangeEditor', () => {
  // @verifies SWR-UI-01#b,c
  it('draws a slider and a typed field when both sides are bounded', () => {
    const out = html(createElement(RangeEditor, { value: 60, bound: PARAM_BOUNDS.audio.volume, onChange: noop }));
    expect(out).toContain('type="range"');
    expect(out).toContain('type="number"');
    expect(out).toContain('range-magnifier');
  });

  // @verifies SWR-UI-01#b,c
  it('draws only a typed field when a side is open', () => {
    const out = html(createElement(RangeEditor, { value: 440, bound: PARAM_BOUNDS.audio.carrier, onChange: noop }));
    expect(out).not.toContain('type="range"');
    expect(out).toContain('type="number"');
    expect(out).toContain('value="440"');
  });

  // @verifies SWR-UI-01#d
  it('turns the magnifier off where one step is already one pixel', () => {
    const needed = html(createElement(RangeEditor, { value: 100, bound: PARAM_BOUNDS.pbmTranscranial.irradiance, onChange: noop }));
    const notNeeded = html(createElement(RangeEditor, { value: 10, bound: PARAM_BOUNDS.pbmTranscranial.duty, onChange: noop }));
    expect(needed).not.toMatch(/range-magnifier[^>]*disabled/);
    expect(notNeeded).toMatch(/disabled[^>]*range-magnifier|range-magnifier[^>]*disabled/);
  });

  // @verifies SWR-UI-01#f
  it('marks a value outside its bounds and shows it as typed, never clamped', () => {
    const out = html(createElement(RangeEditor, { value: 999, bound: PARAM_BOUNDS.audio.volume, onChange: noop }));
    expect(out).toContain('is-out-of-range');
    expect(out).toContain('aria-invalid="true"');
    expect(out).toContain('value="999"');
  });

  // @verifies SWR-UI-01#c
  it('shows an empty typed field for an optional value that is not set', () => {
    const out = html(createElement(RangeEditor, { value: undefined, optional: true, bound: PARAM_BOUNDS.audio.beatRate, placeholder: 'none', onChange: noop }));
    expect(out).toContain('value=""');
    expect(out).toContain('placeholder="none"');
  });
});

describe('ParamGrid', () => {
  // @verifies SWR-UI-01#e
  it('puts choices first, then typed fields, then sliders, then switches, whatever order they are listed in', () => {
    const out = html(
      createElement(
        ParamGrid,
        null,
        createElement(CheckboxField, { label: 'TOGGLE', value: true, onChange: noop }),
        createElement(ParamInput, { label: 'SLIDER', value: 5, bound: PARAM_BOUNDS.pbmTranscranial.duty, onChange: noop }),
        createElement(ParamInput, { label: 'FIELD', value: 440, bound: PARAM_BOUNDS.audio.carrier, onChange: noop }),
        createElement(SelectField, { label: 'CHOICE', value: 'a', options: [{ value: 'a', label: 'A' }], onChange: noop }),
      ),
    );
    const at = (s: string) => out.indexOf(s);
    expect(at('CHOICE')).toBeGreaterThan(-1);
    expect(at('CHOICE')).toBeLessThan(at('FIELD'));
    expect(at('FIELD')).toBeLessThan(at('SLIDER'));
    expect(at('SLIDER')).toBeLessThan(at('TOGGLE'));
    expect(out).toContain('param-grid--slider');
    expect(out).toContain('param-grid--field');
  });
});
