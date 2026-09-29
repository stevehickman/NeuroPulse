// Hub compiler — one wavelength per PBM block, block start offsets, and
// parallel blocks sharing a tile (NP-NPPS-REF-001 §4.1a, §5).
//
// What is pinned here, and why each matters:
//   - A single wavelength drives ONE channel; the other is commanded to 0,
//     which the tile holds as gate-off (NP-FW-HEXTILE-001 §5.3).
//   - A wavelength no rule accepts is REFUSED. The compiler used to send any
//     value other than '660_808nm' to the smart encoder with all three
//     channels on; "808nm" would have lit 660, 808 and 1064 together.
//   - `start` shifts a block, so blocks can run in series.
//   - Two blocks on one tile at the same time are merged only when the tile can
//     deliver both exactly; anything else is refused, never reshaped.

import { describe, it, expect } from 'vitest';
import fs from 'node:fs';
import path from 'node:path';
import { parseNPPS, parseNPPSFile, buildNamespace } from './nppsParser';
import { compileProtocol } from './hubCompiler';
import { DEFAULT_WAVELENGTH_RULES, resolveWavelengthRules } from './wavelengthRules';
import type { NPProtocolDefinition, NPZoneDefinition } from '../types/protocol';

const DIR = path.resolve(__dirname, '../../../../protocols/predefined');
const zones: ReadonlyMap<string, NPZoneDefinition> = new Map(
  parseNPPSFile(fs.readFileSync(path.join(DIR, '00-zones.npps'), 'utf8')).zones.map(z => [z.name, z]),
);

const HEADER_LEN = 64;
const CMD_HDR_LEN = 14;
const NP_MOD_PBM_BASE = 0x01;
const NP_MOD_PBM_SMART = 0x02;

interface Cmd { modType: number; startMs: number; durationMs: number; params: number[]; sockets: number[] }

function commands(proto: NPProtocolDefinition, opts: Parameters<typeof compileProtocol>[1] = {}): Cmd[] {
  const { blob } = compileProtocol(proto, { zones, ...opts });
  const dv = new DataView(blob.buffer, blob.byteOffset, blob.byteLength);
  const out: Cmd[] = [];
  let off = HEADER_LEN;
  for (let i = 0; i < dv.getUint8(7); i++) {
    const modType = dv.getUint8(off);
    const startMs = dv.getUint32(off + 2, true);
    const durationMs = dv.getUint32(off + 6, true);
    const paramsLen = dv.getUint16(off + 10, true);
    const targetLen = dv.getUint8(off + 13);
    off += CMD_HDR_LEN;
    const mask = blob.slice(off, off + targetLen);
    off += targetLen;
    const params = [...blob.slice(off, off + paramsLen)];
    off += paramsLen;
    const sockets: number[] = [];
    for (let bit = 0; bit < mask.length * 8; bit++) if ((mask[bit >> 3] >> (bit & 7)) & 1) sockets.push(bit + 1);
    out.push({ modType, startMs, durationMs, params, sockets });
  }
  return out;
}

function protocol(body: string, duration = '10m'): NPProtocolDefinition {
  const [entry] = parseNPPS(`protocol "T" {\n    duration: ${duration}\n${body}\n}`);
  if (entry.kind !== 'single') throw new Error('expected a protocol');
  return entry.protocol;
}

const pbm = (fields: string) => `    pbm_transcranial {
        intensity: 50%
        frequency: 40Hz
        duty_cycle: 25%
        zones: ["Frontal Left"]
${fields}
    }`;

// intensityReg(50) and the 40 Hz / 25 % codes, read back from a legacy compile
// so these tests do not restate the encoder's arithmetic.
const legacy = commands(protocol(pbm('        wavelength: "660_808nm"')))[0];
const [FC, DUTY, CUR] = [legacy.params[0], legacy.params[1], legacy.params[2]];

describe('one wavelength per block', () => {
  it('808 nm family drives only the 808 channel', () => {
    for (const w of ['808nm', '810nm', '820nm', '830nm']) {
      const [c] = commands(protocol(pbm(`        wavelength: "${w}"`)));
      expect(c.modType, w).toBe(NP_MOD_PBM_BASE);
      expect(c.params, w).toEqual([FC, DUTY, 0, CUR]);
    }
  });

  it('660 nm drives only the 660 channel', () => {
    const [c] = commands(protocol(pbm('        wavelength: "660nm"')));
    expect(c.params).toEqual([FC, DUTY, CUR, 0]);
  });

  it('1070 nm drives only CH_C on a smart command', () => {
    const [c] = commands(protocol(pbm('        wavelength: "1070nm"')));
    expect(c.modType).toBe(NP_MOD_PBM_SMART);
    expect(c.params).toEqual([FC, DUTY, 0, 0, CUR, 0x04]);
  });

  it('legacy names keep their exact encoding', () => {
    expect(legacy.params).toEqual([FC, DUTY, CUR, CUR]);
    const [c] = commands(protocol(pbm('        wavelength: "1064nm"')));
    expect(c.params).toEqual([FC, DUTY, CUR, CUR, CUR, 0x04]);
  });

  it('refuses a wavelength no rule accepts — never the nearest channel, never all channels', () => {
    for (const w of ['850nm', '640nm', '633nm', '1080nm']) {
      expect(() => commands(protocol(pbm(`        wavelength: "${w}"`))), w)
        .toThrow(/No emitter channel delivers/);
    }
  });

  it('refuses a value that is not a wavelength', () => {
    expect(() => commands(protocol(pbm('        wavelength: "red"')))).toThrow(/is not a wavelength/);
  });

  it('compiles against the rules it is given', () => {
    const lab = resolveWavelengthRules(DEFAULT_WAVELENGTH_RULES, {
      name: 'Lab', level: 'user',
      channels: [{ element: 'led_808', nominalNm: 808, minNm: 798, maxNm: 860 }],
    });
    const [c] = commands(protocol(pbm('        wavelength: "850nm"')), { wavelengthRules: lab });
    expect(c.params).toEqual([FC, DUTY, 0, CUR]);
  });
});

