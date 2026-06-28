import { describe, it, expect } from 'vitest';
import { parseNPPS, NPPSParseError } from './nppsParser';
import { readFileSync, readdirSync } from 'fs';
import { join } from 'path';
import type { NPProtocolDefinition, NPCompositeProtocol } from '../types/protocol';

const FIXTURES_DIR = join(__dirname, '..', '..', '..', '..', 'npps', 'fixtures');

interface ExpectedModality {
  type: string;
  enabled: boolean;
  params: Record<string, unknown>;
  interval: { intervalOnSeconds: number; intervalOffSeconds: number };
}

interface ExpectedProtocol {
  kind: 'single';
  protocol: {
    name: string;
    id: string;
    description: string;
    author: string;
    version: string;
    isReadOnly?: boolean;
    tags: string[];
    timingMode: { type: string; seconds: number };
    modalityCount: number;
    modalities: ExpectedModality[];
  };
}

interface ExpectedComposite {
  kind: 'composite';
  composite: {
    name: string;
    id: string;
    description: string;
    conflictResolution: string;
    tags: string[];
    layerCount: number;
    layers: Array<{
      protocolName: string;
      startOffsetSeconds: number;
      durationSeconds: number;
      intensityScale: number;
    }>;
  };
}

type ExpectedEntry = ExpectedProtocol | ExpectedComposite;

function loadFixture(name: string): { npps: string; expected: ExpectedEntry } {
  const npps = readFileSync(join(FIXTURES_DIR, `${name}.npps`), 'utf8');
  const expected = JSON.parse(
    readFileSync(join(FIXTURES_DIR, `${name}.expected.json`), 'utf8')
  );
  return { npps, expected };
}

function normalizeModality(m: { modalityParams: { type: string; params: Record<string, unknown> }; enabled: boolean; interval: { intervalOnSeconds: number; intervalOffSeconds: number } }): ExpectedModality {
  return {
    type: m.modalityParams.type,
    enabled: m.enabled,
    params: m.modalityParams.params as Record<string, unknown>,
    interval: {
      intervalOnSeconds: m.interval.intervalOnSeconds,
      intervalOffSeconds: m.interval.intervalOffSeconds,
    },
  };
}

// Discover all fixture pairs
const fixtureNames = readdirSync(FIXTURES_DIR)
  .filter(f => f.endsWith('.expected.json'))
  .map(f => f.replace('.expected.json', ''));

describe('shared NPPS fixtures', () => {
  for (const name of fixtureNames) {
    it(`parses ${name}.npps and matches expected output`, () => {
      const { npps, expected } = loadFixture(name);
      const entries = parseNPPS(npps);
      expect(entries).toHaveLength(1);

      const entry = entries[0];
      expect(entry.kind).toBe(expected.kind);

      if (expected.kind === 'single' && entry.kind === 'single') {
        const proto = entry.protocol as NPProtocolDefinition;
        const exp = expected.protocol;

        expect(proto.name).toBe(exp.name);
        expect(proto.id).toBe(exp.id);
        expect(proto.description).toBe(exp.description);
        expect(proto.author).toBe(exp.author);
        expect(proto.version).toBe(exp.version);
        if (exp.isReadOnly !== undefined) {
          expect(proto.isReadOnly).toBe(exp.isReadOnly);
        }
        expect(proto.tags).toEqual(exp.tags);
        expect(proto.timingMode).toMatchObject(exp.timingMode);
        expect(proto.modalities).toHaveLength(exp.modalityCount);

        for (let i = 0; i < exp.modalities.length; i++) {
          const actual = normalizeModality(proto.modalities[i]);
          const expMod = exp.modalities[i];
          expect(actual.type).toBe(expMod.type);
          expect(actual.enabled).toBe(expMod.enabled);
          expect(actual.interval).toMatchObject(expMod.interval);
          for (const [key, value] of Object.entries(expMod.params)) {
            expect(actual.params[key]).toEqual(value);
          }
        }
      }

      if (expected.kind === 'composite' && entry.kind === 'composite') {
        const comp = entry.composite as NPCompositeProtocol;
        const exp = (expected as ExpectedComposite).composite;

        expect(comp.name).toBe(exp.name);
        expect(comp.id).toBe(exp.id);
        expect(comp.conflictResolution).toBe(exp.conflictResolution);
        expect(comp.tags).toEqual(exp.tags);
        expect(comp.layers).toHaveLength(exp.layerCount);

        for (let i = 0; i < exp.layers.length; i++) {
          expect(comp.layers[i].protocolName).toBe(exp.layers[i].protocolName);
          expect(comp.layers[i].startOffsetSeconds).toBe(exp.layers[i].startOffsetSeconds);
          expect(comp.layers[i].durationSeconds).toBe(exp.layers[i].durationSeconds);
          expect(comp.layers[i].intensityScale).toBeCloseTo(exp.layers[i].intensityScale);
        }
      }
    });
  }

  // Error fixtures — must throw NPPSParseError
  const errorFixtures = readdirSync(FIXTURES_DIR)
    .filter(f => f.startsWith('error_') && f.endsWith('.npps'));

  for (const f of errorFixtures) {
    it(`rejects ${f} with NPPSParseError`, () => {
      const input = readFileSync(join(FIXTURES_DIR, f), 'utf8');
      expect(() => parseNPPS(input)).toThrow(NPPSParseError);
    });
  }
});
