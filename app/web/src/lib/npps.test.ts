import { describe, it, expect } from 'vitest';
import { parseNPPS, tokenize, NPPSParseError } from './nppsParser';
import { serializeProtocol, serializeNPPS } from './nppsSerializer';
import type { NPProtocolDefinition, NPCompositeProtocol } from '../types/protocol';

// ─── Fixtures ─────────────────────────────────────────────────────────────────

const GAMMA_FOCUS_NPPS = `
protocol "Gamma Focus" {
    id: "10000001-0000-0000-0000-000000000000"
    description: "40Hz multi-sensory protocol"
    author: "NeurOne"
    version: "1.0"
    readonly: true
    tags: [focus, gamma]
    duration: 20m

    pbm_transcranial {
        intensity: 80%
        frequency: 40Hz
        duty_cycle: 25%
        zones: ["All"]
        wavelength: 660_808nm
    }

    audio_entrainment {
        binaural_hz: 40Hz
        noise: none
        carrier_hz: 440Hz
        volume: 70%
        eeg_adaptive: false
        bone_conduction_pacer: false
    }

    visual_stimulation {
        frequency: 40Hz
        mode: binocular
        emdr_cadence: 1Hz
        interval_on: 60s
        interval_off: 30s
        repeat: until_end
    }

    eeg_neurofeedback {
        band: gamma
        channels: all
        closed_loop: true
    }
}
`.trim();

const ALL_T1_NPPS = `
protocol "All T1 Modalities" {
    id: "00000000-0000-0000-0000-000000000001"
    description: "All eight T1 modalities"
    author: "test"
    version: "1.0"
    tags: []
    duration: 30m

    pbm_transcranial {
        intensity: 75%
        frequency: 20Hz
        duty_cycle: 25%
        zones: ["All"]
        wavelength: 660_808nm
    }

    pbm_intranasal {
        intensity: 60%
        frequency: 40Hz
        duty_cycle: 25%
    }

    eeg_neurofeedback {
        band: alpha
        channels: all
        closed_loop: true
    }

    bes_tacs {
        frequency: 10Hz
        intensity: 0.8mA
        waveform: sinusoidal
    }

    tdcs {
        intensity: 1.5mA
        electrode_pairs: [["Fp1", "P3"]]
        ramp: 30s
    }

    vns_hrv {
        frequency: 25Hz
        intensity: 1.5mA
        hrv_protocol: standalone
        breathing_rate: 6
    }

    audio_entrainment {
        binaural_hz: 40Hz
        noise: pink
        carrier_hz: 440Hz
        volume: 60%
        eeg_adaptive: true
        bone_conduction_pacer: true
    }

    visual_stimulation {
        frequency: 40Hz
        mode: binocular
        emdr_cadence: 1Hz
        enable_mode_f: false
    }
}
`.trim();

const COMPOSITE_NPPS = `
composite "Full Multi-Modal RCT" {
    id: "10000100-0000-0000-0000-000000000000"
    description: "RCT protocol"
    author: "NeurOne"
    version: "1.0"
    readonly: true
    tags: [rct, multimodal]
    conflict_resolution: merge

    layer "Vascular Baseline" {
        start: 0s
        duration: 5m
        intensity_scale: 0.5
    }

    layer "Gamma Focus" {
        start: 300s
        duration: 20m
        intensity_scale: 1.0
    }
}
`.trim();

// ─── Tokenizer ────────────────────────────────────────────────────────────────

describe('tokenizer', () => {
  it('tokenizes unit suffixes correctly', () => {
    const tokens = tokenize('40Hz 80% 1.5mA 20m 300s');
    const numbers = tokens.filter(t => t.type === 'NUMBER');
    expect(numbers[0]).toMatchObject({ value: 40, unit: 'Hz' });
    expect(numbers[1]).toMatchObject({ value: 80, unit: '%' });
    expect(numbers[2]).toMatchObject({ value: 1.5, unit: 'mA' });
    expect(numbers[3]).toMatchObject({ value: 20, unit: 'm' });
    expect(numbers[4]).toMatchObject({ value: 300, unit: 's' });
  });

  it('tokenizes strings with escape sequences', () => {
    const tokens = tokenize('"hello \\"world\\""');
    expect(tokens[0]).toMatchObject({ type: 'STRING', value: 'hello "world"' });
  });

  it('throws on unterminated string', () => {
    expect(() => tokenize('"unterminated')).toThrow(NPPSParseError);
  });

  it('throws on stray minus sign', () => {
    expect(() => tokenize('- ')).toThrow(NPPSParseError);
  });

  it('does not include "name" as a keyword', () => {
    const tokens = tokenize('name');
    expect(tokens[0].type).toBe('IDENT');
  });
});

