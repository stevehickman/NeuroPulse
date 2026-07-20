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
import { NP_SOCKETS, NP_SOCKET_COUNT, isValidSocketId } from './socketMap.generated';
import { parseNPPSFile, buildNamespace } from './nppsParser';
import type { NPProtocolDefinition, NPZoneDefinition } from '../types/protocol';

const __repoRoot = join(dirname(fileURLToPath(import.meta.url)), '..', '..', '..', '..');

const zoneNamespace = buildNamespace([
  parseNPPSFile(readFileSync(join(__repoRoot, 'protocols', 'predefined', '00-zones.npps'), 'utf-8')),
]).namespace.zones;

// ─── Socket map ────────────────────────────────────────────────────────────────

describe('generated socket map', () => {
  it('has 78 sockets numbered 1..78 with no gaps', () => {
    expect(NP_SOCKET_COUNT).toBe(78);
    expect(NP_SOCKETS.map(s => s.id)).toEqual(Array.from({ length: 78 }, (_, i) => i + 1));
  });

  it('rejects out-of-range socket ids', () => {
    expect(isValidSocketId(0)).toBe(false);
    expect(isValidSocketId(79)).toBe(false);
    expect(isValidSocketId(1)).toBe(true);
    expect(isValidSocketId(78)).toBe(true);
  });

  /**
   * The load-bearing invariant: the derived geometry must explain the locked
   * zone files. The generator enforces this too, but a stale committed table
   * would slip past that, so the test re-derives from the shipped artefact.
   */
  it('reproduces every shipped lobe zone exactly', () => {
    const derived = new Map<string, number[]>();
    const push = (name: string, id: number) =>
      derived.set(name, [...(derived.get(name) ?? []), id]);

    const label = { frontal: 'Frontal', temporal: 'Temporal', parietal: 'Parietal', occipital: 'Occipital' };

    for (const s of NP_SOCKETS) {
      const lobe = label[s.lobe];
      if (s.side === 'midline') {
        push(`${lobe} Left`, s.id);
        push(`${lobe} Right`, s.id);
      } else {
        push(`${lobe} ${s.side === 'left' ? 'Left' : 'Right'}`, s.id);
      }
    }

    // The eight lobe zones must match exactly. The file also carries aggregate
    // zones (All / Frontal / Posterior) that are unions of these, so they have
    // no lobe+side derivation of their own.
    expect(derived.size).toBe(8);
    for (const [name, ids] of derived) {
      const zone = zoneNamespace.get(name);
      expect(zone, `lobe zone ${name} missing from zone file`).toBeDefined();
      expect(ids.sort((a, b) => a - b), `zone ${name}`)
        .toEqual([...zone!.sockets].sort((a, b) => a - b));
    }
  });

  it('aggregate zones are exact unions of the lobe zones they replace', () => {
    const union = (...names: string[]) =>
      [...new Set(names.flatMap(n => zoneNamespace.get(n)!.sockets))].sort((a, b) => a - b);

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
    expect(sortedOf('All')).toEqual(Array.from({ length: 78 }, (_, i) => i + 1));
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

    expect(covered.length).toBe(60);
    expect(covered.filter(s => occipital.has(s))).toEqual([]);
    // ...including the occipital midline pair, which is in both hemispheres.
    expect(covered).not.toContain(67);
    expect(covered).not.toContain(75);
    // ...but the frontal/parietal midline sockets ARE covered.
    for (const midline of [1, 5, 13, 22, 31, 40, 49, 58]) {
      expect(covered, `midline socket ${midline}`).toContain(midline);
    }
    expect(zoneNamespace.get('All')!.sockets.filter(s => occipital.has(s)).length).toBe(18);
  });

  /**
   * §8 targets "bilateral, midline over SMA/motor". The zone must therefore be
   * balanced across hemispheres, include the midline, and stay inside the
   * frontal lobe — the postcentral gyrus is somatosensory, not motor.
   */
  it('Motor / SMA is a balanced bilateral frontal band', () => {
    const motor = zoneNamespace.get('Motor / SMA')!.sockets;
    expect(motor).toEqual([11, 12, 13, 14, 15, 16, 17, 18, 19]);

    const geo = (id: number) => NP_SOCKETS.find(s => s.id === id)!;

    // Entirely frontal — no parietal/somatosensory bleed.
    expect(motor.every(id => geo(id).lobe === 'frontal')).toBe(true);

    // Balanced bilateral, with the midline included.
    expect(motor.filter(id => geo(id).side === 'left')).toEqual([11, 12, 16, 17]);
    expect(motor.filter(id => geo(id).side === 'right')).toEqual([14, 15, 18, 19]);
    expect(motor.filter(id => geo(id).side === 'midline')).toEqual([13]);

    // The posterior-most frontal band: no frontal socket sits behind it.
    const maxFrontalRow = Math.max(
      ...NP_SOCKETS.filter(s => s.lobe === 'frontal').map(s => s.row),
    );
    expect(Math.max(...motor.map(id => geo(id).row))).toBe(maxFrontalRow);
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

  it('marks exactly the ten midline sockets shared between hemispheres', () => {
    const midline = NP_SOCKETS.filter(s => s.side === 'midline').map(s => s.id);
    expect(midline).toEqual([1, 5, 13, 22, 31, 40, 49, 58, 67, 75]);
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
    expect(provider.getInventory()!.occupiedSockets.length).toBe(78);

    provider.setPreset('partial-frontal');
    // Frontal band is rows 0..5, widths 1+2+3+4+5+4 = 19 sockets.
    expect(provider.getInventory()!.occupiedSockets).toEqual(
      Array.from({ length: 19 }, (_, i) => i + 1),
    );
  });
});

// ─── Coverage ──────────────────────────────────────────────────────────────────

describe('zone coverage', () => {
  const frontalLeft = zoneNamespace.get('Frontal Left')! as NPZoneDefinition;

  it('reports full coverage when every socket has the modules', () => {
    const inv = inventoryWith(Object.fromEntries(frontalLeft.sockets.map(s => [s, 'ZM-PBM-DUAL'])));
    const c = zoneCoverageFor(frontalLeft, 'pbm_transcranial', inv);

    expect(c.satisfied.length).toBe(11);
    expect(c.missing).toEqual([]);
    expect(coverageFraction(c)).toBe(1);
    expect(coverageLabel(c)).toBe('11/11 sockets');
  });

  /** The behaviour you chose: partial zones are offered, with the shortfall shown. */
  it('reports partial coverage rather than excluding the zone', () => {
    const fitting: Record<number, string | null> = {};
    frontalLeft.sockets.forEach((s, i) => (fitting[s] = i < 7 ? 'ZM-PBM-DUAL' : 'ZM-EEG'));
    const c = zoneCoverageFor(frontalLeft, 'pbm_transcranial', inventoryWith(fitting));

    expect(coverageLabel(c)).toBe('7/11 sockets');
    expect(c.missing.length).toBe(4);

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
    fl.sockets.forEach((s, i) => (fitting[s] = i < 7 ? 'ZM-PBM-DUAL' : null));

    const result = evaluateProtocol(pbmProtocol(['Frontal Left']), inventoryWith(fitting), zoneNamespace);
    expect(result.eligible).toBe(true);
    expect(result.degraded).toBe(true);
    expect(result.shortfalls[0].coverage[0].satisfied.length).toBe(7);
  });

  it('is ineligible with per-socket detail when a zone has no support', () => {
    const inv = inventoryWith({}); // empty helmet
    const result = evaluateProtocol(pbmProtocol(['Frontal Left']), inv, zoneNamespace);

    expect(result.eligible).toBe(false);
    expect(result.summary).toContain('Frontal Left');
    expect(result.summary).toContain('11 sockets need re-fitting');

    const shortfall = result.shortfalls[0];
    expect(shortfall.sockets.length).toBe(11);
    expect(shortfall.sockets[0].socketId).toBe(1);
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
    expect(zoneCoverageFor(fl, 'hd_tdcs', dual).satisfied.length).toBe(11);
    // ...while plain tACS is happy with either.
    expect(zoneCoverageFor(fl, 'bes_tacs', tesOnly).satisfied.length).toBe(11);
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
    const targeting = new Map([['pbm_transcranial' as const, [30, 31, 34]]]);
    const result = evaluateProtocol(clinicianTargetedProtocol(), inv, zoneNamespace, targeting);

    expect(result.eligible).toBe(true);
    expect(result.requiresTargeting).toEqual([]);
  });

  it('still checks the fitted hardware at the operator-chosen sockets', () => {
    // Frontal band only — sockets 30/31 are outside it, so they are empty.
    const inv = new NPSimulatedInventoryProvider('partial-frontal').getInventory()!;
    const targeting = new Map([['pbm_transcranial' as const, [30, 31]]]);
    const result = evaluateProtocol(clinicianTargetedProtocol(), inv, zoneNamespace, targeting);

    expect(result.eligible).toBe(false);
    expect(result.requiresTargeting).toEqual([]);
    expect(result.shortfalls[0].sockets.map(s => s.socketId)).toEqual([30, 31]);
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
