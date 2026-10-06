// The field table (common/npps/fields.json) is the data the shared NPPS core reads
// (OI-NPPS-CORE-01). It must say what the reference parser does, or the core and the web
// parser disagree: this fails when a default, a field or an alias drifts in either.

import { describe, it, expect } from 'vitest';
import fs from 'node:fs';
import path from 'node:path';
import { parseNPPS } from './nppsParser';
import { defaultParams, type NPModalityTypeId } from '../types/protocol';

const table = JSON.parse(fs.readFileSync(path.resolve(__dirname, '../npps/fields.json'), 'utf8'));
const MODALITIES = Object.keys(table.modalities) as NPModalityTypeId[];

/** A minimal block that parses: the required fields only, in their short spelling. */
function minimalBlock(type: string): string {
  const fields = table.modalities[type].fields as Array<Record<string, any>>;
  const lines: string[] = [];
  for (const f of fields) {
    if (f.kind === 'wavelength') lines.push('wavelength: "808nm"');
    if (f.kind === 'quantity') lines.push(`${f.short}: 30${f.unit}`);
  }
  return `protocol "T" {\n ${type} {\n${lines.join('\n')}\n }\n}\n`;
}

describe('the field table agrees with the reference parser', () => {
  it('lists exactly the fifteen modality types', () => {
    expect(MODALITIES).toHaveLength(15);
    for (const t of MODALITIES) expect(() => defaultParams(t)).not.toThrow();
  });

  for (const type of ['pbm_transcranial', 'pbm_intranasal', 'eeg_neurofeedback', 'bes_tacs', 'tdcs', 'vns_hrv',
    'audio_entrainment', 'visual_stimulation', 'qeeg_21ch', 'tms', 'pbm_deep_1170nm', 'clinical_tacs',
    'hd_tdcs', 'cervical_vns', 'vibrotactile_40hz']) {
    it(`${type}: every defaulted field has the default defaultParams() gives it`, () => {
      const [entry] = parseNPPS(minimalBlock(type));
      const params = (entry as any).protocol.modalities[0].modalityParams.params as Record<string, unknown>;
      const defaults = defaultParams(type as NPModalityTypeId) as unknown as Record<string, unknown>;
      for (const f of table.modalities[type].fields as Array<Record<string, any>>) {
        if (f.kind === 'zones') {
          expect(params['zones']).toEqual(f.default.zones);
          expect(params['zoneRefs']).toEqual(f.default.zoneRefs);
          continue;
        }
        if (['wavelength', 'quantity', 'optional_number', 'optional_string', 'string_array_optional'].includes(f.kind)) continue;
        // The parse output carries every non-optional field, at its default when unwritten.
        expect(params, `${type}.${f.json}`).toHaveProperty(f.json);
        if ('default' in f) expect(params[f.json], `${type}.${f.json}`).toEqual(f.default);
        // And defaultParams() is where the default is stated in the web code.
        if (f.json in defaults && f.kind !== 'pulse_duty' && f.kind !== 'pulse_frequency') {
          expect(defaults[f.json], `defaultParams ${type}.${f.json}`).toEqual(f.default);
        }
        if (f.kind === 'pulse_frequency' || f.kind === 'pulse_duty') {
          expect(defaults[f.json], `defaultParams ${type}.${f.json}`).toEqual(f.default);
        }
      }
    });

    it(`${type}: the parse output carries no field the table does not list`, () => {
      const [entry] = parseNPPS(minimalBlock(type));
      const params = (entry as any).protocol.modalities[0].modalityParams.params as Record<string, unknown>;
      const listed = new Set<string>();
      for (const f of table.modalities[type].fields as Array<Record<string, any>>) {
        listed.add(f.json);
        if (f.kind === 'zones') listed.add('zoneRefs');
      }
      for (const k of Object.keys(params)) expect(listed.has(k), `${type} emits ${k}`).toBe(true);
    });
  }

  it('the aliases are the reference parser\'s aliases', () => {
    // Each alias, written in place of its canonical name, must give the same parse.
    for (const [alias, canonical] of Object.entries(table.aliases as Record<string, string>)) {
      const owner = MODALITIES.find(m => (table.modalities[m].fields as any[]).some(f => f.key === canonical));
      expect(owner, `no modality owns ${canonical}`).toBeDefined();
      const sample = (canonical.endsWith('_hz') || canonical.endsWith('_seconds') || canonical.includes('rate') || canonical.includes('percent')) ? '7' : 'true';
      const noise = canonical === 'noise_type' ? 'pink' : sample;
      const base = minimalBlock(owner!);
      const withKey = (k: string) => base.replace(/ \}\n\}\n$/, ` ${k}: ${noise}\n }\n}\n`);
      const a = parseNPPS(withKey(alias)) as any[];
      const c = parseNPPS(withKey(canonical)) as any[];
      expect(a[0].protocol.modalities[0].modalityParams, `${alias} -> ${canonical}`)
        .toEqual(c[0].protocol.modalities[0].modalityParams);
    }
  });
});
