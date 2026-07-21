// Hub protocol compiler — NP Hub Protocol v2 socket-addressed targets.
//
// The compiler is the boundary where the app's named, 1-based socket zones
// become the firmware's 0-based socket bitmap (np_module_map.h). These tests
// read the real zone definitions off disk rather than using fixtures, so a
// change to protocols/predefined/00-zones.npps that the compiler cannot express
// fails here rather than on a helmet.

import { describe, it, expect } from 'vitest';
import fs from 'node:fs';
import path from 'node:path';
import { parseNPPSFile } from './nppsParser';
import { compileProtocol } from './hubCompiler';
import { NP_SOCKETS } from './socketMap.generated';
import { defaultParams, MODALITY_META } from '../types/protocol';
import type {
  NPProtocolDefinition,
  NPZoneDefinition,
  PBMTranscranialParams,
} from '../types/protocol';

// ─── Real zone namespace ───────────────────────────────────────────────────────

const ZONES_FILE = path.resolve(__dirname, '../../../../protocols/predefined/00-zones.npps');
const zones: ReadonlyMap<string, NPZoneDefinition> = new Map(
  parseNPPSFile(fs.readFileSync(ZONES_FILE, 'utf8')).zones.map(z => [z.name, z]),
);

// ─── Wire layout constants (mirror np_hub_config.h / np_hub_types.h) ───────────

const HEADER_LEN = 64;
const CMD_HDR_LEN = 14;
const SIG_LEN = 64;
const SOCKET_MASK_BYTES = 16;

const TARGET_SLOT = 0x00;
const TARGET_SOCKET_MASK = 0x01;
const SLOT_NONE = 0xff;

const NP_MOD_PBM_BASE = 0x01;
const NP_MOD_EEG = 0x04;

// ─── Blob reader ───────────────────────────────────────────────────────────────
//
// Walks the compiled blob the way np_protocol.c does, rather than trusting
// offsets the compiler also computed.

interface ParsedCmd {
  modType: number;
  slotId: number;
  startMs: number;
  durationMs: number;
  targetKind: number;
  socketMask: Uint8Array;   // empty unless targetKind is SOCKET_MASK
  params: Uint8Array;
}

function readBlob(blob: Uint8Array): { version: number; cmds: ParsedCmd[] } {
  const dv = new DataView(blob.buffer, blob.byteOffset, blob.byteLength);
  const version = dv.getUint16(4, true);
  const cmdCount = dv.getUint8(7);

  const cmds: ParsedCmd[] = [];
  let off = HEADER_LEN;
  for (let i = 0; i < cmdCount; i++) {
    const modType = dv.getUint8(off + 0);
    const slotId = dv.getUint8(off + 1);
    const startMs = dv.getUint32(off + 2, true);
    const durationMs = dv.getUint32(off + 6, true);
    const paramsLen = dv.getUint16(off + 10, true);
    const targetKind = dv.getUint8(off + 12);
    const targetLen = dv.getUint8(off + 13);
    off += CMD_HDR_LEN;

    const target = blob.slice(off, off + targetLen);
    off += targetLen;
    const params = blob.slice(off, off + paramsLen);
    off += paramsLen;

    cmds.push({
      modType, slotId, startMs, durationMs, targetKind,
      socketMask: targetKind === TARGET_SOCKET_MASK ? target : new Uint8Array(0),
      params,
    });
  }

  // Everything after the last command must be exactly the signature — proves the
  // per-command lengths add up.
  expect(blob.length - off).toBe(SIG_LEN);
  return { version, cmds };
}

/** The 1-based NPPS socket ids a bitmap selects. */
function socketsInMask(mask: Uint8Array): number[] {
  const out: number[] = [];
  for (let bit = 0; bit < mask.length * 8; bit++) {
    if ((mask[bit >> 3] >> (bit & 7)) & 1) out.push(bit + 1);
  }
  return out;
}

// ─── Protocol builders ─────────────────────────────────────────────────────────

