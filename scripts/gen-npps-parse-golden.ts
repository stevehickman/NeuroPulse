#!/usr/bin/env bun
/**
 * gen-npps-parse-golden.ts — the web parser's own output, as the golden vectors the
 * shared NPPS core (common/npps-core, OI-NPPS-CORE-01) is diffed against.
 *
 *   bun scripts/gen-npps-parse-golden.ts           # rewrites app/NeurOneShared/TestData/npps-parse-golden.json
 *   bun scripts/gen-npps-parse-golden.ts --check   # fails if the committed file is stale
 *
 * For every .npps file in protocols/predefined and npps/fixtures it records the normalised
 * `protocol` entries `parseNPPS` produces (ids and timestamps that are generated per parse
 * are dropped) and, for every input the web parser refuses, the exact message. The core must
 * produce the same entries and the same messages. `ERROR_CASES` adds refusals the fixtures do
 * not cover; add a case when a refusal is added.
 */
import fs from 'node:fs';
import path from 'node:path';

const ROOT = new URL('..', import.meta.url).pathname;
const { parseNPPS } = (await import(ROOT + 'common/lib/nppsParser.ts')) as any;

const DIRS = ['protocols/predefined', 'npps/fixtures'];
const OUT = ROOT + 'app/NeurOneShared/TestData/npps-parse-golden.json';

const pbm = (body: string) => `protocol "T" {\n  duration: 5m\n  pbm_transcranial {\n    wavelength: "808nm"\n    irradiance: 100mW_cm2\n${body}\n  }\n}\n`;
const blk = (type: string, body: string) => `protocol "T" {\n  duration: 5m\n  ${type} {\n${body}\n  }\n}\n`;

/** Inputs the fixtures do not cover, each a refusal or a normalisation worth pinning. */
export const ERROR_CASES: Record<string, string> = {
  cw_with_duty: pbm('    frequency: 0Hz\n    duty_cycle: 25%'),
  cw_deep_with_duty: blk('pbm_deep_1170nm', '    intensity_mw_cm2: 500\n    frequency: 0\n    duty_cycle: 50%'),
  cw_intranasal_with_duty: blk('pbm_intranasal', '    wavelength: "660nm"\n    irradiance: 30mW_cm2\n    frequency: 0\n    duty_cycle: 50%'),
  percent_irradiance: pbm('    intensity: 80%'),
  deep_has_no_intensity: blk('pbm_deep_1170nm', '    intensity: 500'),
  unitless_volume: blk('audio_entrainment', '    volume: 70'),
  percent_volume: blk('audio_entrainment', '    volume: 70%'),
  wrong_unit_irradiance: pbm('    irradiance: 12mA'),
  missing_volume: blk('audio_entrainment', '    carrier_hz: 440'),
  retired_wavelength: blk('pbm_transcranial', '    wavelength: "660_808nm"\n    irradiance: 100mW_cm2'),
  missing_wavelength: blk('pbm_transcranial', '    irradiance: 100mW_cm2'),
  empty_zones: pbm('    zones: []'),
  bad_zone_selector: pbm('    zones: everything'),
  unknown_modality: blk('hologram', '    x: 1'),
  intensity_on_unknown_field: blk('eeg_neurofeedback', '    intensity: 1mA'),
  negative_start: blk('bes_tacs', '    start: -5s'),
  unterminated_string: 'protocol "T {\n}\n',
  stray_minus: blk('bes_tacs', '    frequency: -'),
  digit_leading: blk('qeeg_21ch', '    montage: 10-20'),
  bad_top_level: 'banana "T" {}\n',
};