describe('start offsets', () => {
  it('blocks run in series: F3 side 0–4 min, then F4 side 4–8 min', () => {
    const block = (zone: string, start: string) => `    pbm_transcranial {
        wavelength: "810nm"
        intensity: 50%
        frequency: 40Hz
        duty_cycle: 25%
        zones: ["${zone}"]
        ${start}
        interval_on: 4m
        interval_off: 0s
        repeat: 1
    }`;
    const cmds = commands(protocol(`${block('Frontal Left', '')}\n${block('Frontal Right', 'start: 4m')}`, '8m'));
    const on = cmds.filter(c => c.params.length > 0);
    const stops = cmds.filter(c => c.params.length === 0);
    expect(on.map(c => [c.startMs, c.durationMs])).toEqual([[0, 240000], [240000, 240000]]);
    expect(stops.map(c => c.startMs)).toEqual([240000]);   // the second block's stop is at session end, not emitted
    const left = new Set(zones.get('Frontal Left')!.sockets);
    const right = new Set(zones.get('Frontal Right')!.sockets);
    expect(on[0].sockets.every(s => left.has(s))).toBe(true);
    expect(on[1].sockets.every(s => right.has(s))).toBe(true);
  });

  it('a continuous block with a start begins at its offset', () => {
    const [c] = commands(protocol(pbm('        wavelength: "810nm"\n        start: 90s')));
    expect([c.startMs, c.durationMs]).toEqual([90000, 0]);
  });

  it('refuses a block that starts at or after the session end', () => {
    expect(() => commands(protocol(pbm('        wavelength: "810nm"\n        start: 10m')))).toThrow(/never run/);
  });
});

describe('parallel blocks on the same tiles', () => {
  const two = (a: string, b: string) => protocol(`${pbm(a)}\n${pbm(b)}`);

  it('660 nm and 810 nm with identical timing merge into one command driving both channels', () => {
    const cmds = commands(two('        wavelength: "660nm"', '        wavelength: "810nm"'));
    expect(cmds).toHaveLength(1);
    expect(cmds[0].params).toEqual([FC, DUTY, CUR, CUR]);
  });

  it('refuses the same pair at different frequencies', () => {
    const p = protocol(`${pbm('        wavelength: "660nm"')}\n${pbm('        wavelength: "810nm"').replace('frequency: 40Hz', 'frequency: 10Hz')}`);
    expect(() => commands(p)).toThrow(/overlap on socket/);
  });

  it('refuses two blocks on the same channel', () => {
    expect(() => commands(two('        wavelength: "808nm"', '        wavelength: "820nm"'))).toThrow(/overlap on socket/);
  });

  it('refuses a legacy two-channel block overlapping a single-wavelength block', () => {
    expect(() => commands(two('        wavelength: "660_808nm"', '        wavelength: "810nm"'))).toThrow(/overlap on socket/);
  });

  it('refuses a partial socket overlap even with matching timing', () => {
    const p = protocol(`${pbm('        wavelength: "660nm"')}\n${pbm('        wavelength: "810nm"').replace('Frontal Left', 'Frontal')}`);
    expect(() => commands(p)).toThrow(/overlap on socket/);
  });

  it('blocks on disjoint tiles never interact', () => {
    const p = protocol(`${pbm('        wavelength: "660nm"')}\n${pbm('        wavelength: "810nm"').replace('Frontal Left', 'Posterior')}`);
    expect(commands(p)).toHaveLength(2);
  });
});

describe('the shipped library', () => {
  it('every predefined protocol still compiles', () => {
    const manifest = JSON.parse(fs.readFileSync(path.join(DIR, 'manifest.json'), 'utf8'));
    const files = [...manifest.zones, ...manifest.conditions, ...manifest.protocols]
      .map((f: string) => parseNPPSFile(fs.readFileSync(path.join(DIR, f), 'utf8')));
    const { namespace } = buildNamespace(files);
    let compiled = 0;
    for (const entry of namespace.entries) {
      if (entry.kind !== 'single') continue;
      const clinician = entry.protocol.modalities.some(m =>
        (m.modalityParams.params as { zones?: string }).zones === 'clinician_selected');
      compileProtocol(entry.protocol, { zones: namespace.zones, clinicianSockets: clinician ? [1] : undefined });
      compiled++;
    }
    expect(compiled).toBeGreaterThan(50);
  });
});
