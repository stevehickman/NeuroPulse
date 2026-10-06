// The wavelength-rules and `start` pieces that exercise the web-only serializer
// (app/web/src/lib/nppsSerializer.ts). The rest of the wavelength-rules tests, which
// need only common/, are in common/lib/wavelengthRules.test.ts.

import { describe, it, expect } from 'vitest';
import type { NPWavelengthRules } from '../../../../common/lib/wavelengthRules';
import { parseNPPS, parseNPPSFile } from '../../../../common/lib/nppsParser';
import { serializeNPPS, serializeWavelengthRules } from '../../../../common/lib/nppsSerializer';

describe('the wavelength_rules block', () => {
  it('round-trips through the serializer', () => {
    const r: NPWavelengthRules = {
      name: 'Lab A', level: 'user', description: 'looser red',
      channels: [{ element: 'led_660', nominalNm: 660, minNm: 630, maxNm: 680 }],
    };
    const back = parseNPPSFile(serializeWavelengthRules(r)).wavelengthRules[0];
    expect(back).toEqual(r);
  });
});

describe('the modality `start` field', () => {
  const src = (start: string) => `protocol "Series" {
    duration: 8m
    pbm_transcranial {
        wavelength: "810nm"
        irradiance: 250mW_cm2
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
