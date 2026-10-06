/**
 * common/npps/constants.json is the one source of every constant more than one runtime needs (OI-NPPS-CORE-01). The Rust
 * core takes them at build time; `bun scripts/sync-constants.ts` writes the rest (CI runs it with --check, so a stale output
 * fails there). This holds what that cannot: the file's own rules, and that the generated web constants are its numbers.
 */
import { describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import * as generated from './constants.generated';

const read = (rel: string) => readFileSync(new URL(`../../${rel}`, import.meta.url), 'utf8');

interface Constant { name: string; value: number; int?: boolean }
interface Group { name: string; rust: string; targets: string[]; uuidBase?: string; constants: Constant[] }
const groups = (JSON.parse(read('common/npps/constants.json')) as { groups: Group[] }).groups;

const UPPER_SNAKE = /^[A-Z][A-Z0-9]*(_[A-Z0-9]+)*$/;

describe('shared constants', () => {
  it('every constant is UPPER_SNAKE_CASE, and one name means one thing across the groups', () => {
    const names = groups.flatMap(g => g.constants.map(c => c.name));
    expect(names.filter(n => !UPPER_SNAKE.test(n))).toEqual([]);
    expect(new Set(names).size).toBe(names.length);
  });

  it('a whole-number figure is marked int, and only those', () => {
    for (const g of groups) for (const c of g.constants) if (c.int) expect(Number.isInteger(c.value), `${g.name}.${c.name}`).toBe(true);
  });

  it('the generated web constants are the file\'s, group by group (vacuity guard: every group aimed at ts, every constant)', () => {
    const forTs = groups.filter(g => g.targets.includes('ts'));
    expect(forTs.length).toBeGreaterThanOrEqual(2);
    // A group with a uuidBase also gets its UUID strings in a sibling container (GattIds -> GattUuidStrings).
    const uuidTables = forTs.filter(g => g.uuidBase).map(g => g.name.replace(/Ids$/, 'UuidStrings'));
    expect(Object.keys(generated).sort()).toEqual([...forTs.map(g => g.name), ...uuidTables].sort());
    for (const g of forTs) {
      const got = (generated as Record<string, Record<string, number>>)[g.name]!;
      expect(Object.keys(got).sort()).toEqual(g.constants.map(c => c.name).sort());
      for (const c of g.constants) expect(got[c.name], `${g.name}.${c.name}`).toBe(c.value);
    }
  });

  it('every GATT UUID is the base with its id, and no id or UUID repeats', () => {
    const gatt = groups.find(g => g.name === 'GattIds')!;
    const uuids = (generated as Record<string, Record<string, string>>).GattUuidStrings!;
    for (const c of gatt.constants) {
      expect(uuids[c.name], c.name).toBe(gatt.uuidBase!.replace('-0000-', `-${c.value.toString(16).toUpperCase().padStart(4, '0')}-`).toLowerCase());
    }
    expect(new Set(gatt.constants.map(c => c.value)).size).toBe(gatt.constants.length);
  });

  it('the hardware limits the editors and the validator share are in the file', () => {
    const hw = groups.find(g => g.name === 'NPHardwareLimits')!;
    expect(hw.constants.length).toBeGreaterThan(30);
    expect(generated.NPHardwareLimits.PBM_DEEP_MAX_MW_CM2).toBe(1000);
    expect(generated.NPHardwareLimits.CLINICAL_TACS_MAX_CHANNELS).toBe(21);
  });
});
