import { describe, it, expect } from 'vitest';
import {
  parseNPPS,
  parseNPPSFile,
  buildNamespace,
  validateNamespaceReferences,
} from './nppsParser';
import { serializeZone, serializeCondition, serializeProtocol } from './nppsSerializer';
import type {
  NPProtocolDefinition,
  PBMTranscranialParams,
} from '../types/protocol';

// ─── Zone definitions ──────────────────────────────────────────────────────────

describe('zone blocks', () => {
  it('parses a lobe/side zone', () => {
    const { zones } = parseNPPSFile('zone "Left Frontal" { id: "z1" lobe: frontal side: left }');
    expect(zones).toHaveLength(1);
    expect(zones[0]).toMatchObject({ name: 'Left Frontal', lobe: 'frontal', side: 'left', id: 'z1' });
    expect(zones[0].isPredefined).toBe(true);
  });

  it('parses a socket-set zone', () => {
    const { zones } = parseNPPSFile('zone "Temples" { sockets: [12, 13, 14] }');
    expect(zones[0].sockets).toEqual([12, 13, 14]);
    expect(zones[0].isPredefined).toBe(false);
  });

  it('parses an address-set zone with a type filter', () => {
    const { zones } = parseNPPSFile(
      'zone "Frontal LEDs" { addrs: [[3, 0], [3, 1], [4, 0]] types: [led_660, led_808] }'
    );
    expect(zones[0].addrs).toEqual([[3, 0], [3, 1], [4, 0]]);
    expect(zones[0].types).toEqual(['led_660', 'led_808']);
  });

  it('round-trips a zone through the serializer', () => {
    const src = 'zone "Custom" { description: "test" sockets: [1, 2] types: [eeg_electrode] exclude_types: true }';
    const { zones } = parseNPPSFile(src);
    const out = serializeZone(zones[0]);
    const { zones: zones2 } = parseNPPSFile(out);
    expect(zones2[0]).toMatchObject(zones[0]);
  });
});

// ─── Condition definitions ──────────────────────────────────────────────────────

describe('condition blocks', () => {
  it('parses a condition name → link pair', () => {
    const { conditions } = parseNPPSFile(
      'condition "Major Depressive Disorder" { link: "https://en.wikipedia.org/wiki/Major_depressive_disorder" code: "6A70" }'
    );
    expect(conditions[0]).toMatchObject({
      name: 'Major Depressive Disorder',
      link: 'https://en.wikipedia.org/wiki/Major_depressive_disorder',
      code: '6A70',
    });
  });

  it('round-trips a condition through the serializer', () => {
    const { conditions } = parseNPPSFile('condition "Anxiety" { link: "https://x/anxiety" }');
    const out = serializeCondition(conditions[0]);
    const { conditions: c2 } = parseNPPSFile(out);
    expect(c2[0]).toMatchObject(conditions[0]);
  });
});

// ─── Protocol conditions + references + zone refs ───────────────────────────────

const PROTO_WITH_REFS = `
protocol "Depression DLPFC" {
    id: "20000001-0000-0000-0000-000000000000"
    description: "Bilateral DLPFC PBM"
    tags: [depression]
    conditions: ["Major Depressive Disorder"]
    references: [
        "https://doi.org/10.1000/plain",
        ["Cassano 2018 RCT", "https://doi.org/10.1000/cassano"]
    ]
    duration: 20m

    pbm_transcranial {
        intensity: 80%
        frequency: 40Hz
        duty_cycle: 25%
        zones: ["Left Frontal", "Right Frontal"]
        wavelength: 660_808nm
    }
}
`;

describe('protocol conditions, references, and zone refs', () => {
  const proto = (parseNPPS(PROTO_WITH_REFS)[0] as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;

  it('parses the conditions list', () => {
    expect(proto.conditions).toEqual(['Major Depressive Disorder']);
  });

  it('parses bare and labelled references', () => {
    expect(proto.references).toEqual([
      'https://doi.org/10.1000/plain',
      { label: 'Cassano 2018 RCT', url: 'https://doi.org/10.1000/cassano' },
    ]);
  });

  it('parses zones as named references', () => {
    const pbm = proto.modalities[0].modalityParams.params as PBMTranscranialParams;
    expect(pbm.zones).toBe('named');
    expect(pbm.zoneRefs).toEqual(['Left Frontal', 'Right Frontal']);
  });

  it('round-trips conditions, references, and zone refs', () => {
    const out = serializeProtocol({ kind: 'single', protocol: proto });
    const proto2 = (parseNPPS(out)[0] as { kind: 'single'; protocol: NPProtocolDefinition }).protocol;
    expect(proto2.conditions).toEqual(proto.conditions);
    expect(proto2.references).toEqual(proto.references);
    const pbm2 = proto2.modalities[0].modalityParams.params as PBMTranscranialParams;
    expect(pbm2.zoneRefs).toEqual(['Left Frontal', 'Right Frontal']);
  });

  it('preserves legacy keyword zones', () => {
    const legacy = parseNPPS(
      'protocol "L" { id: "x" pbm_transcranial { zones: all wavelength: 660_808nm } }'
    )[0] as { kind: 'single'; protocol: NPProtocolDefinition };
    const pbm = legacy.protocol.modalities[0].modalityParams.params as PBMTranscranialParams;
    expect(pbm.zones).toBe('all');
    expect(pbm.zoneRefs).toBeUndefined();
  });
});

// ─── Single namespace across files ──────────────────────────────────────────────

describe('single namespace and reference validation', () => {
  it('resolves zones and conditions defined in a separate file', () => {
    const defsFile = parseNPPSFile(`
      zone "Left Frontal" { id: "zf-l" lobe: frontal side: left }
      zone "Right Frontal" { id: "zf-r" lobe: frontal side: right }
      condition "Major Depressive Disorder" { link: "https://x/mdd" }
    `);
    const protoFile = parseNPPSFile(PROTO_WITH_REFS);
    const { namespace, warnings } = buildNamespace([defsFile, protoFile]);

    expect(warnings).toEqual([]);
    expect(namespace.zones.has('Left Frontal')).toBe(true);
    expect(namespace.conditions.has('Major Depressive Disorder')).toBe(true);
    expect(validateNamespaceReferences(namespace)).toEqual([]);
  });

  it('reports an unresolved condition reference', () => {
    const protoFile = parseNPPSFile(PROTO_WITH_REFS);
    const { namespace } = buildNamespace([protoFile]);
    const errors = validateNamespaceReferences(namespace);
    // no zones and no conditions defined → 1 condition + 2 zone errors
    expect(errors.some(e => e.includes("undefined condition 'Major Depressive Disorder'"))).toBe(true);
    expect(errors.some(e => e.includes("undefined zone 'Left Frontal'"))).toBe(true);
  });

  it('warns on duplicate zone names (last wins)', () => {
    const a = parseNPPSFile('zone "Z" { lobe: frontal side: left }');
    const b = parseNPPSFile('zone "Z" { lobe: frontal side: right }');
    const { namespace, warnings } = buildNamespace([a, b]);
    expect(warnings.some(w => w.includes("Duplicate zone name 'Z'"))).toBe(true);
    expect(namespace.zones.get('Z')?.side).toBe('right');
  });
});
