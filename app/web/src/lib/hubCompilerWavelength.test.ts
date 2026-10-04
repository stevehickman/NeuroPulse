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

const pbm = (fields: string, irradiance = '201.5mW_cm2') => `    pbm_transcranial {
        irradiance: ${irradiance}
        frequency: 40Hz
        duty_cycle: 25%
        zones: ["Frontal Left"]
${fields}
    }`;

// The 40 Hz / 25 % codes and the register 201.5 mW/cm² becomes on a 660/808
// channel, read back from a compile so these tests do not restate the encoder's
// arithmetic. 14 mW/cm² is half of CH_C's 28 at full drive.
const base = commands(protocol(pbm('        wavelength: "808nm"')))[0];
const [FC, DUTY, CUR] = [base.params[0], base.params[1], base.params[3]];
const CUR_C = commands(protocol(pbm('        wavelength: "1064nm"', '14mW_cm2')))[0].params[4];

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
    const [c] = commands(protocol(pbm('        wavelength: "1070nm"', '14mW_cm2')));
    expect(c.modType).toBe(NP_MOD_PBM_SMART);
    expect(c.params).toEqual([FC, DUTY, 0, 0, CUR_C, 0x04]);
  });

  it('1064nm is a plain wavelength and drives CH_C alone', () => {
    const [c] = commands(protocol(pbm('        wavelength: "1064nm"', '14mW_cm2')));
    expect(c.params).toEqual([FC, DUTY, 0, 0, CUR_C, 0x04]);
  });

  it('the retired combined names are refused, and say which blocks replace them', () => {
    for (const w of ['660_808nm', '660_808_1064nm']) {
      expect(() => commands(protocol(pbm(`        wavelength: "${w}"`))), w).toThrow(/retired|Line/);
    }
  });

  it('drives each channel from its own full-scale table: the same irradiance is a different register', () => {
    // 28 mW/cm² is CH_C at full drive (255); on a 660/808 channel it is ~7 % of 403.
    const c1064 = commands(protocol(pbm('        wavelength: "1064nm"', '28mW_cm2')))[0];
    const c808 = commands(protocol(pbm('        wavelength: "808nm"', '28mW_cm2')))[0];
    expect(c1064.params[4]).toBe(255);
    expect(c808.params[3]).toBe(Math.round(28 / 403 * 255));
  });

  it('REFUSES an irradiance the channel cannot reach, rather than clamping it', () => {
    expect(() => commands(protocol(pbm('        wavelength: "1064nm"', '250mW_cm2'))))
      .toThrow(/exceeds what led_1064 delivers at full drive \(28 mW\/cm²\).*refused, not reduced/);
    expect(() => commands(protocol(pbm('        wavelength: "808nm"', '404mW_cm2')))).toThrow(/exceeds what led_808/);
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
        irradiance: 201.5mW_cm2
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

  it('refuses a partial socket overlap even with matching timing', () => {
    const p = protocol(`${pbm('        wavelength: "660nm"')}\n${pbm('        wavelength: "810nm"').replace('Frontal Left', 'Frontal')}`);
    expect(() => commands(p)).toThrow(/overlap on socket/);
  });

  it('blocks on disjoint tiles never interact', () => {
    const p = protocol(`${pbm('        wavelength: "660nm"')}\n${pbm('        wavelength: "810nm"').replace('Frontal Left', 'Posterior')}`);
    expect(commands(p)).toHaveLength(2);
  });
});

