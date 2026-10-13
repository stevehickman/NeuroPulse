import { describe, it, expect } from 'vitest';
import { parseNPPS, parseNPPSLimits, NPPSParseError } from '../../../../common/lib/nppsParser';
import { serializeProtocol, serializeNPPS } from '../../../../common/lib/nppsSerializer';
import type { NPProtocolDefinition, NPCompositeProtocol } from '../../../../common/types/protocol';
import { MODALITY_META } from '../../../../common/types/protocol';

// ─── Fixtures ─────────────────────────────────────────────────────────────────

const GAMMA_FOCUS_NPPS = `
protocol "Gamma Focus" {
    hardware_version: 1
    id: "10000001-0000-0000-0000-000000000000"
    description: "40Hz multi-sensory protocol"
    author: "NeurOne"
    version: "1.0"
    readonly: true
    tags: [focus, gamma]
    duration: 20m

    pbm_transcranial {
        irradiance: 322mW_cm2
        frequency: 40Hz
        duty_cycle: 25%
        zones: ["All"]
        wavelength: \"808nm\"
    }

    audio_entrainment {
        binaural_hz: 40Hz
        noise: none
        carrier_hz: 440Hz
        volume: 75dB
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
    hardware_version: 1
    id: "00000000-0000-0000-0000-000000000001"
    description: "All eight T1 modalities"
    author: "test"
    version: "1.0"
    tags: []
    duration: 30m

    pbm_transcranial {
        irradiance: 302mW_cm2
        frequency: 20Hz
        duty_cycle: 25%
        zones: ["All"]
        wavelength: \"808nm\"
    }

    pbm_intranasal {
        irradiance: 60mW_cm2
        frequency: 40Hz
        duty_cycle: 25%
        wavelength: "660nm"
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
        electrode_area_cm2: 35
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
        volume: 70dB
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
    hardware_version: 1
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

// The tokenizer is the core's lexer now; its cases are unit tests in common/npps-core/src/lexer.rs.

// ─── Parser — old format rejection ───────────────────────────────────────────

describe('parser — old format rejection', () => {
  // @verifies REQ-NPPS-10
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

  // @verifies REQ-NPPS-10
  it('rejects "modalities [...]" wrapper block', () => {
    const oldFormat = `
protocol "Test" {
    hardware_version: 1
    duration: 20m
    modalities [
        modality { type: "pbm_transcranial" }
    ]
}`.trim();
    expect(() => parseNPPS(oldFormat)).toThrow(NPPSParseError);
  });

  // @verifies REQ-NPPS-10
  it('rejects "timing { ... }" block', () => {
    const oldFormat = `
protocol "Test" {
    hardware_version: 1
    timing { duration: 1200 }
    pbm_transcranial { irradiance: 322mW_cm2 wavelength: "808nm" }
}`.trim();
    expect(() => parseNPPS(oldFormat)).toThrow(NPPSParseError);
  });

  // @verifies REQ-NPPS-10
  it('rejects inline "layers [ ... ]" wrapper in composite', () => {
    const oldFormat = `