function pbmProtocol(params: Partial<PBMTranscranialParams>): NPProtocolDefinition {
  return {
    id: 'test-pbm',
    name: 'Test PBM',
    description: '',
    author: 'test',
    version: '1',
    tags: [],
    createdAt: '2026-01-01',
    modifiedAt: '2026-01-01',
    isPredefined: false,
    timingMode: { type: 'duration', seconds: 600 },
    modalities: [{
      id: 'm1',
      enabled: true,
      interval: { intervalOnSeconds: 0, intervalOffSeconds: 0 },
      modalityParams: {
        type: 'pbm_transcranial',
        params: {
          zones: 'named',
          wavelength: '660_808nm',
          intensityPercent: 80,
          frequencyHz: 40,
          dutyCyclePercent: 25,
          ...params,
        } as PBMTranscranialParams,
      },
    }],
  } as NPProtocolDefinition;
}

function eegProtocol(): NPProtocolDefinition {
  const p = pbmProtocol({ zoneRefs: ['All'] });
  p.modalities = [{
    id: 'm1',
    enabled: true,
    interval: { intervalOnSeconds: 0, intervalOffSeconds: 0 },
    modalityParams: {
      type: 'eeg_neurofeedback',
      params: { channels: 'all', band: 'alpha', closedLoopEnabled: true },
    },
  }] as NPProtocolDefinition['modalities'];
  return p;
}

// ─── Tests ─────────────────────────────────────────────────────────────────────

describe('hub protocol v2 wire format', () => {
  it('emits protocol version 2', () => {
    const { blob } = compileProtocol(pbmProtocol({ zoneRefs: ['Frontal Left'] }), { zones });
    expect(readBlob(blob).version).toBe(2);
  });

  it('emits a 14-byte command header with a target block', () => {
    const { blob } = compileProtocol(pbmProtocol({ zoneRefs: ['Frontal Left'] }), { zones });
    // 1 continuous command: header + cmdhdr + 16-byte mask + 4 params + sig.
    expect(blob.length).toBe(HEADER_LEN + CMD_HDR_LEN + SOCKET_MASK_BYTES + 4 + SIG_LEN);
  });
});

