#!/usr/bin/env bun
/**
 * gen-npps-parse-golden.ts — the web parser's own output, as the golden vectors the
 * shared NPPS core (common/npps-core, OI-NPPS-CORE-01) is diffed against.
 *
 *   bun scripts/gen-npps-parse-golden.ts           # rewrites app/NeurOneShared/TestData/npps-parse-golden.json
 *   bun scripts/gen-npps-parse-golden.ts --check   # fails if the committed file is stale
 *
 * For every .npps file in protocols/predefined and npps/fixtures it records everything the file
 * declares (the protocol and composite entries, the zones, conditions, wavelength rules and the
 * first limits set; ids and timestamps that are generated per parse are dropped) and, for every
 * input the web parser refuses, the exact message. The core must
 * produce the same entries and the same messages. `ERROR_CASES` adds refusals the fixtures do
 * not cover; add a case when a refusal is added.
 */
import fs from 'node:fs';
import path from 'node:path';

const ROOT = new URL('..', import.meta.url).pathname;
const { parseNPPSFile, parseNPPSLimits, buildNamespace, validateNamespaceReferences } = (await import(ROOT + 'common/lib/nppsParser.ts')) as any;

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
  zone_bad_sockets: 'zone "Z" {\n  sockets: [0, 99, "x", 2.5, true, 3]\n}\n',
  zone_one_bad_socket: 'zone "Z" {\n  sockets: [1, 81]\n}\n',
  zone_hex_socket: 'zone "Z" {\n  sockets: ["0x10"]\n}\n',
  zone_no_name: 'zone {\n  sockets: [1]\n}\n',
  condition_no_name: 'condition {\n  link: "http://x"\n}\n',
  composite_no_name: 'composite {\n}\n',
  layer_no_name: 'composite "C" {\n  layer {\n  }\n}\n',
  wr_no_name: 'wavelength_rules {\n}\n',
  wr_bad_level: 'wavelength_rules "W" {\n  level: sometimes\n}\n',
  wr_unknown_channel: 'wavelength_rules "W" {\n  channel "led_900" {\n    nominal_nm: 900\n    min_nm: 890\n    max_nm: 910\n  }\n}\n',
  wr_channel_no_name: 'wavelength_rules "W" {\n  channel {\n  }\n}\n',
  wr_missing_field: 'wavelength_rules "W" {\n  channel "led_808" {\n    nominal_nm: 808\n    min_nm: 798\n  }\n}\n',
  wr_inverted: 'wavelength_rules "W" {\n  channel "led_808" {\n    nominal_nm: 808\n    min_nm: 840\n    max_nm: 798\n  }\n}\n',
  wr_duplicate_channel: 'wavelength_rules "W" {\n  channel "led_808" {\n    nominal_nm: 808\n    min_nm: 798\n    max_nm: 840\n  }\n  channel "led_808" {\n    nominal_nm: 808\n    min_nm: 798\n    max_nm: 840\n  }\n}\n',
  wr_zero_value: 'wavelength_rules "W" {\n  channel "led_660" {\n    nominal_nm: 660\n    min_nm: 0\n    max_nm: 670\n  }\n}\n',
  limits_scalar_as_block: 'limits "L" {\n  level { }\n}\n',
  limits_modality_with_colon: 'limits "L" {\n  tdcs: 1\n}\n',
  limits_unknown_key: 'limits "L" {\n  frobnicate: 1\n}\n',
  limits_retired_pbm_percent: 'limits "L" {\n  pbm_transcranial {\n    max_intensity: 80\n  }\n}\n',
  limits_retired_nasal_percent: 'limits "L" {\n  pbm_intranasal {\n    max_intensity: 80\n  }\n}\n',
  limits_retired_audio_percent: 'limits "L" {\n  audio_entrainment {\n    max_intensity: 80\n  }\n}\n',
  limits_unterminated: 'limits "L" {\n  tdcs {\n',
};