composite "Test" {
    hardware_version: 1
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
  // @verifies REQ-NPPS-16
  it('parses a single protocol', () => {
    const entries = parseNPPS(GAMMA_FOCUS_NPPS);
    expect(entries).toHaveLength(1);
    expect(entries[0].kind).toBe('single');
  });

  // @verifies REQ-NPPS-16
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

  // @verifies REQ-NPPS-16
  it('parses 4 modalities from Gamma Focus', () => {
    const proto = (parseNPPS(GAMMA_FOCUS_NPPS)[0] as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    expect(proto.modalities).toHaveLength(4);
    const types = proto.modalities.map(m => m.modalityParams.type);
    expect(types).toEqual(['pbm_transcranial', 'audio_entrainment', 'visual_stimulation', 'eeg_neurofeedback']);
  });

  // @verifies REQ-NPPS-16
  it('parses pbm_transcranial params with short aliases', () => {
    const proto = (parseNPPS(GAMMA_FOCUS_NPPS)[0] as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    const pbm = proto.modalities[0].modalityParams;
    expect(pbm.type).toBe('pbm_transcranial');
    if (pbm.type === 'pbm_transcranial') {
      expect(pbm.params.irradianceMWcm2).toBe(322);
      expect(pbm.params.frequencyHz).toBe(40);
      expect(pbm.params.dutyCyclePercent).toBe(25);
      expect(pbm.params.zones).toBe('named');
      expect(pbm.params.zoneRefs).toEqual(['All']);
      expect(pbm.params.wavelength).toBe('808nm');
    }
  });

  // @verifies REQ-NPPS-16
  it('parses visual_stimulation interval fields', () => {
    const proto = (parseNPPS(GAMMA_FOCUS_NPPS)[0] as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    const visual = proto.modalities[2];
    expect(visual.modalityParams.type).toBe('visual_stimulation');
    expect(visual.interval.intervalOnSeconds).toBe(60);
    expect(visual.interval.intervalOffSeconds).toBe(30);
    expect(visual.interval.repeatCount).toBeUndefined(); // "until_end"
  });

  // @verifies REQ-NPPS-16
  it('parses eeg_neurofeedback band + closed_loop alias', () => {
    const proto = (parseNPPS(GAMMA_FOCUS_NPPS)[0] as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    const eeg = proto.modalities[3].modalityParams;
    if (eeg.type === 'eeg_neurofeedback') {
      expect(eeg.params.band).toBe('gamma');
      expect(eeg.params.closedLoopEnabled).toBe(true);
    }
  });

  // @verifies REQ-NPPS-16
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

  // @verifies REQ-NPPS-16
  it('parses bes_tacs intensity in mA', () => {
    const proto = (parseNPPS(ALL_T1_NPPS)[0] as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    const bes = proto.modalities.find(m => m.modalityParams.type === 'bes_tacs')!.modalityParams;
    if (bes.type === 'bes_tacs') {
      expect(bes.params.intensityMilliamps).toBe(0.8);
      expect(bes.params.frequencyHz).toBe(10);
    }
  });

  // @verifies REQ-NPPS-16
  it('parses tdcs electrode_pairs', () => {
    const proto = (parseNPPS(ALL_T1_NPPS)[0] as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    const tdcs = proto.modalities.find(m => m.modalityParams.type === 'tdcs')!.modalityParams;
    if (tdcs.type === 'tdcs') {
      expect(tdcs.params.electrodePairs).toEqual([['Fp1', 'P3']]);
      expect(tdcs.params.rampSeconds).toBe(30);
      expect(tdcs.params.electrodeAreaCm2).toBe(35);
    }
  });


  // @verifies REQ-NPPS-16
  it('parses vns_hrv protocol and breathing rate aliases', () => {
    const proto = (parseNPPS(ALL_T1_NPPS)[0] as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    const vns = proto.modalities.find(m => m.modalityParams.type === 'vns_hrv')!.modalityParams;
    if (vns.type === 'vns_hrv') {
      expect(vns.params.hrvProtocol).toBe('standalone');
      expect(vns.params.resonanceBreathingRate).toBe(6);
    }
  });

  // @verifies REQ-NPPS-16
  it('parses duration in minutes', () => {
    const proto = (parseNPPS(ALL_T1_NPPS)[0] as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    expect(proto.timingMode).toMatchObject({ type: 'duration', seconds: 1800 }); // 30m = 1800s
  });

  // @verifies REQ-NPPS-10
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

  // @verifies REQ-NPPS-16
  it('parses multiple top-level entries from one string', () => {
    const combined = GAMMA_FOCUS_NPPS + '\n\n' + COMPOSITE_NPPS;
    const entries = parseNPPS(combined);
    expect(entries).toHaveLength(2);
    expect(entries[0].kind).toBe('single');
    expect(entries[1].kind).toBe('composite');
  });

  // @verifies REQ-NPPS-16
  it('ignores # comments', () => {
    const withComments = `
# This is a comment
protocol "Test" {
    hardware_version: 1
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

  // @verifies REQ-NPPS-16
  it('throws NPPSParseError on empty protocol block with no name', () => {
    expect(() => parseNPPS('protocol {')).toThrow(NPPSParseError);
  });
});

// ─── Serializer ───────────────────────────────────────────────────────────────

describe('serializer', () => {
  // @verifies REQ-NPPS-11
  it('serializes a single protocol with correct header', () => {
    const [entry] = parseNPPS(GAMMA_FOCUS_NPPS);
    const out = serializeProtocol(entry);
    expect(out).toMatch(/^protocol "Gamma Focus" \{/);
    expect(out).toContain('duration: 20m');
    expect(out).toContain('readonly: true');
  });

  // @verifies REQ-NPPS-11
  it('serializes pbm_transcranial fields with unit suffixes', () => {
    const [entry] = parseNPPS(GAMMA_FOCUS_NPPS);
    const out = serializeProtocol(entry);
    expect(out).toContain('pbm_transcranial {');
    expect(out).toContain('irradiance: 322mW_cm2');
    expect(out).toContain('frequency: 40Hz');
    expect(out).toContain('duty_cycle: 25%');
    expect(out).toContain('zones: ["All"]');
    expect(out).toContain('wavelength: \"808nm\"');
  });

  // @verifies REQ-NPPS-11
  it('serializes interval fields inside modality block', () => {
    const [entry] = parseNPPS(GAMMA_FOCUS_NPPS);
    const out = serializeProtocol(entry);
    // 60s = 1m (formatTime normalises to minutes when evenly divisible)
    expect(out).toContain('interval_on: 1m');
    expect(out).toContain('interval_off: 30s');
    expect(out).toContain('repeat: until_end');
  });

  // @verifies REQ-NPPS-10
  it('does NOT emit "name:" field (old format)', () => {
    const [entry] = parseNPPS(GAMMA_FOCUS_NPPS);
    const out = serializeProtocol(entry);
    expect(out).not.toContain('name:');
  });

  // @verifies REQ-NPPS-10
  it('does NOT emit "modalities [" wrapper (old format)', () => {
    const [entry] = parseNPPS(GAMMA_FOCUS_NPPS);
    const out = serializeProtocol(entry);
    expect(out).not.toMatch(/modalities\s*\[/);
  });

  // @verifies REQ-NPPS-10
  it('does NOT emit "modality {" typed wrapper (old format)', () => {
    const [entry] = parseNPPS(GAMMA_FOCUS_NPPS);
    const out = serializeProtocol(entry);
    expect(out).not.toMatch(/\bmodality\s*\{/);
  });

  // @verifies REQ-NPPS-10
  it('does NOT emit "timing {" block (old format)', () => {
    const [entry] = parseNPPS(GAMMA_FOCUS_NPPS);
    const out = serializeProtocol(entry);
    expect(out).not.toMatch(/\btiming\s*\{/);
  });

  // @verifies REQ-NPPS-11
  it('serializes bes_tacs intensity in mA', () => {
    const [entry] = parseNPPS(ALL_T1_NPPS);
    const out = serializeProtocol(entry);
    expect(out).toContain('bes_tacs {');
    expect(out).toContain('intensity: 0.8mA');
  });

  // @verifies REQ-NPPS-11
  it('serializes tdcs with electrode_pairs', () => {
    const [entry] = parseNPPS(ALL_T1_NPPS);
    const out = serializeProtocol(entry);
    expect(out).toContain('tdcs {');
    expect(out).toContain('electrode_pairs:');
    expect(out).toContain('"Fp1"');
    expect(out).toContain('"P3"');
    expect(out).toContain('electrode_area_cm2: 35');
    expect(out).toContain('ramp: 30s');
  });

  // @verifies REQ-NPPS-10
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

  // @verifies REQ-NPPS-16
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

  // @verifies REQ-NPPS-11
  it('Gamma Focus is stable after two serialize/parse cycles', () => {
    const once = roundTrip(GAMMA_FOCUS_NPPS);
    const twice = roundTrip(once);
    expect(once).toBe(twice);
  });

  // @verifies REQ-NPPS-11
  it('All T1 modalities protocol is stable after two serialize/parse cycles', () => {
    const once = roundTrip(ALL_T1_NPPS);
    const twice = roundTrip(once);
    expect(once).toBe(twice);
  });

  // @verifies REQ-NPPS-11
  it('Composite protocol is stable after two serialize/parse cycles', () => {
    const once = roundTrip(COMPOSITE_NPPS);
    const twice = roundTrip(once);
    expect(once).toBe(twice);
  });

  // @verifies REQ-NPPS-11
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

  // @verifies REQ-NPPS-11
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

  // @verifies REQ-NPPS-11
  it('duration in minutes survives round-trip as minutes', () => {
    const [entry] = parseNPPS(GAMMA_FOCUS_NPPS);
    const serialized = serializeProtocol(entry);
    expect(serialized).toContain('duration: 20m');
    const [reparsed] = parseNPPS(serialized);
    const proto = (reparsed as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    expect(proto.timingMode).toMatchObject({ type: 'duration', seconds: 1200 });
  });

  // @verifies REQ-NPPS-11
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

describe('Mode F field name', () => {
  const modeFSource = (key: string) => `
protocol "Retinal" {
    hardware_version: 1
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

  // @verifies REQ-NPPS-16
  it('parses the canonical enable_mode_f key', () => {
    const v = visualOf(modeFSource('enable_mode_f'));
    expect((v.modalityParams.params as { enableModeF: boolean }).enableModeF).toBe(true);
  });

  // @verifies REQ-NPPS-16
  it('does not confuse the mode_f VALUE with a field key', () => {
    const v = visualOf(modeFSource('enable_mode_f'));
    expect((v.modalityParams.params as { mode: string }).mode).toBe('mode_f');
  });

  // @verifies REQ-NPPS-11
  it('serializes the canonical key', () => {
    const [entry] = parseNPPS(modeFSource('enable_mode_f'));
    const serialized = serializeProtocol(entry);
    expect(serialized).toContain('enable_mode_f: true');
    expect(serialized).not.toMatch(/^\s*mode_f:/m);
  });
});

// These nine field names once differed between the web and iOS parsers, so a
// file written on one platform silently lost them on the other. One canonical
// spelling now, matching NP-NPPS-REF-001 §12; the retired spellings are gone
// rather than kept as aliases, and fall under the ordinary unknown-key rule.
describe('canonical field names shared by every runtime', () => {
  const modalityOf = (src: string, type: string) => {
    const [entry] = parseNPPS(src);
    return (entry as { kind: 'single'; protocol: NPProtocolDefinition })
      .protocol.modalities.find(m => m.modalityParams.type === type)!;
  };

  const proto = (body: string) => `protocol "T" {\n  hardware_version: 1\n    duration: 5m\n${body}\n}\n`;

  // OI-CHARGE-04. The area is spelled with its unit in the key
  // (electrode_area_cm2: 35), not as a unit-suffixed literal (35cm2): the
  // lexer's suffixes are Hz/%/mA/s/m, and a digit-leading token that is not a
  // number with a known suffix is a parse error by design (NP-NPPS-REF-001 §2).
  // @verifies REQ-NPPS-05
  it('tdcs electrode_area_cm2 parses, and a cm2 suffix does not', () => {
    const m = modalityOf(proto(`    tdcs {\n        electrode_area_cm2: 25\n    }`), 'tdcs');
    expect((m.modalityParams.params as { electrodeAreaCm2: number }).electrodeAreaCm2).toBe(25);
    expect(() => parseNPPS(proto(`    tdcs {\n        electrode_area_cm2: 35cm2\n    }`)))
      .toThrow(NPPSParseError);
  });

  // @verifies REQ-NPPS-05
  it('tdcs refuses a block that authors no electrode area, rather than defaulting one', () => {
    // The area is compiled into the signed descriptor and the safety MCU's charge limit divides by it
    // (NP-NPPS-REF-001 §4.5, REQ-NPPS-05). A parser that supplied 35 would hand the device a ceiling
    // nobody authored. The editor's new block still starts at 35: that is an authored value.
    expect(() => parseNPPS(proto(`    tdcs {\n        intensity: 1mA\n    }`)))
      .toThrow(/electrode_area_cm2 is required and is never defaulted/);
  });

  // @verifies REQ-NPPS-16
  it('qeeg_21ch sloreta_enabled', () => {
    const m = modalityOf(proto(`    qeeg_21ch {\n        sloreta_enabled: true\n    }`), 'qeeg_21ch');
    expect((m.modalityParams.params as { sloretaEnabled: boolean }).sloretaEnabled).toBe(true);
  });

  // @verifies REQ-NPPS-16
  it('tms intensity_percent_mt', () => {
    const m = modalityOf(proto(`    tms {\n        tms_protocol: rTMS\n        intensity_percent_mt: 90\n    }`), 'tms');
    expect((m.modalityParams.params as { intensityPercentMT: number }).intensityPercentMT).toBe(90);
  });

  // @verifies REQ-NPPS-16
  it('vibrotactile sync_to_audio / sync_to_visual', () => {
    const m = modalityOf(
      proto(`    vibrotactile_40hz {\n        intensity_g: 0.8\n        sync_to_audio: true\n        sync_to_visual: true\n    }`),
      'vibrotactile_40hz',
    );
    const p = m.modalityParams.params as { syncToAudio: boolean; syncToVisual: boolean };
    expect(p.syncToAudio).toBe(true);
    expect(p.syncToVisual).toBe(true);
  });

  // @verifies REQ-NPPS-16
  it('audio limits max_volume_db / max_binaural_beats / max_isochronic_tones', () => {
    const lim = parseNPPSLimits(
      `limits "L" {\n    level: global\n    audio_entrainment {\n        max_volume_db: 85\n        max_binaural_beats: 100\n        max_isochronic_tones: 90\n    }\n}\n`,
    );
    expect(lim!.audioEntrainment).toMatchObject({
      maxVolumeDb: 85,
      maxBinauralBeatsHz: 100,
      maxIsochronicTonesHz: 90,
    });
  });

  // @verifies REQ-NPPS-16
  it('tms limits max_intensity_pct_mt', () => {
    const lim = parseNPPSLimits(`limits "L" {\n    level: global\n    tms {\n        max_intensity_pct_mt: 120\n    }\n}\n`);
    expect(lim!.tms).toMatchObject({ maxIntensityPercentMT: 120 });
  });

  // @verifies REQ-NPPS-16
  it('vibrotactile limits max_intensity', () => {
    const lim = parseNPPSLimits(
      `limits "L" {\n    level: global\n    vibrotactile_40hz {\n        max_intensity: 1.2\n    }\n}\n`,
    );
    expect(lim!.vibrotactile40hz).toMatchObject({ maxIntensityG: 1.2 });
  });

  // @verifies REQ-NPPS-16
  it('hd_tdcs montage uses the spelling all three runtimes and §4.13 share', () => {
    const m = modalityOf(proto(`    hd_tdcs {\n        montage: standard_2_electrode\n    }`), 'hd_tdcs');
    expect((m.modalityParams.params as { montage: string }).montage).toBe('standard_2_electrode');
    const out = serializeProtocol(parseNPPS(proto(`    hd_tdcs {\n        montage: ring_4x1\n    }`))[0]);
    expect(out).toContain('montage: ring_4x1');
    expect(out).not.toContain('4x1_ring');
  });
});

// Rev 6: every value is now a plain identifier, a number, a boolean or a quoted
// string — each of which maps onto a JSON scalar. The two lexer rules that
// admitted awkward bare values are gone, which is also what finally gives
// `montage: 10-20` a spelling that parses.
describe('quoted awkward values (JSON-leaning cleanup)', () => {
  const proto = (body: string) => `protocol "T" {\n  hardware_version: 1\n    duration: 5m\n${body}\n}\n`;

  const modalityOf = (src: string, type: string) => {
    const [entry] = parseNPPS(src);
    return (entry as { kind: 'single'; protocol: NPProtocolDefinition })
      .protocol.modalities.find(m => m.modalityParams.type === type)!;
  };

  // §4.9 documented `10-20`, but no implementation ever mapped that value —
  // both parsers use `standard_1020`. The doc was corrected rather than the
  // lexer extended, so the unquoted form stays a parse error (asserted below).
  // @verifies REQ-NPPS-16
  it('parses the qEEG montage value both parsers actually use', () => {
    const m = modalityOf(proto(`    qeeg_21ch {\n        montage: standard_1020\n    }`), 'qeeg_21ch');
    expect((m.modalityParams.params as { montage: string }).montage).toBe('standard_1020');
  });

  // @verifies REQ-NPPS-03#a
  it('parses one quoted wavelength per block', () => {
    for (const wl of ['660nm', '808nm', '1064nm', '810nm']) {
      const m = modalityOf(proto(`    pbm_transcranial {\n        wavelength: "${wl}"\n        irradiance: 300mW_cm2\n    }`), 'pbm_transcranial');
      expect((m.modalityParams.params as { wavelength: string }).wavelength).toBe(wl);
    }
  });

  // @verifies REQ-NPPS-16
  it('parses quoted hyphenated tags', () => {
    const [entry] = parseNPPS(`protocol "T" {\n  hardware_version: 1\n    tags: ["wind-down", "all-modalities", plain]\n    duration: 5m\n}\n`);
    const p = (entry as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    expect(p.tags).toEqual(['wind-down', 'all-modalities', 'plain']);
  });

  // @verifies REQ-NPPS-10#a
  it.each([
    ['montage: 10-20', '    qeeg_21ch {\n        montage: 10-20\n    }'],
    ['wavelength: 660nm', '    pbm_transcranial {\n        wavelength: 660nm\n        irradiance: 300mW_cm2\n    }'],
    ['wavelength: 1064nm', '    pbm_transcranial {\n        wavelength: 1064nm\n        irradiance: 300mW_cm2\n    }'],
  ])('rejects the unquoted form %s with a message naming the fix', (_label, body) => {
    expect(() => parseNPPS(proto(body))).toThrow(NPPSParseError);
    expect(() => parseNPPS(proto(body))).toThrow(/must be quoted/);
  });

  // @verifies REQ-NPPS-10#a
  it('rejects an unquoted hyphenated tag', () => {
    expect(() => parseNPPS(`protocol "T" {\n  hardware_version: 1\n    tags: [wind-down]\n    duration: 5m\n}\n`)).toThrow(NPPSParseError);
  });

  // @verifies REQ-NPPS-16
  it('still lexes numbers with unit suffixes, which are unaffected', () => {
    const [entry] = parseNPPS(
      proto(`    bes_tacs {\n        frequency: 40Hz\n        intensity: 0.8mA\n        interval_on: 30s\n    }`),
    );
    const p = (entry as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    expect(p.timingMode).toMatchObject({ type: 'duration', seconds: 300 });
    const m = p.modalities.find(x => x.modalityParams.type === 'bes_tacs')!;
    expect((m.modalityParams.params as { frequencyHz: number }).frequencyHz).toBe(40);
    expect(m.interval.intervalOnSeconds).toBe(30);
  });

  // @verifies REQ-NPPS-11
  it('serializes quoted forms so they round-trip', () => {
    const src = proto(`    pbm_transcranial {\n        zones: ["Frontal Left"]\n        wavelength: "808nm"\n irradiance: 300mW_cm2 }`);
    const [entry] = parseNPPS(src);
    const out = serializeProtocol(entry);
    expect(out).toContain('wavelength: "808nm"');
    const m = modalityOf(out, 'pbm_transcranial');
    expect((m.modalityParams.params as { wavelength: string }).wavelength).toBe('808nm');
  });
});

// The zones discriminant leaked into surface syntax: a pbm_transcranial block
// with no `zones` field inherited the default's 'named' tag without its paired
// zoneRefs, and serialized to the bare word `named` — which no parser accepts.
// Swept across every modality so the class stays closed, not just this case.
describe('serialize → parse round-trip holds for every modality', () => {
  const ids = Object.keys(MODALITY_META);

  // @verifies REQ-NPPS-16
  it('covers all modality types', () => {
    expect(ids.length).toBeGreaterThanOrEqual(15);
  });

  // @verifies REQ-NPPS-11
  it.each(ids)('%s survives serialize → parse with all fields defaulted', (id) => {
    // The dose-bearing fields have no default (a defaulted irradiance or sound
    // level, or tDCS electrode area, would deliver a stimulus nobody authored), so those blocks state them.
    const SEED: Record<string, string> = {
      pbm_transcranial: 'wavelength: "808nm"\n        irradiance: 300mW_cm2',
      pbm_intranasal: 'wavelength: "660nm"\n        irradiance: 60mW_cm2',
      audio_entrainment: 'volume: 72dB',
      tdcs: 'electrode_area_cm2: 35',
    };
    const [entry] = parseNPPS(`protocol "T" {\n  hardware_version: 1\n    duration: 5m\n    ${id} {\n        ${SEED[id] ?? ''}\n    }\n}\n`);
    const out = serializeProtocol(entry);
    expect(() => parseNPPS(out)).not.toThrow();
    // and is stable on a second cycle
    expect(serializeProtocol(parseNPPS(out)[0])).toBe(out);
  });

  // @verifies REQ-NPPS-16
  it('a defaulted pbm_transcranial names its zones instead of emitting the tag', () => {
    const [entry] = parseNPPS(`protocol "T" {\n  hardware_version: 1\n    duration: 5m\n    pbm_transcranial {\n        irradiance: 322mW_cm2\n wavelength: "808nm" }\n}\n`);
    const out = serializeProtocol(entry);
    expect(out).toContain('zones: ["All"]');
    expect(out).not.toMatch(/^\s*zones:\s*named\s*$/m);
  });

  // @verifies REQ-NPPS-11
  it('clinician_selected still serializes as the bare keyword it is', () => {
    const [entry] = parseNPPS(
      `protocol "T" {\n  hardware_version: 1\n    duration: 5m\n    pbm_transcranial {\n        zones: clinician_selected\n irradiance: 300mW_cm2 wavelength: "808nm" }\n}\n`,
    );
    const out = serializeProtocol(entry);
    expect(out).toContain('zones: clinician_selected');
    expect(() => parseNPPS(out)).not.toThrow();
  });

  // @verifies REQ-NPPS-16
  it('an empty named ref list is reported as a caller bug, not written out', () => {
    const [entry] = parseNPPS(`protocol "T" {\n  hardware_version: 1\n    duration: 5m\n    pbm_transcranial {\n        irradiance: 322mW_cm2\n wavelength: "808nm" }\n}\n`);
    const proto = (entry as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    const params = proto.modalities[0].modalityParams.params as { zoneRefs?: string[] };
    delete params.zoneRefs;
    expect(() => serializeProtocol(entry)).toThrow(/must list at least one zone/);
  });
});


// ─── Absolute quantities (NP-NPPS-REF-001 §4.1b, §4.7) ───────────────────────

describe('absolute intensity: irradiance in mW/cm², volume in dB', () => {
  const proto = (body: string) => `protocol "T" {\n  hardware_version: 1\n    duration: 5m\n${body}\n}\n`;
  const modalityOf = (src: string, type: string) => {
    const [entry] = parseNPPS(src);
    return (entry as { kind: 'single'; protocol: NPProtocolDefinition }).protocol
      .modalities.find(m => m.modalityParams.type === type)!;
  };
  const pbm = (body: string) => proto(`    pbm_transcranial {\n        ${body}\n    }`);
  const params = (src: string) =>
    modalityOf(src, 'pbm_transcranial').modalityParams.params as {
      irradianceMWcm2: number; wavelength: string;
    };

  // @verifies REQ-NPPS-01#a
  it('reads irradiance with its unit', () => {
    expect(params(pbm('wavelength: "808nm"\n        irradiance: 36mW_cm2')).irradianceMWcm2).toBe(36);
  });

  // @verifies REQ-NPPS-01#a
  it('accepts the canonical name with the unit in the key and a bare number', () => {
    expect(params(pbm('wavelength: "808nm"\n        irradiance_mw_cm2: 36')).irradianceMWcm2).toBe(36);
  });

  // @verifies REQ-NPPS-01#a,b
  it.each([
    ['intensity: 80%', /percentage of a baseline/],
    ['intensity_percent: 80', /percentage of a baseline/],
    ['wavelength: "808nm"\n        irradiance: 80%', /in % is refused/],
    ['wavelength: "808nm"\n        irradiance: 80', /needs its unit written/],
    ['wavelength: "808nm"\n        irradiance: 80mA', /takes mW_cm2, not mA/],
  ])('refuses %s', (body, message) => {
    expect(() => parseNPPS(pbm(body))).toThrow(message);
  });

  // @verifies REQ-NPPS-01#c
  it('requires an irradiance and a wavelength rather than defaulting them', () => {
    expect(() => parseNPPS(pbm('wavelength: "808nm"'))).toThrow(/irradiance .* is required/);
    expect(() => parseNPPS(pbm('irradiance: 300mW_cm2'))).toThrow(/wavelength is required/);
  });

  // @verifies REQ-NPPS-03
  it('refuses the retired combined channel names and says what replaces them', () => {
    for (const wl of ['660_808nm', '660_808_1064nm']) {
      expect(() => parseNPPS(pbm(`wavelength: "${wl}"\n        irradiance: 300mW_cm2`)))
        .toThrow(/retired.*Write one block per wavelength/);
    }
  });

  // @verifies REQ-NPPS-01#b,c
  it('puts a wavelength and an irradiance on every intranasal block too', () => {
    const src = proto(`    pbm_intranasal {\n        wavelength: "660nm"\n        irradiance: 25mW_cm2\n    }`);
    const p = modalityOf(src, 'pbm_intranasal').modalityParams.params as { irradianceMWcm2: number; wavelength: string };
    expect(p).toMatchObject({ irradianceMWcm2: 25, wavelength: '660nm' });
    expect(() => parseNPPS(proto(`    pbm_intranasal {\n        intensity: 60%\n    }`))).toThrow(/percentage of a baseline/);
  });

  // @verifies REQ-NPPS-01#a,b
  it('reads audio volume in dB and refuses a percentage', () => {
    const audio = (body: string) => proto(`    audio_entrainment {\n        carrier_hz: 440Hz\n        ${body}\n    }`);
    const m = modalityOf(audio('volume: 72.5dB'), 'audio_entrainment');
    expect((m.modalityParams.params as { volumeDb: number }).volumeDb).toBe(72.5);
    expect(() => parseNPPS(audio('volume: 70%'))).toThrow(/in % is refused/);
    expect(() => parseNPPS(audio('volume: 70'))).toThrow(/needs its unit written/);
    expect(() => parseNPPS(audio('volume_percent: 70'))).toThrow(/percentage of a baseline/);
    expect(() => parseNPPS(audio('binaural_hz: 10Hz'))).toThrow(/volume in dB SPL .* is required/);
  });

  // @verifies REQ-NPPS-01#b
  it('refuses a percentage ceiling in limits rather than skipping it', () => {
    for (const mod of ['pbm_transcranial', 'pbm_intranasal', 'audio_entrainment']) {
      expect(() => parseNPPSLimits(`limits "L" {\n    level: global\n    ${mod} {\n        max_intensity: 80\n    }\n}\n`))
        .toThrow(/max_intensity is a percentage ceiling and is retired/);
    }
    const lim = parseNPPSLimits(
      `limits "L" {\n    level: global\n    pbm_transcranial {\n        max_irradiance_mw_cm2: 250\n    }\n}\n`);
    expect(lim!.pbmTranscranial).toMatchObject({ maxIrradianceMWcm2: 250 });
  });

  // @verifies REQ-NPPS-11
  it('round-trips the unit-bearing spelling', () => {
    const [entry] = parseNPPS(pbm('wavelength: "660nm"\n        irradiance: 12.5mW_cm2'));
    expect(serializeProtocol(entry)).toContain('irradiance: 12.5mW_cm2');
  });
});

describe('CW with a duty cycle (OI-SESPWR-03)', () => {
  const pbm = (body: string, kind = 'pbm_transcranial') => `
protocol "CW" {
    hardware_version: 1
    duration: 10m
    ${kind} {
        ${kind === 'pbm_intranasal' ? 'wavelength: "660nm"\n        irradiance: 60mW_cm2' : 'wavelength: "808nm"\n        irradiance: 300mW_cm2'}
        ${body}
    }
}
`;
  const paramsOf = (src: string, kind = 'pbm_transcranial') => {
    const [entry] = parseNPPS(src);
    const m = (entry as { kind: 'single'; protocol: NPProtocolDefinition })
      .protocol.modalities.find(x => x.modalityParams.type === kind)!;
    return m.modalityParams.params as { frequencyHz: number; dutyCyclePercent: number };
  };

  // @verifies REQ-NPPS-02
  it('refuses frequency 0 with a duty other than 100 %', () => {
    expect(() => parseNPPS(pbm('frequency: 0Hz\n duty_cycle: 25%'))).toThrow(/continuous wave.*no duty/s);
    expect(() => parseNPPS(pbm('frequency: 0\n duty_cycle: 50%', 'pbm_intranasal'))).toThrow(/no duty/);
  });

  // @verifies REQ-NPPS-02
  it('reads CW as 100 % duty whether the duty is omitted or written', () => {
    expect(paramsOf(pbm('frequency: 0Hz')).dutyCyclePercent).toBe(100);
    expect(paramsOf(pbm('frequency: 0Hz\n duty_cycle: 100%')).dutyCyclePercent).toBe(100);
  });

  // @verifies REQ-NPPS-16
  it('leaves a pulsed block alone', () => {
    const p = paramsOf(pbm('frequency: 40Hz\n duty_cycle: 25%'));
    expect(p).toMatchObject({ frequencyHz: 40, dutyCyclePercent: 25 });
  });
});