// ─── Parser — old format rejection ───────────────────────────────────────────

describe('parser — old format rejection', () => {
  it('rejects protocol with "name:" field (old format key)', () => {
    // Old format: name field inside protocol block was the classic marker
    const oldFormat = `
protocol {
    name: "Gamma Focus"
    timing { duration: 1200 }
    modalities [
        modality { type: "pbm_transcranial" intensity_percent: 80 }
    ]
}`.trim();
    expect(() => parseNPPS(oldFormat)).toThrow(NPPSParseError);
  });

  it('rejects "modalities [...]" wrapper block', () => {
    const oldFormat = `
protocol "Test" {
    duration: 20m
    modalities [
        modality { type: "pbm_transcranial" }
    ]
}`.trim();
    expect(() => parseNPPS(oldFormat)).toThrow(NPPSParseError);
  });

  it('rejects "timing { ... }" block', () => {
    const oldFormat = `
protocol "Test" {
    timing { duration: 1200 }
    pbm_transcranial { intensity: 80% }
}`.trim();
    expect(() => parseNPPS(oldFormat)).toThrow(NPPSParseError);
  });

  it('rejects inline "layers [ ... ]" wrapper in composite', () => {
    const oldFormat = `
composite "Test" {
    conflict_resolution: merge
    layers [
        layer "Base" { start: 0s }
    ]
}`.trim();
    expect(() => parseNPPS(oldFormat)).toThrow(NPPSParseError);
  });
});

// ─── Parser — new format ──────────────────────────────────────────────────────

