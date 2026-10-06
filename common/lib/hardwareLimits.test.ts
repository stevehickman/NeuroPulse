/**
 * common/npps/hardware-limits.json is the one source of every hardware ceiling (OI-NPPS-CORE-01): the shared
 * validator reads it (common/npps-core/src/validate.rs) and `bun scripts/sync-hardware-limits.ts` writes the web, Android
 * and iOS constants from it (CI runs it with --check, so a stale output fails there). This holds what that cannot: the
 * file is well-formed, every ceiling the validator reads is in it, and the generated web constants are the same numbers.
 */
import { describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { NPHardwareLimits } from './hardwareLimits.generated';

const read = (rel: string) => readFileSync(new URL(`../../${rel}`, import.meta.url), 'utf8');

interface Limit { name: string; value: number; int?: boolean }
const limits = (JSON.parse(read('common/npps/hardware-limits.json')) as { limits: Limit[] }).limits;

/** Names the validator reads: `hw("name")`. */
const used = [...new Set([...read('common/npps-core/src/validate.rs').matchAll(/\bhw\("([A-Za-z0-9]+)"\)/g)].map(m => m[1]!))].sort();

describe('hardware limits', () => {
  it('names are unique, and one spelling serves every platform (no per-platform overrides)', () => {
    const names = limits.map(l => l.name);
    expect(new Set(names).size).toBe(names.length);
    for (const l of limits) expect(Object.keys(l).filter(k => !['name', 'value', 'int', 'note'].includes(k)), l.name).toEqual([]);
    for (const n of names) expect(n, `${n} is not camelCase`).toMatch(/^[a-z][A-Za-z0-9]*$/);
  });

  it('every ceiling the validator reads is in the file', () => {
    expect(used.length).toBeGreaterThan(25);
    const names = new Set(limits.map(l => l.name));
    expect(used.filter(n => !names.has(n))).toEqual([]);
  });

  it('the generated web constants hold the file\'s numbers (vacuity guard: all of them, not a subset)', () => {
    expect(Object.keys(NPHardwareLimits).sort()).toEqual(limits.map(l => l.name).sort());
    for (const l of limits) expect(NPHardwareLimits[l.name as keyof typeof NPHardwareLimits], l.name).toBe(l.value);
  });

  it('a whole-number figure is marked int, and only those', () => {
    for (const l of limits) if (l.int) expect(Number.isInteger(l.value), l.name).toBe(true);
  });
});
