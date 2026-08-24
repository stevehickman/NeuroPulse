import { describe, it, expect } from 'vitest';
import fs from 'node:fs';
import path from 'node:path';
import { buildLibrary } from '../../../../simulator/src/npps-runtime';

/**
 * The simulator's library transform moved out of scripts/generate-simulator-data.ts
 * and into simulator/src/npps-runtime.ts when the simulator stopped shipping a
 * build-time cache of the protocol library (NP-NPPS-REF-001 §1.6). It runs in
 * the browser now, so it is covered here against the real shipped files rather
 * than by whatever the generator happened to emit.
 */
const DIR = path.join(__dirname, '../../../../protocols/predefined');

function shippedFiles() {
  const manifest = JSON.parse(fs.readFileSync(path.join(DIR, 'manifest.json'), 'utf8'));
  const names: string[] = [
    ...(manifest.zones ?? []),
    ...(manifest.conditions ?? []),
    ...(manifest.protocols ?? []),
    ...(manifest.composites ?? []),
  ];
  return names.map(filename => ({
    filename,
    text: fs.readFileSync(path.join(DIR, filename), 'utf8'),
  }));
}

describe('simulator runtime library', () => {
  const { protocols, zones, report } = buildLibrary(shippedFiles());

  it('builds the shipped library with no namespace problems', () => {
    expect(report.duplicateDefinitions).toEqual([]);
    expect(report.unresolvedReferences).toEqual([]);
  });

  it('reads every zone from the .npps files', () => {
    expect(zones.length).toBeGreaterThan(0);
    for (const zone of zones) {
      expect(zone.sockets.length, `zone ${zone.name} is empty`).toBeGreaterThan(0);
      expect(new Set(zone.sockets).size, `zone ${zone.name} repeats a socket`)
        .toBe(zone.sockets.length);
    }
  });

  it('zone membership matches the file, not a transcription', () => {
    // Read straight out of 00-zones.npps so this asserts against the source and
    // not against a second copy that could drift with it.
    const source = fs.readFileSync(path.join(DIR, '00-zones.npps'), 'utf8');
    const block = /zone\s+"((?:[^"\\]|\\.)*)"\s*\{([\s\S]*?)\n\}/g;
    let declared = 0;
    for (const m of source.matchAll(block)) {
      const ids = [...(m[2].match(/sockets\s*:\s*\[([^\]]*)\]/)?.[1] ?? '')
        .matchAll(/\d+/g)].map(n => Number(n[0]));
      if (ids.length === 0) continue;
      declared++;
      const zone = zones.find(z => z.name === m[1]);
      expect(zone, `zone ${m[1]} is declared but missing`).toBeDefined();
      expect([...zone!.sockets].sort((a, b) => a - b))
        .toEqual([...new Set(ids)].sort((a, b) => a - b));
    }
    expect(declared).toBeGreaterThan(0);
    expect(zones.length).toBe(declared);
  });

  it('builds a runnable protocol view for every non-composite file', () => {
    expect(Object.keys(protocols).length).toBeGreaterThan(0);
    for (const [id, p] of Object.entries(protocols)) {
      expect(p.id, `${id} id mismatch`).toBe(id);
      expect(p.name.length, `${id} has no name`).toBeGreaterThan(0);
      expect(['T1', 'T2']).toContain(p.tier);
      expect(p.duration, `${id} has no duration`).toBeGreaterThan(0);
      expect(p.phases.length, `${id} has no phases`).toBeGreaterThan(0);
      // Phases must tile the session exactly — the timeline draws them end to end.
      expect(p.phases[0].start).toBe(0);
      expect(p.phases[p.phases.length - 1].end).toBe(p.duration);
      for (let i = 1; i < p.phases.length; i++) {
        expect(p.phases[i].start, `${id} phase ${i} does not abut`).toBe(p.phases[i - 1].end);
      }
    }
  });

  it('resolves PBM zone targets to zones the library defines', () => {
    let checked = 0;
    for (const [id, p] of Object.entries(protocols)) {
      for (const name of p.modalities.pbm?.zones ?? []) {
        expect(zones.some(z => z.name === name), `${id} targets unknown zone '${name}'`).toBe(true);
        checked++;
      }
    }
    expect(checked).toBeGreaterThan(0);
  });
});
