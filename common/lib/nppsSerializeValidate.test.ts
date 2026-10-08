/**
 * The TypeScript wrappers over the core's serializer and validator (OI-NPPS-CORE-01), against the web
 * implementations they replaced: app/NeurOneShared/TestData/npps-serialize-golden.json and
 * npps-validate-golden.json. The Rust tests hold the core to the same files; this holds the marshalling
 * (WebAssembly, JSON, `t()` resolution) to them.
 */
import { describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { nppsSerialize, nppsValidate } from './nppsCore';
import { NppsRefusal } from './nppsCore';
import { coreEntry, serializeNPPSLimits, serializeZone } from './nppsSerializer';
import { validateEntry } from './protocolValidator';
import { parseNPPSLimits } from './nppsParser';
import type { NPProtocolEntry } from '../types/protocol';
import type { NPLimitsSet } from '../types/limits';

const data = (name: string) =>
  JSON.parse(readFileSync(new URL(`../../app/NeurOneShared/TestData/${name}`, import.meta.url), 'utf8'));

/** A parse-shaped entry as the app's model: ids added, `{type, params}` folded under `modalityParams`. */
const toModel = (e: any): NPProtocolEntry =>
  e.kind === 'single'
    ? { kind: 'single', protocol: { ...e.protocol, modalities: e.protocol.modalities.map((m: any, i: number) => ({
        id: `m${i}`, modalityParams: { type: m.type, params: m.params }, interval: m.interval, enabled: m.enabled })) } }
    : { kind: 'composite', composite: { ...e.composite, layers: e.composite.layers.map((l: any) => ({ id: 'l', ...l })) } };

describe('serializer wrapper', () => {
  it('writes every block of the shipped library as the web serializer did', () => {
    const g = data('npps-serialize-golden.json');
    let n = 0;
    for (const items of Object.values<any[]>(g.files)) {
      for (const { item, text } of items) {
        n++;
        expect(nppsSerialize([item])).toBe(text);
      }
    }
    expect(n).toBeGreaterThan(100);
  });

  it('refuses a zone it cannot write, with the message the web gave', () => {
    expect(() => serializeZone({ name: 'Z', sockets: [0, 4, 4, 999] } as any)).toThrow(NppsRefusal);
    expect(() => serializeZone({ name: 'Z', sockets: [0, 4, 4, 999] } as any)).toThrow(
      'cannot serialize zone "Z": 0, 999 are not sockets on this helmet — ids are whole numbers 1–80',
    );
  });

  it('writes a limits set the parser reads back (the web writer wrote keys the parser dropped)', () => {
    const limits = {
      id: 'l', name: 'Clinic', description: '', createdAt: '', modifiedAt: '', level: 'global',
      pbmTranscranial: { maxIrradianceMWcm2: 300, maxFrequencyHz: 40 },
      tms: { maxIntensityPercentMT: 80, allowedTargets: ['dlpfc'] },
    } as NPLimitsSet;
    const back = parseNPPSLimits(serializeNPPSLimits(limits))!;
    expect(back.pbmTranscranial).toEqual(limits.pbmTranscranial);
    expect(back.tms).toEqual(limits.tms);
  });
});

/**
 * The checks the core adds to the web validator's (the iOS and Android ones the web lacked); the golden is the
 * web's output, so they are set aside. common/npps-core/tests/validate_union.rs holds each of them.
 */
const ADDED = new Set([
  'VALIDATE_MSG_GENERAL_DURATION_3', 'VALIDATE_MSG_PBM_CW_DUTY', 'VALIDATE_MSG_PBM_SESSION_DOSE',
  'VALIDATE_MSG_GENERAL_SESSIONDURATION', ...[2, 3, 4, 5, 6, 7, 8].map(n => `VALIDATE_MSG_GENERAL_SESSIONDURATION_${n}`),
  'VALIDATE_MSG_GENERAL_INTENSITYPERCENTMT', 'VALIDATE_MSG_GENERAL_CARDIACINTERLOCK', 'VALIDATE_MSG_GENERAL_FREQUENCYHZ',
  'VALIDATE_MSG_GENERAL_FREQUENCYHZ_2', 'VALIDATE_MSG_GENERAL_INTENSITYMWCM2_2',
  // The zone model (docs/reference/safety-zones.md) is new to the core; common/npps-core/tests/zones.rs holds it.
  'VALIDATE_MSG_ZONE_CAUTION', 'VALIDATE_MSG_ZONE_DANGER',
]);

describe('validator wrapper', () => {
  it('reports what the web validator reported, in English, over the whole corpus', () => {
    const g = data('npps-validate-golden.json');
    expect(g.cases.length).toBeGreaterThan(500);
    for (const c of g.cases) {
      // `limits` names a set in the file's `limits`; `allProtocols` is null, a list, or "library".
      const limits = g.limits[c.limits];
      const entry = toModel(c.entry);
      const all = (c.allProtocols === 'library' ? g.library : c.allProtocols)?.map(toModel);
      const r = validateEntry(entry, limits, all);
      // The same call, unresolved, says which issues are the added checks (same order).
      const raw = nppsValidate({
        entry: coreEntry(entry), limits, allProtocols: all ? all.map(coreEntry) : null,
      }).issues;
      const kept = r.issues.filter((_, i) => {
        const m = raw[i]!.message;
        return !ADDED.has(typeof m === 'string' ? m : m.key);
      });
      const got = kept.map(({ id, ...rest }) => rest);
      // undefined `modality` is absent in the golden.
      // The frozen golden still holds the 2 mA auricular VNS hardware ceiling, removed on the principal's
      // instruction (OI-VNSCLIP-09, issue #554); it is set aside here as in common/npps-core/tests/validate.rs.
      const want = c.expected.filter((i: { modality?: string; parameterKey?: string; limitSource?: string; message: string }) =>
        !(i.modality === 'vns_hrv' && i.parameterKey === 'intensityMilliamps' && i.limitSource === 'hardware'
          && i.message.startsWith('VNS intensity')));
      expect(JSON.parse(JSON.stringify(got)), c.name).toEqual(want);
    }
  });

  it('resolves a nested message: a layer prefix wraps the issue it prefixes', () => {
    const r = validateEntry(
      { kind: 'composite', composite: { id: 'c', name: 'C', description: '', author: 'a', version: '1', tags: [],
        conflictResolution: 'merge', layers: [{ id: 'l', protocolName: 'Bad', startOffsetSeconds: 0, intensityScale: 1 }] } as any },
      { id: 'n', name: 'n', description: '', createdAt: '', modifiedAt: '', level: 'global' },
      [toModel({ kind: 'single', protocol: { id: 'b', name: 'Bad', description: '', author: 'a', version: '1', tags: [],
        timingMode: { type: 'duration', seconds: 600 },
        modalities: [{ type: 'bes_tacs', enabled: true, interval: { intervalOnSeconds: 0, intervalOffSeconds: 0 },
          params: { frequencyHz: 10, intensityMilliamps: 2, waveform: 'square' } }] } })],
    );
    expect(r.errors[0]!.message.startsWith('[Bad] ')).toBe(true);
  });
});
