/**
 * The share form through the TypeScript wrapper (NP-SW-001 SWR-WO-07, OI-PRIV-11 part 2). The rule is the Rust core's and
 * is tested there (common/npps-core/tests/share.rs); this holds the wrapper to it.
 */
import { beforeAll, describe, expect, it } from 'vitest';
import { initNppsCore, NppsRefusal, nppsSerializeShare } from './nppsCore';
import { parseNPPS, parseNPPSLimits } from './nppsParser';
import { serializeNPPS, serializeNPPSLimits, shareNPPS, shareNPPSLimits } from './nppsSerializer';

const SRC = `protocol "Evening" {
  id: "p1"
  family_id: "11111111-2222-3333-4444-555555555555"
  author: "Alex"
  used: true
  conditions: ["Insomnia"]
  duration: 10m
  pbm_transcranial {
    zones: [All]
    wavelength: "660nm"
    irradiance: 200mW_cm2
  }
}`;

const LIMITS = `limits "Clinic helmet" {
  level: helmet
  helmet_id: "NP-000123"
  tdcs {
    max_intensity: 1mA
  }
}`;

beforeAll(async () => { await initNppsCore(); });

describe('the share form', () => {
  // @verifies SWR-WO-07#a
  it('writes no used flag, and the full form still does', () => {
    const entries = parseNPPS(SRC);
    expect(serializeNPPS(entries)).toContain('used: true');
    const shared = shareNPPS(entries);
    expect(shared).not.toContain('used');
    expect(shared).toContain('author: "Alex"');
    expect(shared).toContain('"Insomnia"');
  });

  // @verifies SWR-WO-07#b
  it('writes new ids, and two shares of one protocol do not match', () => {
    const entries = parseNPPS(SRC);
    const ids = (t: string) => t.split('\n').map(l => l.trim()).filter(l => l.startsWith('id: ') || l.startsWith('family_id: '));
    const a = ids(shareNPPS(entries));
    const b = ids(shareNPPS(entries));
    expect(a.length).toBe(2);
    expect(a.join()).not.toContain('p1');
    expect(a.join()).not.toContain('11111111-2222');
    expect(a.some(x => b.includes(x))).toBe(false);
  });

  // @verifies SWR-WO-07#c
  it('a shared limits set keeps its level and names no helmet', () => {
    const limits = parseNPPSLimits(LIMITS)!;
    expect(serializeNPPSLimits(limits)).toContain('NP-000123');
    const shared = shareNPPSLimits(limits);
    expect(shared).not.toContain('helmet_id');
    expect(shared).not.toContain('NP-000123');
    expect(parseNPPSLimits(shared)!.level).toBe('helmet');
  });

  // @verifies SWR-WO-07#e
  it('is refused without 32 random bytes', () => {
    expect(() => nppsSerializeShare([], new Uint8Array(8))).toThrow(NppsRefusal);
  });
});
