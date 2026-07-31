/// <reference types="node" />
import { describe, it, expect } from 'vitest';
import { readFileSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import {
  NPHelmetInventory,
  NPSimulatedInventoryProvider,
  MODULE_TYPES,
  type NPSocketInventory,
} from './helmetInventory';
import {
  evaluateProtocol,
  zoneCoverageFor,
  zonesForModality,
  coverageLabel,
  coverageFraction,
  describeShortfall,
  MODALITY_REQUIREMENTS,
} from './protocolEligibility';
import { NP_SOCKETS, NP_ROW_WIDTHS, NP_TILE_GEOMETRY } from './socketMap.generated';
import {
  NP_SOCKET_COUNT,
  NP_SOCKET_ID_MAX,
  NP_SOCKET_ID_MIN,
  NP_SOCKET_NUMBERING_BASE,
  isValidSocketId,
  unionSockets,
} from './socketSet';
import { parseNPPSFile, buildNamespace } from './nppsParser';
import type { NPProtocolDefinition, NPZoneDefinition } from '../types/protocol';

const __repoRoot = join(dirname(fileURLToPath(import.meta.url)), '..', '..', '..', '..');

const zoneNamespace = buildNamespace([
  parseNPPSFile(readFileSync(join(__repoRoot, 'protocols', 'predefined', '00-zones.npps'), 'utf-8')),
]).namespace.zones;

// ─── Socket map ────────────────────────────────────────────────────────────────

describe('generated socket map', () => {
  it('uses the hexagon area its own module width implies', () => {
    const { moduleWidthMm, hexAreaCm2 } = NP_TILE_GEOMETRY;
    expect(hexAreaCm2).toBeCloseTo((Math.sqrt(3) / 2) * moduleWidthMm ** 2 / 100, 3);
    expect(moduleWidthMm).toBeGreaterThanOrEqual(34); // workable floor
    expect(moduleWidthMm).toBeLessThanOrEqual(46);    // workable ceiling
  });

  /**
   * THE load-bearing geometric assertion. In an offset hex lattice every
   * neighbour sits at exactly one tile width, so any pair closer than that is
   * two modules trying to occupy the same space. A previous revision shipped
   * 13 such pairs because row widths did not alternate parity.
   */
  it('tessellates — no two sockets overlap', () => {
    const collisions: string[] = [];
    for (let i = 0; i < NP_SOCKETS.length; i++) {
      for (let j = i + 1; j < NP_SOCKETS.length; j++) {
        const a = NP_SOCKETS[i], b = NP_SOCKETS[j];
        const d = Math.hypot(a.x - b.x, a.y - b.y);
        if (d < 0.999) collisions.push(`${a.id}&${b.id} at ${d.toFixed(3)}`);
      }
    }
    expect(collisions).toEqual([]);
  });

  /**
   * Every row is symmetric about the midline, so odd rows land on integer tile
   * positions and even rows on half-integers; offset packing needs consecutive
   * rows shifted by half a tile. Both together force strict parity alternation.
   */
  it('alternates row width parity, which is what makes the packing valid', () => {
    for (let i = 1; i < NP_ROW_WIDTHS.length; i++) {
      expect(NP_ROW_WIDTHS[i] % 2, `rows ${i - 1} and ${i} share parity`)
        .not.toBe(NP_ROW_WIDTHS[i - 1] % 2);
    }
    expect(NP_ROW_WIDTHS.reduce((a, b) => a + b, 0)).toBe(NP_SOCKET_COUNT);
  });

  /**
   * The sockets sit on the HELMET INTERIOR, which stands off the largest skull
   * the SKU covers so hair and head-size spread have somewhere to go. Tiling
   * the skull itself under-counted by nearly half.
   */
  it('tiles the helmet interior, standing off the largest skull', () => {
    const g = NP_TILE_GEOMETRY;
    expect(g.standoffMm).toBeGreaterThan(10);
    expect(g.tiledAreaCm2).toBeLessThan(g.vaultSurfaceCm2);
    expect(g.interiorLengthMm).toBeGreaterThan(g.interiorBreadthMm);
  });

  it('spaces rows at the pitch offset hex packing requires', () => {
    const { moduleWidthMm, rowPitchMm } = NP_TILE_GEOMETRY;
    expect(rowPitchMm).toBeCloseTo(0.75 * (2 * moduleWidthMm / Math.sqrt(3)), 2);
  });

  it('numbers sockets contiguously from the numbering base', () => {
    expect(NP_SOCKET_NUMBERING_BASE).toBe(1);
    expect(NP_SOCKET_ID_MIN).toBe(NP_SOCKET_NUMBERING_BASE);
    expect(NP_SOCKET_ID_MAX).toBe(NP_SOCKET_NUMBERING_BASE + NP_SOCKET_COUNT - 1);
    expect(NP_SOCKETS.map(s => s.id)).toEqual(
      Array.from({ length: NP_SOCKET_COUNT }, (_, i) => i + NP_SOCKET_NUMBERING_BASE),
    );
  });

  it('stays within the firmware major-address ceiling', () => {
    // np_module_map.h: NP_HEXMAP_MAX_SOCKETS = 128, the full 7-bit major domain
    // (1 << 7). The interior scan holds ~80; the addressing ceiling is the whole
    // field, so no arbitrary sub-ceiling to re-justify.
    expect(NP_SOCKET_ID_MAX).toBeLessThanOrEqual(128);
  });

  it('rejects out-of-range socket ids', () => {
    expect(isValidSocketId(NP_SOCKET_ID_MIN - 1)).toBe(false);
    expect(isValidSocketId(NP_SOCKET_ID_MAX + 1)).toBe(false);
    expect(isValidSocketId(NP_SOCKET_ID_MIN)).toBe(true);
    expect(isValidSocketId(NP_SOCKET_ID_MAX)).toBe(true);
  });

  /**
   * The load-bearing invariant, post-ZONE-1: every zone the file defines must be
   * ADDRESSABLE on the lattice this app ships. Membership itself is authored in
   * 00-zones.npps and is deliberately NOT re-derived here — an earlier version of
   * this test rebuilt the eight named zones from a `lobe` field and diffed them,
   * which proved only that two copies of the same guess agreed. Anatomical
   * registration is gate REG-1 against the shell CAD, not an assertion.
   *
   * The generator enforces the same structural rules, but a stale committed
   * socket map would slip past that, so this reads the shipped artefacts.
   */
  it('every zone addresses only sockets that exist on this helmet', () => {
    expect(zoneNamespace.size).toBeGreaterThan(0);

    for (const [name, zone] of zoneNamespace) {
      expect(zone.sockets.length, `zone ${name} is empty`).toBeGreaterThan(0);

      const unknown = zone.sockets.filter(id => !isValidSocketId(id));
      expect(unknown, `zone ${name} references non-existent sockets`).toEqual([]);

      // A zone is a SET — a repeat would double-dose that site and inflate every
      // coverage denominator computed from the zone.
      expect(
        [...zone.sockets].sort((a, b) => a - b),
        `zone ${name} lists a socket twice`,
      ).toEqual(unionSockets(zone.sockets));
    }
  });

  it('aggregate zones are exact unions of the zones they name', () => {
    // Deliberately the SAME union helper the app uses: an aggregate that matched
    // a hand-rolled concatenation but not `unionSockets` would mean the shipped
    // list double-counts a midline socket.
    const union = (...names: string[]) =>
      unionSockets(...names.map(n => zoneNamespace.get(n)!.sockets));

    const sortedOf = (name: string) => [...zoneNamespace.get(name)!.sockets].sort((a, b) => a - b);

    expect(sortedOf('Frontal')).toEqual(union('Frontal Left', 'Frontal Right'));
    expect(sortedOf('Posterior')).toEqual(
      union('Parietal Left', 'Parietal Right', 'Occipital Left', 'Occipital Right'),
    );
    expect(sortedOf('Vault (excl. Occipital)')).toEqual(
      union(
        'Frontal Left', 'Frontal Right', 'Temporal Left', 'Temporal Right',
        'Parietal Left', 'Parietal Right',
      ),
    );
    expect(sortedOf('All')).toEqual(
      Array.from({ length: NP_SOCKET_COUNT }, (_, i) => i + NP_SOCKET_NUMBERING_BASE),
    );
  });

  /**
   * The evidence for the whole-head PBM protocols (pbm_neuro_protocols.md §1,
   * §6) enumerates frontal, temporal and parietal — never occipital. The
   * distinction between this zone and `All` is the whole reason it exists, so
   * pin it rather than trusting the socket list to stay right.
   */
  it('Vault (excl. Occipital) excludes occipital; All does not', () => {
    const occipital = new Set([
      ...zoneNamespace.get('Occipital Left')!.sockets,
      ...zoneNamespace.get('Occipital Right')!.sockets,
    ]);
    const covered = zoneNamespace.get('Vault (excl. Occipital)')!.sockets;

    expect(covered.length).toBe(NP_SOCKET_COUNT - occipital.size);
    expect(covered.filter(s => occipital.has(s))).toEqual([]);

    // The midline sockets are the interesting ones: each is listed in BOTH the
    // Left and the Right zone it belongs to, so a naive union would sneak the
    // occipital one in. Which sockets those are is read from the zone file — the
    // test asks the file what is occipital, it does not decide for itself.
    const allMidline = NP_SOCKETS.filter(s => s.side === 'midline').map(s => s.id);
    const occipitalMidline = allMidline.filter(id => occipital.has(id));

    expect(occipitalMidline.length).toBeGreaterThan(0);
    for (const id of occipitalMidline) {
      expect(covered, `occipital midline socket ${id}`).not.toContain(id);
    }
    // ...but every other midline socket IS covered.
    for (const id of allMidline.filter(id => !occipital.has(id))) {
      expect(covered, `midline socket ${id}`).toContain(id);
    }
    expect(zoneNamespace.get('All')!.sockets.filter(s => occipital.has(s)).length)
      .toBe(occipital.size);
  });

  /**
   * §8 targets "bilateral, midline over SMA/motor". The zone must therefore be
   * balanced across hemispheres and include a midline site. Its author placed it
   * on a single coronal row minus that row's two outermost sockets. Those are
   * lattice facts, so they are pinned here; whether that row is really over the
   * motor representation is gate REG-1 against the shell CAD, and no assertion
   * in this file can answer it.
   */
  it('Motor / SMA is a balanced bilateral band over the motor strip', () => {
    const motor = zoneNamespace.get('Motor / SMA')!.sockets;
    const geo = (id: number) => NP_SOCKETS.find(s => s.id === id)!;

    // Balanced bilateral, with exactly one midline site.
    const left = motor.filter(id => geo(id).side === 'left');
    const right = motor.filter(id => geo(id).side === 'right');
    expect(left.length).toBe(right.length);
    expect(left.length).toBeGreaterThan(0);
    expect(motor.filter(id => geo(id).side === 'midline').length).toBe(1);

    // A single coronal band — the precentral motor strip — nothing spread across
    // rows.
    expect(new Set(motor.map(id => geo(id).row)).size).toBe(1);

    // The two outermost sockets of that row are excluded — the zone is the
    // interior of the band, not the whole band. Stated on the lattice (col
    // position within the row), which is what the zone description claims and
    // what a re-cut would change.
    const row = geo(motor[0]).row;
    const rowSockets = NP_SOCKETS.filter(s => s.row === row);
    const outermost = [rowSockets[0].id, rowSockets[rowSockets.length - 1].id];
    expect(motor.length).toBe(rowSockets.length - 2);
    for (const id of outermost) {
      expect(motor, `outermost socket ${id} of the row`).not.toContain(id);
    }
  });

  it("the Parkinson's protocol targets Motor / SMA, not All", () => {
    const source = readFileSync(
      join(__repoRoot, 'protocols', 'predefined', 'clinical-08-pbm-parkinsons.npps'),
      'utf-8',
    );
    expect(source).toContain('zones: ["Motor / SMA"]');
    expect(source).not.toContain('zones: ["All"]');
  });

  it('the whole-head PBM protocols target Vault (excl. Occipital), not All', () => {
    for (const file of [
      'clinical-01-pbm-alzheimers.npps',
      '01-gamma-focus.npps',
      'clinical-06-pbm-tbi.npps',
    ]) {
      const source = readFileSync(join(__repoRoot, 'protocols', 'predefined', file), 'utf-8');
      expect(source, file).toContain('zones: ["Vault (excl. Occipital)"]');
      expect(source, file).not.toContain('zones: ["All"]');
    }
  });

  /**
   * Midline sockets are the reason zone unions must dedup: the zone file lists
   * each one in both a left-side and a right-side zone, so concatenating such a
   * pair double-counts it. There is exactly one per odd-width row.
   */
  it('marks the midline sockets shared between hemispheres', () => {
    const midline = NP_SOCKETS.filter(s => s.side === 'midline').map(s => s.id);
    const oddRows = NP_ROW_WIDTHS.filter(w => w % 2 === 1).length;
    expect(midline.length).toBe(oddRows);

    // Every midline socket appears in at least one "* Left" and one "* Right"
    // zone. Which zones those are is the zone file's business — this only checks
    // that the double membership the picker and the union logic assume is real.
    const sideZones = (side: 'Left' | 'Right') =>
      [...zoneNamespace.values()].filter(z => z.name.endsWith(` ${side}`));

    for (const id of midline) {
      expect(
        sideZones('Left').filter(z => z.sockets.includes(id)).map(z => z.name),
        `${id} in a Left zone`,
      ).not.toEqual([]);
      expect(
        sideZones('Right').filter(z => z.sockets.includes(id)).map(z => z.name),
        `${id} in a Right zone`,
      ).not.toEqual([]);
    }

    // The union of a left/right pair must count each midline socket once.
    const fl = zoneNamespace.get('Frontal Left')!.sockets;
    const fr = zoneNamespace.get('Frontal Right')!.sockets;
    expect(unionSockets(fl, fr).length).toBeLessThan(fl.length + fr.length);
    expect(unionSockets(fl, fr)).toEqual(zoneNamespace.get('Frontal')!.sockets);
  });
});

// ─── Inventory ─────────────────────────────────────────────────────────────────

function inventoryWith(fitting: Record<number, string | null>): NPHelmetInventory {
  const sockets: NPSocketInventory[] = NP_SOCKETS.map(geo => {
    const part = fitting[geo.id] ?? null;
    return part
      ? { socketId: geo.id, present: true, partNumber: part, elements: MODULE_TYPES[part].elements }
      : { socketId: geo.id, present: false, elements: [] };
  });
  return new NPHelmetInventory(sockets);
}

describe('inventory', () => {
  it('reports element availability per socket', () => {
    const inv = inventoryWith({ 1: 'ZM-PBM-DUAL', 2: 'ZM-EEG' });
    expect(inv.provides(1, 'led_660')).toBe(true);
    expect(inv.provides(1, 'eeg_electrode')).toBe(false);
    expect(inv.provides(2, 'eeg_electrode')).toBe(true);
    expect(inv.provides(3, 'led_660')).toBe(false); // empty socket
  });

  it('treats a dual electrode as satisfying either an EEG or a tES need', () => {
    const inv = inventoryWith({ 1: 'ZM-DUAL-EL' });
    expect(inv.satisfies(1, ['eeg_electrode', 'dual_electrode'])).toBe(true);
    expect(inv.satisfies(1, ['tes_electrode', 'dual_electrode'])).toBe(true);
    expect(inv.satisfies(1, ['led_660'])).toBe(false);
  });

  it('simulator presets populate and depopulate sockets', () => {
    const provider = new NPSimulatedInventoryProvider('none');
    expect(provider.getInventory()).toBeNull();

    provider.setPreset('pbm-only');
    expect(provider.getInventory()!.occupiedSockets.length).toBe(NP_SOCKET_COUNT);

    provider.setPreset('partial-anterior');
    // The front half of the lattice rows, whatever the lattice makes it — the
    // preset selects by row, so the expectation reads the row too.
    const anteriorRows = Math.floor(NP_ROW_WIDTHS.length / 2);
    expect(provider.getInventory()!.occupiedSockets).toEqual(
      NP_SOCKETS.filter(s => s.row < anteriorRows).map(s => s.id),
    );
  });
});

// ─── Coverage ──────────────────────────────────────────────────────────────────

describe('zone coverage', () => {
  const frontalLeft = zoneNamespace.get('Frontal Left')! as NPZoneDefinition;
  // Sized from the shipped zone, not restated: the lattice is derived from tile
  // geometry, so a literal here would just be another copy of the socket count.
  const FL = frontalLeft.sockets.length;
  const FL_FITTED = Math.max(1, FL - 1);

  it('reports full coverage when every socket has the modules', () => {
    const inv = inventoryWith(Object.fromEntries(frontalLeft.sockets.map(s => [s, 'ZM-PBM-DUAL'])));
    const c = zoneCoverageFor(frontalLeft, 'pbm_transcranial', inv);

    expect(c.satisfied.length).toBe(FL);
    expect(c.missing).toEqual([]);
    expect(coverageFraction(c)).toBe(1);
    expect(coverageLabel(c)).toBe(`${FL}/${FL} sockets`);
  });

  /** The behaviour you chose: partial zones are offered, with the shortfall shown. */
  it('reports partial coverage rather than excluding the zone', () => {
    const fitting: Record<number, string | null> = {};
    frontalLeft.sockets.forEach((s, i) => (fitting[s] = i < FL_FITTED ? 'ZM-PBM-DUAL' : 'ZM-EEG'));
    const c = zoneCoverageFor(frontalLeft, 'pbm_transcranial', inventoryWith(fitting));

    expect(coverageLabel(c)).toBe(`${FL_FITTED}/${FL} sockets`);
    expect(c.missing.length).toBe(FL - FL_FITTED);

    const offered = zonesForModality([frontalLeft], 'pbm_transcranial', inventoryWith(fitting));
    expect(offered.map(z => z.zoneName)).toContain('Frontal Left');
  });

  it('excludes a zone with zero coverage from the modality dropdown', () => {
    const inv = inventoryWith(Object.fromEntries(frontalLeft.sockets.map(s => [s, 'ZM-EEG'])));
    expect(zonesForModality([frontalLeft], 'pbm_transcranial', inv)).toEqual([]);
    // ...but the same helmet fully supports EEG there.
    expect(zonesForModality([frontalLeft], 'eeg_neurofeedback', inv).length).toBe(1);
  });

  it('requires both wavelengths at the SAME socket, not spread across the zone', () => {
    const fitting: Record<number, string | null> = {};
    // Every socket has one wavelength or the other, none has both.
    frontalLeft.sockets.forEach((s, i) => (fitting[s] = i % 2 === 0 ? 'ZM-EEG' : 'ZM-PBM-DEEP'));
    const c = zoneCoverageFor(frontalLeft, 'pbm_transcranial', inventoryWith(fitting));
    expect(c.satisfied).toEqual([]);
  });

  it('sorts offered zones by coverage, best first', () => {
    const parietalLeft = zoneNamespace.get('Parietal Left')!;
    const fitting: Record<number, string | null> = {};
    frontalLeft.sockets.forEach(s => (fitting[s] = 'ZM-PBM-DUAL'));
    parietalLeft.sockets.forEach((s, i) => (fitting[s] = i < 3 ? 'ZM-PBM-DUAL' : null));

    const offered = zonesForModality(
      [parietalLeft, frontalLeft], 'pbm_transcranial', inventoryWith(fitting),
    );
    expect(offered[0].zoneName).toBe('Frontal Left');
  });
});

// ─── Eligibility ───────────────────────────────────────────────────────────────

function pbmProtocol(zoneRefs: string[]): NPProtocolDefinition {
  return {
    id: '1', name: 'Test PBM', description: '', author: 'test', version: '1.0',
    tags: [], createdAt: '', modifiedAt: '', isPredefined: false,
    timingMode: { type: 'duration', seconds: 600 },
    modalities: [{
      id: 'm1', enabled: true, interval: { intervalOnSeconds: 0, intervalOffSeconds: 0 },
      modalityParams: {
        type: 'pbm_transcranial',
        params: {
          zones: 'named', zoneRefs,
          wavelength: '660_808nm', intensityPercent: 75, frequencyHz: 40, dutyCyclePercent: 25,
        },
      },
    }],
  } as NPProtocolDefinition;
}

/**
 * Three sockets in the rear half of the lattice, taken from the map rather than
 * named, so the clinician-targeting tests survive a re-cut lattice.
 * `partial-anterior` leaves these empty, which is what those tests need.
 */
const POSTERIOR_TARGETS = NP_SOCKETS
  .filter(s => s.row >= Math.floor(NP_ROW_WIDTHS.length / 2))
  .slice(0, 3)
  .map(s => s.id);

describe('protocol eligibility', () => {
  it('is eligible when every requested zone is fully covered', () => {
    const fl = zoneNamespace.get('Frontal Left')!;
    const inv = inventoryWith(Object.fromEntries(fl.sockets.map(s => [s, 'ZM-PBM-DUAL'])));

    const result = evaluateProtocol(pbmProtocol(['Frontal Left']), inv, zoneNamespace);
    expect(result.eligible).toBe(true);
    expect(result.degraded).toBe(false);
    expect(result.summary).toBe('');
  });

  it('is eligible-but-degraded on partial coverage', () => {
    const fl = zoneNamespace.get('Frontal Left')!;
    const fitting: Record<number, string | null> = {};
    const fitted = fl.sockets.length - 1;
    fl.sockets.forEach((s, i) => (fitting[s] = i < fitted ? 'ZM-PBM-DUAL' : null));

    const result = evaluateProtocol(pbmProtocol(['Frontal Left']), inventoryWith(fitting), zoneNamespace);
    expect(result.eligible).toBe(true);
    expect(result.degraded).toBe(true);
    expect(result.shortfalls[0].coverage[0].satisfied.length).toBe(fitted);
  });

  it('is ineligible with per-socket detail when a zone has no support', () => {
    const inv = inventoryWith({}); // empty helmet
    const result = evaluateProtocol(pbmProtocol(['Frontal Left']), inv, zoneNamespace);

    expect(result.eligible).toBe(false);
    expect(result.summary).toContain('Frontal Left');
    const FL = zoneNamespace.get('Frontal Left')!.sockets.length;
    expect(result.summary).toContain(`${FL} sockets need re-fitting`);

    const shortfall = result.shortfalls[0];
    expect(shortfall.sockets.length).toBe(FL);
    expect(shortfall.sockets[0].socketId).toBe(NP_SOCKET_ID_MIN);
    expect(shortfall.sockets[0].fitted).toBeUndefined();
  });

  it('names modules that would fix each blocking socket', () => {
    const inv = inventoryWith({ 1: 'ZM-EEG' });
    const result = evaluateProtocol(pbmProtocol(['Frontal Left']), inv, zoneNamespace);

    const socket1 = result.shortfalls[0].sockets.find(s => s.socketId === 1)!;
    expect(socket1.fitted).toBe('ZM-EEG');
    // Must suggest parts supplying BOTH wavelengths, never a part fixing only one.
    expect(socket1.candidateModules).toContain('ZM-PBM-DUAL');
    expect(socket1.candidateModules).not.toContain('ZM-EEG');
    expect(socket1.candidateModules).not.toContain('ZM-PBM-DEEP');

    // Every candidate must supply BOTH wavelengths — that is the property worth
    // pinning, rather than the exact part list, which grows as the catalogue does.
    for (const part of socket1.candidateModules) {
      expect(MODULE_TYPES[part].elements).toEqual(
        expect.arrayContaining(['led_660', 'led_808']),
      );
    }

    expect(describeShortfall(socket1)).toContain(
      'Socket 1: fitted ZM-EEG, needs 660nm LED + 808nm LED — fit ',
    );
  });

  it('reports an unresolved zone reference distinctly from a hardware shortfall', () => {
    const inv = new NPSimulatedInventoryProvider('full-t1').getInventory()!;
    const result = evaluateProtocol(pbmProtocol(['Nonexistent Zone']), inv, zoneNamespace);

    expect(result.eligible).toBe(false);
    expect(result.summary).toContain('not defined');
    expect(result.shortfalls[0].unresolvedZones).toEqual(['Nonexistent Zone']);
  });

  /** Legacy front/rear/custom selections are not NPPS zones and must not pass. */
  it('rejects legacy non-NPPS zone selections', () => {
    const proto = pbmProtocol([]);
    (proto.modalities[0].modalityParams.params as { zones: string }).zones = 'front';

    const inv = new NPSimulatedInventoryProvider('full-t1').getInventory()!;
    const result = evaluateProtocol(proto, inv, zoneNamespace);

    expect(result.eligible).toBe(false);
    expect(result.shortfalls[0].unresolvedZones[0]).toContain('legacy');
  });

  it('ignores accessory-delivered modalities when judging socket fit', () => {
    for (const type of ['pbm_intranasal', 'vns_hrv', 'audio_entrainment', 'visual_stimulation', 'tms'] as const) {
      expect(MODALITY_REQUIREMENTS[type].socketBased).toBe(false);
    }

    const proto = pbmProtocol(['Frontal Left']);
    proto.modalities[0].modalityParams = {
      type: 'audio_entrainment',
      params: {} as never,
    } as never;

    const result = evaluateProtocol(proto, inventoryWith({}), zoneNamespace);
    expect(result.eligible).toBe(true);
  });

  it('requires the dual-rated electrode specifically for HD-tDCS', () => {
    const fl = zoneNamespace.get('Frontal Left')!;
    const tesOnly = inventoryWith(Object.fromEntries(fl.sockets.map(s => [s, 'ZM-TES'])));
    const dual = inventoryWith(Object.fromEntries(fl.sockets.map(s => [s, 'ZM-DUAL-EL'])));

    expect(zoneCoverageFor(fl, 'hd_tdcs', tesOnly).satisfied).toEqual([]);
    expect(zoneCoverageFor(fl, 'hd_tdcs', dual).satisfied.length).toBe(fl.sockets.length);
    // ...while plain tACS is happy with either.
    expect(zoneCoverageFor(fl, 'bes_tacs', tesOnly).satisfied.length).toBe(fl.sockets.length);
  });

  // ─── Clinician-selected targeting ────────────────────────────────────────────
  //
  // Post-stroke rehab targets perilesional cortex (pbm_neuro_protocols.md §9),
  // which depends on where the individual's lesion is. No preset zone can be
  // right, so the protocol must stay blocked until an operator picks sockets.

  function clinicianTargetedProtocol(): NPProtocolDefinition {
    const p = pbmProtocol([]);
    (p.modalities[0].modalityParams.params as { zones: string }).zones = 'clinician_selected';
    return p;
  }

  it('blocks a clinician-selected protocol until sockets are chosen', () => {
    const inv = new NPSimulatedInventoryProvider('full-t1').getInventory()!;
    const result = evaluateProtocol(clinicianTargetedProtocol(), inv, zoneNamespace);

    expect(result.eligible).toBe(false);
    expect(result.requiresTargeting).toEqual(['pbm_transcranial']);
    // Distinct from a hardware shortfall — the helmet is fully fitted.
    expect(result.shortfalls).toEqual([]);
    expect(result.summary).toContain('selected for this patient');
  });

  it('clears once the operator supplies sockets', () => {
    const inv = new NPSimulatedInventoryProvider('full-t1').getInventory()!;
    const targeting = new Map([['pbm_transcranial' as const, POSTERIOR_TARGETS]]);
    const result = evaluateProtocol(clinicianTargetedProtocol(), inv, zoneNamespace, targeting);

    expect(result.eligible).toBe(true);
    expect(result.requiresTargeting).toEqual([]);
  });

  it('still checks the fitted hardware at the operator-chosen sockets', () => {
    // Anterior rows only, so the posterior targets are empty sockets.
    const inv = new NPSimulatedInventoryProvider('partial-anterior').getInventory()!;
    const targeting = new Map([['pbm_transcranial' as const, POSTERIOR_TARGETS.slice(0, 2)]]);
    const result = evaluateProtocol(clinicianTargetedProtocol(), inv, zoneNamespace, targeting);

    expect(result.eligible).toBe(false);
    expect(result.requiresTargeting).toEqual([]);
    expect(result.shortfalls[0].sockets.map(s => s.socketId)).toEqual(POSTERIOR_TARGETS.slice(0, 2));
  });

  it('an empty selection does not count as targeting', () => {
    const inv = new NPSimulatedInventoryProvider('full-t1').getInventory()!;
    const targeting = new Map([['pbm_transcranial' as const, [] as number[]]]);
    const result = evaluateProtocol(clinicianTargetedProtocol(), inv, zoneNamespace, targeting);

    expect(result.requiresTargeting).toEqual(['pbm_transcranial']);
  });

  it('the shipped stroke protocol defers its target to the clinician', () => {
    const source = readFileSync(
      join(__repoRoot, 'protocols', 'predefined', 'clinical-09-pbm-stroke-rehab.npps'),
      'utf-8',
    );
    const { entries } = parseNPPSFile(source);
    const proto = (entries.find(e => e.kind === 'single') as { protocol: NPProtocolDefinition }).protocol;

    const inv = new NPSimulatedInventoryProvider('full-t1').getInventory()!;
    const result = evaluateProtocol(proto, inv, zoneNamespace);

    expect(result.eligible).toBe(false);
    expect(result.requiresTargeting).toContain('pbm_transcranial');
  });

  it('flags a zone socket that is not addressable on this helmet', () => {
    const bogus: NPZoneDefinition = { name: 'Bogus', sockets: [1, 2, 999] };
    const inv = new NPSimulatedInventoryProvider('pbm-only').getInventory()!;
    const c = zoneCoverageFor(bogus, 'pbm_transcranial', inv);

    expect(c.invalid).toEqual([999]);
    expect(c.total).toBe(2);
  });
});
