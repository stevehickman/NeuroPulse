import { describe, it, expect } from 'vitest';
import {
  parseNPPS,
  parseNPPSFile,
  buildNamespace,
  validateNamespaceReferences,
} from './nppsParser';
import { serializeZone, serializeCondition, serializeProtocol } from './nppsSerializer';
import { NP_SOCKET_ID_MAX, NP_SOCKET_ID_MIN } from './socketSet';
import type {
  NPProtocolDefinition,
  PBMTranscranialParams,
} from '../types/protocol';

// ─── Zone definitions ──────────────────────────────────────────────────────────

describe('zone blocks', () => {
  it('parses a socket-list zone with a stable id (predefined)', () => {
    const { zones } = parseNPPSFile('zone "Frontal Left" { id: "z1" sockets: [1, 2, 4, 5] }');
    expect(zones).toHaveLength(1);
    expect(zones[0]).toMatchObject({ name: 'Frontal Left', sockets: [1, 2, 4, 5], id: 'z1' });
    expect(zones[0].isPredefined).toBe(true);
  });

  it('parses an arbitrary, non-contiguous socket-list zone', () => {
    const { zones } = parseNPPSFile('zone "Scattered" { sockets: [3, 17, 25] }');
    expect(zones[0].sockets).toEqual([3, 17, 25]);
    expect(zones[0].isPredefined).toBe(false);
  });

  // ── Socket-id range enforcement (numbering base is 1, count is derived) ──────

  it('rejects a socket id past the end of the lattice', () => {
    expect(() => parseNPPSFile(`zone "Too far" { sockets: [1, ${NP_SOCKET_ID_MAX + 1}] }`))
      .toThrow(/is not a socket on this helmet/);
  });

  it('rejects socket id 0 — ids are 1-based, not 0-based', () => {
    // The numbering base was documented and then not enforced, so `[0, 1, 2]`
    // parsed happily and addressed a socket that does not exist.
    expect(() => parseNPPSFile('zone "Zero based" { sockets: [0, 1, 2] }'))
      .toThrow(/is not a socket on this helmet/);
  });

  it('rejects negative and fractional socket ids', () => {
    expect(() => parseNPPSFile('zone "Negative" { sockets: [-1] }'))
      .toThrow(/is not a socket on this helmet/);
    expect(() => parseNPPSFile('zone "Fractional" { sockets: [1.5] }'))
      .toThrow(/is not a socket on this helmet/);
  });

  it('rejects values that only LOOK numeric to Number()', () => {
    // Bare Number() maps true->1, [5]->5, '0x10'->16, '1e1'->10, and the lexer
    // admits booleans, identifiers and nested arrays into a generic array — so
    // without strict coercion every one of these reached the socket list.
    for (const src of ['[true, 4]', '[[5], 4]']) {
      expect(() => parseNPPSFile(`zone "Sneaky" { sockets: ${src} }`), src)
        .toThrow(/is not a socket on this helmet/);
    }
    // '0x10' and '1e1' are digit-leading tokens that are not plain numbers, so
    // since Rev 6 the lexer rejects them outright rather than letting them
    // reach socket validation. Still rejected — earlier, and by name.
    for (const src of ['[0x10]', '[1e1]']) {
      expect(() => parseNPPSFile(`zone "Sneaky" { sockets: ${src} }`), src)
        .toThrow(/must be quoted/);
    }
  });

  it('accepts a quoted decimal, which is unambiguous', () => {
    // Deliberate: a plain decimal in quotes names exactly one socket, and the
    // same coercion serves the config UI's text box. Only values that need
    // INTERPRETING (hex, exponent, boolean, array) are rejected.
    expect(parseNPPSFile('zone "Quoted" { sockets: ["4"] }').zones[0].sockets).toEqual([4]);
  });

  it('quotes the offending token rather than reporting NaN', () => {
    // "socket NaN is not on this helmet" named nothing the author could find.
    expect(() => parseNPPSFile('zone "Bad" { sockets: [banana] }'))
      .toThrow(/banana/);
  });

  it('names the offending id and the valid range in the error', () => {
    const bad = NP_SOCKET_ID_MAX + 5;
    expect(() => parseNPPSFile(`zone "Bad" { sockets: [${bad}] }`))
      .toThrow(new RegExp(`${bad}.*${NP_SOCKET_ID_MIN}.*${NP_SOCKET_ID_MAX}`));
  });

  it('accepts both ends of the valid range', () => {
    const src = `zone "Ends" { sockets: [${NP_SOCKET_ID_MIN}, ${NP_SOCKET_ID_MAX}] }`;
    expect(parseNPPSFile(src).zones[0].sockets).toEqual([NP_SOCKET_ID_MIN, NP_SOCKET_ID_MAX]);
  });

  it('deduplicates and sorts a zone socket list', () => {
    // A zone is a SET. A repeated socket would otherwise inflate coverage
    // denominators and get dosed twice by any per-socket iteration.
    const { zones } = parseNPPSFile('zone "Dupes" { sockets: [5, 3, 5, 1, 3] }');
    expect(zones[0].sockets).toEqual([1, 3, 5]);
  });

  it('parses a socket-list zone with an element-type filter', () => {
    const { zones } = parseNPPSFile(
      'zone "Frontal LEDs" { sockets: [3, 4] types: [led_660, led_808] }'
    );
    expect(zones[0].sockets).toEqual([3, 4]);
    expect(zones[0].types).toEqual(['led_660', 'led_808']);
  });

  it('refuses to serialize a zone the parser would reject', () => {
    // The write path must enforce the read path's contract, or the app emits a
    // .npps file it cannot load back.
    expect(() => serializeZone({ name: 'Bad', sockets: [0, 4, 999] }))
      .toThrow(/cannot serialize zone "Bad"/);
  });

  it('canonicalises sockets on serialize, so output always re-parses', () => {
    const out = serializeZone({ name: 'Messy', sockets: [5, 3, 5, 1] });
    expect(out).toContain('sockets: [1, 3, 5]');
    expect(parseNPPSFile(out).zones[0].sockets).toEqual([1, 3, 5]);
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
        wavelength: \"660_808nm\"
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

  it('rejects the retired keyword zone selectors outright', () => {
    // Not "parses but refuses to resolve" — there are no existing users, so the
    // five-slot forms are gone from the grammar entirely.
    for (const selector of ['all', 'front', 'rear', 'custom']) {
      expect(() => parseNPPS(
        `protocol "L" { id: "x" pbm_transcranial { zones: ${selector} wavelength: \"660_808nm\" } }`
      )).toThrow(/unknown zone selector/);
    }
  });

  it('rejects a numeric zone list', () => {
    expect(() => parseNPPS(
      'protocol "L" { id: "x" pbm_transcranial { zones: [1, 2] wavelength: \"660_808nm\" } }'
    )).toThrow(/quoted zone names/);
  });
});

// ─── Single namespace across files ──────────────────────────────────────────────

describe('single namespace and reference validation', () => {
  it('resolves zones and conditions defined in a separate file', () => {
    const defsFile = parseNPPSFile(`
      zone "Left Frontal" { id: "zf-l" sockets: [1, 2, 4, 5] }
      zone "Right Frontal" { id: "zf-r" sockets: [1, 3, 5, 6] }
      condition "Major Depressive Disorder" { link: "https://x/mdd" }
    `);
    const protoFile = parseNPPSFile(PROTO_WITH_REFS);
    const { namespace, errors } = buildNamespace([defsFile, protoFile]);

    expect(errors).toEqual([]);
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

  // A duplicate name is an ERROR and binds to NEITHER definition. Directory
  // traversal order is not guaranteed, so last-write-wins made the winning
  // definition a property of the file system — for a zone, a silent change of
  // which sockets get dosed. Leaving the name unbound is the only outcome that
  // is the same whatever the read order.
  it('errors on a duplicate zone name and leaves it undefined', () => {
    const a = parseNPPSFile('zone "Z" { sockets: [1, 2] }');
    const b = parseNPPSFile('zone "Z" { sockets: [3, 4] }');
    const { namespace, errors } = buildNamespace([a, b]);
    expect(errors.some(e => e.includes("Duplicate zone name 'Z'"))).toBe(true);
    expect(namespace.zones.has('Z')).toBe(false);
  });

  it('gives the same answer whichever order the colliding files are read in', () => {
    const a = parseNPPSFile('zone "Z" { sockets: [1, 2] }');
    const b = parseNPPSFile('zone "Z" { sockets: [3, 4] }');
    const forward = buildNamespace([a, b]);
    const reverse = buildNamespace([b, a]);
    expect(forward.namespace.zones.has('Z')).toBe(false);
    expect(reverse.namespace.zones.has('Z')).toBe(false);
    expect(forward.errors).toEqual(reverse.errors);
  });

  it('errors on a duplicate condition name and leaves it undefined', () => {
    const a = parseNPPSFile('condition "C" { link: "https://x/1" }');
    const b = parseNPPSFile('condition "C" { link: "https://x/2" }');
    const { namespace, errors } = buildNamespace([a, b]);
    expect(errors.some(e => e.includes("Duplicate condition name 'C'"))).toBe(true);
    expect(namespace.conditions.has('C')).toBe(false);
  });

  it('a protocol referencing a collided zone fails reference resolution', () => {
    const a = parseNPPSFile('zone "Left Frontal" { sockets: [1, 2] }');
    const b = parseNPPSFile('zone "Left Frontal" { sockets: [3, 4] }');
    const defs = parseNPPSFile('condition "Major Depressive Disorder" { link: "https://x/mdd" }');
    const protoFile = parseNPPSFile(PROTO_WITH_REFS);
    const { namespace } = buildNamespace([a, b, defs, protoFile]);
    const refErrors = validateNamespaceReferences(namespace);
    expect(refErrors.some(e => e.includes("undefined zone 'Left Frontal'"))).toBe(true);
  });

  it('a third definition cannot re-bind a name already known to collide', () => {
    const a = parseNPPSFile('zone "Z" { sockets: [1] }');
    const b = parseNPPSFile('zone "Z" { sockets: [2] }');
    const c = parseNPPSFile('zone "Z" { sockets: [3] }');
    const { namespace, errors } = buildNamespace([a, b, c]);
    expect(namespace.zones.has('Z')).toBe(false);
    expect(errors.length).toBe(1);
  });
});