describe('intranasal probe blocks', () => {
  const NP_MOD_INTRANASAL = 0x03;
  const nasal = (wl: string, irr = '50mW_cm2', extra = '') => `    pbm_intranasal {
        wavelength: "${wl}"
        irradiance: ${irr}
        frequency: 40Hz
        duty_cycle: 25%
${extra}    }`;

  it('one wavelength drives its probe channel only', () => {
    const [c] = commands(protocol(nasal('660nm')));
    expect(c.modType).toBe(NP_MOD_INTRANASAL);
    expect(c.params[3]).toBeGreaterThan(0);
    expect(c.params[4]).toBe(0);
    const [d] = commands(protocol(nasal('808nm')));
    expect([d.params[3], d.params[4] > 0]).toEqual([0, true]);
  });

  it('660 nm and 808 nm blocks with identical timing merge into one probe command', () => {
    const cmds = commands(protocol(`${nasal('660nm')}\n${nasal('808nm')}`));
    expect(cmds).toHaveLength(1);
    expect(cmds[0].params[3]).toBeGreaterThan(0);
    expect(cmds[0].params[4]).toBeGreaterThan(0);
  });

  it('refuses two blocks on one channel and a wavelength the probe does not carry', () => {
    expect(() => commands(protocol(`${nasal('660nm')}\n${nasal('660nm')}`))).toThrow(/overlap/);
    expect(() => commands(protocol(nasal('1064nm')))).toThrow(/cannot be delivered on led_1064/);
  });

  it('refuses an irradiance above the probe full scale', () => {
    expect(() => commands(protocol(nasal('660nm', '150mW_cm2')))).toThrow(/exceeds what the intranasal probe delivers/);
  });
});

describe('audio level', () => {
  const audio = (v: string) => protocol(`    audio_entrainment {\n        carrier_hz: 440Hz\n        isochronic_hz: 10Hz\n        volume: ${v}\n    }`);
  it('maps dB SPL through the calibration line', () => {
    // 40 + 0.5 × pct: 65 dB is wire value 50.
    expect(commands(audio('65dB'))[0].params[5]).toBe(50);
  });
  it('refuses a level outside the calibrated range', () => {
    expect(() => commands(audio('95dB'))).toThrow(/outside the range the drive can deliver/);
    expect(() => commands(audio('30dB'))).toThrow(/outside the range/);
  });
});

describe('the shipped library', () => {
  // Protocols the hardware or the default wavelength rules cannot deliver are
  // REFUSED with the reason, not reshaped to fit (CLAUDE.md §3). Each one is
  // named here so a new refusal is a deliberate act, not a drift.
  const REFUSED: Record<string, RegExp> = {
    'Memory Boost': /exceeds what led_1064 delivers/,                         // Yao 2022: 250 mW/cm² vs CH_C's 28
    "PBM — Alzheimer's 1064nm (deep-cortical channel)": /exceeds what led_1064 delivers/,
    'PBM — Autism (pediatric, 40Hz)': /No emitter channel delivers 850nm/,  // §7 says 850 nm; the 808 window ends at 840
  };

  it('every predefined protocol compiles, except the ones refused for a stated reason', () => {
    const manifest = JSON.parse(fs.readFileSync(path.join(DIR, 'manifest.json'), 'utf8'));
    const files = [...manifest.zones, ...manifest.conditions, ...manifest.protocols]
      .map((f: string) => parseNPPSFile(fs.readFileSync(path.join(DIR, f), 'utf8')));
    const { namespace } = buildNamespace(files);
    let compiled = 0;
    const refused: string[] = [];
    for (const entry of namespace.entries) {
      if (entry.kind !== 'single') continue;
      const clinician = entry.protocol.modalities.some(m =>
        (m.modalityParams.params as { zones?: string }).zones === 'clinician_selected');
      try {
        compileProtocol(entry.protocol, { zones: namespace.zones, clinicianSockets: clinician ? [1] : undefined });
        compiled++;
      } catch (e) {
        const why = REFUSED[entry.protocol.name];
        expect(why, `${entry.protocol.name}: ${(e as Error).message}`).toBeDefined();
        expect((e as Error).message).toMatch(why);
        refused.push(entry.protocol.name);
      }
    }
    expect(compiled).toBeGreaterThan(50);
    expect(refused.sort()).toEqual(Object.keys(REFUSED).sort());
  });
});
