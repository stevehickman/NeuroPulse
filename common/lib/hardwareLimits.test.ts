/**
 * The hardware ceilings the shared validator checks are the ones every app shows.
 *
 * `common/npps/hardware-limits.json` is what the shared NPPS core's validator reads (common/npps-core/src/validate.rs,
 * OI-NPPS-CORE-01). The editors still read their own copies for slider ranges: app/web/src/lib/hardwareLimits.ts,
 * app/android/core/.../NPHardwareLimits.kt and app/ios/NeurOne/Protocol/NPHardwareLimits.swift. A ceiling that differs
 * between the validator and an editor would let a user drag a slider to a value the validator then refuses, or the
 * reverse, so this fails when any copy disagrees with the JSON on a ceiling the validator reads.
 *
 * A copy that does not declare a name is not an error for iOS and Android (they hold only the limits their screens
 * use); the web copy must declare all of them, because it is the reference the JSON was written from.
 */
import { describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';

const read = (rel: string) => readFileSync(new URL(`../../${rel}`, import.meta.url), 'utf8');

const json = JSON.parse(read('common/npps/hardware-limits.json')) as Record<string, number | string>;
delete json['$comment'];

/** Names the validator reads: `hw("name")`. */
const used = [...new Set([...read('common/npps-core/src/validate.rs').matchAll(/\bhw\("([A-Za-z0-9]+)"\)/g)].map(m => m[1]!))].sort();

const snake = (camel: string) => camel.replace(/([a-z0-9])([A-Z])/g, '$1_$2').replace(/([A-Z])([A-Z][a-z])/g, '$1_$2').toUpperCase();

/** A numeric literal, including `250e-6`. */
const NUM = String.raw`(-?[0-9]+(?:\.[0-9]+)?(?:[eE][-+]?[0-9]+)?)`;

describe('hardware ceilings', () => {
  it('the validator reads at least the ceilings it is known to, all of them in the JSON', () => {
    expect(used.length).toBeGreaterThan(25);
    for (const name of used) expect(typeof json[name], `${name} is read by validate.rs but not in hardware-limits.json`).toBe('number');
  });

  it('the JSON holds only ceilings the validator reads (a ceiling nothing reads is one nothing checks)', () => {
    expect(Object.keys(json).filter(k => !used.includes(k))).toEqual([]);
  });

  it('the web copy declares every one, with the same number', () => {
    const web = read('app/web/src/lib/hardwareLimits.ts');
    for (const name of used) {
      const m = new RegExp(String.raw`\b${name}:\s*${NUM}`).exec(web);
      expect(m, `web hardwareLimits.ts has no ${name}`).not.toBeNull();
      expect(Number(m![1]), `web hardwareLimits.ts: ${name}`).toBe(json[name]);
    }
  });

  it('iOS and Android hold the same number wherever they declare one', () => {
    const ios = read('app/ios/NeurOne/Protocol/NPHardwareLimits.swift');
    const android = read('app/android/core/src/main/kotlin/life/neurone/core/protocol/NPHardwareLimits.kt');
    let checked = 0;
    for (const name of used) {
      const s = new RegExp(String.raw`static let ${name}\b[^=\n]*=\s*${NUM}`).exec(ios);
      if (s) { checked++; expect(Number(s[1]), `iOS NPHardwareLimits.swift: ${name}`).toBe(json[name]); }
      const k = new RegExp(String.raw`const val ${snake(name)}\b[^=\n]*=\s*${NUM}`).exec(android);
      if (k) { checked++; expect(Number(k[1]), `Android NPHardwareLimits.kt: ${snake(name)}`).toBe(json[name]); }
    }
    // Vacuity guard: the patterns must be seeing the files, or this passes by finding nothing.
    expect(checked).toBeGreaterThan(30);
  });
});
