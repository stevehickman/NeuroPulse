// PBM wavelength mapping rules (NP-NPPS-REF-001 §3.1a, §7a).
//
// The rules decide which emitter channel may deliver a protocol's stated
// wavelength. The tests below pin the three properties the feature rests on:
// a wavelength no rule accepts maps to NOTHING (never the nearest channel);
// the answer never depends on rule order; and the authored defaults file and
// the TypeScript copy cannot drift apart.

import { describe, it, expect } from 'vitest';
import fs from 'node:fs';
import path from 'node:path';
import {
  DEFAULT_WAVELENGTH_RULES,
  mapWavelength,
  parsePbmWavelength,
  pbmRequirementGroups,
  resolvePbmChannels,
  resolveWavelengthRules,
  validateWavelengthRules,
  type NPWavelengthRules,
} from './wavelengthRules';
import { parseNPPS, parseNPPSFile, NPPSParseError } from './nppsParser';
import { serializeNPPS, serializeWavelengthRules } from './nppsSerializer';

const DEFAULTS_FILE = path.resolve(__dirname, '../../../../protocols/predefined/00-wavelength-rules.npps');

describe('parsePbmWavelength', () => {
  it('keeps the three legacy channel names', () => {
    expect(parsePbmWavelength('660_808nm')).toMatchObject({ kind: 'legacy', elements: ['led_660', 'led_808'] });
    expect(parsePbmWavelength('1064nm')).toMatchObject({ kind: 'legacy', elements: ['led_1064'] });
    expect(parsePbmWavelength('660_808_1064nm')).toMatchObject({ kind: 'legacy' });
  });

  it('reads one wavelength, including a decimal', () => {
    expect(parsePbmWavelength('810nm')).toEqual({ kind: 'single', value: '810nm', nm: 810 });
    expect(parsePbmWavelength('632.8nm')).toMatchObject({ kind: 'single', nm: 632.8 });
  });

  it('rejects anything else', () => {
    for (const v of ['810', 'nm', '810 nm', '660_808', '-810nm', '0nm', 'abc', '']) {
      expect(parsePbmWavelength(v).kind, v).toBe('invalid');
    }
  });
});

describe('mapWavelength under the shipped defaults', () => {
  const cases: Array<[number, string | null]> = [
    [660, 'led_660'], [670, 'led_660'], [650, 'led_660'], [680, 'led_660'],
    [649, null], [640, null], [633, null], [681, null],
    [808, 'led_808'], [810, 'led_808'], [820, 'led_808'], [823, 'led_808'], [830, 'led_808'],
    [797, null], [850, null],
    [1064, 'led_1064'], [1060, 'led_1064'], [1070, 'led_1064'], [1080, null],
    [1170, null],   // the deep channel is its own modality, never a transcranial mapping
  ];
  for (const [nm, want] of cases) {
    it(`${nm} nm → ${want ?? 'unsupported'}`, () => {
      expect(mapWavelength(nm, DEFAULT_WAVELENGTH_RULES)).toBe(want);
    });
  }
});

describe('user rules', () => {
  const looser: NPWavelengthRules = {
    name: 'Lab A — looser red',
    level: 'user',
    channels: [{ element: 'led_660', nominalNm: 660, minNm: 630, maxNm: 680 }],
  };
  const stricter: NPWavelengthRules = {
    name: 'Lab B — exact 808 only',
    level: 'user',
    channels: [{ element: 'led_808', nominalNm: 808, minNm: 808, maxNm: 808 }],
  };

  it('a user rule replaces only the channel it names', () => {
    const r = resolveWavelengthRules(DEFAULT_WAVELENGTH_RULES, looser);
    expect(r.level).toBe('user');
    expect(mapWavelength(640, r)).toBe('led_660');       // loosened
    expect(mapWavelength(810, r)).toBe('led_808');       // untouched default
    expect(mapWavelength(1064, r)).toBe('led_1064');
  });

  it('tightening can make a previously supported wavelength unsupported', () => {
    const r = resolveWavelengthRules(DEFAULT_WAVELENGTH_RULES, stricter);
    expect(mapWavelength(808, r)).toBe('led_808');
    expect(mapWavelength(810, r)).toBeNull();
  });

  it('never mutates the defaults', () => {
    const before = JSON.stringify(DEFAULT_WAVELENGTH_RULES);
    resolveWavelengthRules(DEFAULT_WAVELENGTH_RULES, looser);
    expect(JSON.stringify(DEFAULT_WAVELENGTH_RULES)).toBe(before);
  });

  it('overlapping windows: nearest nominal wins, and rule order does not matter', () => {
    const wide = (order: 'ab' | 'ba'): NPWavelengthRules => {
      const a = { element: 'led_660' as const, nominalNm: 660, minNm: 600, maxNm: 900 };
      const b = { element: 'led_808' as const, nominalNm: 808, minNm: 600, maxNm: 900 };
      return { name: 'wide', level: 'user', channels: order === 'ab' ? [a, b] : [b, a] };
    };
    for (const order of ['ab', 'ba'] as const) {
      expect(mapWavelength(700, wide(order)), order).toBe('led_660');
      expect(mapWavelength(780, wide(order)), order).toBe('led_808');
      expect(mapWavelength(734, wide(order)), `${order} tie`).toBe('led_660');   // equidistant → first listed channel
    }
  });
});