describe('parser — new format', () => {
  it('parses a single protocol', () => {
    const entries = parseNPPS(GAMMA_FOCUS_NPPS);
    expect(entries).toHaveLength(1);
    expect(entries[0].kind).toBe('single');
  });

  it('parses protocol metadata correctly', () => {
    const proto = (parseNPPS(GAMMA_FOCUS_NPPS)[0] as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    expect(proto.name).toBe('Gamma Focus');
    expect(proto.id).toBe('10000001-0000-0000-0000-000000000000');
    expect(proto.description).toBe('40Hz multi-sensory protocol');
    expect(proto.author).toBe('NeurOne');
    expect(proto.version).toBe('1.0');
    expect(proto.isReadOnly).toBe(true);
    expect(proto.tags).toEqual(['focus', 'gamma']);
    expect(proto.timingMode).toMatchObject({ type: 'duration', seconds: 1200 });
  });

  it('parses 4 modalities from Gamma Focus', () => {
    const proto = (parseNPPS(GAMMA_FOCUS_NPPS)[0] as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    expect(proto.modalities).toHaveLength(4);
    const types = proto.modalities.map(m => m.modalityParams.type);
    expect(types).toEqual(['pbm_transcranial', 'audio_entrainment', 'visual_stimulation', 'eeg_neurofeedback']);
  });

  it('parses pbm_transcranial params with short aliases', () => {
    const proto = (parseNPPS(GAMMA_FOCUS_NPPS)[0] as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    const pbm = proto.modalities[0].modalityParams;
    expect(pbm.type).toBe('pbm_transcranial');
    if (pbm.type === 'pbm_transcranial') {
      expect(pbm.params.intensityPercent).toBe(80);
      expect(pbm.params.frequencyHz).toBe(40);
      expect(pbm.params.dutyCyclePercent).toBe(25);
      expect(pbm.params.zones).toBe('named');
      expect(pbm.params.zoneRefs).toEqual(['All']);
      expect(pbm.params.wavelength).toBe('660_808nm');
    }
  });

  it('parses visual_stimulation interval fields', () => {
    const proto = (parseNPPS(GAMMA_FOCUS_NPPS)[0] as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    const visual = proto.modalities[2];
    expect(visual.modalityParams.type).toBe('visual_stimulation');
    expect(visual.interval.intervalOnSeconds).toBe(60);
    expect(visual.interval.intervalOffSeconds).toBe(30);
    expect(visual.interval.repeatCount).toBeUndefined(); // "until_end"
  });

  it('parses eeg_neurofeedback band + closed_loop alias', () => {
    const proto = (parseNPPS(GAMMA_FOCUS_NPPS)[0] as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    const eeg = proto.modalities[3].modalityParams;
    if (eeg.type === 'eeg_neurofeedback') {
      expect(eeg.params.band).toBe('gamma');
      expect(eeg.params.closedLoopEnabled).toBe(true);
    }
  });

  it('parses all 8 T1 modalities', () => {
    const proto = (parseNPPS(ALL_T1_NPPS)[0] as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    expect(proto.modalities).toHaveLength(8);
    const types = proto.modalities.map(m => m.modalityParams.type);
    expect(types).toContain('pbm_transcranial');
    expect(types).toContain('pbm_intranasal');
    expect(types).toContain('eeg_neurofeedback');
    expect(types).toContain('bes_tacs');
    expect(types).toContain('tdcs');
    expect(types).toContain('vns_hrv');
    expect(types).toContain('audio_entrainment');
    expect(types).toContain('visual_stimulation');
  });

  it('parses bes_tacs intensity in mA', () => {
    const proto = (parseNPPS(ALL_T1_NPPS)[0] as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    const bes = proto.modalities.find(m => m.modalityParams.type === 'bes_tacs')!.modalityParams;
    if (bes.type === 'bes_tacs') {
      expect(bes.params.intensityMilliamps).toBe(0.8);
      expect(bes.params.frequencyHz).toBe(10);
    }
  });

  it('parses tdcs electrode_pairs', () => {
    const proto = (parseNPPS(ALL_T1_NPPS)[0] as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    const tdcs = proto.modalities.find(m => m.modalityParams.type === 'tdcs')!.modalityParams;
    if (tdcs.type === 'tdcs') {
      expect(tdcs.params.electrodePairs).toEqual([['Fp1', 'P3']]);
      expect(tdcs.params.rampSeconds).toBe(30);
    }
  });

  it('parses vns_hrv protocol and breathing rate aliases', () => {
    const proto = (parseNPPS(ALL_T1_NPPS)[0] as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    const vns = proto.modalities.find(m => m.modalityParams.type === 'vns_hrv')!.modalityParams;
    if (vns.type === 'vns_hrv') {
      expect(vns.params.hrvProtocol).toBe('standalone');
      expect(vns.params.resonanceBreathingRate).toBe(6);
    }
  });

  it('parses duration in minutes', () => {
    const proto = (parseNPPS(ALL_T1_NPPS)[0] as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    expect(proto.timingMode).toMatchObject({ type: 'duration', seconds: 1800 }); // 30m = 1800s
  });

  it('parses composite with layers', () => {
    const entries = parseNPPS(COMPOSITE_NPPS);
    expect(entries[0].kind).toBe('composite');
    const composite = (entries[0] as { kind: 'composite'; composite: NPCompositeProtocol }).composite;
    expect(composite.name).toBe('Full Multi-Modal RCT');
    expect(composite.conflictResolution).toBe('merge');
    expect(composite.layers).toHaveLength(2);
    expect(composite.layers[0].protocolName).toBe('Vascular Baseline');
    expect(composite.layers[0].startOffsetSeconds).toBe(0);
    expect(composite.layers[0].durationSeconds).toBe(300); // 5m
    expect(composite.layers[0].intensityScale).toBeCloseTo(0.5);
    expect(composite.layers[1].protocolName).toBe('Gamma Focus');
    expect(composite.layers[1].startOffsetSeconds).toBe(300);
    expect(composite.layers[1].durationSeconds).toBe(1200); // 20m
  });

  it('parses multiple top-level entries from one string', () => {
    const combined = GAMMA_FOCUS_NPPS + '\n\n' + COMPOSITE_NPPS;
    const entries = parseNPPS(combined);
    expect(entries).toHaveLength(2);
    expect(entries[0].kind).toBe('single');
    expect(entries[1].kind).toBe('composite');
  });

  it('ignores # comments', () => {
    const withComments = `
# This is a comment
protocol "Test" {
    # Another comment
    id: "00000000-0000-0000-0000-000000000099"
    description: ""
    author: "test"
    version: "1.0"
    tags: []
    duration: 10m
    # eeg_neurofeedback { band: alpha channels: all }
}`.trim();
    const entries = parseNPPS(withComments);
    expect(entries).toHaveLength(1);
    const proto = (entries[0] as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    expect(proto.modalities).toHaveLength(0); // comment not parsed as modality
  });

  it('throws NPPSParseError on empty protocol block with no name', () => {
    expect(() => parseNPPS('protocol {')).toThrow(NPPSParseError);
  });
});

// ─── Serializer ───────────────────────────────────────────────────────────────

describe('serializer', () => {
  it('serializes a single protocol with correct header', () => {
    const [entry] = parseNPPS(GAMMA_FOCUS_NPPS);
    const out = serializeProtocol(entry);
    expect(out).toMatch(/^protocol "Gamma Focus" \{/);
    expect(out).toContain('duration: 20m');
    expect(out).toContain('readonly: true');
  });

  it('serializes pbm_transcranial fields with unit suffixes', () => {
    const [entry] = parseNPPS(GAMMA_FOCUS_NPPS);
    const out = serializeProtocol(entry);
    expect(out).toContain('pbm_transcranial {');
    expect(out).toContain('intensity: 80%');
    expect(out).toContain('frequency: 40Hz');
    expect(out).toContain('duty_cycle: 25%');
    expect(out).toContain('zones: ["All"]');
    expect(out).toContain('wavelength: 660_808nm');
  });

  it('serializes interval fields inside modality block', () => {
    const [entry] = parseNPPS(GAMMA_FOCUS_NPPS);
    const out = serializeProtocol(entry);
    // 60s = 1m (formatTime normalises to minutes when evenly divisible)
    expect(out).toContain('interval_on: 1m');
    expect(out).toContain('interval_off: 30s');
    expect(out).toContain('repeat: until_end');
  });

  it('does NOT emit "name:" field (old format)', () => {
    const [entry] = parseNPPS(GAMMA_FOCUS_NPPS);
    const out = serializeProtocol(entry);
    expect(out).not.toContain('name:');
  });

  it('does NOT emit "modalities [" wrapper (old format)', () => {
    const [entry] = parseNPPS(GAMMA_FOCUS_NPPS);
    const out = serializeProtocol(entry);
    expect(out).not.toMatch(/modalities\s*\[/);
  });

  it('does NOT emit "modality {" typed wrapper (old format)', () => {
    const [entry] = parseNPPS(GAMMA_FOCUS_NPPS);
    const out = serializeProtocol(entry);
    expect(out).not.toMatch(/\bmodality\s*\{/);
  });

  it('does NOT emit "timing {" block (old format)', () => {
    const [entry] = parseNPPS(GAMMA_FOCUS_NPPS);
    const out = serializeProtocol(entry);
    expect(out).not.toMatch(/\btiming\s*\{/);
  });

  it('serializes bes_tacs intensity in mA', () => {
    const [entry] = parseNPPS(ALL_T1_NPPS);
    const out = serializeProtocol(entry);
    expect(out).toContain('bes_tacs {');
    expect(out).toContain('intensity: 0.8mA');
  });

  it('serializes tdcs with electrode_pairs', () => {
    const [entry] = parseNPPS(ALL_T1_NPPS);
    const out = serializeProtocol(entry);
    expect(out).toContain('tdcs {');
    expect(out).toContain('electrode_pairs:');
    expect(out).toContain('"Fp1"');
    expect(out).toContain('"P3"');
    expect(out).toContain('ramp: 30s');
  });

  it('serializes composite without "layers [" wrapper', () => {
    const [entry] = parseNPPS(COMPOSITE_NPPS);
    const out = serializeProtocol(entry);
    expect(out).toMatch(/^composite "Full Multi-Modal RCT" \{/);
    expect(out).not.toMatch(/\blayers\s*\[/);
    expect(out).toContain('layer "Vascular Baseline" {');
    expect(out).toContain('layer "Gamma Focus" {');
    expect(out).toContain('start: 0s');
    expect(out).toContain('duration: 5m');
    expect(out).toContain('intensity_scale: 0.5');
  });

  it('omits intensity_scale: 1.0 (default) from composite layer', () => {
    const [entry] = parseNPPS(COMPOSITE_NPPS);
    const out = serializeProtocol(entry);
    // Gamma Focus layer has intensity_scale: 1.0 — should be omitted
    const gammaBlock = out.split('layer "Gamma Focus"')[1].split('}')[0];
    expect(gammaBlock).not.toContain('intensity_scale');
  });
});

// ─── Round-trip: serialize → parse → serialize ────────────────────────────────

describe('round-trip', () => {
  function roundTrip(input: string): string {
    const entries = parseNPPS(input);
    const serialized = serializeNPPS(entries);
    const reparsed = parseNPPS(serialized);
    return serializeNPPS(reparsed);
  }

  it('Gamma Focus is stable after two serialize/parse cycles', () => {
    const once = roundTrip(GAMMA_FOCUS_NPPS);
    const twice = roundTrip(once);
    expect(once).toBe(twice);
  });

  it('All T1 modalities protocol is stable after two serialize/parse cycles', () => {
    const once = roundTrip(ALL_T1_NPPS);
    const twice = roundTrip(once);
    expect(once).toBe(twice);
  });

  it('Composite protocol is stable after two serialize/parse cycles', () => {
    const once = roundTrip(COMPOSITE_NPPS);
    const twice = roundTrip(once);
    expect(once).toBe(twice);
  });

  it('protocol name, id, and modality count survive round-trip', () => {
    const [entry] = parseNPPS(GAMMA_FOCUS_NPPS);
    const serialized = serializeProtocol(entry);
    const [reparsed] = parseNPPS(serialized);
    const orig = (entry as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    const rt = (reparsed as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    expect(rt.name).toBe(orig.name);
    expect(rt.id).toBe(orig.id);
    expect(rt.modalities).toHaveLength(orig.modalities.length);
  });

  it('interval params survive round-trip', () => {
    const [entry] = parseNPPS(GAMMA_FOCUS_NPPS);
    const serialized = serializeProtocol(entry);
    const [reparsed] = parseNPPS(serialized);
    const visual = (reparsed as { kind: 'single'; protocol: NPProtocolDefinition })
      .protocol.modalities.find(m => m.modalityParams.type === 'visual_stimulation')!;
    expect(visual.interval.intervalOnSeconds).toBe(60);
    expect(visual.interval.intervalOffSeconds).toBe(30);
    expect(visual.interval.repeatCount).toBeUndefined();
  });

  it('duration in minutes survives round-trip as minutes', () => {
    const [entry] = parseNPPS(GAMMA_FOCUS_NPPS);
    const serialized = serializeProtocol(entry);
    expect(serialized).toContain('duration: 20m');
    const [reparsed] = parseNPPS(serialized);
    const proto = (reparsed as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    expect(proto.timingMode).toMatchObject({ type: 'duration', seconds: 1200 });
  });

  it('composite layers survive round-trip with correct offsets', () => {
    const [entry] = parseNPPS(COMPOSITE_NPPS);
    const serialized = serializeProtocol(entry);
    const [reparsed] = parseNPPS(serialized);
    const composite = (reparsed as { kind: 'composite'; composite: NPCompositeProtocol }).composite;
    expect(composite.layers[0].startOffsetSeconds).toBe(0);
    expect(composite.layers[0].durationSeconds).toBe(300);
    expect(composite.layers[1].startOffsetSeconds).toBe(300);
    expect(composite.layers[1].durationSeconds).toBe(1200);
  });
});

describe('Mode F field name (enable_mode_f / legacy mode_f)', () => {
  const modeFSource = (key: string) => `
protocol "Retinal" {
    duration: 3m
    visual_stimulation {
        frequency: 0Hz
        mode: mode_f
        ${key}: true
    }
}
`;

  const visualOf = (src: string) => {
    const [entry] = parseNPPS(src);
    return (entry as { kind: 'single'; protocol: NPProtocolDefinition })
      .protocol.modalities.find(m => m.modalityParams.type === 'visual_stimulation')!;
  };

  it('parses the canonical enable_mode_f key', () => {
    const v = visualOf(modeFSource('enable_mode_f'));
    expect((v.modalityParams.params as { enableModeF: boolean }).enableModeF).toBe(true);
  });

  // Regression: iOS emitted `mode_f: true` while this parser only read
  // `enable_mode_f`, so the flag was silently dropped as an unknown key —
  // including in the shipped 09-retinal-health.npps.
  it('accepts the legacy mode_f key as an alias', () => {
    const v = visualOf(modeFSource('mode_f'));
    expect((v.modalityParams.params as { enableModeF: boolean }).enableModeF).toBe(true);
  });

  it('does not confuse the mode_f VALUE with the mode_f KEY', () => {
    const v = visualOf(modeFSource('enable_mode_f'));
    expect((v.modalityParams.params as { mode: string }).mode).toBe('mode_f');
  });

  it('serializes the canonical key, and it round-trips from the legacy spelling', () => {
    const [entry] = parseNPPS(modeFSource('mode_f'));
    const serialized = serializeProtocol(entry);
    expect(serialized).toContain('enable_mode_f: true');
    expect(serialized).not.toMatch(/^\s*mode_f:/m);
    const v = visualOf(serialized);
    expect((v.modalityParams.params as { enableModeF: boolean }).enableModeF).toBe(true);
  });
});
