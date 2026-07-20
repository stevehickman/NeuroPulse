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
  const { namespace, warnings } = buildNamespace(parsed);

  it('has no duplicate zone/condition name collisions', () => {
    expect(warnings).toEqual([]);
  });

  it('defines the 8 predefined lobe zones', () => {
    for (const lobe of ['Frontal', 'Temporal', 'Parietal', 'Occipital']) {
      for (const side of ['Left', 'Right']) {
        expect(namespace.zones.has(`${lobe} ${side}`), `${lobe} ${side}`).toBe(true);
      }
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

  it('includes the expected clinical protocol coverage (50 clinical presets)', () => {
    const clinical = manifest.protocols.filter(f => f.startsWith('clinical-'));
    expect(clinical.length).toBe(50);
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