describe('resolvePbmChannels / pbmRequirementGroups', () => {
  it('a single wavelength becomes one requirement group', () => {
    expect(pbmRequirementGroups('810nm', DEFAULT_WAVELENGTH_RULES)).toEqual([['led_808']]);
  });
  it('a legacy name keeps its groups', () => {
    expect(pbmRequirementGroups('660_808nm', DEFAULT_WAVELENGTH_RULES)).toEqual([['led_660'], ['led_808']]);
  });
  it('unmapped and invalid are distinct failures, and neither yields a group', () => {
    expect(resolvePbmChannels('850nm', DEFAULT_WAVELENGTH_RULES)).toMatchObject({ ok: false, reason: 'unmapped', requestedNm: 850 });
    expect(resolvePbmChannels('red', DEFAULT_WAVELENGTH_RULES)).toMatchObject({ ok: false, reason: 'invalid' });
    expect(pbmRequirementGroups('850nm', DEFAULT_WAVELENGTH_RULES)).toBeNull();
  });
});

describe('validateWavelengthRules', () => {
  it('accepts the defaults', () => {
    expect(validateWavelengthRules(DEFAULT_WAVELENGTH_RULES)).toEqual([]);
  });
  it('rejects an inverted window, a duplicate channel and a non-positive value', () => {
    const bad: NPWavelengthRules = {
      name: 'bad', level: 'user', channels: [
        { element: 'led_660', nominalNm: 660, minNm: 690, maxNm: 650 },
        { element: 'led_660', nominalNm: 660, minNm: 650, maxNm: 680 },
        { element: 'led_808', nominalNm: 0, minNm: 800, maxNm: 830 },
      ],
    };
    const errs = validateWavelengthRules(bad).join(' | ');
    expect(errs).toMatch(/above max_nm/);
    expect(errs).toMatch(/more than one rule/);
    expect(errs).toMatch(/nominal_nm must be a positive number/);
  });
});

describe('the wavelength_rules block', () => {
  it('the shipped defaults file parses to exactly DEFAULT_WAVELENGTH_RULES', () => {
    const parsed = parseNPPSFile(fs.readFileSync(DEFAULTS_FILE, 'utf8'));
    expect(parsed.entries).toEqual([]);
    expect(parsed.wavelengthRules).toHaveLength(1);
    const { description: _d, ...fromFile } = parsed.wavelengthRules[0];
    expect(fromFile).toEqual(DEFAULT_WAVELENGTH_RULES);
  });

  it('round-trips through the serializer', () => {
    const r: NPWavelengthRules = {
      name: 'Lab A', level: 'user', description: 'looser red',
      channels: [{ element: 'led_660', nominalNm: 660, minNm: 630, maxNm: 680 }],
    };
    const back = parseNPPSFile(serializeWavelengthRules(r)).wavelengthRules[0];
    expect(back).toEqual(r);
  });

  const reject = (body: string, why: RegExp) => {
    expect(() => parseNPPSFile(`wavelength_rules "x" {\n${body}\n}`)).toThrow(why);
  };
  it('refuses an unknown channel', () => reject('channel "led_900" { nominal_nm: 900 min_nm: 890 max_nm: 910 }', /unknown channel/));
  it('refuses a channel missing a bound', () => reject('channel "led_808" { nominal_nm: 808 min_nm: 800 }', /all required/));
  it('refuses an inverted window', () => reject('channel "led_808" { nominal_nm: 808 min_nm: 840 max_nm: 800 }', /above max_nm/));
  it('refuses a level other than global or user', () => reject('level: helmet', /level must be global or user/));
  it('is an NPPSParseError, with a line', () => {
    try { parseNPPSFile('wavelength_rules "x" {\n level: nope\n}'); } catch (e) {
      expect(e).toBeInstanceOf(NPPSParseError);
      return;
    }
    throw new Error('expected a parse error');
  });
});

describe('the modality `start` field', () => {
  const src = (start: string) => `protocol "Series" {
    duration: 8m
    pbm_transcranial {
        wavelength: "810nm"
        intensity: 62%
        frequency: 0Hz
        duty_cycle: 100%
        zones: ["Frontal Right"]
        ${start}
        interval_on: 4m
        interval_off: 0s
        repeat: 1
    }
}`;

  it('parses to startOffsetSeconds and round-trips', () => {
    const [entry] = parseNPPS(src('start: 4m'));
    if (entry.kind !== 'single') throw new Error('expected a protocol');
    expect(entry.protocol.modalities[0].interval.startOffsetSeconds).toBe(240);
    const [again] = parseNPPS(serializeNPPS([entry]));
    if (again.kind !== 'single') throw new Error('expected a protocol');
    expect(again.protocol.modalities[0].interval.startOffsetSeconds).toBe(240);
    expect(again.protocol.modalities[0].modalityParams).toEqual(entry.protocol.modalities[0].modalityParams);
  });

  it('is absent when omitted or zero, so existing protocols serialize unchanged', () => {
    for (const s of ['', 'start: 0s']) {
      const [entry] = parseNPPS(src(s));
      if (entry.kind !== 'single') throw new Error('expected a protocol');
      expect(entry.protocol.modalities[0].interval.startOffsetSeconds).toBeUndefined();
      expect(serializeNPPS([entry])).not.toMatch(/start:/);
    }
  });
});
