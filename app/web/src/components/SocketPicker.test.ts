/// <reference types="node" />
// Zone grouping for the socket picker (ZONE-1).
//
// The picker used to colour tiles from a `lobe` field on the generated socket
// map. That field is gone — zones are defined only in 00-zones.npps — so the
// colouring now comes from zone membership. These tests pin the two properties
// that makes the picker correct and that the old model could not have: the
// colour is the MOST SPECIFIC zone containing a socket, and EVERY zone in the
// loaded namespace reaches the picker, including user-defined and research zones
// (the SMART-1 research-flexibility rationale).
//
// Only the pure grouping function is exercised. Rendering is not: vite/vitest
// cannot load their native rolldown binding on this host.

import { describe, it, expect } from 'vitest';
import { readFileSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { groupByZone } from './SocketPicker';
import { parseNPPSFile, buildNamespace } from '../lib/nppsParser';
import { NP_SOCKETS } from '../lib/socketMap.generated';
import type { NPZoneDefinition } from '../types/protocol';

const __repoRoot = join(dirname(fileURLToPath(import.meta.url)), '..', '..', '..', '..');

const shippedZones = buildNamespace([
  parseNPPSFile(readFileSync(join(__repoRoot, 'protocols', 'predefined', '00-zones.npps'), 'utf-8')),
]).namespace.zones;

function zoneMap(zones: NPZoneDefinition[]): ReadonlyMap<string, NPZoneDefinition> {
  return new Map(zones.map(z => [z.name, z]));
}

describe('socket picker zone grouping', () => {
  it('colours each socket by the smallest zone containing it', () => {
    // "All" contains every socket, so colouring by any larger zone would paint
    // the whole helmet one colour. Smallest-wins is what makes the map readable.
    const zones = zoneMap([
      { name: 'All', sockets: [1, 2, 3, 4] },
      { name: 'Half', sockets: [1, 2] },
      { name: 'One', sockets: [1] },
    ] as NPZoneDefinition[]);

    const { primaryOf } = groupByZone(zones);
    expect(primaryOf.get(1)).toBe('One');
    expect(primaryOf.get(2)).toBe('Half');
    expect(primaryOf.get(3)).toBe('All');
    expect(primaryOf.get(4)).toBe('All');
  });

  it('breaks equal-size ties by name, so the map is stable across reloads', () => {
    const forward = groupByZone(zoneMap([
      { name: 'Bravo', sockets: [1, 2] },
      { name: 'Alpha', sockets: [1, 2] },
    ] as NPZoneDefinition[]));
    // Same two zones, inserted in the other order.
    const reverse = groupByZone(zoneMap([
      { name: 'Alpha', sockets: [1, 2] },
      { name: 'Bravo', sockets: [1, 2] },
    ] as NPZoneDefinition[]));

    expect(forward.primaryOf.get(1)).toBe('Alpha');
    expect(reverse.primaryOf.get(1)).toBe('Alpha');
    expect(forward.colorOf.get('Alpha')).toBe(reverse.colorOf.get('Alpha'));
  });

  it('reports every zone a socket belongs to, smallest first', () => {
    const { membershipOf } = groupByZone(zoneMap([
      { name: 'All', sockets: [1, 2, 3] },
      { name: 'Pair', sockets: [1, 2] },
    ] as NPZoneDefinition[]));

    expect(membershipOf.get(1)).toEqual(['Pair', 'All']);
    expect(membershipOf.get(3)).toEqual(['All']);
  });

  /**
   * The point of ZONE-1 for this component. Under the old four-lobe palette a
   * research zone was invisible here however it was authored; now any zone in
   * the namespace is picked up, and a narrow one wins the tiles it covers.
   */
  it('surfaces a user-defined zone that no lobe model would have known about', () => {
    const withResearchZone = new Map(shippedZones);
    const target = [...shippedZones.get('Frontal Left')!.sockets].slice(0, 2);
    withResearchZone.set('Study 7 — DLPFC probe', {
      name: 'Study 7 — DLPFC probe',
      sockets: target,
    } as NPZoneDefinition);

    const { primaryOf, legend, colorOf } = groupByZone(withResearchZone);

    expect(legend).toContain('Study 7 — DLPFC probe');
    expect(colorOf.get('Study 7 — DLPFC probe')).toBeDefined();
    for (const id of target) {
      expect(primaryOf.get(id), `socket ${id}`).toBe('Study 7 — DLPFC probe');
    }
  });

  it('assigns every socket on the helmet a zone from the shipped zone file', () => {
    // "All" guarantees this, but it is the property the picker relies on to
    // never render an uncoloured tile against the shipped data.
    const { primaryOf } = groupByZone(shippedZones);
    const unzoned = NP_SOCKETS.filter(s => !primaryOf.has(s.id)).map(s => s.id);
    expect(unzoned).toEqual([]);
  });

  it('renders uncoloured rather than failing before the namespace loads', () => {
    const empty = groupByZone(new Map());
    expect(empty.legend).toEqual([]);
    expect(empty.primaryOf.size).toBe(0);
    expect(empty.membershipOf.size).toBe(0);
  });

  it('never lists a zone in the legend that owns no tile', () => {
    // "All" loses every socket to the narrower zones here, so it must not appear
    // in the legend with a swatch that matches nothing on the map.
    const { legend } = groupByZone(zoneMap([
      { name: 'All', sockets: [1, 2] },
      { name: 'Left', sockets: [1] },
      { name: 'Right', sockets: [2] },
    ] as NPZoneDefinition[]));

    expect(legend).toEqual(['Left', 'Right']);
  });
});
