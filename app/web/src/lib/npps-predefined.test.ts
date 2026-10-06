import { describe, it, expect } from 'vitest';
import fs from 'node:fs';
import path from 'node:path';
import {
  parseNPPSFile,
  buildNamespace,
  validateNamespaceReferences,
} from '../../../../common/lib/nppsParser';
import { validateProtocol } from '../../../../common/lib/protocolValidator';
import type { NPLimitsSet } from '../../../../common/types/limits';

// Load the predefined library straight off disk and prove the whole set parses
// and cross-references cleanly.
//
// This reads the canonical `protocols/` tree — the single source of truth. The
// app fetches the same files at runtime: in dev the Vite plugin in
// vite.config.ts streams them from here, and at build time it copies them into
// the bundle. There is deliberately no second copy under public/.
const DIR = path.resolve(__dirname, '../../../../protocols/predefined');

interface Manifest {
  zones?: string[];
  conditions?: string[];
  protocols: string[];
  composites: string[];
}

const manifest = JSON.parse(fs.readFileSync(path.join(DIR, 'manifest.json'), 'utf8')) as Manifest;
const allFiles = [
  ...(manifest.zones ?? []),
  ...(manifest.conditions ?? []),
  ...manifest.protocols,
  ...manifest.composites,
];

describe('predefined NPPS library', () => {
  it('every manifest file exists and parses without throwing', () => {
    for (const f of allFiles) {
      const fp = path.join(DIR, f);
      expect(fs.existsSync(fp), `missing ${f}`).toBe(true);
      expect(() => parseNPPSFile(fs.readFileSync(fp, 'utf8')), `parse ${f}`).not.toThrow();
    }
  });

  const parsed = allFiles.map(f => parseNPPSFile(fs.readFileSync(path.join(DIR, f), 'utf8')));
  const { namespace, errors: namespaceErrors } = buildNamespace(parsed);

  it('has no duplicate zone/condition name collisions', () => {
    // A collision is an error, not a warning, and leaves the name unbound —
    // so the shipped library having one would also break reference resolution.
    expect(namespaceErrors).toEqual([]);
  });

  /**
   * Zones are defined ONLY in 00-zones.npps, so this asserts the shape every
   * zone must have rather than a list of expected names — naming them here would
   * just be a second copy of the file, and would quietly reject the user-defined
   * and research zones the .npps tree is designed to accept (SMART-1).
   */
  it('every declared zone carries a non-empty, deduplicated socket set', () => {
    expect(namespace.zones.size).toBeGreaterThan(0);

    for (const [name, zone] of namespace.zones) {
      expect(zone.sockets.length, `zone ${name} is empty`).toBeGreaterThan(0);
      expect(
        new Set(zone.sockets).size,
        `zone ${name} lists a socket more than once`,
      ).toBe(zone.sockets.length);
    }
  });

  it('every protocol/composite zone and condition reference resolves', () => {
    const errors = validateNamespaceReferences(namespace);
    expect(errors).toEqual([]);
  });

  it('every protocol condition names a defined condition with a link', () => {
    for (const [, c] of namespace.conditions) {
      expect(c.link, `condition ${c.name} link`).toMatch(/^https?:\/\//);
    }
  });

  // ─── The shipped library must be RUNNABLE, not merely parseable (OI-CHARGE-05) ──
  //
  // This suite proved for a long time that every predefined protocol parses and
  // cross-references, and never that any of them would survive hardware
  // validation. It would not have: as of 2026-09-09, thirteen of the fourteen
  // shipped tDCS protocols exceeded the charge-density ceiling then in force,
  // and nothing in CI said so. (They were not being truncated on device either,
  // because the interlock was inert — which is how a whole unrunnable library
  // went unnoticed from both ends at once.)
  //
  // The ceiling that made them fail was a per-PHASE pulsed figure applied to a
  // DC session dose; OI-CHARGE-05 split it, and NP-DT-001 §3.2.1 derived each
  // half. This test is what stops the shipped library and the enforcer drifting
  // apart again in either direction — a ceiling that rejects the library, or a
  // protocol authored past the ceiling.
  it('every shipped protocol passes hardware validation', () => {
    const hardwareOnly: NPLimitsSet = {
      id: 'predefined-hw-check',
      name: 'Hardware only',
      description: 'No dosage limits — hardware ceilings alone.',
      createdAt: '2026-09-15T00:00:00Z',
      modifiedAt: '2026-09-15T00:00:00Z',
      level: 'global',
    };

    // One known, documented exception, waived by NAME so the waiver cannot
    // outlive the defect: if the protocol is fixed, or if a second protocol
    // starts failing, this list stops matching and the test fails. (Same
    // discipline as the consent-reachability waivers.)
    //
    // OI-NPPS-LIMITS-01: "taVNS — Stroke Motor Rehab (paired)" authors 30 Hz, which is
    // Dawson 2021 VNS-REHAB's frequency — but VNS-REHAB is the IMPLANTED
    // Vivistim device, and NeurOne's auricular VNS ceiling is 1–25 Hz
    // (CLAUDE.md §3, locked). The protocol transcribed a source trial's
    // parameter that this hardware cannot deliver. It is NOT silently retuned
    // to 25 Hz here: that would put a number in a cited clinical protocol that
    // the citation does not support, which is the same class of move as
    // inflating a declared pad area to satisfy a ceiling.
    //
    // Schiffer 2009 ran CW (100% duty) at 250 mW/cm², 4 min at F3 then 4 min
    // at F4. The protocol states exactly that and is refused: a PBM ceiling
    // refuses a protocol and never reshapes one (CLAUDE.md §3). The 25% duty
    // cap is still enforced until OI-HEXTILE-31. 250 mW/cm² is inside the
    // time average at 808 nm (≈329) since OI-HEXTILE-32 adopted the reference
    // skin limit, so in the validator the duty cap is the only refusal. One
    // entry per block. Wang 2023 is the same case: CW at 310 mW/cm² at 820nm
    // (≈348 allowed), one block. (The validator cannot see the 250 or the 310:
    // `intensity` is not irradiance yet, which OI-HEXTILE-31's pre-signing
    // refusal also has to fix.)
    // OI-SESPWR-03 (2026-10-04): the parser refuses `frequency: 0` with a duty
    // other than 100 %, and reads a CW block as 100 % duty, written or not. The
    // library's CW blocks therefore state CW and are refused by the 25 % duty
    // cap until OI-HEXTILE-31, as Schiffer and Wang already were: Cassano 2018
    // (36 mW/cm² CW), Maiello, Memory Boost (Yao 2022), Alzheimer's 1064 nm, Naeser TBI transcranial + intranasal, stroke rehab
    // transcranial + intranasal, and Vascular Baseline. Before this, each of
    // them parsed as 25 % duty and passed, which was a pulsed stimulus
    // standing in for the CW its own source used. Vascular Baseline is also
    // far over R-4's CW ceiling (OI-SESPWR-02).
    // Rev 18 (absolute intensity): one entry per CW BLOCK, and a wavelength is its own block,
    // so Vascular Baseline (660 + 808 nm) lists twice and TBI and stroke list once per block
    // (scalp + two intranasal channels).
    const WAIVED = [
      'taVNS — Stroke Motor Rehab (paired): frequencyHz = 30 Hz (limit 1–25 Hz (hardware))',
      'PBM — Anxiety (Wang 2023, high-irradiance): dutyCyclePercent = 100% (limit 25% (hardware))',
      'PBM — Depression (Schiffer 2009, high-irradiance): dutyCyclePercent = 100% (limit 25% (hardware))',
      'PBM — Depression (Schiffer 2009, high-irradiance): dutyCyclePercent = 100% (limit 25% (hardware))',
      'Memory Boost: dutyCyclePercent = 100% (limit 25% (hardware))',
      "PBM — Alzheimer's 1064nm (deep-cortical channel): dutyCyclePercent = 100% (limit 25% (hardware))",
      'PBM — Anxiety (Maiello 2019, low-irradiance CW): dutyCyclePercent = 100% (limit 25% (hardware))',
      'PBM — Depression (Cassano 2018, low-irradiance CW): dutyCyclePercent = 100% (limit 25% (hardware))',
      'PBM — Stroke (chronic rehab): dutyCyclePercent = 100% (limit 25% (hardware))',
      'PBM — Stroke (chronic rehab): dutyCyclePercent = 100% (limit 25% (hardware))',
      'PBM — Stroke (chronic rehab): dutyCyclePercent = 100% (limit 25% (hardware))',
      'PBM — TBI (chronic) + Intranasal (Naeser 2011/2014): dutyCyclePercent = 100% (limit 25% (hardware))',
      'PBM — TBI (chronic) + Intranasal (Naeser 2011/2014): dutyCyclePercent = 100% (limit 25% (hardware))',
      'PBM — TBI (chronic) + Intranasal (Naeser 2011/2014): dutyCyclePercent = 100% (limit 25% (hardware))',
      'Vascular Baseline: dutyCyclePercent = 100% (limit 25% (hardware))',
      'Vascular Baseline: dutyCyclePercent = 100% (limit 25% (hardware))',
    ];

    const failures: string[] = [];
    for (const entry of namespace.entries) {
      if (entry.kind !== 'single') continue;
      const result = validateProtocol(entry.protocol, hardwareOnly);
      const hardwareErrors = result.issues.filter(
        i => i.severity === 'error' && i.limitSource === 'hardware',
      );
      for (const e of hardwareErrors) {
        failures.push(
          `${entry.protocol.name}: ${e.parameterKey} = ${e.actualValueDescription} (limit ${e.limitValueDescription})`,
        );
      }
    }

    expect(failures.sort(), `shipped protocols failing hardware validation:\n  ${failures.join('\n  ')}`)
      .toEqual([...WAIVED].sort());
  });

  // The charge ceilings specifically carry no waiver — OI-CHARGE-05 exists to
  // make the shipped library admissible under them, so a single failure here
  // means either the library or the ceiling has moved and they no longer agree.
  it('no shipped protocol exceeds either charge ceiling', () => {
    const hardwareOnly: NPLimitsSet = {
      id: 'predefined-charge-check',
      name: 'Hardware only',
      description: 'No dosage limits — hardware ceilings alone.',
      createdAt: '2026-09-15T00:00:00Z',
      modifiedAt: '2026-09-15T00:00:00Z',
      level: 'global',
    };

    const failures: string[] = [];
    for (const entry of namespace.entries) {
      if (entry.kind !== 'single') continue;
      for (const i of validateProtocol(entry.protocol, hardwareOnly).issues) {
        if (i.severity !== 'error') continue;
        if (i.parameterKey !== 'chargeDensityMCcm2' &&
            i.parameterKey !== 'phaseChargeDensityUCcm2') continue;
        failures.push(
          `${entry.protocol.name}: ${i.parameterKey} = ${i.actualValueDescription}`,
        );
      }
    }

    expect(failures, `protocols over a charge ceiling:\n  ${failures.join('\n  ')}`)
      .toEqual([]);
  });

  it('includes the expected clinical protocol coverage (53 clinical presets)', () => {
    const clinical = manifest.protocols.filter(f => f.startsWith('clinical-'));
    // 50 → 53: three sections whose evidence is two trials an order of
    // magnitude apart are now one protocol per trial rather than one per
    // section — anxiety §5 (Maiello 2019 / Wang 2023), depression §4
    // (Cassano 2018 / Schiffer 2009) and Alzheimer's §1 (Woźniak-Mitał 2026 /
    // Chun 2026). See those files' own notes.
    expect(clinical.length).toBe(53);
  });

  it('every clinical protocol carries at least one condition and one reference', () => {
    for (const entry of namespace.entries) {
      if (entry.kind !== 'single') continue;
      if (!entry.protocol.id.startsWith('30000')) continue; // clinical id band
      expect(entry.protocol.conditions?.length, `${entry.protocol.name} conditions`).toBeGreaterThan(0);
      expect(entry.protocol.references?.length, `${entry.protocol.name} references`).toBeGreaterThan(0);
    }
  });

  // The former 'mirrors identical content to protocols/predefined' test is gone
  // with the duplicate it policed: public/protocols/ was a byte-identical copy
  // of protocols/, so every edit had to be made twice. Vite now serves the
  // canonical tree directly (see vite.config.ts), leaving nothing to mirror.
  it('has no second copy of the protocol tree in the web app', () => {
    const publicCopy = path.resolve(__dirname, '../../public/protocols');
    expect(
      fs.existsSync(publicCopy),
      'public/protocols/ has reappeared — protocols/ is the single source of truth',
    ).toBe(false);
  });
});