describe('socket-addressed cranial targets', () => {
  it('compiles a named zone to that zone\'s exact sockets', () => {
    const { blob } = compileProtocol(pbmProtocol({ zoneRefs: ['Frontal Left'] }), { zones });
    const [cmd] = readBlob(blob).cmds;

    expect(cmd.targetKind).toBe(TARGET_SOCKET_MASK);
    expect(cmd.socketMask.length).toBe(SOCKET_MASK_BYTES);
    expect(socketsInMask(cmd.socketMask)).toEqual(zones.get('Frontal Left')!.sockets);
  });

  it('carries no slot id on a socket-addressed command', () => {
    const { blob } = compileProtocol(pbmProtocol({ zoneRefs: ['Frontal Left'] }), { zones });
    expect(readBlob(blob).cmds[0].slotId).toBe(SLOT_NONE);
  });

  it('converts 1-based NPPS ids to 0-based firmware bits', () => {
    // NPPS socket id N sets bit N-1. Derived from the zone's own sockets so the
    // conversion — not a lattice coincidence — is what's under test.
    const fl = zones.get('Frontal Left')!.sockets;
    const { blob } = compileProtocol(pbmProtocol({ zoneRefs: ['Frontal Left'] }), { zones });
    const mask = readBlob(blob).cmds[0].socketMask;

    const bitSet = (n: number) => (mask[(n - 1) >> 3] >> ((n - 1) & 7)) & 1;
    for (const id of fl) expect(bitSet(id)).toBe(1);               // every zone socket → its bit
    const notInZone = NP_SOCKETS.map(s => s.id).find(id => !fl.includes(id))!;
    expect(bitSet(notInZone)).toBe(0);                             // a non-member stays clear
  });

  it('reaches the highest wired socket', () => {
    const { blob } = compileProtocol(pbmProtocol({ zoneRefs: ['All'] }), { zones });
    const selected = socketsInMask(readBlob(blob).cmds[0].socketMask);
    // Derived from the shipped lattice, not a literal, so a lattice re-cut moves
    // it automatically (the socket count has grown 78 -> 80 already).
    const highest = Math.max(...NP_SOCKETS.map(s => s.id));
    expect(selected).toHaveLength(NP_SOCKETS.length);
    expect(selected[selected.length - 1]).toBe(highest);
  });

  it('sizes the mask to the firmware socket domain, not the shell socket count', () => {
    // 16 bytes = 128 bits = NP_HEXMAP_MAX_SOCKETS, though far fewer are wired.
    const { blob } = compileProtocol(pbmProtocol({ zoneRefs: ['All'] }), { zones });
    expect(readBlob(blob).cmds[0].socketMask.length).toBe(16);
    expect(NP_SOCKETS.length).toBeLessThan(16 * 8);
  });

  it('unions multiple zoneRefs', () => {
    const { blob } = compileProtocol(
      pbmProtocol({ zoneRefs: ['Temporal Left', 'Temporal Right'] }), { zones });
    const expected = [...new Set([
      ...zones.get('Temporal Left')!.sockets,
      ...zones.get('Temporal Right')!.sockets,
    ])].sort((a, b) => a - b);
    expect(socketsInMask(readBlob(blob).cmds[0].socketMask)).toEqual(expected);
  });

  // The reason the wire carries a bitmap rather than a socket list: midline
  // sockets belong to both hemisphere zones, so a union names them twice, and a
  // list would drive those modules twice — double J/cm² on a real PBM protocol.
  it('delivers overlapping zones once per socket', () => {
    const fl = zones.get('Frontal Left')!.sockets;
    const fr = zones.get('Frontal Right')!.sockets;
    const overlap = fl.filter(s => fr.includes(s));
    // The frontal midline sockets — derived from the two zones, not a literal
    // (they are 2, 13, 29 on the current lattice). The test's point is that a
    // real overlap exists and is collapsed, whatever the lattice numbers it.
    expect(overlap.length).toBeGreaterThan(0);

    const { blob } = compileProtocol(
      pbmProtocol({ zoneRefs: ['Frontal Left', 'Frontal Right'] }), { zones });
    const selected = socketsInMask(readBlob(blob).cmds[0].socketMask);
    const distinct = [...new Set([...fl, ...fr])].sort((a, b) => a - b);

    expect(fl.length + fr.length).toBe(distinct.length + overlap.length); // list double-counts
    expect(selected).toHaveLength(distinct.length);                        // bitmap does not
    expect(selected).toEqual(distinct);
  });

  it('gives an interval protocol\'s stop commands the same target as the on', () => {
    const proto = pbmProtocol({ zoneRefs: ['Frontal Left'] });
    proto.modalities[0].interval =
      { intervalOnSeconds: 60, intervalOffSeconds: 60, repeatCount: 2 };

    const { cmds } = readBlob(compileProtocol(proto, { zones }).blob);
    expect(cmds.length).toBeGreaterThan(1);
    const first = cmds[0].socketMask;
    for (const c of cmds) {
      expect(c.targetKind).toBe(TARGET_SOCKET_MASK);
      expect(Array.from(c.socketMask)).toEqual(Array.from(first));
    }
  });
});

describe('clinician-selected targets', () => {
  it('refuses to compile without an operator socket selection', () => {
    expect(() => compileProtocol(pbmProtocol({ zones: 'clinician_selected' }), { zones }))
      .toThrow(/operator must choose the sockets/);
  });

  it('compiles the operator\'s sockets when given', () => {
    const { blob } = compileProtocol(
      pbmProtocol({ zones: 'clinician_selected' }),
      { zones, clinicianSockets: [30, 31, 32] });
    expect(socketsInMask(readBlob(blob).cmds[0].socketMask)).toEqual([30, 31, 32]);
  });
});

describe('retired five-slot selectors', () => {
  // These named the legacy zone-module slots. They are not migrated silently:
  // 00-zones.npps and nppsParser.ts disagree about what the numeric form meant,
  // and v1's `front` bitmask (0x07) covered frontal L/R *plus parietal L*, which
  // is not what 00-zones.npps says `front` migrates to.
  it.each([
    ['all', 'All'],
    ['front', 'Frontal'],
    ['rear', 'Posterior'],
  ])('refuses zones: %s and names the migration target %s', (selector, target) => {
    expect(() => compileProtocol(
      pbmProtocol({ zones: selector as PBMTranscranialParams['zones'] }), { zones }))
      .toThrow(new RegExp(`retired zone selector '${selector}'[\\s\\S]*"${target}"`));
  });

  it('refuses numeric custom zones as ambiguous rather than guessing a base', () => {
    expect(() => compileProtocol(
      pbmProtocol({ zones: 'custom', customZones: [1, 2] }), { zones }))
      .toThrow(/base is ambiguous/);
  });
});

