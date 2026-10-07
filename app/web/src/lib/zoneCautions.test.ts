// The caution list the acknowledgement screen shows (docs/reference/safety-zones.md): one entry per axis in its
// caution band, carrying the id the compiler checks, and the id the screen hands back is enough to compile.

import { describe, it, expect } from 'vitest';
import fs from 'node:fs';
import path from 'node:path';
import { buildNamespace, parseNPPSFile } from '../../../../common/lib/nppsParser';
import { nppsValidate } from '../../../../common/lib/nppsCore';
import { coreEntry } from '../../../../common/lib/nppsSerializer';
import { cautionsOf } from '../../../../common/lib/zoneCautions';
import { compileProtocol } from './hubCompiler';

const DIR = path.resolve(__dirname, '../../../../protocols/predefined');
const manifest = JSON.parse(fs.readFileSync(path.join(DIR, 'manifest.json'), 'utf8'));
const { namespace } = buildNamespace(
  [...manifest.zones, ...manifest.conditions, ...manifest.protocols]
    .map((f: string) => parseNPPSFile(fs.readFileSync(path.join(DIR, f), 'utf8'))),
);
const find = (name: string) => namespace.entries.find(e => e.kind === 'single' && e.protocol.name === name)!;

const validate = (name: string) => nppsValidate({ entry: coreEntry(find(name)), limits: {} });

describe('cautionsOf', () => {
  it('lists the one caution of a tDCS protocol in the caution zone, with the id and the dose', () => {
    const v = validate('ADHD Focus');
    expect(v.zone).toBe('caution');
    const [c, ...rest] = cautionsOf(v.zones);
    expect(rest).toEqual([]);
    expect(c).toMatchObject({
      ackId: '1:tdcs:sessionChargeDensity=102.857', modality: 'tdcs', axisNameKey: 'ZONE_AXIS_SESSION_CHARGE_DENSITY',
      unit: 'mC/cm²', caution: 100,
    });
    expect(c.value).toBeCloseTo(102.857, 3);
  });

  it('lists nothing for a protocol in the safe zone', () => {
    const safe = namespace.entries.find(e => e.kind === 'single' && nppsValidate({ entry: coreEntry(e), limits: {} }).zone === 'safe')!;
    expect(cautionsOf(nppsValidate({ entry: coreEntry(safe), limits: {} }).zones)).toEqual([]);
  });

  it('the ids it lists are exactly what lets the protocol compile, and nothing less does', () => {
    const entry = find('ADHD Focus');
    if (entry.kind !== 'single') throw new Error('not single');
    const opts = { zones: namespace.zones };
    expect(() => compileProtocol(entry.protocol, opts)).toThrow(/caution zone/);
    const ids = cautionsOf(validate('ADHD Focus').zones).map(c => c.ackId);
    expect(() => compileProtocol(entry.protocol, { ...opts, acknowledgedCautions: ids })).not.toThrow();
  });
});