/** Accepted inputs that pin alias and default behaviour. */
export const OK_CASES: Record<string, string> = {
  zone_everything: 'zone "Z" {\n  id: "zid"\n  description: "d"\n  sockets: [5, 3, "4", 3, "+7"]\n  types: [led_660, "ntc", 5]\n  exclude_types: true\n  unknown: [1]\n}\n',
  zone_minimal: 'zone "Z" {\n}\n',
  condition_everything: 'condition "C" {\n  id: "cid"\n  link: "http://c"\n  code: "X1"\n  description: "d"\n  unknown: 1\n}\n',
  condition_minimal: 'condition "C" {\n}\n',
  composite_everything: 'composite "C" {\n  id: "11111111-2222-3333-4444-555555555555"\n  description: "d"\n  author: "a"\n  version: "3"\n  readonly: true\n  tags: [a, b]\n  conflict_resolution: sequential\n  conditions: ["x"]\n  references: ["http://a"]\n  unknown: 1\n  layer "P1" {\n    start: 2m\n    end: 5m\n    intensity_scale: 0.5\n    other: 1\n  }\n  layer "P2" {\n    duration: 90s\n  }\n}\n',
  composite_minimal: 'composite "C" {\n}\n',
  wr_everything: 'wavelength_rules "W" {\n  level: user\n  description: "d"\n  unknown: 1\n  channel "led_660" {\n    nominal_nm: 660\n    min_nm: 650\n    max_nm: 670\n    other: 1\n  }\n  channel "led_1064" {\n    nominal_nm: 1064\n    min_nm: 1060\n    max_nm: 1070\n  }\n}\n',
  wr_minimal: 'wavelength_rules "W" {\n}\n',
  limits_everything: 'limits "L" {\n  level: helmet\n  helmet_id: "h1"\n  individual_id: "i1"\n  description: "d"\n  pbm_transcranial {\n    max_irradiance_mw_cm2: 300\n    max_frequency: 50\n    max_duty_cycle: 25\n    max_session_dose: 10\n    max_daily_dose: 30\n    other: 1\n  }\n  pbm_intranasal {\n    max_irradiance_mw_cm2: 60\n    max_session_dose: 5\n    max_session_duration: 600\n  }\n  eeg_neurofeedback {\n    allowed_bands: [alpha, theta]\n    require_closed_loop: true\n  }\n  bes_tacs {\n    max_intensity: 1\n    max_frequency: 40\n    min_frequency: 0.5\n    max_session_duration: 1800\n    max_sessions_per_day: 3\n  }\n  tdcs {\n    max_intensity: 2\n    max_session_duration: 1800\n    max_sessions_per_day: 2\n  }\n  vns_hrv {\n    max_intensity: 2\n    max_frequency: 25\n    max_session_duration: 900\n    allowed_protocols: standalone\n  }\n  audio_entrainment {\n    max_volume_db: 80\n    max_frequency: 40\n    max_binaural_beats: 30\n    max_isochronic_tones: 20\n  }\n  visual_stimulation {\n    max_frequency: 40\n    min_frequency: 1\n    allowed_modes: [binocular]\n    block_high_risk_range: true\n  }\n  tms {\n    max_intensity_pct_mt: 120\n    max_pulses_per_session: 3000\n    max_pulses_per_day: 6000\n    max_sessions_per_week: 5\n    allowed_protocols: [rTMS]\n    allowed_targets: ["DLPFC_L"]\n  }\n  pbm_deep_1170nm {\n    max_intensity: 800\n    max_session_duration: 600\n  }\n  clinical_tacs {\n    max_intensity: 4\n    max_session_duration: 1200\n  }\n  hd_tdcs {\n    max_intensity: 2\n    max_session_duration: 1200\n    allowed_montages: ring_4x1\n  }\n  cervical_vns {\n    max_intensity: 2\n    max_session_duration: 600\n  }\n  vibrotactile_40hz {\n    max_intensity: 1.2\n    max_session_duration: 600\n  }\n}\n',
  limits_defaults: 'limits {\n}\n',
  limits_ident_name: 'limits Mine {\n  level: sideways\n}\n',
  limits_odd_values: 'limits "L" {\n  bes_tacs {\n    max_intensity: "0.5"\n    max_frequency: true\n    min_frequency: [2]\n    max_sessions_per_day: "x"\n  }\n  eeg_neurofeedback {\n    require_closed_loop: "no"\n    allowed_bands: 7\n  }\n}\n',
  everything_in_one_file: 'zone "Z" {\n  sockets: [1]\n}\ncondition "C" {\n}\nprotocol "P" {\n  conditions: ["C"]\n  pbm_transcranial {\n    zones: ["Z"]\n    wavelength: "808nm"\n    irradiance: 100mW_cm2\n  }\n}\ncomposite "K" {\n  layer "P" {\n  }\n}\nlimits "L" {\n}\nwavelength_rules "W" {\n}\n',
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

function normaliseProtocol(p: any): any {
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

function normaliseComposite(c: any): any {
  const out: any = {};
  if (c.isPredefined) out.id = c.id;
  out.name = c.name; out.description = c.description; out.author = c.author; out.version = c.version;
  out.tags = c.tags; out.isPredefined = c.isPredefined;
  out.layers = c.layers.map(({ id: _id, ...layer }: any) => layer);
  out.conflictResolution = c.conflictResolution;
  for (const k of ['isReadOnly', 'conditions', 'references']) if (c[k] !== undefined) out[k] = c[k];
  return out;
}

function normaliseEntry(e: any): any {
  return e.kind === 'single'
    ? { kind: 'single', protocol: normaliseProtocol(e.protocol) }
    : { kind: 'composite', composite: normaliseComposite(e.composite) };
}

/** A limits set without the id and timestamps the web parser generates per parse. */
function normaliseLimits(l: any): any {
  if (!l) return null;
  const { id: _id, createdAt: _c, modifiedAt: _m, ...rest } = l;
  return rest;
}

function parse(src: string): any {
  try {
    const f = parseNPPSFile(src);
    return {
      entries: f.entries.map(normaliseEntry),
      zones: f.zones,
      conditions: f.conditions,
      wavelengthRules: f.wavelengthRules,
      // The web parser keeps only the first limits block (parseNPPSLimits); the core reports every one.
      limits: normaliseLimits(parseNPPSLimits(src)),
    };
  } catch (err: any) {
    return { error: err.message };
  }
}

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

// Namespaces: several files folded into one (`buildNamespace`) and their cross-references checked
// (`validateNamespaceReferences`), in load order. Every duplicate-name and dangling-reference rule is pinned.
const zoneSrc = (name: string, sockets: string) => `zone "${name}" {\n  sockets: [${sockets}]\n}\n`;
const condSrc = (name: string) => `condition "${name}" {\n  link: "http://${name}"\n}\n`;
const useSrc = (name: string, zones: string[], conds: string[]) =>
  `protocol "${name}" {\n  conditions: [${conds.map(c => `"${c}"`).join(', ')}]\n  pbm_transcranial {\n    zones: ${zones.length ? `[${zones.map(z => `"${z}"`).join(', ')}]` : 'clinician_selected'}\n    wavelength: "808nm"\n    irradiance: 100mW_cm2\n  }\n}\n`;
export const NAMESPACE_CASES: Record<string, string[]> = {
  cross_file_ok: [zoneSrc('A', '1, 2') + condSrc('C'), useSrc('P', ['A'], ['C'])],
  duplicate_zone: [zoneSrc('A', '1'), zoneSrc('A', '2'), useSrc('P', ['A'], [])],
  triple_zone: [zoneSrc('A', '1'), zoneSrc('A', '2'), zoneSrc('A', '3'), zoneSrc('B', '4')],
  duplicate_condition: [condSrc('C'), condSrc('C'), useSrc('P', [], ['C'])],
  triple_condition: [condSrc('C'), condSrc('C'), condSrc('C'), condSrc('D')],
  dangling_zone_and_condition: [useSrc('P', ['Nope', 'Gone'], ['Missing']) + 'composite "K" {\n  conditions: ["AlsoMissing"]\n}\n'],
  duplicate_within_one_file: [zoneSrc('A', '1') + zoneSrc('A', '2')],
  empty: [],
};
out.namespaces = {};
for (const [name, srcs] of Object.entries(NAMESPACE_CASES)) {
  out.namespaces[name] = { sources: srcs, ...namespaceOf(srcs) };
}
const libraryFiles = fs.readdirSync(ROOT + 'protocols/predefined').filter(n => n.endsWith('.npps')).sort().map(n => 'protocols/predefined/' + n);
out.namespaces.library = {
  paths: libraryFiles,
  ...namespaceOf(libraryFiles.map(f => fs.readFileSync(ROOT + f, 'utf8'))),
};

function namespaceOf(srcs: string[]) {
  const { namespace, errors } = buildNamespace(srcs.map(src => parseNPPSFile(src)));
  return {
    entries: namespace.entries.map(normaliseEntry),
    zones: [...namespace.zones.values()],
    conditions: [...namespace.conditions.values()],
    errors,
    referenceErrors: validateNamespaceReferences(namespace),
  };
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