/** Accepted inputs that pin alias and default behaviour. */
export const OK_CASES: Record<string, string> = {
  canonical_spellings: blk('bes_tacs', '    frequency_hz: 12\n    intensity_milliamps: 0.6\n    waveform: square'),
  alias_spellings: blk('bes_tacs', '    frequency: 12Hz\n    intensity: 0.6mA\n    waveform: square'),
  canonical_beats_alias: blk('bes_tacs', '    frequency_hz: 40\n    frequency: 10'),
  alias_then_canonical: blk('bes_tacs', '    frequency: 10\n    frequency_hz: 40'),
  cw_reads_as_full_duty: pbm('    frequency: 0Hz'),
  cw_with_full_duty: pbm('    frequency: 0Hz\n    duty_cycle: 100%'),
  pulsed: pbm('    frequency: 40Hz\n    duty_cycle: 25%'),
  clinician_selected: pbm('    zones: clinician_selected'),
  named_zones: pbm('    zones: ["Frontal Left", "Frontal Right"]'),
  noise_none_is_omitted: blk('audio_entrainment', '    volume: 70dB\n    noise: none'),
  noise_pink: blk('audio_entrainment', '    volume: 70dB\n    noise: pink\n    binaural_hz: 40Hz'),
  tdcs_pairs_and_defaults: blk('tdcs', '    electrode_pairs: [["F3","F4"],["Fp1"],["P3","P4"]]'),
  eeg_custom_channels: blk('eeg_neurofeedback', '    channels: custom\n    custom_channels: ["Cz","Pz"]\n    closed_loop: false'),
  interval_and_start: blk('bes_tacs', '    interval_on: 20s\n    interval_off: 10s\n    repeat: 3\n    start: 2m\n    enabled: false'),
  repeat_until_end: blk('bes_tacs', '    repeat: until_end'),
  every_modality_defaults: ['pbm_intranasal', 'eeg_neurofeedback', 'bes_tacs', 'tdcs', 'vns_hrv', 'visual_stimulation', 'qeeg_21ch', 'tms', 'clinical_tacs', 'hd_tdcs', 'cervical_vns', 'vibrotactile_40hz']
    .map(t => (t === 'pbm_intranasal' ? blk(t, '    wavelength: "660nm"\n    irradiance: 30mW_cm2') : blk(t, ''))).join('\n'),
  deep_defaults: blk('pbm_deep_1170nm', ''),
  metadata_everything: 'protocol "M" {\n  id: "11111111-2222-3333-4444-555555555555"\n  description: "d"\n  author: "a"\n  version: "2"\n  readonly: true\n  tags: [alpha, "x y", 40Hz]\n  interval_count: 4\n  conditions: ["c1", c2]\n  references: ["http://a", ["L", "http://b"], ["only"]]\n  unknown_field: [1, [2, 3]]\n}\n',
};

function normaliseEntry(e: any): any {
  if (e.kind !== 'single') return { kind: 'skipped', what: e.kind };
  const p = e.protocol;
  const out: any = {};
  if (p.isPredefined) out.id = p.id;
  out.name = p.name; out.description = p.description; out.author = p.author; out.version = p.version;
  out.tags = p.tags; out.isPredefined = p.isPredefined; out.timingMode = p.timingMode;
  out.modalities = p.modalities.map((m: any) => ({
    type: m.modalityParams.type, enabled: m.enabled, params: m.modalityParams.params, interval: m.interval,
  }));
  for (const k of ['isReadOnly', 'conditions', 'references']) if (p[k] !== undefined) out[k] = p[k];
  return out;
}

function parse(src: string): { entries?: any[]; error?: string } {
  try {
    const entries = parseNPPS(src);
    // Only protocols are interpreted by the core today; other blocks are counted.
    return { entries: entries.map((e: any) => (e.kind === 'single' ? { kind: 'single', protocol: normaliseEntry(e) } : normaliseEntry(e))) };
  } catch (err: any) {
    return { error: err.message };
  }
}

// parseNPPS also yields zones/conditions/limits only through side channels, so a file's
// non-protocol blocks are not in `entries`; the core reports them as `skipped` and the test
// filters those out before comparing.
const out: Record<string, any> = { files: {}, cases: {} };
for (const dir of DIRS) {
  for (const f of fs.readdirSync(ROOT + dir).filter(n => n.endsWith('.npps')).sort()) {
    const r = parse(fs.readFileSync(path.join(ROOT, dir, f), 'utf8'));
    out.files[`${dir}/${f}`] = r;
  }
}
for (const [name, src] of Object.entries({ ...ERROR_CASES, ...OK_CASES })) {
  out.cases[name] = { source: src, ...parse(src) };
}

const text = JSON.stringify(out, null, 1) + '\n';
if (process.argv.includes('--check')) {
  const have = fs.existsSync(OUT) ? fs.readFileSync(OUT, 'utf8') : '';
  if (have !== text) {
    console.error('npps-parse-golden.json is stale: run `bun scripts/gen-npps-parse-golden.ts` and commit it.');
    process.exit(1);
  }
  console.log('npps-parse-golden.json is current');
} else {
  fs.writeFileSync(OUT, text);
  console.log(`${Object.keys(out.files).length} files, ${Object.keys(out.cases).length} cases`);
}
