/**
 * resolveLimits, through the TypeScript wrapper over the shared core (OI-NPPS-CORE-01), against the web function it
 * replaced (app/NeurOneShared/TestData/npps-resolve-golden.json, frozen). The Rust, Kotlin and C# tests hold the same file.
 */
import { describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { resolveLimits, resolveLimitsWithSources, type NPLimitsSet } from '../types/limits';

const golden = JSON.parse(readFileSync(new URL('../../app/NeurOneShared/TestData/npps-resolve-golden.json', import.meta.url), 'utf8'));

describe('resolveLimits', () => {
  it('resolves every tier combination as the web function did', () => {
    expect(golden.cases.length).toBeGreaterThanOrEqual(300);
    for (const c of golden.cases) {
      const r = resolveLimits(c.global ?? undefined, c.helmet ?? undefined, c.individual ?? undefined);
      const { id, name, description, createdAt, modifiedAt, level, ...blocks } = r;
      expect(level).toBe('global');
      expect(JSON.parse(JSON.stringify(blocks)), c.name).toEqual(c.expected);
    }
  });

  it('gives the resolved set the identity the app\'s models carry, and names each value\'s tier', () => {
    const base = { id: 'x', name: 'x', description: '', createdAt: '', modifiedAt: '' };
    const g = { ...base, level: 'global', besTacs: { maxIntensityMilliamps: 1, maxFrequencyHz: 40 } } as NPLimitsSet;
    const i = { ...base, level: 'individual', besTacs: { maxIntensityMilliamps: 0.5 } } as NPLimitsSet;
    const { limits, sources } = resolveLimitsWithSources(g, undefined, i);
    expect(limits.id).toBe('resolved');
    expect(limits.besTacs).toEqual({ maxIntensityMilliamps: 0.5, maxFrequencyHz: 40 });
    expect(sources).toEqual({ besTacs: { maxIntensityMilliamps: 'individual', maxFrequencyHz: 'global' } });
  });
});
