import { describe, it, expect } from 'vitest';
import fs from 'node:fs';
import path from 'node:path';
import {
  parseNPPSFile,
  buildNamespace,
  validateNamespaceReferences,
} from './nppsParser';

// Load the predefined library straight off disk and prove the whole set parses
// and cross-references cleanly.
//
// This reads the canonical `protocols/` tree — the single source of truth. The
// app fetches the same files at runtime: in dev the Vite plugin in
// vite.config.ts streams them from here, and at build time it copies them into
// the bundle. There is deliberately no second copy under public/.
const DIR = path.resolve(__dirname, '../../../../protocols/predefined');

interface Manifest {
  zones?: string[];
  conditions?: string[];
  protocols: string[];
  composites: string[];
}

const manifest = JSON.parse(fs.readFileSync(path.join(DIR, 'manifest.json'), 'utf8')) as Manifest;
const allFiles = [
  ...(manifest.zones ?? []),
  ...(manifest.conditions ?? []),
  ...manifest.protocols,
  ...manifest.composites,
];

describe('predefined NPPS library', () => {
  it('every manifest file exists and parses without throwing', () => {
    for (const f of allFiles) {
      const fp = path.join(DIR, f);
      expect(fs.existsSync(fp), `missing ${f}`).toBe(true);
      expect(() => parseNPPSFile(fs.readFileSync(fp, 'utf8')), `parse ${f}`).not.toThrow();
    }
  });

  const parsed = allFiles.map(f => parseNPPSFile(fs.readFileSync(path.join(DIR, f), 'utf8')));
  const { namespace, errors: namespaceErrors } = buildNamespace(parsed);

  it('has no duplicate zone/condition name collisions', () => {
    // A collision is an error, not a warning, and leaves the name unbound —
    // so the shipped library having one would also break reference resolution.
    expect(namespaceErrors).toEqual([]);
  });

  /**
   * Zones are defined ONLY in 00-zones.npps, so this asserts the shape every
   * zone must have rather than a list of expected names — naming them here would
   * just be a second copy of the file, and would quietly reject the user-defined
   * and research zones the .npps tree is designed to accept (SMART-1).
   */
  it('every declared zone carries a non-empty, deduplicated socket set', () => {
    expect(namespace.zones.size).toBeGreaterThan(0);

    for (const [name, zone] of namespace.zones) {
      expect(zone.sockets.length, `zone ${name} is empty`).toBeGreaterThan(0);
      expect(
        new Set(zone.sockets).size,
        `zone ${name} lists a socket more than once`,
      ).toBe(zone.sockets.length);
    }
  });

  it('every protocol/composite zone and condition reference resolves', () => {
    const errors = validateNamespaceReferences(namespace);
    expect(errors).toEqual([]);
  });

  it('every protocol condition names a defined condition with a link', () => {
    for (const [, c] of namespace.conditions) {
      expect(c.link, `condition ${c.name} link`).toMatch(/^https?:\/\//);
    }
  });

  it('includes the expected clinical protocol coverage (53 clinical presets)', () => {
    const clinical = manifest.protocols.filter(f => f.startsWith('clinical-'));
    // 50 → 53: three sections whose evidence is two trials an order of
    // magnitude apart are now one protocol per trial rather than one per
    // section — anxiety §5 (Maiello 2019 / Wang 2023), depression §4
    // (Cassano 2018 / Schiffer 2009) and Alzheimer's §1 (Woźniak-Mitał 2026 /
    // Chun 2026). See those files' own notes.
    expect(clinical.length).toBe(53);
  });

  it('every clinical protocol carries at least one condition and one reference', () => {
    for (const entry of namespace.entries) {
      if (entry.kind !== 'single') continue;
      if (!entry.protocol.id.startsWith('30000')) continue; // clinical id band
      expect(entry.protocol.conditions?.length, `${entry.protocol.name} conditions`).toBeGreaterThan(0);
      expect(entry.protocol.references?.length, `${entry.protocol.name} references`).toBeGreaterThan(0);
    }
  });

  // The former 'mirrors identical content to protocols/predefined' test is gone
  // with the duplicate it policed: public/protocols/ was a byte-identical copy
  // of protocols/, so every edit had to be made twice. Vite now serves the
  // canonical tree directly (see vite.config.ts), leaving nothing to mirror.
  it('has no second copy of the protocol tree in the web app', () => {
    const publicCopy = path.resolve(__dirname, '../../public/protocols');
    expect(
      fs.existsSync(publicCopy),
      'public/protocols/ has reappeared — protocols/ is the single source of truth',
    ).toBe(false);
  });
});