describe('newly-created protocols', () => {
  // defaultParams() seeds the UI's "add a modality" flow. It used to seed the
  // retired `zones: 'all'` selector, which the compiler now refuses — so a
  // freshly-created protocol would not have compiled at all.
  it('compiles a protocol built from defaultParams', () => {
    const proto = pbmProtocol({});
    proto.modalities[0].modalityParams = {
      type: 'pbm_transcranial',
      params: defaultParams('pbm_transcranial'),
    };
    const { blob } = compileProtocol(proto, { zones });
    const selected = socketsInMask(readBlob(blob).cmds[0].socketMask);
    expect(selected).toHaveLength(NP_SOCKETS.length);
  });
});

describe('zone resolution failures', () => {
  it('refuses a named target with no zone namespace', () => {
    expect(() => compileProtocol(pbmProtocol({ zoneRefs: ['Frontal Left'] }), {}))
      .toThrow(/no zone namespace/);
  });

  it('refuses an unknown zone name', () => {
    expect(() => compileProtocol(pbmProtocol({ zoneRefs: ['Nonexistent'] }), { zones }))
      .toThrow(/not in the zone namespace/);
  });

  it('refuses a zone naming a socket this helmet does not have', () => {
    // One past the highest wired socket — derived, so a lattice re-cut keeps it
    // genuinely out of range (it was 78 sockets, now 80).
    const offHelmet = Math.max(...NP_SOCKETS.map(s => s.id)) + 1;
    const bad = new Map(zones);
    bad.set('Bad', { name: 'Bad', sockets: [offHelmet] });
    expect(() => compileProtocol(pbmProtocol({ zoneRefs: ['Bad'] }), { zones: bad }))
      .toThrow(new RegExp(`Socket ${offHelmet} is not a socket on this helmet`));
  });

  it('refuses a zone that resolves to no sockets', () => {
    const empty = new Map(zones);
    empty.set('Empty', { name: 'Empty', sockets: [] });
    expect(() => compileProtocol(pbmProtocol({ zoneRefs: ['Empty'] }), { zones: empty }))
      .toThrow(/targets no sockets/);
  });
});

describe('slot-addressed fixed devices', () => {
  // v1 emitted slot_mask 0x01 for every non-PBM modality. Bit 0 is zone slot 0,
  // whose registry entry is the PBM driver — so an EEG command was addressed to
  // the PBM module. Every fixed device now names itself.
  it('addresses EEG to the ADS1299 slot, not slot 0', () => {
    const { blob } = compileProtocol(eegProtocol(), { zones });
    const [cmd] = readBlob(blob).cmds;
    expect(cmd.modType).toBe(NP_MOD_EEG);
    expect(cmd.targetKind).toBe(TARGET_SLOT);
    expect(cmd.slotId).toBe(5);               // NP_HUB_SLOT_EEG
  });

  it('carries no target block on a slot-addressed command', () => {
    const { blob } = compileProtocol(eegProtocol(), { zones });
    expect(readBlob(blob).cmds[0].socketMask).toHaveLength(0);
    expect(blob.length).toBe(HEADER_LEN + CMD_HDR_LEN + 5 + SIG_LEN);
  });

  it('never emits a retired zone slot', () => {
    const { blob } = compileProtocol(eegProtocol(), { zones });
    for (const cmd of readBlob(blob).cmds) {
      if (cmd.targetKind === TARGET_SLOT) expect(cmd.slotId).toBeGreaterThanOrEqual(5);
    }
  });
});

