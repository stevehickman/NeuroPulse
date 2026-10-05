// Protocol eligibility for per-wavelength PBM blocks (NP-NPPS-REF-001 §4.1a, §7a).
//
// The operator-facing contract: a protocol is offered only when the fitted
// helmet can deliver every block, under the wavelength rules in force. When a
// module swap would fix it, the shortfall names the sockets and the parts. When
// no module can deliver the stated wavelength, it says so and points at the
// rules, instead of naming parts that would not help.

import { describe, it, expect } from 'vitest';
import { readFileSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { NPSimulatedInventoryProvider, type NPInventoryPreset } from '../../../../common/lib/helmetInventory';
import { evaluateProtocol } from './protocolEligibility';
import { DEFAULT_WAVELENGTH_RULES, resolveWavelengthRules } from '../../../../common/lib/wavelengthRules';
import { parseNPPS, parseNPPSFile, buildNamespace } from '../../../../common/lib/nppsParser';
import type { NPProtocolDefinition } from '../../../../common/types/protocol';

const root = join(dirname(fileURLToPath(import.meta.url)), '..', '..', '..', '..');
const zones = buildNamespace([
  parseNPPSFile(readFileSync(join(root, 'protocols', 'predefined', '00-zones.npps'), 'utf-8')),
]).namespace.zones;

const helmet = (preset: NPInventoryPreset) => new NPSimulatedInventoryProvider(preset).getInventory()!;

function protocol(...blocks: string[]): NPProtocolDefinition {
  const [entry] = parseNPPS(`protocol "T" {\n    duration: 10m\n${blocks.join('\n')}\n}`);
  if (entry.kind !== 'single') throw new Error('expected a protocol');
  return entry.protocol;
}

const pbm = (wavelength: string, zone = 'Frontal Left') => `    pbm_transcranial {
        wavelength: "${wavelength}"
        irradiance: 201.5mW_cm2
        frequency: 40Hz
        duty_cycle: 25%
        zones: ["${zone}"]
    }`;

describe('per-wavelength eligibility', () => {
  it('an 810 nm block runs on a 660/808 helmet', () => {
    const e = evaluateProtocol(protocol(pbm('810nm')), helmet('pbm-only'), zones);
    expect(e.eligible).toBe(true);
    expect(e.shortfalls).toEqual([]);
  });

  it('a 1070 nm block does not, and the shortfall names the parts that would fix it', () => {
    const e = evaluateProtocol(protocol(pbm('1070nm')), helmet('pbm-only'), zones);
    expect(e.eligible).toBe(false);
    const [s] = e.shortfalls;
    expect(s.wavelengthProblem).toBeUndefined();
    expect(s.sockets.length).toBeGreaterThan(0);
    expect(s.sockets[0].missingElements).toEqual([['led_1064']]);
    expect(s.sockets[0].fitted).toBe('ZM-PBM-DUAL');
    expect(s.sockets[0].candidateModules).toEqual(expect.arrayContaining(['ZM-PBM-TRI', 'ZM-COMBO-TRI']));
    expect(s.sockets[0].candidateModules).not.toContain('ZM-PBM-DUAL');
  });

  it('the same 1070 nm block runs once the tri-wavelength parts are fitted', () => {
    expect(evaluateProtocol(protocol(pbm('1070nm')), helmet('full-t2'), zones).eligible).toBe(true);
  });

  it('an 850 nm block is unsupported under the defaults: no part is suggested, the rules are', () => {
    const e = evaluateProtocol(protocol(pbm('850nm')), helmet('full-t2'), zones);
    expect(e.eligible).toBe(false);
    expect(e.shortfalls[0].wavelengthProblem).toEqual({
      value: '850nm', reason: 'unmapped', rulesName: DEFAULT_WAVELENGTH_RULES.name,
    });
    expect(e.shortfalls[0].sockets).toEqual([]);
    expect(e.summary).toMatch(/850nm/);
  });

  it('loosening the rules makes the same 850 nm block runnable', () => {
    const lab = resolveWavelengthRules(DEFAULT_WAVELENGTH_RULES, {
      name: 'Lab', level: 'user',
      channels: [{ element: 'led_808', nominalNm: 808, minNm: 798, maxNm: 860 }],
    });
    expect(evaluateProtocol(protocol(pbm('850nm')), helmet('pbm-only'), zones, undefined, lab).eligible).toBe(true);
  });

  it('tightening the rules makes a previously runnable block unsupported', () => {
    const exact = resolveWavelengthRules(DEFAULT_WAVELENGTH_RULES, {
      name: 'Exact', level: 'user',
      channels: [{ element: 'led_808', nominalNm: 808, minNm: 808, maxNm: 808 }],
    });
    const e = evaluateProtocol(protocol(pbm('810nm')), helmet('pbm-only'), zones, undefined, exact);
    expect(e.eligible).toBe(false);
    expect(e.shortfalls[0].wavelengthProblem?.reason).toBe('unmapped');
  });

  it('a value that is not a wavelength is its own failure', () => {
    const e = evaluateProtocol(protocol(pbm('red')), helmet('full-t2'), zones);
    expect(e.shortfalls[0].wavelengthProblem?.reason).toBe('invalid');
  });

  it('a retired combined name never reaches eligibility: the parser refuses it', () => {
    expect(() => protocol(pbm('660_808nm'))).toThrow(/retired/);
  });

  it('each PBM block is reported on its own', () => {
    const e = evaluateProtocol(protocol(pbm('810nm'), pbm('1070nm', 'Frontal Right')), helmet('pbm-only'), zones);
    expect(e.shortfalls.map(s => s.blockIndex)).toEqual([1]);
  });
});