// Every modality's params[] must be EXACTLY its firmware struct. The drivers
// check the length, and a mismatch either rejects the command or — worse, if the
// struct later grows — reinterprets a stray byte as a real field. The sizes below
// are the packed sizeof() values from np_hub_types.h; nothing but a deliberate
// wire change should move them.
describe('parameter block sizes match the firmware structs', () => {
  const EXPECTED: Record<string, { size: number; slot: number | null; params: object }> = {
    pbm_transcranial:   { size: 4,  slot: null, params: { zones: 'named', zoneRefs: ['All'], wavelength: '660_808nm', intensityPercent: 50, frequencyHz: 40, dutyCyclePercent: 25 } },
    pbm_intranasal:     { size: 5,  slot: 9,    params: { intensityPercent: 60, frequencyHz: 10, dutyCyclePercent: 25 } },
    eeg_neurofeedback:  { size: 5,  slot: 5,    params: { channels: 'all', band: 'alpha', closedLoopEnabled: false } },
    bes_tacs:           { size: 7,  slot: 17,   params: { frequencyHz: 10, intensityMilliamps: 0.8, waveform: 'sinusoidal' } },
    tdcs:               { size: 6,  slot: 18,   params: { intensityMilliamps: 1.5, electrodePairs: [['F3', 'F4']], rampSeconds: 30 } },
    vns_hrv:            { size: 9,  slot: 8,    params: { frequencyHz: 20, intensityMilliamps: 1, hrvProtocol: 'standalone' } },
    audio_entrainment:  { size: 8,  slot: 6,    params: { carrierHz: 200, binauralBeatsHz: 10, volumePercent: 50, boneConductionPacer: false, eegAdaptive: false } },
    visual_stimulation: { size: 9,  slot: 7,    params: { mode: 'binocular', frequencyHz: 10, emdrCadenceHz: 1, enableModeF: false } },
    qeeg_21ch:          { size: 8,  slot: 11,   params: { montage: 'standard_1020', reference: 'linked_ear', sloretaEnabled: true } },
    tms:                { size: 10, slot: 12,   params: { tmsProtocol: 'rTMS', target: 'DLPFC_L', frequencyHz: 10, intensityPercentMT: 110, pulseCount: 3000 } },
    pbm_deep_1170nm:    { size: 5,  slot: 13,   params: { intensityMWcm2: 500, frequencyHz: 40, dutyCyclePercent: 25 } },
    clinical_tacs:      { size: 7,  slot: 14,   params: { frequencyHz: 10, intensityMilliamps: 2, waveform: 'sinusoidal', channelCount: 16 } },
    hd_tdcs:            { size: 6,  slot: 15,   params: { target: 'DLPFC_L', montage: 'ring_4x1', intensityMilliamps: 2 } },
    cervical_vns:       { size: 10, slot: 10,   params: { frequencyHz: 25, intensityMilliamps: 1.5 } },
    vibrotactile_40hz:  { size: 4,  slot: 16,   params: { intensityG: 0.9, syncToAudio: true, syncToVisual: false } },
  };

  it.each(Object.entries(EXPECTED))('%s encodes %o', (type, spec) => {
    const proto = pbmProtocol({});
    proto.modalities[0].modalityParams =
      { type, params: spec.params } as NPProtocolDefinition['modalities'][number]['modalityParams'];

    const [cmd] = readBlob(compileProtocol(proto, { zones }).blob).cmds;
    expect(cmd.params).toHaveLength(spec.size);
    if (spec.slot === null) {
      expect(cmd.targetKind).toBe(TARGET_SOCKET_MASK);
    } else {
      expect(cmd.targetKind).toBe(TARGET_SLOT);
      expect(cmd.slotId).toBe(spec.slot);
    }
  });

  it('covers every modality type', () => {
    // A new modality must appear above, not slip through untested.
    expect(Object.keys(EXPECTED).sort()).toEqual(Object.keys(MODALITY_META).sort());
  });
});

describe('mixed-target protocols', () => {
  it('interleaves socket and slot targets without disturbing either', () => {
    const proto = pbmProtocol({ zoneRefs: ['Occipital Left'] });
    proto.modalities.push({
      id: 'm2',
      enabled: true,
      interval: { intervalOnSeconds: 0, intervalOffSeconds: 0 },
      modalityParams: {
        type: 'eeg_neurofeedback',
        params: { channels: 'all', band: 'alpha', closedLoopEnabled: false },
      },
    } as NPProtocolDefinition['modalities'][number]);

    const { cmds } = readBlob(compileProtocol(proto, { zones }).blob);
    expect(cmds).toHaveLength(2);

    const pbm = cmds.find(c => c.modType === NP_MOD_PBM_BASE)!;
    const eeg = cmds.find(c => c.modType === NP_MOD_EEG)!;

    expect(socketsInMask(pbm.socketMask)).toEqual(zones.get('Occipital Left')!.sockets);
    expect(pbm.params).toHaveLength(4);
    expect(eeg.slotId).toBe(5);
    expect(eeg.params).toHaveLength(5);
  });
});
